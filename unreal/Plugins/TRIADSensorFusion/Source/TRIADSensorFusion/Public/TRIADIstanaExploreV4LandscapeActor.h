#pragma once

#include "CoreMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaExploreV4LandscapeActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** One-shot startup signal emitted after the complete V4 wind MID roster is live. */
DECLARE_MULTICAST_DELEGATE(FTRIADIstanaExploreV4WindRuntimeReady);

/**
 * V4-only HISM whose loaded cluster tree is finalized before validation returns.
 * Density scaling is deliberately unavailable so a later foliage CVar sink
 * cannot reintroduce an asynchronous visual-census change.
 */
UCLASS()
class TRIADSENSORFUSION_API UTRIADIstanaExploreV4SynchronousHismComponent final
    : public UHierarchicalInstancedStaticMeshComponent
{
    GENERATED_BODY()

public:
    explicit UTRIADIstanaExploreV4SynchronousHismComponent(
        const FObjectInitializer& ObjectInitializer);

protected:
    virtual void OnPostLoadPerInstanceData() override;
};

/** Five deliberately broad visual habits; none is a botanical identification. */
UENUM(BlueprintType)
enum class ETRIADIstanaExploreV4TreeForm : uint8
{
    Umbrella,
    Dome,
    HighForkRounded,
    ColumnarNarrow,
    Palm,
    Count UMETA(Hidden)
};

/** Broad render-proxy bands derived from the frozen build-time raster grid. */
UENUM(BlueprintType)
enum class ETRIADIstanaExploreV4CanopyHeightBand : uint8
{
    NoData = 0,
    Low18To23Meters = 1,
    Mid23To29Meters = 2,
    High29To37Meters = 3
};

/** Parallel cue for one inherited V2 visual transform. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV4RasterCue
{
    GENERATED_BODY()

    /** Frozen WorldCover value (0, 10, 30, 50, 60 or 80), not a tree label. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    uint8 BroadRasterClass = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    ETRIADIstanaExploreV4CanopyHeightBand HeightBand =
        ETRIADIstanaExploreV4CanopyHeightBand::NoData;
};

/** Sanitised public anchor metadata. No geographic coordinates or source URL are retained. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV4HeritageAnchor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    FString PublicRecordId;

    /** Registered project-local ground point in Unreal centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    FVector LocalGroundLocationCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    float PublishedHeightMeters = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    float PublishedGirthMeters = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    ETRIADIstanaExploreV4TreeForm FormHint =
        ETRIADIstanaExploreV4TreeForm::Dome;

    /** False for the two records whose public profile states no form hint. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    bool bFormComesFromPublishedQualitativeHint = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    bool bPositionComesFromPublishedRecord = true;

    /** Must remain true unless a later, separately licensed species-exact asset is proven. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    bool bVisualIsSilhouetteProxy = true;
};

/** Complete V4-owned mesh roster. Existing terrain, blockers and HDBs stay external. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV4AssetRoster
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> UmbrellaTree = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> DomeTree = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> HighForkRoundedTree = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> ColumnarNarrowTree = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> PalmTree = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> HeritagePawnBlockerCylinder = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> Shrub = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> Flower = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> Understorey = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> GeometryGrass = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> CloseTurf = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UStaticMesh> PorticoV8 = nullptr;
};

/** One persistent base material to one component slot; every runtime MID is unique. */
UENUM(BlueprintType)
enum class ETRIADIstanaExploreV4WindRole : uint8
{
    UmbrellaTrunk,
    UmbrellaBranch,
    UmbrellaLeaf,
    DomeTrunk,
    DomeBranch,
    DomeLeaf,
    HighForkTrunk,
    HighForkBranch,
    HighForkLeaf,
    ColumnarTrunk,
    ColumnarBranch,
    ColumnarLeaf,
    /** The admitted palm source has one opaque material slot. */
    PalmComposite,
    Shrub,
    Flower,
    Understorey,
    GeometryGrass,
    /** Animated geometry cards only; the external flat lawn surface has no WPO. */
    CloseTurf,
    Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV4WindMaterialBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    ETRIADIstanaExploreV4WindRole Role =
        ETRIADIstanaExploreV4WindRole::UmbrellaTrunk;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    int32 MaterialSlotIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<UMaterialInterface> BaseMaterial = nullptr;

    /** Explicit authoring assertion required by the V4 instancing contract. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    bool bUsesPerInstanceLocalPosition = false;
};

/**
 * Single deterministic population handoff from the editor-side registered
 * grid. Every transform array is world-space and requires an identity actor.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV4PopulationInput
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    /** Exact component-union order: V2 umbrella 272, columnar 232, dome 182, palm 34. */
    TArray<FTransform> InheritedV2TreeWorldTransforms;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TArray<FTRIADIstanaExploreV4RasterCue> RasterCues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TArray<FTRIADIstanaExploreV4HeritageAnchor> HeritageAnchors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TArray<FTransform> ShrubTransforms;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TArray<FTransform> FlowerTransforms;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TArray<FTransform> UnderstoreyTransforms;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TArray<FTransform> GeometryGrassTransforms;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana Explore V4")
    TArray<FTransform> CloseTurfTransforms;
};

/** Pure deterministic row used by runtime population and focused automation tests. */
struct TRIADSENSORFUSION_API FTRIADIstanaExploreV4ClassifiedTreeRow
{
    int32 SourceIndex = INDEX_NONE;
    ETRIADIstanaExploreV4TreeForm Form = ETRIADIstanaExploreV4TreeForm::Dome;
    FTransform WorldTransform = FTransform::Identity;
};

/**
 * Additive Explore V4 public-landscape actor.
 *
 * It reclassifies exactly 720 inherited V2 visual locations into four admitted
 * runtime morphology proxies, retains a fifth low-poly palm component/asset as
 * explicitly unused in free-roam, adds nine Heritage anchors and bounded
 * understorey, owns the frozen render-only V8 central-portico successor, and
 * exposes an optional fail-safe V5C render-only sibling. It does not own
 * terrain, the inherited 720 Pawn blockers, buildings, or sensor truth.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaExploreV4LandscapeActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaExploreV4LandscapeActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLoad() override;

    bool ConfigureRequiredAssets(
        const FTRIADIstanaExploreV4AssetRoster& Assets,
        const TArray<FTRIADIstanaExploreV4WindMaterialBinding>& WindBindings,
        FString& OutError);

    bool PopulateDeterministicLandscape(
        const FTRIADIstanaExploreV4PopulationInput& Input,
        FString& OutError);

    /** Persist the target-map checks that necessarily remain outside this actor. */
    bool RecordExternalTargetMapState(
        int32 PreservedV2PawnBlockerCount,
        bool bV2TreeVisualsHidden,
        bool bV3ReplacementTreeVisualsHidden,
        bool bV3CloseTurfVisualsHidden,
        bool bV7PorticoHidden,
        bool bInheritedTerrainAndCollisionPreserved,
        bool bDistantBuildingsPreserved,
        FString& OutError);

    /** Pure deterministic classifier shared by population and automation tests. */
    static bool BuildDeterministicReclassifiedRows(
        const TArray<FTransform>& InheritedTransforms,
        const TArray<FTRIADIstanaExploreV4RasterCue>& RasterCues,
        const TArray<float>& NativeTreeHeightsCm,
        int32 Seed,
        TArray<FTRIADIstanaExploreV4ClassifiedTreeRow>& OutRows,
        FString& OutError);

    /** Exact-pose gate: scale is intentionally excluded for rescaled non-columnar forms. */
    static bool PreservesSourceTranslationAndRotation(
        const FTransform& Candidate,
        const FTransform& Source,
        double TranslationToleranceCm = 0.0,
        double RotationComponentTolerance = 0.0);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V4|Wind")
    void TriggerWindGust(float PeakStrengthCm = -1.0f);

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana Explore V4|Wind")
    bool IsWindRuntimeActive() const;

    FTRIADIstanaExploreV4WindRuntimeReady& OnWindRuntimeReady()
    {
        return WindRuntimeReadyEvent;
    }

    /** Deliberately excludes positions, source URLs, and geographic coordinates. */
    FString BuildWindRuntimeStateReport() const;

    bool ValidateExploreV4Landscape(FString& OutReport) const;

    /**
     * Atomically opt into the exact V5C identity-transform render successor.
     * The method restores visible V8 first and leaves that fail-safe state in
     * place on every validation failure.
     */
    bool ConfigurePorticoV5CPresentation(
        UStaticMesh* InPorticoV5C,
        FString& OutError);

    /** Restore the frozen V8 presentation without mutating either mesh. */
    void RestorePorticoV8PresentationFailSafe();

    /** Validate only the two-state V8/V5C presentation contract. */
    bool ValidatePorticoV5CPresentation(FString& OutReport) const;

    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana Explore V5C|Portico")
    bool IsPorticoV5CPresentationActive() const
    {
        return bPorticoV5CPresentationActive;
    }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> UmbrellaTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DomeTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HighForkRoundedTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ColumnarNarrowTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Trees")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PalmTreeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Heritage")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HeritageUmbrellaInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Heritage")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HeritageDomeInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Heritage")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HeritageHighForkRoundedInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Heritage")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HeritageColumnarNarrowInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Heritage")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HeritagePalmInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Heritage")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HeritageAnchorPawnBlockers;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Planting")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Planting")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FlowerInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Planting")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> UnderstoreyInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GeometryGrassInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Grass")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CloseTurfInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Portico")
    TObjectPtr<UStaticMeshComponent> PorticoV8RenderOnlyComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Portico")
    TObjectPtr<UStaticMeshComponent> PorticoV5CRenderOnlyComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Truth")
    bool bPorticoV5CPresentationActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Truth")
    bool bPorticoV5CRenderOnly = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Truth")
    bool bPorticoV5CCollisionNavigationOrRfAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Truth")
    bool bPorticoV5CUsesTextureMaps = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Sources")
    FString FrozenPorticoV5CObjSha256 =
        TEXT("883EAC65449A54D284D1E0E15B42F134F73C509A5ACE95BFD6DDD1DD326A3EC5");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Sources")
    FString FrozenPorticoV5CMtlSha256 =
        TEXT("DC38D80587AEDFD31143A755388D4248A7C6E2029C4C9FD92794DDE06F2E339C");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Sources")
    FString FrozenPorticoV5CManifestSha256 =
        TEXT("380A2DE1204029C26FA06A7BBFD99105AA9D7979625575B28FF3E45A4B0337A1");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V5C|Sources")
    FString FrozenPorticoV5CSemanticSha256 =
        TEXT("84E99970FEAF2F9ACCE9D7BE07C320C4A7E8AAA342F5C30463F0ADE311958567");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    FString ClaimLabel =
        TEXT("ISTANA_EXPLORE_V4_PUBLIC_OPEN_DATA_VISUAL_APPROXIMATION_NOT_ONE_TO_ONE_NOT_SURVEY_NOT_BOTANICAL_INVENTORY_NOT_SENSOR_TRUTH");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bOneToOneOneKilometerClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bSurveyAccuracyClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bExactBotanicalInventoryClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bIndividualRasterTreeTruthClaimed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bCollisionOrSensorTruthAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bGoogleOrOneMapContentUsed = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bPorticoCollisionAuthority = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bHeritageVisualsAreSilhouetteProxies = true;

    /** Imported/configured for provenance completeness, but never placed. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bLowPolyPalmRuntimePlacementAllowed = false;

    /** External preserved lawn surface; distinct from animated close-turf cards. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    FString ExternalStaticLawnSurfaceMaterialName =
        TEXT("MI_IPV4_AmbientCG_Grass001_Lawn");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Truth")
    bool bExternalStaticLawnSurfaceUsesWindWpo = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    FString FrozenGeospatialContractSha256 =
        TEXT("CCD9B5200E03602EC6FA6986F4D3AB70920806C63D718B5B1A943CC29F30D8E7");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    FString FrozenPreparedRasterGridSha256 =
        TEXT("262C88795C3C5FB1BB92B2D1CD2D1886B25D5E3885E60F34E407082E4C81C447");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    FString FrozenVegetationContractSchema =
        TEXT("triad.istana_explore_v4_vegetation_contract.v2");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    FString FrozenVegetationContractSha256 =
        TEXT("ABDC65AA14DA9AE89736BBE2D75E6212FA38B76B44DBA491C9266DEBC247E104");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    FString FrozenPorticoV8ObjSha256 =
        TEXT("330B20E8F58289C84EF96CB031D374ADB3AAF52711E5247D1F167C7E94DC4526");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    FString FrozenPorticoV8ManifestSha256 =
        TEXT("3E88B8C4AEF5A499696436920972301EFBDA242BAAA3F64928D1A8B072557943");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    FString FrozenPorticoV8SemanticSha256 =
        TEXT("3F0F48F1B8C07415D5539E91D1F006D3FD9EC3BD7E55A85B16F69883728465D8");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    TArray<FString> RequiredDistributionAttribution;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    bool bRequiredAttributionPresentedInPublicSurface = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Sources")
    bool bPublicDistributionReady = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Layout")
    int32 DeterministicPlacementSeed = 0x4757A6A5;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Layout")
    float ClearCeremonialAxisHalfWidthMeters = 17.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Layout")
    FVector2D FountainCenterMeters = FVector2D(0.0f, 95.0f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Layout")
    float FountainClearanceRadiusMeters = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Wind")
    float BaseWindStrengthCm = 5.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Wind")
    float GustPeakStrengthCm = 48.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Wind")
    float WindSpeed = 1.45f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Wind")
    FVector2D PrevailingWindDirection = FVector2D(0.93f, 0.37f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Wind")
    float RecoveryFrequencyHz = 0.62f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Wind")
    float RecoveryDampingRatio = 0.55f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana Explore V4|Wind")
    float FixedWindStepSeconds = 0.0083333333f;

protected:
    virtual void BeginPlay() override;

private:
    static void ConfigureVisualHism(
        UHierarchicalInstancedStaticMeshComponent* Component,
        int32 MinLod,
        int32 StartCullDistance,
        int32 EndCullDistance,
        int32 WpoDisableDistance,
        bool bCastShadow);
    static void ConfigureVisualStaticMesh(UStaticMeshComponent* Component);
    static bool ValidateExactPorticoV5CMesh(
        const UStaticMesh* Mesh,
        FString& OutError);
    static bool IsSupportedRasterClass(uint8 Value);
    static bool IsFinitePositiveTransform(const FTransform& Transform);
    static float WindResponseForRole(ETRIADIstanaExploreV4WindRole Role);
    static ETRIADIstanaExploreV4TreeForm TreeFormForWindRole(
        ETRIADIstanaExploreV4WindRole Role);
    UHierarchicalInstancedStaticMeshComponent* ComponentForTreeForm(
        ETRIADIstanaExploreV4TreeForm Form) const;
    UHierarchicalInstancedStaticMeshComponent* HeritageComponentForTreeForm(
        ETRIADIstanaExploreV4TreeForm Form) const;
    UStaticMeshComponent* ComponentForWindRole(
        ETRIADIstanaExploreV4WindRole Role) const;
    void ClearAllInstances();
    bool ValidateAssetAndWindConfiguration(FString& OutError) const;
    bool CreateRuntimeWindMaterialInstances(FString& OutError);
    void ApplyWindParameters();
    void AdvanceWindFixedStep(float StepSeconds);

    FTRIADIstanaExploreV4WindRuntimeReady WindRuntimeReadyEvent;

    UPROPERTY()
    FTRIADIstanaExploreV4AssetRoster SavedAssetRoster;

    UPROPERTY()
    TArray<FTRIADIstanaExploreV4WindMaterialBinding> SavedWindBindings;

    UPROPERTY()
    TArray<FTransform> PreservedInheritedV2TreeWorldTransforms;

    UPROPERTY()
    TArray<FTRIADIstanaExploreV4RasterCue> PreservedRasterCues;

    UPROPERTY()
    TArray<FTRIADIstanaExploreV4HeritageAnchor> PreservedHeritageAnchors;

    /** Exact V3 near-turf world transforms replaced by V4, never doubled. */
    UPROPERTY()
    TArray<FTransform> PreservedCloseTurfWorldTransforms;

    /** One persisted form byte per inherited source row. */
    UPROPERTY()
    TArray<uint8> PreservedTreeFormBySourceIndex;

    /** Exact frozen per-form census in enum order for public readback. */
    UPROPERTY()
    TArray<int32> RecordedMainTreeFormCounts;

    UPROPERTY()
    int32 RecordedShrubInstanceCount = 0;

    UPROPERTY()
    int32 RecordedFlowerInstanceCount = 0;

    UPROPERTY()
    int32 RecordedUnderstoreyInstanceCount = 0;

    UPROPERTY()
    int32 RecordedGeometryGrassInstanceCount = 0;

    UPROPERTY()
    int32 RecordedCloseTurfInstanceCount = 0;

    UPROPERTY()
    bool bAssetsAndWindConfigured = false;

    UPROPERTY()
    bool bPopulationConfigured = false;

    UPROPERTY()
    FString ConfiguredPorticoMeshPath;

    UPROPERTY()
    int32 RecordedExternalV2PawnBlockerCount = 0;

    UPROPERTY()
    bool bRecordedV2TreeVisualsHidden = false;

    UPROPERTY()
    bool bRecordedV3ReplacementTreeVisualsHidden = false;

    UPROPERTY()
    bool bRecordedV3CloseTurfVisualsHidden = false;

    UPROPERTY()
    bool bRecordedV7PorticoHidden = false;

    UPROPERTY()
    bool bRecordedInheritedTerrainAndCollisionPreserved = false;

    UPROPERTY()
    bool bRecordedDistantBuildingsPreserved = false;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> WindMaterialInstances;

    FRandomStream GustRandom;
    FVector2D CurrentWindDirection = FVector2D(1.0f, 0.0f);
    FVector2D WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    FVector2D ActiveGustDirection = FVector2D(1.0f, 0.0f);
    float CurrentWindStrengthCm = 0.0f;
    float WindStrengthVelocityCmPerSecond = 0.0f;
    float TimeUntilNextGustSeconds = 8.0f;
    float ActiveGustElapsedSeconds = -1.0f;
    float ActiveGustDurationSeconds = 1.8f;
    float ActiveGustPeakStrengthCm = 0.0f;
    float WindStepAccumulatorSeconds = 0.0f;
};
