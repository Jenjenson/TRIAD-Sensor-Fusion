#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV5AppearanceActor.generated.h"

class ATRIADIstanaExploreV4LandscapeActor;
class ATRIADIstanaPublicViewSceneActor;
class ADirectionalLight;
class UMaterialInterface;
class USceneComponent;
class UTexture2D;

/**
 * Additive, appearance-only bindings for the Explore V5 public-data visual
 * approximation.
 *
 * This actor owns no site geometry. It changes only five exact material
 * bindings, a closed component contact-shadow roster, and four texture
 * parameters on the V4 close-turf runtime MID. Meshes, sections, transforms,
 * instances, collision, navigation, sun pose/colour/intensity and sensor/RF
 * authority remain inherited and untouched. The exact tagged sun's bounded
 * contact-shadow settings are the sole admitted light-state delta.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5AppearanceActor
    : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5AppearanceActor();

    /**
     * Persist the exact V5 appearance roster and apply the static bindings.
     * Close-turf textures are deliberately deferred until the next runtime
     * tick, after V4 has created its unique wind MID.
     */
    bool ConfigureExploreV5Appearance(
        ATRIADIstanaPublicViewSceneActor* InPublicViewSceneActor,
        ATRIADIstanaExploreV4LandscapeActor* InExploreV4LandscapeActor,
        UMaterialInterface* InLawnMaterial,
        UMaterialInterface* InFountainWaterMaterial,
        UMaterialInterface* InHardscapeStoneMaterial,
        UMaterialInterface* InContextRenderMaterial,
        UMaterialInterface* InContextRoofMaterial,
        UTexture2D* InCloseTurfBaseColorTexture,
        UTexture2D* InCloseTurfNormalTexture,
        UTexture2D* InCloseTurfRoughnessTexture,
        UTexture2D* InCloseTurfOpacityTexture,
        FString& OutError);

    /** Reapply only the bounded static-material and contact-shadow delta. */
    bool ReapplyExploreV5Appearance(FString& OutError);

    /**
     * Fail-closed saved/runtime readback. Set bRequireRuntimeCloseTurfRebind
     * only after BeginPlay's deferred V4 MID texture binding has completed.
     */
    bool ValidateExploreV5Appearance(
        FString& OutReport,
        bool bRequireRuntimeCloseTurfRebind = false) const;

    static const FString& ExpectedClaimLabel();
    static const FString& ExpectedLawnMaterialPath();
    static const FString& ExpectedFountainWaterMaterialPath();
    static const FString& ExpectedHardscapeStoneMaterialPath();
    static const FString& ExpectedContextRenderMaterialPath();
    static const FString& ExpectedContextRoofMaterialPath();
    static const FString& ExpectedCloseTurfBaseMaterialPath();
    static const FString& ExpectedCloseTurfBaseColorTexturePath();
    static const FString& ExpectedCloseTurfNormalTexturePath();
    static const FString& ExpectedCloseTurfRoughnessTexturePath();
    static const FString& ExpectedCloseTurfOpacityTexturePath();
    static FName ExpectedDirectionalSunTag();
    static float ExpectedContactShadowLengthCentimeters();
    static bool ExpectedContactShadowLengthInWorldSpace();
    static float ExpectedContactShadowCastingIntensity();
    static float ExpectedContactShadowNonCastingIntensity();
    static int32 ExpectedContactShadowEnabledComponentCount();
    static int32 ExpectedContactShadowDisabledComponentCount();

#if WITH_DEV_AUTOMATION_TESTS
    /**
     * Pure automation hook over the same exact 28-row contract used by the
     * live component validator. Mobility values are EComponentMobility::Type
     * encoded as uint8 so this header does not widen its engine dependencies.
     */
    static bool ValidateContactShadowRosterStateForAutomation(
        const TArray<FName>& ComponentNames,
        const TArray<uint8>& Mobilities,
        const TArray<bool>& CastShadows,
        const TArray<bool>& CastContactShadows,
        bool bRequireAppliedState,
        FString& OutError);

    /** Pure readback for the exact V5D-only suppression of ten V4 tree rows. */
    static bool ValidateV5DTreeRuntimeContactShadowRosterForAutomation(
        const TArray<FName>& ComponentNames,
        const TArray<uint8>& Mobilities,
        const TArray<bool>& CastShadows,
        const TArray<bool>& CastContactShadows,
        FString& OutError);

    /** Validate inherited state, apply the exact V5 bits, then read back. */
    static bool ApplyContactShadowRosterStateForAutomation(
        const TArray<FName>& ComponentNames,
        const TArray<uint8>& Mobilities,
        const TArray<bool>& CastShadows,
        TArray<bool>& InOutCastContactShadows,
        FString& OutError);
#endif

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Targets")
    TObjectPtr<ATRIADIstanaPublicViewSceneActor> PublicViewSceneActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Targets")
    TObjectPtr<ATRIADIstanaExploreV4LandscapeActor> ExploreV4LandscapeActor;

    /** Exact inherited directional sun resolved by class plus stable actor tag. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Targets")
    TObjectPtr<ADirectionalLight> DirectionalSunActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Materials")
    TObjectPtr<UMaterialInterface> LawnMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Materials")
    TObjectPtr<UMaterialInterface> FountainWaterMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Materials")
    TObjectPtr<UMaterialInterface> HardscapeStoneMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Materials")
    TObjectPtr<UMaterialInterface> ContextRenderMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Materials")
    TObjectPtr<UMaterialInterface> ContextRoofMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Close Turf")
    TObjectPtr<UTexture2D> CloseTurfBaseColorTexture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Close Turf")
    TObjectPtr<UTexture2D> CloseTurfNormalTexture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Close Turf")
    TObjectPtr<UTexture2D> CloseTurfRoughnessTexture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Close Turf")
    TObjectPtr<UTexture2D> CloseTurfOpacityTexture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    bool bAppearanceOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    bool bGeometryOrTransformAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    bool bCollisionOrNavigationAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    bool bSurveyOrAsBuiltClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    bool bBotanicalInventoryClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    bool bSensorOrRfMaterialAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    bool bGoogleOrOneMapContentUsed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    bool bSunPoseColorOrIntensityModified = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    bool bInheritedSunPoseColorIntensityRecorded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    FTransform RecordedInheritedSunTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    FLinearColor RecordedInheritedSunColor = FLinearColor::White;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Truth")
    float RecordedInheritedSunIntensity = 0.0f;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5|Runtime")
    bool bRuntimeCloseTurfTextureRebindApplied = false;

protected:
    virtual void BeginPlay() override;

private:
    bool ValidatePersistentInputsAndTargets(FString& OutError) const;
    bool ReapplyExploreV5AppearanceWithExpectedContactShadowPreState(
        bool bRequireInheritedContactShadowPreState,
        FString& OutError);
    bool ApplyCloseTurfRuntimeTextureRebind(FString& OutError);
    void ApplyDeferredRuntimeBindings();
};
