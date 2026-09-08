#include "TRIADIstanaBuildingActor.h"

#include "CesiumGeoreference.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr double CentimetersPerMeter = 100.0;

void ConfigureInstances(
    UHierarchicalInstancedStaticMeshComponent* Component,
    UStaticMesh* Mesh,
    bool bCollision)
{
    Component->SetStaticMesh(Mesh);
    // Globe-anchored actors must remain movable when the Cesium origin shifts.
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetCollisionEnabled(
        bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (bCollision)
    {
        Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    }
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
}

UMaterialInterface* LoadMaterialWithFallback(
    const TCHAR* ProjectMaterialPath,
    UMaterialInterface* Fallback)
{
    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, ProjectMaterialPath))
    {
        return Material;
    }
    return Fallback;
}
}

ATRIADIstanaBuildingActor::ATRIADIstanaBuildingActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.AddUnique(TEXT("TRIADIstanaMainBuilding"));
    Tags.AddUnique(TEXT("TRIADPublicExteriorApproximation"));

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("CesiumGlobeAnchor"));

    Masonry = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Masonry"));
    MasonryAccent = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MasonryAccent"));
    Columns = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Columns"));
    Roofs = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Roofs"));
    DarkDetails = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("DarkDetails"));
    Glass = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Glass"));
    Metal = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Metal"));
    Stone = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Stone"));
    Landscape = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Landscape"));
    Water = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Water"));
    Domes = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Domes"));
    Foliage = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Foliage"));
    RoofCones = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RoofCones"));

    const TArray<UHierarchicalInstancedStaticMeshComponent*> AllComponents = {
        Masonry, MasonryAccent, Columns, Roofs, DarkDetails, Glass,
        Metal, Stone, Landscape, Water, Domes, Foliage, RoofCones};
    for (UHierarchicalInstancedStaticMeshComponent* Component : AllComponents)
    {
        Component->SetupAttachment(SceneRoot);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
    UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
    UStaticMesh* Cylinder = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
    UStaticMesh* Sphere = SphereFinder.Succeeded() ? SphereFinder.Object : nullptr;
    UStaticMesh* Cone = ConeFinder.Succeeded() ? ConeFinder.Object : nullptr;

    ConfigureInstances(Masonry, Cube, true);
    ConfigureInstances(MasonryAccent, Cube, true);
    ConfigureInstances(Columns, Cylinder, true);
    ConfigureInstances(Roofs, Cube, true);
    ConfigureInstances(DarkDetails, Cube, false);
    ConfigureInstances(Glass, Cube, false);
    ConfigureInstances(Metal, Cylinder, false);
    ConfigureInstances(Stone, Cylinder, true);
    ConfigureInstances(Landscape, Cube, false);
    ConfigureInstances(Water, Cylinder, false);
    ConfigureInstances(Domes, Sphere, true);
    ConfigureInstances(Foliage, Sphere, false);
    ConfigureInstances(RoofCones, Cone, true);
}

void ATRIADIstanaBuildingActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildExterior();
}

void ATRIADIstanaBuildingActor::ConfigureGeodeticPlacement(
    double InLongitudeDegrees,
    double InLatitudeDegrees,
    double InHeightMeters,
    double InFootprintYawDegrees,
    ACesiumGeoreference* InGeoreference)
{
    LongitudeDegrees = InLongitudeDegrees;
    LatitudeDegrees = InLatitudeDegrees;
    HeightMeters = InHeightMeters;
    FootprintYawDegrees = InFootprintYawDegrees;
    if (InGeoreference)
    {
        GlobeAnchor->SetGeoreference(TSoftObjectPtr<ACesiumGeoreference>(InGeoreference));
    }
    GlobeAnchor->MoveToLongitudeLatitudeHeight(FVector(
        LongitudeDegrees,
        LatitudeDegrees,
        HeightMeters));
    SetActorRotation(FRotator(0.0, FootprintYawDegrees, 0.0));
}

void ATRIADIstanaBuildingActor::AddBox(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FVector& CenterMeters,
    const FVector& SizeMeters,
    const FRotator& Rotation)
{
    if (Component)
    {
        Component->AddInstance(FTransform(
            Rotation,
            CenterMeters * CentimetersPerMeter,
            SizeMeters), false);
    }
}

void ATRIADIstanaBuildingActor::AddCylinder(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FVector& CenterMeters,
    double RadiusMeters,
    double CylinderHeightMeters,
    const FRotator& Rotation)
{
    if (Component)
    {
        Component->AddInstance(FTransform(
            Rotation,
            CenterMeters * CentimetersPerMeter,
            FVector(RadiusMeters * 2.0, RadiusMeters * 2.0, CylinderHeightMeters)), false);
    }
}

void ATRIADIstanaBuildingActor::AddSphere(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FVector& CenterMeters,
    double RadiusMeters)
{
    if (Component)
    {
        Component->AddInstance(FTransform(
            FRotator::ZeroRotator,
            CenterMeters * CentimetersPerMeter,
            FVector(RadiusMeters * 2.0)), false);
    }
}

void ATRIADIstanaBuildingActor::ApplyMaterials()
{
    UMaterialInterface* Fallback = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    Masonry->SetMaterial(0, LoadMaterialWithFallback(
        TEXT("/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall"), Fallback));
    MasonryAccent->SetMaterial(0, Masonry->GetMaterial(0));
    Columns->SetMaterial(0, Masonry->GetMaterial(0));
    Roofs->SetMaterial(0, LoadMaterialWithFallback(
        TEXT("/Game/StarterContent/Materials/M_Wood_Walnut.M_Wood_Walnut"), Fallback));
    DarkDetails->SetMaterial(0, Roofs->GetMaterial(0));
    Glass->SetMaterial(0, LoadMaterialWithFallback(
        TEXT("/Game/StarterContent/Materials/M_Glass.M_Glass"), Fallback));
    Metal->SetMaterial(0, LoadMaterialWithFallback(
        TEXT("/Game/StarterContent/Materials/M_Metal_Gold.M_Metal_Gold"), Fallback));
    Stone->SetMaterial(0, LoadMaterialWithFallback(
        TEXT("/Game/StarterContent/Materials/M_Concrete_Grime.M_Concrete_Grime"), Fallback));
    Landscape->SetMaterial(0, LoadMaterialWithFallback(
        TEXT("/Game/StarterContent/Materials/M_Ground_Grass.M_Ground_Grass"), Fallback));
    Water->SetMaterial(0, LoadMaterialWithFallback(
        TEXT("/Game/StarterContent/Materials/M_Water_Lake.M_Water_Lake"), Fallback));
    Domes->SetMaterial(0, Masonry->GetMaterial(0));
    Foliage->SetMaterial(0, Landscape->GetMaterial(0));
    RoofCones->SetMaterial(0, Roofs->GetMaterial(0));
}

void ATRIADIstanaBuildingActor::RebuildExterior()
{
    const TArray<UHierarchicalInstancedStaticMeshComponent*> AllComponents = {
        Masonry, MasonryAccent, Columns, Roofs, DarkDetails, Glass,
        Metal, Stone, Landscape, Water, Domes, Foliage, RoofCones};
    for (UHierarchicalInstancedStaticMeshComponent* Component : AllComponents)
    {
        if (Component)
        {
            Component->ClearInstances();
        }
    }
    ApplyMaterials();

    // Plinth and public-facing formal grounds. Thin landscaping is visual only;
    // masonry, roofs, columns, stairs, and fountain stone retain collision.
    AddBox(MasonryAccent, FVector(0.0, -2.0, 0.75), FVector(124.0, 112.0, 1.5));
    AddBox(Landscape, FVector(0.0, 104.0, -0.20), FVector(190.0, 180.0, 0.30));
    AddBox(MasonryAccent, FVector(0.0, 62.0, 0.05), FVector(54.0, 38.0, 0.25));

    // Cross-shaped two-storey body, scaled to the mapping-grade envelope.
    AddBox(Masonry, FVector(0.0, 0.0, 7.25), FVector(112.0, 18.0, 12.5));
    AddBox(Masonry, FVector(0.0, -3.0, 7.25), FVector(26.0, 94.0, 12.5));
    AddBox(MasonryAccent, FVector(0.0, 0.0, 1.25), FVector(116.0, 24.0, 0.50));
    AddBox(MasonryAccent, FVector(0.0, 0.0, 7.15), FVector(116.0, 25.0, 0.42));
    AddBox(MasonryAccent, FVector(0.0, 0.0, 13.45), FVector(118.0, 25.0, 0.55));
    AddBox(MasonryAccent, FVector(0.0, -3.0, 7.15), FVector(31.0, 98.0, 0.42));
    AddBox(MasonryAccent, FVector(0.0, -3.0, 13.45), FVector(32.0, 98.0, 0.55));

    // Open front/rear veranda rhythm. Columns and deep slab edges create true
    // gaps between bays instead of a flat facade-only texture.
    for (double X = -55.0; X <= 55.01; X += 5.5)
    {
        if (FMath::Abs(X) < 15.0)
        {
            continue;
        }
        for (double VerandaY : {12.0, -12.0})
        {
            AddCylinder(Columns, FVector(X, VerandaY, 4.20), 0.42, 5.45);
            AddCylinder(Columns, FVector(X, VerandaY, 10.25), 0.36, 5.15);
            AddBox(MasonryAccent, FVector(X, VerandaY, 1.55), FVector(1.25, 1.25, 0.35));
            AddBox(MasonryAccent, FVector(X, VerandaY, 6.98), FVector(1.45, 1.45, 0.35));
            AddBox(MasonryAccent, FVector(X, VerandaY, 12.92), FVector(1.20, 1.20, 0.28));
        }
    }
    AddBox(MasonryAccent, FVector(-35.0, 12.0, 13.0), FVector(42.0, 1.0, 0.8));
    AddBox(MasonryAccent, FVector(35.0, 12.0, 13.0), FVector(42.0, 1.0, 0.8));
    AddBox(MasonryAccent, FVector(-35.0, -12.0, 13.0), FVector(42.0, 1.0, 0.8));
    AddBox(MasonryAccent, FVector(35.0, -12.0, 13.0), FVector(42.0, 1.0, 0.8));

    // Stylized Doric/Ionic veranda balustrades and capital/base layers.
    for (double X = -52.0; X <= 52.01; X += 2.6)
    {
        if (FMath::Abs(X) < 15.0)
        {
            continue;
        }
        AddBox(MasonryAccent, FVector(X, 12.7, 7.65), FVector(0.18, 0.18, 1.2));
        AddBox(MasonryAccent, FVector(X, -12.7, 7.65), FVector(0.18, 0.18, 1.2));
    }
    AddBox(MasonryAccent, FVector(-35.0, 12.7, 8.25), FVector(42.0, 0.22, 0.22));
    AddBox(MasonryAccent, FVector(35.0, 12.7, 8.25), FVector(42.0, 0.22, 0.22));
    AddBox(MasonryAccent, FVector(-35.0, -12.7, 8.25), FVector(42.0, 0.22, 0.22));
    AddBox(MasonryAccent, FVector(35.0, -12.7, 8.25), FVector(42.0, 0.22, 0.22));

    // Shuttered/louvred windows. Dark recess panels sit behind thin horizontal
    // louvres; repeated spacing matches the long, symmetrical public facade.
    for (double X = -51.0; X <= 51.01; X += 6.0)
    {
        if (FMath::Abs(X) < 15.0)
        {
            continue;
        }
        for (double Z : {4.25, 10.25})
        {
            AddBox(Glass, FVector(X, 9.08, Z), FVector(2.2, 0.16, 2.55));
            AddBox(Glass, FVector(X, -9.08, Z), FVector(2.2, 0.16, 2.55));
            for (double SlatZ : {Z - 0.75, Z, Z + 0.75})
            {
                AddBox(DarkDetails, FVector(X, 9.20, SlatZ), FVector(2.55, 0.10, 0.13));
                AddBox(DarkDetails, FVector(X, -9.20, SlatZ), FVector(2.55, 0.10, 0.13));
            }
        }
    }
    for (double Y = -43.0; Y <= 39.01; Y += 7.0)
    {
        for (double XSide : {-13.08, 13.08})
        {
            for (double Z : {4.25, 10.25})
            {
                AddBox(Glass, FVector(XSide, Y, Z), FVector(0.16, 2.2, 2.55));
                AddBox(DarkDetails, FVector(XSide + (XSide < 0.0 ? -0.12 : 0.12), Y, Z),
                    FVector(0.10, 2.55, 0.13));
            }
        }
    }

    // Central south-facing portico and broad ceremonial stair.
    AddBox(MasonryAccent, FVector(0.0, 46.0, 1.15), FVector(30.0, 10.0, 0.8));
    for (double X : {-10.5, -3.5, 3.5, 10.5})
    {
        AddCylinder(Columns, FVector(X, 48.0, 7.25), 0.52, 11.5);
        AddBox(MasonryAccent, FVector(X, 48.0, 1.65), FVector(1.8, 1.8, 0.45));
        AddBox(MasonryAccent, FVector(X, 48.0, 13.0), FVector(2.0, 2.0, 0.45));
    }
    AddBox(MasonryAccent, FVector(0.0, 48.0, 13.4), FVector(30.0, 3.0, 0.8));
    AddBox(Roofs, FVector(0.0, 48.0, 15.0), FVector(31.0, 11.0, 1.0), FRotator(0.0, 0.0, 0.0));
    for (int32 StepIndex = 0; StepIndex < 9; ++StepIndex)
    {
        const double StepHeight = 0.22 * (StepIndex + 1);
        const double StepY = 52.0 + StepIndex * 1.25;
        const double StepWidth = 30.0 - StepIndex * 0.55;
        AddBox(MasonryAccent, FVector(0.0, StepY, StepHeight * 0.5),
            FVector(StepWidth, 1.40, StepHeight));
    }

    // Panelled public-facing doors beneath the portico.
    for (double DoorX : {-7.0, 0.0, 7.0})
    {
        AddBox(DarkDetails, FVector(DoorX, 44.08, 3.25), FVector(3.1, 0.20, 5.0));
        AddCylinder(Metal, FVector(DoorX + 0.85, 44.25, 3.20), 0.09, 0.20, FRotator(90.0, 0.0, 0.0));
    }

    // Three-storey tower with stylized Corinthian upper colonnade.
    AddBox(Masonry, FVector(0.0, -2.0, 19.5), FVector(28.0, 27.0, 12.0));
    AddBox(MasonryAccent, FVector(0.0, -2.0, 13.8), FVector(31.0, 30.0, 0.65));
    AddBox(MasonryAccent, FVector(0.0, -2.0, 25.4), FVector(31.0, 30.0, 0.65));
    for (double TowerX : {-10.5, -3.5, 3.5, 10.5})
    {
        AddCylinder(Columns, FVector(TowerX, 12.3, 19.6), 0.45, 9.8);
        AddBox(MasonryAccent, FVector(TowerX, 12.3, 15.0), FVector(1.7, 1.7, 0.35));
        AddBox(MasonryAccent, FVector(TowerX, 12.3, 24.3), FVector(2.0, 2.0, 0.45));
    }
    for (double TowerX : {-8.0, 0.0, 8.0})
    {
        AddBox(Glass, FVector(TowerX, 11.58, 20.0), FVector(3.0, 0.18, 4.0));
        for (double SlatZ : {18.9, 20.0, 21.1})
        {
            AddBox(DarkDetails, FVector(TowerX, 11.72, SlatZ), FVector(3.3, 0.10, 0.14));
        }
    }

    // Layered mansard roof silhouettes on both cross axes.
    AddBox(Roofs, FVector(0.0, 0.0, 14.35), FVector(114.0, 22.0, 0.65));
    AddBox(Roofs, FVector(0.0, 8.7, 16.25), FVector(114.0, 7.0, 0.8), FRotator(0.0, 0.0, 31.0));
    AddBox(Roofs, FVector(0.0, -8.7, 16.25), FVector(114.0, 7.0, 0.8), FRotator(0.0, 0.0, -31.0));
    AddBox(Roofs, FVector(0.0, 0.0, 18.0), FVector(114.0, 11.0, 0.65));
    AddBox(Roofs, FVector(0.0, -3.0, 14.45), FVector(30.0, 96.0, 0.70));
    AddBox(Roofs, FVector(10.5, -3.0, 16.2), FVector(7.0, 96.0, 0.8), FRotator(31.0, 0.0, 0.0));
    AddBox(Roofs, FVector(-10.5, -3.0, 16.2), FVector(7.0, 96.0, 0.8), FRotator(-31.0, 0.0, 0.0));
    AddBox(Roofs, FVector(0.0, -3.0, 18.0), FVector(14.0, 96.0, 0.65));

    // Tower mansard, dormers, cupola, and flag mast.
    AddBox(Roofs, FVector(0.0, -2.0, 26.0), FVector(31.0, 30.0, 0.75));
    AddBox(Roofs, FVector(0.0, 10.0, 28.2), FVector(31.0, 8.0, 0.75), FRotator(0.0, 0.0, 34.0));
    AddBox(Roofs, FVector(0.0, -14.0, 28.2), FVector(31.0, 8.0, 0.75), FRotator(0.0, 0.0, -34.0));
    AddBox(Roofs, FVector(12.0, -2.0, 28.2), FVector(8.0, 30.0, 0.75), FRotator(34.0, 0.0, 0.0));
    AddBox(Roofs, FVector(-12.0, -2.0, 28.2), FVector(8.0, 30.0, 0.75), FRotator(-34.0, 0.0, 0.0));
    AddBox(Roofs, FVector(0.0, -2.0, 30.5), FVector(17.0, 16.0, 0.7));
    for (double DormerX : {-8.0, 0.0, 8.0})
    {
        AddBox(Masonry, FVector(DormerX, 10.8, 28.3), FVector(3.3, 2.2, 3.0));
        AddBox(Glass, FVector(DormerX, 12.0, 28.4), FVector(2.0, 0.15, 1.7));
    }
    AddCylinder(Columns, FVector(0.0, -2.0, 32.0), 3.8, 2.6);
    AddSphere(Domes, FVector(0.0, -2.0, 34.0), 3.6);
    AddCylinder(Metal, FVector(0.0, -2.0, 39.0), 0.12, 7.0);

    // Circular fountain and clipped formal-water stack.
    AddCylinder(Stone, FVector(0.0, 94.0, 0.55), 9.0, 1.1);
    AddCylinder(Water, FVector(0.0, 94.0, 1.12), 7.7, 0.18);
    AddCylinder(Stone, FVector(0.0, 94.0, 1.75), 1.5, 2.0);
    AddCylinder(Water, FVector(0.0, 94.0, 4.0), 0.15, 4.5);
    AddSphere(Water, FVector(0.0, 94.0, 6.3), 0.55);

    // Low clipped shrubs frame the approach without inventing restricted routes.
    for (double X : {-72.0, -58.0, 58.0, 72.0})
    {
        for (double Y : {62.0, 88.0, 116.0, 142.0})
        {
            AddSphere(Foliage, FVector(X, Y, 1.4), 1.4);
        }
    }
}

bool ATRIADIstanaBuildingActor::TrySnapFoundationToSurface(double TraceHalfHeightMeters)
{
    UWorld* World = GetWorld();
    if (!World || TraceHalfHeightMeters <= 0.0)
    {
        return false;
    }

    const FVector Origin = GetActorLocation();
    const FVector Start = Origin + FVector::UpVector * TraceHalfHeightMeters * CentimetersPerMeter;
    const FVector End = Origin - FVector::UpVector * TraceHalfHeightMeters * CentimetersPerMeter;
    FCollisionQueryParams QueryParameters(SCENE_QUERY_STAT(TRIADIstanaSurfaceSnap), true);
    QueryParameters.AddIgnoredActor(this);
    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParameters))
    {
        return false;
    }

    SetActorLocation(FVector(Origin.X, Origin.Y, Hit.ImpactPoint.Z));
    if (GlobeAnchor)
    {
        const FVector Updated = GlobeAnchor->GetLongitudeLatitudeHeight();
        LongitudeDegrees = Updated.X;
        LatitudeDegrees = Updated.Y;
        HeightMeters = Updated.Z;
    }
    return true;
}
