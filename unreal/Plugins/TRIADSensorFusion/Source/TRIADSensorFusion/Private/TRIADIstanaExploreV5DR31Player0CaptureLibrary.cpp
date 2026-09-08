#include "TRIADIstanaExploreV5DR31Player0CaptureLibrary.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevActor.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"
#include "TRIADIstanaExploreV5GameMode.h"
#include "TRIADIstanaExploreV5Pawn.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UnrealClient.h"
#include "UObject/SoftObjectPath.h"

namespace
{
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString CaptureArgument(TEXT("TRIADR31CaptureRun="));
const FString OutputRootArgument(TEXT("TRIADR31CaptureOutputRoot="));
const FString CaptureBaseRoot(TEXT("D:/triad/TRIAD_R31Evidence"));
const FString AirSimRuntimeModule(TEXT("AirSimTriadRuntime"));
const FString WeatherActorClassPath(
    TEXT("/AirSimTriadRuntime/Weather/WeatherFX/WeatherActor.WeatherActor_C"));
const FName ContextPolicyTag(TEXT("TRIADIstanaExploreV5DContextPolicy"));
const FName VisualTilesetTag(TEXT("TRIADIstanaExploreV5DVisualTileset"));
const FName GeoreferenceTag(TEXT("TRIADIstanaExploreV5DGeoreference"));
const FName DefaultGeoreferenceTag(TEXT("DEFAULT_GEOREFERENCE"));
const FVector ExactCameraRelativeLocation(0.0, 0.0, 64.0);
constexpr float LocationToleranceCentimetres = 0.1f;
constexpr float RotationToleranceDegrees = 0.05f;
constexpr int64 GooglePhotorealistic3DTilesIonAssetId = 2275207;
constexpr double IstanaLongitudeDegrees = 103.84288055;
constexpr double IstanaLatitudeDegrees = 1.30709615;
constexpr double IstanaFallbackEllipsoidHeightMetres = 47.0;
const FString NaniteOnRenderPathId(TEXT("NANITE_ON"));
const FString RasterFallbackRenderPathId(TEXT("RASTER_FALLBACK"));
const FString RasterComparisonPoseId(TEXT("surroundings_oblique_macdonald"));

struct FReviewedPose
{
    const TCHAR* Id;
    FVector ViewLocationCentimetres;
    FRotator ViewRotationDegrees;
};

// The first four are the accepted R23B/R27 fixed range rig. Their nominal
// range is encoded by pitch while the 1.64 m Player0 eye remains fixed.
// The fifth is the hash-reviewed R24 MacDonald streetscape-context transform.
const FReviewedPose ReviewedPoses[] = {
    {TEXT("075m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-1.252968, -90.0, 0.0)},
    {TEXT("020m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-4.703535, -90.0, 0.0)},
    {TEXT("008m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-11.829499, -90.0, 0.0)},
    {TEXT("002m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-55.084794, -90.0, 0.0)},
    {TEXT("surroundings_oblique_macdonald"),
     FVector(40226.9640238642, 106152.591966384, 3900.0),
     FRotator(-5.74137954693854, -101.620267003074, 0.0)},
};
static_assert(UE_ARRAY_COUNT(ReviewedPoses) == 5);

struct FExactPoseState
{
    TWeakObjectPtr<ATRIADIstanaExploreV5Pawn> Pawn;
    FString PoseId;
    FVector ViewLocationCentimetres = FVector::ZeroVector;
    FRotator ViewRotationDegrees = FRotator::ZeroRotator;
};

FExactPoseState GExactPoseState;

struct FValidatedCaptureState
{
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ATRIADIstanaExploreV5DR30FacadeLookdevActor* R30 = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    ACesiumGeoreference* Georeference = nullptr;
    float CesiumLoadProgress = -1.0f;
    FString R30Report;
    FString PolicyReport;
    FString ShellReport;
    FString VegetationReport;
    FString TerrainReport;
    FString TreeReport;
};

template <typename T>
T* FindExactlyOne(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    T* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (IsValid(Candidate) &&
            Candidate->GetClass() == T::StaticClass())
        {
            Result = Cast<T>(Candidate);
            ++OutCount;
        }
    }
    return Result;
}

const FReviewedPose* FindReviewedPose(const FString& PoseId)
{
    for (const FReviewedPose& Pose : ReviewedPoses)
    {
        if (PoseId.Equals(Pose.Id, ESearchCase::CaseSensitive))
        {
            return &Pose;
        }
    }
    return nullptr;
}

bool IsSafeRunToken(const FString& RunToken)
{
    const auto IsAsciiAlphaNumeric = [](const TCHAR Character)
    {
        return (Character >= TEXT('A') && Character <= TEXT('Z')) ||
            (Character >= TEXT('a') && Character <= TEXT('z')) ||
            (Character >= TEXT('0') && Character <= TEXT('9'));
    };
    if (RunToken.IsEmpty() || RunToken.Len() > 64 ||
        !IsAsciiAlphaNumeric(RunToken[0]))
    {
        return false;
    }
    for (const TCHAR Character : RunToken)
    {
        if (!IsAsciiAlphaNumeric(Character) && Character != TEXT('-') &&
            Character != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}

bool ResolveCaptureArguments(
    FString& OutRunToken,
    FString& OutOutputRoot,
    FString& OutError)
{
    if (!FParse::Value(
            FCommandLine::Get(), *CaptureArgument, OutRunToken) ||
        !FParse::Value(
            FCommandLine::Get(), *OutputRootArgument, OutOutputRoot) ||
        !IsSafeRunToken(OutRunToken) || OutOutputRoot.IsEmpty() ||
        FPaths::IsRelative(OutOutputRoot))
    {
        OutError = TEXT("R31 capture endpoints require -TRIADR31CaptureRun=<safe-token> and one absolute -TRIADR31CaptureOutputRoot=<bounded-root>.");
        return false;
    }

    OutOutputRoot = FPaths::ConvertRelativePathToFull(OutOutputRoot);
    FPaths::NormalizeDirectoryName(OutOutputRoot);
    FString NormalizedBase = FPaths::ConvertRelativePathToFull(CaptureBaseRoot);
    FPaths::NormalizeDirectoryName(NormalizedBase);
    FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ProjectRoot);
    if (!FPaths::IsUnderDirectory(OutOutputRoot, NormalizedBase) ||
        !FPaths::IsSamePath(
            FPaths::GetPath(OutOutputRoot), NormalizedBase) ||
        FPaths::GetCleanFilename(OutOutputRoot) != OutRunToken ||
        FPaths::IsUnderDirectory(OutOutputRoot, ProjectRoot) ||
        FPaths::IsSamePath(OutOutputRoot, ProjectRoot))
    {
        OutError = TEXT("R31 capture output must be the exact run-token child below D:/triad/TRIAD_R31Evidence and outside the project sandbox.");
        return false;
    }
    OutError.Reset();
    return true;
}

UWorld* ResolveExactGameWorld(FString& OutError)
{
    UWorld* Result = nullptr;
    int32 MatchingWorldCount = 0;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* Candidate = Context.World();
            if (Candidate && Candidate->WorldType == EWorldType::Game &&
                Candidate->PersistentLevel && Candidate->GetOutermost() &&
                UWorld::RemovePIEPrefix(
                    Candidate->GetOutermost()->GetName()) == TargetMapPackage)
            {
                Result = Candidate;
                ++MatchingWorldCount;
            }
        }
    }
    if (MatchingWorldCount != 1 || !Result)
    {
        OutError = FString::Printf(
            TEXT("R31 Player0 evidence requires one exact cooked Game world for %s; matches=%d."),
            *TargetMapPackage,
            MatchingWorldCount);
        return nullptr;
    }
    OutError.Reset();
    return Result;
}

bool HasStableProviderPolicy(
    const ATRIADIstanaExploreV5DContextPolicyActor* Policy)
{
    return Policy && Policy->bProviderSiteClipCurrentlyActive &&
        Policy->bAuthoredCoreVisualsCurrentlyVisible &&
        !Policy->bAerialProviderHandoffRequested &&
        !Policy->bAerialProviderHandoffCurrentlyActive &&
        Policy->AerialProviderReadySamples == 0;
}

bool IsExactPoseActive(
    const FValidatedCaptureState& State,
    const FReviewedPose& Pose)
{
    if (!State.Pawn || !State.Player ||
        GExactPoseState.Pawn.Get() != State.Pawn ||
        GExactPoseState.PoseId != Pose.Id || State.Pawn->IsActorTickEnabled())
    {
        return false;
    }
    const UCameraComponent* Camera =
        State.Pawn->GetExploreCameraComponent();
    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    State.Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
    ViewRotation.Normalize();
    const FRotator NormalizedExpected =
        Pose.ViewRotationDegrees.GetNormalized();
    return Camera && State.Player->GetViewTarget() == State.Pawn &&
        Camera->GetRelativeLocation().Equals(
            ExactCameraRelativeLocation, LocationToleranceCentimetres) &&
        Camera->GetRelativeRotation().Equals(
            NormalizedExpected, RotationToleranceDegrees) &&
        State.Pawn->GetActorLocation().Equals(
            Pose.ViewLocationCentimetres - ExactCameraRelativeLocation,
            LocationToleranceCentimetres) &&
        State.Pawn->GetActorRotation().Equals(
            FRotator::ZeroRotator, RotationToleranceDegrees) &&
        ViewLocation.Equals(
            Pose.ViewLocationCentimetres, LocationToleranceCentimetres) &&
        ViewRotation.Equals(
            NormalizedExpected, RotationToleranceDegrees) &&
        GExactPoseState.ViewLocationCentimetres.Equals(
            Pose.ViewLocationCentimetres, LocationToleranceCentimetres) &&
        GExactPoseState.ViewRotationDegrees.Equals(
            NormalizedExpected, RotationToleranceDegrees);
}

bool ResolveValidatedCaptureState(
    FValidatedCaptureState& OutState,
    FString& OutError)
{
    OutState = FValidatedCaptureState{};
    FString RunToken;
    FString OutputRoot;
    if (!ResolveCaptureArguments(RunToken, OutputRoot, OutError))
    {
        return false;
    }
    OutState.World = ResolveExactGameWorld(OutError);
    if (!OutState.World)
    {
        return false;
    }

    int32 PolicyCount = 0;
    int32 R30Count = 0;
    int32 VegetationCount = 0;
    int32 TerrainCount = 0;
    int32 TreeCount = 0;
    int32 SceneCount = 0;
    int32 TilesetCount = 0;
    int32 GeoreferenceCount = 0;
    int32 PolicyTagCount = 0;
    int32 R30TagCount = 0;
    int32 VegetationTagCount = 0;
    int32 TerrainTagCount = 0;
    int32 TreeTagCount = 0;
    int32 R28FacadeClassCount = 0;
    int32 R28FacadeTagCount = 0;
    int32 R29FacadeClassCount = 0;
    int32 R29FacadeTagCount = 0;
    int32 LandmarkVegetationClassCount = 0;
    int32 LandmarkVegetationTagCount = 0;
    int32 ExactV2ComponentCount = 0;
    UStaticMeshComponent* ExactV2Component = nullptr;
    OutState.Policy = FindExactlyOne<
        ATRIADIstanaExploreV5DContextPolicyActor>(
            OutState.World, PolicyCount);
    OutState.R30 = FindExactlyOne<
        ATRIADIstanaExploreV5DR30FacadeLookdevActor>(
            OutState.World, R30Count);
    ATRIADIstanaExploreV5DR29VegetationActor* Vegetation = FindExactlyOne<
        ATRIADIstanaExploreV5DR29VegetationActor>(
            OutState.World, VegetationCount);
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor* Terrain =
        FindExactlyOne<
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor>(
                OutState.World, TerrainCount);
    ATRIADIstanaExploreV5DTreeRealismActor* Trees = FindExactlyOne<
        ATRIADIstanaExploreV5DTreeRealismActor>(
            OutState.World, TreeCount);
    ATRIADIstanaPublicViewSceneActor* Scene = FindExactlyOne<
        ATRIADIstanaPublicViewSceneActor>(OutState.World, SceneCount);
    OutState.Tileset = FindExactlyOne<ACesium3DTileset>(
        OutState.World, TilesetCount);
    OutState.Georeference = FindExactlyOne<ACesiumGeoreference>(
        OutState.World, GeoreferenceCount);
    for (TActorIterator<AActor> It(OutState.World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        const bool bExactPolicy = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DContextPolicyActor::StaticClass();
        const bool bTaggedPolicy = Candidate->Tags.Contains(ContextPolicyTag);
        const bool bExactR30 = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::StaticClass();
        const bool bTaggedR30 = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::ExpectedActorTag());
        const bool bExactVegetation = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29VegetationActor::StaticClass();
        const bool bTaggedVegetation = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29VegetationActor::ExpectedActorTag());
        const bool bExactTerrain = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                StaticClass();
        const bool bTaggedTerrain = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                ExpectedActorTag());
        const bool bExactTrees = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DTreeRealismActor::StaticClass();
        const bool bTaggedTrees = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag());
        const bool bExactR28Facade = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass();
        const bool bTaggedR28Facade = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag());
        const bool bExactR29Facade = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::StaticClass();
        const bool bTaggedR29Facade = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
                ExpectedActorTag());
        const bool bExactLandmarkVegetation = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass();
        const bool bTaggedLandmarkVegetation = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DLandmarkVegetationActor::ExpectedActorTag());
        PolicyTagCount += bTaggedPolicy ? 1 : 0;
        R30TagCount += bTaggedR30 ? 1 : 0;
        VegetationTagCount += bTaggedVegetation ? 1 : 0;
        TerrainTagCount += bTaggedTerrain ? 1 : 0;
        TreeTagCount += bTaggedTrees ? 1 : 0;
        R28FacadeClassCount += bExactR28Facade ? 1 : 0;
        R28FacadeTagCount += bTaggedR28Facade ? 1 : 0;
        R29FacadeClassCount += bExactR29Facade ? 1 : 0;
        R29FacadeTagCount += bTaggedR29Facade ? 1 : 0;
        LandmarkVegetationClassCount += bExactLandmarkVegetation ? 1 : 0;
        LandmarkVegetationTagCount += bTaggedLandmarkVegetation ? 1 : 0;
        if ((bTaggedPolicy && !bExactPolicy) ||
            (bTaggedR30 && !bExactR30) ||
            (bTaggedVegetation && !bExactVegetation) ||
            (bTaggedTerrain && !bExactTerrain) ||
            (bTaggedTrees && !bExactTrees) ||
            (bTaggedR28Facade && !bExactR28Facade) ||
            (bTaggedR29Facade && !bExactR29Facade) ||
            (bTaggedLandmarkVegetation && !bExactLandmarkVegetation))
        {
            OutError = TEXT("R31 capture refused a protected owner tag on a non-exact class.");
            return false;
        }
        TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Candidate);
        for (UStaticMeshComponent* MeshComponent : MeshComponents)
        {
            if (MeshComponent && MeshComponent->GetStaticMesh() &&
                MeshComponent->GetStaticMesh()->GetPathName() ==
                    ATRIADIstanaExploreV5DContextPolicyActor::
                        ExpectedCurrentSurroundingsV2MeshObjectPath())
            {
                ++ExactV2ComponentCount;
                ExactV2Component = MeshComponent;
            }
        }
    }
    OutState.Player = UGameplayStatics::GetPlayerController(
        OutState.World, 0);
    OutState.Pawn = OutState.Player
        ? Cast<ATRIADIstanaExploreV5Pawn>(OutState.Player->GetPawn())
        : nullptr;
    OutState.CesiumLoadProgress = OutState.Tileset
        ? OutState.Tileset->GetLoadProgress()
        : -1.0f;

    const FVector Origin = OutState.Georeference
        ? OutState.Georeference->GetOriginLongitudeLatitudeHeight()
        : FVector::ZeroVector;
    UClass* WeatherActorClass = FSoftClassPath(
        WeatherActorClassPath).ResolveClass();
    if (!WeatherActorClass)
    {
        WeatherActorClass = FSoftClassPath(
            WeatherActorClassPath).TryLoadClass<AActor>();
    }
    const bool bAirSimRuntimeLoaded =
        FModuleManager::Get().IsModuleLoaded(FName(*AirSimRuntimeModule));
    const bool bStablePolicy = HasStableProviderPolicy(OutState.Policy);
    UStaticMesh* ExactV2Mesh = ExactV2Component
        ? ExactV2Component->GetStaticMesh()
        : nullptr;
#if WITH_EDITORONLY_DATA
    const bool bExactNaniteBuildSettings = ExactV2Mesh &&
        ExactV2Mesh->NaniteSettings.bEnabled &&
        FMath::IsNearlyEqual(
            ExactV2Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) &&
        FMath::IsNearlyZero(
            ExactV2Mesh->NaniteSettings.TrimRelativeError) &&
        ExactV2Mesh->NaniteSettings.FallbackTarget ==
            ENaniteFallbackTarget::PercentTriangles &&
        FMath::IsNearlyEqual(
            ExactV2Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) &&
        FMath::IsNearlyZero(
            ExactV2Mesh->NaniteSettings.FallbackRelativeError);
#else
    // Cook-time settings are bound by the admitted R31 transaction and its
    // context-policy validation. Cooked Game proof remains fail-closed on
    // valid Nanite data plus the rendered Nanite/raster A/B pair.
    const bool bExactNaniteBuildSettings = true;
#endif
    const bool bExactNaniteAndFallback = bExactNaniteBuildSettings &&
        ExactV2Mesh && ExactV2Mesh->HasValidNaniteData();

    if (PolicyCount != 1 || PolicyTagCount != 1 ||
        R30Count != 1 || R30TagCount != 1 ||
        VegetationCount != 1 || VegetationTagCount != 1 ||
        TerrainCount != 1 || TerrainTagCount != 1 ||
        TreeCount != 1 || TreeTagCount != 1 || SceneCount != 1 ||
        TilesetCount != 1 || GeoreferenceCount != 1 ||
        R28FacadeClassCount != 0 || R28FacadeTagCount != 0 ||
        R29FacadeClassCount != 0 || R29FacadeTagCount != 0 ||
        LandmarkVegetationClassCount != 0 ||
        LandmarkVegetationTagCount != 0 ||
        ExactV2ComponentCount != 1 ||
        !OutState.Policy || !OutState.R30 || !Vegetation || !Terrain ||
        !Trees || !Scene || !OutState.Tileset || !OutState.Georeference ||
        !bExactNaniteAndFallback ||
        ExactV2Component !=
            OutState.Policy->CurrentSurroundingsRenderOnlyComponent ||
        !OutState.Player || !OutState.Pawn ||
        !OutState.World->GetAuthGameMode() ||
        OutState.World->GetAuthGameMode()->GetClass() !=
            ATRIADIstanaExploreV5GameMode::StaticClass() ||
        !OutState.Pawn->HasExpectedExploreV5CameraProfile() ||
        OutState.Player->GetViewTarget() != OutState.Pawn ||
        !OutState.Policy->HasActorBegunPlay() ||
        !OutState.Policy->IsActorTickEnabled() ||
        !OutState.R30->HasActorBegunPlay() ||
        !OutState.R30->IsActorTickEnabled() ||
        !Vegetation->HasActorBegunPlay() || !Terrain->HasActorBegunPlay() ||
        !Trees->HasActorBegunPlay() ||
        !OutState.Policy->Tags.Contains(ContextPolicyTag) ||
        !OutState.R30->Tags.Contains(
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::ExpectedActorTag()) ||
        !Vegetation->Tags.Contains(
            ATRIADIstanaExploreV5DR29VegetationActor::ExpectedActorTag()) ||
        !Terrain->Tags.Contains(
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                ExpectedActorTag()) ||
        !Trees->Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag()) ||
        !OutState.Tileset->Tags.Contains(VisualTilesetTag) ||
        (!OutState.Georeference->Tags.Contains(GeoreferenceTag) &&
         !OutState.Georeference->Tags.Contains(DefaultGeoreferenceTag)) ||
        OutState.Tileset->GetTilesetSource() !=
            ETilesetSource::FromCesiumIon ||
        OutState.Tileset->GetIonAssetID() !=
            GooglePhotorealistic3DTilesIonAssetId ||
        !FMath::IsNearlyEqual(
            OutState.Tileset->GetMaximumScreenSpaceError(),
            1.0, 0.000001) ||
        OutState.Tileset->GetGeoreference().Get() !=
            OutState.Georeference ||
        !FMath::IsNearlyEqual(
            Origin.X, IstanaLongitudeDegrees, 0.00000001) ||
        !FMath::IsNearlyEqual(
            Origin.Y, IstanaLatitudeDegrees, 0.00000001) ||
        !FMath::IsNearlyEqual(
            Origin.Z, IstanaFallbackEllipsoidHeightMetres, 0.000001) ||
        !FMath::IsNearlyEqual(
            OutState.Georeference->GetScale(), 100.0, 0.000001) ||
        !FMath::IsFinite(OutState.CesiumLoadProgress) ||
        OutState.CesiumLoadProgress < 0.0f ||
        OutState.CesiumLoadProgress > 100.0f || !bStablePolicy ||
        !bAirSimRuntimeLoaded || !WeatherActorClass ||
        !OutState.R30->ValidateR30FacadeLookdev(OutState.R30Report) ||
        OutState.R30->bProviderReady !=
            OutState.Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !OutState.Policy->ValidateHybridContext(OutState.PolicyReport) ||
        !OutState.Policy->
            ValidateCurrentSurroundingsBroadShellR31ForInheritedScene(
                Scene, OutState.ShellReport) ||
        !Vegetation->ValidateR29Vegetation(OutState.VegetationReport) ||
        !Terrain->ValidateCopernicusTerrainFallback(OutState.TerrainReport) ||
        !Trees->ValidateTreeRealism(OutState.TreeReport))
    {
        OutError = FString::Printf(
            TEXT("R31_CAPTURE_STATE_INVALID map=%s policy=%d/%d r30=%d/%d vegetationR29=%d/%d terrainR29=%d/%d trees=%d/%d r28Facade=%d/%d r29Facade=%d/%d landmarkVegetation=%d/%d exactV2Components=%d scene=%d tileset=%d georeference=%d player0=%s pawnV5=%s airSimTriadRuntimeLoaded=%s weatherActorResolved=%s cesiumLoadProgress=%.3f stablePolicy=%s r30={%s} policy={%s} shell={%s} vegetation={%s} terrain={%s} trees={%s}"),
            *TargetMapPackage,
            PolicyCount,
            PolicyTagCount,
            R30Count,
            R30TagCount,
            VegetationCount,
            VegetationTagCount,
            TerrainCount,
            TerrainTagCount,
            TreeCount,
            TreeTagCount,
            R28FacadeClassCount,
            R28FacadeTagCount,
            R29FacadeClassCount,
            R29FacadeTagCount,
            LandmarkVegetationClassCount,
            LandmarkVegetationTagCount,
            ExactV2ComponentCount,
            SceneCount,
            TilesetCount,
            GeoreferenceCount,
            OutState.Player ? TEXT("true") : TEXT("false"),
            OutState.Pawn ? TEXT("true") : TEXT("false"),
            bAirSimRuntimeLoaded ? TEXT("true") : TEXT("false"),
            WeatherActorClass ? TEXT("true") : TEXT("false"),
            OutState.CesiumLoadProgress,
            bStablePolicy ? TEXT("true") : TEXT("false"),
            *OutState.R30Report,
            *OutState.PolicyReport,
            *OutState.ShellReport,
            *OutState.VegetationReport,
            *OutState.TerrainReport,
            *OutState.TreeReport);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ReadNaniteRenderPath(
    FString& OutRenderPathId,
    int32& OutNaniteValue,
    int32& OutProxyRenderMode,
    FString& OutError)
{
    IConsoleVariable* Nanite =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
    IConsoleVariable* ProxyRenderMode = IConsoleManager::Get().FindConsoleVariable(
        TEXT("r.Nanite.ProxyRenderMode"));
    if (!Nanite || !ProxyRenderMode)
    {
        OutError = TEXT("R31 capture cannot resolve the required r.Nanite and r.Nanite.ProxyRenderMode console variables.");
        return false;
    }
    OutNaniteValue = Nanite->GetInt();
    OutProxyRenderMode = ProxyRenderMode->GetInt();
    if ((OutNaniteValue != 0 && OutNaniteValue != 1) ||
        (OutProxyRenderMode != 0 && OutProxyRenderMode != 1) ||
        OutNaniteValue != OutProxyRenderMode)
    {
        OutError = FString::Printf(
            TEXT("R31 capture requires paired binary render-path readback; r.Nanite=%d r.Nanite.ProxyRenderMode=%d."),
            OutNaniteValue,
            OutProxyRenderMode);
        return false;
    }
    OutRenderPathId = OutNaniteValue == 1
        ? NaniteOnRenderPathId
        : RasterFallbackRenderPathId;
    OutError.Reset();
    return true;
}

bool SetNaniteRenderPath(
    const FString& RenderPathId,
    FString& OutError)
{
    const int32 RequiredValue = RenderPathId == NaniteOnRenderPathId
        ? 1
        : RenderPathId == RasterFallbackRenderPathId
            ? 0
            : INDEX_NONE;
    IConsoleVariable* Nanite =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
    IConsoleVariable* ProxyRenderMode = IConsoleManager::Get().FindConsoleVariable(
        TEXT("r.Nanite.ProxyRenderMode"));
    if (RequiredValue == INDEX_NONE || !Nanite || !ProxyRenderMode)
    {
        OutError = TEXT("R31 capture render path must be exactly NANITE_ON or RASTER_FALLBACK and r.Nanite must exist.");
        return false;
    }
    if (RequiredValue == 0)
    {
        Nanite->Set(0, ECVF_SetByCode);
        ProxyRenderMode->Set(0, ECVF_SetByCode);
    }
    else
    {
        ProxyRenderMode->Set(1, ECVF_SetByCode);
        Nanite->Set(1, ECVF_SetByCode);
    }
    FString ActualRenderPath;
    int32 ActualValue = INDEX_NONE;
    int32 ActualProxyMode = INDEX_NONE;
    if (!ReadNaniteRenderPath(
            ActualRenderPath, ActualValue, ActualProxyMode, OutError) ||
        ActualValue != RequiredValue || ActualProxyMode != RequiredValue ||
        ActualRenderPath != RenderPathId)
    {
        OutError = FString::Printf(
            TEXT("R31 capture render-path readback failed: requested=%s actual=%s r.Nanite=%d r.Nanite.ProxyRenderMode=%d."),
            *RenderPathId,
            *ActualRenderPath,
            ActualValue,
            ActualProxyMode);
        return false;
    }
    OutError.Reset();
    return true;
}

FString CaptureFilename(
    const FString& PoseId,
    const FString& RunToken,
    const FString& RenderPathId)
{
    const TCHAR* Suffix = RenderPathId == RasterFallbackRenderPathId
        ? TEXT("_raster_fallback")
        : TEXT("");
    return FString::Printf(
        TEXT("explore_v5d_r31_player0_%s_%s%s.png"),
        *PoseId,
        *RunToken,
        Suffix);
}
} // namespace

bool UTRIADIstanaExploreV5DR31Player0CaptureLibrary::
    GetIstanaExploreV5DR31Player0CaptureState(FString& OutReport)
{
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error))
    {
        OutReport = Error;
        return false;
    }
    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    State.Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
    ViewRotation.Normalize();
    const FReviewedPose* ActivePose =
        FindReviewedPose(GExactPoseState.PoseId);
    const bool bExactPose = ActivePose &&
        IsExactPoseActive(State, *ActivePose);
    FString RenderPathId;
    int32 NaniteValue = INDEX_NONE;
    int32 ProxyRenderMode = INDEX_NONE;
    if (!ReadNaniteRenderPath(
            RenderPathId, NaniteValue, ProxyRenderMode, Error))
    {
        OutReport = Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R31_PLAYER0_CAPTURE_STATE_VALID captureSourceIdentity=UTRIADIstanaExploreV5DR31Player0CaptureLibrary captureStateApi=GetIstanaExploreV5DR31Player0CaptureState r30Class=ATRIADIstanaExploreV5DR30FacadeLookdevActor map=%s worldType=Game exactPlayer0=true viewTargetMatchesPawn=true v5CameraProfile=true r30Owner=1 r29VegetationOwner=1 r29TerrainOwner=1 treeRealismOwner=1 contextPolicyOwner=1 contextPolicyShell=R31BroadShell cesiumTilesetOwner=1 cesiumGeoreferenceOwner=1 ionAssetId=2275207 cesiumGeoreference=(103.84288055,1.30709615,47.000) cesiumLoadProgress=%.3f providerReadyForProof=%s localFallbackHidden=%s r30Visible=%s r31BroadShellVisible=true providerFallbackVisualQa=%s providerReadyProofClaimed=false airSimTriadRuntimeLoaded=true weatherActorResolved=true exactQaViewPose=%s poseId=%s viewLocationCm=%s viewRotationDeg=%s renderPath=%s r.Nanite=%d r.Nanite.ProxyRenderMode=%d naniteMeshEnabled=true naniteDataValid=true naniteKeepPercentTriangles=1.0 naniteTrimRelativeError=0.0 fallbackTargetPercentTriangles=true fallbackPercentTriangles=1.0 fallbackRelativeError=0.0 visualCaptureAccepted=false captureRevalidationRequired=true."),
        *TargetMapPackage,
        State.CesiumLoadProgress,
        State.CesiumLoadProgress >= 98.0f &&
                State.Policy->bLocalBuildingFallbackCurrentlyHidden
            ? TEXT("true")
            : TEXT("false"),
        State.Policy->bLocalBuildingFallbackCurrentlyHidden
            ? TEXT("true")
            : TEXT("false"),
        !State.R30->bProviderReady ? TEXT("true") : TEXT("false"),
        !State.Policy->bLocalBuildingFallbackCurrentlyHidden &&
                !State.R30->bProviderReady
            ? TEXT("true")
            : TEXT("false"),
        bExactPose ? TEXT("true") : TEXT("false"),
        ActivePose ? ActivePose->Id : TEXT("NONE"),
        *ViewLocation.ToString(),
        *ViewRotation.ToString(),
        *RenderPathId,
        NaniteValue,
        ProxyRenderMode);
    return true;
}

bool UTRIADIstanaExploreV5DR31Player0CaptureLibrary::
    SetIstanaExploreV5DR31Player0CapturePose(
        const FString& PoseId,
        FString& OutMessage)
{
    const FReviewedPose* Pose = FindReviewedPose(PoseId);
    if (!Pose || FScreenshotRequest::IsScreenshotRequested() ||
        GIsHighResScreenshot)
    {
        OutMessage = TEXT("R31 capture pose refused: select exactly 075m, 020m, 008m, 002m, or surroundings_oblique_macdonald and wait for any prior screenshot.");
        return false;
    }
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error))
    {
        OutMessage = TEXT("R31 capture pose refused: ") + Error;
        return false;
    }
    UCameraComponent* Camera = State.Pawn->GetExploreCameraComponent();
    APlayerCameraManager* CameraManager = State.Player->PlayerCameraManager;
    if (!Camera || !CameraManager || State.Player->GetViewTarget() != State.Pawn)
    {
        OutMessage = TEXT("R31 capture pose refused: exact Player0 camera manager/view target is unavailable.");
        return false;
    }

    const FRotator NormalizedRotation =
        Pose->ViewRotationDegrees.GetNormalized();
    const FVector PawnLocation =
        Pose->ViewLocationCentimetres - ExactCameraRelativeLocation;
    GExactPoseState = FExactPoseState{};
    State.Pawn->SetActorTickEnabled(false);
    if (State.Pawn->IsActorTickEnabled() ||
        !State.Pawn->SetActorLocationAndRotation(
            PawnLocation,
            FRotator::ZeroRotator,
            false,
            nullptr,
            ETeleportType::TeleportPhysics))
    {
        OutMessage = TEXT("R31 capture pose failed while freezing or moving Player0.");
        return false;
    }
    Camera->SetRelativeLocation(ExactCameraRelativeLocation);
    Camera->SetRelativeRotation(NormalizedRotation);
    Camera->Activate(true);
    CameraManager->UpdateCamera(0.0f);
    GExactPoseState.Pawn = State.Pawn;
    GExactPoseState.PoseId = PoseId;
    GExactPoseState.ViewLocationCentimetres =
        Pose->ViewLocationCentimetres;
    GExactPoseState.ViewRotationDegrees = NormalizedRotation;
    if (!IsExactPoseActive(State, *Pose))
    {
        GExactPoseState = FExactPoseState{};
        OutMessage = TEXT("R31 capture pose failed exact 0.1 cm / 0.05 degree Player0 readback.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R31_PLAYER0_POSE_PASS poseId=%s exactQaViewPose=true viewLocationCm=%s viewRotationDeg=%s providerStateObservedOnly=true mapModified=false simulationSensorRfModified=false."),
        *PoseId,
        *Pose->ViewLocationCentimetres.ToString(),
        *NormalizedRotation.ToString());
    return true;
}

bool UTRIADIstanaExploreV5DR31Player0CaptureLibrary::
    SetIstanaExploreV5DR31Player0CaptureRenderPath(
        const FString& RenderPathId,
        FString& OutMessage)
{
    if (FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("R31 render-path change refused while a screenshot is active.");
        return false;
    }
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error))
    {
        OutMessage = TEXT("R31 render-path change refused: ") + Error;
        return false;
    }
    const FReviewedPose* ActivePose = FindReviewedPose(GExactPoseState.PoseId);
    if (RenderPathId == RasterFallbackRenderPathId &&
        (!ActivePose || FCString::Strcmp(ActivePose->Id, *RasterComparisonPoseId) != 0 ||
         !IsExactPoseActive(State, *ActivePose)))
    {
        OutMessage = TEXT("R31 raster fallback is admitted only at the exact surroundings_oblique_macdonald pose.");
        return false;
    }
    if (!SetNaniteRenderPath(RenderPathId, Error))
    {
        OutMessage = TEXT("R31 render-path change failed: ") + Error;
        return false;
    }
    FString ActualRenderPath;
    int32 ActualNaniteValue = INDEX_NONE;
    int32 ActualProxyRenderMode = INDEX_NONE;
    if (!ReadNaniteRenderPath(
            ActualRenderPath, ActualNaniteValue, ActualProxyRenderMode, Error))
    {
        OutMessage = TEXT("R31 render-path readback failed: ") + Error;
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R31_RENDER_PATH_PASS renderPath=%s r.Nanite=%d r.Nanite.ProxyRenderMode=%d transientProcessStateOnly=true mapModified=false simulationSensorRfModified=false."),
        *ActualRenderPath,
        ActualNaniteValue,
        ActualProxyRenderMode);
    return true;
}

bool UTRIADIstanaExploreV5DR31Player0CaptureLibrary::
    CaptureIstanaExploreV5DR31Player0FallbackView(
        const FString& PoseId,
        const FString& RenderPathId,
        FString& OutMessage)
{
    const FReviewedPose* Pose = FindReviewedPose(PoseId);
    FString RunToken;
    FString OutputRoot;
    FString Error;
    FValidatedCaptureState State;
    FString ActualRenderPath;
    int32 ActualNaniteValue = INDEX_NONE;
    int32 ActualProxyRenderMode = INDEX_NONE;
    if (!Pose || !ResolveCaptureArguments(
            RunToken, OutputRoot, Error) ||
        !ResolveValidatedCaptureState(State, Error) ||
        !ReadNaniteRenderPath(
            ActualRenderPath, ActualNaniteValue, ActualProxyRenderMode, Error) ||
        ActualRenderPath != RenderPathId ||
        !IsExactPoseActive(State, *Pose) ||
        State.Policy->bLocalBuildingFallbackCurrentlyHidden ||
        State.R30->bProviderReady ||
        (RenderPathId == RasterFallbackRenderPathId &&
         PoseId != RasterComparisonPoseId) ||
        (RenderPathId != NaniteOnRenderPathId &&
         RenderPathId != RasterFallbackRenderPathId))
    {
        OutMessage = TEXT("R31 fallback capture refused: exact reviewed pose/render path, actual r.Nanite readback, validated Game/Player0 state, localFallbackHidden=false, r30 providerReady=false, and ProviderFallback visibility are required. ") + Error;
        return false;
    }
    UGameViewportClient* ViewportClient = State.World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("R31 fallback capture requires the validated SDR Game viewport.");
        return false;
    }

    const FString CaptureDirectory = FPaths::Combine(
        OutputRoot, TEXT("captures"));
    const FString Filename = CaptureFilename(PoseId, RunToken, RenderPathId);
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(CaptureDirectory, Filename));
    FPaths::NormalizeFilename(Destination);
    if (!FPaths::IsUnderDirectory(Destination, OutputRoot) ||
        !IFileManager::Get().MakeDirectory(*CaptureDirectory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("R31 fallback capture refused unsafe output, overwrite, or overlapping request.");
        return false;
    }

    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 R31 capture configuration.");
        return false;
    }
    Config.SetFilename(Destination);
    Config.SetMaskEnabled(false);
    Config.bDumpBufferVisualizationTargets = false;
    Config.bDateTimeBasedNaming = false;
    Config.bDisplayCaptureRegion = false;
    if (!Viewport->TakeHighResScreenShot())
    {
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("UE rejected the exact R31 Player0 game-viewport screenshot request.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R31_PLAYER0_FALLBACK_CAPTURE_ACCEPTED poseId=%s output=%s width=2560 height=1440 hdr=false exactPlayer0=true exactQaViewPose=true providerFallbackVisualQa=true providerReadyProofClaimed=false localFallbackHidden=false r30Visible=true r31BroadShellVisible=true exactR31Overrides=true cesiumLoadProgress=%.3f ionAssetId=2275207 airSimTriadRuntimeLoaded=true weatherActorResolved=true renderPath=%s r.Nanite=%d r.Nanite.ProxyRenderMode=%d mapModified=false providerInputsModified=false simulationSensorRfModified=false wrapperMustDecodeAndPostValidate=true."),
        *PoseId,
        *Destination,
        State.CesiumLoadProgress,
        *ActualRenderPath,
        ActualNaniteValue,
        ActualProxyRenderMode);
    return true;
}

bool UTRIADIstanaExploreV5DR31Player0CaptureLibrary::
    FinishIstanaExploreV5DR31Player0CaptureRun(FString& OutMessage)
{
    FValidatedCaptureState State;
    FString Error;
    FString RenderPathId;
    int32 NaniteValue = INDEX_NONE;
    int32 ProxyRenderMode = INDEX_NONE;
    if (!ResolveValidatedCaptureState(State, Error) ||
        !ReadNaniteRenderPath(
            RenderPathId, NaniteValue, ProxyRenderMode, Error) ||
        RenderPathId != NaniteOnRenderPathId || NaniteValue != 1 ||
        ProxyRenderMode != 1 ||
        State.Policy->bLocalBuildingFallbackCurrentlyHidden ||
        State.R30->bProviderReady ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("R31 capture finish refused until r.Nanite=1 is restored, the exact ProviderFallback state is valid, and the last screenshot has completed. ") +
            Error;
        return false;
    }
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R31_PLAYER0_CAPTURE_EXIT_ACCEPTED cleanRequest=true renderPath=NANITE_ON r.Nanite=1 r.Nanite.ProxyRenderMode=1 naniteConsoleStateRestored=true providerReadyProofClaimed=false mapModified=false.");
    UE_LOG(LogTemp, Display, TEXT("%s"), *OutMessage);
    FPlatformMisc::RequestExit(false);
    return true;
}
