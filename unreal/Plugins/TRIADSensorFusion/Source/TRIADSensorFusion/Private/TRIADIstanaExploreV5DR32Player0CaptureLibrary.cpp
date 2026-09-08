#include "TRIADIstanaExploreV5DR32Player0CaptureLibrary.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "Components/StaticMeshComponent.h"
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
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevActor.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h"
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
const FString CaptureArgument(TEXT("TRIADR32CaptureRun="));
const FString OutputRootArgument(TEXT("TRIADR32CaptureOutputRoot="));
const FString CaptureBaseRoot(TEXT("D:/triad/TRIAD_R32Evidence"));
const FString AirSimRuntimeModule(TEXT("AirSimTriadRuntime"));
const FString WeatherActorClassPath(
    TEXT("/AirSimTriadRuntime/Weather/WeatherFX/WeatherActor.WeatherActor_C"));
const FName ContextPolicyTag(TEXT("TRIADIstanaExploreV5DContextPolicy"));
const FName GroundVegetationTag(
    TEXT("TRIADIstanaExploreV5DGroundVegetation"));
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
    bool bTurfIntersectionDistanceProbe;
    double IntersectionDistanceMetres;
};

// The inherited five poses preserve the previously reviewed scene comparison.
// The five added poses retain the same X/Y/Z rig and use
// atan(1.64 m / distance), so the view ray meets the Z=0 lawn plane at exact
// 12 and 50 metre readability points and at the 65, 90, and 95 metre fade/
// cull points.
const FReviewedPose ReviewedPoses[] = {
    {TEXT("075m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-1.252968, -90.0, 0.0), false, 0.0},
    {TEXT("020m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-4.703535, -90.0, 0.0), false, 0.0},
    {TEXT("008m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-11.829499, -90.0, 0.0), false, 0.0},
    {TEXT("002m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-55.084794, -90.0, 0.0), false, 0.0},
    {TEXT("surroundings_oblique_macdonald"),
     FVector(40226.9640238642, 106152.591966384, 3900.0),
     FRotator(-5.74137954693854, -101.620267003074, 0.0), false, 0.0},
    {TEXT("012m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-7.782210724, -90.0, 0.0), true, 12.0},
    {TEXT("050m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-1.878628060, -90.0, 0.0), true, 50.0},
    {TEXT("065m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-1.445309952, -90.0, 0.0), true, 65.0},
    {TEXT("090m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-1.043940890, -90.0, 0.0), true, 90.0},
    {TEXT("095m"), FVector(3500.0, 15000.0, 164.0),
     FRotator(-0.989007849, -90.0, 0.0), true, 95.0},
};
static_assert(UE_ARRAY_COUNT(ReviewedPoses) == 10);

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
    ACesium3DTileset* Tileset = nullptr;
    ACesiumGeoreference* Georeference = nullptr;
    float CesiumLoadProgress = -1.0f;
    FString R30Report;
    FString R31ShellReport;
    FString PolicyReport;
    FString GroundReport;
    FString VegetationReport;
    FString TerrainReport;
    FString TreeReport;
    FString R32Report;
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
        !FParse::Value(
            FCommandLine::Get(), *OutputRootArgument, OutOutputRoot) ||
        !IsSafeRunToken(OutRunToken) || OutOutputRoot.IsEmpty() ||
        FPaths::IsRelative(OutOutputRoot))
    {
        OutError = TEXT("R32 capture requires -TRIADR32CaptureRun=<safe-token> and an absolute -TRIADR32CaptureOutputRoot=<bounded-root>.");
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
        OutError = TEXT("R32 capture output must be the exact run-token child below D:/triad/TRIAD_R32Evidence and outside the project.");
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
            TEXT("R32 Player0 evidence requires one exact cooked Game world for %s; matches=%d."),
            *TargetMapPackage,
            MatchingWorldCount);
        return nullptr;
    }
    OutError.Reset();
    return Result;
}

bool HasStableProviderFallback(
    const ATRIADIstanaExploreV5DContextPolicyActor* Policy,
    const ATRIADIstanaExploreV5DR30FacadeLookdevActor* R30)
{
    return Policy && R30 && Policy->bProviderSiteClipCurrentlyActive &&
        Policy->bAuthoredCoreVisualsCurrentlyVisible &&
        !Policy->bAerialProviderHandoffRequested &&
        !Policy->bAerialProviderHandoffCurrentlyActive &&
        Policy->AerialProviderReadySamples == 0 &&
        !Policy->bLocalBuildingFallbackCurrentlyHidden &&
        !R30->bProviderReady;
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
    int32 R32Count = 0;
    int32 GroundCount = 0;
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
    OutState.R32 = FindExactlyOne<
        ATRIADIstanaExploreV5DR32MediumDistanceTurfActor>(
            OutState.World, R32Count);
    ATRIADIstanaExploreV5DGroundVegetationActor* Ground = FindExactlyOne<
        ATRIADIstanaExploreV5DGroundVegetationActor>(
            OutState.World, GroundCount);
    ATRIADIstanaExploreV5DR29VegetationActor* Vegetation = FindExactlyOne<
        ATRIADIstanaExploreV5DR29VegetationActor>(
            OutState.World, VegetationCount);
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor* Terrain =
        FindExactlyOne<ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor>(
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

    int32 PolicyTagCount = 0;
    int32 R30TagCount = 0;
    int32 R32TagCount = 0;
    int32 GroundTagCount = 0;
    int32 VegetationTagCount = 0;
    int32 TerrainTagCount = 0;
    int32 TreeTagCount = 0;
    int32 R28FacadeCount = 0;
    int32 R28FacadeTagCount = 0;
    int32 R29FacadeCount = 0;
    int32 R29FacadeTagCount = 0;
    int32 LandmarkVegetationCount = 0;
    int32 LandmarkVegetationTagCount = 0;
    int32 ExactV2ComponentCount = 0;
    UStaticMeshComponent* ExactV2Component = nullptr;
    for (TActorIterator<AActor> It(OutState.World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        const bool bPolicy = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DContextPolicyActor::StaticClass();
        const bool bR30 = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::StaticClass();
        const bool bR32 = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::StaticClass();
        const bool bGround = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass();
        const bool bVegetation = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29VegetationActor::StaticClass();
        const bool bTerrain = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                StaticClass();
        const bool bTrees = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DTreeRealismActor::StaticClass();
        const bool bR28Facade = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass();
        const bool bR29Facade = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::StaticClass();
        const bool bLandmarkVegetation = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass();
        const bool bPolicyTag = Candidate->Tags.Contains(ContextPolicyTag);
        const bool bR30Tag = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::ExpectedActorTag());
        const bool bR32Tag = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
                ExpectedActorTag());
        const bool bGroundTag = Candidate->Tags.Contains(GroundVegetationTag);
        const bool bVegetationTag = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29VegetationActor::ExpectedActorTag());
        const bool bTerrainTag = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                ExpectedActorTag());
        const bool bTreeTag = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag());
        const bool bR28FacadeTag = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag());
        const bool bR29FacadeTag = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
                ExpectedActorTag());
        const bool bLandmarkVegetationTag = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DLandmarkVegetationActor::
                ExpectedActorTag());
        PolicyTagCount += bPolicyTag ? 1 : 0;
        R30TagCount += bR30Tag ? 1 : 0;
        R32TagCount += bR32Tag ? 1 : 0;
        GroundTagCount += bGroundTag ? 1 : 0;
        VegetationTagCount += bVegetationTag ? 1 : 0;
        TerrainTagCount += bTerrainTag ? 1 : 0;
        TreeTagCount += bTreeTag ? 1 : 0;
        R28FacadeCount += bR28Facade ? 1 : 0;
        R28FacadeTagCount += bR28FacadeTag ? 1 : 0;
        R29FacadeCount += bR29Facade ? 1 : 0;
        R29FacadeTagCount += bR29FacadeTag ? 1 : 0;
        LandmarkVegetationCount += bLandmarkVegetation ? 1 : 0;
        LandmarkVegetationTagCount += bLandmarkVegetationTag ? 1 : 0;
        if ((bPolicyTag && !bPolicy) || (bR30Tag && !bR30) ||
            (bR32Tag && !bR32) || (bGroundTag && !bGround) ||
            (bVegetationTag && !bVegetation) ||
            (bTerrainTag && !bTerrain) || (bTreeTag && !bTrees) ||
            (bR28FacadeTag && !bR28Facade) ||
            (bR29FacadeTag && !bR29Facade) ||
            (bLandmarkVegetationTag && !bLandmarkVegetation))
        {
            OutError = TEXT("R32 capture refused a protected owner tag on a non-exact class.");
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

    if (PolicyCount != 1 || PolicyTagCount != 1 ||
        R30Count != 1 || R30TagCount != 1 ||
        R32Count != 1 || R32TagCount != 1 ||
        GroundCount != 1 || GroundTagCount != 1 ||
        VegetationCount != 1 || VegetationTagCount != 1 ||
        TerrainCount != 1 || TerrainTagCount != 1 ||
        TreeCount != 1 || TreeTagCount != 1 || SceneCount != 1 ||
        TilesetCount != 1 || GeoreferenceCount != 1 ||
        R28FacadeCount != 0 || R28FacadeTagCount != 0 ||
        R29FacadeCount != 0 || R29FacadeTagCount != 0 ||
        LandmarkVegetationCount != 0 ||
        LandmarkVegetationTagCount != 0 || ExactV2ComponentCount != 1 ||
        !OutState.Policy || !OutState.R30 || !OutState.R32 || !Ground ||
        !Vegetation || !Terrain || !Trees || !Scene || !OutState.Tileset ||
        !OutState.Georeference || !OutState.Player || !OutState.Pawn ||
        ExactV2Component !=
            OutState.Policy->CurrentSurroundingsRenderOnlyComponent ||
        !OutState.World->GetAuthGameMode() ||
        OutState.World->GetAuthGameMode()->GetClass() !=
            ATRIADIstanaExploreV5GameMode::StaticClass() ||
        !OutState.Pawn->HasExpectedExploreV5CameraProfile() ||
        OutState.Player->GetViewTarget() != OutState.Pawn ||
        !OutState.Policy->HasActorBegunPlay() ||
        !OutState.Policy->IsActorTickEnabled() ||
        !OutState.R30->HasActorBegunPlay() ||
        !OutState.R30->IsActorTickEnabled() ||
        !OutState.R32->HasActorBegunPlay() ||
        !Ground->HasActorBegunPlay() || !Vegetation->HasActorBegunPlay() ||
        !Terrain->HasActorBegunPlay() || !Trees->HasActorBegunPlay() ||
        OutState.R32->SourceGroundVegetation != Ground ||
        OutState.R32->bVisualCaptureAccepted ||
        OutState.R32->GrassComponents.Num() !=
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
                ExpectedBucketCount() ||
        OutState.R32->SavedLayout.Total() !=
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
                ExpectedInstanceCount() ||
        !OutState.Tileset->Tags.Contains(VisualTilesetTag) ||
        (!OutState.Georeference->Tags.Contains(GeoreferenceTag) &&
         !OutState.Georeference->Tags.Contains(DefaultGeoreferenceTag)) ||
        OutState.Tileset->GetTilesetSource() !=
            ETilesetSource::FromCesiumIon ||
        OutState.Tileset->GetIonAssetID() !=
            GooglePhotorealistic3DTilesIonAssetId ||
        !FMath::IsNearlyEqual(
            OutState.Tileset->GetMaximumScreenSpaceError(), 1.0, 0.000001) ||
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
        OutState.CesiumLoadProgress > 100.0f ||
        !HasStableProviderFallback(OutState.Policy, OutState.R30) ||
        !bAirSimRuntimeLoaded || !WeatherActorClass ||
        !OutState.R30->ValidateR30FacadeLookdev(OutState.R30Report) ||
        !OutState.Policy->ValidateHybridContext(OutState.PolicyReport) ||
        !OutState.Policy->
            ValidateCurrentSurroundingsBroadShellR31ForInheritedScene(
                Scene, OutState.R31ShellReport) ||
        !Ground->ValidateGroundVegetationRealism(OutState.GroundReport) ||
        !Vegetation->ValidateR29Vegetation(OutState.VegetationReport) ||
        !Terrain->ValidateCopernicusTerrainFallback(OutState.TerrainReport) ||
        !Trees->ValidateTreeRealism(OutState.TreeReport) ||
        !OutState.R32->ValidateR32MediumDistanceTurf(OutState.R32Report))
    {
        OutError = FString::Printf(
            TEXT("R32_CAPTURE_STATE_INVALID map=%s policy=%d/%d r30=%d/%d r32=%d/%d ground=%d/%d vegetationR29=%d/%d terrainR29=%d/%d trees=%d/%d r28Facade=%d/%d r29Facade=%d/%d landmarkVegetation=%d/%d exactV2Components=%d scene=%d tileset=%d georeference=%d player0=%s airSimTriadRuntimeLoaded=%s weatherActorResolved=%s cesiumLoadProgress=%.3f r30={%s} r31Shell={%s} policy={%s} ground={%s} vegetation={%s} terrain={%s} trees={%s} r32={%s}"),
            *TargetMapPackage,
            PolicyCount, PolicyTagCount,
            R30Count, R30TagCount,
            R32Count, R32TagCount,
            GroundCount, GroundTagCount,
            VegetationCount, VegetationTagCount,
            TerrainCount, TerrainTagCount,
            TreeCount, TreeTagCount,
            R28FacadeCount, R28FacadeTagCount,
            R29FacadeCount, R29FacadeTagCount,
            LandmarkVegetationCount, LandmarkVegetationTagCount,
            ExactV2ComponentCount,
            SceneCount, TilesetCount, GeoreferenceCount,
            OutState.Player ? TEXT("true") : TEXT("false"),
            bAirSimRuntimeLoaded ? TEXT("true") : TEXT("false"),
            WeatherActorClass ? TEXT("true") : TEXT("false"),
            OutState.CesiumLoadProgress,
            *OutState.R30Report,
            *OutState.R31ShellReport,
            *OutState.PolicyReport,
            *OutState.GroundReport,
            *OutState.VegetationReport,
            *OutState.TerrainReport,
            *OutState.TreeReport,
            *OutState.R32Report);
        return false;
    }
    OutError.Reset();
    return true;
}

FString CaptureFilename(const FString& PoseId, const FString& RunToken)
{
    return FString::Printf(
        TEXT("explore_v5d_r32_player0_%s_%s.png"),
        *PoseId,
        *RunToken);
}
} // namespace

bool UTRIADIstanaExploreV5DR32Player0CaptureLibrary::
    GetIstanaExploreV5DR32Player0CaptureState(FString& OutReport)
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
    const bool bExactPose = ActivePose &&
        IsExactPoseActive(State, *ActivePose);
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R32_PLAYER0_CAPTURE_STATE_VALID captureSourceIdentity=UTRIADIstanaExploreV5DR32Player0CaptureLibrary map=%s worldType=Game exactPlayer0=true viewTargetMatchesPawn=true v5CameraProfile=true r32MediumDistanceTurfOwner=1 r32SelectedTransforms=4608 r32OwnedHismCount=12 r32CullMeters=65-90 groundVegetationOwner=1 r31BroadShellVisible=true r30Owner=1 r29VegetationOwner=1 r29TerrainOwner=1 treeRealismOwner=1 contextPolicyOwner=1 cesiumTilesetOwner=1 cesiumGeoreferenceOwner=1 ionAssetId=2275207 cesiumLoadProgress=%.3f providerFallbackVisualQa=true providerReadyProofClaimed=false airSimTriadRuntimeLoaded=true weatherActorResolved=true exactQaViewPose=%s poseId=%s turfIntersectionDistanceProbe=%s turfIntersectionDistanceMeters=%.3f viewLocationCm=%s viewRotationDeg=%s visualCaptureAccepted=false performanceAccepted=false surveyClaim=false botanicalClaim=false currentConditionClaim=false captureRevalidationRequired=true."),
        *TargetMapPackage,
        State.CesiumLoadProgress,
        bExactPose ? TEXT("true") : TEXT("false"),
        ActivePose ? ActivePose->Id : TEXT("NONE"),
        ActivePose && ActivePose->bTurfIntersectionDistanceProbe
            ? TEXT("true") : TEXT("false"),
        ActivePose ? ActivePose->IntersectionDistanceMetres : 0.0,
        *ViewLocation.ToString(),
        *ViewRotation.ToString());
    return true;
}

bool UTRIADIstanaExploreV5DR32Player0CaptureLibrary::
    SetIstanaExploreV5DR32Player0CapturePose(
        const FString& PoseId,
        FString& OutMessage)
{
    const FReviewedPose* Pose = FindReviewedPose(PoseId);
    if (!Pose || FScreenshotRequest::IsScreenshotRequested() ||
        GIsHighResScreenshot)
    {
        OutMessage = TEXT("R32 capture pose refused: select exactly 075m, 020m, 008m, 002m, surroundings_oblique_macdonald, 012m, 050m, 065m, 090m, or 095m and wait for any prior screenshot.");
        return false;
    }
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error))
    {
        OutMessage = TEXT("R32 capture pose refused: ") + Error;
        return false;
    }
    UCameraComponent* Camera = State.Pawn->GetExploreCameraComponent();
    APlayerCameraManager* CameraManager = State.Player->PlayerCameraManager;
    if (!Camera || !CameraManager || State.Player->GetViewTarget() != State.Pawn)
    {
        OutMessage = TEXT("R32 capture pose refused: exact Player0 camera manager/view target is unavailable.");
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
        OutMessage = TEXT("R32 capture pose failed while freezing or moving Player0.");
        return false;
    }
    Camera->SetRelativeLocation(ExactCameraRelativeLocation);
    Camera->SetRelativeRotation(NormalizedRotation);
    Camera->Activate(true);
    CameraManager->UpdateCamera(0.0f);
    GExactPoseState.Pawn = State.Pawn;
    GExactPoseState.PoseId = PoseId;
    GExactPoseState.ViewLocationCentimetres = Pose->ViewLocationCentimetres;
    GExactPoseState.ViewRotationDegrees = NormalizedRotation;
    if (!IsExactPoseActive(State, *Pose))
    {
        GExactPoseState = FExactPoseState{};
        OutMessage = TEXT("R32 capture pose failed exact 0.1 cm / 0.05 degree Player0 readback.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R32_PLAYER0_POSE_PASS poseId=%s exactQaViewPose=true turfIntersectionDistanceProbe=%s turfIntersectionDistanceMeters=%.3f viewLocationCm=%s viewRotationDeg=%s mapModified=false simulationCollisionNavigationSensorRfModified=false."),
        *PoseId,
        Pose->bTurfIntersectionDistanceProbe ? TEXT("true") : TEXT("false"),
        Pose->IntersectionDistanceMetres,
        *Pose->ViewLocationCentimetres.ToString(),
        *NormalizedRotation.ToString());
    return true;
}

bool UTRIADIstanaExploreV5DR32Player0CaptureLibrary::
    CaptureIstanaExploreV5DR32Player0TurfView(
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
        !IsExactPoseActive(State, *Pose))
    {
        OutMessage = TEXT("R32 turf capture refused: exact reviewed pose and validated R32 Game/Player0 state are required. ") + Error;
        return false;
    }
    UGameViewportClient* ViewportClient = State.World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("R32 turf capture requires the validated SDR Game viewport.");
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
        OutMessage = TEXT("R32 turf capture refused unsafe output, overwrite, or overlapping request.");
        return false;
    }

    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 R32 capture configuration.");
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
        OutMessage = TEXT("UE rejected the exact R32 Player0 game-viewport screenshot request.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R32_PLAYER0_TURF_CAPTURE_ACCEPTED poseId=%s output=%s width=2560 height=1440 hdr=false exactPlayer0=true exactQaViewPose=true r32MediumDistanceTurfVisible=true r32SelectedTransforms=4608 r32OwnedHismCount=12 r32CullMeters=65-90 turfIntersectionDistanceProbe=%s turfIntersectionDistanceMeters=%.3f providerFallbackVisualQa=true providerReadyProofClaimed=false ionAssetId=2275207 airSimTriadRuntimeLoaded=true weatherActorResolved=true mapModified=false providerInputsModified=false simulationCollisionNavigationSensorRfModified=false performanceAccepted=false surveyClaim=false botanicalClaim=false currentConditionClaim=false wrapperMustDecodeAndPostValidate=true."),
        *PoseId,
        *Destination,
        Pose->bTurfIntersectionDistanceProbe ? TEXT("true") : TEXT("false"),
        Pose->IntersectionDistanceMetres);
    return true;
}

bool UTRIADIstanaExploreV5DR32Player0CaptureLibrary::
    FinishIstanaExploreV5DR32Player0CaptureRun(FString& OutMessage)
{
    FValidatedCaptureState State;
    FString Error;
    if (!ResolveValidatedCaptureState(State, Error) ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("R32 capture finish refused until the exact R32 state is valid and the last screenshot has completed. ") +
            Error;
        return false;
    }
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R32_PLAYER0_CAPTURE_EXIT_ACCEPTED cleanRequest=true mapModified=false simulationCollisionNavigationSensorRfModified=false.");
    UE_LOG(LogTemp, Display, TEXT("%s"), *OutMessage);
    FPlatformMisc::RequestExit(false);
    return true;
}
