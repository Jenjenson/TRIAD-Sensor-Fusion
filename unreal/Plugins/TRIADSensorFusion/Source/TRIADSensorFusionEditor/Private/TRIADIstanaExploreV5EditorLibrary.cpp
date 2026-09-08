#include "TRIADIstanaExploreV5EditorLibrary.h"

#include "AI/NavigationSystemConfig.h"
#include "Components/ActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Level.h"
#include "Engine/Light.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkyLight.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Slate/SceneViewport.h"
#include "ShowFlags.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV4EditorLibrary.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV2LandscapeActor.h"
#include "TRIADIstanaExploreV3SupplementActor.h"
#include "TRIADIstanaExploreV5AppearanceActor.h"
#include "TRIADIstanaExploreV5GameMode.h"
#include "TRIADIstanaExploreV5MaterialFactory.h"
#include "TRIADIstanaExploreV5Pawn.h"
#include "TRIADIstanaFreeRoamPawn.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UnrealClient.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include <array>

namespace
{
const FString SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v4"));
const FString DestinationMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5"));
const FString DestinationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5.Istana_PublicView_Explore_v5"));
const FString ExpectedSourceMapSha256(
    TEXT("D31EBF410A8C0E7201103BCCBDBC05F3BD1A2DF124FF090D080DCFAC27DEBA72"));
constexpr int64 ExpectedSourceMapBytes = 12153074;
const FName V4LandscapeTag(TEXT("TRIADIstanaExploreLandscapeV4"));
const FName V5AppearanceTag(TEXT("TRIADIstanaExploreAppearanceV5"));

TWeakObjectPtr<UWorld> FullyAcceptedQaEditorWorld;
TWeakObjectPtr<UWorld> FullyAcceptedQaPlayWorld;

template <typename TObjectType>
TObjectType* LoadExact(const FString& ObjectPath)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
}

// A private portable implementation avoids the UE 5.5 Win64 platform SHA-256
// path that asserts for incremental file hashing.  The source map is streamed;
// it is never copied into a large temporary buffer.
class FStreamingSha256 final
{
public:
    FStreamingSha256()
        : State{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u}
    {
    }

    void Update(const uint8* Data, uint64 Length)
    {
        TotalBytes += Length;
        while (Length > 0)
        {
            const uint32 CopyCount = static_cast<uint32>(FMath::Min<uint64>(
                Length, 64u - BufferedBytes));
            FMemory::Memcpy(Buffer.data() + BufferedBytes, Data, CopyCount);
            BufferedBytes += CopyCount;
            Data += CopyCount;
            Length -= CopyCount;
            if (BufferedBytes == 64)
            {
                Transform(Buffer.data());
                BufferedBytes = 0;
            }
        }
    }

    FString FinalUpperHex()
    {
        const uint64 BitCount = TotalBytes * 8u;
        uint8 Padding[128] = {0x80};
        const uint32 PaddingBytes = BufferedBytes < 56
            ? 56u - BufferedBytes
            : 120u - BufferedBytes;
        Update(Padding, PaddingBytes);
        uint8 LengthBytes[8];
        for (int32 Index = 0; Index < 8; ++Index)
        {
            LengthBytes[7 - Index] = static_cast<uint8>(BitCount >> (Index * 8));
        }
        Update(LengthBytes, 8);

        FString Hex;
        Hex.Reserve(64);
        for (uint32 Word : State)
        {
            Hex += FString::Printf(TEXT("%08X"), Word);
        }
        return Hex;
    }

private:
    static uint32 RotateRight(uint32 Value, uint32 Count)
    {
        return (Value >> Count) | (Value << (32u - Count));
    }

    void Transform(const uint8* Block)
    {
        static constexpr uint32 K[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
            0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
            0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
            0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        uint32 W[64];
        for (int32 Index = 0; Index < 16; ++Index)
        {
            W[Index] =
                (static_cast<uint32>(Block[Index * 4]) << 24) |
                (static_cast<uint32>(Block[Index * 4 + 1]) << 16) |
                (static_cast<uint32>(Block[Index * 4 + 2]) << 8) |
                static_cast<uint32>(Block[Index * 4 + 3]);
        }
        for (int32 Index = 16; Index < 64; ++Index)
        {
            const uint32 S0 = RotateRight(W[Index - 15], 7) ^
                RotateRight(W[Index - 15], 18) ^ (W[Index - 15] >> 3);
            const uint32 S1 = RotateRight(W[Index - 2], 17) ^
                RotateRight(W[Index - 2], 19) ^ (W[Index - 2] >> 10);
            W[Index] = W[Index - 16] + S0 + W[Index - 7] + S1;
        }

        uint32 A = State[0];
        uint32 B = State[1];
        uint32 C = State[2];
        uint32 D = State[3];
        uint32 E = State[4];
        uint32 F = State[5];
        uint32 G = State[6];
        uint32 H = State[7];
        for (int32 Index = 0; Index < 64; ++Index)
        {
            const uint32 BigS1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^
                RotateRight(E, 25);
            const uint32 Choice = (E & F) ^ ((~E) & G);
            const uint32 T1 = H + BigS1 + Choice + K[Index] + W[Index];
            const uint32 BigS0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^
                RotateRight(A, 22);
            const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
            const uint32 T2 = BigS0 + Majority;
            H = G;
            G = F;
            F = E;
            E = D + T1;
            D = C;
            C = B;
            B = A;
            A = T1 + T2;
        }
        State[0] += A;
        State[1] += B;
        State[2] += C;
        State[3] += D;
        State[4] += E;
        State[5] += F;
        State[6] += G;
        State[7] += H;
    }

    std::array<uint32, 8> State;
    std::array<uint8, 64> Buffer{};
    uint64 TotalBytes = 0;
    uint32 BufferedBytes = 0;
};

bool StreamingSha256IsValid()
{
    static const bool bValid = []()
    {
        static constexpr uint8 Abc[] = {'a', 'b', 'c'};
        FStreamingSha256 Hasher;
        Hasher.Update(Abc, UE_ARRAY_COUNT(Abc));
        return Hasher.FinalUpperHex() ==
            TEXT("BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
    }();
    return bValid;
}

bool HashFile(const FString& Filename, int64& OutBytes, FString& OutSha)
{
    OutBytes = IFileManager::Get().FileSize(*Filename);
    OutSha.Reset();
    if (OutBytes < 0 || !StreamingSha256IsValid())
    {
        return false;
    }
    TUniquePtr<IFileHandle> Handle(
        FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Filename));
    if (!Handle)
    {
        return false;
    }
    FStreamingSha256 Hasher;
    TArray<uint8> Chunk;
    Chunk.SetNumUninitialized(1024 * 1024);
    int64 Remaining = OutBytes;
    while (Remaining > 0)
    {
        const int64 ReadBytes = FMath::Min<int64>(Remaining, Chunk.Num());
        if (!Handle->Read(Chunk.GetData(), ReadBytes))
        {
            return false;
        }
        Hasher.Update(Chunk.GetData(), static_cast<uint64>(ReadBytes));
        Remaining -= ReadBytes;
    }
    OutSha = Hasher.FinalUpperHex();
    return OutSha.Len() == 64;
}

struct FPackageFileDigest
{
    FString Filename;
    bool bDirectoryMarker = false;
    bool bExisted = false;
    int64 Bytes = 0;
    FString Sha256;
};

bool ResolveMapPhysicalRoster(
    const FString& PackageName,
    const FString& KnownMainFilename,
    TArray<FString>& OutPackageFiles,
    TArray<FString>& OutExternalDirectories,
    FString& OutError)
{
    FString MainFilename = KnownMainFilename;
    if (MainFilename.IsEmpty() &&
        !FPackageName::TryConvertLongPackageNameToFilename(
            PackageName,
            MainFilename,
            FPackageName::GetMapPackageExtension()))
    {
        OutError = TEXT("Could not resolve the exact map package to a physical filename: ") +
            PackageName;
        return false;
    }
    MainFilename = FPaths::ConvertRelativePathToFull(MainFilename);
    FPaths::NormalizeFilename(MainFilename);
    const FString Base = FPaths::ChangeExtension(MainFilename, TEXT(""));
    OutPackageFiles = {
        MainFilename,
        Base + TEXT(".uexp"),
        Base + TEXT(".ubulk"),
        Base + TEXT(".uptnl"),
        Base + TEXT(".m.ubulk"),
        Base + TEXT(".upayload")};

    TSet<FString> UniqueDirectories;
    for (const FString& ExternalPackagePath :
         ULevel::GetExternalObjectsPaths(PackageName))
    {
        FString Directory;
        if (!FPackageName::TryConvertLongPackageNameToFilename(
                ExternalPackagePath, Directory))
        {
            OutError = TEXT("Could not resolve an external actor/object package directory: ") +
                ExternalPackagePath;
            return false;
        }
        Directory = FPaths::ConvertRelativePathToFull(Directory);
        FPaths::NormalizeDirectoryName(Directory);
        UniqueDirectories.Add(MoveTemp(Directory));
    }
    OutExternalDirectories = UniqueDirectories.Array();
    OutExternalDirectories.Sort();
    OutError.Reset();
    return true;
}

bool CaptureExactV4MapSnapshot(
    TArray<FPackageFileDigest>& OutSnapshot,
    FString& OutError)
{
    FString MainFilename;
    if (!FPackageName::DoesPackageExist(SourceMapPackage, &MainFilename))
    {
        OutError = TEXT("The exact Explore V4 source map package is absent.");
        return false;
    }
    TArray<FString> Candidates;
    TArray<FString> ExternalDirectories;
    if (!ResolveMapPhysicalRoster(
            SourceMapPackage,
            MainFilename,
            Candidates,
            ExternalDirectories,
            OutError))
    {
        return false;
    }

    OutSnapshot.Reset();
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        FPackageFileDigest Digest;
        Digest.Filename = Candidates[Index];
        Digest.Bytes = IFileManager::Get().FileSize(*Digest.Filename);
        Digest.bExisted = Digest.Bytes >= 0;
        if (Index == 0 && (!Digest.bExisted ||
            Digest.Bytes != ExpectedSourceMapBytes))
        {
            OutError = FString::Printf(
                TEXT("Explore V4 source .umap bytes changed: expected %lld, got %lld."),
                ExpectedSourceMapBytes,
                Digest.Bytes);
            return false;
        }
        if (Digest.bExisted &&
            (!HashFile(Digest.Filename, Digest.Bytes, Digest.Sha256) ||
             (Index == 0 && Digest.Sha256 != ExpectedSourceMapSha256)))
        {
            OutError = Index == 0
                ? TEXT("Explore V4 source .umap SHA-256 is not the frozen D31EBF...DEBA72 digest.")
                : TEXT("An Explore V4 sidecar could not be hashed: ") +
                    Digest.Filename;
            return false;
        }
        if (!Digest.bExisted)
        {
            Digest.Bytes = 0;
        }
        OutSnapshot.Add(MoveTemp(Digest));
    }

    for (const FString& Directory : ExternalDirectories)
    {
        FPackageFileDigest DirectoryDigest;
        DirectoryDigest.Filename = Directory;
        DirectoryDigest.bDirectoryMarker = true;
        DirectoryDigest.bExisted =
            IFileManager::Get().DirectoryExists(*Directory);
        OutSnapshot.Add(MoveTemp(DirectoryDigest));
        if (!IFileManager::Get().DirectoryExists(*Directory))
        {
            continue;
        }

        TArray<FString> ExternalFiles;
        IFileManager::Get().FindFilesRecursive(
            ExternalFiles,
            *Directory,
            TEXT("*"),
            true,
            false);
        for (FString& ExternalFile : ExternalFiles)
        {
            ExternalFile = FPaths::ConvertRelativePathToFull(ExternalFile);
            FPaths::NormalizeFilename(ExternalFile);
        }
        ExternalFiles.Sort();
        for (const FString& ExternalFile : ExternalFiles)
        {
            FPackageFileDigest Digest;
            Digest.Filename = ExternalFile;
            Digest.Bytes = IFileManager::Get().FileSize(*Digest.Filename);
            Digest.bExisted = Digest.Bytes >= 0;
            if (!Digest.bExisted ||
                !HashFile(Digest.Filename, Digest.Bytes, Digest.Sha256))
            {
                OutError = TEXT("An Explore V4 external actor/object package file could not be hashed: ") +
                    Digest.Filename;
                return false;
            }
            OutSnapshot.Add(MoveTemp(Digest));
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateExactV4MapSnapshot(
    const TArray<FPackageFileDigest>& Snapshot,
    FString& OutError)
{
    if (Snapshot.Num() < 8)
    {
        OutError = TEXT("The Explore V4 package snapshot roster is incomplete.");
        return false;
    }
    TArray<FPackageFileDigest> Current;
    if (!CaptureExactV4MapSnapshot(Current, OutError))
    {
        return false;
    }
    if (Current.Num() != Snapshot.Num())
    {
        OutError = TEXT("Explore V4 package/sidecar/external-object roster changed.");
        return false;
    }
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
    {
        const FPackageFileDigest& Expected = Snapshot[Index];
        const FPackageFileDigest& Actual = Current[Index];
        if (Expected.Filename != Actual.Filename ||
            Expected.bDirectoryMarker != Actual.bDirectoryMarker ||
            Expected.bExisted != Actual.bExisted ||
            Expected.Bytes != Actual.Bytes ||
            Expected.Sha256 != Actual.Sha256)
        {
            OutError = TEXT("Explore V4 package/sidecar/external-object state changed at: ") +
                Expected.Filename;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool InspectDestinationMapPresence(
    UEditorAssetSubsystem* Assets,
    bool& bOutAnyExists,
    FString& OutDetail)
{
    bOutAnyExists = false;
    if (!Assets)
    {
        OutDetail = TEXT("The editor asset subsystem is unavailable.");
        return false;
    }
    TArray<FString> PackageFiles;
    TArray<FString> ExternalDirectories;
    if (!ResolveMapPhysicalRoster(
            DestinationMapPackage,
            FString(),
            PackageFiles,
            ExternalDirectories,
            OutDetail))
    {
        return false;
    }
    for (const FString& Filename : PackageFiles)
    {
        if (IFileManager::Get().FileSize(*Filename) >= 0)
        {
            bOutAnyExists = true;
            OutDetail = Filename;
            return true;
        }
    }
    for (const FString& Directory : ExternalDirectories)
    {
        if (IFileManager::Get().DirectoryExists(*Directory))
        {
            bOutAnyExists = true;
            OutDetail = Directory;
            return true;
        }
    }
    if (Assets->DoesAssetExist(DestinationMapPackage) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath))
    {
        bOutAnyExists = true;
        OutDetail = TEXT("asset registry or loaded-object state");
        return true;
    }
    OutDetail.Reset();
    return true;
}

bool ValidateMapLoadSafety(FString& OutError)
{
    if (!GEditor)
    {
        OutError = TEXT("The editor engine is unavailable; map loading is refused.");
        return false;
    }
    if (GEditor->PlayWorld)
    {
        OutError = TEXT("A PIE world is active; map loading is refused rather than prompting to terminate play.");
        return false;
    }
    UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
    if (!CurrentWorld || CurrentWorld->WorldType != EWorldType::Editor ||
        !CurrentWorld->GetOutermost())
    {
        OutError = TEXT("The exact current editor world/package is unavailable.");
        return false;
    }

    TSet<UPackage*> Packages;
    Packages.Add(CurrentWorld->GetOutermost());
    for (ULevel* Level : CurrentWorld->GetLevels())
    {
        if (!Level || !Level->GetOutermost())
        {
            OutError = TEXT("The current editor world contains a null level/package.");
            return false;
        }
        Packages.Add(Level->GetOutermost());
        if (Level->MapBuildData && Level->MapBuildData->GetOutermost())
        {
            Packages.Add(Level->MapBuildData->GetOutermost());
        }
        for (UPackage* ExternalPackage :
             Level->GetLoadedExternalObjectPackages())
        {
            if (!ExternalPackage)
            {
                OutError = TEXT("The current editor level contains a null loaded external package.");
                return false;
            }
            Packages.Add(ExternalPackage);
        }
    }
    for (UPackage* Package : Packages)
    {
        if (!Package || Package->IsDirty())
        {
            OutError = TEXT("Map loading would discard a dirty current world/level/build-data/external package: ") +
                GetPathNameSafe(Package);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

TArray<FString> ExactV5MaterialObjectPaths()
{
    return {
        TRIADIstanaExploreV5MaterialFactory::GetGrassBaseMaterialObjectPath(),
        TRIADIstanaExploreV5MaterialFactory::GetGrassMaterialInstanceObjectPath(),
        TRIADIstanaExploreV5MaterialFactory::GetFountainWaterMaterialObjectPath(),
        TRIADIstanaExploreV5MaterialFactory::GetHardscapeStoneMaterialInstanceObjectPath(),
        TRIADIstanaExploreV5MaterialFactory::GetContextRenderMaterialInstanceObjectPath(),
        TRIADIstanaExploreV5MaterialFactory::GetContextRoofMaterialInstanceObjectPath()};
}

bool ValidatePersistedV5Assets(FString& OutReport)
{
    if (!TRIADIstanaExploreV5MaterialFactory::ValidateExploreV5Materials(
            OutReport))
    {
        return false;
    }
    const TArray<FString> Paths = ExactV5MaterialObjectPaths();
    for (const FString& Path : Paths)
    {
        UObject* Object = LoadObject<UObject>(nullptr, *Path);
        UPackage* Package = Object ? Object->GetOutermost() : nullptr;
        FString Filename;
        if (!Object || Object->GetPathName() != Path || !Package ||
            Package->IsDirty() ||
            !FPackageName::DoesPackageExist(Package->GetName(), &Filename) ||
            IFileManager::Get().FileSize(*Filename) <= 0)
        {
            OutReport = TEXT("Explore V5 exact material graph exists only partially/in memory or has a dirty/non-positive-byte package: ") +
                Path;
            return false;
        }
    }
    OutReport = TEXT("EXPLORE_V5_ASSETS_PERSISTED_VALID: exact six clean positive-byte V5 material packages plus helper graph/shader validation passed.");
    return true;
}

template <typename TActorType>
TActorType* FindExactlyOne(UWorld* World, int32& OutCount)
{
    TActorType* Result = nullptr;
    OutCount = 0;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<TActorType> It(World); It; ++It)
    {
        Result = *It;
        ++OutCount;
    }
    return OutCount == 1 ? Result : nullptr;
}

bool IsAdmittedAppearanceMaterialSlot(
    const ATRIADIstanaPublicViewSceneActor* Scene,
    const UStaticMeshComponent* Component,
    int32 Slot)
{
    if (!Scene || !Component || Slot < 0)
    {
        return false;
    }
    if (Component == Scene->TerrainComponent && Slot == 0)
    {
        return true;
    }
    const UStaticMesh* Mesh = Component->GetStaticMesh();
    if (!Mesh)
    {
        return false;
    }
    const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();
    if (!StaticMaterials.IsValidIndex(Slot))
    {
        return false;
    }
    const FStaticMaterial& StaticMaterial = StaticMaterials[Slot];
    const auto Matches = [&StaticMaterial](const FName Name)
    {
        return StaticMaterial.MaterialSlotName == Name ||
            StaticMaterial.ImportedMaterialSlotName == Name;
    };
    if (Component == Scene->HardscapeComponent)
    {
        return Matches(FName(TEXT("M_IPV_Water"))) ||
            Matches(FName(TEXT("M_IPV_Stone")));
    }
    if (Component == Scene->OSMContextBuildingsComponent)
    {
        return Matches(FName(TEXT("M_IPV_ContextRender"))) ||
            Matches(FName(TEXT("M_IPV_ContextRoof")));
    }
    return false;
}

bool IsAdmittedContactShadowComponent(
    const ATRIADIstanaPublicViewSceneActor* Scene,
    const ATRIADIstanaExploreV4LandscapeActor* V4,
    const UPrimitiveComponent* Component)
{
    if (!Scene || !V4 || !Component)
    {
        return false;
    }
    const UPrimitiveComponent* ExactRoster[] = {
        Scene->BuildingHeroVisualComponent,
        Scene->HardscapeComponent,
        Scene->BuildingCollisionComponent,
        Scene->TerrainComponent,
        Scene->TerrainSkirtComponent,
        Scene->ContextBuildingsComponent,
        Scene->OSMContextBuildingsComponent,
        Scene->RainTreeInstances,
        Scene->PalmTreeInstances,
        Scene->FramingTreeInstances,
        V4->UmbrellaTreeInstances,
        V4->DomeTreeInstances,
        V4->HighForkRoundedTreeInstances,
        V4->ColumnarNarrowTreeInstances,
        V4->PalmTreeInstances,
        V4->HeritageUmbrellaInstances,
        V4->HeritageDomeInstances,
        V4->HeritageHighForkRoundedInstances,
        V4->HeritageColumnarNarrowInstances,
        V4->HeritagePalmInstances,
        V4->ShrubInstances,
        V4->FlowerInstances,
        V4->UnderstoreyInstances,
        V4->PorticoV8RenderOnlyComponent,
        V4->PorticoV5CRenderOnlyComponent,
        V4->HeritageAnchorPawnBlockers,
        V4->GeometryGrassInstances,
        V4->CloseTurfInstances};
    for (const UPrimitiveComponent* Admitted : ExactRoster)
    {
        if (Component == Admitted)
        {
            return true;
        }
    }
    return false;
}

FString CanonicalTransform(const FTransform& Transform)
{
    const FVector Translation = Transform.GetTranslation();
    const FQuat Rotation = Transform.GetRotation();
    const FVector Scale = Transform.GetScale3D();
    return FString::Printf(
        TEXT("T=%.17g,%.17g,%.17g|Q=%.17g,%.17g,%.17g,%.17g|S=%.17g,%.17g,%.17g"),
        Translation.X, Translation.Y, Translation.Z,
        Rotation.X, Rotation.Y, Rotation.Z, Rotation.W,
        Scale.X, Scale.Y, Scale.Z);
}

FString CanonicalVector(const FVector& Vector)
{
    return FString::Printf(
        TEXT("%.17g,%.17g,%.17g"), Vector.X, Vector.Y, Vector.Z);
}

FString NormalizeWorldLocalIdentity(const UWorld* World, FString Value)
{
    const FString PackageName = World && World->GetOutermost()
        ? World->GetOutermost()->GetName()
        : FString();
    const FString AssetName = !PackageName.IsEmpty()
        ? FPackageName::GetLongPackageAssetName(PackageName)
        : FString();
    if (!PackageName.IsEmpty())
    {
        Value.ReplaceInline(*PackageName, TEXT("<MAP_PACKAGE>"));
    }
    if (!AssetName.IsEmpty())
    {
        Value.ReplaceInline(*AssetName, TEXT("<MAP_ASSET>"));
    }
    Value.ReplaceInline(
        TEXT("Istana_PublicView_Explore_v4"), TEXT("<MAP_ASSET>"));
    Value.ReplaceInline(
        TEXT("Istana_PublicView_Explore_v5"), TEXT("<MAP_ASSET>"));
    return Value;
}

FString CanonicalEditableObjectState(
    const UWorld* World,
    const UObject* Object)
{
    if (!Object)
    {
        return TEXT("<null>");
    }
    TArray<const FProperty*> Properties;
    for (TFieldIterator<FProperty> It(
             Object->GetClass(), EFieldIteratorFlags::IncludeSuper);
         It;
         ++It)
    {
        const FProperty* Property = *It;
        if (Property && Property->HasAnyPropertyFlags(CPF_Edit) &&
            !Property->HasAnyPropertyFlags(
                CPF_Transient | CPF_DuplicateTransient |
                CPF_NonPIEDuplicateTransient | CPF_Deprecated |
                CPF_SkipSerialization))
        {
            Properties.Add(Property);
        }
    }
    Properties.Sort([](const FProperty& A, const FProperty& B)
    {
        return A.GetFullName() < B.GetFullName();
    });

    FString State = TEXT("path=") + NormalizeWorldLocalIdentity(
        World, Object->GetPathName()) + TEXT("|class=") +
        NormalizeWorldLocalIdentity(
            World, Object->GetClass()->GetPathName());
    for (const FProperty* Property : Properties)
    {
        for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
        {
            FString Value;
            Property->ExportTextItem_Direct(
                Value,
                Property->ContainerPtrToValuePtr<void>(Object, Index),
                nullptr,
                const_cast<UObject*>(Object),
                PPF_None);
            State += FString::Printf(
                TEXT("|%s[%d]=%s"),
                *Property->GetFullName(),
                Index,
                *NormalizeWorldLocalIdentity(World, MoveTemp(Value)));
        }
    }
    return State;
}

/**
 * Canonical readback of every inherited actor/component property within V5's
 * authority boundary.  Only the five named appearance slots and the additive
 * V5 actor are normalized away; the appearance actor validates those deltas.
 */
bool CaptureInheritedWorldRows(
    UWorld* World,
    TArray<FString>& OutRows,
    FString& OutError)
{
    int32 SceneCount = 0;
    int32 V4Count = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    if (!World || SceneCount != 1 || V4Count != 1 || !Scene || !V4)
    {
        OutError = TEXT("Exactly one inherited public-view Scene and V4 actor are required.");
        return false;
    }

    OutRows.Reset();
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        // Compare only persisted inherited map state. Active editor worlds own
        // transient navigation actors (for example AbstractNavData-Default)
        // that an inactive LoadObject baseline intentionally does not create.
        if (!Actor || Actor->HasAnyFlags(RF_Transient) ||
            Actor->IsA<ATRIADIstanaExploreV5AppearanceActor>())
        {
            continue;
        }
        TArray<FString> Tags;
        for (const FName Tag : Actor->Tags)
        {
            Tags.Add(Tag.ToString());
        }
        Tags.Sort();
        const FString ActorName = NormalizeWorldLocalIdentity(
            World, Actor->GetName());
        const FString ActorClassPath = NormalizeWorldLocalIdentity(
            World, Actor->GetClass()->GetPathName());
        FString ActorRow = FString::Printf(
            TEXT("A|%s|%s|%s|hidden=%d|collision=%d|tags=%s"),
            *ActorName,
            *ActorClassPath,
            *CanonicalTransform(Actor->GetActorTransform()),
            Actor->IsHidden() ? 1 : 0,
            Actor->GetActorEnableCollision() ? 1 : 0,
            *FString::Join(Tags, TEXT(",")));
        if (const AWorldSettings* WorldSettings =
                Cast<AWorldSettings>(Actor))
        {
            const FObjectPropertyBase* NavigationConfigProperty =
                FindFProperty<FObjectPropertyBase>(
                    WorldSettings->GetClass(),
                    FName(TEXT("NavigationSystemConfig")));
            if (!NavigationConfigProperty)
            {
                OutError = TEXT("WorldSettings has no persisted NavigationSystemConfig property.");
                return false;
            }
            const UObject* NavigationConfig =
                NavigationConfigProperty->GetObjectPropertyValue_InContainer(
                    WorldSettings);
            ActorRow += TEXT("|navigationConfig=") +
                CanonicalEditableObjectState(World, NavigationConfig);
        }
        OutRows.Add(MoveTemp(ActorRow));

        TInlineComponentArray<UActorComponent*> Components(Actor);
        Components.Sort([](const UActorComponent& A, const UActorComponent& B)
        {
            return A.GetName() < B.GetName();
        });
        for (UActorComponent* Component : Components)
        {
            if (!Component)
            {
                OutError = TEXT("An inherited actor contains a null component.");
                return false;
            }
            if (Component->HasAnyFlags(RF_Transient))
            {
                continue;
            }
            const FString ComponentName = NormalizeWorldLocalIdentity(
                World, Component->GetName());
            const FString ComponentClassPath = NormalizeWorldLocalIdentity(
                World, Component->GetClass()->GetPathName());
            FString Row = FString::Printf(
                TEXT("C|%s|%s|%s|auto=%d|nav=%d"),
                *ActorName,
                *ComponentName,
                *ComponentClassPath,
                Component->bAutoActivate ? 1 : 0,
                Component->CanEverAffectNavigation() ? 1 : 0);

            if (const USceneComponent* SceneComponent =
                    Cast<USceneComponent>(Component))
            {
                Row += FString::Printf(
                    TEXT("|relative=%s|world=%s|mobility=%d|attachParent=%s|attachSocket=%s"),
                    *CanonicalTransform(
                        SceneComponent->GetRelativeTransform()),
                    *CanonicalTransform(
                        SceneComponent->GetComponentTransform()),
                    static_cast<int32>(SceneComponent->Mobility.GetValue()),
                    *NormalizeWorldLocalIdentity(
                        World,
                        GetPathNameSafe(SceneComponent->GetAttachParent())),
                    *SceneComponent->GetAttachSocketName().ToString());
            }
            if (const UPrimitiveComponent* Primitive =
                    Cast<UPrimitiveComponent>(Component))
            {
                Row += FString::Printf(
                    TEXT("|visible=%d|hiddenInGame=%d|castShadow=%d|contactShadow=%s|customDepth=%d|customStencil=%d|ownerNoSee=%d|onlyOwnerSee=%d|reflectionCapture=%d|realTimeSkyCapture=%d|sceneCaptureOnly=%d|hiddenSceneCapture=%d|rayTracing=%d|mainPass=%d|depthPass=%d|collisionProfile=%s|collisionEnabled=%d|objectType=%d|overlap=%d|responses="),
                    Primitive->IsVisible() ? 1 : 0,
                    Primitive->bHiddenInGame ? 1 : 0,
                    Primitive->CastShadow ? 1 : 0,
                    IsAdmittedContactShadowComponent(Scene, V4, Primitive)
                        ? TEXT("<V5_CONTACT_ROSTER>")
                        : (Primitive->bCastContactShadow
                            ? TEXT("1")
                            : TEXT("0")),
                    Primitive->bRenderCustomDepth ? 1 : 0,
                    Primitive->CustomDepthStencilValue,
                    Primitive->bOwnerNoSee ? 1 : 0,
                    Primitive->bOnlyOwnerSee ? 1 : 0,
                    Primitive->bVisibleInReflectionCaptures ? 1 : 0,
                    Primitive->bVisibleInRealTimeSkyCaptures ? 1 : 0,
                    Primitive->bVisibleInSceneCaptureOnly ? 1 : 0,
                    Primitive->bHiddenInSceneCapture ? 1 : 0,
                    Primitive->bVisibleInRayTracing ? 1 : 0,
                    Primitive->bRenderInMainPass ? 1 : 0,
                    Primitive->bRenderInDepthPass ? 1 : 0,
                    *Primitive->GetCollisionProfileName().ToString(),
                    static_cast<int32>(Primitive->GetCollisionEnabled()),
                    static_cast<int32>(Primitive->GetCollisionObjectType()),
                    Primitive->GetGenerateOverlapEvents() ? 1 : 0);
                for (int32 Channel = 0;
                     Channel <= static_cast<int32>(ECC_GameTraceChannel18);
                     ++Channel)
                {
                    Row += FString::FromInt(static_cast<int32>(
                        Primitive->GetCollisionResponseToChannel(
                            static_cast<ECollisionChannel>(Channel))));
                }
            }
            if (const UBoxComponent* Box = Cast<UBoxComponent>(Component))
            {
                Row += TEXT("|shape=box|unscaledExtent=") +
                    CanonicalVector(Box->GetUnscaledBoxExtent());
            }
            else if (const UCapsuleComponent* Capsule =
                         Cast<UCapsuleComponent>(Component))
            {
                Row += FString::Printf(
                    TEXT("|shape=capsule|unscaledRadius=%.17g|unscaledHalfHeight=%.17g"),
                    Capsule->GetUnscaledCapsuleRadius(),
                    Capsule->GetUnscaledCapsuleHalfHeight());
            }
            else if (const USphereComponent* Sphere =
                         Cast<USphereComponent>(Component))
            {
                Row += FString::Printf(
                    TEXT("|shape=sphere|unscaledRadius=%.17g"),
                    Sphere->GetUnscaledSphereRadius());
            }
            if (const UStaticMeshComponent* StaticMesh =
                    Cast<UStaticMeshComponent>(Component))
            {
                Row += FString::Printf(
                    TEXT("|mesh=%s|forcedLod=%d|overrideMinLod=%d|minLod=%d|wpoDisable=%d|materials="),
                    *NormalizeWorldLocalIdentity(
                        World,
                        GetPathNameSafe(StaticMesh->GetStaticMesh())),
                    StaticMesh->ForcedLodModel,
                    StaticMesh->bOverrideMinLOD ? 1 : 0,
                    StaticMesh->MinLOD,
                    StaticMesh->WorldPositionOffsetDisableDistance);
                for (int32 Slot = 0;
                     Slot < StaticMesh->GetNumMaterials();
                     ++Slot)
                {
                    Row += IsAdmittedAppearanceMaterialSlot(
                            Scene, StaticMesh, Slot)
                        ? TEXT("<V5_APPEARANCE_SLOT>")
                        : NormalizeWorldLocalIdentity(
                            World,
                            GetPathNameSafe(StaticMesh->GetMaterial(Slot)));
                    Row += TEXT(";");
                }
            }
            if (const UInstancedStaticMeshComponent* Instances =
                    Cast<UInstancedStaticMeshComponent>(Component))
            {
                Row += FString::Printf(
                    TEXT("|instances=%d|startCull=%d|endCull=%d|instanceTransforms="),
                    Instances->GetInstanceCount(),
                    Instances->InstanceStartCullDistance,
                    Instances->InstanceEndCullDistance);
                for (int32 Index = 0;
                     Index < Instances->GetInstanceCount();
                     ++Index)
                {
                    FTransform Transform;
                    if (!Instances->GetInstanceTransform(Index, Transform, false))
                    {
                        OutError = TEXT("An inherited instance transform could not be read.");
                        return false;
                    }
                    Row += CanonicalTransform(Transform) + TEXT(";");
                }
            }
            if (const ULightComponent* Light = Cast<ULightComponent>(Component))
            {
                const FLinearColor LightColor = Light->GetLightColor();
                Row += FString::Printf(
                    TEXT("|lightIntensity=%.9g|lightColor=%.9g,%.9g,%.9g,%.9g"),
                    Light->Intensity,
                    LightColor.R,
                    LightColor.G,
                    LightColor.B,
                    LightColor.A);
            }
            if (const USkyLightComponent* SkyLight =
                    Cast<USkyLightComponent>(Component))
            {
                const FLinearColor SkyColor = SkyLight->GetLightColor();
                Row += FString::Printf(
                    TEXT("|skyIntensity=%.9g|skyColor=%.9g,%.9g,%.9g,%.9g|realTimeCapture=%d|sourceType=%d|cubemap=%s|cubemapAngle=%.9g|cubemapResolution=%d|skyDistance=%.9g|captureEmissiveOnly=%d|lowerHemisphereBlack=%d|lowerHemisphere=%.9g,%.9g,%.9g,%.9g"),
                    SkyLight->Intensity,
                    SkyColor.R,
                    SkyColor.G,
                    SkyColor.B,
                    SkyColor.A,
                    SkyLight->bRealTimeCapture ? 1 : 0,
                    static_cast<int32>(SkyLight->SourceType.GetValue()),
                    *NormalizeWorldLocalIdentity(
                        World, GetPathNameSafe(SkyLight->Cubemap)),
                    SkyLight->SourceCubemapAngle,
                    SkyLight->CubemapResolution,
                    SkyLight->SkyDistanceThreshold,
                    SkyLight->bCaptureEmissiveOnly ? 1 : 0,
                    SkyLight->bLowerHemisphereIsBlack ? 1 : 0,
                    SkyLight->LowerHemisphereColor.R,
                    SkyLight->LowerHemisphereColor.G,
                    SkyLight->LowerHemisphereColor.B,
                    SkyLight->LowerHemisphereColor.A);
            }
            OutRows.Add(MoveTemp(Row));
        }
    }
    OutRows.Sort();
    OutError.Reset();
    return true;
}

bool CompareInheritedWorlds(
    UWorld* Source,
    UWorld* Candidate,
    FString& OutError)
{
    TArray<FString> SourceRows;
    TArray<FString> CandidateRows;
    if (!CaptureInheritedWorldRows(Source, SourceRows, OutError) ||
        !CaptureInheritedWorldRows(Candidate, CandidateRows, OutError))
    {
        return false;
    }
    if (SourceRows != CandidateRows)
    {
        int32 Difference = 0;
        while (SourceRows.IsValidIndex(Difference) &&
               CandidateRows.IsValidIndex(Difference) &&
               SourceRows[Difference] == CandidateRows[Difference])
        {
            ++Difference;
        }
        const FString SourceRow = SourceRows.IsValidIndex(Difference)
            ? SourceRows[Difference]
            : TEXT("<SOURCE_END>");
        const FString CandidateRow = CandidateRows.IsValidIndex(Difference)
            ? CandidateRows[Difference]
            : TEXT("<CANDIDATE_END>");
        OutError = FString::Printf(
            TEXT("Inherited V4 geometry/transform/instance/collision/navigation/visibility/light state changed at canonical row %d (sourceRows=%d candidateRows=%d). sourceRow=[%s] candidateRow=[%s]"),
            Difference,
            SourceRows.Num(),
            CandidateRows.Num(),
            *SourceRow,
            *CandidateRow);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateExactLightingCensus(UWorld* World, FString& OutError)
{
    int32 DirectionalLightCount = 0;
    int32 OtherLightCount = 0;
    int32 SkyLightCount = 0;
    int32 SkyAtmosphereCount = 0;
    int32 ExponentialHeightFogCount = 0;

    if (!World)
    {
        OutError = TEXT("The lighting census has no world.");
        return false;
    }
    for (TActorIterator<ALight> It(World); It; ++It)
    {
        if (Cast<ADirectionalLight>(*It))
        {
            ++DirectionalLightCount;
        }
        else
        {
            ++OtherLightCount;
        }
    }
    for (TActorIterator<ASkyLight> It(World); It; ++It)
    {
        ++SkyLightCount;
    }
    for (TActorIterator<ASkyAtmosphere> It(World); It; ++It)
    {
        ++SkyAtmosphereCount;
    }
    for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
    {
        ++ExponentialHeightFogCount;
    }

    if (DirectionalLightCount != 1 || OtherLightCount != 0 ||
        SkyLightCount != 1 || SkyAtmosphereCount != 1 ||
        ExponentialHeightFogCount != 0)
    {
        OutError = FString::Printf(
            TEXT("Require exactly one directional light, one skylight, one sky atmosphere, zero other lights and zero exponential-height fog actors (got directional=%d otherLights=%d skylights=%d skyAtmospheres=%d fog=%d)."),
            DirectionalLightCount,
            OtherLightCount,
            SkyLightCount,
            SkyAtmosphereCount,
            ExponentialHeightFogCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateEffectiveCaptureRenderer(UWorld* World, FString& OutError)
{
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
    const FEngineShowFlags* ShowFlags = ViewportClient
        ? ViewportClient->GetEngineShowFlags()
        : nullptr;
    const IConsoleVariable* SsgiQuality =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSGI.Quality"));
    const IConsoleVariable* SsrQuality =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.SSR.Quality"));

    if (!GameInstance || GameInstance->GetNumLocalPlayers() != 1 ||
        !ViewportClient ||
        ViewportClient->GetCurrentSplitscreenConfiguration() !=
            ESplitScreenType::None ||
        !ShowFlags || !ShowFlags->PostProcessing || !ShowFlags->Lighting ||
        !ShowFlags->Materials || !ShowFlags->GlobalIllumination ||
        !ShowFlags->ScreenSpaceReflections ||
        !SsgiQuality || SsgiQuality->GetInt() <= 0 ||
        !SsrQuality || SsrQuality->GetInt() <= 0)
    {
        OutError = FString::Printf(
            TEXT("Capture renderer is not the exact one-player/no-splitscreen lit-material post-process path with SSGI and SSR enabled (localPlayers=%d split=%d SSGI=%d SSR=%d)."),
            GameInstance ? GameInstance->GetNumLocalPlayers() : -1,
            ViewportClient
                ? static_cast<int32>(
                    ViewportClient->GetCurrentSplitscreenConfiguration())
                : -1,
            SsgiQuality ? SsgiQuality->GetInt() : -1,
            SsrQuality ? SsrQuality->GetInt() : -1);
        return false;
    }
    OutError.Reset();
    return true;
}

UWorld* LoadInactiveV4World(FString& OutError)
{
    UPackage* Package = FindPackage(nullptr, *SourceMapPackage);
    if (Package && Package->IsDirty())
    {
        OutError = TEXT("The already-loaded inactive Explore V4 source package is dirty; disk-exact structural comparison is refused.");
        return nullptr;
    }
    if (!Package)
    {
        Package = LoadPackage(nullptr, *SourceMapPackage, LOAD_None);
    }
    UWorld* World = Package
        ? FindObject<UWorld>(
            Package,
            *FPackageName::GetLongPackageAssetName(SourceMapPackage))
        : nullptr;
    if (!Package || Package->IsDirty() || !World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != SourceMapPackage)
    {
        OutError = TEXT("The exact clean inactive Explore V4 source world could not be loaded for structural comparison.");
        return nullptr;
    }
    OutError.Reset();
    return World;
}

bool ValidateNamedMaterial(
    UStaticMeshComponent* Component,
    const FName SlotName,
    const FString& ExpectedObjectPath,
    FString& OutError)
{
    const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
    int32 Slot = INDEX_NONE;
    int32 Matches = 0;
    if (Mesh)
    {
        const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();
        for (int32 Index = 0; Index < StaticMaterials.Num(); ++Index)
        {
            if (StaticMaterials[Index].MaterialSlotName == SlotName ||
                StaticMaterials[Index].ImportedMaterialSlotName == SlotName)
            {
                Slot = Index;
                ++Matches;
            }
        }
    }
    UMaterialInterface* Material = Slot != INDEX_NONE
        ? Component->GetMaterial(Slot)
        : nullptr;
    if (!Component || Matches != 1 || Slot == INDEX_NONE || !Material ||
        Material->GetPathName() != ExpectedObjectPath)
    {
        OutError = FString::Printf(
            TEXT("Exact V5 named material override '%s' -> '%s' is absent."),
            *SlotName.ToString(),
            *ExpectedObjectPath);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateExactAppearanceSlots(
    ATRIADIstanaPublicViewSceneActor* Scene,
    ATRIADIstanaExploreV5AppearanceActor* Appearance,
    FString& OutError)
{
    if (!Scene || !Appearance || !Scene->TerrainComponent ||
        !Scene->HardscapeComponent || !Scene->OSMContextBuildingsComponent ||
        Scene->TerrainComponent->GetNumMaterials() <= 0 ||
        !Scene->TerrainComponent->GetMaterial(0) ||
        Scene->TerrainComponent->GetMaterial(0)->GetPathName() !=
            ATRIADIstanaExploreV5AppearanceActor::ExpectedLawnMaterialPath() ||
        !ValidateNamedMaterial(
            Scene->HardscapeComponent,
            FName(TEXT("M_IPV_Water")),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedFountainWaterMaterialPath(),
            OutError) ||
        !ValidateNamedMaterial(
            Scene->HardscapeComponent,
            FName(TEXT("M_IPV_Stone")),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedHardscapeStoneMaterialPath(),
            OutError) ||
        !ValidateNamedMaterial(
            Scene->OSMContextBuildingsComponent,
            FName(TEXT("M_IPV_ContextRender")),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedContextRenderMaterialPath(),
            OutError) ||
        !ValidateNamedMaterial(
            Scene->OSMContextBuildingsComponent,
            FName(TEXT("M_IPV_ContextRoof")),
            ATRIADIstanaExploreV5AppearanceActor::ExpectedContextRoofMaterialPath(),
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact five V5 appearance material slots are not bound.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateV5World(UWorld* World, FString& OutReport)
{
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != DestinationMapPackage ||
        World->GetOutermost()->IsDirty())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5_MAP_INVALID: the exact V5 map is not the active editor world.");
        return false;
    }

    FString Error;
    TArray<FPackageFileDigest> FrozenV4;
    FString AssetReport;
    if (!CaptureExactV4MapSnapshot(FrozenV4, Error) ||
        !UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Assets(
            AssetReport) ||
        !ValidatePersistedV5Assets(AssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5_MAP_INVALID: V4/V5 asset or frozen-source preflight failed. ") +
            Error + TEXT(" ") + AssetReport;
        return false;
    }

    int32 SceneCount = 0;
    int32 V4Count = 0;
    int32 AppearanceCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    ATRIADIstanaExploreV5AppearanceActor* Appearance =
        FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(
            World, AppearanceCount);
    if (SceneCount != 1 || V4Count != 1 || AppearanceCount != 1 ||
        !Scene || !V4 || !Appearance ||
        !V4->Tags.Contains(V4LandscapeTag) ||
        !Appearance->Tags.Contains(V5AppearanceTag) ||
        !Appearance->GetActorTransform().Equals(FTransform::Identity, 0.0f) ||
        !World->GetWorldSettings() ||
        World->GetWorldSettings()->DefaultGameMode.Get() !=
            ATRIADIstanaExploreV5GameMode::StaticClass())
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5_MAP_INVALID: require exact game mode, identity tagged appearance actor and exactly one Scene/V4/V5 actor (got %d/%d/%d)."),
            SceneCount, V4Count, AppearanceCount);
        return false;
    }
    if (!ValidateExactLightingCensus(World, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5_MAP_INVALID: exact no-fog public daylight census failed. ") +
            Error;
        return false;
    }

    UWorld* SourceWorld = LoadInactiveV4World(Error);
    int32 SourceSceneCount = 0;
    int32 SourceV4Count = 0;
    ATRIADIstanaPublicViewSceneActor* SourceScene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(
            SourceWorld, SourceSceneCount);
    ATRIADIstanaExploreV4LandscapeActor* SourceV4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(
            SourceWorld, SourceV4Count);
    FString SourceSceneReport;
    FString SourceV4Report;
    FString CandidateV4Report;
    FString AppearanceReport;
    if (!SourceWorld || SourceSceneCount != 1 || SourceV4Count != 1 ||
        !SourceScene || !SourceV4 ||
        !SourceScene->ValidatePublicViewScene(SourceSceneReport, false) ||
        !SourceV4->ValidateExploreV4Landscape(SourceV4Report) ||
        !V4->ValidateExploreV4Landscape(CandidateV4Report) ||
        !CompareInheritedWorlds(SourceWorld, World, Error) ||
        !ValidateExactAppearanceSlots(Scene, Appearance, Error) ||
        !Appearance->ValidateExploreV5Appearance(
            AppearanceReport, false) ||
        !ValidateExactV4MapSnapshot(FrozenV4, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5_MAP_INVALID: inherited V4 structural/material-boundary or V5 appearance validation failed. ") +
            Error + TEXT(" sourceScene=") + SourceSceneReport +
            TEXT(" sourceV4=") + SourceV4Report +
            TEXT(" candidateV4=") + CandidateV4Report +
            TEXT(" appearance=") + AppearanceReport;
        return false;
    }

    OutReport = TEXT("Validated additive Explore V5: exact 12,153,074-byte/D31EBF...DEBA72 V4 source plus every UE5.5 package sidecar/external actor-object tree remain byte-identical; one inherited Scene and one structurally valid V4 landscape are unchanged in geometry, transforms, instance census, visibility, collision and navigation; inherited sun pose/colour/intensity remain unchanged while V5 alone owns its bounded contact-shadow light-field changes; exactly one identity V5 appearance actor owns only five exact named material overrides and four exact Foliage008 close-turf textures; exact V5 game mode/pawn is selected. No survey, as-built, botanical, collision, navigation, sensor or RF claim was added.");
    return true;
}

bool GetLightweightAcceptedV5PlayState(
    UWorld*& OutPlayWorld,
    APlayerController*& OutPlayer,
    ATRIADIstanaExploreV5Pawn*& OutPawn,
    ATRIADIstanaExploreV4LandscapeActor*& OutV4,
    ATRIADIstanaExploreV5AppearanceActor*& OutAppearance,
    bool bRequirePriorFullAcceptance,
    FString& OutError)
{
    UWorld* EditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    if (!EditorWorld || !EditorWorld->GetOutermost() ||
        EditorWorld->GetOutermost()->GetName() != DestinationMapPackage ||
        EditorWorld->GetOutermost()->IsDirty())
    {
        OutError = TEXT("The accepted exact V5 editor map is absent or dirty.");
        return false;
    }
    OutPlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!OutPlayWorld || OutPlayWorld->WorldType != EWorldType::PIE ||
        !OutPlayWorld->IsGameWorld() || !OutPlayWorld->HasBegunPlay() ||
        UWorld::RemovePIEPrefix(
            OutPlayWorld->GetOutermost()->GetName()) != DestinationMapPackage)
    {
        OutError = TEXT("The exact begun Explore V5 PIE world is absent.");
        return false;
    }
    if (bRequirePriorFullAcceptance &&
        (FullyAcceptedQaEditorWorld.Get() != EditorWorld ||
         FullyAcceptedQaPlayWorld.Get() != OutPlayWorld))
    {
        OutError = TEXT("Run ValidateIstanaExploreV5PlayWorld once for this exact editor/PIE pair.");
        return false;
    }

    OutPlayer = UGameplayStatics::GetPlayerController(OutPlayWorld, 0);
    OutPawn = OutPlayer
        ? Cast<ATRIADIstanaExploreV5Pawn>(OutPlayer->GetPawn())
        : nullptr;
    int32 SceneCount = 0;
    int32 V2Count = 0;
    int32 V3Count = 0;
    int32 V4Count = 0;
    int32 AppearanceCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(
            OutPlayWorld, SceneCount);
    ATRIADIstanaExploreV2LandscapeActor* V2 =
        FindExactlyOne<ATRIADIstanaExploreV2LandscapeActor>(
            OutPlayWorld, V2Count);
    ATRIADIstanaExploreV3SupplementActor* V3 =
        FindExactlyOne<ATRIADIstanaExploreV3SupplementActor>(
            OutPlayWorld, V3Count);
    OutV4 = FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(
        OutPlayWorld, V4Count);
    OutAppearance = FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(
        OutPlayWorld, AppearanceCount);
    FString RuntimeAppearanceReport;
    AGameModeBase* RuntimeGameMode = OutPlayWorld->GetAuthGameMode();
    if (!RuntimeGameMode ||
        RuntimeGameMode->GetClass() !=
            ATRIADIstanaExploreV5GameMode::StaticClass() ||
        !OutPlayer || !OutPawn ||
        !OutPawn->HasExpectedExploreV5CameraProfile() ||
        OutPlayer->GetViewTarget() != OutPawn ||
        !OutPawn->HasActorBegunPlay() ||
        SceneCount != 1 || !Scene || !Scene->HasActorBegunPlay() ||
        V2Count != 1 || !V2 || !V2->HasActorBegunPlay() ||
        !V2->IsWindRuntimeActive() ||
        V3Count != 1 || !V3 || !V3->HasActorBegunPlay() ||
        !V3->IsWindRuntimeActive() ||
        V4Count != 1 || !OutV4 || !OutV4->HasActorBegunPlay() ||
        !OutV4->IsWindRuntimeActive() ||
        AppearanceCount != 1 || !OutAppearance ||
        !OutAppearance->HasActorBegunPlay() ||
        !OutAppearance->ValidateExploreV5Appearance(
            RuntimeAppearanceReport, true) ||
        !ValidateExactLightingCensus(OutPlayWorld, OutError) ||
        !ValidateEffectiveCaptureRenderer(OutPlayWorld, OutError))
    {
        OutError = FString::Printf(
            TEXT("V5 PIE Player0/V5-pawn/view-target, exact Scene/V2/V3/V4 runtime roster and inherited wind, or deferred close-turf appearance identity failed (Scene/V2/V3/V4/V5Appearance=%d/%d/%d/%d/%d). %s"),
            SceneCount,
            V2Count,
            V3Count,
            V4Count,
            AppearanceCount,
            *(RuntimeAppearanceReport + TEXT(" ") + OutError));
        return false;
    }
    OutError.Reset();
    return true;
}

bool GetValidatedV5PlayState(
    UWorld*& OutPlayWorld,
    APlayerController*& OutPlayer,
    ATRIADIstanaExploreV5Pawn*& OutPawn,
    ATRIADIstanaExploreV4LandscapeActor*& OutV4,
    ATRIADIstanaExploreV5AppearanceActor*& OutAppearance,
    FString& OutError)
{
    FullyAcceptedQaEditorWorld.Reset();
    FullyAcceptedQaPlayWorld.Reset();
    UWorld* EditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString EditorReport;
    if (!ValidateV5World(EditorWorld, EditorReport) ||
        !GetLightweightAcceptedV5PlayState(
            OutPlayWorld,
            OutPlayer,
            OutPawn,
            OutV4,
            OutAppearance,
            false,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5 editor-map validation failed: ") + EditorReport;
        }
        return false;
    }
    FString V4Report;
    FString AppearanceReport;
    if (!OutV4->ValidateExploreV4Landscape(V4Report) ||
        !OutAppearance->ValidateExploreV5Appearance(
            AppearanceReport, true))
    {
        OutError = TEXT("V5 full runtime acceptance failed: ") +
            V4Report + TEXT(" ") + AppearanceReport;
        return false;
    }
    FullyAcceptedQaEditorWorld = EditorWorld;
    FullyAcceptedQaPlayWorld = OutPlayWorld;
    OutError.Reset();
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5EditorLibrary::ImportIstanaExploreV5Assets(
    FString& OutMessage)
{
    FString ExistingReport;
    if (ValidatePersistedV5Assets(ExistingReport))
    {
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!Assets)
    {
        OutMessage = TEXT("EXPLORE_V5_IMPORT_REFUSED: the editor asset subsystem is unavailable before fresh asset creation.");
        return false;
    }
    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5MaterialFactory::CreateFreshExploreV5Materials(
            FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5_IMPORT_REFUSED_OR_FAILED: ") + Error;
        return false;
    }
    const FString Root =
        TRIADIstanaExploreV5MaterialFactory::GetMaterialRootPath() + TEXT("/");
    if (FreshAssets.Num() != 6 || FreshAssets.Contains(nullptr))
    {
        OutMessage = TEXT("EXPLORE_V5_IMPORT_FAILED: helper did not return the exact six fresh V5-owned material assets.");
        return false;
    }
    TSet<FString> ExpectedFreshPaths;
    for (const FString& ExpectedPath : ExactV5MaterialObjectPaths())
    {
        ExpectedFreshPaths.Add(ExpectedPath);
    }
    for (UObject* Asset : FreshAssets)
    {
        if (!Asset->GetPathName().StartsWith(Root) ||
            !ExpectedFreshPaths.Remove(Asset->GetPathName()) ||
            !Asset->GetOutermost() || !Asset->GetOutermost()->IsDirty())
        {
            OutMessage = TEXT("EXPLORE_V5_IMPORT_FAILED: a helper result escaped the fresh V5 material namespace/dirty-package boundary.");
            return false;
        }
    }
    if (ExpectedFreshPaths.Num() != 0)
    {
        OutMessage = TEXT("EXPLORE_V5_IMPORT_FAILED: helper fresh roster omitted an exact V5 material path.");
        return false;
    }
    if (!Assets->SaveLoadedAssets(FreshAssets, false) ||
        !ValidatePersistedV5Assets(Error))
    {
        OutMessage = TEXT("EXPLORE_V5_IMPORT_FAILED: only the six fresh V5 assets were offered to save, but post-save validation failed. ") +
            Error;
        return false;
    }
    OutMessage = TEXT("Imported and validated the fresh Explore V5 appearance namespace: one original grass base plus five exact appearance materials. No inherited V4 package was saved.");
    return true;
}

bool UTRIADIstanaExploreV5EditorLibrary::ValidateIstanaExploreV5Assets(
    FString& OutReport)
{
    return ValidatePersistedV5Assets(OutReport);
}

bool UTRIADIstanaExploreV5EditorLibrary::BuildIstanaExploreV5Map(
    FString& OutMessage)
{
    FString Error;
    if (!ValidateMapLoadSafety(Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_MAP_LOAD_SAFETY: ") +
            Error;
        return false;
    }
    TArray<FPackageFileDigest> FrozenV4;
    if (!CaptureExactV4MapSnapshot(FrozenV4, Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_FROZEN_V4: ") + Error;
        return false;
    }

    FString V4AssetReport;
    if (!UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Assets(
            V4AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_V4_ASSETS: ") +
            V4AssetReport;
        return false;
    }
    FString SourceFilename;
    if (!FPackageName::DoesPackageExist(
            SourceMapPackage, &SourceFilename))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_V4_LOAD: the exact V4 source package disappeared after its frozen snapshot.");
        return false;
    }
    if (!ValidateMapLoadSafety(Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_V4_LOAD: ") + Error;
        return false;
    }
    UWorld* SourceWorld =
        UEditorLoadingAndSavingUtils::LoadMap(SourceFilename);
    FString V4MapReport;
    if (!SourceWorld || !SourceWorld->GetOutermost() ||
        SourceWorld->GetOutermost()->GetName() != SourceMapPackage ||
        SourceWorld->GetOutermost()->IsDirty() ||
        !UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Map(
            V4MapReport) ||
        SourceWorld->GetOutermost()->IsDirty() ||
        !ValidateExactV4MapSnapshot(FrozenV4, Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_V4_MAP: ") +
            V4MapReport + TEXT(" ") + Error;
        return false;
    }

    TArray<FString> ExactSourceRows;
    if (!CaptureInheritedWorldRows(SourceWorld, ExactSourceRows, Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_V4_STRUCTURE: ") + Error;
        return false;
    }
    FString V5AssetReport;
    if (!ValidatePersistedV5Assets(V5AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_V5_ASSETS: run ImportIstanaExploreV5Assets first. ") +
            V5AssetReport;
        return false;
    }

    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!Assets)
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED: editor asset subsystem is unavailable.");
        return false;
    }
    bool bAnyTargetExists = false;
    FString ExistingDetail;
    if (!InspectDestinationMapPresence(
            Assets, bAnyTargetExists, ExistingDetail))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_DESTINATION_PREFLIGHT: ") +
            ExistingDetail;
        return false;
    }
    if (bAnyTargetExists)
    {
        FString ExistingFilename;
        const bool bDiskTargetExists = FPackageName::DoesPackageExist(
            DestinationMapPackage, &ExistingFilename);
        if (bDiskTargetExists && !ValidateMapLoadSafety(Error))
        {
            OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_EXISTING_LOAD_SAFETY: ") +
                Error;
            return false;
        }
        UWorld* ExistingWorld = bDiskTargetExists
            ? UEditorLoadingAndSavingUtils::LoadMap(ExistingFilename)
            : nullptr;
        FString ExistingReport;
        if (ExistingWorld && ValidateV5World(ExistingWorld, ExistingReport) &&
            ValidateExactV4MapSnapshot(FrozenV4, Error))
        {
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5_MAP_ALREADY_VALID: ") +
                ExistingReport;
            return true;
        }
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_PARTIAL_DESTINATION: /Game/Maps/Istana_PublicView_Explore_v5 already exists but is not exact; overwrite/repair is refused. ") +
            ExistingReport + TEXT(" found=") + ExistingDetail +
            TEXT(" ") + Error;
        return false;
    }

    SourceWorld = nullptr;
    if (!ValidateMapLoadSafety(Error) ||
        !FEditorFileUtils::LoadMap(SourceFilename, true, false))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_FAILED_TEMPLATE: exact V4 map could not be opened safely as a non-destructive untitled duplicate. ") +
            Error;
        return false;
    }
    UWorld* TargetWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    TArray<FString> TemplateRows;
    int32 TemplateAppearanceCount = 0;
    FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(
        TargetWorld, TemplateAppearanceCount);
    if (!TargetWorld || !TargetWorld->GetOutermost() ||
        !FPackageName::IsTempPackage(TargetWorld->GetOutermost()->GetName()) ||
        TemplateAppearanceCount != 0 ||
        !CaptureInheritedWorldRows(TargetWorld, TemplateRows, Error) ||
        TemplateRows != ExactSourceRows ||
        !ValidateExactV4MapSnapshot(FrozenV4, Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_FAILED_TEMPLATE_GATE: untitled V4 duplication was not exact before V5 mutation. ") +
            Error;
        return false;
    }

    int32 SceneCount = 0;
    int32 V4Count = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(TargetWorld, SceneCount);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(TargetWorld, V4Count);
    UMaterialInterface* Lawn = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5MaterialFactory::GetGrassMaterialInstanceObjectPath());
    UMaterialInterface* Water = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5MaterialFactory::GetFountainWaterMaterialObjectPath());
    UMaterialInterface* Stone = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5MaterialFactory::GetHardscapeStoneMaterialInstanceObjectPath());
    UMaterialInterface* ContextRender = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5MaterialFactory::GetContextRenderMaterialInstanceObjectPath());
    UMaterialInterface* ContextRoof = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5MaterialFactory::GetContextRoofMaterialInstanceObjectPath());
    UTexture2D* TurfBaseColor = LoadExact<UTexture2D>(
        ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfBaseColorTexturePath());
    UTexture2D* TurfNormal = LoadExact<UTexture2D>(
        ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfNormalTexturePath());
    UTexture2D* TurfRoughness = LoadExact<UTexture2D>(
        ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfRoughnessTexturePath());
    UTexture2D* TurfOpacity = LoadExact<UTexture2D>(
        ATRIADIstanaExploreV5AppearanceActor::ExpectedCloseTurfOpacityTexturePath());
    if (SceneCount != 1 || V4Count != 1 || !Scene || !V4 ||
        !Lawn || !Water || !Stone || !ContextRender || !ContextRoof ||
        !TurfBaseColor || !TurfNormal || !TurfRoughness || !TurfOpacity ||
        !TargetWorld->GetWorldSettings())
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_FAILED_ROSTER: exact Scene/V4 actor, five V5 materials, four Foliage008 textures or WorldSettings is absent.");
        return false;
    }

    TargetWorld->GetWorldSettings()->Modify();
    TargetWorld->GetWorldSettings()->DefaultGameMode =
        ATRIADIstanaExploreV5GameMode::StaticClass();
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIADIstanaExploreAppearanceV5");
    SpawnParameters.OverrideLevel = TargetWorld->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADIstanaExploreV5AppearanceActor* Appearance =
        TargetWorld->SpawnActor<ATRIADIstanaExploreV5AppearanceActor>(
            ATRIADIstanaExploreV5AppearanceActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    FString AppearanceReport;
    if (!Appearance)
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_FAILED_SPAWN: identity V5 appearance actor could not be spawned.");
        return false;
    }
    Appearance->Tags.AddUnique(V5AppearanceTag);
    Appearance->SetActorLabel(
        TEXT("TRIAD Istana Explore V5 Appearance-Only Public-Data Approximation"));
    if (!Appearance->ConfigureExploreV5Appearance(
            Scene,
            V4,
            Lawn,
            Water,
            Stone,
            ContextRender,
            ContextRoof,
            TurfBaseColor,
            TurfNormal,
            TurfRoughness,
            TurfOpacity,
            Error) ||
        !Appearance->ReapplyExploreV5Appearance(Error) ||
        !Appearance->ValidateExploreV5Appearance(
            AppearanceReport, false) ||
        !ValidateExactAppearanceSlots(Scene, Appearance, Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_FAILED_APPEARANCE: ") +
            Error + TEXT(" ") + AppearanceReport;
        return false;
    }

    TArray<FString> MutatedRows;
    int32 FinalAppearanceCount = 0;
    FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(
        TargetWorld, FinalAppearanceCount);
    FString V4StructuralReport;
    if (FinalAppearanceCount != 1 ||
        !V4->ValidateExploreV4Landscape(V4StructuralReport) ||
        !CaptureInheritedWorldRows(TargetWorld, MutatedRows, Error) ||
        MutatedRows != ExactSourceRows ||
        !ValidateExactV4MapSnapshot(FrozenV4, Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_FAILED_INHERITANCE: a non-appearance V4 state changed before save. ") +
            Error + TEXT(" ") + V4StructuralReport;
        return false;
    }

    bool bLateDestinationExists = false;
    FString LateDestinationDetail;
    if (!InspectDestinationMapPresence(
            Assets, bLateDestinationExists, LateDestinationDetail))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_LATE_DESTINATION_PREFLIGHT: ") +
            LateDestinationDetail;
        return false;
    }
    if (bLateDestinationExists)
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_REFUSED_LATE_DESTINATION: target appeared after preflight; no overwrite was attempted. found=") +
            LateDestinationDetail;
        return false;
    }
    TargetWorld->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            TargetWorld, DestinationMapPackage) ||
        !TargetWorld->GetOutermost() ||
        TargetWorld->GetOutermost()->GetName() != DestinationMapPackage ||
        !ValidateExactV4MapSnapshot(FrozenV4, Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_FAILED_SAVE: only the new V5 target was offered to save; it is now a refused partial destination. ") +
            Error;
        return false;
    }

    FString SourceReloadFilename;
    UWorld* ReloadedSource = nullptr;
    if (!FPackageName::DoesPackageExist(
            SourceMapPackage, &SourceReloadFilename))
    {
        Error = TEXT("The V4 source package disappeared before cold unload.");
    }
    else if (ValidateMapLoadSafety(Error))
    {
        ReloadedSource =
            UEditorLoadingAndSavingUtils::LoadMap(SourceReloadFilename);
    }
    FString PostV4Report;
    if (!ReloadedSource || !ReloadedSource->GetOutermost() ||
        ReloadedSource->GetOutermost()->IsDirty() ||
        !UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Map(
            PostV4Report) ||
        ReloadedSource->GetOutermost()->IsDirty() ||
        !ValidateExactV4MapSnapshot(FrozenV4, Error) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_FAILED_COLD_UNLOAD: V5 could not be unloaded at the still-exact public-validated V4 source. ") +
            PostV4Report + TEXT(" ") + Error;
        return false;
    }

    FString DestinationFilename;
    UWorld* ReloadedTarget = nullptr;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename))
    {
        Error = TEXT("The new V5 destination package is absent before cold reload.");
    }
    else if (ValidateMapLoadSafety(Error))
    {
        ReloadedTarget =
            UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    }
    FString ColdReport;
    if (!ReloadedTarget || !ValidateV5World(ReloadedTarget, ColdReport) ||
        !ValidateExactV4MapSnapshot(FrozenV4, Error))
    {
        OutMessage = TEXT("EXPLORE_V5_BUILD_FAILED_COLD_RELOAD: saved V5 map failed exact cold validation and remains a refused partial destination. ") +
            ColdReport + TEXT(" ") + Error;
        return false;
    }

    OutMessage = TEXT("Created and cold-reload-validated /Game/Maps/Istana_PublicView_Explore_v5 as an additive appearance-only duplicate of the exact frozen V4 map. Exactly one V5 actor applies five named material overrides plus four exact Foliage008 runtime turf textures; V5 game mode/pawn is selected; inherited geometry, transforms, instances, hidden states, collision, navigation, sensor/RF truth and sun pose/colour/intensity remain exact, while V5 alone owns its bounded contact-shadow light-field changes. ") +
        ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5EditorLibrary::ValidateIstanaExploreV5Map(
    FString& OutReport)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    return ValidateV5World(World, OutReport);
}

bool UTRIADIstanaExploreV5EditorLibrary::ValidateIstanaExploreV5PlayWorld(
    FString& OutReport)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* Appearance = nullptr;
    FString Error;
    if (!GetValidatedV5PlayState(
            PlayWorld, Player, Pawn, V4, Appearance, Error) ||
        !Pawn->HasExpectedExploreV5CameraProfile())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5_PIE_INVALID: ") + Error;
        return false;
    }
    OutReport = TEXT("Explore V5 PIE is live: Player0 owns and views the exact V5 manual-exposure pawn; the exact Scene/V2/V3/V4 runtime census and all inherited wind layers are active; the deferred close-turf MID has all four exact Foliage008 textures; the five static appearance slots and appearance-only truth boundary validate.");
    return true;
}

bool UTRIADIstanaExploreV5EditorLibrary::CaptureIstanaExploreV5PlayView(
    const FString& OutputFileName,
    FString& OutMessage)
{
    const bool bV4Filename = OutputFileName.StartsWith(
        TEXT("explore_v4_"), ESearchCase::IgnoreCase);
    const bool bV5Filename = OutputFileName.StartsWith(
        TEXT("explore_v5_"), ESearchCase::IgnoreCase);
    bool bHasControlCharacter = false;
    for (const TCHAR Character : OutputFileName)
    {
        bHasControlCharacter |=
            Character < static_cast<TCHAR>(0x20) ||
            Character == static_cast<TCHAR>(0x7f);
    }
    FText FilenameValidationReason;
    if ((!bV4Filename && !bV5Filename) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        FPaths::MakeValidFileName(OutputFileName) != OutputFileName ||
        !FPaths::ValidatePath(
            OutputFileName, &FilenameValidationReason) ||
        bHasControlCharacter)
    {
        OutMessage = TEXT("Matched Explore capture requires one filesystem-safe exact 'explore_v4_*.png' or 'explore_v5_*.png' filename. ") +
            FilenameValidationReason.ToString();
        return false;
    }

    // Narrow baseline exception: this capture request alone may operate in the
    // exact V4 PIE world so the V4 wrapper can recapture a matched 1920x1080
    // baseline. All validation, teleport, gust and state helpers remain V5-only.
    UWorld* EditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    const FString EditorPackage = EditorWorld && EditorWorld->GetOutermost()
        ? EditorWorld->GetOutermost()->GetName()
        : FString();
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    APlayerController* Player = nullptr;
    FString Validation;
    if (EditorPackage == DestinationMapPackage)
    {
        ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
        ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
        ATRIADIstanaExploreV5AppearanceActor* Appearance = nullptr;
        if (!bV5Filename ||
            !GetLightweightAcceptedV5PlayState(
                PlayWorld,
                Player,
                Pawn,
                V4,
                Appearance,
                true,
                Validation) ||
            !Pawn->HasExpectedExploreV5CameraProfile())
        {
            OutMessage = TEXT("Explore V5 high-resolution capture requires the validated V5 Player0 world and an explore_v5_ filename. ") +
                Validation;
            return false;
        }
    }
    else if (EditorPackage == SourceMapPackage)
    {
        FString V4Report;
        if (!bV4Filename ||
            !UTRIADIstanaExploreV4EditorLibrary::
                ValidateIstanaExploreV4PlayWorld(V4Report))
        {
            OutMessage = TEXT("Explore V4 matched-baseline capture requires the exact validated V4 Player0 world and an explore_v4_ filename. ") +
                V4Report;
            return false;
        }
        PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
        Player = UGameplayStatics::GetPlayerController(PlayWorld, 0);
        ATRIADIstanaFreeRoamPawn* V4Pawn = Player
            ? Cast<ATRIADIstanaFreeRoamPawn>(Player->GetPawn())
            : nullptr;
        int32 V4Count = 0;
        ATRIADIstanaExploreV4LandscapeActor* V4 =
            FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(
                PlayWorld, V4Count);
        if (!PlayWorld || !Player || !V4Pawn ||
            !V4Pawn->HasExpectedExploreCameraProfile() ||
            Player->GetViewTarget() != V4Pawn || V4Count != 1 || !V4 ||
            !V4->HasActorBegunPlay() || !V4->IsWindRuntimeActive())
        {
            OutMessage = TEXT("Explore V4 matched-baseline capture failed its post-validation Player0/pawn/view-target/wind identity gate.");
            return false;
        }
    }
    else
    {
        OutMessage = TEXT("High-resolution matched capture requires the exact Explore V4 or V5 editor map.");
        return false;
    }

    UGameViewportClient* ViewportClient = PlayWorld->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || !Player)
    {
        OutMessage = TEXT("The validated current Player0 game viewport is unavailable.");
        return false;
    }
    if (Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("Matched Explore capture requires an SDR game viewport; UE reports that scene HDR is enabled.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        bV4Filename
            ? TEXT("TRIAD/IstanaPreviews/ExploreV4")
            : TEXT("TRIAD/IstanaPreviews/ExploreV5"));
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Directory, OutputFileName));
    FPaths::NormalizeFilename(Destination);
    FText DestinationValidationReason;
    if (!FPaths::ValidatePath(
            Destination, &DestinationValidationReason) ||
        FPaths::GetCleanFilename(Destination) != OutputFileName)
    {
        OutMessage = TEXT("Matched Explore capture resolved to an unsafe or changed destination path. ") +
            DestinationValidationReason.ToString();
        return false;
    }
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    {
        OutMessage = TEXT("Could not create the matched Explore QA capture directory.");
        return false;
    }
    if (IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("Matched Explore capture refused a changed filename, overwrite or overlapping screenshot request.");
        return false;
    }

    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(1920, 1080, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 1920x1080 high-resolution configuration.");
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
        OutMessage = TEXT("UE rejected the exact 1920x1080 high-resolution game-viewport request.");
        return false;
    }
    OutMessage = TEXT("Accepted exact 1920x1080 HDR-off high-resolution Player0 game-viewport capture to '") +
        Destination +
        TEXT("'. This is request acceptance only; the wrapper must wait for and decode the exact PNG as 1920x1080 with positive bytes. No UI capture path was used.");
    return true;
}

bool UTRIADIstanaExploreV5EditorLibrary::TeleportIstanaExploreV5PlayPawnForQa(
        FVector WorldLocationCentimeters,
        FRotator WorldRotationDegrees,
        FString& OutMessage)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* Appearance = nullptr;
    FString Error;
    if (WorldLocationCentimeters.ContainsNaN() ||
        WorldRotationDegrees.ContainsNaN() ||
        FVector2D(
            WorldLocationCentimeters.X,
            WorldLocationCentimeters.Y).Size() > 95000.0 ||
        WorldLocationCentimeters.Z < 50.0 ||
        WorldLocationCentimeters.Z > 20000.0 ||
        !GetLightweightAcceptedV5PlayState(
            PlayWorld,
            Player,
            Pawn,
            V4,
            Appearance,
            true,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5_QA_TELEPORT_REFUSED: require exact V5 PIE and a finite pose inside the 950 m / 0.5-200 m QA envelope. ") +
            Error;
        return false;
    }
    const FRotator NormalizedRotation = WorldRotationDegrees.GetNormalized();
    const bool bTeleported = Pawn->SetActorLocationAndRotation(
        WorldLocationCentimeters,
        NormalizedRotation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    if (!bTeleported ||
        !Pawn->GetActorLocation().Equals(WorldLocationCentimeters, 0.1f) ||
        !Pawn->GetActorRotation().Equals(NormalizedRotation, 0.1f) ||
        Player->GetViewTarget() != Pawn)
    {
        OutMessage = TEXT("EXPLORE_V5_QA_TELEPORT_FAILED: exact bounded V5 pawn pose/view-target readback failed.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Teleported validated Explore V5 Player0 pawn to %s at %s; viewTargetMatchesPawn=true."),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString());
    return true;
}

bool UTRIADIstanaExploreV5EditorLibrary::TriggerIstanaExploreV5PlayWindGust(
        float PeakStrengthCm,
        FString& OutMessage)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* Appearance = nullptr;
    FString Error;
    if (!FMath::IsFinite(PeakStrengthCm) || PeakStrengthCm < 6.0f ||
        PeakStrengthCm > 120.0f ||
        !GetLightweightAcceptedV5PlayState(
            PlayWorld,
            Player,
            Pawn,
            V4,
            Appearance,
            true,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5_QA_GUST_REFUSED: require exact V5 PIE and a finite 6-120 cm peak. ") +
            Error;
        return false;
    }
    V4->TriggerWindGust(PeakStrengthCm);
    OutMessage = FString::Printf(
        TEXT("Triggered deterministic inherited V4 wind on V5 at %.3f cm; %s"),
        PeakStrengthCm,
        *V4->BuildWindRuntimeStateReport());
    return true;
}

bool UTRIADIstanaExploreV5EditorLibrary::GetIstanaExploreV5PlayStateReport(
    FString& OutReport)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* Appearance = nullptr;
    FString Error;
    if (!GetLightweightAcceptedV5PlayState(
            PlayWorld,
            Player,
            Pawn,
            V4,
            Appearance,
            true,
            Error))
    {
        OutReport = TEXT("EXPLORE_V5_QA_STATE_INVALID: ") + Error;
        return false;
    }
    FString AppearanceReport;
    if (!Appearance->ValidateExploreV5Appearance(AppearanceReport, true))
    {
        OutReport = TEXT("EXPLORE_V5_QA_STATE_INVALID: ") +
            AppearanceReport;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("map=%s pawn=%s pawnLocationCm=%s pawnRotationDeg=%s viewTarget=%s viewTargetMatchesPawn=true v5CameraProfile=true closeTurfFoliage008Rebind=true %s appearance={%s}"),
        *UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName()),
        *Pawn->GetName(),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString(),
        *GetNameSafe(Player->GetViewTarget()),
        *V4->BuildWindRuntimeStateReport(),
        *AppearanceReport);
    return true;
}

bool UTRIADIstanaExploreV5EditorLibrary::QuiesceIstanaExploreV5PlayWorldForStop(
    FString& OutMessage)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* Appearance = nullptr;
    FString Readiness;
    if (!GetLightweightAcceptedV5PlayState(
            PlayWorld,
            Player,
            Pawn,
            V4,
            Appearance,
            true,
            Readiness))
    {
        OutMessage = TEXT("Refusing scripted Explore V5 PIE stop because exact runtime identity failed. ") +
            Readiness;
        return false;
    }
    OutMessage = TEXT("Explore V5 PIE identity, Player0 V5 pawn/view target, exact Scene/V2/V3/V4 census, inherited wind and deferred four-texture turf appearance are exact; no fragile UEDPIE object path or console command is needed, so scripted stop may proceed.");
    return true;
}
