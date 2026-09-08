#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TRIADIstanaFreeRoamPawn.generated.h"

class APlayerController;
class UCameraComponent;
class UCapsuleComponent;

/**
 * Collision-aware free-roam camera for the additive Istana Explore map.
 *
 * Controls are deliberately polled without project Input mappings:
 *   - W/S: move forward/back on the local horizontal plane
 *   - A/D: strafe left/right
 *   - E/Q: move up/down
 *   - Mouse: look
 *   - Shift: boost; Ctrl: precision/slow movement (Ctrl wins if both are held)
 *
 * Escape and Shift+F1 are intentionally not bound or consumed here, leaving
 * Unreal Editor's stop-PIE and release-mouse behavior intact. Travel is bounded
 * around the pawn's spawn point and within a configurable local-world altitude
 * band so an Explore session cannot accidentally leave the Istana context.
 */
UCLASS(Blueprintable, BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaFreeRoamPawn : public APawn
{
    GENERATED_BODY()

public:
    ATRIADIstanaFreeRoamPawn(const FObjectInitializer& ObjectInitializer);

    virtual void Tick(float DeltaSeconds) override;
    virtual void PossessedBy(AController* NewController) override;

    /** Camera owned by this pawn and used as Player 0's Explore view. */
    UCameraComponent* GetExploreCameraComponent() const;

    /** True only when the camera retains the deterministic public-view exposure profile. */
    bool HasExpectedExploreCameraProfile() const;

protected:
    virtual void BeginPlay() override;

private:
    void ConfigureLocalPlayer(APlayerController* PlayerController);
    void EnsureLocalPlayerViewTarget(APlayerController* PlayerController);
    void ApplyMouseLook(APlayerController* PlayerController);
    void ApplyMovement(APlayerController* PlayerController, float DeltaSeconds);
    FVector ClampToTravelVolume(const FVector& DesiredLocation) const;
    void MoveWithCollisionAndSlide(const FVector& DesiredDelta);

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Components",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCapsuleComponent> CollisionCapsule;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Components",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> ExploreCamera;

    /** Human-readable controls shown on the pawn defaults for map authors. */
    UPROPERTY(
        VisibleDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Controls",
        meta = (AllowPrivateAccess = "true", MultiLine = "true"))
    FString ControlReference =
        TEXT("WASD move | Mouse look | E/Q up/down | Shift boost | Ctrl slow | "
             "Escape stop PIE | Shift+F1 release mouse");

    /** Normal translation speed; Unreal distances are centimetres. */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Movement",
        meta = (AllowPrivateAccess = "true", ClampMin = "100.0", UIMin = "100.0"))
    float BaseMovementSpeedCentimetersPerSecond = 1200.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Movement",
        meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
    float BoostSpeedMultiplier = 4.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Movement",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.05", ClampMax = "1.0"))
    float SlowSpeedMultiplier = 0.25f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Look",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.01", ClampMax = "2.0"))
    float MouseSensitivityDegreesPerCount = 0.12f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Look",
        meta = (AllowPrivateAccess = "true", ClampMin = "-89.0", ClampMax = "0.0"))
    float MinimumViewPitchDegrees = -85.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Look",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.0"))
    float MaximumViewPitchDegrees = 85.0f;

    /** Maximum horizontal distance from the pawn's BeginPlay location. */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Travel Limits",
        meta = (AllowPrivateAccess = "true", ClampMin = "100.0", ClampMax = "950.0"))
    float MaximumHorizontalTravelMeters = 950.0f;

    /** Minimum camera-pawn height above the map's local Z=0 plane. */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Travel Limits",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.5", ClampMax = "20.0"))
    float MinimumLocalAltitudeMeters = 1.5f;

    /** Maximum camera-pawn height above the map's local Z=0 plane. */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "TRIAD|Istana Explore|Travel Limits",
        meta = (AllowPrivateAccess = "true", ClampMin = "20.0", ClampMax = "500.0"))
    float MaximumLocalAltitudeMeters = 300.0f;

    FVector TravelAnchorLocation = FVector::ZeroVector;
    float ViewYawDegrees = 0.0f;
    float ViewPitchDegrees = 0.0f;
    bool bTravelAnchorInitialized = false;
};
