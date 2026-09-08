#include "TRIADIstanaPublicViewSceneActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"

namespace
{
const FString PublicViewClaimLabel(
    TEXT("PUBLIC_REFERENCE_VISUAL_APPROXIMATION_NOT_SURVEY_CONTROLLED"));
const FName RainTreeArchetype(TEXT("RAIN"));
const FName PalmTreeArchetype(TEXT("PALM"));
const FName FramingTreeArchetype(TEXT("FRAMING"));
const FString V5CSurroundingsMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/"
         "SM_IPV5C_OfficialPreferredSurroundingContext."
         "SM_IPV5C_OfficialPreferredSurroundingContext"));
const FString V5CGroundContextMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/"
         "SM_IPV5C_OfficialPlanningGroundContext."
         "SM_IPV5C_OfficialPlanningGroundContext"));
constexpr int32 ExpectedV5CSurroundingsTriangleCount = 35424;
constexpr int32 ExpectedV5CSurroundingsMaterialCount = 4;
const FName ExpectedV5CSurroundingsMaterials[] = {
    TEXT("M_IPV5C_OfficialContextRender"),
    TEXT("M_IPV5C_OfficialContextRoof"),
    TEXT("M_IPV5C_OsmFallbackContextRender"),
    TEXT("M_IPV5C_OsmFallbackContextRoof")};
const int32 ExpectedV5CSurroundingsTrianglesByMaterial[] = {
    7690, 2945, 17690, 7099};
constexpr int32 ExpectedV5CGroundContextImportedTriangleCount = 30403;
constexpr int32 ExpectedV5CGroundContextMaterialCount = 2;
const FName ExpectedV5CGroundContextMaterials[] = {
    TEXT("MI_IPV5C_OfficialPlanningRoadZone"),
    TEXT("MI_IPV5C_OfficialPlanningRoadGraphic")};
const int32 ExpectedV5CGroundContextImportedTrianglesByMaterial[] = {
    23336, 7067};
const FString ExpectedV5CGroundContextMaterialObjectPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadZone."
         "MI_IPV5C_OfficialPlanningRoadZone"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadGraphic."
         "MI_IPV5C_OfficialPlanningRoadGraphic")};
static_assert(
    UE_ARRAY_COUNT(ExpectedV5CSurroundingsMaterials) ==
        ExpectedV5CSurroundingsMaterialCount);
static_assert(
    UE_ARRAY_COUNT(ExpectedV5CSurroundingsTrianglesByMaterial) ==
        ExpectedV5CSurroundingsMaterialCount);
static_assert(
    UE_ARRAY_COUNT(ExpectedV5CGroundContextMaterials) ==
        ExpectedV5CGroundContextMaterialCount);
static_assert(
    UE_ARRAY_COUNT(ExpectedV5CGroundContextImportedTrianglesByMaterial) ==
        ExpectedV5CGroundContextMaterialCount);
static_assert(
    UE_ARRAY_COUNT(ExpectedV5CGroundContextMaterialObjectPaths) ==
        ExpectedV5CGroundContextMaterialCount);

const FString ExpectedV5CSurroundingsObjSha256(
    TEXT("774F7E30456B989D0BF9EEB013C10D2688A2B3DE87936529BBB154E83D344C74"));
const FString ExpectedV5CSurroundingsMtlSha256(
    TEXT("6BBDA30E125F92EEF36D060404CA6EBB3F7DFC9DF95D7895766A57A99A737CA7"));
const FString ExpectedV5CSurroundingsFeaturesSha256(
    TEXT("8825DDC93E7C6465B01AD5A5AD23B2E8368F2C825B6F8F8EB1FBBD2DA8138793"));
const FString ExpectedV5CSurroundingsManifestSha256(
    TEXT("CEBFA56EC84E697305A35DA9CCD06B29619AB4500701B4A806B958B415F04F20"));
const FString ExpectedV5CSurroundingsContractSha256(
    TEXT("52587014FC80459732B1C943056D1724684287B67CA0DE8556AF7E1E2F8C7FBD"));
const FName HumanOnlyOverlayComponentTag(TEXT("TRIADHumanOnlyOverlay"));

FString ExpectedV5CSurroundingsMaterialObjectPath(const FName MaterialName)
{
    const FString Name = MaterialName.ToString();
    return TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/") +
        Name + TEXT(".") + Name;
}

bool HasThreeDimensionalExtent(
    const UStaticMesh* Mesh,
    double MinimumExtentCentimeters)
{
    if (!Mesh || !FMath::IsFinite(MinimumExtentCentimeters) ||
        MinimumExtentCentimeters <= 0.0)
    {
        return false;
    }
    const FVector Extent = Mesh->GetBounds().BoxExtent;
    return !Extent.ContainsNaN() &&
        Extent.X >= MinimumExtentCentimeters &&
        Extent.Y >= MinimumExtentCentimeters &&
        Extent.Z >= MinimumExtentCentimeters;
}

bool IsFiniteTransform(const FTransform& Transform)
{
    const FVector Scale = Transform.GetScale3D();
    return !Transform.ContainsNaN() &&
        !Transform.GetScale3D().IsNearlyZero() &&
        Scale.GetMin() >= 0.01 &&
        FMath::IsNearlyEqual(Scale.X, Scale.Y, 0.0001) &&
        FMath::IsNearlyEqual(Scale.X, Scale.Z, 0.0001);
}

bool IgnoresEveryCollisionChannel(const UPrimitiveComponent* Component)
{
    return Component &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore);
}

void ConfigureV5CSurroundingsRenderOnlyComponent(
    UStaticMeshComponent* Component)
{
    if (!Component)
    {
        return;
    }
    Component->SetRelativeTransform(FTransform::Identity);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(true);
    Component->SetCastContactShadow(false);
    Component->bHiddenInSceneCapture = true;
    Component->ComponentTags.AddUnique(HumanOnlyOverlayComponentTag);
}

void ConfigureV5CGroundContextRenderOnlyComponent(
    UStaticMeshComponent* Component)
{
    ConfigureV5CSurroundingsRenderOnlyComponent(Component);
    if (Component)
    {
        // This thin planning graphic follows the visible ground and must not
        // introduce a second authored shadow surface.
        Component->SetCastShadow(false);
    }
}

bool ValidateExactV5CSurroundingsMesh(
    const UStaticMesh* Mesh,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || Mesh->GetPathName() != V5CSurroundingsMeshObjectPath ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() !=
            ExpectedV5CSurroundingsTriangleCount ||
        RenderData->LODResources[0].Sections.Num() !=
            ExpectedV5CSurroundingsMaterialCount ||
        Mesh->GetStaticMaterials().Num() !=
            ExpectedV5CSurroundingsMaterialCount ||
        !Mesh->HasValidNaniteData())
    {
        OutError = TEXT("V5C surroundings require the exact Nanite-enabled 35,424-triangle, four-slot identity mesh in the isolated namespace.");
        return false;
    }

    const UBodySetup* Body = Mesh->GetBodySetup();
    if (Body && (Body->AggGeom.GetElementCount() != 0 ||
                 Body->CollisionTraceFlag == CTF_UseComplexAsSimple))
    {
        OutError = TEXT("The V5C surroundings mesh must remain render-only with no simple or complex-as-simple collision.");
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(
            FVector(-98438.3058, -99831.9761, -233.2826), 1.0) ||
        !BoundsMax.Equals(
            FVector(99880.8394, 99339.8856, 11604.1346), 1.0))
    {
        OutError = FString::Printf(
            TEXT("V5C surroundings imported centimetre bounds changed: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            BoundsMin.X,
            BoundsMin.Y,
            BoundsMin.Z,
            BoundsMax.X,
            BoundsMax.Y,
            BoundsMax.Z);
        return false;
    }

    for (int32 Slot = 0;
         Slot < ExpectedV5CSurroundingsMaterialCount;
         ++Slot)
    {
        const FStaticMaterial& StaticMaterial =
            Mesh->GetStaticMaterials()[Slot];
        const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
        bool bImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
        bImportedSlotNameMatches =
            StaticMaterial.ImportedMaterialSlotName ==
                ExpectedV5CSurroundingsMaterials[Slot];
#endif
        if (StaticMaterial.MaterialSlotName !=
                ExpectedV5CSurroundingsMaterials[Slot] ||
            !bImportedSlotNameMatches ||
            !Material || Material->GetPathName() !=
                ExpectedV5CSurroundingsMaterialObjectPath(
                    ExpectedV5CSurroundingsMaterials[Slot]))
        {
            OutError = FString::Printf(
                TEXT("V5C surroundings material slot %d lost exact name, order or binding."),
                Slot);
            return false;
        }
    }

    TSet<int32> SeenSectionMaterials;
    for (const FStaticMeshSection& Section :
         RenderData->LODResources[0].Sections)
    {
        if (Section.MaterialIndex < 0 ||
            Section.MaterialIndex >= ExpectedV5CSurroundingsMaterialCount ||
            SeenSectionMaterials.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                ExpectedV5CSurroundingsTrianglesByMaterial[
                    Section.MaterialIndex])
        {
            OutError = TEXT("V5C surroundings exact four-section material/triangle topology changed.");
            return false;
        }
        SeenSectionMaterials.Add(Section.MaterialIndex);
    }
    if (SeenSectionMaterials.Num() !=
        ExpectedV5CSurroundingsMaterialCount)
    {
        OutError = TEXT("V5C surroundings exact material-section census is incomplete.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool ValidateExactV5CGroundContextMesh(
    const UStaticMesh* Mesh,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || Mesh->GetPathName() != V5CGroundContextMeshObjectPath ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() !=
            ExpectedV5CGroundContextImportedTriangleCount ||
        RenderData->LODResources[0].Sections.Num() !=
            ExpectedV5CGroundContextMaterialCount ||
        Mesh->GetStaticMaterials().Num() !=
            ExpectedV5CGroundContextMaterialCount ||
        !Mesh->HasValidNaniteData())
    {
        OutError = TEXT("V5C ground context requires the exact Nanite-enabled 30,403 imported-triangle, two-slot official-planning mesh in the isolated namespace.");
        return false;
    }

    const UBodySetup* Body = Mesh->GetBodySetup();
    if (Body && (Body->AggGeom.GetElementCount() != 0 ||
                 Body->CollisionTraceFlag == CTF_UseComplexAsSimple))
    {
        OutError = TEXT("The V5C official planning ground context must remain render-only with no simple or complex-as-simple collision.");
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(
            FVector(-99838.5847, -99957.6768, -128.0038645876),
            0.25) ||
        !BoundsMax.Equals(
            FVector(99892.9611, 99966.3390, 191.8825290834),
            0.25))
    {
        OutError = FString::Printf(
            TEXT("V5C official planning ground-context imported centimetre bounds changed: min=(%.4f,%.4f,%.4f) max=(%.4f,%.4f,%.4f)."),
            BoundsMin.X,
            BoundsMin.Y,
            BoundsMin.Z,
            BoundsMax.X,
            BoundsMax.Y,
            BoundsMax.Z);
        return false;
    }

    TSet<int32> SeenSectionMaterials;
    for (int32 SectionIndex = 0;
         SectionIndex < RenderData->LODResources[0].Sections.Num();
         ++SectionIndex)
    {
        const FStaticMeshSection& Section =
            RenderData->LODResources[0].Sections[SectionIndex];
        if (Section.MaterialIndex < 0 ||
            Section.MaterialIndex >= ExpectedV5CGroundContextMaterialCount ||
            SeenSectionMaterials.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                ExpectedV5CGroundContextImportedTrianglesByMaterial[
                    Section.MaterialIndex])
        {
            OutError = TEXT("V5C official planning ground context material-section order or uniqueness changed.");
            return false;
        }
        SeenSectionMaterials.Add(Section.MaterialIndex);
    }
    if (SeenSectionMaterials.Num() !=
        ExpectedV5CGroundContextMaterialCount)
    {
        OutError = TEXT("V5C official planning ground context material-section census is incomplete.");
        return false;
    }

    for (int32 Slot = 0;
         Slot < ExpectedV5CGroundContextMaterialCount;
         ++Slot)
    {
        const FStaticMaterial& StaticMaterial =
            Mesh->GetStaticMaterials()[Slot];
        const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
        bool bImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
        bImportedSlotNameMatches =
            StaticMaterial.ImportedMaterialSlotName ==
                ExpectedV5CGroundContextMaterials[Slot];
#endif
        if (StaticMaterial.MaterialSlotName !=
                ExpectedV5CGroundContextMaterials[Slot] ||
            !bImportedSlotNameMatches ||
            !Material || Material->GetPathName() !=
                ExpectedV5CGroundContextMaterialObjectPaths[Slot])
        {
            OutError = FString::Printf(
                TEXT("V5C official planning ground context material slot %d lost its exact ordered binding."),
                Slot);
            return false;
        }
    }

    OutError.Reset();
    return true;
}
}

ATRIADIstanaPublicViewSceneActor::ATRIADIstanaPublicViewSceneActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    BuildingHeroVisualComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingHeroVisual"));
    BuildingHeroVisualComponent->SetupAttachment(SceneRoot);
    BuildingHeroVisualComponent->SetMobility(EComponentMobility::Static);
    BuildingHeroVisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BuildingCollisionComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingCollision"));
    BuildingCollisionComponent->SetupAttachment(SceneRoot);
    BuildingCollisionComponent->SetMobility(EComponentMobility::Static);
    BuildingCollisionComponent->SetVisibility(false, true);
    BuildingCollisionComponent->SetHiddenInGame(true, true);
    BuildingCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    TerrainComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerrainVisualCollision"));
    TerrainComponent->SetupAttachment(SceneRoot);
    TerrainComponent->SetMobility(EComponentMobility::Static);
    TerrainComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    TerrainSkirtComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SyntheticTerrainBoundarySkirt"));
    TerrainSkirtComponent->SetupAttachment(SceneRoot);
    TerrainSkirtComponent->SetMobility(EComponentMobility::Static);
    TerrainSkirtComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TerrainSkirtComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    TerrainSkirtComponent->SetGenerateOverlapEvents(false);

    HardscapeComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PublicForecourtHardscape"));
    HardscapeComponent->SetupAttachment(SceneRoot);
    HardscapeComponent->SetMobility(EComponentMobility::Static);
    HardscapeComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    ContextBuildingsComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnonymousContextBuildings"));
    ContextBuildingsComponent->SetupAttachment(SceneRoot);
    ContextBuildingsComponent->SetMobility(EComponentMobility::Static);
    ContextBuildingsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ContextBuildingsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    ContextBuildingsComponent->SetGenerateOverlapEvents(false);
    ContextBuildingsComponent->SetVisibility(false, true);
    ContextBuildingsComponent->SetHiddenInGame(true, true);
    ContextBuildingsComponent->SetAutoActivate(false);
    ContextBuildingsComponent->SetActive(false);

    OSMContextBuildingsComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ODbLMappingGradeContextBuildings"));
    OSMContextBuildingsComponent->SetupAttachment(SceneRoot);
    OSMContextBuildingsComponent->SetMobility(EComponentMobility::Static);
    OSMContextBuildingsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OSMContextBuildingsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    OSMContextBuildingsComponent->SetGenerateOverlapEvents(false);
    OSMContextBuildingsComponent->SetCanEverAffectNavigation(false);
    OSMContextBuildingsComponent->SetAutoActivate(true);
    OSMContextBuildingsComponent->SetVisibility(true, true);
    OSMContextBuildingsComponent->SetHiddenInGame(false, true);

    V5CSurroundingsRenderOnlyComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("V5COfficialPreferredSurroundingContextRenderOnlySuccessor"));
    V5CSurroundingsRenderOnlyComponent->SetupAttachment(SceneRoot);
    ConfigureV5CSurroundingsRenderOnlyComponent(
        V5CSurroundingsRenderOnlyComponent);
    V5CSurroundingsRenderOnlyComponent->bAutoActivate = false;
    V5CSurroundingsRenderOnlyComponent->SetVisibility(false, true);
    V5CSurroundingsRenderOnlyComponent->SetHiddenInGame(true, true);
    V5CSurroundingsRenderOnlyComponent->SetActive(false);

    V5CGroundContextRenderOnlyComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("V5COfficialPlanningGroundContextRenderOnlySuccessor"));
    V5CGroundContextRenderOnlyComponent->SetupAttachment(SceneRoot);
    ConfigureV5CGroundContextRenderOnlyComponent(
        V5CGroundContextRenderOnlyComponent);
    V5CGroundContextRenderOnlyComponent->bAutoActivate = false;
    V5CGroundContextRenderOnlyComponent->SetVisibility(false, true);
    V5CGroundContextRenderOnlyComponent->SetHiddenInGame(true, true);
    V5CGroundContextRenderOnlyComponent->SetActive(false);

    RainTreeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("VolumetricRainTrees"));
    RainTreeInstances->SetupAttachment(SceneRoot);
    RainTreeInstances->SetMobility(EComponentMobility::Static);
    RainTreeInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RainTreeInstances->SetCollisionResponseToAllChannels(ECR_Ignore);
    RainTreeInstances->SetGenerateOverlapEvents(false);

    PalmTreeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("VolumetricPalmTrees"));
    PalmTreeInstances->SetupAttachment(SceneRoot);
    PalmTreeInstances->SetMobility(EComponentMobility::Static);
    PalmTreeInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PalmTreeInstances->SetCollisionResponseToAllChannels(ECR_Ignore);
    PalmTreeInstances->SetGenerateOverlapEvents(false);

    FramingTreeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("VolumetricFramingTrees"));
    FramingTreeInstances->SetupAttachment(SceneRoot);
    FramingTreeInstances->SetMobility(EComponentMobility::Static);
    FramingTreeInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FramingTreeInstances->SetCollisionResponseToAllChannels(ECR_Ignore);
    FramingTreeInstances->SetGenerateOverlapEvents(false);

    ClaimLabel = ExpectedClaimLabel();
}

const FString& ATRIADIstanaPublicViewSceneActor::ExpectedClaimLabel()
{
    return PublicViewClaimLabel;
}

bool ATRIADIstanaPublicViewSceneActor::ConfigurePublicViewAssets(
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
    FString& OutError)
{
    if (!InBuildingHero || !InBuildingCollision || !InTerrain || !InTerrainSkirt ||
        !InHardscape || !InContextBuildings || !InOsmContextBuildings || !InRainTree ||
        !InPalmTree || !InFramingTree)
    {
        OutError = TEXT("ASSETS_MISSING: all ten isolated public-view meshes are required.");
        return false;
    }

    BuildingHeroVisualComponent->SetStaticMesh(InBuildingHero);
    BuildingCollisionComponent->SetStaticMesh(InBuildingCollision);
    TerrainComponent->SetStaticMesh(InTerrain);
    TerrainSkirtComponent->SetStaticMesh(InTerrainSkirt);
    HardscapeComponent->SetStaticMesh(InHardscape);
    ContextBuildingsComponent->SetStaticMesh(InContextBuildings);
    OSMContextBuildingsComponent->SetStaticMesh(InOsmContextBuildings);
    ContextBuildingsComponent->SetVisibility(false, true);
    ContextBuildingsComponent->SetHiddenInGame(true, true);
    ContextBuildingsComponent->SetActive(false);
    RestoreLegacyOsmSurroundingsPresentationFailSafe();
    RainTreeInstances->SetStaticMesh(InRainTree);
    PalmTreeInstances->SetStaticMesh(InPalmTree);
    FramingTreeInstances->SetStaticMesh(InFramingTree);

    ClaimLabel = ExpectedClaimLabel();
    ReferenceEpoch = TEXT("2024-04");
    bSurveyControlled = false;
    bArbitraryViewIndistinguishabilityClaimed = false;
    bPhotorealMaterialAcceptanceClaimed = false;
    bPbrTexturePackIntegrated = true;
    MaterialImplementationTier =
        TEXT("ORIGINAL_AI_ASSISTED_PBR_TEXTURES_INTEGRATED_PHOTO_QA_NOT_ACCEPTED");
    DisplayColorPolicy =
        TEXT("NEUTRAL_CAMERA_LOCAL_EXPOSURE_DISABLED_EXTERNAL_SRGB_REC709_DISPLAY_NOT_OCIO_VALIDATED");
    bExternalOcioDisplayValidated = false;
    PhotoQaEvidenceStatus =
        TEXT("NOT_READY_UNCALIBRATED_COMMONS_EVIDENCE_METADATA_ONLY");
    bPhotoQaEvidenceConsumedByMap = false;
    bContainsWholeTreeBillboards = false;
    SensorOcclusionPolicy =
        TEXT("SYNTHETIC_CONTEXT_AND_VEGETATION_VISUAL_ONLY_NO_COLLISION");
    TerrainBoundaryPolicy =
        TEXT("SYNTHETIC_DOWNWARD_VISUAL_ONLY_SKIRT_NO_COLLISION");
    OSMContextIntegrationStatus =
        TEXT("COORDINATE_CONTRACT_ALIGNED_MAPPING_GRADE_NOT_VISUALLY_OR_PHOTO_ACCEPTED");
    OSMContextAttribution = TEXT("© OpenStreetMap contributors; ODbL-1.0");
    bOSMContextUsedForSensorTruth = false;
    bSyntheticContextBuildingsEnabled = false;

    OutError.Reset();
    return true;
}

void ATRIADIstanaPublicViewSceneActor::
    RestoreLegacyOsmSurroundingsPresentationFailSafe()
{
    bV5CSurroundingsPresentationActive = false;
    if (OSMContextBuildingsComponent)
    {
        OSMContextBuildingsComponent->SetRelativeTransform(
            FTransform::Identity);
        OSMContextBuildingsComponent->SetCollisionEnabled(
            ECollisionEnabled::NoCollision);
        OSMContextBuildingsComponent->SetCollisionResponseToAllChannels(
            ECR_Ignore);
        OSMContextBuildingsComponent->SetGenerateOverlapEvents(false);
        OSMContextBuildingsComponent->SetCanEverAffectNavigation(false);
        OSMContextBuildingsComponent->bAutoActivate = true;
        OSMContextBuildingsComponent->SetVisibility(true, true);
        OSMContextBuildingsComponent->SetHiddenInGame(false, true);
        OSMContextBuildingsComponent->SetActive(true);
    }
    if (V5CSurroundingsRenderOnlyComponent)
    {
        V5CSurroundingsRenderOnlyComponent->SetStaticMesh(nullptr);
        ConfigureV5CSurroundingsRenderOnlyComponent(
            V5CSurroundingsRenderOnlyComponent);
        V5CSurroundingsRenderOnlyComponent->bAutoActivate = false;
        V5CSurroundingsRenderOnlyComponent->SetVisibility(false, true);
        V5CSurroundingsRenderOnlyComponent->SetHiddenInGame(true, true);
        V5CSurroundingsRenderOnlyComponent->SetActive(false);
    }
    if (V5CGroundContextRenderOnlyComponent)
    {
        V5CGroundContextRenderOnlyComponent->SetStaticMesh(nullptr);
        ConfigureV5CGroundContextRenderOnlyComponent(
            V5CGroundContextRenderOnlyComponent);
        V5CGroundContextRenderOnlyComponent->bAutoActivate = false;
        V5CGroundContextRenderOnlyComponent->SetVisibility(false, true);
        V5CGroundContextRenderOnlyComponent->SetHiddenInGame(true, true);
        V5CGroundContextRenderOnlyComponent->SetActive(false);
    }
}

bool ATRIADIstanaPublicViewSceneActor::
    ConfigureV5CSurroundingsPresentation(
        UStaticMesh* InSurroundingsV5C,
        UStaticMesh* InGroundContextV5C,
        FString& OutError)
{
    // Establish the visible legacy fallback before inspecting either candidate.
    RestoreLegacyOsmSurroundingsPresentationFailSafe();

    if (!OSMContextBuildingsComponent ||
        !OSMContextBuildingsComponent->GetStaticMesh() ||
        !V5CSurroundingsRenderOnlyComponent ||
        !V5CGroundContextRenderOnlyComponent)
    {
        OutError = TEXT("Atomic V5C surroundings presentation refused because the configured legacy fallback, building sibling, or ground-context sibling component is absent.");
        return false;
    }

    FString BuildingMeshError;
    FString GroundMeshError;
    const bool bBuildingMeshValid = ValidateExactV5CSurroundingsMesh(
        InSurroundingsV5C, BuildingMeshError);
    const bool bGroundMeshValid = ValidateExactV5CGroundContextMesh(
        InGroundContextV5C, GroundMeshError);
    if (!bBuildingMeshValid || !bGroundMeshValid)
    {
        OutError = FString::Printf(
            TEXT("Atomic V5C surroundings presentation refused; legacy ODbL context remains visible and both V5C siblings remain empty. building='%s' ground='%s'."),
            *BuildingMeshError,
            *GroundMeshError);
        return false;
    }

    // Mutate neither V5C sibling until both independent candidates pass.
    V5CSurroundingsRenderOnlyComponent->SetStaticMesh(InSurroundingsV5C);
    ConfigureV5CSurroundingsRenderOnlyComponent(
        V5CSurroundingsRenderOnlyComponent);
    V5CSurroundingsRenderOnlyComponent->bAutoActivate = true;
    V5CSurroundingsRenderOnlyComponent->SetVisibility(true, true);
    V5CSurroundingsRenderOnlyComponent->SetHiddenInGame(false, true);
    V5CSurroundingsRenderOnlyComponent->SetActive(true);

    V5CGroundContextRenderOnlyComponent->SetStaticMesh(
        InGroundContextV5C);
    ConfigureV5CGroundContextRenderOnlyComponent(
        V5CGroundContextRenderOnlyComponent);
    V5CGroundContextRenderOnlyComponent->bAutoActivate = true;
    V5CGroundContextRenderOnlyComponent->SetVisibility(true, true);
    V5CGroundContextRenderOnlyComponent->SetHiddenInGame(false, true);
    V5CGroundContextRenderOnlyComponent->SetActive(true);

    bV5CSurroundingsPresentationActive = true;
    // Keep PIE component initialization from reactivating the hidden legacy
    // sibling after this transient/editor state is duplicated into play.
    OSMContextBuildingsComponent->bAutoActivate = false;
    OSMContextBuildingsComponent->SetVisibility(false, true);
    OSMContextBuildingsComponent->SetHiddenInGame(true, true);
    OSMContextBuildingsComponent->SetActive(false);

    FString PresentationError;
    if (!ValidateV5CSurroundingsPresentation(PresentationError))
    {
        RestoreLegacyOsmSurroundingsPresentationFailSafe();
        OutError = TEXT("Atomic V5C surroundings presentation readback failed; legacy ODbL context was restored and both V5C siblings were emptied. ") +
            PresentationError;
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaPublicViewSceneActor::
    ValidateV5CSurroundingsPresentation(FString& OutReport) const
{
    const auto IsRenderOnlyIdentity = [this](
        const UStaticMeshComponent* Component)
    {
        return Component && Component->GetAttachParent() == SceneRoot &&
            Component->Mobility == EComponentMobility::Static &&
            Component->GetRelativeTransform().Equals(
                FTransform::Identity, 0.001) &&
            Component->GetCollisionEnabled() ==
                ECollisionEnabled::NoCollision &&
            !Component->GetGenerateOverlapEvents() &&
            !Component->CanEverAffectNavigation() &&
            IgnoresEveryCollisionChannel(Component);
    };
    if (!IsRenderOnlyIdentity(OSMContextBuildingsComponent) ||
        !IsRenderOnlyIdentity(V5CSurroundingsRenderOnlyComponent) ||
        !IsRenderOnlyIdentity(V5CGroundContextRenderOnlyComponent) ||
        !V5CSurroundingsRenderOnlyComponent->bHiddenInSceneCapture ||
        V5CSurroundingsRenderOnlyComponent->bCastContactShadow ||
        !V5CSurroundingsRenderOnlyComponent->ComponentHasTag(
            HumanOnlyOverlayComponentTag) ||
        !V5CGroundContextRenderOnlyComponent->bHiddenInSceneCapture ||
        V5CGroundContextRenderOnlyComponent->CastShadow ||
        V5CGroundContextRenderOnlyComponent->bCastContactShadow ||
        !V5CGroundContextRenderOnlyComponent->ComponentHasTag(
            HumanOnlyOverlayComponentTag) ||
        !bV5CSurroundingsRenderOnly ||
        bV5CSurroundingsCollisionNavigationSensorOrRfAuthority ||
        bV5CSurroundingsMeasuredHeightClaimed ||
        bV5CSurroundingsFacadeOrRoofFormClaimed ||
        bV5CSurroundingsTerrainGradeOrFoundationClaimed ||
        bV5CSurroundingsVisualAcceptanceClaimed ||
        !bV5CGroundContextRenderOnly ||
        bV5CGroundContextRoadWidthOrMaterialClaimed ||
        bV5CGroundContextElevationOrSurveyClaimed ||
        bV5CGroundContextCollisionNavigationSensorOrRfAuthority ||
        FrozenV5CSurroundingsObjSha256 !=
            ExpectedV5CSurroundingsObjSha256 ||
        FrozenV5CSurroundingsMtlSha256 !=
            ExpectedV5CSurroundingsMtlSha256 ||
        FrozenV5CSurroundingsFeaturesSha256 !=
            ExpectedV5CSurroundingsFeaturesSha256 ||
        FrozenV5CSurroundingsManifestSha256 !=
            ExpectedV5CSurroundingsManifestSha256 ||
        FrozenV5CSurroundingsContractSha256 !=
            ExpectedV5CSurroundingsContractSha256)
    {
        OutReport = TEXT("Legacy/atomic-V5C presentation siblings lost identity, NoCollision, no-navigation, human-only capture exclusion, no physical terrain-grade/foundation/facade, road/material/Z/survey/sensor/RF authority, or frozen-source/claim boundaries.");
        return false;
    }

    if (!bV5CSurroundingsPresentationActive)
    {
        if (!OSMContextBuildingsComponent->bAutoActivate ||
            !OSMContextBuildingsComponent->GetStaticMesh() ||
            !OSMContextBuildingsComponent->IsVisible() ||
            OSMContextBuildingsComponent->bHiddenInGame ||
            !OSMContextBuildingsComponent->IsActive() ||
            V5CSurroundingsRenderOnlyComponent->GetStaticMesh() ||
            V5CSurroundingsRenderOnlyComponent->bAutoActivate ||
            V5CSurroundingsRenderOnlyComponent->IsVisible() ||
            !V5CSurroundingsRenderOnlyComponent->bHiddenInGame ||
            V5CSurroundingsRenderOnlyComponent->IsActive() ||
            V5CGroundContextRenderOnlyComponent->GetStaticMesh() ||
            V5CGroundContextRenderOnlyComponent->bAutoActivate ||
            V5CGroundContextRenderOnlyComponent->IsVisible() ||
            !V5CGroundContextRenderOnlyComponent->bHiddenInGame ||
            V5CGroundContextRenderOnlyComponent->IsActive())
        {
            OutReport = TEXT("Inactive V5C surroundings state must expose legacy ODbL context and keep both empty V5C siblings hidden and inactive.");
            return false;
        }
        OutReport = TEXT("ISTANA_EXPLORE_V5C_SURROUNDINGS_PRESENTATION_VALID active=false failSafeLegacyOsmVisible=true v5cBuildingsEmpty=true v5cGroundContextEmpty=true.");
        return true;
    }

    FString BuildingMeshError;
    FString GroundMeshError;
    const bool bBuildingMeshValid = ValidateExactV5CSurroundingsMesh(
        V5CSurroundingsRenderOnlyComponent->GetStaticMesh(),
        BuildingMeshError);
    const bool bGroundMeshValid = ValidateExactV5CGroundContextMesh(
        V5CGroundContextRenderOnlyComponent->GetStaticMesh(),
        GroundMeshError);
    if (!bBuildingMeshValid ||
        !bGroundMeshValid ||
        OSMContextBuildingsComponent->bAutoActivate ||
        OSMContextBuildingsComponent->IsVisible() ||
        !OSMContextBuildingsComponent->bHiddenInGame ||
        OSMContextBuildingsComponent->IsActive() ||
        !V5CSurroundingsRenderOnlyComponent->bAutoActivate ||
        !V5CSurroundingsRenderOnlyComponent->IsVisible() ||
        V5CSurroundingsRenderOnlyComponent->bHiddenInGame ||
        !V5CSurroundingsRenderOnlyComponent->IsActive() ||
        !V5CGroundContextRenderOnlyComponent->bAutoActivate ||
        !V5CGroundContextRenderOnlyComponent->IsVisible() ||
        V5CGroundContextRenderOnlyComponent->bHiddenInGame ||
        !V5CGroundContextRenderOnlyComponent->IsActive())
    {
        OutReport = FString::Printf(
            TEXT("Active V5C surroundings state requires exact visible building and ground-context siblings with hidden legacy ODbL context. buildingMeshError='%s' groundMeshError='%s' legacyAuto=%s legacyVisible=%s legacyHiddenInGame=%s legacyActive=%s buildingAuto=%s buildingVisible=%s buildingHiddenInGame=%s buildingActive=%s groundAuto=%s groundVisible=%s groundHiddenInGame=%s groundActive=%s."),
            *BuildingMeshError,
            *GroundMeshError,
            OSMContextBuildingsComponent->bAutoActivate ? TEXT("true") : TEXT("false"),
            OSMContextBuildingsComponent->IsVisible() ? TEXT("true") : TEXT("false"),
            OSMContextBuildingsComponent->bHiddenInGame ? TEXT("true") : TEXT("false"),
            OSMContextBuildingsComponent->IsActive() ? TEXT("true") : TEXT("false"),
            V5CSurroundingsRenderOnlyComponent->bAutoActivate ? TEXT("true") : TEXT("false"),
            V5CSurroundingsRenderOnlyComponent->IsVisible() ? TEXT("true") : TEXT("false"),
            V5CSurroundingsRenderOnlyComponent->bHiddenInGame ? TEXT("true") : TEXT("false"),
            V5CSurroundingsRenderOnlyComponent->IsActive() ? TEXT("true") : TEXT("false"),
            V5CGroundContextRenderOnlyComponent->bAutoActivate ? TEXT("true") : TEXT("false"),
            V5CGroundContextRenderOnlyComponent->IsVisible() ? TEXT("true") : TEXT("false"),
            V5CGroundContextRenderOnlyComponent->bHiddenInGame ? TEXT("true") : TEXT("false"),
            V5CGroundContextRenderOnlyComponent->IsActive() ? TEXT("true") : TEXT("false"));
        return false;
    }

    OutReport = TEXT("ISTANA_EXPLORE_V5C_SURROUNDINGS_PRESENTATION_VALID active=true atomicBuildingsAndGroundContext=true legacyOsmHidden=true identity=true renderOnly=true collision=false navigation=false syntheticTerrainGrounding=true hiddenFoundationSkirtMeters=1.0 nonAuthoritativeFacadeCues=true physicalGradeFoundationFacadeAuthority=false roadWidthMaterialAuthority=false elevationSurveyAuthority=false sensorRfAuthority=false officialPreferredBuildings=449 osmFallbackBuildings=889 visualAcceptance=false.");
    return true;
}

void ATRIADIstanaPublicViewSceneActor::ClearVolumetricTreeInstances()
{
    RainTreeInstances->ClearInstances();
    PalmTreeInstances->ClearInstances();
    FramingTreeInstances->ClearInstances();
}

bool ATRIADIstanaPublicViewSceneActor::AddVolumetricTreeInstance(
    const FName& Archetype,
    const FTransform& LocalTransform,
    FString& OutError)
{
    UHierarchicalInstancedStaticMeshComponent* Target = nullptr;
    if (Archetype == RainTreeArchetype)
    {
        Target = RainTreeInstances;
    }
    else if (Archetype == PalmTreeArchetype)
    {
        Target = PalmTreeInstances;
    }
    else if (Archetype == FramingTreeArchetype)
    {
        Target = FramingTreeInstances;
    }
    if (!Target || !Target->GetStaticMesh())
    {
        OutError = FString::Printf(
            TEXT("Unknown or unconfigured volumetric tree archetype '%s'."),
            *Archetype.ToString());
        return false;
    }
    if (!IsFiniteTransform(LocalTransform))
    {
        OutError = FString::Printf(
            TEXT("Tree archetype '%s' received a non-finite or degenerate transform."),
            *Archetype.ToString());
        return false;
    }
    if (LocalTransform.GetTranslation().Size2D() >
        (ContextRadiusMeters * 100.0f + 1.0f))
    {
        OutError = FString::Printf(
            TEXT("Tree archetype '%s' lies outside the %.0f m public-view context."),
            *Archetype.ToString(),
            ContextRadiusMeters);
        return false;
    }

    if (Target->AddInstance(LocalTransform, false) == INDEX_NONE)
    {
        OutError = FString::Printf(
            TEXT("Could not add volumetric tree instance for archetype '%s'."),
            *Archetype.ToString());
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaPublicViewSceneActor::ValidatePublicViewScene(
    FString& OutReport,
    bool bRequireLegacyTreeInstances) const
{
    TArray<FString> Errors;
    const UStaticMesh* BuildingHero = BuildingHeroVisualComponent
        ? BuildingHeroVisualComponent->GetStaticMesh()
        : nullptr;
    const UStaticMesh* BuildingCollision = BuildingCollisionComponent
        ? BuildingCollisionComponent->GetStaticMesh()
        : nullptr;
    const UStaticMesh* Terrain = TerrainComponent
        ? TerrainComponent->GetStaticMesh()
        : nullptr;
    const UStaticMesh* TerrainSkirt = TerrainSkirtComponent
        ? TerrainSkirtComponent->GetStaticMesh()
        : nullptr;
    const UStaticMesh* Hardscape = HardscapeComponent
        ? HardscapeComponent->GetStaticMesh()
        : nullptr;
    const UStaticMesh* ContextBuildings = ContextBuildingsComponent
        ? ContextBuildingsComponent->GetStaticMesh()
        : nullptr;
    const UStaticMesh* OSMContextBuildings = OSMContextBuildingsComponent
        ? OSMContextBuildingsComponent->GetStaticMesh()
        : nullptr;
    const UStaticMesh* RainTree = RainTreeInstances
        ? RainTreeInstances->GetStaticMesh()
        : nullptr;
    const UStaticMesh* PalmTree = PalmTreeInstances
        ? PalmTreeInstances->GetStaticMesh()
        : nullptr;
    const UStaticMesh* FramingTree = FramingTreeInstances
        ? FramingTreeInstances->GetStaticMesh()
        : nullptr;

    if (!HasThreeDimensionalExtent(BuildingHero, 50.0))
    {
        Errors.Add(TEXT("Building hero is absent or lacks three-dimensional extent."));
    }
    if (!HasThreeDimensionalExtent(BuildingCollision, 50.0))
    {
        Errors.Add(TEXT("Building collision mesh is absent or volumetrically invalid."));
    }
    if (!HasThreeDimensionalExtent(Terrain, 25.0))
    {
        Errors.Add(TEXT("Terrain is absent or lacks the required non-flat authored relief."));
    }
    else
    {
        const FVector TerrainSize = Terrain->GetBounds().BoxExtent * 2.0;
        if (TerrainSize.X < 195000.0 || TerrainSize.Y < 195000.0 ||
            TerrainSize.Z < 100.0)
        {
            Errors.Add(FString::Printf(
                TEXT("Terrain bounds %.1f x %.1f x %.1f cm do not cover the required non-flat two-kilometre context surface."),
                TerrainSize.X,
                TerrainSize.Y,
                TerrainSize.Z));
        }
    }
    if (!HasThreeDimensionalExtent(TerrainSkirt, 25.0))
    {
        Errors.Add(TEXT("Synthetic one-kilometre terrain-edge skirt is absent or lacks downward extent."));
    }
    if (!HasThreeDimensionalExtent(Hardscape, 1.0))
    {
        Errors.Add(TEXT("Public forecourt hardscape is absent or planar."));
    }
    if (!HasThreeDimensionalExtent(ContextBuildings, 50.0))
    {
        Errors.Add(TEXT("Frozen synthetic context fallback is absent or volumetrically invalid."));
    }
    if (!HasThreeDimensionalExtent(OSMContextBuildings, 50.0))
    {
        Errors.Add(TEXT("ODbL mapping-grade outer-context buildings are absent or volumetrically invalid."));
    }
    struct FTreeValidationRow
    {
        const TCHAR* Name;
        const UStaticMesh* Mesh;
    };
    const FTreeValidationRow Trees[] = {
        {TEXT("RAIN"), RainTree},
        {TEXT("PALM"), PalmTree},
        {TEXT("FRAMING"), FramingTree}};
    for (const FTreeValidationRow& Tree : Trees)
    {
        if (!HasThreeDimensionalExtent(Tree.Mesh, 10.0))
        {
            Errors.Add(FString::Printf(
                TEXT("%s tree archetype is absent or is not a complete volumetric mesh."),
                Tree.Name));
        }
    }

    const int32 RainCount = RainTreeInstances ? RainTreeInstances->GetInstanceCount() : 0;
    const int32 PalmCount = PalmTreeInstances ? PalmTreeInstances->GetInstanceCount() : 0;
    const int32 FramingCount = FramingTreeInstances ? FramingTreeInstances->GetInstanceCount() : 0;
    const int32 TreeCount = RainCount + PalmCount + FramingCount;
    if (bRequireLegacyTreeInstances &&
        (TreeCount < MinimumRequiredTreeInstances ||
         RainCount < 1 || PalmCount < 1 || FramingCount < 1))
    {
        Errors.Add(FString::Printf(
            TEXT("Volumetric vegetation requires at least %d total instances and every archetype; found RAIN=%d PALM=%d FRAMING=%d."),
            MinimumRequiredTreeInstances,
            RainCount,
            PalmCount,
            FramingCount));
    }

    int32 HeroTreeCount = 0;
    const UHierarchicalInstancedStaticMeshComponent* TreeComponents[] = {
        RainTreeInstances.Get(),
        PalmTreeInstances.Get(),
        FramingTreeInstances.Get()};
    const double HeroRadiusCentimeters = HeroTreeRadiusMeters * 100.0;
    for (const UHierarchicalInstancedStaticMeshComponent* TreeComponent : TreeComponents)
    {
        if (!TreeComponent)
        {
            continue;
        }
        for (int32 InstanceIndex = 0;
             InstanceIndex < TreeComponent->GetInstanceCount();
             ++InstanceIndex)
        {
            FTransform InstanceTransform;
            if (TreeComponent->GetInstanceTransform(
                    InstanceIndex,
                    InstanceTransform,
                    false) &&
                InstanceTransform.GetTranslation().Size2D() <=
                    HeroRadiusCentimeters + 1.0)
            {
                ++HeroTreeCount;
            }
        }
    }
    if (bRequireLegacyTreeInstances &&
        HeroTreeCount < MinimumRequiredHeroTreeInstances)
    {
        Errors.Add(FString::Printf(
            TEXT("Hero vegetation requires at least %d volumetric trees within %.0f m; found %d."),
            MinimumRequiredHeroTreeInstances,
            HeroTreeRadiusMeters,
            HeroTreeCount));
    }

    if (ClaimLabel != ExpectedClaimLabel() ||
        ReferenceEpoch != TEXT("2024-04") ||
        bSurveyControlled ||
        bArbitraryViewIndistinguishabilityClaimed ||
        bPhotorealMaterialAcceptanceClaimed ||
        !bPbrTexturePackIntegrated ||
        MaterialImplementationTier !=
            TEXT("ORIGINAL_AI_ASSISTED_PBR_TEXTURES_INTEGRATED_PHOTO_QA_NOT_ACCEPTED") ||
        DisplayColorPolicy !=
            TEXT("NEUTRAL_CAMERA_LOCAL_EXPOSURE_DISABLED_EXTERNAL_SRGB_REC709_DISPLAY_NOT_OCIO_VALIDATED") ||
        bExternalOcioDisplayValidated ||
        PhotoQaEvidenceStatus !=
            TEXT("NOT_READY_UNCALIBRATED_COMMONS_EVIDENCE_METADATA_ONLY") ||
        bPhotoQaEvidenceConsumedByMap ||
        bContainsWholeTreeBillboards ||
        OSMContextIntegrationStatus !=
            TEXT("COORDINATE_CONTRACT_ALIGNED_MAPPING_GRADE_NOT_VISUALLY_OR_PHOTO_ACCEPTED") ||
        OSMContextAttribution != TEXT("© OpenStreetMap contributors; ODbL-1.0") ||
        bOSMContextUsedForSensorTruth ||
        bSyntheticContextBuildingsEnabled)
    {
        Errors.Add(TEXT("Public-view claim/epoch boundary is invalid."));
    }
    if (!BuildingCollisionComponent ||
        BuildingCollisionComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision ||
        BuildingCollisionComponent->IsVisible() ||
        !BuildingCollisionComponent->bHiddenInGame)
    {
        Errors.Add(TEXT("Dedicated building collision must be enabled and visually hidden."));
    }
    if (!BuildingHeroVisualComponent ||
        BuildingHeroVisualComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
    {
        Errors.Add(TEXT("Hero visual mesh must not substitute for dedicated collision."));
    }
    if (!ContextBuildingsComponent || ContextBuildingsComponent->IsVisible() ||
        !ContextBuildingsComponent->bHiddenInGame || ContextBuildingsComponent->IsActive())
    {
        Errors.Add(TEXT("Frozen synthetic context fallback must remain imported but hidden and inactive while OSM context is selected."));
    }
    FString SurroundingsPresentationReport;
    if (!ValidateV5CSurroundingsPresentation(
            SurroundingsPresentationReport))
    {
        Errors.Add(TEXT("Legacy/V5C surroundings presentation is invalid: ") +
            SurroundingsPresentationReport);
    }
    const UPrimitiveComponent* VisualOnlyComponents[] = {
        TerrainSkirtComponent.Get(),
        ContextBuildingsComponent.Get(),
        OSMContextBuildingsComponent.Get(),
        V5CSurroundingsRenderOnlyComponent.Get(),
        V5CGroundContextRenderOnlyComponent.Get(),
        RainTreeInstances.Get(),
        PalmTreeInstances.Get(),
        FramingTreeInstances.Get()};
    for (const UPrimitiveComponent* VisualOnlyComponent : VisualOnlyComponents)
    {
        if (!VisualOnlyComponent ||
            VisualOnlyComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            !IgnoresEveryCollisionChannel(VisualOnlyComponent) ||
            VisualOnlyComponent->GetGenerateOverlapEvents())
        {
            Errors.Add(TEXT("Terrain skirt, all building/ground context variants, and vegetation must be visual-only, overlap-free, and ignore every collision/trace channel."));
            break;
        }
    }
    if (SensorOcclusionPolicy !=
        TEXT("SYNTHETIC_CONTEXT_AND_VEGETATION_VISUAL_ONLY_NO_COLLISION"))
    {
        Errors.Add(TEXT("Synthetic visual-context sensor-occlusion policy marker changed."));
    }
    if (TerrainBoundaryPolicy !=
        TEXT("SYNTHETIC_DOWNWARD_VISUAL_ONLY_SKIRT_NO_COLLISION"))
    {
        Errors.Add(TEXT("Synthetic terrain-boundary policy marker changed."));
    }

    if (Errors.Num() > 0)
    {
        OutReport = TEXT("Istana public-view scene validation failed:\n - ") +
            FString::Join(Errors, TEXT("\n - "));
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Validated isolated April 2024 public-view scene: non-flat authored terrain, synthetic non-colliding one-kilometre edge skirt, hardscape, selected %s outer-context presentation, hidden frozen synthetic fallback, ten required baseline mesh roles, %d three-dimensional tree instances, and %d hero-radius trees. Every building/ground context variant is non-colliding and never sensor/RF truth; the official planning ground graphic claims no road width/material, elevation/Z, or survey authority. Material tier=%s with exact PBR graph integration; V5C surroundings and photo-QA/OCIO acceptance remain explicitly unclaimed. Claim=%s."),
        bV5CSurroundingsPresentationActive
            ? TEXT("atomic V5C official-preferred/fallback buildings plus official planning ground graphic render-only")
            : TEXT("legacy ODbL mapping-grade/non-accepted"),
        TreeCount,
        HeroTreeCount,
        *MaterialImplementationTier,
        *ClaimLabel);
    return true;
}
