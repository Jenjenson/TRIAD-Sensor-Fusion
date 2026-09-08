#include "TRIADIstanaFreeRoamPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Scene.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
constexpr double CentimetersPerMeter = 100.0;
constexpr float MaximumInputDeltaSeconds = 0.1f;
constexpr float ExploreFieldOfViewDegrees = 80.0f;
constexpr float ExploreCameraShutterSpeed = 125.0f;
constexpr float ExploreCameraIso = 100.0f;
constexpr float ExploreCameraFStop = 8.0f;
constexpr float ExploreCameraWhiteTemperature = 6500.0f;

void ConfigureExploreCameraProfile(UCameraComponent* Camera)
{
    if (!Camera)
    {
        return;
    }

    Camera->SetFieldOfView(ExploreFieldOfViewDegrees);
    Camera->SetPostProcessBlendWeight(1.0f);
    Camera->PostProcessSettings = FPostProcessSettings();
    FPostProcessSettings& Settings = Camera->PostProcessSettings;
    Settings.bOverride_AutoExposureMethod = true;
    Settings.AutoExposureMethod = AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Settings.AutoExposureApplyPhysicalCameraExposure = true;
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = 0.0f;
    Settings.bOverride_CameraShutterSpeed = true;
    Settings.CameraShutterSpeed = ExploreCameraShutterSpeed;
    Settings.bOverride_CameraISO = true;
    Settings.CameraISO = ExploreCameraIso;
    Settings.bOverride_DepthOfFieldFstop = true;
    Settings.DepthOfFieldFstop = ExploreCameraFStop;
    Settings.bOverride_DepthOfFieldScale = true;
    Settings.DepthOfFieldScale = 0.0f;
    Settings.bOverride_TemperatureType = true;
    Settings.TemperatureType = TEMP_WhiteBalance;
    Settings.bOverride_WhiteTemp = true;
    Settings.WhiteTemp = ExploreCameraWhiteTemperature;
    Settings.bOverride_WhiteTint = true;
    Settings.WhiteTint = 0.0f;
    Settings.bOverride_LocalExposureHighlightContrastScale = true;
    Settings.LocalExposureHighlightContrastScale = 1.0f;
    Settings.bOverride_LocalExposureShadowContrastScale = true;
    Settings.LocalExposureShadowContrastScale = 1.0f;
    Settings.bOverride_LocalExposureDetailStrength = true;
    Settings.LocalExposureDetailStrength = 1.0f;
    Settings.bOverride_LocalExposureMiddleGreyBias = true;
    Settings.LocalExposureMiddleGreyBias = 0.0f;
    Settings.bOverride_LocalExposureHighlightContrastCurve = true;
    Settings.LocalExposureHighlightContrastCurve = nullptr;
    Settings.bOverride_LocalExposureShadowContrastCurve = true;
    Settings.LocalExposureShadowContrastCurve = nullptr;
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = 0.0f;
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = 0.0f;
    Settings.bOverride_MotionBlurAmount = true;
    Settings.MotionBlurAmount = 0.0f;
    Settings.bOverride_SceneFringeIntensity = true;
    Settings.SceneFringeIntensity = 0.0f;
}

bool HasExploreCameraProfile(const UCameraComponent* Camera)
{
    if (!Camera)
    {
        return false;
    }

    const FPostProcessSettings& Settings = Camera->PostProcessSettings;
    return FMath::IsNearlyEqual(Camera->FieldOfView, ExploreFieldOfViewDegrees) &&
        FMath::IsNearlyEqual(Camera->PostProcessBlendWeight, 1.0f) &&
        Settings.bOverride_AutoExposureMethod &&
        Settings.AutoExposureMethod == AEM_Manual &&
        Settings.bOverride_AutoExposureApplyPhysicalCameraExposure &&
        Settings.AutoExposureApplyPhysicalCameraExposure &&
        Settings.bOverride_AutoExposureBias &&
        FMath::IsNearlyZero(Settings.AutoExposureBias) &&
        Settings.bOverride_CameraShutterSpeed &&
        FMath::IsNearlyEqual(
            Settings.CameraShutterSpeed,
            ExploreCameraShutterSpeed) &&
        Settings.bOverride_CameraISO &&
        FMath::IsNearlyEqual(Settings.CameraISO, ExploreCameraIso) &&
        Settings.bOverride_DepthOfFieldFstop &&
        FMath::IsNearlyEqual(Settings.DepthOfFieldFstop, ExploreCameraFStop) &&
        Settings.bOverride_DepthOfFieldScale &&
        FMath::IsNearlyZero(Settings.DepthOfFieldScale) &&
        Settings.bOverride_TemperatureType &&
        Settings.TemperatureType == TEMP_WhiteBalance &&
        Settings.bOverride_WhiteTemp &&
        FMath::IsNearlyEqual(
            Settings.WhiteTemp,
            ExploreCameraWhiteTemperature) &&
        Settings.bOverride_WhiteTint &&
        FMath::IsNearlyZero(Settings.WhiteTint) &&
        Settings.bOverride_LocalExposureHighlightContrastScale &&
        FMath::IsNearlyEqual(
            Settings.LocalExposureHighlightContrastScale,
            1.0f) &&
        Settings.bOverride_LocalExposureShadowContrastScale &&
        FMath::IsNearlyEqual(
            Settings.LocalExposureShadowContrastScale,
            1.0f) &&
        Settings.bOverride_LocalExposureDetailStrength &&
        FMath::IsNearlyEqual(Settings.LocalExposureDetailStrength, 1.0f) &&
        Settings.bOverride_LocalExposureMiddleGreyBias &&
        FMath::IsNearlyZero(Settings.LocalExposureMiddleGreyBias) &&
        Settings.bOverride_LocalExposureHighlightContrastCurve &&
        Settings.LocalExposureHighlightContrastCurve == nullptr &&
        Settings.bOverride_LocalExposureShadowContrastCurve &&
        Settings.LocalExposureShadowContrastCurve == nullptr &&
        Settings.bOverride_BloomIntensity &&
        FMath::IsNearlyZero(Settings.BloomIntensity) &&
        Settings.bOverride_VignetteIntensity &&
        FMath::IsNearlyZero(Settings.VignetteIntensity) &&
        Settings.bOverride_MotionBlurAmount &&
        FMath::IsNearlyZero(Settings.MotionBlurAmount) &&
        Settings.bOverride_SceneFringeIntensity &&
        FMath::IsNearlyZero(Settings.SceneFringeIntensity);
}

bool IsEitherKeyDown(
    const APlayerController& PlayerController,
    const FKey& FirstKey,
    const FKey& SecondKey)
{
    return PlayerController.IsInputKeyDown(FirstKey) ||
        PlayerController.IsInputKeyDown(SecondKey);
}

float GetDigitalAxis(
    const APlayerController& PlayerController,
    const FKey& PositiveKey,
    const FKey& NegativeKey)
{
    const float Positive = PlayerController.IsInputKeyDown(PositiveKey) ? 1.0f : 0.0f;
    const float Negative = PlayerController.IsInputKeyDown(NegativeKey) ? 1.0f : 0.0f;
    return Positive - Negative;
}
}

ATRIADIstanaFreeRoamPawn::ATRIADIstanaFreeRoamPawn(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
    SetRootComponent(CollisionCapsule);
    CollisionCapsule->InitCapsuleSize(34.0f, 88.0f);
    CollisionCapsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
    CollisionCapsule->SetGenerateOverlapEvents(false);
    CollisionCapsule->SetCanEverAffectNavigation(false);
    CollisionCapsule->CanCharacterStepUpOn = ECB_No;

    ExploreCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ExploreCamera"));
    ExploreCamera->SetupAttachment(CollisionCapsule);
    ExploreCamera->SetRelativeLocation(FVector(0.0, 0.0, 64.0));
    ExploreCamera->bAutoActivate = true;
    ExploreCamera->bUsePawnControlRotation = false;
    ConfigureExploreCameraProfile(ExploreCamera);

    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    SetActorEnableCollision(true);
}

void ATRIADIstanaFreeRoamPawn::BeginPlay()
{
    Super::BeginPlay();

    const FRotator SpawnRotation = GetActorRotation();
    TravelAnchorLocation = GetActorLocation();
    bTravelAnchorInitialized = true;
    ViewYawDegrees = FRotator::NormalizeAxis(SpawnRotation.Yaw);
    ViewPitchDegrees = FMath::Clamp(
        FRotator::NormalizeAxis(SpawnRotation.Pitch),
        MinimumViewPitchDegrees,
        MaximumViewPitchDegrees);
    SetActorRotation(FRotator(0.0f, ViewYawDegrees, 0.0f));
    ExploreCamera->SetRelativeRotation(
        FRotator(ViewPitchDegrees, 0.0f, 0.0f));
    ExploreCamera->Activate(true);

    ConfigureLocalPlayer(Cast<APlayerController>(GetController()));
}

UCameraComponent* ATRIADIstanaFreeRoamPawn::GetExploreCameraComponent() const
{
    return ExploreCamera.Get();
}

bool ATRIADIstanaFreeRoamPawn::HasExpectedExploreCameraProfile() const
{
    return HasExploreCameraProfile(ExploreCamera.Get());
}

void ATRIADIstanaFreeRoamPawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    ConfigureLocalPlayer(Cast<APlayerController>(NewController));
}

void ATRIADIstanaFreeRoamPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return;
    }

    EnsureLocalPlayerViewTarget(PlayerController);
    ApplyMouseLook(PlayerController);
    ApplyMovement(PlayerController, DeltaSeconds);
}

void ATRIADIstanaFreeRoamPawn::ConfigureLocalPlayer(
    APlayerController* PlayerController)
{
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return;
    }

    FInputModeGameOnly InputMode;
    InputMode.SetConsumeCaptureMouseDown(true);
    PlayerController->SetInputMode(InputMode);
    PlayerController->bShowMouseCursor = false;
    PlayerController->SetIgnoreLookInput(false);
    PlayerController->SetIgnoreMoveInput(false);
    PlayerController->SetViewTarget(this);
}

void ATRIADIstanaFreeRoamPawn::EnsureLocalPlayerViewTarget(
    APlayerController* PlayerController)
{
    if (PlayerController && PlayerController->GetViewTarget() != this)
    {
        PlayerController->SetViewTarget(this);
    }
}

void ATRIADIstanaFreeRoamPawn::ApplyMouseLook(
    APlayerController* PlayerController)
{
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    PlayerController->GetInputMouseDelta(MouseX, MouseY);

    ViewYawDegrees = FRotator::NormalizeAxis(
        ViewYawDegrees + MouseX * MouseSensitivityDegreesPerCount);
    ViewPitchDegrees = FMath::Clamp(
        ViewPitchDegrees - MouseY * MouseSensitivityDegreesPerCount,
        MinimumViewPitchDegrees,
        MaximumViewPitchDegrees);

    SetActorRotation(FRotator(0.0f, ViewYawDegrees, 0.0f));
    ExploreCamera->SetRelativeRotation(FRotator(ViewPitchDegrees, 0.0f, 0.0f));
}

void ATRIADIstanaFreeRoamPawn::ApplyMovement(
    APlayerController* PlayerController,
    float DeltaSeconds)
{
    const float ForwardInput = GetDigitalAxis(*PlayerController, EKeys::W, EKeys::S);
    const float RightInput = GetDigitalAxis(*PlayerController, EKeys::D, EKeys::A);
    const float VerticalInput = GetDigitalAxis(*PlayerController, EKeys::E, EKeys::Q);

    FVector MovementDirection =
        GetActorForwardVector() * ForwardInput +
        GetActorRightVector() * RightInput +
        FVector::UpVector * VerticalInput;
    MovementDirection = MovementDirection.GetClampedToMaxSize(1.0);
    if (MovementDirection.IsNearlyZero())
    {
        return;
    }

    float SpeedMultiplier = 1.0f;
    const bool bSlowMovement = IsEitherKeyDown(
        *PlayerController,
        EKeys::LeftControl,
        EKeys::RightControl);
    const bool bBoostMovement = IsEitherKeyDown(
        *PlayerController,
        EKeys::LeftShift,
        EKeys::RightShift);
    if (bSlowMovement)
    {
        SpeedMultiplier = SlowSpeedMultiplier;
    }
    else if (bBoostMovement)
    {
        SpeedMultiplier = BoostSpeedMultiplier;
    }

    const float SafeDeltaSeconds = FMath::Clamp(
        DeltaSeconds,
        0.0f,
        MaximumInputDeltaSeconds);
    const FVector DesiredDelta = MovementDirection *
        BaseMovementSpeedCentimetersPerSecond * SpeedMultiplier * SafeDeltaSeconds;
    MoveWithCollisionAndSlide(DesiredDelta);
}

FVector ATRIADIstanaFreeRoamPawn::ClampToTravelVolume(
    const FVector& DesiredLocation) const
{
    if (!bTravelAnchorInitialized)
    {
        return DesiredLocation;
    }

    const double MaximumRadiusCentimeters =
        FMath::Max(0.0, static_cast<double>(MaximumHorizontalTravelMeters)) *
        CentimetersPerMeter;
    FVector2D HorizontalOffset(
        DesiredLocation.X - TravelAnchorLocation.X,
        DesiredLocation.Y - TravelAnchorLocation.Y);
    const double HorizontalDistanceCentimeters = HorizontalOffset.Size();
    if (HorizontalDistanceCentimeters > MaximumRadiusCentimeters &&
        HorizontalDistanceCentimeters > UE_DOUBLE_SMALL_NUMBER)
    {
        HorizontalOffset *= MaximumRadiusCentimeters / HorizontalDistanceCentimeters;
    }

    FVector ClampedLocation(
        TravelAnchorLocation.X + HorizontalOffset.X,
        TravelAnchorLocation.Y + HorizontalOffset.Y,
        DesiredLocation.Z);
    const double MinimumAltitudeCentimeters =
        static_cast<double>(MinimumLocalAltitudeMeters) * CentimetersPerMeter;
    const double MaximumAltitudeCentimeters =
        FMath::Max(
            static_cast<double>(MinimumLocalAltitudeMeters),
            static_cast<double>(MaximumLocalAltitudeMeters)) *
        CentimetersPerMeter;
    ClampedLocation.Z = FMath::Clamp(
        ClampedLocation.Z,
        MinimumAltitudeCentimeters,
        MaximumAltitudeCentimeters);
    return ClampedLocation;
}

void ATRIADIstanaFreeRoamPawn::MoveWithCollisionAndSlide(
    const FVector& DesiredDelta)
{
    const FVector FirstTarget = ClampToTravelVolume(GetActorLocation() + DesiredDelta);
    const FVector FirstDelta = FirstTarget - GetActorLocation();

    FHitResult FirstHit;
    AddActorWorldOffset(FirstDelta, true, &FirstHit, ETeleportType::None);
    if (!FirstHit.bBlockingHit || FirstHit.Time >= 1.0f)
    {
        return;
    }

    const FVector RemainingDelta = FirstDelta * (1.0f - FirstHit.Time);
    const FVector SlideDelta = FVector::VectorPlaneProject(RemainingDelta, FirstHit.Normal);
    if (SlideDelta.IsNearlyZero())
    {
        return;
    }

    const FVector SlideTarget = ClampToTravelVolume(GetActorLocation() + SlideDelta);
    FHitResult SlideHit;
    AddActorWorldOffset(
        SlideTarget - GetActorLocation(),
        true,
        &SlideHit,
        ETeleportType::None);
}
