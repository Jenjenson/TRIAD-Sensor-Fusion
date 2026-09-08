#include "TRIADIstanaRuntimePolicyActor.h"

// Keep Windows SDK aliases from rewriting Cesium's numeric_limits::max call
// and Unreal's UTexture::UpdateResource virtual while AirSim headers expand.
#ifdef max
#undef max
#endif
#ifdef UpdateResource
#undef UpdateResource
#endif

#include "Cesium3DTileset.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Scene.h"
#include "Engine/Texture2DArray.h"
#include "Engine/World.h"
#include "Engine/ExponentialHeightFog.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SimMode/SimModeBase.h"
#include "TimerManager.h"
#include "TRIADIstanaExteriorMeshActor.h"
#include "TRIADIstanaStudyAreaActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicles/ComputerVision/SimModeComputerVision.h"
#include "Weather/WeatherLib.h"

namespace
{
const FName HumanOnlyOverlayComponentTag(TEXT("TRIADHumanOnlyOverlay"));
constexpr float RuntimeCameraAutoExposureBias = -0.25f;
constexpr float RuntimeCameraLocalHighlightContrast = 0.85f;
constexpr float RuntimeCameraLocalShadowContrast = 0.90f;
constexpr float RuntimeCameraLocalDetailStrength = 1.15f;
constexpr double RuntimeCameraGlobalContrast = 1.08;
constexpr float RuntimeCameraSharpen = 0.35f;
}

ATRIADIstanaRuntimePolicyActor::ATRIADIstanaRuntimePolicyActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);

    static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection>
        WeatherGlobalParams(
            TEXT("/AirSimTriadRuntime/Weather/WeatherFX/WeatherGlobalParams.WeatherGlobalParams"));
    WeatherGlobalParamsCookReference = WeatherGlobalParams.Object;
}

bool ATRIADIstanaRuntimePolicyActor::ShouldSuppressAirSimVisualWeather(UWorld* World)
{
    if (!World)
    {
        return false;
    }

    for (TActorIterator<ATRIADIstanaRuntimePolicyActor> It(World); It; ++It)
    {
        if (IsValid(*It) && It->bSuppressAirSimVisualWeather)
        {
            return true;
        }
    }
    return false;
}

bool ATRIADIstanaRuntimePolicyActor::IsVisualWeatherSuppressionActive() const
{
    UWorld* World = GetWorld();
    return World &&
        !UWeatherLib::getIsWeatherEnabled(World) &&
        FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_FOG),
            0.001f) &&
        FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_RAIN),
            0.001f) &&
        FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADWETNESS),
            0.001f) &&
        FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_DUST),
            0.001f) &&
        FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_SNOW),
            0.001f) &&
        FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADSNOW),
            0.001f) &&
        FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_MAPLELEAF),
            0.001f) &&
        FMath::IsNearlyZero(UWeatherLib::getWeatherParamScalar(
            World,
            EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADLEAF),
            0.001f);
}

bool ATRIADIstanaRuntimePolicyActor::IsExponentialHeightFogSuppressionActive(
    int32& OutComponentCount) const
{
    OutComponentCount = 0;
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
        if (!IsValid(FogComponent))
        {
            bAllSuppressed = false;
            continue;
        }

        ++OutComponentCount;
        bAllSuppressed = bAllSuppressed &&
            FogComponent->FogDensity == 0.0f &&
            FogComponent->SecondFogData.FogDensity == 0.0f &&
            FogComponent->FogMaxOpacity == 0.0f &&
            !FogComponent->bEnableVolumetricFog;
    }
    return bAllSuppressed;
}

bool ATRIADIstanaRuntimePolicyActor::IsRuntimeCameraQualityProfileActive(
    FString& OutReason) const
{
    UWorld* World = GetWorld();
    ACameraActor* RuntimeCamera = nullptr;
    int32 MatchingCameraCount = 0;
    if (World && World->WorldType == EWorldType::PIE)
    {
        for (TActorIterator<ACameraActor> It(World); It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == World &&
                It->ActorHasTag(RequiredRuntimeCameraTag))
            {
                RuntimeCamera = *It;
                ++MatchingCameraCount;
            }
        }
    }
    const UCameraComponent* CameraComponent = RuntimeCamera
        ? RuntimeCamera->GetCameraComponent()
        : nullptr;
    if (MatchingCameraCount != 1 || !CameraComponent)
    {
        OutReason = FString::Printf(
            TEXT("Expected one tagged runtime camera with a camera component; found %d."),
            MatchingCameraCount);
        return false;
    }

    const FPostProcessSettings& Settings =
        CameraComponent->PostProcessSettings;
    const bool bVerified =
        !CameraComponent->bConstrainAspectRatio &&
        FMath::IsNearlyEqual(
            CameraComponent->PostProcessBlendWeight,
            1.0f,
            KINDA_SMALL_NUMBER) &&
        Settings.bOverride_AutoExposureBias &&
        FMath::IsNearlyEqual(
            Settings.AutoExposureBias,
            RuntimeCameraAutoExposureBias,
            KINDA_SMALL_NUMBER) &&
        Settings.bOverride_LocalExposureMethod &&
        Settings.LocalExposureMethod == ELocalExposureMethod::Bilateral &&
        Settings.bOverride_LocalExposureHighlightContrastScale &&
        FMath::IsNearlyEqual(
            Settings.LocalExposureHighlightContrastScale,
            RuntimeCameraLocalHighlightContrast,
            KINDA_SMALL_NUMBER) &&
        Settings.bOverride_LocalExposureShadowContrastScale &&
        FMath::IsNearlyEqual(
            Settings.LocalExposureShadowContrastScale,
            RuntimeCameraLocalShadowContrast,
            KINDA_SMALL_NUMBER) &&
        Settings.bOverride_LocalExposureDetailStrength &&
        FMath::IsNearlyEqual(
            Settings.LocalExposureDetailStrength,
            RuntimeCameraLocalDetailStrength,
            KINDA_SMALL_NUMBER) &&
        Settings.bOverride_ColorContrast &&
        FMath::IsNearlyEqual(Settings.ColorContrast.X, RuntimeCameraGlobalContrast, KINDA_SMALL_NUMBER) &&
        FMath::IsNearlyEqual(Settings.ColorContrast.Y, RuntimeCameraGlobalContrast, KINDA_SMALL_NUMBER) &&
        FMath::IsNearlyEqual(Settings.ColorContrast.Z, RuntimeCameraGlobalContrast, KINDA_SMALL_NUMBER) &&
        FMath::IsNearlyEqual(Settings.ColorContrast.W, 1.0, KINDA_SMALL_NUMBER) &&
        Settings.bOverride_Sharpen &&
        FMath::IsNearlyEqual(Settings.Sharpen, RuntimeCameraSharpen, KINDA_SMALL_NUMBER) &&
        Settings.bOverride_BloomIntensity &&
        Settings.BloomIntensity == 0.0f &&
        Settings.bOverride_VignetteIntensity &&
        Settings.VignetteIntensity == 0.0f &&
        Settings.bOverride_MotionBlurAmount &&
        Settings.MotionBlurAmount == 0.0f &&
        Settings.bOverride_SceneFringeIntensity &&
        Settings.SceneFringeIntensity == 0.0f;
    OutReason = bVerified
        ? TEXT("Runtime camera uses unconstrained aspect ratio and the exact conservative Istana exposure/local-detail/contrast/sharpen/no-lens-effects profile.")
        : TEXT("Runtime camera quality profile readback does not match the required transient Istana values.");
    return bVerified;
}

bool ATRIADIstanaRuntimePolicyActor::IsStreamedPrimaryVisualActive(
    int32& OutTilesetCount,
    float& OutMinimumLoadProgress,
    FString& OutReason) const
{
    OutTilesetCount = 0;
    OutMinimumLoadProgress = 100.0f;
    UWorld* World = GetWorld();
    bool bEveryTilesetFiniteWithPhysics =
        World && World->WorldType == EWorldType::PIE;
    if (bEveryTilesetFiniteWithPhysics)
    {
        for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
        {
            ACesium3DTileset* Tileset = *It;
            if (!IsValid(Tileset) || Tileset->GetWorld() != World)
            {
                continue;
            }
            ++OutTilesetCount;
            const float LoadProgress = Tileset->GetLoadProgress();
            if (!FMath::IsFinite(LoadProgress))
            {
                OutMinimumLoadProgress = 0.0f;
                bEveryTilesetFiniteWithPhysics = false;
            }
            else
            {
                OutMinimumLoadProgress = FMath::Min(
                    OutMinimumLoadProgress,
                    LoadProgress);
            }
            bEveryTilesetFiniteWithPhysics =
                bEveryTilesetFiniteWithPhysics &&
                Tileset->GetCreatePhysicsMeshes();
        }
    }
    bEveryTilesetFiniteWithPhysics =
        bEveryTilesetFiniteWithPhysics && OutTilesetCount > 0;

    ATRIADIstanaExteriorMeshActor* Exterior = nullptr;
    int32 ExteriorCount = 0;
    if (World)
    {
        for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(World); It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == World)
            {
                Exterior = *It;
                ++ExteriorCount;
            }
        }
    }
    const UStaticMeshComponent* ExteriorMesh = Exterior
        ? Exterior->ExteriorMeshComponent.Get()
        : nullptr;
    const bool bCollisionPreserved = ExteriorMesh &&
        ExteriorMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
    const bool bRendererHidden = ExteriorMesh &&
        !ExteriorMesh->IsVisible() && ExteriorMesh->bHiddenInGame;
    const bool bAboveRestoreThreshold =
        FMath::IsFinite(OutMinimumLoadProgress) &&
        OutMinimumLoadProgress >= FMath::Clamp(
            StreamedPrimaryRestoreBelowLoadProgress,
            95.0f,
            99.9f);
    const bool bVerified = bPreferStreamedIstanaVisualWhenReady &&
        bStreamedPrimaryVisualActiveAtRuntime &&
        bAuthoredExteriorCollisionPreservedAtRuntime &&
        !bAuthoredExteriorFallbackVisibleAtRuntime &&
        ExteriorCount == 1 && bCollisionPreserved && bRendererHidden &&
        bEveryTilesetFiniteWithPhysics && bAboveRestoreThreshold;
    OutReason = FString::Printf(
        TEXT("streamed-primary=%s, exterior-count=%d, authored-renderer-hidden=%s, authored-collision=%s, tilesets=%d, minimum-progress=%.3f%%"),
        bVerified ? TEXT("verified") : TEXT("inactive/unverified"),
        ExteriorCount,
        bRendererHidden ? TEXT("true") : TEXT("false"),
        bCollisionPreserved ? TEXT("true") : TEXT("false"),
        OutTilesetCount,
        OutMinimumLoadProgress);
    return bVerified;
}

bool ATRIADIstanaRuntimePolicyActor::IsVisualAcceptanceStudyOverlaySuppressionActive(
    int32& OutHiddenComponentCount) const
{
    OutHiddenComponentCount = 0;
    UWorld* World = GetWorld();
    int32 StudyAreaCount = 0;
    bool bAllTaggedComponentsHidden =
        World && World->WorldType == EWorldType::PIE;
    if (bAllTaggedComponentsHidden)
    {
        for (TActorIterator<ATRIADIstanaStudyAreaActor> It(World); It; ++It)
        {
            ATRIADIstanaStudyAreaActor* StudyArea = *It;
            if (!IsValid(StudyArea) || StudyArea->GetWorld() != World)
            {
                continue;
            }
            ++StudyAreaCount;
            TArray<USceneComponent*> Components;
            StudyArea->GetComponents<USceneComponent>(Components);
            for (const USceneComponent* Component : Components)
            {
                if (!IsValid(Component) ||
                    !Component->ComponentHasTag(HumanOnlyOverlayComponentTag))
                {
                    continue;
                }
                ++OutHiddenComponentCount;
                bAllTaggedComponentsHidden =
                    bAllTaggedComponentsHidden &&
                    !Component->IsVisible() && Component->bHiddenInGame;
            }
        }
    }
    return StudyAreaCount == 1 && OutHiddenComponentCount > 0 &&
        bAllTaggedComponentsHidden;
}

bool ATRIADIstanaRuntimePolicyActor::IsVisualAcceptanceAirSimProfileActive(
    FString& OutReason) const
{
    if (!FParse::Param(
            FCommandLine::Get(),
            TEXT("TRIADIstanaVisualAcceptance")))
    {
        OutReason = TEXT("The editor process was not launched with -TRIADIstanaVisualAcceptance.");
        return false;
    }
    if (!bVisualAcceptanceStreamingPolicyAppliedAtRuntime)
    {
        OutReason = TEXT("The transient visual-acceptance Cesium streaming policy has not been applied in this PlayWorld.");
        return false;
    }

    FString SettingsArgument;
    if (!FParse::Value(
            FCommandLine::Get(),
            TEXT("-settings="),
            SettingsArgument,
            false))
    {
        OutReason = TEXT("The editor process has no explicit -settings file argument.");
        return false;
    }
    SettingsArgument.TrimStartAndEndInline();
    SettingsArgument.TrimQuotesInline();
    FString ActualSettingsPath = FPaths::ConvertRelativePathToFull(SettingsArgument);
    FString ExpectedSettingsPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectConfigDir(),
        TEXT("IstanaVisualAcceptance.settings.json")));
    FPaths::NormalizeFilename(ActualSettingsPath);
    FPaths::NormalizeFilename(ExpectedSettingsPath);
    if (!FPaths::IsSamePath(ActualSettingsPath, ExpectedSettingsPath))
    {
        OutReason = TEXT("The -settings argument is not the exact project Config/IstanaVisualAcceptance.settings.json file.");
        return false;
    }

    const int64 SettingsFileSize = IFileManager::Get().FileSize(*ActualSettingsPath);
    if (SettingsFileSize <= 0 || SettingsFileSize > 64 * 1024)
    {
        OutReason = TEXT("The project visual-acceptance settings file is missing, empty, or unexpectedly large.");
        return false;
    }

    FString LoadedSettingsText;
    if (!FFileHelper::LoadFileToString(LoadedSettingsText, *ActualSettingsPath))
    {
        OutReason = TEXT("The exact project visual-acceptance settings file could not be read.");
        return false;
    }
    TSharedPtr<FJsonObject> SettingsObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(
        LoadedSettingsText);
    if (!FJsonSerializer::Deserialize(Reader, SettingsObject) ||
        !SettingsObject.IsValid())
    {
        OutReason = TEXT("The exact project visual-acceptance settings file is not valid JSON.");
        return false;
    }

    FString ProfileName;
    FString SimModeName;
    bool bProductionSensorProfile = true;
    bool bEnableRpc = true;
    if (!SettingsObject->TryGetStringField(TEXT("TRIADProfile"), ProfileName) ||
        ProfileName != TEXT("VISUAL_ACCEPTANCE_ONLY") ||
        !SettingsObject->TryGetBoolField(
            TEXT("TRIADProductionSensorProfile"),
            bProductionSensorProfile) ||
        bProductionSensorProfile ||
        !SettingsObject->TryGetStringField(TEXT("SimMode"), SimModeName) ||
        SimModeName != TEXT("ComputerVision") ||
        !SettingsObject->TryGetBoolField(TEXT("EnableRpc"), bEnableRpc) ||
        bEnableRpc)
    {
        OutReason = TEXT("The exact settings file lacks the required non-production marker, ComputerVision SimMode, or EnableRpc=false.");
        return false;
    }

    if (SettingsObject->HasField(TEXT("DefaultSensors")) ||
        LoadedSettingsText.Contains(TEXT("lidar"), ESearchCase::IgnoreCase))
    {
        OutReason = TEXT("The visual-acceptance settings must not declare DefaultSensors or any lidar field/value.");
        return false;
    }

    const TSharedPtr<FJsonObject>* VehiclesObject = nullptr;
    if (!SettingsObject->TryGetObjectField(TEXT("Vehicles"), VehiclesObject) ||
        !VehiclesObject || !VehiclesObject->IsValid() ||
        (*VehiclesObject)->Values.Num() != 1)
    {
        OutReason = TEXT("The exact settings file must declare one visual-acceptance vehicle.");
        return false;
    }

    const auto VehicleIterator = (*VehiclesObject)->Values.CreateConstIterator();
    const TSharedPtr<FJsonValue> VehicleValue = VehicleIterator.Value();
    const TSharedPtr<FJsonObject> VehicleObject =
        VehicleValue.IsValid() && VehicleValue->Type == EJson::Object
            ? VehicleValue->AsObject()
            : nullptr;
    FString VehicleType;
    bool bAutoCreate = false;
    const TSharedPtr<FJsonObject>* SensorsObject = nullptr;
    if (!VehicleObject.IsValid() ||
        !VehicleObject->TryGetStringField(TEXT("VehicleType"), VehicleType) ||
        VehicleType != TEXT("ComputerVision") ||
        !VehicleObject->TryGetBoolField(TEXT("AutoCreate"), bAutoCreate) ||
        !bAutoCreate ||
        !VehicleObject->TryGetObjectField(TEXT("Sensors"), SensorsObject) ||
        !SensorsObject || !SensorsObject->IsValid() ||
        (*SensorsObject)->Values.Num() != 0)
    {
        OutReason = TEXT("The sole settings vehicle must be auto-created ComputerVision with an empty Sensors object.");
        return false;
    }

    UWorld* World = GetWorld();
    int32 SimModeCount = 0;
    int32 ComputerVisionSimModeCount = 0;
    if (World)
    {
        for (TActorIterator<ASimModeBase> It(World); It; ++It)
        {
            ASimModeBase* SimMode = *It;
            if (!IsValid(SimMode) || SimMode->GetWorld() != World ||
                !SimMode->HasActorBegunPlay())
            {
                continue;
            }
            ++SimModeCount;
            if (SimMode->IsA<ASimModeComputerVision>())
            {
                ++ComputerVisionSimModeCount;
            }
        }
    }
    if (SimModeCount != 1 || ComputerVisionSimModeCount != 1)
    {
        OutReason = FString::Printf(
            TEXT("PlayWorld must contain exactly one begun AirSim ComputerVision SimMode and no other SimMode; found %d total, %d ComputerVision."),
            SimModeCount,
            ComputerVisionSimModeCount);
        return false;
    }

    OutReason = TEXT("Parsed exact project-owned VISUAL_ACCEPTANCE_ONLY settings and found exactly one live ComputerVision SimMode: RPC disabled, one camera vehicle, empty Sensors, no lidar declaration.");
    return true;
}

bool ATRIADIstanaRuntimePolicyActor::RequestAirSimQuiescenceForTeardown(
    int32& OutSimModeCount)
{
    OutSimModeCount = 0;
    bAirSimQuiescenceRequestedForTeardown = false;
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

    // No SimMode means there is no AirSim physics/sensor updater to drain.
    bAirSimQuiescenceRequestedForTeardown = bAllPaused;
    return bAirSimQuiescenceRequestedForTeardown;
}

void ATRIADIstanaRuntimePolicyActor::ConfigureMapMetadata(
    const FVector& InPreservedGeoreferenceOriginLongitudeLatitudeHeight,
    const FVector& InStudyCenterLongitudeLatitudeHeight)
{
    PreservedGeoreferenceOriginLongitudeLatitudeHeight =
        InPreservedGeoreferenceOriginLongitudeLatitudeHeight;
    StudyCenterLongitudeLatitudeHeight = InStudyCenterLongitudeLatitudeHeight;
}

void ATRIADIstanaRuntimePolicyActor::ConfigureGroundCalibration(
    double InGroundHeightMeters,
    int32 InAcceptedSampleCount,
    bool bInCalibrated,
    const FString& InHeightSource)
{
    ExteriorGroundHeightMeters = InGroundHeightMeters;
    ExteriorGroundAcceptedSampleCount = FMath::Max(InAcceptedSampleCount, 0);
    bExteriorGroundHeightCalibrated = bInCalibrated;
    ExteriorGroundHeightSource = InHeightSource;
    StudyCenterLongitudeLatitudeHeight.Z = InGroundHeightMeters;
}

void ATRIADIstanaRuntimePolicyActor::BeginPlay()
{
    Super::BeginPlay();

    RuntimeCameraEnforcementAttempts = 0;
    bRuntimeCameraEnforcementComplete = false;
    bRuntimeCameraViewTargetVerifiedAtRuntime = false;
    bRuntimeCameraQualityProfileVerifiedAtRuntime = false;
    bAirSimQuiescenceRequestedForTeardown = false;
    bVisualAcceptanceStreamingPolicyAppliedAtRuntime = false;
    bVisualAcceptanceStudyOverlaysHiddenAtRuntime = false;
    VisualAcceptanceHiddenStudyOverlayCountAtRuntime = 0;
    bStreamedPrimaryVisualActiveAtRuntime = false;
    bAuthoredExteriorFallbackVisibleAtRuntime = true;
    bAuthoredExteriorCollisionPreservedAtRuntime = false;
    StreamedPrimaryTilesetCountAtRuntime = 0;
    StreamedPrimaryMinimumLoadProgressAtRuntime = 0.0f;
    bExponentialHeightFogSuppressionVerifiedAtRuntime = false;
    ExponentialHeightFogComponentCountAtRuntime = 0;
    ExponentialFogSuppressionAttempts = 0;
    TilesetRefreshAttempts = 0;

    AGameModeBase* ActiveGameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
    bGameModeOverrideVerifiedAtRuntime =
        !bRequireIstanaAirSimGameMode ||
        (ActiveGameMode &&
         ActiveGameMode->GetClass()->GetPathName() == RequiredGameModeClassPath);
    if (!bGameModeOverrideVerifiedAtRuntime)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("TRIAD Istana PIE started without required persisted GameMode '%s' (actual '%s'). Stop PIE and rebuild or run the exact-v2 GameMode repair."),
            *RequiredGameModeClassPath,
            ActiveGameMode ? *ActiveGameMode->GetClass()->GetPathName() : TEXT("<none>"));
    }

    if (bSuppressAirSimVisualWeather)
    {
        ApplyClearVisualWeatherPolicy();
    }

    // This is a destination-map visual policy, independent of AirSim's
    // weather material system and independent of visual-acceptance mode.
    ApplyClearExponentialHeightFogPolicy();
    GetWorldTimerManager().SetTimer(
        ExponentialFogSuppressionTimerHandle,
        this,
        &ATRIADIstanaRuntimePolicyActor::EnforceClearExponentialHeightFog,
        FMath::Clamp(ExponentialFogSuppressionInitialDelaySeconds, 0.1f, 10.0f),
        false);

    if (FParse::Param(
            FCommandLine::Get(),
            TEXT("TRIADIstanaVisualAcceptance")))
    {
        ApplyVisualAcceptanceStreamingPolicy();
    }

    if (bEnforceTaggedRuntimeCameraForPlayer0 &&
        MaximumRuntimeCameraEnforcementAttempts > 0)
    {
        GetWorldTimerManager().SetTimer(
            RuntimeCameraEnforcementTimerHandle,
            this,
            &ATRIADIstanaRuntimePolicyActor::EnforceRuntimeCameraForPlayer0,
            FMath::Clamp(RuntimeCameraEnforcementInitialDelaySeconds, 0.1f, 10.0f),
            false);
    }
    else
    {
        bRuntimeCameraEnforcementComplete = true;
        bRuntimeCameraViewTargetVerifiedAtRuntime = !bEnforceTaggedRuntimeCameraForPlayer0;
    }

    if (bRefreshUnreadyCesiumTilesets && MaximumTilesetRefreshAttempts > 0)
    {
        GetWorldTimerManager().SetTimer(
            TilesetRefreshTimerHandle,
            this,
            &ATRIADIstanaRuntimePolicyActor::RefreshUnreadyTilesets,
            FMath::Clamp(InitialTilesetCheckDelaySeconds, 1.0f, 60.0f),
            false);
    }

    // The first evaluation is fail-safe: authored rendering stays visible
    // until every tileset reaches exact 100% with physics. A clamped recurring
    // PlayWorld-only timer provides hysteretic restoration if readiness drops.
    EvaluateStreamedPrimaryVisual();
    if (bPreferStreamedIstanaVisualWhenReady)
    {
        const float EvaluationInterval = FMath::Clamp(
            StreamedPrimaryEvaluationIntervalSeconds,
            0.25f,
            5.0f);
        GetWorldTimerManager().SetTimer(
            StreamedPrimaryEvaluationTimerHandle,
            this,
            &ATRIADIstanaRuntimePolicyActor::EvaluateStreamedPrimaryVisual,
            EvaluationInterval,
            true,
            EvaluationInterval);
    }
}

void ATRIADIstanaRuntimePolicyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(TilesetRefreshTimerHandle);
    GetWorldTimerManager().ClearTimer(RuntimeCameraEnforcementTimerHandle);
    GetWorldTimerManager().ClearTimer(ExponentialFogSuppressionTimerHandle);
    GetWorldTimerManager().ClearTimer(StreamedPrimaryEvaluationTimerHandle);
    Super::EndPlay(EndPlayReason);
}

void ATRIADIstanaRuntimePolicyActor::ApplyClearVisualWeatherPolicy()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // AirSim visual weather is a world-wide material/actor system. Disable it
    // without deleting the template's authored sky or weather controllers.
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

void ATRIADIstanaRuntimePolicyActor::ApplyClearExponentialHeightFogPolicy()
{
    UWorld* World = GetWorld();
    if (!World || !bSuppressInheritedExponentialHeightFog)
    {
        return;
    }

    for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
    {
        AExponentialHeightFog* FogActor = *It;
        UExponentialHeightFogComponent* FogComponent = FogActor
            ? FogActor->GetComponent()
            : nullptr;
        if (!IsValid(FogActor) || FogActor->GetWorld() != World ||
            !IsValid(FogComponent))
        {
            continue;
        }

        FogComponent->SetFogDensity(0.0f);
        FogComponent->SetSecondFogDensity(0.0f);
        FogComponent->SetFogMaxOpacity(0.0f);
        FogComponent->SetVolumetricFog(false);
    }
}

void ATRIADIstanaRuntimePolicyActor::EnforceClearExponentialHeightFog()
{
    if (ExponentialFogSuppressionAttempts >=
        MaximumExponentialFogSuppressionAttempts)
    {
        return;
    }

    ++ExponentialFogSuppressionAttempts;
    ApplyClearExponentialHeightFogPolicy();
    bExponentialHeightFogSuppressionVerifiedAtRuntime =
        IsExponentialHeightFogSuppressionActive(
            ExponentialHeightFogComponentCountAtRuntime);
    if (!bExponentialHeightFogSuppressionVerifiedAtRuntime &&
        ExponentialFogSuppressionAttempts <
            MaximumExponentialFogSuppressionAttempts)
    {
        GetWorldTimerManager().SetTimer(
            ExponentialFogSuppressionTimerHandle,
            this,
            &ATRIADIstanaRuntimePolicyActor::EnforceClearExponentialHeightFog,
            FMath::Clamp(ExponentialFogSuppressionRetryDelaySeconds, 0.1f, 5.0f),
            false);
    }
}

void ATRIADIstanaRuntimePolicyActor::ApplyVisualAcceptanceStreamingPolicy()
{
    UWorld* World = GetWorld();
    if (!World || World->WorldType != EWorldType::PIE)
    {
        return;
    }

    int32 TilesetCount = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        if (!IsValid(Tileset) || Tileset->GetWorld() != World)
        {
            continue;
        }

        // This actor exists only in the duplicated PIE world. Re-enable
        // frustum culling for the visual acceptance view without Modify(),
        // package dirtiness, or any change to the production all-AOI policy.
        Tileset->EnableFrustumCulling = true;
        Tileset->EnableFogCulling = false;
        Tileset->EnforceCulledScreenSpaceError = true;
        Tileset->CulledScreenSpaceError = 32.0;
        Tileset->RefreshTileset();
        ++TilesetCount;
    }

    bVisualAcceptanceStreamingPolicyAppliedAtRuntime = TilesetCount > 0;

    int32 HiddenOverlayCount = 0;
    for (TActorIterator<ATRIADIstanaStudyAreaActor> It(World); It; ++It)
    {
        ATRIADIstanaStudyAreaActor* StudyArea = *It;
        if (!IsValid(StudyArea) || StudyArea->GetWorld() != World)
        {
            continue;
        }
        TArray<USceneComponent*> Components;
        StudyArea->GetComponents<USceneComponent>(Components);
        for (USceneComponent* Component : Components)
        {
            if (!IsValid(Component) ||
                !Component->ComponentHasTag(HumanOnlyOverlayComponentTag))
            {
                continue;
            }
            // Hide only human-debug rendering in the duplicated PlayWorld.
            // The study actor, exact Cesium clip, and collision semantics are
            // untouched.
            Component->SetVisibility(false, false);
            Component->SetHiddenInGame(true, false);
            ++HiddenOverlayCount;
        }
    }
    int32 VerifiedHiddenOverlayCount = 0;
    bVisualAcceptanceStudyOverlaysHiddenAtRuntime =
        IsVisualAcceptanceStudyOverlaySuppressionActive(
            VerifiedHiddenOverlayCount) &&
        VerifiedHiddenOverlayCount == HiddenOverlayCount;
    VisualAcceptanceHiddenStudyOverlayCountAtRuntime =
        VerifiedHiddenOverlayCount;
    if (bVisualAcceptanceStreamingPolicyAppliedAtRuntime)
    {
        // The visual-mode property transition already performed its one
        // intentional refresh; suppress the later generic zero-progress retry.
        TilesetRefreshAttempts = MaximumTilesetRefreshAttempts;
    }
}

void ATRIADIstanaRuntimePolicyActor::ApplyRuntimeCameraQualityProfile(
    ACameraActor* RuntimeCamera)
{
    UCameraComponent* CameraComponent = RuntimeCamera
        ? RuntimeCamera->GetCameraComponent()
        : nullptr;
    if (!bApplyRuntimeCameraQualityProfile || !CameraComponent ||
        RuntimeCamera->GetWorld() != GetWorld())
    {
        bRuntimeCameraQualityProfileVerifiedAtRuntime = false;
        return;
    }

    // Runtime-only view tuning: this is applied to the duplicated PIE camera
    // without Modify(), package dirtiness, or a map save.
    CameraComponent->SetConstraintAspectRatio(false);
    CameraComponent->SetPostProcessBlendWeight(1.0f);
    FPostProcessSettings& Settings = CameraComponent->PostProcessSettings;
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = RuntimeCameraAutoExposureBias;
    Settings.bOverride_LocalExposureMethod = true;
    Settings.LocalExposureMethod = ELocalExposureMethod::Bilateral;
    Settings.bOverride_LocalExposureHighlightContrastScale = true;
    Settings.LocalExposureHighlightContrastScale =
        RuntimeCameraLocalHighlightContrast;
    Settings.bOverride_LocalExposureShadowContrastScale = true;
    Settings.LocalExposureShadowContrastScale =
        RuntimeCameraLocalShadowContrast;
    Settings.bOverride_LocalExposureDetailStrength = true;
    Settings.LocalExposureDetailStrength = RuntimeCameraLocalDetailStrength;
    Settings.bOverride_ColorContrast = true;
    Settings.ColorContrast = FVector4(
        RuntimeCameraGlobalContrast,
        RuntimeCameraGlobalContrast,
        RuntimeCameraGlobalContrast,
        1.0);
    Settings.bOverride_Sharpen = true;
    Settings.Sharpen = RuntimeCameraSharpen;
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = 0.0f;
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = 0.0f;
    Settings.bOverride_MotionBlurAmount = true;
    Settings.MotionBlurAmount = 0.0f;
    Settings.bOverride_SceneFringeIntensity = true;
    Settings.SceneFringeIntensity = 0.0f;

    FString QualityReason;
    bRuntimeCameraQualityProfileVerifiedAtRuntime =
        IsRuntimeCameraQualityProfileActive(QualityReason);
}

void ATRIADIstanaRuntimePolicyActor::EnforceRuntimeCameraForPlayer0()
{
    UWorld* World = GetWorld();
    if (!World || RuntimeCameraEnforcementAttempts >= MaximumRuntimeCameraEnforcementAttempts)
    {
        bRuntimeCameraEnforcementComplete = true;
        return;
    }

    ACameraActor* RuntimeCamera = nullptr;
    int32 MatchingCameraCount = 0;
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (It->ActorHasTag(RequiredRuntimeCameraTag))
        {
            RuntimeCamera = *It;
            ++MatchingCameraCount;
        }
    }
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (PlayerController &&
        (PlayerController->GetWorld() != World || !PlayerController->IsLocalController()))
    {
        PlayerController = nullptr;
    }
    if (MatchingCameraCount == 1 && PlayerController && RuntimeCamera)
    {
        ApplyRuntimeCameraQualityProfile(RuntimeCamera);
        PlayerController->SetViewTargetWithBlend(RuntimeCamera, 0.0f);
        bRuntimeCameraViewTargetVerifiedAtRuntime =
            PlayerController->GetViewTarget() == RuntimeCamera;
    }
    else
    {
        bRuntimeCameraViewTargetVerifiedAtRuntime = false;
    }

    ++RuntimeCameraEnforcementAttempts;
    bRuntimeCameraEnforcementComplete =
        RuntimeCameraEnforcementAttempts >= MaximumRuntimeCameraEnforcementAttempts;
    if (!bRuntimeCameraEnforcementComplete)
    {
        GetWorldTimerManager().SetTimer(
            RuntimeCameraEnforcementTimerHandle,
            this,
            &ATRIADIstanaRuntimePolicyActor::EnforceRuntimeCameraForPlayer0,
            FMath::Clamp(RuntimeCameraEnforcementRetryDelaySeconds, 0.1f, 5.0f),
            false);
        return;
    }

    if (bRuntimeCameraViewTargetVerifiedAtRuntime)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("TRIAD Istana bounded Player 0 camera enforcement completed after %d attempt(s): tagged camera count=%d, verified=true."),
            RuntimeCameraEnforcementAttempts,
            MatchingCameraCount);
    }
    else
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("TRIAD Istana bounded Player 0 camera enforcement failed after %d attempt(s): tagged camera count=%d."),
            RuntimeCameraEnforcementAttempts,
            MatchingCameraCount);
    }
}

void ATRIADIstanaRuntimePolicyActor::EvaluateStreamedPrimaryVisual()
{
    UWorld* World = GetWorld();
    if (!World || World->WorldType != EWorldType::PIE)
    {
        return;
    }

    TArray<ATRIADIstanaExteriorMeshActor*> ExteriorActors;
    for (TActorIterator<ATRIADIstanaExteriorMeshActor> It(World); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == World)
        {
            ExteriorActors.Add(*It);
        }
    }
    UStaticMeshComponent* ExteriorMesh =
        ExteriorActors.Num() == 1 && ExteriorActors[0]
            ? ExteriorActors[0]->ExteriorMeshComponent.Get()
            : nullptr;

    bool bAllExactHundredWithPhysics = true;
    bool bReadinessDroppedMaterially = false;
    int32 TilesetCount = 0;
    float MinimumLoadProgress = 100.0f;
    const float RestoreThreshold = FMath::Clamp(
        StreamedPrimaryRestoreBelowLoadProgress,
        95.0f,
        99.9f);
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        if (!IsValid(Tileset) || Tileset->GetWorld() != World)
        {
            continue;
        }
        ++TilesetCount;
        const float LoadProgress = Tileset->GetLoadProgress();
        const bool bFinite = FMath::IsFinite(LoadProgress);
        if (bFinite)
        {
            MinimumLoadProgress = FMath::Min(
                MinimumLoadProgress,
                LoadProgress);
        }
        else
        {
            MinimumLoadProgress = 0.0f;
        }
        const bool bPhysics = Tileset->GetCreatePhysicsMeshes();
        bAllExactHundredWithPhysics =
            bAllExactHundredWithPhysics && bFinite && bPhysics &&
            LoadProgress == 100.0f;
        bReadinessDroppedMaterially =
            bReadinessDroppedMaterially || !bFinite || !bPhysics ||
            (bFinite && LoadProgress < RestoreThreshold);
    }
    bAllExactHundredWithPhysics =
        bAllExactHundredWithPhysics && TilesetCount > 0;
    bReadinessDroppedMaterially =
        bReadinessDroppedMaterially || TilesetCount == 0;
    StreamedPrimaryTilesetCountAtRuntime = TilesetCount;
    StreamedPrimaryMinimumLoadProgressAtRuntime =
        TilesetCount > 0 ? MinimumLoadProgress : 0.0f;

    const bool bCollisionPreserved = ExteriorMesh &&
        ExteriorMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
    const bool bKeepStreamedPrimary =
        bStreamedPrimaryVisualActiveAtRuntime &&
        !bReadinessDroppedMaterially;
    const bool bActivateStreamedPrimary =
        !bStreamedPrimaryVisualActiveAtRuntime &&
        bAllExactHundredWithPhysics;
    const bool bHideAuthoredRenderer =
        bPreferStreamedIstanaVisualWhenReady &&
        ExteriorActors.Num() == 1 && ExteriorMesh && bCollisionPreserved &&
        (bKeepStreamedPrimary || bActivateStreamedPrimary);

    if (bHideAuthoredRenderer)
    {
        ExteriorMesh->SetVisibility(false, false);
        ExteriorMesh->SetHiddenInGame(true, false);
    }
    else
    {
        // Fail safe across missing/duplicate actors, opt-out, collision loss,
        // no tilesets, non-finite progress, lost physics, or material load drop.
        for (ATRIADIstanaExteriorMeshActor* Exterior : ExteriorActors)
        {
            UStaticMeshComponent* Candidate = Exterior
                ? Exterior->ExteriorMeshComponent.Get()
                : nullptr;
            if (Candidate)
            {
                Candidate->SetVisibility(true, false);
                Candidate->SetHiddenInGame(false, false);
            }
        }
    }

    bAuthoredExteriorCollisionPreservedAtRuntime = bCollisionPreserved;
    bAuthoredExteriorFallbackVisibleAtRuntime = ExteriorMesh &&
        ExteriorMesh->IsVisible() && !ExteriorMesh->bHiddenInGame;
    bStreamedPrimaryVisualActiveAtRuntime = ExteriorMesh &&
        !ExteriorMesh->IsVisible() && ExteriorMesh->bHiddenInGame &&
        bCollisionPreserved && bHideAuthoredRenderer;

    // A failed component visibility readback must never leave the fallback
    // half-hidden while claiming streamed-primary success.
    if (bHideAuthoredRenderer && !bStreamedPrimaryVisualActiveAtRuntime &&
        ExteriorMesh)
    {
        ExteriorMesh->SetVisibility(true, false);
        ExteriorMesh->SetHiddenInGame(false, false);
        bAuthoredExteriorFallbackVisibleAtRuntime = true;
    }
}

void ATRIADIstanaRuntimePolicyActor::RefreshUnreadyTilesets()
{
    UWorld* World = GetWorld();
    if (!World || TilesetRefreshAttempts >= MaximumTilesetRefreshAttempts)
    {
        return;
    }

    int32 TilesetCount = 0;
    int32 RefreshedCount = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        if (!IsValid(Tileset))
        {
            continue;
        }

        ++TilesetCount;
        Tileset->EnableFogCulling = false;
        if (Tileset->GetLoadProgress() <= KINDA_SMALL_NUMBER)
        {
            // RefreshTileset clears the native tileset and reacquires its
            // endpoint/session. Never log its URL, token, or request details.
            Tileset->RefreshTileset();
            ++RefreshedCount;
        }
    }

    ++TilesetRefreshAttempts;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("TRIAD Istana runtime checked %d Cesium tileset(s); refreshed %d still at zero progress (attempt %d/%d)."),
        TilesetCount,
        RefreshedCount,
        TilesetRefreshAttempts,
        MaximumTilesetRefreshAttempts);

    if (RefreshedCount > 0 && TilesetRefreshAttempts < MaximumTilesetRefreshAttempts)
    {
        GetWorldTimerManager().SetTimer(
            TilesetRefreshTimerHandle,
            this,
            &ATRIADIstanaRuntimePolicyActor::RefreshUnreadyTilesets,
            FMath::Clamp(TilesetRetryDelaySeconds, 1.0f, 120.0f),
            false);
    }
}
