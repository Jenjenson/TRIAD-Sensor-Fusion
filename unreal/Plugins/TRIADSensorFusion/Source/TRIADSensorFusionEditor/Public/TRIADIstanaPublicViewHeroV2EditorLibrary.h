#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaPublicViewHeroV2EditorLibrary.generated.h"

/**
 * Additive, fail-closed integration for the v2 Istana public-view hero.
 *
 * The v2 workflow owns only a new hero/material namespace and a new map copy.
 * It never overwrites the v1 map or assets, and migration changes only the
 * public-view scene actor's BuildingHeroVisualComponent. The v1 collision,
 * terrain, hardscape, context buildings, and vegetation remain authoritative.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaPublicViewHeroV2EditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Prove that a Remote Control caller reached the intended local project. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V2|Editor")
    static bool ValidateIstanaPublicViewHeroV2RemoteControlProject(
        const FString& ExpectedProjectPath,
        FString& OutReport);

    /**
     * Import the frozen hero mesh and HeroMaterialsV2 pack into new v2-only
     * namespaces. A complete valid set is accepted idempotently; any partial
     * or invalid existing set is rejected and never overwritten.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V2|Editor")
    static bool ImportIstanaPublicViewHeroV2Assets(FString& OutMessage);

    /** Read-only source-freeze and imported-asset validation. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V2|Editor")
    static bool ValidateIstanaPublicViewHeroV2Assets(FString& OutReport);

    /**
     * Duplicate the exact v1 public-view map to the new v2 destination and
     * replace only BuildingHeroVisualComponent on the duplicate.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V2|Editor")
    static bool MigrateIstanaPublicViewExteriorMapToHeroV2(FString& OutMessage);

    /** Validate the currently loaded exact v2 map and hero-only preservation. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V2|Editor")
    static bool ValidateIstanaPublicViewHeroV2Map(FString& OutReport);
};
