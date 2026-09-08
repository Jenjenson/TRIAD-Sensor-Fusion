#include "TRIADIstanaExploreV5Pawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"

namespace
{
constexpr float V5IndirectLightingIntensity = 1.0f;
constexpr float V5ScreenSpaceReflectionIntensity = 100.0f;
constexpr float V5ScreenSpaceReflectionQuality = 100.0f;
constexpr float V5ScreenSpaceReflectionMaxRoughness = 0.8f;
constexpr float V5DisabledFilmGrainIntensity = 0.0f;
constexpr float V5DisabledFilmGrainTonalIntensity = 0.0f;
constexpr float V5DisabledFilmGrainShadowsMax = 0.0f;
constexpr float V5DisabledFilmGrainHighlightsMin = 0.0f;
constexpr float V5DisabledFilmGrainHighlightsMax = 1.0f;
constexpr float V5NeutralFilmGrainTexelSize = 1.0f;
}

ATRIADIstanaExploreV5Pawn::ATRIADIstanaExploreV5Pawn(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ApplyExploreV5ScreenSpaceProfile();
}

float ATRIADIstanaExploreV5Pawn::ExpectedScreenSpaceReflectionIntensity()
{
    return V5ScreenSpaceReflectionIntensity;
}

float ATRIADIstanaExploreV5Pawn::ExpectedScreenSpaceReflectionQuality()
{
    return V5ScreenSpaceReflectionQuality;
}

float ATRIADIstanaExploreV5Pawn::ExpectedScreenSpaceReflectionMaxRoughness()
{
    return V5ScreenSpaceReflectionMaxRoughness;
}

void ATRIADIstanaExploreV5Pawn::ApplyExploreV5ScreenSpaceProfile()
{
    UCameraComponent* Camera = GetExploreCameraComponent();
    if (!Camera)
    {
        return;
    }

    FPostProcessSettings& Settings = Camera->PostProcessSettings;
    Settings.bOverride_DynamicGlobalIlluminationMethod = true;
    Settings.DynamicGlobalIlluminationMethod =
        EDynamicGlobalIlluminationMethod::ScreenSpace;
    Settings.bOverride_IndirectLightingIntensity = true;
    Settings.IndirectLightingIntensity = V5IndirectLightingIntensity;
    Settings.bOverride_ReflectionMethod = true;
    Settings.ReflectionMethod = EReflectionMethod::ScreenSpace;
    Settings.bOverride_ScreenSpaceReflectionIntensity = true;
    Settings.ScreenSpaceReflectionIntensity =
        V5ScreenSpaceReflectionIntensity;
    Settings.bOverride_ScreenSpaceReflectionQuality = true;
    Settings.ScreenSpaceReflectionQuality = V5ScreenSpaceReflectionQuality;
    Settings.bOverride_ScreenSpaceReflectionMaxRoughness = true;
    Settings.ScreenSpaceReflectionMaxRoughness =
        V5ScreenSpaceReflectionMaxRoughness;

    // UE 5.5 has no ELocalExposureMethod::Disabled enumerator. Renderer-side
    // enablement is instead controlled by the two contrast scales, two curves
    // and detail strength. Pin the only stable method while overriding every
    // renderer enable gate to its neutral value; this is the explicit UE 5.5
    // equivalent of a disabled local-exposure pass.
    Settings.bOverride_LocalExposureMethod = true;
    Settings.LocalExposureMethod = ELocalExposureMethod::Bilateral;
    Settings.bOverride_LocalExposureHighlightContrastScale = true;
    Settings.LocalExposureHighlightContrastScale = 1.0f;
    Settings.bOverride_LocalExposureShadowContrastScale = true;
    Settings.LocalExposureShadowContrastScale = 1.0f;
    Settings.bOverride_LocalExposureHighlightContrastCurve = true;
    Settings.LocalExposureHighlightContrastCurve = nullptr;
    Settings.bOverride_LocalExposureShadowContrastCurve = true;
    Settings.LocalExposureShadowContrastCurve = nullptr;
    Settings.bOverride_LocalExposureDetailStrength = true;
    Settings.LocalExposureDetailStrength = 1.0f;
    Settings.bOverride_LocalExposureMiddleGreyBias = true;
    Settings.LocalExposureMiddleGreyBias = 0.0f;
    Settings.bOverride_LocalExposureBlurredLuminanceBlend = true;
    Settings.LocalExposureBlurredLuminanceBlend = 0.0f;

    Settings.bOverride_ColorGradingLUT = true;
    Settings.ColorGradingLUT = nullptr;

    Settings.bOverride_FilmGrainIntensity = true;
    Settings.FilmGrainIntensity = V5DisabledFilmGrainIntensity;
    Settings.bOverride_FilmGrainIntensityShadows = true;
    Settings.FilmGrainIntensityShadows = V5DisabledFilmGrainTonalIntensity;
    Settings.bOverride_FilmGrainIntensityMidtones = true;
    Settings.FilmGrainIntensityMidtones = V5DisabledFilmGrainTonalIntensity;
    Settings.bOverride_FilmGrainIntensityHighlights = true;
    Settings.FilmGrainIntensityHighlights = V5DisabledFilmGrainTonalIntensity;
    Settings.bOverride_FilmGrainShadowsMax = true;
    Settings.FilmGrainShadowsMax = V5DisabledFilmGrainShadowsMax;
    Settings.bOverride_FilmGrainHighlightsMin = true;
    Settings.FilmGrainHighlightsMin = V5DisabledFilmGrainHighlightsMin;
    Settings.bOverride_FilmGrainHighlightsMax = true;
    Settings.FilmGrainHighlightsMax = V5DisabledFilmGrainHighlightsMax;
    Settings.bOverride_FilmGrainTexelSize = true;
    Settings.FilmGrainTexelSize = V5NeutralFilmGrainTexelSize;
    Settings.bOverride_FilmGrainTexture = true;
    Settings.FilmGrainTexture = nullptr;

    // This UE5.5 override has no corresponding setting value and must not be
    // toggled by the V5 profile.
    Settings.bOverride_ScreenSpaceReflectionRoughnessScale = false;
}

bool ATRIADIstanaExploreV5Pawn::HasExpectedExploreV5CameraProfile() const
{
    const UCameraComponent* Camera = GetExploreCameraComponent();
    if (!HasExpectedExploreCameraProfile() || !Camera)
    {
        return false;
    }

    const FPostProcessSettings& Settings = Camera->PostProcessSettings;
    return Settings.bOverride_DynamicGlobalIlluminationMethod &&
        Settings.DynamicGlobalIlluminationMethod ==
            EDynamicGlobalIlluminationMethod::ScreenSpace &&
        Settings.bOverride_IndirectLightingIntensity &&
        FMath::IsNearlyEqual(
            Settings.IndirectLightingIntensity,
            V5IndirectLightingIntensity) &&
        Settings.bOverride_ReflectionMethod &&
        Settings.ReflectionMethod == EReflectionMethod::ScreenSpace &&
        Settings.bOverride_ScreenSpaceReflectionIntensity &&
        FMath::IsNearlyEqual(
            Settings.ScreenSpaceReflectionIntensity,
            V5ScreenSpaceReflectionIntensity) &&
        Settings.bOverride_ScreenSpaceReflectionQuality &&
        FMath::IsNearlyEqual(
            Settings.ScreenSpaceReflectionQuality,
            V5ScreenSpaceReflectionQuality) &&
        Settings.bOverride_ScreenSpaceReflectionMaxRoughness &&
        FMath::IsNearlyEqual(
            Settings.ScreenSpaceReflectionMaxRoughness,
            V5ScreenSpaceReflectionMaxRoughness) &&
        Settings.bOverride_LocalExposureMethod &&
        Settings.LocalExposureMethod == ELocalExposureMethod::Bilateral &&
        Settings.bOverride_LocalExposureHighlightContrastScale &&
        FMath::IsNearlyEqual(
            Settings.LocalExposureHighlightContrastScale, 1.0f) &&
        Settings.bOverride_LocalExposureShadowContrastScale &&
        FMath::IsNearlyEqual(
            Settings.LocalExposureShadowContrastScale, 1.0f) &&
        Settings.bOverride_LocalExposureHighlightContrastCurve &&
        Settings.LocalExposureHighlightContrastCurve == nullptr &&
        Settings.bOverride_LocalExposureShadowContrastCurve &&
        Settings.LocalExposureShadowContrastCurve == nullptr &&
        Settings.bOverride_LocalExposureDetailStrength &&
        FMath::IsNearlyEqual(Settings.LocalExposureDetailStrength, 1.0f) &&
        Settings.bOverride_LocalExposureMiddleGreyBias &&
        FMath::IsNearlyZero(Settings.LocalExposureMiddleGreyBias) &&
        Settings.bOverride_LocalExposureBlurredLuminanceBlend &&
        FMath::IsNearlyZero(Settings.LocalExposureBlurredLuminanceBlend) &&
        Settings.bOverride_ColorGradingLUT &&
        Settings.ColorGradingLUT == nullptr &&
        Settings.bOverride_FilmGrainIntensity &&
        FMath::IsNearlyEqual(
            Settings.FilmGrainIntensity,
            V5DisabledFilmGrainIntensity) &&
        Settings.bOverride_FilmGrainIntensityShadows &&
        FMath::IsNearlyEqual(
            Settings.FilmGrainIntensityShadows,
            V5DisabledFilmGrainTonalIntensity) &&
        Settings.bOverride_FilmGrainIntensityMidtones &&
        FMath::IsNearlyEqual(
            Settings.FilmGrainIntensityMidtones,
            V5DisabledFilmGrainTonalIntensity) &&
        Settings.bOverride_FilmGrainIntensityHighlights &&
        FMath::IsNearlyEqual(
            Settings.FilmGrainIntensityHighlights,
            V5DisabledFilmGrainTonalIntensity) &&
        Settings.bOverride_FilmGrainShadowsMax &&
        FMath::IsNearlyEqual(
            Settings.FilmGrainShadowsMax,
            V5DisabledFilmGrainShadowsMax) &&
        Settings.bOverride_FilmGrainHighlightsMin &&
        FMath::IsNearlyEqual(
            Settings.FilmGrainHighlightsMin,
            V5DisabledFilmGrainHighlightsMin) &&
        Settings.bOverride_FilmGrainHighlightsMax &&
        FMath::IsNearlyEqual(
            Settings.FilmGrainHighlightsMax,
            V5DisabledFilmGrainHighlightsMax) &&
        Settings.bOverride_FilmGrainTexelSize &&
        FMath::IsNearlyEqual(
            Settings.FilmGrainTexelSize,
            V5NeutralFilmGrainTexelSize) &&
        Settings.bOverride_FilmGrainTexture &&
        Settings.FilmGrainTexture == nullptr &&
        !Settings.bOverride_ScreenSpaceReflectionRoughnessScale;
}

void ATRIADIstanaExploreV5Pawn::BeginPlay()
{
    Super::BeginPlay();
    ApplyExploreV5ScreenSpaceProfile();
}
