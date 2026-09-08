#include "TRIADIstanaExteriorMeshActor.h"

#include "CesiumGeoreference.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"

namespace
{
const FSoftObjectPath RequiredIstanaExteriorMeshPath(
    TEXT("/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined.SM_IstanaExterior_Refined"));
}

ATRIADIstanaExteriorMeshActor::ATRIADIstanaExteriorMeshActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
    GlobeAnchor->SetAdjustOrientationForGlobeWhenMoving(true);

    ExteriorMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExteriorMesh"));
    ExteriorMeshComponent->SetupAttachment(SceneRoot);
    ExteriorMeshComponent->SetMobility(EComponentMobility::Movable);
    ExteriorMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ExteriorMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    ExteriorMeshComponent->SetGenerateOverlapEvents(false);
    ExteriorMeshComponent->SetCastShadow(true);

    RequiredExteriorMeshAsset = TSoftObjectPtr<UStaticMesh>(RequiredIstanaExteriorMeshPath);
}

void ATRIADIstanaExteriorMeshActor::ConfigureGeodeticPlacement(
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
    GlobeAnchor->SetEastSouthUpRotation(
        FRotator(0.0, FootprintYawDegrees, 0.0).Quaternion());
}

bool ATRIADIstanaExteriorMeshActor::SetExteriorMesh(
    UStaticMesh* InExteriorMesh,
    FString& OutError)
{
    if (!InExteriorMesh || !ExteriorMeshComponent)
    {
        bRequiredExteriorMeshLoaded = false;
        OutError = TEXT("Required Istana exterior static mesh is unavailable; no procedural fallback was used.");
        return false;
    }

    ExteriorMeshComponent->SetStaticMesh(InExteriorMesh);
    if (ExteriorMeshComponent->GetStaticMesh() != InExteriorMesh)
    {
        bRequiredExteriorMeshLoaded = false;
        OutError = TEXT("Unreal refused the Istana exterior static-mesh assignment; no fallback substitution was made.");
        return false;
    }
    ExteriorMeshComponent->SetRelativeTransform(FTransform(
        FQuat::Identity,
        FVector::ZeroVector,
        FVector(
            FMath::Clamp(StreamedReplacementHorizontalScale, 1.0, 1.01),
            FMath::Clamp(StreamedReplacementHorizontalScale, 1.0, 1.01),
            1.0)));
    RequiredExteriorMeshAsset = TSoftObjectPtr<UStaticMesh>(InExteriorMesh);
    bRequiredExteriorMeshLoaded = true;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExteriorMeshActor::LoadRequiredExteriorMesh(FString& OutError)
{
    UStaticMesh* ExteriorMesh = RequiredExteriorMeshAsset.LoadSynchronous();
    return SetExteriorMesh(ExteriorMesh, OutError);
}
