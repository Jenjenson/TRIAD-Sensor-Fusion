#include "TRIADIstanaExploreV5GameMode.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "TRIADIstanaExploreV5Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogTRIADIstanaExploreV5GameMode, Log, All);

namespace
{
const FName ExplorePlayerStartTag(TEXT("TRIADIstanaExplorePlayerStartV1"));
}

ATRIADIstanaExploreV5GameMode::ATRIADIstanaExploreV5GameMode(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DefaultPawnClass = ATRIADIstanaExploreV5Pawn::StaticClass();
    HUDClass = nullptr;
}

FName ATRIADIstanaExploreV5GameMode::ExpectedExplorePlayerStartTag()
{
    return ExplorePlayerStartTag;
}

int32 ATRIADIstanaExploreV5GameMode::FindUniqueExplorePlayerStartIndex(
    const TArray<TArray<FName>>& CandidateActorTags,
    int32& OutMatchingStartCount)
{
    int32 UniqueIndex = INDEX_NONE;
    OutMatchingStartCount = 0;
    for (int32 Index = 0; Index < CandidateActorTags.Num(); ++Index)
    {
        if (!CandidateActorTags[Index].Contains(ExplorePlayerStartTag))
        {
            continue;
        }
        UniqueIndex = Index;
        ++OutMatchingStartCount;
    }
    return OutMatchingStartCount == 1 ? UniqueIndex : INDEX_NONE;
}

AActor* ATRIADIstanaExploreV5GameMode::ChoosePlayerStart_Implementation(
    AController* Player)
{
    (void)Player;
    UWorld* const World = GetWorld();
    if (!World)
    {
        UE_LOG(
            LogTRIADIstanaExploreV5GameMode,
            Error,
            TEXT("ISTANA_EXPLORE_V5_PLAYER_START_INVALID tagged=0 total=0 requiredTagged=1 tag=%s world=null"),
            *ExplorePlayerStartTag.ToString());
        return nullptr;
    }

    TArray<APlayerStart*> CandidateStarts;
    TArray<TArray<FName>> CandidateTags;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        CandidateStarts.Add(*It);
        CandidateTags.Add(It->Tags);
    }
    int32 MatchingStartCount = 0;
    const int32 UniqueIndex = FindUniqueExplorePlayerStartIndex(
        CandidateTags, MatchingStartCount);
    if (!CandidateStarts.IsValidIndex(UniqueIndex))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5GameMode,
            Error,
            TEXT("ISTANA_EXPLORE_V5_PLAYER_START_INVALID tagged=%d total=%d requiredTagged=1 tag=%s"),
            MatchingStartCount,
            CandidateStarts.Num(),
            *ExplorePlayerStartTag.ToString());
        return nullptr;
    }
    return CandidateStarts[UniqueIndex];
}
