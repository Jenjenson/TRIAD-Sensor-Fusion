#pragma once

#include "CoreMinimal.h"
#include "TRIADIstanaFreeRoamPawn.h"
#include "TRIADIstanaExploreV5Pawn.generated.h"

/**
 * V5-only free-roam camera that retains the neutral Explore camera profile and
 * explicitly selects Screen Space GI and Screen Space Reflections.
 */
UCLASS(Blueprintable, BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5Pawn
    : public ATRIADIstanaFreeRoamPawn
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5Pawn(const FObjectInitializer& ObjectInitializer);

    /** Exact V4 neutral camera plus the bounded V5 SSGI/SSR overrides. */
    bool HasExpectedExploreV5CameraProfile() const;

    static float ExpectedScreenSpaceReflectionIntensity();
    static float ExpectedScreenSpaceReflectionQuality();
    static float ExpectedScreenSpaceReflectionMaxRoughness();

protected:
    virtual void BeginPlay() override;

private:
    void ApplyExploreV5ScreenSpaceProfile();
};
