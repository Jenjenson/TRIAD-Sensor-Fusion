#include "TRIADIstanaExploreV5DContextPolicyActor.h"

// Some installed Windows SDK headers leave the function-like max alias in the
// game-target preprocessor state. Cesium correctly calls numeric_limits::max,
// so remove the platform alias before its public headers are parsed.
#ifdef max
#undef max
#endif

#include "Cesium3DTileset.h"
#include "CesiumCartographicPolygon.h"
#include "CesiumGeoreference.h"
#include "CesiumIonServer.h"
#include "CesiumPolygonRasterOverlay.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Http.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Materials/MaterialInterface.h"
#include "TRIADIstanaExploreV2LandscapeActor.h"
#include "TRIADIstanaExploreV3SupplementActor.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5BVisualActor.h"
#include "TRIADIstanaExploreV5DFountainRealismActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaExploreV5DPublicRealmActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"
#include "TRIADIstanaExploreV5DCurrentSurroundingsProvenance.h"
#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.h"
#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.h"
#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"

#include <Cesium3DTilesSelection/Tileset.h>
#include <limits>
#include <memory>
#include <spdlog/spdlog.h>

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
class FScopedCesiumQuitLoggerLevel final
{
public:
    explicit FScopedCesiumQuitLoggerLevel(
        const std::shared_ptr<spdlog::logger>& InLogger)
        : Logger(InLogger),
          OriginalLevel(InLogger ? InLogger->level() : spdlog::level::off)
    {
        if (Logger && OriginalLevel < spdlog::level::critical)
        {
            Logger->set_level(spdlog::level::critical);
        }
    }

    ~FScopedCesiumQuitLoggerLevel()
    {
        if (Logger)
        {
            Logger->set_level(OriginalLevel);
        }
    }

    FScopedCesiumQuitLoggerLevel(
        const FScopedCesiumQuitLoggerLevel&) = delete;
    FScopedCesiumQuitLoggerLevel& operator=(
        const FScopedCesiumQuitLoggerLevel&) = delete;

private:
    std::shared_ptr<spdlog::logger> Logger;
    spdlog::level::level_enum OriginalLevel;
};

const FName HybridContextPolicyTag(TEXT("TRIADIstanaExploreV5DContextPolicy"));
const FName HybridVisualTilesetTag(TEXT("TRIADIstanaExploreV5DVisualTileset"));
const FName ContextPolicyR33CwtTilesetTag(
    TEXT("TRIADIstanaExploreV5DR33CesiumWorldTerrainTileset"));
const FName ContextPolicyR33CesiumContextMemberTag(
    TEXT("TRIADIstanaExploreV5DR33CesiumContextMember"));
const FName HybridGeoreferenceTag(TEXT("TRIADIstanaExploreV5DGeoreference"));
const FName HybridSiteClipTag(TEXT("TRIADIstanaExploreV5DProviderSiteClip"));
const FName CesiumDefaultGeoreferenceTag(TEXT("DEFAULT_GEOREFERENCE"));
// One-pixel geometric error is the bounded high-fidelity presentation preset.
// It may refine roughly four times as many visible tiles as the former 2 px
// threshold. The provider-safe 12-load cap and 2 GiB non-required-tile cache
// target are exact settings; tiles required for rendering may still take
// memory above the cache target, so this is not a hard process-memory claim.
constexpr int64 RequiredProviderCacheBytes =
    2LL * 1024LL * 1024LL * 1024LL;
constexpr int32 RequiredProviderSimultaneousLoads = 12;
// Retain a renderable ancestor while the bounded loader pursues target detail.
// Keep Cesium's gradual-refinement descendant limit explicit: increasing it
// can delay the visible refinement and make a larger detail tier pop in at once.
constexpr int32 RequiredProviderLoadingDescendantLimit = 20;
constexpr double RequiredProviderCulledScreenSpaceError = 8.0;
constexpr int32 RequiredProviderReadySamples = 3;
constexpr float RequiredProviderPolicyTickIntervalSeconds = 0.5f;
constexpr double RequiredIstanaLongitudeDegrees = 103.84288055;
constexpr double RequiredIstanaLatitudeDegrees = 1.30709615;
constexpr double RequiredIstanaFallbackEllipsoidHeightMeters = 47.0;
constexpr double RequiredCesiumScale = 100.0;
constexpr int32 RequiredSiteClipSplinePoints = 64;
// The reviewed OBJ deliberately carries one source vertex per face corner.
// UE 5.5 welds identical position/UV/normal tuples when it builds render data,
// so source topology and the runtime vertex buffer have different exact counts.
constexpr int32 OuterGroundLoadingFallbackSourceCornerCount = 3840;
constexpr int32 OuterGroundLoadingFallbackRenderVertexCount = 768;
constexpr double CentimetersPerMeter = 100.0;
constexpr double RequiredSiteClipCenterXMeters = 0.0;
constexpr double RequiredSiteClipCenterYMeters = 55.0;
constexpr double RequiredSiteClipSemiAxisXMeters = 185.0;
constexpr double RequiredSiteClipSemiAxisYMeters = 245.0;
constexpr double RequiredSiteClipRipple3Amplitude = 0.035;
constexpr double RequiredSiteClipRipple3PhaseRadians = 0.43;
constexpr double RequiredSiteClipRipple5Amplitude = 0.015;
constexpr double RequiredSiteClipRipple5PhaseRadians = -0.91;
constexpr double RequiredSiteClipRipple7Amplitude = 0.006;
constexpr double RequiredSiteClipRipple7PhaseRadians = 1.37;
constexpr double RequiredSiteClipSegmentParameterEpsilon = 1.0e-6;
const FString CurrentSurroundingsMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LocalFallbackSuppressionV1/"
         "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v1."
         "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v1"));
const FString CurrentSurroundingsV2MeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LocalFallbackSuppressionV2/"
         "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2."
         "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2"));
const FString OuterGroundLoadingFallbackMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback/"
         "SM_IPV5D_OuterGroundLoadingFallback_Render."
         "SM_IPV5D_OuterGroundLoadingFallback_Render"));
const FString OuterGroundLoadingFallbackMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback/"
         "Materials/M_IPV5D_OuterGroundLoadingFallback."
         "M_IPV5D_OuterGroundLoadingFallback"));
const FString V5COfficialWallMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OfficialContextRender.M_IPV5C_OfficialContextRender"));
const FString V5COfficialRoofMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OfficialContextRoof.M_IPV5C_OfficialContextRoof"));
const FString V5CGenericWallMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OsmFallbackContextRender.M_IPV5C_OsmFallbackContextRender"));
const FString V5CGenericRoofMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OsmFallbackContextRoof.M_IPV5C_OsmFallbackContextRoof"));
const FString R25OfficialWallMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/ContextFacadeR25/Materials/"
         "MI_IPV5D_ContextFacadeR25_OfficialWall."
         "MI_IPV5D_ContextFacadeR25_OfficialWall"));
const FString R25OfficialRoofMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/ContextFacadeR25/Materials/"
         "MI_IPV5D_ContextFacadeR25_OfficialRoof."
         "MI_IPV5D_ContextFacadeR25_OfficialRoof"));
const FString R25FallbackWallMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/ContextFacadeR25/Materials/"
         "MI_IPV5D_ContextFacadeR25_FallbackWall."
         "MI_IPV5D_ContextFacadeR25_FallbackWall"));
const FString R25FallbackRoofMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/ContextFacadeR25/Materials/"
         "MI_IPV5D_ContextFacadeR25_FallbackRoof."
         "MI_IPV5D_ContextFacadeR25_FallbackRoof"));
const FString R31OfficialWallMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/"
         "MI_IPV5D_R31_OfficialWall.MI_IPV5D_R31_OfficialWall"));
const FString R31OfficialRoofMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/"
         "MI_IPV5D_R31_OfficialRoof.MI_IPV5D_R31_OfficialRoof"));
const FString R31FallbackWallMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/"
         "MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall"));
const FString R31FallbackRoofMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/"
         "MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof"));

int32 NextProviderReadyConsecutiveSamples(
    float LoadProgress,
    float ReadyThreshold,
    int32 CurrentSamples,
    int32 RequiredSamples)
{
    if (!FMath::IsFinite(LoadProgress) ||
        !FMath::IsFinite(ReadyThreshold) || RequiredSamples <= 0 ||
        LoadProgress < ReadyThreshold)
    {
        return 0;
    }
    return FMath::Min(
        FMath::Max(CurrentSamples, 0) + 1,
        RequiredSamples);
}

bool ShouldRestoreLocalBuildingFallback(
    bool bFallbackCurrentlyHidden,
    float LoadProgress,
    float RestoreThreshold)
{
    return bFallbackCurrentlyHidden && FMath::IsFinite(LoadProgress) &&
        FMath::IsFinite(RestoreThreshold) &&
        LoadProgress < RestoreThreshold;
}

bool HasExpectedCurrentViewProviderWorkloadState(
    bool bIsGameWorld,
    bool bPolicyApplied,
    bool bFogCullingSnapshotValid,
    bool bFogCullingBeforePolicy,
    bool bCurrentFogCullingEnabled)
{
    if (bIsGameWorld)
    {
        return bPolicyApplied && bFogCullingSnapshotValid &&
            !bFogCullingBeforePolicy && bCurrentFogCullingEnabled;
    }
    return !bPolicyApplied && !bFogCullingSnapshotValid &&
        !bFogCullingBeforePolicy && !bCurrentFogCullingEnabled;
}

struct FCurrentSurroundingsMaterialSpec
{
    const TCHAR* Name;
    int32 Triangles;
    const FString* MaterialPath;
};

const FCurrentSurroundingsMaterialSpec CurrentSurroundingsMaterials[] = {
    {TEXT("MAT_BOTTOM_HIDDEN"), 9446, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_COMMERCIAL_HINT"), 2166, &V5CGenericWallMaterialPath},
    {TEXT("MAT_GENERIC_BUILDING_HINT"), 14696, &V5CGenericWallMaterialPath},
    {TEXT("MAT_HEALTHCARE_HINT"), 278, &V5CGenericWallMaterialPath},
    {TEXT("MAT_HOTEL_HINT"), 414, &V5COfficialWallMaterialPath},
    {TEXT("MAT_INDUSTRIAL_HINT"), 8, &V5CGenericWallMaterialPath},
    {TEXT("MAT_RELIGIOUS_HINT"), 150, &V5COfficialWallMaterialPath},
    {TEXT("MAT_RESIDENTIAL_HINT"), 6340, &V5CGenericWallMaterialPath},
    {TEXT("MAT_ROOF_COMMERCIAL_HINT"), 1029, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"), 5452, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_HEALTHCARE_HINT"), 131, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_HOTEL_HINT"), 169, &V5COfficialRoofMaterialPath},
    {TEXT("MAT_ROOF_INDUSTRIAL_HINT"), 2, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_RELIGIOUS_HINT"), 53, &V5COfficialRoofMaterialPath},
    {TEXT("MAT_ROOF_RESIDENTIAL_HINT"), 2694, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_TRANSPORT_HINT"), 132, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_TRANSPORT_HINT"), 332, &V5CGenericWallMaterialPath}};
static_assert(UE_ARRAY_COUNT(CurrentSurroundingsMaterials) == 17);

const FCurrentSurroundingsMaterialSpec CurrentSurroundingsV2Materials[] = {
    {TEXT("MAT_BOTTOM_HIDDEN"), 9436, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_COMMERCIAL_HINT"), 2166, &V5CGenericWallMaterialPath},
    {TEXT("MAT_GENERIC_BUILDING_HINT"), 14696, &V5CGenericWallMaterialPath},
    {TEXT("MAT_HEALTHCARE_HINT"), 278, &V5CGenericWallMaterialPath},
    {TEXT("MAT_HOTEL_HINT"), 414, &V5COfficialWallMaterialPath},
    {TEXT("MAT_INDUSTRIAL_HINT"), 8, &V5CGenericWallMaterialPath},
    {TEXT("MAT_RELIGIOUS_HINT"), 150, &V5COfficialWallMaterialPath},
    {TEXT("MAT_RESIDENTIAL_HINT"), 6316, &V5CGenericWallMaterialPath},
    {TEXT("MAT_ROOF_COMMERCIAL_HINT"), 1029, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"), 5452, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_HEALTHCARE_HINT"), 131, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_HOTEL_HINT"), 169, &V5COfficialRoofMaterialPath},
    {TEXT("MAT_ROOF_INDUSTRIAL_HINT"), 2, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_RELIGIOUS_HINT"), 53, &V5COfficialRoofMaterialPath},
    {TEXT("MAT_ROOF_RESIDENTIAL_HINT"), 2684, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_ROOF_TRANSPORT_HINT"), 132, &V5CGenericRoofMaterialPath},
    {TEXT("MAT_TRANSPORT_HINT"), 332, &V5CGenericWallMaterialPath}};
static_assert(UE_ARRAY_COUNT(CurrentSurroundingsV2Materials) == 17);

struct FContextFacadeR25OverrideSpec
{
    const TCHAR* SlotName;
    const FString* MaterialPath;
};

// Exact lexical static-material order inherited from the unchanged V2 mesh.
const FContextFacadeR25OverrideSpec ContextFacadeR25Overrides[] = {
    {TEXT("MAT_BOTTOM_HIDDEN"), &R25FallbackRoofMaterialPath},
    {TEXT("MAT_COMMERCIAL_HINT"), &R25FallbackWallMaterialPath},
    {TEXT("MAT_GENERIC_BUILDING_HINT"), &R25FallbackWallMaterialPath},
    {TEXT("MAT_HEALTHCARE_HINT"), &R25FallbackWallMaterialPath},
    {TEXT("MAT_HOTEL_HINT"), &R25OfficialWallMaterialPath},
    {TEXT("MAT_INDUSTRIAL_HINT"), &R25FallbackWallMaterialPath},
    {TEXT("MAT_RELIGIOUS_HINT"), &R25OfficialWallMaterialPath},
    {TEXT("MAT_RESIDENTIAL_HINT"), &R25FallbackWallMaterialPath},
    {TEXT("MAT_ROOF_COMMERCIAL_HINT"), &R25FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"), &R25FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_HEALTHCARE_HINT"), &R25FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_HOTEL_HINT"), &R25OfficialRoofMaterialPath},
    {TEXT("MAT_ROOF_INDUSTRIAL_HINT"), &R25FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_RELIGIOUS_HINT"), &R25OfficialRoofMaterialPath},
    {TEXT("MAT_ROOF_RESIDENTIAL_HINT"), &R25FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_TRANSPORT_HINT"), &R25FallbackRoofMaterialPath},
    {TEXT("MAT_TRANSPORT_HINT"), &R25FallbackWallMaterialPath}};
static_assert(UE_ARRAY_COUNT(ContextFacadeR25Overrides) == 17);

const FContextFacadeR25OverrideSpec BroadShellR31Overrides[] = {
    {TEXT("MAT_BOTTOM_HIDDEN"), &R31FallbackRoofMaterialPath},
    {TEXT("MAT_COMMERCIAL_HINT"), &R31FallbackWallMaterialPath},
    {TEXT("MAT_GENERIC_BUILDING_HINT"), &R31FallbackWallMaterialPath},
    {TEXT("MAT_HEALTHCARE_HINT"), &R31FallbackWallMaterialPath},
    {TEXT("MAT_HOTEL_HINT"), &R31OfficialWallMaterialPath},
    {TEXT("MAT_INDUSTRIAL_HINT"), &R31FallbackWallMaterialPath},
    {TEXT("MAT_RELIGIOUS_HINT"), &R31OfficialWallMaterialPath},
    {TEXT("MAT_RESIDENTIAL_HINT"), &R31FallbackWallMaterialPath},
    {TEXT("MAT_ROOF_COMMERCIAL_HINT"), &R31FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"), &R31FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_HEALTHCARE_HINT"), &R31FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_HOTEL_HINT"), &R31OfficialRoofMaterialPath},
    {TEXT("MAT_ROOF_INDUSTRIAL_HINT"), &R31FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_RELIGIOUS_HINT"), &R31OfficialRoofMaterialPath},
    {TEXT("MAT_ROOF_RESIDENTIAL_HINT"), &R31FallbackRoofMaterialPath},
    {TEXT("MAT_ROOF_TRANSPORT_HINT"), &R31FallbackRoofMaterialPath},
    {TEXT("MAT_TRANSPORT_HINT"), &R31FallbackWallMaterialPath}};
static_assert(UE_ARRAY_COUNT(BroadShellR31Overrides) == 17);

bool ValidateContextFacadeR25Overrides(
    const UStaticMeshComponent* Component,
    FString& OutError)
{
    if (!Component ||
        Component->GetNumOverrideMaterials() !=
            UE_ARRAY_COUNT(ContextFacadeR25Overrides))
    {
        OutError = TEXT("R25 context facade requires exactly 17 component material overrides.");
        return false;
    }
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(ContextFacadeR25Overrides);
         ++Index)
    {
        const FContextFacadeR25OverrideSpec& Expected =
            ContextFacadeR25Overrides[Index];
        UMaterialInterface* Material = Component->GetMaterial(Index);
        if (!Material || Material->GetPathName() != *Expected.MaterialPath)
        {
            OutError = FString::Printf(
                TEXT("R25 context-facade override %d '%s' must resolve to '%s'."),
                Index,
                Expected.SlotName,
                **Expected.MaterialPath);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateBroadShellR31Overrides(
    const UStaticMeshComponent* Component,
    FString& OutError)
{
    if (!Component ||
        Component->GetNumOverrideMaterials() !=
            UE_ARRAY_COUNT(BroadShellR31Overrides))
    {
        OutError = TEXT("R31 broad shell requires exactly 17 component material overrides.");
        return false;
    }
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(BroadShellR31Overrides);
         ++Index)
    {
        const FContextFacadeR25OverrideSpec& Expected =
            BroadShellR31Overrides[Index];
        UMaterialInterface* Material = Component->GetMaterial(Index);
        if (!Material || Material->GetPathName() != *Expected.MaterialPath)
        {
            OutError = FString::Printf(
                TEXT("R31 broad-shell override %d '%s' must resolve to '%s'."),
                Index,
                Expected.SlotName,
                **Expected.MaterialPath);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

enum class EAdmittedCurrentShellMaterialState : uint8
{
    LegacyEmbedded,
    ContextFacadeR25,
    BroadShellR31,
    Invalid
};

EAdmittedCurrentShellMaterialState ValidateAdmittedCurrentShellMaterialState(
    const UStaticMeshComponent* Component,
    FString& OutReport)
{
    if (Component && Component->GetNumOverrideMaterials() == 0)
    {
        OutReport = TEXT("legacyEmbeddedMaterials=true");
        return EAdmittedCurrentShellMaterialState::LegacyEmbedded;
    }
    FString R25Error;
    if (ValidateContextFacadeR25Overrides(Component, R25Error))
    {
        OutReport = TEXT("contextFacadeR25=true");
        return EAdmittedCurrentShellMaterialState::ContextFacadeR25;
    }
    FString R31Error;
    if (ValidateBroadShellR31Overrides(Component, R31Error))
    {
        OutReport = TEXT("broadShellR31=true");
        return EAdmittedCurrentShellMaterialState::BroadShellR31;
    }
    OutReport = TEXT("No admitted current-shell material state: ") +
        R25Error + TEXT(" ") + R31Error;
    return EAdmittedCurrentShellMaterialState::Invalid;
}

bool ValidateCurrentSurroundingsMesh(
    const UStaticMesh* Mesh,
    FString& OutError)
{
    const FString MeshPath = Mesh ? Mesh->GetPathName() : FString();
    const bool bSuppressionV1 = MeshPath == CurrentSurroundingsMeshObjectPath;
    const bool bSuppressionV2 =
        MeshPath == CurrentSurroundingsV2MeshObjectPath;
    const FCurrentSurroundingsMaterialSpec* MaterialSpecs =
        bSuppressionV2
        ? CurrentSurroundingsV2Materials
        : CurrentSurroundingsMaterials;
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
#if WITH_EDITORONLY_DATA
    const bool bHasExactFullNaniteBuildSettings =
        Mesh && Mesh->NaniteSettings.bEnabled &&
        FMath::IsNearlyEqual(
            Mesh->NaniteSettings.KeepPercentTriangles,
            1.0f) &&
        FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) &&
        Mesh->NaniteSettings.FallbackTarget ==
            ENaniteFallbackTarget::PercentTriangles &&
        FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles,
            1.0f) &&
        FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError);
#else
    const bool bHasExactFullNaniteBuildSettings = true;
#endif
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        (!bSuppressionV1 && !bSuppressionV2) ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        (bSuppressionV1 &&
         RenderData->LODResources[0].GetNumTriangles() != 43492) ||
        (bSuppressionV2 &&
         RenderData->LODResources[0].GetNumTriangles() != 43448) ||
        RenderData->LODResources[0].GetNumVertices() == 0 ||
        RenderData->LODResources[0].Sections.Num() != 17 ||
        RenderData->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.
            GetNumTexCoords() != 1 ||
        Mesh->GetStaticMaterials().Num() != 17 ||
        !Mesh->HasValidNaniteData() ||
        !bHasExactFullNaniteBuildSettings || !Body ||
        Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision())
    {
        OutError = TEXT("Current V5D surroundings successor is neither the exact suppression-V1 nor exact suppression-V2 one-LOD, nonempty-render-vertex, 17-section, one-UV0, full-Nanite/raster, zero-collision, no-navigation contract.");
        return false;
    }

    int32 V1ProvenanceCount = 0;
    int32 V2ProvenanceCount = 0;
    int32 LegacyProvenanceCount = 0;
    const UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance*
        V1Provenance = nullptr;
    const UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance*
        V2Provenance = nullptr;
    const TArray<UAssetUserData*>* AssetUserData =
        Mesh->GetAssetUserDataArray();
    if (AssetUserData)
    {
        for (const UAssetUserData* Data : *AssetUserData)
        {
            if (Data && Data->IsA(
                    UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
                        StaticClass()))
            {
                ++V1ProvenanceCount;
                V1Provenance = Cast<
                    UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance>(
                        Data);
            }
            if (Data && Data->IsA(
                    UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance::
                        StaticClass()))
            {
                ++V2ProvenanceCount;
                V2Provenance = Cast<
                    UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance>(
                        Data);
            }
            LegacyProvenanceCount += Data && Data->IsA(
                UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
                    StaticClass());
        }
    }
    const bool bValidV1Provenance = bSuppressionV1 &&
        V1ProvenanceCount == 1 && V2ProvenanceCount == 0 && V1Provenance &&
        V1Provenance->GetClass() ==
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
                StaticClass() &&
        V1Provenance->GetOuter() == Mesh &&
        !V1Provenance->HasAnyFlags(RF_Transient) &&
        V1Provenance->IsCanonicalContract();
    const bool bValidV2Provenance = bSuppressionV2 &&
        V1ProvenanceCount == 0 && V2ProvenanceCount == 1 && V2Provenance &&
        V2Provenance->GetClass() ==
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance::
                StaticClass() &&
        V2Provenance->GetOuter() == Mesh &&
        !V2Provenance->HasAnyFlags(RF_Transient) &&
        V2Provenance->IsCanonicalContract();
    if (!AssetUserData || AssetUserData->Num() != 1 ||
        LegacyProvenanceCount != 0 ||
        (!bValidV1Provenance && !bValidV2Provenance))
    {
        OutError = FString::Printf(
            TEXT("Current V5D surroundings successor must own exactly one path-matched cooked V1 or V2 suppression provenance and no other/legacy provenance; v1=%d v2=%d legacy=%d."),
            V1ProvenanceCount,
            V2ProvenanceCount,
            LegacyProvenanceCount);
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(FVector(-98438.3058, -99831.9761, 0.0), 1.0) ||
        !BoundsMax.Equals(FVector(99875.1074, 99339.8856, 15200.0), 1.0))
    {
        OutError = FString::Printf(
            TEXT("Current V5D surroundings centimetre bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            BoundsMin.X, BoundsMin.Y, BoundsMin.Z,
            BoundsMax.X, BoundsMax.Y, BoundsMax.Z);
        return false;
    }

    TArray<bool> Seen;
    Seen.Init(false, 17);
    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    for (const FStaticMeshSection& Section : Lod.Sections)
    {
        if (!Seen.IsValidIndex(Section.MaterialIndex) ||
            Seen[Section.MaterialIndex] ||
            Section.NumTriangles !=
                MaterialSpecs[Section.MaterialIndex].Triangles)
        {
            OutError = TEXT("Current V5D surroundings semantic section census drifted.");
            return false;
        }
        Seen[Section.MaterialIndex] = true;
    }
    for (int32 Index = 0; Index < 17; ++Index)
    {
        const FStaticMaterial& Material = Mesh->GetStaticMaterials()[Index];
        const UMaterialInterface* Interface = Material.MaterialInterface;
        if (!Seen[Index] ||
            Material.MaterialSlotName !=
                FName(MaterialSpecs[Index].Name) ||
            !Interface ||
            Interface->GetPathName() !=
                *MaterialSpecs[Index].MaterialPath)
        {
            OutError = FString::Printf(
                TEXT("Current V5D surroundings material slot %d lost exact semantic identity or visual-only binding."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateOuterGroundLoadingFallbackMesh(
    const UStaticMesh* Mesh,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
#if WITH_EDITORONLY_DATA
    const bool bHasExactFullNaniteBuildSettings =
        Mesh && Mesh->NaniteSettings.bEnabled &&
        FMath::IsNearlyEqual(
            Mesh->NaniteSettings.KeepPercentTriangles,
            1.0f) &&
        FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) &&
        Mesh->NaniteSettings.FallbackTarget ==
            ENaniteFallbackTarget::PercentTriangles &&
        FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles,
            1.0f) &&
        FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError);
#else
    const bool bHasExactFullNaniteBuildSettings = true;
#endif
#if WITH_EDITOR
    const bool bHasExactEditorSourceBuildSettings =
        Mesh && Mesh->GetNumSourceModels() == 1 &&
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D ==
            FVector::OneVector &&
        !Mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs &&
        Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs &&
        !Mesh->GetSourceModel(0).BuildSettings.bRecomputeNormals &&
        Mesh->GetSourceModel(0).BuildSettings.bRecomputeTangents &&
        Mesh->GetSourceModel(0).BuildSettings.bUseMikkTSpace &&
        !Mesh->GetSourceModel(0).BuildSettings.bRemoveDegenerates;
#else
    const bool bHasExactEditorSourceBuildSettings = true;
#endif
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        Mesh->GetPathName() != OuterGroundLoadingFallbackMeshObjectPath ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() != 1280 ||
        RenderData->LODResources[0].GetNumVertices() !=
            OuterGroundLoadingFallbackRenderVertexCount ||
        RenderData->LODResources[0].Sections.Num() != 1 ||
        RenderData->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.
            GetNumTexCoords() != 1 ||
        Mesh->GetStaticMaterials().Num() != 1 ||
        !Mesh->HasValidNaniteData() ||
        !bHasExactFullNaniteBuildSettings ||
        !bHasExactEditorSourceBuildSettings ||
        Mesh->GetLightMapCoordinateIndex() != 0 ||
        Mesh->bGenerateMeshDistanceField || !Body ||
        Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision())
    {
        OutError = FString::Printf(
            TEXT("Outer-ground loading fallback lost its exact one-LOD, %d-render-vertex, 1,280-triangle, one-section/UV0, full-Nanite/raster, zero-collision, no-navigation contract; the pinned source separately carries %d face-corner vertices."),
            OuterGroundLoadingFallbackRenderVertexCount,
            OuterGroundLoadingFallbackSourceCornerCount);
        return false;
    }

    const FStaticMaterial& Material = Mesh->GetStaticMaterials()[0];
    const UMaterialInterface* Interface = Material.MaterialInterface;
    const FStaticMeshSection& Section =
        RenderData->LODResources[0].Sections[0];
    bool bImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
    bImportedSlotNameMatches =
        Material.ImportedMaterialSlotName ==
        FName(TEXT("M_IPV5D_OuterGroundLoadingFallback"));
#endif
    if (Material.MaterialSlotName !=
            FName(TEXT("M_IPV5D_OuterGroundLoadingFallback")) ||
        !bImportedSlotNameMatches ||
        !Interface ||
        Interface->GetPathName() !=
            OuterGroundLoadingFallbackMaterialObjectPath ||
        Section.MaterialIndex != 0 || Section.NumTriangles != 1280 ||
        Section.bEnableCollision || Section.bCastShadow ||
        Section.bAffectDistanceFieldLighting)
    {
        OutError = TEXT("Outer-ground loading fallback lost its exact single material slot, binding, or section census.");
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(
            FVector(-125000.0, -125000.0, -51.51655292), 0.25) ||
        !BoundsMax.Equals(
            FVector(125000.0, 125000.0, 201.68388265), 0.25))
    {
        OutError = FString::Printf(
            TEXT("Outer-ground loading fallback centimetre bounds drifted: min=(%.6f,%.6f,%.6f) max=(%.6f,%.6f,%.6f)."),
            BoundsMin.X, BoundsMin.Y, BoundsMin.Z,
            BoundsMax.X, BoundsMax.Y, BoundsMax.Z);
        return false;
    }

    const TArray<UAssetUserData*>* AssetUserData =
        Mesh->GetAssetUserDataArray();
    int32 ProvenanceCount = 0;
    const UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance*
        Provenance = nullptr;
    if (AssetUserData)
    {
        for (const UAssetUserData* Data : *AssetUserData)
        {
            if (Data && Data->IsA(
                    UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance::
                        StaticClass()))
            {
                ++ProvenanceCount;
                Provenance = Cast<
                    UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance>(
                        Data);
            }
        }
    }
    if (!AssetUserData || AssetUserData->Num() != 1 ||
        ProvenanceCount != 1 || !Provenance ||
        Provenance->GetClass() !=
            UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance::
                StaticClass() ||
        Provenance->GetOuter() != Mesh ||
        Provenance->HasAnyFlags(RF_Transient) ||
        !Provenance->IsCanonicalContract())
    {
        OutError = TEXT("Outer-ground loading fallback must own exactly one canonical cooked negative-authority provenance object.");
        return false;
    }

    OutError.Reset();
    return true;
}

template <typename T>
T* FindExactlyOne(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    T* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<T> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            Result = *It;
            ++OutCount;
        }
    }
    return Result;
}

double Cross2D(const FVector2D& A, const FVector2D& B)
{
    return A.X * B.Y - A.Y * B.X;
}

bool HasExpectedIstanaGeoreferenceValues(
    EOriginPlacement OriginPlacement,
    const FVector& OriginLongitudeLatitudeHeight,
    double Scale)
{
    return OriginPlacement == EOriginPlacement::CartographicOrigin &&
        FMath::IsNearlyEqual(
            OriginLongitudeLatitudeHeight.X,
            RequiredIstanaLongitudeDegrees,
            0.00000001) &&
        FMath::IsNearlyEqual(
            OriginLongitudeLatitudeHeight.Y,
            RequiredIstanaLatitudeDegrees,
            0.00000001) &&
        FMath::IsNearlyEqual(
            OriginLongitudeLatitudeHeight.Z,
            RequiredIstanaFallbackEllipsoidHeightMeters,
            0.000001) &&
        FMath::IsNearlyEqual(Scale, RequiredCesiumScale, 0.000001);
}

bool HasExpectedVisualTilesetRoster(
    int32 TotalTilesetCount,
    int32 TaggedVisualTilesetCount)
{
    return TotalTilesetCount == 1 && TaggedVisualTilesetCount == 1;
}

bool ValidateExactIstanaGeospatialAnchor(
    UWorld* World,
    ACesium3DTileset* Tileset,
    FString& OutError)
{
    if (!World || !Tileset || Tileset->GetWorld() != World)
    {
        OutError = TEXT("V5D geospatial anchor has no exact world or tileset.");
        return false;
    }

    ACesiumGeoreference* Georeference = nullptr;
    int32 GeoreferenceCount = 0;
    for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            Georeference = *It;
            ++GeoreferenceCount;
        }
    }

    ACesiumCartographicPolygon* SiteClip = nullptr;
    int32 TaggedSiteClipCount = 0;
    for (TActorIterator<ACesiumCartographicPolygon> It(World); It; ++It)
    {
        if (IsValid(*It) && It->Tags.Contains(HybridSiteClipTag))
        {
            SiteClip = *It;
            ++TaggedSiteClipCount;
        }
    }

    if (GeoreferenceCount != 1 || TaggedSiteClipCount != 1 ||
        !Georeference || !SiteClip || !SiteClip->GlobeAnchor)
    {
        OutError = FString::Printf(
            TEXT("V5D geospatial anchor requires one unique Cesium georeference and one tagged anchored site clip; georeference=%d siteClip=%d."),
            GeoreferenceCount,
            TaggedSiteClipCount);
        return false;
    }

    if (!Georeference->Tags.Contains(HybridGeoreferenceTag) ||
        !Georeference->Tags.Contains(CesiumDefaultGeoreferenceTag) ||
        !HasExpectedIstanaGeoreferenceValues(
            Georeference->GetOriginPlacement(),
            Georeference->GetOriginLongitudeLatitudeHeight(),
            Georeference->GetScale()) ||
        Tileset->GetGeoreference().Get() != Georeference ||
        SiteClip->GlobeAnchor->GetGeoreference().Get() != Georeference ||
        SiteClip->GlobeAnchor->GetResolvedGeoreference() != Georeference ||
        !Georeference->GetActorTransform().Equals(
            FTransform::Identity,
            0.001) ||
        !Tileset->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !SiteClip->GetActorTransform().Equals(FTransform::Identity, 0.001))
    {
        OutError = TEXT("V5D geospatial anchor, explicit tileset/site-clip bindings, or identity geospatial/authored-core alignment changed.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool ValidateProviderSiteClip(
    UWorld* World,
    ACesium3DTileset* Tileset,
    FString& OutError)
{
    if (!World || !Tileset)
    {
        OutError = TEXT("V5D provider-site clip has no world or tileset.");
        return false;
    }

    ACesiumCartographicPolygon* Polygon = nullptr;
    int32 PolygonCount = 0;
    for (TActorIterator<ACesiumCartographicPolygon> It(World); It; ++It)
    {
        if (IsValid(*It) && It->Tags.Contains(HybridSiteClipTag))
        {
            Polygon = *It;
            ++PolygonCount;
        }
    }
    TArray<UCesiumPolygonRasterOverlay*> Overlays;
    Tileset->GetComponents<UCesiumPolygonRasterOverlay>(Overlays);
    if (PolygonCount != 1 || !Polygon || !Polygon->Polygon ||
        Overlays.Num() != 1 || !Overlays[0])
    {
        OutError = FString::Printf(
            TEXT("V5D provider-site clip requires one tagged polygon and one polygon overlay; polygon=%d overlay=%d."),
            PolygonCount,
            Overlays.Num());
        return false;
    }

    UCesiumPolygonRasterOverlay* Overlay = Overlays[0];
    if (Overlay->MaterialLayerKey != TEXT("Clipping") ||
        Overlay->InvertSelection || !Overlay->ExcludeSelectedTiles ||
        Overlay->Polygons.Num() != 1 || Overlay->Polygons[0].Get() != Polygon ||
        !Polygon->Polygon->IsClosedLoop() ||
        Polygon->Polygon->GetNumberOfSplinePoints() != RequiredSiteClipSplinePoints)
    {
        OutError = TEXT("V5D provider-site clipping overlay, selection, or closed spline contract changed.");
        return false;
    }
    for (int32 Index = 0; Index < RequiredSiteClipSplinePoints; ++Index)
    {
        const FVector Point = Polygon->Polygon->GetLocationAtSplinePoint(
            Index,
            ESplineCoordinateSpace::Local);
        const FVector2D Expected =
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedProviderSiteClipPointCentimeters(Index);
        if (!FMath::IsNearlyZero(Point.Z, 0.01) ||
            !FVector2D(Point.X, Point.Y).Equals(Expected, 0.1) ||
            Polygon->Polygon->GetSplinePointType(Index) !=
                ESplinePointType::Linear)
        {
            OutError = FString::Printf(
                TEXT("V5D provider-site clip point %d left the exact linear irregular-ellipse footprint."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}
}

int32 ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedProviderSiteClipSplinePoints()
{
    return RequiredSiteClipSplinePoints;
}

FVector2D ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedProviderSiteClipCenterCentimeters()
{
    return FVector2D(
        RequiredSiteClipCenterXMeters * CentimetersPerMeter,
        RequiredSiteClipCenterYMeters * CentimetersPerMeter);
}

FVector2D ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedProviderSiteClipSemiAxesCentimeters()
{
    return FVector2D(
        RequiredSiteClipSemiAxisXMeters * CentimetersPerMeter,
        RequiredSiteClipSemiAxisYMeters * CentimetersPerMeter);
}

FVector ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedProviderSiteClipRippleAmplitudes()
{
    return FVector(
        RequiredSiteClipRipple3Amplitude,
        RequiredSiteClipRipple5Amplitude,
        RequiredSiteClipRipple7Amplitude);
}

FVector ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedProviderSiteClipRipplePhasesRadians()
{
    return FVector(
        RequiredSiteClipRipple3PhaseRadians,
        RequiredSiteClipRipple5PhaseRadians,
        RequiredSiteClipRipple7PhaseRadians);
}

double ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedProviderSiteClipSegmentParameterEpsilon()
{
    return RequiredSiteClipSegmentParameterEpsilon;
}

FVector2D ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedProviderSiteClipPointCentimeters(int32 Index)
{
    const int32 NormalizedIndex =
        ((Index % RequiredSiteClipSplinePoints) +
            RequiredSiteClipSplinePoints) %
        RequiredSiteClipSplinePoints;
    const double Theta = 2.0 * UE_DOUBLE_PI *
        static_cast<double>(NormalizedIndex) /
        static_cast<double>(RequiredSiteClipSplinePoints);
    const double RadialScale = 1.0 +
        RequiredSiteClipRipple3Amplitude * FMath::Sin(
            3.0 * Theta + RequiredSiteClipRipple3PhaseRadians) +
        RequiredSiteClipRipple5Amplitude * FMath::Sin(
            5.0 * Theta + RequiredSiteClipRipple5PhaseRadians) +
        RequiredSiteClipRipple7Amplitude * FMath::Sin(
            7.0 * Theta + RequiredSiteClipRipple7PhaseRadians);
    const FVector2D Center = ExpectedProviderSiteClipCenterCentimeters();
    const FVector2D SemiAxes = ExpectedProviderSiteClipSemiAxesCentimeters();
    return Center + FVector2D(
        RadialScale * SemiAxes.X * FMath::Cos(Theta),
        RadialScale * SemiAxes.Y * FMath::Sin(Theta));
}

double ATRIADIstanaExploreV5DContextPolicyActor::
    EvaluateProviderSiteClipSignedInwardDistanceCentimeters(
        const FVector2D& WorldXYCentimeters)
{
    if (!FMath::IsFinite(WorldXYCentimeters.X) ||
        !FMath::IsFinite(WorldXYCentimeters.Y))
    {
        return -TNumericLimits<double>::Max();
    }

    const FVector2D Centered =
        WorldXYCentimeters - ExpectedProviderSiteClipCenterCentimeters();
    const FVector2D SemiAxes =
        ExpectedProviderSiteClipSemiAxesCentimeters();
    const double RadialDistance = Centered.Length();
    if (RadialDistance <= UE_DOUBLE_SMALL_NUMBER)
    {
        const double MaximumRippleAmplitude =
            RequiredSiteClipRipple3Amplitude +
            RequiredSiteClipRipple5Amplitude +
            RequiredSiteClipRipple7Amplitude;
        return FMath::Min(SemiAxes.X, SemiAxes.Y) *
            (1.0 - MaximumRippleAmplitude);
    }

    // Every vertex is positiveScalar(theta) * (a*cos(theta), b*sin(theta)).
    // Dividing by the semi-axes therefore recovers the monotonic parametric
    // direction despite the ripple, and exactly brackets the adjacent linear
    // polygon edge without scanning all 64 segments.
    double ParametricTheta = FMath::Atan2(
        Centered.Y / SemiAxes.Y,
        Centered.X / SemiAxes.X);
    if (ParametricTheta < 0.0)
    {
        ParametricTheta += 2.0 * UE_DOUBLE_PI;
    }
    const double SectorRadians = 2.0 * UE_DOUBLE_PI /
        static_cast<double>(RequiredSiteClipSplinePoints);
    const int32 SectorIndex = FMath::Clamp(
        FMath::FloorToInt(ParametricTheta / SectorRadians),
        0,
        RequiredSiteClipSplinePoints - 1);
    const FVector2D Center = ExpectedProviderSiteClipCenterCentimeters();
    const FVector2D P0 =
        ExpectedProviderSiteClipPointCentimeters(SectorIndex) - Center;
    const FVector2D P1 =
        ExpectedProviderSiteClipPointCentimeters(SectorIndex + 1) - Center;
    const FVector2D Edge = P1 - P0;
    const FVector2D Ray = Centered / RadialDistance;
    const double Denominator = Cross2D(Ray, Edge);
    if (!FMath::IsFinite(Denominator) ||
        FMath::Abs(Denominator) <= UE_DOUBLE_SMALL_NUMBER)
    {
        return -TNumericLimits<double>::Max();
    }

    const double RawSegmentParameter = Cross2D(P0, Ray) / Denominator;
    const double BoundaryRayDistance = Cross2D(P0, Edge) / Denominator;
    const double SegmentParameterEpsilon =
        ExpectedProviderSiteClipSegmentParameterEpsilon();
    if (!FMath::IsFinite(RawSegmentParameter) ||
        !FMath::IsFinite(BoundaryRayDistance) ||
        RawSegmentParameter < -SegmentParameterEpsilon ||
        RawSegmentParameter > 1.0 + SegmentParameterEpsilon ||
        BoundaryRayDistance <= 0.0)
    {
        return -TNumericLimits<double>::Max();
    }
    const double SegmentParameter = FMath::Clamp(
        RawSegmentParameter,
        0.0,
        1.0);
    if (SegmentParameter < 0.0 || SegmentParameter > 1.0)
    {
        return -TNumericLimits<double>::Max();
    }
    return BoundaryRayDistance - RadialDistance;
}

const FString& ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedCurrentSurroundingsMeshObjectPath()
{
    return CurrentSurroundingsMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedCurrentSurroundingsV2MeshObjectPath()
{
    return CurrentSurroundingsV2MeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DContextPolicyActor::
    ExpectedOuterGroundLoadingFallbackMeshObjectPath()
{
    return OuterGroundLoadingFallbackMeshObjectPath;
}

ATRIADIstanaExploreV5DContextPolicyActor::
    ATRIADIstanaExploreV5DContextPolicyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = RequiredProviderPolicyTickIntervalSeconds;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Static);
    CurrentSurroundingsRenderOnlyComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("CurrentSurroundingsRenderOnly"));
    CurrentSurroundingsRenderOnlyComponent->SetupAttachment(RootComponent);
    CurrentSurroundingsRenderOnlyComponent->SetMobility(
        EComponentMobility::Static);
    CurrentSurroundingsRenderOnlyComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);
    CurrentSurroundingsRenderOnlyComponent->SetCollisionResponseToAllChannels(
        ECR_Ignore);
    CurrentSurroundingsRenderOnlyComponent->SetGenerateOverlapEvents(false);
    CurrentSurroundingsRenderOnlyComponent->SetCanEverAffectNavigation(false);
    CurrentSurroundingsRenderOnlyComponent->SetCastShadow(true);
    CurrentSurroundingsRenderOnlyComponent->SetRenderInMainPass(true);
    CurrentSurroundingsRenderOnlyComponent->EmptyOverrideMaterials();
    CurrentSurroundingsRenderOnlyComponent->SetOverlayMaterial(nullptr);
    CurrentSurroundingsRenderOnlyComponent->SetVisibility(true, true);
    CurrentSurroundingsRenderOnlyComponent->SetHiddenInGame(false, true);
    CurrentSurroundingsRenderOnlyComponent->ComponentTags.AddUnique(
        TEXT("TRIADV5DCurrentPublicContextRenderOnly"));

    OuterGroundLoadingFallbackRenderOnlyComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("OuterGroundLoadingFallbackRenderOnly"));
    OuterGroundLoadingFallbackRenderOnlyComponent->SetupAttachment(
        RootComponent);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetMobility(
        EComponentMobility::Static);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);
    OuterGroundLoadingFallbackRenderOnlyComponent->
        SetCollisionResponseToAllChannels(ECR_Ignore);
    OuterGroundLoadingFallbackRenderOnlyComponent->
        SetGenerateOverlapEvents(false);
    OuterGroundLoadingFallbackRenderOnlyComponent->
        SetCanEverAffectNavigation(false);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetCastShadow(false);
    OuterGroundLoadingFallbackRenderOnlyComponent->bCastContactShadow = false;
    OuterGroundLoadingFallbackRenderOnlyComponent->
        bAffectDistanceFieldLighting = false;
    OuterGroundLoadingFallbackRenderOnlyComponent->
        bAffectDynamicIndirectLighting = false;
    OuterGroundLoadingFallbackRenderOnlyComponent->SetRenderInMainPass(true);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetRenderCustomDepth(false);
    OuterGroundLoadingFallbackRenderOnlyComponent->EmptyOverrideMaterials();
    OuterGroundLoadingFallbackRenderOnlyComponent->SetOverlayMaterial(nullptr);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetVisibility(true, true);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetHiddenInGame(false, true);
    OuterGroundLoadingFallbackRenderOnlyComponent->ComponentTags.AddUnique(
        TEXT("TRIADV5DOuterGroundLoadingFallbackRenderOnly"));
    Tags.AddUnique(HybridContextPolicyTag);
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ConfigureCurrentSurroundingsPresentation(
        UStaticMesh* Mesh,
        ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutError)
{
    return ConfigureCurrentSurroundingsAndOuterGroundPresentation(
        Mesh,
        OuterGroundLoadingFallbackRenderOnlyComponent
            ? OuterGroundLoadingFallbackRenderOnlyComponent->GetStaticMesh()
            : nullptr,
        Scene,
        OutError);
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ConfigureCurrentSurroundingsAndOuterGroundPresentation(
        UStaticMesh* CurrentSurroundingsMesh,
        UStaticMesh* OuterGroundLoadingFallbackMesh,
        ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutError)
{
    if (!Scene || !CurrentSurroundingsRenderOnlyComponent ||
        !OuterGroundLoadingFallbackRenderOnlyComponent ||
        !ValidateCurrentSurroundingsMesh(CurrentSurroundingsMesh, OutError) ||
        !ValidateOuterGroundLoadingFallbackMesh(
            OuterGroundLoadingFallbackMesh,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D fallback configuration requires the exact scene, both native components, and both validated successor meshes.");
        }
        return false;
    }
#if WITH_EDITOR
    const bool bActorHiddenInEditor = IsHiddenEd();
#else
    const bool bActorHiddenInEditor = false;
#endif
    if (!GetWorld() || Scene->GetWorld() != GetWorld() ||
        !RootComponent ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        IsHidden() || bActorHiddenInEditor ||
        RootComponent->Mobility != EComponentMobility::Static ||
        CurrentSurroundingsRenderOnlyComponent->GetAttachParent() !=
            RootComponent ||
        CurrentSurroundingsRenderOnlyComponent->Mobility !=
            EComponentMobility::Static ||
        !CurrentSurroundingsRenderOnlyComponent->ComponentTags.Contains(
            TEXT("TRIADV5DCurrentPublicContextRenderOnly")) ||
        CurrentSurroundingsRenderOnlyComponent->ComponentTags.Contains(
            TEXT("TRIADHumanOnlyOverlay")) ||
        OuterGroundLoadingFallbackRenderOnlyComponent->GetAttachParent() !=
            RootComponent ||
        OuterGroundLoadingFallbackRenderOnlyComponent->Mobility !=
            EComponentMobility::Static ||
        !OuterGroundLoadingFallbackRenderOnlyComponent->ComponentTags.Contains(
            TEXT("TRIADV5DOuterGroundLoadingFallbackRenderOnly")) ||
        OuterGroundLoadingFallbackRenderOnlyComponent->ComponentTags.Contains(
            TEXT("TRIADHumanOnlyOverlay")) ||
        !bCurrentSurroundingsRenderOnly ||
        bCurrentSurroundingsCollisionNavigationSensorOrRfAuthority ||
        bCurrentSurroundingsMeasuredSurveyAsBuiltOrHyperreal ||
        !bOuterGroundLoadingFallbackRenderOnly ||
        bOuterGroundLoadingFallbackCollisionNavigationSensorRfTerrainAuthority ||
        bOuterGroundLoadingFallbackSurveyAsBuilt ||
        !Scene->ContextBuildingsComponent ||
        !Scene->OSMContextBuildingsComponent ||
        !Scene->V5CSurroundingsRenderOnlyComponent ||
        !Scene->V5CGroundContextRenderOnlyComponent)
    {
        OutError = TEXT("V5D fallback configuration preflight rejected actor, component, scene, or negative-authority drift before mutation.");
        return false;
    }
    CurrentSurroundingsRenderOnlyComponent->SetStaticMesh(
        CurrentSurroundingsMesh);
    CurrentSurroundingsRenderOnlyComponent->SetRelativeTransform(
        FTransform::Identity);
    CurrentSurroundingsRenderOnlyComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);
    CurrentSurroundingsRenderOnlyComponent->SetCollisionResponseToAllChannels(
        ECR_Ignore);
    CurrentSurroundingsRenderOnlyComponent->SetGenerateOverlapEvents(false);
    CurrentSurroundingsRenderOnlyComponent->SetCanEverAffectNavigation(false);
    CurrentSurroundingsRenderOnlyComponent->SetCastShadow(true);
    CurrentSurroundingsRenderOnlyComponent->SetRenderInMainPass(true);
    CurrentSurroundingsRenderOnlyComponent->EmptyOverrideMaterials();
    CurrentSurroundingsRenderOnlyComponent->SetOverlayMaterial(nullptr);

    OuterGroundLoadingFallbackRenderOnlyComponent->SetStaticMesh(
        OuterGroundLoadingFallbackMesh);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetRelativeTransform(
        FTransform::Identity);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);
    OuterGroundLoadingFallbackRenderOnlyComponent->
        SetCollisionResponseToAllChannels(ECR_Ignore);
    OuterGroundLoadingFallbackRenderOnlyComponent->
        SetGenerateOverlapEvents(false);
    OuterGroundLoadingFallbackRenderOnlyComponent->
        SetCanEverAffectNavigation(false);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetCastShadow(false);
    OuterGroundLoadingFallbackRenderOnlyComponent->bCastContactShadow = false;
    OuterGroundLoadingFallbackRenderOnlyComponent->
        bAffectDistanceFieldLighting = false;
    OuterGroundLoadingFallbackRenderOnlyComponent->
        bAffectDynamicIndirectLighting = false;
    OuterGroundLoadingFallbackRenderOnlyComponent->SetRenderInMainPass(true);
    OuterGroundLoadingFallbackRenderOnlyComponent->SetRenderCustomDepth(false);
    OuterGroundLoadingFallbackRenderOnlyComponent->EmptyOverrideMaterials();
    OuterGroundLoadingFallbackRenderOnlyComponent->SetOverlayMaterial(nullptr);
    bLocalBuildingFallbackCurrentlyHidden = false;
    ProviderReadyConsecutiveSamples = 0;
    SetLocalBuildingFallbackVisible(Scene, true);
    return ValidateCurrentSurroundingsPresentation(Scene, OutError);
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ApplyCurrentSurroundingsContextFacadeR25(FString& OutError)
{
    UStaticMeshComponent* Component =
        CurrentSurroundingsRenderOnlyComponent;
    UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
    if (!Component || !Mesh ||
        Mesh->GetPathName() != CurrentSurroundingsV2MeshObjectPath ||
        (Component->GetNumOverrideMaterials() != 0 &&
         Component->GetNumOverrideMaterials() !=
             UE_ARRAY_COUNT(ContextFacadeR25Overrides)))
    {
        OutError = TEXT("R25 context-facade apply requires the exact V2 render-only mesh and either zero or 17 existing overrides.");
        return false;
    }

    FString ExistingReport;
    if (ValidateContextFacadeR25Overrides(Component, ExistingReport))
    {
        OutError.Reset();
        return true;
    }
    if (Component->GetNumOverrideMaterials() != 0)
    {
        OutError = TEXT("R25 context-facade apply refused a partial or drifted 17-entry override roster. ") +
            ExistingReport;
        return false;
    }

    TArray<UMaterialInterface*> ExactMaterials;
    ExactMaterials.Reserve(UE_ARRAY_COUNT(ContextFacadeR25Overrides));
    for (const FContextFacadeR25OverrideSpec& Expected :
         ContextFacadeR25Overrides)
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(
            nullptr, **Expected.MaterialPath);
        if (!Material || Material->GetPathName() != *Expected.MaterialPath)
        {
            OutError = TEXT("R25 context-facade apply could not load exact material '") +
                *Expected.MaterialPath + TEXT("'.");
            return false;
        }
        ExactMaterials.Add(Material);
    }

    Component->Modify();
    Component->EmptyOverrideMaterials();
    for (int32 Index = 0; Index < ExactMaterials.Num(); ++Index)
    {
        Component->SetMaterial(Index, ExactMaterials[Index]);
    }
    if (!ValidateContextFacadeR25Overrides(Component, OutError))
    {
        Component->EmptyOverrideMaterials();
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ApplyCurrentSurroundingsBroadShellR31(FString& OutError)
{
    UStaticMeshComponent* Component =
        CurrentSurroundingsRenderOnlyComponent;
    UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
    FString ExistingR25Report;
    FString ExistingR31Report;
    const bool bHasR25 =
        ValidateContextFacadeR25Overrides(Component, ExistingR25Report);
    const bool bHasR31 =
        ValidateBroadShellR31Overrides(Component, ExistingR31Report);
    if (!Component || !Mesh ||
        Mesh->GetPathName() != CurrentSurroundingsV2MeshObjectPath ||
        (!bHasR25 && !bHasR31))
    {
        OutError = TEXT("R31 broad-shell apply requires the exact V2 render-only mesh and a complete R25 or already-R31 17-entry override roster. ") +
            ExistingR25Report + TEXT(" ") + ExistingR31Report;
        return false;
    }
    if (bHasR31)
    {
        OutError.Reset();
        return true;
    }

    TArray<UMaterialInterface*> ExactMaterials;
    ExactMaterials.Reserve(UE_ARRAY_COUNT(BroadShellR31Overrides));
    for (const FContextFacadeR25OverrideSpec& Expected :
         BroadShellR31Overrides)
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(
            nullptr, **Expected.MaterialPath);
        if (!Material || Material->GetPathName() != *Expected.MaterialPath)
        {
            OutError = TEXT("R31 broad-shell apply could not load exact material '") +
                *Expected.MaterialPath + TEXT("'.");
            return false;
        }
        ExactMaterials.Add(Material);
    }

    TArray<UMaterialInterface*> PreviousMaterials;
    PreviousMaterials.Reserve(Component->GetNumOverrideMaterials());
    for (int32 Index = 0;
         Index < Component->GetNumOverrideMaterials();
         ++Index)
    {
        PreviousMaterials.Add(Component->GetMaterial(Index));
    }
    Component->Modify();
    Component->EmptyOverrideMaterials();
    for (int32 Index = 0; Index < ExactMaterials.Num(); ++Index)
    {
        Component->SetMaterial(Index, ExactMaterials[Index]);
    }
    if (!ValidateBroadShellR31Overrides(Component, OutError))
    {
        Component->EmptyOverrideMaterials();
        for (int32 Index = 0; Index < PreviousMaterials.Num(); ++Index)
        {
            Component->SetMaterial(Index, PreviousMaterials[Index]);
        }
        FString RollbackReport;
        if (!ValidateContextFacadeR25Overrides(Component, RollbackReport))
        {
            OutError += TEXT(" R31_BROAD_SHELL_COMPONENT_ROLLBACK_INCOMPLETE: ") +
                RollbackReport;
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    SuppressInheritedPlanningGroundPresentation(
        ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutError)
{
    UStaticMeshComponent* PlanningGround = Scene
        ? Scene->V5CGroundContextRenderOnlyComponent
        : nullptr;
    if (!PlanningGround)
    {
        OutError = TEXT("V5D planning-ground suppression requires the exact inherited V5C render-only component.");
        return false;
    }
    PlanningGround->SetVisibility(false, true);
    PlanningGround->SetHiddenInGame(true, true);
    if (PlanningGround->IsVisible() || !PlanningGround->bHiddenInGame)
    {
        OutError = TEXT("V5D planning-ground suppression did not produce visible=false and hiddenInGame=true readbacks.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::ResolveSceneAndTileset(
    ATRIADIstanaPublicViewSceneActor*& OutScene,
    ACesium3DTileset*& OutTileset,
    FString& OutError) const
{
    OutScene = nullptr;
    OutTileset = nullptr;
    UWorld* World = GetWorld();
    if (!World)
    {
        OutError = TEXT("V5D hybrid context has no world.");
        return false;
    }

    int32 SceneCount = 0;
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            OutScene = *It;
            ++SceneCount;
        }
    }
    int32 TotalTilesetCount = 0;
    int32 TaggedVisualTilesetCount = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            ++TotalTilesetCount;
            if (It->Tags.Contains(HybridVisualTilesetTag))
            {
                OutTileset = *It;
                ++TaggedVisualTilesetCount;
            }
        }
    }
    if (SceneCount != 1 ||
        !HasExpectedVisualTilesetRoster(
            TotalTilesetCount,
            TaggedVisualTilesetCount) ||
        !OutScene || !OutTileset)
    {
        OutError = FString::Printf(
            TEXT("V5D hybrid context requires exactly one Istana scene and exactly one Cesium tileset, which must be the tagged visual provider; scene=%d totalTilesets=%d taggedVisualTilesets=%d."),
            SceneCount,
            TotalTilesetCount,
            TaggedVisualTilesetCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::ResolveSceneAndR33Tilesets(
    ATRIADIstanaPublicViewSceneActor*& OutScene,
    ACesium3DTileset*& OutGoogleTileset,
    ACesium3DTileset*& OutCwtTileset,
    FString& OutError) const
{
    OutScene = nullptr;
    OutGoogleTileset = nullptr;
    OutCwtTileset = nullptr;
    UWorld* World = GetWorld();
    if (!World)
    {
        OutError = TEXT("V5D R33 visual context has no world.");
        return false;
    }

    int32 SceneCount = 0;
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            OutScene = *It;
            ++SceneCount;
        }
    }

    int32 TotalTilesetCount = 0;
    int32 ContextMemberCount = 0;
    int32 GoogleRoleCount = 0;
    int32 CwtRoleCount = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        ++TotalTilesetCount;
        ContextMemberCount +=
            Candidate->Tags.Contains(
                ContextPolicyR33CesiumContextMemberTag) ? 1 : 0;
        if (Candidate->Tags.Contains(HybridVisualTilesetTag))
        {
            OutGoogleTileset = Candidate;
            ++GoogleRoleCount;
        }
        if (Candidate->Tags.Contains(ContextPolicyR33CwtTilesetTag))
        {
            OutCwtTileset = Candidate;
            ++CwtRoleCount;
        }
    }

    if (SceneCount != 1 || TotalTilesetCount != 2 ||
        ContextMemberCount != 2 || GoogleRoleCount != 1 ||
        CwtRoleCount != 1 || !OutScene || !OutGoogleTileset ||
        !OutCwtTileset || OutGoogleTileset == OutCwtTileset ||
        OutGoogleTileset->Tags.Contains(ContextPolicyR33CwtTilesetTag) ||
        OutCwtTileset->Tags.Contains(HybridVisualTilesetTag) ||
        OutGoogleTileset->GetIonAssetID() != 2275207 ||
        OutCwtTileset->GetIonAssetID() != 1 ||
        !IsValid(OutGoogleTileset->GetCesiumIonServer()) ||
        !IsValid(OutCwtTileset->GetCesiumIonServer()) ||
        OutGoogleTileset->GetCesiumIonServer() !=
            OutCwtTileset->GetCesiumIonServer())
    {
        OutError = FString::Printf(
            TEXT("V5D R33 context requires one scene and exactly two distinct common-member tilesets: Google asset 2275207 and CWT asset 1; scene=%d totalTilesets=%d members=%d google=%d cwt=%d."),
            SceneCount,
            TotalTilesetCount,
            ContextMemberCount,
            GoogleRoleCount,
            CwtRoleCount);
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ApplyTransientCurrentViewProviderWorkloadPolicy(
        ACesium3DTileset* Tileset,
        FString& OutError)
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || !Tileset ||
        Tileset->GetWorld() != World)
    {
        OutError = TEXT("V5D transient current-view provider workload policy requires the exact game-world tileset.");
        return false;
    }

    if (bTransientCurrentViewProviderWorkloadPolicyApplied)
    {
        if (!HasExpectedCurrentViewProviderWorkloadState(
                true,
                bTransientCurrentViewProviderWorkloadPolicyApplied,
                bProviderFogCullingSnapshotValid,
                bProviderFogCullingBeforeTransientPolicy,
                Tileset->EnableFogCulling))
        {
            OutError = TEXT("V5D transient current-view provider workload policy was marked applied but its exact runtime state drifted.");
            return false;
        }
        OutError.Reset();
        return true;
    }

    // Refuse to reinterpret a different saved provider-quality tuple. This
    // runtime-only policy changes one direct field after the exact serialized
    // V5D settings are observed; it never calls Modify, RefreshTileset, or a
    // setter that could rebuild or save the provider source.
    if (bProviderFogCullingSnapshotValid ||
        ExpectedIonAssetId != 2275207 ||
        !FMath::IsNearlyEqual(
            HideLocalBuildingFallbackAtLoadProgress,
            98.0f) ||
        !FMath::IsNearlyEqual(
            RestoreLocalBuildingFallbackBelowLoadProgress,
            90.0f) ||
        RequiredProviderReadyConsecutiveSamples !=
            RequiredProviderReadySamples ||
        !bCesiumLayerIsVisualOnly ||
        bCesiumCollisionNavigationSensorOrRfAuthority ||
        !bCesiumStandardPersistentHttpRequestCacheAcknowledged ||
        bTriadReadSerializedOrLoggedProviderToken ||
        bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent ||
        !bProviderContentClippedFromAuthoredCore ||
        Tileset->GetTilesetSource() != ETilesetSource::FromCesiumIon ||
        Tileset->GetIonAssetID() != ExpectedIonAssetId ||
        !FMath::IsNearlyEqual(
            Tileset->GetMaximumScreenSpaceError(),
            RequiredMaximumScreenSpaceError,
            0.000001) ||
        Tileset->ApplyDpiScaling != EApplyDpiScaling::No ||
        Tileset->GetCreatePhysicsMeshes() ||
        Tileset->GetCreateNavCollision() ||
        Tileset->MaximumCachedBytes != RequiredProviderCacheBytes ||
        Tileset->MaximumSimultaneousTileLoads !=
            RequiredProviderSimultaneousLoads ||
        !Tileset->ForbidHoles ||
        Tileset->LoadingDescendantLimit !=
            RequiredProviderLoadingDescendantLimit ||
        !Tileset->ShowCreditsOnScreen ||
        !Tileset->PreloadAncestors || Tileset->PreloadSiblings ||
        !Tileset->EnableFrustumCulling || Tileset->EnableFogCulling ||
        !Tileset->EnforceCulledScreenSpaceError ||
        !FMath::IsNearlyEqual(
            Tileset->CulledScreenSpaceError,
            RequiredProviderCulledScreenSpaceError,
            0.000001) ||
        Tileset->GetUseLodTransitions() ||
        Tileset->GetIgnoreKhrMaterialsUnlit() ||
        Tileset->GetGenerateSmoothNormals())
    {
        OutError = TEXT("V5D transient current-view provider workload policy refused a tileset that did not match the exact saved visual-only quality tuple.");
        return false;
    }

    FString GeospatialAnchorError;
    if (!ValidateExactIstanaGeospatialAnchor(
            World,
            Tileset,
            GeospatialAnchorError))
    {
        OutError =
            TEXT("V5D transient current-view provider workload policy refused an invalid geospatial anchor before mutation: ") +
            GeospatialAnchorError;
        return false;
    }

    FString SiteClipError;
    if (!ValidateProviderSiteClip(World, Tileset, SiteClipError))
    {
        OutError =
            TEXT("V5D transient current-view provider workload policy refused an invalid saved authored-core clipping footprint: ") +
            SiteClipError;
        return false;
    }

    bProviderFogCullingBeforeTransientPolicy =
        Tileset->EnableFogCulling;
    bProviderFogCullingSnapshotValid = true;
    Tileset->EnableFogCulling = true;
    bTransientCurrentViewProviderWorkloadPolicyApplied = true;
    if (!HasExpectedCurrentViewProviderWorkloadState(
            true,
            bTransientCurrentViewProviderWorkloadPolicyApplied,
            bProviderFogCullingSnapshotValid,
            bProviderFogCullingBeforeTransientPolicy,
            Tileset->EnableFogCulling))
    {
        Tileset->EnableFogCulling =
            bProviderFogCullingBeforeTransientPolicy;
        bTransientCurrentViewProviderWorkloadPolicyApplied = false;
        bProviderFogCullingSnapshotValid = false;
        bProviderFogCullingBeforeTransientPolicy = false;
        OutError = TEXT("V5D transient current-view provider workload policy did not produce the exact runtime readback and was restored.");
        return false;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("ISTANA_EXPLORE_V5D_CURRENT_VIEW_PROVIDER_WORKLOAD_APPLY_PASS "
              "transient=true savedFogCulling=false runtimeFogCulling=true "
              "cesiumGeoreference=(103.84288055,1.30709615,47.000) "
              "geospatialAnchorValidated=true "
              "geospatialActorTransformsIdentity=true "
              "uniqueCesiumTilesetRoster=true "
              "visibleMaximumSse=1.0 forbidHoles=true "
             "providerReadyThresholdPercent=98.0 "
             "providerReadyRequiredConsecutiveSamples=3 "
             "collisionNavigationSensorRfAuthority=false"));
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    RestoreTransientCurrentViewProviderWorkloadPolicy(FString& OutError)
{
    if (!bTransientCurrentViewProviderWorkloadPolicyApplied &&
        !bProviderFogCullingSnapshotValid)
    {
        bProviderFogCullingBeforeTransientPolicy = false;
        OutError.Reset();
        return true;
    }

    ACesium3DTileset* Tileset = CachedTileset.Get();
    if (!Tileset || Tileset->GetWorld() != GetWorld() ||
        !bProviderFogCullingSnapshotValid ||
        bProviderFogCullingBeforeTransientPolicy)
    {
        OutError = TEXT("V5D transient current-view provider workload policy could not prove its exact saved fog-culling snapshot during restore.");
        return false;
    }

    Tileset->EnableFogCulling =
        bProviderFogCullingBeforeTransientPolicy;
    if (Tileset->EnableFogCulling)
    {
        OutError = TEXT("V5D transient current-view provider workload policy failed to restore saved fog-culling=false.");
        return false;
    }

    bTransientCurrentViewProviderWorkloadPolicyApplied = false;
    bProviderFogCullingSnapshotValid = false;
    bProviderFogCullingBeforeTransientPolicy = false;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("ISTANA_EXPLORE_V5D_CURRENT_VIEW_PROVIDER_WORKLOAD_RESTORE_PASS "
             "savedFogCulling=false runtimeMutationPersisted=false "
             "collisionNavigationSensorRfAuthority=false"));
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ResolveProviderSiteClipOverlay(
        ACesium3DTileset* Tileset,
        UCesiumPolygonRasterOverlay*& OutOverlay,
        FString& OutError) const
{
    OutOverlay = nullptr;
    TArray<UCesiumPolygonRasterOverlay*> Overlays;
    if (Tileset)
    {
        Tileset->GetComponents<UCesiumPolygonRasterOverlay>(Overlays);
    }
    if (Overlays.Num() != 1 || !Overlays[0])
    {
        OutError = FString::Printf(
            TEXT("V5D aerial handoff requires the one exact provider-site clipping overlay; found %d."),
            Overlays.Num());
        return false;
    }
    OutOverlay = Overlays[0];
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::ResolvePublicRealmActor(
    ATRIADIstanaExploreV5DPublicRealmActor*& OutPublicRealm,
    FString& OutError) const
{
    int32 PublicRealmCount = 0;
    OutPublicRealm = FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
        GetWorld(),
        PublicRealmCount);
    FString PublicRealmReport;
    if (PublicRealmCount != 1 || !OutPublicRealm ||
        !OutPublicRealm->ValidatePublicRealm(PublicRealmReport))
    {
        OutError = FString::Printf(
            TEXT("V5D visual context requires exactly one valid configured public-realm actor; count=%d report={%s}."),
            PublicRealmCount,
            *PublicRealmReport);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ResolveOptionalR28EnvironmentActor(
        ATRIADIstanaExploreV5DR28EnvironmentActor*& OutEnvironment,
        FString& OutError) const
{
    if (!GetWorld())
    {
        OutEnvironment = nullptr;
        OutError = TEXT("V5D optional R28 environment resolution has no world.");
        return false;
    }
    int32 R28EnvironmentCount = 0;
    OutEnvironment =
        FindExactlyOne<ATRIADIstanaExploreV5DR28EnvironmentActor>(
            GetWorld(),
            R28EnvironmentCount);
    if (R28EnvironmentCount > 1)
    {
        OutEnvironment = nullptr;
        OutError = FString::Printf(
            TEXT("V5D provider readiness permits zero or one R28 render-only environment actor, never more; count=%d."),
            R28EnvironmentCount);
        return false;
    }
    if (R28EnvironmentCount == 0)
    {
        OutError.Reset();
        return true;
    }

    FString R28Report;
    if (!OutEnvironment || OutEnvironment->GetWorld() != GetWorld() ||
        !OutEnvironment->Tags.Contains(
            ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag()) ||
        !OutEnvironment->ValidateR28Environment(R28Report))
    {
        OutError = FString::Printf(
            TEXT("V5D optional R28 environment actor is present but invalid; count=%d report={%s}."),
            R28EnvironmentCount,
            *R28Report);
        OutEnvironment = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ValidateOptionalR28EnvironmentCoherence(
        const ATRIADIstanaExploreV5DR28EnvironmentActor* Environment,
        bool bExpectedProviderReady,
        FString& OutReport) const
{
    if (!Environment)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_ABSENT optional=true count=0.");
        return true;
    }

    FString EnvironmentReport;
    // Its actor-level validator proves static renderers, scene-capture
    // exclusion, and no collision/navigation.  Reassert the public truth
    // tuple here so this optional presentation can never become terrain,
    // collision, sensor, navigation, or RF authority through this policy.
    if (!IsValid(Environment) || Environment->GetWorld() != GetWorld() ||
        !Environment->bConfigured || !Environment->bRenderOnly ||
        Environment->bCollisionNavigationSensorOrRfAuthority ||
        Environment->bMeasuredSurveyAsBuiltOrCurrentCompleteClaimed ||
        Environment->bExistingSimulationOrRfInputsModified ||
        Environment->bRuntimeGeometryGenerated ||
        Environment->bProviderReady != bExpectedProviderReady ||
        !Environment->ValidateR28Environment(EnvironmentReport) ||
        !EnvironmentReport.StartsWith(
            TEXT("ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_VALID")))
    {
        OutReport = FString::Printf(
            TEXT("R28 environment lost provider coherence or its render-only negative-authority contract; expectedProviderReady=%s report={%s}."),
            bExpectedProviderReady ? TEXT("true") : TEXT("false"),
            *EnvironmentReport);
        return false;
    }

    OutReport = EnvironmentReport;
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    RestoreOptionalR28EnvironmentToFallback(FString& OutError)
{
    CachedR28Environment.Reset();
    UWorld* World = GetWorld();
    if (!World)
    {
        OutError = TEXT("V5D optional R28 fallback restore has no world.");
        return false;
    }

    TArray<ATRIADIstanaExploreV5DR28EnvironmentActor*> Environments;
    for (TActorIterator<ATRIADIstanaExploreV5DR28EnvironmentActor> It(World);
         It;
         ++It)
    {
        if (IsValid(*It))
        {
            Environments.Add(*It);
        }
    }

    bool bEveryEnvironmentRestored = true;
    FString RestoreDetails;
    for (ATRIADIstanaExploreV5DR28EnvironmentActor* Environment : Environments)
    {
        FString TransitionError;
        FString ValidationReport;
        const bool bTransitioned =
            Environment->SetProviderReady(false, TransitionError);
        const bool bValidated = bTransitioned &&
            ValidateOptionalR28EnvironmentCoherence(
                Environment,
                false,
                ValidationReport);
        if (!bValidated)
        {
            bEveryEnvironmentRestored = false;
            RestoreDetails += FString::Printf(
                TEXT(" transition={%s} validation={%s}"),
                *TransitionError,
                *ValidationReport);
        }
    }

    if (Environments.Num() == 1 && bEveryEnvironmentRestored)
    {
        CachedR28Environment = Environments[0];
    }
    if (Environments.Num() > 1 || !bEveryEnvironmentRestored)
    {
        OutError = FString::Printf(
            TEXT("V5D optional R28 fallback restore failed closed; zero or one actor is permitted, count=%d.%s"),
            Environments.Num(),
            *RestoreDetails);
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ResolveAuthoredCoreVisualActors(
        TArray<AActor*>& OutActors,
        FString& OutError) const
{
    OutActors.Reset();
    UWorld* World = GetWorld();
    int32 SceneCount = 0;
    int32 V2Count = 0;
    int32 V3Count = 0;
    int32 V4Count = 0;
    int32 V5BCount = 0;
    int32 FountainCount = 0;
    int32 GroundCount = 0;
    int32 TreeCount = 0;
    int32 PublicRealmCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV2LandscapeActor* V2 =
        FindExactlyOne<ATRIADIstanaExploreV2LandscapeActor>(World, V2Count);
    ATRIADIstanaExploreV3SupplementActor* V3 =
        FindExactlyOne<ATRIADIstanaExploreV3SupplementActor>(World, V3Count);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    ATRIADIstanaExploreV5BVisualActor* V5B =
        FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(World, V5BCount);
    ATRIADIstanaExploreV5DFountainRealismActor* Fountain =
        FindExactlyOne<ATRIADIstanaExploreV5DFountainRealismActor>(
            World,
            FountainCount);
    ATRIADIstanaExploreV5DGroundVegetationActor* Ground =
        FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
            World,
            GroundCount);
    ATRIADIstanaExploreV5DTreeRealismActor* Tree =
        FindExactlyOne<ATRIADIstanaExploreV5DTreeRealismActor>(
            World,
            TreeCount);
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
            World,
            PublicRealmCount);
    if (SceneCount != 1 || V2Count != 1 || V3Count != 1 || V4Count != 1 ||
        V5BCount != 1 || FountainCount != 1 || GroundCount != 1 ||
        TreeCount != 1 || PublicRealmCount != 1 || !Scene || !V2 || !V3 ||
        !V4 || !V5B || !Fountain || !Ground || !Tree || !PublicRealm)
    {
        OutError = FString::Printf(
            TEXT("V5D aerial handoff requires one exact visual owner of each class; scene=%d v2=%d v3=%d v4=%d v5b=%d fountain=%d ground=%d tree=%d publicRealm=%d."),
            SceneCount,
            V2Count,
            V3Count,
            V4Count,
            V5BCount,
            FountainCount,
            GroundCount,
            TreeCount,
            PublicRealmCount);
        return false;
    }
    OutActors = {
        Scene,
        V2,
        V3,
        V4,
        V5B,
        Fountain,
        Ground,
        Tree,
        PublicRealm};
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    RestoreGroundLevelPresentation(FString& OutError)
{
    FString R28RestoreError;
    const bool bR28EnvironmentRestored =
        RestoreOptionalR28EnvironmentToFallback(R28RestoreError);

    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        CachedPublicRealm.Get();
    FString PublicRealmError;
    if ((!IsValid(PublicRealm) || PublicRealm->GetWorld() != GetWorld()) &&
        ResolvePublicRealmActor(PublicRealm, PublicRealmError))
    {
        CachedPublicRealm = PublicRealm;
    }
    bool bPublicRealmRestored = false;
    if (IsValid(PublicRealm) && PublicRealm->GetWorld() == GetWorld())
    {
        bPublicRealmRestored =
            PublicRealm->SetProviderReady(false, PublicRealmError);
    }

    ATRIADIstanaPublicViewSceneActor* Scene = CachedScene.Get();
    ACesium3DTileset* Tileset = CachedTileset.Get();
    ACesium3DTileset* R33CwtTileset = CachedR33CwtTileset.Get();
    FString ResolveError;
    const bool bNeedsResolve = !Scene || !Tileset ||
        (bR33DualCesiumContextConfigured && !R33CwtTileset);
    const bool bResolved = !bNeedsResolve ||
        (bR33DualCesiumContextConfigured
             ? ResolveSceneAndR33Tilesets(
                   Scene,
                   Tileset,
                   R33CwtTileset,
                   ResolveError)
             : ResolveSceneAndTileset(Scene, Tileset, ResolveError));
    if (!bResolved)
    {
        SetLocalBuildingFallbackVisible(Scene, true);
        bLocalBuildingFallbackCurrentlyHidden = false;
        bAerialProviderHandoffRequested = false;
        bAerialProviderHandoffCurrentlyActive = false;
        AerialProviderReadySamples = 0;
        ProviderReadyConsecutiveSamples = 0;
        OutError = PublicRealmError + TEXT(" ") + R28RestoreError +
            TEXT(" ") + ResolveError;
        return false;
    }

    UCesiumPolygonRasterOverlay* Overlay = CachedSiteClipOverlay.Get();
    if (!Overlay &&
        !ResolveProviderSiteClipOverlay(Tileset, Overlay, ResolveError))
    {
        SetLocalBuildingFallbackVisible(Scene, true);
        bLocalBuildingFallbackCurrentlyHidden = false;
        bAerialProviderHandoffRequested = false;
        bAerialProviderHandoffCurrentlyActive = false;
        AerialProviderReadySamples = 0;
        ProviderReadyConsecutiveSamples = 0;
        OutError = PublicRealmError + TEXT(" ") + R28RestoreError +
            TEXT(" ") + ResolveError;
        return false;
    }
    CachedScene = Scene;
    CachedTileset = Tileset;
    if (R33CwtTileset)
    {
        CachedR33CwtTileset = R33CwtTileset;
    }
    CachedSiteClipOverlay = Overlay;

    // Ground restore is the fail-safe path: make the local fallback visible
    // even when a malformed optional participant prevents the later coherent
    // three-party transaction from validating.
    SetLocalBuildingFallbackVisible(Scene, true);
    bLocalBuildingFallbackCurrentlyHidden = false;

    FString ProviderPresentationError;
    const bool bProviderPresentationRestored =
        SetProviderReadyPresentation(
            Scene,
            false,
            ProviderPresentationError);
    bAerialProviderHandoffRequested = false;
    bAerialProviderHandoffCurrentlyActive = false;
    bProviderSiteClipCurrentlyActive = true;
    bAuthoredCoreVisualsCurrentlyVisible = true;
    AerialProviderReadySamples = 0;
    ProviderReadyConsecutiveSamples = 0;
    if (!Overlay || !Overlay->IsActive() || !bPublicRealmRestored ||
        !bR28EnvironmentRestored || !bProviderPresentationRestored)
    {
        OutError = PublicRealmError + TEXT(" ") + R28RestoreError +
            TEXT(" ") + ProviderPresentationError +
            TEXT(" Provider-site clipping overlay and every optional render-only fallback must remain safely restored.");
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DContextPolicyActor::
    SetLocalBuildingFallbackVisible(
        ATRIADIstanaPublicViewSceneActor* Scene,
        bool bVisible)
{
    const auto SetRendererVisible = [](
        UStaticMeshComponent* Component,
        bool bShouldBeVisible)
    {
        if (Component)
        {
            Component->SetVisibility(bShouldBeVisible, true);
            Component->SetHiddenInGame(!bShouldBeVisible, true);
        }
    };

    FString MeshError;
    const bool bUseCurrentSurroundings =
        CurrentSurroundingsRenderOnlyComponent &&
        ValidateCurrentSurroundingsMesh(
            CurrentSurroundingsRenderOnlyComponent->GetStaticMesh(),
            MeshError);
    SetRendererVisible(
        CurrentSurroundingsRenderOnlyComponent,
        bVisible && bUseCurrentSurroundings);
    FString OuterGroundError;
    const bool bUseOuterGroundLoadingFallback =
        OuterGroundLoadingFallbackRenderOnlyComponent &&
        ValidateOuterGroundLoadingFallbackMesh(
            OuterGroundLoadingFallbackRenderOnlyComponent->GetStaticMesh(),
            OuterGroundError);
    SetRendererVisible(
        OuterGroundLoadingFallbackRenderOnlyComponent,
        bVisible && bUseOuterGroundLoadingFallback &&
            !(bR33DualCesiumContextConfigured &&
              bR33CwtPresentationActive));
    if (!Scene)
    {
        return;
    }

    // V5D PublicRealm owns the road/sidewalk presentation in both its core
    // and readiness fallback. The inherited V5C planning-ground renderer is
    // a human-reference RoadZone/RoadGraphic sheet, not a building fallback;
    // leaving it visible produces a near-black 300 m ring and planning-grid
    // seams wherever streamed provider geometry does not occlude it. Keep the
    // frozen asset intact but hidden for every V5D presentation state.
    FString PlanningGroundError;
    SuppressInheritedPlanningGroundPresentation(
        Scene,
        PlanningGroundError);

    // The dated V5D building mesh owns local presentation when valid. V5C and
    // legacy ODbL buildings remain mutually exclusive hidden fallbacks and are
    // used only if the exact current asset is absent or invalid.
    const bool bUseV5CSurroundings =
        !bUseCurrentSurroundings &&
        Scene->IsV5CSurroundingsPresentationActive();
    SetRendererVisible(Scene->ContextBuildingsComponent, false);
    SetRendererVisible(
        Scene->OSMContextBuildingsComponent,
        bVisible && !bUseCurrentSurroundings && !bUseV5CSurroundings);
    SetRendererVisible(
        Scene->V5CSurroundingsRenderOnlyComponent,
        bVisible && bUseV5CSurroundings);
}

void ATRIADIstanaExploreV5DContextPolicyActor::
    SetR33LocalFallbackVisibility(
        ATRIADIstanaPublicViewSceneActor* Scene,
        bool bBuildingsVisible,
        bool bOuterGroundVisible)
{
    // Reuse the inherited exact building-fallback selection, then decouple its
    // synthetic outer ground. CWT supplies only terrain, so R33 must retain
    // local buildings without rendering two outer ground surfaces.
    SetLocalBuildingFallbackVisible(Scene, bBuildingsVisible);
    if (OuterGroundLoadingFallbackRenderOnlyComponent)
    {
        FString MeshError;
        const bool bHasValidOuterGround =
            ValidateOuterGroundLoadingFallbackMesh(
                OuterGroundLoadingFallbackRenderOnlyComponent->GetStaticMesh(),
                MeshError);
        const bool bShowOuterGround =
            bOuterGroundVisible && bHasValidOuterGround;
        OuterGroundLoadingFallbackRenderOnlyComponent->SetVisibility(
            bShowOuterGround,
            true);
        OuterGroundLoadingFallbackRenderOnlyComponent->SetHiddenInGame(
            !bShowOuterGround,
            true);
    }
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    RegisterR33CesiumWorldTerrainController(
        ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor* Controller,
        ACesium3DTileset* GoogleTileset,
        ACesium3DTileset* CwtTileset,
        FString& OutError)
{
    UWorld* World = GetWorld();
    if (!World || !IsValid(Controller) || !IsValid(GoogleTileset) ||
        !IsValid(CwtTileset) || Controller->GetWorld() != World ||
        GoogleTileset->GetWorld() != World || CwtTileset->GetWorld() != World ||
        GoogleTileset == CwtTileset ||
        !IsValid(GoogleTileset->GetCesiumIonServer()) ||
        !IsValid(CwtTileset->GetCesiumIonServer()) ||
        GoogleTileset->GetCesiumIonServer() !=
            CwtTileset->GetCesiumIonServer() ||
        !Controller->Tags.Contains(
            ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
                ExpectedControllerTag()))
    {
        OutError = TEXT("V5D R33 registration requires one exact same-world controller and two distinct same-world tilesets.");
        return false;
    }

    int32 ControllerCount = 0;
    ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor*
        ResolvedController = nullptr;
    for (TActorIterator<
             ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor>
             It(World);
         It;
         ++It)
    {
        if (IsValid(*It) &&
            It->Tags.Contains(
                ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
                    ExpectedControllerTag()))
        {
            ResolvedController = *It;
            ++ControllerCount;
        }
    }

    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ACesium3DTileset* ResolvedGoogle = nullptr;
    ACesium3DTileset* ResolvedCwt = nullptr;
    if (ControllerCount != 1 || ResolvedController != Controller ||
        !ResolveSceneAndR33Tilesets(
            Scene,
            ResolvedGoogle,
            ResolvedCwt,
            OutError) ||
        ResolvedGoogle != GoogleTileset || ResolvedCwt != CwtTileset ||
        (CachedR33Controller.IsValid() &&
         CachedR33Controller.Get() != Controller))
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("V5D R33 registration requires one controller and the exact dual-role roster; controllers=%d."),
                ControllerCount);
        }
        return false;
    }

    CachedScene = Scene;
    CachedTileset = GoogleTileset;
    CachedR33CwtTileset = CwtTileset;
    CachedR33Controller = Controller;
    bR33DualCesiumContextConfigured = true;
    bR33CwtPresentationActive = false;
    bR33SafeLocalPresentationActive = false;
    if (World->IsGameWorld() &&
        !ApplyTransientCurrentViewProviderWorkloadPolicy(
            GoogleTileset,
            OutError))
    {
        CachedR33Controller.Reset();
        CachedR33CwtTileset.Reset();
        bR33DualCesiumContextConfigured = false;
        return false;
    }
    AddTickPrerequisiteActor(Controller);
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DContextPolicyActor::
    UnregisterR33CesiumWorldTerrainController(
        ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor* Controller)
{
    if (!Controller || CachedR33Controller.Get() != Controller)
    {
        return;
    }

    ATRIADIstanaPublicViewSceneActor* Scene = CachedScene.Get();
    bR33CwtPresentationActive = false;
    bR33SafeLocalPresentationActive = true;
    FString Ignored;
    if (!SetProviderReadyPresentation(Scene, false, Ignored))
    {
        SetR33LocalFallbackVisibility(Scene, true, true);
        bLocalBuildingFallbackCurrentlyHidden = false;
    }
    RemoveTickPrerequisiteActor(Controller);
    CachedR33Controller.Reset();
    bR33DualCesiumContextConfigured = false;
    bR33CwtPresentationActive = false;
    bR33SafeLocalPresentationActive = false;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ApplyR33CesiumPresentationMode(
        const ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor*
            Controller,
        bool bCwtPresented,
        bool bSafeLocal,
        FString& OutError)
{
    if (!bR33DualCesiumContextConfigured || !Controller ||
        CachedR33Controller.Get() != Controller ||
        (bCwtPresented && bSafeLocal))
    {
        OutError = TEXT("V5D R33 presentation rejected an unregistered controller or mutually active CWT/SafeLocal modes.");
        return false;
    }

    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ACesium3DTileset* GoogleTileset = nullptr;
    ACesium3DTileset* CwtTileset = nullptr;
    if (!ResolveSceneAndR33Tilesets(
            Scene,
            GoogleTileset,
            CwtTileset,
            OutError) ||
        GoogleTileset != CachedTileset.Get() ||
        CwtTileset != CachedR33CwtTileset.Get() ||
        !IsValid(GoogleTileset->GetCesiumIonServer()) ||
        !IsValid(CwtTileset->GetCesiumIonServer()) ||
        GoogleTileset->GetCesiumIonServer() !=
            CwtTileset->GetCesiumIonServer())
    {
        bR33CwtPresentationActive = false;
        bR33SafeLocalPresentationActive = true;
        SetR33LocalFallbackVisibility(
            Scene ? Scene : CachedScene.Get(),
            true,
            true);
        bLocalBuildingFallbackCurrentlyHidden = false;
        ProviderReadyConsecutiveSamples = 0;
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D R33 presentation binding drifted from its registered dual-role roster.");
        }
        return false;
    }

    bR33CwtPresentationActive = bCwtPresented;
    bR33SafeLocalPresentationActive = bSafeLocal;

    // Every stream-to-stream transition first exposes the exact local visual
    // fallback. The normal Google readiness hysteresis may hide it again; CWT
    // keeps buildings/public realm but never the synthetic outer ground.
    FString TransitionError;
    if (!SetProviderReadyPresentation(Scene, false, TransitionError))
    {
        bR33CwtPresentationActive = false;
        bR33SafeLocalPresentationActive = true;
        SetR33LocalFallbackVisibility(Scene, true, true);
        bLocalBuildingFallbackCurrentlyHidden = false;
        ProviderReadyConsecutiveSamples = 0;
        OutError = TEXT("V5D R33 local fallback transition failed closed to SafeLocal: ") +
            TransitionError;
        return false;
    }

    SetR33LocalFallbackVisibility(
        Scene,
        true,
        !bCwtPresented);
    bLocalBuildingFallbackCurrentlyHidden = false;
    FString PresentationError;
    if (!ValidateCurrentSurroundingsPresentation(
            Scene,
            PresentationError))
    {
        bR33CwtPresentationActive = false;
        bR33SafeLocalPresentationActive = true;
        SetR33LocalFallbackVisibility(Scene, true, true);
        bLocalBuildingFallbackCurrentlyHidden = false;
        ProviderReadyConsecutiveSamples = 0;
        OutError = TEXT("V5D R33 local building/terrain readback failed closed to SafeLocal: ") +
            PresentationError;
        return false;
    }

    ProviderReadyConsecutiveSamples = 0;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ValidateR33CesiumPresentationBinding(
        const ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor*
            Controller,
        FString& OutReport) const
{
    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ACesium3DTileset* GoogleTileset = nullptr;
    ACesium3DTileset* CwtTileset = nullptr;
    FString Error;
    if (!bR33DualCesiumContextConfigured || !Controller ||
        CachedR33Controller.Get() != Controller ||
        !ResolveSceneAndR33Tilesets(
            Scene,
            GoogleTileset,
            CwtTileset,
            Error) ||
        GoogleTileset != CachedTileset.Get() ||
        CwtTileset != CachedR33CwtTileset.Get() ||
        !IsValid(GoogleTileset->GetCesiumIonServer()) ||
        !IsValid(CwtTileset->GetCesiumIonServer()) ||
        GoogleTileset->GetCesiumIonServer() !=
            CwtTileset->GetCesiumIonServer())
    {
        OutReport = TEXT("V5D R33 context/controller binding is not the exact registered dual-role roster. ") +
            Error;
        return false;
    }

    bool bExpectedCwtPresentationActive = false;
    bool bExpectedSafeLocalPresentationActive = false;
    bool bExpectedGoogleVisible = false;
    bool bExpectedGoogleSuspended = true;
    bool bExpectedCwtVisible = false;
    bool bExpectedCwtSuspended = true;
    switch (Controller->GetPresentationState())
    {
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary:
        bExpectedGoogleVisible = true;
        bExpectedGoogleSuspended = false;
        break;
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming:
        bExpectedGoogleVisible = true;
        bExpectedGoogleSuspended = false;
        bExpectedCwtSuspended = false;
        break;
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented:
        bExpectedCwtPresentationActive = true;
        bExpectedCwtVisible = true;
        bExpectedCwtSuspended = false;
        break;
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal:
        bExpectedSafeLocalPresentationActive = true;
        break;
    default:
        OutReport = TEXT("V5D R33 context/controller binding encountered an unknown state.");
        return false;
    }

    const bool bModeRequiresVisibleLocalFallback =
        bExpectedCwtPresentationActive ||
        bExpectedSafeLocalPresentationActive;
    if (bR33CwtPresentationActive !=
            bExpectedCwtPresentationActive ||
        bR33SafeLocalPresentationActive !=
            bExpectedSafeLocalPresentationActive ||
        (bR33CwtPresentationActive &&
         bR33SafeLocalPresentationActive) ||
        (!GoogleTileset->IsHidden()) != bExpectedGoogleVisible ||
        (!CwtTileset->IsHidden()) != bExpectedCwtVisible ||
        GoogleTileset->SuspendUpdate != bExpectedGoogleSuspended ||
        CwtTileset->SuspendUpdate != bExpectedCwtSuspended ||
        (bModeRequiresVisibleLocalFallback &&
         bLocalBuildingFallbackCurrentlyHidden) ||
        !ValidateCurrentSurroundingsPresentation(Scene, Error) ||
        ShouldHideSourceTerrainRendererForVisualContext() !=
            (bExpectedCwtPresentationActive ||
             bExpectedSafeLocalPresentationActive ||
             bLocalBuildingFallbackCurrentlyHidden) ||
        ShouldPresentR29CopernicusFallback() !=
            (bExpectedSafeLocalPresentationActive ||
             (!bExpectedCwtPresentationActive &&
              !bLocalBuildingFallbackCurrentlyHidden)))
    {
        OutReport = TEXT("V5D R33 context/controller mode, local buildings, outer ground, or terrain renderer policy is incoherent. ") +
            Error;
        return false;
    }

    OutReport = TEXT("ISTANA_EXPLORE_V5D_R33_CONTEXT_BINDING_VALID dualTilesets=true localBuildingsIndependentFromOuterTerrain=true sourceCollisionAuthorityUnchanged=true");
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ShouldHideSourceTerrainRendererForVisualContext() const
{
    if (!bR33DualCesiumContextConfigured)
    {
        return bLocalBuildingFallbackCurrentlyHidden;
    }
    return bR33CwtPresentationActive ||
        bR33SafeLocalPresentationActive ||
        bLocalBuildingFallbackCurrentlyHidden;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ShouldPresentR29CopernicusFallback() const
{
    if (!bR33DualCesiumContextConfigured)
    {
        return !bLocalBuildingFallbackCurrentlyHidden;
    }
    if (bR33CwtPresentationActive)
    {
        return false;
    }
    return bR33SafeLocalPresentationActive ||
        !bLocalBuildingFallbackCurrentlyHidden;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    SetProviderReadyPresentation(
        ATRIADIstanaPublicViewSceneActor* Scene,
        bool bProviderReady,
        FString& OutError)
{
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        CachedPublicRealm.Get();
    if (!Scene || !IsValid(PublicRealm) ||
        PublicRealm->GetWorld() != GetWorld())
    {
        if (!Scene || !ResolvePublicRealmActor(PublicRealm, OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("V5D provider readiness requires the exact scene and public-realm actor.");
            }
            return false;
        }
    }

    ATRIADIstanaExploreV5DR28EnvironmentActor* R28Environment = nullptr;
    FString R28ResolveError;
    if (!ResolveOptionalR28EnvironmentActor(
            R28Environment,
            R28ResolveError))
    {
        CachedR28Environment.Reset();
        OutError =
            TEXT("V5D provider readiness rejected the optional R28 environment: ") +
            R28ResolveError;
        return false;
    }
    if (R28Environment)
    {
        CachedR28Environment = R28Environment;
    }
    else
    {
        CachedR28Environment.Reset();
    }

    const bool bPreviousProviderReady = PublicRealm->bProviderReady;
    const bool bPreviousLocalFallbackHidden =
        bLocalBuildingFallbackCurrentlyHidden;
    const bool bPreviousR28ProviderReady =
        R28Environment ? R28Environment->bProviderReady : false;

    FString PreviousPublicRealmReport;
    FString PreviousR28Report;
    if (!PublicRealm->ValidatePublicRealm(PreviousPublicRealmReport) ||
        !ValidateOptionalR28EnvironmentCoherence(
            R28Environment,
            bPreviousR28ProviderReady,
            PreviousR28Report))
    {
        OutError =
            TEXT("V5D provider readiness refused to mutate an invalid participant: ") +
            PreviousPublicRealmReport + TEXT(" ") + PreviousR28Report;
        return false;
    }

    FString PublicRealmError;
    if (!PublicRealm->SetProviderReady(bProviderReady, PublicRealmError))
    {
        FString PublicRollbackError;
        FString R28RollbackError;
        const bool bPublicRollbackSucceeded =
            PublicRealm->SetProviderReady(
                bPreviousProviderReady,
                PublicRollbackError);
        bool bR28RollbackSucceeded = true;
        if (R28Environment)
        {
            bR28RollbackSucceeded = R28Environment->SetProviderReady(
                bPreviousR28ProviderReady,
                R28RollbackError);
        }
        SetLocalBuildingFallbackVisible(
            Scene,
            !bPreviousLocalFallbackHidden);
        bLocalBuildingFallbackCurrentlyHidden =
            bPreviousLocalFallbackHidden;
        OutError =
            TEXT("V5D provider readiness could not update the public-realm fallback; every participant was rolled back: ") +
            PublicRealmError + TEXT(" publicRollback=") +
            (bPublicRollbackSucceeded ? TEXT("true") : TEXT("false")) +
            TEXT(" r28Rollback=") +
            (bR28RollbackSucceeded ? TEXT("true") : TEXT("false")) +
            TEXT(" ") + PublicRollbackError + TEXT(" ") + R28RollbackError;
        return false;
    }

    FString R28TransitionError;
    if (R28Environment &&
        !R28Environment->SetProviderReady(
            bProviderReady,
            R28TransitionError))
    {
        FString PublicRollbackError;
        FString R28RollbackError;
        const bool bPublicRollbackSucceeded =
            PublicRealm->SetProviderReady(
                bPreviousProviderReady,
                PublicRollbackError);
        const bool bR28RollbackSucceeded =
            R28Environment->SetProviderReady(
                bPreviousR28ProviderReady,
                R28RollbackError);
        SetLocalBuildingFallbackVisible(
            Scene,
            !bPreviousLocalFallbackHidden);
        bLocalBuildingFallbackCurrentlyHidden =
            bPreviousLocalFallbackHidden;
        OutError =
            TEXT("V5D provider readiness could not update the optional R28 render-only fallback; every participant was rolled back: ") +
            R28TransitionError + TEXT(" publicRollback=") +
            (bPublicRollbackSucceeded ? TEXT("true") : TEXT("false")) +
            TEXT(" r28Rollback=") +
            (bR28RollbackSucceeded ? TEXT("true") : TEXT("false")) +
            TEXT(" ") + PublicRollbackError + TEXT(" ") + R28RollbackError;
        return false;
    }

    SetLocalBuildingFallbackVisible(Scene, !bProviderReady);
    bLocalBuildingFallbackCurrentlyHidden = bProviderReady;
    FString CurrentSurroundingsError;
    FString PublicRealmReport;
    FString R28Report;
    if (!ValidateCurrentSurroundingsPresentation(
            Scene,
            CurrentSurroundingsError) ||
        !PublicRealm->ValidatePublicRealm(PublicRealmReport) ||
        PublicRealm->bProviderReady !=
            bLocalBuildingFallbackCurrentlyHidden ||
        !ValidateOptionalR28EnvironmentCoherence(
            R28Environment,
            bLocalBuildingFallbackCurrentlyHidden,
            R28Report))
    {
        FString RollbackError;
        FString R28RollbackError;
        const bool bPublicRollbackSucceeded =
            PublicRealm->SetProviderReady(
            bPreviousProviderReady,
            RollbackError);
        bool bR28RollbackSucceeded = true;
        if (R28Environment)
        {
            bR28RollbackSucceeded = R28Environment->SetProviderReady(
                bPreviousR28ProviderReady,
                R28RollbackError);
        }
        SetLocalBuildingFallbackVisible(
            Scene,
            !bPreviousLocalFallbackHidden);
        bLocalBuildingFallbackCurrentlyHidden =
            bPreviousLocalFallbackHidden;

        FString RollbackCurrentSurroundingsReport;
        FString RollbackPublicRealmReport;
        FString RollbackR28Report;
        const bool bRollbackValidated =
            bPublicRollbackSucceeded && bR28RollbackSucceeded &&
            ValidateCurrentSurroundingsPresentation(
                Scene,
                RollbackCurrentSurroundingsReport) &&
            PublicRealm->ValidatePublicRealm(RollbackPublicRealmReport) &&
            PublicRealm->bProviderReady == bPreviousProviderReady &&
            bLocalBuildingFallbackCurrentlyHidden ==
                bPreviousLocalFallbackHidden &&
            ValidateOptionalR28EnvironmentCoherence(
                R28Environment,
                bPreviousR28ProviderReady,
                RollbackR28Report);
        OutError =
            TEXT("V5D provider readiness lost atomic current-context/public-realm coherence and was restored: ") +
            CurrentSurroundingsError + TEXT(" ") + PublicRealmReport +
            TEXT(" ") + R28Report + TEXT(" publicRollback=") +
            (bPublicRollbackSucceeded ? TEXT("true") : TEXT("false")) +
            TEXT(" r28Rollback=") +
            (bR28RollbackSucceeded ? TEXT("true") : TEXT("false")) +
            TEXT(" rollbackValidated=") +
            (bRollbackValidated ? TEXT("true") : TEXT("false")) +
            TEXT(" ") + RollbackError + TEXT(" ") + R28RollbackError +
            TEXT(" ") + RollbackCurrentSurroundingsReport + TEXT(" ") +
            RollbackPublicRealmReport + TEXT(" ") + RollbackR28Report;
        return false;
    }

    CachedPublicRealm = PublicRealm;
    if (R28Environment)
    {
        CachedR28Environment = R28Environment;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ValidateCurrentSurroundingsPresentation(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutError) const
{
    if (!Scene || !CurrentSurroundingsRenderOnlyComponent ||
        !OuterGroundLoadingFallbackRenderOnlyComponent ||
        !ValidateCurrentSurroundingsMesh(
            CurrentSurroundingsRenderOnlyComponent->GetStaticMesh(),
            OutError) ||
        !ValidateOuterGroundLoadingFallbackMesh(
            OuterGroundLoadingFallbackRenderOnlyComponent->GetStaticMesh(),
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D fallback presentation is missing its exact scene, native components, or validated meshes.");
        }
        return false;
    }
    const UStaticMeshComponent* Component =
        CurrentSurroundingsRenderOnlyComponent;
    const UStaticMeshComponent* OuterComponent =
        OuterGroundLoadingFallbackRenderOnlyComponent;
    const bool bExpectedVisible = !bLocalBuildingFallbackCurrentlyHidden;
    const bool bExpectedOuterVisible =
        bR33DualCesiumContextConfigured && bR33CwtPresentationActive
        ? false
        : bExpectedVisible;
    FString CurrentShellMaterialReport;
    const EAdmittedCurrentShellMaterialState CurrentShellMaterialState =
        ValidateAdmittedCurrentShellMaterialState(
            Component, CurrentShellMaterialReport);
    const bool bVersionedOverridesUseExactV2Mesh =
        (CurrentShellMaterialState !=
             EAdmittedCurrentShellMaterialState::ContextFacadeR25 &&
         CurrentShellMaterialState !=
             EAdmittedCurrentShellMaterialState::BroadShellR31) ||
        (Component->GetStaticMesh() &&
         Component->GetStaticMesh()->GetPathName() ==
             CurrentSurroundingsV2MeshObjectPath);
#if WITH_EDITOR
    const bool bActorHiddenInEditor = IsHiddenEd();
#else
    const bool bActorHiddenInEditor = false;
#endif
    const auto IsComponentHidden = [](const UStaticMeshComponent* Candidate)
    {
        return Candidate &&
            !Candidate->IsVisible() &&
            Candidate->bHiddenInGame;
    };
    if (!bCurrentSurroundingsRenderOnly ||
        bCurrentSurroundingsCollisionNavigationSensorOrRfAuthority ||
        bCurrentSurroundingsMeasuredSurveyAsBuiltOrHyperreal ||
        !bOuterGroundLoadingFallbackRenderOnly ||
        bOuterGroundLoadingFallbackCollisionNavigationSensorRfTerrainAuthority ||
        bOuterGroundLoadingFallbackSurveyAsBuilt ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        IsHidden() || bActorHiddenInEditor ||
        !RootComponent || RootComponent->Mobility != EComponentMobility::Static ||
        Component->GetAttachParent() != RootComponent ||
        !Component->GetRelativeTransform().Equals(FTransform::Identity, 0.001) ||
        !Component->GetComponentTransform().Equals(FTransform::Identity, 0.001) ||
        Component->Mobility != EComponentMobility::Static ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetCollisionResponseToChannels() !=
            FCollisionResponseContainer(ECR_Ignore) ||
        Component->GetGenerateOverlapEvents() ||
        Component->CanEverAffectNavigation() ||
        !Component->CastShadow ||
        !Component->bRenderInMainPass ||
        CurrentShellMaterialState ==
            EAdmittedCurrentShellMaterialState::Invalid ||
        !bVersionedOverridesUseExactV2Mesh ||
        Component->GetOverlayMaterial() != nullptr ||
        !Component->ComponentTags.Contains(
            TEXT("TRIADV5DCurrentPublicContextRenderOnly")) ||
        Component->ComponentTags.Contains(TEXT("TRIADHumanOnlyOverlay")) ||
        Component->IsVisible() != bExpectedVisible ||
        Component->bHiddenInGame == bExpectedVisible ||
        OuterComponent->GetAttachParent() != RootComponent ||
        !OuterComponent->GetRelativeTransform().Equals(
            FTransform::Identity, 0.001) ||
        !OuterComponent->GetComponentTransform().Equals(
            FTransform::Identity, 0.001) ||
        OuterComponent->Mobility != EComponentMobility::Static ||
        OuterComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        OuterComponent->GetCollisionResponseToChannels() !=
            FCollisionResponseContainer(ECR_Ignore) ||
        OuterComponent->GetGenerateOverlapEvents() ||
        OuterComponent->CanEverAffectNavigation() ||
        OuterComponent->CastShadow || OuterComponent->bCastContactShadow ||
        OuterComponent->bAffectDistanceFieldLighting ||
        OuterComponent->bAffectDynamicIndirectLighting ||
        !OuterComponent->bRenderInMainPass ||
        OuterComponent->bRenderCustomDepth ||
        OuterComponent->GetNumOverrideMaterials() != 0 ||
        OuterComponent->GetOverlayMaterial() != nullptr ||
        !OuterComponent->ComponentTags.Contains(
            TEXT("TRIADV5DOuterGroundLoadingFallbackRenderOnly")) ||
        OuterComponent->ComponentTags.Contains(TEXT("TRIADHumanOnlyOverlay")) ||
        (!bR33DualCesiumContextConfigured &&
         OuterComponent->IsVisible() != bExpectedVisible) ||
        (bR33DualCesiumContextConfigured &&
         OuterComponent->IsVisible() != bExpectedOuterVisible) ||
        OuterComponent->bHiddenInGame == bExpectedOuterVisible ||
        !IsComponentHidden(Scene->ContextBuildingsComponent) ||
        !IsComponentHidden(Scene->OSMContextBuildingsComponent) ||
        !IsComponentHidden(Scene->V5CSurroundingsRenderOnlyComponent) ||
        !IsComponentHidden(Scene->V5CGroundContextRenderOnlyComponent))
    {
        OutError = TEXT("V5D successor/outer-ground presentation lost its exact identity, admitted legacy/R25/R31 material state (R31 requires exact V2), provider-coupled visibility, render-only/no-authority state, or hidden atomic V5C/legacy fallbacks. ") +
            CurrentShellMaterialReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ValidateCurrentSurroundingsSuccessorForInheritedScene(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutReport) const
{
    FString Error;
    if (!Scene || !GetWorld() || Scene->GetWorld() != GetWorld() ||
        !Tags.Contains(HybridContextPolicyTag) ||
        !CurrentSurroundingsRenderOnlyComponent ||
        !CurrentSurroundingsRenderOnlyComponent->GetStaticMesh() ||
        CurrentSurroundingsRenderOnlyComponent->GetStaticMesh()->GetPathName() !=
            CurrentSurroundingsMeshObjectPath ||
        !ValidateCurrentSurroundingsPresentation(Scene, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_CURRENT_SURROUNDINGS_SUCCESSOR_INVALID: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_CURRENT_SURROUNDINGS_SUCCESSOR_VALID suppressionContract=local_fallback_suppression_v1 triangles=43492 visibleFeatures=1387 visiblePolygonParts=1389 suppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490 legacyPredecessorAcceptedAtRuntime=false providerOverlapResolved=false outerGroundLoadingFallbackTriangles=1280 outerGroundProviderCoupled=true identity=true renderOnly=true legacyAndV5CFallbacksHidden=true collisionNavigationSensorRfTerrainAuthority=false surveyAsBuiltHyperreal=false.");
    return true;
}


bool ATRIADIstanaExploreV5DContextPolicyActor::
    ValidateCurrentSurroundingsV2SuccessorForInheritedScene(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutReport) const
{
    FString Error;
    if (!Scene || !GetWorld() || Scene->GetWorld() != GetWorld() ||
        !Tags.Contains(HybridContextPolicyTag) ||
        !CurrentSurroundingsRenderOnlyComponent ||
        !CurrentSurroundingsRenderOnlyComponent->GetStaticMesh() ||
        CurrentSurroundingsRenderOnlyComponent->GetStaticMesh()->GetPathName() !=
            CurrentSurroundingsV2MeshObjectPath ||
        !ValidateCurrentSurroundingsPresentation(Scene, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_CURRENT_SURROUNDINGS_V2_SUCCESSOR_INVALID: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_CURRENT_SURROUNDINGS_V2_SUCCESSOR_VALID suppressionContract=local_fallback_suppression_v2 triangles=43448 visibleFeatures=1386 visiblePolygonParts=1388 suppressedTriangles=96 suppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490+OSM:way:429681826 legacyPredecessorAcceptedAtRuntime=false providerOverlapResolved=false outerGroundLoadingFallbackTriangles=1280 outerGroundProviderCoupled=true identity=true renderOnly=true legacyAndV5CFallbacksHidden=true collisionNavigationSensorRfTerrainAuthority=false surveyAsBuiltHyperreal=false.");
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutReport) const
{
    FString Error;
    if (!Scene || !GetWorld() || Scene->GetWorld() != GetWorld() ||
        !Tags.Contains(HybridContextPolicyTag) ||
        !CurrentSurroundingsRenderOnlyComponent ||
        !CurrentSurroundingsRenderOnlyComponent->GetStaticMesh() ||
        CurrentSurroundingsRenderOnlyComponent->GetStaticMesh()->GetPathName() !=
            CurrentSurroundingsV2MeshObjectPath ||
        !ValidateCurrentSurroundingsPresentation(Scene, Error) ||
        !ValidateContextFacadeR25Overrides(
            CurrentSurroundingsRenderOnlyComponent, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_INVALID: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_VALID mesh=local_fallback_suppression_v2 meshTriangles=43448 meshGeometryUnchanged=true materialOverrides=17 officialWall=2 officialRoof=2 fallbackWall=6 fallbackRoof=7 textureFree=true distanceReadableGlazing=true recessedGlazingCue=true mullionTransomCue=true plinthContactCue=true providerVisibilityPolicyUnchanged=true collisionNavigationSensorRfAuthority=false surveyAsBuiltHyperreal=false sharedV5CAssetsMutated=false.");
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::
    ValidateCurrentSurroundingsBroadShellR31ForInheritedScene(
        const ATRIADIstanaPublicViewSceneActor* Scene,
        FString& OutReport) const
{
    FString Error;
    if (!Scene || !GetWorld() || Scene->GetWorld() != GetWorld() ||
        !Tags.Contains(HybridContextPolicyTag) ||
        !CurrentSurroundingsRenderOnlyComponent ||
        !CurrentSurroundingsRenderOnlyComponent->GetStaticMesh() ||
        CurrentSurroundingsRenderOnlyComponent->GetStaticMesh()->GetPathName() !=
            CurrentSurroundingsV2MeshObjectPath ||
        !ValidateCurrentSurroundingsPresentation(Scene, Error) ||
        !ValidateBroadShellR31Overrides(
            CurrentSurroundingsRenderOnlyComponent, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_INVALID: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_VALID mesh=local_fallback_suppression_v2 meshTriangles=43448 meshGeometryUnchanged=true materialOverrides=17 officialWall=2 officialRoof=2 fallbackWall=6 fallbackRoof=7 textureBacked=true textureSets=Plaster+Stone+Slate sourceMetreUv0=true wallUPerSegment=true wallVAbsoluteSourceZ=true roofBottomUvHeroLocalXy=true sourceObjVRecovered=true buildingLocalAboveGradeCoordinate=false wallDepthCuesRoleGated=true deterministicWeathering=true r25FrameGlassMullionTransomRevealCellToneFresnelAtmosphereRetained=true r25PlinthContactRetained=false providerVisibilityPolicyUnchanged=true identityTransform=true collisionNavigationSensorRfAuthority=false surveyAsBuiltCurrentCompletePhysicalMaterialHyperreal=false.");
    return true;
}

bool ATRIADIstanaExploreV5DContextPolicyActor::ValidateHybridContext(
    FString& OutReport) const
{
    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    ACesium3DTileset* R33CwtTileset = nullptr;
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm = nullptr;
    ATRIADIstanaExploreV5DR28EnvironmentActor* R28Environment = nullptr;
    FString Error;
    FString PublicRealmReport;
    FString R28Report;
    const bool bResolvedSceneAndTileset = bR33DualCesiumContextConfigured
        ? ResolveSceneAndR33Tilesets(
              Scene,
              Tileset,
              R33CwtTileset,
              Error)
        : ResolveSceneAndTileset(Scene, Tileset, Error);
    if (!Tags.Contains(HybridContextPolicyTag) ||
        !bResolvedSceneAndTileset ||
        !ValidateCurrentSurroundingsPresentation(Scene, Error) ||
        !ResolvePublicRealmActor(PublicRealm, Error) ||
        !PublicRealm->ValidatePublicRealm(PublicRealmReport) ||
        !ResolveOptionalR28EnvironmentActor(R28Environment, Error) ||
        !ValidateOptionalR28EnvironmentCoherence(
            R28Environment,
            bLocalBuildingFallbackCurrentlyHidden,
            R28Report))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_INVALID: ") + Error;
        return false;
    }

    if (!ValidateExactIstanaGeospatialAnchor(GetWorld(), Tileset, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_INVALID: ") + Error;
        return false;
    }

    FString R33BindingReport;
    if (bR33DualCesiumContextConfigured &&
        (!R33CwtTileset || !CachedR33Controller.IsValid() ||
         CachedTileset.Get() != Tileset ||
         CachedR33CwtTileset.Get() != R33CwtTileset ||
         !ValidateExactIstanaGeospatialAnchor(
             GetWorld(),
             R33CwtTileset,
             Error) ||
         !ValidateProviderSiteClip(GetWorld(), R33CwtTileset, Error) ||
         R33CwtTileset->GetTilesetSource() !=
             ETilesetSource::FromCesiumIon ||
         R33CwtTileset->GetIonAssetID() != 1 ||
         !FMath::IsNearlyEqual(
             R33CwtTileset->GetMaximumScreenSpaceError(),
             8.0,
             0.000001) ||
         R33CwtTileset->ApplyDpiScaling != EApplyDpiScaling::No ||
         R33CwtTileset->GetCreatePhysicsMeshes() ||
         R33CwtTileset->GetCreateNavCollision() ||
         R33CwtTileset->GetActorEnableCollision() ||
         !R33CwtTileset->ShowCreditsOnScreen ||
         R33CwtTileset->GetGeoreference() != Tileset->GetGeoreference() ||
         R33CwtTileset->ResolveGeoreference() !=
             Tileset->ResolveGeoreference() ||
         !IsValid(Tileset->GetCesiumIonServer()) ||
         !IsValid(R33CwtTileset->GetCesiumIonServer()) ||
         R33CwtTileset->GetCesiumIonServer() !=
             Tileset->GetCesiumIonServer() ||
         !ValidateR33CesiumPresentationBinding(
             CachedR33Controller.Get(),
             R33BindingReport)))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_INVALID: R33 dual-terrain binding or visual-only CWT tuple changed. ") +
            Error + TEXT(" ") + R33BindingReport;
        return false;
    }

    const bool bIsGameWorld = GetWorld()->IsGameWorld();
    const bool bExpectedRuntimeFogCulling = bIsGameWorld;
    const bool bHasExpectedCurrentViewProviderWorkloadState =
        HasExpectedCurrentViewProviderWorkloadState(
            bIsGameWorld,
            bTransientCurrentViewProviderWorkloadPolicyApplied,
            bProviderFogCullingSnapshotValid,
            bProviderFogCullingBeforeTransientPolicy,
            Tileset->EnableFogCulling);
    if (ExpectedIonAssetId != 2275207 ||
        !FMath::IsNearlyEqual(RequiredMaximumScreenSpaceError, 1.0, 0.000001) ||
        !FMath::IsNearlyEqual(HideLocalBuildingFallbackAtLoadProgress, 98.0f) ||
        !FMath::IsNearlyEqual(RestoreLocalBuildingFallbackBelowLoadProgress, 90.0f) ||
        RequiredProviderReadyConsecutiveSamples != RequiredProviderReadySamples ||
        !FMath::IsNearlyEqual(
            PrimaryActorTick.TickInterval,
            RequiredProviderPolicyTickIntervalSeconds) ||
        !bCesiumLayerIsVisualOnly ||
        bCesiumCollisionNavigationSensorOrRfAuthority ||
        !bCesiumStandardPersistentHttpRequestCacheAcknowledged ||
        bTriadReadSerializedOrLoggedProviderToken ||
        bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent ||
        !bProviderContentClippedFromAuthoredCore ||
        !ProviderSiteClipCenterMeters.Equals(
            ExpectedProviderSiteClipCenterCentimeters() /
                CentimetersPerMeter,
            0.000001) ||
        !ProviderSiteClipSemiAxesMeters.Equals(
            ExpectedProviderSiteClipSemiAxesCentimeters() /
                CentimetersPerMeter,
            0.000001) ||
        ProviderSiteClipSplinePoints != RequiredSiteClipSplinePoints ||
        Tileset->GetTilesetSource() != ETilesetSource::FromCesiumIon ||
        Tileset->GetIonAssetID() != ExpectedIonAssetId ||
        !FMath::IsNearlyEqual(
            Tileset->GetMaximumScreenSpaceError(),
            RequiredMaximumScreenSpaceError,
            0.000001) ||
        Tileset->ApplyDpiScaling != EApplyDpiScaling::No ||
        Tileset->GetCreatePhysicsMeshes() ||
        Tileset->GetCreateNavCollision() ||
        Tileset->MaximumCachedBytes != RequiredProviderCacheBytes ||
        Tileset->MaximumSimultaneousTileLoads !=
            RequiredProviderSimultaneousLoads ||
        !Tileset->ForbidHoles ||
        Tileset->LoadingDescendantLimit !=
            RequiredProviderLoadingDescendantLimit ||
        !Tileset->ShowCreditsOnScreen ||
        !Tileset->PreloadAncestors || Tileset->PreloadSiblings ||
        !Tileset->EnableFrustumCulling ||
        Tileset->EnableFogCulling != bExpectedRuntimeFogCulling ||
        !bHasExpectedCurrentViewProviderWorkloadState ||
        !Tileset->EnforceCulledScreenSpaceError ||
        !FMath::IsNearlyEqual(
            Tileset->CulledScreenSpaceError,
            RequiredProviderCulledScreenSpaceError,
            0.000001) ||
        Tileset->GetUseLodTransitions() ||
        Tileset->GetIgnoreKhrMaterialsUnlit() ||
        Tileset->GetGenerateSmoothNormals())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_INVALID: visual-only Cesium asset/settings/readiness truth tuple changed.");
        return false;
    }

    if (!ValidateProviderSiteClip(GetWorld(), Tileset, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_INVALID: ") + Error;
        return false;
    }

    UCesiumPolygonRasterOverlay* Overlay = nullptr;
    TArray<AActor*> VisualActors;
    if (!ResolveProviderSiteClipOverlay(Tileset, Overlay, Error) ||
        !ResolveAuthoredCoreVisualActors(VisualActors, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_INVALID: ") + Error;
        return false;
    }
    bool bActualAuthoredCoreVisualsVisible = true;
    for (const AActor* Actor : VisualActors)
    {
        bActualAuthoredCoreVisualsVisible =
            bActualAuthoredCoreVisualsVisible && Actor && !Actor->IsHidden();
    }
    const bool bStableVisualPolicyTuple =
        !bAerialProviderHandoffRequested &&
        !bAerialProviderHandoffCurrentlyActive &&
        bProviderSiteClipCurrentlyActive &&
        bAuthoredCoreVisualsCurrentlyVisible &&
        AerialProviderReadySamples == 0 &&
        ProviderReadyConsecutiveSamples >= 0 &&
        ProviderReadyConsecutiveSamples <
            RequiredProviderReadyConsecutiveSamples;
    if (!Overlay || !Overlay->IsActive() ||
        PublicRealm->bProviderReady !=
            bLocalBuildingFallbackCurrentlyHidden ||
        !bStableVisualPolicyTuple || !bActualAuthoredCoreVisualsVisible)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_INVALID: stable provider clipping or authored-core visibility tuple drifted.");
        return false;
    }

    const bool bUsingSuppressionV2 =
        CurrentSurroundingsRenderOnlyComponent &&
        CurrentSurroundingsRenderOnlyComponent->GetStaticMesh() &&
        CurrentSurroundingsRenderOnlyComponent->GetStaticMesh()->GetPathName() ==
            CurrentSurroundingsV2MeshObjectPath;
    FString ContextFacadeR25Report;
    const bool bUsingContextFacadeR25 = ValidateContextFacadeR25Overrides(
        CurrentSurroundingsRenderOnlyComponent, ContextFacadeR25Report);
    FString BroadShellR31Report;
    const bool bUsingBroadShellR31 = ValidateBroadShellR31Overrides(
        CurrentSurroundingsRenderOnlyComponent, BroadShellR31Report);
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_HYBRID_VALID assetId=%lld cesiumGeoreference=(%.8f,%.8f,%.3f) geospatialAnchorValidated=true geospatialActorTransformsIdentity=true uniqueCesiumTilesetRoster=true maximumSse=%.1f applyDpiScaling=false maximumCachedBytesSetting=%lld simultaneousLoads=%d forbidHoles=true loadingDescendantLimit=%d preloadAncestors=true preloadSiblings=false frustumCulling=true savedFogCulling=false currentFogCulling=%s transientCurrentViewProviderWorkloadPolicy=%s enforceCulledSse=true culledSse=%.1f useLodTransitions=false generateSmoothNormals=false preserveKhrMaterialsUnlit=true loadProgress=%.1f providerReadyPolicyTickSeconds=%.1f providerReadyRequiredConsecutiveSamples=%d providerReadyConsecutiveSamples=%d visualOnly=true currentPublicContextEpoch=2026-08-31 currentContextSuppressionContract=%s currentContextFeatures=%d currentContextParts=%d currentContextTriangles=%d currentContextSuppressedTriangles=%d currentContextPhysicalUv0=true currentContextNaniteFullMesh=true currentContextRasterFallbackFullMesh=true currentContextRenderOnly=true contextFacadeR25=%s broadShellR31=%s currentContextProviderOverlapResolved=false currentContextCollisionNavigationSensorRfAuthority=false currentContextSurveyAsBuiltHyperreal=false legacyCurrentContextAcceptedAtRuntime=false outerGroundLoadingFallbackTriangles=1280 outerGroundLoadingFallbackSourceCorners=3840 outerGroundLoadingFallbackRenderVertices=768 outerGroundLoadingFallbackProviderCoupled=true outerGroundLoadingFallbackRenderOnly=true outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false outerGroundSurveyAsBuilt=false inheritedV5CBuildingFallbackHidden=true inheritedV5CPlanningGroundHidden=true publicRealmProviderReady=%s publicRealm={%s} r28EnvironmentOptional=true r28EnvironmentCount=%d r28EnvironmentPresent=%s r28EnvironmentProviderReady=%s r28EnvironmentRenderOnlyWhenPresent=true r28EnvironmentCollisionNavigationSensorRfTerrainAuthority=false r28Environment={%s} authoredCoreClipShape=irregularEllipse64 clipCenterMeters=(%.1f,%.1f) clipSemiAxesMeters=(%.1f,%.1f) providerSiteClipActive=true authoredCoreVisualsVisible=true aerialProviderHandoffRequested=false aerialProviderHandoffActive=false aerialReadySamples=0 rendererStateOnly=true cesiumCollisionNavigationSensorRfAuthority=false localFallbackHidden=%s cesiumStandardPersistentHttpRequestCacheAcknowledged=true triadProviderTokenReadSerializedOrLogged=false triadProviderContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false."),
        Tileset->GetIonAssetID(),
        RequiredIstanaLongitudeDegrees,
        RequiredIstanaLatitudeDegrees,
        RequiredIstanaFallbackEllipsoidHeightMeters,
        Tileset->GetMaximumScreenSpaceError(),
        Tileset->MaximumCachedBytes,
        Tileset->MaximumSimultaneousTileLoads,
        Tileset->LoadingDescendantLimit,
        Tileset->EnableFogCulling ? TEXT("true") : TEXT("false"),
        bTransientCurrentViewProviderWorkloadPolicyApplied
            ? TEXT("true")
            : TEXT("false"),
        Tileset->CulledScreenSpaceError,
        Tileset->GetLoadProgress(),
        PrimaryActorTick.TickInterval,
        RequiredProviderReadyConsecutiveSamples,
        ProviderReadyConsecutiveSamples,
        bUsingSuppressionV2
            ? TEXT("local_fallback_suppression_v2")
            : TEXT("local_fallback_suppression_v1"),
        bUsingSuppressionV2 ? 1386 : 1387,
        bUsingSuppressionV2 ? 1388 : 1389,
        bUsingSuppressionV2 ? 43448 : 43492,
        bUsingSuppressionV2 ? 96 : 52,
        bUsingContextFacadeR25 ? TEXT("true") : TEXT("false"),
        bUsingBroadShellR31 ? TEXT("true") : TEXT("false"),
        PublicRealm->bProviderReady ? TEXT("true") : TEXT("false"),
        *PublicRealmReport,
        R28Environment ? 1 : 0,
        R28Environment ? TEXT("true") : TEXT("false"),
        R28Environment
            ? (R28Environment->bProviderReady
                ? TEXT("true")
                : TEXT("false"))
            : TEXT("notApplicable"),
        *R28Report,
        ProviderSiteClipCenterMeters.X,
        ProviderSiteClipCenterMeters.Y,
        ProviderSiteClipSemiAxesMeters.X,
        ProviderSiteClipSemiAxesMeters.Y,
        bLocalBuildingFallbackCurrentlyHidden ? TEXT("true") : TEXT("false"));
    return true;
}

void ATRIADIstanaExploreV5DContextPolicyActor::BeginPlay()
{
    Super::BeginPlay();
    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    ACesium3DTileset* R33CwtTileset = nullptr;
    FString Error;
    const bool bResolvedSceneAndTileset = bR33DualCesiumContextConfigured
        ? ResolveSceneAndR33Tilesets(
              Scene,
              Tileset,
              R33CwtTileset,
              Error)
        : ResolveSceneAndTileset(Scene, Tileset, Error);
    if (bResolvedSceneAndTileset)
    {
        CachedScene = Scene;
        CachedTileset = Tileset;
        if (R33CwtTileset)
        {
            CachedR33CwtTileset = R33CwtTileset;
        }
        if (!ApplyTransientCurrentViewProviderWorkloadPolicy(
                Tileset,
                Error))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("ISTANA_EXPLORE_V5D_CURRENT_VIEW_PROVIDER_WORKLOAD_APPLY_FAIL: %s"),
                *Error);
        }
        UCesiumPolygonRasterOverlay* Overlay = nullptr;
        if (ResolveProviderSiteClipOverlay(Tileset, Overlay, Error))
        {
            CachedSiteClipOverlay = Overlay;
        }
    }
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm = nullptr;
    if (ResolvePublicRealmActor(PublicRealm, Error))
    {
        CachedPublicRealm = PublicRealm;
    }
    ATRIADIstanaExploreV5DR28EnvironmentActor* R28Environment = nullptr;
    if (ResolveOptionalR28EnvironmentActor(R28Environment, Error) &&
        R28Environment)
    {
        CachedR28Environment = R28Environment;
    }
    // Always attempt a safe ground presentation. In particular, do not let
    // stale serialized/editor visibility prevent recovery merely because it
    // makes the pre-recovery contract validation fail.
    RestoreGroundLevelPresentation(Error);
}

void ATRIADIstanaExploreV5DContextPolicyActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ATRIADIstanaPublicViewSceneActor* Scene = CachedScene.Get();
    ACesium3DTileset* Tileset = CachedTileset.Get();
    ACesium3DTileset* R33CwtTileset = CachedR33CwtTileset.Get();
    FString Error;
    FString ValidationReport;
    const auto EnterR33SafeLocal = [this]()
    {
        if (ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor*
                Controller = CachedR33Controller.Get())
        {
            FString R33Error;
            Controller->EnterSafeLocal(R33Error);
        }
    };
    if (!Scene || !Tileset ||
        !ValidateHybridContext(ValidationReport))
    {
        const bool bResolvedSceneAndTileset =
            bR33DualCesiumContextConfigured
            ? ResolveSceneAndR33Tilesets(
                  Scene,
                  Tileset,
                  R33CwtTileset,
                  Error)
            : ResolveSceneAndTileset(Scene, Tileset, Error);
        if (!bResolvedSceneAndTileset ||
            !ValidateHybridContext(ValidationReport))
        {
            EnterR33SafeLocal();
            RestoreGroundLevelPresentation(Error);
            return;
        }
        CachedScene = Scene;
        CachedTileset = Tileset;
        if (R33CwtTileset)
        {
            CachedR33CwtTileset = R33CwtTileset;
        }
    }

    UCesiumPolygonRasterOverlay* Overlay = CachedSiteClipOverlay.Get();
    if (!Overlay &&
        !ResolveProviderSiteClipOverlay(Tileset, Overlay, Error))
    {
        EnterR33SafeLocal();
        RestoreGroundLevelPresentation(Error);
        return;
    }
    CachedSiteClipOverlay = Overlay;

    const float LoadProgress = Tileset->GetLoadProgress();
    if (!FMath::IsFinite(LoadProgress))
    {
        EnterR33SafeLocal();
        RestoreGroundLevelPresentation(Error);
        return;
    }

    if (bR33DualCesiumContextConfigured &&
        (bR33CwtPresentationActive ||
         bR33SafeLocalPresentationActive))
    {
        ProviderReadyConsecutiveSamples = 0;
        bProviderSiteClipCurrentlyActive = true;
        bAuthoredCoreVisualsCurrentlyVisible = true;
        bAerialProviderHandoffRequested = false;
        bAerialProviderHandoffCurrentlyActive = false;
        AerialProviderReadySamples = 0;
        return;
    }

    if (!bLocalBuildingFallbackCurrentlyHidden)
    {
        if (LoadProgress >= HideLocalBuildingFallbackAtLoadProgress)
        {
            ProviderReadyConsecutiveSamples =
                NextProviderReadyConsecutiveSamples(
                LoadProgress,
                HideLocalBuildingFallbackAtLoadProgress,
                ProviderReadyConsecutiveSamples,
                RequiredProviderReadyConsecutiveSamples);
            if (ProviderReadyConsecutiveSamples >=
                RequiredProviderReadyConsecutiveSamples)
            {
                if (!SetProviderReadyPresentation(Scene, true, Error))
                {
                    EnterR33SafeLocal();
                    RestoreGroundLevelPresentation(Error);
                    return;
                }
                ProviderReadyConsecutiveSamples = 0;
            }
        }
        else
        {
            ProviderReadyConsecutiveSamples = 0;
        }
    }
    else
    {
        ProviderReadyConsecutiveSamples = 0;
        if (ShouldRestoreLocalBuildingFallback(
                bLocalBuildingFallbackCurrentlyHidden,
                LoadProgress,
                RestoreLocalBuildingFallbackBelowLoadProgress) &&
            !SetProviderReadyPresentation(Scene, false, Error))
        {
            EnterR33SafeLocal();
            RestoreGroundLevelPresentation(Error);
            return;
        }
    }

    // Provider readiness may control render-only fallbacks, but altitude
    // never changes the authored core or the clipping overlay lifecycle.
    bProviderSiteClipCurrentlyActive = true;
    bAuthoredCoreVisualsCurrentlyVisible = true;
    bAerialProviderHandoffRequested = false;
    bAerialProviderHandoffCurrentlyActive = false;
    AerialProviderReadySamples = 0;
}

void ATRIADIstanaExploreV5DContextPolicyActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    FString Error;
    if (ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor*
            Controller = CachedR33Controller.Get())
    {
        FString R33SafeLocalError;
        Controller->EnterSafeLocal(R33SafeLocalError);
        UnregisterR33CesiumWorldTerrainController(Controller);
    }
    RestoreGroundLevelPresentation(Error);
    if (!SetProviderReadyPresentation(CachedScene.Get(), false, Error))
    {
        if (ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
                CachedPublicRealm.Get())
        {
            PublicRealm->SetProviderReady(false, Error);
        }
        SetLocalBuildingFallbackVisible(CachedScene.Get(), true);
        bLocalBuildingFallbackCurrentlyHidden = false;
    }
    if (ATRIADIstanaExploreV5DR28EnvironmentActor* R28Environment =
            CachedR28Environment.Get())
    {
        FString R28EndPlayError;
        R28Environment->SetProviderReady(false, R28EndPlayError);
    }
    FString R28EndPlayRestoreError;
    RestoreOptionalR28EnvironmentToFallback(R28EndPlayRestoreError);
    bAerialProviderHandoffRequested = false;
    bAerialProviderHandoffCurrentlyActive = false;
    bProviderSiteClipCurrentlyActive = true;
    bAuthoredCoreVisualsCurrentlyVisible = true;
    AerialProviderReadySamples = 0;
    ProviderReadyConsecutiveSamples = 0;
    bR33DualCesiumContextConfigured = false;
    bR33CwtPresentationActive = false;
    bR33SafeLocalPresentationActive = false;
    FString ProviderWorkloadRestoreError;
    if (!RestoreTransientCurrentViewProviderWorkloadPolicy(
            ProviderWorkloadRestoreError))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("ISTANA_EXPLORE_V5D_CURRENT_VIEW_PROVIDER_WORKLOAD_RESTORE_FAIL: %s"),
            *ProviderWorkloadRestoreError);
    }
    CachedScene.Reset();
    CachedPublicRealm.Reset();
    CachedR28Environment.Reset();
    if (EndPlayReason == EEndPlayReason::Quit)
    {
        if (ACesium3DTileset* Tileset = CachedTileset.Get())
        {
            // Cesium's native tileset destruction waits for outstanding HTTP.
            // Start that work while the actor is still reachable, bound the
            // quit-only requests, and prove all native destroy callbacks
            // completed before Unreal's GC wait budget begins.
            std::shared_ptr<spdlog::logger> NativeTilesetLogger;
            const bool bNativeTilesetPresent = Tileset->GetTileset() != nullptr;
            ACesium3DTileset* Cwt = CachedR33CwtTileset.Get();
            if (Cwt == Tileset)
            {
                Cwt = nullptr;
            }
            std::shared_ptr<spdlog::logger> CwtNativeTilesetLogger;
            const bool bCwtNativeTilesetPresent =
                !Cwt || Cwt->GetTileset() != nullptr;
            if (Cesium3DTilesSelection::Tileset* NativeTileset =
                    Tileset->GetTileset())
            {
                NativeTilesetLogger =
                    NativeTileset->getExternals().pLogger;
            }
            if (Cwt)
            {
                if (Cesium3DTilesSelection::Tileset* CwtNativeTileset =
                        Cwt->GetTileset())
                {
                    CwtNativeTilesetLogger =
                        CwtNativeTileset->getExternals().pLogger;
                }
            }

            constexpr double QuitDrainTimeoutSeconds = 5.0;
            const double QuitDrainStartSeconds = FPlatformTime::Seconds();
            bool bQuitDrainReady = false;
            {
                // The default HTTP flush intentionally cancels outstanding
                // quit-only tile requests. Suppress only expected warnings;
                // HTTP errors and critical Cesium failures remain visible.
                FScopedCesiumQuitLoggerLevel CesiumLogGuard(
                    NativeTilesetLogger);
                FScopedCesiumQuitLoggerLevel CwtCesiumLogGuard(
                    CwtNativeTilesetLogger);
                FLogScopedVerbosityOverride HttpLogGuard(
                    &LogHttp,
                    ELogVerbosity::Error);

                Tileset->SuspendUpdate = true;
                Tileset->RefreshTileset();
                if (Cwt)
                {
                    Cwt->SuspendUpdate = true;
                    Cwt->RefreshTileset();
                }
                FHttpModule::Get().GetHttpManager().Flush(
                    EHttpFlushReason::Default);

                do
                {
                    bQuitDrainReady = Tileset->IsReadyForFinishDestroy();
                    if (Cwt)
                    {
                        bQuitDrainReady = bQuitDrainReady &&
                            Cwt->IsReadyForFinishDestroy();
                    }
                    if (!bQuitDrainReady)
                    {
                        FPlatformProcess::SleepNoStats(0.001f);
                    }
                }
                while (!bQuitDrainReady &&
                       FPlatformTime::Seconds() - QuitDrainStartSeconds <
                           QuitDrainTimeoutSeconds);
            }

            const double QuitDrainElapsedMilliseconds =
                (FPlatformTime::Seconds() - QuitDrainStartSeconds) * 1000.0;
            if (bQuitDrainReady && bNativeTilesetPresent &&
                bCwtNativeTilesetPresent)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("ISTANA_EXPLORE_V5D_CESIUM_QUIT_DRAIN_PASS "
                         "ready=true nativeTilesetPresent=true elapsedMs=%.3f"),
                    QuitDrainElapsedMilliseconds);
            }
            else
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("ISTANA_EXPLORE_V5D_CESIUM_QUIT_DRAIN_FAIL "
                         "ready=%s nativeTilesetPresent=%s elapsedMs=%.3f"),
                    bQuitDrainReady ? TEXT("true") : TEXT("false"),
                    bNativeTilesetPresent ? TEXT("true") : TEXT("false"),
                    QuitDrainElapsedMilliseconds);
            }
        }
        else
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("ISTANA_EXPLORE_V5D_CESIUM_QUIT_DRAIN_FAIL "
                     "ready=false nativeTilesetPresent=false elapsedMs=0.000"));
        }
    }
    CachedTileset.Reset();
    CachedR33CwtTileset.Reset();
    CachedR33Controller.Reset();
    CachedSiteClipOverlay.Reset();
    Super::EndPlay(EndPlayReason);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DGeospatialAnchorValuesTest,
    "TRIAD.Istana.ExploreV5D.Hybrid.GeospatialAnchorValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DGeospatialAnchorValuesTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    const FVector ExpectedOrigin(
        RequiredIstanaLongitudeDegrees,
        RequiredIstanaLatitudeDegrees,
        RequiredIstanaFallbackEllipsoidHeightMeters);
    TestTrue(
        TEXT("Exact Istana cartographic-origin values are accepted"),
        HasExpectedIstanaGeoreferenceValues(
            EOriginPlacement::CartographicOrigin,
            ExpectedOrigin,
            RequiredCesiumScale));
    TestFalse(
        TEXT("A non-cartographic origin placement is rejected"),
        HasExpectedIstanaGeoreferenceValues(
            EOriginPlacement::TrueOrigin,
            ExpectedOrigin,
            RequiredCesiumScale));
    TestFalse(
        TEXT("A longitude drift outside the exact tolerance is rejected"),
        HasExpectedIstanaGeoreferenceValues(
            EOriginPlacement::CartographicOrigin,
            ExpectedOrigin + FVector(0.000001, 0.0, 0.0),
            RequiredCesiumScale));
    TestFalse(
        TEXT("A latitude drift outside the exact tolerance is rejected"),
        HasExpectedIstanaGeoreferenceValues(
            EOriginPlacement::CartographicOrigin,
            ExpectedOrigin + FVector(0.0, 0.000001, 0.0),
            RequiredCesiumScale));
    TestFalse(
        TEXT("An ellipsoid-height drift is rejected"),
        HasExpectedIstanaGeoreferenceValues(
            EOriginPlacement::CartographicOrigin,
            ExpectedOrigin + FVector(0.0, 0.0, 0.01),
            RequiredCesiumScale));
    TestFalse(
        TEXT("A Cesium scale drift is rejected"),
        HasExpectedIstanaGeoreferenceValues(
            EOriginPlacement::CartographicOrigin,
            ExpectedOrigin,
            RequiredCesiumScale + 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DUniqueVisualTilesetRosterTest,
    "TRIAD.Istana.ExploreV5D.Hybrid.UniqueVisualTilesetRoster",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DUniqueVisualTilesetRosterTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TestTrue(
        TEXT("Exactly one Cesium tileset carrying the visual-provider tag is accepted"),
        HasExpectedVisualTilesetRoster(1, 1));
    TestFalse(
        TEXT("An extra untagged Cesium tileset is rejected"),
        HasExpectedVisualTilesetRoster(2, 1));
    TestFalse(
        TEXT("A sole untagged Cesium tileset is rejected"),
        HasExpectedVisualTilesetRoster(1, 0));
    TestFalse(
        TEXT("Multiple tagged Cesium tilesets are rejected"),
        HasExpectedVisualTilesetRoster(2, 2));
    TestFalse(
        TEXT("A world without a Cesium tileset is rejected"),
        HasExpectedVisualTilesetRoster(0, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DCurrentViewProviderWorkloadStateTest,
    "TRIAD.Istana.ExploreV5D.Hybrid.CurrentViewProviderWorkloadState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DCurrentViewProviderWorkloadStateTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TestTrue(
        TEXT("A cold editor world retains the exact saved fog-culling state"),
        HasExpectedCurrentViewProviderWorkloadState(
            false,
            false,
            false,
            false,
            false));
    TestTrue(
        TEXT("A game world requires the exact transient current-view state"),
        HasExpectedCurrentViewProviderWorkloadState(
            true,
            true,
            true,
            false,
            true));
    TestFalse(
        TEXT("A game world rejects an unapplied workload policy"),
        HasExpectedCurrentViewProviderWorkloadState(
            true,
            false,
            false,
            false,
            false));
    TestFalse(
        TEXT("A game world rejects a noncanonical saved fog-culling value"),
        HasExpectedCurrentViewProviderWorkloadState(
            true,
            true,
            true,
            true,
            true));
    TestFalse(
        TEXT("A cold editor world rejects leaked runtime state"),
        HasExpectedCurrentViewProviderWorkloadState(
            false,
            true,
            true,
            false,
            true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DStableVisualPolicyTest,
    "TRIAD.Istana.ExploreV5D.Hybrid.StableVisualPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DStableVisualPolicyTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    const ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        GetDefault<ATRIADIstanaExploreV5DContextPolicyActor>();
    TestNotNull(TEXT("ContextPolicy default object exists"), Policy);
    if (Policy)
    {
        TestTrue(TEXT("Provider site clipping remains active"),
            Policy->bProviderSiteClipCurrentlyActive);
        TestTrue(TEXT("Authored core remains visible"),
            Policy->bAuthoredCoreVisualsCurrentlyVisible);
        TestFalse(TEXT("No aerial handoff is requested"),
            Policy->bAerialProviderHandoffRequested);
        TestFalse(TEXT("No aerial handoff is active"),
            Policy->bAerialProviderHandoffCurrentlyActive);
        TestEqual(TEXT("No aerial readiness samples are retained"),
            Policy->AerialProviderReadySamples, 0);
        TestEqual(TEXT("Three stable provider samples are required"),
            Policy->RequiredProviderReadyConsecutiveSamples, 3);
        TestEqual(TEXT("No provider readiness samples are retained initially"),
            Policy->ProviderReadyConsecutiveSamples, 0);
        TestFalse(
            TEXT("No transient current-view workload state is serialized"),
            Policy->bTransientCurrentViewProviderWorkloadPolicyApplied);
        TestEqual(
            TEXT("Runtime pins the suppression-V1 predecessor path"),
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedCurrentSurroundingsMeshObjectPath(),
            CurrentSurroundingsMeshObjectPath);
        TestEqual(
            TEXT("Runtime pins the independent suppression-V2 successor path"),
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedCurrentSurroundingsV2MeshObjectPath(),
            CurrentSurroundingsV2MeshObjectPath);
        TestEqual(
            TEXT("Runtime pins the outer-ground loading fallback path"),
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedOuterGroundLoadingFallbackMeshObjectPath(),
            OuterGroundLoadingFallbackMeshObjectPath);
        TestTrue(TEXT("Outer ground is explicitly render only"),
            Policy->bOuterGroundLoadingFallbackRenderOnly);
        TestFalse(TEXT("Outer ground has no collision/navigation/sensor/RF/terrain authority"),
            Policy->bOuterGroundLoadingFallbackCollisionNavigationSensorRfTerrainAuthority);
        TestFalse(TEXT("Outer ground has no survey/as-built claim"),
            Policy->bOuterGroundLoadingFallbackSurveyAsBuilt);
        const UStaticMeshComponent* Outer =
            Policy->OuterGroundLoadingFallbackRenderOnlyComponent;
        TestNotNull(TEXT("Native outer-ground default subobject exists"), Outer);
        if (Outer)
        {
            TestEqual(TEXT("Outer ground has no collision"),
                Outer->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
            TestFalse(TEXT("Outer ground generates no overlap events"),
                Outer->GetGenerateOverlapEvents());
            TestFalse(TEXT("Outer ground has no navigation relevance"),
                Outer->CanEverAffectNavigation());
            TestFalse(TEXT("Outer ground casts no shadow"), Outer->CastShadow);
            TestFalse(TEXT("Outer ground affects no distance field"),
                Outer->bAffectDistanceFieldLighting);
            TestTrue(TEXT("Outer ground starts fail-closed visible"),
                Outer->IsVisible() && !Outer->bHiddenInGame);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DSuccessorFallbackContractTest,
    "TRIAD.Istana.ExploreV5D.Hybrid.SuccessorFallbackContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DSuccessorFallbackContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    int32 TriangleTotal = 0;
    for (const FCurrentSurroundingsMaterialSpec& Spec :
         CurrentSurroundingsMaterials)
    {
        TriangleTotal += Spec.Triangles;
    }
    TestEqual(TEXT("Suppression successor exact section total"),
        TriangleTotal, 43492);
    const auto* SuccessorProvenance = GetDefault<
        UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance>();
    const auto* OuterProvenance = GetDefault<
        UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance>();
    TestTrue(TEXT("Suppression successor provenance is canonical"),
        SuccessorProvenance && SuccessorProvenance->IsCanonicalContract());
    int32 V2TriangleTotal = 0;
    for (const FCurrentSurroundingsMaterialSpec& Spec :
         CurrentSurroundingsV2Materials)
    {
        V2TriangleTotal += Spec.Triangles;
    }
    TestEqual(TEXT("Suppression V2 successor exact section total"),
        V2TriangleTotal, 43448);
    const auto* V2SuccessorProvenance = GetDefault<
        UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance>();
    TestTrue(TEXT("Suppression V2 successor provenance is canonical"),
        V2SuccessorProvenance &&
            V2SuccessorProvenance->IsCanonicalContract());
    TestEqual(TEXT("Suppression V2 omits exactly 96 triangles"),
        V2SuccessorProvenance
            ? V2SuccessorProvenance->SuppressedTriangles
            : 0,
        96);
    TestTrue(TEXT("Outer-ground provenance is canonical"),
        OuterProvenance && OuterProvenance->IsCanonicalContract());
    TestEqual(TEXT("Outer-ground exact triangle census"),
        OuterProvenance ? OuterProvenance->Triangles : 0, 1280);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DProviderReadinessHysteresisTest,
    "TRIAD.Istana.ExploreV5D.Hybrid.ProviderReadinessHysteresis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DProviderReadinessHysteresisTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    int32 Samples = 0;
    Samples = NextProviderReadyConsecutiveSamples(98.0f, 98.0f, Samples, 3);
    TestEqual(TEXT("First ready observation is retained"), Samples, 1);
    Samples = NextProviderReadyConsecutiveSamples(100.0f, 98.0f, Samples, 3);
    TestEqual(TEXT("Second ready observation is retained"), Samples, 2);
    Samples = NextProviderReadyConsecutiveSamples(97.99f, 98.0f, Samples, 3);
    TestEqual(TEXT("One interrupted observation resets the dwell"), Samples, 0);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Samples = NextProviderReadyConsecutiveSamples(
            98.0f,
            98.0f,
            Samples,
            3);
    }
    TestEqual(TEXT("Exactly three consecutive observations reach readiness"),
        Samples, 3);
    TestTrue(TEXT("Hidden fallback restores below 90 percent"),
        ShouldRestoreLocalBuildingFallback(true, 89.99f, 90.0f));
    TestFalse(TEXT("Restore threshold itself remains in hysteresis"),
        ShouldRestoreLocalBuildingFallback(true, 90.0f, 90.0f));
    TestFalse(TEXT("Hidden fallback remains hidden inside the 90 to 98 percent hysteresis band"),
        ShouldRestoreLocalBuildingFallback(true, 95.0f, 90.0f));
    TestFalse(TEXT("Visible fallback is never redundantly restored"),
        ShouldRestoreLocalBuildingFallback(false, 0.0f, 90.0f));
    return true;
}
#endif
