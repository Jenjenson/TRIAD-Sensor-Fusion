#include "TRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "TRIADIstanaExploreV5DDynamicRangeRealismActor.h"

namespace
{
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));

template <typename TActorType>
TActorType* FindExactlyOne(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    TActorType* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<TActorType> It(World); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == World)
        {
            Result = *It;
            ++OutCount;
        }
    }
    return OutCount == 1 ? Result : nullptr;
}

bool IsExactTargetEditorWorld(const UWorld* World)
{
    return World && World->GetOutermost() &&
        World->GetOutermost()->GetName() == TargetMapPackage;
}

bool ValidateOneActor(
    UWorld* World,
    bool bRequireRuntime,
    ATRIADIstanaExploreV5DDynamicRangeRealismActor*& OutActor,
    FString& OutReport)
{
    int32 Count = 0;
    OutActor = FindExactlyOne<
        ATRIADIstanaExploreV5DDynamicRangeRealismActor>(World, Count);
    if (Count != 1 || !OutActor ||
        !OutActor->Tags.Contains(
            ATRIADIstanaExploreV5DDynamicRangeRealismActor::
                ExpectedActorTag()) ||
        !OutActor->ValidateDynamicRangeRealism(
            bRequireRuntime,
            OutReport))
    {
        if (OutReport.IsEmpty())
        {
            OutReport = FString::Printf(
                TEXT("V5D dynamic range requires exactly one tagged actor; found %d."),
                Count);
        }
        return false;
    }
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary::
    ApplyDynamicRangeRealismPassToLoadedV5DHybridMap(
        FString& OutMessage)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    if (!IsExactTargetEditorWorld(World))
    {
        OutMessage = TEXT("V5D_DYNAMIC_RANGE_APPLY_REFUSED: load the exact V5D hybrid destination map first.");
        return false;
    }
    return ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder(
        World,
        false,
        OutMessage);
}

bool UTRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary::
    ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder(
        UWorld* InWorld,
        bool bTrustedUntitledHybridBuilder,
        FString& OutMessage)
{
    const bool bExactTarget = IsExactTargetEditorWorld(InWorld);
    const bool bTrustedTemp = InWorld && InWorld->GetOutermost() &&
        bTrustedUntitledHybridBuilder &&
        FPackageName::IsTempPackage(InWorld->GetOutermost()->GetName());
    if (!bExactTarget && !bTrustedTemp)
    {
        OutMessage = TEXT("V5D_DYNAMIC_RANGE_APPLY_REFUSED_WORLD: only the exact target or an explicitly trusted untitled hybrid-builder world is admitted.");
        return false;
    }

    int32 ExistingCount = 0;
    ATRIADIstanaExploreV5DDynamicRangeRealismActor* Existing =
        FindExactlyOne<ATRIADIstanaExploreV5DDynamicRangeRealismActor>(
            InWorld,
            ExistingCount);
    if (ExistingCount == 1 && Existing)
    {
        FString ExistingReport;
        if (Existing->ValidateDynamicRangeRealism(false, ExistingReport))
        {
            OutMessage = TEXT("IDEMPOTENT_V5D_DYNAMIC_RANGE_ALREADY_VALID: ") +
                ExistingReport;
            return true;
        }
        OutMessage = TEXT("V5D_DYNAMIC_RANGE_APPLY_REFUSED_EXISTING_INVALID: ") +
            ExistingReport;
        return false;
    }
    if (ExistingCount != 0)
    {
        OutMessage = FString::Printf(
            TEXT("V5D_DYNAMIC_RANGE_APPLY_REFUSED_DUPLICATES: found %d actors."),
            ExistingCount);
        return false;
    }

    FTRIADIstanaExploreV5DSourceLightingSnapshot BeforeLighting;
    FString Error;
    if (!ATRIADIstanaExploreV5DDynamicRangeRealismActor::
            CaptureExactSourceLightingSnapshot(
                InWorld,
                BeforeLighting,
                Error))
    {
        OutMessage = TEXT("V5D_DYNAMIC_RANGE_APPLY_REFUSED_LIGHTING: ") +
            Error;
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name =
        TEXT("TRIADIstanaExploreV5DDynamicRangeRealism");
    SpawnParameters.OverrideLevel = InWorld->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADIstanaExploreV5DDynamicRangeRealismActor* Actor =
        InWorld->SpawnActor<
            ATRIADIstanaExploreV5DDynamicRangeRealismActor>(
                ATRIADIstanaExploreV5DDynamicRangeRealismActor::
                    StaticClass(),
                FTransform::Identity,
                SpawnParameters);
    if (!Actor)
    {
        OutMessage = TEXT("V5D_DYNAMIC_RANGE_APPLY_FAILED_SPAWN: the isolated identity lookdev actor could not be spawned.");
        return false;
    }
    Actor->Tags.AddUnique(
        ATRIADIstanaExploreV5DDynamicRangeRealismActor::ExpectedActorTag());
    Actor->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D Dynamic Range Visual Assumption"));

    FTRIADIstanaExploreV5DSourceLightingSnapshot AfterLighting;
    FString Report;
    if (!Actor->ConfigureDynamicRangeRealism(
            bTrustedUntitledHybridBuilder,
            Error) ||
        !ATRIADIstanaExploreV5DDynamicRangeRealismActor::
            CaptureExactSourceLightingSnapshot(
                InWorld,
                AfterLighting,
                Error) ||
        !BeforeLighting.Equals(AfterLighting, 0.0001) ||
        !Actor->SavedSourceLightingSnapshot.Equals(
            BeforeLighting,
            0.0001) ||
        !Actor->ValidateDynamicRangeRealism(false, Report))
    {
        Actor->Destroy();
        OutMessage = TEXT("V5D_DYNAMIC_RANGE_APPLY_FAILED_COLD_VALIDATION: ") +
            Error + TEXT(" ") + Report +
            TEXT(" Source lighting mutation is forbidden.");
        return false;
    }

    InWorld->MarkPackageDirty();
    OutMessage = TEXT("V5D_DYNAMIC_RANGE_APPLIED_CALLER_MUST_SAVE_MAP: ") +
        Report;
    return true;
}

bool UTRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary::
    ValidateDynamicRangeRealismPassInLoadedV5DHybridMap(
        FString& OutReport)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    if (!IsExactTargetEditorWorld(World))
    {
        OutReport = TEXT("V5D_DYNAMIC_RANGE_VALIDATION_FAILED: the exact V5D hybrid editor map is not loaded.");
        return false;
    }
    ATRIADIstanaExploreV5DDynamicRangeRealismActor* Actor = nullptr;
    return ValidateOneActor(World, false, Actor, OutReport);
}

bool UTRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary::
    ValidateDynamicRangeRealismPassInPlayWorld(FString& OutReport)
{
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld ||
        UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName()) !=
            TargetMapPackage)
    {
        OutReport = TEXT("V5D_DYNAMIC_RANGE_PIE_VALIDATION_FAILED: exact V5D hybrid PIE is not running.");
        return false;
    }
    ATRIADIstanaExploreV5DDynamicRangeRealismActor* Actor = nullptr;
    return ValidateOneActor(PlayWorld, true, Actor, OutReport);
}
