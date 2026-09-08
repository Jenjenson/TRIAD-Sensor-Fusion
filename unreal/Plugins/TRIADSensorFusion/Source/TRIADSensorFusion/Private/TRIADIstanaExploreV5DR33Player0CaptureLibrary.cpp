#include "TRIADIstanaExploreV5DR33Player0CaptureLibrary.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "CesiumIonServer.h"
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
#include "UObject/SoftObjectPath.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevActor.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h"
#include "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"
#include "TRIADIstanaExploreV5GameMode.h"
#include "TRIADIstanaExploreV5Pawn.h"
#include "UnrealClient.h"

namespace
{
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString CaptureArgument(TEXT("TRIADR33CaptureRun="));
const FString OutputRootArgument(TEXT("TRIADR33CaptureOutputRoot="));
const FString CaptureBaseRoot(TEXT("D:/triad/TRIAD_R33Evidence"));
const FString AirSimRuntimeModule(TEXT("AirSimTriadRuntime"));
const FString WeatherActorClassPath(
    TEXT("/AirSimTriadRuntime/Weather/WeatherFX/WeatherActor.WeatherActor_C"));
const FVector ExactCameraRelativeLocation(0.0, 0.0, 64.0);
constexpr float LocationToleranceCentimetres = 0.1f;
constexpr float RotationToleranceDegrees = 0.05f;
constexpr int64 GooglePhotorealistic3DTilesIonAssetId = 2275207;
constexpr int64 CesiumWorldTerrainIonAssetId = 1;

struct FReviewedPose
{
    const TCHAR* Id;
    FVector ViewLocationCentimetres;
    FRotator ViewRotationDegrees;
    const TCHAR* ReviewPurpose;
};

// Each pose is captured unchanged in both streamed presentation modes.
// The 95 m grazing view exposes turf/terrain seams, holes, and double render;
// the MacDonald oblique exposes the wider surrounding-building relationship.
const FReviewedPose ReviewedPoses[] = {
    {TEXT("075m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-1.252968, -90.0, 0.0), TEXT("terrain_wide")},
    {TEXT("020m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-4.703535, -90.0, 0.0), TEXT("terrain_mid")},
    {TEXT("095m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-0.989007849, -90.0, 0.0), TEXT("grazing_hole_double_render")},
    {TEXT("surroundings_oblique_macdonald"),
     FVector(40226.9640238642, 106152.591966384, 3900.0),
     FRotator(-5.74137954693854, -101.620267003074, 0.0),
     TEXT("surroundings_oblique")},
};
static_assert(UE_ARRAY_COUNT(ReviewedPoses) == 4);

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
    ATRIADIstanaExploreV5DR32MediumDistanceTurfActor* R32 = nullptr;
    ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor* R33 = nullptr;
    ACesium3DTileset* GoogleTileset = nullptr;
    ACesium3DTileset* CwtTileset = nullptr;
    ACesiumGeoreference* Georeference = nullptr;
    FString R30Report;
    FString R31Report;
    FString PolicyReport;
    FString GroundReport;
    FString VegetationReport;
    FString TerrainReport;
    FString TreeReport;
    FString R32Report;
    FString R33Report;
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
        if (IsValid(Candidate) && Candidate->GetClass() == T::StaticClass())
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

const TCHAR* PresentationName(
    ETRIADIstanaExploreV5DR33TerrainPresentationState State)
{
    switch (State)
    {
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary:
        return TEXT("GooglePrimary");
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming:
        return TEXT("CwtWarming");
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented:
        return TEXT("CwtPresented");
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal:
        return TEXT("SafeLocal");
    default:
        return TEXT("Unknown");
    }
}

bool IsCapturablePresentation(const FString& PresentationId)
{
    return PresentationId.Equals(TEXT("GooglePrimary"), ESearchCase::CaseSensitive) ||
        PresentationId.Equals(TEXT("CwtPresented"), ESearchCase::CaseSensitive);
}

bool IsExpectedPresentation(
    const FValidatedCaptureState& State,
    const FString& PresentationId)
{
    if (!State.R33 || !IsCapturablePresentation(PresentationId))
    {
        return false;
    }
    const auto Current = State.R33->GetPresentationState();
    return (PresentationId == TEXT("GooglePrimary") &&
            Current == ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary) ||
        (PresentationId == TEXT("CwtPresented") &&
         Current == ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented);
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
    if (!FParse::Value(FCommandLine::Get(), *CaptureArgument, OutRunToken) ||
        !FParse::Value(FCommandLine::Get(), *OutputRootArgument, OutOutputRoot) ||
        !IsSafeRunToken(OutRunToken) || OutOutputRoot.IsEmpty() ||
        FPaths::IsRelative(OutOutputRoot))
    {
        OutError = TEXT("R33 capture requires -TRIADR33CaptureRun=<safe-token> and an absolute -TRIADR33CaptureOutputRoot=<bounded-root>.");
        return false;
    }
    OutOutputRoot = FPaths::ConvertRelativePathToFull(OutOutputRoot);
    FPaths::NormalizeDirectoryName(OutOutputRoot);
    FString NormalizedBase = FPaths::ConvertRelativePathToFull(CaptureBaseRoot);
    FPaths::NormalizeDirectoryName(NormalizedBase);
    FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ProjectRoot);
    if (!FPaths::IsUnderDirectory(OutOutputRoot, NormalizedBase) ||
        !FPaths::IsSamePath(FPaths::GetPath(OutOutputRoot), NormalizedBase) ||
        FPaths::GetCleanFilename(OutOutputRoot) != OutRunToken ||
        FPaths::IsUnderDirectory(OutOutputRoot, ProjectRoot) ||
        FPaths::IsSamePath(OutOutputRoot, ProjectRoot))
    {
        OutError = TEXT("R33 capture output must be the exact run-token child below D:/triad/TRIAD_R33Evidence and outside the project.");
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
                UWorld::RemovePIEPrefix(Candidate->GetOutermost()->GetName()) ==
                    TargetMapPackage)
            {
                Result = Candidate;
                ++MatchingWorldCount;
            }
        }
    }
    if (MatchingWorldCount != 1 || !Result)
    {
        OutError = FString::Printf(
            TEXT("R33 Player0 evidence requires one exact cooked Game world for %s; matches=%d."),
            *TargetMapPackage,
            MatchingWorldCount);
        return nullptr;
    }
    OutError.Reset();
    return Result;
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
    const UCameraComponent* Camera = State.Pawn->GetExploreCameraComponent();
    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    State.Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
    ViewRotation.Normalize();
    const FRotator Expected = Pose.ViewRotationDegrees.GetNormalized();
    return Camera && State.Player->GetViewTarget() == State.Pawn &&
        Camera->GetRelativeLocation().Equals(
            ExactCameraRelativeLocation, LocationToleranceCentimetres) &&
        Camera->GetRelativeRotation().Equals(Expected, RotationToleranceDegrees) &&
        State.Pawn->GetActorLocation().Equals(
            Pose.ViewLocationCentimetres - ExactCameraRelativeLocation,
            LocationToleranceCentimetres) &&
        State.Pawn->GetActorRotation().Equals(
            FRotator::ZeroRotator, RotationToleranceDegrees) &&
        ViewLocation.Equals(Pose.ViewLocationCentimetres, LocationToleranceCentimetres) &&
        ViewRotation.Equals(Expected, RotationToleranceDegrees) &&
        GExactPoseState.ViewLocationCentimetres.Equals(
            Pose.ViewLocationCentimetres, LocationToleranceCentimetres) &&
        GExactPoseState.ViewRotationDegrees.Equals(Expected, RotationToleranceDegrees);
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
    int32 R32Count = 0;
    int32 R33Count = 0;
    int32 GroundCount = 0;
    int32 VegetationCount = 0;
    int32 TerrainCount = 0;
    int32 TreeCount = 0;
    OutState.Policy = FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
        OutState.World, PolicyCount);
    OutState.R30 = FindExactlyOne<ATRIADIstanaExploreV5DR30FacadeLookdevActor>(
        OutState.World, R30Count);
    OutState.R32 = FindExactlyOne<ATRIADIstanaExploreV5DR32MediumDistanceTurfActor>(
        OutState.World, R32Count);
    OutState.R33 = FindExactlyOne<
        ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor>(
            OutState.World, R33Count);
    ATRIADIstanaExploreV5DGroundVegetationActor* Ground = FindExactlyOne<
        ATRIADIstanaExploreV5DGroundVegetationActor>(OutState.World, GroundCount);
    ATRIADIstanaExploreV5DR29VegetationActor* Vegetation = FindExactlyOne<
        ATRIADIstanaExploreV5DR29VegetationActor>(OutState.World, VegetationCount);
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor* Terrain =
        FindExactlyOne<ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor>(
            OutState.World, TerrainCount);
    ATRIADIstanaExploreV5DTreeRealismActor* Trees = FindExactlyOne<
        ATRIADIstanaExploreV5DTreeRealismActor>(OutState.World, TreeCount);

    OutState.Player = UGameplayStatics::GetPlayerController(OutState.World, 0);
    OutState.Pawn = OutState.Player
        ? Cast<ATRIADIstanaExploreV5Pawn>(OutState.Player->GetPawn())
        : nullptr;
    OutState.GoogleTileset = OutState.R33 ? OutState.R33->GoogleTileset : nullptr;
    OutState.CwtTileset = OutState.R33 ? OutState.R33->CwtTileset : nullptr;
    OutState.Georeference = OutState.R33 ? OutState.R33->SharedGeoreference : nullptr;

    UClass* WeatherActorClass = FSoftClassPath(WeatherActorClassPath).ResolveClass();
    if (!WeatherActorClass)
    {
        WeatherActorClass = FSoftClassPath(WeatherActorClassPath).TryLoadClass<AActor>();
    }
    const bool bAirSimRuntimeLoaded =
        FModuleManager::Get().IsModuleLoaded(FName(*AirSimRuntimeModule));

    if (PolicyCount != 1 || R30Count != 1 || R32Count != 1 || R33Count != 1 ||
        GroundCount != 1 || VegetationCount != 1 || TerrainCount != 1 ||
        TreeCount != 1 || !OutState.Policy || !OutState.R30 || !OutState.R32 ||
        !OutState.R33 || !Ground || !Vegetation || !Terrain || !Trees ||
        !OutState.GoogleTileset || !OutState.CwtTileset || !OutState.Georeference ||
        !OutState.Player || !OutState.Pawn ||
        !OutState.World->GetAuthGameMode() ||
        OutState.World->GetAuthGameMode()->GetClass() !=
            ATRIADIstanaExploreV5GameMode::StaticClass() ||
        !OutState.Pawn->HasExpectedExploreV5CameraProfile() ||
        OutState.Player->GetViewTarget() != OutState.Pawn ||
        OutState.R33->ContextPolicy != OutState.Policy ||
        OutState.GoogleTileset == OutState.CwtTileset ||
        OutState.GoogleTileset->GetIonAssetID() !=
            GooglePhotorealistic3DTilesIonAssetId ||
        OutState.CwtTileset->GetIonAssetID() != CesiumWorldTerrainIonAssetId ||
        !IsValid(OutState.GoogleTileset->GetCesiumIonServer()) ||
        !IsValid(OutState.CwtTileset->GetCesiumIonServer()) ||
        OutState.GoogleTileset->GetCesiumIonServer() !=
            OutState.CwtTileset->GetCesiumIonServer() ||
        OutState.GoogleTileset->GetGeoreference().Get() != OutState.Georeference ||
        OutState.CwtTileset->GetGeoreference().Get() != OutState.Georeference ||
        OutState.R33->bVisualAcceptance ||
        !OutState.R33->bVisualReferenceOnly ||
        OutState.R33->bHeightSamplesPersisted ||
        OutState.R33->bSurveyAccuracyClaimed ||
        OutState.R33->bVerticalDatumResolved ||
        OutState.R33->bCollisionNavigationLineOfSightSensorOrRfAuthority ||
        OutState.R32->SourceGroundVegetation != Ground ||
        OutState.R32->bVisualCaptureAccepted ||
        !bAirSimRuntimeLoaded || !WeatherActorClass ||
        !OutState.R30->ValidateR30FacadeLookdev(OutState.R30Report) ||
        !OutState.Policy->ValidateHybridContext(OutState.PolicyReport) ||
        !Ground->ValidateGroundVegetationRealism(OutState.GroundReport) ||
        !Vegetation->ValidateR29Vegetation(OutState.VegetationReport) ||
        !Terrain->ValidateCopernicusTerrainFallback(OutState.TerrainReport) ||
        !Trees->ValidateTreeRealism(OutState.TreeReport) ||
        !OutState.R32->ValidateR32MediumDistanceTurf(OutState.R32Report) ||
        !OutState.R33->ValidateR33CesiumWorldTerrainReference(OutState.R33Report))
    {
        OutError = FString::Printf(
            TEXT("R33_CAPTURE_STATE_INVALID map=%s policy=%d r30=%d r32=%d r33=%d ground=%d vegetation=%d terrain=%d trees=%d player0=%s googleAsset=%lld cwtAsset=%lld airSimTriadRuntimeLoaded=%s weatherActorResolved=%s r30={%s} policy={%s} ground={%s} vegetation={%s} terrain={%s} trees={%s} r32={%s} r33={%s}"),
            *TargetMapPackage,
            PolicyCount,
            R30Count,
            R32Count,
            R33Count,
            GroundCount,
            VegetationCount,
            TerrainCount,
            TreeCount,
            OutState.Player ? TEXT("true") : TEXT("false"),
            OutState.GoogleTileset ? OutState.GoogleTileset->GetIonAssetID() : -1,
            OutState.CwtTileset ? OutState.CwtTileset->GetIonAssetID() : -1,
            bAirSimRuntimeLoaded ? TEXT("true") : TEXT("false"),
            WeatherActorClass ? TEXT("true") : TEXT("false"),
            *OutState.R30Report,
            *OutState.PolicyReport,
            *OutState.GroundReport,
            *OutState.VegetationReport,
            *OutState.TerrainReport,
            *OutState.TreeReport,
            *OutState.R32Report,
            *OutState.R33Report);
        return false;
    }
    OutError.Reset();
    return true;
}

FString CaptureFilename(
    const FString& PresentationId,
    const FString& PoseId,
    const FString& RunToken)
{
    const TCHAR* StateSlug = PresentationId == TEXT("GooglePrimary")
        ? TEXT("google_primary")
        : TEXT("cwt_presented");
    return FString::Printf(
        TEXT("explore_v5d_r33_player0_%s_%s_%s.png"),
        StateSlug,
        *PoseId,
        *RunToken);
}
} // namespace

bool UTRIADIstanaExploreV5DR33Player0CaptureLibrary::
    GetIstanaExploreV5DR33Player0CaptureState(FString& OutReport)
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
    const FReviewedPose* ActivePose = FindReviewedPose(GExactPoseState.PoseId);
    const bool bExactPose = ActivePose && IsExactPoseActive(State, *ActivePose);
    const auto Presentation = State.R33->GetPresentationState();
    const bool bGoogleVisible = !State.GoogleTileset->IsHidden();
    const bool bCwtVisible = !State.CwtTileset->IsHidden();
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R33_PLAYER0_CAPTURE_STATE_VALID captureSourceIdentity=UTRIADIstanaExploreV5DR33Player0CaptureLibrary map=%s worldType=Game exactPlayer0=true viewTargetMatchesPawn=true v5CameraProfile=true r33ControllerOwner=1 presentation=%s googleIonAssetId=2275207 cwtIonAssetId=1 sharedGeoreference=true googleVisible=%s cwtVisible=%s doubleVisible=%s cwtLoadProgress=%.3f exactQaViewPose=%s poseId=%s airSimTriadRuntimeLoaded=true weatherActorResolved=true visualReferenceOnly=true providerReadyProofClaimed=false accurateRealWorldTerrainClaimed=false surveyAccuracyClaimed=false verticalDatumResolved=false heightSamplesPersisted=false visualCaptureAccepted=false mapModified=false simulationCollisionNavigationSensorRfModified=false captureRevalidationRequired=true viewLocationCm=%s viewRotationDeg=%s."),
        *TargetMapPackage,
        PresentationName(Presentation),
        bGoogleVisible ? TEXT("true") : TEXT("false"),
        bCwtVisible ? TEXT("true") : TEXT("false"),
        bGoogleVisible && bCwtVisible ? TEXT("true") : TEXT("false"),
        State.CwtTileset->GetLoadProgress(),
        bExactPose ? TEXT("true") : TEXT("false"),
        ActivePose ? ActivePose->Id : TEXT("NONE"),
        *ViewLocation.ToString(),
        *ViewRotation.ToString());
    return true;
}

bool UTRIADIstanaExploreV5DR33Player0CaptureLibrary::
    SetIstanaExploreV5DR33Player0Presentation(
        const FString& PresentationId,
        FString& OutMessage)
{
    if (!IsCapturablePresentation(PresentationId) ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("R33 presentation request must be exactly GooglePrimary or CwtPresented and no screenshot may be active.");
        return false;
    }
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error))
    {
        OutMessage = TEXT("R33 presentation request refused: ") + Error;
        return false;
    }
    const bool bRequestCwt = PresentationId == TEXT("CwtPresented");
    if (!State.R33->RequestCwtPresentation(bRequestCwt, Error))
    {
        OutMessage = TEXT("R33 presentation request failed: ") + Error;
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R33_PLAYER0_PRESENTATION_REQUEST_ACCEPTED requested=%s current=%s asynchronousReadiness=%s directSafeLocalTriggerUsed=false networkSabotageUsed=false mapModified=false simulationCollisionNavigationSensorRfModified=false."),
        *PresentationId,
        PresentationName(State.R33->GetPresentationState()),
        bRequestCwt ? TEXT("true") : TEXT("false"));
    return true;
}

bool UTRIADIstanaExploreV5DR33Player0CaptureLibrary::
    SetIstanaExploreV5DR33Player0CapturePose(
        const FString& PoseId,
        FString& OutMessage)
{
    const FReviewedPose* Pose = FindReviewedPose(PoseId);
    if (!Pose || FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("R33 capture pose must be exactly 075m, 020m, 095m, or surroundings_oblique_macdonald and no screenshot may be active.");
        return false;
    }
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error))
    {
        OutMessage = TEXT("R33 capture pose refused: ") + Error;
        return false;
    }
    const auto Presentation = State.R33->GetPresentationState();
    if (Presentation != ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary &&
        Presentation != ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented)
    {
        OutMessage = TEXT("R33 capture pose requires stable GooglePrimary or CwtPresented, never warming or SafeLocal.");
        return false;
    }
    UCameraComponent* Camera = State.Pawn->GetExploreCameraComponent();
    APlayerCameraManager* CameraManager = State.Player->PlayerCameraManager;
    if (!Camera || !CameraManager || State.Player->GetViewTarget() != State.Pawn)
    {
        OutMessage = TEXT("R33 capture pose refused: exact Player0 camera manager/view target is unavailable.");
        return false;
    }
    const FRotator Rotation = Pose->ViewRotationDegrees.GetNormalized();
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
        OutMessage = TEXT("R33 capture pose failed while freezing or moving Player0.");
        return false;
    }
    Camera->SetRelativeLocation(ExactCameraRelativeLocation);
    Camera->SetRelativeRotation(Rotation);
    Camera->Activate(true);
    CameraManager->UpdateCamera(0.0f);
    GExactPoseState.Pawn = State.Pawn;
    GExactPoseState.PoseId = PoseId;
    GExactPoseState.ViewLocationCentimetres = Pose->ViewLocationCentimetres;
    GExactPoseState.ViewRotationDegrees = Rotation;
    if (!IsExactPoseActive(State, *Pose))
    {
        GExactPoseState = FExactPoseState{};
        OutMessage = TEXT("R33 capture pose failed exact 0.1 cm / 0.05 degree Player0 readback.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R33_PLAYER0_POSE_PASS poseId=%s reviewPurpose=%s exactQaViewPose=true viewLocationCm=%s viewRotationDeg=%s mapModified=false simulationCollisionNavigationSensorRfModified=false."),
        *PoseId,
        Pose->ReviewPurpose,
        *Pose->ViewLocationCentimetres.ToString(),
        *Rotation.ToString());
    return true;
}

bool UTRIADIstanaExploreV5DR33Player0CaptureLibrary::
    CaptureIstanaExploreV5DR33Player0ComparisonView(
        const FString& PresentationId,
        const FString& PoseId,
        FString& OutMessage)
{
    const FReviewedPose* Pose = FindReviewedPose(PoseId);
    FString RunToken;
    FString OutputRoot;
    FString Error;
    FValidatedCaptureState State;
    if (!Pose || !ResolveCaptureArguments(RunToken, OutputRoot, Error) ||
        !ResolveValidatedCaptureState(State, Error) ||
        !IsExpectedPresentation(State, PresentationId) ||
        !IsExactPoseActive(State, *Pose))
    {
        OutMessage = TEXT("R33 comparison capture refused: exact state, pose, and validated R33 Game/Player0 boundary are required. ") + Error;
        return false;
    }
    UGameViewportClient* ViewportClient = State.World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("R33 comparison capture requires the validated SDR Game viewport.");
        return false;
    }
    const FString CaptureDirectory = FPaths::Combine(OutputRoot, TEXT("captures"));
    const FString Filename = CaptureFilename(PresentationId, PoseId, RunToken);
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(CaptureDirectory, Filename));
    FPaths::NormalizeFilename(Destination);
    if (!FPaths::IsUnderDirectory(Destination, OutputRoot) ||
        !IFileManager::Get().MakeDirectory(*CaptureDirectory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("R33 comparison capture refused unsafe output, overwrite, or overlapping request.");
        return false;
    }
    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 R33 capture configuration.");
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
        OutMessage = TEXT("UE rejected the exact R33 Player0 game-viewport screenshot request.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R33_PLAYER0_COMPARISON_CAPTURE_ACCEPTED presentation=%s poseId=%s reviewPurpose=%s output=%s width=2560 height=1440 hdr=false exactPlayer0=true exactQaViewPose=true googleIonAssetId=2275207 cwtIonAssetId=1 doubleVisible=false visualReferenceOnly=true providerReadyProofClaimed=false accurateRealWorldTerrainClaimed=false surveyAccuracyClaimed=false verticalDatumResolved=false heightSamplesPersisted=false airSimTriadRuntimeLoaded=true mapModified=false simulationCollisionNavigationSensorRfModified=false wrapperMustDecodeAndPostValidate=true."),
        *PresentationId,
        *PoseId,
        Pose->ReviewPurpose,
        *Destination);
    return true;
}

bool UTRIADIstanaExploreV5DR33Player0CaptureLibrary::
    FinishIstanaExploreV5DR33Player0CaptureRun(FString& OutMessage)
{
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error) ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot ||
        !State.R33->RequestCwtPresentation(false, Error) ||
        State.R33->GetPresentationState() !=
            ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary ||
        !State.R33->ValidateR33CesiumWorldTerrainReference(State.R33Report))
    {
        OutMessage = TEXT("R33 capture finish refused until the exact state is valid, the last screenshot is complete, and GooglePrimary is restored. ") +
            Error + TEXT(" ") + State.R33Report;
        return false;
    }
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R33_PLAYER0_CAPTURE_EXIT_ACCEPTED restoredPresentation=GooglePrimary cleanRequest=true safeLocalDirectTriggerUsed=false networkSabotageUsed=false mapModified=false simulationCollisionNavigationSensorRfModified=false.");
    UE_LOG(LogTemp, Display, TEXT("%s"), *OutMessage);
    FPlatformMisc::RequestExit(false);
    return true;
}
