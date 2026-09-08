#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TRIADIstanaExploreV5GameMode.generated.h"

/**
 * V5-only visual Explore game mode. It selects the exact tagged Explore start
 * and prevents HUD-driven sensor pawn possession from replacing the V5
 * free-roam camera.
 */
UCLASS()
class TRIADSENSORFUSION_API ATRIADIstanaExploreV5GameMode
    : public AGameModeBase
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV5GameMode(
        const FObjectInitializer& ObjectInitializer);

    virtual AActor* ChoosePlayerStart_Implementation(
        AController* Player) override;

    static FName ExpectedExplorePlayerStartTag();

    /** Pure fail-closed selector used by runtime code and focused tests. */
    static int32 FindUniqueExplorePlayerStartIndex(
        const TArray<TArray<FName>>& CandidateActorTags,
        int32& OutMatchingStartCount);
};
