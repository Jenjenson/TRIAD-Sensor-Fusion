#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaPublicViewRuntimePolicyActor.generated.h"

class ACameraActor;

/** Runtime-only visibility and fixed-camera policy for the isolated public-view map. */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaPublicViewRuntimePolicyActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaPublicViewRuntimePolicyActor();

    /** Exact-zero readback for all eight AirSim weather channels and every fog component. */
    bool IsFogSuppressionActive(int32& OutFogComponentCount) const;

    /** Verify the tagged camera and the neutral fixed-exposure profile. */
    bool IsFixedPrimaryCameraProfileActive(FString& OutReason) const;

    /**
     * Pause every begun AirSim simulation mode before an explicit PIE stop.
     * Zero modes is already quiescent; every discovered mode must read back
     * paused before this operation succeeds.
     */
    bool RequestAirSimQuiescenceForTeardown(int32& OutSimModeCount);

    static const FString& ExpectedClaimLabel();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    FString ReferenceEpoch = TEXT("2024-04");

    /** Neutral camera is enforced; acceptance-lab OCIO/display state is external. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    FString DisplayColorPolicy =
        TEXT("NEUTRAL_CAMERA_LOCAL_EXPOSURE_DISABLED_EXTERNAL_SRGB_REC709_DISPLAY_NOT_OCIO_VALIDATED");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    bool bExternalOcioDisplayValidated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Public View|Runtime")
    bool bSuppressAirSimVisualWeather = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Public View|Runtime")
    bool bSuppressExponentialHeightFog = true;

    /** Require the map's exact TRIAD-owned AirSim GameMode wrapper. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Runtime")
    bool bRequireIstanaAirSimGameMode = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Runtime")
    FString RequiredGameModeClassPath = TEXT("/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode");

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Runtime")
    bool bGameModeOverrideVerifiedAtRuntime = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Public View|Runtime")
    bool bEnforceFixedPrimaryCamera = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Runtime")
    FName RequiredPrimaryCameraTag = TEXT("TRIADIstanaPublicViewCamera_Primary");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Public View|Runtime", meta = (ClampMin = "1", ClampMax = "12"))
    int32 MaximumEnforcementAttempts = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Public View|Runtime", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float EnforcementRetrySeconds = 0.75f;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Runtime")
    bool bFogSuppressionVerifiedAtRuntime = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Runtime")
    int32 FogComponentCountAtRuntime = 0;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Runtime")
    bool bPrimaryCameraVerifiedAtRuntime = false;

    /** True only after the final pass of the bounded AirSim settle window. */
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Runtime")
    bool bRuntimePolicySettledAtRuntime = false;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ApplyClearWeatherAndFog();
    void ApplyFixedCameraProfile(ACameraActor* Camera);
    void EnforceRuntimePolicy();

    FTimerHandle EnforcementTimerHandle;
    int32 EnforcementAttempts = 0;
};
