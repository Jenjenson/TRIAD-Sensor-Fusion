#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaPublicViewSceneActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Isolated visual scene for the April 2024 public-view reconstruction.
 *
 * This actor never loads or references the legacy Istana or DigitalTwin
 * namespaces. Its terrain and every tree archetype must have non-zero extent
 * on all three axes; whole-tree billboards and image planes are not accepted.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaPublicViewSceneActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaPublicViewSceneActor();

    /** Assign the complete, separately imported public-view source set. */
    bool ConfigurePublicViewAssets(
        UStaticMesh* InBuildingHero,
        UStaticMesh* InBuildingCollision,
        UStaticMesh* InTerrain,
        UStaticMesh* InTerrainSkirt,
        UStaticMesh* InHardscape,
        UStaticMesh* InContextBuildings,
        UStaticMesh* InOsmContextBuildings,
        UStaticMesh* InRainTree,
        UStaticMesh* InPalmTree,
        UStaticMesh* InFramingTree,
        FString& OutError);

    /**
     * Atomically opt into the exact V5C identity-transform render successors.
     * The legacy ODbL mesh is restored before both candidates are validated
     * and on every failure. The building massing and official planning ground
     * graphic are inseparable presentation siblings; neither gains collision,
     * navigation, survey, sensor or RF authority.
     */
    bool ConfigureV5CSurroundingsPresentation(
        UStaticMesh* InSurroundingsV5C,
        UStaticMesh* InGroundContextV5C,
        FString& OutError);

    /** Restore legacy ODbL and empty both V5C presentation siblings. */
    void RestoreLegacyOsmSurroundingsPresentationFailSafe();

    /** Validate the two-state legacy/atomic-V5C presentation. */
    bool ValidateV5CSurroundingsPresentation(FString& OutReport) const;

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana Explore V5C|Surroundings")
    bool IsV5CSurroundingsPresentationActive() const
    {
        return bV5CSurroundingsPresentationActive;
    }

    /** Remove only this actor's deterministic vegetation instances. */
    void ClearVolumetricTreeInstances();

    /** Add one local-space instance of a complete three-dimensional tree. */
    bool AddVolumetricTreeInstance(
        const FName& Archetype,
        const FTransform& LocalTransform,
        FString& OutError);

    /** Fail-closed readback used before and after saving the isolated map. */
    bool ValidatePublicViewScene(
        FString& OutReport,
        bool bRequireLegacyTreeInstances = true) const;

    static const FString& ExpectedClaimLabel();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Building")
    TObjectPtr<UStaticMeshComponent> BuildingHeroVisualComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Building")
    TObjectPtr<UStaticMeshComponent> BuildingCollisionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    TObjectPtr<UStaticMeshComponent> TerrainComponent;

    /** Synthetic visual-only downward closure at the exact one-kilometre edge. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    TObjectPtr<UStaticMeshComponent> TerrainSkirtComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    TObjectPtr<UStaticMeshComponent> HardscapeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    TObjectPtr<UStaticMeshComponent> ContextBuildingsComponent;

    /**
     * ODbL-derived, mapping-grade outer context. This is visible lookdev
     * massing only: it never supplies collision, navigation, or sensor truth.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    TObjectPtr<UStaticMeshComponent> OSMContextBuildingsComponent;

    /**
     * Hash-pinned official-preferred/fallback V5C outer-context successor.
     * Kept as a separate sibling so the legacy ODbL presentation remains an
     * immediate, deterministic rollback state.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings")
    TObjectPtr<UStaticMeshComponent> V5CSurroundingsRenderOnlyComponent;

    /**
     * Official-source planning ground graphic paired atomically with the V5C
     * building context. It is a visual-only sibling, never terrain or roads.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Ground Context")
    TObjectPtr<UStaticMeshComponent> V5CGroundContextRenderOnlyComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Truth")
    bool bV5CSurroundingsPresentationActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Truth")
    bool bV5CSurroundingsRenderOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Truth")
    bool bV5CSurroundingsCollisionNavigationSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Truth")
    bool bV5CSurroundingsMeasuredHeightClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Truth")
    bool bV5CSurroundingsFacadeOrRoofFormClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Truth")
    bool bV5CSurroundingsTerrainGradeOrFoundationClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Truth")
    bool bV5CSurroundingsVisualAcceptanceClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Ground Context|Truth")
    bool bV5CGroundContextRenderOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Ground Context|Truth")
    bool bV5CGroundContextRoadWidthOrMaterialClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Ground Context|Truth")
    bool bV5CGroundContextElevationOrSurveyClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Ground Context|Truth")
    bool bV5CGroundContextCollisionNavigationSensorOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Sources")
    FString FrozenV5CSurroundingsObjSha256 =
        TEXT("774F7E30456B989D0BF9EEB013C10D2688A2B3DE87936529BBB154E83D344C74");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Sources")
    FString FrozenV5CSurroundingsMtlSha256 =
        TEXT("6BBDA30E125F92EEF36D060404CA6EBB3F7DFC9DF95D7895766A57A99A737CA7");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Sources")
    FString FrozenV5CSurroundingsFeaturesSha256 =
        TEXT("8825DDC93E7C6465B01AD5A5AD23B2E8368F2C825B6F8F8EB1FBBD2DA8138793");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Sources")
    FString FrozenV5CSurroundingsManifestSha256 =
        TEXT("CEBFA56EC84E697305A35DA9CCD06B29619AB4500701B4A806B958B415F04F20");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Surroundings|Sources")
    FString FrozenV5CSurroundingsContractSha256 =
        TEXT("52587014FC80459732B1C943056D1724684287B67CA0DE8556AF7E1E2F8C7FBD");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Vegetation")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RainTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Vegetation")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PalmTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Vegetation")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FramingTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    FString ClaimLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    FString ReferenceEpoch = TEXT("2024-04");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    bool bSurveyControlled = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    bool bArbitraryViewIndistinguishabilityClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    bool bPhotorealMaterialAcceptanceClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    bool bPbrTexturePackIntegrated = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    FString MaterialImplementationTier =
        TEXT("ORIGINAL_AI_ASSISTED_PBR_TEXTURES_INTEGRATED_PHOTO_QA_NOT_ACCEPTED");

    /** OCIO/display transforms remain an external acceptance-lab responsibility. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    FString DisplayColorPolicy =
        TEXT("NEUTRAL_CAMERA_LOCAL_EXPOSURE_DISABLED_EXTERNAL_SRGB_REC709_DISPLAY_NOT_OCIO_VALIDATED");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    bool bExternalOcioDisplayValidated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    FString PhotoQaEvidenceStatus =
        TEXT("NOT_READY_UNCALIBRATED_COMMONS_EVIDENCE_METADATA_ONLY");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Claim")
    bool bPhotoQaEvidenceConsumedByMap = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    bool bContainsWholeTreeBillboards = false;

    /** Synthetic visual context must never create false sensor occlusion. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    FString SensorOcclusionPolicy =
        TEXT("SYNTHETIC_CONTEXT_AND_VEGETATION_VISUAL_ONLY_NO_COLLISION");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    FString TerrainBoundaryPolicy =
        TEXT("SYNTHETIC_DOWNWARD_VISUAL_ONLY_SKIRT_NO_COLLISION");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    FString OSMContextIntegrationStatus =
        TEXT("COORDINATE_CONTRACT_ALIGNED_MAPPING_GRADE_NOT_VISUALLY_OR_PHOTO_ACCEPTED");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    FString OSMContextAttribution = TEXT("© OpenStreetMap contributors; ODbL-1.0");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    bool bOSMContextUsedForSensorTruth = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    bool bSyntheticContextBuildingsEnabled = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    float ContextRadiusMeters = 1000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    int32 MinimumRequiredTreeInstances = 600;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    int32 MinimumRequiredHeroTreeInstances = 120;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Public View|Context")
    float HeroTreeRadiusMeters = 250.0f;

};
