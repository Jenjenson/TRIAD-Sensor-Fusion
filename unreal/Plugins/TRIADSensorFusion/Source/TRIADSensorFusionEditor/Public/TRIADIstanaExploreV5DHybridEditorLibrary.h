#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DHybridEditorLibrary.generated.h"

/**
 * Builds and validates the additive V5D hybrid-visual map.  The map streams
 * the project's already licensed Cesium ion photorealistic context as a
 * visual-only layer while preserving every inherited deterministic simulation
 * layer.  Provider content is never exported, cached by this tool, traced,
 * analysed, baked, or admitted as RF/collision authority.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaExploreV5DHybridEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool BuildIstanaExploreV5DHybridMap(FString& OutMessage);

    /**
     * Add only the public-realm actor to the exact hash-pinned predecessor
     * map after creating and verifying a non-overwriting backup.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ApplyIstanaExploreV5DPublicRealmPassToLoadedHybridMap(
        FString& OutMessage);

    /**
     * Hash-gated, map-only appearance migration from the exact planning-
     * material predecessor to V5D-owned PBR asphalt component overrides.
     * It never modifies either mesh asset or the original migration backup.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ApplyIstanaExploreV5DPublicRealmMaterialCorrectionToLoadedHybridMap(
        FString& OutMessage);

    /**
     * Hash-gated, map-only successor migration that keeps the inherited V5C
     * planning-ground asset intact but serializes it fully hidden in V5D.
     * The exact R2 material-correction map is backed up and never overwritten.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ApplyIstanaExploreV5DInheritedPlanningGroundSuppressionToLoadedHybridMap(
        FString& OutMessage);

    /**
     * Hash-pinned, single-save visual-quality migration for the exact current
     * V5D predecessor. It updates Cesium DPI/hole quality, replaces only the
     * local fallback's two duplicate landmark shells with its admitted V1
     * derivative, and attaches the bounded outer-ground loading visual. It
     * preserves the geospatial/clip and all simulation-authority contracts,
     * then cold-validates or restores the verified non-overwriting backup.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ApplyIstanaExploreV5DProviderQualityPassToLoadedHybridMap(
        FString& OutMessage);

    /**
     * One-shot migration from the exact hash-pinned final V5D map to its
     * provider-throttled successor. The only serialized map deltas are
     * MaximumSimultaneousTileLoads 64 -> 12 and PreloadSiblings true -> false.
     * The migration is idempotent only when the predecessor backup remains
     * exact, and every failure after mutation cold-restores that backup.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ApplyIstanaExploreV5DProviderThrottleSuccessorToLoadedHybridMap(
        FString& OutMessage);

    /**
     * One-shot migration from the exact provider-throttle map receipt to the
     * exact LocalFallbackSuppressionV2 mesh. The sole map delta is the
     * CurrentSurroundingsRenderOnly static-mesh reference; V1 and canonical
     * assets remain immutable. Failures cold-restore the pinned predecessor.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ApplyIstanaExploreV5DLocalFallbackSuppressionV2ToLoadedHybridMap(
        FString& OutMessage);

    /** Validate the loaded map as the exact V2 render-only successor. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ValidateIstanaExploreV5DLocalFallbackSuppressionV2SuccessorMap(
        FString& OutReport);

    /**
     * Hash-gated map-only successor that keeps the exact V2 geometry and adds
     * only the exact 17-entry R25 context-facade material override roster.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ValidateIstanaExploreV5DContextFacadeR25SuccessorMap(
        FString& OutReport);

    /**
     * Hash-gated additive migration from the exact R25 façade successor. It
     * adds one deterministic, render-only vegetation actor around the two R24
     * landmarks while retaining the R25 façade roster and every simulation,
     * collision, navigation, sensor, RF and Cesium-provider boundary.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Apply Istana Explore V5D Landmark Vegetation R26 To Loaded Hybrid Map"))
    static bool ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap(
        FString& OutMessage);

    /** Validate the loaded map as the exact semantic R26 successor. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Validate Istana Explore V5D Landmark Vegetation R26 Successor Map"))
    static bool ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap(
        FString& OutReport);

    /**
     * Reconfigure the sole existing landmark-vegetation actor with the R27
     * distance-readable material/scale contract. The caller must supply the
     * exact current map receipt and a separately created, byte-identical
     * backup below Saved/TRIAD/NativeTransactions/V5DLandmarkVegetationR27V1.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Apply Istana Explore V5D Landmark Vegetation R27 Visual Correction"))
    static bool ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutMessage);

    /** Strict saved/cold-world validation for the R27 visual correction. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Validate Istana Explore V5D Landmark Vegetation R27 Successor Map"))
    static bool ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap(
        FString& OutReport);

    /**
     * Atomically promote the exact clean R27 map to the combined R28 visual
     * successor. The surroundings editor seam mutates the loaded world without
     * saving; the sole landmark actor is then reconfigured to R28, the complete
     * combined contract is validated, and this endpoint owns the one map save.
     * The supplied external backup must be byte-identical and reside below
     * Saved/TRIAD/NativeTransactions/V5DVisualRealismR28V1.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Apply Istana Explore V5D R28 Visual Successor"))
    static bool ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutMessage);

    /** Strict saved/cold-world validation for both R28 visual actors. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Validate Istana Explore V5D R28 Visual Successor Map"))
    static bool ValidateIstanaExploreV5DR28VisualSuccessorMap(
        FString& OutReport);

    /**
     * Add the exact R24 MacDonald House render-only landmark to a clean,
     * semantically validated V5D predecessor after creating a verified,
     * non-overwriting backup. The dedicated landmark stays visible across
     * provider transitions; provider readiness is telemetry only because no
     * landmark-specific exclusion/readiness proof exists yet.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ApplyIstanaExploreV5DMacDonaldHouseR24ToLoadedHybridMap(
        FString& OutMessage);

    /**
     * Add one exact R24 Temasek Shophouse render-only overlay after the
     * MacDonald successor. Provider/coarse-shell overlap stays explicit and
     * unresolved; global provider state is telemetry only.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ApplyIstanaExploreV5DTemasekShophouseR24ToLoadedHybridMap(
        FString& OutMessage);

    /** Validate the deliberate between-migrations Mac-only successor. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ValidateIstanaExploreV5DMacDonaldHouseR24SuccessorMap(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ValidateIstanaExploreV5DHybridMap(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ValidateIstanaExploreV5DHybridPlayWorld(FString& OutReport);

    /** R28-aware PIE validation; the legacy R27 endpoint remains unchanged. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Validate Istana Explore V5D R28 Visual Successor Play World"))
    static bool ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld(
        FString& OutReport);

    /**
     * Atomically create or cold-validate the exact two V5D-only fountain
     * materials. Partial, dirty, or mixed material state is refused.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool EnsureIstanaExploreV5DFountainRealismMaterialAssets(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool UpgradeIstanaExploreV5DRequiredNaniteMaterialUsage(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool ValidateIstanaExploreV5DRequiredNaniteMaterialUsage(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool GetIstanaExploreV5DHybridPlayStateReport(FString& OutReport);

    /** Exact Player0/provider state report using the combined R28 contract. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Get Istana Explore V5D R28 Visual Successor Play State Report"))
    static bool GetIstanaExploreV5DR28VisualSuccessorPlayStateReport(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool TeleportIstanaExploreV5DHybridPlayPawnForQa(
        FVector WorldLocationCentimeters,
        FRotator WorldRotationDegrees,
        FString& OutMessage);

    /**
     * Place the validated PIE Player0 camera at one exact proof pose without
     * changing any persistent actor, map, simulation, RF, or provider state.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool SetIstanaExploreV5DHybridPlayViewPoseForQa(
        FVector WorldViewLocationCentimeters,
        FRotator WorldViewRotationDegrees,
        FString& OutMessage);

    /** Place Player0 using only the combined R28 world contract. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Set Istana Explore V5D R28 Visual Successor Play View Pose For QA"))
    static bool SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa(
        FVector WorldViewLocationCentimeters,
        FRotator WorldViewRotationDegrees,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool CaptureIstanaExploreV5DHybridPlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    /** Provider-ready Player0 capture gated by the combined R28 contract. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Capture Istana Explore V5D R28 Visual Successor Play View"))
    static bool CaptureIstanaExploreV5DR28VisualSuccessorPlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    /**
     * Capture an exact binary visible-grass semantic matte from the validated
     * Player0 view. The transient two-depth-pass capture renders the original
     * R23 masked materials and never changes a material, component, map,
     * custom-depth/stencil, simulation, sensor, or RF state.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    /**
     * Capture provider-ready, explicitly non-proof attribution evidence with
     * only the exact public-realm core excluded from Player0's view. The
     * component itself and all simulation/RF state remain unchanged.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool CaptureIstanaExploreV5DHybridPublicRealmCoreHiddenDiagnosticPlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    /**
     * Capture provider-ready, explicitly non-proof attribution evidence with
     * only the exact V5D ground macro overlay excluded from Player0's view.
     * The component itself and all simulation/RF state remain unchanged.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool CaptureIstanaExploreV5DHybridGroundMacroOverlayHiddenDiagnosticPlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor")
    static bool CaptureIstanaExploreV5DHybridDiagnosticPlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    /**
     * R28 fallback-state Player0 evidence. The R28 environment remains visible
     * in the game viewport while its renderers remain excluded from sensor
     * SceneCapture feeds.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Editor",
        meta = (DisplayName = "Capture Istana Explore V5D R28 Visual Successor Diagnostic Play View"))
    static bool CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView(
        const FString& OutputFileName,
        FString& OutMessage);
};
