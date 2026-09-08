#include "TRIADIstanaExploreV5DDynamicRangeRealismActor.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/PackageName.h"
#include "TRIADIstanaExploreV5GameMode.h"
#include "TRIADIstanaExploreV5Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(
    LogTRIADIstanaExploreV5DDynamicRangeRealism,
    Log,
    All);

namespace
{
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString DynamicRangeClaimLabel(
    TEXT("ISTANA_EXPLORE_V5D_APPEARANCE_ONLY_DYNAMIC_RANGE_LOOKDEV_NOT_SURVEY_NOT_DISPLAY_CALIBRATION_NOT_CURRENT_WEATHER_NOT_MEASURED_CAMERA_RESPONSE_NOT_COLLISION_NAVIGATION_SENSOR_OR_RF_TRUTH"));
const FName DynamicRangeActorTag(
    TEXT("TRIADIstanaExploreV5DDynamicRangeRealism"));
const FName SourceLightingTag(TEXT("TRIADIstanaPublicViewLighting_v1"));
const FName SourceSunName(TEXT("TRIAD_IPV_DirectionalSun"));
const FName SourceSkyLightName(TEXT("TRIAD_IPV_SkyLight"));
const FName SourceSkyAtmosphereName(TEXT("TRIAD_IPV_SkyAtmosphere"));

constexpr float RequiredPostProcessPriority = 6400.0f;
constexpr float RequiredPostProcessBlendWeight = 1.0f;
constexpr float RequiredPostProcessBlendRadius = 0.0f;
constexpr float HighlightContrastScale = 0.66f;
constexpr float ShadowContrastScale = 0.58f;
constexpr float DetailStrength = 1.01f;
constexpr float BlurredLuminanceBlend = 0.55f;
constexpr float BlurredLuminanceKernelSizePercent = 50.0f;
constexpr float MiddleGreyBias = 0.0f;
constexpr float GlobalSaturation = 0.970f;
constexpr float GlobalContrast = 0.990f;
constexpr float SourceSunIntensityLux = 80000.0f;
constexpr float SourceSkyLightIntensity = 1.0f;
constexpr int32 SourceAtmosphereSunLightIndex = 0;
constexpr double SourceValueTolerance = 0.0001;
const FRotator SourceSunRotation(-32.0, -145.0, 0.0);

template <typename TActorType>
TActorType* FindExactlyOne(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    TActorType* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<TActorType> It(World); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == World)
        {
            Result = *It;
            ++OutCount;
        }
    }
    return OutCount == 1 ? Result : nullptr;
}

bool IsExactTargetWorld(const UWorld* World)
{
    return World && World->GetOutermost() &&
        UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) ==
            TargetMapPackage;
}

bool IsTrustedUntitledBuilderWorld(
    const UWorld* World,
    bool bTrustedUntitledHybridBuilder)
{
    return World && World->GetOutermost() &&
        bTrustedUntitledHybridBuilder &&
        FPackageName::IsTempPackage(World->GetOutermost()->GetName());
}

FVector4 NeutralVector(float Value)
{
    return FVector4(Value, Value, Value, 1.0f);
}

bool NearlyEqualVector4(
    const FVector4& A,
    const FVector4& B,
    float Tolerance = KINDA_SMALL_NUMBER)
{
    return FMath::IsNearlyEqual(A.X, B.X, Tolerance) &&
        FMath::IsNearlyEqual(A.Y, B.Y, Tolerance) &&
        FMath::IsNearlyEqual(A.Z, B.Z, Tolerance) &&
        FMath::IsNearlyEqual(A.W, B.W, Tolerance);
}

bool ResolveExactSourceLighting(
    UWorld* World,
    ADirectionalLight*& OutSun,
    ASkyLight*& OutSkyLight,
    ASkyAtmosphere*& OutSkyAtmosphere,
    FString& OutError)
{
    OutSun = nullptr;
    OutSkyLight = nullptr;
    OutSkyAtmosphere = nullptr;
    int32 SunCount = 0;
    int32 SkyLightCount = 0;
    int32 SkyAtmosphereCount = 0;
    OutSun = FindExactlyOne<ADirectionalLight>(World, SunCount);
    OutSkyLight = FindExactlyOne<ASkyLight>(World, SkyLightCount);
    OutSkyAtmosphere = FindExactlyOne<ASkyAtmosphere>(
        World,
        SkyAtmosphereCount);
    UDirectionalLightComponent* SunComponent = OutSun
        ? Cast<UDirectionalLightComponent>(OutSun->GetLightComponent())
        : nullptr;
    USkyLightComponent* SkyLightComponent = OutSkyLight
        ? OutSkyLight->GetLightComponent()
        : nullptr;
    if (SunCount != 1 || SkyLightCount != 1 || SkyAtmosphereCount != 1 ||
        !OutSun || !OutSkyLight || !OutSkyAtmosphere ||
        !SunComponent || !SkyLightComponent ||
        OutSun->GetFName() != SourceSunName ||
        OutSkyLight->GetFName() != SourceSkyLightName ||
        OutSkyAtmosphere->GetFName() != SourceSkyAtmosphereName ||
        !OutSun->Tags.Contains(SourceLightingTag) ||
        !OutSkyLight->Tags.Contains(SourceLightingTag) ||
        !OutSkyAtmosphere->Tags.Contains(SourceLightingTag) ||
        SunComponent->Mobility != EComponentMobility::Movable ||
        !FMath::IsNearlyEqual(
            SunComponent->Intensity,
            SourceSunIntensityLux,
            0.1f) ||
        !OutSun->GetActorRotation().Equals(SourceSunRotation, 0.01) ||
        !SunComponent->IsUsedAsAtmosphereSunLight() ||
        SunComponent->GetAtmosphereSunLightIndex() !=
            SourceAtmosphereSunLightIndex ||
        SkyLightComponent->Mobility != EComponentMobility::Movable ||
        !FMath::IsNearlyEqual(
            SkyLightComponent->Intensity,
            SourceSkyLightIntensity,
            0.0001f) ||
        !SkyLightComponent->bRealTimeCapture)
    {
        OutError = FString::Printf(
            TEXT("V5D dynamic range requires the inherited exact clear-day source roster and values; sun/sky/atmosphere=%d/%d/%d, tagged names and 80000-lux/realtime settings must remain unchanged."),
            SunCount,
            SkyLightCount,
            SkyAtmosphereCount);
        return false;
    }
    OutError.Reset();
    return true;
}

FTRIADIstanaExploreV5DSourceLightingSnapshot MakeLightingSnapshot(
    const ADirectionalLight* Sun,
    const ASkyLight* SkyLight,
    const ASkyAtmosphere* SkyAtmosphere)
{
    FTRIADIstanaExploreV5DSourceLightingSnapshot Snapshot;
    const UDirectionalLightComponent* SunComponent = Sun
        ? Cast<UDirectionalLightComponent>(Sun->GetLightComponent())
        : nullptr;
    const USkyLightComponent* SkyLightComponent = SkyLight
        ? SkyLight->GetLightComponent()
        : nullptr;
    if (Sun && SkyLight && SkyAtmosphere && SunComponent &&
        SkyLightComponent)
    {
        Snapshot.SunActorTransform = Sun->GetActorTransform();
        Snapshot.SkyLightActorTransform = SkyLight->GetActorTransform();
        Snapshot.SkyAtmosphereActorTransform =
            SkyAtmosphere->GetActorTransform();
        Snapshot.SunIntensityLux = SunComponent->Intensity;
        Snapshot.SkyLightIntensity = SkyLightComponent->Intensity;
        Snapshot.SunMobility = static_cast<uint8>(SunComponent->Mobility);
        Snapshot.SkyLightMobility =
            static_cast<uint8>(SkyLightComponent->Mobility);
        Snapshot.bSunIsAtmosphereLight =
            SunComponent->IsUsedAsAtmosphereSunLight();
        Snapshot.AtmosphereSunLightIndex =
            SunComponent->GetAtmosphereSunLightIndex();
        Snapshot.bSkyLightRealTimeCapture =
            SkyLightComponent->bRealTimeCapture;
    }
    return Snapshot;
}
} // namespace

bool FTRIADIstanaExploreV5DSourceLightingSnapshot::Equals(
    const FTRIADIstanaExploreV5DSourceLightingSnapshot& Other,
    double Tolerance) const
{
    return SunActorTransform.Equals(Other.SunActorTransform, Tolerance) &&
        SkyLightActorTransform.Equals(
            Other.SkyLightActorTransform,
            Tolerance) &&
        SkyAtmosphereActorTransform.Equals(
            Other.SkyAtmosphereActorTransform,
            Tolerance) &&
        FMath::IsNearlyEqual(
            SunIntensityLux,
            Other.SunIntensityLux,
            static_cast<float>(Tolerance)) &&
        FMath::IsNearlyEqual(
            SkyLightIntensity,
            Other.SkyLightIntensity,
            static_cast<float>(Tolerance)) &&
        SunMobility == Other.SunMobility &&
        SkyLightMobility == Other.SkyLightMobility &&
        bSunIsAtmosphereLight == Other.bSunIsAtmosphereLight &&
        AtmosphereSunLightIndex == Other.AtmosphereSunLightIndex &&
        bSkyLightRealTimeCapture == Other.bSkyLightRealTimeCapture;
}

UTRIADIstanaExploreV5DDynamicRangeCameraModifier::
    UTRIADIstanaExploreV5DDynamicRangeCameraModifier(
        const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Lowest modifier priority executes last. The cache entry is explicitly an
    // override so the component settings follow the neutral physical camera.
    Priority = 255;
    bExclusive = false;
    AlphaInTime = 0.0f;
    AlphaOutTime = 0.0f;
}

void UTRIADIstanaExploreV5DDynamicRangeCameraModifier::
    InitializeForDynamicRangeActor(
        ATRIADIstanaExploreV5DDynamicRangeRealismActor* InActor)
{
    DynamicRangeActor = InActor;
}

bool UTRIADIstanaExploreV5DDynamicRangeCameraModifier::ModifyCamera(
    float DeltaTime,
    FMinimalViewInfo& InOutPOV)
{
    (void)DeltaTime;
    (void)InOutPOV;
    if (DynamicRangeActor && CameraOwner)
    {
        DynamicRangeActor->AppendFinalCameraOverride(CameraOwner);
    }
    return false;
}

ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ATRIADIstanaExploreV5DDynamicRangeRealismActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickInterval = 0.0f;
    ClaimLabel = ExpectedClaimLabel();
    Tags.AddUnique(ExpectedActorTag());

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("V5DDynamicRangeRealismRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    DynamicRangePostProcess =
        CreateDefaultSubobject<UPostProcessComponent>(
            TEXT("V5DDynamicRangePostProcess"));
    DynamicRangePostProcess->SetupAttachment(SceneRoot);
    DynamicRangePostProcess->SetRelativeTransform(FTransform::Identity);
    DynamicRangePostProcess->SetMobility(EComponentMobility::Static);
    DynamicRangePostProcess->bUnbound = true;
    DynamicRangePostProcess->bEnabled = true;
    DynamicRangePostProcess->Priority = RequiredPostProcessPriority;
    DynamicRangePostProcess->BlendWeight =
        RequiredPostProcessBlendWeight;
    DynamicRangePostProcess->BlendRadius = RequiredPostProcessBlendRadius;
    BuildExpectedPostProcessSettings(DynamicRangePostProcess->Settings);
}

const FString&
ATRIADIstanaExploreV5DDynamicRangeRealismActor::ExpectedClaimLabel()
{
    return DynamicRangeClaimLabel;
}

const FString&
ATRIADIstanaExploreV5DDynamicRangeRealismActor::ExpectedTargetMapPackage()
{
    return TargetMapPackage;
}

const FName&
ATRIADIstanaExploreV5DDynamicRangeRealismActor::ExpectedActorTag()
{
    return DynamicRangeActorTag;
}

float ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ExpectedHighlightContrastScale()
{
    return HighlightContrastScale;
}

float ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ExpectedShadowContrastScale()
{
    return ShadowContrastScale;
}

float ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ExpectedDetailStrength()
{
    return DetailStrength;
}

float ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ExpectedBlurredLuminanceBlend()
{
    return BlurredLuminanceBlend;
}

float ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ExpectedBlurredLuminanceKernelSizePercent()
{
    return BlurredLuminanceKernelSizePercent;
}

float ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ExpectedGlobalSaturation()
{
    return GlobalSaturation;
}

float ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ExpectedGlobalContrast()
{
    return GlobalContrast;
}

void ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    BuildExpectedPostProcessSettings(FPostProcessSettings& OutSettings)
{
    OutSettings = FPostProcessSettings();
    OutSettings.bOverride_LocalExposureMethod = true;
    OutSettings.LocalExposureMethod = ELocalExposureMethod::Bilateral;
    OutSettings.bOverride_LocalExposureHighlightContrastScale = true;
    OutSettings.LocalExposureHighlightContrastScale =
        HighlightContrastScale;
    OutSettings.bOverride_LocalExposureShadowContrastScale = true;
    OutSettings.LocalExposureShadowContrastScale = ShadowContrastScale;
    OutSettings.bOverride_LocalExposureHighlightContrastCurve = true;
    OutSettings.LocalExposureHighlightContrastCurve = nullptr;
    OutSettings.bOverride_LocalExposureShadowContrastCurve = true;
    OutSettings.LocalExposureShadowContrastCurve = nullptr;
    OutSettings.bOverride_LocalExposureDetailStrength = true;
    OutSettings.LocalExposureDetailStrength = DetailStrength;
    OutSettings.bOverride_LocalExposureBlurredLuminanceBlend = true;
    OutSettings.LocalExposureBlurredLuminanceBlend =
        BlurredLuminanceBlend;
    OutSettings.bOverride_LocalExposureBlurredLuminanceKernelSizePercent =
        true;
    OutSettings.LocalExposureBlurredLuminanceKernelSizePercent =
        BlurredLuminanceKernelSizePercent;
    OutSettings.bOverride_LocalExposureMiddleGreyBias = true;
    OutSettings.LocalExposureMiddleGreyBias = MiddleGreyBias;

    OutSettings.bOverride_ColorSaturation = true;
    OutSettings.ColorSaturation = NeutralVector(GlobalSaturation);
    OutSettings.bOverride_ColorContrast = true;
    OutSettings.ColorContrast = NeutralVector(GlobalContrast);
}

bool ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ValidateExpectedPostProcessSettings(
        const FPostProcessSettings& Settings,
        FString& OutError)
{
    if (!Settings.bOverride_LocalExposureMethod ||
        Settings.LocalExposureMethod != ELocalExposureMethod::Bilateral ||
        !Settings.bOverride_LocalExposureHighlightContrastScale ||
        !FMath::IsNearlyEqual(
            Settings.LocalExposureHighlightContrastScale,
            HighlightContrastScale) ||
        !Settings.bOverride_LocalExposureShadowContrastScale ||
        !FMath::IsNearlyEqual(
            Settings.LocalExposureShadowContrastScale,
            ShadowContrastScale) ||
        !Settings.bOverride_LocalExposureHighlightContrastCurve ||
        Settings.LocalExposureHighlightContrastCurve != nullptr ||
        !Settings.bOverride_LocalExposureShadowContrastCurve ||
        Settings.LocalExposureShadowContrastCurve != nullptr ||
        !Settings.bOverride_LocalExposureDetailStrength ||
        !FMath::IsNearlyEqual(
            Settings.LocalExposureDetailStrength,
            DetailStrength) ||
        !Settings.bOverride_LocalExposureBlurredLuminanceBlend ||
        !FMath::IsNearlyEqual(
            Settings.LocalExposureBlurredLuminanceBlend,
            BlurredLuminanceBlend) ||
        !Settings.bOverride_LocalExposureBlurredLuminanceKernelSizePercent ||
        !FMath::IsNearlyEqual(
            Settings.LocalExposureBlurredLuminanceKernelSizePercent,
            BlurredLuminanceKernelSizePercent) ||
        !Settings.bOverride_LocalExposureMiddleGreyBias ||
        !FMath::IsNearlyEqual(
            Settings.LocalExposureMiddleGreyBias,
            MiddleGreyBias) ||
        !Settings.bOverride_ColorSaturation ||
        !NearlyEqualVector4(
            Settings.ColorSaturation,
            NeutralVector(GlobalSaturation)) ||
        !Settings.bOverride_ColorContrast ||
        !NearlyEqualVector4(
            Settings.ColorContrast,
            NeutralVector(GlobalContrast)))
    {
        OutError = TEXT("V5D dynamic-range local-exposure or restrained color-grade values changed.");
        return false;
    }

    // These flags are deliberately absent. The pawn remains the sole owner of
    // manual exposure, physical camera, white balance and depth-of-field state.
    if (Settings.bOverride_AutoExposureMethod ||
        Settings.bOverride_AutoExposureApplyPhysicalCameraExposure ||
        Settings.bOverride_AutoExposureBias ||
        Settings.bOverride_CameraShutterSpeed ||
        Settings.bOverride_CameraISO ||
        Settings.bOverride_DepthOfFieldFstop ||
        Settings.bOverride_DepthOfFieldScale ||
        Settings.bOverride_TemperatureType ||
        Settings.bOverride_WhiteTemp ||
        Settings.bOverride_WhiteTint ||
        Settings.bOverride_ColorGradingLUT ||
        Settings.ColorGradingLUT != nullptr ||
        Settings.WeightedBlendables.Array.Num() != 0)
    {
        OutError = TEXT("V5D dynamic range attempted to own exposure, physical camera, white balance, LUT, blendable or depth-of-field state.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    CaptureExactSourceLightingSnapshot(
        UWorld* World,
        FTRIADIstanaExploreV5DSourceLightingSnapshot& OutSnapshot,
        FString& OutError)
{
    ADirectionalLight* Sun = nullptr;
    ASkyLight* SkyLight = nullptr;
    ASkyAtmosphere* SkyAtmosphere = nullptr;
    if (!ResolveExactSourceLighting(
            World,
            Sun,
            SkyLight,
            SkyAtmosphere,
            OutError))
    {
        OutSnapshot = FTRIADIstanaExploreV5DSourceLightingSnapshot();
        return false;
    }
    OutSnapshot = MakeLightingSnapshot(Sun, SkyLight, SkyAtmosphere);
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ConfigureDynamicRangeRealism(
        bool bTrustedUntitledHybridBuilder,
        FString& OutError)
{
    UWorld* World = GetWorld();
    const bool bExactTarget = IsExactTargetWorld(World);
    const bool bTrustedTemp = IsTrustedUntitledBuilderWorld(
        World,
        bTrustedUntitledHybridBuilder);
    if (bConfigured || (!bExactTarget && !bTrustedTemp) ||
        !World || !World->GetWorldSettings() ||
        World->GetWorldSettings()->DefaultGameMode !=
            ATRIADIstanaExploreV5GameMode::StaticClass())
    {
        OutError = TEXT("V5D dynamic range configures once, only in the exact V5D map or an explicitly trusted untitled hybrid-builder world with the inherited V5 game mode.");
        return false;
    }

    ADirectionalLight* Sun = nullptr;
    ASkyLight* SkyLight = nullptr;
    ASkyAtmosphere* SkyAtmosphere = nullptr;
    FTRIADIstanaExploreV5DSourceLightingSnapshot Snapshot;
    FString SettingsError;
    if (!ResolveExactSourceLighting(
            World,
            Sun,
            SkyLight,
            SkyAtmosphere,
            OutError) ||
        !CaptureExactSourceLightingSnapshot(World, Snapshot, OutError) ||
        !DynamicRangePostProcess ||
        !ValidateExpectedPostProcessSettings(
            DynamicRangePostProcess->Settings,
            SettingsError))
    {
        if (OutError.IsEmpty())
        {
            OutError = SettingsError.IsEmpty()
                ? TEXT("The one V5D post-process component is absent.")
                : SettingsError;
        }
        return false;
    }

    SourceSun = Sun;
    SourceSkyLight = SkyLight;
    SourceSkyAtmosphere = SkyAtmosphere;
    SavedSourceLightingSnapshot = Snapshot;
    bConfiguredInTrustedUntitledHybridBuilder = bTrustedTemp;
    bConfigured = true;
    Tags.AddUnique(ExpectedActorTag());

    FString Report;
    if (!ValidateDynamicRangeRealism(false, Report))
    {
        bConfigured = false;
        SourceSun = nullptr;
        SourceSkyLight = nullptr;
        SourceSkyAtmosphere = nullptr;
        OutError = TEXT("V5D dynamic range failed closed configuration: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ValidateSourceLightingStillMatches(FString& OutError) const
{
    FTRIADIstanaExploreV5DSourceLightingSnapshot Current;
    if (!CaptureExactSourceLightingSnapshot(GetWorld(), Current, OutError) ||
        !SourceSun || !SourceSkyLight || !SourceSkyAtmosphere ||
        SourceSun->GetWorld() != GetWorld() ||
        SourceSkyLight->GetWorld() != GetWorld() ||
        SourceSkyAtmosphere->GetWorld() != GetWorld() ||
        !Current.Equals(SavedSourceLightingSnapshot, SourceValueTolerance))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The inherited clear-day lighting no longer equals the pre-pass snapshot.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ResolveExpectedRuntimePawn(
        ATRIADIstanaExploreV5Pawn*& OutPawn,
        APlayerCameraManager*& OutCameraManager,
        FString& OutError) const
{
    OutPawn = nullptr;
    OutCameraManager = nullptr;
    UWorld* World = GetWorld();
    APlayerController* PlayerController = World
        ? World->GetFirstPlayerController()
        : nullptr;
    OutPawn = PlayerController
        ? Cast<ATRIADIstanaExploreV5Pawn>(PlayerController->GetPawn())
        : nullptr;
    OutCameraManager = PlayerController
        ? PlayerController->PlayerCameraManager
        : nullptr;
    int32 V5PawnCount = 0;
    ATRIADIstanaExploreV5Pawn* UniquePawn =
        FindExactlyOne<ATRIADIstanaExploreV5Pawn>(World, V5PawnCount);
    if (!PlayerController || !OutCameraManager || !OutPawn ||
        V5PawnCount != 1 || UniquePawn != OutPawn ||
        !OutPawn->HasExpectedExploreV5CameraProfile())
    {
        OutError = FString::Printf(
            TEXT("V5D dynamic range requires exact Player0 possession of the one unchanged V5 physical-camera pawn; pawnCount=%d."),
            V5PawnCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    ValidateDynamicRangeRealism(
        bool bRequireRuntime,
        FString& OutReport) const
{
    UWorld* World = GetWorld();
    const bool bAdmittedEditorWorld = IsExactTargetWorld(World) ||
        (bConfiguredInTrustedUntitledHybridBuilder && World &&
         World->GetOutermost() &&
         FPackageName::IsTempPackage(World->GetOutermost()->GetName()));
    int32 ActorCount = 0;
    ATRIADIstanaExploreV5DDynamicRangeRealismActor* UniqueActor =
        FindExactlyOne<ATRIADIstanaExploreV5DDynamicRangeRealismActor>(
            World,
            ActorCount);
    TArray<UPostProcessComponent*> OwnedPostProcessComponents;
    GetComponents<UPostProcessComponent>(OwnedPostProcessComponents);
    FString SettingsError;
    FString LightingError;
    if (!bConfigured || !bAdmittedEditorWorld || !World ||
        !World->PersistentLevel || ActorCount != 1 || UniqueActor != this ||
        !Tags.Contains(ExpectedActorTag()) ||
        ClaimLabel != ExpectedClaimLabel() || !bAppearanceOnly ||
        !bExactV5DMapOnly || bSourceSunSkyOrAtmosphereModified ||
        bPawnPhysicalCameraProfileModified || bMaterialsOrEmissiveRetuned ||
        bCollisionNavigationSensorOrRfAuthority ||
        bSurveyDisplayCalibrationOrCurrentWeatherClaimed ||
        !GetActorTransform().Equals(FTransform::Identity, 0.0001) ||
        !SceneRoot || !DynamicRangePostProcess ||
        OwnedPostProcessComponents.Num() != 1 ||
        OwnedPostProcessComponents[0] != DynamicRangePostProcess ||
        DynamicRangePostProcess->GetAttachParent() != SceneRoot ||
        !DynamicRangePostProcess->GetRelativeTransform().Equals(
            FTransform::Identity,
            0.0001) ||
        !DynamicRangePostProcess->bUnbound ||
        !FMath::IsNearlyEqual(
            DynamicRangePostProcess->Priority,
            RequiredPostProcessPriority) ||
        !FMath::IsNearlyEqual(
            DynamicRangePostProcess->BlendWeight,
            RequiredPostProcessBlendWeight) ||
        !FMath::IsNearlyEqual(
            DynamicRangePostProcess->BlendRadius,
            RequiredPostProcessBlendRadius) ||
        !ValidateExpectedPostProcessSettings(
            DynamicRangePostProcess->Settings,
            SettingsError) ||
        !ValidateSourceLightingStillMatches(LightingError) ||
        !World->GetWorldSettings() ||
        World->GetWorldSettings()->DefaultGameMode !=
            ATRIADIstanaExploreV5GameMode::StaticClass())
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_DYNAMIC_RANGE_INVALID actor=%d postProcess=%d settings={%s} lighting={%s}"),
            ActorCount,
            OwnedPostProcessComponents.Num(),
            *SettingsError,
            *LightingError);
        return false;
    }

    if (!bRequireRuntime)
    {
        if (HasActorBegunPlay() || !DynamicRangePostProcess->bEnabled ||
            bRuntimeFinalCameraOverrideReady || RuntimeCameraModifier ||
            RuntimeV5Pawn)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_DYNAMIC_RANGE_INVALID: cold authored state contains disabled presentation or leaked transient runtime ownership.");
            return false;
        }
    }
    else
    {
        ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
        APlayerCameraManager* CameraManager = nullptr;
        FString CameraError;
        if (!IsExactTargetWorld(World) || !World->IsGameWorld() ||
            !HasActorBegunPlay() || !DynamicRangePostProcess->bEnabled ||
            !bRuntimeFinalCameraOverrideReady || !RuntimeCameraModifier ||
            RuntimeV5Pawn == nullptr ||
            !ResolveExpectedRuntimePawn(Pawn, CameraManager, CameraError) ||
            Pawn != RuntimeV5Pawn ||
            CameraManager->FindCameraModifierByClass(
                UTRIADIstanaExploreV5DDynamicRangeCameraModifier::
                    StaticClass()) != RuntimeCameraModifier ||
            RuntimeCameraModifier->DynamicRangeActor != this ||
            RuntimeCameraModifier->Priority != 255 ||
            RuntimeCameraModifier->IsDisabled() ||
            !CanSupplyFinalCameraOverride())
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_DYNAMIC_RANGE_INVALID_RUNTIME: ") +
                CameraError;
            return false;
        }
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_DYNAMIC_RANGE_VALID runtime=%s oneUnboundPostProcess=true priority=%.0f highlightContrast=%.2f shadowContrast=%.2f detailStrength=%.2f blurredLuminanceBlend=%.2f kernelPercent=%.0f middleGreyBias=0.00 saturation=%.3f contrast=%.3f taggedSunLux=80000 sourceSunSkyAtmosphereUntouched=true pawnPhysicalCameraProfileUntouched=true finalCameraOverrideUsesSameComponentSettings=%s materialsEmissiveUntouched=true collisionNavigationSensorRfAuthority=false survey=false displayCalibration=false currentWeather=false measuredCameraResponse=false"),
        bRequireRuntime ? TEXT("true") : TEXT("false"),
        RequiredPostProcessPriority,
        HighlightContrastScale,
        ShadowContrastScale,
        DetailStrength,
        BlurredLuminanceBlend,
        BlurredLuminanceKernelSizePercent,
        GlobalSaturation,
        GlobalContrast,
        bRequireRuntime ? TEXT("true") : TEXT("notRequired"));
    return true;
}

bool ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    TryActivateRuntimeFinalCameraOverride(FString& OutError)
{
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    APlayerCameraManager* CameraManager = nullptr;
    FString LightingError;
    FString SettingsError;
    if (!bConfigured || !IsExactTargetWorld(GetWorld()) ||
        !ValidateSourceLightingStillMatches(LightingError) ||
        !ValidateExpectedPostProcessSettings(
            DynamicRangePostProcess->Settings,
            SettingsError) ||
        !ResolveExpectedRuntimePawn(Pawn, CameraManager, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = LightingError.IsEmpty() ? SettingsError : LightingError;
        }
        return false;
    }
    if (CameraManager->FindCameraModifierByClass(
            UTRIADIstanaExploreV5DDynamicRangeCameraModifier::StaticClass()))
    {
        OutError = TEXT("A V5D dynamic-range camera modifier already exists without this actor's ownership.");
        return false;
    }

    UTRIADIstanaExploreV5DDynamicRangeCameraModifier* Modifier = Cast<
        UTRIADIstanaExploreV5DDynamicRangeCameraModifier>(
            CameraManager->AddNewCameraModifier(
                UTRIADIstanaExploreV5DDynamicRangeCameraModifier::
                    StaticClass()));
    if (!Modifier)
    {
        OutError = TEXT("Player0 camera manager refused the isolated V5D final-stage modifier.");
        return false;
    }
    RuntimeV5Pawn = Pawn;
    RuntimeCameraModifier = Modifier;
    Modifier->InitializeForDynamicRangeActor(this);
    DynamicRangePostProcess->bEnabled = true;
    bRuntimeFinalCameraOverrideReady = true;

    FString Report;
    if (!ValidateDynamicRangeRealism(true, Report))
    {
        DeactivateRuntimeFinalCameraOverride();
        OutError = TEXT("V5D dynamic-range runtime activation failed closed: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    DeactivateRuntimeFinalCameraOverride()
{
    if (RuntimeCameraModifier)
    {
        APlayerCameraManager* CameraManager =
            Cast<APlayerCameraManager>(RuntimeCameraModifier->GetOuter());
        RuntimeCameraModifier->DynamicRangeActor = nullptr;
        if (CameraManager)
        {
            CameraManager->RemoveCameraModifier(RuntimeCameraModifier);
        }
    }
    RuntimeCameraModifier = nullptr;
    RuntimeV5Pawn = nullptr;
    bRuntimeFinalCameraOverrideReady = false;
    if (DynamicRangePostProcess)
    {
        DynamicRangePostProcess->bEnabled = false;
    }
}

bool ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    CanSupplyFinalCameraOverride() const
{
    if (!bRuntimeFinalCameraOverrideReady || !DynamicRangePostProcess ||
        !DynamicRangePostProcess->bEnabled || !RuntimeV5Pawn ||
        !RuntimeV5Pawn->HasExpectedExploreV5CameraProfile() ||
        !SourceSun || !SourceSkyLight || !SourceSkyAtmosphere ||
        !IsExactTargetWorld(GetWorld()))
    {
        return false;
    }
    FString SettingsError;
    if (!ValidateExpectedPostProcessSettings(
            DynamicRangePostProcess->Settings,
            SettingsError))
    {
        return false;
    }
    const FTRIADIstanaExploreV5DSourceLightingSnapshot Current =
        MakeLightingSnapshot(SourceSun, SourceSkyLight, SourceSkyAtmosphere);
    return Current.Equals(
        SavedSourceLightingSnapshot,
        SourceValueTolerance);
}

bool ATRIADIstanaExploreV5DDynamicRangeRealismActor::
    AppendFinalCameraOverride(APlayerCameraManager* CameraManager) const
{
    if (!CameraManager || !RuntimeCameraModifier ||
        RuntimeCameraModifier->GetOuter() != CameraManager ||
        !CanSupplyFinalCameraOverride())
    {
        return false;
    }
    FPostProcessSettings FinalOverride =
        DynamicRangePostProcess->Settings;
    CameraManager->AddCachedPPBlend(
        FinalOverride,
        RequiredPostProcessBlendWeight,
        VTBlendOrder_Override);
    return true;
}

void ATRIADIstanaExploreV5DDynamicRangeRealismActor::BeginPlay()
{
    Super::BeginPlay();
    RuntimeActivationAttempts = 0;
    bRuntimeFinalCameraOverrideReady = false;
    RuntimeCameraModifier = nullptr;
    RuntimeV5Pawn = nullptr;
    if (DynamicRangePostProcess)
    {
        DynamicRangePostProcess->bEnabled = false;
    }
    SetActorTickInterval(0.0f);
    SetActorTickEnabled(true);
}

void ATRIADIstanaExploreV5DDynamicRangeRealismActor::Tick(
    float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bRuntimeFinalCameraOverrideReady)
    {
        ++RuntimeActivationAttempts;
        FString Error;
        if (TryActivateRuntimeFinalCameraOverride(Error))
        {
            SetActorTickInterval(0.5f);
            FString Report;
            ValidateDynamicRangeRealism(true, Report);
            UE_LOG(
                LogTRIADIstanaExploreV5DDynamicRangeRealism,
                Display,
                TEXT("%s"),
                *Report);
            return;
        }
        if (RuntimeActivationAttempts >= MaximumRuntimeActivationAttempts)
        {
            DeactivateRuntimeFinalCameraOverride();
            SetActorTickEnabled(false);
            UE_LOG(
                LogTRIADIstanaExploreV5DDynamicRangeRealism,
                Error,
                TEXT("V5D dynamic range failed closed after %d attempts: %s"),
                RuntimeActivationAttempts,
                *Error);
        }
        return;
    }

    FString Report;
    if (!ValidateDynamicRangeRealism(true, Report))
    {
        DeactivateRuntimeFinalCameraOverride();
        SetActorTickEnabled(false);
        UE_LOG(
            LogTRIADIstanaExploreV5DDynamicRangeRealism,
            Error,
            TEXT("V5D dynamic-range runtime drifted and was disabled: %s"),
            *Report);
    }
}

void ATRIADIstanaExploreV5DDynamicRangeRealismActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    DeactivateRuntimeFinalCameraOverride();
    Super::EndPlay(EndPlayReason);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DDynamicRangeSettingsTest,
    "TRIAD.Istana.ExploreV5D.DynamicRange.ExpectedSettings",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DDynamicRangeSettingsTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    FPostProcessSettings Settings;
    ATRIADIstanaExploreV5DDynamicRangeRealismActor::
        BuildExpectedPostProcessSettings(Settings);
    FString Error;
    TestTrue(
        TEXT("Exact V5D dynamic-range settings validate"),
        ATRIADIstanaExploreV5DDynamicRangeRealismActor::
            ValidateExpectedPostProcessSettings(Settings, Error));
    TestEqual(
        TEXT("Highlight recovery stays bounded"),
        Settings.LocalExposureHighlightContrastScale,
        HighlightContrastScale);
    TestEqual(
        TEXT("Shadow recovery stays bounded"),
        Settings.LocalExposureShadowContrastScale,
        ShadowContrastScale);
    TestEqual(
        TEXT("Detail strength stays restrained"),
        Settings.LocalExposureDetailStrength,
        DetailStrength);
    TestFalse(
        TEXT("Dynamic-range pass does not own auto exposure"),
        Settings.bOverride_AutoExposureMethod != 0);
    TestFalse(
        TEXT("Dynamic-range pass does not own shutter speed"),
        Settings.bOverride_CameraShutterSpeed != 0);
    TestFalse(
        TEXT("Dynamic-range pass does not own ISO"),
        Settings.bOverride_CameraISO != 0);
    TestFalse(
        TEXT("Dynamic-range pass does not own aperture"),
        Settings.bOverride_DepthOfFieldFstop != 0);
    Settings.LocalExposureHighlightContrastScale = 1.0f;
    TestFalse(
        TEXT("A drifted highlight setting fails closed"),
        ATRIADIstanaExploreV5DDynamicRangeRealismActor::
            ValidateExpectedPostProcessSettings(Settings, Error));

    FTRIADIstanaExploreV5DSourceLightingSnapshot A;
    A.SunIntensityLux = SourceSunIntensityLux;
    A.SkyLightIntensity = SourceSkyLightIntensity;
    A.SunMobility = static_cast<uint8>(EComponentMobility::Movable);
    A.SkyLightMobility = static_cast<uint8>(EComponentMobility::Movable);
    A.bSunIsAtmosphereLight = true;
    A.AtmosphereSunLightIndex = SourceAtmosphereSunLightIndex;
    A.bSkyLightRealTimeCapture = true;
    FTRIADIstanaExploreV5DSourceLightingSnapshot B = A;
    TestTrue(TEXT("Equal source-light snapshots compare equal"), A.Equals(B));
    B.SunIntensityLux = 79999.0f;
    TestFalse(
        TEXT("Changed source sun intensity fails the snapshot"),
        A.Equals(B));
    return true;
}
#endif
