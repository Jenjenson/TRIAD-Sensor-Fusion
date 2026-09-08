#include "TRIADSensorNodeActor.h"

#include "CesiumGeoreference.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "TRIADRFEmitterComponent.h"
#include "TRIADSensorNodeComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
const FName HumanOnlyOverlayComponentTag(TEXT("TRIADHumanOnlyOverlay"));

bool IsHumanOnlyOverlayComponent(const UActorComponent* Component)
{
    return IsValid(Component) && Component->ComponentHasTag(HumanOnlyOverlayComponentTag);
}

FString TrimmedFrequencyValue(double Value, int32 DecimalPlaces)
{
    FString Text = FString::Printf(TEXT("%.*f"), DecimalPlaces, Value);
    while (Text.EndsWith(TEXT("0")))
    {
        Text.LeftChopInline(1, EAllowShrinking::No);
    }
    if (Text.EndsWith(TEXT(".")))
    {
        Text.LeftChopInline(1, EAllowShrinking::No);
    }
    return Text;
}

FString FormatRFFrequency(double FrequencyGHz, bool bCompactUnit)
{
    if (FrequencyGHz < 1.0)
    {
        return TrimmedFrequencyValue(FrequencyGHz * 1000.0, 3) +
            (bCompactUnit ? TEXT("M") : TEXT(" MHz"));
    }
    return TrimmedFrequencyValue(FrequencyGHz, 3) +
        (bCompactUnit ? TEXT("G") : TEXT(" GHz"));
}

FBox GetModelFacingPrimitiveBounds(const AActor* Target)
{
    FBox Bounds(EForceInit::ForceInit);
    if (!IsValid(Target))
    {
        return Bounds;
    }

    TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Target);
    for (const UPrimitiveComponent* Component : PrimitiveComponents)
    {
        if (!IsValid(Component) || IsHumanOnlyOverlayComponent(Component) || !Component->IsVisible())
        {
            continue;
        }
        Bounds += Component->Bounds.GetBox();
    }
    return Bounds;
}

bool AtomicallyPublishLatestFile(const FString& TemporaryPath, const FString& DestinationPath)
{
#if PLATFORM_WINDOWS
    return ::MoveFileExW(
        *TemporaryPath,
        *DestinationPath,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return IFileManager::Get().Move(*DestinationPath, *TemporaryPath, true, true);
#endif
}

FColor SimulatedThermalPalette(uint8 Intensity)
{
    const double T = static_cast<double>(Intensity) / 255.0;
    const uint8 Red = static_cast<uint8>(255.0 * FMath::Clamp(3.0 * T - 1.0, 0.0, 1.0));
    const uint8 Green = static_cast<uint8>(255.0 * FMath::Clamp(3.0 * T - 0.5, 0.0, 1.0));
    const uint8 Blue = static_cast<uint8>(255.0 * FMath::Clamp(1.5 - 3.0 * T, 0.0, 1.0));
    return FColor(Red, Green, Blue, 255);
}

const uint8* ThermalLabelGlyph(TCHAR Character)
{
    static const uint8 Space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8 A[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8 C[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8 E[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8 H[7] = {17, 17, 17, 31, 17, 17, 17};
    static const uint8 I[7] = {31, 4, 4, 4, 4, 4, 31};
    static const uint8 L[7] = {16, 16, 16, 16, 16, 16, 31};
    static const uint8 M[7] = {17, 27, 21, 21, 17, 17, 17};
    static const uint8 N[7] = {17, 25, 21, 19, 17, 17, 17};
    static const uint8 R[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8 S[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8 T[7] = {31, 4, 4, 4, 4, 4, 4};
    static const uint8 Y[7] = {17, 17, 10, 4, 4, 4, 4};
    switch (FChar::ToUpper(Character))
    {
    case 'A': return A;
    case 'C': return C;
    case 'E': return E;
    case 'H': return H;
    case 'I': return I;
    case 'L': return L;
    case 'M': return M;
    case 'N': return N;
    case 'R': return R;
    case 'S': return S;
    case 'T': return T;
    case 'Y': return Y;
    default: return Space;
    }
}

void DrawSyntheticThermalLabel(TArray<FColor>& Pixels, int32 Width, int32 Height)
{
    if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
    {
        return;
    }
    const int32 Scale = FMath::Clamp(Width / 480, 1, 3);
    const int32 BannerHeight = FMath::Min(12 * Scale, Height);
    for (int32 Y = 0; Y < BannerHeight; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            Pixels[Y * Width + X] = FColor(12, 12, 18, 255);
        }
    }

    const FString Label(TEXT("SIM THERMAL SYNTHETIC"));
    int32 CursorX = 3 * Scale;
    const int32 OriginY = 2 * Scale;
    for (const TCHAR Character : Label)
    {
        const uint8* Glyph = ThermalLabelGlyph(Character);
        for (int32 Row = 0; Row < 7; ++Row)
        {
            for (int32 Column = 0; Column < 5; ++Column)
            {
                if ((Glyph[Row] & (1 << (4 - Column))) == 0)
                {
                    continue;
                }
                for (int32 Dy = 0; Dy < Scale; ++Dy)
                {
                    for (int32 Dx = 0; Dx < Scale; ++Dx)
                    {
                        const int32 X = CursorX + Column * Scale + Dx;
                        const int32 Y = OriginY + Row * Scale + Dy;
                        if (X >= 0 && X < Width && Y >= 0 && Y < Height)
                        {
                            Pixels[Y * Width + X] = FColor(255, 235, 90, 255);
                        }
                    }
                }
            }
        }
        CursorX += 6 * Scale;
        if (CursorX + 5 * Scale >= Width)
        {
            break;
        }
    }
}
}

ATRIADSensorNodeActor::ATRIADSensorNodeActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.TickInterval = 0.2f;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
    MarkerMesh->SetupAttachment(SceneRoot);
    MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MarkerMesh->SetCastShadow(false);
    MarkerMesh->ComponentTags.AddUnique(HumanOnlyOverlayComponentTag);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (MarkerMeshFinder.Succeeded())
    {
        MarkerMesh->SetStaticMesh(MarkerMeshFinder.Object);
    }

    NodeLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NodeLabel"));
    NodeLabel->SetupAttachment(SceneRoot);
    NodeLabel->SetRelativeLocation(FVector(0.0, 0.0, 300.0));
    NodeLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    NodeLabel->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    NodeLabel->SetTextRenderColor(FColor::Cyan);
    NodeLabel->SetWorldSize(1200.0f);
    NodeLabel->bAlwaysRenderAsText = true;
    NodeLabel->SetCastShadow(false);
    NodeLabel->ComponentTags.AddUnique(HumanOnlyOverlayComponentTag);
    NodeLabel->SetText(FText::FromString(TEXT("TRIAD Sensor Node")));

    RGBCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RGBCapture"));
    RGBCapture->SetupAttachment(SceneRoot);
    RGBCapture->bCaptureEveryFrame = false;
    RGBCapture->bCaptureOnMovement = false;
    RGBCapture->bAlwaysPersistRenderingState = true;
    RGBCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    RGBCapture->ShowFlags.SetOnScreenDebug(false);
    RGBCapture->ShowFlags.SetGameplayDebug(false);
    RGBCapture->ShowFlags.SetServerDrawDebug(false);

    DepthCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("DepthCapture"));
    DepthCapture->SetupAttachment(SceneRoot);
    DepthCapture->bCaptureEveryFrame = false;
    DepthCapture->bCaptureOnMovement = false;
    DepthCapture->bAlwaysPersistRenderingState = true;
    DepthCapture->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
    DepthCapture->ShowFlags.SetOnScreenDebug(false);
    DepthCapture->ShowFlags.SetGameplayDebug(false);
    DepthCapture->ShowFlags.SetServerDrawDebug(false);

    EOPTZCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("EOPTZCapture"));
    EOPTZCapture->SetupAttachment(SceneRoot);
    EOPTZCapture->bCaptureEveryFrame = false;
    EOPTZCapture->bCaptureOnMovement = false;
    EOPTZCapture->bAlwaysPersistRenderingState = true;
    EOPTZCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    EOPTZCapture->ShowFlags.SetOnScreenDebug(false);
    EOPTZCapture->ShowFlags.SetGameplayDebug(false);
    EOPTZCapture->ShowFlags.SetServerDrawDebug(false);

    ThermalPTZCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ThermalPTZCapture"));
    ThermalPTZCapture->SetupAttachment(SceneRoot);
    ThermalPTZCapture->bCaptureEveryFrame = false;
    ThermalPTZCapture->bCaptureOnMovement = false;
    ThermalPTZCapture->bAlwaysPersistRenderingState = true;
    ThermalPTZCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    ThermalPTZCapture->ShowFlags.SetOnScreenDebug(false);
    ThermalPTZCapture->ShowFlags.SetGameplayDebug(false);
    ThermalPTZCapture->ShowFlags.SetServerDrawDebug(false);

    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("CesiumGlobeAnchor"));
    SensorNode = CreateDefaultSubobject<UTRIADSensorNodeComponent>(TEXT("SensorNode"));
}

void ATRIADSensorNodeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateLongRangePTZ(DeltaSeconds);

    DrawPersistentNodeVisualization();

    // Keep the large world-space label readable from an aerial operator camera.
    if (NodeLabel && NodeLabel->IsVisible())
    {
        if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
        {
            const FVector ToCamera = CameraManager->GetCameraLocation() - NodeLabel->GetComponentLocation();
            if (!ToCamera.IsNearlyZero())
            {
                FRotator FacingRotation = ToCamera.Rotation();
                FacingRotation.Yaw += 180.0f;
                NodeLabel->SetWorldRotation(FacingRotation);
            }
        }
    }
}

void ATRIADSensorNodeActor::ConfigureNode(
    const FTRIADGeodeticSensorNode& InDefinition,
    ACesiumGeoreference* InGeoreference,
    const FString& InFramesRootDirectory,
    int32 InMaxFrames,
    int64 InMaxFrameBytes)
{
    SensorNode->ConfigureNode(InDefinition);
    MarkerMesh->SetRelativeScale3D(FVector(FMath::Max(InDefinition.MarkerScale, 0.1f)));
    MarkerMaterial = MarkerMesh->CreateAndSetMaterialInstanceDynamic(0);
    NodeLabel->SetVisibility(InDefinition.bVisualizeNode);
    MarkerMesh->SetVisibility(InDefinition.bVisualizeNode);
    NodeLabel->SetRelativeLocation(FVector(
        0.0,
        0.0,
        FMath::Clamp(InDefinition.VisualBeaconHeightMeters * 0.08f, 35.0f, 90.0f) * 100.0f));
    NodeLabel->SetWorldSize(FMath::Max(InDefinition.VisualLabelWorldSizeMeters, 1.0f) * 100.0f);
    VisualDetectionState = EVisualDetectionState::Idle;
    ApplyVisualState();

    RGBCapture->FOVAngle = FMath::Clamp(InDefinition.CameraFieldOfViewDegrees, 5.0f, 170.0f);
    DepthCapture->FOVAngle = RGBCapture->FOVAngle;
    RGBCapture->SetRelativeRotation(InDefinition.CameraRelativeRotation);
    DepthCapture->SetRelativeRotation(InDefinition.CameraRelativeRotation);
    EOPTZCapture->FOVAngle = FMath::Clamp(InDefinition.EOPTZFieldOfViewDegrees, 1.0f, 45.0f);
    ThermalPTZCapture->FOVAngle = FMath::Clamp(InDefinition.ThermalPTZFieldOfViewDegrees, 1.0f, 45.0f);
    EOPTZCapture->SetRelativeRotation(InDefinition.CameraRelativeRotation);
    ThermalPTZCapture->SetRelativeRotation(InDefinition.CameraRelativeRotation);

    if (InGeoreference)
    {
        GlobeAnchor->SetGeoreference(TSoftObjectPtr<ACesiumGeoreference>(InGeoreference));
    }
    GlobeAnchor->MoveToLongitudeLatitudeHeight(FVector(
        InDefinition.LongitudeDegrees,
        InDefinition.LatitudeDegrees,
        InDefinition.HeightMeters));

    MaximumFrameCount = FMath::Max(InMaxFrames, 1);
    MaximumFrameBytes = FMath::Max<int64>(InMaxFrameBytes, 1024);
    FrameDirectory = FPaths::Combine(InFramesRootDirectory, FPaths::MakeValidFileName(InDefinition.NodeId, TEXT('_')));
    LongRangeTelemetryRootDirectory = FPaths::GetPath(InFramesRootDirectory);
    if (InDefinition.bEnableEOPTZ || InDefinition.bEnableThermalPTZ)
    {
        InitializeLongRangeCaptureTargets();
    }

    GetWorldTimerManager().ClearTimer(CaptureTimerHandle);
    if (InDefinition.bCaptureCameraFrames)
    {
        InitializeCaptureTargets();
        bDebugVisualCaptureFilterActive = RefreshCaptureExclusionList();
        IFileManager::Get().MakeDirectory(*FrameDirectory, true);
        GetWorldTimerManager().SetTimer(
            CaptureTimerHandle,
            this,
            &ATRIADSensorNodeActor::CaptureCameraFrame,
            FMath::Max(InDefinition.CameraCaptureCadenceSeconds, 0.1f),
            true,
            0.25f);
    }
}

void ATRIADSensorNodeActor::BeginRFVisualizationSample()
{
    VisualDetectionState = EVisualDetectionState::Idle;
    SampleRFDetectionCount = 0;
    SamplePreliminaryCueEvidenceCount = 0;
    SampleRFLinksDrawn = 0;
    SampleSearchRadarDetectionCount = 0;
    SampleMaximumSnrDb = -TNumericLimits<double>::Max();
    SampleDetectedFrequenciesGHz.Reset();
    ApplyVisualState();
}

void ATRIADSensorNodeActor::RecordSearchRadarDetection(
    const FVector& TargetWorldLocation,
    const FString& TrackId,
    double RangeMeters,
    double Confidence,
    float VisualizationSeconds)
{
    ++SampleSearchRadarDetectionCount;
    if (!GetWorld() || !SensorNode || !SensorNode->Definition.bVisualizeLongRangeSensorCue)
    {
        return;
    }
    const float LifeTime = FMath::Max(VisualizationSeconds, 0.05f);
    const float Thickness = static_cast<float>(FMath::Lerp(2.0, 6.0, FMath::Clamp(Confidence, 0.0, 1.0)));
    DrawDebugDirectionalArrow(
        GetWorld(),
        GetActorLocation(),
        TargetWorldLocation,
        350.0f,
        FColor(40, 200, 255),
        false,
        LifeTime,
        2,
        Thickness);
    DrawDebugSphere(
        GetWorld(),
        TargetWorldLocation,
        260.0f,
        12,
        FColor(40, 200, 255),
        false,
        LifeTime,
        2,
        Thickness);
    UE_LOG(
        LogTemp,
        VeryVerbose,
        TEXT("[SIM DEBUG RADAR CUE] %s | %.0f m | conf %.2f"),
        *TrackId,
        RangeMeters,
        Confidence);
}

void ATRIADSensorNodeActor::RecordRFDetection(
    const FVector& TargetWorldLocation,
    double FrequencyGHz,
    bool bPreliminaryCueEvidence,
    double SnrDb,
    float VisualizationSeconds,
    bool bDrawLink,
    int32 MaxLinksToDraw)
{
    ++SampleRFDetectionCount;
    SamplePreliminaryCueEvidenceCount += bPreliminaryCueEvidence ? 1 : 0;
    SampleMaximumSnrDb = FMath::Max(SampleMaximumSnrDb, SnrDb);
    SampleDetectedFrequenciesGHz.AddUnique(FrequencyGHz);
    if (VisualDetectionState == EVisualDetectionState::Idle)
    {
        VisualDetectionState = EVisualDetectionState::RFCandidate;
        ApplyVisualState();
    }

    if (!bDrawLink || !GetWorld() || SampleRFLinksDrawn >= FMath::Max(MaxLinksToDraw, 1))
    {
        return;
    }
    ++SampleRFLinksDrawn;

    // Band color remains invariant so a link never visually implies a different frequency.
    const FColor BandColor = FMath::IsNearlyEqual(FrequencyGHz, 2.4, 0.05)
        ? FColor(0, 220, 255)
        : FMath::IsNearlyEqual(FrequencyGHz, 5.8, 0.05)
            ? FColor(255, 40, 220)
            : FColor::White;
    const float LifeTime = FMath::Max(VisualizationSeconds, 0.05f);
    const float Thickness = bPreliminaryCueEvidence ? 5.0f : 3.0f;
    DrawDebugDirectionalArrow(
        GetWorld(),
        GetActorLocation(),
        TargetWorldLocation,
        400.0f,
        BandColor,
        false,
        LifeTime,
        1,
        Thickness);
    DrawDebugSphere(
        GetWorld(),
        TargetWorldLocation,
        bPreliminaryCueEvidence ? 500.0f : 350.0f,
        10,
        BandColor,
        false,
        LifeTime,
        1,
        Thickness);
}

void ATRIADSensorNodeActor::MarkRFMultinodePreliminaryCue()
{
    if (VisualDetectionState != EVisualDetectionState::RFMultinodeCue)
    {
        VisualDetectionState = EVisualDetectionState::RFMultinodeCue;
        ApplyVisualState();
    }
}

void ATRIADSensorNodeActor::FinalizeRFVisualizationSample(bool bShowOnScreenStatus, float StatusSeconds)
{
    UpdateWorldLabel();
    if (!bShowOnScreenStatus || !GEngine || !SensorNode)
    {
        return;
    }

    SampleDetectedFrequenciesGHz.Sort();
    FString Bands;
    for (const double FrequencyGHz : SampleDetectedFrequenciesGHz)
    {
        if (!Bands.IsEmpty())
        {
            Bands += TEXT(",");
        }
        Bands += FormatRFFrequency(FrequencyGHz, true);
    }
    if (Bands.IsEmpty())
    {
        Bands = TEXT("none");
    }

    const FString SnrText = SampleRFDetectionCount > 0 && FMath::IsFinite(SampleMaximumSnrDb)
        ? FString::Printf(TEXT("%.1f dB max SNR"), SampleMaximumSnrDb)
        : TEXT("no RF detection");
    const FString Status = FString::Printf(
        TEXT("SENSOR NODE %-18s | SEARCH RADAR %d | RF detected %-25s | links %d (drawn %d) | cue evidence %d | %s | WX %s | %s"),
        *SensorNode->Definition.NodeId,
        SampleSearchRadarDetectionCount,
        *Bands,
        SampleRFDetectionCount,
        SampleRFLinksDrawn,
        SamplePreliminaryCueEvidenceCount,
        *SnrText,
        *ActiveWeatherProfileName,
        *GetStateText());
    const uint64 MessageKey =
        (static_cast<uint64>(GetTypeHash(SensorNode->Definition.NodeId)) << 32) | 0x52464E44ULL; // "RFND"
    GEngine->AddOnScreenDebugMessage(MessageKey, FMath::Max(StatusSeconds, 0.25f), GetStateColor(), Status);
}

void ATRIADSensorNodeActor::SetWeatherMetadata(
    const FString& ProfileName,
    bool bVisualWeatherApplied,
    double RainRateMillimetersPerHour,
    double VisibilityMeters)
{
    ActiveWeatherProfileName = ProfileName;
    bActiveVisualWeatherApplied = bVisualWeatherApplied;
    ActiveRainRateMillimetersPerHour = FMath::Max(RainRateMillimetersPerHour, 0.0);
    ActiveVisibilityMeters = FMath::Max(VisibilityMeters, 1.0);
    UpdateWorldLabel();
}

FColor ATRIADSensorNodeActor::GetStateColor() const
{
    switch (VisualDetectionState)
    {
    case EVisualDetectionState::RFCandidate:
        return FColor(255, 210, 0);
    case EVisualDetectionState::RFMultinodeCue:
        return FColor(255, 140, 0);
    case EVisualDetectionState::Idle:
    default:
        return FColor(0, 220, 255);
    }
}

FString ATRIADSensorNodeActor::GetStateText() const
{
    switch (VisualDetectionState)
    {
    case EVisualDetectionState::RFCandidate:
        return TEXT("RF CANDIDATE");
    case EVisualDetectionState::RFMultinodeCue:
        return TEXT("RF MULTI-NODE CUE");
    case EVisualDetectionState::Idle:
    default:
        return TEXT("ONLINE / IDLE");
    }
}

void ATRIADSensorNodeActor::ApplyVisualState()
{
    const FColor StateColor = GetStateColor();
    if (NodeLabel)
    {
        NodeLabel->SetTextRenderColor(StateColor);
    }
    if (MarkerMaterial)
    {
        MarkerMaterial->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor(StateColor));
    }
    UpdateWorldLabel();
}

void ATRIADSensorNodeActor::UpdateWorldLabel()
{
    if (!NodeLabel || !SensorNode)
    {
        return;
    }

    const FTRIADGeodeticSensorNode& Definition = SensorNode->Definition;
    TArray<double> ConfiguredFrequencies = Definition.SupportedFrequenciesGHz;
    ConfiguredFrequencies.Sort();
    const FString RFConfigurationText = ConfiguredFrequencies.IsEmpty()
        ? TEXT("RF NO CHANNELS")
        : FString::Printf(
            TEXT("RF WIDEBAND %d CH | %s to %s"),
            ConfiguredFrequencies.Num(),
            *FormatRFFrequency(ConfiguredFrequencies[0], false),
            *FormatRFFrequency(ConfiguredFrequencies.Last(), false));

    NodeLabel->SetText(FText::FromString(FString::Printf(
        TEXT("[ SENSOR NODE ] %s\n%s | RF RANGE %.1f km\nSEARCH RADAR %.1f km | EO PTZ %.1f deg | THERMAL PTZ %.1f deg\n%.5f N, %.5f E | WX %s | %s"),
        *Definition.NodeId,
        *RFConfigurationText,
        Definition.DetectionRangeMeters / 1000.0,
        Definition.bEnableSearchRadar ? FMath::Max(Definition.SearchRadarRangeMeters, 5000.0) / 1000.0 : 0.0,
        Definition.bEnableEOPTZ ? Definition.EOPTZFieldOfViewDegrees : 0.0f,
        Definition.bEnableThermalPTZ ? Definition.ThermalPTZFieldOfViewDegrees : 0.0f,
        Definition.LatitudeDegrees,
        Definition.LongitudeDegrees,
        *ActiveWeatherProfileName,
        *GetStateText())));
}

void ATRIADSensorNodeActor::DrawPersistentNodeVisualization() const
{
    if (!SensorNode || !SensorNode->Definition.bVisualizeNode || !GetWorld())
    {
        return;
    }

    const FTRIADGeodeticSensorNode& Definition = SensorNode->Definition;
    const FColor StateColor = GetStateColor();
    const FVector Base = GetActorLocation();
    const FVector Up = GetActorUpVector();
    const FVector AxisX = GetActorForwardVector();
    const FVector AxisY = GetActorRightVector();
    const FVector BeaconTop = Base + Up * (FMath::Max(Definition.VisualBeaconHeightMeters, 10.0f) * 100.0f);
    const float LifeTime = FMath::Max(PrimaryActorTick.TickInterval * 1.75f, 0.35f);

    DrawDebugDirectionalArrow(GetWorld(), Base, BeaconTop, 1500.0f, StateColor, false, LifeTime, 1, 18.0f);
    DrawDebugSphere(
        GetWorld(),
        Base,
        FMath::Max(Definition.MarkerScale * 500.0f, 1000.0f),
        16,
        StateColor,
        false,
        LifeTime,
        1,
        12.0f);

    const float RangeCentimeters = static_cast<float>(FMath::Max(Definition.DetectionRangeMeters, 0.0) * 100.0);
    if (RangeCentimeters > 0.0f)
    {
        const int32 Segments = FMath::Clamp(Definition.VisualRangeRingSegments, 24, 256);
        DrawDebugCircle(GetWorld(), Base, RangeCentimeters, Segments, StateColor, false, LifeTime, 0, 8.0f, AxisX, AxisY, false);
        DrawDebugCircle(GetWorld(), Base, RangeCentimeters * 0.5f, Segments / 2, StateColor, false, LifeTime, 0, 3.0f, AxisX, AxisY, false);
    }

    // Operator-only green ring identifies the geometry-based SEARCH_RADAR
    // envelope separately from the RF receiver ring above.
    if (Definition.bEnableSearchRadar)
    {
        const float RadarRangeCentimeters = static_cast<float>(
            FMath::Max(Definition.SearchRadarRangeMeters, 5000.0) * 100.0);
        DrawDebugCircle(
            GetWorld(),
            Base + Up * 200.0f,
            RadarRangeCentimeters,
            FMath::Clamp(Definition.VisualRangeRingSegments, 24, 256),
            FColor(80, 255, 110),
            false,
            LifeTime,
            2,
            5.0f,
            AxisX,
            AxisY,
            false);
    }
}

const FTRIADGeodeticSensorNode& ATRIADSensorNodeActor::GetNodeDefinition() const
{
    return SensorNode->Definition;
}

void ATRIADSensorNodeActor::AimCamerasAtWorldLocation(const FVector& TargetWorldLocation)
{
    const FVector Direction = TargetWorldLocation - GetActorLocation();
    if (Direction.IsNearlyZero())
    {
        return;
    }

    const FRotator BoresightRotation = Direction.Rotation();
    RGBCapture->SetWorldRotation(BoresightRotation);
    DepthCapture->SetWorldRotation(BoresightRotation);
}

void ATRIADSensorNodeActor::CueLongRangeSensors(
    AActor* Target,
    const FString& ContactId,
    const FString& TrackId,
    double MeasuredRangeMeters,
    double MeasuredBearingDegrees,
    double MeasuredElevationDegrees,
    double RadarConfidence,
    double SimulationSeconds)
{
    if (!IsValid(Target) || !SensorNode || !SensorNode->Definition.bEnableSearchRadar ||
        (!SensorNode->Definition.bEnableEOPTZ && !SensorNode->Definition.bEnableThermalPTZ))
    {
        ClearLongRangeCue();
        return;
    }

    const bool bNewCue = CurrentRadarTrackId != TrackId || CurrentRadarCueTarget.Get() != Target;
    CurrentRadarCueTarget = Target;
    CurrentSensorContactId = ContactId;
    CurrentRadarTrackId = TrackId;
    CurrentRadarMeasuredRangeMeters = FMath::Max(MeasuredRangeMeters, 0.0);
    CurrentRadarMeasuredBearingDegrees = FMath::Fmod(
        FMath::Fmod(MeasuredBearingDegrees, 360.0) + 360.0,
        360.0);
    CurrentRadarMeasuredElevationDegrees = MeasuredElevationDegrees;
    CurrentRadarConfidence = FMath::Clamp(RadarConfidence, 0.0, 1.0);
    LastRadarCueSimulationSeconds = SimulationSeconds;

    // Convert the radar's local north-clockwise bearing into the node actor's
    // East-South-Up local frame, then into an Unreal world boresight.
    const double BearingRadians = FMath::DegreesToRadians(CurrentRadarMeasuredBearingDegrees);
    const double ElevationRadians = FMath::DegreesToRadians(CurrentRadarMeasuredElevationDegrees);
    const double CosElevation = FMath::Cos(ElevationRadians);
    const FVector LocalCueDirection(
        FMath::Sin(BearingRadians) * CosElevation,
        -FMath::Cos(BearingRadians) * CosElevation,
        FMath::Sin(ElevationRadians));
    const FVector WorldCueDirection = GetActorTransform().TransformVectorNoScale(LocalCueDirection).GetSafeNormal();
    if (!WorldCueDirection.IsNearlyZero())
    {
        DesiredPTZWorldRotation = WorldCueDirection.Rotation();
    }

    if (bNewCue)
    {
        RadarCueStartSimulationSeconds = SimulationSeconds;
        PTZSlewState = EPTZSlewState::Slewing;
        PTZSettleStartSimulationSeconds = 0.0;
        LastPTZCaptureSimulationSeconds = -TNumericLimits<double>::Max();
        LatestPTZConfirmations.Reset();
    }

    const FTRIADGeodeticSensorNode& Definition = SensorNode->Definition;
    auto AddPendingConfirmation = [this, Target, ContactId, SimulationSeconds, &Definition](
        const FString& SensorSuffix,
        const FString& SensorType,
        const FString& Modality,
        double FieldOfViewDegrees,
        bool bSyntheticThermal)
    {
        FTRIADPTZConfirmationResult Pending;
        Pending.TimestampUtc = FDateTime::UtcNow().ToIso8601();
        Pending.NodeId = Definition.NodeId;
        Pending.SensorId = Definition.NodeId + SensorSuffix;
        Pending.SensorType = SensorType;
        Pending.Modality = Modality;
        Pending.ContactId = ContactId;
        Pending.TrackId = CurrentRadarTrackId;
        Pending.TargetActor = Target->GetName();
        Pending.RadarCueSensorId = Definition.NodeId + TEXT(":SEARCH_RADAR");
        Pending.SlewState = TEXT("SLEWING");
        Pending.SimulationSeconds = SimulationSeconds;
        Pending.CueAgeSeconds = FMath::Max(SimulationSeconds - LastRadarCueSimulationSeconds, 0.0);
        Pending.RangeMeters = CurrentRadarMeasuredRangeMeters;
        Pending.Confidence = 0.0;
        Pending.WeatherProfile = ActiveWeatherProfileName;
        Pending.WeatherVisibilityMeters = ActiveVisibilityMeters;
        Pending.WeatherRainRateMillimetersPerHour = ActiveRainRateMillimetersPerHour;
        Pending.WeatherConfidenceFactor = 0.0;
        Pending.WeatherConfidenceSemantics = bSyntheticThermal
            ? TEXT("thermal_proxy_factor_reduced_by_rain_rate")
            : TEXT("EO_factor_visibilityMeters_divided_by_rangeMeters_clamped_0.15_to_1");
        Pending.AzimuthDegrees = CurrentRadarMeasuredBearingDegrees;
        Pending.ElevationDegrees = CurrentRadarMeasuredElevationDegrees;
        Pending.FieldOfViewDegrees = FieldOfViewDegrees;
        Pending.ImageWidthPixels = FMath::Clamp(Definition.PTZCaptureWidth, 64, 4096);
        Pending.ImageHeightPixels = FMath::Clamp(Definition.PTZCaptureHeight, 64, 4096);
        Pending.bSyntheticThermal = bSyntheticThermal;
        Pending.ThermalSemantics = bSyntheticThermal
            ? TEXT("visible_scene_luminance_false_colour_with_analytic_target_heat_gain_NOT_a_physical_thermal_camera")
            : TEXT("");
        LatestPTZConfirmations.Add(MoveTemp(Pending));
    };
    if (bNewCue)
    {
        if (Definition.bEnableEOPTZ)
        {
            AddPendingConfirmation(
                TEXT(":EO_PTZ"),
                TEXT("EO_PTZ"),
                TEXT("EO_VISIBLE"),
                Definition.EOPTZFieldOfViewDegrees,
                false);
        }
        if (Definition.bEnableThermalPTZ)
        {
            AddPendingConfirmation(
                TEXT(":THERMAL_PTZ"),
                TEXT("THERMAL_PTZ"),
                TEXT("THERMAL_SYNTHETIC"),
                Definition.ThermalPTZFieldOfViewDegrees,
                true);
        }
    }
    else
    {
        for (FTRIADPTZConfirmationResult& Pending : LatestPTZConfirmations)
        {
            Pending.ContactId = CurrentSensorContactId;
            if (Pending.FrameRelativePath.IsEmpty())
            {
                // Pending records have no captured image timestamp yet, so keep
                // their metadata current while the mount slews and settles.
                Pending.TimestampUtc = FDateTime::UtcNow().ToIso8601();
                Pending.SimulationSeconds = SimulationSeconds;
                Pending.WeatherProfile = ActiveWeatherProfileName;
                Pending.WeatherVisibilityMeters = ActiveVisibilityMeters;
                Pending.WeatherRainRateMillimetersPerHour = ActiveRainRateMillimetersPerHour;
            }

            // Range/angles describe the live radar cue, not the older image
            // capture. Refresh them even between PTZ captures so a moving target
            // cannot fail the fusion geometry gate solely because the PTZ cadence
            // is slower than the search-radar cadence.
            Pending.CueAgeSeconds = FMath::Max(SimulationSeconds - LastRadarCueSimulationSeconds, 0.0);
            Pending.RangeMeters = CurrentRadarMeasuredRangeMeters;
            Pending.AzimuthDegrees = CurrentRadarMeasuredBearingDegrees;
            Pending.ElevationDegrees = CurrentRadarMeasuredElevationDegrees;
        }
    }
}

void ATRIADSensorNodeActor::ClearLongRangeCue()
{
    CurrentRadarCueTarget.Reset();
    CurrentSensorContactId.Reset();
    CurrentRadarTrackId.Reset();
    PTZSlewState = EPTZSlewState::Idle;
    LastRadarCueSimulationSeconds = 0.0;
    LatestPTZConfirmations.Reset();
}

void ATRIADSensorNodeActor::GetLatestPTZConfirmations(
    TArray<FTRIADPTZConfirmationResult>& OutConfirmations) const
{
    FString CurrentSlewState = TEXT("IDLE");
    switch (PTZSlewState)
    {
    case EPTZSlewState::Slewing: CurrentSlewState = TEXT("SLEWING"); break;
    case EPTZSlewState::Settling: CurrentSlewState = TEXT("SETTLING"); break;
    case EPTZSlewState::Settled: CurrentSlewState = TEXT("SETTLED"); break;
    case EPTZSlewState::Idle:
    default: break;
    }

    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    for (const FTRIADPTZConfirmationResult& Stored : LatestPTZConfirmations)
    {
        FTRIADPTZConfirmationResult Result = Stored;
        Result.SlewState = CurrentSlewState;
        Result.CueAgeSeconds = FMath::Max(NowSeconds - LastRadarCueSimulationSeconds, 0.0);
        OutConfirmations.Add(MoveTemp(Result));
    }
}

bool ATRIADSensorNodeActor::TryGetFreshOperatorEOView(
    const FString& TrackId,
    double MaximumCueAgeSeconds,
    UTextureRenderTarget2D*& OutRenderTarget,
    FTRIADPTZConfirmationResult& OutConfirmation) const
{
    OutRenderTarget = nullptr;
    OutConfirmation = FTRIADPTZConfirmationResult();
    if (TrackId.IsEmpty() || CurrentRadarTrackId != TrackId || !EOPTZRenderTarget ||
        PTZSlewState != EPTZSlewState::Settled)
    {
        return false;
    }

    TArray<FTRIADPTZConfirmationResult> Confirmations;
    GetLatestPTZConfirmations(Confirmations);
    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const double FreshnessSeconds = FMath::Max(MaximumCueAgeSeconds, 0.1);
    for (const FTRIADPTZConfirmationResult& Confirmation : Confirmations)
    {
        const double FrameAgeSeconds = FMath::Max(NowSeconds - Confirmation.SimulationSeconds, 0.0);
        if (Confirmation.TrackId == TrackId &&
            Confirmation.Modality == TEXT("EO_VISIBLE") &&
            Confirmation.bConfirmed &&
            Confirmation.bLineOfSight &&
            Confirmation.CueAgeSeconds <= FreshnessSeconds &&
            FrameAgeSeconds <= FreshnessSeconds)
        {
            OutRenderTarget = EOPTZRenderTarget;
            OutConfirmation = Confirmation;
            return true;
        }
    }
    return false;
}

void ATRIADSensorNodeActor::UpdateLongRangePTZ(float DeltaSeconds)
{
    AActor* Target = CurrentRadarCueTarget.Get();
    if (!IsValid(Target) || !SensorNode || PTZSlewState == EPTZSlewState::Idle)
    {
        if (!IsValid(Target) && !CurrentRadarTrackId.IsEmpty())
        {
            ClearLongRangeCue();
        }
        return;
    }

    const FTRIADGeodeticSensorNode& Definition = SensorNode->Definition;
    const FRotator CurrentRotation = EOPTZCapture
        ? EOPTZCapture->GetComponentRotation()
        : ThermalPTZCapture->GetComponentRotation();
    const FRotator NextRotation = FMath::RInterpConstantTo(
        CurrentRotation,
        DesiredPTZWorldRotation,
        DeltaSeconds,
        FMath::Max(Definition.PTZSlewRateDegreesPerSecond, 1.0f));
    if (EOPTZCapture)
    {
        EOPTZCapture->SetWorldRotation(NextRotation);
    }
    if (ThermalPTZCapture)
    {
        ThermalPTZCapture->SetWorldRotation(NextRotation);
    }

    const double YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(NextRotation.Yaw, DesiredPTZWorldRotation.Yaw));
    const double PitchError = FMath::Abs(FMath::FindDeltaAngleDegrees(NextRotation.Pitch, DesiredPTZWorldRotation.Pitch));
    const double PointingErrorDegrees = FMath::Max(YawError, PitchError);
    constexpr double PTZPointingToleranceDegrees = 0.10;
    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    // A continuing track can still move materially between radar samples. Never
    // leave the mount marked settled when the latest cue has moved off boresight.
    if ((PTZSlewState == EPTZSlewState::Settled || PTZSlewState == EPTZSlewState::Settling) &&
        PointingErrorDegrees > PTZPointingToleranceDegrees)
    {
        PTZSlewState = EPTZSlewState::Slewing;
        PTZSettleStartSimulationSeconds = 0.0;
    }
    if (PTZSlewState == EPTZSlewState::Slewing &&
        PointingErrorDegrees <= PTZPointingToleranceDegrees)
    {
        PTZSlewState = EPTZSlewState::Settling;
        PTZSettleStartSimulationSeconds = NowSeconds;
    }
    if (PTZSlewState == EPTZSlewState::Settling &&
        NowSeconds - PTZSettleStartSimulationSeconds >= FMath::Max(Definition.PTZSettleSeconds, 0.0f))
    {
        PTZSlewState = EPTZSlewState::Settled;
    }
    if (PTZSlewState == EPTZSlewState::Settled &&
        PointingErrorDegrees <= PTZPointingToleranceDegrees &&
        NowSeconds - LastPTZCaptureSimulationSeconds >= FMath::Max(Definition.PTZCaptureCadenceSeconds, 0.1f))
    {
        LastPTZCaptureSimulationSeconds = NowSeconds;
        CaptureLongRangeConfirmation();
    }

    FString CurrentSlewState = PTZSlewState == EPTZSlewState::Slewing ? TEXT("SLEWING") :
        PTZSlewState == EPTZSlewState::Settling ? TEXT("SETTLING") :
        PTZSlewState == EPTZSlewState::Settled ? TEXT("SETTLED") : TEXT("IDLE");
    for (FTRIADPTZConfirmationResult& Result : LatestPTZConfirmations)
    {
        Result.SlewState = CurrentSlewState;
        Result.CueAgeSeconds = FMath::Max(NowSeconds - LastRadarCueSimulationSeconds, 0.0);
    }
}

void ATRIADSensorNodeActor::InitializeLongRangeCaptureTargets()
{
    if (!SensorNode)
    {
        return;
    }
    const FTRIADGeodeticSensorNode& Definition = SensorNode->Definition;
    const int32 Width = FMath::Clamp(Definition.PTZCaptureWidth, 64, 4096);
    const int32 Height = FMath::Clamp(Definition.PTZCaptureHeight, 64, 4096);

    if (Definition.bEnableEOPTZ)
    {
        EOPTZRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("TRIAD_EO_PTZ_RenderTarget"));
        EOPTZRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
        EOPTZRenderTarget->ClearColor = FLinearColor::Black;
        EOPTZRenderTarget->InitAutoFormat(Width, Height);
        EOPTZRenderTarget->UpdateResourceImmediate(true);
        EOPTZCapture->TextureTarget = EOPTZRenderTarget;
    }
    if (Definition.bEnableThermalPTZ)
    {
        ThermalPTZRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("TRIAD_Thermal_PTZ_RenderTarget"));
        ThermalPTZRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
        ThermalPTZRenderTarget->ClearColor = FLinearColor::Black;
        ThermalPTZRenderTarget->InitAutoFormat(Width, Height);
        ThermalPTZRenderTarget->UpdateResourceImmediate(true);
        ThermalPTZCapture->TextureTarget = ThermalPTZRenderTarget;
    }

    const FString SanitizedNodeId = FPaths::MakeValidFileName(Definition.NodeId, TEXT('_'));
    IFileManager::Get().MakeDirectory(
        *FPaths::Combine(LongRangeTelemetryRootDirectory, TEXT("RadarPtzFrames"), TEXT("eo"), SanitizedNodeId),
        true);
    IFileManager::Get().MakeDirectory(
        *FPaths::Combine(LongRangeTelemetryRootDirectory, TEXT("RadarPtzFrames"), TEXT("thermal"), SanitizedNodeId),
        true);
    bDebugVisualCaptureFilterActive = RefreshCaptureExclusionList();
}

bool ATRIADSensorNodeActor::ProjectTargetIntoCapture(
    const USceneCaptureComponent2D* Capture,
    const AActor* Target,
    int32 Width,
    int32 Height,
    double& OutMinimumX,
    double& OutMinimumY,
    double& OutMaximumX,
    double& OutMaximumY) const
{
    if (!Capture || !IsValid(Target) || Width <= 0 || Height <= 0)
    {
        return false;
    }

    const FBox ComponentBounds = GetModelFacingPrimitiveBounds(Target);
    TArray<FVector, TInlineAllocator<8>> WorldCorners;
    if (ComponentBounds.IsValid != 0)
    {
        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            WorldCorners.Add(FVector(
                (CornerIndex & 1) ? ComponentBounds.Max.X : ComponentBounds.Min.X,
                (CornerIndex & 2) ? ComponentBounds.Max.Y : ComponentBounds.Min.Y,
                (CornerIndex & 4) ? ComponentBounds.Max.Z : ComponentBounds.Min.Z));
        }
    }
    else
    {
        WorldCorners.Add(Target->GetActorLocation());
    }

    const FTransform CameraTransform = Capture->GetComponentTransform();
    const double AspectRatio = static_cast<double>(Width) / static_cast<double>(Height);
    const double TanHalfHorizontalFov = FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle * 0.5));
    const double TanHalfVerticalFov = TanHalfHorizontalFov / FMath::Max(AspectRatio, 0.000001);
    int32 ProjectedCornerCount = 0;
    OutMinimumX = TNumericLimits<double>::Max();
    OutMinimumY = TNumericLimits<double>::Max();
    OutMaximumX = -TNumericLimits<double>::Max();
    OutMaximumY = -TNumericLimits<double>::Max();
    for (const FVector& WorldCorner : WorldCorners)
    {
        const FVector CameraLocal = CameraTransform.InverseTransformPosition(WorldCorner);
        if (CameraLocal.X <= 1.0)
        {
            continue;
        }
        ++ProjectedCornerCount;
        const double NormalizedX = CameraLocal.Y / (CameraLocal.X * TanHalfHorizontalFov);
        const double NormalizedY = CameraLocal.Z / (CameraLocal.X * TanHalfVerticalFov);
        const double PixelX = (NormalizedX * 0.5 + 0.5) * static_cast<double>(Width);
        const double PixelY = (0.5 - NormalizedY * 0.5) * static_cast<double>(Height);
        OutMinimumX = FMath::Min(OutMinimumX, PixelX);
        OutMinimumY = FMath::Min(OutMinimumY, PixelY);
        OutMaximumX = FMath::Max(OutMaximumX, PixelX);
        OutMaximumY = FMath::Max(OutMaximumY, PixelY);
    }

    const bool bIntersectsFrame = ProjectedCornerCount > 0 &&
        OutMaximumX >= 0.0 && OutMaximumY >= 0.0 &&
        OutMinimumX <= static_cast<double>(Width - 1) &&
        OutMinimumY <= static_cast<double>(Height - 1);
    if (bIntersectsFrame)
    {
        OutMinimumX = FMath::Clamp(OutMinimumX, 0.0, static_cast<double>(Width - 1));
        OutMinimumY = FMath::Clamp(OutMinimumY, 0.0, static_cast<double>(Height - 1));
        OutMaximumX = FMath::Clamp(OutMaximumX, 0.0, static_cast<double>(Width - 1));
        OutMaximumY = FMath::Clamp(OutMaximumY, 0.0, static_cast<double>(Height - 1));
    }
    return bIntersectsFrame;
}

bool ATRIADSensorNodeActor::ComputePTZLineOfSight(
    const USceneCaptureComponent2D* Capture,
    const AActor* Target,
    FString& OutBlockingActor) const
{
    OutBlockingActor.Reset();
    const UWorld* World = GetWorld();
    if (!World || !Capture || !IsValid(Target) || !SensorNode)
    {
        OutBlockingActor = TEXT("PTZ_LOS_UNAVAILABLE");
        return false;
    }

    TArray<FVector, TInlineAllocator<9>> AimPoints;
    const FBox Bounds = GetModelFacingPrimitiveBounds(Target);
    AimPoints.Add(Bounds.IsValid != 0 ? Bounds.GetCenter() : Target->GetActorLocation());
    if (Bounds.IsValid != 0)
    {
        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            AimPoints.Add(FVector(
                (CornerIndex & 1) ? Bounds.Max.X : Bounds.Min.X,
                (CornerIndex & 2) ? Bounds.Max.Y : Bounds.Min.Y,
                (CornerIndex & 4) ? Bounds.Max.Z : Bounds.Min.Z));
        }
    }

    const FTRIADGeodeticSensorNode& Definition = SensorNode->Definition;
    FCollisionQueryParams QueryParameters(SCENE_QUERY_STAT(TRIADPTZConfirmationLOS), Definition.bPTZTraceComplex);
    QueryParameters.AddIgnoredActor(this);
    const int32 MaximumChannel =
        static_cast<int32>(ECC_GameTraceChannel18);
    const ECollisionChannel TraceChannel = static_cast<ECollisionChannel>(
        FMath::Clamp(Definition.PTZLineOfSightTraceChannel, 0, MaximumChannel));
    const FVector CaptureOrigin = Capture->GetComponentLocation();
    for (const FVector& AimPoint : AimPoints)
    {
        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(
            Hit,
            CaptureOrigin,
            AimPoint,
            TraceChannel,
            QueryParameters);
        if (!bHit || Hit.GetActor() == Target)
        {
            return true;
        }
        if (OutBlockingActor.IsEmpty())
        {
            OutBlockingActor = Hit.GetActor()
                ? Hit.GetActor()->GetName()
                : Hit.Component.IsValid() ? Hit.Component->GetName() : TEXT("UnknownGeometry");
        }
    }
    if (OutBlockingActor.IsEmpty())
    {
        OutBlockingActor = TEXT("UnknownGeometry");
    }
    return false;
}

void ATRIADSensorNodeActor::CaptureLongRangeConfirmation()
{
    AActor* Target = CurrentRadarCueTarget.Get();
    if (!IsValid(Target) || !SensorNode)
    {
        return;
    }
    const FTRIADGeodeticSensorNode& Definition = SensorNode->Definition;
    const double SimulationSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const FString TimestampUtc = FDateTime::UtcNow().ToIso8601();
    const FString RadarSensorId = Definition.NodeId + TEXT(":SEARCH_RADAR");
    LatestPTZConfirmations.Reset();
    bool bCaptureFilterAttempted = false;
    bool bCaptureFilterReady = false;

    auto CaptureModality = [
        this,
        Target,
        &Definition,
        SimulationSeconds,
        &TimestampUtc,
        &RadarSensorId,
        &bCaptureFilterAttempted,
        &bCaptureFilterReady](
        USceneCaptureComponent2D* Capture,
        UTextureRenderTarget2D* RenderTarget,
        const FString& SensorSuffix,
        const FString& SensorType,
        const FString& Modality,
        const FString& DirectoryName,
        double ConfirmationRangeMeters,
        bool bSyntheticThermal)
    {
        if (!Capture || !RenderTarget)
        {
            return;
        }

        const int32 Width = RenderTarget->SizeX;
        const int32 Height = RenderTarget->SizeY;
        FTRIADPTZConfirmationResult Result;
        Result.TimestampUtc = TimestampUtc;
        Result.SimulationSeconds = SimulationSeconds;
        Result.NodeId = Definition.NodeId;
        Result.SensorId = Definition.NodeId + SensorSuffix;
        Result.SensorType = SensorType;
        Result.Modality = Modality;
        Result.ContactId = CurrentSensorContactId;
        Result.TrackId = CurrentRadarTrackId;
        Result.TargetActor = Target->GetName();
        Result.RadarCueSensorId = RadarSensorId;
        Result.SlewState = TEXT("SETTLED");
        Result.CueAgeSeconds = FMath::Max(SimulationSeconds - LastRadarCueSimulationSeconds, 0.0);
        Result.RangeMeters = CurrentRadarMeasuredRangeMeters;
        Result.AzimuthDegrees = CurrentRadarMeasuredBearingDegrees;
        Result.ElevationDegrees = CurrentRadarMeasuredElevationDegrees;
        Result.FieldOfViewDegrees = Capture->FOVAngle;
        Result.ImageWidthPixels = Width;
        Result.ImageHeightPixels = Height;
        Result.bSyntheticThermal = bSyntheticThermal;
        Result.WeatherProfile = ActiveWeatherProfileName;
        Result.WeatherVisibilityMeters = ActiveVisibilityMeters;
        Result.WeatherRainRateMillimetersPerHour = ActiveRainRateMillimetersPerHour;
        Result.ThermalSemantics = bSyntheticThermal
            ? TEXT("visible_scene_luminance_false_colour_with_analytic_target_heat_gain_NOT_a_physical_thermal_camera")
            : TEXT("");
        Result.WeatherConfidenceFactor = bSyntheticThermal
            ? FMath::Clamp(1.0 - ActiveRainRateMillimetersPerHour / 250.0, 0.55, 1.0)
            : FMath::Clamp(ActiveVisibilityMeters / FMath::Max(Result.RangeMeters, 1.0), 0.15, 1.0);
        Result.WeatherConfidenceSemantics = bSyntheticThermal
            ? TEXT("thermal_proxy_factor=max(0.55,1-rainRateMillimetersPerHour/250)")
            : TEXT("EO_factor=clamp(visibilityMeters/rangeMeters,0.15,1)");

        // Keep the far-range radar cue/status cheap. GPU capture, readback, PNG
        // compression, and publication begin only inside this modality's own
        // confirmation envelope.
        if (Result.RangeMeters > FMath::Max(ConfirmationRangeMeters, 500.0))
        {
            Result.Confidence = 0.0;
            Result.bConfirmed = false;
            Result.bLineOfSight = false;
            Result.BlockingActor = TEXT("NOT_EVALUATED_OUTSIDE_CONFIRMATION_RANGE");
            Result.OcclusionSemantics =
                TEXT("not_evaluated_outside_modality_confirmation_range_no_capture_or_readback");
            LatestPTZConfirmations.Add(MoveTemp(Result));
            return;
        }

        if (!bCaptureFilterAttempted)
        {
            bCaptureFilterAttempted = true;
            bCaptureFilterReady = RefreshCaptureExclusionList();
        }
        if (!bCaptureFilterReady)
        {
            Result.Confidence = 0.0;
            Result.bConfirmed = false;
            Result.bLineOfSight = false;
            Result.BlockingActor = TEXT("CAPTURE_FILTER_UNAVAILABLE");
            Result.OcclusionSemantics =
                TEXT("capture_suppressed_because_human_debug_exclusion_filter_was_unavailable");
            LatestPTZConfirmations.Add(MoveTemp(Result));
            return;
        }

        Capture->CaptureScene();
        FRenderTarget* Resource = RenderTarget->GameThread_GetRenderTargetResource();
        TArray<FColor> Pixels;
        if (!Resource || !Resource->ReadPixels(Pixels))
        {
            UE_LOG(LogTemp, Warning, TEXT("TRIAD %s capture read failed at node '%s'."), *SensorType, *Definition.NodeId);
            return;
        }
        if (Pixels.Num() != Width * Height)
        {
            return;
        }

        const bool bIntersectsFrame = ProjectTargetIntoCapture(
            Capture,
            Target,
            Width,
            Height,
            Result.BoundingBoxMinimumX,
            Result.BoundingBoxMinimumY,
            Result.BoundingBoxMaximumX,
            Result.BoundingBoxMaximumY);
        Result.bLineOfSight = ComputePTZLineOfSight(Capture, Target, Result.BlockingActor);
        Result.OcclusionSemantics =
            TEXT("fail_closed_visibility_trace_from_PTZ_capture_origin_to_model_bounds_center_and_corners");
        Result.PixelExtentWidth = bIntersectsFrame
            ? FMath::Max(Result.BoundingBoxMaximumX - Result.BoundingBoxMinimumX, 0.0)
            : 0.0;
        Result.PixelExtentHeight = bIntersectsFrame
            ? FMath::Max(Result.BoundingBoxMaximumY - Result.BoundingBoxMinimumY, 0.0)
            : 0.0;
        const double PixelSupport = FMath::Clamp(
            FMath::Max(Result.PixelExtentWidth, Result.PixelExtentHeight) / 3.0,
            0.0,
            1.0);
        const double WeatherFactor = bSyntheticThermal
            ? FMath::Clamp(1.0 - ActiveRainRateMillimetersPerHour / 250.0, 0.55, 1.0)
            : FMath::Clamp(ActiveVisibilityMeters / FMath::Max(Result.RangeMeters, 1.0), 0.15, 1.0);
        Result.WeatherConfidenceFactor = WeatherFactor;
        Result.Confidence = FMath::Clamp(CurrentRadarConfidence * PixelSupport * WeatherFactor, 0.0, 1.0);
        Result.bConfirmed = bIntersectsFrame && Result.bLineOfSight &&
            Result.RangeMeters <= FMath::Max(ConfirmationRangeMeters, 500.0) &&
            FMath::Min(Result.PixelExtentWidth, Result.PixelExtentHeight) >= 2.0;

        if (bSyntheticThermal)
        {
            const int32 BoxMinimumX = FMath::Clamp(FMath::FloorToInt(Result.BoundingBoxMinimumX), 0, Width - 1);
            const int32 BoxMinimumY = FMath::Clamp(FMath::FloorToInt(Result.BoundingBoxMinimumY), 0, Height - 1);
            const int32 BoxMaximumX = FMath::Clamp(FMath::CeilToInt(Result.BoundingBoxMaximumX), 0, Width - 1);
            const int32 BoxMaximumY = FMath::Clamp(FMath::CeilToInt(Result.BoundingBoxMaximumY), 0, Height - 1);
            for (int32 Y = 0; Y < Height; ++Y)
            {
                for (int32 X = 0; X < Width; ++X)
                {
                    const FColor Source = Pixels[Y * Width + X];
                    int32 Intensity = FMath::RoundToInt(
                        0.2126 * Source.R + 0.7152 * Source.G + 0.0722 * Source.B);
                    if (bIntersectsFrame && Result.bLineOfSight && X >= BoxMinimumX && X <= BoxMaximumX &&
                        Y >= BoxMinimumY && Y <= BoxMaximumY)
                    {
                        Intensity = FMath::Min(Intensity + 55, 255);
                    }
                    Pixels[Y * Width + X] = SimulatedThermalPalette(static_cast<uint8>(Intensity));
                }
            }
            DrawSyntheticThermalLabel(Pixels, Width, Height);
        }

        // `latest.*` is a last-positive-confirmation artifact, never a raw
        // capture slot.  In particular, an off-boresight/occluded capture must
        // not replace the useful operator image with a blank frame.  The
        // current unconfirmed status record is still exported below, but with
        // no path, so consumers cannot mistake the older retained file for
        // evidence from this sample.
        if (Result.bConfirmed)
        {
            FString FrameRelativePath;
            FString MetadataRelativePath;
            if (PublishLongRangeFrame(
                    DirectoryName,
                    Pixels,
                    Result,
                    bSyntheticThermal,
                    FrameRelativePath,
                    MetadataRelativePath))
            {
                Result.FrameRelativePath = MoveTemp(FrameRelativePath);
                Result.MetadataRelativePath = MoveTemp(MetadataRelativePath);
            }
        }
        else
        {
            // Be explicit even though a newly constructed result starts empty;
            // this is a safety invariant if the record type is later reused.
            Result.FrameRelativePath.Reset();
            Result.MetadataRelativePath.Reset();
        }
        LatestPTZConfirmations.Add(MoveTemp(Result));
    };

    if (Definition.bEnableEOPTZ)
    {
        CaptureModality(
            EOPTZCapture,
            EOPTZRenderTarget,
            TEXT(":EO_PTZ"),
            TEXT("EO_PTZ"),
            TEXT("EO_VISIBLE"),
            TEXT("eo"),
            Definition.EOPTZConfirmationRangeMeters,
            false);
    }
    if (Definition.bEnableThermalPTZ)
    {
        CaptureModality(
            ThermalPTZCapture,
            ThermalPTZRenderTarget,
            TEXT(":THERMAL_PTZ"),
            TEXT("THERMAL_PTZ"),
            TEXT("THERMAL_SYNTHETIC"),
            TEXT("thermal"),
            Definition.ThermalPTZConfirmationRangeMeters,
            true);
    }

    const FTRIADPTZConfirmationResult* ConfirmedResult = LatestPTZConfirmations.FindByPredicate(
        [](const FTRIADPTZConfirmationResult& Result)
        {
            return Result.bConfirmed;
        });
    if (ConfirmedResult && Definition.bVisualizeLongRangeSensorCue && GetWorld())
    {
        const FBox TargetBounds = GetModelFacingPrimitiveBounds(Target);
        const FVector Center = TargetBounds.IsValid != 0 ? TargetBounds.GetCenter() : Target->GetActorLocation();
        const FVector Extent = TargetBounds.IsValid != 0
            ? TargetBounds.GetExtent().ComponentMax(FVector(80.0))
            : FVector(180.0);
        const float LifeTime = FMath::Max(Definition.LongRangeVisualizationSeconds, 0.05f);
        DrawDebugBox(GetWorld(), Center, Extent, FColor(90, 255, 90), false, LifeTime, 3, 5.0f);
        UE_LOG(
            LogTemp,
            VeryVerbose,
            TEXT("[SIM DEBUG PTZ CONFIRMED] %s | %.0f m | projection conf %.2f"),
            *ConfirmedResult->TrackId,
            ConfirmedResult->RangeMeters,
            ConfirmedResult->Confidence);
    }
}

bool ATRIADSensorNodeActor::PublishLongRangeFrame(
    const FString& ModalityDirectoryName,
    const TArray<FColor>& Pixels,
    const FTRIADPTZConfirmationResult& Result,
    bool bSyntheticThermal,
    FString& OutFrameRelativePath,
    FString& OutMetadataRelativePath) const
{
    if (!Result.bConfirmed || LongRangeTelemetryRootDirectory.IsEmpty() || Pixels.Num() == 0 ||
        Result.ImageWidthPixels <= 0 || Result.ImageHeightPixels <= 0)
    {
        return false;
    }
    const FString SanitizedNodeId = FPaths::MakeValidFileName(Result.NodeId, TEXT('_'));
    FString RelativeDirectory = FPaths::Combine(TEXT("RadarPtzFrames"), ModalityDirectoryName, SanitizedNodeId);
    RelativeDirectory.ReplaceInline(TEXT("\\"), TEXT("/"));
    OutFrameRelativePath = RelativeDirectory + TEXT("/latest.png");
    OutMetadataRelativePath = RelativeDirectory + TEXT("/latest.json");
    const FString AbsoluteDirectory = FPaths::Combine(
        LongRangeTelemetryRootDirectory,
        TEXT("RadarPtzFrames"),
        ModalityDirectoryName,
        SanitizedNodeId);
    IFileManager::Get().MakeDirectory(*AbsoluteDirectory, true);
    const FString FramePath = FPaths::Combine(AbsoluteDirectory, TEXT("latest.png"));
    const FString MetadataPath = FPaths::Combine(AbsoluteDirectory, TEXT("latest.json"));

    TArray64<uint8> PngBytes;
    FImageUtils::PNGCompressImageArray(
        Result.ImageWidthPixels,
        Result.ImageHeightPixels,
        TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()),
        PngBytes);

    TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
    Metadata->SetStringField(TEXT("schemaVersion"), TEXT("triad.radar_cued_ptz_frame.v1"));
    Metadata->SetStringField(TEXT("kind"), TEXT("SIMULATED_SENSOR_CONFIRMATION"));
    Metadata->SetStringField(TEXT("source"), TEXT("SIMULATION_PROJECTION"));
    Metadata->SetStringField(TEXT("confirmationMethod"), TEXT("SIMULATION_PROJECTION_TRUTH"));
    Metadata->SetStringField(TEXT("boxSource"), TEXT("DEBUG_PROJECTION"));
    Metadata->SetBoolField(TEXT("simulated"), true);
    Metadata->SetBoolField(TEXT("calibratedDetector"), false);
    Metadata->SetStringField(TEXT("timestampUtc"), Result.TimestampUtc);
    Metadata->SetNumberField(TEXT("simulationSeconds"), Result.SimulationSeconds);
    Metadata->SetStringField(TEXT("nodeId"), Result.NodeId);
    Metadata->SetStringField(TEXT("sensorId"), Result.SensorId);
    Metadata->SetStringField(TEXT("sensorType"), Result.SensorType);
    Metadata->SetStringField(TEXT("modality"), Result.Modality);
    Metadata->SetStringField(TEXT("contactId"), Result.ContactId);
    Metadata->SetStringField(TEXT("trackId"), Result.TrackId);
    Metadata->SetStringField(TEXT("targetActor"), Result.TargetActor);
    Metadata->SetStringField(TEXT("radarCueSensorId"), Result.RadarCueSensorId);
    Metadata->SetStringField(TEXT("slewState"), Result.SlewState);
    Metadata->SetBoolField(TEXT("confirmed"), Result.bConfirmed);
    Metadata->SetBoolField(TEXT("lineOfSight"), Result.bLineOfSight);
    Metadata->SetStringField(TEXT("blockingActor"), Result.BlockingActor);
    Metadata->SetStringField(TEXT("occlusionSemantics"), Result.OcclusionSemantics);
    Metadata->SetNumberField(TEXT("rangeMeters"), Result.RangeMeters);
    Metadata->SetNumberField(TEXT("confidence"), Result.Confidence);
    Metadata->SetStringField(TEXT("weatherProfile"), Result.WeatherProfile);
    Metadata->SetNumberField(TEXT("weatherVisibilityMeters"), Result.WeatherVisibilityMeters);
    Metadata->SetNumberField(
        TEXT("weatherRainRateMillimetersPerHour"),
        Result.WeatherRainRateMillimetersPerHour);
    Metadata->SetNumberField(TEXT("weatherConfidenceFactor"), Result.WeatherConfidenceFactor);
    Metadata->SetStringField(TEXT("weatherConfidenceSemantics"), Result.WeatherConfidenceSemantics);
    Metadata->SetNumberField(TEXT("azimuthDeg"), Result.AzimuthDegrees);
    Metadata->SetNumberField(TEXT("elevationDeg"), Result.ElevationDegrees);
    Metadata->SetNumberField(TEXT("horizontalFovDeg"), Result.FieldOfViewDegrees);
    Metadata->SetNumberField(TEXT("width"), Result.ImageWidthPixels);
    Metadata->SetNumberField(TEXT("height"), Result.ImageHeightPixels);
    Metadata->SetStringField(TEXT("relativePath"), OutFrameRelativePath);
    Metadata->SetStringField(TEXT("frameRelativePath"), OutFrameRelativePath);
    Metadata->SetStringField(TEXT("metadataRelativePath"), OutMetadataRelativePath);
    Metadata->SetBoolField(TEXT("syntheticThermal"), bSyntheticThermal);
    Metadata->SetStringField(TEXT("thermalSemantics"), Result.ThermalSemantics);
    Metadata->SetStringField(
        TEXT("pixelSemantics"),
        bSyntheticThermal
            ? TEXT("synthetic_false_colour_postprocess_of_Unreal_SceneCapture_pixels")
            : TEXT("raw_Unreal_SceneCapture_visible_pixels_without_operator_debug_aids"));

    TSharedRef<FJsonObject> Reticle = MakeShared<FJsonObject>();
    Reticle->SetNumberField(TEXT("x"), Result.ImageWidthPixels * 0.5);
    Reticle->SetNumberField(TEXT("y"), Result.ImageHeightPixels * 0.5);
    Reticle->SetStringField(TEXT("coordinateSpace"), TEXT("pixels"));
    Metadata->SetObjectField(TEXT("reticle"), MoveTemp(Reticle));

    TArray<TSharedPtr<FJsonValue>> Boxes;
    if (Result.BoundingBoxMaximumX > Result.BoundingBoxMinimumX &&
        Result.BoundingBoxMaximumY > Result.BoundingBoxMinimumY)
    {
        TSharedRef<FJsonObject> Box = MakeShared<FJsonObject>();
        Box->SetStringField(TEXT("kind"), TEXT("SIMULATED_SENSOR_CONFIRMATION"));
        Box->SetStringField(TEXT("source"), TEXT("SIMULATION_PROJECTION"));
        Box->SetStringField(TEXT("label"), TEXT("SIMULATED SENSOR CONFIRMATION"));
        Box->SetBoolField(TEXT("simulated"), true);
        TArray<TSharedPtr<FJsonValue>> PixelCoordinates;
        PixelCoordinates.Add(MakeShared<FJsonValueNumber>(Result.BoundingBoxMinimumX));
        PixelCoordinates.Add(MakeShared<FJsonValueNumber>(Result.BoundingBoxMinimumY));
        PixelCoordinates.Add(MakeShared<FJsonValueNumber>(Result.BoundingBoxMaximumX));
        PixelCoordinates.Add(MakeShared<FJsonValueNumber>(Result.BoundingBoxMaximumY));
        Box->SetArrayField(TEXT("xyxyPixels"), PixelCoordinates);
        TArray<TSharedPtr<FJsonValue>> NormalizedCoordinates;
        NormalizedCoordinates.Add(MakeShared<FJsonValueNumber>(Result.BoundingBoxMinimumX / Result.ImageWidthPixels));
        NormalizedCoordinates.Add(MakeShared<FJsonValueNumber>(Result.BoundingBoxMinimumY / Result.ImageHeightPixels));
        NormalizedCoordinates.Add(MakeShared<FJsonValueNumber>(Result.BoundingBoxMaximumX / Result.ImageWidthPixels));
        NormalizedCoordinates.Add(MakeShared<FJsonValueNumber>(Result.BoundingBoxMaximumY / Result.ImageHeightPixels));
        Box->SetArrayField(TEXT("xyxyNormalized"), NormalizedCoordinates);
        Boxes.Add(MakeShared<FJsonValueObject>(MoveTemp(Box)));
    }
    Metadata->SetArrayField(TEXT("boxes"), Boxes);

    FString MetadataString;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetadataString);
    FJsonSerializer::Serialize(Metadata, Writer);
    const FString FrameTemporaryPath = FramePath + TEXT(".tmp");
    const FString MetadataTemporaryPath = MetadataPath + TEXT(".tmp");
    const bool bFrameStaged = FFileHelper::SaveArrayToFile(PngBytes, *FrameTemporaryPath);
    const bool bMetadataStaged = FFileHelper::SaveStringToFile(
        MetadataString,
        *MetadataTemporaryPath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    const bool bFramePublished = bFrameStaged && AtomicallyPublishLatestFile(FrameTemporaryPath, FramePath);
    const bool bMetadataPublished = bMetadataStaged && AtomicallyPublishLatestFile(MetadataTemporaryPath, MetadataPath);
    if (!bFramePublished || !bMetadataPublished)
    {
        IFileManager::Get().Delete(*FrameTemporaryPath, false, true);
        IFileManager::Get().Delete(*MetadataTemporaryPath, false, true);
        UE_LOG(LogTemp, Warning, TEXT("TRIAD could not publish bounded latest %s frame for node '%s'."), *Result.SensorType, *Result.NodeId);
        return false;
    }
    return true;
}

void ATRIADSensorNodeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(CaptureTimerHandle);
    Super::EndPlay(EndPlayReason);
}

void ATRIADSensorNodeActor::InitializeCaptureTargets()
{
    const FTRIADGeodeticSensorNode& Definition = SensorNode->Definition;
    const int32 Width = FMath::Clamp(Definition.CameraCaptureWidth, 16, 4096);
    const int32 Height = FMath::Clamp(Definition.CameraCaptureHeight, 16, 4096);

    RGBRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("TRIAD_RGB_RenderTarget"));
    RGBRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
    RGBRenderTarget->ClearColor = FLinearColor::Black;
    RGBRenderTarget->InitAutoFormat(Width, Height);
    RGBRenderTarget->UpdateResourceImmediate(true);
    RGBCapture->TextureTarget = RGBRenderTarget;

    DepthRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("TRIAD_Depth_RenderTarget"));
    DepthRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA32f;
    DepthRenderTarget->ClearColor = FLinearColor::Black;
    DepthRenderTarget->InitAutoFormat(Width, Height);
    DepthRenderTarget->UpdateResourceImmediate(true);
    DepthCapture->TextureTarget = DepthRenderTarget;
}

bool ATRIADSensorNodeActor::RefreshCaptureExclusionList()
{
    UWorld* World = GetWorld();
    if (!World || !RGBCapture || !DepthCapture || !EOPTZCapture || !ThermalPTZCapture)
    {
        bDebugVisualCaptureFilterActive = false;
        return false;
    }

    auto HideFromSensorCaptures = [this](UPrimitiveComponent* Component)
    {
        if (!IsValid(Component))
        {
            return;
        }
        RGBCapture->HideComponent(Component);
        DepthCapture->HideComponent(Component);
        EOPTZCapture->HideComponent(Component);
        ThermalPTZCapture->HideComponent(Component);
    };

    // Demo-target labels and sensor-site markers remain in the player's main
    // view, while physical drone meshes and ordinary scene geometry remain in
    // the model-facing captures.
    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(*ActorIt);
        for (UPrimitiveComponent* Component : PrimitiveComponents)
        {
            if (IsHumanOnlyOverlayComponent(Component))
            {
                HideFromSensorCaptures(Component);
            }
        }
    }

    // DrawDebugDirectionalArrow/Sphere/Circle submit through world line-batch
    // components rather than through the actors that requested them. Hide those
    // batches explicitly so RF links, range rings, and debug target spheres do
    // not become RGB/depth/event-camera features.
    for (TObjectIterator<ULineBatchComponent> LineBatchIt; LineBatchIt; ++LineBatchIt)
    {
        ULineBatchComponent* LineBatch = *LineBatchIt;
        if (IsValid(LineBatch) && LineBatch->GetWorld() == World)
        {
            HideFromSensorCaptures(LineBatch);
        }
    }

    bDebugVisualCaptureFilterActive = true;
    return true;
}

void ATRIADSensorNodeActor::CaptureCameraFrame()
{
    if (!RGBRenderTarget || !DepthRenderTarget || CapturedFrameCount >= MaximumFrameCount)
    {
        StopFrameCapture(TEXT("per-node frame-count limit reached"));
        return;
    }

    // New actors and line-batch components can appear after node creation. Do
    // not save a model-facing frame unless the exclusion filter is current.
    if (!RefreshCaptureExclusionList())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("TRIAD sensor node '%s' skipped a camera frame because debug-visual exclusion was unavailable."),
            *SensorNode->Definition.NodeId);
        return;
    }

    RGBCapture->CaptureScene();
    DepthCapture->CaptureScene();

    TArray<FColor> RGBPixels;
    TArray<FLinearColor> DepthPixels;
    FRenderTarget* RGBResource = RGBRenderTarget->GameThread_GetRenderTargetResource();
    FRenderTarget* DepthResource = DepthRenderTarget->GameThread_GetRenderTargetResource();
    if (!RGBResource || !DepthResource || !RGBResource->ReadPixels(RGBPixels) ||
        !DepthResource->ReadLinearColorPixels(DepthPixels, FReadSurfaceDataFlags(RCM_MinMax)))
    {
        UE_LOG(LogTemp, Warning, TEXT("TRIAD sensor node '%s' could not read RGB/depth render targets."), *SensorNode->Definition.NodeId);
        return;
    }

    const int32 Width = RGBRenderTarget->SizeX;
    const int32 Height = RGBRenderTarget->SizeY;
    if (RGBPixels.Num() != Width * Height || DepthPixels.Num() != Width * Height)
    {
        UE_LOG(LogTemp, Warning, TEXT("TRIAD sensor node '%s' received unexpected render-target dimensions."), *SensorNode->Definition.NodeId);
        return;
    }

    TArray<FColor> NormalizedDepthPixels;
    NormalizedDepthPixels.SetNumUninitialized(DepthPixels.Num());
    TArray64<uint8> RawDepthU32MillimeterBytes;
    RawDepthU32MillimeterBytes.SetNumUninitialized(static_cast<int64>(DepthPixels.Num()) * 4);
    constexpr uint32 RawDepthInvalidValue = 0;
    constexpr uint32 RawDepthSaturatedValue = 0xffffffffu;
    constexpr double RawDepthQuantizationMeters = 0.001;
    constexpr double RawDepthMaximumRepresentableMeters = 4294967.294;
    const double MaxDepthCentimeters = FMath::Max<double>(SensorNode->Definition.DepthNormalizationMaxMeters * 100.0, 100.0);
    double MinimumObservedMeters = TNumericLimits<double>::Max();
    double MaximumObservedMeters = 0.0;
    int32 RawDepthSaturatedPixelCount = 0;
    for (int32 PixelIndex = 0; PixelIndex < DepthPixels.Num(); ++PixelIndex)
    {
        const double CapturedDepthCentimeters = static_cast<double>(DepthPixels[PixelIndex].R);
        const bool bValidCapturedDepth = FMath::IsFinite(CapturedDepthCentimeters) && CapturedDepthCentimeters > 0.0;
        const double DepthCentimeters = bValidCapturedDepth ? CapturedDepthCentimeters : 0.0;
        const double DepthMeters = DepthCentimeters / 100.0;
        if (bValidCapturedDepth)
        {
            MinimumObservedMeters = FMath::Min(MinimumObservedMeters, DepthMeters);
            MaximumObservedMeters = FMath::Max(MaximumObservedMeters, DepthMeters);
        }

        uint32 EncodedDepthMillimeters = RawDepthInvalidValue;
        if (bValidCapturedDepth)
        {
            const double UnclampedMillimeters = DepthMeters * 1000.0;
            const int64 RoundedMillimeters = FMath::RoundToInt64(FMath::Min(UnclampedMillimeters, 4294967295.0));
            EncodedDepthMillimeters = static_cast<uint32>(FMath::Clamp<int64>(RoundedMillimeters, 1, static_cast<int64>(RawDepthSaturatedValue)));
            if (EncodedDepthMillimeters == RawDepthSaturatedValue)
            {
                ++RawDepthSaturatedPixelCount;
            }
        }
        const int64 RawByteOffset = static_cast<int64>(PixelIndex) * 4;
        RawDepthU32MillimeterBytes[RawByteOffset] = static_cast<uint8>(EncodedDepthMillimeters & 0x000000ffu);
        RawDepthU32MillimeterBytes[RawByteOffset + 1] = static_cast<uint8>((EncodedDepthMillimeters >> 8) & 0x000000ffu);
        RawDepthU32MillimeterBytes[RawByteOffset + 2] = static_cast<uint8>((EncodedDepthMillimeters >> 16) & 0x000000ffu);
        RawDepthU32MillimeterBytes[RawByteOffset + 3] = static_cast<uint8>((EncodedDepthMillimeters >> 24) & 0x000000ffu);

        const uint8 Gray = static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(DepthCentimeters / MaxDepthCentimeters, 0.0, 1.0) * 255.0));
        NormalizedDepthPixels[PixelIndex] = FColor(Gray, Gray, Gray, 255);
    }
    if (MinimumObservedMeters == TNumericLimits<double>::Max())
    {
        MinimumObservedMeters = 0.0;
    }

    TArray64<uint8> RGBPng;
    TArray64<uint8> DepthPng;
    FImageUtils::PNGCompressImageArray(
        Width,
        Height,
        TArrayView64<const FColor>(RGBPixels.GetData(), RGBPixels.Num()),
        RGBPng);
    FImageUtils::PNGCompressImageArray(
        Width,
        Height,
        TArrayView64<const FColor>(NormalizedDepthPixels.GetData(), NormalizedDepthPixels.Num()),
        DepthPng);

    const FDateTime CaptureTimestampUtc = FDateTime::UtcNow();
    const FString TimestampIso = CaptureTimestampUtc.ToIso8601();
    const FString TimestampForFile = CaptureTimestampUtc.ToString(TEXT("%Y%m%dT%H%M%S%fZ"));
    const FString BaseName = FString::Printf(TEXT("frame_%06d_%s"), CapturedFrameCount, *TimestampForFile);
    const FString RawDepthFileName = BaseName + TEXT("_depth_u32_mm.bin");
    TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
    Metadata->SetStringField(TEXT("timestampUtc"), TimestampIso);
    Metadata->SetStringField(TEXT("nodeId"), SensorNode->Definition.NodeId);
    Metadata->SetNumberField(TEXT("frameIndex"), CapturedFrameCount);
    Metadata->SetNumberField(TEXT("width"), Width);
    Metadata->SetNumberField(TEXT("height"), Height);
    Metadata->SetNumberField(TEXT("fovDegrees"), RGBCapture->FOVAngle);
    Metadata->SetStringField(TEXT("depthCaptureSource"), TEXT("SCS_SceneDepth_R_centimeters"));
    Metadata->SetStringField(TEXT("depthPngEncoding"), TEXT("gray/255 * normalizationMaxMeters"));
    Metadata->SetNumberField(TEXT("normalizationMaxMeters"), SensorNode->Definition.DepthNormalizationMaxMeters);
    Metadata->SetStringField(TEXT("depthRawFileName"), RawDepthFileName);
    Metadata->SetStringField(TEXT("depthRawEncoding"), TEXT("uint32_millimeters"));
    Metadata->SetNumberField(TEXT("depthRawQuantizationMeters"), RawDepthQuantizationMeters);
    Metadata->SetStringField(TEXT("depthRawUnits"), TEXT("millimeters"));
    Metadata->SetStringField(TEXT("depthRawByteOrder"), TEXT("little-endian"));
    Metadata->SetNumberField(TEXT("depthRawWidth"), Width);
    Metadata->SetNumberField(TEXT("depthRawHeight"), Height);
    Metadata->SetNumberField(TEXT("depthRawInvalidValue"), RawDepthInvalidValue);
    Metadata->SetNumberField(TEXT("depthRawSaturatedValue"), RawDepthSaturatedValue);
    Metadata->SetNumberField(TEXT("depthRawMaximumRepresentableMeters"), RawDepthMaximumRepresentableMeters);
    Metadata->SetNumberField(TEXT("depthRawSaturatedPixelCount"), RawDepthSaturatedPixelCount);
    Metadata->SetNumberField(TEXT("depthRawFileByteCount"), RawDepthU32MillimeterBytes.Num());
    Metadata->SetStringField(TEXT("depthRawSemantics"), TEXT("simulation_scene_depth_z_not_physical_oak_accuracy"));
    Metadata->SetBoolField(TEXT("depthRawSimulationOnly"), true);
    Metadata->SetNumberField(TEXT("minimumObservedMeters"), MinimumObservedMeters);
    Metadata->SetNumberField(TEXT("maximumObservedMeters"), MaximumObservedMeters);
    Metadata->SetStringField(TEXT("weatherProfile"), ActiveWeatherProfileName);
    Metadata->SetBoolField(TEXT("airSimVisualWeatherApplied"), bActiveVisualWeatherApplied);
    Metadata->SetNumberField(TEXT("weatherRainRateMillimetersPerHour"), ActiveRainRateMillimetersPerHour);
    Metadata->SetNumberField(TEXT("weatherVisibilityMeters"), ActiveVisibilityMeters);
    if (bDebugVisualCaptureFilterActive)
    {
        Metadata->SetBoolField(TEXT("debugVisualsExcludedFromSensorCapture"), true);
        Metadata->SetStringField(
            TEXT("sensorCaptureVisualSemantics"),
            TEXT("scene_pixels_with_TRIAD_operator_labels_site_markers_and_debug_line_batches_excluded"));
    }

    // Project tagged drone component bounds using this capture's own transform/FOV.
    // This is frustum ground truth only; it intentionally does not claim visual ML
    // classification or hidden-surface/occlusion testing.
    const FTransform CameraTransform = RGBCapture->GetComponentTransform();
    const double AspectRatio = static_cast<double>(Width) / static_cast<double>(Height);
    const double TanHalfHorizontalFov = FMath::Tan(FMath::DegreesToRadians(RGBCapture->FOVAngle * 0.5));
    const double TanHalfVerticalFov = TanHalfHorizontalFov / FMath::Max(AspectRatio, 0.000001);
    constexpr double ProjectionNearPlaneCentimeters = 1.0;
    int32 TaggedTargetCount = 0;
    int32 VisibleTargetCount = 0;
    TArray<TSharedPtr<FJsonValue>> ProjectedTargets;
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Target = *It;
            if (!IsValid(Target) || Target == this || !Target->ActorHasTag(TEXT("DroneTarget")))
            {
                continue;
            }
            ++TaggedTargetCount;

            // Bounding boxes describe model-visible primitive geometry only.
            // Operator labels previously inflated these bounds and made visual
            // confirmation look like unexplained colored rectangles.
            const FBox ComponentBounds = GetModelFacingPrimitiveBounds(Target);
            const bool bBoundsValid = ComponentBounds.IsValid != 0;
            TArray<FVector, TInlineAllocator<8>> WorldCorners;
            if (bBoundsValid)
            {
                WorldCorners.Reserve(8);
                for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
                {
                    WorldCorners.Add(FVector(
                        (CornerIndex & 1) ? ComponentBounds.Max.X : ComponentBounds.Min.X,
                        (CornerIndex & 2) ? ComponentBounds.Max.Y : ComponentBounds.Min.Y,
                        (CornerIndex & 4) ? ComponentBounds.Max.Z : ComponentBounds.Min.Z));
                }
            }
            else
            {
                WorldCorners.Add(Target->GetActorLocation());
            }

            bool bInFront = false;
            int32 ProjectedCornerCount = 0;
            double RawMinimumX = TNumericLimits<double>::Max();
            double RawMinimumY = TNumericLimits<double>::Max();
            double RawMaximumX = -TNumericLimits<double>::Max();
            double RawMaximumY = -TNumericLimits<double>::Max();
            for (const FVector& WorldCorner : WorldCorners)
            {
                const FVector CameraLocal = CameraTransform.InverseTransformPosition(WorldCorner);
                if (CameraLocal.X <= ProjectionNearPlaneCentimeters)
                {
                    continue;
                }

                bInFront = true;
                ++ProjectedCornerCount;
                const double NormalizedX = CameraLocal.Y / (CameraLocal.X * TanHalfHorizontalFov);
                const double NormalizedY = CameraLocal.Z / (CameraLocal.X * TanHalfVerticalFov);
                const double PixelX = (NormalizedX * 0.5 + 0.5) * static_cast<double>(Width);
                const double PixelY = (0.5 - NormalizedY * 0.5) * static_cast<double>(Height);
                RawMinimumX = FMath::Min(RawMinimumX, PixelX);
                RawMinimumY = FMath::Min(RawMinimumY, PixelY);
                RawMaximumX = FMath::Max(RawMaximumX, PixelX);
                RawMaximumY = FMath::Max(RawMaximumY, PixelY);
            }

            const bool bIntersectsFrame = ProjectedCornerCount > 0 &&
                RawMaximumX >= 0.0 && RawMaximumY >= 0.0 &&
                RawMinimumX <= static_cast<double>(Width - 1) &&
                RawMinimumY <= static_cast<double>(Height - 1);
            if (bIntersectsFrame)
            {
                ++VisibleTargetCount;
            }

            bool bHostileScenarioTruth = false;
            if (const UTRIADRFEmitterComponent* Emitter = Target->FindComponentByClass<UTRIADRFEmitterComponent>())
            {
                bHostileScenarioTruth = Emitter->Definition.bHostileScenarioTruth;
            }

            TArray<TSharedPtr<FJsonValue>> BoundingBoxValues;
            if (bIntersectsFrame)
            {
                BoundingBoxValues.Add(MakeShared<FJsonValueNumber>(FMath::Clamp(RawMinimumX, 0.0, static_cast<double>(Width - 1))));
                BoundingBoxValues.Add(MakeShared<FJsonValueNumber>(FMath::Clamp(RawMinimumY, 0.0, static_cast<double>(Height - 1))));
                BoundingBoxValues.Add(MakeShared<FJsonValueNumber>(FMath::Clamp(RawMaximumX, 0.0, static_cast<double>(Width - 1))));
                BoundingBoxValues.Add(MakeShared<FJsonValueNumber>(FMath::Clamp(RawMaximumY, 0.0, static_cast<double>(Height - 1))));
            }

            TSharedRef<FJsonObject> TargetMetadata = MakeShared<FJsonObject>();
            TargetMetadata->SetStringField(TEXT("actorName"), Target->GetName());
            TargetMetadata->SetBoolField(TEXT("hostileScenarioTruth"), bHostileScenarioTruth);
            TargetMetadata->SetArrayField(TEXT("bboxXyxyPixels"), BoundingBoxValues);
            TargetMetadata->SetNumberField(
                TEXT("distanceMeters"),
                FVector::Distance(CameraTransform.GetLocation(), Target->GetActorLocation()) / 100.0);
            TargetMetadata->SetBoolField(TEXT("inFront"), bInFront);
            TargetMetadata->SetBoolField(TEXT("intersectsFrame"), bIntersectsFrame);
            TargetMetadata->SetBoolField(TEXT("componentBoundsValid"), bBoundsValid);
            TargetMetadata->SetNumberField(TEXT("projectedCornerCount"), ProjectedCornerCount);
            ProjectedTargets.Add(MakeShared<FJsonValueObject>(TargetMetadata));
        }
    }
    Metadata->SetStringField(
        TEXT("targetProjectionSemantics"),
        TEXT("model_facing_physical_primitive_bounds_frustum_only_no_occlusion_test"));
    Metadata->SetStringField(
        TEXT("targetBoundsSemantics"),
        TEXT("visible_primitive_component_bounds_excluding_TRIAD_human_only_overlays"));
    Metadata->SetNumberField(TEXT("cameraAspectRatio"), AspectRatio);
    Metadata->SetNumberField(TEXT("taggedTargetCount"), TaggedTargetCount);
    Metadata->SetNumberField(TEXT("visibleTargetCount"), VisibleTargetCount);
    Metadata->SetArrayField(TEXT("targets"), ProjectedTargets);

    FString MetadataString;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetadataString);
    FJsonSerializer::Serialize(Metadata, Writer);

    FTCHARToUTF8 MetadataUtf8(*MetadataString);
    const int64 FrameByteCount = RGBPng.Num() + DepthPng.Num() + RawDepthU32MillimeterBytes.Num() + MetadataUtf8.Length();
    if (CapturedFrameBytes + FrameByteCount > MaximumFrameBytes)
    {
        StopFrameCapture(TEXT("per-node frame-byte limit reached"));
        return;
    }

    const FString RGBPath = FPaths::Combine(FrameDirectory, BaseName + TEXT("_rgb.png"));
    const FString DepthPath = FPaths::Combine(FrameDirectory, BaseName + TEXT("_depth.png"));
    const FString RawDepthPath = FPaths::Combine(FrameDirectory, RawDepthFileName);
    const FString MetadataPath = FPaths::Combine(FrameDirectory, BaseName + TEXT("_depth.json"));

    const bool bSaved = FFileHelper::SaveArrayToFile(RGBPng, *RGBPath) &&
        FFileHelper::SaveArrayToFile(DepthPng, *DepthPath) &&
        FFileHelper::SaveArrayToFile(RawDepthU32MillimeterBytes, *RawDepthPath) &&
        FFileHelper::SaveStringToFile(MetadataString, *MetadataPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bSaved)
    {
        ++CapturedFrameCount;
        CapturedFrameBytes += FrameByteCount;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("TRIAD sensor node '%s' failed to save a camera frame under '%s'."), *SensorNode->Definition.NodeId, *FrameDirectory);
    }
}

void ATRIADSensorNodeActor::StopFrameCapture(const FString& Reason)
{
    GetWorldTimerManager().ClearTimer(CaptureTimerHandle);
    if (!bCaptureLimitReported)
    {
        bCaptureLimitReported = true;
        UE_LOG(LogTemp, Display, TEXT("TRIAD sensor node '%s' stopped camera capture: %s."), *SensorNode->Definition.NodeId, *Reason);
    }
}
