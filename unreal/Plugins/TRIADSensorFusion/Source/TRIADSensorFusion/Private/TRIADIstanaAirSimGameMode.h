#pragma once

#include "CoreMinimal.h"
#include "AirSimGameMode.h"
#include "TRIADIstanaAirSimGameMode.generated.h"

/**
 * TRIAD-owned serialization boundary for the Istana runtime map.
 *
 * The host project has repeatedly stripped a direct
 * /Script/AirSimTriadRuntime.AirSimGameMode reference from World Settings
 * during a map save/reload round trip. This class deliberately changes no
 * gameplay behavior: it only gives the map a class in the same runtime module
 * as its other persisted TRIAD actors while inheriting AirSimGameMode intact.
 */
UCLASS()
class TRIADSENSORFUSION_API ATRIADIstanaAirSimGameMode : public AAirSimGameMode
{
    GENERATED_BODY()

public:
    ATRIADIstanaAirSimGameMode(const FObjectInitializer& ObjectInitializer);
};
