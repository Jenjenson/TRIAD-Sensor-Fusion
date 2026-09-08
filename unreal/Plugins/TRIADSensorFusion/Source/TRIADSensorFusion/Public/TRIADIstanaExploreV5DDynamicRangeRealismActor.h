#pragma once

#include "Camera/CameraModifier.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5DDynamicRangeRealismActor.generated.h"

class ADirectionalLight;
class APlayerCameraManager;
class ASkyAtmosphere;
class ASkyLight;
class ATRIADIstanaExploreV5DDynamicRangeRealismActor;
class ATRIADIstanaExploreV5Pawn;
class UPostProcessComponent;
class USceneComponent;
class UWorld;

/**
 * Small, serializable readback of the inherited clear-day lighting actors.
 * The V5D dynamic-range pass captures this before configuration and refuses
 * to operate if any of these source values later move.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API
    FTRIADIstanaExploreV5DSourceLightingSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTransform SunActorTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTransform SkyLightActorTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FTransform SkyAtmosphereActorTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float SunIntensityLux = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float SkyLightIntensity = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    uint8 SunMobility = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    uint8 SkyLightMobility = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bSunIsAtmosphereLight = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 AtmosphereSunLightIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bSkyLightRealTimeCapture = false;

    bool Equals(
        const FTRIADIstanaExploreV5DSourceLightingSnapshot& Other,
        double Tolerance = 0.0001) const;
};

/**
 * Runtime bridge that reapplies the one actor-owned post-process component at
 * the final camera override stage. UE 5.5 composes a camera's neutral physical
 * profile after world volumes, so a base-only unbound volume would otherwise
 * be overwritten. This modifier never writes location, rotation, FOV, shutter,
 * ISO, aperture, white balance, exposure bias, or the pawn camera settings.
 */
UCLASS(NotBlueprintable, Transient)
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DDynamicRangeCameraModifier
    : public UCameraModifier
{
    GENERATED_BODY()

public:
    UTRIADIstanaExploreV5DDynamicRangeCameraModifier(
        const FObjectInitializer& ObjectInitializer);

    void InitializeForDynamicRangeActor(
        ATRIADIstanaExploreV5DDynamicRangeRealismActor* InActor);

    virtual bool ModifyCamera(
        float DeltaTime,
        FMinimalViewInfo& InOutPOV) override;

    UPROPERTY(Transient)
    TObjectPtr<ATRIADIstanaExploreV5DDynamicRangeRealismActor>
        DynamicRangeActor = nullptr;
};

/**
 * V5D-only photographic dynamic-range presentation.
 *
 * Exactly one bounded-settings, unbound UPostProcessComponent reduces local
 * highlight/shadow base contrast, retains fine detail, and applies restrained
 * neutral color grading. It is appearance-only: inherited sun/sky actors,
 * physical camera settings, materials, collision, navigation, sensors and RF
 * authority remain untouched. Values are look-development assumptions, not a
 * survey, display calibration, current weather, or measured camera response.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API
    ATRIADIstanaExploreV5DDynamicRangeRealismActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5DDynamicRangeRealismActor();

    /** C++ builder entry point; only the exact map or trusted temp build world. */
    bool ConfigureDynamicRangeRealism(
        bool bTrustedUntitledHybridBuilder,
        FString& OutError);

    /** Exact authored/readback contract; set bRequireRuntime for Player0 proof. */
    bool ValidateDynamicRangeRealism(
        bool bRequireRuntime,
        FString& OutReport) const;

    /** Pure settings builder used by the component and native automation. */
    static void BuildExpectedPostProcessSettings(
        FPostProcessSettings& OutSettings);

    static bool ValidateExpectedPostProcessSettings(
        const FPostProcessSettings& Settings,
        FString& OutError);

    static bool CaptureExactSourceLightingSnapshot(
        UWorld* World,
        FTRIADIstanaExploreV5DSourceLightingSnapshot& OutSnapshot,
        FString& OutError);

    static const FString& ExpectedClaimLabel();
    static const FString& ExpectedTargetMapPackage();
    static const FName& ExpectedActorTag();
    static float ExpectedHighlightContrastScale();
    static float ExpectedShadowContrastScale();
    static float ExpectedDetailStrength();
    static float ExpectedBlurredLuminanceBlend();
    static float ExpectedBlurredLuminanceKernelSizePercent();
    static float ExpectedGlobalSaturation();
    static float ExpectedGlobalContrast();

    /** Called only by the actor-owned camera modifier. */
    bool CanSupplyFinalCameraOverride() const;
    bool AppendFinalCameraOverride(APlayerCameraManager* CameraManager) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    /** The only post-process component owned by this pass. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Components")
    TObjectPtr<UPostProcessComponent> DynamicRangePostProcess;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Sources")
    TObjectPtr<ADirectionalLight> SourceSun;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Sources")
    TObjectPtr<ASkyLight> SourceSkyLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Sources")
    TObjectPtr<ASkyAtmosphere> SourceSkyAtmosphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Sources")
    FTRIADIstanaExploreV5DSourceLightingSnapshot
        SavedSourceLightingSnapshot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bExactV5DMapOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bSourceSunSkyOrAtmosphereModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bPawnPhysicalCameraProfileModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bMaterialsOrEmissiveRetuned = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bCollisionNavigationSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Truth")
    bool bSurveyDisplayCalibrationOrCurrentWeatherClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    bool bRuntimeFinalCameraOverrideReady = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    int32 RuntimeActivationAttempts = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5D|Runtime")
    int32 MaximumRuntimeActivationAttempts = 8;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool TryActivateRuntimeFinalCameraOverride(FString& OutError);
    void DeactivateRuntimeFinalCameraOverride();
    bool ValidateSourceLightingStillMatches(FString& OutError) const;
    bool ResolveExpectedRuntimePawn(
        ATRIADIstanaExploreV5Pawn*& OutPawn,
        APlayerCameraManager*& OutCameraManager,
        FString& OutError) const;

    UPROPERTY(Transient)
    TObjectPtr<UTRIADIstanaExploreV5DDynamicRangeCameraModifier>
        RuntimeCameraModifier = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<ATRIADIstanaExploreV5Pawn> RuntimeV5Pawn = nullptr;

    UPROPERTY()
    bool bConfigured = false;

    UPROPERTY()
    bool bConfiguredInTrustedUntitledHybridBuilder = false;
};
