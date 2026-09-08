#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.generated.h"

class UWorld;

/** Editor-only creation, application and validation for the V5D visual pass. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DGroundVegetationEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * C++-only exact validator for an isolated derivative of one canonical
     * R23B grass profile. The complete render-critical 32-node graph,
     * calibrated expression values and connections, inactive material outputs
     * and compiled shader state remain under the canonical R23B contract; only
     * the stable-visibility node description, code and object path may use the
     * supplied derivative values.
     */
    static bool ValidateExactR23BDerivativeGrassMaterial(
        const FString& MaterialObjectPath,
        int32 ProfileIndex,
        const FString& StableVisibilityDescription,
        const FString& StableVisibilityCode,
        bool bRequireSaved,
        FString& OutReport);

    /**
     * Exact validator for a visually calibrated R23B derivative. The complete
     * 32-node graph and all render-critical state remain canonical; only the
     * object path, stable-visibility custom node, and blade-colour custom node
     * may use the supplied replacement descriptions/codes. This is the narrow
     * R28 seam used to correct the independently observed straw-biased
     * landmark turf without weakening validation of the protected R23B or R27
     * source packages.
     */
    static bool ValidateExactR23BVisualDerivativeGrassMaterial(
        const FString& MaterialObjectPath,
        int32 ProfileIndex,
        const FString& StableVisibilityDescription,
        const FString& StableVisibilityCode,
        const FString& BladeColorDescription,
        const FString& BladeColorCode,
        bool bRequireSaved,
        FString& OutReport);

    /**
     * Create or cold-validate the isolated material namespace, then add exactly
     * one ground/vegetation actor to the already-loaded V5D hybrid map. The
     * caller retains the existing map-save boundary.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool ApplyGroundVegetationRealismPassToLoadedV5DHybridMap(
        FString& OutMessage);

    /**
     * C++-only atomic build hook. The explicit trust flag admits only an
     * untitled temp-package world owned by the V5D hybrid builder, allowing
     * this pass to fail before that builder performs its first target save.
     * The Blueprint hook above remains exact-target-package-only.
     */
    static bool ApplyGroundVegetationRealismPassToWorldForTrustedHybridBuilder(
        UWorld* InWorld,
        bool bTrustedUntitledHybridBuilder,
        FString& OutMessage);

    /**
     * Read-only current-material gate: validates complete R23 first, then the
     * deliberate complete R22, R21 and R19 fallbacks, and requires the actor's
     * independently derived runtime material revision to agree.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool ValidateGroundVegetationRealismPassInLoadedV5DHybridMap(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool EnsureGroundVegetationRealismMaterialAssets(
        FString& OutReport);

    /**
     * Atomic populated-namespace migration for only the four fine-turf proxy
     * profiles and the lawn overlay. Requires the complete valid six-material
     * roster, preserves soil and map package hashes, backs up every package
     * artifact/absent sidecar, and rolls all five targets back together.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismMaterialAssetsToR10(
        FString& OutReport);

    /**
     * Atomic R10-to-R12 response upgrade for the same exact four fine-turf
     * profiles and lawn overlay. The valid clean R11 edge derivative is an
     * additional immutable hash guard; soil, edge and map packages are never
     * offered to save. Uniform R10 and idempotent R12 rosters are admitted,
     * while mixed-version or partial state is refused.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismMaterialAssetsToR12(
        FString& OutReport);

    /**
     * Atomic R12-to-R13 hyperreal response upgrade for exactly the four
     * fine-turf profiles and lawn overlay. Soil, R11 edge fade and target map
     * remain immutable hash guards. Uniform clean R12 and idempotent R13
     * rosters are admitted; mixed or partial state is refused.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismMaterialAssetsToR13(
        FString& OutReport);

    /**
     * Atomic R13-to-R15 shaded-lawn response upgrade for exactly the four
     * fine-turf profiles and lawn overlay. Soil, the R11 edge derivative,
     * source assets and target map remain immutable hash guards. Uniform clean
     * R13 and idempotent R15 rosters are admitted; mixed state is refused.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismMaterialAssetsToR15(
        FString& OutReport);

    /**
     * Atomic one-package R15-to-R16 seam-cover migration for only
     * M_IPV5D_LawnMacroVariation. The four R15 grass profiles, soil, R11
     * edge fade and target map are immutable artifact/hash guards. The entry
     * point admits exactly clean R15 input or an idempotent clean R16 target;
     * no map, source asset, collision, navigation, sensor, or RF state is
     * offered for mutation.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismLawnOverlayToR16(
        FString& OutReport);

    /**
     * Atomic R16-to-R17 photographic managed-turf material migration for the
     * four grass profiles and lawn overlay only. It hash-guards soil, the R11
     * edge fade, target map, and V3/V4/V5B source assets; structural instance
     * transforms remain map-owned and are intentionally not changed here.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismMaterialAssetsToR17(
        FString& OutReport);

    /**
     * Atomic one-package R17-to-R18 calibrated-lawn migration for only
     * M_IPV5D_LawnMacroVariation. The four exact R17 grass packages, soil,
     * R11 edge fade, source assets, textures, target map, and every physical
     * simulation invariant remain immutable artifact/hash guards.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismLawnOverlayToR18(
        FString& OutReport);

    /**
     * Atomic four-package R18-to-R19 fine-turf response migration. Only the
     * four V5D grass materials are changed in place; the exact R18 lawn
     * overlay, soil, R11 edge fade, source materials/textures and target map
     * remain immutable artifact/hash guards. Uniform clean R18 input and
     * idempotent R19 state are admitted, while mixed grass revisions fail
     * closed without offering any map or physical-simulation state to save.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismGrassMaterialsToR19(
        FString& OutReport);

    /**
     * Atomic map-only R14-to-R20 migration for the serialized fine-turf
     * layout. Requires the exact clean, hash-pinned target map and current
     * seven-material R19 roster, backs up all six possible map artifacts,
     * rebuilds the one tagged exact-class actor in place, saves only the
     * target map, then unloads through V5B and cold-validates the result.
     * Every post-backup failure restores the exact predecessor artifacts and
     * cold-reloads them before returning false.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationSerializedPresentationToR20(
        FString& OutReport);

    /**
     * Atomic R19-to-R21 calibrated fine-turf material migration after the
     * serialized R20 layout is present. The entry point admits only the four
     * exact hash-pinned R19 predecessor packages or an idempotent uniform R21
     * graph. It changes those four existing materials in place, retains the
     * 29-expression/zero-texture topology and full upstream calm-wind WPO
     * fingerprint, and hash-guards the R20 map, R18 overlay, soil, edge fade
     * and all sources. R21 alone backs up and rolls back seven artifact states
     * per target, including .upayload; legacy transactions remain six-state.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismGrassMaterialsToR21(
        FString& OutReport);

    /**
     * Atomic six-package R21/R18/R11-to-R22 grass-system migration. The four
     * fine-turf materials, lawn overlay, and owned edge-fade material change
     * together; soil, source assets, Grass004 textures, the serialized R20
     * map, transforms, collision, navigation, and sensor/RF authority remain
     * immutable. Exact predecessor hashes or a uniform idempotent R22 graph
     * are the only admitted states.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismGrassSystemToR22(
        FString& OutReport);

    /**
     * Atomic six-package R22-to-R23 photographic-depth grass-system
     * migration. Mutation requires and preserves the exact R20 predecessor;
     * uniform R23 idempotence admits only that pin or the exact serialized R23
     * successor. Only the four fine-turf response graphs, lawn overlay response,
     * and owned edge fade can change. Stable world-space visibility extends to
     * 20-28 m; source assets, soil, transforms, collision, navigation, and
     * sensor/RF authority are immutable.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismGrassSystemToR23(
        FString& OutReport);

    /**
     * Atomic six-package R23-to-R23B photographic material calibration. The
     * serialized R23 map/layout, soil, source assets, transforms, collision,
     * navigation, and sensor/RF authority remain immutable. Mutation admits
     * only the exact six canonical R23 package pins; R23B retains runtime
     * presentation revision 23 and adds a connected material-calibration
     * revision marker of one. The first serialization may only bootstrap
     * measured final pins and is not publication-ready until those pins are
     * sealed in source and an idempotent cold validation succeeds.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationRealismGrassSystemToR23B(
        FString& OutReport);

    /**
     * Atomic map-only R20-to-R23 migration for the serialized photographic
     * fine-turf layout. It requires the exact clean R20 map predecessor and
     * exact R23 material/soil pins, backs up all six possible map artifacts,
     * rebuilds the one tagged exact-class actor in place, saves only the target
     * map, then unloads and cold-validates the R23 result. Every post-backup
     * failure restores and cold-reloads the exact R20 predecessor map; success
     * and idempotence require the exact pinned R23 serialized artifact.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationSerializedPresentationToR23(
        FString& OutReport);

    /**
     * Separate atomic R11 creation of the V5D-owned edge-card fade material.
     * Six uniformly clean R13, R15 or R16 core packages, the exact V4 source and
     * target map are immutable hash guards; only the initially absent seventh
     * package may be saved.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool UpgradeGroundVegetationEdgeGrassFadeAssetToR11(
        FString& OutReport);
};
