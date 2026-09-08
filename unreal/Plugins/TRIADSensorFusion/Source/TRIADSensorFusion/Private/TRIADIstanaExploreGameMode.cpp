#include "TRIADIstanaExploreGameMode.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "TRIADIstanaFreeRoamPawn.h"

ATRIADIstanaExploreGameMode::ATRIADIstanaExploreGameMode(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DefaultPawnClass = ATRIADIstanaFreeRoamPawn::StaticClass();
    // The Explore map is a visual navigation mode, not a sensor simulation.
    // Disabling AirSim's HUD prevents SimMode from spawning a ComputerVision
    // pawn and stealing Player 0 possession after the free-roam pawn starts.
    // All V1-V5 simulation/capture maps retain the original AirSim HUD.
    HUDClass = nullptr;
}

AActor* ATRIADIstanaExploreGameMode::ChoosePlayerStart_Implementation(
    AController* Player)
{
    static const FName ExploreStartTag(TEXT("TRIADIstanaExplorePlayerStartV1"));
    APlayerStart* ExactStart = nullptr;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        if (!It->Tags.Contains(ExploreStartTag))
        {
            continue;
        }
        if (ExactStart)
        {
            return Super::ChoosePlayerStart_Implementation(Player);
        }
        ExactStart = *It;
    }
    return ExactStart
        ? ExactStart
        : Super::ChoosePlayerStart_Implementation(Player);
}
