#include "TRIADIstanaExploreV5DR30Player0CaptureLibrary.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"
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
const FString CaptureArgument(TEXT("TRIADR30CaptureRun="));
const FString OutputRootArgument(TEXT("TRIADR30CaptureOutputRoot="));
const FString CaptureBaseRoot(TEXT("D:/triad/TRIAD_R30Evidence"));
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
        OutError = TEXT("R30 capture endpoints require -TRIADR30CaptureRun=<safe-token> and one absolute -TRIADR30CaptureOutputRoot=<bounded-root>.");
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
        OutError = TEXT("R30 capture output must be the exact run-token child below D:/triad/TRIAD_R30Evidence and outside the project sandbox.");
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
            TEXT("R30 Player0 evidence requires one exact cooked Game world for %s; matches=%d."),
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

    if (PolicyCount != 1 || R30Count != 1 || VegetationCount != 1 ||
        TerrainCount != 1 || TreeCount != 1 || SceneCount != 1 ||
        TilesetCount != 1 || GeoreferenceCount != 1 ||
        !OutState.Policy || !OutState.R30 || !Vegetation || !Terrain ||
        !Trees || !Scene || !OutState.Tileset || !OutState.Georeference ||
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
            ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
                Scene, OutState.ShellReport) ||
        !Vegetation->ValidateR29Vegetation(OutState.VegetationReport) ||
        !Terrain->ValidateCopernicusTerrainFallback(OutState.TerrainReport) ||
        !Trees->ValidateTreeRealism(OutState.TreeReport))
    {
        OutError = FString::Printf(
            TEXT("R30_CAPTURE_STATE_INVALID map=%s policy=%d r30=%d vegetationR29=%d terrainR29=%d trees=%d scene=%d tileset=%d georeference=%d player0=%s pawnV5=%s airSimTriadRuntimeLoaded=%s weatherActorResolved=%s cesiumLoadProgress=%.3f stablePolicy=%s r30={%s} policy={%s} shell={%s} vegetation={%s} terrain={%s} trees={%s}"),
            *TargetMapPackage,
            PolicyCount,
            R30Count,
            VegetationCount,
            TerrainCount,
            TreeCount,
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

FString CaptureFilename(const FString& PoseId, const FString& RunToken)
{
    return FString::Printf(
        TEXT("explore_v5d_r30_player0_%s_%s.png"),
        *PoseId,
        *RunToken);
}
} // namespace

bool UTRIADIstanaExploreV5DR30Player0CaptureLibrary::
    GetIstanaExploreV5DR30Player0CaptureState(FString& OutReport)
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
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_STATE_VALID captureSourceIdentity=UTRIADIstanaExploreV5DR30Player0CaptureLibrary captureStateApi=GetIstanaExploreV5DR30Player0CaptureState r30Class=ATRIADIstanaExploreV5DR30FacadeLookdevActor map=%s worldType=Game exactPlayer0=true viewTargetMatchesPawn=true v5CameraProfile=true r30Owner=1 r29VegetationOwner=1 r29TerrainOwner=1 treeRealismOwner=1 treeMaterialResponseV3=true treeResponseMaterials=13 treeRuntimeResponseMids=26 treePlacementGeometryOpacityWindAuthorityModified=false contextPolicyOwner=1 contextPolicyShell=R25Inherited cesiumTilesetOwner=1 cesiumGeoreferenceOwner=1 ionAssetId=2275207 cesiumGeoreference=(103.84288055,1.30709615,47.000) cesiumLoadProgress=%.3f providerReadyForProof=%s localFallbackHidden=%s r30Visible=%s providerFallbackVisualQa=%s providerReadyProofClaimed=false airSimTriadRuntimeLoaded=true weatherActorResolved=true exactQaViewPose=%s poseId=%s viewLocationCm=%s viewRotationDeg=%s visualCaptureAccepted=false captureRevalidationRequired=true."),
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
        *ViewRotation.ToString());
    return true;
}

bool UTRIADIstanaExploreV5DR30Player0CaptureLibrary::
    SetIstanaExploreV5DR30Player0CapturePose(
        const FString& PoseId,
        FString& OutMessage)
{
    const FReviewedPose* Pose = FindReviewedPose(PoseId);
    if (!Pose || FScreenshotRequest::IsScreenshotRequested() ||
        GIsHighResScreenshot)
    {
        OutMessage = TEXT("R30 capture pose refused: select exactly 075m, 020m, 008m, 002m, or surroundings_oblique_macdonald and wait for any prior screenshot.");
        return false;
    }
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error))
    {
        OutMessage = TEXT("R30 capture pose refused: ") + Error;
        return false;
    }
    UCameraComponent* Camera = State.Pawn->GetExploreCameraComponent();
    APlayerCameraManager* CameraManager = State.Player->PlayerCameraManager;
    if (!Camera || !CameraManager || State.Player->GetViewTarget() != State.Pawn)
    {
        OutMessage = TEXT("R30 capture pose refused: exact Player0 camera manager/view target is unavailable.");
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
        OutMessage = TEXT("R30 capture pose failed while freezing or moving Player0.");
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
        OutMessage = TEXT("R30 capture pose failed exact 0.1 cm / 0.05 degree Player0 readback.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R30_PLAYER0_POSE_PASS poseId=%s exactQaViewPose=true viewLocationCm=%s viewRotationDeg=%s providerStateObservedOnly=true mapModified=false simulationSensorRfModified=false."),
        *PoseId,
        *Pose->ViewLocationCentimetres.ToString(),
        *NormalizedRotation.ToString());
    return true;
}

bool UTRIADIstanaExploreV5DR30Player0CaptureLibrary::
    CaptureIstanaExploreV5DR30Player0FallbackView(
        const FString& PoseId,
        FString& OutMessage)
{
    const FReviewedPose* Pose = FindReviewedPose(PoseId);
    FString RunToken;
    FString OutputRoot;
    FString Error;
    FValidatedCaptureState State;
    if (!Pose || !ResolveCaptureArguments(
            RunToken, OutputRoot, Error) ||
        !ResolveValidatedCaptureState(State, Error) ||
        !IsExactPoseActive(State, *Pose) ||
        State.Policy->bLocalBuildingFallbackCurrentlyHidden ||
        State.R30->bProviderReady)
    {
        OutMessage = TEXT("R30 fallback capture refused: exact reviewed pose, validated Game/Player0 state, localFallbackHidden=false, r30 providerReady=false, and ProviderFallback visibility are required. ") + Error;
        return false;
    }
    UGameViewportClient* ViewportClient = State.World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("R30 fallback capture requires the validated SDR Game viewport.");
        return false;
    }

    const FString CaptureDirectory = FPaths::Combine(
        OutputRoot, TEXT("captures"));
    const FString Filename = CaptureFilename(PoseId, RunToken);
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(CaptureDirectory, Filename));
    FPaths::NormalizeFilename(Destination);
    if (!FPaths::IsUnderDirectory(Destination, OutputRoot) ||
        !IFileManager::Get().MakeDirectory(*CaptureDirectory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("R30 fallback capture refused unsafe output, overwrite, or overlapping request.");
        return false;
    }

    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 R30 capture configuration.");
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
        OutMessage = TEXT("UE rejected the exact R30 Player0 game-viewport screenshot request.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R30_PLAYER0_FALLBACK_CAPTURE_ACCEPTED poseId=%s output=%s width=2560 height=1440 hdr=false exactPlayer0=true exactQaViewPose=true providerFallbackVisualQa=true providerReadyProofClaimed=false localFallbackHidden=false r30Visible=true cesiumLoadProgress=%.3f ionAssetId=2275207 airSimTriadRuntimeLoaded=true weatherActorResolved=true mapModified=false providerInputsModified=false simulationSensorRfModified=false wrapperMustDecodeAndPostValidate=true."),
        *PoseId,
        *Destination,
        State.CesiumLoadProgress);
    return true;
}

bool UTRIADIstanaExploreV5DR30Player0CaptureLibrary::
    FinishIstanaExploreV5DR30Player0CaptureRun(FString& OutMessage)
{
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error) ||
        State.Policy->bLocalBuildingFallbackCurrentlyHidden ||
        State.R30->bProviderReady ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("R30 capture finish refused until the exact ProviderFallback state is valid and the last screenshot has completed. ") +
            Error;
        return false;
    }
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R30_PLAYER0_CAPTURE_EXIT_ACCEPTED cleanRequest=true providerReadyProofClaimed=false mapModified=false.");
    UE_LOG(LogTemp, Display, TEXT("%s"), *OutMessage);
    FPlatformMisc::RequestExit(false);
    return true;
}
