#include "TRIADDemoDroneActor.h"

#include "CesiumGeoreference.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TRIADRFEmitterComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr double DemoDroneMaximumVisualDimensionCentimeters = 100.0;

void NormalizeDemoDroneVisualScale(UStaticMeshComponent* VisualMesh)
{
    if (!VisualMesh || !VisualMesh->GetStaticMesh())
    {
        return;
    }
    const FVector MeshSizeCentimeters = VisualMesh->GetStaticMesh()->GetBoundingBox().GetSize();
    const double LargestDimensionCentimeters = FMath::Max3(
        MeshSizeCentimeters.X,
        MeshSizeCentimeters.Y,
        MeshSizeCentimeters.Z);
    if (LargestDimensionCentimeters <= UE_DOUBLE_SMALL_NUMBER)
    {
        return;
    }

    // The bundled AirSim quadcopter body is authored at roughly 15.9 m in its
    // longest raw mesh dimension. Normalize every configured/fallback mesh to a
    // one-metre maximum visual dimension so PTZ imagery and projected boxes use
    // a plausible small-UAS scale. The separate 0.5 x 0.2 m analytic pinhole
    // acceptance fixture remains deliberately conservative.
    const float UniformScale = static_cast<float>(
        DemoDroneMaximumVisualDimensionCentimeters / LargestDimensionCentimeters);
    VisualMesh->SetRelativeScale3D(FVector(UniformScale));
}
}

ATRIADDemoDroneActor::ATRIADDemoDroneActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(SceneRoot);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    VisualMesh->SetCollisionResponseToAllChannels(ECR_Block);
    VisualMesh->SetRelativeScale3D(FVector(1.0));
    // The bundled asset's thin axis is local Y. Roll it onto the world vertical
    // axis so the normalized 100 x 84 x 19 cm body reads as a horizontal quad,
    // rather than an 84 cm-tall sliver, in narrow-FOV PTZ imagery.
    VisualMesh->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));

    // The enabled AirSimTriadRuntime plugin ships this recognizable quadrotor.
    // Individual scenario definitions can still replace it with VisualMeshPath.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultDroneMeshFinder(
        TEXT("/AirSimTriadRuntime/Models/MiniQuadCopter/QuadcopterBody.QuadcopterBody"));
    if (DefaultDroneMeshFinder.Succeeded())
    {
        VisualMesh->SetStaticMesh(DefaultDroneMeshFinder.Object);
    }
    else
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (FallbackMeshFinder.Succeeded())
        {
            VisualMesh->SetStaticMesh(FallbackMeshFinder.Object);
        }
    }
    NormalizeDemoDroneVisualScale(VisualMesh);

    TargetLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TargetLabel"));
    TargetLabel->SetupAttachment(SceneRoot);
    TargetLabel->SetRelativeLocation(FVector(0.0, 0.0, 250.0));
    TargetLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    TargetLabel->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    TargetLabel->SetTextRenderColor(FColor::Magenta);
    TargetLabel->SetWorldSize(120.0f);
    TargetLabel->bAlwaysRenderAsText = true;
    TargetLabel->SetCastShadow(false);
    TargetLabel->ComponentTags.AddUnique(TEXT("TRIADHumanOnlyOverlay"));
    TargetLabel->SetText(FText::FromString(TEXT("[SIM DRONE]\nDrone1")));

    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("CesiumGlobeAnchor"));
    RFEmitter = CreateDefaultSubobject<UTRIADRFEmitterComponent>(TEXT("RFEmitter"));
    Tags.AddUnique(TEXT("DroneTarget"));
}

void ATRIADDemoDroneActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (const UWorld* World = GetWorld())
    {
        UpdateGeodeticPosition(World->GetTimeSeconds() - TrajectoryStartWorldSeconds);
    }

    // Keep the operator-only label legible without baking it into RGB, depth,
    // or event-camera input. Sensor captures explicitly hide tagged overlays.
    if (TargetLabel && TargetLabel->IsVisible())
    {
        if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
        {
            const FVector ToCamera = CameraManager->GetCameraLocation() - TargetLabel->GetComponentLocation();
            if (!ToCamera.IsNearlyZero())
            {
                FRotator FacingRotation = ToCamera.Rotation();
                FacingRotation.Yaw += 180.0f;
                TargetLabel->SetWorldRotation(FacingRotation);
            }
        }
    }
}

void ATRIADDemoDroneActor::ConfigureDemoTarget(
    const FTRIADDemoTargetDefinition& InDefinition,
    ACesiumGeoreference* InGeoreference)
{
    Definition = InDefinition;
    RFEmitter->ConfigureEmitter(Definition.RFEmitter);
    const FString LabelText = Definition.IngressCorridorId.IsEmpty()
        ? FString::Printf(TEXT("[SIM DRONE]\n%s"), *Definition.ActorName)
        : FString::Printf(TEXT("[SIM DRONE]\n%s\n%s"), *Definition.ActorName, *Definition.IngressCorridorId);
    TargetLabel->SetText(FText::FromString(LabelText));
    Tags.AddUnique(TEXT("DroneTarget"));

    if (!Definition.VisualMeshPath.IsEmpty())
    {
        if (UStaticMesh* ConfiguredMesh = LoadObject<UStaticMesh>(nullptr, *Definition.VisualMeshPath))
        {
            VisualMesh->SetStaticMesh(ConfiguredMesh);
        }
        else
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("TRIAD demo target mesh '%s' was unavailable; retaining the default quadrotor/fallback mesh."),
                *Definition.VisualMeshPath);
        }
    }
    NormalizeDemoDroneVisualScale(VisualMesh);

    if (InGeoreference)
    {
        GlobeAnchor->SetGeoreference(TSoftObjectPtr<ACesiumGeoreference>(InGeoreference));
    }

    TrajectoryStartWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    UpdateGeodeticPosition(0.0);
    SetActorTickEnabled(Definition.Trajectory != ETRIADDemoTrajectory::Stationary);
}

void ATRIADDemoDroneActor::UpdateGeodeticPosition(double ElapsedSeconds)
{
    FVector OffsetEnuMeters = FVector::ZeroVector;
    FVector VelocityEnuMetersPerSecond = FVector::ZeroVector;
    if (Definition.Trajectory == ETRIADDemoTrajectory::Circular)
    {
        const double AngleRadians = FMath::DegreesToRadians(Definition.AngularSpeedDegreesPerSecond * ElapsedSeconds);
        const double RadiusMeters = FMath::Max(Definition.CircularRadiusMeters, 0.0);
        const double AngularSpeedRadiansPerSecond = FMath::DegreesToRadians(Definition.AngularSpeedDegreesPerSecond);
        OffsetEnuMeters.X = RadiusMeters * FMath::Cos(AngleRadians);
        OffsetEnuMeters.Y = RadiusMeters * FMath::Sin(AngleRadians);
        VelocityEnuMetersPerSecond.X = -RadiusMeters * AngularSpeedRadiansPerSecond * FMath::Sin(AngleRadians);
        VelocityEnuMetersPerSecond.Y = RadiusMeters * AngularSpeedRadiansPerSecond * FMath::Cos(AngleRadians);
    }
    else if (Definition.Trajectory == ETRIADDemoTrajectory::Linear)
    {
        const FVector Direction = Definition.LinearDirectionEnu.GetSafeNormal();
        const double PathLength = FMath::Max(Definition.LinearDistanceMeters, 0.0);
        const double Travel = FMath::Max(Definition.LinearSpeedMetersPerSecond, 0.0) * FMath::Max(ElapsedSeconds, 0.0);
        double PathPosition = 0.0;
        if (PathLength > UE_DOUBLE_SMALL_NUMBER)
        {
            if (Definition.bPingPongLinearPath)
            {
                const double CyclePosition = FMath::Fmod(Travel, PathLength * 2.0);
                PathPosition = CyclePosition <= PathLength ? CyclePosition : PathLength * 2.0 - CyclePosition;
                VelocityEnuMetersPerSecond = Direction *
                    (CyclePosition <= PathLength ? Definition.LinearSpeedMetersPerSecond : -Definition.LinearSpeedMetersPerSecond);
            }
            else
            {
                PathPosition = FMath::Fmod(Travel, PathLength);
                VelocityEnuMetersPerSecond = Direction * Definition.LinearSpeedMetersPerSecond;
            }
        }
        OffsetEnuMeters = Direction * PathPosition;
    }

    CurrentSpeedMetersPerSecond = VelocityEnuMetersPerSecond.Length();
    if (CurrentSpeedMetersPerSecond > UE_DOUBLE_SMALL_NUMBER)
    {
        CurrentHeadingDegrees = FMath::Fmod(
            FMath::RadiansToDegrees(FMath::Atan2(VelocityEnuMetersPerSecond.X, VelocityEnuMetersPerSecond.Y)) + 360.0,
            360.0);
    }

    constexpr double Wgs84EquatorialRadiusMeters = 6378137.0;
    const double StartLatitudeRadians = FMath::DegreesToRadians(Definition.StartLatitudeDegrees);
    const double CosLatitude = FMath::Max(FMath::Abs(FMath::Cos(StartLatitudeRadians)), 0.000001);
    const double LatitudeDegrees = Definition.StartLatitudeDegrees +
        FMath::RadiansToDegrees(OffsetEnuMeters.Y / Wgs84EquatorialRadiusMeters);
    const double LongitudeDegrees = Definition.StartLongitudeDegrees +
        FMath::RadiansToDegrees(OffsetEnuMeters.X / (Wgs84EquatorialRadiusMeters * CosLatitude));
    const double HeightMeters = Definition.StartHeightMeters + OffsetEnuMeters.Z;

    GlobeAnchor->MoveToLongitudeLatitudeHeight(FVector(LongitudeDegrees, LatitudeDegrees, HeightMeters));
}
