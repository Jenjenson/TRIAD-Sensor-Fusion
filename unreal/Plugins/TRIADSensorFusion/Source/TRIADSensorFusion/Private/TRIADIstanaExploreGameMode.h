#pragma once

#include "CoreMinimal.h"
#include "TRIADIstanaAirSimGameMode.h"
#include "TRIADIstanaExploreGameMode.generated.h"

/**
 * Additive game mode for interactive Istana Explore maps.
 *
 * Existing acceptance/capture maps retain ATRIADIstanaAirSimGameMode and their
 * fixed cameras. Only a map that explicitly selects this class receives the
 * collision-aware free-roam pawn.
 */
UCLASS()
class TRIADSENSORFUSION_API ATRIADIstanaExploreGameMode
    : public ATRIADIstanaAirSimGameMode
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreGameMode(const FObjectInitializer& ObjectInitializer);

    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
