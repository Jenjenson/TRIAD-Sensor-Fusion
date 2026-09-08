#include "TRIADIstanaExploreV5DTreeRealismActor.h"

#include "ComponentReregisterContext.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5BVisualActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogTRIADIstanaExploreV5DTreeRealism, Log, All);

namespace
{
const FString TreeRealismClaimLabel(
    TEXT("ISTANA_EXPLORE_V5D_TREE_CANOPY_RENDER_ONLY_VISUAL_ASSUMPTION_NOT_SURVEY_NOT_BOTANICAL_INVENTORY_NOT_CURRENT_SEASON_NOT_COLLISION_NAVIGATION_SENSOR_OR_RF_TRUTH"));
const FName TreeRealismActorTag(
    TEXT("TRIADIstanaExploreV5DTreeRealism"));
const FName TreeRuntimeOverrideTag(
    TEXT("TRIADIstanaExploreV5DTreeRuntimeVisualOverride"));

constexpr int32 TreeFormCount = 5;
constexpr int32 SourceTreeComponentCount = 10;
constexpr int32 SourceTreeCount = 729;
constexpr int32 TreeMaterialCount = 13;
constexpr int32 RuntimeTreeResponseMaterialCount = TreeMaterialCount * 2;
constexpr int32 WindScalarParameterCount = 5;
constexpr int32 ExistingTreeBaseCount = 64;
constexpr int32 SelectedNearTreeCount = 128;
constexpr int32 RootFragmentsPerTree = 3;
constexpr int32 LeafLitterPerTree = 8;
constexpr int32 RootFragmentCount =
    SelectedNearTreeCount * RootFragmentsPerTree;
constexpr int32 LeafLitterCount = SelectedNearTreeCount * LeafLitterPerTree;
static_assert(RootFragmentCount == 384);
static_assert(LeafLitterCount == 1024);
constexpr uint32 LayoutSeed = 0x56354454u;
constexpr double ExistingBaseClearanceCm = 430.0;
constexpr double RootApronZOffsetCm = -0.20;
constexpr double LeafLitterZOffsetCm = 0.28;
constexpr double RootFragmentOffsetMinCm = 18.0;
constexpr double RootFragmentOffsetMaxCm = 62.0;
constexpr double RootApronScaleMin = 0.38;
constexpr double RootApronScaleMax = 0.58;
constexpr double LeafLitterScaleMin = 0.035;
constexpr double LeafLitterScaleMax = 0.070;
constexpr double PreferredVisualRadiusCm = 30000.0;
// The isolated derivatives retain the complete geometry chain for provenance
// validation. The previous pass fixed all 729 trees to source LOD1, which
// flattened nearby branch and leaf silhouettes. Restore automatic selection,
// admit the exact source LOD0 near the camera, and hold each transition longer
// so medium-range crowns retain their irregular tropical layering. This is a
// render-only component policy; source assets, transforms and RF authority are
// still snapshotted and untouched.
constexpr int32 RuntimeTreeMinimumLod = 0;
constexpr int32 RuntimeTreeForcedLodModel = 0;
// UE multiplies HISM screen-size transition distance by this value. A modest
// extension keeps detailed source foliage readable without forcing every tree
// to the multi-million-triangle LOD0 at every distance.
constexpr float RuntimeTreeLodDistanceScale = 1.8f;
const FVector2D PreferredVisualFocusCm(0.0, 9500.0);

const FString TreeBaseTransitionMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_SoilMulch_Layered.M_IPV5D_SoilMulch_Layered"));

const FString NearLodZeroMeshPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Dome_NearLOD0.SM_IPV5D_Tree_Dome_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_HighForkRounded_NearLOD0.SM_IPV5D_Tree_HighForkRounded_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_ColumnarNarrow_NearLOD0.SM_IPV5D_Tree_ColumnarNarrow_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Palm_NearLOD0.SM_IPV5D_Tree_Palm_NearLOD0")};
static_assert(UE_ARRAY_COUNT(NearLodZeroMeshPaths) == TreeFormCount);

// Flattened in exact mesh-form/slot order: umbrella 3, dome 3,
// high-fork 3, columnar 3, palm 1. The source paths are evidence gates only;
// they are never mutated. The response paths are isolated V5D-owned assets.
const int32 TreeMaterialOffsets[] = {0, 3, 6, 9, 12, 13};
static_assert(UE_ARRAY_COUNT(TreeMaterialOffsets) == TreeFormCount + 1);
const FString SourceTreeMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree02_Trunk_Wind.M_IPV4_IslandTree02_Trunk_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree02_Leaves_Wind.M_IPV4_IslandTree02_Leaves_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree02_Branches_Wind.M_IPV4_IslandTree02_Branches_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_TreeSmall02_Branches_Wind.M_IPV4_TreeSmall02_Branches_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_TreeSmall02_Leaves_Wind.M_IPV4_TreeSmall02_Leaves_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_TreeSmall02_Trunk_Wind.M_IPV4_TreeSmall02_Trunk_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree01_Trunk_Wind.M_IPV4_IslandTree01_Trunk_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree01_Leaves_Wind.M_IPV4_IslandTree01_Leaves_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_IslandTree01_Branches_Wind.M_IPV4_IslandTree01_Branches_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_BroadleafBranches_Wind.M_IPV4_BroadleafBranches_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_BroadleafTrunk_Wind.M_IPV4_BroadleafTrunk_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_BroadleafLeavesDark_Wind.M_IPV4_BroadleafLeavesDark_Wind"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_QuaterniusPalm_Atlas_Wind.M_IPV4_QuaterniusPalm_Atlas_Wind")};
const FString TreeResponseMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Trunk_Response.M_IPV5D_Tree_Umbrella_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Leaves_Response.M_IPV5D_Tree_Umbrella_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Branches_Response.M_IPV5D_Tree_Umbrella_Branches_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Dome_Branches_Response.M_IPV5D_Tree_Dome_Branches_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Dome_Leaves_Response.M_IPV5D_Tree_Dome_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Dome_Trunk_Response.M_IPV5D_Tree_Dome_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_HighFork_Trunk_Response.M_IPV5D_Tree_HighFork_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_HighFork_Leaves_Response.M_IPV5D_Tree_HighFork_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_HighFork_Branches_Response.M_IPV5D_Tree_HighFork_Branches_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Columnar_Branches_Response.M_IPV5D_Tree_Columnar_Branches_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Columnar_Trunk_Response.M_IPV5D_Tree_Columnar_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Columnar_Leaves_Response.M_IPV5D_Tree_Columnar_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Palm_Composite_Response.M_IPV5D_Tree_Palm_Composite_Response")};
static_assert(UE_ARRAY_COUNT(SourceTreeMaterialPaths) == TreeMaterialCount);
static_assert(UE_ARRAY_COUNT(TreeResponseMaterialPaths) == TreeMaterialCount);

const FName WindScalarParameters[] = {
    TEXT("TRIAD_WindStrengthCm"),
    TEXT("TRIAD_WindSpeed"),
    TEXT("TRIAD_WindHeightCm"),
    TEXT("TRIAD_WindResponseScale"),
    TEXT("TRIAD_MaxWpoCm")};
const FName WindDirectionParameter(TEXT("TRIAD_WindDirection"));
static_assert(UE_ARRAY_COUNT(WindScalarParameters) == WindScalarParameterCount);

int32 FlattenedTreeMaterialIndex(int32 FormIndex, int32 Slot)
{
    if (FormIndex < 0 || FormIndex >= TreeFormCount || Slot < 0 ||
        Slot >= TreeMaterialOffsets[FormIndex + 1] -
            TreeMaterialOffsets[FormIndex])
    {
        return INDEX_NONE;
    }
    return TreeMaterialOffsets[FormIndex] + Slot;
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

uint32 HashTransform(const FTransform& Transform, uint32 Salt)
{
    const FVector Location = Transform.GetTranslation();
    const uint32 X = static_cast<uint32>(FMath::RoundToInt(Location.X * 0.1));
    const uint32 Y = static_cast<uint32>(FMath::RoundToInt(Location.Y * 0.1));
    return MixBits(X * 0x9E3779B9u ^ Y * 0x85EBCA6Bu ^ Salt ^ LayoutSeed);
}

double DistanceSquared2D(const FVector& A, const FVector& B)
{
    const double DX = A.X - B.X;
    const double DY = A.Y - B.Y;
    return DX * DX + DY * DY;
}

bool IsFinitePositiveTransform(const FTransform& Transform)
{
    const FVector Location = Transform.GetTranslation();
    const FVector Scale = Transform.GetScale3D();
    return !Transform.ContainsNaN() && !Location.ContainsNaN() &&
        !Scale.ContainsNaN() && Transform.GetRotation().IsNormalized() &&
        FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y) &&
        FMath::IsFinite(Location.Z) && FMath::IsFinite(Scale.X) &&
        FMath::IsFinite(Scale.Y) && FMath::IsFinite(Scale.Z) &&
        Scale.X > 0.0 && Scale.Y > 0.0 && Scale.Z > 0.0;
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

void GetTreeComponents(
    const ATRIADIstanaExploreV4LandscapeActor* Actor,
    TArray<UHierarchicalInstancedStaticMeshComponent*>& OutComponents)
{
    OutComponents.Reset();
    if (!Actor)
    {
        return;
    }
    OutComponents = {
        Actor->UmbrellaTreeInstances,
        Actor->DomeTreeInstances,
        Actor->HighForkRoundedTreeInstances,
        Actor->ColumnarNarrowTreeInstances,
        Actor->PalmTreeInstances,
        Actor->HeritageUmbrellaInstances,
        Actor->HeritageDomeInstances,
        Actor->HeritageHighForkRoundedInstances,
        Actor->HeritageColumnarNarrowInstances,
        Actor->HeritagePalmInstances};
}

bool AppendWorldTransforms(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    TArray<FTransform>& OutTransforms,
    FString& OutError)
{
    if (!Component || !Component->GetStaticMesh())
    {
        OutError = TEXT("A V5D source tree component or mesh is absent.");
        return false;
    }
    for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
    {
        FTransform WorldTransform;
        if (!Component->GetInstanceTransform(Index, WorldTransform, true) ||
            !IsFinitePositiveTransform(WorldTransform))
        {
            OutError = FString::Printf(
                TEXT("V5D could not read finite tree transform %d from '%s'."),
                Index,
                *Component->GetName());
            return false;
        }
        OutTransforms.Add(WorldTransform);
    }
    return true;
}

bool MeshesShareGeometryAndSlots(
    const UStaticMesh* Candidate,
    const UStaticMesh* Source,
    int32 FormIndex,
    FString& OutError)
{
    const FStaticMeshRenderData* CandidateRender = Candidate
        ? Candidate->GetRenderData()
        : nullptr;
    const FStaticMeshRenderData* SourceRender = Source
        ? Source->GetRenderData()
        : nullptr;
    if (!Candidate || !Source || FormIndex < 0 || FormIndex >= TreeFormCount ||
        Candidate == Source ||
        Candidate->GetPathName() != NearLodZeroMeshPaths[FormIndex] ||
        Candidate->GetMinLODIdx() != 0 || Source->GetMinLODIdx() != 1 ||
        !Candidate->GetBounds().Origin.Equals(Source->GetBounds().Origin, 0.001) ||
        !Candidate->GetBounds().BoxExtent.Equals(
            Source->GetBounds().BoxExtent,
            0.001) ||
        !CandidateRender || !SourceRender ||
        CandidateRender->LODResources.Num() != SourceRender->LODResources.Num() ||
        Candidate->GetStaticMaterials().Num() !=
            Source->GetStaticMaterials().Num())
    {
        OutError = FString::Printf(
            TEXT("V5D isolated tree derivative %d lost its exact source geometry/slot/min-LOD relationship."),
            FormIndex);
        return false;
    }
    for (int32 Lod = 0; Lod < CandidateRender->LODResources.Num(); ++Lod)
    {
        if (CandidateRender->LODResources[Lod].GetNumTriangles() !=
                SourceRender->LODResources[Lod].GetNumTriangles() ||
            CandidateRender->LODResources[Lod].Sections.Num() !=
                SourceRender->LODResources[Lod].Sections.Num())
        {
            OutError = FString::Printf(
                TEXT("V5D isolated tree derivative %d changed LOD %d topology."),
                FormIndex,
                Lod);
            return false;
        }
    }
    for (int32 Slot = 0; Slot < Candidate->GetStaticMaterials().Num(); ++Slot)
    {
        const int32 MaterialIndex =
            FlattenedTreeMaterialIndex(FormIndex, Slot);
        const FStaticMaterial& CandidateMaterial =
            Candidate->GetStaticMaterials()[Slot];
        const FStaticMaterial& SourceMaterial = Source->GetStaticMaterials()[Slot];
        bool bImportedSlotNamesMatch = true;
#if WITH_EDITORONLY_DATA
        bImportedSlotNamesMatch =
            CandidateMaterial.ImportedMaterialSlotName ==
                SourceMaterial.ImportedMaterialSlotName;
#endif
        if (MaterialIndex == INDEX_NONE ||
            CandidateMaterial.MaterialSlotName != SourceMaterial.MaterialSlotName ||
            !bImportedSlotNamesMatch)
        {
            OutError = FString::Printf(
                TEXT("V5D isolated tree derivative %d changed source-slot identity %d."),
                FormIndex,
                Slot);
            return false;
        }
        const UMaterialInterface* CandidateResponseMaterial =
            Candidate->GetMaterial(Slot);
        if (!CandidateResponseMaterial ||
            CandidateResponseMaterial->GetPathName() !=
                TreeResponseMaterialPaths[MaterialIndex])
        {
            OutError = FString::Printf(
                TEXT("V5D isolated tree derivative %d slot %d lost its exact response material: actual=%s expected=%s."),
                FormIndex,
                Slot,
                CandidateResponseMaterial
                    ? *CandidateResponseMaterial->GetPathName()
                    : TEXT("<null>"),
                *TreeResponseMaterialPaths[MaterialIndex]);
            return false;
        }
    }
    return true;
}

bool SourceComponentsUseExactV4Materials(
    const UHierarchicalInstancedStaticMeshComponent* MainSourceComponent,
    const UHierarchicalInstancedStaticMeshComponent* HeritageSourceComponent,
    const UStaticMesh* Source,
    int32 FormIndex,
    FString& OutError)
{
    if (!MainSourceComponent || !HeritageSourceComponent || !Source ||
        FormIndex < 0 || FormIndex >= TreeFormCount ||
        MainSourceComponent->GetStaticMesh() != Source ||
        HeritageSourceComponent->GetStaticMesh() != Source ||
        MainSourceComponent->GetNumMaterials() !=
            Source->GetStaticMaterials().Num() ||
        HeritageSourceComponent->GetNumMaterials() !=
            Source->GetStaticMaterials().Num())
    {
        OutError = FString::Printf(
            TEXT("V5D source tree form %d lost its exact main/Heritage component, mesh or material-slot binding."),
            FormIndex);
        return false;
    }
    for (int32 Slot = 0; Slot < Source->GetStaticMaterials().Num(); ++Slot)
    {
        const int32 MaterialIndex =
            FlattenedTreeMaterialIndex(FormIndex, Slot);
        const UMaterialInterface* MainEffectiveSourceMaterial =
            MainSourceComponent->GetMaterial(Slot);
        const UMaterialInterface* HeritageEffectiveSourceMaterial =
            HeritageSourceComponent->GetMaterial(Slot);
        if (MaterialIndex == INDEX_NONE || !MainEffectiveSourceMaterial ||
            MainEffectiveSourceMaterial->GetPathName() !=
                SourceTreeMaterialPaths[MaterialIndex] ||
            !HeritageEffectiveSourceMaterial ||
            HeritageEffectiveSourceMaterial->GetPathName() !=
                SourceTreeMaterialPaths[MaterialIndex])
        {
            OutError = FString::Printf(
                TEXT("V5D source tree form %d slot %d lost its exact main/Heritage effective V4 wind material: main=%s heritage=%s expected=%s."),
                FormIndex,
                Slot,
                MainEffectiveSourceMaterial
                    ? *MainEffectiveSourceMaterial->GetPathName()
                    : TEXT("<null>"),
                HeritageEffectiveSourceMaterial
                    ? *HeritageEffectiveSourceMaterial->GetPathName()
                    : TEXT("<null>"),
                MaterialIndex != INDEX_NONE
                    ? *SourceTreeMaterialPaths[MaterialIndex]
                    : TEXT("<invalid-index>"));
            return false;
        }
    }
    return true;
}

void ConfigureRenderOnlyTransition(
    UHierarchicalInstancedStaticMeshComponent* Component,
    int32 StartCullDistance,
    int32 EndCullDistance)
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
    Component->SetCastShadow(false);
    Component->SetCastContactShadow(false);
    Component->SetAffectDistanceFieldLighting(false);
    Component->SetCullDistances(StartCullDistance, EndCullDistance);
    Component->bEnableDensityScaling = false;
    Component->bAutoRebuildTreeOnInstanceChanges = false;
}

struct FTreeCandidate
{
    int32 SourceIndex = INDEX_NONE;
    double Score = 0.0;
    uint32 Hash = 0u;
};
} // namespace

ATRIADIstanaExploreV5DTreeRealismActor::
    ATRIADIstanaExploreV5DTreeRealismActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.bAllowTickOnDedicatedServer = false;
    ClaimLabel = ExpectedClaimLabel();

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("V5DTreeRealismRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    RootApronInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DTreeRootApronTransitions"));
    LeafLitterInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DTreeLeafLitterTransitions"));
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         {RootApronInstances.Get(), LeafLitterInstances.Get()})
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
    }
    ConfigureRenderOnlyTransition(RootApronInstances, 0, 85000);
    ConfigureRenderOnlyTransition(LeafLitterInstances, 0, 65000);
}

const FString& ATRIADIstanaExploreV5DTreeRealismActor::ExpectedClaimLabel()
{
    return TreeRealismClaimLabel;
}

const FName& ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag()
{
    return TreeRealismActorTag;
}

const FName& ATRIADIstanaExploreV5DTreeRealismActor::RuntimeOverrideTag()
{
    return TreeRuntimeOverrideTag;
}

int32 ATRIADIstanaExploreV5DTreeRealismActor::
    ExpectedRuntimeTreeMinimumLod()
{
    return RuntimeTreeMinimumLod;
}

int32 ATRIADIstanaExploreV5DTreeRealismActor::
    ExpectedRuntimeTreeForcedLodModel()
{
    return RuntimeTreeForcedLodModel;
}

int32 ATRIADIstanaExploreV5DTreeRealismActor::ExpectedSourceTreeCount()
{
    return SourceTreeCount;
}

int32 ATRIADIstanaExploreV5DTreeRealismActor::ExpectedExistingTreeBaseCount()
{
    return ExistingTreeBaseCount;
}

int32 ATRIADIstanaExploreV5DTreeRealismActor::ExpectedSelectedNearTreeCount()
{
    return SelectedNearTreeCount;
}

int32 ATRIADIstanaExploreV5DTreeRealismActor::ExpectedLeafLitterCount()
{
    return LeafLitterCount;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DTreeRealismAssets& Assets,
    const ATRIADIstanaExploreV4LandscapeActor* SourceActor,
    FString& OutError)
{
    TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
    GetTreeComponents(SourceActor, Components);
    if (Assets.NearLodZeroTreeMeshes.Num() != TreeFormCount ||
        Assets.NearLodZeroTreeMeshes.Contains(nullptr) ||
        Components.Num() != SourceTreeComponentCount ||
        !Assets.TreeBaseTransitionMesh ||
        !Assets.TreeBaseTransitionMaterial ||
        Assets.TreeBaseTransitionMaterial->GetPathName() !=
            TreeBaseTransitionMaterialPath)
    {
        OutError = TEXT("The exact five-mesh V5D tree derivative and tree-base asset roster is incomplete.");
        return false;
    }
    for (int32 FormIndex = 0; FormIndex < TreeFormCount; ++FormIndex)
    {
        const UHierarchicalInstancedStaticMeshComponent* MainSourceComponent =
            Components[FormIndex];
        const UHierarchicalInstancedStaticMeshComponent* HeritageSourceComponent =
            Components[FormIndex + TreeFormCount];
        const UStaticMesh* MainSource = MainSourceComponent
            ? MainSourceComponent->GetStaticMesh()
            : nullptr;
        const UStaticMesh* HeritageSource = HeritageSourceComponent
            ? HeritageSourceComponent->GetStaticMesh()
            : nullptr;
        if (!MainSource || MainSource != HeritageSource ||
            !SourceComponentsUseExactV4Materials(
                MainSourceComponent,
                HeritageSourceComponent,
                MainSource,
                FormIndex,
                OutError) ||
            !MeshesShareGeometryAndSlots(
                Assets.NearLodZeroTreeMeshes[FormIndex],
                MainSource,
                FormIndex,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A main and Heritage tree form no longer share their exact admitted V4 source mesh.");
            }
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::
    BuildDeterministicTreeBaseLayout(
        const TArray<FTransform>& SourceTreeWorldTransforms,
        const TArray<FTransform>& ExistingTreeBaseWorldTransforms,
        FTRIADIstanaExploreV5DTreeRealismLayout& OutLayout,
        FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DTreeRealismLayout();
    if (SourceTreeWorldTransforms.Num() != SourceTreeCount ||
        ExistingTreeBaseWorldTransforms.Num() != ExistingTreeBaseCount)
    {
        OutError = FString::Printf(
            TEXT("V5D tree transition source census drifted: trees=%d/%d existingBases=%d/%d."),
            SourceTreeWorldTransforms.Num(),
            SourceTreeCount,
            ExistingTreeBaseWorldTransforms.Num(),
            ExistingTreeBaseCount);
        return false;
    }

    TArray<FTreeCandidate> Preferred;
    TArray<FTreeCandidate> Fallback;
    Preferred.Reserve(SourceTreeCount);
    Fallback.Reserve(SourceTreeCount);
    const FVector Focus(
        PreferredVisualFocusCm.X,
        PreferredVisualFocusCm.Y,
        0.0);
    for (int32 Index = 0; Index < SourceTreeWorldTransforms.Num(); ++Index)
    {
        const FTransform& Source = SourceTreeWorldTransforms[Index];
        if (!IsFinitePositiveTransform(Source))
        {
            OutError = FString::Printf(
                TEXT("V5D source tree transform %d is not finite and positive."),
                Index);
            return false;
        }
        const FVector Location = Source.GetTranslation();
        if (IsNearAny(
                Location,
                ExistingTreeBaseWorldTransforms,
                ExistingBaseClearanceCm + RootFragmentOffsetMaxCm))
        {
            continue;
        }
        const uint32 Hash = HashTransform(Source, static_cast<uint32>(Index));
        const double FocusDistanceSquared = DistanceSquared2D(Location, Focus);
        FTreeCandidate Candidate;
        Candidate.SourceIndex = Index;
        Candidate.Hash = Hash;
        Candidate.Score = FocusDistanceSquared +
            HashUnit(Hash ^ 0xA511E9B3u) * 64000000.0;
        Fallback.Add(Candidate);
        if (FocusDistanceSquared <=
            PreferredVisualRadiusCm * PreferredVisualRadiusCm)
        {
            Preferred.Add(Candidate);
        }
    }
    const auto SortCandidates = [](TArray<FTreeCandidate>& Candidates)
    {
        Candidates.Sort([](const FTreeCandidate& A, const FTreeCandidate& B)
        {
            if (!FMath::IsNearlyEqual(A.Score, B.Score, 0.001))
            {
                return A.Score < B.Score;
            }
            return A.SourceIndex < B.SourceIndex;
        });
    };
    SortCandidates(Preferred);
    SortCandidates(Fallback);

    TSet<int32> Selected;
    for (const TArray<FTreeCandidate>* Source : {&Preferred, &Fallback})
    {
        for (const FTreeCandidate& Candidate : *Source)
        {
            if (Selected.Num() >= SelectedNearTreeCount)
            {
                break;
            }
            Selected.Add(Candidate.SourceIndex);
        }
    }
    if (Selected.Num() != SelectedNearTreeCount)
    {
        OutError = FString::Printf(
            TEXT("V5D needs %d separated near-visible tree bases; only %d are available."),
            SelectedNearTreeCount,
            Selected.Num());
        return false;
    }

    TArray<int32> Ordered = Selected.Array();
    Ordered.Sort();
    OutLayout.SelectedSourceTreeIndices = Ordered;
    OutLayout.RootApronWorldTransforms.Reserve(RootFragmentCount);
    OutLayout.LeafLitterWorldTransforms.Reserve(LeafLitterCount);
    for (int32 SourceIndex : Ordered)
    {
        const FTransform& Source = SourceTreeWorldTransforms[SourceIndex];
        const uint32 Hash = HashTransform(
            Source,
            static_cast<uint32>(SourceIndex) ^ 0x524F4F54u);
        const double RootBaseAngleDegrees =
            HashUnit(Hash ^ 0x6D2B79F5u) * 360.0;
        const int32 RootAngleRotation = static_cast<int32>(
            MixBits(Hash ^ 0xA24BAED5u) %
            static_cast<uint32>(RootFragmentsPerTree));
        const int32 RootRadiusRotation = static_cast<int32>(
            MixBits(Hash ^ 0xB5297A4Du) %
            static_cast<uint32>(RootFragmentsPerTree));
        const int32 RootScaleRotation = static_cast<int32>(
            MixBits(Hash ^ 0xC2B2AE35u) %
            static_cast<uint32>(RootFragmentsPerTree));
        const double RootAngleSectorDegrees = 360.0 / RootFragmentsPerTree;
        const double RootRadiusBandWidth =
            (RootFragmentOffsetMaxCm - RootFragmentOffsetMinCm) /
            RootFragmentsPerTree;
        const double RootScaleBandWidth =
            (RootApronScaleMax - RootApronScaleMin) /
            RootFragmentsPerTree;
        for (int32 RootFragmentIndex = 0;
             RootFragmentIndex < RootFragmentsPerTree;
             ++RootFragmentIndex)
        {
            const uint32 RootFragmentHash = MixBits(
                Hash ^ (static_cast<uint32>(RootFragmentIndex) + 1u) *
                    0x9E3779B9u);
            const int32 AngleSector =
                (RootFragmentIndex + RootAngleRotation) %
                RootFragmentsPerTree;
            const int32 RadiusBand =
                (RootFragmentIndex + RootRadiusRotation) %
                RootFragmentsPerTree;
            const int32 ScaleBand =
                (RootFragmentIndex + RootScaleRotation) %
                RootFragmentsPerTree;
            const double RootOffsetAngleDegrees = RootBaseAngleDegrees +
                static_cast<double>(AngleSector) * RootAngleSectorDegrees +
                (HashUnit(RootFragmentHash ^ 0x27D4EB2Fu) - 0.5) *
                    RootAngleSectorDegrees * 0.34;
            const double RootOffsetAngleRadians = FMath::DegreesToRadians(
                RootOffsetAngleDegrees);
            const double RootOffsetRadius = RootFragmentOffsetMinCm +
                (static_cast<double>(RadiusBand) + 0.12 +
                    HashUnit(RootFragmentHash ^ 0x165667B1u) * 0.76) *
                    RootRadiusBandWidth;
            FVector RootLocation = Source.GetTranslation();
            RootLocation.X +=
                FMath::Cos(RootOffsetAngleRadians) * RootOffsetRadius;
            RootLocation.Y +=
                FMath::Sin(RootOffsetAngleRadians) * RootOffsetRadius;
            RootLocation.Z += RootApronZOffsetCm;
            const double RootScale = RootApronScaleMin +
                (static_cast<double>(ScaleBand) + 0.12 +
                    HashUnit(RootFragmentHash ^ 0x85EBCA6Bu) * 0.76) *
                    RootScaleBandWidth;
            OutLayout.RootApronWorldTransforms.Add(FTransform(
                FRotator(
                    0.0,
                    RootOffsetAngleDegrees +
                        (HashUnit(RootFragmentHash ^ 0xD3A2646Cu) - 0.5) *
                            24.0,
                    0.0),
                RootLocation,
                FVector(
                    RootScale,
                    RootScale * FMath::Lerp(
                        0.10,
                        0.22,
                        HashUnit(RootFragmentHash ^ 0xE6546B64u)),
                    RootScale * 0.10)));
        }

        const double LitterBaseAngleDegrees =
            HashUnit(Hash ^ 0xF1357AE5u) * 360.0;
        const int32 LitterAngleRotation = static_cast<int32>(
            MixBits(Hash ^ 0x94D049BBu) %
            static_cast<uint32>(LeafLitterPerTree));
        const int32 LitterRadiusRotation = static_cast<int32>(
            MixBits(Hash ^ 0x369DEA0Fu) %
            static_cast<uint32>(LeafLitterPerTree));
        const int32 LitterScaleRotation = static_cast<int32>(
            MixBits(Hash ^ 0xDB4F0B91u) %
            static_cast<uint32>(LeafLitterPerTree));
        const double LitterAngleSectorDegrees = 360.0 / LeafLitterPerTree;
        const double LitterRadiusBandWidth = (240.0 - 45.0) /
            LeafLitterPerTree;
        const double LitterScaleBandWidth =
            (LeafLitterScaleMax - LeafLitterScaleMin) /
            LeafLitterPerTree;
        for (int32 LitterIndex = 0;
             LitterIndex < LeafLitterPerTree;
             ++LitterIndex)
        {
            const uint32 LitterHash = MixBits(
                Hash ^ (static_cast<uint32>(LitterIndex) + 1u) *
                    0xC2B2AE3Du);
            const int32 AngleSector =
                (LitterIndex + LitterAngleRotation) % LeafLitterPerTree;
            const int32 RadiusBand =
                (LitterIndex + LitterRadiusRotation) % LeafLitterPerTree;
            const int32 ScaleBand =
                (LitterIndex + LitterScaleRotation) % LeafLitterPerTree;
            const double AngleDegrees = LitterBaseAngleDegrees +
                static_cast<double>(AngleSector) * LitterAngleSectorDegrees +
                (HashUnit(LitterHash ^ 0x85EBCA6Bu) - 0.5) *
                    LitterAngleSectorDegrees * 0.42;
            const double AngleRadians = FMath::DegreesToRadians(AngleDegrees);
            const double Radius = 45.0 +
                (static_cast<double>(RadiusBand) + 0.12 +
                    HashUnit(LitterHash ^ 0xD3A2646Cu) * 0.76) *
                    LitterRadiusBandWidth;
            FVector LitterLocation = Source.GetTranslation() + FVector(
                FMath::Cos(AngleRadians) * Radius,
                FMath::Sin(AngleRadians) * Radius,
                LeafLitterZOffsetCm);
            const double LitterScale = LeafLitterScaleMin +
                (static_cast<double>(ScaleBand) + 0.12 +
                    HashUnit(LitterHash ^ 0xA24BAED5u) * 0.76) *
                    LitterScaleBandWidth;
            OutLayout.LeafLitterWorldTransforms.Add(FTransform(
                FRotator(
                    0.0,
                    AngleDegrees +
                        (HashUnit(LitterHash ^ 0x9FB21C65u) - 0.5) * 38.0,
                    0.0),
                LitterLocation,
                FVector(
                    LitterScale,
                    LitterScale * FMath::Lerp(
                        0.22,
                        0.52,
                        HashUnit(LitterHash ^ 0xE6546B64u)),
                    LitterScale * 0.24)));
        }
    }
    if (OutLayout.RootApronWorldTransforms.Num() != RootFragmentCount ||
        OutLayout.LeafLitterWorldTransforms.Num() != LeafLitterCount)
    {
        OutLayout = FTRIADIstanaExploreV5DTreeRealismLayout();
        OutError = TEXT("The deterministic V5D tree transition census is incomplete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::ExtractSourceTreeTransforms(
    TArray<FTransform>& OutTrees,
    FString& OutError) const
{
    OutTrees.Reset();
    TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
    GetTreeComponents(V4LandscapeActor, Components);
    if (!V4LandscapeActor || V4LandscapeActor->GetWorld() != GetWorld() ||
        Components.Num() != SourceTreeComponentCount)
    {
        OutError = TEXT("V5D tree realism requires the exact V4 source actor in the same world.");
        return false;
    }
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (!AppendWorldTransforms(Component, OutTrees, OutError))
        {
            OutTrees.Reset();
            return false;
        }
    }
    if (OutTrees.Num() != SourceTreeCount)
    {
        OutError = FString::Printf(
            TEXT("V5D source tree census is %d/%d."),
            OutTrees.Num(),
            SourceTreeCount);
        OutTrees.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::PopulateOwnedTransitions(
    const FTRIADIstanaExploreV5DTreeRealismLayout& Layout,
    FString& OutError)
{
    if (!RootApronInstances || !LeafLitterInstances ||
        !SavedAssets.TreeBaseTransitionMesh ||
        !SavedAssets.TreeBaseTransitionMaterial ||
        Layout.SelectedSourceTreeIndices.Num() != SelectedNearTreeCount ||
        Layout.RootApronWorldTransforms.Num() != RootFragmentCount ||
        Layout.LeafLitterWorldTransforms.Num() != LeafLitterCount)
    {
        OutError = TEXT("The V5D owned tree-transition roster or layout is incomplete.");
        return false;
    }
    struct FPopulation
    {
        UHierarchicalInstancedStaticMeshComponent* Component;
        const TArray<FTransform>* Transforms;
    };
    const FPopulation Populations[] = {
        {RootApronInstances, &Layout.RootApronWorldTransforms},
        {LeafLitterInstances, &Layout.LeafLitterWorldTransforms}};
    for (const FPopulation& Population : Populations)
    {
        Population.Component->ClearInstances();
        Population.Component->SetStaticMesh(
            SavedAssets.TreeBaseTransitionMesh);
        Population.Component->EmptyOverrideMaterials();
        Population.Component->SetMaterial(
            0,
            SavedAssets.TreeBaseTransitionMaterial);
        Population.Component->AddInstances(
            *Population.Transforms,
            false,
            true,
            false);
        Population.Component->BuildTreeIfOutdated(true, true);
        // UE 5.5 can leave a superseded registration-time async job flagged
        // after this forced synchronous build has made the current tree ready.
        // IsTreeFullyBuilt is authoritative; the stale async job will be
        // discarded by ApplyBuildTreeAsync when the current tree is not stale.
        if (!Population.Component->IsTreeFullyBuilt())
        {
            OutError = TEXT("A V5D tree-transition HISM did not finish its synchronous cluster tree.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::ConfigureTreeRealism(
    ATRIADIstanaExploreV4LandscapeActor* InV4Landscape,
    ATRIADIstanaExploreV5BVisualActor* InV5BVisual,
    const FTRIADIstanaExploreV5DTreeRealismAssets& InAssets,
    FString& OutError)
{
    if (bConfigured || !GetWorld() ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !InV4Landscape || !InV5BVisual ||
        InV4Landscape->GetWorld() != GetWorld() ||
        InV5BVisual->GetWorld() != GetWorld() ||
        !ValidateAssetRoster(InAssets, InV4Landscape, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D tree realism requires a fresh identity actor and exact same-world V4/V5B sources.");
        }
        return false;
    }
    FString V4Report;
    FString V5BReport;
    if (!InV4Landscape->ValidateExploreV4Landscape(V4Report) ||
        !InV5BVisual->ValidateExploreV5BVisuals(V5BReport) ||
        InV4Landscape->Tags.Contains(RuntimeOverrideTag()))
    {
        OutError = TEXT("V5D tree realism requires cold-valid, non-overridden V4/V5B sources: ") +
            V4Report + TEXT(" ") + V5BReport;
        return false;
    }

    V4LandscapeActor = InV4Landscape;
    V5BVisualActor = InV5BVisual;
    SavedAssets = InAssets;
    TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
    GetTreeComponents(V4LandscapeActor, Components);
    SavedOriginalTreeMeshes.Reset();
    SavedSourceComponentInstanceCounts.Reset();
    for (int32 Index = 0; Index < Components.Num(); ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = Components[Index];
        if (!Component || !Component->GetStaticMesh() ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            Component->CanEverAffectNavigation() ||
            !Component->bOverrideMinLOD || Component->MinLOD != 1 ||
            Component->ForcedLodModel != 0)
        {
            OutError = FString::Printf(
                TEXT("V5D tree source component %d drifted before snapshot."),
                Index);
            return false;
        }
        SavedOriginalTreeMeshes.Add(Component->GetStaticMesh());
        SavedSourceComponentInstanceCounts.Add(Component->GetInstanceCount());
    }
    if (!ExtractSourceTreeTransforms(SavedSourceTreeWorldTransforms, OutError))
    {
        return false;
    }
    SavedExistingTreeBaseWorldTransforms.Reset();
    if (!V5BVisualActor->TreeBaseMulchInstances ||
        V5BVisualActor->TreeBaseMulchInstances->GetStaticMesh() !=
            SavedAssets.TreeBaseTransitionMesh ||
        !AppendWorldTransforms(
            V5BVisualActor->TreeBaseMulchInstances,
            SavedExistingTreeBaseWorldTransforms,
            OutError) ||
        SavedExistingTreeBaseWorldTransforms.Num() != ExistingTreeBaseCount)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact 64-instance V5B tree-base source is absent.");
        }
        return false;
    }

    FTRIADIstanaExploreV5DTreeRealismLayout Layout;
    if (!BuildDeterministicTreeBaseLayout(
            SavedSourceTreeWorldTransforms,
            SavedExistingTreeBaseWorldTransforms,
            Layout,
            OutError) ||
        !PopulateOwnedTransitions(Layout, OutError))
    {
        return false;
    }
    SavedSelectedSourceTreeIndices = Layout.SelectedSourceTreeIndices;
    SavedRootApronWorldTransforms = Layout.RootApronWorldTransforms;
    SavedLeafLitterWorldTransforms = Layout.LeafLitterWorldTransforms;
    bConfigured = true;

    FString Report;
    if (!ValidateTreeRealism(Report))
    {
        bConfigured = false;
        OutError = TEXT("V5D tree realism post-configuration validation failed: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::ValidateOwnedTransitions(
    FString& OutError) const
{
    struct FExpected
    {
        const UHierarchicalInstancedStaticMeshComponent* Component;
        const TArray<FTransform>* Transforms;
    };
    const FExpected Expected[] = {
        {RootApronInstances, &SavedRootApronWorldTransforms},
        {LeafLitterInstances, &SavedLeafLitterWorldTransforms}};
    for (const FExpected& Row : Expected)
    {
        if (!Row.Component || Row.Component->GetOwner() != this ||
            Row.Component->GetStaticMesh() !=
                SavedAssets.TreeBaseTransitionMesh ||
            Row.Component->GetMaterial(0) !=
                SavedAssets.TreeBaseTransitionMaterial ||
            Row.Component->GetInstanceCount() != Row.Transforms->Num() ||
            Row.Component->GetCollisionEnabled() !=
                ECollisionEnabled::NoCollision ||
            Row.Component->GetGenerateOverlapEvents() ||
            Row.Component->CanEverAffectNavigation() ||
            Row.Component->CastShadow || Row.Component->bCastContactShadow ||
            Row.Component->bAffectDistanceFieldLighting ||
            Row.Component->bEnableDensityScaling)
        {
            OutError = TEXT("A V5D tree-base transition lost its exact render-only component policy.");
            return false;
        }
        // HISM async/out-of-date flags are transient cluster-build state, not
        // serialized map truth. UE 5.5 can retain an older async job after a
        // forced synchronous build has made the current tree fully built; the
        // authoritative current-tree readiness predicate is IsTreeFullyBuilt.
        if (HasActorBegunPlay() && !Row.Component->IsTreeFullyBuilt())
        {
            OutError = TEXT("A begun-play V5D tree-base transition cluster tree is not render-ready.");
            return false;
        }
        for (int32 Index = 0; Index < Row.Transforms->Num(); ++Index)
        {
            FTransform Actual;
            if (!Row.Component->GetInstanceTransform(Index, Actual, true) ||
                !Actual.Equals((*Row.Transforms)[Index], 0.0001))
            {
                OutError = TEXT("A saved V5D tree-base transition transform changed.");
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::
    ApplyRuntimeLegacyTreeBaseApronSuppression(FString& OutError)
{
    // V5B's 64 grounding patches use a nominal 110--180 cm radius and retain
    // the source mound's full Z scale.  In deep shade their opaque material
    // reads as a black elliptical plate.  V5D changes only component
    // visibility in the PIE world; the exact source instances remain intact.
    UHierarchicalInstancedStaticMeshComponent* LegacyAprons =
        V5BVisualActor ? V5BVisualActor->TreeBaseMulchInstances.Get() : nullptr;
    TArray<FTransform> CurrentTransforms;
    if (bRuntimeLegacyTreeBaseApronSnapshotValid || !LegacyAprons ||
        LegacyAprons->GetStaticMesh() != SavedAssets.TreeBaseTransitionMesh ||
        LegacyAprons->GetInstanceCount() != ExistingTreeBaseCount ||
        LegacyAprons->GetNumMaterials() != 1 ||
        LegacyAprons->GetMaterial(0) !=
            V5BVisualActor->SavedAssetRoster.FormalBedVeneerMaterial ||
        LegacyAprons->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        LegacyAprons->GetGenerateOverlapEvents() ||
        LegacyAprons->CanEverAffectNavigation() ||
        !LegacyAprons->IsVisible() || LegacyAprons->bHiddenInGame ||
        !AppendWorldTransforms(LegacyAprons, CurrentTransforms, OutError) ||
        !TransformArraysEqual(
            CurrentTransforms,
            SavedExistingTreeBaseWorldTransforms))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact visible 64-instance V5B full tree-base apron source drifted before V5D suppression.");
        }
        return false;
    }

    bRuntimeLegacyTreeBaseApronWasVisible = LegacyAprons->IsVisible();
    bRuntimeLegacyTreeBaseApronWasHiddenInGame =
        LegacyAprons->bHiddenInGame;
    bRuntimeLegacyTreeBaseApronSnapshotValid = true;
    LegacyAprons->SetVisibility(false, false);
    LegacyAprons->MarkRenderStateDirty();

    if (!ValidateRuntimeLegacyTreeBaseApronSuppression(OutError))
    {
        RestoreRuntimeLegacyTreeBaseApronSuppression();
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::
    ValidateRuntimeLegacyTreeBaseApronSuppression(FString& OutError) const
{
    const UHierarchicalInstancedStaticMeshComponent* LegacyAprons =
        V5BVisualActor ? V5BVisualActor->TreeBaseMulchInstances.Get() : nullptr;
    TArray<FTransform> CurrentTransforms;
    if (!bRuntimeLegacyTreeBaseApronSnapshotValid || !LegacyAprons ||
        LegacyAprons->GetStaticMesh() != SavedAssets.TreeBaseTransitionMesh ||
        LegacyAprons->GetInstanceCount() != ExistingTreeBaseCount ||
        LegacyAprons->GetNumMaterials() != 1 ||
        LegacyAprons->GetMaterial(0) !=
            V5BVisualActor->SavedAssetRoster.FormalBedVeneerMaterial ||
        LegacyAprons->IsVisible() ||
        LegacyAprons->bHiddenInGame !=
            bRuntimeLegacyTreeBaseApronWasHiddenInGame ||
        LegacyAprons->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        LegacyAprons->GetGenerateOverlapEvents() ||
        LegacyAprons->CanEverAffectNavigation() ||
        !AppendWorldTransforms(LegacyAprons, CurrentTransforms, OutError) ||
        !TransformArraysEqual(
            CurrentTransforms,
            SavedExistingTreeBaseWorldTransforms))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The V5D legacy tree-base apron suppression lost visibility-only ownership or changed its exact 64 transforms.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DTreeRealismActor::
    RestoreRuntimeLegacyTreeBaseApronSuppression()
{
    UHierarchicalInstancedStaticMeshComponent* LegacyAprons =
        V5BVisualActor ? V5BVisualActor->TreeBaseMulchInstances.Get() : nullptr;
    if (bRuntimeLegacyTreeBaseApronSnapshotValid && LegacyAprons)
    {
        LegacyAprons->SetVisibility(
            bRuntimeLegacyTreeBaseApronWasVisible,
            false);
        LegacyAprons->SetHiddenInGame(
            bRuntimeLegacyTreeBaseApronWasHiddenInGame,
            false);
        LegacyAprons->MarkRenderStateDirty();
    }
    bRuntimeLegacyTreeBaseApronSnapshotValid = false;
    bRuntimeLegacyTreeBaseApronWasVisible = true;
    bRuntimeLegacyTreeBaseApronWasHiddenInGame = false;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::
    ValidateAppliedRuntimeOverrideForSourceActor(
        const ATRIADIstanaExploreV4LandscapeActor* Candidate,
        FString& OutReport) const
{
    TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
    GetTreeComponents(Candidate, Components);
    if (!bConfigured || !bRuntimeOverrideApplied || !Candidate ||
        Candidate != V4LandscapeActor || Candidate->GetWorld() != GetWorld() ||
        !Candidate->Tags.Contains(RuntimeOverrideTag()) ||
        Components.Num() != SourceTreeComponentCount ||
        SavedOriginalTreeMeshes.Num() != SourceTreeComponentCount ||
        SavedSourceComponentInstanceCounts.Num() != SourceTreeComponentCount ||
        RuntimeOriginalComponentMeshes.Num() != SourceTreeComponentCount ||
        RuntimeOriginalMinLods.Num() != SourceTreeComponentCount ||
        RuntimeOriginalLodDistanceScales.Num() != SourceTreeComponentCount ||
        RuntimeOriginalCastContactShadow.Num() != SourceTreeComponentCount ||
        RuntimeMaterialOffsets.Num() != SourceTreeComponentCount + 1 ||
        RuntimeTreeResponseMaterials.Num() !=
            RuntimeTreeResponseMaterialCount ||
        !ValidateRuntimeTreeMaterialResponse(OutReport) ||
        !ValidateRuntimeLegacyTreeBaseApronSuppression(OutReport))
    {
        OutReport = TEXT("V5D runtime tree override ownership, source, tag or snapshot roster is incomplete.");
        return false;
    }

    TArray<FTransform> CurrentTransforms;
    for (int32 ComponentIndex = 0;
         ComponentIndex < Components.Num();
         ++ComponentIndex)
    {
        const int32 FormIndex = ComponentIndex % TreeFormCount;
        UHierarchicalInstancedStaticMeshComponent* Component =
            Components[ComponentIndex];
        UStaticMesh* ExpectedMesh =
            SavedAssets.NearLodZeroTreeMeshes[FormIndex];
        FString MeshError;
        if (!Component)
        {
            OutReport = FString::Printf(
                TEXT("V5D runtime tree component %d is absent."),
                ComponentIndex);
            return false;
        }
        if (Component->GetStaticMesh() != ExpectedMesh)
        {
            OutReport = FString::Printf(
                TEXT("V5D runtime tree component %d has mesh %s instead of %s (worldBegun=%s mobility=%d)."),
                ComponentIndex,
                *GetNameSafe(Component->GetStaticMesh()),
                *GetNameSafe(ExpectedMesh),
                GetWorld() && GetWorld()->HasBegunPlay() ? TEXT("true") : TEXT("false"),
                static_cast<int32>(Component->Mobility));
            return false;
        }
        if (!MeshesShareGeometryAndSlots(
                Component->GetStaticMesh(),
                SavedOriginalTreeMeshes[ComponentIndex],
                FormIndex,
                MeshError))
        {
            OutReport = MeshError;
            return false;
        }
        if (
            Component->GetInstanceCount() !=
                SavedSourceComponentInstanceCounts[ComponentIndex] ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            Component->GetGenerateOverlapEvents() ||
            Component->CanEverAffectNavigation() ||
            !Component->IsVisible() || Component->bHiddenInGame ||
            !Component->bOverrideMinLOD ||
            Component->MinLOD != RuntimeTreeMinimumLod ||
            Component->ForcedLodModel != RuntimeTreeForcedLodModel ||
            !FMath::IsNearlyEqual(
             Component->InstanceLODDistanceScale,
             RuntimeTreeLodDistanceScale,
             0.0001f) ||
            RuntimeOriginalCastContactShadow[ComponentIndex] != 1u ||
            !Component->CastShadow || Component->bCastContactShadow ||
            !Component->IsTreeFullyBuilt())
        {
            OutReport = FString::Printf(
                TEXT("V5D runtime tree component %d changed count/LOD/shadow or simulation isolation."),
                ComponentIndex);
            return false;
        }
        if (!AppendWorldTransforms(Component, CurrentTransforms, OutReport))
        {
            return false;
        }
        const int32 MaterialStart = RuntimeMaterialOffsets[ComponentIndex];
        const int32 MaterialEnd = RuntimeMaterialOffsets[ComponentIndex + 1];
        if (MaterialEnd - MaterialStart != Component->GetNumMaterials())
        {
            OutReport = TEXT("V5D runtime tree material snapshot boundaries changed.");
            return false;
        }
        for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
        {
            if (Component->GetMaterial(Slot) !=
                RuntimeTreeResponseMaterials[MaterialStart + Slot])
            {
                OutReport = TEXT("A V5D tree swap displaced an isolated response MID.");
                return false;
            }
        }
    }
    if (!TransformArraysEqual(
            CurrentTransforms,
            SavedSourceTreeWorldTransforms))
    {
        OutReport = TEXT("V5D runtime tree transforms or their exact order changed.");
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_TREE_RUNTIME_OVERRIDE_VALID sourceTrees=729 sourceTransformsAndCensusUntouched=true isolatedTreeDerivatives=5 isolatedResponseMaterials=13 responseMids=26 runtimeMinimumLod=%d forcedLodModel=%d sourceLOD0AvailableNearCamera=true automaticScreenSizeLod=true runtimeLodDistanceScale=%.1f mediumRangeCrownDetailRetained=true allTreesForcedToLod0=false contactShadowSuppressed=true directTreeShadows=true sourceWindMidsSnapshotted=true sixWindParametersCachedAndPropagated=true windWpoPreserved=true legacyV5BFullApronsHiddenRenderOnly=true legacyV5BApronTransformsAndCensusUntouched=true collisionNavigationRfUntouched=true sourceAssetPackagesUntouched=true"),
        RuntimeTreeMinimumLod,
        RuntimeTreeForcedLodModel,
        RuntimeTreeLodDistanceScale);
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::
    ValidateActiveRuntimeOverrideForSourceActor(
        const ATRIADIstanaExploreV4LandscapeActor* Candidate,
        FString& OutReport)
{
    if (!Candidate || !Candidate->GetWorld())
    {
        OutReport = TEXT("V5D active runtime tree override source/world is absent.");
        return false;
    }
    int32 MatchCount = 0;
    const ATRIADIstanaExploreV5DTreeRealismActor* Match = nullptr;
    for (TActorIterator<ATRIADIstanaExploreV5DTreeRealismActor> It(
             Candidate->GetWorld());
         It;
         ++It)
    {
        if (It->V4LandscapeActor == Candidate && It->bRuntimeOverrideApplied)
        {
            ++MatchCount;
            Match = *It;
        }
    }
    if (MatchCount != 1 || !Match)
    {
        OutReport = FString::Printf(
            TEXT("V5D expected exactly one active tree override for the tagged V4 source; found %d."),
            MatchCount);
        return false;
    }
    return Match->ValidateAppliedRuntimeOverrideForSourceActor(
        Candidate,
        OutReport);
}

void ATRIADIstanaExploreV5DTreeRealismActor::
    ResetRuntimeTreeMaterialResponseState()
{
    RuntimeTreeResponseMaterials.Reset();
    RuntimeCachedWindScalarValues.Reset();
    RuntimeCachedWindDirectionValues.Reset();
}

bool ATRIADIstanaExploreV5DTreeRealismActor::
    CreateRuntimeTreeMaterialResponseInstances(FString& OutError)
{
    ResetRuntimeTreeMaterialResponseState();
    if (RuntimeOriginalMaterials.Num() != RuntimeTreeResponseMaterialCount ||
        RuntimeMaterialOffsets.Num() != SourceTreeComponentCount + 1 ||
        SavedAssets.NearLodZeroTreeMeshes.Num() != TreeFormCount)
    {
        OutError = TEXT("The exact 26-slot V4 wind snapshot or five-mesh V5D response roster is incomplete.");
        return false;
    }

    RuntimeTreeResponseMaterials.Reserve(RuntimeTreeResponseMaterialCount);
    for (int32 ComponentIndex = 0;
         ComponentIndex < SourceTreeComponentCount;
         ++ComponentIndex)
    {
        const int32 FormIndex = ComponentIndex % TreeFormCount;
        UStaticMesh* ResponseMesh =
            SavedAssets.NearLodZeroTreeMeshes[FormIndex];
        const int32 Start = RuntimeMaterialOffsets[ComponentIndex];
        const int32 End = RuntimeMaterialOffsets[ComponentIndex + 1];
        const int32 ExpectedSlotCount =
            TreeMaterialOffsets[FormIndex + 1] -
            TreeMaterialOffsets[FormIndex];
        if (!ResponseMesh || End - Start != ExpectedSlotCount ||
            ResponseMesh->GetStaticMaterials().Num() != ExpectedSlotCount)
        {
            OutError = FString::Printf(
                TEXT("V5D tree component %d lost its exact material-slot count."),
                ComponentIndex);
            ResetRuntimeTreeMaterialResponseState();
            return false;
        }
        for (int32 Slot = 0; Slot < ExpectedSlotCount; ++Slot)
        {
            const int32 MaterialIndex =
                FlattenedTreeMaterialIndex(FormIndex, Slot);
            UMaterialInterface* Original = RuntimeOriginalMaterials[Start + Slot];
            const UMaterialInstanceDynamic* OriginalMid =
                Cast<UMaterialInstanceDynamic>(Original);
            UMaterialInterface* ResponseBase = ResponseMesh->GetMaterial(Slot);
            if (MaterialIndex == INDEX_NONE || !OriginalMid ||
                !OriginalMid->Parent ||
                OriginalMid->Parent->GetPathName() !=
                    SourceTreeMaterialPaths[MaterialIndex] ||
                !ResponseBase || ResponseBase->GetPathName() !=
                    TreeResponseMaterialPaths[MaterialIndex])
            {
                OutError = FString::Printf(
                    TEXT("V5D tree component %d slot %d is not the exact V4-wind/V5D-response pair."),
                    ComponentIndex,
                    Slot);
                ResetRuntimeTreeMaterialResponseState();
                return false;
            }
            UMaterialInstanceDynamic* ResponseMid =
                UMaterialInstanceDynamic::Create(ResponseBase, this);
            if (!ResponseMid || ResponseMid->Parent != ResponseBase)
            {
                OutError = FString::Printf(
                    TEXT("V5D could not create isolated response MID %d/%d."),
                    ComponentIndex,
                    Slot);
                ResetRuntimeTreeMaterialResponseState();
                return false;
            }
            RuntimeTreeResponseMaterials.Add(ResponseMid);
        }
    }

    RuntimeCachedWindScalarValues.Init(
        MAX_flt,
        RuntimeTreeResponseMaterialCount * WindScalarParameterCount);
    RuntimeCachedWindDirectionValues.Init(
        FLinearColor(MAX_flt, MAX_flt, MAX_flt, MAX_flt),
        RuntimeTreeResponseMaterialCount);
    if (!SyncRuntimeTreeWindParameters(true, OutError))
    {
        ResetRuntimeTreeMaterialResponseState();
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::
    SyncRuntimeTreeWindParameters(
        bool bForceWrite,
        FString& OutError)
{
    if (RuntimeOriginalMaterials.Num() != RuntimeTreeResponseMaterialCount ||
        RuntimeTreeResponseMaterials.Num() !=
            RuntimeTreeResponseMaterialCount ||
        RuntimeCachedWindScalarValues.Num() !=
            RuntimeTreeResponseMaterialCount * WindScalarParameterCount ||
        RuntimeCachedWindDirectionValues.Num() !=
            RuntimeTreeResponseMaterialCount)
    {
        OutError = TEXT("The cached V5D tree wind-propagation roster is incomplete.");
        return false;
    }

    for (int32 MaterialIndex = 0;
         MaterialIndex < RuntimeTreeResponseMaterialCount;
         ++MaterialIndex)
    {
        const UMaterialInterface* Original =
            RuntimeOriginalMaterials[MaterialIndex];
        UMaterialInstanceDynamic* Response =
            RuntimeTreeResponseMaterials[MaterialIndex];
        if (!Original || !Response)
        {
            OutError = TEXT("A cached V5D source or response wind material is absent.");
            return false;
        }
        for (int32 ScalarIndex = 0;
             ScalarIndex < WindScalarParameterCount;
             ++ScalarIndex)
        {
            float SourceValue = 0.0f;
            if (!Original->GetScalarParameterValue(
                    FMaterialParameterInfo(
                        WindScalarParameters[ScalarIndex]),
                    SourceValue) ||
                !FMath::IsFinite(SourceValue))
            {
                OutError = FString::Printf(
                    TEXT("V5D source wind scalar %s is absent or non-finite."),
                    *WindScalarParameters[ScalarIndex].ToString());
                return false;
            }
            const int32 CacheIndex =
                MaterialIndex * WindScalarParameterCount + ScalarIndex;
            if (bForceWrite || !FMath::IsNearlyEqual(
                    RuntimeCachedWindScalarValues[CacheIndex],
                    SourceValue,
                    0.000001f))
            {
                Response->SetScalarParameterValue(
                    WindScalarParameters[ScalarIndex],
                    SourceValue);
                RuntimeCachedWindScalarValues[CacheIndex] = SourceValue;
            }
        }

        FLinearColor SourceDirection = FLinearColor::Black;
        if (!Original->GetVectorParameterValue(
                FMaterialParameterInfo(WindDirectionParameter),
                SourceDirection) ||
            !FMath::IsFinite(SourceDirection.R) ||
            !FMath::IsFinite(SourceDirection.G) ||
            !FMath::IsFinite(SourceDirection.B) ||
            !FMath::IsFinite(SourceDirection.A))
        {
            OutError = TEXT("The V5D source wind direction is absent or non-finite.");
            return false;
        }
        if (bForceWrite || !RuntimeCachedWindDirectionValues[MaterialIndex]
                .Equals(SourceDirection, 0.000001f))
        {
            Response->SetVectorParameterValue(
                WindDirectionParameter,
                SourceDirection);
            RuntimeCachedWindDirectionValues[MaterialIndex] = SourceDirection;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::
    ValidateRuntimeTreeMaterialResponse(FString& OutError) const
{
    if (RuntimeOriginalMaterials.Num() != RuntimeTreeResponseMaterialCount ||
        RuntimeTreeResponseMaterials.Num() !=
            RuntimeTreeResponseMaterialCount ||
        RuntimeCachedWindScalarValues.Num() !=
            RuntimeTreeResponseMaterialCount * WindScalarParameterCount ||
        RuntimeCachedWindDirectionValues.Num() !=
            RuntimeTreeResponseMaterialCount)
    {
        OutError = TEXT("The exact 26-MID V5D response/wind cache is incomplete.");
        return false;
    }
    for (int32 RuntimeIndex = 0;
         RuntimeIndex < RuntimeTreeResponseMaterialCount;
         ++RuntimeIndex)
    {
        const int32 ResponseMaterialIndex = RuntimeIndex % TreeMaterialCount;
        const UMaterialInterface* Original =
            RuntimeOriginalMaterials[RuntimeIndex];
        const UMaterialInstanceDynamic* OriginalMid =
            Cast<UMaterialInstanceDynamic>(Original);
        const UMaterialInstanceDynamic* Response =
            RuntimeTreeResponseMaterials[RuntimeIndex];
        if (!OriginalMid || !OriginalMid->Parent || !Response ||
            !Response->Parent ||
            OriginalMid->Parent->GetPathName() !=
                SourceTreeMaterialPaths[ResponseMaterialIndex] ||
            Response->Parent->GetPathName() !=
                TreeResponseMaterialPaths[ResponseMaterialIndex])
        {
            OutError = TEXT("A V5D response MID lost its exact source/derivative parent pair.");
            return false;
        }
        for (int32 ScalarIndex = 0;
             ScalarIndex < WindScalarParameterCount;
             ++ScalarIndex)
        {
            float SourceValue = 0.0f;
            float ResponseValue = 0.0f;
            const int32 CacheIndex =
                RuntimeIndex * WindScalarParameterCount + ScalarIndex;
            if (!Original->GetScalarParameterValue(
                    FMaterialParameterInfo(
                        WindScalarParameters[ScalarIndex]),
                    SourceValue) ||
                !Response->GetScalarParameterValue(
                    FMaterialParameterInfo(
                        WindScalarParameters[ScalarIndex]),
                    ResponseValue) ||
                !FMath::IsNearlyEqual(SourceValue, ResponseValue, 0.000001f) ||
                !FMath::IsNearlyEqual(
                    SourceValue,
                    RuntimeCachedWindScalarValues[CacheIndex],
                    0.000001f))
            {
                OutError = FString::Printf(
                    TEXT("V5D response wind scalar %s drifted from its snapshotted V4 MID."),
                    *WindScalarParameters[ScalarIndex].ToString());
                return false;
            }
        }
        FLinearColor SourceDirection = FLinearColor::Black;
        FLinearColor ResponseDirection = FLinearColor::Black;
        if (!Original->GetVectorParameterValue(
                FMaterialParameterInfo(WindDirectionParameter),
                SourceDirection) ||
            !Response->GetVectorParameterValue(
                FMaterialParameterInfo(WindDirectionParameter),
                ResponseDirection) ||
            !SourceDirection.Equals(ResponseDirection, 0.000001f) ||
            !SourceDirection.Equals(
                RuntimeCachedWindDirectionValues[RuntimeIndex],
                0.000001f))
        {
            OutError = TEXT("V5D response wind direction drifted from its snapshotted V4 MID.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DTreeRealismActor::
    ApplyRuntimeTreeVisualOverride(FString& OutError)
{
    UWorld* World = GetWorld();
    if (!bConfigured || bRuntimeOverrideApplied || !HasActorBegunPlay() ||
        !World || World->HasBegunPlay() ||
        !V4LandscapeActor || !V4LandscapeActor->HasActorBegunPlay() ||
        !V4LandscapeActor->IsWindRuntimeActive() ||
        V4LandscapeActor->Tags.Contains(RuntimeOverrideTag()))
    {
        OutError = TEXT("V5D tree runtime override requires one fresh, begun, wind-active V4 source before UWorld enters begun play.");
        return false;
    }
    FString V4Report;
    if (!V4LandscapeActor->ValidateExploreV4Landscape(V4Report))
    {
        OutError = TEXT("V5D tree runtime override refused an invalid cold V4 source: ") +
            V4Report;
        return false;
    }

    TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
    GetTreeComponents(V4LandscapeActor, Components);
    RuntimeOriginalComponentMeshes.Reset();
    RuntimeOriginalMinLods.Reset();
    RuntimeOriginalLodDistanceScales.Reset();
    RuntimeOriginalCastContactShadow.Reset();
    RuntimeMaterialOffsets.Reset();
    RuntimeOriginalMaterials.Reset();
    ResetRuntimeTreeMaterialResponseState();
    RuntimeMaterialOffsets.Add(0);
    for (int32 ComponentIndex = 0;
         ComponentIndex < Components.Num();
         ++ComponentIndex)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            Components[ComponentIndex];
        if (!Component ||
            Component->GetStaticMesh() !=
                SavedOriginalTreeMeshes[ComponentIndex] ||
            Component->GetInstanceCount() !=
                SavedSourceComponentInstanceCounts[ComponentIndex] ||
            !Component->bCastContactShadow)
        {
            OutError = TEXT("A V4 tree source changed after the V5D cold snapshot or lost its exact applied-V5 contact-shadow prestate.");
            RuntimeOriginalComponentMeshes.Reset();
            RuntimeOriginalMinLods.Reset();
            RuntimeOriginalLodDistanceScales.Reset();
            RuntimeOriginalCastContactShadow.Reset();
            RuntimeMaterialOffsets.Reset();
            RuntimeOriginalMaterials.Reset();
            ResetRuntimeTreeMaterialResponseState();
            return false;
        }
        RuntimeOriginalComponentMeshes.Add(Component->GetStaticMesh());
        RuntimeOriginalMinLods.Add(Component->MinLOD);
        RuntimeOriginalLodDistanceScales.Add(Component->InstanceLODDistanceScale);
        RuntimeOriginalCastContactShadow.Add(
            Component->bCastContactShadow ? 1u : 0u);
        for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
        {
            RuntimeOriginalMaterials.Add(Component->GetMaterial(Slot));
        }
        RuntimeMaterialOffsets.Add(RuntimeOriginalMaterials.Num());
    }

    if (!ApplyRuntimeLegacyTreeBaseApronSuppression(OutError))
    {
        const FString SuppressionError = OutError;
        RuntimeOriginalComponentMeshes.Reset();
        RuntimeOriginalMinLods.Reset();
        RuntimeOriginalLodDistanceScales.Reset();
        RuntimeOriginalCastContactShadow.Reset();
        RuntimeMaterialOffsets.Reset();
        RuntimeOriginalMaterials.Reset();
        ResetRuntimeTreeMaterialResponseState();
        OutError = TEXT("V5D could not suppress the inherited full tree-base aprons without changing their source state: ") +
            SuppressionError;
        return false;
    }

    if (!CreateRuntimeTreeMaterialResponseInstances(OutError))
    {
        const FString MaterialError = OutError;
        RestoreRuntimeLegacyTreeBaseApronSuppression();
        RuntimeOriginalComponentMeshes.Reset();
        RuntimeOriginalMinLods.Reset();
        RuntimeOriginalLodDistanceScales.Reset();
        RuntimeOriginalCastContactShadow.Reset();
        RuntimeMaterialOffsets.Reset();
        RuntimeOriginalMaterials.Reset();
        ResetRuntimeTreeMaterialResponseState();
        OutError = TEXT("V5D could not create exact cached tree response MIDs: ") +
            MaterialError;
        return false;
    }

    for (int32 ComponentIndex = 0;
         ComponentIndex < Components.Num();
         ++ComponentIndex)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            Components[ComponentIndex];
        const int32 FormIndex = ComponentIndex % TreeFormCount;
        UStaticMesh* ExpectedMesh =
            SavedAssets.NearLodZeroTreeMeshes[FormIndex];
        if (!Component->SetStaticMesh(ExpectedMesh))
        {
            OutError = FString::Printf(
                TEXT("V5D tree mesh swap %d was rejected: actual=%s expected=%s worldBegun=%s mobility=%d."),
                ComponentIndex,
                *GetNameSafe(Component->GetStaticMesh()),
                *GetNameSafe(ExpectedMesh),
                World->HasBegunPlay() ? TEXT("true") : TEXT("false"),
                static_cast<int32>(Component->Mobility));
            RestoreRuntimeTreeVisualOverride();
            return false;
        }
        const int32 MaterialStart = RuntimeMaterialOffsets[ComponentIndex];
        for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
        {
            Component->SetMaterial(
                Slot,
                RuntimeTreeResponseMaterials[MaterialStart + Slot]);
        }
        Component->bOverrideMinLOD = true;
        Component->MinLOD = RuntimeTreeMinimumLod;
        Component->ForcedLodModel = RuntimeTreeForcedLodModel;
        Component->InstanceLODDistanceScale = RuntimeTreeLodDistanceScale;
        Component->SetCastShadow(true);
        Component->SetCastContactShadow(false);
        // Validation below requires the replacement HISM's current tree to be
        // render-ready in this frame. UE can retain a superseded async job after
        // this synchronous build, so validation keys on IsTreeFullyBuilt only.
        Component->BuildTreeIfOutdated(false, true);
        Component->MarkRenderStateDirty();
    }
    V4LandscapeActor->Tags.AddUnique(RuntimeOverrideTag());
    bRuntimeOverrideApplied = true;

    FString Report;
    if (!ValidateAppliedRuntimeOverrideForSourceActor(
            V4LandscapeActor,
            Report))
    {
        RestoreRuntimeTreeVisualOverride();
        OutError = TEXT("V5D runtime tree override failed closed validation: ") +
            Report;
        return false;
    }
    SetActorTickEnabled(true);
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DTreeRealismActor::
    RestoreRuntimeTreeVisualOverride()
{
    SetActorTickEnabled(false);
    TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
    GetTreeComponents(V4LandscapeActor, Components);
    UWorld* World = GetWorld();
    if (World && World->HasBegunPlay() && World->bIsTearingDown)
    {
        // UWorld clears begun-play only after every actor has routed EndPlay.
        // The world lifecycle callback restores these Static components then.
        return;
    }
    TUniquePtr<FMultiComponentReregisterContext> ReregisterContext;
    if (World && World->HasBegunPlay())
    {
        TArray<UActorComponent*> ComponentsToReregister;
        ComponentsToReregister.Reserve(Components.Num());
        for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
        {
            if (Component)
            {
                ComponentsToReregister.Add(Component);
            }
        }
        ReregisterContext = MakeUnique<FMultiComponentReregisterContext>(
            ComponentsToReregister);
    }
    bool bRestoreSucceeded = true;
    if (Components.Num() == SourceTreeComponentCount &&
        RuntimeOriginalComponentMeshes.Num() == SourceTreeComponentCount &&
        RuntimeOriginalMinLods.Num() == SourceTreeComponentCount &&
        RuntimeOriginalLodDistanceScales.Num() == SourceTreeComponentCount &&
        RuntimeOriginalCastContactShadow.Num() == SourceTreeComponentCount &&
        RuntimeMaterialOffsets.Num() == SourceTreeComponentCount + 1)
    {
        for (int32 ComponentIndex = 0;
             ComponentIndex < Components.Num();
             ++ComponentIndex)
        {
            UHierarchicalInstancedStaticMeshComponent* Component =
                Components[ComponentIndex];
            if (!Component)
            {
                continue;
            }
            UStaticMesh* OriginalMesh =
                RuntimeOriginalComponentMeshes[ComponentIndex];
            if (Component->GetStaticMesh() != OriginalMesh &&
                !Component->SetStaticMesh(OriginalMesh))
            {
                UE_LOG(
                    LogTRIADIstanaExploreV5DTreeRealism,
                    Error,
                    TEXT("V5D tree mesh restore %d was rejected: actual=%s expected=%s worldBegun=%s mobility=%d."),
                    ComponentIndex,
                    *GetNameSafe(Component->GetStaticMesh()),
                    *GetNameSafe(OriginalMesh),
                    World && World->HasBegunPlay() ? TEXT("true") : TEXT("false"),
                    static_cast<int32>(Component->Mobility));
                bRestoreSucceeded = false;
                continue;
            }
            const int32 Start = RuntimeMaterialOffsets[ComponentIndex];
            const int32 End = RuntimeMaterialOffsets[ComponentIndex + 1];
            for (int32 Slot = 0;
                 Slot < End - Start && Slot < Component->GetNumMaterials();
                 ++Slot)
            {
                Component->SetMaterial(
                    Slot,
                    RuntimeOriginalMaterials[Start + Slot]);
            }
            Component->bOverrideMinLOD = true;
            Component->MinLOD = RuntimeOriginalMinLods[ComponentIndex];
            Component->ForcedLodModel = 0;
            Component->InstanceLODDistanceScale =
                RuntimeOriginalLodDistanceScales[ComponentIndex];
            Component->SetCastContactShadow(
                RuntimeOriginalCastContactShadow[ComponentIndex] != 0u);
            Component->BuildTreeIfOutdated(true, true);
            Component->MarkRenderStateDirty();
            bool bMaterialsRestored =
                End - Start == Component->GetNumMaterials();
            for (int32 Slot = 0;
                 bMaterialsRestored && Slot < Component->GetNumMaterials();
                 ++Slot)
            {
                bMaterialsRestored = Component->GetMaterial(Slot) ==
                    RuntimeOriginalMaterials[Start + Slot];
            }
            if (Component->GetStaticMesh() != OriginalMesh ||
                !bMaterialsRestored || !Component->bOverrideMinLOD ||
                Component->MinLOD != RuntimeOriginalMinLods[ComponentIndex] ||
                Component->ForcedLodModel != 0 ||
                !FMath::IsNearlyEqual(
                    Component->InstanceLODDistanceScale,
                    RuntimeOriginalLodDistanceScales[ComponentIndex],
                    0.0001f) ||
                Component->bCastContactShadow !=
                    (RuntimeOriginalCastContactShadow[ComponentIndex] != 0u))
            {
                UE_LOG(
                    LogTRIADIstanaExploreV5DTreeRealism,
                    Error,
                    TEXT("V5D tree restore %d failed its exact mesh/material/MinLOD/distance-scale/contact-shadow assertions."),
                    ComponentIndex);
                bRestoreSucceeded = false;
            }
        }
    }
    if (!bRestoreSucceeded)
    {
        return;
    }
    if (V4LandscapeActor)
    {
        V4LandscapeActor->Tags.Remove(RuntimeOverrideTag());
    }
    RestoreRuntimeLegacyTreeBaseApronSuppression();
    bRuntimeOverrideApplied = false;
    RuntimeOriginalComponentMeshes.Reset();
    RuntimeOriginalMinLods.Reset();
    RuntimeOriginalLodDistanceScales.Reset();
    RuntimeOriginalCastContactShadow.Reset();
    RuntimeMaterialOffsets.Reset();
    RuntimeOriginalMaterials.Reset();
    ResetRuntimeTreeMaterialResponseState();
}

bool ATRIADIstanaExploreV5DTreeRealismActor::ValidateTreeRealism(
    FString& OutReport) const
{
    FString Error;
    if (!bConfigured || !GetWorld() ||
        !Tags.Contains(ExpectedActorTag()) ||
        ClaimLabel != ExpectedClaimLabel() || !bAppearanceOnly ||
        bSourceTransformsOrCensusModified ||
        bBlockerCollisionNavigationOrRfAuthorityModified ||
        bSourceAssetPackagesModified ||
        bBotanicalSurveyOrCurrentSeasonClaimed ||
        !bContactShadowSuppressionIsVisualLookdevOnly ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !V4LandscapeActor || !V5BVisualActor ||
        V4LandscapeActor->GetWorld() != GetWorld() ||
        V5BVisualActor->GetWorld() != GetWorld() ||
        SavedOriginalTreeMeshes.Num() != SourceTreeComponentCount ||
        SavedSourceComponentInstanceCounts.Num() != SourceTreeComponentCount ||
        SavedSourceTreeWorldTransforms.Num() != SourceTreeCount ||
        SavedExistingTreeBaseWorldTransforms.Num() != ExistingTreeBaseCount ||
        SavedSelectedSourceTreeIndices.Num() != SelectedNearTreeCount ||
        SavedRootApronWorldTransforms.Num() != RootFragmentCount ||
        SavedLeafLitterWorldTransforms.Num() != LeafLitterCount ||
        !ValidateOwnedTransitions(Error))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("V5D tree realism truth, ownership, census or identity state drifted.")
            : Error;
        return false;
    }

    FTRIADIstanaExploreV5DTreeRealismLayout Rebuilt;
    if (!BuildDeterministicTreeBaseLayout(
            SavedSourceTreeWorldTransforms,
            SavedExistingTreeBaseWorldTransforms,
            Rebuilt,
            Error) ||
        Rebuilt.SelectedSourceTreeIndices != SavedSelectedSourceTreeIndices ||
        !TransformArraysEqual(
            Rebuilt.RootApronWorldTransforms,
            SavedRootApronWorldTransforms) ||
        !TransformArraysEqual(
            Rebuilt.LeafLitterWorldTransforms,
            SavedLeafLitterWorldTransforms))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("Saved V5D tree transitions no longer equal the pure deterministic rebuild.")
            : Error;
        return false;
    }

    if (HasActorBegunPlay())
    {
        if (!ValidateAppliedRuntimeOverrideForSourceActor(
                V4LandscapeActor,
                Error))
        {
            OutReport = Error;
            return false;
        }
    }
    else
    {
        if (!ValidateAssetRoster(SavedAssets, V4LandscapeActor, Error))
        {
            OutReport = TEXT("The cold V5D tree derivative asset roster failed closed: ") +
                Error;
            return false;
        }

        TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
        GetTreeComponents(V4LandscapeActor, Components);
        TArray<FTransform> CurrentTransforms;
        for (int32 Index = 0; Index < Components.Num(); ++Index)
        {
            UHierarchicalInstancedStaticMeshComponent* Component =
                Components[Index];
            if (!Component ||
                Component->GetStaticMesh() != SavedOriginalTreeMeshes[Index] ||
                Component->GetInstanceCount() !=
                    SavedSourceComponentInstanceCounts[Index] ||
                Component->MinLOD != 1 || Component->ForcedLodModel != 0 ||
                !AppendWorldTransforms(Component, CurrentTransforms, Error))
            {
                OutReport = Error.IsEmpty()
                    ? TEXT("The cold V4 tree source changed before the runtime-only V5D swap.")
                    : Error;
                return false;
            }
        }
        TArray<FTransform> CurrentExistingTreeBaseTransforms;
        if (bRuntimeOverrideApplied ||
            bRuntimeLegacyTreeBaseApronSnapshotValid ||
            V4LandscapeActor->Tags.Contains(RuntimeOverrideTag()) ||
            !V5BVisualActor->TreeBaseMulchInstances ||
            !V5BVisualActor->TreeBaseMulchInstances->IsVisible() ||
            V5BVisualActor->TreeBaseMulchInstances->bHiddenInGame ||
            !AppendWorldTransforms(
                V5BVisualActor->TreeBaseMulchInstances,
                CurrentExistingTreeBaseTransforms,
                Error) ||
            !TransformArraysEqual(
                CurrentExistingTreeBaseTransforms,
                SavedExistingTreeBaseWorldTransforms) ||
            !TransformArraysEqual(
                CurrentTransforms,
                SavedSourceTreeWorldTransforms))
        {
            OutReport = TEXT("A runtime V5D tree override leaked into the cold editor state.");
            return false;
        }
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_TREE_REALISM_VALID sourceTrees=%d exactSourceTransformsAndCensusUntouched=true sourceTreeMeshesUntouched=true isolatedTreeDerivatives=%d isolatedTreeResponseMaterials=13 runtimeResponseMids=26 runtimeMinimumLod=%d forcedLodModel=%d sourceLOD0AvailableNearCamera=true automaticScreenSizeLod=true runtimeLodDistanceScale=%.1f mediumRangeCrownDetailRetained=true allTreesForcedToLod0=false contactShadowInteriorCrushReducedAtRuntime=true directTreeShadowsPreserved=true sourceWindMidsSnapshottedAndRestored=true sixWindParametersCachedAndPropagatedAfterV4Tick=true windWpoCodePreserved=true opacityMaskAndClipPreserved=true inheritedFullApronsHiddenAtRuntime=true inheritedApronTransformsAndCensusUntouched=true selectedNearTrees=%d rootFragmentsPerTree=%d deterministicRootFragments=%d rootFragmentsPairwiseNonCoincident=true rootFragmentOffsetCm=18..62 rootFragmentMajorScale=0.38..0.58 rootFragmentMinorFraction=0.10..0.22 rootFragmentZFraction=0.10 rootFragmentZOffsetCm=-0.20 leafLitterFragmentsPerTree=%d deterministicFragmentedLeafLitter=%d edgeBlendedRootTransitionMaterial=true sourceBlockersCollisionNavigationRfUntouched=true renderOnlyTransitions=true isolatedPerInstanceCanopyAndBarkResponse=true sourceMaterialMutation=false survey=false botanicalInventory=false currentSeasonClaim=false"),
        SourceTreeCount,
        TreeFormCount,
        RuntimeTreeMinimumLod,
        RuntimeTreeForcedLodModel,
        RuntimeTreeLodDistanceScale,
        SelectedNearTreeCount,
        RootFragmentsPerTree,
        RootFragmentCount,
        LeafLitterPerTree,
        LeafLitterCount);
    return true;
}

void ATRIADIstanaExploreV5DTreeRealismActor::BeginPlay()
{
    Super::BeginPlay();
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         {RootApronInstances.Get(), LeafLitterInstances.Get()})
    {
        if (Component)
        {
            Component->BuildTreeIfOutdated(false, true);
            Component->SetVisibility(false, true);
        }
    }

    UWorld* World = GetWorld();
    if (!World || !V4LandscapeActor)
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DTreeRealism,
            Error,
            TEXT("V5D tree realism runtime failed closed: startup world or V4 source is absent."));
        return;
    }

    // V4 remains the sole gust-state authority. This prerequisite makes the
    // cached response-MID copy observe V4's values from the same frame.
    AddTickPrerequisiteActor(V4LandscapeActor);
    SetActorTickEnabled(false);

    WorldLifecycleHandle = World->GetOnBeginPlayEvent().AddUObject(
        this,
        &ATRIADIstanaExploreV5DTreeRealismActor::
            HandleWorldBegunPlayStateChanged);

    // V4 constructs its wind MIDs in BeginPlay. Apply immediately when V4 was
    // dispatched first, or consume its one-shot readiness signal when V5D was
    // dispatched first. Both paths run before UWorld enters begun play, which
    // is the required mutation window for registered Static components.
    if (V4LandscapeActor->IsWindRuntimeActive())
    {
        ApplyRuntimeTreeVisualOverrideAtStartup();
        return;
    }
    V4WindRuntimeReadyHandle =
        V4LandscapeActor->OnWindRuntimeReady().AddUObject(
            this,
            &ATRIADIstanaExploreV5DTreeRealismActor::
                HandleV4WindRuntimeReady);
}

void ATRIADIstanaExploreV5DTreeRealismActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bRuntimeOverrideApplied)
    {
        return;
    }
    FString Error;
    if (!V4LandscapeActor || !V4LandscapeActor->IsWindRuntimeActive() ||
        !SyncRuntimeTreeWindParameters(false, Error))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DTreeRealism,
            Error,
            TEXT("V5D tree material response failed closed during cached wind propagation: %s"),
            *Error);
        RestoreRuntimeTreeVisualOverride();
    }
}

void ATRIADIstanaExploreV5DTreeRealismActor::
    HandleV4WindRuntimeReady()
{
    ApplyRuntimeTreeVisualOverrideAtStartup();
}

void ATRIADIstanaExploreV5DTreeRealismActor::
    ApplyRuntimeTreeVisualOverrideAtStartup()
{
    RemoveV4WindRuntimeReadyBinding();
    FString Error;
    FString Report;
    const bool bOverrideApplied = ApplyRuntimeTreeVisualOverride(Error);
    if (bOverrideApplied)
    {
        if (RootApronInstances)
        {
            RootApronInstances->SetVisibility(true, true);
        }
        if (LeafLitterInstances)
        {
            LeafLitterInstances->SetVisibility(true, true);
        }
    }
    if (!bOverrideApplied || !ValidateTreeRealism(Report))
    {
        RestoreRuntimeTreeVisualOverride();
        if (RootApronInstances)
        {
            RootApronInstances->SetVisibility(false, true);
        }
        if (LeafLitterInstances)
        {
            LeafLitterInstances->SetVisibility(false, true);
        }
        RemoveWorldLifecycleBinding();
        UE_LOG(
            LogTRIADIstanaExploreV5DTreeRealism,
            Error,
            TEXT("V5D tree realism runtime failed closed: %s %s"),
            *Error,
            *Report);
        return;
    }
    UE_LOG(
        LogTRIADIstanaExploreV5DTreeRealism,
        Display,
        TEXT("%s"),
        *Report);
}

void ATRIADIstanaExploreV5DTreeRealismActor::
    HandleWorldBegunPlayStateChanged(bool bHasBegunPlay)
{
    if (bHasBegunPlay)
    {
        if (!bRuntimeOverrideApplied && V4WindRuntimeReadyHandle.IsValid())
        {
            RemoveV4WindRuntimeReadyBinding();
            RemoveWorldLifecycleBinding();
            UE_LOG(
                LogTRIADIstanaExploreV5DTreeRealism,
                Error,
                TEXT("V5D tree realism runtime failed closed: V4 wind readiness did not arrive before UWorld entered begun play."));
        }
        return;
    }

    RestoreRuntimeTreeVisualOverride();
    RemoveV4WindRuntimeReadyBinding();
    RemoveWorldLifecycleBinding();
}

void ATRIADIstanaExploreV5DTreeRealismActor::
    RemoveV4WindRuntimeReadyBinding()
{
    if (V4LandscapeActor && V4WindRuntimeReadyHandle.IsValid())
    {
        V4LandscapeActor->OnWindRuntimeReady().Remove(
            V4WindRuntimeReadyHandle);
    }
    V4WindRuntimeReadyHandle.Reset();
}

void ATRIADIstanaExploreV5DTreeRealismActor::
    RemoveWorldLifecycleBinding()
{
    if (UWorld* World = GetWorld();
        World && WorldLifecycleHandle.IsValid())
    {
        World->GetOnBeginPlayEvent().Remove(WorldLifecycleHandle);
    }
    WorldLifecycleHandle.Reset();
}

void ATRIADIstanaExploreV5DTreeRealismActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    RemoveV4WindRuntimeReadyBinding();
    UWorld* World = GetWorld();
    const bool bRestoreAtWorldFalseEdge =
        World && World->HasBegunPlay() && World->bIsTearingDown &&
        WorldLifecycleHandle.IsValid();
    if (!bRestoreAtWorldFalseEdge)
    {
        RestoreRuntimeTreeVisualOverride();
        RemoveWorldLifecycleBinding();
    }
    if (V4LandscapeActor)
    {
        RemoveTickPrerequisiteActor(V4LandscapeActor);
    }
    Super::EndPlay(EndPlayReason);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DTreeRealismDeterministicTransitionsTest,
    "TRIAD.Istana.ExploreV5D.TreeRealism.DeterministicTransitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DTreeRealismDeterministicTransitionsTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TArray<FTransform> Trees;
    Trees.Reserve(SourceTreeCount);
    for (int32 Y = 0; Y < 27; ++Y)
    {
        for (int32 X = 0; X < 27; ++X)
        {
            Trees.Add(FTransform(
                FRotator(
                    0.0,
                    static_cast<double>((X * 31 + Y * 13) % 360),
                    0.0),
                FVector(
                    -26000.0 + static_cast<double>(X) * 2000.0,
                    -9000.0 + static_cast<double>(Y) * 1600.0,
                    39.0),
                FVector::OneVector));
        }
    }
    TArray<FTransform> ExistingBases;
    ExistingBases.Reserve(ExistingTreeBaseCount);
    for (int32 Index = 0; Index < ExistingTreeBaseCount; ++Index)
    {
        ExistingBases.Add(FTransform(
            FRotator::ZeroRotator,
            FVector(
                -24000.0 + static_cast<double>(Index % 16) * 3200.0,
                -7000.0 + static_cast<double>(Index / 16) * 9600.0,
                39.2),
            FVector::OneVector));
    }
    const TArray<FTransform> SourceTreesBeforeBuild = Trees;
    const TArray<FTransform> ExistingBasesBeforeBuild = ExistingBases;
    FTRIADIstanaExploreV5DTreeRealismLayout A;
    FTRIADIstanaExploreV5DTreeRealismLayout B;
    FString Error;
    TestTrue(
        TEXT("First deterministic V5D tree layout builds"),
        ATRIADIstanaExploreV5DTreeRealismActor::
            BuildDeterministicTreeBaseLayout(Trees, ExistingBases, A, Error));
    TestTrue(
        TEXT("Repeated deterministic V5D tree layout builds"),
        ATRIADIstanaExploreV5DTreeRealismActor::
            BuildDeterministicTreeBaseLayout(Trees, ExistingBases, B, Error));
    TestEqual(
        TEXT("Exact selected near-tree count"),
        A.SelectedSourceTreeIndices.Num(),
        SelectedNearTreeCount);
    TestEqual(
        TEXT("Exact root-apron count"),
        A.RootApronWorldTransforms.Num(),
        RootFragmentCount);
    TestEqual(
        TEXT("Exact leaf-litter count"),
        A.LeafLitterWorldTransforms.Num(),
        LeafLitterCount);
    TestTrue(
        TEXT("Pure layout builds preserve both source transform censuses exactly"),
        Trees.Num() == SourceTreeCount &&
            ExistingBases.Num() == ExistingTreeBaseCount &&
            TransformArraysEqual(Trees, SourceTreesBeforeBuild) &&
            TransformArraysEqual(ExistingBases, ExistingBasesBeforeBuild));
    TestTrue(
        TEXT("Tree selection order is deterministic"),
        A.SelectedSourceTreeIndices == B.SelectedSourceTreeIndices);
    TestTrue(
        TEXT("Root and litter transforms are deterministic"),
        TransformArraysEqual(
            A.RootApronWorldTransforms,
            B.RootApronWorldTransforms) &&
            TransformArraysEqual(
                A.LeafLitterWorldTransforms,
                B.LeafLitterWorldTransforms));
    bool bExistingBasesClear = true;
    for (const FTransform& Root : A.RootApronWorldTransforms)
    {
        bExistingBasesClear = bExistingBasesClear &&
            !IsNearAny(
                Root.GetTranslation(),
                ExistingBases,
                ExistingBaseClearanceCm);
    }
    TestTrue(
        TEXT("New root aprons exclude all 64 existing V5B tree bases"),
        bExistingBasesClear);

    TArray<FTransform> BoundaryTrees;
    BoundaryTrees.Reserve(SourceTreeCount);
    for (int32 Index = 0; Index < SourceTreeCount; ++Index)
    {
        BoundaryTrees.Add(FTransform(
            FRotator::ZeroRotator,
            FVector(
                40000.0 + static_cast<double>(Index % 27) * 2000.0,
                -9000.0 + static_cast<double>(Index / 27) * 1600.0,
                39.0),
            FVector::OneVector));
    }
    BoundaryTrees[0].SetTranslation(FVector(0.0, 9500.0, 39.0));
    TArray<FTransform> BoundaryBases;
    BoundaryBases.Reserve(ExistingTreeBaseCount);
    for (int32 Index = 0; Index < ExistingTreeBaseCount; ++Index)
    {
        BoundaryBases.Add(FTransform(
            FRotator::ZeroRotator,
            FVector(
                -100000.0 - static_cast<double>(Index) * 1000.0,
                -100000.0,
                39.2),
            FVector::OneVector));
    }
    // The tempting focus tree is 470 cm from this base: outside the final
    // 430 cm fragment clearance but inside 430 + the maximum 62 cm offset.
    BoundaryBases[0].SetTranslation(FVector(470.0, 9500.0, 39.2));
    FTRIADIstanaExploreV5DTreeRealismLayout BoundaryLayout;
    TestTrue(
        TEXT("Conservative fragment-boundary layout still fills 128 selected trees and 384 roots"),
        ATRIADIstanaExploreV5DTreeRealismActor::
            BuildDeterministicTreeBaseLayout(
                BoundaryTrees,
                BoundaryBases,
                BoundaryLayout,
                Error));
    TestFalse(
        TEXT("A source only 470 cm from a legacy base is rejected before offset"),
        BoundaryLayout.SelectedSourceTreeIndices.Contains(0));
    bool bBoundaryLayoutClear = true;
    for (const FTransform& Root : BoundaryLayout.RootApronWorldTransforms)
    {
        bBoundaryLayoutClear = bBoundaryLayoutClear &&
            !IsNearAny(
                Root.GetTranslation(),
                BoundaryBases,
                ExistingBaseClearanceCm);
    }
    TestTrue(
        TEXT("Every adversarial-layout fragment retains the 430 cm clearance"),
        bBoundaryLayoutClear);

    bool bRootFragmentsNarrowAndScattered = true;
    bool bRootFragmentsPairwiseNonCoincidentAndVaried = true;
    bool bLitterFragmentsNarrowAndScattered = true;
    bool bLitterFragmentsPairwiseNonCoincident = true;
    for (int32 Ordinal = 0;
         Ordinal < A.SelectedSourceTreeIndices.Num();
         ++Ordinal)
    {
        const FVector SourceLocation =
            Trees[A.SelectedSourceTreeIndices[Ordinal]].GetTranslation();
        for (int32 RootFragmentIndex = 0;
             RootFragmentIndex < RootFragmentsPerTree;
             ++RootFragmentIndex)
        {
            const int32 RootIndex =
                Ordinal * RootFragmentsPerTree + RootFragmentIndex;
            const FTransform& Root = A.RootApronWorldTransforms[RootIndex];
            const FVector RootScale = Root.GetScale3D();
            const FVector RootOffset = Root.GetTranslation() - SourceLocation;
            const double RootRadius = FMath::Sqrt(
                DistanceSquared2D(Root.GetTranslation(), SourceLocation));
            bRootFragmentsNarrowAndScattered =
                bRootFragmentsNarrowAndScattered &&
                RootScale.X >= RootApronScaleMin - 0.0001 &&
                RootScale.X <= RootApronScaleMax + 0.0001 &&
                RootScale.Y / RootScale.X >= 0.10 - 0.0001 &&
                RootScale.Y / RootScale.X <= 0.22 + 0.0001 &&
                FMath::IsNearlyEqual(
                    RootScale.Z / RootScale.X,
                    0.10,
                    0.0001) &&
                FMath::IsNearlyEqual(
                    RootOffset.Z,
                    RootApronZOffsetCm,
                    0.0001) &&
                RootRadius >= RootFragmentOffsetMinCm - 0.0001 &&
                RootRadius <= RootFragmentOffsetMaxCm + 0.0001;
            for (int32 PreviousFragmentIndex = 0;
                 PreviousFragmentIndex < RootFragmentIndex;
                 ++PreviousFragmentIndex)
            {
                const FTransform& Previous = A.RootApronWorldTransforms[
                    Ordinal * RootFragmentsPerTree + PreviousFragmentIndex];
                const FVector PreviousScale = Previous.GetScale3D();
                const FVector PreviousOffset =
                    Previous.GetTranslation() - SourceLocation;
                const double PreviousRadius = FMath::Sqrt(
                    DistanceSquared2D(
                        Previous.GetTranslation(),
                        SourceLocation));
                const double RootAngleDegrees = FMath::RadiansToDegrees(
                    FMath::Atan2(RootOffset.Y, RootOffset.X));
                const double PreviousAngleDegrees = FMath::RadiansToDegrees(
                    FMath::Atan2(PreviousOffset.Y, PreviousOffset.X));
                bRootFragmentsPairwiseNonCoincidentAndVaried =
                    bRootFragmentsPairwiseNonCoincidentAndVaried &&
                    DistanceSquared2D(
                        Root.GetTranslation(),
                        Previous.GetTranslation()) > 1.0 &&
                    FMath::Abs(RootRadius - PreviousRadius) > 1.0 &&
                    FMath::Abs(RootScale.X - PreviousScale.X) > 0.001 &&
                    FMath::Abs(FMath::FindDeltaAngleDegrees(
                        RootAngleDegrees,
                        PreviousAngleDegrees)) > 45.0;
            }
        }
        for (int32 LitterIndex = 0;
             LitterIndex < LeafLitterPerTree;
             ++LitterIndex)
        {
            const FTransform& Litter = A.LeafLitterWorldTransforms[
                Ordinal * LeafLitterPerTree + LitterIndex];
            const FVector LitterScale = Litter.GetScale3D();
            const double LitterRadius = FMath::Sqrt(
                DistanceSquared2D(
                    Litter.GetTranslation(),
                    SourceLocation));
            bLitterFragmentsNarrowAndScattered =
                bLitterFragmentsNarrowAndScattered &&
                LitterScale.X >= LeafLitterScaleMin - 0.0001 &&
                LitterScale.X <= LeafLitterScaleMax + 0.0001 &&
                LitterScale.Y / LitterScale.X >= 0.22 - 0.0001 &&
                LitterScale.Y / LitterScale.X <= 0.52 + 0.0001 &&
                FMath::IsNearlyEqual(
                    LitterScale.Z / LitterScale.X,
                    0.24,
                    0.0001) &&
                LitterRadius >= 45.0 - 0.0001 &&
                LitterRadius <= 240.0 + 0.0001;
            for (int32 PreviousLitterIndex = 0;
                 PreviousLitterIndex < LitterIndex;
                 ++PreviousLitterIndex)
            {
                const FTransform& Previous = A.LeafLitterWorldTransforms[
                    Ordinal * LeafLitterPerTree + PreviousLitterIndex];
                bLitterFragmentsPairwiseNonCoincident =
                    bLitterFragmentsPairwiseNonCoincident &&
                    DistanceSquared2D(
                        Litter.GetTranslation(),
                        Previous.GetTranslation()) > 1.0;
            }
        }
    }
    TestTrue(
        TEXT("Root fragments are long, narrow, shallow and anchored at trunk bases"),
        bRootFragmentsNarrowAndScattered);
    TestTrue(
        TEXT("Each selected tree owns three non-coincident root fragments with varied angle, radius and scale"),
        bRootFragmentsPairwiseNonCoincidentAndVaried);
    TestTrue(
        TEXT("Litter fragments are narrow, flat and independently scattered"),
        bLitterFragmentsNarrowAndScattered);
    TestTrue(
        TEXT("Each selected tree owns eight pairwise non-coincident leaf-litter fragments"),
        bLitterFragmentsPairwiseNonCoincident);
    return true;
}
#endif
