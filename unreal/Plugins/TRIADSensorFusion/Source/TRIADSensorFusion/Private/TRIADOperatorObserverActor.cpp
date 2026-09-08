#include "TRIADOperatorObserverActor.h"

#include "Algo/StableSort.h"
#include "CesiumGeoreference.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/LineBatchComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TRIADOperatorSlateWidget.h"
#include "TRIADGeodesy.h"
#include "TRIADSensorNodeActor.h"
#include "UnrealClient.h"

ATRIADOperatorObserverActor::ATRIADOperatorObserverActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.TickInterval = 1.0f / 30.0f;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    ObserverCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ApproachObserverCapture"));
    ObserverCapture->SetupAttachment(SceneRoot);
    ObserverCapture->bCaptureEveryFrame = false;
    ObserverCapture->bCaptureOnMovement = false;
    ObserverCapture->bAlwaysPersistRenderingState = true;
    ObserverCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    ObserverCapture->FOVAngle = 58.0f;
    // The approach feed is a clean scene view. Human-facing range rings,
    // labels and debug arrows remain in the main world view but must not cover
    // the drone/terrain image in this presentation camera.
    ObserverCapture->ShowFlags.SetOnScreenDebug(false);
    ObserverCapture->ShowFlags.SetGameplayDebug(false);
    ObserverCapture->ShowFlags.SetServerDrawDebug(false);
}

void ATRIADOperatorObserverActor::BeginPlay()
{
    Super::BeginPlay();
    EnsureOverlay();
}

void ATRIADOperatorObserverActor::InitializeObserver(
    const FTRIADOperatorObserverSettings& InSettings,
    const FTRIADSimulationPerimeter& InPerimeter,
    ACesiumGeoreference* InGeoreference,
    const TArray<TObjectPtr<ATRIADSensorNodeActor>>& InSensorNodes)
{
    Settings = InSettings;
    SimulationPerimeter = InPerimeter;
    Georeference = InGeoreference;
    SensorNodes = InSensorNodes;
    MinimapNodes.Reset();
    for (const ATRIADSensorNodeActor* Node : SensorNodes)
    {
        if (IsValid(Node))
        {
            MinimapNodes.Add(Node->GetNodeDefinition());
        }
    }

    ObserverRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("TRIADOperatorObserverRenderTarget"));
    ObserverRenderTarget->ClearColor = FLinearColor(0.002f, 0.012f, 0.018f, 1.0f);
    ObserverRenderTarget->InitCustomFormat(
        FMath::Clamp(Settings.CaptureWidth, 320, 1920),
        FMath::Clamp(Settings.CaptureHeight, 180, 1080),
        PF_B8G8R8A8,
        false);
    ObserverRenderTarget->UpdateResourceImmediate(true);
    ObserverCapture->TextureTarget = ObserverRenderTarget;

    // Human-facing labels/markers are useful in the free world camera but are
    // not pixels from the simulated scene. Keep them out of the clean observer
    // and live EO presentation feeds.
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
        {
            TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(*ActorIterator);
            for (UPrimitiveComponent* Component : PrimitiveComponents)
            {
                if (Component && Component->ComponentHasTag(TEXT("TRIADHumanOnlyOverlay")))
                {
                    ObserverCapture->HideComponent(Component);
                }
            }
        }
    }

    bOverlayVisible = Settings.bEnabled;
    if (GEngine && !bScreenMessageStateCaptured)
    {
        bPreviousOnScreenDebugMessagesEnabled = GEngine->bEnableOnScreenDebugMessages;
        bPreviousOnScreenDebugMessagesDisplay = GEngine->bEnableOnScreenDebugMessagesDisplay;
        bScreenMessageStateCaptured = true;
        if (bOverlayVisible)
        {
            GEngine->bEnableOnScreenDebugMessages = false;
            GEngine->bEnableOnScreenDebugMessagesDisplay = false;
        }
    }
    bInitialized = true;
    EnsureOverlay();
}

void ATRIADOperatorObserverActor::EnsureOverlay()
{
    if (!bInitialized || !Settings.bEnabled || OverlayWidget.IsValid() || !GEngine || !GEngine->GameViewport)
    {
        return;
    }

    OverlayWidget = SNew(STRIADOperatorSlateWidget)
        .Observer(TWeakObjectPtr<ATRIADOperatorObserverActor>(this));
    GEngine->GameViewport->AddViewportWidgetContent(OverlayWidget.ToSharedRef(), 1000);
}

void ATRIADOperatorObserverActor::RemoveOverlay()
{
    if (OverlayWidget.IsValid() && GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(OverlayWidget.ToSharedRef());
    }
    OverlayWidget.Reset();
}

void ATRIADOperatorObserverActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GEngine && bScreenMessageStateCaptured)
    {
        GEngine->bEnableOnScreenDebugMessages = bPreviousOnScreenDebugMessagesEnabled;
        GEngine->bEnableOnScreenDebugMessagesDisplay = bPreviousOnScreenDebugMessagesDisplay;
    }
    RemoveOverlay();
    ObserverCapture->TextureTarget = nullptr;
    ObserverRenderTarget = nullptr;
    CurrentContacts.Reset();
    SelectedRadarTrail.Reset();
    Super::EndPlay(EndPlayReason);
}

int32 ATRIADOperatorObserverActor::ScoreContact(const FTRIADOperatorObservedContact& Contact) const
{
    // Ranking is sensor-evidence-only.  Exercise-route and authored-role fields
    // may be shown as labelled context, but they cannot create or prioritize a
    // contact in the operator workflow.
    int32 Score = 0;
    Score += Contact.bDetectedBySearchRadar && Contact.bDetectedByRF ? 120 : 0;
    Score += Contact.bDetectedBySearchRadar ? 60 : 0;
    Score += Contact.bDetectedByRF ? 30 : 0;
    return Score;
}

void ATRIADOperatorObserverActor::UpdateDetectedContacts(
    const TArray<FTRIADOperatorObservedContact>& InContacts,
    double SampleSimulationSeconds)
{
    LastSampleSimulationSeconds = SampleSimulationSeconds;
    CurrentContacts.Reset();
    for (const FTRIADOperatorObservedContact& Contact : InContacts)
    {
        // This is the fail-closed boundary: a contact must carry a live target,
        // a reporting node, an opaque contact ID, and at least one positive sensor result.
        if (Contact.TargetActor.IsValid() &&
            Contact.ReportingNode.IsValid() &&
            !Contact.ContactId.IsEmpty() &&
            (Contact.bDetectedBySearchRadar || Contact.bDetectedByRF))
        {
            CurrentContacts.Add(Contact);
        }
    }

    Algo::StableSort(CurrentContacts, [this](const FTRIADOperatorObservedContact& Left, const FTRIADOperatorObservedContact& Right)
    {
        const int32 LeftScore = ScoreContact(Left);
        const int32 RightScore = ScoreContact(Right);
        if (LeftScore != RightScore)
        {
            return LeftScore > RightScore;
        }
        if (!FMath::IsNearlyEqual(Left.ReportedRangeMeters, Right.ReportedRangeMeters, 0.001))
        {
            return Left.ReportedRangeMeters < Right.ReportedRangeMeters;
        }
        return Left.ContactId < Right.ContactId;
    });

    SelectBestOrRetainedContact(false);
    const FTRIADOperatorObservedContact* Contact = GetSelectedContact();
    if (Contact && Contact->bHasRadarPositionEstimate)
    {
        const FVector2D Position(Contact->EstimatedLongitudeDegrees, Contact->EstimatedLatitudeDegrees);
        if (SelectedRadarTrail.IsEmpty() || !SelectedRadarTrail.Last().Equals(Position, 0.000001))
        {
            SelectedRadarTrail.Add(Position);
            const int32 MaximumPoints = FMath::Clamp(Settings.MaximumTrailPoints, 8, 512);
            if (SelectedRadarTrail.Num() > MaximumPoints)
            {
                SelectedRadarTrail.RemoveAt(0, SelectedRadarTrail.Num() - MaximumPoints, EAllowShrinking::No);
            }
        }
    }
}

void ATRIADOperatorObserverActor::SelectBestOrRetainedContact(bool bForceAdvance)
{
    const FString PreviousContactId = SelectedContactId;
    if (CurrentContacts.IsEmpty())
    {
        SelectedContactIndex = INDEX_NONE;
        SelectedContactId.Reset();
        SelectedRadarTrail.Reset();
        return;
    }

    int32 RetainedIndex = INDEX_NONE;
    if (!SelectedContactId.IsEmpty())
    {
        RetainedIndex = CurrentContacts.IndexOfByPredicate([this](const FTRIADOperatorObservedContact& Contact)
        {
            return Contact.ContactId == SelectedContactId;
        });
    }

    if (bForceAdvance)
    {
        SelectedContactIndex = RetainedIndex == INDEX_NONE
            ? 0
            : (RetainedIndex + 1) % CurrentContacts.Num();
    }
    else
    {
        SelectedContactIndex = RetainedIndex == INDEX_NONE ? 0 : RetainedIndex;
    }

    SelectedContactId = CurrentContacts[SelectedContactIndex].ContactId;
    if (SelectedContactId != PreviousContactId)
    {
        SelectedRadarTrail.Reset();
        LastContactSelectionSimulationSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    }
}

bool ATRIADOperatorObserverActor::HasFreshSelectedContact() const
{
    const FTRIADOperatorObservedContact* Contact = GetSelectedContact();
    if (!Contact || !GetWorld())
    {
        return false;
    }
    const double NowSeconds = GetWorld()->GetTimeSeconds();
    const double MaximumAgeSeconds = FMath::Max(Settings.ContactFreshnessSeconds, 0.1f);
    return NowSeconds - LastSampleSimulationSeconds <= MaximumAgeSeconds &&
        NowSeconds - Contact->ObservationSimulationSeconds <= MaximumAgeSeconds;
}

const FTRIADOperatorObservedContact* ATRIADOperatorObserverActor::GetSelectedContact() const
{
    return CurrentContacts.IsValidIndex(SelectedContactIndex)
        ? &CurrentContacts[SelectedContactIndex]
        : nullptr;
}

bool ATRIADOperatorObserverActor::TryGetFreshSensorView(
    UTextureRenderTarget2D*& OutRenderTarget,
    FTRIADPTZConfirmationResult& OutConfirmation) const
{
    OutRenderTarget = nullptr;
    OutConfirmation = FTRIADPTZConfirmationResult();
    const FTRIADOperatorObservedContact* Contact = GetSelectedContact();
    ATRIADSensorNodeActor* Node = Contact ? Contact->ReportingNode.Get() : nullptr;
    if (!HasFreshSelectedContact() || !Node || !Contact->bDetectedBySearchRadar || Contact->RadarTrackId.IsEmpty())
    {
        return false;
    }
    return Node->TryGetFreshOperatorEOView(
        Contact->RadarTrackId,
        FMath::Max(Settings.ContactFreshnessSeconds, 0.1f),
        OutRenderTarget,
        OutConfirmation);
}

FString ATRIADOperatorObserverActor::GetCameraModeLabel() const
{
    return bShowingSensorAngle
        ? TEXT("ANGLE: NEAREST REPORTING SENSOR / LIVE CUE GEOMETRY")
        : TEXT("ANGLE: INBOUND DRONE / SINGAPORE AHEAD");
}

void ATRIADOperatorObserverActor::DrawOperatorWorldAids() const
{
    if (!HasFreshSelectedContact() || !GetWorld())
    {
        return;
    }
    const FTRIADOperatorObservedContact* Contact = GetSelectedContact();
    AActor* Target = Contact ? Contact->TargetActor.Get() : nullptr;
    ATRIADSensorNodeActor* Node = Contact ? Contact->ReportingNode.Get() : nullptr;
    if (!Target || !Node)
    {
        return;
    }

    const FColor EvidenceColor = Contact->bDetectedBySearchRadar && Contact->bDetectedByRF
        ? FColor(70, 255, 130)
        : Contact->bDetectedBySearchRadar
            ? FColor(40, 200, 255)
            : FColor(255, 40, 220);
    const float LifeTime = FMath::Max(PrimaryActorTick.TickInterval * 1.8f, 0.08f);
    DrawDebugDirectionalArrow(
        GetWorld(),
        Node->GetActorLocation(),
        Target->GetActorLocation(),
        450.0f,
        EvidenceColor,
        false,
        LifeTime,
        3,
        12.0f);
    DrawDebugSphere(GetWorld(), Node->GetActorLocation(), 2400.0f, 18, FColor(255, 180, 0), false, LifeTime, 3, 12.0f);
    DrawDebugSphere(GetWorld(), Target->GetActorLocation(), 550.0f, 14, EvidenceColor, false, LifeTime, 3, 9.0f);
}

void ATRIADOperatorObserverActor::UpdateObserverCamera()
{
    if (!HasFreshSelectedContact() || !ObserverCapture)
    {
        return;
    }
    const FTRIADOperatorObservedContact* Contact = GetSelectedContact();
    AActor* Target = Contact ? Contact->TargetActor.Get() : nullptr;
    ATRIADSensorNodeActor* Node = Contact ? Contact->ReportingNode.Get() : nullptr;
    if (!Target || !Node)
    {
        return;
    }

    // DrawDebug helpers are stored in world line-batcher components and do not
    // consistently obey every SceneCapture show flag. Explicitly hide those
    // components so range rings cannot obscure the drone or Singapore terrain.
    if (UWorld* World = GetWorld())
    {
        ObserverCapture->HideComponent(World->LineBatcher.Get());
        ObserverCapture->HideComponent(World->PersistentLineBatcher.Get());
        ObserverCapture->HideComponent(World->ForegroundLineBatcher.Get());
    }

    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const double AngleSeconds = FMath::Max(Settings.CameraAngleSeconds, 2.0f);
    bShowingSensorAngle = FMath::Fmod(NowSeconds, AngleSeconds * 2.0) >= AngleSeconds;

    const FVector TargetLocation = Target->GetActorLocation();
    const FVector SensorLocation = Node->GetActorLocation();
    FVector CameraLocation;
    FVector LookAtLocation;
    if (bShowingSensorAngle)
    {
        const FVector DirectionToTarget = (TargetLocation - SensorLocation).GetSafeNormal();
        const FVector Up = Node->GetActorUpVector().GetSafeNormal();
        CameraLocation = SensorLocation - DirectionToTarget * 6500.0f + Up * 2800.0f;
        LookAtLocation = SensorLocation + DirectionToTarget * 4200.0f + Up * 500.0f;
    }
    else
    {
        FVector DirectionToSingapore = (SensorLocation - TargetLocation).GetSafeNormal();
        if (Georeference && SimulationPerimeter.bEnabled)
        {
            const bool bCirclePerimeter = TRIAD::Geodesy::IsCircle(SimulationPerimeter);
            const FVector SingaporeCenter = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(
                bCirclePerimeter
                    ? SimulationPerimeter.CenterLongitudeDegrees
                    : (SimulationPerimeter.MinimumLongitudeDegrees + SimulationPerimeter.MaximumLongitudeDegrees) * 0.5,
                bCirclePerimeter
                    ? SimulationPerimeter.CenterLatitudeDegrees
                    : (SimulationPerimeter.MinimumLatitudeDegrees + SimulationPerimeter.MaximumLatitudeDegrees) * 0.5,
                FMath::Max(Contact->EstimatedHeightMeters, 100.0)));
            const FVector CenterDirection = (SingaporeCenter - TargetLocation).GetSafeNormal();
            if (!CenterDirection.IsNearlyZero())
            {
                DirectionToSingapore = CenterDirection;
            }
        }
        const FVector Up = Target->GetActorUpVector().GetSafeNormal();
        const FVector Side = FVector::CrossProduct(Up, DirectionToSingapore).GetSafeNormal();
        CameraLocation = TargetLocation - DirectionToSingapore * 1800.0f + Up * 650.0f + Side * 380.0f;
        LookAtLocation = TargetLocation + Up * 20.0f;
    }

    SetActorLocationAndRotation(CameraLocation, (LookAtLocation - CameraLocation).Rotation());
    ObserverCapture->CaptureScene();
}

void ATRIADOperatorObserverActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    EnsureOverlay();

    if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        if (PlayerController->WasInputKeyJustPressed(EKeys::O))
        {
            bOverlayVisible = !bOverlayVisible;
            if (GEngine && bScreenMessageStateCaptured)
            {
                GEngine->bEnableOnScreenDebugMessages = bOverlayVisible
                    ? false
                    : bPreviousOnScreenDebugMessagesEnabled;
                GEngine->bEnableOnScreenDebugMessagesDisplay = bOverlayVisible
                    ? false
                    : bPreviousOnScreenDebugMessagesDisplay;
            }
        }
        if (PlayerController->WasInputKeyJustPressed(EKeys::N) && bOverlayVisible)
        {
            SelectBestOrRetainedContact(true);
        }
    }

    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (HasFreshSelectedContact() &&
        NowSeconds - LastContactSelectionSimulationSeconds >= FMath::Max(Settings.ContactCycleSeconds, 2.0f))
    {
        SelectBestOrRetainedContact(true);
    }

    if (bOverlayVisible && HasFreshSelectedContact())
    {
        DrawOperatorWorldAids();
        // DrawDebugString is rendered by the player HUD rather than the scene
        // line batchers, so clear those transient world labels while the
        // structured operator panel is visible. Hiding the panel restores the
        // normal debug-label workflow on the next sensor sample.
        FlushDebugStrings(GetWorld());
        UpdateObserverCamera();

        // Opt-in runtime evidence for automated validation. This is inert in
        // normal editor/game sessions and captures the Slate UI as actually
        // rendered by Unreal, rather than fabricating a mock-up.
        if (!bValidationScreenshotRequested &&
            FParse::Param(FCommandLine::Get(), TEXT("TRIADCaptureObserver")) &&
            NowSeconds >= 5.0)
        {
            const FString ScreenshotDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"));
            IFileManager::Get().MakeDirectory(*ScreenshotDirectory, true);
            const FString ScreenshotPath = FPaths::Combine(
                ScreenshotDirectory,
                TEXT("TRIAD_OperatorObserver_Validation.png"));
            FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
            bValidationScreenshotRequested = true;
            UE_LOG(LogTemp, Display, TEXT("TRIAD operator observer validation screenshot requested: %s"), *ScreenshotPath);
        }
    }
}
