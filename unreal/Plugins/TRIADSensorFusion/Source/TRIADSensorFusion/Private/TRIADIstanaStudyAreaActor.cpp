#include "TRIADIstanaStudyAreaActor.h"

#include "CesiumGeoreference.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FName HumanOnlyOverlayComponentTag(TEXT("TRIADHumanOnlyOverlay"));
const FName ApproximateTerrainComponentTag(TEXT("TRIADApproximateTerrainFallback"));
constexpr double CentimetersPerMeter = 100.0;
}

ATRIADIstanaStudyAreaActor::ATRIADIstanaStudyAreaActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.AddUnique(TEXT("TRIADIstanaStudyArea"));
    Tags.AddUnique(TEXT("TRIADNonLegalSimulationBoundary"));

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("CesiumGlobeAnchor"));

    BoundarySegmentsMesh = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("VisibleBoundarySegments"));
    BoundarySegmentsMesh->SetupAttachment(SceneRoot);
    BoundarySegmentsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoundarySegmentsMesh->SetCastShadow(false);
    BoundarySegmentsMesh->SetCanEverAffectNavigation(false);
    BoundarySegmentsMesh->ComponentTags.AddUnique(HumanOnlyOverlayComponentTag);

    BoundaryPosts = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("VisibleBoundaryPosts"));
    BoundaryPosts->SetupAttachment(SceneRoot);
    BoundaryPosts->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoundaryPosts->SetCastShadow(false);
    BoundaryPosts->SetCanEverAffectNavigation(false);
    BoundaryPosts->ComponentTags.AddUnique(HumanOnlyOverlayComponentTag);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CubeFinder.Succeeded())
    {
        BoundarySegmentsMesh->SetStaticMesh(CubeFinder.Object);
    }
    if (CylinderFinder.Succeeded())
    {
        BoundaryPosts->SetStaticMesh(CylinderFinder.Object);
    }

    ApproximateTerrainFallback = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("ApproximateTerrainFallbackCollision"));
    ApproximateTerrainFallback->SetupAttachment(SceneRoot);
    ApproximateTerrainFallback->SetStaticMesh(CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr);
    ApproximateTerrainFallback->SetMobility(EComponentMobility::Movable);
    ApproximateTerrainFallback->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ApproximateTerrainFallback->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    ApproximateTerrainFallback->SetGenerateOverlapEvents(false);
    ApproximateTerrainFallback->SetCanEverAffectNavigation(false);
    ApproximateTerrainFallback->SetCastShadow(false);
    ApproximateTerrainFallback->SetVisibility(false, true);
    ApproximateTerrainFallback->SetHiddenInGame(true);
    ApproximateTerrainFallback->ComponentTags.AddUnique(ApproximateTerrainComponentTag);

    BoundaryLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BoundaryLabel"));
    BoundaryLabel->SetupAttachment(SceneRoot);
    BoundaryLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    BoundaryLabel->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    BoundaryLabel->SetTextRenderColor(FColor(255, 175, 30));
    BoundaryLabel->SetWorldSize(4000.0f);
    BoundaryLabel->SetCastShadow(false);
    BoundaryLabel->bAlwaysRenderAsText = true;
    BoundaryLabel->ComponentTags.AddUnique(HumanOnlyOverlayComponentTag);
}

void ATRIADIstanaStudyAreaActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildBoundary();
}

void ATRIADIstanaStudyAreaActor::ConfigureStudyArea(
    double InCenterLongitudeDegrees,
    double InCenterLatitudeDegrees,
    double InCenterHeightMeters,
    double InRadiusMeters,
    int32 InBoundarySegments,
    ACesiumGeoreference* InGeoreference)
{
    CenterLongitudeDegrees = InCenterLongitudeDegrees;
    CenterLatitudeDegrees = InCenterLatitudeDegrees;
    CenterHeightMeters = InCenterHeightMeters;
    RadiusMeters = FMath::Max(InRadiusMeters, 1.0);
    BoundarySegments = FMath::Clamp(InBoundarySegments, 16, 256);
    if (InGeoreference)
    {
        GlobeAnchor->SetGeoreference(TSoftObjectPtr<ACesiumGeoreference>(InGeoreference));
    }
    GlobeAnchor->MoveToLongitudeLatitudeHeight(FVector(
        CenterLongitudeDegrees,
        CenterLatitudeDegrees,
        CenterHeightMeters));
    RebuildBoundary();
}

void ATRIADIstanaStudyAreaActor::RebuildBoundary()
{
    if (!BoundarySegmentsMesh || !BoundaryPosts || !BoundaryLabel || !ApproximateTerrainFallback)
    {
        return;
    }
    BoundarySegmentsMesh->ClearInstances();
    BoundaryPosts->ClearInstances();

    const int32 SegmentCount = FMath::Clamp(BoundarySegments, 16, 256);
    const double SafeRadiusMeters = FMath::Max(RadiusMeters, 1.0);
    // The Engine cylinder is 1 m in diameter and 1 m tall. A 0.5 m-thick
    // collision disk puts its top exactly at the AOI anchor height.
    ApproximateTerrainFallback->SetRelativeLocation(FVector(0.0, 0.0, -25.0));
    ApproximateTerrainFallback->SetRelativeScale3D(FVector(
        SafeRadiusMeters * 2.0,
        SafeRadiusMeters * 2.0,
        0.5));
    const double ChordMeters = 2.0 * SafeRadiusMeters * FMath::Sin(PI / SegmentCount);
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        const double AngleRadians = (static_cast<double>(Index) + 0.5) * 2.0 * PI / SegmentCount;
        const FVector CenterMeters(
            SafeRadiusMeters * FMath::Cos(AngleRadians),
            SafeRadiusMeters * FMath::Sin(AngleRadians),
            1.0);
        const double TangentYawDegrees = FMath::RadiansToDegrees(AngleRadians) + 90.0;
        BoundarySegmentsMesh->AddInstance(FTransform(
            FRotator(0.0, TangentYawDegrees, 0.0),
            CenterMeters * CentimetersPerMeter,
            FVector(ChordMeters * 0.94, 0.65, 2.0)), false);

        if (Index % 4 == 0)
        {
            const double PointAngle = static_cast<double>(Index) * 2.0 * PI / SegmentCount;
            const FVector PostMeters(
                SafeRadiusMeters * FMath::Cos(PointAngle),
                SafeRadiusMeters * FMath::Sin(PointAngle),
                2.0);
            BoundaryPosts->AddInstance(FTransform(
                FRotator::ZeroRotator,
                PostMeters * CentimetersPerMeter,
                FVector(0.7, 0.7, 4.0)), false);
        }
    }

    BoundaryLabel->SetRelativeLocation(FVector(
        0.0,
        SafeRadiusMeters * CentimetersPerMeter,
        600.0));
    BoundaryLabel->SetText(FText::FromString(FString::Printf(
        TEXT("TRIAD ISTANA %.0f m SIMULATION AOI - NON-LEGAL BOUNDARY"),
        SafeRadiusMeters)));
}
