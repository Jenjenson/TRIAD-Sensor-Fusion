#include "TRIADIstanaPublicViewRuntimePolicyActor.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SimMode/SimModeBase.h"
#include "TimerManager.h"
#include "Weather/WeatherLib.h"

namespace
{
const FString PublicViewClaimLabel(
    TEXT("PUBLIC_REFERENCE_VISUAL_APPROXIMATION_NOT_SURVEY_CONTROLLED"));
constexpr float PublicViewFieldOfViewDegrees = 52.0f;
constexpr float PublicViewWhiteBalanceKelvin = 6500.0f;
constexpr float PublicViewShutterSpeed = 125.0f;
constexpr float PublicViewIso = 100.0f;
constexpr float PublicViewAperture = 8.0f;
constexpr float PublicViewAspectRatio = 16.0f / 9.0f;
}

ATRIADIstanaPublicViewRuntimePolicyActor::ATRIADIstanaPublicViewRuntimePolicyActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);
    ClaimLabel = ExpectedClaimLabel();
}

const FString& ATRIADIstanaPublicViewRuntimePolicyActor::ExpectedClaimLabel()
{
    return PublicViewClaimLabel;
}

bool ATRIADIstanaPublicViewRuntimePolicyActor::IsFogSuppressionActive(
    int32& OutFogComponentCount) const
{
    OutFogComponentCount = 0;
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    bool bAllSuppressed = true;
    for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
    {
        AExponentialHeightFog* FogActor = *It;
        UExponentialHeightFogComponent* FogComponent = FogActor
            ? FogActor->GetComponent()
            : nullptr;
        if (!IsValid(FogActor) || FogActor->GetWorld() != World)
        {
            continue;
        }
        ++OutFogComponentCount;
        bAllSuppressed = bAllSuppressed && FogComponent &&
            FogComponent->FogDensity == 0.0f &&
            FogComponent->SecondFogData.FogDensity == 0.0f &&
            FogComponent->FogMaxOpacity == 0.0f &&
            !FogComponent->bEnableVolumetricFog;
    }
    return bAllSuppressed &&
        (!bSuppressAirSimVisualWeather ||
         (!UWeatherLib::getIsWeatherEnabled(World) &&
          FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
              World,
              EWeatherParamScalar::WEATHER_PARAM_SCALAR_RAIN), 0.001f) &&
          FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
              World,
              EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADWETNESS), 0.001f) &&
          FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
              World,
              EWeatherParamScalar::WEATHER_PARAM_SCALAR_FOG), 0.001f) &&
          FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
              World,
              EWeatherParamScalar::WEATHER_PARAM_SCALAR_DUST), 0.001f) &&
          FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
              World,
              EWeatherParamScalar::WEATHER_PARAM_SCALAR_SNOW), 0.001f) &&
          FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
              World,
              EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADSNOW), 0.001f) &&
          FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
              World,
              EWeatherParamScalar::WEATHER_PARAM_SCALAR_MAPLELEAF), 0.001f) &&
          FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
              World,
              EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADLEAF), 0.001f)));
}

bool ATRIADIstanaPublicViewRuntimePolicyActor::IsFixedPrimaryCameraProfileActive(
    FString& OutReason) const
{
    UWorld* World = GetWorld();
    ACameraActor* Camera = nullptr;
    int32 MatchingCameraCount = 0;
    if (World)
    {
        for (TActorIterator<ACameraActor> It(World); It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == World &&
                It->ActorHasTag(RequiredPrimaryCameraTag))
            {
                Camera = *It;
                ++MatchingCameraCount;
            }
        }
    }
    const UCameraComponent* CameraComponent = Camera
        ? Camera->GetCameraComponent()
        : nullptr;
    if (MatchingCameraCount != 1 || !CameraComponent)
    {
        OutReason = FString::Printf(
            TEXT("Expected one fixed primary public-view camera; found %d."),
            MatchingCameraCount);
        return false;
    }

    const FPostProcessSettings& Settings = CameraComponent->PostProcessSettings;
    const bool bVerified =
        FMath::IsNearlyEqual(CameraComponent->FieldOfView, PublicViewFieldOfViewDegrees) &&
        CameraComponent->bConstrainAspectRatio &&
        FMath::IsNearlyEqual(CameraComponent->AspectRatio, PublicViewAspectRatio) &&
        FMath::IsNearlyEqual(CameraComponent->PostProcessBlendWeight, 1.0f) &&
        Settings.bOverride_AutoExposureMethod &&
        Settings.AutoExposureMethod == AEM_Manual &&
        Settings.bOverride_AutoExposureApplyPhysicalCameraExposure &&
        Settings.AutoExposureApplyPhysicalCameraExposure &&
        Settings.bOverride_AutoExposureBias &&
        FMath::IsNearlyZero(Settings.AutoExposureBias) &&
        Settings.bOverride_CameraShutterSpeed &&
        FMath::IsNearlyEqual(Settings.CameraShutterSpeed, PublicViewShutterSpeed) &&
        Settings.bOverride_CameraISO &&
        FMath::IsNearlyEqual(Settings.CameraISO, PublicViewIso) &&
        Settings.bOverride_DepthOfFieldFstop &&
        FMath::IsNearlyEqual(Settings.DepthOfFieldFstop, PublicViewAperture) &&
        Settings.bOverride_DepthOfFieldScale &&
        FMath::IsNearlyZero(Settings.DepthOfFieldScale) &&
        Settings.bOverride_WhiteTemp &&
        FMath::IsNearlyEqual(Settings.WhiteTemp, PublicViewWhiteBalanceKelvin) &&
        Settings.bOverride_WhiteTint &&
        FMath::IsNearlyZero(Settings.WhiteTint) &&
        Settings.bOverride_LocalExposureHighlightContrastScale &&
        FMath::IsNearlyEqual(Settings.LocalExposureHighlightContrastScale, 1.0f) &&
        Settings.bOverride_LocalExposureShadowContrastScale &&
        FMath::IsNearlyEqual(Settings.LocalExposureShadowContrastScale, 1.0f) &&
        Settings.bOverride_LocalExposureDetailStrength &&
        FMath::IsNearlyEqual(Settings.LocalExposureDetailStrength, 1.0f) &&
        Settings.bOverride_LocalExposureMiddleGreyBias &&
        FMath::IsNearlyZero(Settings.LocalExposureMiddleGreyBias) &&
        Settings.bOverride_LocalExposureHighlightContrastCurve &&
        Settings.LocalExposureHighlightContrastCurve == nullptr &&
        Settings.bOverride_LocalExposureShadowContrastCurve &&
        Settings.LocalExposureShadowContrastCurve == nullptr &&
        Settings.bOverride_BloomIntensity && Settings.BloomIntensity == 0.0f &&
        Settings.bOverride_VignetteIntensity && Settings.VignetteIntensity == 0.0f &&
        Settings.bOverride_MotionBlurAmount && Settings.MotionBlurAmount == 0.0f &&
        Settings.bOverride_SceneFringeIntensity && Settings.SceneFringeIntensity == 0.0f;
    if (!bVerified)
    {
        OutReason = TEXT("Tagged public-view camera does not have the fixed neutral exposure/lens profile.");
        return false;
    }
    OutReason = TEXT("Fixed primary public-view camera profile is active.");
    return true;
}

bool ATRIADIstanaPublicViewRuntimePolicyActor::RequestAirSimQuiescenceForTeardown(
    int32& OutSimModeCount)
{
    OutSimModeCount = 0;
    UWorld* World = GetWorld();
    if (!World || World->WorldType != EWorldType::PIE)
    {
        return false;
    }

    bool bAllPaused = true;
    for (TActorIterator<ASimModeBase> It(World); It; ++It)
    {
        ASimModeBase* SimMode = *It;
        if (!IsValid(SimMode) || SimMode->GetWorld() != World ||
            !SimMode->HasActorBegunPlay())
        {
            continue;
        }

        SimMode->pause(true);
        ++OutSimModeCount;
        bAllPaused = bAllPaused && SimMode->isPaused();
    }

    // No SimMode means there is no AirSim physics/sensor worker to drain.
    return bAllPaused;
}

void ATRIADIstanaPublicViewRuntimePolicyActor::BeginPlay()
{
    Super::BeginPlay();
    EnforcementAttempts = 0;
    bFogSuppressionVerifiedAtRuntime = false;
    bPrimaryCameraVerifiedAtRuntime = false;
    bGameModeOverrideVerifiedAtRuntime = false;
    bRuntimePolicySettledAtRuntime = false;
    ClaimLabel = ExpectedClaimLabel();
    DisplayColorPolicy =
        TEXT("NEUTRAL_CAMERA_LOCAL_EXPOSURE_DISABLED_EXTERNAL_SRGB_REC709_DISPLAY_NOT_OCIO_VALIDATED");
    bExternalOcioDisplayValidated = false;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("TRIAD Istana public-view runtime: epoch=%s claim=%s"),
        *ReferenceEpoch,
        *ClaimLabel);

    EnforceRuntimePolicy();
}

void ATRIADIstanaPublicViewRuntimePolicyActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(EnforcementTimerHandle);
    Super::EndPlay(EndPlayReason);
}

void ATRIADIstanaPublicViewRuntimePolicyActor::ApplyClearWeatherAndFog()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    if (bSuppressAirSimVisualWeather)
    {
        UWeatherLib::setWeatherEnabled(World, false);
        UWeatherLib::setWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_RAIN,
            0.0f);
        UWeatherLib::setWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADWETNESS,
            0.0f);
        UWeatherLib::setWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_FOG,
            0.0f);
        UWeatherLib::setWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_DUST,
            0.0f);
        UWeatherLib::setWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_SNOW,
            0.0f);
        UWeatherLib::setWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADSNOW,
            0.0f);
        UWeatherLib::setWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_MAPLELEAF,
            0.0f);
        UWeatherLib::setWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADLEAF,
            0.0f);
    }
    if (!bSuppressExponentialHeightFog)
    {
        return;
    }
    for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
    {
        AExponentialHeightFog* FogActor = *It;
        UExponentialHeightFogComponent* FogComponent = FogActor
            ? FogActor->GetComponent()
            : nullptr;
        if (!IsValid(FogActor) || FogActor->GetWorld() != World || !FogComponent)
        {
            continue;
        }
        FogComponent->SetFogDensity(0.0f);
        FogComponent->SetSecondFogDensity(0.0f);
        FogComponent->SetFogMaxOpacity(0.0f);
        FogComponent->SetVolumetricFog(false);
    }
}

void ATRIADIstanaPublicViewRuntimePolicyActor::ApplyFixedCameraProfile(
    ACameraActor* Camera)
{
    UCameraComponent* CameraComponent = Camera
        ? Camera->GetCameraComponent()
        : nullptr;
    if (!CameraComponent || Camera->GetWorld() != GetWorld())
    {
        return;
    }

    CameraComponent->SetFieldOfView(PublicViewFieldOfViewDegrees);
    CameraComponent->SetAspectRatio(PublicViewAspectRatio);
    CameraComponent->SetConstraintAspectRatio(true);
    CameraComponent->SetPostProcessBlendWeight(1.0f);
    FPostProcessSettings& Settings = CameraComponent->PostProcessSettings;
    Settings.bOverride_AutoExposureMethod = true;
    Settings.AutoExposureMethod = AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Settings.AutoExposureApplyPhysicalCameraExposure = true;
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = 0.0f;
    Settings.bOverride_CameraShutterSpeed = true;
    Settings.CameraShutterSpeed = PublicViewShutterSpeed;
    Settings.bOverride_CameraISO = true;
    Settings.CameraISO = PublicViewIso;
    Settings.bOverride_DepthOfFieldFstop = true;
    Settings.DepthOfFieldFstop = PublicViewAperture;
    Settings.bOverride_DepthOfFieldScale = true;
    Settings.DepthOfFieldScale = 0.0f;
    Settings.bOverride_WhiteTemp = true;
    Settings.WhiteTemp = PublicViewWhiteBalanceKelvin;
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

void ATRIADIstanaPublicViewRuntimePolicyActor::EnforceRuntimePolicy()
{
    UWorld* World = GetWorld();
    if (!World || EnforcementAttempts >= MaximumEnforcementAttempts)
    {
        return;
    }

    AGameModeBase* ActiveGameMode = World->GetAuthGameMode();
    bGameModeOverrideVerifiedAtRuntime =
        !bRequireIstanaAirSimGameMode ||
        (ActiveGameMode &&
         ActiveGameMode->GetClass()->GetPathName() == RequiredGameModeClassPath);

    ApplyClearWeatherAndFog();
    bFogSuppressionVerifiedAtRuntime =
        IsFogSuppressionActive(FogComponentCountAtRuntime);

    ACameraActor* PrimaryCamera = nullptr;
    int32 MatchingCameraCount = 0;
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == World &&
            It->ActorHasTag(RequiredPrimaryCameraTag))
        {
            PrimaryCamera = *It;
            ++MatchingCameraCount;
        }
    }
    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(World, 0);
    if (bEnforceFixedPrimaryCamera && MatchingCameraCount == 1 &&
        PrimaryCamera && PlayerController &&
        PlayerController->GetWorld() == World && PlayerController->IsLocalController())
    {
        ApplyFixedCameraProfile(PrimaryCamera);
        PlayerController->SetViewTargetWithBlend(PrimaryCamera, 0.0f);
        FString CameraReason;
        bPrimaryCameraVerifiedAtRuntime =
            PlayerController->GetViewTarget() == PrimaryCamera &&
            IsFixedPrimaryCameraProfileActive(CameraReason);
    }
    else
    {
        bPrimaryCameraVerifiedAtRuntime = !bEnforceFixedPrimaryCamera;
    }

    ++EnforcementAttempts;
    const bool bFinalAttempt =
        EnforcementAttempts >= MaximumEnforcementAttempts;
    if (!bFinalAttempt)
    {
        // Always run the complete bounded settle window. AirSim can finish
        // weather setup or PlayerController possession after an initially
        // successful BeginPlay readback.
        GetWorldTimerManager().SetTimer(
            EnforcementTimerHandle,
            this,
            &ATRIADIstanaPublicViewRuntimePolicyActor::EnforceRuntimePolicy,
            FMath::Clamp(EnforcementRetrySeconds, 0.1f, 5.0f),
            false);
        return;
    }

    bRuntimePolicySettledAtRuntime =
        bGameModeOverrideVerifiedAtRuntime &&
        bFogSuppressionVerifiedAtRuntime &&
        bPrimaryCameraVerifiedAtRuntime;
    if (!bRuntimePolicySettledAtRuntime)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("TRIAD Istana public-view policy did not settle after %d attempts: GameMode=%s weather/fog=%s camera=%s (actual GameMode '%s')."),
            EnforcementAttempts,
            bGameModeOverrideVerifiedAtRuntime ? TEXT("verified") : TEXT("mismatch"),
            bFogSuppressionVerifiedAtRuntime ? TEXT("verified") : TEXT("mismatch"),
            bPrimaryCameraVerifiedAtRuntime ? TEXT("verified") : TEXT("mismatch"),
            ActiveGameMode
                ? *ActiveGameMode->GetClass()->GetPathName()
                : TEXT("<none>"));
    }
}
