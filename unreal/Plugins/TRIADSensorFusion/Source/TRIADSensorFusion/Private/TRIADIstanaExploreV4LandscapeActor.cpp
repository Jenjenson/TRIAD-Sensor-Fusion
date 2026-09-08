#include "TRIADIstanaExploreV4LandscapeActor.h"

#include "Algo/AnyOf.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"

UTRIADIstanaExploreV4SynchronousHismComponent::
    UTRIADIstanaExploreV4SynchronousHismComponent(
        const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bEnableDensityScaling = false;
    CurrentDensityScaling = 1.0f;
#if WITH_EDITOR
    bCanEnableDensityScaling = false;
#endif
}

void UTRIADIstanaExploreV4SynchronousHismComponent::
    OnPostLoadPerInstanceData()
{
    // This deliberately mirrors UE 5.5 HISM post-load bookkeeping without
    // invoking either asynchronous HISM post-load or the ISM implementation.
    bEnableDensityScaling = false;
    CurrentDensityScaling = 1.0f;
#if WITH_EDITOR
    bCanEnableDensityScaling = false;
#endif

    // A compiling mesh is finalized synchronously by the inherited
    // UHierarchicalInstancedStaticMeshComponent::PostStaticMeshCompilation.
    if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || IsCompiling())
    {
        return;
    }

    bool bForceTreeBuild = false;
#if WITH_EDITOR
    bForceTreeBuild = Algo::AnyOf(
        InstanceReorderTable,
        [this](int32 ReorderIndex)
        {
            return !PerInstanceSMData.IsValidIndex(ReorderIndex);
        });
#endif

    if (!bForceTreeBuild && PerInstanceSMData.Num() > 0 &&
        PerInstanceSMData.Num() != InstanceReorderTable.Num())
    {
        bForceTreeBuild = true;
    }

    if (!bForceTreeBuild)
    {
        if (AActor* Owner = GetOwner())
        {
            if (ULevel* OwnerLevel = Owner->GetLevel())
            {
                UWorld* OwnerWorld = OwnerLevel->OwningWorld;
                if (OwnerWorld && OwnerWorld->GetActiveLightingScenario() &&
                    OwnerWorld->GetActiveLightingScenario() != OwnerLevel)
                {
                    bForceTreeBuild = true;
                }
            }
        }
    }

    if (!bForceTreeBuild)
    {
        NumBuiltRenderInstances = NumBuiltInstances;
        InstanceCountToRender = NumBuiltInstances;
    }

    BuildTreeIfOutdated(false, bForceTreeBuild);
}

namespace
{
constexpr double CentimetersPerMeter = 100.0;
constexpr int32 ExpectedInheritedTreeCount = 720;
constexpr int32 FrozenV2ColumnarStartIndex = 272;
constexpr int32 FrozenV2ColumnarCount = 232;
constexpr int32 FrozenV2ColumnarEndIndex =
    FrozenV2ColumnarStartIndex + FrozenV2ColumnarCount;
constexpr int32 ExpectedHeritageAnchorCount = 9;
constexpr int32 ExpectedShrubCount = 512;
constexpr int32 ExpectedFlowerCount = 192;
constexpr int32 ExpectedUnderstoreyCount = 384;
constexpr int32 ExpectedGeometryGrassCount = 1536;
constexpr int32 ExpectedCloseTurfCount = 18432;
constexpr int32 ExpectedHismComponentCount = 16;
constexpr int32 ExpectedVisibleHismComponentCount = 15;
constexpr int32 ExpectedHiddenHismBlockerCount = 1;
constexpr int32 ExpectedNonEmptyHismComponentCount = 13;
constexpr int32 ExpectedTotalHismInstanceCount =
    ExpectedInheritedTreeCount + ExpectedHeritageAnchorCount * 2 +
    ExpectedShrubCount + ExpectedFlowerCount + ExpectedUnderstoreyCount +
    ExpectedGeometryGrassCount + ExpectedCloseTurfCount;
constexpr int32 ExpectedVisibleHismInstanceCount =
    ExpectedTotalHismInstanceCount - ExpectedHeritageAnchorCount;
constexpr int32 ExpectedPorticoTriangleCount = 6592;
constexpr int32 ExpectedPorticoMaterialCount = 7;
constexpr int32 ExpectedPorticoV5CTriangleCount = 13772;
constexpr int32 ExpectedPorticoV5CMaterialCount = 8;
constexpr int32 ExpectedWindBindingCount =
    static_cast<int32>(ETRIADIstanaExploreV4WindRole::Count);
constexpr int32 ExpectedTreeWindBindingCount = 13;
constexpr int32 ExpectedRuntimeWindMidCount =
    ExpectedWindBindingCount + ExpectedTreeWindBindingCount;
constexpr float HeritageBlockerHeightCm = 240.0f;
const FString V4RuntimeObjectRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/"));
const FString V4VegetationRuntimeObjectRoot(
    V4RuntimeObjectRoot + TEXT("Vegetation/"));
const FString V4MaterialRuntimeObjectRoot(
    V4VegetationRuntimeObjectRoot + TEXT("Materials/"));
const FString V4PorticoRuntimeObjectRoot(
    V4RuntimeObjectRoot + TEXT("Portico/"));
const FString V5CPorticoRuntimeObjectRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Portico/"));
const FString V5CPorticoMeshObjectPath(
    V5CPorticoRuntimeObjectRoot +
    TEXT("SM_IPV5C_CentralPorticoFidelityOverlay.SM_IPV5C_CentralPorticoFidelityOverlay"));
const FString V5CPorticoMaterialObjectRoot(
    V5CPorticoRuntimeObjectRoot + TEXT("Materials/"));
const FString ProtectedHighForkRuntimeObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_IslandTree01.SM_IPVExploreV3_IslandTree01"));
const FString ProtectedColumnarRuntimeObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_Broadleaf_A.SM_IstanaPublicViewExploreV1_Broadleaf_A"));
const FName V5BRenderSuccessorTag(TEXT("TRIADIstanaExploreV5BRenderSuccessor"));

static_assert(ExpectedWindBindingCount == 18);
static_assert(ExpectedRuntimeWindMidCount == 31);
static_assert(ExpectedTotalHismInstanceCount == 21794);
static_assert(ExpectedVisibleHismInstanceCount == 21785);

bool IsAdmittedTreeMeshPath(const UStaticMesh* Mesh, int32 TreeFormIndex)
{
    if (!Mesh)
    {
        return false;
    }
    const FString Path = Mesh->GetPathName();
    if (TreeFormIndex == 2)
    {
        return Path == ProtectedHighForkRuntimeObjectPath;
    }
    if (TreeFormIndex == 3)
    {
        return Path == ProtectedColumnarRuntimeObjectPath;
    }
    return Path.StartsWith(V4VegetationRuntimeObjectRoot);
}

const FName WindStrengthParameter(TEXT("TRIAD_WindStrengthCm"));
const FName WindSpeedParameter(TEXT("TRIAD_WindSpeed"));
const FName WindDirectionParameter(TEXT("TRIAD_WindDirection"));
const FName WindHeightParameter(TEXT("TRIAD_WindHeightCm"));
const FName WindResponseParameter(TEXT("TRIAD_WindResponseScale"));
const FName MaximumWpoParameter(TEXT("TRIAD_MaxWpoCm"));

struct FExpectedHeritageAnchor
{
    const TCHAR* PublicRecordId;
    float HeightMeters;
    float GirthMeters;
    ETRIADIstanaExploreV4TreeForm Form;
    bool bFormComesFromPublishedQualitativeHint;
};

const FExpectedHeritageAnchor ExpectedHeritageAnchors[] = {
    {TEXT("HT2018-295"), 23.2f, 3.53f, ETRIADIstanaExploreV4TreeForm::Dome, false},
    {TEXT("HT2020-313"), 26.2f, 3.30f, ETRIADIstanaExploreV4TreeForm::Dome, false},
    {TEXT("HT2003-108"), 25.6f, 6.55f, ETRIADIstanaExploreV4TreeForm::Umbrella, true},
    {TEXT("HT2003-87"), 36.5f, 4.23f, ETRIADIstanaExploreV4TreeForm::Dome, true},
    {TEXT("HT2018-292"), 28.3f, 3.07f, ETRIADIstanaExploreV4TreeForm::HighForkRounded, true},
    {TEXT("HT2021-319"), 14.6f, 3.50f, ETRIADIstanaExploreV4TreeForm::Dome, true},
    {TEXT("HT2008-169"), 20.4f, 7.20f, ETRIADIstanaExploreV4TreeForm::Umbrella, true},
    {TEXT("HT2018-298"), 18.8f, 3.67f, ETRIADIstanaExploreV4TreeForm::Umbrella, true},
    {TEXT("HT2019-306"), 21.2f, 4.88f, ETRIADIstanaExploreV4TreeForm::Umbrella, true}};

const FName ExpectedPorticoMaterials[] = {
    TEXT("M_IPV8_Portico_Trim"),
    TEXT("M_IPV8_Portico_Stone"),
    TEXT("M_IPV8_Portico_Render"),
    TEXT("M_IPV8_Portico_Soffit"),
    TEXT("M_IPV8_Portico_Recess"),
    TEXT("M_IPV8_Portico_Metal"),
    TEXT("M_IPV8_Portico_Louvre")};
const int32 ExpectedPorticoTrianglesByMaterial[] = {
    3100, 1704, 48, 1092, 36, 144, 468};

// Exact OBJ/MTL first-use order. Import is accepted only when UE preserves
// this ordered material contract.
const FName ExpectedPorticoV5CMaterials[] = {
    TEXT("M_IPV5C_Portico_StoneWarm"),
    TEXT("M_IPV5C_Portico_TrimIvory"),
    TEXT("M_IPV5C_Portico_RenderWarm"),
    TEXT("M_IPV5C_Portico_RecessWarmShadow"),
    TEXT("M_IPV5C_Portico_Soffit"),
    TEXT("M_IPV5C_Portico_DoorTimber"),
    TEXT("M_IPV5C_Portico_FanlightGlass"),
    TEXT("M_IPV5C_Portico_LouvreWarmIvory")};
const int32 ExpectedPorticoV5CTrianglesByMaterial[] = {
    2616, 5504, 180, 240, 1632, 432, 288, 2880};

static_assert(UE_ARRAY_COUNT(ExpectedHeritageAnchors) == ExpectedHeritageAnchorCount);
static_assert(UE_ARRAY_COUNT(ExpectedPorticoMaterials) == ExpectedPorticoMaterialCount);
static_assert(UE_ARRAY_COUNT(ExpectedPorticoTrianglesByMaterial) == ExpectedPorticoMaterialCount);
static_assert(UE_ARRAY_COUNT(ExpectedPorticoV5CMaterials) == ExpectedPorticoV5CMaterialCount);
static_assert(UE_ARRAY_COUNT(ExpectedPorticoV5CTrianglesByMaterial) == ExpectedPorticoV5CMaterialCount);

FString ExpectedPorticoMaterialObjectPath(const FName MaterialName)
{
    const FString Name = MaterialName.ToString();
    return V4MaterialRuntimeObjectRoot + Name + TEXT(".") + Name;
}

FString ExpectedPorticoV5CMaterialObjectPath(const FName MaterialName)
{
    const FString Name = MaterialName.ToString();
    return V5CPorticoMaterialObjectRoot + Name + TEXT(".") + Name;
}

uint32 Mix32(uint32 Value)
{
    Value ^= Value >> 16;
    Value *= 0x7FEB352Du;
    Value ^= Value >> 15;
    Value *= 0x846CA68Bu;
    Value ^= Value >> 16;
    return Value;
}

uint32 HashTreeRow(
    int32 SourceIndex,
    const FTRIADIstanaExploreV4RasterCue& Cue,
    int32 Seed)
{
    return Mix32(
        static_cast<uint32>(Seed) ^
        (static_cast<uint32>(SourceIndex) * 0x9E3779B9u) ^
        (static_cast<uint32>(Cue.BroadRasterClass) << 17u) ^
        (static_cast<uint32>(Cue.HeightBand) << 25u));
}

int32 ExpectedMaterialSlotForWindRole(
    ETRIADIstanaExploreV4WindRole Role)
{
    switch (Role)
    {
    case ETRIADIstanaExploreV4WindRole::UmbrellaTrunk:
    case ETRIADIstanaExploreV4WindRole::HighForkTrunk:
        return 0;
    case ETRIADIstanaExploreV4WindRole::UmbrellaBranch:
    case ETRIADIstanaExploreV4WindRole::HighForkBranch:
    case ETRIADIstanaExploreV4WindRole::ColumnarLeaf:
    case ETRIADIstanaExploreV4WindRole::DomeTrunk:
        return 2;
    case ETRIADIstanaExploreV4WindRole::UmbrellaLeaf:
    case ETRIADIstanaExploreV4WindRole::DomeLeaf:
    case ETRIADIstanaExploreV4WindRole::HighForkLeaf:
    case ETRIADIstanaExploreV4WindRole::ColumnarTrunk:
        return 1;
    case ETRIADIstanaExploreV4WindRole::DomeBranch:
    case ETRIADIstanaExploreV4WindRole::ColumnarBranch:
    case ETRIADIstanaExploreV4WindRole::PalmComposite:
    case ETRIADIstanaExploreV4WindRole::Shrub:
    case ETRIADIstanaExploreV4WindRole::Flower:
    case ETRIADIstanaExploreV4WindRole::Understorey:
    case ETRIADIstanaExploreV4WindRole::GeometryGrass:
    case ETRIADIstanaExploreV4WindRole::CloseTurf:
        return 0;
    default:
        return INDEX_NONE;
    }
}

bool IgnoresAllChannels(const UPrimitiveComponent* Component)
{
    return Component &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore);
}

bool HasPawnOnlyChannelResponses(const UPrimitiveComponent* Component)
{
    if (!Component)
    {
        return false;
    }
    FCollisionResponseContainer ExpectedPawnOnlyResponses(ECR_Ignore);
    ExpectedPawnOnlyResponses.SetResponse(ECC_Pawn, ECR_Block);
    return Component->GetCollisionResponseToChannels() ==
        ExpectedPawnOnlyResponses;
}

bool IsPawnOnlyQueryBlocker(const UPrimitiveComponent* Component)
{
    return Component &&
        Component->GetCollisionEnabled() == ECollisionEnabled::QueryOnly &&
        !Component->GetGenerateOverlapEvents() &&
        HasPawnOnlyChannelResponses(Component);
}

struct FV4HismColdGateRow
{
    const TCHAR* Label = TEXT("");
    const UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
    const UStaticMesh* ExpectedMesh = nullptr;
    int32 ExpectedInstances = 0;
    ECollisionEnabled::Type ExpectedCollision = ECollisionEnabled::NoCollision;
    bool bExpectedVisible = true;
    bool bExpectedHiddenInGame = false;
    bool bPawnOnlyChannels = false;
};

const TCHAR* BoolText(bool Value)
{
    return Value ? TEXT("true") : TEXT("false");
}

bool ValidateExactV4HismColdGate(
    const TArray<FV4HismColdGateRow>& Rows,
    bool bRenderVisualsHiddenByV5BSuccessor,
    FString& OutError)
{
    if (Rows.Num() != ExpectedHismComponentCount)
    {
        OutError = FString::Printf(
            TEXT("ISTANA_EXPLORE_V4_HISM_COLD_GATE_FAILED components=%d expectedComponents=%d"),
            Rows.Num(),
            ExpectedHismComponentCount);
        return false;
    }

    int32 VisibleComponents = 0;
    int32 HiddenBlockers = 0;
    int32 NonEmptyComponents = 0;
    int32 TotalInstances = 0;
    int32 VisibleInstances = 0;
    int32 AutoRebuildComponents = 0;
    int32 AsyncComponents = 0;
    int32 FullyBuiltComponents = 0;
    int32 RenderCountMatches = 0;

    for (const FV4HismColdGateRow& Row : Rows)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            Row.Component;
        const bool bClassOk = Component &&
            Component->GetClass() ==
                UTRIADIstanaExploreV4SynchronousHismComponent::StaticClass();
        const int32 ActualInstances = Component
            ? Component->GetInstanceCount()
            : INDEX_NONE;
        const int32 RenderInstances = Component
            ? Component->GetNumRenderInstances()
            : INDEX_NONE;
        const UStaticMesh* ActualMesh = Component
            ? Component->GetStaticMesh()
            : nullptr;
        const bool bMeshOk = ActualMesh && ActualMesh == Row.ExpectedMesh;
        const bool bRelativeIdentity = Component &&
            Component->GetRelativeTransform().Equals(
                FTransform::Identity, 0.001f);
        const bool bCollisionOk = Component &&
            Component->GetCollisionEnabled() == Row.ExpectedCollision;
        const bool bOverlap = Component &&
            Component->GetGenerateOverlapEvents();
        const bool bChannelsOk = Row.bPawnOnlyChannels
            ? HasPawnOnlyChannelResponses(Component)
            : IgnoresAllChannels(Component);
        const bool bAutoRebuild = Component &&
            Component->bAutoRebuildTreeOnInstanceChanges;
        const bool bAsync = Component && Component->IsAsyncBuilding();
        const bool bFullyBuilt = Component && Component->IsTreeFullyBuilt();
        const bool bVisible = Component && Component->IsVisible();
        const bool bHiddenInGame = Component && Component->bHiddenInGame;
        bool bDensityScalingOff = Component &&
            !Component->bEnableDensityScaling &&
            FMath::IsNearlyEqual(Component->CurrentDensityScaling, 1.0f);
#if WITH_EDITOR
        bDensityScalingOff = bDensityScalingOff &&
            !Component->bCanEnableDensityScaling;
#endif
        const bool bRenderCountMatches = Component &&
            RenderInstances == ActualInstances;

        const bool bRowValid = bClassOk &&
            ActualInstances == Row.ExpectedInstances &&
            bRenderCountMatches && bMeshOk && bRelativeIdentity &&
            bCollisionOk && !bOverlap && bChannelsOk && bAutoRebuild &&
            !bAsync && bFullyBuilt &&
            bVisible == Row.bExpectedVisible &&
            bHiddenInGame == Row.bExpectedHiddenInGame &&
            bDensityScalingOff;
        if (!bRowValid)
        {
            const FString ComponentName = Component
                ? Component->GetName()
                : TEXT("<null>");
            const FString ActualMeshName = ActualMesh
                ? ActualMesh->GetPathName()
                : TEXT("<null>");
            const FString ExpectedMeshName = Row.ExpectedMesh
                ? Row.ExpectedMesh->GetPathName()
                : TEXT("<null>");
            OutError = FString::Printf(
                TEXT("ISTANA_EXPLORE_V4_HISM_COLD_GATE_FAILED label=%s component=%s classOk=%s expectedInstances=%d actualInstances=%d renderInstances=%d actualMesh=%s expectedMesh=%s meshOk=%s relativeIdentity=%s collision=%d expectedCollision=%d overlap=%s channelsOk=%s autoRebuild=%s async=%s fullyBuilt=%s visible=%s expectedVisible=%s hiddenInGame=%s expectedHiddenInGame=%s densityScaling=%s"),
                Row.Label,
                *ComponentName,
                BoolText(bClassOk),
                Row.ExpectedInstances,
                ActualInstances,
                RenderInstances,
                *ActualMeshName,
                *ExpectedMeshName,
                BoolText(bMeshOk),
                BoolText(bRelativeIdentity),
                Component
                    ? static_cast<int32>(Component->GetCollisionEnabled())
                    : INDEX_NONE,
                static_cast<int32>(Row.ExpectedCollision),
                BoolText(bOverlap),
                BoolText(bChannelsOk),
                BoolText(bAutoRebuild),
                BoolText(bAsync),
                BoolText(bFullyBuilt),
                BoolText(bVisible),
                BoolText(Row.bExpectedVisible),
                BoolText(bHiddenInGame),
                BoolText(Row.bExpectedHiddenInGame),
                bDensityScalingOff ? TEXT("off") : TEXT("on"));
            return false;
        }

        VisibleComponents += bVisible && !bHiddenInGame ? 1 : 0;
        HiddenBlockers += Row.bPawnOnlyChannels && !bVisible && bHiddenInGame
            ? 1
            : 0;
        NonEmptyComponents += ActualInstances > 0 ? 1 : 0;
        TotalInstances += ActualInstances;
        VisibleInstances += bVisible && !bHiddenInGame
            ? ActualInstances
            : 0;
        AutoRebuildComponents += bAutoRebuild ? 1 : 0;
        AsyncComponents += bAsync ? 1 : 0;
        FullyBuiltComponents += bFullyBuilt ? 1 : 0;
        RenderCountMatches += bRenderCountMatches ? 1 : 0;
    }

    const int32 RequiredVisibleComponents = ExpectedVisibleHismComponentCount -
        (bRenderVisualsHiddenByV5BSuccessor ? 5 : 0);
    const int32 RequiredVisibleInstances = ExpectedVisibleHismInstanceCount -
        (bRenderVisualsHiddenByV5BSuccessor
            ? ExpectedCloseTurfCount + ExpectedShrubCount +
                ExpectedFlowerCount + ExpectedUnderstoreyCount +
                ExpectedGeometryGrassCount
            : 0);
    if (VisibleComponents != RequiredVisibleComponents ||
        HiddenBlockers != ExpectedHiddenHismBlockerCount ||
        NonEmptyComponents != ExpectedNonEmptyHismComponentCount ||
        TotalInstances != ExpectedTotalHismInstanceCount ||
        VisibleInstances != RequiredVisibleInstances ||
        AutoRebuildComponents != ExpectedHismComponentCount ||
        AsyncComponents != 0 ||
        FullyBuiltComponents != ExpectedHismComponentCount ||
        RenderCountMatches != ExpectedHismComponentCount)
    {
        OutError = FString::Printf(
            TEXT("ISTANA_EXPLORE_V4_HISM_COLD_GATE_TOTALS_INVALID components=%d visible=%d hiddenBlockers=%d nonEmpty=%d totalInstances=%d visibleInstances=%d autoRebuild=%d async=%d fullyBuilt=%d renderCountMatches=%d"),
            Rows.Num(),
            VisibleComponents,
            HiddenBlockers,
            NonEmptyComponents,
            TotalInstances,
            VisibleInstances,
            AutoRebuildComponents,
            AsyncComponents,
            FullyBuiltComponents,
            RenderCountMatches);
        return false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ISTANA_EXPLORE_V4_HISM_COLD_GATE_PASS components=%d visible=%d hiddenBlockers=%d nonEmpty=%d totalInstances=%d visibleInstances=%d autoRebuild=%d async=%d fullyBuilt=%d renderCountMatches=%d"),
        Rows.Num(),
        VisibleComponents,
        HiddenBlockers,
        NonEmptyComponents,
        TotalInstances,
        VisibleInstances,
        AutoRebuildComponents,
        AsyncComponents,
        FullyBuiltComponents,
        RenderCountMatches);
    OutError.Reset();
    return true;
}

int32 CountTriangles(const UStaticMesh* Mesh, int32 LodIndex)
{
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    if (!RenderData || !RenderData->LODResources.IsValidIndex(LodIndex))
    {
        return INDEX_NONE;
    }
    uint64 TriangleCount = 0;
    for (const FStaticMeshSection& Section :
         RenderData->LODResources[LodIndex].Sections)
    {
        TriangleCount += Section.NumTriangles;
    }
    return TriangleCount <= static_cast<uint64>(MAX_int32)
        ? static_cast<int32>(TriangleCount)
        : INDEX_NONE;
}

double MeshHeightCm(const UStaticMesh* Mesh)
{
    return Mesh ? Mesh->GetBounds().BoxExtent.Z * 2.0 : 0.0;
}

bool HasBaseOrRuntimeMidParent(
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
    const UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Material);
    return Mid && Mid->Parent == ExpectedBase;
}

bool HasCompleteRuntimeWindParameterRoster(
    const UMaterialInterface* Material)
{
    float ScalarValue = 0.0f;
    FLinearColor VectorValue = FLinearColor::Black;
    return Material &&
        Material->GetScalarParameterValue(
            FMaterialParameterInfo(WindStrengthParameter), ScalarValue) &&
        Material->GetScalarParameterValue(
            FMaterialParameterInfo(WindSpeedParameter), ScalarValue) &&
        Material->GetScalarParameterValue(
            FMaterialParameterInfo(WindHeightParameter), ScalarValue) &&
        Material->GetScalarParameterValue(
            FMaterialParameterInfo(WindResponseParameter), ScalarValue) &&
        Material->GetScalarParameterValue(
            FMaterialParameterInfo(MaximumWpoParameter), ScalarValue) &&
        Material->GetVectorParameterValue(
            FMaterialParameterInfo(WindDirectionParameter), VectorValue);
}

bool IsInsideFountainClearance(
    const FTransform& Transform,
    const FVector2D& FountainCenterMeters,
    float RadiusMeters)
{
    const FVector LocationMeters =
        Transform.GetTranslation() / CentimetersPerMeter;
    return FVector2D(
        LocationMeters.X - FountainCenterMeters.X,
        LocationMeters.Y - FountainCenterMeters.Y).SizeSquared() <
        FMath::Square(static_cast<double>(RadiusMeters));
}

bool IsInsideWoodyCeremonialClearance(
    const FTransform& Transform,
    float AxisHalfWidthMeters)
{
    const FVector LocationMeters =
        Transform.GetTranslation() / CentimetersPerMeter;
    return FMath::Abs(LocationMeters.X) < AxisHalfWidthMeters &&
        LocationMeters.Y >= 0.0 && LocationMeters.Y < 138.0;
}

bool IsInsideOneKilometre(const FTransform& Transform)
{
    const FVector LocationMeters =
        Transform.GetTranslation() / CentimetersPerMeter;
    return FVector2D(LocationMeters.X, LocationMeters.Y).SizeSquared() <=
        FMath::Square(1000.0) + 0.01;
}
}

ATRIADIstanaExploreV4LandscapeActor::ATRIADIstanaExploreV4LandscapeActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bAllowTickOnDedicatedServer = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ExploreV4LandscapeRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    UmbrellaTreeInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4UmbrellaTreeReclassification"));
    DomeTreeInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4DomeTreeReclassification"));
    HighForkRoundedTreeInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4HighForkRoundedTreeReclassification"));
    ColumnarNarrowTreeInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4ColumnarNarrowTreeReclassification"));
    PalmTreeInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4PalmTreeReclassification"));

    HeritageUmbrellaInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4HeritageUmbrellaSilhouetteProxies"));
    HeritageDomeInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4HeritageDomeSilhouetteProxies"));
    HeritageHighForkRoundedInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4HeritageHighForkSilhouetteProxies"));
    HeritageColumnarNarrowInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4HeritageColumnarSilhouetteProxies"));
    HeritagePalmInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4HeritagePalmSilhouetteProxies"));
    HeritageAnchorPawnBlockers = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4HeritageAnchorPawnOnlyBlockers"));

    ShrubInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4LayeredShrubs"));
    FlowerInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4FloweringAccents"));
    UnderstoreyInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4TropicalUnderstorey"));
    GeometryGrassInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4TallerEdgeGeometryGrass"));
    CloseTurfInstances = CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(TEXT("V4AnimatedCloseTurfGeometryCardReplacement"));
    PorticoV8RenderOnlyComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("V8CentralPorticoRenderOnlySuccessor"));
    PorticoV5CRenderOnlyComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("V5CCentralPorticoRenderOnlySuccessor"));

    const TArray<USceneComponent*> LandscapeOwnedComponents = {
        UmbrellaTreeInstances,
        DomeTreeInstances,
        HighForkRoundedTreeInstances,
        ColumnarNarrowTreeInstances,
        PalmTreeInstances,
        HeritageUmbrellaInstances,
        HeritageDomeInstances,
        HeritageHighForkRoundedInstances,
        HeritageColumnarNarrowInstances,
        HeritagePalmInstances,
        HeritageAnchorPawnBlockers,
        ShrubInstances,
        FlowerInstances,
        UnderstoreyInstances,
        GeometryGrassInstances,
        CloseTurfInstances,
        PorticoV8RenderOnlyComponent,
        PorticoV5CRenderOnlyComponent};
    for (USceneComponent* Component : LandscapeOwnedComponents)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
        Component->SetMobility(EComponentMobility::Static);
    }

    ConfigureVisualHism(UmbrellaTreeInstances, 1, 22000, 185000, 65000, true);
    ConfigureVisualHism(DomeTreeInstances, 1, 22000, 180000, 65000, true);
    ConfigureVisualHism(HighForkRoundedTreeInstances, 1, 20000, 170000, 62000, true);
    ConfigureVisualHism(ColumnarNarrowTreeInstances, 1, 18000, 155000, 58000, true);
    ConfigureVisualHism(PalmTreeInstances, 1, 18000, 150000, 52000, true);

    ConfigureVisualHism(HeritageUmbrellaInstances, 1, 25000, 200000, 70000, true);
    ConfigureVisualHism(HeritageDomeInstances, 1, 25000, 200000, 70000, true);
    ConfigureVisualHism(HeritageHighForkRoundedInstances, 1, 25000, 200000, 70000, true);
    ConfigureVisualHism(HeritageColumnarNarrowInstances, 1, 25000, 200000, 70000, true);
    ConfigureVisualHism(HeritagePalmInstances, 1, 25000, 200000, 60000, true);

    ConfigureVisualHism(ShrubInstances, 1, 6500, 65000, 30000, true);
    ConfigureVisualHism(FlowerInstances, 1, 4500, 45000, 24000, true);
    ConfigureVisualHism(UnderstoreyInstances, 1, 5500, 55000, 28000, true);
    ConfigureVisualHism(GeometryGrassInstances, 1, 3500, 32000, 16000, false);
    ConfigureVisualHism(CloseTurfInstances, 1, 800, 10000, 9000, false);
    ConfigureVisualStaticMesh(PorticoV8RenderOnlyComponent);
    ConfigureVisualStaticMesh(PorticoV5CRenderOnlyComponent);
    PorticoV5CRenderOnlyComponent->SetVisibility(false, true);
    PorticoV5CRenderOnlyComponent->SetHiddenInGame(true);

    HeritageAnchorPawnBlockers->bEnableDensityScaling = false;
    HeritageAnchorPawnBlockers->CurrentDensityScaling = 1.0f;
#if WITH_EDITOR
    HeritageAnchorPawnBlockers->bCanEnableDensityScaling = false;
#endif
    HeritageAnchorPawnBlockers->SetVisibility(false, true);
    HeritageAnchorPawnBlockers->SetHiddenInGame(true);
    HeritageAnchorPawnBlockers->SetGenerateOverlapEvents(false);
    HeritageAnchorPawnBlockers->SetCanEverAffectNavigation(false);
    HeritageAnchorPawnBlockers->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HeritageAnchorPawnBlockers->SetCollisionResponseToAllChannels(ECR_Ignore);
    HeritageAnchorPawnBlockers->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

    RequiredDistributionAttribution = {
        TEXT("Contains information from Heritage Trees and Tree Conservation Area, accessed on 25 August 2026 from data.gov.sg, made available under the Singapore Open Data Licence version 1.0 https://www.sla.gov.sg/singapore-open-data-licence/"),
        TEXT("© ESA WorldCover project 2021 / Contains modified Copernicus Sentinel data (2021) processed by ESA WorldCover consortium."),
        TEXT("ETH Global Canopy Height 2020, Lang et al. (2023), https://doi.org/10.3929/ethz-b-000609802, licensed CC BY 4.0.")};

    GustRandom.Initialize(DeterministicPlacementSeed ^ 0x57494E44);
}

bool ATRIADIstanaExploreV4LandscapeActor::PreservesSourceTranslationAndRotation(
    const FTransform& Candidate,
    const FTransform& Source,
    double TranslationToleranceCm,
    double RotationComponentTolerance)
{
    return Candidate.GetTranslation().Equals(
               Source.GetTranslation(), TranslationToleranceCm) &&
        Candidate.GetRotation().Equals(
            Source.GetRotation(), RotationComponentTolerance);
}

void ATRIADIstanaExploreV4LandscapeActor::ConfigureVisualHism(
    UHierarchicalInstancedStaticMeshComponent* Component,
    int32 MinLod,
    int32 StartCullDistance,
    int32 EndCullDistance,
    int32 WpoDisableDistance,
    bool bCastShadow)
{
    check(Component);
    Component->bEnableDensityScaling = false;
    Component->CurrentDensityScaling = 1.0f;
#if WITH_EDITOR
    Component->bCanEnableDensityScaling = false;
#endif
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetVisibility(true, true);
    Component->SetHiddenInGame(false);
    Component->SetCullDistances(StartCullDistance, EndCullDistance);
    Component->SetWorldPositionOffsetDisableDistance(WpoDisableDistance);
    Component->ForcedLodModel = 0;
    Component->bOverrideMinLOD = true;
    Component->MinLOD = MinLod;
    Component->SetCastShadow(bCastShadow);
}

void ATRIADIstanaExploreV4LandscapeActor::ConfigureVisualStaticMesh(
    UStaticMeshComponent* Component)
{
    check(Component);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetVisibility(true, true);
    Component->SetHiddenInGame(false);
    Component->SetCastShadow(true);
}

bool ATRIADIstanaExploreV4LandscapeActor::ValidateExactPorticoV5CMesh(
    const UStaticMesh* Mesh,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || Mesh->GetPathName() != V5CPorticoMeshObjectPath ||
        CountTriangles(Mesh, 0) != ExpectedPorticoV5CTriangleCount ||
        Mesh->GetStaticMaterials().Num() != ExpectedPorticoV5CMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() !=
            ExpectedPorticoV5CMaterialCount)
    {
        OutError = TEXT("V5C requires the exact 13,772-triangle, eight-slot identity portico mesh in its isolated namespace.");
        return false;
    }

    const UBodySetup* Body = Mesh->GetBodySetup();
    if (Body && (Body->AggGeom.GetElementCount() != 0 ||
                 Body->CollisionTraceFlag == CTF_UseComplexAsSimple))
    {
        OutError = TEXT("The V5C portico mesh must remain render-only with no simple or complex-as-simple collision.");
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(FVector(-1350.0, 1250.0, 64.0), 0.5) ||
        !BoundsMax.Equals(FVector(1350.0, 2592.0, 2118.0), 0.5))
    {
        OutError = FString::Printf(
            TEXT("V5C imported centimetre bounds changed: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            BoundsMin.X,
            BoundsMin.Y,
            BoundsMin.Z,
            BoundsMax.X,
            BoundsMax.Y,
            BoundsMax.Z);
        return false;
    }

    for (int32 Slot = 0; Slot < ExpectedPorticoV5CMaterialCount; ++Slot)
    {
        const FStaticMaterial& StaticMaterial = Mesh->GetStaticMaterials()[Slot];
        const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
        bool bImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
        bImportedSlotNameMatches =
            StaticMaterial.ImportedMaterialSlotName ==
                ExpectedPorticoV5CMaterials[Slot];
#endif
        if (StaticMaterial.MaterialSlotName != ExpectedPorticoV5CMaterials[Slot] ||
            !bImportedSlotNameMatches ||
            !Material || Material->GetPathName() !=
                ExpectedPorticoV5CMaterialObjectPath(
                    ExpectedPorticoV5CMaterials[Slot]))
        {
            OutError = FString::Printf(
                TEXT("V5C material slot %d lost exact imported name/order/binding."),
                Slot);
            return false;
        }
    }

    TSet<int32> SeenSectionMaterials;
    for (const FStaticMeshSection& Section :
         RenderData->LODResources[0].Sections)
    {
        if (Section.MaterialIndex < 0 ||
            Section.MaterialIndex >= ExpectedPorticoV5CMaterialCount ||
            SeenSectionMaterials.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                ExpectedPorticoV5CTrianglesByMaterial[Section.MaterialIndex])
        {
            OutError = TEXT("V5C exact eight-section material/triangle topology changed.");
            return false;
        }
        SeenSectionMaterials.Add(Section.MaterialIndex);
    }
    if (SeenSectionMaterials.Num() != ExpectedPorticoV5CMaterialCount)
    {
        OutError = TEXT("V5C exact material-section census is incomplete.");
        return false;
    }

    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV4LandscapeActor::
    RestorePorticoV8PresentationFailSafe()
{
    bPorticoV5CPresentationActive = false;
    if (PorticoV8RenderOnlyComponent)
    {
        PorticoV8RenderOnlyComponent->SetRelativeTransform(FTransform::Identity);
        ConfigureVisualStaticMesh(PorticoV8RenderOnlyComponent);
        PorticoV8RenderOnlyComponent->SetVisibility(true, true);
        PorticoV8RenderOnlyComponent->SetHiddenInGame(false);
    }
    if (PorticoV5CRenderOnlyComponent)
    {
        PorticoV5CRenderOnlyComponent->SetStaticMesh(nullptr);
        PorticoV5CRenderOnlyComponent->SetRelativeTransform(FTransform::Identity);
        ConfigureVisualStaticMesh(PorticoV5CRenderOnlyComponent);
        PorticoV5CRenderOnlyComponent->SetVisibility(false, true);
        PorticoV5CRenderOnlyComponent->SetHiddenInGame(true);
    }
}

bool ATRIADIstanaExploreV4LandscapeActor::ConfigurePorticoV5CPresentation(
    UStaticMesh* InPorticoV5C,
    FString& OutError)
{
    // Establish the visible V8 fallback before inspecting any candidate.
    RestorePorticoV8PresentationFailSafe();

    if (!PorticoV8RenderOnlyComponent || !PorticoV5CRenderOnlyComponent)
    {
        OutError = TEXT("V5C presentation refused because an exact V8/V5C sibling component is absent.");
        return false;
    }

    FString MeshError;
    if (!ValidateExactPorticoV5CMesh(InPorticoV5C, MeshError))
    {
        OutError = TEXT("V5C presentation refused; V8 remains visible. ") +
            MeshError;
        return false;
    }

    PorticoV5CRenderOnlyComponent->SetStaticMesh(InPorticoV5C);
    PorticoV5CRenderOnlyComponent->SetRelativeTransform(FTransform::Identity);
    ConfigureVisualStaticMesh(PorticoV5CRenderOnlyComponent);
    bPorticoV5CPresentationActive = true;
    PorticoV8RenderOnlyComponent->SetVisibility(false, true);
    PorticoV8RenderOnlyComponent->SetHiddenInGame(true);

    FString PresentationError;
    if (!ValidatePorticoV5CPresentation(PresentationError))
    {
        RestorePorticoV8PresentationFailSafe();
        OutError = TEXT("V5C presentation readback failed; V8 was restored. ") +
            PresentationError;
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV4LandscapeActor::ValidatePorticoV5CPresentation(
    FString& OutReport) const
{
    const auto IsRenderOnlyIdentity = [this](
        const UStaticMeshComponent* Component)
    {
        return Component && Component->GetAttachParent() == SceneRoot &&
            Component->Mobility == EComponentMobility::Static &&
            Component->GetRelativeTransform().Equals(
                FTransform::Identity, 0.001) &&
            Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
            !Component->GetGenerateOverlapEvents() &&
            !Component->CanEverAffectNavigation() &&
            IgnoresAllChannels(Component);
    };
    if (!IsRenderOnlyIdentity(PorticoV8RenderOnlyComponent) ||
        !IsRenderOnlyIdentity(PorticoV5CRenderOnlyComponent) ||
        !PorticoV8RenderOnlyComponent->GetComponentTransform().Equals(
            FTransform::Identity, 0.001) ||
        !PorticoV5CRenderOnlyComponent->GetComponentTransform().Equals(
            FTransform::Identity, 0.001) ||
        !bPorticoV5CRenderOnly ||
        bPorticoV5CCollisionNavigationOrRfAuthority ||
        bPorticoV5CUsesTextureMaps)
    {
        OutReport = TEXT("V8/V5C sibling components lost identity, NoCollision, no-navigation, no-RF or no-texture authority boundaries.");
        return false;
    }

    if (!bPorticoV5CPresentationActive)
    {
        if (!PorticoV8RenderOnlyComponent->IsVisible() ||
            PorticoV8RenderOnlyComponent->bHiddenInGame ||
            PorticoV5CRenderOnlyComponent->GetStaticMesh() ||
            PorticoV5CRenderOnlyComponent->IsVisible() ||
            !PorticoV5CRenderOnlyComponent->bHiddenInGame)
        {
            OutReport = TEXT("Inactive V5C state must expose V8 and keep the empty V5C sibling hidden.");
            return false;
        }
        OutReport = TEXT("ISTANA_EXPLORE_V5C_PORTICO_PRESENTATION_VALID active=false failSafeV8Visible=true.");
        return true;
    }

    FString MeshError;
    if (!ValidateExactPorticoV5CMesh(
            PorticoV5CRenderOnlyComponent->GetStaticMesh(), MeshError) ||
        PorticoV8RenderOnlyComponent->IsVisible() ||
        !PorticoV8RenderOnlyComponent->bHiddenInGame ||
        !PorticoV5CRenderOnlyComponent->IsVisible() ||
        PorticoV5CRenderOnlyComponent->bHiddenInGame)
    {
        OutReport = TEXT("Active V5C state requires exact visible V5C and hidden V8. ") +
            MeshError;
        return false;
    }

    OutReport = TEXT("ISTANA_EXPLORE_V5C_PORTICO_PRESENTATION_VALID active=true V8Hidden=true identity=true renderOnly=true collision=false navigation=false rfAuthority=false textureMaps=false.");
    return true;
}

bool ATRIADIstanaExploreV4LandscapeActor::IsSupportedRasterClass(uint8 Value)
{
    return Value == 0 || Value == 10 || Value == 30 || Value == 50 ||
        Value == 60 || Value == 80;
}

bool ATRIADIstanaExploreV4LandscapeActor::IsFinitePositiveTransform(
    const FTransform& Transform)
{
    const FVector Scale = Transform.GetScale3D();
    return !Transform.ContainsNaN() &&
        Scale.X > 0.0001 && Scale.Y > 0.0001 && Scale.Z > 0.0001;
}

UHierarchicalInstancedStaticMeshComponent*
ATRIADIstanaExploreV4LandscapeActor::ComponentForTreeForm(
    ETRIADIstanaExploreV4TreeForm Form) const
{
    switch (Form)
    {
    case ETRIADIstanaExploreV4TreeForm::Umbrella:
        return UmbrellaTreeInstances;
    case ETRIADIstanaExploreV4TreeForm::Dome:
        return DomeTreeInstances;
    case ETRIADIstanaExploreV4TreeForm::HighForkRounded:
        return HighForkRoundedTreeInstances;
    case ETRIADIstanaExploreV4TreeForm::ColumnarNarrow:
        return ColumnarNarrowTreeInstances;
    case ETRIADIstanaExploreV4TreeForm::Palm:
        return PalmTreeInstances;
    default:
        return nullptr;
    }
}

UHierarchicalInstancedStaticMeshComponent*
ATRIADIstanaExploreV4LandscapeActor::HeritageComponentForTreeForm(
    ETRIADIstanaExploreV4TreeForm Form) const
{
    switch (Form)
    {
    case ETRIADIstanaExploreV4TreeForm::Umbrella:
        return HeritageUmbrellaInstances;
    case ETRIADIstanaExploreV4TreeForm::Dome:
        return HeritageDomeInstances;
    case ETRIADIstanaExploreV4TreeForm::HighForkRounded:
        return HeritageHighForkRoundedInstances;
    case ETRIADIstanaExploreV4TreeForm::ColumnarNarrow:
        return HeritageColumnarNarrowInstances;
    case ETRIADIstanaExploreV4TreeForm::Palm:
        return HeritagePalmInstances;
    default:
        return nullptr;
    }
}

ETRIADIstanaExploreV4TreeForm
ATRIADIstanaExploreV4LandscapeActor::TreeFormForWindRole(
    ETRIADIstanaExploreV4WindRole Role)
{
    switch (Role)
    {
    case ETRIADIstanaExploreV4WindRole::UmbrellaTrunk:
    case ETRIADIstanaExploreV4WindRole::UmbrellaBranch:
    case ETRIADIstanaExploreV4WindRole::UmbrellaLeaf:
        return ETRIADIstanaExploreV4TreeForm::Umbrella;
    case ETRIADIstanaExploreV4WindRole::DomeTrunk:
    case ETRIADIstanaExploreV4WindRole::DomeBranch:
    case ETRIADIstanaExploreV4WindRole::DomeLeaf:
        return ETRIADIstanaExploreV4TreeForm::Dome;
    case ETRIADIstanaExploreV4WindRole::HighForkTrunk:
    case ETRIADIstanaExploreV4WindRole::HighForkBranch:
    case ETRIADIstanaExploreV4WindRole::HighForkLeaf:
        return ETRIADIstanaExploreV4TreeForm::HighForkRounded;
    case ETRIADIstanaExploreV4WindRole::ColumnarTrunk:
    case ETRIADIstanaExploreV4WindRole::ColumnarBranch:
    case ETRIADIstanaExploreV4WindRole::ColumnarLeaf:
        return ETRIADIstanaExploreV4TreeForm::ColumnarNarrow;
    case ETRIADIstanaExploreV4WindRole::PalmComposite:
        return ETRIADIstanaExploreV4TreeForm::Palm;
    default:
        return ETRIADIstanaExploreV4TreeForm::Count;
    }
}

UStaticMeshComponent* ATRIADIstanaExploreV4LandscapeActor::ComponentForWindRole(
    ETRIADIstanaExploreV4WindRole WindRole) const
{
    const ETRIADIstanaExploreV4TreeForm TreeForm = TreeFormForWindRole(WindRole);
    if (TreeForm != ETRIADIstanaExploreV4TreeForm::Count)
    {
        return ComponentForTreeForm(TreeForm);
    }
    switch (WindRole)
    {
    case ETRIADIstanaExploreV4WindRole::Shrub:
        return ShrubInstances;
    case ETRIADIstanaExploreV4WindRole::Flower:
        return FlowerInstances;
    case ETRIADIstanaExploreV4WindRole::Understorey:
        return UnderstoreyInstances;
    case ETRIADIstanaExploreV4WindRole::GeometryGrass:
        return GeometryGrassInstances;
    case ETRIADIstanaExploreV4WindRole::CloseTurf:
        return CloseTurfInstances;
    default:
        return nullptr;
    }
}

float ATRIADIstanaExploreV4LandscapeActor::WindResponseForRole(
    ETRIADIstanaExploreV4WindRole Role)
{
    switch (Role)
    {
    case ETRIADIstanaExploreV4WindRole::UmbrellaTrunk:
    case ETRIADIstanaExploreV4WindRole::DomeTrunk:
    case ETRIADIstanaExploreV4WindRole::HighForkTrunk:
    case ETRIADIstanaExploreV4WindRole::ColumnarTrunk:
        return 0.16f;
    case ETRIADIstanaExploreV4WindRole::UmbrellaBranch:
    case ETRIADIstanaExploreV4WindRole::DomeBranch:
    case ETRIADIstanaExploreV4WindRole::HighForkBranch:
    case ETRIADIstanaExploreV4WindRole::ColumnarBranch:
        return 0.48f;
    case ETRIADIstanaExploreV4WindRole::UmbrellaLeaf:
    case ETRIADIstanaExploreV4WindRole::DomeLeaf:
    case ETRIADIstanaExploreV4WindRole::HighForkLeaf:
    case ETRIADIstanaExploreV4WindRole::ColumnarLeaf:
        return 1.0f;
    case ETRIADIstanaExploreV4WindRole::PalmComposite:
        return 0.82f;
    case ETRIADIstanaExploreV4WindRole::Shrub:
        return 0.62f;
    case ETRIADIstanaExploreV4WindRole::Flower:
        return 0.78f;
    case ETRIADIstanaExploreV4WindRole::Understorey:
        return 0.72f;
    case ETRIADIstanaExploreV4WindRole::GeometryGrass:
        return 1.0f;
    case ETRIADIstanaExploreV4WindRole::CloseTurf:
        return 0.32f;
    default:
        return 0.0f;
    }
}

bool ATRIADIstanaExploreV4LandscapeActor::ConfigureRequiredAssets(
    const FTRIADIstanaExploreV4AssetRoster& Assets,
    const TArray<FTRIADIstanaExploreV4WindMaterialBinding>& WindBindings,
    FString& OutError)
{
    const TArray<UStaticMesh*> RequiredOwnedVegetationMeshes = {
        Assets.UmbrellaTree,
        Assets.DomeTree,
        Assets.PalmTree,
        Assets.HeritagePawnBlockerCylinder,
        Assets.Shrub,
        Assets.Flower,
        Assets.Understorey,
        Assets.GeometryGrass,
        Assets.CloseTurf};
    for (const UStaticMesh* Mesh : RequiredOwnedVegetationMeshes)
    {
        if (!Mesh || !FMath::IsFinite(MeshHeightCm(Mesh)) ||
            MeshHeightCm(Mesh) <= 0.01 ||
            !Mesh->GetPathName().StartsWith(
                V4VegetationRuntimeObjectRoot))
        {
            OutError = TEXT("Every V4-owned vegetation mesh must be non-null, finite and contained only in the admitted V4 /Vegetation runtime namespace.");
            return false;
        }
    }
    if (!Assets.HighForkRoundedTree || !Assets.ColumnarNarrowTree ||
        !FMath::IsFinite(MeshHeightCm(Assets.HighForkRoundedTree)) ||
        !FMath::IsFinite(MeshHeightCm(Assets.ColumnarNarrowTree)) ||
        MeshHeightCm(Assets.HighForkRoundedTree) <= 0.01 ||
        MeshHeightCm(Assets.ColumnarNarrowTree) <= 0.01 ||
        Assets.HighForkRoundedTree->GetPathName() !=
            ProtectedHighForkRuntimeObjectPath ||
        Assets.ColumnarNarrowTree->GetPathName() !=
            ProtectedColumnarRuntimeObjectPath)
    {
        OutError = TEXT("High-fork and columnar trees must use only the two exact protected V3/V1 read-only mesh references; no V4 duplicate or broad protected-prefix exception is admitted.");
        return false;
    }
    if (!Assets.PorticoV8 ||
        !FMath::IsFinite(MeshHeightCm(Assets.PorticoV8)) ||
        MeshHeightCm(Assets.PorticoV8) <= 0.01 ||
        !Assets.PorticoV8->GetPathName().StartsWith(
            V4PorticoRuntimeObjectRoot))
    {
        OutError = TEXT("The V8 portico must be non-null, finite and contained only in the V4 /Portico runtime namespace.");
        return false;
    }

    const TArray<UStaticMesh*> FiveTreeMeshes = {
        Assets.UmbrellaTree,
        Assets.DomeTree,
        Assets.HighForkRoundedTree,
        Assets.ColumnarNarrowTree,
        Assets.PalmTree};
    TSet<const UStaticMesh*> DistinctTreeMeshes;
    for (const UStaticMesh* Mesh : FiveTreeMeshes)
    {
        DistinctTreeMeshes.Add(Mesh);
    }
    if (DistinctTreeMeshes.Num() != 5)
    {
        OutError = TEXT("V4 requires exactly five distinct admitted tree-form meshes.");
        return false;
    }

    UmbrellaTreeInstances->SetStaticMesh(Assets.UmbrellaTree);
    DomeTreeInstances->SetStaticMesh(Assets.DomeTree);
    HighForkRoundedTreeInstances->SetStaticMesh(Assets.HighForkRoundedTree);
    ColumnarNarrowTreeInstances->SetStaticMesh(Assets.ColumnarNarrowTree);
    PalmTreeInstances->SetStaticMesh(Assets.PalmTree);
    HeritageUmbrellaInstances->SetStaticMesh(Assets.UmbrellaTree);
    HeritageDomeInstances->SetStaticMesh(Assets.DomeTree);
    HeritageHighForkRoundedInstances->SetStaticMesh(Assets.HighForkRoundedTree);
    HeritageColumnarNarrowInstances->SetStaticMesh(Assets.ColumnarNarrowTree);
    HeritagePalmInstances->SetStaticMesh(Assets.PalmTree);
    HeritageAnchorPawnBlockers->SetStaticMesh(Assets.HeritagePawnBlockerCylinder);
    ShrubInstances->SetStaticMesh(Assets.Shrub);
    FlowerInstances->SetStaticMesh(Assets.Flower);
    UnderstoreyInstances->SetStaticMesh(Assets.Understorey);
    GeometryGrassInstances->SetStaticMesh(Assets.GeometryGrass);
    CloseTurfInstances->SetStaticMesh(Assets.CloseTurf);
    PorticoV8RenderOnlyComponent->SetStaticMesh(Assets.PorticoV8);
    RestorePorticoV8PresentationFailSafe();

    SavedAssetRoster = Assets;
    ConfiguredPorticoMeshPath = Assets.PorticoV8->GetPathName();
    SavedWindBindings.Reset();
    WindMaterialInstances.Reset();
    bAssetsAndWindConfigured = false;
    RecordedExternalV2PawnBlockerCount = 0;
    bRecordedV2TreeVisualsHidden = false;
    bRecordedV3ReplacementTreeVisualsHidden = false;
    bRecordedV3CloseTurfVisualsHidden = false;
    bRecordedV7PorticoHidden = false;
    bRecordedInheritedTerrainAndCollisionPreserved = false;
    bRecordedDistantBuildingsPreserved = false;

    if (WindBindings.Num() != ExpectedWindBindingCount)
    {
        OutError = FString::Printf(
            TEXT("V4 requires exactly %d persistent wind material bindings; got %d."),
            ExpectedWindBindingCount,
            WindBindings.Num());
        return false;
    }

    TSet<uint8> SeenRoles;
    TSet<uint64> SeenComponentSlots;
    for (const FTRIADIstanaExploreV4WindMaterialBinding& Binding : WindBindings)
    {
        const uint8 RoleValue = static_cast<uint8>(Binding.Role);
        UStaticMeshComponent* Component = ComponentForWindRole(Binding.Role);
        if (RoleValue >= static_cast<uint8>(ETRIADIstanaExploreV4WindRole::Count) ||
            SeenRoles.Contains(RoleValue) || !Component || !Binding.BaseMaterial ||
            !Binding.BaseMaterial->GetPathName().StartsWith(
                V4VegetationRuntimeObjectRoot) ||
            !Binding.bUsesPerInstanceLocalPosition ||
            !HasCompleteRuntimeWindParameterRoster(Binding.BaseMaterial) ||
            Binding.MaterialSlotIndex < 0 ||
            Binding.MaterialSlotIndex !=
                ExpectedMaterialSlotForWindRole(Binding.Role) ||
            Binding.MaterialSlotIndex >= Component->GetNumMaterials())
        {
            OutError = TEXT("Every V4 wind role must occur once with a valid slot, persistent base material, and per-instance-local authoring assertion.");
            return false;
        }
        SeenRoles.Add(RoleValue);

        const uint64 ComponentSlotKey =
            (static_cast<uint64>(GetTypeHash(Component)) << 32u) |
            static_cast<uint32>(Binding.MaterialSlotIndex);
        if (SeenComponentSlots.Contains(ComponentSlotKey))
        {
            OutError = TEXT("Distinct trunk, branch, leaf and plant roles may not alias one component material slot.");
            return false;
        }
        SeenComponentSlots.Add(ComponentSlotKey);
    }

    SavedWindBindings = WindBindings;
    SavedWindBindings.Sort([](
        const FTRIADIstanaExploreV4WindMaterialBinding& Left,
        const FTRIADIstanaExploreV4WindMaterialBinding& Right)
    {
        return static_cast<uint8>(Left.Role) < static_cast<uint8>(Right.Role);
    });

    for (const FTRIADIstanaExploreV4WindMaterialBinding& Binding :
         SavedWindBindings)
    {
        UStaticMeshComponent* MainComponent = ComponentForWindRole(Binding.Role);
        check(MainComponent);
        MainComponent->SetMaterial(Binding.MaterialSlotIndex, Binding.BaseMaterial);

        const ETRIADIstanaExploreV4TreeForm TreeForm =
            TreeFormForWindRole(Binding.Role);
        if (TreeForm != ETRIADIstanaExploreV4TreeForm::Count)
        {
            UHierarchicalInstancedStaticMeshComponent* HeritageComponent =
                HeritageComponentForTreeForm(TreeForm);
            check(HeritageComponent);
            if (Binding.MaterialSlotIndex >= HeritageComponent->GetNumMaterials())
            {
                OutError = TEXT("A form-specific Heritage HISM does not retain the matching tree material-slot topology.");
                return false;
            }
            HeritageComponent->SetMaterial(
                Binding.MaterialSlotIndex, Binding.BaseMaterial);
        }
    }

    bAssetsAndWindConfigured = true;
    ClearAllInstances();
    bPopulationConfigured = false;
    if (!ValidateAssetAndWindConfiguration(OutError))
    {
        bAssetsAndWindConfigured = false;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV4LandscapeActor::BuildDeterministicReclassifiedRows(
    const TArray<FTransform>& InheritedTransforms,
    const TArray<FTRIADIstanaExploreV4RasterCue>& RasterCues,
    const TArray<float>& NativeTreeHeightsCm,
    int32 Seed,
    TArray<FTRIADIstanaExploreV4ClassifiedTreeRow>& OutRows,
    FString& OutError)
{
    OutRows.Reset();
    if (InheritedTransforms.Num() != ExpectedInheritedTreeCount ||
        RasterCues.Num() != ExpectedInheritedTreeCount ||
        NativeTreeHeightsCm.Num() != 5)
    {
        OutError = TEXT("Reclassification requires exactly 720 transforms, 720 parallel raster cues, and five native tree heights.");
        return false;
    }
    for (float NativeHeightCm : NativeTreeHeightsCm)
    {
        if (!FMath::IsFinite(NativeHeightCm) || NativeHeightCm <= 1.0f)
        {
            OutError = TEXT("Every reclassified tree form needs a finite positive native height.");
            return false;
        }
    }

    OutRows.Reserve(ExpectedInheritedTreeCount);
    int32 FormCounts[5] = {0, 0, 0, 0, 0};
    for (int32 Index = 0; Index < ExpectedInheritedTreeCount; ++Index)
    {
        const FTransform& Source = InheritedTransforms[Index];
        const FTRIADIstanaExploreV4RasterCue& Cue = RasterCues[Index];
        if (!IsFinitePositiveTransform(Source) ||
            !IsSupportedRasterClass(Cue.BroadRasterClass) ||
            static_cast<uint8>(Cue.HeightBand) >
                static_cast<uint8>(ETRIADIstanaExploreV4CanopyHeightBand::High29To37Meters))
        {
            OutRows.Reset();
            OutError = TEXT("An inherited transform or its broad raster cue is invalid.");
            return false;
        }

        const uint32 Hash = HashTreeRow(Index, Cue, Seed);
        const bool bFrozenV2ColumnarRow =
            Index >= FrozenV2ColumnarStartIndex &&
            Index < FrozenV2ColumnarEndIndex;
        ETRIADIstanaExploreV4TreeForm Form =
            ETRIADIstanaExploreV4TreeForm::HighForkRounded;
        if (bFrozenV2ColumnarRow)
        {
            Form = ETRIADIstanaExploreV4TreeForm::ColumnarNarrow;
        }
        else if (Cue.HeightBand ==
                 ETRIADIstanaExploreV4CanopyHeightBand::Low18To23Meters)
        {
            Form = ETRIADIstanaExploreV4TreeForm::Umbrella;
        }
        else if (Cue.HeightBand ==
                 ETRIADIstanaExploreV4CanopyHeightBand::High29To37Meters)
        {
            Form = ETRIADIstanaExploreV4TreeForm::Dome;
        }
        else if (Cue.HeightBand ==
                 ETRIADIstanaExploreV4CanopyHeightBand::NoData)
        {
            Form = static_cast<ETRIADIstanaExploreV4TreeForm>(Hash % 3u);
        }
        const double Fraction =
            static_cast<double>(Mix32(Hash ^ 0xA53A9E1Du) & 0xFFFFu) /
            65535.0;
        const int32 FormIndex = static_cast<int32>(Form);
        FVector TargetScale = Source.GetScale3D();
        if (Form != ETRIADIstanaExploreV4TreeForm::ColumnarNarrow)
        {
            double TargetHeightMeters = 28.3;
            if (Form == ETRIADIstanaExploreV4TreeForm::Umbrella)
            {
                const double MaximumUmbrellaHeightMeters =
                    Cue.HeightBand ==
                            ETRIADIstanaExploreV4CanopyHeightBand::Low18To23Meters
                        ? 23.0
                        : 24.28810830713951;
                TargetHeightMeters = FMath::Lerp(
                    20.0, MaximumUmbrellaHeightMeters, Fraction);
            }
            else if (Form == ETRIADIstanaExploreV4TreeForm::Dome)
            {
                TargetHeightMeters = 36.5;
            }
            const double UniformScale =
                TargetHeightMeters * CentimetersPerMeter /
                NativeTreeHeightsCm[FormIndex];
            TargetScale = FVector(UniformScale);
        }
        FTRIADIstanaExploreV4ClassifiedTreeRow Row;
        Row.SourceIndex = Index;
        Row.Form = Form;
        Row.WorldTransform = bFrozenV2ColumnarRow
            ? Source
            : FTransform(
                Source.GetRotation(),
                Source.GetTranslation(),
                TargetScale);
        const bool bPreservesRequiredSourceTransform = bFrozenV2ColumnarRow
            ? Row.WorldTransform.Equals(Source, 0.0f)
            : PreservesSourceTranslationAndRotation(Row.WorldTransform, Source);
        if (!IsFinitePositiveTransform(Row.WorldTransform) ||
            !bPreservesRequiredSourceTransform)
        {
            OutRows.Reset();
            OutError = TEXT("A deterministic reclassified tree row failed finite transform, exact translation/full-rotation preservation, or the full frozen-columnar transform gate.");
            return false;
        }
        ++FormCounts[FormIndex];
        OutRows.Add(Row);
    }

    for (int32 FormIndex = 0; FormIndex < 3; ++FormIndex)
    {
        if (FormCounts[FormIndex] <= 0)
        {
            OutRows.Reset();
            OutError = TEXT("The deterministic 720-row roster did not exercise every admitted runtime tree form.");
            return false;
        }
    }
    if (FormCounts[static_cast<int32>(ETRIADIstanaExploreV4TreeForm::ColumnarNarrow)] !=
            FrozenV2ColumnarCount ||
        FormCounts[static_cast<int32>(ETRIADIstanaExploreV4TreeForm::Palm)] != 0)
    {
        OutRows.Reset();
        OutError = TEXT("V4 must preserve exactly 232 frozen V2 columnar transforms and place zero low-poly palms in free-roam.");
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV4LandscapeActor::ClearAllInstances()
{
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components = {
        UmbrellaTreeInstances,
        DomeTreeInstances,
        HighForkRoundedTreeInstances,
        ColumnarNarrowTreeInstances,
        PalmTreeInstances,
        HeritageUmbrellaInstances,
        HeritageDomeInstances,
        HeritageHighForkRoundedInstances,
        HeritageColumnarNarrowInstances,
        HeritagePalmInstances,
        HeritageAnchorPawnBlockers,
        ShrubInstances,
        FlowerInstances,
        UnderstoreyInstances,
        GeometryGrassInstances,
        CloseTurfInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (Component)
        {
            Component->ClearInstances();
        }
    }
    PreservedInheritedV2TreeWorldTransforms.Reset();
    PreservedRasterCues.Reset();
    PreservedHeritageAnchors.Reset();
    PreservedCloseTurfWorldTransforms.Reset();
    PreservedTreeFormBySourceIndex.Reset();
    RecordedMainTreeFormCounts.Reset();
    RecordedShrubInstanceCount = 0;
    RecordedFlowerInstanceCount = 0;
    RecordedUnderstoreyInstanceCount = 0;
    RecordedGeometryGrassInstanceCount = 0;
    RecordedCloseTurfInstanceCount = 0;
    bPopulationConfigured = false;
}

bool ATRIADIstanaExploreV4LandscapeActor::PopulateDeterministicLandscape(
    const FTRIADIstanaExploreV4PopulationInput& Input,
    FString& OutError)
{
    if (!bAssetsAndWindConfigured ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001f))
    {
        OutError = TEXT("Configure the complete V4 roster on an identity actor before population.");
        return false;
    }
    if (Input.HeritageAnchors.Num() != ExpectedHeritageAnchorCount ||
        Input.ShrubTransforms.Num() != ExpectedShrubCount ||
        Input.FlowerTransforms.Num() != ExpectedFlowerCount ||
        Input.UnderstoreyTransforms.Num() != ExpectedUnderstoreyCount ||
        Input.GeometryGrassTransforms.Num() != ExpectedGeometryGrassCount ||
        Input.CloseTurfTransforms.Num() != ExpectedCloseTurfCount)
    {
        OutError = FString::Printf(
            TEXT("V4 exact population census changed: heritage=%d shrub=%d flower=%d understorey=%d geometryGrass=%d closeTurf=%d."),
            Input.HeritageAnchors.Num(),
            Input.ShrubTransforms.Num(),
            Input.FlowerTransforms.Num(),
            Input.UnderstoreyTransforms.Num(),
            Input.GeometryGrassTransforms.Num(),
            Input.CloseTurfTransforms.Num());
        return false;
    }

    const TArray<float> NativeTreeHeightsCm = {
        static_cast<float>(MeshHeightCm(SavedAssetRoster.UmbrellaTree)),
        static_cast<float>(MeshHeightCm(SavedAssetRoster.DomeTree)),
        static_cast<float>(MeshHeightCm(SavedAssetRoster.HighForkRoundedTree)),
        static_cast<float>(MeshHeightCm(SavedAssetRoster.ColumnarNarrowTree)),
        static_cast<float>(MeshHeightCm(SavedAssetRoster.PalmTree))};
    TArray<FTRIADIstanaExploreV4ClassifiedTreeRow> Rows;
    if (!BuildDeterministicReclassifiedRows(
            Input.InheritedV2TreeWorldTransforms,
            Input.RasterCues,
            NativeTreeHeightsCm,
            DeterministicPlacementSeed,
            Rows,
            OutError))
    {
        return false;
    }

    for (int32 Index = 0; Index < ExpectedHeritageAnchorCount; ++Index)
    {
        const FTRIADIstanaExploreV4HeritageAnchor& Anchor =
            Input.HeritageAnchors[Index];
        const FExpectedHeritageAnchor& Expected = ExpectedHeritageAnchors[Index];
        if (Anchor.PublicRecordId != Expected.PublicRecordId ||
            !FMath::IsNearlyEqual(Anchor.PublishedHeightMeters, Expected.HeightMeters, 0.001f) ||
            !FMath::IsNearlyEqual(Anchor.PublishedGirthMeters, Expected.GirthMeters, 0.001f) ||
            Anchor.FormHint != Expected.Form ||
            Anchor.bFormComesFromPublishedQualitativeHint !=
                Expected.bFormComesFromPublishedQualitativeHint ||
            !Anchor.bPositionComesFromPublishedRecord ||
            !Anchor.bVisualIsSilhouetteProxy ||
            Anchor.LocalGroundLocationCm.ContainsNaN() ||
            !FMath::IsFinite(Anchor.PublishedHeightMeters) ||
            !FMath::IsFinite(Anchor.PublishedGirthMeters))
        {
            OutError = TEXT("The exact ordered nine-anchor public height/girth and chosen proxy-form provenance contract changed.");
            return false;
        }
        const FTransform LocationProbe(
            FQuat::Identity, Anchor.LocalGroundLocationCm, FVector::OneVector);
        if (!IsInsideOneKilometre(LocationProbe) ||
            IsInsideWoodyCeremonialClearance(
                LocationProbe, ClearCeremonialAxisHalfWidthMeters) ||
            IsInsideFountainClearance(
                LocationProbe, FountainCenterMeters,
                FountainClearanceRadiusMeters))
        {
            OutError = TEXT("A Heritage anchor escaped the one-kilometre public frame or entered a protected ceremonial/fountain clearance.");
            return false;
        }
    }

    const auto ValidateAdditionTransforms = [this](
        const TArray<FTransform>& Transforms,
        bool bRequireWoodyAxisClearance,
        const TCHAR* Label,
        FString& Error) -> bool
    {
        for (const FTransform& Transform : Transforms)
        {
            if (!IsFinitePositiveTransform(Transform) ||
                !IsInsideOneKilometre(Transform) ||
                IsInsideFountainClearance(
                    Transform, FountainCenterMeters,
                    FountainClearanceRadiusMeters) ||
                (bRequireWoodyAxisClearance &&
                    IsInsideWoodyCeremonialClearance(
                        Transform, ClearCeremonialAxisHalfWidthMeters)))
            {
                Error = FString::Printf(
                    TEXT("A %s transform is non-finite, outside one kilometre, or inside a protected clearance."),
                    Label);
                return false;
            }
        }
        return true;
    };
    if (!ValidateAdditionTransforms(Input.ShrubTransforms, true, TEXT("shrub"), OutError) ||
        !ValidateAdditionTransforms(Input.FlowerTransforms, true, TEXT("flower"), OutError) ||
        !ValidateAdditionTransforms(Input.UnderstoreyTransforms, true, TEXT("understorey"), OutError) ||
        !ValidateAdditionTransforms(Input.GeometryGrassTransforms, true, TEXT("geometry-grass"), OutError) ||
        !ValidateAdditionTransforms(Input.CloseTurfTransforms, false, TEXT("close-turf replacement"), OutError))
    {
        return false;
    }

    TArray<FTransform> MainTreeWorldTransformsByForm[5];
    TArray<uint8> NewTreeFormBySourceIndex;
    NewTreeFormBySourceIndex.SetNumUninitialized(ExpectedInheritedTreeCount);
    TArray<int32> NewMainTreeFormCounts;
    NewMainTreeFormCounts.Init(0, 5);
    for (const FTRIADIstanaExploreV4ClassifiedTreeRow& Row : Rows)
    {
        const int32 FormIndex = static_cast<int32>(Row.Form);
        MainTreeWorldTransformsByForm[FormIndex].Add(Row.WorldTransform);
        NewTreeFormBySourceIndex[Row.SourceIndex] =
            static_cast<uint8>(Row.Form);
        ++NewMainTreeFormCounts[FormIndex];
    }

    const UStaticMesh* BlockerMesh =
        HeritageAnchorPawnBlockers->GetStaticMesh();
    const FVector BlockerSizeCm = BlockerMesh
        ? BlockerMesh->GetBounds().BoxExtent * 2.0
        : FVector::ZeroVector;
    const double NativeBlockerDiameterCm =
        FMath::Max(BlockerSizeCm.X, BlockerSizeCm.Y);
    if (NativeBlockerDiameterCm <= 0.01 || BlockerSizeCm.Z <= 0.01)
    {
        OutError = TEXT("The Heritage Pawn blocker cylinder has invalid native bounds.");
        return false;
    }

    TArray<FTransform> HeritageWorldTransformsByForm[5];
    TArray<FTransform> HeritageBlockerWorldTransforms;
    HeritageBlockerWorldTransforms.Reserve(ExpectedHeritageAnchorCount);
    for (int32 Index = 0; Index < ExpectedHeritageAnchorCount; ++Index)
    {
        const FTRIADIstanaExploreV4HeritageAnchor& Anchor =
            Input.HeritageAnchors[Index];
        UHierarchicalInstancedStaticMeshComponent* VisualComponent =
            HeritageComponentForTreeForm(Anchor.FormHint);
        UStaticMesh* VisualMesh = VisualComponent
            ? VisualComponent->GetStaticMesh()
            : nullptr;
        const double NativeHeightCm = MeshHeightCm(VisualMesh);
        if (!VisualComponent || NativeHeightCm <= 0.01)
        {
            OutError = TEXT("A Heritage form-specific silhouette mesh has invalid native height.");
            return false;
        }

        const uint32 AnchorHash = Mix32(
            static_cast<uint32>(DeterministicPlacementSeed) ^
            (static_cast<uint32>(Index) * 0x6D2B79F5u));
        const double YawDegrees =
            static_cast<double>(AnchorHash % 36000u) / 100.0;
        const double UniformScale =
            Anchor.PublishedHeightMeters * CentimetersPerMeter /
            NativeHeightCm;
        const FTransform VisualTransform(
            FRotator(0.0, YawDegrees, 0.0),
            Anchor.LocalGroundLocationCm,
            FVector(UniformScale));
        const double PublishedTrunkDiameterCm =
            Anchor.PublishedGirthMeters * CentimetersPerMeter / PI;
        FVector BlockerLocation = Anchor.LocalGroundLocationCm;
        BlockerLocation.Z += HeritageBlockerHeightCm * 0.5f;
        const FTransform BlockerTransform(
            FRotator(0.0, YawDegrees, 0.0),
            BlockerLocation,
            FVector(
                PublishedTrunkDiameterCm / NativeBlockerDiameterCm,
                PublishedTrunkDiameterCm / NativeBlockerDiameterCm,
                HeritageBlockerHeightCm / BlockerSizeCm.Z));
        if (!IsFinitePositiveTransform(VisualTransform) ||
            !IsFinitePositiveTransform(BlockerTransform))
        {
            OutError = TEXT("A Heritage silhouette or separate Pawn-only blocker transform is invalid.");
            return false;
        }
        HeritageWorldTransformsByForm[
            static_cast<int32>(Anchor.FormHint)].Add(VisualTransform);
        HeritageBlockerWorldTransforms.Add(BlockerTransform);
    }

    struct FOrderedHismBulkPopulation
    {
        UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
        const TArray<FTransform>* WorldTransforms = nullptr;
    };
    const TArray<FOrderedHismBulkPopulation> BulkPopulations = {
        {UmbrellaTreeInstances, &MainTreeWorldTransformsByForm[0]},
        {DomeTreeInstances, &MainTreeWorldTransformsByForm[1]},
        {HighForkRoundedTreeInstances, &MainTreeWorldTransformsByForm[2]},
        {ColumnarNarrowTreeInstances, &MainTreeWorldTransformsByForm[3]},
        {PalmTreeInstances, &MainTreeWorldTransformsByForm[4]},
        {HeritageUmbrellaInstances, &HeritageWorldTransformsByForm[0]},
        {HeritageDomeInstances, &HeritageWorldTransformsByForm[1]},
        {HeritageHighForkRoundedInstances, &HeritageWorldTransformsByForm[2]},
        {HeritageColumnarNarrowInstances, &HeritageWorldTransformsByForm[3]},
        {HeritagePalmInstances, &HeritageWorldTransformsByForm[4]},
        {HeritageAnchorPawnBlockers, &HeritageBlockerWorldTransforms},
        {ShrubInstances, &Input.ShrubTransforms},
        {FlowerInstances, &Input.FlowerTransforms},
        {UnderstoreyInstances, &Input.UnderstoreyTransforms},
        {GeometryGrassInstances, &Input.GeometryGrassTransforms},
        {CloseTurfInstances, &Input.CloseTurfTransforms}};
    if (BulkPopulations.Num() != 16)
    {
        OutError = TEXT("The exact V4 bulk-population roster must contain 16 HISMs.");
        return false;
    }
    for (const FOrderedHismBulkPopulation& Entry : BulkPopulations)
    {
        if (!Entry.Component || !Entry.WorldTransforms)
        {
            OutError = TEXT("The exact V4 bulk-population HISM roster is incomplete.");
            return false;
        }
    }

    for (const FOrderedHismBulkPopulation& Entry : BulkPopulations)
    {
        Entry.Component->bAutoRebuildTreeOnInstanceChanges = false;
    }
    ClearAllInstances();
    const auto FailManualHismPopulation =
        [this, &BulkPopulations, &OutError](const TCHAR* Message) -> bool
    {
        ClearAllInstances();
        for (const FOrderedHismBulkPopulation& Entry : BulkPopulations)
        {
            Entry.Component->bAutoRebuildTreeOnInstanceChanges = true;
        }
        OutError = Message;
        return false;
    };

    PreservedInheritedV2TreeWorldTransforms =
        Input.InheritedV2TreeWorldTransforms;
    PreservedRasterCues = Input.RasterCues;
    PreservedHeritageAnchors = Input.HeritageAnchors;
    PreservedCloseTurfWorldTransforms = Input.CloseTurfTransforms;
    PreservedTreeFormBySourceIndex = MoveTemp(NewTreeFormBySourceIndex);
    RecordedMainTreeFormCounts = MoveTemp(NewMainTreeFormCounts);

    for (const FOrderedHismBulkPopulation& Entry : BulkPopulations)
    {
        if (!Entry.WorldTransforms->IsEmpty())
        {
            Entry.Component->AddInstances(
                *Entry.WorldTransforms, false, true, false);
        }
    }
    for (const FOrderedHismBulkPopulation& Entry : BulkPopulations)
    {
        if (Entry.Component->GetInstanceCount() !=
            Entry.WorldTransforms->Num())
        {
            return FailManualHismPopulation(
                TEXT("A V4 HISM bulk add did not retain its exact ordered instance count."));
        }
    }
    for (const FOrderedHismBulkPopulation& Entry : BulkPopulations)
    {
        if (!Entry.Component->BuildTreeIfOutdated(false, true) ||
            Entry.Component->IsAsyncBuilding() ||
            !Entry.Component->IsTreeFullyBuilt())
        {
            return FailManualHismPopulation(
                TEXT("A V4 HISM failed its sole forced synchronous tree build."));
        }
    }
    for (const FOrderedHismBulkPopulation& Entry : BulkPopulations)
    {
        if (Entry.Component->IsAsyncBuilding() ||
            !Entry.Component->IsTreeFullyBuilt())
        {
            return FailManualHismPopulation(
                TEXT("A V4 HISM remained asynchronous or not fully built after synchronous finalization."));
        }
        Entry.Component->bAutoRebuildTreeOnInstanceChanges = true;
    }

    RecordedShrubInstanceCount = Input.ShrubTransforms.Num();
    RecordedFlowerInstanceCount = Input.FlowerTransforms.Num();
    RecordedUnderstoreyInstanceCount = Input.UnderstoreyTransforms.Num();
    RecordedGeometryGrassInstanceCount = Input.GeometryGrassTransforms.Num();
    RecordedCloseTurfInstanceCount = Input.CloseTurfTransforms.Num();
    bPopulationConfigured = true;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV4LandscapeActor::RecordExternalTargetMapState(
    int32 PreservedV2PawnBlockerCount,
    bool bV2TreeVisualsHidden,
    bool bV3ReplacementTreeVisualsHidden,
    bool bV3CloseTurfVisualsHidden,
    bool bV7PorticoHidden,
    bool bInheritedTerrainAndCollisionPreserved,
    bool bDistantBuildingsPreserved,
    FString& OutError)
{
    if (PreservedV2PawnBlockerCount != ExpectedInheritedTreeCount ||
        !bV2TreeVisualsHidden || !bV3ReplacementTreeVisualsHidden ||
        !bV3CloseTurfVisualsHidden || !bV7PorticoHidden ||
        !bInheritedTerrainAndCollisionPreserved ||
        !bDistantBuildingsPreserved)
    {
        OutError = TEXT("The V4 target-map handoff must preserve 720 external V2 Pawn blockers, terrain/collision and buildings while hiding superseded V2/V3 tree, V3 turf and V7 portico visuals.");
        return false;
    }
    RecordedExternalV2PawnBlockerCount = PreservedV2PawnBlockerCount;
    bRecordedV2TreeVisualsHidden = bV2TreeVisualsHidden;
    bRecordedV3ReplacementTreeVisualsHidden =
        bV3ReplacementTreeVisualsHidden;
    bRecordedV3CloseTurfVisualsHidden = bV3CloseTurfVisualsHidden;
    bRecordedV7PorticoHidden = bV7PorticoHidden;
    bRecordedInheritedTerrainAndCollisionPreserved =
        bInheritedTerrainAndCollisionPreserved;
    bRecordedDistantBuildingsPreserved = bDistantBuildingsPreserved;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV4LandscapeActor::ValidateAssetAndWindConfiguration(
    FString& OutError) const
{
    if (!bAssetsAndWindConfigured ||
        SavedWindBindings.Num() != ExpectedWindBindingCount ||
        ConfiguredPorticoMeshPath.IsEmpty() ||
        !FMath::IsFinite(BaseWindStrengthCm) ||
        !FMath::IsFinite(GustPeakStrengthCm) ||
        !FMath::IsFinite(WindSpeed) ||
        !FMath::IsFinite(RecoveryFrequencyHz) ||
        !FMath::IsFinite(RecoveryDampingRatio) ||
        !FMath::IsFinite(FixedWindStepSeconds) ||
        !FMath::IsFinite(PrevailingWindDirection.X) ||
        !FMath::IsFinite(PrevailingWindDirection.Y) ||
        BaseWindStrengthCm < 0.0f || GustPeakStrengthCm <= BaseWindStrengthCm ||
        GustPeakStrengthCm > 150.0f || WindSpeed <= 0.0f ||
        RecoveryFrequencyHz <= 0.0f || RecoveryDampingRatio <= 0.0f ||
        RecoveryDampingRatio >= 1.0f || FixedWindStepSeconds <= 0.0f ||
        PrevailingWindDirection.SizeSquared() <= 0.01f)
    {
        OutError = TEXT("The complete V4 asset/wind roster or finite underdamped fixed-step wind policy is not configured.");
        return false;
    }

    const bool bV5DTreeRuntimeOverrideTag =
        Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::RuntimeOverrideTag());
    FString V5DTreeRuntimeOverrideReport;
    const bool bV5DTreeRuntimeOverride = bV5DTreeRuntimeOverrideTag &&
        ATRIADIstanaExploreV5DTreeRealismActor::
            ValidateActiveRuntimeOverrideForSourceActor(
                this,
                V5DTreeRuntimeOverrideReport);
    if (bV5DTreeRuntimeOverrideTag && !bV5DTreeRuntimeOverride)
    {
        OutError = TEXT("A tagged V5D tree visual override failed its exact source/census/LOD/simulation-isolation gate: ") +
            V5DTreeRuntimeOverrideReport;
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* MainTreeComponents[] = {
        UmbrellaTreeInstances,
        DomeTreeInstances,
        HighForkRoundedTreeInstances,
        ColumnarNarrowTreeInstances,
        PalmTreeInstances};
    const UHierarchicalInstancedStaticMeshComponent* HeritageTreeComponents[] = {
        HeritageUmbrellaInstances,
        HeritageDomeInstances,
        HeritageHighForkRoundedInstances,
        HeritageColumnarNarrowInstances,
        HeritagePalmInstances};
    const UStaticMesh* ExpectedTreeMeshes[] = {
        SavedAssetRoster.UmbrellaTree,
        SavedAssetRoster.DomeTree,
        SavedAssetRoster.HighForkRoundedTree,
        SavedAssetRoster.ColumnarNarrowTree,
        SavedAssetRoster.PalmTree};
    const TArray<TArray<FName>> ExpectedTreeSlotNames = {
        {TEXT("island_tree_02"), TEXT("island_tree_02_leaves"), TEXT("island_tree_02_branches")},
        {TEXT("tree_small_02_branches"), TEXT("tree_small_02_leaves"), TEXT("tree_small_02_trunk")},
        {TEXT("island_tree_01"), TEXT("island_tree_01_leaves"), TEXT("island_tree_01_branches")},
        {TEXT("jacaranda_tree_branches"), TEXT("jacaranda_tree_trunk"), TEXT("jacaranda_tree_leaves")},
        {TEXT("Atlas")}};
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const int32 ExpectedMinLod = bV5DTreeRuntimeOverride
            ? ATRIADIstanaExploreV5DTreeRealismActor::
                ExpectedRuntimeTreeMinimumLod()
            : 1;
        const int32 ExpectedForcedLodModel = bV5DTreeRuntimeOverride
            ? ATRIADIstanaExploreV5DTreeRealismActor::
                ExpectedRuntimeTreeForcedLodModel()
            : 0;
        const int32 ExpectedMaterialSlots = Index == 4 ? 1 : 3;
        const UStaticMesh* ActiveTreeMesh = MainTreeComponents[Index]
            ? MainTreeComponents[Index]->GetStaticMesh()
            : nullptr;
        for (const UHierarchicalInstancedStaticMeshComponent* Component :
             {MainTreeComponents[Index], HeritageTreeComponents[Index]})
        {
            if (!ExpectedTreeMeshes[Index] || !Component ||
                Component->GetStaticMesh() != ActiveTreeMesh ||
                (!bV5DTreeRuntimeOverride &&
                    ActiveTreeMesh != ExpectedTreeMeshes[Index]) ||
                !IsAdmittedTreeMeshPath(ExpectedTreeMeshes[Index], Index) ||
                Component->GetNumMaterials() != ExpectedMaterialSlots ||
                !Component->bOverrideMinLOD ||
                Component->MinLOD != ExpectedMinLod ||
                Component->ForcedLodModel != ExpectedForcedLodModel)
            {
                OutError = TEXT("A main/Heritage tree-form mesh or exact component MinLOD/forced-LOD policy changed.");
                return false;
            }
        }
        if (ExpectedTreeMeshes[Index]->GetNumLODs() < 2 ||
            ExpectedTreeMeshes[Index]->GetMinLODIdx() < 1)
        {
            OutError = TEXT("A V4 tree derivative still permits runtime selection of source LOD0.");
            return false;
        }
        const TArray<FStaticMaterial>& StaticMaterials =
            ActiveTreeMesh->GetStaticMaterials();
        if (StaticMaterials.Num() != ExpectedTreeSlotNames[Index].Num())
        {
            OutError = TEXT("A V4 tree derivative material-slot census changed.");
            return false;
        }
        for (int32 Slot = 0; Slot < StaticMaterials.Num(); ++Slot)
        {
            bool bImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
            bImportedSlotNameMatches =
                StaticMaterials[Slot].ImportedMaterialSlotName ==
                    ExpectedTreeSlotNames[Index][Slot];
#endif
            if (StaticMaterials[Slot].MaterialSlotName !=
                    ExpectedTreeSlotNames[Index][Slot] ||
                !bImportedSlotNameMatches)
            {
                OutError = TEXT("A V4 tree derivative exact runtime material-slot order changed.");
                return false;
            }
        }
    }

    const UHierarchicalInstancedStaticMeshComponent* PlantComponents[] = {
        ShrubInstances,
        FlowerInstances,
        UnderstoreyInstances,
        GeometryGrassInstances,
        CloseTurfInstances};
    const UStaticMesh* ExpectedPlantMeshes[] = {
        SavedAssetRoster.Shrub,
        SavedAssetRoster.Flower,
        SavedAssetRoster.Understorey,
        SavedAssetRoster.GeometryGrass,
        SavedAssetRoster.CloseTurf};
    const FName ExpectedPlantSlotNames[] = {
        TEXT("shrub_04"),
        TEXT("periwinkle_plant"),
        TEXT("calathea_orbifolia_01"),
        TEXT("grass_medium_01"),
        NAME_None};
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const int32 ExpectedMinLod = 1;
        const UHierarchicalInstancedStaticMeshComponent* Component =
            PlantComponents[Index];
        if (!ExpectedPlantMeshes[Index] || !Component ||
            Component->GetStaticMesh() != ExpectedPlantMeshes[Index] ||
            !ExpectedPlantMeshes[Index]->GetPathName().StartsWith(
                V4VegetationRuntimeObjectRoot) ||
            Component->GetNumMaterials() != 1 ||
            !Component->bOverrideMinLOD || Component->MinLOD != ExpectedMinLod ||
            Component->ForcedLodModel != 0 ||
            ExpectedPlantMeshes[Index]->GetNumLODs() < 2 ||
            ExpectedPlantMeshes[Index]->GetMinLODIdx() < 1)
        {
            OutError = TEXT("A V4 shrub/flower/understorey/grass mesh or exact MinLOD policy changed.");
            return false;
        }
        const TArray<FStaticMaterial>& StaticMaterials =
            ExpectedPlantMeshes[Index]->GetStaticMaterials();
        bool bImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
        bImportedSlotNameMatches =
            StaticMaterials.Num() == 1 &&
            (ExpectedPlantSlotNames[Index] == NAME_None ||
             StaticMaterials[0].ImportedMaterialSlotName ==
                 ExpectedPlantSlotNames[Index]);
#endif
        if (StaticMaterials.Num() != 1 ||
            (ExpectedPlantSlotNames[Index] != NAME_None &&
                StaticMaterials[0].MaterialSlotName !=
                    ExpectedPlantSlotNames[Index]) ||
            !bImportedSlotNameMatches)
        {
            OutError = TEXT("A V4 single-slot plant derivative exact runtime material name changed.");
            return false;
        }
    }

    if (!HeritageAnchorPawnBlockers ||
        HeritageAnchorPawnBlockers->GetStaticMesh() !=
            SavedAssetRoster.HeritagePawnBlockerCylinder ||
        !SavedAssetRoster.HeritagePawnBlockerCylinder ||
        !SavedAssetRoster.HeritagePawnBlockerCylinder->GetPathName().StartsWith(
            V4VegetationRuntimeObjectRoot))
    {
        OutError = TEXT("The separate Heritage Pawn blocker mesh escaped its exact V4 vegetation roster binding.");
        return false;
    }

    if (!SavedAssetRoster.PorticoV8 || !PorticoV8RenderOnlyComponent ||
        PorticoV8RenderOnlyComponent->GetStaticMesh() != SavedAssetRoster.PorticoV8 ||
        !SavedAssetRoster.PorticoV8->GetPathName().StartsWith(
            V4PorticoRuntimeObjectRoot) ||
        PorticoV8RenderOnlyComponent->GetStaticMesh()->GetPathName() !=
            ConfiguredPorticoMeshPath ||
        CountTriangles(PorticoV8RenderOnlyComponent->GetStaticMesh(), 0) !=
            ExpectedPorticoTriangleCount ||
        PorticoV8RenderOnlyComponent->GetNumMaterials() !=
            ExpectedPorticoMaterialCount ||
        PorticoV8RenderOnlyComponent->GetStaticMesh()->GetStaticMaterials().Num() !=
            ExpectedPorticoMaterialCount)
    {
        OutError = TEXT("The identity V8 portico must retain its configured mesh path, 6,592 triangles and seven material slots.");
        return false;
    }
    const FStaticMeshRenderData* PorticoRenderData =
        PorticoV8RenderOnlyComponent->GetStaticMesh()->GetRenderData();
    if (!PorticoRenderData || PorticoRenderData->LODResources.IsEmpty() ||
        PorticoRenderData->LODResources[0].Sections.Num() !=
            ExpectedPorticoMaterialCount)
    {
        OutError = TEXT("The V8 portico must retain exactly seven LOD0 material sections.");
        return false;
    }
    for (int32 Slot = 0; Slot < ExpectedPorticoMaterialCount; ++Slot)
    {
        const FStaticMaterial& StaticMaterial =
            PorticoV8RenderOnlyComponent->GetStaticMesh()->GetStaticMaterials()[Slot];
        const UMaterialInterface* BoundMaterial =
            PorticoV8RenderOnlyComponent->GetMaterial(Slot);
        bool bImportedSlotNameMatches = true;
        FString ImportedSlotDescription(
            TEXT("<not serialized in cooked builds>"));
#if WITH_EDITORONLY_DATA
        bImportedSlotNameMatches =
            StaticMaterial.ImportedMaterialSlotName ==
                ExpectedPorticoMaterials[Slot];
        ImportedSlotDescription =
            StaticMaterial.ImportedMaterialSlotName.ToString();
#endif
        if (StaticMaterial.MaterialSlotName != ExpectedPorticoMaterials[Slot] ||
            !bImportedSlotNameMatches ||
            !BoundMaterial ||
            BoundMaterial->GetPathName() !=
                ExpectedPorticoMaterialObjectPath(ExpectedPorticoMaterials[Slot]))
        {
            OutError = FString::Printf(
                TEXT("The exact V8 portico imported material order/name/binding changed at slot %d; expectedSlot=%s actualSlot=%s actualImportedSlot=%s expectedMaterial=%s actualMaterial=%s."),
                Slot,
                *ExpectedPorticoMaterials[Slot].ToString(),
                *StaticMaterial.MaterialSlotName.ToString(),
                *ImportedSlotDescription,
                *ExpectedPorticoMaterialObjectPath(ExpectedPorticoMaterials[Slot]),
                BoundMaterial ? *BoundMaterial->GetPathName() : TEXT("<null>"));
            return false;
        }
    }
    TSet<int32> SeenPorticoSectionMaterials;
    for (const FStaticMeshSection& Section :
         PorticoRenderData->LODResources[0].Sections)
    {
        if (Section.MaterialIndex < 0 ||
            Section.MaterialIndex >= ExpectedPorticoMaterialCount ||
            SeenPorticoSectionMaterials.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                ExpectedPorticoTrianglesByMaterial[Section.MaterialIndex])
        {
            OutError = TEXT("The exact seven-section V8 portico material/triangle topology changed.");
            return false;
        }
        SeenPorticoSectionMaterials.Add(Section.MaterialIndex);
    }

    TSet<uint8> SeenRoles;
    for (const FTRIADIstanaExploreV4WindMaterialBinding& Binding :
         SavedWindBindings)
    {
        UStaticMeshComponent* MainComponent = ComponentForWindRole(Binding.Role);
        const uint8 RoleValue = static_cast<uint8>(Binding.Role);
        const ETRIADIstanaExploreV4TreeForm TreeForm =
            TreeFormForWindRole(Binding.Role);
        // A validated V5D tree override replaces the V4 tree slots with exact
        // response MIDs whose direct parents are the isolated response assets,
        // not Binding.BaseMaterial. The admission above already proves every
        // response MID, its snapshotted V4 source parent and its wind values.
        // Keep the original direct-parent gate for every non-tree wind role and
        // whenever the exact V5D override is absent or invalid.
        const bool bTreeMaterialBindingDelegatedToExactV5DOverride =
            bV5DTreeRuntimeOverride &&
            TreeForm != ETRIADIstanaExploreV4TreeForm::Count;
        if (!MainComponent || !Binding.BaseMaterial ||
            !Binding.BaseMaterial->GetPathName().StartsWith(
                V4VegetationRuntimeObjectRoot) ||
            !Binding.bUsesPerInstanceLocalPosition ||
            !HasCompleteRuntimeWindParameterRoster(Binding.BaseMaterial) ||
            SeenRoles.Contains(RoleValue) ||
            Binding.MaterialSlotIndex < 0 ||
            Binding.MaterialSlotIndex !=
                ExpectedMaterialSlotForWindRole(Binding.Role) ||
            Binding.MaterialSlotIndex >= MainComponent->GetNumMaterials() ||
            (!bTreeMaterialBindingDelegatedToExactV5DOverride &&
                !HasBaseOrRuntimeMidParent(
                    MainComponent->GetMaterial(Binding.MaterialSlotIndex),
                    Binding.BaseMaterial)))
        {
            OutError = TEXT("A persistent V4 base-material binding, role, slot, or per-instance-local assertion changed.");
            return false;
        }
        SeenRoles.Add(RoleValue);

        if (TreeForm != ETRIADIstanaExploreV4TreeForm::Count)
        {
            const UHierarchicalInstancedStaticMeshComponent* HeritageComponent =
                HeritageComponentForTreeForm(TreeForm);
            if (!HeritageComponent ||
                Binding.MaterialSlotIndex >= HeritageComponent->GetNumMaterials() ||
                (!bTreeMaterialBindingDelegatedToExactV5DOverride &&
                    !HasBaseOrRuntimeMidParent(
                        HeritageComponent->GetMaterial(Binding.MaterialSlotIndex),
                        Binding.BaseMaterial)))
            {
                OutError = TEXT("A form-specific Heritage HISM lost its matching persistent wind base binding.");
                return false;
            }
        }
    }

    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV4LandscapeActor::PostLoad()
{
    Super::PostLoad();
    UStaticMesh* SavedPorticoV5C =
        bPorticoV5CPresentationActive && PorticoV5CRenderOnlyComponent
        ? PorticoV5CRenderOnlyComponent->GetStaticMesh()
        : nullptr;
    if (SavedPorticoV5C)
    {
        FString IgnoredPresentationError;
        ConfigurePorticoV5CPresentation(
            SavedPorticoV5C, IgnoredPresentationError);
    }
    else
    {
        RestorePorticoV8PresentationFailSafe();
    }
    WindMaterialInstances.Reset();
    if (!bAssetsAndWindConfigured)
    {
        return;
    }
    for (const FTRIADIstanaExploreV4WindMaterialBinding& Binding :
         SavedWindBindings)
    {
        if (UStaticMeshComponent* MainComponent =
                ComponentForWindRole(Binding.Role))
        {
            MainComponent->SetMaterial(
                Binding.MaterialSlotIndex, Binding.BaseMaterial);
        }
        const ETRIADIstanaExploreV4TreeForm TreeForm =
            TreeFormForWindRole(Binding.Role);
        if (TreeForm != ETRIADIstanaExploreV4TreeForm::Count)
        {
            if (UHierarchicalInstancedStaticMeshComponent* HeritageComponent =
                    HeritageComponentForTreeForm(TreeForm))
            {
                HeritageComponent->SetMaterial(
                    Binding.MaterialSlotIndex, Binding.BaseMaterial);
            }
        }
    }
}

bool ATRIADIstanaExploreV4LandscapeActor::CreateRuntimeWindMaterialInstances(
    FString& OutError)
{
    WindMaterialInstances.Reset();
    for (const FTRIADIstanaExploreV4WindMaterialBinding& Binding :
         SavedWindBindings)
    {
        UStaticMeshComponent* MainComponent = ComponentForWindRole(Binding.Role);
        UMaterialInstanceDynamic* MainMid = MainComponent
            ? UMaterialInstanceDynamic::Create(Binding.BaseMaterial, this)
            : nullptr;
        if (!MainMid)
        {
            WindMaterialInstances.Reset();
            OutError = TEXT("A unique runtime MID could not be created for a V4 wind role.");
            return false;
        }
        MainMid->SetScalarParameterValue(
            WindResponseParameter, WindResponseForRole(Binding.Role));
        float MainResponse = 0.0f;
        if (!MainMid->GetScalarParameterValue(
                FMaterialParameterInfo(WindResponseParameter), MainResponse) ||
            !FMath::IsNearlyEqual(
                MainResponse, WindResponseForRole(Binding.Role), 0.0001f))
        {
            WindMaterialInstances.Reset();
            OutError = TEXT("A V4 runtime MID did not retain its exact TRIAD_WindResponseScale value.");
            return false;
        }
        MainComponent->SetMaterial(Binding.MaterialSlotIndex, MainMid);
        WindMaterialInstances.Add(MainMid);

        const ETRIADIstanaExploreV4TreeForm TreeForm =
            TreeFormForWindRole(Binding.Role);
        if (TreeForm != ETRIADIstanaExploreV4TreeForm::Count)
        {
            UHierarchicalInstancedStaticMeshComponent* HeritageComponent =
                HeritageComponentForTreeForm(TreeForm);
            UMaterialInstanceDynamic* HeritageMid = HeritageComponent
                ? UMaterialInstanceDynamic::Create(Binding.BaseMaterial, this)
                : nullptr;
            if (!HeritageMid)
            {
                WindMaterialInstances.Reset();
                OutError = TEXT("A unique form-specific Heritage runtime MID could not be created.");
                return false;
            }
            HeritageMid->SetScalarParameterValue(
                WindResponseParameter, WindResponseForRole(Binding.Role));
            float HeritageResponse = 0.0f;
            if (!HeritageMid->GetScalarParameterValue(
                    FMaterialParameterInfo(WindResponseParameter),
                    HeritageResponse) ||
                !FMath::IsNearlyEqual(
                    HeritageResponse,
                    WindResponseForRole(Binding.Role),
                    0.0001f))
            {
                WindMaterialInstances.Reset();
                OutError = TEXT("A form-specific Heritage runtime MID did not retain its exact TRIAD_WindResponseScale value.");
                return false;
            }
            HeritageComponent->SetMaterial(
                Binding.MaterialSlotIndex, HeritageMid);
            WindMaterialInstances.Add(HeritageMid);
        }
    }
    if (WindMaterialInstances.Num() != ExpectedRuntimeWindMidCount)
    {
        WindMaterialInstances.Reset();
        OutError = TEXT("The exact 31-MID V4 runtime wind roster changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV4LandscapeActor::ApplyWindParameters()
{
    const FVector2D Direction = CurrentWindDirection.GetSafeNormal();
    for (UMaterialInstanceDynamic* Mid : WindMaterialInstances)
    {
        if (!Mid)
        {
            continue;
        }
        Mid->SetScalarParameterValue(WindStrengthParameter, CurrentWindStrengthCm);
        Mid->SetScalarParameterValue(WindSpeedParameter, WindSpeed);
        Mid->SetVectorParameterValue(
            WindDirectionParameter,
            FLinearColor(Direction.X, Direction.Y, 0.0f, 1.0f));
    }
}

void ATRIADIstanaExploreV4LandscapeActor::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    if (!ValidateAssetAndWindConfiguration(Error) ||
        !CreateRuntimeWindMaterialInstances(Error))
    {
        SetActorTickEnabled(false);
        return;
    }

    GustRandom.Initialize(DeterministicPlacementSeed ^ 0x57494E44);
    CurrentWindStrengthCm = BaseWindStrengthCm;
    CurrentWindDirection = PrevailingWindDirection.GetSafeNormal();
    WindStrengthVelocityCmPerSecond = 0.0f;
    WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    ActiveGustDirection = CurrentWindDirection;
    TimeUntilNextGustSeconds = GustRandom.FRandRange(7.0f, 14.0f);
    ActiveGustElapsedSeconds = -1.0f;
    WindStepAccumulatorSeconds = 0.0f;
    ApplyWindParameters();
    WindRuntimeReadyEvent.Broadcast();
    WindRuntimeReadyEvent.Clear();
}

void ATRIADIstanaExploreV4LandscapeActor::TriggerWindGust(float PeakStrengthCm)
{
    const float RequestedPeak = PeakStrengthCm < 0.0f
        ? GustPeakStrengthCm
        : PeakStrengthCm;
    ActiveGustPeakStrengthCm = FMath::Clamp(
        RequestedPeak,
        BaseWindStrengthCm,
        GustPeakStrengthCm);
    ActiveGustDurationSeconds = GustRandom.FRandRange(1.6f, 3.2f);
    ActiveGustElapsedSeconds = 0.0f;

    const FVector2D Prevailing = PrevailingWindDirection.GetSafeNormal();
    const float AngleRadians = FMath::DegreesToRadians(
        GustRandom.FRandRange(-32.0f, 32.0f));
    ActiveGustDirection = FVector2D(
        Prevailing.X * FMath::Cos(AngleRadians) -
            Prevailing.Y * FMath::Sin(AngleRadians),
        Prevailing.X * FMath::Sin(AngleRadians) +
            Prevailing.Y * FMath::Cos(AngleRadians)).GetSafeNormal();
}

void ATRIADIstanaExploreV4LandscapeActor::AdvanceWindFixedStep(
    float StepSeconds)
{
    float TargetStrength = BaseWindStrengthCm;
    FVector2D TargetDirection = PrevailingWindDirection.GetSafeNormal();

    if (ActiveGustElapsedSeconds >= 0.0f)
    {
        ActiveGustElapsedSeconds += StepSeconds;
        if (ActiveGustElapsedSeconds <= ActiveGustDurationSeconds)
        {
            const float Phase = ActiveGustElapsedSeconds /
                ActiveGustDurationSeconds;
            const float Pulse = FMath::Sin(PI * Phase);
            TargetStrength = FMath::Lerp(
                BaseWindStrengthCm, ActiveGustPeakStrengthCm, Pulse);
            TargetDirection = FMath::Lerp(
                PrevailingWindDirection.GetSafeNormal(),
                ActiveGustDirection,
                Pulse).GetSafeNormal();
        }
        else
        {
            ActiveGustElapsedSeconds = -1.0f;
            TimeUntilNextGustSeconds = GustRandom.FRandRange(7.0f, 14.0f);
        }
    }
    else
    {
        TimeUntilNextGustSeconds -= StepSeconds;
        if (TimeUntilNextGustSeconds <= 0.0f)
        {
            TriggerWindGust();
        }
    }

    const float AngularFrequency = 2.0f * PI * RecoveryFrequencyHz;
    const float StrengthAcceleration =
        AngularFrequency * AngularFrequency *
            (TargetStrength - CurrentWindStrengthCm) -
        2.0f * RecoveryDampingRatio * AngularFrequency *
            WindStrengthVelocityCmPerSecond;
    WindStrengthVelocityCmPerSecond += StrengthAcceleration * StepSeconds;
    CurrentWindStrengthCm +=
        WindStrengthVelocityCmPerSecond * StepSeconds;

    const FVector2D DirectionAcceleration =
        AngularFrequency * AngularFrequency *
            (TargetDirection - CurrentWindDirection) -
        2.0f * RecoveryDampingRatio * AngularFrequency *
            WindDirectionVelocityPerSecond;
    WindDirectionVelocityPerSecond +=
        DirectionAcceleration * StepSeconds;
    CurrentWindDirection +=
        WindDirectionVelocityPerSecond * StepSeconds;

    CurrentWindStrengthCm = FMath::Clamp(
        CurrentWindStrengthCm, 0.0f, 160.0f);
    WindStrengthVelocityCmPerSecond = FMath::Clamp(
        WindStrengthVelocityCmPerSecond, -1000.0f, 1000.0f);
    if (!FMath::IsFinite(CurrentWindDirection.X) ||
        !FMath::IsFinite(CurrentWindDirection.Y) ||
        CurrentWindDirection.SizeSquared() < 0.16f)
    {
        CurrentWindDirection = PrevailingWindDirection.GetSafeNormal();
        WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    }
    else
    {
        CurrentWindDirection.Normalize();
    }
}

void ATRIADIstanaExploreV4LandscapeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsWindRuntimeActive() || !FMath::IsFinite(DeltaSeconds) ||
        DeltaSeconds <= 0.0f || !FMath::IsFinite(FixedWindStepSeconds) ||
        FixedWindStepSeconds <= 0.0f)
    {
        return;
    }

    WindStepAccumulatorSeconds += FMath::Min(DeltaSeconds, 0.25f);
    int32 Steps = 0;
    while (WindStepAccumulatorSeconds + UE_SMALL_NUMBER >=
               FixedWindStepSeconds &&
           Steps < 32)
    {
        AdvanceWindFixedStep(FixedWindStepSeconds);
        WindStepAccumulatorSeconds -= FixedWindStepSeconds;
        ++Steps;
    }
    if (Steps == 32 && WindStepAccumulatorSeconds >= FixedWindStepSeconds)
    {
        WindStepAccumulatorSeconds = FMath::Fmod(
            WindStepAccumulatorSeconds, FixedWindStepSeconds);
    }
    ApplyWindParameters();
}

bool ATRIADIstanaExploreV4LandscapeActor::IsWindRuntimeActive() const
{
    if (!HasActorBegunPlay() ||
        WindMaterialInstances.Num() != ExpectedRuntimeWindMidCount)
    {
        return false;
    }
    TSet<const UMaterialInstanceDynamic*> UniqueMids;
    for (const UMaterialInstanceDynamic* Mid : WindMaterialInstances)
    {
        if (!Mid)
        {
            return false;
        }
        UniqueMids.Add(Mid);
    }
    return UniqueMids.Num() == ExpectedRuntimeWindMidCount &&
        FMath::IsFinite(CurrentWindStrengthCm) &&
        FMath::IsFinite(CurrentWindDirection.X) &&
        FMath::IsFinite(CurrentWindDirection.Y);
}

FString ATRIADIstanaExploreV4LandscapeActor::BuildWindRuntimeStateReport() const
{
    return FString::Printf(
        TEXT("ISTANA_EXPLORE_V4_WIND active=%s strength_cm=%.3f velocity_cm_s=%.3f direction=(%.4f,%.4f) direction_velocity=(%.4f,%.4f) gust_active=%s fixed_step_s=%.7f mids=%d/%d animated_close_turf_geometry_cards=true external_static_lawn_surface=MI_IPV4_AmbientCG_Grass001_Lawn external_static_lawn_surface_wpo=false. No positions, source URLs, geographic coordinates, botanical, survey, collision or sensor claims are reported."),
        IsWindRuntimeActive() ? TEXT("true") : TEXT("false"),
        CurrentWindStrengthCm,
        WindStrengthVelocityCmPerSecond,
        CurrentWindDirection.X,
        CurrentWindDirection.Y,
        WindDirectionVelocityPerSecond.X,
        WindDirectionVelocityPerSecond.Y,
        ActiveGustElapsedSeconds >= 0.0f ? TEXT("true") : TEXT("false"),
        FixedWindStepSeconds,
        WindMaterialInstances.Num(),
        ExpectedRuntimeWindMidCount);
}

bool ATRIADIstanaExploreV4LandscapeActor::ValidateExploreV4Landscape(
    FString& OutReport) const
{
    if (!GetActorTransform().Equals(FTransform::Identity, 0.001f) ||
        !SceneRoot ||
        !SceneRoot->GetRelativeTransform().Equals(FTransform::Identity, 0.001f) ||
        ClaimLabel != TEXT("ISTANA_EXPLORE_V4_PUBLIC_OPEN_DATA_VISUAL_APPROXIMATION_NOT_ONE_TO_ONE_NOT_SURVEY_NOT_BOTANICAL_INVENTORY_NOT_SENSOR_TRUTH") ||
        bOneToOneOneKilometerClaimed || bSurveyAccuracyClaimed ||
        bExactBotanicalInventoryClaimed || bIndividualRasterTreeTruthClaimed ||
        bCollisionOrSensorTruthAuthority || bGoogleOrOneMapContentUsed ||
        bPorticoCollisionAuthority || !bHeritageVisualsAreSilhouetteProxies ||
        bLowPolyPalmRuntimePlacementAllowed ||
        ExternalStaticLawnSurfaceMaterialName !=
            TEXT("MI_IPV4_AmbientCG_Grass001_Lawn") ||
        bExternalStaticLawnSurfaceUsesWindWpo ||
        DeterministicPlacementSeed != 0x4757A6A5 ||
        !FMath::IsNearlyEqual(BaseWindStrengthCm, 5.5f, 0.001f) ||
        !FMath::IsNearlyEqual(GustPeakStrengthCm, 48.0f, 0.001f) ||
        !FMath::IsNearlyEqual(WindSpeed, 1.45f, 0.001f) ||
        !PrevailingWindDirection.Equals(FVector2D(0.93f, 0.37f), 0.001f) ||
        !FMath::IsNearlyEqual(RecoveryFrequencyHz, 0.62f, 0.001f) ||
        !FMath::IsNearlyEqual(RecoveryDampingRatio, 0.55f, 0.001f) ||
        !FMath::IsNearlyEqual(FixedWindStepSeconds, 1.0f / 120.0f, 0.0000001f) ||
        FrozenGeospatialContractSha256 != TEXT("CCD9B5200E03602EC6FA6986F4D3AB70920806C63D718B5B1A943CC29F30D8E7") ||
        FrozenPreparedRasterGridSha256 != TEXT("262C88795C3C5FB1BB92B2D1CD2D1886B25D5E3885E60F34E407082E4C81C447") ||
        FrozenVegetationContractSchema !=
            TEXT("triad.istana_explore_v4_vegetation_contract.v2") ||
        FrozenVegetationContractSha256 != TEXT("ABDC65AA14DA9AE89736BBE2D75E6212FA38B76B44DBA491C9266DEBC247E104") ||
        FrozenPorticoV8ObjSha256 != TEXT("330B20E8F58289C84EF96CB031D374ADB3AAF52711E5247D1F167C7E94DC4526") ||
        FrozenPorticoV8ManifestSha256 != TEXT("3E88B8C4AEF5A499696436920972301EFBDA242BAAA3F64928D1A8B072557943") ||
        FrozenPorticoV8SemanticSha256 != TEXT("3F0F48F1B8C07415D5539E91D1F006D3FD9EC3BD7E55A85B16F69883728465D8") ||
        FrozenPorticoV5CObjSha256 != TEXT("883EAC65449A54D284D1E0E15B42F134F73C509A5ACE95BFD6DDD1DD326A3EC5") ||
        FrozenPorticoV5CMtlSha256 != TEXT("DC38D80587AEDFD31143A755388D4248A7C6E2029C4C9FD92794DDE06F2E339C") ||
        FrozenPorticoV5CManifestSha256 != TEXT("380A2DE1204029C26FA06A7BBFD99105AA9D7979625575B28FF3E45A4B0337A1") ||
        FrozenPorticoV5CSemanticSha256 != TEXT("84E99970FEAF2F9ACCE9D7BE07C320C4A7E8AAA342F5C30463F0ADE311958567") ||
        !bPorticoV5CRenderOnly ||
        bPorticoV5CCollisionNavigationOrRfAuthority ||
        bPorticoV5CUsesTextureMaps ||
        bRequiredAttributionPresentedInPublicSurface || bPublicDistributionReady)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: identity, truthful claim boundary, frozen source binding, or development attribution flags changed.");
        return false;
    }

    const TArray<FString> ExpectedAttribution = {
        TEXT("Contains information from Heritage Trees and Tree Conservation Area, accessed on 25 August 2026 from data.gov.sg, made available under the Singapore Open Data Licence version 1.0 https://www.sla.gov.sg/singapore-open-data-licence/"),
        TEXT("© ESA WorldCover project 2021 / Contains modified Copernicus Sentinel data (2021) processed by ESA WorldCover consortium."),
        TEXT("ETH Global Canopy Height 2020, Lang et al. (2023), https://doi.org/10.3929/ethz-b-000609802, licensed CC BY 4.0.")};
    if (RequiredDistributionAttribution != ExpectedAttribution)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: the exact retained attribution roster changed.");
        return false;
    }

    FString ConfigurationError;
    if (!ValidateAssetAndWindConfiguration(ConfigurationError))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: ") + ConfigurationError;
        return false;
    }
    if (!bPopulationConfigured ||
        PreservedInheritedV2TreeWorldTransforms.Num() != ExpectedInheritedTreeCount ||
        PreservedRasterCues.Num() != ExpectedInheritedTreeCount ||
        PreservedTreeFormBySourceIndex.Num() != ExpectedInheritedTreeCount ||
        RecordedMainTreeFormCounts.Num() != 5 ||
        PreservedHeritageAnchors.Num() != ExpectedHeritageAnchorCount ||
        PreservedCloseTurfWorldTransforms.Num() != ExpectedCloseTurfCount ||
        RecordedShrubInstanceCount != ExpectedShrubCount ||
        RecordedFlowerInstanceCount != ExpectedFlowerCount ||
        RecordedUnderstoreyInstanceCount != ExpectedUnderstoreyCount ||
        RecordedGeometryGrassInstanceCount != ExpectedGeometryGrassCount ||
        RecordedCloseTurfInstanceCount != ExpectedCloseTurfCount)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: the exact persisted population census changed.");
        return false;
    }

    FString PorticoPresentationReport;
    if (!ValidatePorticoV5CPresentation(PorticoPresentationReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: ") +
            PorticoPresentationReport;
        return false;
    }

    const TArray<float> NativeTreeHeightsCm = {
        static_cast<float>(MeshHeightCm(SavedAssetRoster.UmbrellaTree)),
        static_cast<float>(MeshHeightCm(SavedAssetRoster.DomeTree)),
        static_cast<float>(MeshHeightCm(SavedAssetRoster.HighForkRoundedTree)),
        static_cast<float>(MeshHeightCm(SavedAssetRoster.ColumnarNarrowTree)),
        static_cast<float>(MeshHeightCm(SavedAssetRoster.PalmTree))};
    TArray<FTRIADIstanaExploreV4ClassifiedTreeRow> ExpectedRows;
    FString RowError;
    if (!BuildDeterministicReclassifiedRows(
            PreservedInheritedV2TreeWorldTransforms,
            PreservedRasterCues,
            NativeTreeHeightsCm,
            DeterministicPlacementSeed,
            ExpectedRows,
            RowError))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: ") + RowError;
        return false;
    }

    int32 ExpectedFormCounts[5] = {0, 0, 0, 0, 0};
    for (const FTRIADIstanaExploreV4ClassifiedTreeRow& Row : ExpectedRows)
    {
        ++ExpectedFormCounts[static_cast<int32>(Row.Form)];
    }
    const int32 ExpectedHeritageFormCounts[5] = {4, 4, 1, 0, 0};
    // V5B is an additive render successor: after snapshotting all exact source
    // transforms it may hide the inherited close turf, coarse edge grass and
    // three planting renderers while owning full render-only replacements.
    // Every V4 mesh, count, transform, collision, navigation, and wind
    // contract remains validated here.
    const bool bRenderVisualsHiddenByV5BSuccessor =
        Tags.Contains(V5BRenderSuccessorTag);
    const bool bV5DTreeRuntimeOverride =
        Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::RuntimeOverrideTag());
    const TArray<FV4HismColdGateRow> HismRows = {
        {TEXT("main.umbrella"), UmbrellaTreeInstances,
         bV5DTreeRuntimeOverride ? UmbrellaTreeInstances->GetStaticMesh() :
             SavedAssetRoster.UmbrellaTree, ExpectedFormCounts[0],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("main.dome"), DomeTreeInstances,
         bV5DTreeRuntimeOverride ? DomeTreeInstances->GetStaticMesh() :
             SavedAssetRoster.DomeTree, ExpectedFormCounts[1],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("main.highForkRounded"), HighForkRoundedTreeInstances,
         bV5DTreeRuntimeOverride ?
             HighForkRoundedTreeInstances->GetStaticMesh() :
             SavedAssetRoster.HighForkRoundedTree, ExpectedFormCounts[2],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("main.columnarNarrow"), ColumnarNarrowTreeInstances,
         bV5DTreeRuntimeOverride ?
             ColumnarNarrowTreeInstances->GetStaticMesh() :
             SavedAssetRoster.ColumnarNarrowTree, ExpectedFormCounts[3],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("main.palm"), PalmTreeInstances,
         bV5DTreeRuntimeOverride ? PalmTreeInstances->GetStaticMesh() :
             SavedAssetRoster.PalmTree, ExpectedFormCounts[4],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("heritage.umbrella"), HeritageUmbrellaInstances,
         bV5DTreeRuntimeOverride ?
             HeritageUmbrellaInstances->GetStaticMesh() :
             SavedAssetRoster.UmbrellaTree, ExpectedHeritageFormCounts[0],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("heritage.dome"), HeritageDomeInstances,
         bV5DTreeRuntimeOverride ? HeritageDomeInstances->GetStaticMesh() :
             SavedAssetRoster.DomeTree, ExpectedHeritageFormCounts[1],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("heritage.highForkRounded"), HeritageHighForkRoundedInstances,
         bV5DTreeRuntimeOverride ?
             HeritageHighForkRoundedInstances->GetStaticMesh() :
             SavedAssetRoster.HighForkRoundedTree, ExpectedHeritageFormCounts[2],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("heritage.columnarNarrow"), HeritageColumnarNarrowInstances,
         bV5DTreeRuntimeOverride ?
             HeritageColumnarNarrowInstances->GetStaticMesh() :
             SavedAssetRoster.ColumnarNarrowTree, ExpectedHeritageFormCounts[3],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("heritage.palm"), HeritagePalmInstances,
         bV5DTreeRuntimeOverride ? HeritagePalmInstances->GetStaticMesh() :
             SavedAssetRoster.PalmTree, ExpectedHeritageFormCounts[4],
         ECollisionEnabled::NoCollision,
         true, false, false},
        {TEXT("heritage.pawnBlockers"), HeritageAnchorPawnBlockers,
         SavedAssetRoster.HeritagePawnBlockerCylinder,
         ExpectedHeritageAnchorCount, ECollisionEnabled::QueryOnly,
         false, true, true},
        {TEXT("planting.shrub"), ShrubInstances,
         SavedAssetRoster.Shrub, ExpectedShrubCount,
         ECollisionEnabled::NoCollision,
         !bRenderVisualsHiddenByV5BSuccessor,
         bRenderVisualsHiddenByV5BSuccessor, false},
        {TEXT("planting.flower"), FlowerInstances,
         SavedAssetRoster.Flower, ExpectedFlowerCount,
         ECollisionEnabled::NoCollision,
         !bRenderVisualsHiddenByV5BSuccessor,
         bRenderVisualsHiddenByV5BSuccessor, false},
        {TEXT("planting.understorey"), UnderstoreyInstances,
         SavedAssetRoster.Understorey, ExpectedUnderstoreyCount,
         ECollisionEnabled::NoCollision,
         !bRenderVisualsHiddenByV5BSuccessor,
         bRenderVisualsHiddenByV5BSuccessor, false},
        {TEXT("grass.edgeGeometry"), GeometryGrassInstances,
         SavedAssetRoster.GeometryGrass, ExpectedGeometryGrassCount,
         ECollisionEnabled::NoCollision,
         !bRenderVisualsHiddenByV5BSuccessor,
         bRenderVisualsHiddenByV5BSuccessor, false},
        {TEXT("grass.closeTurf"), CloseTurfInstances,
         SavedAssetRoster.CloseTurf, ExpectedCloseTurfCount,
         ECollisionEnabled::NoCollision,
         !bRenderVisualsHiddenByV5BSuccessor,
         bRenderVisualsHiddenByV5BSuccessor,
         false}};
    FString HismGateError;
    if (!ValidateExactV4HismColdGate(
            HismRows,
            bRenderVisualsHiddenByV5BSuccessor,
            HismGateError))
    {
        OutReport = HismGateError;
        return false;
    }

    int32 ReadIndices[5] = {0, 0, 0, 0, 0};
    for (const FTRIADIstanaExploreV4ClassifiedTreeRow& Row : ExpectedRows)
    {
        const int32 FormIndex = static_cast<int32>(Row.Form);
        if (PreservedTreeFormBySourceIndex[Row.SourceIndex] !=
            static_cast<uint8>(Row.Form))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: persisted deterministic form assignment changed.");
            return false;
        }
        const UHierarchicalInstancedStaticMeshComponent* Component =
            ComponentForTreeForm(Row.Form);
        FTransform ActualWorldTransform;
        const FTransform& SourceTransform =
            PreservedInheritedV2TreeWorldTransforms[Row.SourceIndex];
        const bool bExpectedRowPreservesSource =
            Row.Form == ETRIADIstanaExploreV4TreeForm::ColumnarNarrow
            ? Row.WorldTransform.Equals(SourceTransform, 0.0f)
            : PreservesSourceTranslationAndRotation(
                  Row.WorldTransform, SourceTransform);
        if (!Component || !Component->GetInstanceTransform(
                ReadIndices[FormIndex]++, ActualWorldTransform, true) ||
            !ActualWorldTransform.Equals(Row.WorldTransform, 0.001f) ||
            !bExpectedRowPreservesSource)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: the five-component tree union no longer reproduces all 720 inherited translations/full rotations and deterministic scales, including the exact full 232-transform columnar subset.");
            return false;
        }
    }
    int32 MainTreeTotal = 0;
    for (int32 FormIndex = 0; FormIndex < 5; ++FormIndex)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            ComponentForTreeForm(
                static_cast<ETRIADIstanaExploreV4TreeForm>(FormIndex));
        const bool bExpectedRuntimeCensus = FormIndex < 3
            ? ExpectedFormCounts[FormIndex] > 0
            : FormIndex == static_cast<int32>(
                  ETRIADIstanaExploreV4TreeForm::ColumnarNarrow)
                ? ExpectedFormCounts[FormIndex] == FrozenV2ColumnarCount
                : ExpectedFormCounts[FormIndex] == 0;
        if (!Component ||
            Component->GetInstanceCount() != ExpectedFormCounts[FormIndex] ||
            RecordedMainTreeFormCounts[FormIndex] !=
                ExpectedFormCounts[FormIndex] ||
            !bExpectedRuntimeCensus)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: an exact main-tree form census changed, a required runtime morphology disappeared, or the low-poly palm was placed.");
            return false;
        }
        MainTreeTotal += Component->GetInstanceCount();
    }
    if (MainTreeTotal != ExpectedInheritedTreeCount)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: main tree-form HISM union is not exactly 720.");
        return false;
    }

    int32 HeritageReadIndices[5] = {0, 0, 0, 0, 0};
    const UStaticMesh* BlockerMesh = HeritageAnchorPawnBlockers->GetStaticMesh();
    const FVector BlockerSizeCm = BlockerMesh
        ? BlockerMesh->GetBounds().BoxExtent * 2.0
        : FVector::ZeroVector;
    const double NativeBlockerDiameterCm =
        FMath::Max(BlockerSizeCm.X, BlockerSizeCm.Y);
    for (int32 Index = 0; Index < ExpectedHeritageAnchorCount; ++Index)
    {
        const FTRIADIstanaExploreV4HeritageAnchor& Anchor =
            PreservedHeritageAnchors[Index];
        const FExpectedHeritageAnchor& Expected = ExpectedHeritageAnchors[Index];
        if (Anchor.PublicRecordId != Expected.PublicRecordId ||
            Anchor.FormHint != Expected.Form ||
            Anchor.bFormComesFromPublishedQualitativeHint !=
                Expected.bFormComesFromPublishedQualitativeHint ||
            !FMath::IsNearlyEqual(Anchor.PublishedHeightMeters, Expected.HeightMeters, 0.001f) ||
            !FMath::IsNearlyEqual(Anchor.PublishedGirthMeters, Expected.GirthMeters, 0.001f) ||
            !Anchor.bPositionComesFromPublishedRecord ||
            !Anchor.bVisualIsSilhouetteProxy)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: ordered Heritage public metadata or chosen proxy-form provenance changed.");
            return false;
        }
        const int32 FormIndex = static_cast<int32>(Anchor.FormHint);
        const UHierarchicalInstancedStaticMeshComponent* VisualComponent =
            HeritageComponentForTreeForm(Anchor.FormHint);
        const double NativeHeightCm = MeshHeightCm(
            VisualComponent ? VisualComponent->GetStaticMesh() : nullptr);
        const uint32 AnchorHash = Mix32(
            static_cast<uint32>(DeterministicPlacementSeed) ^
            (static_cast<uint32>(Index) * 0x6D2B79F5u));
        const double YawDegrees =
            static_cast<double>(AnchorHash % 36000u) / 100.0;
        const double UniformScale =
            Anchor.PublishedHeightMeters * CentimetersPerMeter / NativeHeightCm;
        const FTransform ExpectedVisualTransform(
            FRotator(0.0, YawDegrees, 0.0),
            Anchor.LocalGroundLocationCm,
            FVector(UniformScale));
        FTransform ActualVisualTransform;
        if (!VisualComponent ||
            !VisualComponent->GetInstanceTransform(
                HeritageReadIndices[FormIndex]++,
                ActualVisualTransform,
                true) ||
            !ActualVisualTransform.Equals(ExpectedVisualTransform, 0.001f) ||
            !ActualVisualTransform.GetScale3D().Equals(
                FVector(UniformScale), 0.001f))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: a Heritage silhouette no longer uses uniform published-height scaling on its matching broad form.");
            return false;
        }

        const double PublishedTrunkDiameterCm =
            Anchor.PublishedGirthMeters * CentimetersPerMeter / PI;
        FVector BlockerLocation = Anchor.LocalGroundLocationCm;
        BlockerLocation.Z += HeritageBlockerHeightCm * 0.5f;
        const FTransform ExpectedBlockerTransform(
            FRotator(0.0, YawDegrees, 0.0),
            BlockerLocation,
            FVector(
                PublishedTrunkDiameterCm / NativeBlockerDiameterCm,
                PublishedTrunkDiameterCm / NativeBlockerDiameterCm,
                HeritageBlockerHeightCm / BlockerSizeCm.Z));
        FTransform ActualBlockerTransform;
        if (!HeritageAnchorPawnBlockers->GetInstanceTransform(
                Index, ActualBlockerTransform, true) ||
            !ActualBlockerTransform.Equals(ExpectedBlockerTransform, 0.001f))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: a separate Heritage Pawn blocker no longer matches published girth and ground position.");
            return false;
        }
    }
    int32 HeritageTotal = 0;
    for (int32 FormIndex = 0; FormIndex < 5; ++FormIndex)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            HeritageComponentForTreeForm(
                static_cast<ETRIADIstanaExploreV4TreeForm>(FormIndex));
        if (!Component ||
            Component->GetInstanceCount() !=
                ExpectedHeritageFormCounts[FormIndex])
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: exact per-form Heritage silhouette census changed.");
            return false;
        }
        HeritageTotal += Component->GetInstanceCount();
    }
    if (HeritageTotal != ExpectedHeritageAnchorCount ||
        !HeritageAnchorPawnBlockers ||
        HeritageAnchorPawnBlockers->GetInstanceCount() !=
            ExpectedHeritageAnchorCount ||
        !HeritageAnchorPawnBlockers->bAutoRebuildTreeOnInstanceChanges ||
        HeritageAnchorPawnBlockers->IsAsyncBuilding() ||
        !HeritageAnchorPawnBlockers->IsTreeFullyBuilt() ||
        HeritageAnchorPawnBlockers->IsVisible() ||
        !HeritageAnchorPawnBlockers->bHiddenInGame ||
        !IsPawnOnlyQueryBlocker(HeritageAnchorPawnBlockers))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: exact nine Heritage silhouettes or separate Pawn-only blockers changed.");
        return false;
    }

    if (ShrubInstances->GetInstanceCount() != ExpectedShrubCount ||
        FlowerInstances->GetInstanceCount() != ExpectedFlowerCount ||
        UnderstoreyInstances->GetInstanceCount() != ExpectedUnderstoreyCount ||
        GeometryGrassInstances->GetInstanceCount() != ExpectedGeometryGrassCount ||
        CloseTurfInstances->GetInstanceCount() != ExpectedCloseTurfCount)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: exact shrub/flower/understorey/geometry-grass/close-turf census changed.");
        return false;
    }

    const TArray<const UHierarchicalInstancedStaticMeshComponent*> AdditionComponents = {
        HeritageUmbrellaInstances,
        HeritageDomeInstances,
        HeritageHighForkRoundedInstances,
        HeritageColumnarNarrowInstances,
        HeritagePalmInstances,
        ShrubInstances,
        FlowerInstances,
        UnderstoreyInstances,
        GeometryGrassInstances,
        CloseTurfInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         AdditionComponents)
    {
        const bool bCloseTurf = Component == CloseTurfInstances;
        for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
        {
            FTransform Transform;
            if (!Component->GetInstanceTransform(Index, Transform, true) ||
                !IsFinitePositiveTransform(Transform) ||
                !IsInsideOneKilometre(Transform) ||
                IsInsideFountainClearance(
                    Transform, FountainCenterMeters,
                    FountainClearanceRadiusMeters) ||
                (!bCloseTurf && IsInsideWoodyCeremonialClearance(
                    Transform, ClearCeremonialAxisHalfWidthMeters)))
            {
                OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: an addition is non-finite, out of scope, or entered protected ceremonial/fountain clearance.");
                return false;
            }
        }
    }
    for (int32 Index = 0; Index < ExpectedCloseTurfCount; ++Index)
    {
        FTransform ActualWorldTransform;
        if (!CloseTurfInstances->GetInstanceTransform(
                Index, ActualWorldTransform, true) ||
            !ActualWorldTransform.Equals(
                PreservedCloseTurfWorldTransforms[Index], 0.001f))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: V4 close turf no longer exactly replaces all 18,432 full V3 transforms.");
            return false;
        }
    }

    if (RecordedExternalV2PawnBlockerCount != ExpectedInheritedTreeCount ||
        !bRecordedV2TreeVisualsHidden ||
        !bRecordedV3ReplacementTreeVisualsHidden ||
        !bRecordedV3CloseTurfVisualsHidden ||
        !bRecordedV7PorticoHidden ||
        !bRecordedInheritedTerrainAndCollisionPreserved ||
        !bRecordedDistantBuildingsPreserved)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_INVALID: required external target-map preservation/hiding record is incomplete.");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("Validated additive Explore V4: exactly %d inherited V2 tree translations/full rotations reclassified as umbrella=%d, dome=%d, high-fork=%d and the exact unchanged 232-transform inherited columnar subset=%d; palm=%d because the imported low-poly mid/far palm proxy is intentionally unused in the free-roam map; all five distinct broad-form HISMs remain explicit. Raster bands route only broad qualitative forms/heights and do not claim a measured individual-tree fit. %d broad-form-routed Heritage silhouette anchors (seven published qualitative hints, two explicitly generic dome fallbacks) uniformly scaled from published height with %d separate Pawn-only blockers sized from published girth; %d shrubs, %d flowers, %d understorey, %d taller edge-grass and an exact %d full-transform animated geometry-card replacement of V3 close turf; the preserved MI_IPV4_AmbientCG_Grass001_Lawn surface has no WPO; the identity render-only portico presentation is in an exact V8-visible fail-safe or validated V5C-visible/V8-hidden state; V2 terrain/collision, %d external V2 Pawn blockers and distant buildings preserved. One-to-one, survey, exact botanical inventory, individual-raster-tree, collision and sensor claims remain false; public distribution remains disabled until attribution is presented."),
        ExpectedInheritedTreeCount,
        RecordedMainTreeFormCounts[0],
        RecordedMainTreeFormCounts[1],
        RecordedMainTreeFormCounts[2],
        RecordedMainTreeFormCounts[3],
        RecordedMainTreeFormCounts[4],
        ExpectedHeritageAnchorCount,
        ExpectedHeritageAnchorCount,
        ExpectedShrubCount,
        ExpectedFlowerCount,
        ExpectedUnderstoreyCount,
        ExpectedGeometryGrassCount,
        ExpectedCloseTurfCount,
        ExpectedInheritedTreeCount);
    return true;
}
