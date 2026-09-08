#include "TRIADIstanaExploreV4EditorLibrary.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetCompilingManager.h"
#include "Camera/CameraComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreGlobals.h"
#include "CesiumIonServer.h"
#include "Dom/JsonObject.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataRequestOwner.h"
#include "DistanceFieldAtlas.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "IMeshBuilderModule.h"
#include "Kismet/GameplayStatics.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshDescription.h"
#include "MeshCardBuild.h"
#include "MeshCardRepresentation.h"
#include "MeshBudgetProjectSettings.h"
#include "MeshUtilities.h"
#include "MeshReductionSettings.h"
#include "Misc/Base64.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreMisc.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Misc/FileHelper.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NaniteBuilder.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/SceneViewport.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TextureCompiler.h"
#include "TRIADIstanaExploreV2LandscapeActor.h"
#include "TRIADIstanaExploreV3EditorLibrary.h"
#include "TRIADIstanaExploreV3SupplementActor.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaFreeRoamPawn.h"
#include "TRIADIstanaPublicViewEditorLibrary.h"
#include "TRIADIstanaPublicViewRuntimePolicyActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/Package.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/DevObjectVersion.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealClient.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>

DEFINE_LOG_CATEGORY_STATIC(LogTRIADIstanaExploreV4Editor, Log, All);

namespace
{
const FString SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v3"));
const FString SourceMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v3.Istana_PublicView_Explore_v3"));
const FString V2ValidationMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v2"));
const FString V2ValidationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v2.Istana_PublicView_Explore_v2"));
const FString DestinationMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v4"));
const FString DestinationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v4.Istana_PublicView_Explore_v4"));
const FString ExploreGameModeClassPath(
    TEXT("/Script/TRIADSensorFusion.TRIADIstanaExploreGameMode"));
const FString AssetRoot(TEXT("/Game/TRIAD/IstanaPublicViewExploreV4"));
const FString PrewarmEntryMapPackage(TEXT("/Engine/Maps/Entry"));
const FString ProtectedPrewarmReceiptSchema(
    TEXT("TRIAD_ISTANA_EXPLORE_V4_PROTECTED_PREWARM_RECEIPT_V1"));
const FString VegetationAssetPath(AssetRoot + TEXT("/Vegetation"));
const FString TextureAssetPath(VegetationAssetPath + TEXT("/Textures"));
const FString MaterialAssetPath(VegetationAssetPath + TEXT("/Materials"));
const FString MeshAssetPath(VegetationAssetPath + TEXT("/Meshes"));
const FString StagingVegetationAssetPath(
    AssetRoot + TEXT("/_SourceStaging/Vegetation"));
const FString PorticoAssetPath(AssetRoot + TEXT("/Portico"));

const FName V2LandscapeTag(TEXT("TRIADIstanaExploreLandscapeV2"));
const FName V3SupplementTag(TEXT("TRIADIstanaExploreSupplementV3"));
const FName V4LandscapeTag(TEXT("TRIADIstanaExploreLandscapeV4"));
const FName PlacementPolicyTag(
    TEXT("TRIAD_IPV4_CLEARANCE_CM_S150_300_100_200_F75_200_60_U100_250_90_G50_200_100"));
TWeakObjectPtr<UWorld> FullyAcceptedQaEditorWorld;
TWeakObjectPtr<UWorld> FullyAcceptedQaPlayWorld;

void LogV4BuildPhaseMemory(const TCHAR* Phase)
{
    const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
    constexpr double BytesPerMiB = 1024.0 * 1024.0;
    UE_LOG(
        LogTRIADIstanaExploreV4Editor,
        Display,
        TEXT("ISTANA_EXPLORE_V4_BUILD_PHASE_MEMORY phase=%s availableVirtualMiB=%.2f availablePhysicalMiB=%.2f processVirtualMiB=%.2f processPhysicalMiB=%.2f peakProcessVirtualMiB=%.2f peakProcessPhysicalMiB=%.2f"),
        Phase ? Phase : TEXT("UNKNOWN"),
        static_cast<double>(Stats.AvailableVirtual) / BytesPerMiB,
        static_cast<double>(Stats.AvailablePhysical) / BytesPerMiB,
        static_cast<double>(Stats.UsedVirtual) / BytesPerMiB,
        static_cast<double>(Stats.UsedPhysical) / BytesPerMiB,
        static_cast<double>(Stats.PeakUsedVirtual) / BytesPerMiB,
        static_cast<double>(Stats.PeakUsedPhysical) / BytesPerMiB);
}

const FString GeospatialContractSha(
    TEXT("CCD9B5200E03602EC6FA6986F4D3AB70920806C63D718B5B1A943CC29F30D8E7"));
const FString RasterGridSha(
    TEXT("262C88795C3C5FB1BB92B2D1CD2D1886B25D5E3885E60F34E407082E4C81C447"));
const FString VegetationZoningSha(
    TEXT("3AA50AECDA04DDC68BC548EC17FAB0CC2292D05532390E51A523F2817E91645E"));
const FString PorticoManifestSha(
    TEXT("3E88B8C4AEF5A499696436920972301EFBDA242BAAA3F64928D1A8B072557943"));
const FString PorticoObjSha(
    TEXT("330B20E8F58289C84EF96CB031D374ADB3AAF52711E5247D1F167C7E94DC4526"));
const FString PorticoMtlSha(
    TEXT("593E5721C2968236142E6C7C23E5AC7757722D2DF1E8D540C7E86A96D1369600"));
const FString VegetationManifestSha(
    TEXT("87A48A09EE6C9C977C5865D9D6B821581A654E30F5095C52C2901E48D6E5B3C6"));
// Updated only when the separately reviewed, byte-frozen source contract is
// replaced as one whole file.  It is intentionally independent of the
// geospatial contract above.
const FString VegetationContractSha(
    TEXT("ABDC65AA14DA9AE89736BBE2D75E6212FA38B76B44DBA491C9266DEBC247E104"));
constexpr int64 VegetationContractBytes = 117730;

const FString ProtectedHighForkSourceObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_IslandTree01.SM_IPVExploreV3_IslandTree01"));
const FString ProtectedColumnarSourceObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_Broadleaf_A.SM_IstanaPublicViewExploreV1_Broadleaf_A"));
const FString ProtectedCloseTurfSourceObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_CloseTurfCards.SM_IPVExploreV3_CloseTurfCards"));
const FString EngineCylinderObjectPath(
    TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

const FString UmbrellaMeshName(TEXT("SM_IPV4_UmbrellaTree"));
const FString DomeMeshName(TEXT("SM_IPV4_DenseDomeTree"));
const FString HighForkMeshName(TEXT("SM_IPV4_IslandTree01_HighForkProxy"));
const FString ColumnarMeshName(TEXT("SM_IPV4_Broadleaf_ColumnarProxy"));
const FString PalmMeshName(TEXT("SM_IPV4_PalmAccent"));
const FString ShrubMeshName(TEXT("SM_IPV4_Shrub04_A"));
const FString FlowerMeshName(TEXT("SM_IPV4_Periwinkle06_F"));
const FString UnderstoreyMeshName(TEXT("SM_IPV4_Calathea_D"));
const FString GeometryGrassMeshName(TEXT("SM_IPV4_GrassMedium_SmallA"));
const FString CloseTurfMeshName(TEXT("SM_IPV4_CloseTurfCards"));
const FString HeritageBlockerMeshName(TEXT("SM_IPV4_HeritagePawnBlockerCylinder"));
const FString PorticoMeshName(
    TEXT("SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live"));
const FString LawnBaseMaterialName(TEXT("M_IPV4_AmbientCG_Grass001_Lawn_Base"));
const FString LawnMaterialInstanceName(TEXT("MI_IPV4_AmbientCG_Grass001_Lawn"));

constexpr int32 ExpectedV2UmbrellaCount = 272;
constexpr int32 ExpectedV2ColumnarCount = 232;
constexpr int32 ExpectedV2DomeCount = 182;
constexpr int32 ExpectedV2PalmCount = 34;
constexpr int32 ExpectedV2TreeUnionCount = 720;
constexpr int32 ExpectedHeritageAnchorCount = 9;
constexpr int32 ExpectedShrubCount = 512;
constexpr int32 ExpectedFlowerCount = 192;
constexpr int32 ExpectedUnderstoreyCount = 384;
constexpr int32 ExpectedGeometryGrassCount = 1536;
constexpr int32 ExpectedCloseTurfCount = 18432;
constexpr int32 FormalBedShrubCount = 256;
constexpr int32 FormalBedFlowerCount = 128;
constexpr int32 FormalBedUnderstoreyCount = 192;
constexpr int32 RasterGridWidth = 201;
constexpr int32 RasterGridHeight = 201;
constexpr int32 RasterGridPixels = RasterGridWidth * RasterGridHeight;
constexpr float RasterGridMinimumMeters = -1000.0f;
constexpr float RasterGridStepMeters = 10.0f;

const FString WindCustomDescription(
    TEXT("TRIAD_EXPLORE_V4_INSTANCE_LOCAL_PIVOT_UNDERDAMPED_WPO_V1"));
const FString WindCustomCode(
    TEXT("float InstanceRandom = GetPerInstanceRandom(Parameters);\n")
    TEXT("float h = saturate(max(InstanceLocalPosition.z, 0.0) / max(HeightCm, 1.0));\n")
    TEXT("float phase = TimeSeconds * WindSpeed * 6.28318530718 + InstanceRandom * 6.28318530718 + dot(WorldPosition.xy, float2(0.0017, 0.0023));\n")
    TEXT("float wave = sin(phase) + 0.31 * sin(phase * 1.73 + 1.2);\n")
    TEXT("float2 direction = normalize(WindDirection.xy + float2(0.0001, 0.0));\n")
    TEXT("float bend = clamp(WindStrengthCm * ResponseScale * wave * h * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("float lift = clamp(abs(WindStrengthCm) * ResponseScale * 0.025 * sin(phase * 0.71) * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("return float3(direction * bend, lift);"));

FString ObjectPath(const FString& PackagePath, const FString& AssetName)
{
    return PackagePath + TEXT("/") + AssetName + TEXT(".") + AssetName;
}

FString ProjectSourcePath(const TCHAR* Relative)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("SourceAssets"), Relative));
}

template <typename T>
T* LoadExact(const FString& ExactObjectPath)
{
    T* Object = LoadObject<T>(nullptr, *ExactObjectPath);
    return Object && Object->GetPathName() == ExactObjectPath ? Object : nullptr;
}

bool LoadJsonObject(const FString& Filename, TSharedPtr<FJsonObject>& OutObject)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Filename))
    {
        return false;
    }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}

// Portable SHA-256 is required here: UE5.5 Win64's platform SHA-256 entrypoint
// is not implemented and asserts.  This is the same independently vector-gated
// streaming implementation already exercised by the Explore V3 integration.
// TRIAD_EXPLORE_V4_SHA256_HOST_BEGIN
namespace TriadExploreV4Sha256
{
using FDigest = std::array<std::uint8_t, 32>;
using FLowerHexDigest = std::array<char, 65>;

class FPortableSha256 final
{
public:
    FPortableSha256()
        : State_{
              0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
              0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u}
    {
        Buffer_.fill(0u);
    }

    bool Update(const std::uint8_t* Data, std::size_t Length)
    {
        static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
        constexpr std::uint64_t MaximumBytes =
            std::numeric_limits<std::uint64_t>::max() / 8u;
        if (!bValid_)
        {
            return false;
        }
        if (Length == 0u)
        {
            return true;
        }
        if (!Data ||
            static_cast<std::uint64_t>(Length) > MaximumBytes - TotalBytes_)
        {
            bValid_ = false;
            return false;
        }
        TotalBytes_ += static_cast<std::uint64_t>(Length);
        std::size_t Offset = 0u;
        if (BufferedBytes_ > 0u)
        {
            const std::size_t Available = 64u - BufferedBytes_;
            const std::size_t CopyBytes = std::min(Length, Available);
            std::memcpy(Buffer_.data() + BufferedBytes_, Data, CopyBytes);
            BufferedBytes_ += CopyBytes;
            Offset += CopyBytes;
            if (BufferedBytes_ == 64u)
            {
                Transform(Buffer_.data());
                BufferedBytes_ = 0u;
            }
        }
        while (Length - Offset >= 64u)
        {
            Transform(Data + Offset);
            Offset += 64u;
        }
        const std::size_t Remaining = Length - Offset;
        if (Remaining > 0u)
        {
            std::memcpy(Buffer_.data(), Data + Offset, Remaining);
            BufferedBytes_ = Remaining;
        }
        return true;
    }

    bool Finalize(FDigest& OutDigest) const
    {
        if (!bValid_)
        {
            return false;
        }
        FPortableSha256 Copy(*this);
        Copy.FinalizeInPlace(OutDigest);
        return true;
    }

private:
    static std::uint32_t RotateRight(std::uint32_t Value, unsigned Count)
    {
        return (Value >> Count) | (Value << (32u - Count));
    }

    void Transform(const std::uint8_t* Block)
    {
        static constexpr std::array<std::uint32_t, 64> Constants = {
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
        std::array<std::uint32_t, 64> Words{};
        for (std::size_t Index = 0u; Index < 16u; ++Index)
        {
            const std::size_t Offset = Index * 4u;
            Words[Index] =
                (static_cast<std::uint32_t>(Block[Offset]) << 24u) |
                (static_cast<std::uint32_t>(Block[Offset + 1u]) << 16u) |
                (static_cast<std::uint32_t>(Block[Offset + 2u]) << 8u) |
                static_cast<std::uint32_t>(Block[Offset + 3u]);
        }
        for (std::size_t Index = 16u; Index < Words.size(); ++Index)
        {
            const std::uint32_t S0 =
                RotateRight(Words[Index - 15u], 7u) ^
                RotateRight(Words[Index - 15u], 18u) ^
                (Words[Index - 15u] >> 3u);
            const std::uint32_t S1 =
                RotateRight(Words[Index - 2u], 17u) ^
                RotateRight(Words[Index - 2u], 19u) ^
                (Words[Index - 2u] >> 10u);
            Words[Index] = Words[Index - 16u] + S0 +
                Words[Index - 7u] + S1;
        }
        std::uint32_t A = State_[0];
        std::uint32_t B = State_[1];
        std::uint32_t C = State_[2];
        std::uint32_t D = State_[3];
        std::uint32_t E = State_[4];
        std::uint32_t F = State_[5];
        std::uint32_t G = State_[6];
        std::uint32_t H = State_[7];
        for (std::size_t Index = 0u; Index < Words.size(); ++Index)
        {
            const std::uint32_t Sum1 =
                RotateRight(E, 6u) ^ RotateRight(E, 11u) ^ RotateRight(E, 25u);
            const std::uint32_t Choice = (E & F) ^ ((~E) & G);
            const std::uint32_t Temporary1 =
                H + Sum1 + Choice + Constants[Index] + Words[Index];
            const std::uint32_t Sum0 =
                RotateRight(A, 2u) ^ RotateRight(A, 13u) ^ RotateRight(A, 22u);
            const std::uint32_t Majority = (A & B) ^ (A & C) ^ (B & C);
            const std::uint32_t Temporary2 = Sum0 + Majority;
            H = G;
            G = F;
            F = E;
            E = D + Temporary1;
            D = C;
            C = B;
            B = A;
            A = Temporary1 + Temporary2;
        }
        State_[0] += A;
        State_[1] += B;
        State_[2] += C;
        State_[3] += D;
        State_[4] += E;
        State_[5] += F;
        State_[6] += G;
        State_[7] += H;
    }

    void FinalizeInPlace(FDigest& OutDigest)
    {
        const std::uint64_t OriginalBitCount = TotalBytes_ * 8u;
        Buffer_[BufferedBytes_++] = 0x80u;
        if (BufferedBytes_ > 56u)
        {
            while (BufferedBytes_ < 64u)
            {
                Buffer_[BufferedBytes_++] = 0u;
            }
            Transform(Buffer_.data());
            BufferedBytes_ = 0u;
        }
        while (BufferedBytes_ < 56u)
        {
            Buffer_[BufferedBytes_++] = 0u;
        }
        for (std::size_t Index = 0u; Index < 8u; ++Index)
        {
            Buffer_[63u - Index] = static_cast<std::uint8_t>(
                OriginalBitCount >> (Index * 8u));
        }
        Transform(Buffer_.data());
        for (std::size_t Index = 0u; Index < State_.size(); ++Index)
        {
            const std::uint32_t Word = State_[Index];
            const std::size_t Offset = Index * 4u;
            OutDigest[Offset] = static_cast<std::uint8_t>(Word >> 24u);
            OutDigest[Offset + 1u] = static_cast<std::uint8_t>(Word >> 16u);
            OutDigest[Offset + 2u] = static_cast<std::uint8_t>(Word >> 8u);
            OutDigest[Offset + 3u] = static_cast<std::uint8_t>(Word);
        }
    }

    std::array<std::uint32_t, 8> State_{};
    std::array<std::uint8_t, 64> Buffer_{};
    std::uint64_t TotalBytes_ = 0u;
    std::size_t BufferedBytes_ = 0u;
    bool bValid_ = true;
};

FLowerHexDigest ToLowerHex(const FDigest& Digest)
{
    static constexpr char HexDigits[] = "0123456789abcdef";
    FLowerHexDigest Result{};
    for (std::size_t Index = 0u; Index < Digest.size(); ++Index)
    {
        Result[Index * 2u] = HexDigits[Digest[Index] >> 4u];
        Result[Index * 2u + 1u] = HexDigits[Digest[Index] & 0x0fu];
    }
    Result[64] = '\0';
    return Result;
}

bool HasDigest(const FPortableSha256& Hasher, const char* ExpectedLowerHex)
{
    FDigest Digest{};
    if (!ExpectedLowerHex || !Hasher.Finalize(Digest))
    {
        return false;
    }
    const FLowerHexDigest Actual = ToLowerHex(Digest);
    return std::strcmp(Actual.data(), ExpectedLowerHex) == 0;
}

bool VerifyKnownVectors()
{
    FPortableSha256 Empty;
    if (!Empty.Update(nullptr, 0u) ||
        !HasDigest(Empty,
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855"))
    {
        return false;
    }
    FPortableSha256 Abc;
    const char* AbcText = "abc";
    if (!Abc.Update(reinterpret_cast<const std::uint8_t*>(AbcText), 1u) ||
        !Abc.Update(reinterpret_cast<const std::uint8_t*>(AbcText + 1), 2u) ||
        !HasDigest(Abc,
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad"))
    {
        return false;
    }
    const char* Multi =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    FPortableSha256 MultiBlock;
    if (!MultiBlock.Update(
            reinterpret_cast<const std::uint8_t*>(Multi), 13u) ||
        !MultiBlock.Update(
            reinterpret_cast<const std::uint8_t*>(Multi + 13), 43u) ||
        !HasDigest(MultiBlock,
            "248d6a61d20638b8e5c026930c3e6039"
            "a33ce45964ff2167f6ecedd419db06c1"))
    {
        return false;
    }
    std::array<std::uint8_t, 1024> Chunk{};
    Chunk.fill(static_cast<std::uint8_t>('a'));
    FPortableSha256 GreaterThanOneMiB;
    for (std::size_t Index = 0u; Index < 1024u; ++Index)
    {
        if (!GreaterThanOneMiB.Update(Chunk.data(), Chunk.size()))
        {
            return false;
        }
    }
    std::array<std::uint8_t, 17> Tail{};
    Tail.fill(static_cast<std::uint8_t>('a'));
    return GreaterThanOneMiB.Update(Tail.data(), Tail.size()) &&
        HasDigest(GreaterThanOneMiB,
            "c26032d5154f96bd29c799447d715ab6"
            "81d8d0aa308ecc6f321a35d98f0672da");
}
} // namespace TriadExploreV4Sha256
// TRIAD_EXPLORE_V4_SHA256_HOST_END

bool Sha256ImplementationIsValid()
{
    static const bool bKnownVectorsValid =
        TriadExploreV4Sha256::VerifyKnownVectors();
    return bKnownVectorsValid;
}

bool HashFileSha256(const FString& Filename, FString& OutSha, int64& OutBytes)
{
    OutSha.Reset();
    OutBytes = -1;
    if (!Sha256ImplementationIsValid())
    {
        return false;
    }
    TUniquePtr<FArchive> Reader(
        IFileManager::Get().CreateFileReader(*Filename, FILEREAD_Silent));
    if (!Reader || Reader->TotalSize() < 0)
    {
        return false;
    }
    constexpr int64 ReadChunkBytes = 1024 * 1024;
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(static_cast<int32>(ReadChunkBytes));
    TriadExploreV4Sha256::FPortableSha256 Hasher;
    int64 Remaining = Reader->TotalSize();
    while (Remaining > 0)
    {
        const int64 ThisChunk = FMath::Min(Remaining, ReadChunkBytes);
        Reader->Serialize(Buffer.GetData(), ThisChunk);
        if (Reader->IsError() ||
            !Hasher.Update(
                Buffer.GetData(), static_cast<std::size_t>(ThisChunk)))
        {
            return false;
        }
        Remaining -= ThisChunk;
    }
    TriadExploreV4Sha256::FDigest Digest{};
    if (!Hasher.Finalize(Digest))
    {
        return false;
    }
    const TriadExploreV4Sha256::FLowerHexDigest Hex =
        TriadExploreV4Sha256::ToLowerHex(Digest);
    OutSha = UTF8_TO_TCHAR(Hex.data());
    OutSha.ToUpperInline();
    OutBytes = Reader->TotalSize();
    return OutSha.Len() == 64;
}

bool HashUtf8StringSha256(const FString& Value, FString& OutSha)
{
    OutSha.Reset();
    if (!Sha256ImplementationIsValid())
    {
        return false;
    }
    const FTCHARToUTF8 Utf8(*Value);
    TriadExploreV4Sha256::FPortableSha256 Hasher;
    if (!Hasher.Update(
            reinterpret_cast<const std::uint8_t*>(Utf8.Get()),
            static_cast<std::size_t>(Utf8.Length())))
    {
        return false;
    }
    TriadExploreV4Sha256::FDigest Digest{};
    if (!Hasher.Finalize(Digest))
    {
        return false;
    }
    const TriadExploreV4Sha256::FLowerHexDigest Hex =
        TriadExploreV4Sha256::ToLowerHex(Digest);
    OutSha = UTF8_TO_TCHAR(Hex.data());
    OutSha.ToUpperInline();
    return OutSha.Len() == 64;
}

bool ValidateExactFile(
    const FString& Filename,
    int64 ExpectedBytes,
    const FString& ExpectedSha,
    FString& OutError)
{
    FString ActualSha;
    int64 ActualBytes = 0;
    if (!HashFileSha256(Filename, ActualSha, ActualBytes) ||
        ActualBytes != ExpectedBytes ||
        !ActualSha.Equals(ExpectedSha, ESearchCase::IgnoreCase))
    {
        OutError = FString::Printf(
            TEXT("Frozen source mismatch: '%s' expected %lld bytes/%s, got %lld/%s."),
            *Filename,
            ExpectedBytes,
            *ExpectedSha,
            ActualBytes,
            *ActualSha);
        return false;
    }
    OutError.Reset();
    return true;
}

bool HasDisallowedDirtyPackages(
    int32& OutKnownAutoDirtyMaterialCount,
    FString& OutError)
{
    TArray<UPackage*> DirtyMaps;
    TArray<UPackage*> DirtyContent;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContent);
    OutKnownAutoDirtyMaterialCount = 0;
    if (!DirtyMaps.IsEmpty())
    {
        OutError = TEXT("Close or save every dirty map before V4 import/build.");
        return true;
    }
    static const TMap<FString, FString> ExactKnownAutoDirtyMaterials = {
        {TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_Bark"),
         TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_Bark.M_IPV_Bark")},
        {TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafDark"),
         TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafDark.M_IPV_LeafDark")},
        {TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafMid"),
         TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafMid.M_IPV_LeafMid")},
        {TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafLight"),
         TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafLight.M_IPV_LeafLight")}};
    for (UPackage* Package : DirtyContent)
    {
        const FString PackageName = Package ? Package->GetName() : FString();
        const FString* ExactObjectPath =
            ExactKnownAutoDirtyMaterials.Find(PackageName);
        UMaterial* Material = ExactObjectPath
            ? LoadExact<UMaterial>(*ExactObjectPath)
            : nullptr;
        if (!Package || !Package->IsDirty() || !ExactObjectPath ||
            !Material || Material->GetOutermost() != Package ||
            !Material->bUsedWithInstancedStaticMeshes)
        {
            OutError = TEXT("A dirty content package is not one exact known V1 HISM-usage auto-dirty material: ") +
                PackageName;
            return true;
        }
        ++OutKnownAutoDirtyMaterialCount;
    }
    if (OutKnownAutoDirtyMaterialCount > 0)
    {
        FString V1Report;
        if (!UTRIADIstanaPublicViewEditorLibrary::
                ValidateIstanaPublicViewAssets(V1Report))
        {
            OutError = TEXT("A known auto-dirty V1 material failed the complete V1 graph/texture validator: ") +
                V1Report;
            return true;
        }
        TArray<UPackage*> RecheckedDirtyMaps;
        TArray<UPackage*> RecheckedDirtyContent;
        UEditorLoadingAndSavingUtils::GetDirtyMapPackages(RecheckedDirtyMaps);
        UEditorLoadingAndSavingUtils::GetDirtyContentPackages(
            RecheckedDirtyContent);
        if (!RecheckedDirtyMaps.IsEmpty())
        {
            OutError = TEXT("The V1 validation preflight introduced a dirty map.");
            return true;
        }
        OutKnownAutoDirtyMaterialCount = 0;
        for (UPackage* Package : RecheckedDirtyContent)
        {
            const FString PackageName = Package
                ? Package->GetName()
                : FString();
            const FString* ExactObjectPath =
                ExactKnownAutoDirtyMaterials.Find(PackageName);
            UMaterial* Material = ExactObjectPath
                ? LoadExact<UMaterial>(*ExactObjectPath)
                : nullptr;
            if (!Package || !Package->IsDirty() || !ExactObjectPath ||
                !Material || Material->GetOutermost() != Package ||
                !Material->bUsedWithInstancedStaticMeshes)
            {
                OutError = TEXT("The V1 validation preflight introduced a disallowed dirty package.");
                return true;
            }
            ++OutKnownAutoDirtyMaterialCount;
        }
    }
    OutError.Reset();
    return false;
}

struct FProtectedFileDigest
{
    FString PackageName;
    FString Filename;
    bool bExisted = false;
    int64 ByteCount = 0;
    FString Sha256;
};

bool CaptureExactPackageFileDigests(
    const TArray<FString>& PackageNames,
    const FString& Label,
    TArray<FProtectedFileDigest>& OutRecords,
    FString& OutError)
{
    TSet<FString> UniquePackages;
    OutRecords.Reset();
    for (const FString& PackageName : PackageNames)
    {
        FString MainFilename;
        if (PackageName.IsEmpty() || UniquePackages.Contains(PackageName) ||
            !FPackageName::DoesPackageExist(PackageName, &MainFilename))
        {
            OutError = Label + TEXT(" package is absent or duplicated: ") +
                PackageName;
            return false;
        }
        UniquePackages.Add(PackageName);
        const FString Base = FPaths::ChangeExtension(MainFilename, TEXT(""));
        const TArray<FString> Candidates = {
            MainFilename,
            Base + TEXT(".uexp"),
            Base + TEXT(".ubulk"),
            Base + TEXT(".uptnl")};
        for (int32 CandidateIndex = 0;
             CandidateIndex < Candidates.Num();
             ++CandidateIndex)
        {
            FProtectedFileDigest Record;
            Record.PackageName = PackageName;
            Record.Filename = FPaths::ConvertRelativePathToFull(
                Candidates[CandidateIndex]);
            FPaths::NormalizeFilename(Record.Filename);
            Record.ByteCount = IFileManager::Get().FileSize(*Record.Filename);
            Record.bExisted = Record.ByteCount >= 0;
            if (CandidateIndex == 0 && !Record.bExisted)
            {
                OutError = Label + TEXT(" main package file disappeared: ") +
                    Record.Filename;
                return false;
            }
            if (Record.bExisted)
            {
                int64 HashedBytes = 0;
                if (Record.ByteCount <= 0 ||
                    !HashFileSha256(
                        Record.Filename,
                        Record.Sha256,
                        HashedBytes) ||
                    HashedBytes != Record.ByteCount)
                {
                    OutError = Label + TEXT(" package file could not be streaming-hashed: ") +
                        Record.Filename;
                    return false;
                }
            }
            else
            {
                Record.ByteCount = 0;
                Record.Sha256.Reset();
            }
            OutRecords.Add(MoveTemp(Record));
        }
    }
    if (UniquePackages.Num() != PackageNames.Num() ||
        OutRecords.Num() != PackageNames.Num() * 4)
    {
        OutError = Label + TEXT(" package-file digest roster is incomplete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateExactPackageFileDigests(
    const TArray<FProtectedFileDigest>& Records,
    int32 ExpectedPackageCount,
    const FString& Label,
    FString& OutError)
{
    TSet<FString> Packages;
    for (const FProtectedFileDigest& Record : Records)
    {
        Packages.Add(Record.PackageName);
        const int64 CurrentBytes =
            IFileManager::Get().FileSize(*Record.Filename);
        if ((!Record.bExisted && CurrentBytes >= 0) ||
            (Record.bExisted && CurrentBytes != Record.ByteCount))
        {
            OutError = Label + TEXT(" package-file presence/size changed: ") +
                Record.Filename;
            return false;
        }
        if (Record.bExisted)
        {
            FString CurrentSha;
            int64 HashedBytes = 0;
            if (!HashFileSha256(
                    Record.Filename, CurrentSha, HashedBytes) ||
                HashedBytes != Record.ByteCount ||
                !CurrentSha.Equals(
                    Record.Sha256, ESearchCase::IgnoreCase))
            {
                OutError = Label + TEXT(" package-file SHA changed: ") +
                    Record.Filename;
                return false;
            }
        }
    }
    if (ExpectedPackageCount <= 0 ||
        Packages.Num() != ExpectedPackageCount ||
        Records.Num() != ExpectedPackageCount * 4)
    {
        OutError = Label + TEXT(" package-file validation roster is incomplete.");
        return false;
    }
    OutError.Reset();
    return true;
}

const TArray<FString>& ProtectedMapPackages()
{
    static const TArray<FString> Packages = {
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v1"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v2"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v3"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v4"),
        TEXT("/Game/Maps/Istana_PublicView_Exterior_v5"),
        TEXT("/Game/Maps/Istana_PublicView_Explore_v1"),
        TEXT("/Game/Maps/Istana_PublicView_Explore_v2"),
        SourceMapPackage};
    return Packages;
}

const TArray<FString>& ProtectedAssetRoots()
{
    static const TArray<FString> Roots = {
        TEXT("/Game/TRIAD/IstanaPublicView"),
        TEXT("/Game/TRIAD/IstanaPublicViewV2"),
        TEXT("/Game/TRIAD/IstanaPublicViewV3"),
        TEXT("/Game/TRIAD/IstanaPublicViewV4"),
        TEXT("/Game/TRIAD/IstanaPublicViewV5"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV1"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV2"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV3")};
    return Roots;
}

bool GatherProtectedPackageNames(TArray<FString>& OutPackages, FString& OutError)
{
    TSet<FString> Unique;
    for (const FString& Map : ProtectedMapPackages())
    {
        if (!FPackageName::DoesPackageExist(Map))
        {
            OutError = TEXT("A mandatory protected V1-V5/Explore V1-V3 map is absent: ") + Map;
            return false;
        }
        Unique.Add(Map);
    }
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    for (const FString& Root : ProtectedAssetRoots())
    {
        TArray<FAssetData> Assets;
        RegistryModule.Get().GetAssetsByPath(FName(*Root), Assets, true, false);
        if (Assets.IsEmpty())
        {
            OutError = TEXT("A mandatory protected V1/V2/V3 asset tree is empty: ") + Root;
            return false;
        }
        for (const FAssetData& Asset : Assets)
        {
            Unique.Add(Asset.PackageName.ToString());
        }
    }
    OutPackages = Unique.Array();
    OutPackages.Sort();
    OutError.Reset();
    return true;
}

bool CaptureProtectedPackages(
    TArray<FProtectedFileDigest>& OutRecords,
    FString& OutError)
{
    TArray<FString> PackageNames;
    if (!GatherProtectedPackageNames(PackageNames, OutError))
    {
        return false;
    }
    OutRecords.Reset();
    for (const FString& PackageName : PackageNames)
    {
        FString MainFilename;
        if (!FPackageName::DoesPackageExist(PackageName, &MainFilename))
        {
            OutError = TEXT("Could not resolve protected package: ") + PackageName;
            return false;
        }
        const FString Base = FPaths::ChangeExtension(MainFilename, TEXT(""));
        const TArray<FString> Candidates = {
            MainFilename,
            Base + TEXT(".uexp"),
            Base + TEXT(".ubulk"),
            Base + TEXT(".uptnl")};
        for (int32 CandidateIndex = 0;
             CandidateIndex < Candidates.Num();
             ++CandidateIndex)
        {
            const FString& Candidate = Candidates[CandidateIndex];
            FProtectedFileDigest Record;
            Record.PackageName = PackageName;
            Record.Filename = FPaths::ConvertRelativePathToFull(Candidate);
            FPaths::NormalizeFilename(Record.Filename);
            Record.ByteCount = IFileManager::Get().FileSize(*Record.Filename);
            Record.bExisted = Record.ByteCount >= 0;
            if (CandidateIndex == 0 && !Record.bExisted)
            {
                OutError = TEXT("Protected package main file disappeared: ") +
                    Record.Filename;
                return false;
            }
            if (Record.bExisted)
            {
                int64 HashedBytes = 0;
                if (Record.ByteCount <= 0 ||
                    !HashFileSha256(
                        Record.Filename,
                        Record.Sha256,
                        HashedBytes) ||
                    HashedBytes != Record.ByteCount)
                {
                    OutError = TEXT("Could not streaming-hash protected package file: ") +
                        Record.Filename;
                    return false;
                }
            }
            else
            {
                Record.ByteCount = 0;
                Record.Sha256.Reset();
            }
            OutRecords.Add(MoveTemp(Record));
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedPackages(
    const TArray<FProtectedFileDigest>& Records,
    FString& OutError)
{
    TArray<FString> CurrentPackages;
    if (Records.IsEmpty() ||
        !GatherProtectedPackageNames(CurrentPackages, OutError))
    {
        return false;
    }
    TSet<FString> CapturedPackages;
    for (const FProtectedFileDigest& Record : Records)
    {
        CapturedPackages.Add(Record.PackageName);
        const int64 CurrentBytes =
            IFileManager::Get().FileSize(*Record.Filename);
        if (!CurrentPackages.Contains(Record.PackageName) ||
            (!Record.bExisted && CurrentBytes >= 0))
        {
            OutError = TEXT("Protected V1-V5/Explore V1-V3/HDB/OSM file presence changed: ") +
                Record.PackageName + TEXT(" -> ") + Record.Filename;
            return false;
        }
        if (Record.bExisted)
        {
            FString CurrentSha;
            int64 HashedBytes = 0;
            if (CurrentBytes != Record.ByteCount ||
                !HashFileSha256(
                    Record.Filename, CurrentSha, HashedBytes) ||
                HashedBytes != Record.ByteCount ||
                !CurrentSha.Equals(
                    Record.Sha256, ESearchCase::IgnoreCase))
            {
                OutError = TEXT("Protected V1-V5/Explore V1-V3/HDB/OSM size/SHA changed: ") +
                    Record.PackageName + TEXT(" -> ") + Record.Filename;
                return false;
            }
        }
    }
    if (CapturedPackages.Num() != CurrentPackages.Num())
    {
        OutError = TEXT("Protected package roster changed while producing V4.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool EnsureAssetRegistryDiscoveryComplete(FString& OutError)
{
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    if (RegistryModule.Get().IsLoadingAssets())
    {
        RegistryModule.Get().WaitForCompletion();
    }
    if (RegistryModule.Get().IsLoadingAssets())
    {
        OutError = TEXT("Asset Registry discovery did not complete; exact V4 namespace and protected-package censuses are not trustworthy.");
        return false;
    }
    OutError.Reset();
    return true;
}

int32 CountAssetsUnderV4Root()
{
    TArray<FAssetData> Assets;
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    RegistryModule.Get().GetAssetsByPath(FName(*AssetRoot), Assets, true, false);
    return Assets.Num();
}

bool ValidateFrozenPublicDataContracts(FString& OutError)
{
    const FString Geo = ProjectSourcePath(
        TEXT("IstanaPublicViewExploreV4/explore_v4_geospatial.contract.json"));
    const FString Grid = ProjectSourcePath(
        TEXT("IstanaPublicViewExploreV4/Prepared/v4_raster_sample_grid.json"));
    const FString Zoning = ProjectSourcePath(
        TEXT("IstanaPublicViewExploreV4/Sources/Geospatial/v4_public_vegetation_zoning.json"));
    const FString PorticoManifest = ProjectSourcePath(
        TEXT("IstanaPublicViewV8Portico/Generated/IstanaPublicViewV8CentralPorticoDepthOverlayV5Live.manifest.json"));
    const FString PorticoObj = ProjectSourcePath(
        TEXT("IstanaPublicViewV8Portico/Generated/SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live.obj"));
    const FString PorticoMtl = ProjectSourcePath(
        TEXT("IstanaPublicViewV8Portico/Generated/SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live.mtl"));
    if (!ValidateExactFile(Geo, 16706, GeospatialContractSha, OutError) ||
        !ValidateExactFile(Grid, 170691, RasterGridSha, OutError) ||
        !ValidateExactFile(Zoning, 11673, VegetationZoningSha, OutError) ||
        !ValidateExactFile(
            PorticoManifest, 6310, PorticoManifestSha, OutError) ||
        !ValidateExactFile(PorticoObj, 1383806, PorticoObjSha, OutError) ||
        !ValidateExactFile(PorticoMtl, 1218, PorticoMtlSha, OutError))
    {
        return false;
    }

    TSharedPtr<FJsonObject> GeoJson;
    TSharedPtr<FJsonObject> GridJson;
    TSharedPtr<FJsonObject> ZoningJson;
    TSharedPtr<FJsonObject> PorticoJson;
    if (!LoadJsonObject(Geo, GeoJson) ||
        !LoadJsonObject(Grid, GridJson) ||
        !LoadJsonObject(Zoning, ZoningJson) ||
        !LoadJsonObject(PorticoManifest, PorticoJson) ||
        GeoJson->GetStringField(TEXT("schema")) !=
            TEXT("triad.istana_explore_v4_geospatial_source_contract.v1") ||
        GeoJson->GetObjectField(TEXT("scope"))->GetStringField(TEXT("sourceMap")) !=
            SourceMapPackage ||
        GeoJson->GetObjectField(TEXT("scope"))->GetStringField(TEXT("targetMap")) !=
            DestinationMapPackage ||
        GeoJson->GetObjectField(TEXT("scope"))->GetStringField(
            TEXT("targetContentNamespace")) != AssetRoot ||
        GeoJson->GetObjectField(TEXT("claimBoundary"))->GetBoolField(
            TEXT("oneToOneOneKilometerClaimed")) ||
        GeoJson->GetObjectField(TEXT("claimBoundary"))->GetBoolField(
            TEXT("sensorOrOcclusionTruthClaimed")) ||
        GeoJson->GetObjectField(TEXT("claimBoundary"))->GetBoolField(
            TEXT("googleOrOneMapContentUsed")) ||
        GridJson->GetStringField(TEXT("schema")) !=
            TEXT("triad.istana_explore_v4_local_raster_sample_grid.v1") ||
        GridJson->GetObjectField(TEXT("grid"))->GetIntegerField(TEXT("width")) !=
            RasterGridWidth ||
        GridJson->GetObjectField(TEXT("grid"))->GetIntegerField(TEXT("height")) !=
            RasterGridHeight ||
        ZoningJson->GetStringField(TEXT("schema")) !=
            TEXT("triad.istana_explore_v4_public_vegetation_zoning.v1") ||
        PorticoJson->GetStringField(TEXT("schemaVersion")) !=
            TEXT("triad.istana_public_view_v8_portico_depth_overlay.v1") ||
        PorticoJson->GetObjectField(TEXT("mesh"))->GetIntegerField(
            TEXT("triangleCount")) != 6592 ||
        PorticoJson->GetObjectField(TEXT("mesh"))->GetIntegerField(
            TEXT("materialDefinitionCount")) != 7)
    {
        OutError = TEXT("A frozen V4 public-data/geospatial/portico contract changed.");
        return false;
    }
    static const TArray<FString> ExpectedPlacementRules = {
        TEXT("Prefer reclassification and rescaling of inherited V2 tree visuals at their exact existing positions so their Pawn blockers remain aligned."),
        TEXT("For render-only filler, jitter within the 10 m source cell deterministically, clip again to the exact geodesic circle, and reject any point outside class 10."),
        TEXT("Reject candidates intersecting inherited building, hardscape, road, water, formal-lawn/ceremonial-clearance or existing-tree clearance masks."),
        TEXT("Do not derive individual trunk positions, crown outlines or species from WorldCover, ETH canopy height or Sentinel pixels."),
        TEXT("Keep central formal turf at 3-6 cm and existing peripheral tall-grass bounds; no global tall-grass override.")};
    const TArray<TSharedPtr<FJsonValue>>& PlacementRules =
        ZoningJson->GetObjectField(TEXT("zoning"))->GetArrayField(
            TEXT("placementRules"));
    if (PlacementRules.Num() != ExpectedPlacementRules.Num())
    {
        OutError = TEXT("The frozen public zoning placement-rule census changed.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedPlacementRules.Num(); ++Index)
    {
        if (!PlacementRules[Index].IsValid() ||
            PlacementRules[Index]->AsString() != ExpectedPlacementRules[Index])
        {
            OutError = TEXT("A frozen public zoning placement rule changed.");
            return false;
        }
    }
    const TSharedPtr<FJsonObject> Integration =
        PorticoJson->GetObjectField(TEXT("integration"));
    if (Integration->GetBoolField(TEXT("collisionEnabled")) ||
        Integration->GetBoolField(TEXT("v7AndV8ConcurrentRenderingAuthorized")) ||
        !Integration->GetBoolField(TEXT("presentationSuccessorOfFrozenV7")) ||
        PorticoJson->GetObjectField(TEXT("claimBoundary"))->GetBoolField(
            TEXT("hyperrealClaimed")))
    {
        OutError = TEXT("The V8 render-only/successor/truth boundary changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

struct FComponentState
{
    FString MeshPath;
    TArray<FString> MaterialPaths;
    FTransform RelativeTransform = FTransform::Identity;
    FTransform WorldTransform = FTransform::Identity;
    ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
    TArray<TEnumAsByte<ECollisionResponse>> CollisionResponses;
    bool bVisible = false;
    bool bHiddenInGame = true;
    bool bActive = false;
    bool bAutoActivate = false;
    bool bGenerateOverlapEvents = false;
    TArray<FTransform> InstanceWorldTransforms;
};

struct FV3WorldSnapshot
{
    FTransform V2ActorTransform = FTransform::Identity;
    FTransform V3ActorTransform = FTransform::Identity;
    TArray<FComponentState> V2Components;
    TArray<FComponentState> V3Components;
    TArray<FComponentState> SceneComponents;
    FString GameModeClassPath;
};

ATRIADIstanaPublicViewSceneActor* FindScene(UWorld* World)
{
    ATRIADIstanaPublicViewSceneActor* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(World); It; ++It)
    {
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

ATRIADIstanaPublicViewRuntimePolicyActor* FindPolicy(UWorld* World)
{
    ATRIADIstanaPublicViewRuntimePolicyActor* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ATRIADIstanaPublicViewRuntimePolicyActor> It(World); It; ++It)
    {
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

ATRIADIstanaExploreV2LandscapeActor* FindV2Landscape(UWorld* World)
{
    ATRIADIstanaExploreV2LandscapeActor* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ATRIADIstanaExploreV2LandscapeActor> It(World); It; ++It)
    {
        if (!It->Tags.Contains(V2LandscapeTag))
        {
            continue;
        }
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

ATRIADIstanaExploreV3SupplementActor* FindV3Supplement(UWorld* World)
{
    ATRIADIstanaExploreV3SupplementActor* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ATRIADIstanaExploreV3SupplementActor> It(World); It; ++It)
    {
        if (!It->Tags.Contains(V3SupplementTag))
        {
            continue;
        }
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

ATRIADIstanaExploreV4LandscapeActor* FindV4Landscape(UWorld* World)
{
    ATRIADIstanaExploreV4LandscapeActor* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ATRIADIstanaExploreV4LandscapeActor> It(World); It; ++It)
    {
        if (!It->Tags.Contains(V4LandscapeTag))
        {
            continue;
        }
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

TArray<UHierarchicalInstancedStaticMeshComponent*> V2Components(
    ATRIADIstanaExploreV2LandscapeActor* Landscape)
{
    if (!Landscape)
    {
        return {};
    }
    return {
        Landscape->UmbrellaBroadleafInstances,
        Landscape->ColumnarBroadleafInstances,
        Landscape->DomeBroadleafInstances,
        Landscape->PalmInstances,
        Landscape->ShrubInstancesA,
        Landscape->ShrubInstancesB,
        Landscape->HedgeInstances,
        Landscape->GroundcoverInstances,
        Landscape->NearTurfInstances,
        Landscape->MeadowSedgeInstances,
        Landscape->TreeTrunkPawnBlockers};
}

TArray<UStaticMeshComponent*> V3Components(
    ATRIADIstanaExploreV3SupplementActor* Supplement)
{
    if (!Supplement)
    {
        return {};
    }
    return {
        Supplement->IslandTree01Instances,
        Supplement->DarkColumnarBroadleafInstances,
        Supplement->Shrub02Instances,
        Supplement->Fern02Instances,
        Supplement->Moss01Instances,
        Supplement->BermudaGrassInstances,
        Supplement->AmbientCgNearTurfInstances,
        Supplement->SupplementalTreePawnBlockers,
        Supplement->PorticoV7RenderOnlyComponent,
        Supplement->LowFrequencyTerrainComponent,
        Supplement->OsmPublicRoadsComponent,
        Supplement->UraIndicativeRoadsComponent,
        Supplement->OsmWaterComponent};
}

TArray<UStaticMeshComponent*> SceneComponents(
    ATRIADIstanaPublicViewSceneActor* Scene)
{
    if (!Scene)
    {
        return {};
    }
    return {
        Scene->BuildingHeroVisualComponent,
        Scene->BuildingCollisionComponent,
        Scene->TerrainComponent,
        Scene->TerrainSkirtComponent,
        Scene->HardscapeComponent,
        Scene->ContextBuildingsComponent,
        Scene->OSMContextBuildingsComponent,
        Scene->RainTreeInstances,
        Scene->PalmTreeInstances,
        Scene->FramingTreeInstances};
}

bool CaptureComponentState(
    UStaticMeshComponent* Component,
    FComponentState& OutState,
    FString& OutError)
{
    if (!Component || !Component->GetStaticMesh())
    {
        OutError = TEXT("A required inherited V3 component/mesh is absent.");
        return false;
    }
    OutState = FComponentState();
    OutState.MeshPath = Component->GetStaticMesh()->GetPathName();
    for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
    {
        const UMaterialInterface* Material = Component->GetMaterial(Slot);
        if (!Material)
        {
            OutError = TEXT("An inherited component has a null material slot.");
            return false;
        }
        OutState.MaterialPaths.Add(Material->GetPathName());
    }
    OutState.RelativeTransform = Component->GetRelativeTransform();
    OutState.WorldTransform = Component->GetComponentTransform();
    OutState.CollisionEnabled = Component->GetCollisionEnabled();
    for (int32 Channel = 0;
         Channel <= static_cast<int32>(ECC_GameTraceChannel18);
         ++Channel)
    {
        OutState.CollisionResponses.Add(Component->GetCollisionResponseToChannel(
            static_cast<ECollisionChannel>(Channel)));
    }
    OutState.bVisible = Component->IsVisible();
    OutState.bHiddenInGame = Component->bHiddenInGame;
    OutState.bActive = Component->IsActive();
    OutState.bAutoActivate = Component->bAutoActivate;
    OutState.bGenerateOverlapEvents = Component->GetGenerateOverlapEvents();
    if (const UHierarchicalInstancedStaticMeshComponent* Hism =
            Cast<UHierarchicalInstancedStaticMeshComponent>(Component))
    {
        for (int32 Index = 0; Index < Hism->GetInstanceCount(); ++Index)
        {
            FTransform Transform;
            if (!Hism->GetInstanceTransform(Index, Transform, true))
            {
                OutError = TEXT("Could not snapshot an inherited HISM world transform.");
                return false;
            }
            OutState.InstanceWorldTransforms.Add(Transform);
        }
    }
    OutError.Reset();
    return true;
}

bool MatchesComponentState(
    UStaticMeshComponent* Component,
    const FComponentState& Expected,
    bool bRequireHidden,
    const FString& OptionalExactSlotZeroMaterial,
    FString& OutError)
{
    FComponentState Current;
    if (!CaptureComponentState(Component, Current, OutError))
    {
        return false;
    }
    TArray<FString> ExpectedMaterials = Expected.MaterialPaths;
    if (!OptionalExactSlotZeroMaterial.IsEmpty())
    {
        if (ExpectedMaterials.IsEmpty())
        {
            OutError = TEXT("An approved terrain slot-0 delta has no inherited slot 0.");
            return false;
        }
        ExpectedMaterials[0] = OptionalExactSlotZeroMaterial;
    }
    const bool bVisibilityExact = bRequireHidden
        ? !Current.bVisible && Current.bHiddenInGame
        : Current.bVisible == Expected.bVisible &&
            Current.bHiddenInGame == Expected.bHiddenInGame;
    if (Current.MeshPath != Expected.MeshPath ||
        Current.MaterialPaths != ExpectedMaterials ||
        !Current.RelativeTransform.Equals(Expected.RelativeTransform, 0.001f) ||
        !Current.WorldTransform.Equals(Expected.WorldTransform, 0.001f) ||
        Current.CollisionEnabled != Expected.CollisionEnabled ||
        Current.CollisionResponses != Expected.CollisionResponses ||
        !bVisibilityExact ||
        Current.bActive != Expected.bActive ||
        Current.bAutoActivate != Expected.bAutoActivate ||
        Current.bGenerateOverlapEvents != Expected.bGenerateOverlapEvents ||
        Current.InstanceWorldTransforms.Num() !=
            Expected.InstanceWorldTransforms.Num())
    {
        OutError = TEXT("An inherited V3 component changed outside the exact approved V4 visibility/material deltas.");
        return false;
    }
    for (int32 Index = 0; Index < Current.InstanceWorldTransforms.Num(); ++Index)
    {
        if (!Current.InstanceWorldTransforms[Index].Equals(
                Expected.InstanceWorldTransforms[Index], 0.001f))
        {
            OutError = TEXT("An inherited V3 instance world transform changed.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool CaptureV3WorldSnapshot(
    UWorld* World,
    FV3WorldSnapshot& OutSnapshot,
    FString& OutError)
{
    ATRIADIstanaExploreV2LandscapeActor* V2 = FindV2Landscape(World);
    ATRIADIstanaExploreV3SupplementActor* V3 = FindV3Supplement(World);
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(World);
    if (!World || !V2 || !V3 || !Scene || !World->GetWorldSettings() ||
        !World->GetWorldSettings()->DefaultGameMode)
    {
        OutError = TEXT("The exact inherited V3 world/actor/game-mode roster is absent.");
        return false;
    }
    OutSnapshot = FV3WorldSnapshot();
    OutSnapshot.V2ActorTransform = V2->GetActorTransform();
    OutSnapshot.V3ActorTransform = V3->GetActorTransform();
    for (UStaticMeshComponent* Component : V2Components(V2))
    {
        FComponentState State;
        if (!CaptureComponentState(Component, State, OutError))
        {
            return false;
        }
        OutSnapshot.V2Components.Add(MoveTemp(State));
    }
    for (UStaticMeshComponent* Component : V3Components(V3))
    {
        FComponentState State;
        if (!CaptureComponentState(Component, State, OutError))
        {
            return false;
        }
        OutSnapshot.V3Components.Add(MoveTemp(State));
    }
    for (UStaticMeshComponent* Component : SceneComponents(Scene))
    {
        FComponentState State;
        if (!CaptureComponentState(Component, State, OutError))
        {
            return false;
        }
        OutSnapshot.SceneComponents.Add(MoveTemp(State));
    }
    OutSnapshot.GameModeClassPath =
        World->GetWorldSettings()->DefaultGameMode.Get()->GetPathName();
    OutError.Reset();
    return true;
}

bool ValidateExactV3SourceWorld(UWorld* World, FString& OutError)
{
    ATRIADIstanaExploreV2LandscapeActor* V2 = FindV2Landscape(World);
    ATRIADIstanaExploreV3SupplementActor* V3 = FindV3Supplement(World);
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(World);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(World);
    FString V2Report;
    FString V3Report;
    FString SceneReport;
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != SourceMapPackage ||
        !V2 || !V3 || !Scene || !Policy || Policy->bEnforceFixedPrimaryCamera ||
        !V2->ValidateExploreV2Landscape(V2Report) ||
        !V3->ValidateExploreV3Supplement(V3Report) ||
        !Scene->ValidatePublicViewScene(SceneReport, false))
    {
        OutError = TEXT("Exact Explore V3 source validation failed. ") +
            V2Report + TEXT(" ") + V3Report + TEXT(" ") + SceneReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateTargetInheritance(
    UWorld* TargetWorld,
    const FV3WorldSnapshot& Source,
    const FString& V4LawnMaterialObjectPath,
    FString& OutError)
{
    ATRIADIstanaExploreV2LandscapeActor* V2 = FindV2Landscape(TargetWorld);
    ATRIADIstanaExploreV3SupplementActor* V3 = FindV3Supplement(TargetWorld);
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(TargetWorld);
    const TArray<UHierarchicalInstancedStaticMeshComponent*> V2Live =
        V2Components(V2);
    const TArray<UStaticMeshComponent*> V3Live = V3Components(V3);
    const TArray<UStaticMeshComponent*> SceneLive = SceneComponents(Scene);
    if (!V2 || !V3 || !Scene ||
        !V2->GetActorTransform().Equals(Source.V2ActorTransform, 0.001f) ||
        !V3->GetActorTransform().Equals(Source.V3ActorTransform, 0.001f) ||
        V2Live.Num() != Source.V2Components.Num() ||
        V3Live.Num() != Source.V3Components.Num() ||
        SceneLive.Num() != Source.SceneComponents.Num())
    {
        OutError = TEXT("The inherited V3 actor/component roster changed.");
        return false;
    }
    for (int32 Index = 0; Index < V2Live.Num(); ++Index)
    {
        const bool bRequireHidden = Index >= 0 && Index <= 3;
        if (!MatchesComponentState(
                V2Live[Index], Source.V2Components[Index], bRequireHidden,
                FString(), OutError))
        {
            return false;
        }
    }
    for (int32 Index = 0; Index < V3Live.Num(); ++Index)
    {
        const bool bRequireHidden = Index == 1 || Index == 6 || Index == 8;
        if (!MatchesComponentState(
                V3Live[Index], Source.V3Components[Index], bRequireHidden,
                FString(), OutError))
        {
            return false;
        }
    }
    for (int32 Index = 0; Index < SceneLive.Num(); ++Index)
    {
        const FString MaterialOverride = Index == 2
            ? V4LawnMaterialObjectPath
            : FString();
        if (!MatchesComponentState(
                SceneLive[Index], Source.SceneComponents[Index], false,
                MaterialOverride, OutError))
        {
            return false;
        }
    }
    if (V2Live[0]->GetInstanceCount() != ExpectedV2UmbrellaCount ||
        V2Live[1]->GetInstanceCount() != ExpectedV2ColumnarCount ||
        V2Live[2]->GetInstanceCount() != ExpectedV2DomeCount ||
        V2Live[3]->GetInstanceCount() != ExpectedV2PalmCount ||
        V2Live[10]->GetInstanceCount() != ExpectedV2TreeUnionCount ||
        !V3->DarkColumnarBroadleafInstances ||
        V3->DarkColumnarBroadleafInstances->GetInstanceCount() !=
            ExpectedV2ColumnarCount ||
        !V3->AmbientCgNearTurfInstances ||
        V3->AmbientCgNearTurfInstances->GetInstanceCount() != 18432 ||
        V3Live[8]->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
    {
        OutError = TEXT("The exact V2 tree union/blockers, V3 replacement/turf or V7 collision census changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

struct FFrozenRasterGrid
{
    TArray<uint8> WorldCover;
    TArray<uint8> CanopyHeightMeters;
    TArray<uint8> CanopyPredictiveSdMeters;
};

bool DecodeExactBase64Band(
    const TSharedPtr<FJsonObject>& Encoding,
    const TCHAR* Field,
    TArray<uint8>& OutBand,
    FString& OutError)
{
    FString Encoded;
    if (!Encoding || !Encoding->TryGetStringField(Field, Encoded) ||
        !FBase64::Decode(Encoded, OutBand) ||
        OutBand.Num() != RasterGridPixels)
    {
        OutError = FString::Printf(
            TEXT("Frozen raster band '%s' did not decode to exactly 40,401 uint8 samples."),
            Field);
        return false;
    }
    return true;
}

bool LoadFrozenRasterGrid(FFrozenRasterGrid& OutGrid, FString& OutError)
{
    if (!ValidateFrozenPublicDataContracts(OutError))
    {
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    const FString Path = ProjectSourcePath(
        TEXT("IstanaPublicViewExploreV4/Prepared/v4_raster_sample_grid.json"));
    if (!LoadJsonObject(Path, Root) ||
        !Root->HasTypedField<EJson::Object>(TEXT("encoding")))
    {
        OutError = TEXT("Frozen V4 raster-grid JSON is absent or malformed.");
        return false;
    }
    const TSharedPtr<FJsonObject> Encoding =
        Root->GetObjectField(TEXT("encoding"));
    const TSharedPtr<FJsonObject> SamplingPolicy =
        Root->GetObjectField(TEXT("samplingPolicy"));
    const TArray<TSharedPtr<FJsonValue>>& HeightBands =
        SamplingPolicy->GetArrayField(TEXT("canopyHeightBandsMeters"));
    const auto HasExactBand = [&HeightBands](
        int32 Index, int32 Minimum, int32 Maximum)
    {
        if (!HeightBands.IsValidIndex(Index))
        {
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>& Band =
            HeightBands[Index]->AsArray();
        return Band.Num() == 2 &&
            static_cast<int32>(Band[0]->AsNumber()) == Minimum &&
            static_cast<int32>(Band[1]->AsNumber()) == Maximum;
    };
    if (Encoding->GetStringField(TEXT("format")) !=
            TEXT("RFC4648_BASE64_OF_RAW_ROW_MAJOR_UINT8") ||
        Encoding->GetIntegerField(TEXT("decodedBytesPerBand")) !=
            RasterGridPixels ||
        HeightBands.Num() != 3 ||
        !HasExactBand(0, 1, 16) || !HasExactBand(1, 17, 22) ||
        !HasExactBand(2, 23, 32) ||
        !DecodeExactBase64Band(
            Encoding, TEXT("worldCover"), OutGrid.WorldCover, OutError) ||
        !DecodeExactBase64Band(
            Encoding, TEXT("canopyHeightMeters"),
            OutGrid.CanopyHeightMeters, OutError) ||
        !DecodeExactBase64Band(
            Encoding, TEXT("canopyHeightPredictiveSdMeters"),
            OutGrid.CanopyPredictiveSdMeters, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Frozen V4 raster encoding policy changed.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool SampleRasterGrid(
    const FFrozenRasterGrid& Grid,
    const FVector& WorldLocationCm,
    uint8& OutWorldCover,
    uint8& OutCanopyHeight,
    uint8& OutCanopySd)
{
    const float Xmeters = WorldLocationCm.X / 100.0f;
    const float Ymeters = WorldLocationCm.Y / 100.0f;
    const int32 Column = FMath::RoundToInt(
        (Xmeters - RasterGridMinimumMeters) / RasterGridStepMeters);
    const int32 Row = FMath::RoundToInt(
        (Ymeters - RasterGridMinimumMeters) / RasterGridStepMeters);
    if (Column < 0 || Column >= RasterGridWidth ||
        Row < 0 || Row >= RasterGridHeight)
    {
        return false;
    }
    const int32 Index = Row * RasterGridWidth + Column;
    if (!Grid.WorldCover.IsValidIndex(Index) ||
        !Grid.CanopyHeightMeters.IsValidIndex(Index) ||
        !Grid.CanopyPredictiveSdMeters.IsValidIndex(Index))
    {
        return false;
    }
    OutWorldCover = Grid.WorldCover[Index];
    OutCanopyHeight = Grid.CanopyHeightMeters[Index];
    OutCanopySd = Grid.CanopyPredictiveSdMeters[Index];
    return true;
}

ETRIADIstanaExploreV4CanopyHeightBand HeightBandForSample(uint8 Value)
{
    if (Value == 255 || Value == 0)
    {
        return ETRIADIstanaExploreV4CanopyHeightBand::NoData;
    }
    if (Value <= 16)
    {
        return ETRIADIstanaExploreV4CanopyHeightBand::Low18To23Meters;
    }
    if (Value <= 22)
    {
        return ETRIADIstanaExploreV4CanopyHeightBand::Mid23To29Meters;
    }
    return ETRIADIstanaExploreV4CanopyHeightBand::High29To37Meters;
}

bool GatherExactTreeUnionAndRasterCues(
    ATRIADIstanaExploreV2LandscapeActor* V2,
    ATRIADIstanaExploreV3SupplementActor* V3,
    const FFrozenRasterGrid& Grid,
    TArray<FTransform>& OutTransforms,
    TArray<FTRIADIstanaExploreV4RasterCue>& OutCues,
    FString& OutError)
{
    if (!V2 || !V3)
    {
        OutError = TEXT("V2/V3 actors required for the exact replacement union are absent.");
        return false;
    }
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components = {
        V2->UmbrellaBroadleafInstances,
        V2->ColumnarBroadleafInstances,
        V2->DomeBroadleafInstances,
        V2->PalmInstances};
    const TArray<int32> Counts = {
        ExpectedV2UmbrellaCount,
        ExpectedV2ColumnarCount,
        ExpectedV2DomeCount,
        ExpectedV2PalmCount};
    OutTransforms.Reset();
    OutCues.Reset();
    TArray<FTransform> ExactColumnar;
    for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ++ComponentIndex)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            Components[ComponentIndex];
        if (!Component || Component->GetInstanceCount() != Counts[ComponentIndex])
        {
            OutError = TEXT("The exact 272+232+182+34 V2 tree component census changed.");
            return false;
        }
        for (int32 Instance = 0; Instance < Component->GetInstanceCount(); ++Instance)
        {
            FTransform Transform;
            if (!Component->GetInstanceTransform(Instance, Transform, true) ||
                Transform.ContainsNaN())
            {
                OutError = TEXT("Could not read one exact inherited V2 tree world transform.");
                return false;
            }
            if (ComponentIndex == 1)
            {
                ExactColumnar.Add(Transform);
            }
            uint8 WorldCover = 0;
            uint8 Height = 255;
            uint8 Sd = 255;
            if (!SampleRasterGrid(
                    Grid, Transform.GetLocation(), WorldCover, Height, Sd) ||
                !(WorldCover == 0 || WorldCover == 10 || WorldCover == 30 ||
                  WorldCover == 50 || WorldCover == 60 || WorldCover == 80) ||
                ((Height == 255) != (Sd == 255)))
            {
                OutError = TEXT("An inherited V2 tree did not resolve to one valid frozen 201x201 raster cue.");
                return false;
            }
            FTRIADIstanaExploreV4RasterCue Cue;
            Cue.BroadRasterClass = WorldCover;
            Cue.HeightBand = HeightBandForSample(Height);
            OutTransforms.Add(Transform);
            OutCues.Add(Cue);
        }
    }
    if (OutTransforms.Num() != ExpectedV2TreeUnionCount ||
        OutCues.Num() != ExpectedV2TreeUnionCount ||
        V3->DarkColumnarBroadleafInstances->GetInstanceCount() !=
            ExpectedV2ColumnarCount ||
        V3->PreservedV2ColumnarWorldTransforms.Num() !=
            ExpectedV2ColumnarCount)
    {
        OutError = TEXT("The exact V2 union or V3 232-columnar binding changed.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedV2ColumnarCount; ++Index)
    {
        FTransform V3Dark;
        if (!V3->DarkColumnarBroadleafInstances->GetInstanceTransform(
                Index, V3Dark, true) ||
            !V3Dark.Equals(ExactColumnar[Index], 0.001f) ||
            !V3->PreservedV2ColumnarWorldTransforms[Index].Equals(
                ExactColumnar[Index], 0.001f))
        {
            OutError = TEXT("V3 dark-columnar readback no longer matches the exact V2 232-transform subset.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool TraceInheritedTerrainOnly(
    UStaticMeshComponent* Terrain,
    const FVector2D& LocalXYCentimeters,
    FVector& OutGroundPoint,
    FString& OutError)
{
    if (!Terrain || Terrain->GetCollisionEnabled() !=
            ECollisionEnabled::QueryAndPhysics)
    {
        OutError = TEXT("Exact inherited QueryAndPhysics TerrainComponent is absent.");
        return false;
    }
    const FBodyInstance* TerrainBody = Terrain->GetBodyInstance();
    if (!Terrain->GetWorld() || !Terrain->IsRegistered() ||
        !Terrain->IsPhysicsStateCreated() || !TerrainBody ||
        !TerrainBody->IsValidBodyInstance())
    {
        OutError = TEXT("Exact inherited TerrainComponent is not registered with a valid physics body for terrain-only tracing.");
        return false;
    }
    const FVector Start(LocalXYCentimeters.X, LocalXYCentimeters.Y, 100000.0f);
    const FVector End(LocalXYCentimeters.X, LocalXYCentimeters.Y, -100000.0f);
    FCollisionQueryParams Params(
        SCENE_QUERY_STAT(TRIADExploreV4TerrainOnlyPlacement), true);
    FHitResult Hit;
    if (!Terrain->LineTraceComponent(Hit, Start, End, Params) ||
        Hit.Component.Get() != Terrain || Hit.Location.ContainsNaN())
    {
        OutError = TEXT("A V4 placement failed the terrain-component-only trace.");
        return false;
    }
    OutGroundPoint = Hit.Location + FVector(0.0f, 0.0f, 2.0f);
    OutError.Reset();
    return true;
}

struct FPlanarExclusionTriangle
{
    FVector2D A = FVector2D::ZeroVector;
    FVector2D B = FVector2D::ZeroVector;
    FVector2D C = FVector2D::ZeroVector;
    bool bFormalPlantingBedSurface = false;
};

struct FPlanarExclusionIndex
{
    static constexpr float CellSizeCm = 1000.0f;
    static constexpr float MaximumQueryMarginCm = 400.0f;
    static constexpr int32 MinimumCell = -102;
    static constexpr int32 MaximumCell = 102;

    TArray<FPlanarExclusionTriangle> Triangles;
    TMap<uint64, TArray<int32>> TriangleIndicesByCell;
    int32 SourceComponentCount = 0;
    int32 FormalPlantingTriangleCount = 0;

    static uint64 CellKey(int32 X, int32 Y)
    {
        return (static_cast<uint64>(static_cast<uint32>(X)) << 32u) |
            static_cast<uint32>(Y);
    }

    bool AddComponent(
        UStaticMeshComponent* Component,
        bool bRequireFormalPlantingSlot,
        FString& OutError)
    {
        UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
        const FStaticMeshRenderData* RenderData = Mesh
            ? Mesh->GetRenderData()
            : nullptr;
        if (!Component || !Mesh || !RenderData ||
            !RenderData->LODResources.IsValidIndex(0))
        {
            OutError = TEXT("An inherited planar exclusion component has no readable LOD0 geometry.");
            return false;
        }
        const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
        const FPositionVertexBuffer& Positions =
            Lod.VertexBuffers.PositionVertexBuffer;
        const FIndexArrayView Indices = Lod.IndexBuffer.GetArrayView();
        const FTransform ComponentToWorld = Component->GetComponentTransform();
        int32 AddedForComponent = 0;
        for (const FStaticMeshSection& Section : Lod.Sections)
        {
            bool bFormalPlantingSection = false;
            if (!Mesh->GetStaticMaterials().IsValidIndex(
                    Section.MaterialIndex))
            {
                OutError = TEXT("An inherited exclusion section has an invalid material index.");
                return false;
            }
            const FStaticMaterial& StaticMaterial =
                Mesh->GetStaticMaterials()[Section.MaterialIndex];
            UMaterialInterface* SectionMaterial = StaticMaterial.MaterialInterface;
            if (bRequireFormalPlantingSlot &&
                StaticMaterial.ImportedMaterialSlotName ==
                    TEXT("M_IPV_Planting") &&
                StaticMaterial.MaterialSlotName == TEXT("M_IPV_Planting") &&
                SectionMaterial &&
                SectionMaterial->GetPathName() ==
                    TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_Planting.M_IPV_Planting"))
            {
                bFormalPlantingSection = true;
                FormalPlantingTriangleCount +=
                    static_cast<int32>(Section.NumTriangles);
            }
            for (uint32 Triangle = 0;
                 Triangle < Section.NumTriangles;
                 ++Triangle)
            {
                const uint32 First = Section.FirstIndex + Triangle * 3u;
                if (First + 2u >= static_cast<uint32>(Indices.Num()))
                {
                    OutError = TEXT("An inherited exclusion section index escaped LOD0.");
                    return false;
                }
                const uint32 I0 = Indices[First];
                const uint32 I1 = Indices[First + 1u];
                const uint32 I2 = Indices[First + 2u];
                if (I0 >= Positions.GetNumVertices() ||
                    I1 >= Positions.GetNumVertices() ||
                    I2 >= Positions.GetNumVertices())
                {
                    OutError = TEXT("An inherited exclusion triangle has an invalid vertex index.");
                    return false;
                }
                const FVector P0 = ComponentToWorld.TransformPosition(
                    FVector(Positions.VertexPosition(I0)));
                const FVector P1 = ComponentToWorld.TransformPosition(
                    FVector(Positions.VertexPosition(I1)));
                const FVector P2 = ComponentToWorld.TransformPosition(
                    FVector(Positions.VertexPosition(I2)));
                if (P0.ContainsNaN() || P1.ContainsNaN() || P2.ContainsNaN())
                {
                    OutError = TEXT("An inherited exclusion triangle is non-finite.");
                    return false;
                }
                FPlanarExclusionTriangle Item;
                Item.A = FVector2D(P0.X, P0.Y);
                Item.B = FVector2D(P1.X, P1.Y);
                Item.C = FVector2D(P2.X, P2.Y);
                Item.bFormalPlantingBedSurface = bFormalPlantingSection;
                const int32 TriangleIndex = Triangles.Add(Item);
                const float MinimumX = FMath::Min3(
                    Item.A.X, Item.B.X, Item.C.X) - MaximumQueryMarginCm;
                const float MaximumX = FMath::Max3(
                    Item.A.X, Item.B.X, Item.C.X) + MaximumQueryMarginCm;
                const float MinimumY = FMath::Min3(
                    Item.A.Y, Item.B.Y, Item.C.Y) - MaximumQueryMarginCm;
                const float MaximumY = FMath::Max3(
                    Item.A.Y, Item.B.Y, Item.C.Y) + MaximumQueryMarginCm;
                const int32 FirstCellX = FMath::Clamp(
                    FMath::FloorToInt(MinimumX / CellSizeCm),
                    MinimumCell, MaximumCell);
                const int32 LastCellX = FMath::Clamp(
                    FMath::FloorToInt(MaximumX / CellSizeCm),
                    MinimumCell, MaximumCell);
                const int32 FirstCellY = FMath::Clamp(
                    FMath::FloorToInt(MinimumY / CellSizeCm),
                    MinimumCell, MaximumCell);
                const int32 LastCellY = FMath::Clamp(
                    FMath::FloorToInt(MaximumY / CellSizeCm),
                    MinimumCell, MaximumCell);
                for (int32 CellX = FirstCellX; CellX <= LastCellX; ++CellX)
                {
                    for (int32 CellY = FirstCellY;
                         CellY <= LastCellY;
                         ++CellY)
                    {
                        TriangleIndicesByCell.FindOrAdd(
                            CellKey(CellX, CellY)).Add(TriangleIndex);
                    }
                }
                ++AddedForComponent;
            }
        }
        if (AddedForComponent <= 0)
        {
            OutError = TEXT("An inherited planar exclusion component contributed no triangles.");
            return false;
        }
        ++SourceComponentCount;
        OutError.Reset();
        return true;
    }

    static double DistanceSquaredToSegment(
        const FVector2D& Point,
        const FVector2D& Start,
        const FVector2D& End)
    {
        const FVector2D Segment = End - Start;
        const double LengthSquared = Segment.SizeSquared();
        if (LengthSquared <= UE_DOUBLE_SMALL_NUMBER)
        {
            return FVector2D::DistSquared(Point, Start);
        }
        const double T = FMath::Clamp(
            FVector2D::DotProduct(Point - Start, Segment) / LengthSquared,
            0.0,
            1.0);
        return FVector2D::DistSquared(Point, Start + Segment * T);
    }

    static bool ContainsOrWithinMargin(
        const FPlanarExclusionTriangle& Triangle,
        const FVector2D& Point,
        float MarginCm)
    {
        const double C0 = FVector2D::CrossProduct(
            Triangle.B - Triangle.A, Point - Triangle.A);
        const double C1 = FVector2D::CrossProduct(
            Triangle.C - Triangle.B, Point - Triangle.B);
        const double C2 = FVector2D::CrossProduct(
            Triangle.A - Triangle.C, Point - Triangle.C);
        const double ProjectedDoubleArea = FMath::Abs(
            FVector2D::CrossProduct(
                Triangle.B - Triangle.A,
                Triangle.C - Triangle.A));
        const bool bInside = ProjectedDoubleArea > 0.001 &&
            ((C0 >= -0.001 && C1 >= -0.001 && C2 >= -0.001) ||
             (C0 <= 0.001 && C1 <= 0.001 && C2 <= 0.001));
        const double MarginSquared =
            static_cast<double>(MarginCm) * MarginCm;
        return bInside ||
            DistanceSquaredToSegment(
                Point, Triangle.A, Triangle.B) <= MarginSquared ||
            DistanceSquaredToSegment(
                Point, Triangle.B, Triangle.C) <= MarginSquared ||
            DistanceSquaredToSegment(
                Point, Triangle.C, Triangle.A) <= MarginSquared;
    }

    bool IsExcluded(
        const FVector2D& PointCm,
        float MarginCm,
        bool bAllowFormalPlantingBedSurface = false) const
    {
        if (!FMath::IsFinite(MarginCm) || MarginCm < 0.0f ||
            MarginCm > MaximumQueryMarginCm)
        {
            return true;
        }
        const int32 CellX = FMath::Clamp(
            FMath::FloorToInt(PointCm.X / CellSizeCm),
            MinimumCell, MaximumCell);
        const int32 CellY = FMath::Clamp(
            FMath::FloorToInt(PointCm.Y / CellSizeCm),
            MinimumCell, MaximumCell);
        const TArray<int32>* Candidates = TriangleIndicesByCell.Find(
            CellKey(CellX, CellY));
        if (!Candidates)
        {
            return false;
        }
        for (const int32 Index : *Candidates)
        {
            if (!Triangles.IsValidIndex(Index))
            {
                return true;
            }
            if (bAllowFormalPlantingBedSurface &&
                Triangles[Index].bFormalPlantingBedSurface)
            {
                continue;
            }
            if (
                ContainsOrWithinMargin(Triangles[Index], PointCm, MarginCm))
            {
                return true;
            }
        }
        return false;
    }
};

struct FPlacementRoleAudit
{
    FString Role;
    int32 Attempts = 0;
    int32 RasterRejected = 0;
    int32 PlanarRejected = 0;
    int32 FormalRejected = 0;
    int32 TreeRejected = 0;
    int32 MutualRejected = 0;
    int32 TraceRejected = 0;
    int32 Accepted = 0;
};

struct FPlacementAudit
{
    int32 ExclusionSourceComponentCount = 0;
    int32 ExclusionTriangleCount = 0;
    int32 FormalPlantingTriangleCount = 0;
    TArray<FPlacementRoleAudit> Roles;

    FString BuildPersistedTag() const
    {
        FString Value = FString::Printf(
            TEXT("TRIAD_IPV4_EXCLUSION_V1_C%d_T%d_B%d"),
            ExclusionSourceComponentCount,
            ExclusionTriangleCount,
            FormalPlantingTriangleCount);
        for (const FPlacementRoleAudit& Role : Roles)
        {
            Value += FString::Printf(
                TEXT("_%s_A%d_R%d_P%d_F%d_T%d_M%d_Z%d_N%d"),
                *Role.Role,
                Role.Attempts,
                Role.RasterRejected,
                Role.PlanarRejected,
                Role.FormalRejected,
                Role.TreeRejected,
                Role.MutualRejected,
                Role.TraceRejected,
                Role.Accepted);
        }
        return Value;
    }
};

bool BuildInheritedPlanarExclusionIndex(
    ATRIADIstanaPublicViewSceneActor* Scene,
    ATRIADIstanaExploreV3SupplementActor* V3,
    FPlanarExclusionIndex& OutIndex,
    FString& OutError)
{
    const TArray<UStaticMeshComponent*> ExactSources = {
        Scene ? Scene->BuildingHeroVisualComponent.Get() : nullptr,
        Scene ? Scene->BuildingCollisionComponent.Get() : nullptr,
        Scene ? Scene->HardscapeComponent.Get() : nullptr,
        Scene ? Scene->ContextBuildingsComponent.Get() : nullptr,
        Scene ? Scene->OSMContextBuildingsComponent.Get() : nullptr,
        V3 ? V3->OsmPublicRoadsComponent.Get() : nullptr,
        V3 ? V3->UraIndicativeRoadsComponent.Get() : nullptr,
        V3 ? V3->OsmWaterComponent.Get() : nullptr};
    if (ExactSources.Num() != 8 || ExactSources.Contains(nullptr))
    {
        OutError = TEXT("The exact eight inherited building/hardscape/road/water exclusion components are absent.");
        return false;
    }
    OutIndex = FPlanarExclusionIndex();
    for (int32 SourceIndex = 0; SourceIndex < ExactSources.Num(); ++SourceIndex)
    {
        if (!OutIndex.AddComponent(
                ExactSources[SourceIndex],
                SourceIndex == 2,
                OutError))
        {
            return false;
        }
    }
    if (OutIndex.SourceComponentCount != 8 ||
        OutIndex.FormalPlantingTriangleCount != 24 ||
        OutIndex.Triangles.IsEmpty() ||
        OutIndex.TriangleIndicesByCell.IsEmpty())
    {
        OutError = TEXT("The inherited exclusion spatial index is empty or incomplete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool AddHismWorldLocations(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    int32 ExactCountOrNegative,
    TArray<FVector2D>& OutLocations,
    FString& OutError)
{
    if (!Component ||
        (ExactCountOrNegative >= 0 &&
         Component->GetInstanceCount() != ExactCountOrNegative))
    {
        OutError = TEXT("An inherited tree-clearance HISM/census changed.");
        return false;
    }
    for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
    {
        FTransform Transform;
        if (!Component->GetInstanceTransform(Index, Transform, true) ||
            Transform.ContainsNaN())
        {
            OutError = TEXT("Could not read an inherited tree-clearance world transform.");
            return false;
        }
        OutLocations.Add(FVector2D(
            Transform.GetLocation().X, Transform.GetLocation().Y));
    }
    return true;
}

bool GatherInheritedTreeClearanceCenters(
    ATRIADIstanaExploreV2LandscapeActor* V2,
    ATRIADIstanaExploreV3SupplementActor* V3,
    ATRIADIstanaPublicViewSceneActor* Scene,
    const TArray<FTRIADIstanaExploreV4HeritageAnchor>& HeritageAnchors,
    TArray<FVector2D>& OutCenters,
    FString& OutError)
{
    OutCenters.Reset();
    if (!V2 || !V3 || !Scene || HeritageAnchors.Num() != 9 ||
        !AddHismWorldLocations(V2->UmbrellaBroadleafInstances,
            ExpectedV2UmbrellaCount, OutCenters, OutError) ||
        !AddHismWorldLocations(V2->ColumnarBroadleafInstances,
            ExpectedV2ColumnarCount, OutCenters, OutError) ||
        !AddHismWorldLocations(V2->DomeBroadleafInstances,
            ExpectedV2DomeCount, OutCenters, OutError) ||
        !AddHismWorldLocations(V2->PalmInstances,
            ExpectedV2PalmCount, OutCenters, OutError) ||
        !AddHismWorldLocations(V3->IslandTree01Instances,
            -1, OutCenters, OutError) ||
        !AddHismWorldLocations(Scene->RainTreeInstances,
            -1, OutCenters, OutError) ||
        !AddHismWorldLocations(Scene->PalmTreeInstances,
            -1, OutCenters, OutError) ||
        !AddHismWorldLocations(Scene->FramingTreeInstances,
            -1, OutCenters, OutError))
    {
        return false;
    }
    for (const FTRIADIstanaExploreV4HeritageAnchor& Anchor : HeritageAnchors)
    {
        OutCenters.Add(FVector2D(
            Anchor.LocalGroundLocationCm.X,
            Anchor.LocalGroundLocationCm.Y));
    }
    if (OutCenters.Num() < ExpectedV2TreeUnionCount + 9)
    {
        OutError = TEXT("Tree-clearance centers do not include all 720 V2 positions and nine Heritage anchors.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool IsWithinAnyClearance(
    const FVector2D& CandidateCm,
    const TArray<FVector2D>& CentersCm,
    float MinimumDistanceCm)
{
    const double MinimumSquared =
        static_cast<double>(MinimumDistanceCm) * MinimumDistanceCm;
    for (const FVector2D& Center : CentersCm)
    {
        if (FVector2D::DistSquared(CandidateCm, Center) < MinimumSquared)
        {
            return true;
        }
    }
    return false;
}

bool IsFountainOrFormalClearance(const FVector2D& Meters)
{
    const FVector2D Fountain(0.0f, 95.0f);
    const bool bFountain = FVector2D::Distance(Meters, Fountain) < 20.0f;
    const bool bFormalAxis = FMath::Abs(Meters.X) < 20.0f &&
        Meters.Y > 15.0f && Meters.Y < 310.0f;
    const bool bBuildingOrApron = FMath::Abs(Meters.X) < 118.0f &&
        Meters.Y > -90.0f && Meters.Y < 38.0f;
    return bFountain || bFormalAxis || bBuildingOrApron;
}

bool IsBroadFormalLawnClearance(const FVector2D& Meters)
{
    // Additive flank planting must not migrate onto the ceremonial lawn.  The
    // paired formal-bed prefix is the only caller allowed to bypass this broad
    // envelope; it still passes the exact geometry/fountain/tree clearances.
    return FMath::Abs(Meters.X) < 105.0f &&
        Meters.Y > 5.0f && Meters.Y < 225.0f;
}

bool BuildTracedPlantingTransforms(
    UStaticMeshComponent* Terrain,
    const FFrozenRasterGrid& Grid,
    int32 Count,
    int32 Seed,
    float MinimumRadiusMeters,
    float MaximumRadiusMeters,
    float MinimumScale,
    float MaximumScale,
    uint8 RequiredWorldCoverClass,
    const FPlanarExclusionIndex& ExclusionIndex,
    const TArray<FVector2D>& TreeClearanceCentersCm,
    float PlanarMarginCm,
    float TreeClearanceCm,
    float MutualClearanceCm,
    TArray<FVector2D>& InOutAcceptedNewCentersCm,
    FPlacementRoleAudit& OutAudit,
    TArray<FTransform>& OutTransforms,
    FString& OutError)
{
    if ((RequiredWorldCoverClass != 10 &&
         RequiredWorldCoverClass != 30) ||
        PlanarMarginCm < 0.0f ||
        PlanarMarginCm > FPlanarExclusionIndex::MaximumQueryMarginCm ||
        TreeClearanceCm <= 0.0f || MutualClearanceCm <= 0.0f ||
        OutAudit.Role.IsEmpty())
    {
        OutError = TEXT("A deterministic flank-placement policy is invalid.");
        return false;
    }
    FRandomStream Random(Seed);
    OutTransforms.Reset();
    constexpr int32 MaximumAttemptsPerInstance = 600;
    for (int32 Instance = 0; Instance < Count; ++Instance)
    {
        bool bPlaced = false;
        for (int32 Attempt = 0;
             Attempt < MaximumAttemptsPerInstance && !bPlaced;
             ++Attempt)
        {
            ++OutAudit.Attempts;
            const float Angle = Random.FRandRange(-PI, PI);
            const float Radius = FMath::Sqrt(Random.FRandRange(
                MinimumRadiusMeters * MinimumRadiusMeters,
                MaximumRadiusMeters * MaximumRadiusMeters));
            const FVector2D Meters(
                FMath::Cos(Angle) * Radius,
                FMath::Sin(Angle) * Radius + 90.0f);
            if (Meters.Size() > 960.0f ||
                IsFountainOrFormalClearance(Meters) ||
                IsBroadFormalLawnClearance(Meters))
            {
                ++OutAudit.FormalRejected;
                continue;
            }
            uint8 WorldCover = 0;
            uint8 Height = 255;
            uint8 Sd = 255;
            if (!SampleRasterGrid(
                    Grid, FVector(Meters.X * 100.0f, Meters.Y * 100.0f, 0.0f),
                    WorldCover, Height, Sd) ||
                WorldCover != RequiredWorldCoverClass)
            {
                ++OutAudit.RasterRejected;
                continue;
            }
            const FVector2D CandidateCm = Meters * 100.0f;
            if (ExclusionIndex.IsExcluded(CandidateCm, PlanarMarginCm))
            {
                ++OutAudit.PlanarRejected;
                continue;
            }
            if (IsWithinAnyClearance(
                    CandidateCm,
                    TreeClearanceCentersCm,
                    TreeClearanceCm))
            {
                ++OutAudit.TreeRejected;
                continue;
            }
            if (IsWithinAnyClearance(
                    CandidateCm,
                    InOutAcceptedNewCentersCm,
                    MutualClearanceCm))
            {
                ++OutAudit.MutualRejected;
                continue;
            }
            FVector Ground;
            if (!TraceInheritedTerrainOnly(
                    Terrain, CandidateCm, Ground, OutError))
            {
                ++OutAudit.TraceRejected;
                OutError.Reset();
                continue;
            }
            const float Uniform = Random.FRandRange(MinimumScale, MaximumScale);
            const float YScale = Uniform * Random.FRandRange(0.88f, 1.12f);
            OutTransforms.Add(FTransform(
                FRotator(0.0f, Random.FRandRange(-180.0f, 180.0f), 0.0f),
                Ground,
                FVector(Uniform, YScale, Uniform)));
            InOutAcceptedNewCentersCm.Add(CandidateCm);
            ++OutAudit.Accepted;
            bPlaced = true;
        }
        if (!bPlaced)
        {
            OutError = FString::Printf(
                TEXT("Terrain-only deterministic placement exhausted at %d/%d."),
                Instance, Count);
            return false;
        }
    }
    if (OutAudit.Accepted != Count || OutTransforms.Num() != Count)
    {
        OutError = TEXT("A deterministic flank-placement audit census changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool BuildPairedFormalBedTransforms(
    UStaticMeshComponent* Terrain,
    int32 Count,
    int32 Seed,
    float MinimumScale,
    float MaximumScale,
    const FPlanarExclusionIndex& ExclusionIndex,
    const TArray<FVector2D>& TreeClearanceCentersCm,
    float PlanarMarginCm,
    float TreeClearanceCm,
    float MutualClearanceCm,
    TArray<FVector2D>& InOutAcceptedNewCentersCm,
    FPlacementRoleAudit& OutAudit,
    TArray<FTransform>& OutTransforms,
    FString& OutError)
{
    if (Count <= 0 || (Count & 1) != 0 ||
        PlanarMarginCm < 0.0f ||
        PlanarMarginCm > FPlanarExclusionIndex::MaximumQueryMarginCm ||
        TreeClearanceCm <= 0.0f || MutualClearanceCm <= 0.0f ||
        OutAudit.Role.IsEmpty())
    {
        OutError = TEXT("A deterministic formal-bed placement policy is invalid.");
        return false;
    }
    FRandomStream Random(Seed);
    OutTransforms.Reset();
    constexpr int32 MaximumAttemptsPerPair = 2000;
    for (int32 Pair = 0; Pair < Count / 2; ++Pair)
    {
        bool bPlaced = false;
        for (int32 Attempt = 0;
             Attempt < MaximumAttemptsPerPair && !bPlaced;
             ++Attempt)
        {
            OutAudit.Attempts += 2;
            const int32 CoverageQuadrant = Pair & 3;
            const bool bHighAbsoluteX = (CoverageQuadrant & 1) != 0;
            const bool bHighY = (CoverageQuadrant & 2) != 0;
            const float AbsoluteX = Random.FRandRange(
                bHighAbsoluteX ? 34.125f : 20.75f,
                bHighAbsoluteX ? 47.25f : 33.875f);
            const float Y = Random.FRandRange(
                bHighY ? 88.125f : 79.75f,
                bHighY ? 96.25f : 87.875f);
            const FVector2D LeftMeters(-AbsoluteX, Y);
            const FVector2D RightMeters(AbsoluteX, Y);
            const FVector2D LeftCm = LeftMeters * 100.0f;
            const FVector2D RightCm = RightMeters * 100.0f;
            if (IsFountainOrFormalClearance(LeftMeters) ||
                IsFountainOrFormalClearance(RightMeters))
            {
                OutAudit.FormalRejected += 2;
                continue;
            }
            if (ExclusionIndex.IsExcluded(
                    LeftCm, PlanarMarginCm, true) ||
                ExclusionIndex.IsExcluded(
                    RightCm, PlanarMarginCm, true))
            {
                OutAudit.PlanarRejected += 2;
                continue;
            }
            if (IsWithinAnyClearance(
                    LeftCm,
                    TreeClearanceCentersCm,
                    TreeClearanceCm) ||
                IsWithinAnyClearance(
                    RightCm,
                    TreeClearanceCentersCm,
                    TreeClearanceCm))
            {
                OutAudit.TreeRejected += 2;
                continue;
            }
            if (IsWithinAnyClearance(
                    LeftCm,
                    InOutAcceptedNewCentersCm,
                    MutualClearanceCm) ||
                IsWithinAnyClearance(
                    RightCm,
                    InOutAcceptedNewCentersCm,
                    MutualClearanceCm))
            {
                OutAudit.MutualRejected += 2;
                continue;
            }
            FVector LeftGround;
            FVector RightGround;
            if (!TraceInheritedTerrainOnly(
                    Terrain, LeftCm, LeftGround, OutError) ||
                !TraceInheritedTerrainOnly(
                    Terrain, RightCm, RightGround, OutError))
            {
                OutAudit.TraceRejected += 2;
                OutError.Reset();
                continue;
            }
            const float Scale = Random.FRandRange(MinimumScale, MaximumScale);
            const float YScale =
                Scale * Random.FRandRange(0.88f, 1.12f);
            const float Yaw = Random.FRandRange(-180.0f, 180.0f);
            OutTransforms.Add(FTransform(
                FRotator(0.0f, Yaw, 0.0f),
                LeftGround,
                FVector(Scale, YScale, Scale)));
            OutTransforms.Add(FTransform(
                FRotator(0.0f, -Yaw, 0.0f),
                RightGround,
                FVector(Scale, YScale, Scale)));
            InOutAcceptedNewCentersCm.Add(LeftCm);
            InOutAcceptedNewCentersCm.Add(RightCm);
            OutAudit.Accepted += 2;
            bPlaced = true;
        }
        if (!bPlaced)
        {
            OutError = TEXT("Paired formal-bed terrain-only placement exhausted.");
            return false;
        }
    }
    int32 Left = 0;
    int32 Right = 0;
    for (const FTransform& Transform : OutTransforms)
    {
        const FVector LocationMeters = Transform.GetLocation() / 100.0f;
        const bool bInBed = FMath::Abs(LocationMeters.X) >= 20.75f &&
            FMath::Abs(LocationMeters.X) <= 47.25f &&
            LocationMeters.Y >= 79.75f && LocationMeters.Y <= 96.25f;
        if (!bInBed)
        {
            OutError = TEXT("A paired formal-bed placement escaped the exact bed envelope.");
            return false;
        }
        Left += LocationMeters.X < 0.0f ? 1 : 0;
        Right += LocationMeters.X > 0.0f ? 1 : 0;
    }
    if (OutTransforms.Num() != Count ||
        OutAudit.Accepted != Count ||
        Left != Count / 2 || Right != Count / 2)
    {
        OutError = TEXT("Paired formal-bed coverage lost its deterministic left/right census.");
        return false;
    }
    for (int32 Pair = 0; Pair < Count / 2; ++Pair)
    {
        const FVector LeftLocation =
            OutTransforms[Pair * 2].GetLocation() / 100.0f;
        const FVector RightLocation =
            OutTransforms[Pair * 2 + 1].GetLocation() / 100.0f;
        if (LeftLocation.X >= 0.0f || RightLocation.X <= 0.0f ||
            !FMath::IsNearlyEqual(
                -LeftLocation.X, RightLocation.X, 0.001f) ||
            !FMath::IsNearlyEqual(
                LeftLocation.Y, RightLocation.Y, 0.001f))
        {
            OutError = TEXT("A formal planting pair lost its mirrored XY layout.");
            return false;
        }
    }
    int32 CoverageQuadrants[4] = {0, 0, 0, 0};
    for (int32 Pair = 0; Pair < Count / 2; ++Pair)
    {
        const FVector LocationMeters =
            OutTransforms[Pair * 2].GetLocation() / 100.0f;
        const int32 Quadrant =
            (FMath::Abs(LocationMeters.X) >= 34.0f ? 1 : 0) |
            (LocationMeters.Y >= 88.0f ? 2 : 0);
        ++CoverageQuadrants[Quadrant];
    }
    for (const int32 QuadrantCount : CoverageQuadrants)
    {
        if (QuadrantCount != Count / 8)
        {
            OutError = TEXT("A paired formal-bed role lost exact four-quadrant planting-box coverage.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool GatherExactV3CloseTurfTransforms(
    ATRIADIstanaExploreV3SupplementActor* V3,
    TArray<FTransform>& OutTransforms,
    FString& OutError)
{
    UHierarchicalInstancedStaticMeshComponent* Source =
        V3 ? V3->AmbientCgNearTurfInstances.Get() : nullptr;
    if (!Source || Source->GetInstanceCount() != 18432)
    {
        OutError = TEXT("Exact 18,432-transform V3 close-turf source is absent.");
        return false;
    }
    OutTransforms.Reset();
    for (int32 Index = 0; Index < Source->GetInstanceCount(); ++Index)
    {
        FTransform Transform;
        if (!Source->GetInstanceTransform(Index, Transform, true) ||
            Transform.ContainsNaN())
        {
            OutError = TEXT("Could not capture every exact V3 close-turf world transform.");
            return false;
        }
        OutTransforms.Add(Transform);
    }
    OutError.Reset();
    return true;
}

bool BuildSanitizedHeritageAnchors(
    UStaticMeshComponent* Terrain,
    TArray<FTRIADIstanaExploreV4HeritageAnchor>& OutAnchors,
    FString& OutError)
{
    struct FExpectedAnchor
    {
        const TCHAR* Id;
        double X;
        double Y;
        double Height;
        double Girth;
        ETRIADIstanaExploreV4TreeForm Form;
    };
    const TArray<FExpectedAnchor> Expected = {
        {TEXT("HT2018-295"), -122.3025, 141.9932, 23.2, 3.53,
            ETRIADIstanaExploreV4TreeForm::Dome},
        {TEXT("HT2020-313"), -317.1625, -52.5359, 26.2, 3.30,
            ETRIADIstanaExploreV4TreeForm::Dome},
        {TEXT("HT2003-108"), 332.2176, 45.9237, 25.6, 6.55,
            ETRIADIstanaExploreV4TreeForm::Umbrella},
        {TEXT("HT2003-87"), -117.5828, 441.0249, 36.5, 4.23,
            ETRIADIstanaExploreV4TreeForm::Dome},
        {TEXT("HT2018-292"), 27.6713, -550.7064, 28.3, 3.07,
            ETRIADIstanaExploreV4TreeForm::HighForkRounded},
        {TEXT("HT2021-319"), 493.2631, 267.7526, 14.6, 3.50,
            ETRIADIstanaExploreV4TreeForm::Dome},
        {TEXT("HT2008-169"), 571.5710, -241.1864, 20.4, 7.20,
            ETRIADIstanaExploreV4TreeForm::Umbrella},
        {TEXT("HT2018-298"), 570.8166, 308.4544, 18.8, 3.67,
            ETRIADIstanaExploreV4TreeForm::Umbrella},
        {TEXT("HT2019-306"), 597.6401, 334.7457, 21.2, 4.88,
            ETRIADIstanaExploreV4TreeForm::Umbrella}};

    TSharedPtr<FJsonObject> Root;
    if (!LoadJsonObject(
            ProjectSourcePath(TEXT(
                "IstanaPublicViewExploreV4/explore_v4_geospatial.contract.json")),
            Root) ||
        !Root->HasTypedField<EJson::Array>(TEXT("heritageTreeAnchors")))
    {
        OutError = TEXT("Frozen sanitized Heritage anchor contract is absent.");
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>& Rows =
        Root->GetArrayField(TEXT("heritageTreeAnchors"));
    if (Rows.Num() != ExpectedHeritageAnchorCount ||
        Expected.Num() != ExpectedHeritageAnchorCount)
    {
        OutError = TEXT("Frozen Heritage anchor roster must contain exactly nine rows.");
        return false;
    }
    OutAnchors.Reset();
    for (int32 Index = 0; Index < Expected.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Row = Rows[Index]->AsObject();
        const FExpectedAnchor& Exact = Expected[Index];
        FString FormHint;
        const bool bHasPublishedQualitativeHint = Row &&
            Row->TryGetStringField(TEXT("formHint"), FormHint) &&
            !FormHint.IsEmpty();
        const FString ExpectedEvidenceStatus = Index < 2
            ? TEXT("NOT_STATED_NO_FORM_CLAIM")
            : TEXT("PUBLIC_PROFILE_QUALITATIVE_HINT");
        if (!Row || Row->GetStringField(TEXT("zoningId")) != Exact.Id ||
            !FMath::IsNearlyEqual(
                Row->GetNumberField(TEXT("localXMeters")), Exact.X, 0.00001) ||
            !FMath::IsNearlyEqual(
                Row->GetNumberField(TEXT("localYMeters")), Exact.Y, 0.00001) ||
            !FMath::IsNearlyEqual(
                Row->GetNumberField(TEXT("heightMeters")), Exact.Height, 0.00001) ||
            !FMath::IsNearlyEqual(
                Row->GetNumberField(TEXT("girthMeters")), Exact.Girth, 0.00001) ||
            Row->GetStringField(TEXT("formEvidenceStatus")) !=
                ExpectedEvidenceStatus ||
            bHasPublishedQualitativeHint != (Index >= 2))
        {
            OutError = TEXT("A frozen Heritage public position/height/girth/form-provenance row changed.");
            return false;
        }
        const FVector2D LocalXYCentimeters(
            Exact.X * 100.0, Exact.Y * 100.0);
        FVector Ground;
        FString TraceError;
        if (!TraceInheritedTerrainOnly(
                Terrain,
                LocalXYCentimeters,
                Ground,
                TraceError))
        {
            OutError = FString::Printf(
                TEXT("Heritage anchor terrain trace failed at index=%d id=%s localXYCm=(%.2f, %.2f) startZCm=100000.00 endZCm=-100000.00: %s"),
                Index,
                Exact.Id,
                LocalXYCentimeters.X,
                LocalXYCentimeters.Y,
                *TraceError);
            return false;
        }
        FTRIADIstanaExploreV4HeritageAnchor Anchor;
        Anchor.PublicRecordId = Exact.Id;
        Anchor.LocalGroundLocationCm = Ground;
        Anchor.PublishedHeightMeters = static_cast<float>(Exact.Height);
        Anchor.PublishedGirthMeters = static_cast<float>(Exact.Girth);
        Anchor.FormHint = Exact.Form;
        Anchor.bPositionComesFromPublishedRecord = true;
        Anchor.bVisualIsSilhouetteProxy = true;
        Anchor.bFormComesFromPublishedQualitativeHint = Index >= 2;
        OutAnchors.Add(MoveTemp(Anchor));
    }
    OutError.Reset();
    return true;
}

struct FTextureSourceSpec
{
    FString RelativePath;
    FString SourcePath;
    FString AssetName;
    int64 SourceBytes = 0;
    FString SourceSha256;
    int32 ExpectedWidth = 2048;
    int32 ExpectedHeight = 2048;
    bool bSrgb = false;
    TextureCompressionSettings Compression = TC_Default;
    TextureGroup Group = TEXTUREGROUP_World;
    bool bFlipGreen = false;
};

struct FWindMaterialSpec
{
    FString AssetName;
    ETRIADIstanaExploreV4WindRole Role =
        ETRIADIstanaExploreV4WindRole::UmbrellaTrunk;
    FName ImportedSlotName;
    FString BaseColorTexture;
    FString NormalTexture;
    FString RoughnessTexture;
    FString OpacityTexture;
    FString ProtectedSourceMaterialObjectPath;
    int64 ProtectedSourceMaterialBytes = 0;
    FString ProtectedSourceMaterialSha256;
    bool bMasked = false;
    bool bTwoSidedFoliage = false;
    float HeightCm = 100.0f;
    float ResponseScale = 0.5f;
    float MaximumWpoCm = 20.0f;
};

FString TextureObjectPath(const FString& Name)
{
    return ObjectPath(TextureAssetPath, Name);
}

FString MaterialObjectPath(const FString& Name)
{
    return ObjectPath(MaterialAssetPath, Name);
}

template <typename T>
T* AddMaterialExpression(UMaterial* Material, int32 EditorX, int32 EditorY)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    T* Expression = EditorOnly
        ? NewObject<T>(Material, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        EditorOnly->ExpressionCollection.Expressions.Add(Expression);
        Expression->MaterialExpressionEditorX = EditorX;
        Expression->MaterialExpressionEditorY = EditorY;
    }
    return Expression;
}

void ConnectCustomInput(
    UMaterialExpressionCustom* Custom,
    const FName Name,
    UMaterialExpression* Expression)
{
    FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
    Input.InputName = Name;
    Input.Input.Connect(0, Expression);
}

UMaterialExpressionTextureSampleParameter2D* AddTextureSample(
    UMaterial* Material,
    UTexture2D* Texture,
    const FName ParameterName,
    EMaterialSamplerType SamplerType,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionTextureSampleParameter2D* Sample =
        AddMaterialExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material, EditorX, EditorY);
    if (Sample)
    {
        Sample->Texture = Texture;
        Sample->ParameterName = ParameterName;
        Sample->SamplerType = SamplerType;
        Sample->SamplerSource = SSM_FromTextureAsset;
    }
    return Sample;
}

UMaterialExpression* AddTwoSidedSignNormalCorrection(
    UMaterial* Material,
    UMaterialExpression* SourceNormal,
    int32 SourceOutputIndex,
    FString& OutError)
{
    UMaterialExpressionConstant2Vector* TangentXY =
        AddMaterialExpression<UMaterialExpressionConstant2Vector>(
            Material, -500, 60);
    UMaterialExpressionTwoSidedSign* Sign =
        AddMaterialExpression<UMaterialExpressionTwoSidedSign>(
            Material, -500, 140);
    UMaterialExpressionAppendVector* Facing =
        AddMaterialExpression<UMaterialExpressionAppendVector>(
            Material, -300, 100);
    UMaterialExpressionMultiply* Corrected =
        AddMaterialExpression<UMaterialExpressionMultiply>(
            Material, -100, 40);
    if (!SourceNormal || !TangentXY || !Sign || !Facing || !Corrected)
    {
        OutError = TEXT("Could not allocate the exact TwoSidedSign foliage normal correction.");
        return nullptr;
    }
    TangentXY->R = 1.0f;
    TangentXY->G = 1.0f;
    Facing->A.Connect(0, TangentXY);
    Facing->B.Connect(0, Sign);
    Corrected->A.Connect(SourceOutputIndex, SourceNormal);
    Corrected->B.Connect(0, Facing);
    OutError.Reset();
    return Corrected;
}

EMaterialSamplerType SamplerTypeForTexture(const UTexture2D* Texture)
{
    if (Texture && Texture->CompressionSettings == TC_Normalmap)
    {
        return SAMPLERTYPE_Normal;
    }
    if (Texture && Texture->CompressionSettings == TC_Masks)
    {
        return SAMPLERTYPE_Masks;
    }
    return Texture && Texture->SRGB
        ? SAMPLERTYPE_Color
        : SAMPLERTYPE_LinearColor;
}

bool AddInstanceLocalWindGraph(
    UMaterial* Material,
    const FWindMaterialSpec& Spec,
    FString& OutError)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly || EditorOnly->WorldPositionOffset.Expression)
    {
        OutError = TEXT("V4 wind graph requires one fresh material with no prior WPO.");
        return false;
    }
    UMaterialExpressionWorldPosition* WorldPosition =
        AddMaterialExpression<UMaterialExpressionWorldPosition>(Material, -1320, 520);
    UMaterialExpressionTransformPosition* InstanceLocalPosition =
        AddMaterialExpression<UMaterialExpressionTransformPosition>(Material, -1080, 520);
    UMaterialExpressionTime* Time =
        AddMaterialExpression<UMaterialExpressionTime>(Material, -1080, 720);
    UMaterialExpressionScalarParameter* Strength =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -820, 520);
    UMaterialExpressionScalarParameter* Speed =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -820, 620);
    UMaterialExpressionScalarParameter* Height =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -820, 720);
    UMaterialExpressionScalarParameter* Response =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -820, 820);
    UMaterialExpressionScalarParameter* MaximumWpo =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -820, 920);
    UMaterialExpressionVectorParameter* Direction =
        AddMaterialExpression<UMaterialExpressionVectorParameter>(Material, -820, 1020);
    UMaterialExpressionCustom* Wind =
        AddMaterialExpression<UMaterialExpressionCustom>(Material, -430, 650);
    if (!WorldPosition || !InstanceLocalPosition || !Time || !Strength ||
        !Speed || !Height || !Response || !MaximumWpo || !Direction || !Wind)
    {
        OutError = TEXT("Could not allocate the complete V4 instance-local wind graph.");
        return false;
    }
    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    InstanceLocalPosition->TransformSourceType = TRANSFORMPOSSOURCE_World;
    InstanceLocalPosition->TransformType = TRANSFORMPOSSOURCE_Instance;
    InstanceLocalPosition->Input.Connect(0, WorldPosition);
    Strength->ParameterName = TEXT("TRIAD_WindStrengthCm");
    Strength->DefaultValue = 5.5f;
    Speed->ParameterName = TEXT("TRIAD_WindSpeed");
    Speed->DefaultValue = 1.45f;
    Height->ParameterName = TEXT("TRIAD_WindHeightCm");
    Height->DefaultValue = Spec.HeightCm;
    Response->ParameterName = TEXT("TRIAD_WindResponseScale");
    Response->DefaultValue = Spec.ResponseScale;
    MaximumWpo->ParameterName = TEXT("TRIAD_MaxWpoCm");
    MaximumWpo->DefaultValue = Spec.MaximumWpoCm;
    Direction->ParameterName = TEXT("TRIAD_WindDirection");
    Direction->DefaultValue = FLinearColor(0.93f, 0.37f, 0.0f, 0.0f);
    Wind->Description = WindCustomDescription;
    Wind->OutputType = CMOT_Float3;
    Wind->Code = WindCustomCode;
    Wind->Inputs.Reset();
    ConnectCustomInput(Wind, TEXT("WorldPosition"), WorldPosition);
    ConnectCustomInput(Wind, TEXT("InstanceLocalPosition"), InstanceLocalPosition);
    ConnectCustomInput(Wind, TEXT("TimeSeconds"), Time);
    ConnectCustomInput(Wind, TEXT("WindStrengthCm"), Strength);
    ConnectCustomInput(Wind, TEXT("WindSpeed"), Speed);
    ConnectCustomInput(Wind, TEXT("WindDirection"), Direction);
    ConnectCustomInput(Wind, TEXT("HeightCm"), Height);
    ConnectCustomInput(Wind, TEXT("ResponseScale"), Response);
    ConnectCustomInput(Wind, TEXT("MaxWpoCm"), MaximumWpo);
    EditorOnly->WorldPositionOffset.Connect(0, Wind);
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->MaxWorldPositionOffsetDisplacement = Spec.MaximumWpoCm;
    OutError.Reset();
    return true;
}

bool ValidateWindMaterial(
    UMaterial* Material,
    const FWindMaterialSpec& Spec,
    FString& OutError)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const UMaterialExpressionCustom* ExactWind = nullptr;
    const UMaterialExpressionTransformPosition* ExactTransform = nullptr;
    const UMaterialExpressionScalarParameter* Height = nullptr;
    const UMaterialExpressionScalarParameter* Response = nullptr;
    const UMaterialExpressionScalarParameter* MaximumWpo = nullptr;
    const UMaterialExpressionScalarParameter* Strength = nullptr;
    const UMaterialExpressionScalarParameter* Speed = nullptr;
    const UMaterialExpressionVectorParameter* Direction = nullptr;
    const UMaterialExpressionWorldPosition* WorldPosition = nullptr;
    const UMaterialExpressionTime* Time = nullptr;
    int32 CustomCount = 0;
    int32 TransformCount = 0;
    int32 TimeCount = 0;
    int32 WorldPositionCount = 0;
    int32 ScalarCount = 0;
    int32 VectorCount = 0;
    int32 TwoSidedSignCount = 0;
    if (EditorOnly)
    {
        for (const UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            if (const UMaterialExpressionCustom* Custom =
                    Cast<UMaterialExpressionCustom>(Expression))
            {
                ++CustomCount;
                if (Custom->Description == WindCustomDescription &&
                    Custom->Code == WindCustomCode)
                {
                    ExactWind = Custom;
                }
            }
            if (const UMaterialExpressionTransformPosition* Transform =
                    Cast<UMaterialExpressionTransformPosition>(Expression))
            {
                ++TransformCount;
                ExactTransform = Transform;
            }
            if (const UMaterialExpressionWorldPosition* Candidate =
                    Cast<UMaterialExpressionWorldPosition>(Expression))
            {
                ++WorldPositionCount;
                WorldPosition = Candidate;
            }
            if (const UMaterialExpressionTime* Candidate =
                    Cast<UMaterialExpressionTime>(Expression))
            {
                ++TimeCount;
                Time = Candidate;
            }
            TwoSidedSignCount +=
                Cast<UMaterialExpressionTwoSidedSign>(Expression) ? 1 : 0;
            if (const UMaterialExpressionScalarParameter* Scalar =
                    Cast<UMaterialExpressionScalarParameter>(Expression))
            {
                ++ScalarCount;
                if (Scalar->ParameterName == TEXT("TRIAD_WindHeightCm")) Height = Scalar;
                if (Scalar->ParameterName == TEXT("TRIAD_WindResponseScale")) Response = Scalar;
                if (Scalar->ParameterName == TEXT("TRIAD_MaxWpoCm")) MaximumWpo = Scalar;
                if (Scalar->ParameterName == TEXT("TRIAD_WindStrengthCm")) Strength = Scalar;
                if (Scalar->ParameterName == TEXT("TRIAD_WindSpeed")) Speed = Scalar;
            }
            if (const UMaterialExpressionVectorParameter* Vector =
                    Cast<UMaterialExpressionVectorParameter>(Expression))
            {
                ++VectorCount;
                if (Vector->ParameterName == TEXT("TRIAD_WindDirection")) Direction = Vector;
            }
        }
    }
    const TArray<FName> ExactInputNames = {
        TEXT("WorldPosition"), TEXT("InstanceLocalPosition"),
        TEXT("TimeSeconds"), TEXT("WindStrengthCm"), TEXT("WindSpeed"),
        TEXT("WindDirection"), TEXT("HeightCm"), TEXT("ResponseScale"),
        TEXT("MaxWpoCm")};
    const TArray<const UMaterialExpression*> ExactInputExpressions = {
        WorldPosition, ExactTransform, Time, Strength, Speed, Direction,
        Height, Response, MaximumWpo};
    bool bWindInputsExact = ExactWind && ExactWind->Inputs.Num() == 9;
    for (int32 Index = 0; bWindInputsExact && Index < 9; ++Index)
    {
        bWindInputsExact =
            ExactWind->Inputs[Index].InputName == ExactInputNames[Index] &&
            ExactWind->Inputs[Index].Input.Expression ==
                ExactInputExpressions[Index] &&
            ExactWind->Inputs[Index].Input.OutputIndex == 0;
    }
    const UMaterialExpressionMultiply* CorrectedNormal = EditorOnly
        ? Cast<UMaterialExpressionMultiply>(EditorOnly->Normal.Expression)
        : nullptr;
    const UMaterialExpressionAppendVector* Facing = CorrectedNormal
        ? Cast<UMaterialExpressionAppendVector>(CorrectedNormal->B.Expression)
        : nullptr;
    const UMaterialExpressionConstant2Vector* TangentXY = Facing
        ? Cast<UMaterialExpressionConstant2Vector>(Facing->A.Expression)
        : nullptr;
    const UMaterialExpressionTwoSidedSign* FacingSign = Facing
        ? Cast<UMaterialExpressionTwoSidedSign>(Facing->B.Expression)
        : nullptr;
    const bool bNormalCorrectionExact = Spec.bTwoSidedFoliage
        ? CorrectedNormal && CorrectedNormal->A.Expression && Facing &&
            TangentXY && FMath::IsNearlyEqual(TangentXY->R, 1.0f) &&
            FMath::IsNearlyEqual(TangentXY->G, 1.0f) && FacingSign &&
            TwoSidedSignCount == 1
        : TwoSidedSignCount == 0;
    if (!Material || !EditorOnly ||
        Material->GetPathName() != MaterialObjectPath(Spec.AssetName) ||
        Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5) ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Material->MaterialDomain != MD_Surface || Material->bUseMaterialAttributes ||
        Material->GetBlendMode() != (Spec.bMasked ? BLEND_Masked : BLEND_Opaque) ||
        Material->IsTwoSided() != Spec.bTwoSidedFoliage ||
        !Material->GetShadingModels().HasOnlyShadingModel(
            Spec.bTwoSidedFoliage ? MSM_TwoSidedFoliage : MSM_DefaultLit) ||
        !EditorOnly->BaseColor.Expression || !EditorOnly->Normal.Expression ||
        !EditorOnly->Roughness.Expression ||
        (Spec.bMasked && !EditorOnly->OpacityMask.Expression) ||
        (Spec.bTwoSidedFoliage && !EditorOnly->SubsurfaceColor.Expression) ||
        EditorOnly->Metallic.Expression || EditorOnly->EmissiveColor.Expression ||
        !bNormalCorrectionExact ||
        CustomCount != 1 || TransformCount != 1 || TimeCount != 1 ||
        WorldPositionCount != 1 || ScalarCount != 5 || VectorCount != 1 ||
        !ExactWind || !bWindInputsExact ||
        EditorOnly->WorldPositionOffset.Expression != ExactWind ||
        !ExactTransform ||
        ExactTransform->TransformSourceType != TRANSFORMPOSSOURCE_World ||
        ExactTransform->TransformType != TRANSFORMPOSSOURCE_Instance ||
        !Height || !FMath::IsNearlyEqual(Height->DefaultValue, Spec.HeightCm, 0.01f) ||
        !Response || !FMath::IsNearlyEqual(Response->DefaultValue, Spec.ResponseScale, 0.001f) ||
        !MaximumWpo || !FMath::IsNearlyEqual(MaximumWpo->DefaultValue, Spec.MaximumWpoCm, 0.001f) ||
        !Strength || !FMath::IsNearlyEqual(Strength->DefaultValue, 5.5f, 0.001f) ||
        !Speed || !FMath::IsNearlyEqual(Speed->DefaultValue, 1.45f, 0.001f) ||
        !Direction || !FMath::IsNearlyEqual(Direction->DefaultValue.R, 0.93f, 0.001f) ||
        !FMath::IsNearlyEqual(Direction->DefaultValue.G, 0.37f, 0.001f) ||
        !FMath::IsNearlyEqual(
            Material->MaxWorldPositionOffsetDisplacement,
            Spec.MaximumWpoCm,
            0.001f))
    {
        OutError = TEXT("A V4 material lost its exact PBR/instance-local wind/shader contract: ") +
            Spec.AssetName;
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterial* CreateWindMaterial(
    IAssetTools& AssetTools,
    const FWindMaterialSpec& Spec,
    const TMap<FString, UTexture2D*>& Textures,
    FString& OutError)
{
    UTexture2D* Base = Textures.FindRef(Spec.BaseColorTexture);
    UTexture2D* Normal = Spec.NormalTexture.IsEmpty()
        ? nullptr
        : Textures.FindRef(Spec.NormalTexture);
    UTexture2D* Roughness = Spec.RoughnessTexture.IsEmpty()
        ? nullptr
        : Textures.FindRef(Spec.RoughnessTexture);
    UTexture2D* Opacity = Spec.OpacityTexture.IsEmpty()
        ? nullptr
        : Textures.FindRef(Spec.OpacityTexture);
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            Spec.AssetName,
            MaterialAssetPath,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaExploreV4Assets"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly || !Base ||
        (Spec.bMasked && !Opacity) ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create fresh complete V4 wind material: ") +
            Spec.AssetName;
        return nullptr;
    }
    UMaterialExpressionTextureSampleParameter2D* BaseSample = AddTextureSample(
        Material, Base, TEXT("BaseColorTexture"), SAMPLERTYPE_Color, -720, -180);
    UMaterialExpression* NormalExpression = Normal
        ? static_cast<UMaterialExpression*>(AddTextureSample(
            Material, Normal, TEXT("NormalTexture"), SAMPLERTYPE_Normal, -720, 20))
        : static_cast<UMaterialExpression*>(
            AddMaterialExpression<UMaterialExpressionConstant3Vector>(
                Material, -720, 20));
    UMaterialExpression* RoughnessExpression = Roughness
        ? static_cast<UMaterialExpression*>(AddTextureSample(
            Material, Roughness, TEXT("RoughnessTexture"),
            SAMPLERTYPE_Masks, -720, 220))
        : static_cast<UMaterialExpression*>(
            AddMaterialExpression<UMaterialExpressionConstant>(
                Material, -720, 220));
    UMaterialExpressionTextureSampleParameter2D* OpacitySample = Spec.bMasked
        ? AddTextureSample(
            Material, Opacity, TEXT("OpacityTexture"),
            SAMPLERTYPE_Masks, -720, 400)
        : nullptr;
    if (UMaterialExpressionConstant3Vector* FlatNormal =
            Cast<UMaterialExpressionConstant3Vector>(NormalExpression))
    {
        FlatNormal->Constant = FLinearColor(0.0f, 0.0f, 1.0f, 0.0f);
    }
    if (UMaterialExpressionConstant* RoughConstant =
            Cast<UMaterialExpressionConstant>(RoughnessExpression))
    {
        RoughConstant->R = 0.78f;
    }
    UMaterialExpression* FinalNormalExpression = NormalExpression;
    if (Spec.bTwoSidedFoliage)
    {
        FinalNormalExpression = AddTwoSidedSignNormalCorrection(
            Material,
            NormalExpression,
            0,
            OutError);
    }
    if (!BaseSample || !NormalExpression || !RoughnessExpression ||
        !FinalNormalExpression ||
        (Spec.bMasked && !OpacitySample) ||
        !AddInstanceLocalWindGraph(Material, Spec, OutError))
    {
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->bUseMaterialAttributes = false;
    Material->BlendMode = Spec.bMasked ? BLEND_Masked : BLEND_Opaque;
    Material->TwoSided = Spec.bTwoSidedFoliage;
    Material->OpacityMaskClipValue = 0.333f;
    Material->SetShadingModel(
        Spec.bTwoSidedFoliage ? MSM_TwoSidedFoliage : MSM_DefaultLit);
    EditorOnly->BaseColor.Connect(0, BaseSample);
    EditorOnly->Normal.Connect(0, FinalNormalExpression);
    EditorOnly->Roughness.Connect(
        Roughness ? 1 : 0, RoughnessExpression);
    if (OpacitySample)
    {
        EditorOnly->OpacityMask.Connect(1, OpacitySample);
    }
    if (Spec.bTwoSidedFoliage)
    {
        EditorOnly->SubsurfaceColor.Connect(0, BaseSample);
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateWindMaterial(Material, Spec, OutError))
    {
        return nullptr;
    }
    OutError.Reset();
    return Material;
}

struct FSimpleMaterialSpec
{
    FString Name;
    FLinearColor Color;
    float Roughness = 0.7f;
    float Metallic = 0.0f;
};

const TArray<FSimpleMaterialSpec>& PorticoMaterialSpecs()
{
    static const TArray<FSimpleMaterialSpec> Specs = {
        {TEXT("M_IPV8_Portico_Stone"), FLinearColor(0.74f, 0.70f, 0.60f), 0.72f, 0.0f},
        {TEXT("M_IPV8_Portico_Trim"), FLinearColor(0.95f, 0.93f, 0.85f), 0.58f, 0.0f},
        {TEXT("M_IPV8_Portico_Render"), FLinearColor(0.84f, 0.81f, 0.70f), 0.76f, 0.0f},
        {TEXT("M_IPV8_Portico_Soffit"), FLinearColor(0.80f, 0.77f, 0.69f), 0.68f, 0.0f},
        {TEXT("M_IPV8_Portico_Recess"), FLinearColor(0.105f, 0.125f, 0.110f), 0.84f, 0.0f},
        {TEXT("M_IPV8_Portico_Louvre"), FLinearColor(0.255f, 0.285f, 0.225f), 0.78f, 0.0f},
        {TEXT("M_IPV8_Portico_Metal"), FLinearColor(0.145f, 0.160f, 0.140f), 0.64f, 0.18f}};
    return Specs;
}

UMaterial* CreateSimpleMaterial(
    IAssetTools& AssetTools,
    const FSimpleMaterialSpec& Spec,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            Spec.Name, MaterialAssetPath, UMaterial::StaticClass(), Factory,
            FName(TEXT("TRIAD.ImportIstanaExploreV4Assets"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create a fresh V8 portico base material.");
        return nullptr;
    }
    UMaterialExpressionConstant3Vector* Base =
        AddMaterialExpression<UMaterialExpressionConstant3Vector>(
            Material, -420, -80);
    UMaterialExpressionConstant* Rough =
        AddMaterialExpression<UMaterialExpressionConstant>(Material, -420, 80);
    UMaterialExpressionConstant* Metal =
        AddMaterialExpression<UMaterialExpressionConstant>(Material, -420, 220);
    if (!Base || !Rough || !Metal)
    {
        OutError = TEXT("Could not allocate exact V8 portico material nodes.");
        return nullptr;
    }
    Base->Constant = Spec.Color;
    Rough->R = Spec.Roughness;
    Metal->R = Spec.Metallic;
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = false;
    Material->SetShadingModel(MSM_DefaultLit);
    EditorOnly->BaseColor.Connect(0, Base);
    EditorOnly->Roughness.Connect(0, Rough);
    EditorOnly->Metallic.Connect(0, Metal);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    OutError.Reset();
    return Material;
}

FString VegetationContractFilename()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV4/explore_v4_vegetation.contract.json"));
}

FString VegetationSourceRoot()
{
    return ProjectSourcePath(TEXT("IstanaPublicViewExploreV4"));
}

bool FindFrozenVegetationFile(
    const TSharedPtr<FJsonObject>& Contract,
    const FString& RelativePath,
    int64& OutBytes,
    FString& OutSha256)
{
    OutBytes = -1;
    OutSha256.Reset();
    if (!Contract ||
        !Contract->HasTypedField<EJson::Object>(TEXT("sourceFreeze")))
    {
        return false;
    }
    const TSharedPtr<FJsonObject> Freeze =
        Contract->GetObjectField(TEXT("sourceFreeze"));
    if (!Freeze ||
        !Freeze->HasTypedField<EJson::Array>(TEXT("packagedFiles")))
    {
        return false;
    }
    int32 Matches = 0;
    for (const TSharedPtr<FJsonValue>& Value :
         Freeze->GetArrayField(TEXT("packagedFiles")))
    {
        const TSharedPtr<FJsonObject> Row = Value ? Value->AsObject() : nullptr;
        if (Row && Row->GetStringField(TEXT("path")) == RelativePath)
        {
            OutBytes = static_cast<int64>(Row->GetNumberField(TEXT("bytes")));
            OutSha256 = Row->GetStringField(TEXT("sha256"));
            ++Matches;
        }
    }
    return Matches == 1 && OutBytes > 0 && OutSha256.Len() == 64;
}

bool ValidateFrozenVegetationContract(
    TSharedPtr<FJsonObject>& OutContract,
    FString& OutError)
{
    const FString ContractFilename = VegetationContractFilename();
    const FString ManifestFilename = ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV4/Sources/Vegetation/manifest.json"));
    if (!ValidateExactFile(
            ContractFilename,
            VegetationContractBytes,
            VegetationContractSha,
            OutError) ||
        !ValidateExactFile(
            ManifestFilename,
            329362,
            VegetationManifestSha,
            OutError) ||
        !LoadJsonObject(ContractFilename, OutContract))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The frozen V4 vegetation contract is unreadable.");
        }
        return false;
    }

    const TSharedPtr<FJsonObject> Scope =
        OutContract->GetObjectField(TEXT("scope"));
    const TSharedPtr<FJsonObject> Claims =
        OutContract->GetObjectField(TEXT("claimBoundary"));
    const TSharedPtr<FJsonObject> Freeze =
        OutContract->GetObjectField(TEXT("sourceFreeze"));
    const TSharedPtr<FJsonObject> RuntimePolicy =
        OutContract->GetObjectField(TEXT("runtimeSelectionPolicy"));
    const TSharedPtr<FJsonObject> Manifest =
        Freeze->GetObjectField(TEXT("referenceCacheManifest"));
    const TArray<TSharedPtr<FJsonValue>>& ProtectedReferenceExceptions =
        RuntimePolicy->GetArrayField(
            TEXT("protectedExactRuntimeReferenceExceptions"));
    const TSharedPtr<FJsonObject> HighForkReference =
        ProtectedReferenceExceptions.Num() == 2 &&
            ProtectedReferenceExceptions[0]
        ? ProtectedReferenceExceptions[0]->AsObject()
        : nullptr;
    const TSharedPtr<FJsonObject> ColumnarReference =
        ProtectedReferenceExceptions.Num() == 2 &&
            ProtectedReferenceExceptions[1]
        ? ProtectedReferenceExceptions[1]->AsObject()
        : nullptr;
    if (OutContract->GetStringField(TEXT("schema")) !=
            TEXT("triad.istana_explore_v4_vegetation_contract.v2") ||
        Scope->GetStringField(TEXT("sourceMap")) != SourceMapPackage ||
        Scope->GetStringField(TEXT("targetMap")) != DestinationMapPackage ||
        Scope->GetStringField(TEXT("targetContentNamespace")) != AssetRoot ||
        Scope->GetStringField(TEXT("stagingNamespace")) !=
            StagingVegetationAssetPath ||
        Scope->GetStringField(TEXT("runtimeVegetationNamespace")) !=
            VegetationAssetPath ||
        Claims->GetBoolField(TEXT("oneToOneOneKilometerClaimed")) ||
        Claims->GetBoolField(TEXT("surveyAccuracyClaimed")) ||
        Claims->GetBoolField(TEXT("exactTreeInventoryClaimed")) ||
        Claims->GetBoolField(TEXT("speciesIdentityClaimed")) ||
        Claims->GetBoolField(TEXT("collisionOrSensorAuthorityClaimed")) ||
        Claims->GetBoolField(TEXT("googleOrOneMapImageryUsed")) ||
        Manifest->GetStringField(TEXT("path")) !=
            TEXT("Sources/Vegetation/manifest.json") ||
        static_cast<int64>(Manifest->GetNumberField(TEXT("bytes"))) != 329362 ||
        !Manifest->GetStringField(TEXT("sha256")).Equals(
            VegetationManifestSha, ESearchCase::IgnoreCase) ||
        !RuntimePolicy->GetBoolField(
            TEXT("rawSourceAssetsAreEditorOnlyStaging")) ||
        RuntimePolicy->GetBoolField(
            TEXT("rawLod0MayBeSelectedByMapOrRuntimeActor")) ||
        RuntimePolicy->GetIntegerField(
            TEXT("allRuntimeMeshDerivativesRequireMinLODAtLeast")) != 1 ||
        RuntimePolicy->GetStringField(
            TEXT("runtimeReferencesMustResolveOnlyUnder")) !=
            VegetationAssetPath ||
        ProtectedReferenceExceptions.Num() != 2 ||
        !HighForkReference || !ColumnarReference ||
        HighForkReference->GetStringField(TEXT("selectionId")) !=
            TEXT("high_fork_tree") ||
        HighForkReference->GetStringField(TEXT("objectPath")) !=
            ProtectedHighForkSourceObjectPath ||
        ColumnarReference->GetStringField(TEXT("selectionId")) !=
            TEXT("columnar_tree") ||
        ColumnarReference->GetStringField(TEXT("objectPath")) !=
            ProtectedColumnarSourceObjectPath ||
        !RuntimePolicy->GetBoolField(
            TEXT("protectedReferencesAreReadOnly")) ||
        !RuntimePolicy->GetBoolField(
            TEXT("protectedReferenceMaterialOverridesAreComponentLocal")) ||
        RuntimePolicy->GetBoolField(
            TEXT("protectedReferencePackagesMayBeSavedByV4Workflow")))
    {
        OutError = TEXT("The frozen V4 vegetation scope/claim/runtime boundary changed.");
        return false;
    }

    const TSharedPtr<FJsonObject> MainPlacement =
        RuntimePolicy->GetObjectField(TEXT("mainTreePlacement"));
    const TSharedPtr<FJsonObject> SourceUnion =
        MainPlacement->GetObjectField(TEXT("sourceUnion"));
    const TSharedPtr<FJsonObject> Columnar =
        MainPlacement->GetObjectField(TEXT("columnarInheritance"));
    const TSharedPtr<FJsonObject> NonColumnar =
        MainPlacement->GetObjectField(TEXT("reclassifiedNonColumnar"));
    const TSharedPtr<FJsonObject> NonColumnarRules =
        NonColumnar->GetObjectField(TEXT("rules"));
    const TSharedPtr<FJsonObject> UmbrellaRule =
        NonColumnarRules->GetObjectField(TEXT("umbrella_tree"));
    const TSharedPtr<FJsonObject> DomeRule =
        NonColumnarRules->GetObjectField(TEXT("dense_dome_tree"));
    const TSharedPtr<FJsonObject> HighForkRule =
        NonColumnarRules->GetObjectField(TEXT("high_fork_tree"));
    const TSharedPtr<FJsonObject> PalmPlacement =
        MainPlacement->GetObjectField(TEXT("palm"));
    const TSharedPtr<FJsonObject> HeritagePlacement =
        RuntimePolicy->GetObjectField(TEXT("heritageTreePlacement"));
    const TSharedPtr<FJsonObject> EvidenceCounts =
        HeritagePlacement->GetObjectField(TEXT("formEvidenceCounts"));
    const TArray<TSharedPtr<FJsonValue>>& UnionCounts =
        SourceUnion->GetArrayField(TEXT("orderedV2ComponentCounts"));
    const TArray<TSharedPtr<FJsonValue>>& UnionNames =
        SourceUnion->GetArrayField(TEXT("orderedV2ComponentNames"));
    const TArray<TSharedPtr<FJsonValue>>& ColumnarRange =
        Columnar->GetArrayField(TEXT("sourceIndexRangeInclusive"));
    const TArray<TSharedPtr<FJsonValue>>& AllowedNonColumnar =
        NonColumnar->GetArrayField(TEXT("allowedSelectionIds"));
    if (SourceUnion->GetIntegerField(TEXT("count")) != 720 ||
        UnionCounts.Num() != 4 ||
        static_cast<int32>(UnionCounts[0]->AsNumber()) != 272 ||
        static_cast<int32>(UnionCounts[1]->AsNumber()) != 232 ||
        static_cast<int32>(UnionCounts[2]->AsNumber()) != 182 ||
        static_cast<int32>(UnionCounts[3]->AsNumber()) != 34 ||
        UnionNames.Num() != 4 ||
        UnionNames[0]->AsString() != TEXT("UmbrellaBroadleafInstances") ||
        UnionNames[1]->AsString() != TEXT("ColumnarBroadleafInstances") ||
        UnionNames[2]->AsString() != TEXT("DomeBroadleafInstances") ||
        UnionNames[3]->AsString() != TEXT("PalmInstances") ||
        !SourceUnion->GetBoolField(TEXT("preserveTranslationAndRotation")) ||
        Columnar->GetStringField(TEXT("selectionId")) !=
            TEXT("columnar_tree") ||
        ColumnarRange.Num() != 2 ||
        static_cast<int32>(ColumnarRange[0]->AsNumber()) != 272 ||
        static_cast<int32>(ColumnarRange[1]->AsNumber()) != 503 ||
        Columnar->GetIntegerField(TEXT("instanceCount")) != 232 ||
        Columnar->GetStringField(TEXT("transformMode")) !=
            TEXT("EXACT_INHERITED_V2_WORLD_TRANSFORM") ||
        Columnar->GetBoolField(TEXT("rasterBandHeightFitClaimed")) ||
        Columnar->GetBoolField(TEXT("crownReshapeClaimed")) ||
        NonColumnar->GetIntegerField(TEXT("sourceInstanceCount")) != 488 ||
        AllowedNonColumnar.Num() != 3 ||
        AllowedNonColumnar[0]->AsString() != TEXT("umbrella_tree") ||
        AllowedNonColumnar[1]->AsString() != TEXT("dense_dome_tree") ||
        AllowedNonColumnar[2]->AsString() != TEXT("high_fork_tree") ||
        NonColumnar->GetStringField(TEXT("scaleMode")) !=
            TEXT("UNIFORM_HEIGHT_ONLY") ||
        NonColumnar->GetBoolField(TEXT("nonUniformScaleAllowed")) ||
        !FMath::IsNearlyEqual(
            UmbrellaRule->GetNumberField(TEXT("minimumHeightMeters")),
            20.0, 0.000000001) ||
        !FMath::IsNearlyEqual(
            UmbrellaRule->GetNumberField(TEXT("maximumHeightMeters")),
            24.28810830713951, 0.000000001) ||
        !FMath::IsNearlyEqual(
            DomeRule->GetNumberField(TEXT("heightMeters")),
            36.5, 0.000000001) ||
        !FMath::IsNearlyEqual(
            HighForkRule->GetNumberField(TEXT("heightMeters")),
            28.3, 0.000000001) ||
        PalmPlacement->GetStringField(TEXT("selectionId")) !=
            TEXT("palm_accent") ||
        !PalmPlacement->GetBoolField(TEXT("runtimeAssetMayRemainImported")) ||
        PalmPlacement->GetIntegerField(TEXT("freeRoamInstanceCount")) != 0 ||
        PalmPlacement->GetBoolField(TEXT("closeHeroUseAllowed")) ||
        MainPlacement->GetBoolField(TEXT("individualTreeOrSpeciesClaimed")) ||
        HeritagePlacement->GetIntegerField(TEXT("anchorCount")) != 9 ||
        HeritagePlacement->GetStringField(TEXT("anchorSource")) !=
            TEXT("explore_v4_geospatial.contract.json heritageTreeAnchors") ||
        HeritagePlacement->GetStringField(TEXT("visualMeshPolicy")) !=
            TEXT("CLOSEST_AVAILABLE_BROAD_SILHOUETTE_PROXY") ||
        HeritagePlacement->GetStringField(TEXT("visualScaleMode")) !=
            TEXT("UNIFORM_PUBLISHED_HEIGHT_ONLY") ||
        HeritagePlacement->GetBoolField(
            TEXT("publishedHeightMayBeClampedToMainTreeRange")) ||
        HeritagePlacement->GetBoolField(
            TEXT("publishedGirthMayAffectVisualMeshTransform")) ||
        HeritagePlacement->GetStringField(TEXT("publishedGirthUse")) !=
            TEXT("SEPARATE_QUERY_ONLY_PAWN_BLOCKER_DIAMETER_EQUALS_GIRTH_DIVIDED_BY_PI") ||
        HeritagePlacement->GetBoolField(TEXT("speciesMeshClaimed")) ||
        HeritagePlacement->GetBoolField(TEXT("crownTopologyClaimed")) ||
        HeritagePlacement->GetBoolField(TEXT("meshNativeGirthClaimed")) ||
        EvidenceCounts->GetIntegerField(TEXT("publishedQualitativeHint")) != 7 ||
        EvidenceCounts->GetIntegerField(TEXT("explicitGenericFallback")) != 2)
    {
        OutError = TEXT("The frozen V4 main/Heritage uniform-height placement policy changed.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>& Files =
        Freeze->GetArrayField(TEXT("packagedFiles"));
    const int32 ExpectedCount =
        Freeze->GetIntegerField(TEXT("packageFileCount"));
    const int64 ExpectedBytes = static_cast<int64>(
        Freeze->GetNumberField(TEXT("packageBytes")));
    if (Files.Num() != ExpectedCount || ExpectedCount <= 0 ||
        ExpectedBytes <= 0)
    {
        OutError = TEXT("Frozen V4 vegetation file-count/byte census changed.");
        return false;
    }
    const FString Root = VegetationSourceRoot();
    int64 ActualByteCensus = 0;
    TSet<FString> SeenRelativePaths;
    for (const TSharedPtr<FJsonValue>& Value : Files)
    {
        const TSharedPtr<FJsonObject> Row = Value ? Value->AsObject() : nullptr;
        const FString Relative = Row
            ? Row->GetStringField(TEXT("path"))
            : FString();
        const int64 Bytes = Row
            ? static_cast<int64>(Row->GetNumberField(TEXT("bytes")))
            : -1;
        const FString Sha = Row
            ? Row->GetStringField(TEXT("sha256"))
            : FString();
        FString NormalizedRelative = Relative;
        FPaths::NormalizeFilename(NormalizedRelative);
        const FString Full = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(Root, NormalizedRelative));
        if (!Row || Relative.IsEmpty() || FPaths::IsRelative(Relative) == false ||
            NormalizedRelative.StartsWith(TEXT("../")) ||
            SeenRelativePaths.Contains(NormalizedRelative) ||
            !FPaths::IsUnderDirectory(Full, Root) ||
            !ValidateExactFile(Full, Bytes, Sha, OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A frozen V4 vegetation package row is unsafe or duplicated.");
            }
            return false;
        }
        SeenRelativePaths.Add(NormalizedRelative);
        ActualByteCensus += Bytes;
    }
    if (ActualByteCensus != ExpectedBytes)
    {
        OutError = TEXT("Frozen V4 vegetation package byte census changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

struct FTextureMapping
{
    const TCHAR* RelativePath;
    const TCHAR* AssetName;
    bool bSrgb;
    TextureCompressionSettings Compression;
    bool bFlipGreen;
    int32 Width;
    int32 Height;
};

const TArray<FTextureMapping>& RequiredTextureMappings()
{
    static const TArray<FTextureMapping> Mappings = {
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_diff_2k.jpg"), TEXT("T_IPV4_IslandTree02_Trunk_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_nor_gl_2k.exr"), TEXT("T_IPV4_IslandTree02_Trunk_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_rough_2k.exr"), TEXT("T_IPV4_IslandTree02_Trunk_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_branches_diff_2k.png"), TEXT("T_IPV4_IslandTree02_Branch_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_branches_nor_gl_2k.png"), TEXT("T_IPV4_IslandTree02_Branch_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_branches_rough_2k.png"), TEXT("T_IPV4_IslandTree02_Branch_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_leaves_diff_2k.png"), TEXT("T_IPV4_IslandTree02_Leaf_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_leaves_nor_gl_2k.png"), TEXT("T_IPV4_IslandTree02_Leaf_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_leaves_rough_2k.png"), TEXT("T_IPV4_IslandTree02_Leaf_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/textures/island_tree_02_leaves_alpha_2k.png"), TEXT("T_IPV4_IslandTree02_Leaf_Opacity"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_diff_2k.jpg"), TEXT("T_IPV4_TreeSmall02_Trunk_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_nor_gl_2k.exr"), TEXT("T_IPV4_TreeSmall02_Trunk_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_rough_2k.exr"), TEXT("T_IPV4_TreeSmall02_Trunk_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_branch_diff_2k.png"), TEXT("T_IPV4_TreeSmall02_Branch_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_branch_nor_gl_2k.png"), TEXT("T_IPV4_TreeSmall02_Branch_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_branch_rough_2k.png"), TEXT("T_IPV4_TreeSmall02_Branch_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_leaves_diff_2k.png"), TEXT("T_IPV4_TreeSmall02_Leaf_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_leaves_nor_gl_2k.png"), TEXT("T_IPV4_TreeSmall02_Leaf_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_leaves_rough_2k.png"), TEXT("T_IPV4_TreeSmall02_Leaf_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/textures/tree_small_02_leaves_alpha_2k.png"), TEXT("T_IPV4_TreeSmall02_Leaf_Opacity"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/poly-pizza/Palm_Tree_Quaternius/derived_obj_zup/Atlas_Pirate.png"), TEXT("T_IPV4_QuaterniusPalm_Atlas"), true, TC_Default, false, 1024, 1024},
        {TEXT("Sources/Vegetation/assets/polyhaven/shrub_04/textures/shrub_04_diff_2k.jpg"), TEXT("T_IPV4_Shrub04_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/shrub_04/textures/shrub_04_nor_gl_2k.exr"), TEXT("T_IPV4_Shrub04_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/shrub_04/textures/shrub_04_rough_2k.exr"), TEXT("T_IPV4_Shrub04_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/shrub_04/textures/shrub_04_alpha_2k.png"), TEXT("T_IPV4_Shrub04_Opacity"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/periwinkle_plant/textures/periwinkle_plant_diff_2k.jpg"), TEXT("T_IPV4_Periwinkle_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/periwinkle_plant/textures/periwinkle_plant_nor_gl_2k.exr"), TEXT("T_IPV4_Periwinkle_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/periwinkle_plant/textures/periwinkle_plant_rough_2k.exr"), TEXT("T_IPV4_Periwinkle_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/periwinkle_plant/textures/periwinkle_plant_opacity_2k.png"), TEXT("T_IPV4_Periwinkle_Opacity"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/calathea_orbifolia_01/textures/calathea_orbifolia_01_diff_2k.jpg"), TEXT("T_IPV4_Calathea_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/calathea_orbifolia_01/textures/calathea_orbifolia_01_nor_gl_2k.exr"), TEXT("T_IPV4_Calathea_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/calathea_orbifolia_01/textures/calathea_orbifolia_01_rough_2k.exr"), TEXT("T_IPV4_Calathea_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/calathea_orbifolia_01/textures/calathea_orbifolia_01_alpha_2k.png"), TEXT("T_IPV4_Calathea_Opacity"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/grass_medium_01/textures/grass_medium_01_diff_2k.jpg"), TEXT("T_IPV4_GrassMedium_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/grass_medium_01/textures/grass_medium_01_nor_gl_2k.exr"), TEXT("T_IPV4_GrassMedium_NormalDX"), false, TC_Normalmap, true, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/grass_medium_01/textures/grass_medium_01_rough_2k.exr"), TEXT("T_IPV4_GrassMedium_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/polyhaven/grass_medium_01/textures/grass_medium_01_alpha_2k.png"), TEXT("T_IPV4_GrassMedium_Opacity"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/ambientcg/Grass001/extracted/Grass001_2K-JPG_Color.jpg"), TEXT("T_IPV4_Grass001_BaseColor"), true, TC_Default, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/ambientcg/Grass001/extracted/Grass001_2K-JPG_NormalDX.jpg"), TEXT("T_IPV4_Grass001_NormalDX"), false, TC_Normalmap, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/ambientcg/Grass001/extracted/Grass001_2K-JPG_Roughness.jpg"), TEXT("T_IPV4_Grass001_Roughness"), false, TC_Masks, false, 2048, 2048},
        {TEXT("Sources/Vegetation/assets/ambientcg/Grass001/extracted/Grass001_2K-JPG_AmbientOcclusion.jpg"), TEXT("T_IPV4_Grass001_AmbientOcclusion"), false, TC_Masks, false, 2048, 2048}};
    return Mappings;
}

bool BuildRequiredTextureSpecs(
    const TSharedPtr<FJsonObject>& Contract,
    TArray<FTextureSourceSpec>& OutSpecs,
    FString& OutError)
{
    OutSpecs.Reset();
    TSet<FString> Names;
    for (const FTextureMapping& Mapping : RequiredTextureMappings())
    {
        int64 Bytes = -1;
        FString Sha;
        if (Names.Contains(Mapping.AssetName) ||
            !FindFrozenVegetationFile(
                Contract, Mapping.RelativePath, Bytes, Sha))
        {
            OutError = TEXT("A required V4 material texture is absent from the frozen source roster: ") +
                FString(Mapping.RelativePath);
            return false;
        }
        FTextureSourceSpec Spec;
        Spec.RelativePath = Mapping.RelativePath;
        Spec.SourcePath = FPaths::Combine(
            VegetationSourceRoot(), Mapping.RelativePath);
        Spec.AssetName = Mapping.AssetName;
        Spec.SourceBytes = Bytes;
        Spec.SourceSha256 = Sha;
        Spec.ExpectedWidth = Mapping.Width;
        Spec.ExpectedHeight = Mapping.Height;
        Spec.bSrgb = Mapping.bSrgb;
        Spec.Compression = Mapping.Compression;
        Spec.Group = TEXTUREGROUP_World;
        Spec.bFlipGreen = Mapping.bFlipGreen;
        Names.Add(Spec.AssetName);
        OutSpecs.Add(MoveTemp(Spec));
    }
    if (OutSpecs.Num() != 41)
    {
        OutError = TEXT("The exact V4 texture roster must contain 41 assets.");
        return false;
    }
    OutError.Reset();
    return true;
}

UTexture2D* ResolveImportedTexture(
    UAssetImportTask* Task,
    const FString& ExactObjectPath)
{
    if (Task)
    {
        for (UObject* Object : Task->GetObjects())
        {
            UTexture2D* Texture = Cast<UTexture2D>(Object);
            if (Texture && Texture->GetPathName() == ExactObjectPath)
            {
                return Texture;
            }
        }
    }
    return LoadExact<UTexture2D>(ExactObjectPath);
}

bool ValidateImportedTexture(
    UTexture2D* Texture,
    const FTextureSourceSpec& Spec,
    FString& OutError)
{
    const FString ExactObjectPath = TextureObjectPath(Spec.AssetName);
    const TArray<FString> Sources = Texture && Texture->AssetImportData
        ? Texture->AssetImportData->ExtractFilenames()
        : TArray<FString>();
    FString ActualSource = Sources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(Sources[0])
        : FString();
    FString ExpectedSource = FPaths::ConvertRelativePathToFull(Spec.SourcePath);
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(ExpectedSource);
    if (!Texture || Texture->GetPathName() != ExactObjectPath ||
        Texture->Source.GetSizeX() != Spec.ExpectedWidth ||
        Texture->Source.GetSizeY() != Spec.ExpectedHeight ||
        Texture->SRGB != Spec.bSrgb ||
        Texture->CompressionSettings != Spec.Compression ||
        Texture->LODGroup != Spec.Group ||
        Texture->MipGenSettings != TMGS_FromTextureGroup ||
        Texture->AddressX != TA_Wrap || Texture->AddressY != TA_Wrap ||
        Texture->bFlipGreenChannel != Spec.bFlipGreen ||
        Texture->VirtualTextureStreaming || Texture->NeverStream ||
        Sources.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        !ValidateExactFile(
            ExpectedSource,
            Spec.SourceBytes,
            Spec.SourceSha256,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A V4 texture/source/import-policy binding changed: ") +
                Spec.AssetName;
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ImportRequiredTextures(
    IAssetTools& AssetTools,
    const TArray<FTextureSourceSpec>& Specs,
    TMap<FString, UTexture2D*>& OutTextures,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    OutTextures.Reset();
    const int32 InitialSaveCount = OutAssetsToSave.Num();
    TSet<FString> ImportedNames;
    if (Specs.Num() != 41)
    {
        OutError = TEXT("V4 import requires exactly 41 frozen texture inputs.");
        return false;
    }
    for (const FTextureSourceSpec& Spec : Specs)
    {
        UTextureFactory* Factory = NewObject<UTextureFactory>();
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        if (!Factory || !Task || ImportedNames.Contains(Spec.AssetName))
        {
            OutError = TEXT("Could not allocate the exact V4 texture import roster.");
            return false;
        }
        Factory->bCreateMaterial = false;
        Factory->NoAlpha = false;
        Factory->bDeferCompression = false;
        Factory->CompressionSettings = Spec.Compression;
        Factory->LODGroup = Spec.Group;
        Factory->MipGenSettings = TMGS_FromTextureGroup;
        Factory->bFlipNormalMapGreenChannel = Spec.bFlipGreen;
        Factory->ColorSpaceMode = Spec.bSrgb
            ? ETextureSourceColorSpace::SRGB
            : ETextureSourceColorSpace::Linear;
        Task->Filename = Spec.SourcePath;
        Task->DestinationPath = TextureAssetPath;
        Task->DestinationName = Spec.AssetName;
        Task->bReplaceExisting = false;
        Task->bReplaceExistingSettings = false;
        Task->bAutomated = true;
        Task->bSave = false;
        Task->bAsync = false;
        Task->Factory = Factory;
        Task->Options = Factory;
        AssetTools.ImportAssetTasks({Task});
        UTexture2D* Texture = ResolveImportedTexture(
            Task,
            TextureObjectPath(Spec.AssetName));
        if (!Texture)
        {
            OutError = TEXT("Texture import produced no exact V4 object: ") +
                Spec.AssetName;
            return false;
        }
        Texture->Modify();
        Texture->SRGB = Spec.bSrgb;
        Texture->CompressionSettings = Spec.Compression;
        Texture->LODGroup = Spec.Group;
        Texture->MipGenSettings = TMGS_FromTextureGroup;
        Texture->AddressX = TA_Wrap;
        Texture->AddressY = TA_Wrap;
        Texture->Filter = TF_Default;
        Texture->bFlipGreenChannel = Spec.bFlipGreen;
        Texture->VirtualTextureStreaming = false;
        Texture->NeverStream = false;
        Texture->PostEditChange();
        FTextureCompilingManager::Get().FinishCompilation({Texture});
        if (FTextureCompilingManager::Get().IsCompilingTexture(Texture))
        {
            OutError = TEXT("A V4 texture remained compiling after its exact serial import barrier: ") +
                Spec.AssetName;
            return false;
        }
        Texture->MarkPackageDirty();
        if (!ValidateImportedTexture(Texture, Spec, OutError))
        {
            return false;
        }
        ImportedNames.Add(Spec.AssetName);
        OutTextures.Add(Spec.AssetName, Texture);
        OutAssetsToSave.Add(Texture);
        FMemory::Trim(true);
    }
    if (ImportedNames.Num() != 41 || OutTextures.Num() != 41 ||
        OutAssetsToSave.Num() != InitialSaveCount + 41)
    {
        OutError = TEXT("The exact serial V4 texture import roster did not finish at 41 assets.");
        return false;
    }
    OutError.Reset();
    return true;
}

int32 CountTriangles(const UStaticMesh* Mesh, int32 LodIndex)
{
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    return RenderData && RenderData->LODResources.IsValidIndex(LodIndex)
        ? RenderData->LODResources[LodIndex].GetNumTriangles()
        : INDEX_NONE;
}

TArray<int32> CountLodTrianglesByMaterialSlot(
    const UStaticMesh* Mesh,
    int32 LodIndex)
{
    TArray<int32> Counts;
    if (!Mesh)
    {
        return Counts;
    }
    Counts.Init(0, Mesh->GetStaticMaterials().Num());
    const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
    if (!RenderData || !RenderData->LODResources.IsValidIndex(LodIndex))
    {
        Counts.Reset();
        return Counts;
    }
    for (const FStaticMeshSection& Section :
         RenderData->LODResources[LodIndex].Sections)
    {
        if (!Counts.IsValidIndex(Section.MaterialIndex))
        {
            Counts.Reset();
            return Counts;
        }
        Counts[Section.MaterialIndex] += Section.NumTriangles;
    }
    return Counts;
}

TArray<FName> ImportedSlotOrder(const UStaticMesh* Mesh)
{
    TArray<FName> Slots;
    if (!Mesh)
    {
        return Slots;
    }
    for (const FStaticMaterial& Material : Mesh->GetStaticMaterials())
    {
        if (Material.MaterialSlotName != Material.ImportedMaterialSlotName)
        {
            return {};
        }
        Slots.Add(Material.ImportedMaterialSlotName);
    }
    return Slots;
}

struct FLodCap
{
    int32 MaximumTriangles = 0;
    float ScreenSize = 0.0f;
};

struct FExternalMeshSpec
{
    FString SelectionId;
    FString RelativeSourcePath;
    int64 SourceBytes = 0;
    FString SourceSha256;
    FString SourceModelName;
    int32 SourceTriangles = 0;
    TArray<FName> SlotOrder;
    TArray<int32> SectionTrianglesBySlot;
    double ExpectedImportedZSpanCm = 0.0;
    FString StagingAssetName;
    FString RuntimeAssetName;
    TArray<FLodCap> RuntimeLodCaps;
    TArray<TArray<int32>> RuntimeTopologySectionTriangleCapsByLod;
    TArray<int32> ExpectedSourceComponentCountsBySlot;
    TArray<int32> ContractCensusSlotIndices;
    int32 TopologyTrunkSlotIndex = INDEX_NONE;
    bool bObjMetresZUp = false;
};

const TArray<FExternalMeshSpec>& ExternalMeshSpecs()
{
    static const TArray<FExternalMeshSpec> Specs = {
        {TEXT("dense_dome_tree"), TEXT("Sources/Vegetation/assets/polyhaven/tree_small_02/tree_small_02_2k.fbx"), 75487180, TEXT("14D6D0754B79A442254795663C4E5FFA1F679E54562F48B21AB0BBC78858724D"), TEXT("tree_small_02_LOD0"), 2062487, {TEXT("tree_small_02_branches"), TEXT("tree_small_02_leaves"), TEXT("tree_small_02_trunk")}, {94814, 1939380, 28293}, 455.674096569419, TEXT("SM_SRC_IPV4_TreeSmall02"), DomeMeshName, {{300000, 1.0f}, {200000, 0.60f}, {120000, 0.22f}, {60000, 0.055f}}, {{94814, 176890, 28293}, {80000, 91707, 28293}, {50000, 41707, 28293}, {15000, 16707, 28293}}, {751, 30250, 1}, {1}, 2, false},
        {TEXT("umbrella_tree"), TEXT("Sources/Vegetation/assets/polyhaven/island_tree_02/island_tree_02_2k.fbx"), 32258924, TEXT("B80EE7416B5CA9BCADD0EF21F105A42619B9A48C861681B58E8B59A368D673B9"), TEXT("island_tree_02_LOD0"), 1072213, {TEXT("island_tree_02"), TEXT("island_tree_02_leaves"), TEXT("island_tree_02_branches")}, {27298, 714744, 330171}, 340.660579688847, TEXT("SM_SRC_IPV4_IslandTree02"), UmbrellaMeshName, {{300000, 1.0f}, {200000, 0.60f}, {120000, 0.22f}, {60000, 0.055f}}, {{27298, 142512, 130190}, {27298, 100008, 72694}, {27298, 48000, 44702}, {27298, 12000, 20702}}, {1, 29781, 5541}, {1, 2}, 0, false},
        {TEXT("palm_accent"), TEXT("Sources/Vegetation/assets/poly-pizza/Palm_Tree_Quaternius/derived_obj_zup/Palm_Tree_Quaternius_ZUp.obj"), 420198, TEXT("F13C9C6F45C0637D1F81DC0728B507FF5B225F40B530722715DA33CDE8A32E7B"), TEXT("Environment_PalmTree_1"), 3208, {TEXT("Atlas")}, {3208}, 358.735837489434, TEXT("SM_SRC_IPV4_QuaterniusPalm_ZUp"), PalmMeshName, {{3208, 1.0f}, {800, 0.42f}, {8, 0.075f}}, {}, {}, {}, INDEX_NONE, true},
        {TEXT("shrub_04_a"), TEXT("Sources/Vegetation/assets/polyhaven/shrub_04/shrub_04_2k.fbx"), 852556, TEXT("BB07591834364E3DE8898C4DEE76B77CA5C45916D497F2E1FA146A5F9CE22D1B"), TEXT("shrub_04_a_LOD0"), 3726, {TEXT("shrub_04")}, {3726}, 19.5048979483545, TEXT("SM_SRC_IPV4_Shrub04_A"), ShrubMeshName, {{3726, 1.0f}, {1490, 0.42f}, {12, 0.075f}}, {}, {}, {}, INDEX_NONE, false},
        {TEXT("periwinkle_06_f"), TEXT("Sources/Vegetation/assets/polyhaven/periwinkle_plant/periwinkle_plant_2k.fbx"), 1170860, TEXT("C379BBB56256CFED3E29D84783C8762AD57724EFD69B3EFBC9D1D1D5639B2E7E"), TEXT("periwinkle_plant_06_LOD0"), 1478, {TEXT("periwinkle_plant")}, {1478}, 15.9729687395156, TEXT("SM_SRC_IPV4_Periwinkle06_F"), FlowerMeshName, {{1478, 1.0f}, {591, 0.42f}, {12, 0.075f}}, {}, {}, {}, INDEX_NONE, false},
        {TEXT("calathea_d"), TEXT("Sources/Vegetation/assets/polyhaven/calathea_orbifolia_01/calathea_orbifolia_01_2k.fbx"), 481804, TEXT("72D5A4A2B2DDD71128FC9CAFB323C536C653272B25FD7588B1C94530E581F99E"), TEXT("calathea_orbifolia_01_d"), 1802, {TEXT("calathea_orbifolia_01")}, {1802}, 13.0358794238418, TEXT("SM_SRC_IPV4_Calathea_D"), UnderstoreyMeshName, {{1802, 1.0f}, {720, 0.42f}, {12, 0.075f}}, {}, {}, {}, INDEX_NONE, false},
        {TEXT("grass_medium_small_a"), TEXT("Sources/Vegetation/assets/polyhaven/grass_medium_01/grass_medium_01_2k.fbx"), 1056796, TEXT("B3E83C69883501FB3AF03C6829DBF69089896ADD52D16D596376BADD54AD2419"), TEXT("grass_medium_01_small_a_LOD0"), 833, {TEXT("grass_medium_01")}, {833}, 14.0426675323397, TEXT("SM_SRC_IPV4_GrassMedium_SmallA"), GeometryGrassMeshName, {{833, 1.0f}, {208, 0.42f}, {8, 0.075f}}, {}, {}, {}, INDEX_NONE, false}};
    return Specs;
}

FString StagingMeshObjectPath(const FExternalMeshSpec& Spec)
{
    return ObjectPath(StagingVegetationAssetPath, Spec.StagingAssetName);
}

FString RuntimeMeshObjectPath(const FString& AssetName)
{
    return ObjectPath(MeshAssetPath, AssetName);
}

bool JsonIntRosterEquals(
    const TSharedPtr<FJsonObject>& Object,
    const TArray<int32>& Expected)
{
    if (!Object || Object->Values.Num() != Expected.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < Expected.Num(); ++Index)
    {
        const FString Key = LexToString(Index);
        if (!Object->HasTypedField<EJson::Number>(Key) ||
            Object->GetIntegerField(Key) != Expected[Index])
        {
            return false;
        }
    }
    return true;
}

bool HasExactTopologyReplayReadbackContract(
    const TSharedPtr<FJsonObject>& Object)
{
    return Object && Object->GetBoolField(TEXT("required")) &&
        Object->GetStringField(TEXT("triangleCount")) ==
            TEXT("REQUIRED_EXACT_RUNTIME_METADATA") &&
        Object->GetStringField(TEXT("sectionTriangleCountsBySlot")) ==
            TEXT("REQUIRED_EXACT_RUNTIME_METADATA") &&
        Object->GetStringField(TEXT("canonicalComponentRoster")) ==
            TEXT("REQUIRED_EXACT_RUNTIME_METADATA") &&
        Object->GetStringField(TEXT("componentRosterSha256")) ==
            TEXT("REQUIRED_EXACT_RUNTIME_METADATA");
}

bool ValidateRuntimePreReductionContract(
    const TSharedPtr<FJsonObject>& Row,
    const FExternalMeshSpec& Spec,
    FString& OutError)
{
    const TSharedPtr<FJsonObject> Prebase =
        Row ? Row->GetObjectField(TEXT("runtimePreReductionBase")) : nullptr;
    const bool bHeavy =
        Spec.RuntimeTopologySectionTriangleCapsByLod.Num() ==
            Spec.RuntimeLodCaps.Num();
    const TSharedPtr<FJsonObject> ComponentPolicy = Prebase
        ? Prebase->GetObjectField(TEXT("componentPolicy"))
        : nullptr;
    const TSharedPtr<FJsonObject> Extrema = Prebase
        ? Prebase->GetObjectField(TEXT("globalExtrema"))
        : nullptr;
    const TArray<TSharedPtr<FJsonValue>>& Directions = Extrema
        ? Extrema->GetArrayField(TEXT("directions"))
        : TArray<TSharedPtr<FJsonValue>>();
    const TArray<FString> ExpectedDirections = {
        TEXT("-X"), TEXT("+X"), TEXT("-Y"),
        TEXT("+Y"), TEXT("-Z"), TEXT("+Z")};
    if (!Prebase || !ComponentPolicy || !Extrema ||
        !Prebase->GetBoolField(TEXT("noNewCutBoundaries")) ||
        Prebase->GetStringField(TEXT("digestAlgorithm")) != TEXT("SHA-256") ||
        Prebase->GetStringField(TEXT("digestEncoding")) !=
            TEXT("UPPERCASE_HEX_64") ||
        !Extrema->GetBoolField(TEXT("forced")) ||
        Extrema->GetStringField(TEXT("coordinateFrame")) !=
            TEXT("ABSOLUTE_IMPORTED_VERTEX_XYZ") ||
        Directions.Num() != ExpectedDirections.Num())
    {
        OutError = TEXT("A selected V4 mesh topology/common digest contract changed: ") +
            Spec.SelectionId;
        return false;
    }
    for (int32 Index = 0; Index < ExpectedDirections.Num(); ++Index)
    {
        if (Directions[Index]->AsString() != ExpectedDirections[Index])
        {
            OutError = TEXT("A selected V4 mesh global-extrema direction roster changed: ") +
                Spec.SelectionId;
            return false;
        }
    }
    if (!bHeavy)
    {
        const TSharedPtr<FJsonObject> Census =
            Prebase->GetObjectField(TEXT("rawComponentCensus"));
        if (Prebase->GetStringField(TEXT("algorithmVersion")) !=
                TEXT("EXACT_SOURCE_NO_PRE_REDUCTION_V1") ||
            Prebase->GetBoolField(TEXT("applied")) ||
            Prebase->GetIntegerField(TEXT("prebaseTriangleCap")) !=
                Spec.SourceTriangles ||
            ComponentPolicy->GetStringField(TEXT("partition")) !=
                TEXT("NOT_APPLICABLE_EXACT_SOURCE") ||
            ComponentPolicy->GetStringField(TEXT("retentionUnit")) !=
                TEXT("ENTIRE_EXACT_SOURCE_MESHDESCRIPTION") ||
            ComponentPolicy->GetStringField(TEXT("selectionOrder")) !=
                TEXT("NOT_APPLICABLE_EXACT_SOURCE") ||
            ComponentPolicy->GetBoolField(TEXT("partialComponentRetentionAllowed")) ||
            Extrema->GetStringField(TEXT("retentionUnit")) !=
                TEXT("ENTIRE_EXACT_SOURCE_MESHDESCRIPTION") ||
            Extrema->GetStringField(TEXT("tieBreak")) !=
                TEXT("NOT_APPLICABLE_EXACT_SOURCE") ||
            !Census || Census->GetStringField(TEXT("coverage")) !=
                TEXT("NOT_REQUIRED_EXACT_SOURCE_NO_PRE_REDUCTION") ||
            !Census->GetArrayField(TEXT("byMaterialSlot")).IsEmpty() ||
            !JsonIntRosterEquals(
                Prebase->GetObjectField(TEXT("sectionTriangleTargetsMaximum")),
                Spec.SectionTrianglesBySlot) ||
            !JsonIntRosterEquals(
                Prebase->GetObjectField(
                    TEXT("expectedRawReplayRetainedSectionTrianglesBySlot")),
                Spec.SectionTrianglesBySlot) ||
            Prebase->GetIntegerField(TEXT("expectedRawReplayRetainedTriangles")) !=
                Spec.SourceTriangles ||
            Prebase->GetStringField(TEXT("canonicalRetainedComponentRoster")) !=
                TEXT("ENTIRE_EXACT_SOURCE_IN_IMPORTED_MESHDESCRIPTION_ORDER") ||
            Prebase->HasField(TEXT("runtimeSourceLods")))
        {
            OutError = TEXT("A selected V4 exact-source no-pre-reduction contract changed: ") +
                Spec.SelectionId;
            return false;
        }
        OutError.Reset();
        return true;
    }

    const TSharedPtr<FJsonObject> RuntimeLods =
        Prebase->GetObjectField(TEXT("runtimeSourceLods"));
    const TArray<TSharedPtr<FJsonValue>>& LodRows =
        RuntimeLods->GetArrayField(TEXT("lods"));
    const TSharedPtr<FJsonObject> Census =
        Prebase->GetObjectField(TEXT("rawComponentCensus"));
    const TArray<TSharedPtr<FJsonValue>>& CensusRows =
        Census->GetArrayField(TEXT("byMaterialSlot"));
    if (Prebase->GetStringField(TEXT("algorithmVersion")) !=
            TEXT("TOPOLOGY_PRESERVING_MATERIAL_EDGE_COMPONENTS_V1") ||
        !Prebase->GetBoolField(TEXT("applied")) ||
        Prebase->GetIntegerField(TEXT("prebaseTriangleCap")) !=
            Spec.RuntimeLodCaps[0].MaximumTriangles ||
        ComponentPolicy->GetStringField(TEXT("partition")) !=
            TEXT("MATERIAL_SLOT_THEN_SHARED_MESHDESCRIPTION_EDGE_CONNECTIVITY") ||
        ComponentPolicy->GetStringField(TEXT("retentionUnit")) !=
            TEXT("WHOLE_EDGE_CONNECTED_COMPONENT") ||
        ComponentPolicy->GetStringField(TEXT("selectionOrder")) !=
            TEXT("DETERMINISTIC_RAW_COMPONENT_ROSTER_ORDER_V1") ||
        !ComponentPolicy->GetBoolField(TEXT("fullTrunkComponentRequired")) ||
        !ComponentPolicy->GetBoolField(TEXT("wholeBranchAndLeafComponentsOnly")) ||
        ComponentPolicy->GetBoolField(TEXT("partialComponentRetentionAllowed")) ||
        Extrema->GetStringField(TEXT("retentionUnit")) !=
            TEXT("ENTIRE_OWNING_EDGE_CONNECTED_COMPONENT") ||
        Extrema->GetStringField(TEXT("tieBreak")) !=
            TEXT("LOWEST_MESHDESCRIPTION_VERTEX_ID_THEN_LOWEST_COMPONENT_ROSTER_INDEX") ||
        !JsonIntRosterEquals(
            Prebase->GetObjectField(TEXT("sectionTriangleTargetsMaximum")),
            Spec.RuntimeTopologySectionTriangleCapsByLod[0]) ||
        !HasExactTopologyReplayReadbackContract(
            Prebase->GetObjectField(TEXT("retainedReplayReadback"))) ||
        Prebase->GetStringField(TEXT("canonicalRetainedComponentRoster")) !=
            TEXT("ASCENDING_MATERIAL_SLOT_THEN_ASCENDING_MINIMUM_RAW_POLYGON_ID_WITH_SLOT_MIN_POLYGON_POLYGON_COUNT_TRIANGLE_COUNT") ||
        !RuntimeLods || RuntimeLods->GetStringField(TEXT("generationPolicy")) !=
            TEXT("CUSTOM_TOPOLOGY_PRESERVING_SELF_BASED_SOURCE_LODS_V1") ||
        RuntimeLods->GetIntegerField(TEXT("sourceLodCount")) !=
            Spec.RuntimeLodCaps.Num() ||
        !RuntimeLods->GetBoolField(TEXT("explicitCustomMeshDescriptionRequiredEveryLod")) ||
        RuntimeLods->GetStringField(TEXT("sourceModelBaseLodPolicy")) !=
            TEXT("EACH_SOURCE_MODEL_BASE_LOD_EQUALS_ITS_OWN_LOD_INDEX") ||
        RuntimeLods->GetBoolField(TEXT("isReductionActiveRequired")) ||
        RuntimeLods->GetBoolField(TEXT("automaticQuadricReductionAllowed")) ||
        RuntimeLods->GetStringField(TEXT("componentSelectionPolicy")) !=
            TEXT("NESTED_WHOLE_PER_MATERIAL_SHARED_EDGE_COMPONENT_SUBSETS") ||
        RuntimeLods->GetStringField(TEXT("nestingOrder")) !=
            TEXT("LOD3_SUBSET_OF_LOD2_SUBSET_OF_LOD1_SUBSET_OF_LOD0_RETAINED_BASE") ||
        RuntimeLods->GetIntegerField(TEXT("trunkMaterialSlotIndex")) !=
            Spec.TopologyTrunkSlotIndex ||
        !RuntimeLods->GetBoolField(TEXT("fullTrunkComponentRequiredEveryLod")) ||
        !RuntimeLods->GetBoolField(TEXT("globalXYZExtremaComponentsRequiredEveryLod")) ||
        !RuntimeLods->GetBoolField(TEXT("noNewCutBoundariesEveryLod")) ||
        LodRows.Num() != Spec.RuntimeLodCaps.Num() || !Census ||
        Census->GetStringField(TEXT("coverage")) !=
            TEXT("SUPPORTED_REPLAY_CENSUS_ONLY_NOT_A_COMPLETE_MESH_CENSUS") ||
        CensusRows.Num() != Spec.ContractCensusSlotIndices.Num())
    {
        OutError = TEXT("A selected V4 topology-preserving source-LOD contract changed: ") +
            Spec.SelectionId;
        return false;
    }
    for (int32 Index = 0; Index < CensusRows.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> CensusRow = CensusRows[Index]->AsObject();
        const int32 SlotIndex = Spec.ContractCensusSlotIndices[Index];
        if (!CensusRow || !Spec.SlotOrder.IsValidIndex(SlotIndex) ||
            !Spec.ExpectedSourceComponentCountsBySlot.IsValidIndex(SlotIndex) ||
            CensusRow->GetIntegerField(TEXT("slotIndex")) != SlotIndex ||
            CensusRow->GetStringField(TEXT("slotName")) !=
                Spec.SlotOrder[SlotIndex].ToString() ||
            CensusRow->GetIntegerField(TEXT("componentCount")) !=
                Spec.ExpectedSourceComponentCountsBySlot[SlotIndex])
        {
            OutError = TEXT("A selected V4 supported raw-component census changed: ") +
                Spec.SelectionId;
            return false;
        }
    }
    for (int32 Lod = 0; Lod < LodRows.Num(); ++Lod)
    {
        const TSharedPtr<FJsonObject> LodRow = LodRows[Lod]->AsObject();
        const FString ExpectedParent = Lod == 0
            ? TEXT("RETAINED_PREBASE")
            : FString::Printf(TEXT("LOD%d"), Lod - 1);
        if (!LodRow ||
            LodRow->GetStringField(TEXT("lod")) !=
                FString::Printf(TEXT("LOD%d"), Lod) ||
            LodRow->GetIntegerField(TEXT("lodIndex")) != Lod ||
            LodRow->GetIntegerField(TEXT("triangleCap")) !=
                Spec.RuntimeLodCaps[Lod].MaximumTriangles ||
            !JsonIntRosterEquals(
                LodRow->GetObjectField(TEXT("sectionTriangleCapsMaximum")),
                Spec.RuntimeTopologySectionTriangleCapsByLod[Lod]) ||
            LodRow->GetStringField(TEXT("componentSelectionSubsetOf")) !=
                ExpectedParent ||
            !LodRow->GetBoolField(TEXT("fullTrunkComponentRequired")) ||
            LodRow->GetIntegerField(TEXT("fullTrunkSourceTriangles")) !=
                Spec.SectionTrianglesBySlot[Spec.TopologyTrunkSlotIndex] ||
            !LodRow->GetBoolField(TEXT("globalXYZExtremaComponentsRequired")) ||
            !LodRow->GetBoolField(TEXT("wholeBranchAndLeafComponentsOnly")) ||
            !HasExactTopologyReplayReadbackContract(
                LodRow->GetObjectField(TEXT("retainedReplayReadback"))))
        {
            OutError = TEXT("A selected V4 topology-preserving per-LOD contract changed: ") +
                Spec.SelectionId;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateExternalMeshContractRows(
    const TSharedPtr<FJsonObject>& Contract,
    FString& OutError)
{
    if (!Contract ||
        !Contract->HasTypedField<EJson::Array>(TEXT("selectedMeshDerivatives")))
    {
        OutError = TEXT("The frozen selected-mesh derivative roster is absent.");
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>& Rows =
        Contract->GetArrayField(TEXT("selectedMeshDerivatives"));
    for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())
    {
        TSharedPtr<FJsonObject> Match;
        int32 Matches = 0;
        for (const TSharedPtr<FJsonValue>& Value : Rows)
        {
            const TSharedPtr<FJsonObject> Row = Value ? Value->AsObject() : nullptr;
            if (Row && Row->GetStringField(TEXT("selectionId")) == Spec.SelectionId)
            {
                Match = Row;
                ++Matches;
            }
        }
        if (Matches != 1 || !Match)
        {
            OutError = TEXT("A selected V4 source mesh row is absent or duplicated: ") +
                Spec.SelectionId;
            return false;
        }
        const TSharedPtr<FJsonObject> Source =
            Match->GetObjectField(TEXT("sourceMesh"));
        const TArray<TSharedPtr<FJsonValue>>& Slots =
            Match->GetArrayField(TEXT("materialSlotOrder"));
        const TSharedPtr<FJsonObject> Caps =
            Match->GetObjectField(TEXT("runtimeTriangleCaps"));
        if (Source->GetStringField(TEXT("path")) != Spec.RelativeSourcePath ||
            static_cast<int64>(Source->GetNumberField(TEXT("bytes"))) !=
                Spec.SourceBytes ||
            !Source->GetStringField(TEXT("sha256")).Equals(
                Spec.SourceSha256, ESearchCase::IgnoreCase) ||
            Match->GetStringField(TEXT("sourceModelName")) !=
                Spec.SourceModelName ||
            Match->GetIntegerField(TEXT("sourceTriangles")) !=
                Spec.SourceTriangles ||
            Slots.Num() != Spec.SlotOrder.Num() ||
            !FMath::IsNearlyEqual(
                Match->GetNumberField(TEXT("expectedUnscaledImportedZSpanCm")),
                Spec.ExpectedImportedZSpanCm,
                0.0000001) ||
            Match->GetStringField(TEXT("stagingAsset")) !=
                StagingVegetationAssetPath + TEXT("/") + Spec.StagingAssetName ||
            Match->GetStringField(TEXT("runtimeDerivedAsset")) !=
                MeshAssetPath + TEXT("/") + Spec.RuntimeAssetName ||
            Match->GetBoolField(TEXT("rawSourceRuntimeSelected")) ||
            Match->GetIntegerField(TEXT("runtimeRequiredMinLOD")) != 1 ||
            Spec.RuntimeLodCaps.Num() < 2)
        {
            OutError = TEXT("A selected V4 mesh source/import/path/LOD row changed: ") +
                Spec.SelectionId;
            return false;
        }
        for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
        {
            if (Slots[Slot]->AsString() != Spec.SlotOrder[Slot].ToString())
            {
                OutError = TEXT("A selected V4 mesh material-slot order changed: ") +
                    Spec.SelectionId;
                return false;
            }
        }
        for (int32 Lod = 0; Lod < Spec.RuntimeLodCaps.Num(); ++Lod)
        {
            if (Caps->GetIntegerField(*FString::Printf(TEXT("LOD%d"), Lod)) !=
                    Spec.RuntimeLodCaps[Lod].MaximumTriangles)
            {
                OutError = TEXT("A selected V4 runtime LOD cap changed: ") +
                    Spec.SelectionId;
                return false;
            }
        }
        if (!ValidateRuntimePreReductionContract(Match, Spec, OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

UAssetImportTask* MakeRawMeshImportTask(
    const FExternalMeshSpec& Spec)
{
    UFbxImportUI* Options = NewObject<UFbxImportUI>();
    UFbxFactory* Factory = NewObject<UFbxFactory>();
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    if (!Options || !Factory || !Task || !Options->StaticMeshImportData)
    {
        return nullptr;
    }
    Options->bImportAsSkeletal = false;
    Options->MeshTypeToImport = FBXIT_StaticMesh;
    Options->bAutomatedImportShouldDetectType = false;
    Options->bImportMesh = true;
    Options->bImportMaterials = false;
    Options->bImportTextures = false;
    Options->StaticMeshImportData->bConvertScene = !Spec.bObjMetresZUp;
    Options->StaticMeshImportData->bConvertSceneUnit = !Spec.bObjMetresZUp;
    Options->StaticMeshImportData->ImportUniformScale =
        Spec.bObjMetresZUp ? 100.0f : 1.0f;
    Options->StaticMeshImportData->bForceFrontXAxis = false;
    Options->StaticMeshImportData->bCombineMeshes = false;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = true;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormalsAndTangents;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = false;
    Options->StaticMeshImportData->bRemoveDegenerates = true;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = FPaths::Combine(
        VegetationSourceRoot(), Spec.RelativeSourcePath);
    Task->DestinationPath = StagingVegetationAssetPath;
    Task->DestinationName = Spec.StagingAssetName;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

bool ValidateRawMeshImportTask(
    UAssetImportTask* Task,
    const FExternalMeshSpec& Spec,
    FString& OutError)
{
    const UFbxImportUI* Options = Task
        ? Cast<UFbxImportUI>(Task->Options)
        : nullptr;
    const UFbxStaticMeshImportData* Data = Options
        ? Options->StaticMeshImportData
        : nullptr;
    if (!Task || !Options || !Data ||
        Task->Filename != FPaths::Combine(
            VegetationSourceRoot(), Spec.RelativeSourcePath) ||
        Task->DestinationPath != StagingVegetationAssetPath ||
        Task->DestinationName != Spec.StagingAssetName ||
        Options->bImportAsSkeletal ||
        Options->MeshTypeToImport != FBXIT_StaticMesh ||
        Options->bAutomatedImportShouldDetectType ||
        !Options->bImportMesh || Options->bImportMaterials ||
        Options->bImportTextures ||
        Data->bConvertScene != !Spec.bObjMetresZUp ||
        Data->bConvertSceneUnit != !Spec.bObjMetresZUp ||
        !FMath::IsNearlyEqual(
            Data->ImportUniformScale,
            Spec.bObjMetresZUp ? 100.0f : 1.0f) ||
        Data->bForceFrontXAxis || Data->bCombineMeshes ||
        Data->bImportMeshLODs || !Data->bTransformVertexToAbsolute ||
        Data->bBakePivotInVertex || Data->bAutoGenerateCollision ||
        !Data->bGenerateLightmapUVs ||
        Data->NormalImportMethod !=
            EFBXNormalImportMethod::FBXNIM_ImportNormalsAndTangents ||
        Data->NormalGenerationMethod !=
            EFBXNormalGenerationMethod::MikkTSpace ||
        Data->bBuildNanite || !Data->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The in-memory V4 raw mesh importer policy changed: ") +
            Spec.SelectionId;
        return false;
    }
    OutError.Reset();
    return true;
}

struct FMeshTopologyPredicateSnapshot final
{
    int32 Triangles = 0;
    TArray<FName> Slots;
    TArray<int32> Sections;
    double AggregateZSpanCm = 0.0;
    double Lod0PositionZSpanCm = 0.0;
    bool bHasLod0PositionBounds = false;
};

bool LodPositionBufferBounds(
    const UStaticMesh* Mesh,
    int32 LodIndex,
    FBox3f& OutBounds)
{
    const FStaticMeshRenderData* RenderData = Mesh
        ? Mesh->GetRenderData()
        : nullptr;
    if (!RenderData ||
        !RenderData->LODResources.IsValidIndex(LodIndex))
    {
        return false;
    }
    const FPositionVertexBuffer& Positions =
        RenderData->LODResources[LodIndex].VertexBuffers.PositionVertexBuffer;
    if (Positions.GetNumVertices() == 0)
    {
        return false;
    }
    FVector3f Minimum = Positions.VertexPosition(0);
    FVector3f Maximum = Minimum;
    for (uint32 Index = 1; Index < Positions.GetNumVertices(); ++Index)
    {
        const FVector3f Position = Positions.VertexPosition(Index);
        Minimum.X = FMath::Min(Minimum.X, Position.X);
        Minimum.Y = FMath::Min(Minimum.Y, Position.Y);
        Minimum.Z = FMath::Min(Minimum.Z, Position.Z);
        Maximum.X = FMath::Max(Maximum.X, Position.X);
        Maximum.Y = FMath::Max(Maximum.Y, Position.Y);
        Maximum.Z = FMath::Max(Maximum.Z, Position.Z);
    }
    OutBounds = FBox3f(Minimum, Maximum);
    return true;
}

FMeshTopologyPredicateSnapshot MeshTopologyPredicateSnapshot(
    UStaticMesh* Mesh)
{
    FMeshTopologyPredicateSnapshot Snapshot;
    if (!Mesh)
    {
        return Snapshot;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    Snapshot.Triangles = CountTriangles(Mesh, 0);
    Snapshot.Slots = ImportedSlotOrder(Mesh);
    Snapshot.Sections = CountLodTrianglesByMaterialSlot(Mesh, 0);
    Snapshot.AggregateZSpanCm = Mesh->GetBounds().BoxExtent.Z * 2.0;
    FBox3f Lod0Bounds;
    Snapshot.bHasLod0PositionBounds =
        LodPositionBufferBounds(Mesh, 0, Lod0Bounds);
    if (Snapshot.bHasLod0PositionBounds)
    {
        Snapshot.Lod0PositionZSpanCm =
            static_cast<double>(Lod0Bounds.Max.Z - Lod0Bounds.Min.Z);
    }
    return Snapshot;
}

bool HasExactRawMeshTopology(
    UStaticMesh* Mesh,
    const FExternalMeshSpec& Spec)
{
    if (!Mesh)
    {
        return false;
    }
    const FMeshTopologyPredicateSnapshot Snapshot =
        MeshTopologyPredicateSnapshot(Mesh);
    return Snapshot.Triangles == Spec.SourceTriangles &&
        Snapshot.Slots == Spec.SlotOrder &&
        Snapshot.Sections == Spec.SectionTrianglesBySlot &&
        Snapshot.bHasLod0PositionBounds &&
        FMath::IsNearlyEqual(
            Snapshot.Lod0PositionZSpanCm,
            Spec.ExpectedImportedZSpanCm,
            FMath::Max(0.10, Spec.ExpectedImportedZSpanCm * 0.001));
}

void ApplyVisualCollisionPolicyWithoutBuild(UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return;
    }
    Mesh->Modify();
    Mesh->NaniteSettings.bEnabled = false;
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseDefault;
    }
    Mesh->MarkPackageDirty();
}

void RemoveVisualCollision(UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return;
    }
    ApplyVisualCollisionPolicyWithoutBuild(Mesh);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
}

FString IntRoster(const TArray<int32>& Values)
{
    TArray<FString> Strings;
    Strings.Reserve(Values.Num());
    for (const int32 Value : Values)
    {
        Strings.Add(LexToString(Value));
    }
    return FString::Join(Strings, TEXT(","));
}

struct FTopologyComponent final
{
    int32 SlotIndex = INDEX_NONE;
    int32 MinimumPolygonId = MAX_int32;
    int32 MinimumVertexId = MAX_int32;
    int32 TriangleCount = 0;
    uint64 StableOrder = 0;
    FBox3f Bounds = FBox3f(EForceInit::ForceInit);
    TArray<FPolygonID> Polygons;
    bool bForcedExtremum = false;
    bool bKeep = false;
};

struct FTopologyLodReadback final
{
    int32 TriangleCount = 0;
    TArray<int32> SectionTriangles;
    TArray<int32> ComponentCounts;
    FString CanonicalComponentRoster;
    FString ComponentRosterSha256;
};

const FName RawTopologyPolygonIdAttribute(
    TEXT("TRIAD_IPV4_RawTopologyPolygonId"));
const FName RawTopologyVertexIdAttribute(
    TEXT("TRIAD_IPV4_RawTopologyVertexId"));

bool EnsureRawTopologyIdentityAttributes(
    FMeshDescription& Description,
    bool bMayInitialize,
    FString& OutError)
{
    TPolygonAttributesRef<int32> RawPolygonIds =
        Description.PolygonAttributes().GetAttributesRef<int32>(
            RawTopologyPolygonIdAttribute);
    TVertexAttributesRef<int32> RawVertexIds =
        Description.VertexAttributes().GetAttributesRef<int32>(
            RawTopologyVertexIdAttribute);
    bool bInitializedPolygonIds = false;
    bool bInitializedVertexIds = false;
    if (!RawPolygonIds.IsValid() && bMayInitialize)
    {
        Description.PolygonAttributes().RegisterAttribute<int32>(
            RawTopologyPolygonIdAttribute,
            1,
            INDEX_NONE,
            EMeshAttributeFlags::None);
        RawPolygonIds = Description.PolygonAttributes().GetAttributesRef<int32>(
            RawTopologyPolygonIdAttribute);
        if (RawPolygonIds.IsValid())
        {
            bInitializedPolygonIds = true;
            for (const FPolygonID PolygonId :
                 Description.Polygons().GetElementIDs())
            {
                RawPolygonIds[PolygonId] = PolygonId.GetValue();
            }
        }
    }
    if (!RawVertexIds.IsValid() && bMayInitialize)
    {
        Description.VertexAttributes().RegisterAttribute<int32>(
            RawTopologyVertexIdAttribute,
            1,
            INDEX_NONE,
            EMeshAttributeFlags::None);
        RawVertexIds = Description.VertexAttributes().GetAttributesRef<int32>(
            RawTopologyVertexIdAttribute);
        if (RawVertexIds.IsValid())
        {
            bInitializedVertexIds = true;
            for (const FVertexID VertexId : Description.Vertices().GetElementIDs())
            {
                RawVertexIds[VertexId] = VertexId.GetValue();
            }
        }
    }
    if (!RawPolygonIds.IsValid() || !RawVertexIds.IsValid())
    {
        OutError = TEXT("A topology-preserving V4 source LOD lacks persistent raw identity attributes.");
        return false;
    }
    if (bInitializedPolygonIds && bInitializedVertexIds)
    {
        OutError.Reset();
        return true;
    }
    TSet<int32> UniquePolygonIds;
    for (const FPolygonID PolygonId : Description.Polygons().GetElementIDs())
    {
        const int32 RawId = RawPolygonIds[PolygonId];
        if (RawId < 0 || UniquePolygonIds.Contains(RawId))
        {
            OutError = TEXT("A topology-preserving V4 raw polygon identity is invalid or duplicated.");
            return false;
        }
        UniquePolygonIds.Add(RawId);
    }
    TSet<int32> UniqueVertexIds;
    for (const FVertexID VertexId : Description.Vertices().GetElementIDs())
    {
        const int32 RawId = RawVertexIds[VertexId];
        if (RawId < 0 || UniqueVertexIds.Contains(RawId))
        {
            OutError = TEXT("A topology-preserving V4 raw vertex identity is invalid or duplicated.");
            return false;
        }
        UniqueVertexIds.Add(RawId);
    }
    OutError.Reset();
    return true;
}

bool AnalyzeTopologyComponents(
    FMeshDescription& Description,
    const TArray<FName>& SlotOrder,
    TArray<TArray<FTopologyComponent>>& OutBySlot,
    FString& OutError)
{
    OutBySlot.Reset();
    OutBySlot.SetNum(SlotOrder.Num());
    if (SlotOrder.IsEmpty() || Description.Polygons().Num() <= 0)
    {
        OutError = TEXT("A topology-preserving V4 source LOD is empty.");
        return false;
    }
    FStaticMeshAttributes Attributes(Description);
    const TPolygonGroupAttributesConstRef<FName> GroupSlotNames =
        Attributes.GetPolygonGroupMaterialSlotNames();
    const TVertexAttributesConstRef<FVector3f> VertexPositions =
        Attributes.GetVertexPositions();
    const TPolygonAttributesConstRef<int32> RawPolygonIds =
        Description.PolygonAttributes().GetAttributesRef<int32>(
            RawTopologyPolygonIdAttribute);
    const TVertexAttributesConstRef<int32> RawVertexIds =
        Description.VertexAttributes().GetAttributesRef<int32>(
            RawTopologyVertexIdAttribute);
    if (!RawPolygonIds.IsValid() || !RawVertexIds.IsValid())
    {
        OutError = TEXT("A topology-preserving V4 source LOD lost its raw identity attributes.");
        return false;
    }
    TArray<int32> GroupToSlot;
    GroupToSlot.Init(INDEX_NONE, Description.PolygonGroups().GetArraySize());
    for (const FPolygonGroupID GroupId :
         Description.PolygonGroups().GetElementIDs())
    {
        const int32 SlotIndex = SlotOrder.IndexOfByKey(GroupSlotNames[GroupId]);
        if (SlotIndex == INDEX_NONE)
        {
            OutError = TEXT("A topology-preserving V4 source LOD has an unknown polygon-group slot.");
            return false;
        }
        GroupToSlot[GroupId.GetValue()] = SlotIndex;
    }
    TBitArray<> Visited(false, Description.Polygons().GetArraySize());
    for (const FPolygonID StartPolygon : Description.Polygons().GetElementIDs())
    {
        if (Visited[StartPolygon.GetValue()])
        {
            continue;
        }
        const FPolygonGroupID StartGroup =
            Description.GetPolygonPolygonGroup(StartPolygon);
        const int32 SlotIndex = GroupToSlot[StartGroup.GetValue()];
        FTopologyComponent Component;
        Component.SlotIndex = SlotIndex;
        TArray<FPolygonID> Queue;
        Queue.Add(StartPolygon);
        Visited[StartPolygon.GetValue()] = true;
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const FPolygonID PolygonId = Queue[Head];
            Component.Polygons.Add(PolygonId);
            Component.MinimumPolygonId = FMath::Min(
                Component.MinimumPolygonId, RawPolygonIds[PolygonId]);
            Component.TriangleCount +=
                Description.GetPolygonTriangles(PolygonId).Num();
            TArray<FVertexID, TInlineAllocator<4>> PolygonVertices;
            Description.GetPolygonVertices(PolygonId, PolygonVertices);
            for (const FVertexID VertexId : PolygonVertices)
            {
                Component.MinimumVertexId = FMath::Min(
                    Component.MinimumVertexId, RawVertexIds[VertexId]);
                Component.Bounds += VertexPositions[VertexId];
            }
            TArray<FEdgeID, TInlineAllocator<4>> Edges;
            Description.GetPolygonPerimeterEdges(PolygonId, Edges);
            for (const FEdgeID EdgeId : Edges)
            {
                TArray<FPolygonID, TInlineAllocator<4>> Neighbours;
                Description.GetEdgeConnectedPolygons(EdgeId, Neighbours);
                for (const FPolygonID Neighbour : Neighbours)
                {
                    if (Neighbour == PolygonId ||
                        !Description.Polygons().IsValid(Neighbour) ||
                        Visited[Neighbour.GetValue()] ||
                        GroupToSlot[
                            Description.GetPolygonPolygonGroup(Neighbour).GetValue()] !=
                            SlotIndex)
                    {
                        continue;
                    }
                    Visited[Neighbour.GetValue()] = true;
                    Queue.Add(Neighbour);
                }
            }
        }
        if (Component.Polygons.IsEmpty() || Component.TriangleCount <= 0 ||
            Component.MinimumPolygonId == MAX_int32 ||
            Component.MinimumVertexId == MAX_int32 || !Component.Bounds.IsValid)
        {
            OutError = TEXT("A topology-preserving V4 component is invalid.");
            return false;
        }
        const uint64 Id = static_cast<uint64>(
            static_cast<uint32>(Component.MinimumPolygonId));
        Component.StableOrder =
            (Id * 0x9E3779B185EBCA87ull) ^
            (static_cast<uint64>(SlotIndex + 1) * 0xC2B2AE3D27D4EB4Full);
        OutBySlot[SlotIndex].Add(MoveTemp(Component));
    }
    OutError.Reset();
    return true;
}

void MarkGlobalExtremumComponents(
    TArray<TArray<FTopologyComponent>>& BySlot,
    const FBox3f& FullBounds)
{
    struct FExtremumChoice final
    {
        FTopologyComponent* Component = nullptr;
        int32 MinimumVertexId = MAX_int32;
        int32 MinimumPolygonId = MAX_int32;
    };
    FExtremumChoice Choices[6];
    constexpr float Tolerance = 0.001f;
    for (TArray<FTopologyComponent>& Components : BySlot)
    {
        for (FTopologyComponent& Component : Components)
        {
            const bool Matches[6] = {
                FMath::IsNearlyEqual(Component.Bounds.Min.X, FullBounds.Min.X, Tolerance),
                FMath::IsNearlyEqual(Component.Bounds.Max.X, FullBounds.Max.X, Tolerance),
                FMath::IsNearlyEqual(Component.Bounds.Min.Y, FullBounds.Min.Y, Tolerance),
                FMath::IsNearlyEqual(Component.Bounds.Max.Y, FullBounds.Max.Y, Tolerance),
                FMath::IsNearlyEqual(Component.Bounds.Min.Z, FullBounds.Min.Z, Tolerance),
                FMath::IsNearlyEqual(Component.Bounds.Max.Z, FullBounds.Max.Z, Tolerance)};
            for (int32 Direction = 0; Direction < 6; ++Direction)
            {
                if (Matches[Direction] &&
                    (Component.MinimumVertexId <
                        Choices[Direction].MinimumVertexId ||
                     (Component.MinimumVertexId ==
                        Choices[Direction].MinimumVertexId &&
                      Component.MinimumPolygonId <
                        Choices[Direction].MinimumPolygonId)))
                {
                    Choices[Direction].Component = &Component;
                    Choices[Direction].MinimumVertexId = Component.MinimumVertexId;
                    Choices[Direction].MinimumPolygonId = Component.MinimumPolygonId;
                }
            }
        }
    }
    for (FExtremumChoice& Choice : Choices)
    {
        if (Choice.Component)
        {
            Choice.Component->bForcedExtremum = true;
        }
    }
}

bool BuildTopologyReadback(
    FMeshDescription& Description,
    const TArray<FName>& SlotOrder,
    FTopologyLodReadback& OutReadback,
    FString& OutError)
{
    TArray<TArray<FTopologyComponent>> BySlot;
    if (!AnalyzeTopologyComponents(Description, SlotOrder, BySlot, OutError))
    {
        return false;
    }
    OutReadback = FTopologyLodReadback();
    OutReadback.SectionTriangles.Init(0, SlotOrder.Num());
    OutReadback.ComponentCounts.Init(0, SlotOrder.Num());
    TArray<FString> Roster;
    for (int32 SlotIndex = 0; SlotIndex < BySlot.Num(); ++SlotIndex)
    {
        TArray<FTopologyComponent>& Components = BySlot[SlotIndex];
        Components.Sort([](const FTopologyComponent& A, const FTopologyComponent& B)
        {
            return A.MinimumPolygonId < B.MinimumPolygonId;
        });
        OutReadback.ComponentCounts[SlotIndex] = Components.Num();
        for (const FTopologyComponent& Component : Components)
        {
            OutReadback.SectionTriangles[SlotIndex] += Component.TriangleCount;
            Roster.Add(FString::Printf(
                TEXT("S%d:P%d:N%d:T%d"),
                SlotIndex,
                Component.MinimumPolygonId,
                Component.Polygons.Num(),
                Component.TriangleCount));
        }
    }
    OutReadback.TriangleCount = Description.Triangles().Num();
    OutReadback.CanonicalComponentRoster = FString::Join(Roster, TEXT(";"));
    if (OutReadback.TriangleCount <= 0 ||
        !HashUtf8StringSha256(
            OutReadback.CanonicalComponentRoster,
            OutReadback.ComponentRosterSha256))
    {
        OutError = TEXT("A topology-preserving V4 component roster could not be hashed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool FilterWholeTopologyComponents(
    FMeshDescription& Description,
    const FExternalMeshSpec& Spec,
    const TArray<int32>& SectionCaps,
    const TArray<int32>* ExpectedInputComponentCounts,
    FTopologyLodReadback& OutReadback,
    FString& OutError)
{
    if (SectionCaps.Num() != Spec.SlotOrder.Num() ||
        !SectionCaps.IsValidIndex(Spec.TopologyTrunkSlotIndex))
    {
        OutError = TEXT("A V4 topology-preserving section-cap roster is invalid: ") +
            Spec.SelectionId;
        return false;
    }
    const FBox3f InputBounds(Description.ComputeBoundingBox());
    TArray<TArray<FTopologyComponent>> BySlot;
    if (!InputBounds.IsValid ||
        !AnalyzeTopologyComponents(Description, Spec.SlotOrder, BySlot, OutError))
    {
        return false;
    }
    MarkGlobalExtremumComponents(BySlot, InputBounds);
    if (ExpectedInputComponentCounts &&
        *ExpectedInputComponentCounts != [&BySlot]()
        {
            TArray<int32> Counts;
            for (const TArray<FTopologyComponent>& Components : BySlot)
            {
                Counts.Add(Components.Num());
            }
            return Counts;
        }())
    {
        OutError = TEXT("The frozen V4 raw edge-component census changed: ") +
            Spec.SelectionId;
        return false;
    }
    TBitArray<> KeepPolygon(false, Description.Polygons().GetArraySize());
    TArray<FPolygonID> PolygonsToDelete;
    for (int32 SlotIndex = 0; SlotIndex < BySlot.Num(); ++SlotIndex)
    {
        TArray<FTopologyComponent>& Components = BySlot[SlotIndex];
        Components.Sort([](const FTopologyComponent& A, const FTopologyComponent& B)
        {
            return A.StableOrder == B.StableOrder
                ? A.MinimumPolygonId < B.MinimumPolygonId
                : A.StableOrder < B.StableOrder;
        });
        const int32 SlotCap = SectionCaps[SlotIndex];
        int32 SourceTriangles = 0;
        int32 KeptTriangles = 0;
        for (FTopologyComponent& Component : Components)
        {
            SourceTriangles += Component.TriangleCount;
            if (Component.bForcedExtremum ||
                SlotIndex == Spec.TopologyTrunkSlotIndex)
            {
                Component.bKeep = true;
                KeptTriangles += Component.TriangleCount;
            }
        }
        if (KeptTriangles > SlotCap ||
            (SlotIndex == Spec.TopologyTrunkSlotIndex &&
             SlotCap < SourceTriangles))
        {
            OutError = TEXT("A V4 topology-preserving cap cannot retain the required trunk/extrema: ") +
                Spec.SelectionId + TEXT("/") + Spec.SlotOrder[SlotIndex].ToString();
            return false;
        }
        for (FTopologyComponent& Component : Components)
        {
            if (!Component.bKeep &&
                KeptTriangles + Component.TriangleCount <= SlotCap)
            {
                Component.bKeep = true;
                KeptTriangles += Component.TriangleCount;
            }
            for (const FPolygonID PolygonId : Component.Polygons)
            {
                if (Component.bKeep)
                {
                    KeepPolygon[PolygonId.GetValue()] = true;
                }
                else
                {
                    PolygonsToDelete.Add(PolygonId);
                }
            }
        }
        if (KeptTriangles <= 0 || KeptTriangles > SlotCap)
        {
            OutError = TEXT("A V4 topology-preserving material section missed its cap: ") +
                Spec.SelectionId + TEXT("/") + Spec.SlotOrder[SlotIndex].ToString();
            return false;
        }
    }
    for (const FEdgeID EdgeId : Description.Edges().GetElementIDs())
    {
        TArray<FPolygonID, TInlineAllocator<4>> Connected;
        Description.GetEdgeConnectedPolygons(EdgeId, Connected);
        bool bSawKept = false;
        bool bSawDeleted = false;
        for (const FPolygonID PolygonId : Connected)
        {
            bSawKept |= KeepPolygon[PolygonId.GetValue()];
            bSawDeleted |= !KeepPolygon[PolygonId.GetValue()];
        }
        if (bSawKept && bSawDeleted)
        {
            OutError = TEXT("A V4 topology selection would introduce a new cut boundary: ") +
                Spec.SelectionId;
            return false;
        }
    }
    constexpr int32 DeleteBatchSize = 64 * 1024;
    for (int32 Offset = 0; Offset < PolygonsToDelete.Num(); Offset += DeleteBatchSize)
    {
        const int32 Count = FMath::Min(
            DeleteBatchSize, PolygonsToDelete.Num() - Offset);
        TArray<FPolygonID> Batch;
        Batch.Append(PolygonsToDelete.GetData() + Offset, Count);
        Description.DeletePolygons(Batch);
    }
    // The persistent raw-ID attributes survive this compaction, so all later
    // source LODs stay memory-bounded while their whole-component identities
    // remain comparable to LOD0 after a cold reload.
    FElementIDRemappings Remappings;
    Description.Compact(Remappings);
    const FBox3f OutputBounds(Description.ComputeBoundingBox());
    constexpr float BoundTolerance = 0.001f;
    if (!OutputBounds.IsValid ||
        !OutputBounds.Min.Equals(InputBounds.Min, BoundTolerance) ||
        !OutputBounds.Max.Equals(InputBounds.Max, BoundTolerance) ||
        !BuildTopologyReadback(
            Description, Spec.SlotOrder, OutReadback, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A V4 topology-preserving LOD lost an extremum: ") +
                Spec.SelectionId;
        }
        return false;
    }
    for (int32 SlotIndex = 0; SlotIndex < SectionCaps.Num(); ++SlotIndex)
    {
        if (OutReadback.SectionTriangles[SlotIndex] <= 0 ||
            OutReadback.SectionTriangles[SlotIndex] > SectionCaps[SlotIndex])
        {
            OutError = TEXT("A V4 topology-preserving LOD readback exceeded its section cap: ") +
                Spec.SelectionId;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

FString NestedIntRosters(const TArray<FTopologyLodReadback>& Readbacks, bool bComponents)
{
    TArray<FString> Values;
    for (const FTopologyLodReadback& Readback : Readbacks)
    {
        Values.Add(IntRoster(
            bComponents ? Readback.ComponentCounts : Readback.SectionTriangles));
    }
    return FString::Join(Values, TEXT("|"));
}

bool ConfigureTopologyPreservingRuntimeLods(
    UStaticMesh* Mesh,
    const FExternalMeshSpec& Spec,
    FString& OutError)
{
    if (!Mesh || Spec.RuntimeLodCaps.Num() < 2 ||
        Spec.RuntimeTopologySectionTriangleCapsByLod.Num() !=
            Spec.RuntimeLodCaps.Num() ||
        Spec.ExpectedSourceComponentCountsBySlot.Num() != Spec.SlotOrder.Num() ||
        !Spec.SlotOrder.IsValidIndex(Spec.TopologyTrunkSlotIndex))
    {
        OutError = TEXT("A V4 heavy tree has no exact topology-preserving LOD policy: ") +
            Spec.SelectionId;
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    Mesh->Modify();
    Mesh->SetNumSourceModels(Spec.RuntimeLodCaps.Num());
    Mesh->bAutoComputeLODScreenSize = false;
    TArray<FTopologyLodReadback> Readbacks;
    Readbacks.SetNum(Spec.RuntimeLodCaps.Num());
    for (int32 Lod = 0; Lod < Spec.RuntimeLodCaps.Num(); ++Lod)
    {
        FMeshDescription* Description = nullptr;
        if (Lod == 0)
        {
            Description = Mesh->GetMeshDescription(0);
        }
        else
        {
            FMeshDescription Clone;
            if (!Mesh->CloneMeshDescription(Lod - 1, Clone))
            {
                OutError = TEXT("Could not clone a bounded V4 topology source LOD: ") +
                    Spec.SelectionId;
                return false;
            }
            Description = Mesh->CreateMeshDescription(Lod, MoveTemp(Clone));
        }
        const TArray<int32>* ExpectedComponents =
            Lod == 0 ? &Spec.ExpectedSourceComponentCountsBySlot : nullptr;
        if (!Description ||
            !EnsureRawTopologyIdentityAttributes(
                *Description, Lod == 0, OutError) ||
            !FilterWholeTopologyComponents(
                *Description,
                Spec,
                Spec.RuntimeTopologySectionTriangleCapsByLod[Lod],
                ExpectedComponents,
                Readbacks[Lod],
                OutError) ||
            Readbacks[Lod].TriangleCount >
                Spec.RuntimeLodCaps[Lod].MaximumTriangles)
        {
            return false;
        }
        UStaticMesh::FCommitMeshDescriptionParams CommitParams;
        CommitParams.bMarkPackageDirty = false;
        CommitParams.bUseHashAsGuid = true;
        Mesh->CommitMeshDescription(Lod, CommitParams);
        FStaticMeshSourceModel& Model = Mesh->GetSourceModel(Lod);
        Model.ResetReductionSetting();
        Model.ScreenSize = FPerPlatformFloat(Spec.RuntimeLodCaps[Lod].ScreenSize);
        FMeshReductionSettings& Reduction = Model.ReductionSettings;
        Reduction.PercentTriangles = 1.0f;
        Reduction.MaxNumOfTriangles = MAX_uint32;
        Reduction.PercentVertices = 1.0f;
        Reduction.MaxNumOfVerts = MAX_uint32;
        Reduction.MaxDeviation = 0.0f;
        Reduction.WeldingThreshold = 0.0f;
        Reduction.BaseLODModel = Lod;
        Reduction.bRecalculateNormals = false;
        Reduction.bGenerateUniqueLightmapUVs = false;
        Model.BuildSettings.bGenerateLightmapUVs = false;
        if (Mesh->IsReductionActive(Lod))
        {
            OutError = TEXT("A custom V4 topology LOD unexpectedly activates reduction: ") +
                Spec.SelectionId;
            return false;
        }
    }
    ApplyVisualCollisionPolicyWithoutBuild(Mesh);
    Mesh->SetMinLODIdx(1);
    if (UMetaData* MetaData = Mesh->GetOutermost()->GetMetaData())
    {
        MetaData->SetValue(
            Mesh,
            TEXT("TRIAD_IPV4_RuntimeBaseSourcePolicy"),
            TEXT("TOPOLOGY_PRESERVING_MATERIAL_EDGE_COMPONENTS_V1"));
        TArray<FString> TriangleStrings;
        TArray<FString> DigestStrings;
        for (int32 Lod = 0; Lod < Readbacks.Num(); ++Lod)
        {
            TriangleStrings.Add(LexToString(Readbacks[Lod].TriangleCount));
            DigestStrings.Add(Readbacks[Lod].ComponentRosterSha256);
            MetaData->SetValue(
                Mesh,
                *FString::Printf(TEXT("TRIAD_IPV4_RuntimeTopologyLod%dComponentRoster"), Lod),
                *Readbacks[Lod].CanonicalComponentRoster);
        }
        MetaData->SetValue(
            Mesh,
            TEXT("TRIAD_IPV4_RuntimeTopologyLodTriangles"),
            *FString::Join(TriangleStrings, TEXT("|")));
        MetaData->SetValue(
            Mesh,
            TEXT("TRIAD_IPV4_RuntimeTopologyLodSectionTriangles"),
            *NestedIntRosters(Readbacks, false));
        MetaData->SetValue(
            Mesh,
            TEXT("TRIAD_IPV4_RuntimeTopologyLodComponentCounts"),
            *NestedIntRosters(Readbacks, true));
        MetaData->SetValue(
            Mesh,
            TEXT("TRIAD_IPV4_RuntimeTopologyLodComponentDigests"),
            *FString::Join(DigestStrings, TEXT("|")));
    }
    Mesh->ClearMeshDescriptions();
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    if (Mesh->GetMinLODIdx() != 1 ||
        Mesh->GetNumLODs() != Spec.RuntimeLodCaps.Num())
    {
        OutError = TEXT("A custom V4 topology tree did not persist MinLOD1/LOD count: ") +
            Spec.SelectionId;
        return false;
    }
    int32 Previous = MAX_int32;
    for (int32 Lod = 0; Lod < Readbacks.Num(); ++Lod)
    {
        if (Mesh->IsReductionActive(Lod) ||
            CountTriangles(Mesh, Lod) != Readbacks[Lod].TriangleCount ||
            CountLodTrianglesByMaterialSlot(Mesh, Lod) !=
                Readbacks[Lod].SectionTriangles ||
            Readbacks[Lod].TriangleCount >= Previous)
        {
            OutError = TEXT("A custom V4 topology tree render LOD changed after build: ") +
                Spec.SelectionId;
            return false;
        }
        Previous = Readbacks[Lod].TriangleCount;
    }
    FMemory::Trim(true);
    OutError.Reset();
    return true;
}

bool StampExactSourceRuntimeBasePolicy(
    UStaticMesh* Mesh,
    const FExternalMeshSpec& Spec,
    int32& OutRuntimeSourceTriangles,
    FString& OutError)
{
    OutRuntimeSourceTriangles = 0;
    if (!Mesh || Spec.SourceTriangles <= 0 ||
        Spec.SectionTrianglesBySlot.Num() != Spec.SlotOrder.Num())
    {
        OutError = TEXT("A V4 within-cap runtime mesh has no exact source policy: ") +
            Spec.SelectionId;
        return false;
    }
    OutRuntimeSourceTriangles = Spec.SourceTriangles;
    if (UMetaData* MetaData = Mesh->GetOutermost()->GetMetaData())
    {
        MetaData->SetValue(
            Mesh,
            TEXT("TRIAD_IPV4_RuntimeBaseSourcePolicy"),
            TEXT("EXACT_SOURCE_NO_PRE_REDUCTION_V1"));
        MetaData->SetValue(
            Mesh,
            TEXT("TRIAD_IPV4_RuntimeBaseSourceTriangles"),
            *LexToString(OutRuntimeSourceTriangles));
        MetaData->SetValue(
            Mesh,
            TEXT("TRIAD_IPV4_RuntimeBaseSectionTriangleCaps"),
            *IntRoster(Spec.SectionTrianglesBySlot));
    }
    OutError.Reset();
    return true;
}

bool ConfigureRuntimeDerivativeLods(
    UStaticMesh* Mesh,
    int32 SourceTriangles,
    const TArray<FLodCap>& Caps,
    FString& OutError)
{
    if (!Mesh || SourceTriangles <= 0 || Caps.Num() < 2 ||
        Caps[0].MaximumTriangles <= 0)
    {
        OutError = TEXT("A V4 runtime derivative has no exact LOD plan.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    Mesh->Modify();
    Mesh->SetNumSourceModels(Caps.Num());
    Mesh->bAutoComputeLODScreenSize = false;
    for (int32 Lod = 0; Lod < Caps.Num(); ++Lod)
    {
        FStaticMeshSourceModel& Model = Mesh->GetSourceModel(Lod);
        Model.ScreenSize = FPerPlatformFloat(Caps[Lod].ScreenSize);
        FMeshReductionSettings& Reduction = Model.ReductionSettings;
        Reduction.TerminationCriterion =
            EStaticMeshReductionTerimationCriterion::Triangles;
        Reduction.PercentTriangles = FMath::Min(
            1.0f,
            static_cast<float>(Caps[Lod].MaximumTriangles) /
                static_cast<float>(SourceTriangles));
        Reduction.MaxNumOfTriangles = Caps[Lod].MaximumTriangles;
        Reduction.PercentVertices = 1.0f;
        Reduction.MaxNumOfVerts = MAX_uint32;
        Reduction.MaxDeviation = 0.0f;
        Reduction.WeldingThreshold = 0.0f;
        Reduction.BaseLODModel = 0;
        Reduction.SilhouetteImportance = EMeshFeatureImportance::Highest;
        Reduction.TextureImportance = EMeshFeatureImportance::Highest;
        Reduction.ShadingImportance = EMeshFeatureImportance::Highest;
        Reduction.bRecalculateNormals = false;
        Reduction.bGenerateUniqueLightmapUVs = false;
    }
    Mesh->SetMinLODIdx(1);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    if (Mesh->GetMinLODIdx() != 1 || Mesh->GetNumLODs() != Caps.Num())
    {
        OutError = TEXT("A V4 runtime derivative did not persist exact MinLOD1/LOD count.");
        return false;
    }
    int32 Previous = MAX_int32;
    for (int32 Lod = 0; Lod < Caps.Num(); ++Lod)
    {
        const int32 Triangles = CountTriangles(Mesh, Lod);
        if (Triangles <= 0 || Triangles > Caps[Lod].MaximumTriangles ||
            (Lod > 0 && Triangles >= Previous))
        {
            OutError = FString::Printf(
                TEXT("V4 runtime derivative LOD%d violates cap/order: %s triangles=%d cap=%d."),
                Lod,
                *Mesh->GetPathName(),
                Triangles,
                Caps[Lod].MaximumTriangles);
            return false;
        }
        Previous = Triangles;
    }
    OutError.Reset();
    return true;
}

void StampDerivativeProvenance(
    UStaticMesh* Mesh,
    const FString& SourceIdentity,
    const FString& SourceSha256,
    const FString& SelectionId)
{
    if (!Mesh || !Mesh->GetOutermost())
    {
        return;
    }
    UMetaData* MetaData = Mesh->GetOutermost()->GetMetaData();
    MetaData->SetValue(Mesh, TEXT("TRIAD_IPV4_SourceIdentity"), *SourceIdentity);
    MetaData->SetValue(Mesh, TEXT("TRIAD_IPV4_SourceSha256"), *SourceSha256);
    MetaData->SetValue(Mesh, TEXT("TRIAD_IPV4_SelectionId"), *SelectionId);
    MetaData->SetValue(Mesh, TEXT("TRIAD_IPV4_RawLod0RuntimeSelected"), TEXT("false"));
}

bool HasDerivativeProvenance(
    UStaticMesh* Mesh,
    const FString& SourceIdentity,
    const FString& SourceSha256,
    const FString& SelectionId)
{
    UMetaData* MetaData = Mesh && Mesh->GetOutermost()
        ? Mesh->GetOutermost()->GetMetaData()
        : nullptr;
    return MetaData &&
        FString(MetaData->GetValue(Mesh, TEXT("TRIAD_IPV4_SourceIdentity"))) ==
            SourceIdentity &&
        FString(MetaData->GetValue(Mesh, TEXT("TRIAD_IPV4_SourceSha256"))).Equals(
            SourceSha256, ESearchCase::IgnoreCase) &&
        FString(MetaData->GetValue(Mesh, TEXT("TRIAD_IPV4_SelectionId"))) ==
            SelectionId &&
        FString(MetaData->GetValue(
            Mesh, TEXT("TRIAD_IPV4_RawLod0RuntimeSelected"))) == TEXT("false");
}

bool HasRuntimeBasePolicy(
    UStaticMesh* Mesh,
    const FExternalMeshSpec& Spec,
    FString& OutError)
{
    UMetaData* MetaData = Mesh && Mesh->GetOutermost()
        ? Mesh->GetOutermost()->GetMetaData()
        : nullptr;
    if (!MetaData || Spec.RuntimeLodCaps.IsEmpty())
    {
        OutError = TEXT("A V4 runtime mesh has no persisted runtime-base metadata: ") +
            Spec.SelectionId;
        return false;
    }
    const bool bHeavy =
        Spec.RuntimeTopologySectionTriangleCapsByLod.Num() ==
            Spec.RuntimeLodCaps.Num();
    if (!bHeavy)
    {
        const int32 StoredTriangles = FCString::Atoi(*FString(
            MetaData->GetValue(
                Mesh, TEXT("TRIAD_IPV4_RuntimeBaseSourceTriangles"))));
        const TArray<int32> ActualSections =
            CountLodTrianglesByMaterialSlot(Mesh, 0);
        if (FString(MetaData->GetValue(
                Mesh, TEXT("TRIAD_IPV4_RuntimeBaseSourcePolicy"))) !=
                TEXT("EXACT_SOURCE_NO_PRE_REDUCTION_V1") ||
            FString(MetaData->GetValue(
                Mesh, TEXT("TRIAD_IPV4_RuntimeBaseSectionTriangleCaps"))) !=
                IntRoster(Spec.SectionTrianglesBySlot) ||
            StoredTriangles != Spec.SourceTriangles ||
            StoredTriangles != CountTriangles(Mesh, 0) ||
            ActualSections != Spec.SectionTrianglesBySlot)
        {
            OutError = TEXT("A V4 exact-source runtime LOD0 or its metadata changed: ") +
                Spec.SelectionId;
            return false;
        }
        OutError.Reset();
        return true;
    }

    if (Spec.RuntimeTopologySectionTriangleCapsByLod.Num() !=
            Spec.RuntimeLodCaps.Num() ||
        !Spec.SlotOrder.IsValidIndex(Spec.TopologyTrunkSlotIndex) ||
        FString(MetaData->GetValue(
            Mesh, TEXT("TRIAD_IPV4_RuntimeBaseSourcePolicy"))) !=
            TEXT("TOPOLOGY_PRESERVING_MATERIAL_EDGE_COMPONENTS_V1"))
    {
        OutError = TEXT("A V4 topology-preserving runtime policy changed: ") +
            Spec.SelectionId;
        return false;
    }

    TArray<FTopologyLodReadback> Readbacks;
    TArray<FString> TriangleStrings;
    TArray<FString> DigestStrings;
    FString PreviousRoster;
    FBox3f ReferenceBounds(EForceInit::ForceInit);
    for (int32 Lod = 0; Lod < Spec.RuntimeLodCaps.Num(); ++Lod)
    {
        FMeshDescription Description;
        if (!Mesh->CloneMeshDescription(Lod, Description))
        {
            OutError = TEXT("A V4 topology-preserving source MeshDescription is absent: ") +
                Spec.SelectionId + FString::Printf(TEXT("/LOD%d"), Lod);
            return false;
        }
        FTopologyLodReadback Readback;
        if (!EnsureRawTopologyIdentityAttributes(
                Description, false, OutError) ||
            !BuildTopologyReadback(
                Description, Spec.SlotOrder, Readback, OutError))
        {
            return false;
        }
        const FBox3f Bounds(Description.ComputeBoundingBox());
        const TArray<int32>& SectionCaps =
            Spec.RuntimeTopologySectionTriangleCapsByLod[Lod];
        const FStaticMeshSourceModel& Model = Mesh->GetSourceModel(Lod);
        if (!Bounds.IsValid || SectionCaps.Num() != Spec.SlotOrder.Num() ||
            Readback.SectionTriangles.Num() != SectionCaps.Num() ||
            Readback.TriangleCount <= 0 ||
            Readback.TriangleCount > Spec.RuntimeLodCaps[Lod].MaximumTriangles ||
            CountTriangles(Mesh, Lod) != Readback.TriangleCount ||
            CountLodTrianglesByMaterialSlot(Mesh, Lod) !=
                Readback.SectionTriangles ||
            Mesh->IsReductionActive(Lod) ||
            Model.ReductionSettings.BaseLODModel != Lod ||
            !FMath::IsNearlyEqual(
                Model.ScreenSize.Default,
                Spec.RuntimeLodCaps[Lod].ScreenSize,
                0.000001f))
        {
            OutError = TEXT("A V4 topology-preserving custom source/render LOD changed: ") +
                Spec.SelectionId + FString::Printf(TEXT("/LOD%d"), Lod);
            return false;
        }
        for (int32 SlotIndex = 0; SlotIndex < SectionCaps.Num(); ++SlotIndex)
        {
            if (Readback.SectionTriangles[SlotIndex] <= 0 ||
                Readback.SectionTriangles[SlotIndex] > SectionCaps[SlotIndex] ||
                (SlotIndex == Spec.TopologyTrunkSlotIndex &&
                 Readback.SectionTriangles[SlotIndex] !=
                    Spec.SectionTrianglesBySlot[SlotIndex]))
            {
                OutError = TEXT("A V4 topology-preserving material-section/trunk LOD changed: ") +
                    Spec.SelectionId + FString::Printf(TEXT("/LOD%d"), Lod);
                return false;
            }
        }
        if (Lod == 0)
        {
            ReferenceBounds = Bounds;
        }
        else
        {
            constexpr float BoundsTolerance = 0.001f;
            if (!Bounds.Min.Equals(ReferenceBounds.Min, BoundsTolerance) ||
                !Bounds.Max.Equals(ReferenceBounds.Max, BoundsTolerance))
            {
                OutError = TEXT("A V4 topology-preserving LOD lost a global source extremum: ") +
                    Spec.SelectionId + FString::Printf(TEXT("/LOD%d"), Lod);
                return false;
            }
            TArray<FString> ParentTokens;
            TArray<FString> ChildTokens;
            PreviousRoster.ParseIntoArray(ParentTokens, TEXT(";"), true);
            Readback.CanonicalComponentRoster.ParseIntoArray(
                ChildTokens, TEXT(";"), true);
            TSet<FString> ParentSet;
            for (const FString& Token : ParentTokens)
            {
                ParentSet.Add(Token);
            }
            for (const FString& Token : ChildTokens)
            {
                if (!ParentSet.Contains(Token))
                {
                    OutError = TEXT("A V4 topology-preserving LOD is not a nested whole-component subset: ") +
                        Spec.SelectionId + FString::Printf(TEXT("/LOD%d"), Lod);
                    return false;
                }
            }
        }
        const FString StoredRoster(MetaData->GetValue(
            Mesh,
            *FString::Printf(
                TEXT("TRIAD_IPV4_RuntimeTopologyLod%dComponentRoster"), Lod)));
        if (StoredRoster != Readback.CanonicalComponentRoster)
        {
            OutError = TEXT("A V4 topology-preserving canonical component roster changed: ") +
                Spec.SelectionId + FString::Printf(TEXT("/LOD%d"), Lod);
            return false;
        }
        PreviousRoster = Readback.CanonicalComponentRoster;
        TriangleStrings.Add(LexToString(Readback.TriangleCount));
        DigestStrings.Add(Readback.ComponentRosterSha256);
        Readbacks.Add(MoveTemp(Readback));
    }
    if (FString(MetaData->GetValue(
            Mesh, TEXT("TRIAD_IPV4_RuntimeTopologyLodTriangles"))) !=
            FString::Join(TriangleStrings, TEXT("|")) ||
        FString(MetaData->GetValue(
            Mesh, TEXT("TRIAD_IPV4_RuntimeTopologyLodSectionTriangles"))) !=
            NestedIntRosters(Readbacks, false) ||
        FString(MetaData->GetValue(
            Mesh, TEXT("TRIAD_IPV4_RuntimeTopologyLodComponentCounts"))) !=
            NestedIntRosters(Readbacks, true) ||
        FString(MetaData->GetValue(
            Mesh, TEXT("TRIAD_IPV4_RuntimeTopologyLodComponentDigests"))) !=
            FString::Join(DigestStrings, TEXT("|")))
    {
        OutError = TEXT("A V4 topology-preserving exact LOD readback digest/census changed: ") +
            Spec.SelectionId;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateStagingMesh(
    UStaticMesh* Mesh,
    const FExternalMeshSpec& Spec,
    FString& OutError)
{
    UAssetImportData* ImportData = Mesh ? Mesh->GetAssetImportData() : nullptr;
    TArray<FString> Sources = ImportData
        ? ImportData->ExtractFilenames()
        : TArray<FString>();
    FString Actual = Sources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(Sources[0])
        : FString();
    FString Expected = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        VegetationSourceRoot(), Spec.RelativeSourcePath));
    FPaths::NormalizeFilename(Actual);
    FPaths::NormalizeFilename(Expected);
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    if (!Mesh || Mesh->GetPathName() != StagingMeshObjectPath(Spec) ||
        !HasExactRawMeshTopology(Mesh, Spec) || Sources.Num() != 1 ||
        !FPaths::IsSamePath(Actual, Expected) ||
        !ValidateExactFile(
            Expected, Spec.SourceBytes, Spec.SourceSha256, OutError) ||
        Mesh->NaniteSettings.bEnabled ||
        (Body && (Body->AggGeom.GetElementCount() != 0 ||
                  Body->CollisionTraceFlag == CTF_UseComplexAsSimple)) ||
        Mesh->GetMinLODIdx() != 0)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A V4 editor-only raw staging mesh changed: ") +
                Spec.SelectionId;
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateRuntimeDerivative(
    UStaticMesh* Mesh,
    const FExternalMeshSpec& Spec,
    FString& OutError)
{
    if (!Mesh || Mesh->GetPathName() !=
            RuntimeMeshObjectPath(Spec.RuntimeAssetName) ||
        !Mesh->GetPathName().StartsWith(VegetationAssetPath + TEXT("/")) ||
        Mesh->NaniteSettings.bEnabled ||
        Mesh->GetMinLODIdx() != 1 ||
        Mesh->GetNumLODs() != Spec.RuntimeLodCaps.Num() ||
        ImportedSlotOrder(Mesh) != Spec.SlotOrder)
    {
        OutError = TEXT("A V4 runtime mesh derivative identity/source/slot/MinLOD changed: ") +
            Spec.SelectionId;
        return false;
    }
    const FMeshTopologyPredicateSnapshot Snapshot =
        MeshTopologyPredicateSnapshot(Mesh);
    if (!Snapshot.bHasLod0PositionBounds ||
        !FMath::IsNearlyEqual(
            Snapshot.Lod0PositionZSpanCm,
            Spec.ExpectedImportedZSpanCm,
            FMath::Max(0.10, Spec.ExpectedImportedZSpanCm * 0.001)))
    {
        TArray<FString> SlotStrings;
        for (const FName Slot : Snapshot.Slots)
        {
            SlotStrings.Add(Slot.ToString());
        }
        OutError = FString::Printf(
            TEXT("A V4 runtime LOD0 source-identity predicate changed: %s triangles=%d slots=[%s] sections=[%s] aggregateZSpanCm=%.9f lod0PositionZSpanCm=%.9f expectedLod0ZSpanCm=%.9f."),
            *Spec.SelectionId,
            Snapshot.Triangles,
            *FString::Join(SlotStrings, TEXT(",")),
            *IntRoster(Snapshot.Sections),
            Snapshot.AggregateZSpanCm,
            Snapshot.Lod0PositionZSpanCm,
            Spec.ExpectedImportedZSpanCm);
        return false;
    }
    if (!HasDerivativeProvenance(
            Mesh,
            Spec.RelativeSourcePath,
            Spec.SourceSha256,
            Spec.SelectionId))
    {
        OutError = TEXT("A V4 runtime mesh derivative provenance changed: ") +
            Spec.SelectionId;
        return false;
    }
    if (!HasRuntimeBasePolicy(Mesh, Spec, OutError))
    {
        return false;
    }
    int32 Previous = MAX_int32;
    for (int32 Lod = 0; Lod < Spec.RuntimeLodCaps.Num(); ++Lod)
    {
        const int32 Triangles = CountTriangles(Mesh, Lod);
        if (Triangles <= 0 ||
            Triangles > Spec.RuntimeLodCaps[Lod].MaximumTriangles ||
            (Lod > 0 && Triangles >= Previous))
        {
            OutError = TEXT("A V4 runtime derivative LOD cap/order changed: ") +
                Spec.SelectionId;
            return false;
        }
        Previous = Triangles;
    }
    const UBodySetup* Body = Mesh->GetBodySetup();
    if (Body && (Body->AggGeom.GetElementCount() != 0 ||
                 Body->CollisionTraceFlag == CTF_UseComplexAsSimple))
    {
        OutError = TEXT("A V4 render-only runtime derivative gained collision: ") +
            Spec.SelectionId;
        return false;
    }
    OutError.Reset();
    return true;
}

UStaticMesh* CreateUnbuiltTopologyRuntimeDerivative(
    UStaticMesh* Staging,
    const FExternalMeshSpec& Spec,
    FString& OutError)
{
    const FString PackageName =
        MeshAssetPath + TEXT("/") + Spec.RuntimeAssetName;
    const FString ExactObjectPath =
        RuntimeMeshObjectPath(Spec.RuntimeAssetName);
    if (!Staging || FindPackage(nullptr, *PackageName) ||
        FindObject<UStaticMesh>(nullptr, *ExactObjectPath) ||
        FPackageName::DoesPackageExist(PackageName))
    {
        OutError = TEXT("The exact V4 topology runtime destination already exists: ") +
            Spec.SelectionId;
        return nullptr;
    }
    FMeshDescription SourceDescription;
    if (!Staging->CloneMeshDescription(0, SourceDescription))
    {
        OutError = TEXT("Could not clone the validated V4 staging MeshDescription without building a raw runtime duplicate: ") +
            Spec.SelectionId;
        return nullptr;
    }
    UPackage* Package = CreatePackage(*PackageName);
    UStaticMesh* Runtime = Package
        ? NewObject<UStaticMesh>(
            Package,
            *Spec.RuntimeAssetName,
            RF_Public | RF_Standalone | RF_Transactional)
        : nullptr;
    if (!Runtime)
    {
        OutError = TEXT("Could not allocate the exact unbuilt V4 topology runtime asset: ") +
            Spec.SelectionId;
        return nullptr;
    }
    Runtime->AddSourceModel();
    FMeshDescription* RuntimeSource = Runtime->CreateMeshDescription(
        0, MoveTemp(SourceDescription));
    if (!RuntimeSource)
    {
        OutError = TEXT("Could not install the validated source MeshDescription into the unbuilt V4 topology runtime asset: ") +
            Spec.SelectionId;
        return nullptr;
    }
    Runtime->GetSourceModel(0).BuildSettings =
        Staging->GetSourceModel(0).BuildSettings;
    Runtime->SetStaticMaterials(Staging->GetStaticMaterials());
    Runtime->ImportVersion = Staging->ImportVersion;
    Runtime->SetLightMapCoordinateIndex(
        Staging->GetLightMapCoordinateIndex());
    Runtime->SetLightMapResolution(Staging->GetLightMapResolution());
    // Do not serialize the multi-million-triangle source here. The caller
    // clears the staging cache, then ConfigureTopologyPreservingRuntimeLods
    // filters this moved cached description to the frozen bounded LOD0 before
    // its first and only LOD0 commit.
    FAssetRegistryModule::AssetCreated(Runtime);
    Runtime->MarkPackageDirty();
    OutError.Reset();
    return Runtime;
}

bool ImportExternalMeshDerivatives(
    IAssetTools& AssetTools,
    TMap<FString, UStaticMesh*>& OutRuntimeMeshes,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    const int32 InitialCount = OutRuntimeMeshes.Num();
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    if (ExternalMeshSpecs().Num() != 7)
    {
        OutError = TEXT("The exact seven-source external V4 mesh specification roster changed.");
        return false;
    }
    for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())
    {
        if (OutRuntimeMeshes.Contains(Spec.RuntimeAssetName))
        {
            OutError = TEXT("An external V4 runtime mesh key collides with a preexisting protected/runtime reference: ") +
                Spec.RuntimeAssetName;
            return false;
        }
    }
    for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())
    {
        const FString SourceFilename = FPaths::Combine(
            VegetationSourceRoot(), Spec.RelativeSourcePath);
        if (!ValidateExactFile(
                SourceFilename,
                Spec.SourceBytes,
                Spec.SourceSha256,
                OutError))
        {
            return false;
        }
        UAssetImportTask* Task = MakeRawMeshImportTask(Spec);
        if (!Task || !ValidateRawMeshImportTask(Task, Spec, OutError))
        {
            return false;
        }
        AssetTools.ImportAssetTasks({Task});
        TArray<UStaticMesh*> Matching;
        TArray<UObject*> Unselected;
        TArray<FString> CandidateDiagnostics;
        for (UObject* Object : Task->GetObjects())
        {
            if (UStaticMesh* Mesh = Cast<UStaticMesh>(Object))
            {
                if (HasExactRawMeshTopology(Mesh, Spec))
                {
                    Matching.Add(Mesh);
                }
                else if (Mesh->GetPathName().StartsWith(
                    StagingVegetationAssetPath + TEXT("/")))
                {
                    Unselected.Add(Mesh);
                    TArray<FString> SlotNames;
                    for (const FName SlotName : ImportedSlotOrder(Mesh))
                    {
                        SlotNames.Add(SlotName.ToString());
                    }
                    TArray<FString> SectionCounts;
                    for (const int32 SectionCount :
                         CountLodTrianglesByMaterialSlot(Mesh, 0))
                    {
                        SectionCounts.Add(LexToString(SectionCount));
                    }
                    CandidateDiagnostics.Add(FString::Printf(
                        TEXT("%s triangles=%d slots=[%s] sections=[%s] zSpanCm=%.9f"),
                        *Mesh->GetPathName(),
                        CountTriangles(Mesh, 0),
                        *FString::Join(SlotNames, TEXT(",")),
                        *FString::Join(SectionCounts, TEXT(",")),
                        Mesh->GetBounds().BoxExtent.Z * 2.0));
                }
            }
        }
        if (Matching.Num() != 1)
        {
            TArray<FString> ExpectedSlotNames;
            for (const FName SlotName : Spec.SlotOrder)
            {
                ExpectedSlotNames.Add(SlotName.ToString());
            }
            TArray<FString> ExpectedSectionCounts;
            for (const int32 SectionCount : Spec.SectionTrianglesBySlot)
            {
                ExpectedSectionCounts.Add(LexToString(SectionCount));
            }
            OutError = FString::Printf(
                TEXT("Raw V4 import did not expose exactly one mesh with the exact UE5.5 imported topology tuple for source model '%s'; got %d. expected={triangles=%d slots=[%s] sections=[%s] zSpanCm=%.9f} candidates={%s}"),
                *Spec.SourceModelName,
                Matching.Num(),
                Spec.SourceTriangles,
                *FString::Join(ExpectedSlotNames, TEXT(",")),
                *FString::Join(ExpectedSectionCounts, TEXT(",")),
                Spec.ExpectedImportedZSpanCm,
                *FString::Join(CandidateDiagnostics, TEXT("; ")));
            for (UObject* Object : Unselected)
            {
                if (Object && !Object->GetPathName().StartsWith(
                    StagingVegetationAssetPath + TEXT("/")))
                {
                    OutError += TEXT(" An imported mismatch escaped the exact V4 staging namespace.");
                    return false;
                }
            }
            return false;
        }
        UStaticMesh* Staging = Matching[0];
        const FString ExactStagePath = StagingMeshObjectPath(Spec);
        if (Staging->GetPathName() != ExactStagePath)
        {
            const TArray<FAssetRenameData> Rename = {
                FAssetRenameData(
                    Staging,
                    StagingVegetationAssetPath,
                    Spec.StagingAssetName)};
            if (RegistryModule.Get().IsLoadingAssets())
            {
                OutError = TEXT("Asset Registry discovery resumed before exact V4 staging normalization; refusing to rename while discovery is active: ") +
                    Spec.SelectionId;
                return false;
            }
            if (!AssetTools.RenameAssets(Rename) ||
                Staging->GetPathName() != ExactStagePath)
            {
                OutError = TEXT("Could not normalize the selected raw staging mesh to its exact V4 path: ") +
                    Spec.SelectionId;
                return false;
            }
        }
        if (!Unselected.IsEmpty())
        {
            for (UObject* Object : Unselected)
            {
                if (!Object || !Object->GetPathName().StartsWith(
                        StagingVegetationAssetPath + TEXT("/")))
                {
                    OutError = TEXT("Raw import produced an unselected object outside the exact disposable V4 staging namespace.");
                    return false;
                }
            }
            if (ObjectTools::DeleteObjectsUnchecked(Unselected) !=
                Unselected.Num())
            {
                OutError = TEXT("Could not discard every unselected unsaved V4 raw import output.");
                return false;
            }
        }
        RemoveVisualCollision(Staging);
        if (!ValidateStagingMesh(Staging, Spec, OutError))
        {
            return false;
        }
        const bool bTopologyPreservingHeavyTree =
            Spec.RuntimeTopologySectionTriangleCapsByLod.Num() ==
                Spec.RuntimeLodCaps.Num();
        UStaticMesh* Runtime = bTopologyPreservingHeavyTree
            ? CreateUnbuiltTopologyRuntimeDerivative(Staging, Spec, OutError)
            : Cast<UStaticMesh>(AssetTools.DuplicateAsset(
                Spec.RuntimeAssetName,
                MeshAssetPath,
                Staging));
        if (!Runtime || Runtime->GetPathName() !=
                RuntimeMeshObjectPath(Spec.RuntimeAssetName))
        {
            OutError = TEXT("Could not create the exact separately owned V4 runtime derivative: ") +
                Spec.SelectionId;
            return false;
        }
        StampDerivativeProvenance(
            Runtime,
            Spec.RelativeSourcePath,
            Spec.SourceSha256,
            Spec.SelectionId);
        Staging->ClearMeshDescriptions();
        FMemory::Trim(true);
        if (bTopologyPreservingHeavyTree)
        {
            if (!ConfigureTopologyPreservingRuntimeLods(
                    Runtime, Spec, OutError))
            {
                return false;
            }
        }
        else
        {
            int32 RuntimeSourceTriangles = 0;
            if (!StampExactSourceRuntimeBasePolicy(
                    Runtime,
                    Spec,
                    RuntimeSourceTriangles,
                    OutError))
            {
                return false;
            }
            RemoveVisualCollision(Runtime);
            if (!ConfigureRuntimeDerivativeLods(
                    Runtime,
                    RuntimeSourceTriangles,
                    Spec.RuntimeLodCaps,
                    OutError))
            {
                return false;
            }
        }
        if (!ValidateRuntimeDerivative(Runtime, Spec, OutError))
        {
            return false;
        }
        OutRuntimeMeshes.Add(Spec.RuntimeAssetName, Runtime);
        OutAssetsToSave.Add(Staging);
        OutAssetsToSave.Add(Runtime);
    }
    if (OutRuntimeMeshes.Num() != InitialCount + 7)
    {
        OutError = TEXT("The exact seven externally sourced V4 runtime derivatives were not added to the preexisting runtime-reference roster.");
        return false;
    }
    OutError.Reset();
    return true;
}

TArray<FString> ExactExternalStagingObjectPaths()
{
    TArray<FString> Paths;
    for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())
    {
        Paths.Add(StagingMeshObjectPath(Spec));
    }
    Paths.Sort();
    return Paths;
}

bool FinishAndValidateExactExternalMeshRoster(
    const TMap<FString, UStaticMesh*>& RuntimeMeshes,
    const TArray<UObject*>& AssetsToSave,
    FString& OutError)
{
    TArray<UStaticMesh*> ExactExternalMeshes;
    TSet<FString> ExpectedObjectPaths;
    for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())
    {
        UStaticMesh* Staging = LoadExact<UStaticMesh>(
            StagingMeshObjectPath(Spec));
        UStaticMesh* Runtime = RuntimeMeshes.FindRef(Spec.RuntimeAssetName);
        if (!Staging || !Runtime ||
            Runtime->GetPathName() !=
                RuntimeMeshObjectPath(Spec.RuntimeAssetName))
        {
            OutError = TEXT("The exact external staging/runtime validation roster is incomplete: ") +
                Spec.SelectionId;
            return false;
        }
        ExactExternalMeshes.Add(Staging);
        ExactExternalMeshes.Add(Runtime);
        ExpectedObjectPaths.Add(Staging->GetPathName());
        ExpectedObjectPaths.Add(Runtime->GetPathName());
    }
    TSet<FString> ActualSavePaths;
    for (const UObject* Asset : AssetsToSave)
    {
        if (Asset)
        {
            ActualSavePaths.Add(Asset->GetPathName());
        }
    }
    if (RuntimeMeshes.Num() != 7 || ExternalMeshSpecs().Num() != 7 ||
        ExactExternalMeshes.Num() != 14 ||
        ExpectedObjectPaths.Num() != 14 ||
        AssetsToSave.Num() != 14 ||
        ActualSavePaths.Num() != ExpectedObjectPaths.Num() ||
        !ActualSavePaths.Includes(ExpectedObjectPaths))
    {
        OutError = TEXT("The external import phase must contain exactly seven staging/runtime pairs and no other save objects.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(
        ExactExternalMeshes);
    for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())
    {
        if (!ValidateStagingMesh(
                LoadExact<UStaticMesh>(StagingMeshObjectPath(Spec)),
                Spec,
                OutError) ||
            !ValidateRuntimeDerivative(
                RuntimeMeshes.FindRef(Spec.RuntimeAssetName),
                Spec,
                OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool SealAndUnloadExactExternalStagingPackages(
    UEditorAssetSubsystem* AssetSubsystem,
    TArray<UObject*>& InOutAssetsToSave,
    TArray<FString>& OutSealedObjectPaths,
    TArray<FProtectedFileDigest>& OutSealedPackageFiles,
    FString& OutError)
{
    OutSealedObjectPaths = ExactExternalStagingObjectPaths();
    OutSealedPackageFiles.Reset();
    if (!AssetSubsystem || OutSealedObjectPaths.Num() != 7)
    {
        OutError = TEXT("The exact seven-package staging seal has no editor asset subsystem or path roster.");
        return false;
    }
    TSet<FString> SealedPathSet;
    for (const FString& ObjectPathToSeal : OutSealedObjectPaths)
    {
        SealedPathSet.Add(ObjectPathToSeal);
    }
    TArray<UObject*> StagingAssets;
    TArray<UPackage*> StagingPackages;
    TArray<FString> StagingPackageNames;
    for (const FString& ObjectPathToSeal : OutSealedObjectPaths)
    {
        UObject* Asset = nullptr;
        for (UObject* Candidate : InOutAssetsToSave)
        {
            if (Candidate && Candidate->GetPathName() == ObjectPathToSeal)
            {
                Asset = Candidate;
                break;
            }
        }
        UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(ObjectPathToSeal);
        if (!Asset || !Package || Package->GetName() != PackageName ||
            StagingPackages.Contains(Package) ||
            StagingPackageNames.Contains(PackageName))
        {
            OutError = TEXT("The exact staging seal contains a missing, duplicate or wrong-package asset: ") +
                ObjectPathToSeal;
            return false;
        }
        StagingAssets.Add(Asset);
        StagingPackages.Add(Package);
        StagingPackageNames.Add(PackageName);
    }
    if (StagingAssets.Num() != 7 || StagingPackages.Num() != 7 ||
        StagingPackageNames.Num() != 7 ||
        !AssetSubsystem->SaveLoadedAssets(StagingAssets, false))
    {
        OutError = TEXT("The exact seven-package staging seal could not be persisted.");
        return false;
    }
    for (UPackage* Package : StagingPackages)
    {
        if (!Package || Package->IsDirty())
        {
            OutError = TEXT("A sealed V4 staging package remained dirty after its exact save.");
            return false;
        }
    }
    if (!CaptureExactPackageFileDigests(
            StagingPackageNames,
            TEXT("Sealed V4 staging"),
            OutSealedPackageFiles,
            OutError) ||
        !ValidateExactPackageFileDigests(
            OutSealedPackageFiles,
            7,
            TEXT("Sealed V4 staging"),
            OutError))
    {
        return false;
    }
    const int32 Removed = InOutAssetsToSave.RemoveAll(
        [&SealedPathSet](const UObject* Asset)
        {
            return Asset && SealedPathSet.Contains(Asset->GetPathName());
        });
    if (Removed != 7 || InOutAssetsToSave.Num() != 7)
    {
        OutError = TEXT("The exact seven sealed staging assets were not removed from the live save roster.");
        return false;
    }
    StagingAssets.Reset();
    UPackageTools::FUnloadPackageParams UnloadParams(StagingPackages);
    UnloadParams.bUnloadDirtyPackages = false;
    UnloadParams.bResetTransBuffer = true;
    if (!UPackageTools::UnloadPackages(UnloadParams))
    {
        OutError = TEXT("The exact seven clean staging packages could not be unloaded: ") +
            UnloadParams.OutErrorMessage.ToString();
        return false;
    }
    StagingPackages.Reset();
    for (const FString& ObjectPathToSeal : OutSealedObjectPaths)
    {
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(ObjectPathToSeal);
        if (FindObject<UObject>(nullptr, *ObjectPathToSeal) ||
            FindPackage(nullptr, *PackageName))
        {
            OutError = TEXT("A sealed staging object/package remained loaded before the protected/texture phases: ") +
                ObjectPathToSeal;
            return false;
        }
    }
    if (!ValidateExactPackageFileDigests(
            OutSealedPackageFiles,
            7,
            TEXT("Sealed V4 staging"),
            OutError))
    {
        return false;
    }
    FMemory::Trim(true);
    OutError.Reset();
    return true;
}

struct FProtectedDerivativeSpec
{
    FString SelectionId;
    FString SourceObjectPath;
    int64 SourcePackageBytes = 0;
    FString SourcePackageSha256;
    FString RuntimeAssetName;
    int32 SourceLodCount = 4;
    TArray<FName> SlotOrder;
    TArray<int32> Lod0TrianglesBySlot;
    double SourceZSpanCm = 0.0;
    TArray<int32> LodTriangleCaps;
};

const TArray<FProtectedDerivativeSpec>& ProtectedDerivativeSpecs()
{
    static const TArray<FProtectedDerivativeSpec> Specs = {
        {TEXT("high_fork_tree"), ProtectedHighForkSourceObjectPath, 49124278, TEXT("17F3441CA25A2B0DEEE0C4B0CD8B525E1A18D61E342441A8532ADC8B8D872A9E"), HighForkMeshName, 4, {TEXT("island_tree_01"), TEXT("island_tree_01_leaves"), TEXT("island_tree_01_branches")}, {34787, 1060032, 504584}, 502.744660153985, {1599403, 150000, 60000, 20000}},
        {TEXT("columnar_tree"), ProtectedColumnarSourceObjectPath, 119724142, TEXT("5839F778FDBD3CB9B79C34E410D6A112FAFF64518761F600039B0BBEBB81E6F1"), ColumnarMeshName, 4, {TEXT("jacaranda_tree_branches"), TEXT("jacaranda_tree_trunk"), TEXT("jacaranda_tree_leaves")}, {1231286, 230112, 2402434}, 1946.8875035643578, {3863832, 350000, 120000, 40000}}};
    return Specs;
}

bool ValidateProtectedDerivativeContractRows(
    const TSharedPtr<FJsonObject>& Contract,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>& Rows =
        Contract->GetArrayField(TEXT("selectedMeshDerivatives"));
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        TSharedPtr<FJsonObject> Match;
        int32 Matches = 0;
        for (const TSharedPtr<FJsonValue>& Value : Rows)
        {
            const TSharedPtr<FJsonObject> Row = Value ? Value->AsObject() : nullptr;
            if (Row && Row->GetStringField(TEXT("selectionId")) == Spec.SelectionId)
            {
                Match = Row;
                ++Matches;
            }
        }
        if (Matches != 1 || !Match ||
            Match->GetStringField(TEXT("sourceKind")) !=
                TEXT("PROTECTED_INHERITED_UASSET_EXACT_READ_ONLY_REFERENCE"))
        {
            OutError = TEXT("A protected exact read-only reference row is absent: ") +
                Spec.SelectionId;
            return false;
        }
        const TSharedPtr<FJsonObject> Source =
            Match->GetObjectField(TEXT("sourceMesh"));
        const TArray<TSharedPtr<FJsonValue>>& Slots =
            Match->GetArrayField(TEXT("materialSlotOrder"));
        const TSharedPtr<FJsonObject> Caps =
            Match->GetObjectField(TEXT("runtimeTriangleCaps"));
        const TSharedPtr<FJsonValue>* RuntimeDerivedAsset =
            Match->Values.Find(TEXT("runtimeDerivedAsset"));
        if (Source->GetStringField(TEXT("objectPath")) != Spec.SourceObjectPath ||
            static_cast<int64>(Source->GetNumberField(TEXT("bytes"))) !=
                Spec.SourcePackageBytes ||
            !Source->GetStringField(TEXT("sha256")).Equals(
                Spec.SourcePackageSha256, ESearchCase::IgnoreCase) ||
            Source->GetIntegerField(TEXT("sourceLodCount")) != Spec.SourceLodCount ||
            Source->GetIntegerField(TEXT("sourceMinLOD")) != 1 ||
            Match->GetIntegerField(TEXT("sourceTriangles")) !=
                Spec.LodTriangleCaps[0] ||
            Slots.Num() != Spec.SlotOrder.Num() ||
            !FMath::IsNearlyEqual(
                Match->GetNumberField(TEXT("expectedUnscaledImportedZSpanCm")),
                Spec.SourceZSpanCm,
                0.0000001) ||
            Match->GetStringField(TEXT("sourceImportPolicyId")) !=
                TEXT("PROTECTED_UASSET_DIRECT_REFERENCE_NO_IMPORT_OR_DUPLICATION") ||
            Match->GetStringField(TEXT("referenceMode")) !=
                TEXT("DIRECT_READ_ONLY_USTATICMESH_REFERENCE_WITH_COMPONENT_MATERIAL_OVERRIDES") ||
            Match->GetStringField(TEXT("runtimeMeshReference")) !=
                Spec.SourceObjectPath ||
            !RuntimeDerivedAsset || !RuntimeDerivedAsset->IsValid() ||
            (*RuntimeDerivedAsset)->Type != EJson::Null ||
            Match->GetBoolField(TEXT("rawSourceRuntimeSelected")) ||
            Match->GetIntegerField(TEXT("runtimeRequiredMinLOD")) != 1 ||
            !Match->GetBoolField(
                TEXT("protectedSourceUassetReferencedAtRuntime")) ||
            Match->GetBoolField(TEXT("sourceMeshMaterialMutationAllowed")) ||
            !Match->GetBoolField(TEXT("componentMaterialOverridesRequired")) ||
            Match->GetBoolField(TEXT("sourcePackageMayBeSavedByV4Workflow")) ||
            !Match->GetBoolField(TEXT("sourceProtectedBytesMustRemainUnchanged")))
        {
            OutError = TEXT("A protected V4 read-only source/path/truth row changed: ") +
                Spec.SelectionId;
            return false;
        }
        for (int32 Index = 0; Index < Slots.Num(); ++Index)
        {
            if (Slots[Index]->AsString() != Spec.SlotOrder[Index].ToString())
            {
                OutError = TEXT("A protected V4 derivative slot order changed: ") +
                    Spec.SelectionId;
                return false;
            }
        }
        for (int32 Lod = 0; Lod < Spec.LodTriangleCaps.Num(); ++Lod)
        {
            if (Caps->GetIntegerField(*FString::Printf(TEXT("LOD%d"), Lod)) !=
                Spec.LodTriangleCaps[Lod])
            {
                OutError = TEXT("A protected V4 derivative LOD cap changed: ") +
                    Spec.SelectionId;
                return false;
            }
        }
        const TSharedPtr<FJsonObject> Prebase =
            Match->GetObjectField(TEXT("runtimePreReductionBase"));
        const TSharedPtr<FJsonObject> ComponentPolicy =
            Prebase->GetObjectField(TEXT("componentPolicy"));
        if (!Prebase || !ComponentPolicy ||
            Prebase->GetStringField(TEXT("algorithmVersion")) !=
                TEXT("EXACT_SOURCE_NO_PRE_REDUCTION_V1") ||
            Prebase->GetBoolField(TEXT("applied")) ||
            Prebase->GetIntegerField(TEXT("prebaseTriangleCap")) !=
                Spec.LodTriangleCaps[0] ||
            ComponentPolicy->GetStringField(TEXT("retentionUnit")) !=
                TEXT("ENTIRE_EXACT_SOURCE_MESHDESCRIPTION") ||
            ComponentPolicy->GetBoolField(TEXT("partialComponentRetentionAllowed")) ||
            !Prebase->GetBoolField(TEXT("noNewCutBoundaries")) ||
            !JsonIntRosterEquals(
                Prebase->GetObjectField(TEXT("sectionTriangleTargetsMaximum")),
                Spec.Lod0TrianglesBySlot) ||
            !JsonIntRosterEquals(
                Prebase->GetObjectField(
                    TEXT("expectedRawReplayRetainedSectionTrianglesBySlot")),
                Spec.Lod0TrianglesBySlot) ||
            Prebase->GetIntegerField(TEXT("expectedRawReplayRetainedTriangles")) !=
                Spec.LodTriangleCaps[0] ||
            Prebase->GetStringField(TEXT("canonicalRetainedComponentRoster")) !=
                TEXT("ENTIRE_EXACT_SOURCE_IN_IMPORTED_MESHDESCRIPTION_ORDER"))
        {
            OutError = TEXT("A protected V4 exact-source no-pre-reduction contract changed: ") +
                Spec.SelectionId;
            return false;
        }
        const TSharedPtr<FJsonValue>* NativeGirth =
            Match->Values.Find(TEXT("sourceNativeBiologicalGirthMeters"));
        if (!NativeGirth || !NativeGirth->IsValid() ||
            (*NativeGirth)->Type != EJson::Null)
        {
            OutError = TEXT("Protected proxy source must retain explicit null biological girth: ") +
                Spec.SelectionId;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ResolvePackageFileAndValidate(
    const FString& ObjectPath,
    int64 ExpectedBytes,
    const FString& ExpectedSha,
    FString& OutFilename,
    FString& OutError)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
    if (!FPackageName::DoesPackageExist(PackageName, &OutFilename) ||
        !ValidateExactFile(
            OutFilename, ExpectedBytes, ExpectedSha, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A protected source package is absent: ") + PackageName;
        }
        return false;
    }
    return true;
}

bool UnloadOneExactCleanPackage(
    const FString& ObjectPath,
    UPackage* Package,
    bool bResetTransBuffer,
    const FString& Label,
    FString& OutError)
{
    const FString PackageName =
        FPackageName::ObjectPathToPackageName(ObjectPath);
    if (!Package || PackageName.IsEmpty() ||
        Package->GetName() != PackageName || Package->IsDirty())
    {
        OutError = Label +
            TEXT(" exact package is absent, misresolved or dirty; it was not unloaded: ") +
            PackageName;
        return false;
    }
    TArray<UPackage*> OnePackage = {Package};
    UPackageTools::FUnloadPackageParams UnloadParams(OnePackage);
    UnloadParams.bUnloadDirtyPackages = false;
    UnloadParams.bResetTransBuffer = bResetTransBuffer;
    if (!UPackageTools::UnloadPackages(UnloadParams) ||
        FindObject<UObject>(nullptr, *ObjectPath) ||
        FindPackage(nullptr, *PackageName))
    {
        OutError = Label + TEXT(" exact package did not fully unload: ") +
            PackageName + TEXT(" ") +
            UnloadParams.OutErrorMessage.ToString();
        return false;
    }
    FMemory::Trim(true);
    OutError.Reset();
    return true;
}

bool CaptureV3WorldSnapshotForV4Validation(
    FV3WorldSnapshot& OutSnapshot,
    FString& OutError)
{
    UWorld* SourceWorld = LoadExact<UWorld>(SourceMapObjectPath);
    UWorld* CurrentEditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    const bool bSourceIsCurrent = SourceWorld &&
        SourceWorld == CurrentEditorWorld;
    UPackage* SourcePackage = SourceWorld
        ? SourceWorld->GetOutermost()
        : FindPackage(nullptr, *SourceMapPackage);
    FString ValidationError;
    const bool bSnapshotValid =
        ValidateExactV3SourceWorld(SourceWorld, ValidationError) &&
        CaptureV3WorldSnapshot(SourceWorld, OutSnapshot, ValidationError);
    if (bSourceIsCurrent)
    {
        OutError = ValidationError;
        return bSnapshotValid;
    }

    SourceWorld = nullptr;
    FString UnloadError;
    const bool bUnloaded = !SourcePackage ||
        UnloadOneExactCleanPackage(
            SourceMapObjectPath,
            SourcePackage,
            false,
            TEXT("V4 validation inactive V3 snapshot source"),
            UnloadError);
    if (!bSnapshotValid || !bUnloaded)
    {
        OutError = ValidationError;
        if (!bUnloaded)
        {
            if (!OutError.IsEmpty())
            {
                OutError += TEXT(" ");
            }
            OutError += TEXT("sourceUnload=") + UnloadError;
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateStaticMeshDerivedDataRecord(
    const FString& DerivedDataKey,
    const FString& ObjectPath,
    FString& OutMeshDataRawHash,
    int64& OutMeshDataRawSize,
    FString& OutError)
{
    using namespace UE::DerivedData;
    OutMeshDataRawHash.Reset();
    OutMeshDataRawSize = 0;
    if (DerivedDataKey.IsEmpty() || ObjectPath.IsEmpty())
    {
        OutError = TEXT("A protected prewarm receipt has an empty StaticMesh DDC key or object path.");
        return false;
    }
    FCacheKey CacheKey;
    CacheKey.Bucket = FCacheBucket(TEXT("StaticMesh"));
    CacheKey.Hash = FIoHash::HashBuffer(
        MakeMemoryView(FTCHARToUTF8(DerivedDataKey)));
    FCacheGetRequest Request;
    Request.Name = UE::FSharedString(ObjectPath);
    Request.Key = CacheKey;
    Request.Policy = FCacheRecordPolicy(
        ECachePolicy::QueryLocal | ECachePolicy::SkipData |
        ECachePolicy::KeepAlive);
    static const FValueId MeshDataId = FValueId::FromName("MeshData");
    bool bExactRecordExists = false;
    FRequestOwner RequestOwner(EPriority::Blocking);
    GetCache().Get(
        MakeArrayView(&Request, 1),
        RequestOwner,
        [
            &bExactRecordExists,
            &OutMeshDataRawHash,
            &OutMeshDataRawSize](FCacheGetResponse&& Response)
        {
            if (Response.Status == EStatus::Ok)
            {
                const FValueWithId& MeshData =
                    Response.Record.GetValue(MeshDataId);
                const uint64 RawSize = MeshData.GetRawSize();
                bExactRecordExists = MeshData.IsValid() && RawSize > 0 &&
                    RawSize <= static_cast<uint64>(MAX_int64);
                if (bExactRecordExists)
                {
                    OutMeshDataRawHash = LexToString(
                        MeshData.GetRawHash()).ToUpper();
                    OutMeshDataRawSize = static_cast<int64>(RawSize);
                    bExactRecordExists =
                        OutMeshDataRawHash.Len() == 40;
                }
            }
        });
    RequestOwner.Wait();
    if (!bExactRecordExists)
    {
        OutError = TEXT("The exact protected StaticMesh structured-DDC record is absent: ") +
            ObjectPath;
        return false;
    }
    OutError.Reset();
    return true;
}

struct FProtectedPrewarmDerivedDataProof
{
    FString RenderDerivedDataKey;
    FString SecondaryMode;
    FString DistanceFieldDerivedDataKey;
    FString DistanceFieldRawHash;
    int64 DistanceFieldRawSize = 0;
    FString CardDerivedDataKey;
    FString CardRawHash;
    int64 CardRawSize = 0;
};

bool ValidateLegacyDerivedDataValueRecord(
    const FString& LegacyDerivedDataKey,
    const FString& ObjectPath,
    const FString& Label,
    FString& OutRawHash,
    int64& OutRawSize,
    FString& OutError)
{
    using namespace UE::DerivedData;
    OutRawHash.Reset();
    OutRawSize = 0;
    if (LegacyDerivedDataKey.IsEmpty() || ObjectPath.IsEmpty())
    {
        OutError = Label + TEXT(" has an empty legacy DDC key or object path.");
        return false;
    }
    FCacheGetValueRequest Request;
    Request.Name = UE::FSharedString(ObjectPath);
    Request.Key = ConvertLegacyCacheKey(LegacyDerivedDataKey);
    Request.Policy = ECachePolicy::QueryLocal | ECachePolicy::SkipData |
        ECachePolicy::KeepAlive;
    bool bExactValueExists = false;
    FRequestOwner RequestOwner(EPriority::Blocking);
    GetCache().GetValue(
        MakeArrayView(&Request, 1),
        RequestOwner,
        [
            &bExactValueExists,
            &OutRawHash,
            &OutRawSize](FCacheGetValueResponse&& Response)
        {
            const uint64 RawSize = Response.Value.GetRawSize();
            bExactValueExists = Response.Status == EStatus::Ok &&
                !Response.Value.GetRawHash().IsZero() && RawSize > 0 &&
                RawSize <= static_cast<uint64>(MAX_int64);
            if (bExactValueExists)
            {
                OutRawHash = LexToString(
                    Response.Value.GetRawHash()).ToUpper();
                OutRawSize = static_cast<int64>(RawSize);
                bExactValueExists = OutRawHash.Len() == 40;
            }
        });
    RequestOwner.Wait();
    if (!bExactValueExists)
    {
        OutError = Label + TEXT(" exact local legacy-DDC value is absent: ") +
            ObjectPath;
        return false;
    }
    OutError.Reset();
    return true;
}

bool BuildProtectedSecondaryDerivedDataProof(
    UStaticMesh* Source,
    const FString& RenderDerivedDataKey,
    FProtectedPrewarmDerivedDataProof& OutProof,
    FString& OutError)
{
    OutProof = FProtectedPrewarmDerivedDataProof();
    OutProof.RenderDerivedDataKey = RenderDerivedDataKey;
    IConsoleVariable* GenerateDistanceFields =
        IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.GenerateMeshDistanceFields"));
    IConsoleVariable* GenerateCards =
        IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.MeshCardRepresentation"));
    IConsoleVariable* ForceNaniteMeshes =
        IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.Nanite.ForceEnableMeshes"));
    if (!Source || RenderDerivedDataKey.IsEmpty() ||
        Source->GetNumSourceModels() <= 0 || !Source->GetRenderData() ||
        !Source->GetRenderData()->LODResources.IsValidIndex(0) ||
        !GenerateDistanceFields || !GenerateCards || !ForceNaniteMeshes ||
        ForceNaniteMeshes->GetInt() != 0 ||
        Source->IsNaniteLandscape() || Source->IsNaniteEnabled() ||
        !Source->LODGroup.IsNone())
    {
        OutError = TEXT("A protected mesh cannot produce the exact supported NAME_None-LOD-group secondary StaticMesh DDC proof.");
        return false;
    }
    const bool bDistanceField = GenerateDistanceFields->GetInt() != 0 ||
        Source->bGenerateMeshDistanceField;
    const bool bCard = GenerateCards->GetInt() == 1;
    if (bDistanceField || !bCard || MeshCardRepresentation::IsDebugMode())
    {
        OutError = TEXT("Protected prewarm supports only the exact bounded Card-only secondary-build state: r.GenerateMeshDistanceFields=0, source bGenerateMeshDistanceField=false, r.MeshCardRepresentation=1 and Mesh Card debug mode off.");
        return false;
    }
    OutProof.SecondaryMode = TEXT("Card");
    const FMeshBuildSettings& BuildSettings =
        Source->GetSourceModel(0).BuildSettings;
    OutProof.CardDerivedDataKey =
        FDerivedDataCacheInterface::BuildCacheKey(
            TEXT("CARD"),
            *FString::Printf(
                TEXT("%s_7DD7930F-6ED7-4CF1-BE60-E9819779DBAF%.3f_%.3f_%d"),
                *RenderDerivedDataKey,
                MeshCardRepresentation::GetMinDensity(),
                MeshCardRepresentation::GetNormalTreshold(),
                BuildSettings.MaxLumenMeshCards),
            TEXT(""));
    OutError.Reset();
    return true;
}

bool ValidateProtectedSecondaryDerivedDataProof(
    FProtectedPrewarmDerivedDataProof& InOutProof,
    const FString& ObjectPath,
    FString& OutError)
{
    if (InOutProof.SecondaryMode != TEXT("Card") ||
        !InOutProof.DistanceFieldDerivedDataKey.IsEmpty() ||
        !InOutProof.DistanceFieldRawHash.IsEmpty() ||
        InOutProof.DistanceFieldRawSize != 0 ||
        InOutProof.CardDerivedDataKey.IsEmpty())
    {
        OutError = TEXT("A protected prewarm secondary DDC proof mode/key roster is inconsistent: ") +
            ObjectPath;
        return false;
    }
    if (!ValidateLegacyDerivedDataValueRecord(
            InOutProof.CardDerivedDataKey,
            ObjectPath,
            TEXT("Mesh-card"),
            InOutProof.CardRawHash,
            InOutProof.CardRawSize,
            OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedMeshTopology(
    UStaticMesh* Mesh,
    const FProtectedDerivativeSpec& Spec,
    FString& OutError)
{
    if (!Mesh || Mesh->GetPathName() != Spec.SourceObjectPath ||
        Mesh->GetNumLODs() != Spec.SourceLodCount || Mesh->GetMinLODIdx() < 1 ||
        ImportedSlotOrder(Mesh) != Spec.SlotOrder ||
        CountLodTrianglesByMaterialSlot(Mesh, 0) != Spec.Lod0TrianglesBySlot ||
        !FMath::IsNearlyEqual(
            Mesh->GetBounds().BoxExtent.Z * 2.0,
            Spec.SourceZSpanCm,
            0.05))
    {
        OutError = TEXT("A protected inherited V4 proxy source topology changed: ") +
            Spec.SelectionId;
        return false;
    }
    for (int32 Lod = 0; Lod < Spec.SourceLodCount; ++Lod)
    {
        const int32 Triangles = CountTriangles(Mesh, Lod);
        if (Triangles <= 0 || Triangles > Spec.LodTriangleCaps[Lod])
        {
            OutError = TEXT("A protected inherited V4 proxy source LOD cap changed: ") +
                Spec.SelectionId;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool PrewarmAndUnloadProtectedMeshReferences(
    const TArray<FProtectedFileDigest>& ProtectedSnapshot,
    TMap<FString, FProtectedPrewarmDerivedDataProof>& OutDerivedDataProofs,
    FString& OutError)
{
    OutDerivedDataProofs.Reset();
    if (ProtectedDerivativeSpecs().Num() != 2)
    {
        OutError = TEXT("Protected mesh prewarm requires exactly two read-only source specifications.");
        return false;
    }
    int32 PrewarmedCount = 0;
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(Spec.SourceObjectPath);
        if (FindObject<UStaticMesh>(nullptr, *Spec.SourceObjectPath) ||
            FindPackage(nullptr, *PackageName))
        {
            OutError = TEXT("Protected mesh prewarm requires each exact source package to begin unloaded at the clean Entry map: ") +
                Spec.SelectionId;
            return false;
        }
        FString PackageFilename;
        if (!ResolvePackageFileAndValidate(
                Spec.SourceObjectPath,
                Spec.SourcePackageBytes,
                Spec.SourcePackageSha256,
                PackageFilename,
                OutError))
        {
            return false;
        }
        UStaticMesh* Source = LoadExact<UStaticMesh>(Spec.SourceObjectPath);
        const auto ReleaseAndRevalidate =
            [&Source, &Spec, &PackageName](
                const FString& Failure,
                FString& Error) -> bool
        {
            UPackage* SourcePackage = Source
                ? Source->GetOutermost()
                : FindPackage(nullptr, *PackageName);
            if (Source)
            {
                Source->ClearMeshDescriptions();
            }
            Source = nullptr;
            FString ReleaseError;
            const bool bReleased = !SourcePackage ||
                UnloadOneExactCleanPackage(
                    Spec.SourceObjectPath,
                    SourcePackage,
                    false,
                    TEXT("Protected prewarm ") + Spec.SelectionId,
                    ReleaseError);
            FString RevalidatedFilename;
            FString HashError;
            const bool bHashExact = ResolvePackageFileAndValidate(
                Spec.SourceObjectPath,
                Spec.SourcePackageBytes,
                Spec.SourcePackageSha256,
                RevalidatedFilename,
                HashError);
            if (!Failure.IsEmpty() || !bReleased || !bHashExact)
            {
                Error = Failure;
                if (!bReleased)
                {
                    if (!Error.IsEmpty())
                    {
                        Error += TEXT(" ");
                    }
                    Error += TEXT("release=") + ReleaseError;
                }
                if (!bHashExact)
                {
                    if (!Error.IsEmpty())
                    {
                        Error += TEXT(" ");
                    }
                    Error += TEXT("protectedSizeSha=") + HashError;
                }
                return false;
            }
            Error.Reset();
            return true;
        };
        if (!Source)
        {
            return ReleaseAndRevalidate(
                TEXT("Protected mesh prewarm could not load one exact hash-validated source package: ") +
                    Spec.SelectionId,
                OutError);
        }
        FStaticMeshCompilingManager::Get().FinishCompilation({Source});
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (GDistanceFieldAsyncQueue)
        {
            GDistanceFieldAsyncQueue->BlockUntilBuildComplete(Source, true);
        }
        if (GCardRepresentationAsyncQueue)
        {
            GCardRepresentationAsyncQueue->BlockUntilBuildComplete(
                Source, true);
        }
        FAssetCompilingManager::Get().FinishAllCompilation();
        GetDerivedDataCacheRef().WaitForQuiescence(false);
        if (!ValidateProtectedMeshTopology(Source, Spec, OutError))
        {
            const FString Failure = OutError;
            return ReleaseAndRevalidate(Failure, OutError);
        }
        const FStaticMeshRenderData* RenderData = Source->GetRenderData();
        const FString DerivedDataKey = RenderData
            ? RenderData->DerivedDataKey
            : FString();
        FString MeshDataRawHashBeforeUnload;
        int64 MeshDataRawSizeBeforeUnload = 0;
        if (!ValidateStaticMeshDerivedDataRecord(
                DerivedDataKey,
                Spec.SourceObjectPath,
                MeshDataRawHashBeforeUnload,
                MeshDataRawSizeBeforeUnload,
                OutError))
        {
            const FString Failure = OutError;
            return ReleaseAndRevalidate(Failure, OutError);
        }
        FProtectedPrewarmDerivedDataProof DerivedDataProof;
        if (!BuildProtectedSecondaryDerivedDataProof(
                Source,
                DerivedDataKey,
                DerivedDataProof,
                OutError) ||
            !ValidateProtectedSecondaryDerivedDataProof(
                DerivedDataProof,
                Spec.SourceObjectPath,
                OutError))
        {
            const FString Failure = OutError;
            return ReleaseAndRevalidate(Failure, OutError);
        }
        if (!ReleaseAndRevalidate(FString(), OutError))
        {
            return false;
        }
        FString MeshDataRawHashAfterUnload;
        int64 MeshDataRawSizeAfterUnload = 0;
        FProtectedPrewarmDerivedDataProof ProofAfterUnload =
            DerivedDataProof;
        if (!ValidateStaticMeshDerivedDataRecord(
                DerivedDataKey,
                Spec.SourceObjectPath,
                MeshDataRawHashAfterUnload,
                MeshDataRawSizeAfterUnload,
                OutError) ||
            !ValidateProtectedSecondaryDerivedDataProof(
                ProofAfterUnload,
                Spec.SourceObjectPath,
                OutError) ||
            MeshDataRawHashAfterUnload != MeshDataRawHashBeforeUnload ||
            MeshDataRawSizeAfterUnload != MeshDataRawSizeBeforeUnload ||
            ProofAfterUnload.DistanceFieldRawHash !=
                DerivedDataProof.DistanceFieldRawHash ||
            ProofAfterUnload.DistanceFieldRawSize !=
                DerivedDataProof.DistanceFieldRawSize ||
            ProofAfterUnload.CardRawHash != DerivedDataProof.CardRawHash ||
            ProofAfterUnload.CardRawSize != DerivedDataProof.CardRawSize ||
            OutDerivedDataProofs.Contains(Spec.SourceObjectPath))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A protected prewarm local-DDC MeshData/CARD/DIST record changed or its receipt key was duplicated: ") +
                    Spec.SourceObjectPath;
            }
            return false;
        }
        OutDerivedDataProofs.Add(
            Spec.SourceObjectPath, MoveTemp(DerivedDataProof));
        ++PrewarmedCount;
    }
    if (PrewarmedCount != 2 ||
        OutDerivedDataProofs.Num() != 2 ||
        !ValidateProtectedPackages(ProtectedSnapshot, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

struct FProtectedPrewarmReceiptFile
{
    FString Path;
    int64 Bytes = 0;
    FString Sha256;
};

struct FProtectedPrewarmReceiptEnvironmentRow
{
    FString Name;
    FString ValueSha256;
};

bool IsSafeStaticMeshEnvironmentRowName(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() > 512)
    {
        return false;
    }
    for (const TCHAR Character : Name)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('.') &&
            Character != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}

struct FProtectedPrewarmReceiptMesh
{
    FString ObjectPath;
    FString PackageName;
    FString PackageFile;
    int64 PackageBytes = 0;
    FString PackageSha256;
    FString RenderDerivedDataKey;
    FString MeshDataRawHash;
    int64 MeshDataRawSize = 0;
    FString SecondaryMode;
    FString DistanceFieldDerivedDataKey;
    FString DistanceFieldRawHash;
    int64 DistanceFieldRawSize = 0;
    FString CardDerivedDataKey;
    FString CardRawHash;
    int64 CardRawSize = 0;
};

struct FProtectedPrewarmReceipt
{
    FString Schema;
    FString ReceiptId;
    FString CreatedUtc;
    FString ProducerProcessId;
    FString ProducerAppInstanceId;
    FProtectedPrewarmReceiptFile ProjectFile;
    FProtectedPrewarmReceiptFile EditorModule;
    FProtectedPrewarmReceiptFile EngineBuildVersionFile;
    FString EngineVersion;
    FString EngineChangelist;
    FProtectedPrewarmReceiptFile VegetationContract;
    int32 ProtectedSnapshotRecordCount = 0;
    FString ProtectedSnapshotSha256;
    FString DdcGraphName;
    TArray<FString> DdcDirectories;
    FString DdcZenIdentitySha256;
    int32 StaticMeshKeyEnvironmentRecordCount = 0;
    FString StaticMeshKeyEnvironmentSha256;
    TArray<FProtectedPrewarmReceiptEnvironmentRow>
        StaticMeshKeyEnvironmentRows;
    TArray<FProtectedPrewarmReceiptMesh> ProtectedMeshes;
    int32 V4AssetCountAtIssue = -1;
    FString BindingSha256;
};

bool CompareStaticMeshKeyEnvironmentRows(
    const TArray<FProtectedPrewarmReceiptEnvironmentRow>& Expected,
    const TArray<FProtectedPrewarmReceiptEnvironmentRow>& Actual,
    FString& OutDifference)
{
    constexpr int32 MaxReportedDifferences = 8;
    TArray<FString> ReportedDifferences;
    int32 DifferenceCount = 0;
    int32 ExpectedIndex = 0;
    int32 ActualIndex = 0;
    const auto Report =
        [&ReportedDifferences, &DifferenceCount](const FString& Detail)
    {
        ++DifferenceCount;
        if (ReportedDifferences.Num() < MaxReportedDifferences)
        {
            ReportedDifferences.Add(Detail);
        }
    };
    while (ExpectedIndex < Expected.Num() || ActualIndex < Actual.Num())
    {
        if (ExpectedIndex >= Expected.Num())
        {
            const FProtectedPrewarmReceiptEnvironmentRow& Row =
                Actual[ActualIndex++];
            Report(FString::Printf(
                TEXT("currentOnly[%s]=%s"),
                *Row.Name,
                *Row.ValueSha256));
            continue;
        }
        if (ActualIndex >= Actual.Num())
        {
            const FProtectedPrewarmReceiptEnvironmentRow& Row =
                Expected[ExpectedIndex++];
            Report(FString::Printf(
                TEXT("producerOnly[%s]=%s"),
                *Row.Name,
                *Row.ValueSha256));
            continue;
        }
        const FProtectedPrewarmReceiptEnvironmentRow& ExpectedRow =
            Expected[ExpectedIndex];
        const FProtectedPrewarmReceiptEnvironmentRow& ActualRow =
            Actual[ActualIndex];
        const int32 NameComparison =
            ExpectedRow.Name.Compare(ActualRow.Name);
        if (NameComparison < 0)
        {
            Report(FString::Printf(
                TEXT("producerOnly[%s]=%s"),
                *ExpectedRow.Name,
                *ExpectedRow.ValueSha256));
            ++ExpectedIndex;
        }
        else if (NameComparison > 0)
        {
            Report(FString::Printf(
                TEXT("currentOnly[%s]=%s"),
                *ActualRow.Name,
                *ActualRow.ValueSha256));
            ++ActualIndex;
        }
        else
        {
            if (ExpectedRow.ValueSha256 != ActualRow.ValueSha256)
            {
                Report(FString::Printf(
                    TEXT("changed[%s]{producerSha=%s,currentSha=%s}"),
                    *ExpectedRow.Name,
                    *ExpectedRow.ValueSha256,
                    *ActualRow.ValueSha256));
            }
            ++ExpectedIndex;
            ++ActualIndex;
        }
    }
    if (DifferenceCount == 0)
    {
        OutDifference.Reset();
        return true;
    }
    OutDifference = FString::Join(ReportedDifferences, TEXT(";"));
    if (DifferenceCount > ReportedDifferences.Num())
    {
        OutDifference += FString::Printf(
            TEXT(";and%dMore"),
            DifferenceCount - ReportedDifferences.Num());
    }
    return false;
}

FString ProtectedPrewarmReceiptFilename()
{
    FString Path = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaExploreV4/ProtectedPrewarmReceipt.v1.json"));
    Path = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Path);
    return Path;
}

FString NormalizeReceiptPath(const FString& InPath)
{
    FString Path = FPaths::ConvertRelativePathToFull(InPath);
    FPaths::NormalizeFilename(Path);
    return Path;
}

void AppendReceiptBindingField(
    FString& InOutCanonical,
    const FString& Name,
    const FString& Value)
{
    InOutCanonical += FString::Printf(
        TEXT("%d:%s=%d:%s\n"),
        Name.Len(),
        *Name,
        Value.Len(),
        *Value);
}

FString BuildProtectedPrewarmReceiptCanonical(
    const FProtectedPrewarmReceipt& Receipt)
{
    FString Canonical;
    AppendReceiptBindingField(Canonical, TEXT("schema"), Receipt.Schema);
    AppendReceiptBindingField(Canonical, TEXT("receiptId"), Receipt.ReceiptId);
    AppendReceiptBindingField(Canonical, TEXT("createdUtc"), Receipt.CreatedUtc);
    AppendReceiptBindingField(
        Canonical, TEXT("producerProcessId"), Receipt.ProducerProcessId);
    AppendReceiptBindingField(
        Canonical,
        TEXT("producerAppInstanceId"),
        Receipt.ProducerAppInstanceId);
    const auto AppendFile = [&Canonical](
        const FString& Prefix,
        const FProtectedPrewarmReceiptFile& File)
    {
        AppendReceiptBindingField(Canonical, Prefix + TEXT(".path"), File.Path);
        AppendReceiptBindingField(
            Canonical, Prefix + TEXT(".bytes"), LexToString(File.Bytes));
        AppendReceiptBindingField(
            Canonical, Prefix + TEXT(".sha256"), File.Sha256);
    };
    AppendFile(TEXT("projectFile"), Receipt.ProjectFile);
    AppendFile(TEXT("editorModule"), Receipt.EditorModule);
    AppendFile(
        TEXT("engineBuildVersionFile"),
        Receipt.EngineBuildVersionFile);
    AppendReceiptBindingField(
        Canonical, TEXT("engineVersion"), Receipt.EngineVersion);
    AppendReceiptBindingField(
        Canonical, TEXT("engineChangelist"), Receipt.EngineChangelist);
    AppendFile(TEXT("vegetationContract"), Receipt.VegetationContract);
    AppendReceiptBindingField(
        Canonical,
        TEXT("protectedSnapshot.recordCount"),
        LexToString(Receipt.ProtectedSnapshotRecordCount));
    AppendReceiptBindingField(
        Canonical,
        TEXT("protectedSnapshot.sha256"),
        Receipt.ProtectedSnapshotSha256);
    AppendReceiptBindingField(
        Canonical, TEXT("ddcZen.graphName"), Receipt.DdcGraphName);
    AppendReceiptBindingField(
        Canonical,
        TEXT("ddcZen.directoryCount"),
        LexToString(Receipt.DdcDirectories.Num()));
    for (int32 Index = 0; Index < Receipt.DdcDirectories.Num(); ++Index)
    {
        AppendReceiptBindingField(
            Canonical,
            FString::Printf(TEXT("ddcZen.directory.%d"), Index),
            Receipt.DdcDirectories[Index]);
    }
    AppendReceiptBindingField(
        Canonical,
        TEXT("ddcZen.identitySha256"),
        Receipt.DdcZenIdentitySha256);
    AppendReceiptBindingField(
        Canonical,
        TEXT("staticMeshKeyEnvironment.recordCount"),
        LexToString(Receipt.StaticMeshKeyEnvironmentRecordCount));
    AppendReceiptBindingField(
        Canonical,
        TEXT("staticMeshKeyEnvironment.sha256"),
        Receipt.StaticMeshKeyEnvironmentSha256);
    AppendReceiptBindingField(
        Canonical,
        TEXT("staticMeshKeyEnvironment.rowDigestCount"),
        LexToString(Receipt.StaticMeshKeyEnvironmentRows.Num()));
    for (int32 Index = 0;
         Index < Receipt.StaticMeshKeyEnvironmentRows.Num();
         ++Index)
    {
        const FProtectedPrewarmReceiptEnvironmentRow& Row =
            Receipt.StaticMeshKeyEnvironmentRows[Index];
        const FString Prefix = FString::Printf(
            TEXT("staticMeshKeyEnvironment.rowDigest.%d"), Index);
        AppendReceiptBindingField(
            Canonical, Prefix + TEXT(".name"), Row.Name);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".valueSha256"),
            Row.ValueSha256);
    }
    AppendReceiptBindingField(
        Canonical,
        TEXT("protectedMeshCount"),
        LexToString(Receipt.ProtectedMeshes.Num()));
    for (int32 Index = 0; Index < Receipt.ProtectedMeshes.Num(); ++Index)
    {
        const FProtectedPrewarmReceiptMesh& Mesh =
            Receipt.ProtectedMeshes[Index];
        const FString Prefix =
            FString::Printf(TEXT("protectedMesh.%d"), Index);
        AppendReceiptBindingField(
            Canonical, Prefix + TEXT(".objectPath"), Mesh.ObjectPath);
        AppendReceiptBindingField(
            Canonical, Prefix + TEXT(".packageName"), Mesh.PackageName);
        AppendReceiptBindingField(
            Canonical, Prefix + TEXT(".packageFile"), Mesh.PackageFile);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".packageBytes"),
            LexToString(Mesh.PackageBytes));
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".packageSha256"),
            Mesh.PackageSha256);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".renderDerivedDataKey"),
            Mesh.RenderDerivedDataKey);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".meshDataRawHash"),
            Mesh.MeshDataRawHash);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".meshDataRawSize"),
            LexToString(Mesh.MeshDataRawSize));
        AppendReceiptBindingField(
            Canonical, Prefix + TEXT(".secondaryMode"), Mesh.SecondaryMode);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".distanceFieldDerivedDataKey"),
            Mesh.DistanceFieldDerivedDataKey);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".distanceFieldRawHash"),
            Mesh.DistanceFieldRawHash);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".distanceFieldRawSize"),
            LexToString(Mesh.DistanceFieldRawSize));
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".cardDerivedDataKey"),
            Mesh.CardDerivedDataKey);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".cardRawHash"),
            Mesh.CardRawHash);
        AppendReceiptBindingField(
            Canonical,
            Prefix + TEXT(".cardRawSize"),
            LexToString(Mesh.CardRawSize));
    }
    AppendReceiptBindingField(
        Canonical,
        TEXT("v4AssetCountAtIssue"),
        LexToString(Receipt.V4AssetCountAtIssue));
    return Canonical;
}

bool CaptureReceiptFile(
    const FString& Filename,
    const FString& Label,
    FProtectedPrewarmReceiptFile& OutFile,
    FString& OutError)
{
    OutFile.Path = NormalizeReceiptPath(Filename);
    if (!HashFileSha256(
            OutFile.Path, OutFile.Sha256, OutFile.Bytes) ||
        OutFile.Bytes <= 0 || OutFile.Sha256.Len() != 64)
    {
        OutError = Label + TEXT(" could not be exact size/SHA bound: ") +
            OutFile.Path;
        return false;
    }
    OutError.Reset();
    return true;
}

bool CaptureProtectedSnapshotBinding(
    const TArray<FProtectedFileDigest>& ProtectedSnapshot,
    int32& OutRecordCount,
    FString& OutSha256,
    FString& OutError)
{
    TArray<FString> Rows;
    for (const FProtectedFileDigest& Record : ProtectedSnapshot)
    {
        FString Row;
        AppendReceiptBindingField(Row, TEXT("packageName"), Record.PackageName);
        AppendReceiptBindingField(Row, TEXT("filename"), Record.Filename);
        AppendReceiptBindingField(
            Row,
            TEXT("existed"),
            Record.bExisted ? TEXT("true") : TEXT("false"));
        AppendReceiptBindingField(
            Row, TEXT("bytes"), LexToString(Record.ByteCount));
        AppendReceiptBindingField(Row, TEXT("sha256"), Record.Sha256);
        Rows.Add(MoveTemp(Row));
    }
    Rows.Sort();
    OutRecordCount = Rows.Num();
    if (OutRecordCount <= 0 ||
        !HashUtf8StringSha256(FString::Join(Rows, TEXT("")), OutSha256))
    {
        OutError = TEXT("The exact protected-package snapshot receipt binding could not be generated.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool CaptureDdcZenIdentity(
    FString& OutGraphName,
    TArray<FString>& OutDirectories,
    FString& OutIdentitySha256,
    FString& OutError)
{
    FDerivedDataCacheInterface& Cache = GetDerivedDataCacheRef();
    OutGraphName = Cache.GetGraphName();
    TArray<FString> RawDirectories;
    Cache.GetDirectories(RawDirectories);
    TSet<FString> UniqueDirectories;
    for (FString Directory : RawDirectories)
    {
        Directory.TrimStartAndEndInline();
        Directory.ReplaceInline(TEXT("\\"), TEXT("/"));
        if (!Directory.IsEmpty())
        {
            UniqueDirectories.Add(Directory);
        }
    }
    OutDirectories = UniqueDirectories.Array();
    OutDirectories.Sort();
    FString Canonical;
    AppendReceiptBindingField(Canonical, TEXT("graphName"), OutGraphName);
    for (const FString& Directory : OutDirectories)
    {
        AppendReceiptBindingField(Canonical, TEXT("directory"), Directory);
    }
    if (OutGraphName.IsEmpty() ||
        !HashUtf8StringSha256(Canonical, OutIdentitySha256))
    {
        OutError = TEXT("The active DDC/Zen graph identity could not be captured.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool CaptureStaticMeshKeyEnvironmentBinding(
    int32& OutRecordCount,
    FString& OutSha256,
    TArray<FProtectedPrewarmReceiptEnvironmentRow>& OutRows,
    FString& OutError)
{
    OutRecordCount = 0;
    OutSha256.Reset();
    OutRows.Reset();
    ITargetPlatform* RunningTargetPlatform =
        GetTargetPlatformManagerRef().GetRunningTargetPlatform();
    if (!RunningTargetPlatform || !GConfig || GIsAutomationTesting)
    {
        OutError = TEXT("The StaticMesh DDC-key environment binding requires a running target platform, an effective config system, and a non-automation editor process.");
        return false;
    }
    TArray<FString> Rows;
    bool bRowDigestFailed = false;
    const auto Add =
        [&Rows, &OutRows, &bRowDigestFailed](
            const FString& Name,
            const FString& Value)
    {
        FString Row;
        AppendReceiptBindingField(Row, Name, Value);
        Rows.Add(MoveTemp(Row));
        FProtectedPrewarmReceiptEnvironmentRow DiagnosticRow;
        DiagnosticRow.Name = Name;
        if (!HashUtf8StringSha256(
                Value, DiagnosticRow.ValueSha256))
        {
            bRowDigestFailed = true;
        }
        OutRows.Add(MoveTemp(DiagnosticRow));
    };
    const auto AddBool = [&Add](const FString& Name, bool bValue)
    {
        Add(Name, bValue ? TEXT("true") : TEXT("false"));
    };
    const auto FloatBits = [](float Value) -> FString
    {
        uint32 Bits = 0;
        static_assert(sizeof(Bits) == sizeof(Value));
        FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
        return FString::Printf(TEXT("%08X"), Bits);
    };
    const FDataDrivenPlatformInfo& PlatformInfo =
        RunningTargetPlatform->GetPlatformInfo();
    AddBool(TEXT("process.isAutomationTesting"), GIsAutomationTesting);
    Add(TEXT("target.platformName"),
        RunningTargetPlatform->PlatformName());
    Add(TEXT("target.iniPlatformName"),
        RunningTargetPlatform->IniPlatformName());
    Add(TEXT("target.globalIdentifier"),
        PlatformInfo.GlobalIdentifier.ToString(EGuidFormats::Digits));
    Add(TEXT("target.platformGroupName"),
        PlatformInfo.PlatformGroupName.ToString());
    Add(TEXT("target.ubtPlatform"), PlatformInfo.UBTPlatformString);
    Add(TEXT("target.settingsIniSection"),
        PlatformInfo.TargetSettingsIniSectionName);
    Add(TEXT("target.iniParentChain"),
        FString::Join(PlatformInfo.IniParentChain, TEXT("|")));
    AddBool(TEXT("target.hasEditorOnlyData"),
        RunningTargetPlatform->HasEditorOnlyData());
    AddBool(TEXT("target.usesDistanceFields"),
        RunningTargetPlatform->UsesDistanceFields());
    AddBool(TEXT("target.usesRayTracing"),
        RunningTargetPlatform->UsesRayTracing());
    Add(TEXT("target.rayTracingMode"), LexToString(
        static_cast<int32>(RunningTargetPlatform->GetRayTracingMode())));
    Add(TEXT("target.supportedHardwareMask"), LexToString(
        RunningTargetPlatform->GetSupportedHardwareMask()));
    Add(TEXT("target.staticMeshOfflineBvhMode"), LexToString(
        static_cast<int32>(
            RunningTargetPlatform->GetStaticMeshOfflineBVHMode())));
    AddBool(TEXT("target.staticMeshOfflineBvhCompression"),
        RunningTargetPlatform->GetStaticMeshOfflineBVHCompression());
    AddBool(TEXT("target.feature.meshLodStreaming"),
        RunningTargetPlatform->SupportsFeature(
            ETargetPlatformFeatures::MeshLODStreaming));
    AddBool(TEXT("target.feature.hardwareLzDecompression"),
        RunningTargetPlatform->SupportsFeature(
            ETargetPlatformFeatures::HardwareLZDecompression));
    const FName MeshBuilderModuleName =
        RunningTargetPlatform->GetMeshBuilderModuleName();
    IMeshBuilderModule& MeshBuilderModule =
        IMeshBuilderModule::GetForPlatform(RunningTargetPlatform);
    FString MeshBuilderDdcContribution;
    MeshBuilderModule.AppendToDDCKey(
        MeshBuilderDdcContribution, false);
    Add(TEXT("target.meshBuilderModule.staticMeshDdcContribution"),
        MeshBuilderDdcContribution);
    const FString MeshBuilderModuleFilename = NormalizeReceiptPath(
        FModuleManager::Get().GetModuleFilename(MeshBuilderModuleName));
    FString MeshBuilderModuleSha;
    int64 MeshBuilderModuleBytes = 0;
    if (MeshBuilderModuleName.IsNone() ||
        !HashFileSha256(
            MeshBuilderModuleFilename,
            MeshBuilderModuleSha,
            MeshBuilderModuleBytes) ||
        MeshBuilderModuleBytes <= 0 || MeshBuilderModuleSha.Len() != 64)
    {
        OutError = TEXT("The running target platform's exact mesh-builder module could not be size/SHA bound.");
        return false;
    }
    Add(TEXT("target.meshBuilderModule.name"),
        MeshBuilderModuleName.ToString());
    Add(TEXT("target.meshBuilderModule.path"), MeshBuilderModuleFilename);
    Add(TEXT("target.meshBuilderModule.bytes"),
        LexToString(MeshBuilderModuleBytes));
    Add(TEXT("target.meshBuilderModule.sha256"), MeshBuilderModuleSha);
    IMeshUtilities& MeshUtilities =
        FModuleManager::LoadModuleChecked<IMeshUtilities>(
            TEXT("MeshUtilities"));
    Nanite::IBuilderModule& NaniteBuilder =
        Nanite::IBuilderModule::Get();
    Add(TEXT("staticMesh.version.systemGuid"),
        FDevSystemGuids::GetSystemGuid(
            FDevSystemGuids::Get().STATICMESH_DERIVEDDATA_VER).ToString());
    Add(TEXT("staticMesh.version.meshUtilities"),
        MeshUtilities.GetVersionString());
    Add(TEXT("staticMesh.version.naniteBuilder"),
        NaniteBuilder.GetVersionString());
    const auto AddBoundBinary =
        [&Add](
            const FString& Prefix,
            const FString& Filename,
            FString& Error) -> bool
    {
        const FString NormalizedFilename =
            NormalizeReceiptPath(Filename);
        FString Sha;
        int64 Bytes = 0;
        if (!HashFileSha256(NormalizedFilename, Sha, Bytes) ||
            Bytes <= 0 || Sha.Len() != 64)
        {
            Error = TEXT("A StaticMesh DDC-key implementation binary could not be exact size/SHA bound: ") +
                Prefix;
            return false;
        }
        Add(Prefix + TEXT(".path"), NormalizedFilename);
        Add(Prefix + TEXT(".bytes"), LexToString(Bytes));
        Add(Prefix + TEXT(".sha256"), Sha);
        Error.Reset();
        return true;
    };
    if (!AddBoundBinary(
            TEXT("binary.engine"),
            FModuleManager::Get().GetModuleFilename(FName(TEXT("Engine"))),
            OutError) ||
        !AddBoundBinary(
            TEXT("binary.meshUtilities"),
            FModuleManager::Get().GetModuleFilename(
                FName(TEXT("MeshUtilities"))),
            OutError) ||
        !AddBoundBinary(
            TEXT("binary.naniteBuilder"),
            FModuleManager::Get().GetModuleFilename(
                FName(TEXT("NaniteBuilder"))),
            OutError) ||
        !AddBoundBinary(
            TEXT("binary.editorExecutable"),
            FPlatformProcess::ExecutablePath(),
            OutError))
    {
        return false;
    }

    // GetOriginal is immutable for the process; the mutable command-line
    // accessor may be modified after startup. The guarded wrapper supplies
    // the same launch contract, while resolved key-affecting state is bound
    // separately below.
    const FString OriginalCommandLine(FCommandLine::GetOriginal());
    FString OriginalCommandLineSha;
    const FTCHARToUTF8 OriginalCommandLineUtf8(*OriginalCommandLine);
    if (!HashUtf8StringSha256(
            OriginalCommandLine, OriginalCommandLineSha))
    {
        OutError = TEXT("The immutable editor command line could not be bound.");
        return false;
    }
    Add(TEXT("process.commandLineBinding"),
        TEXT("immutable-original-plus-resolved-static-mesh-key-state-v1"));
    Add(TEXT("process.originalCommandLineUtf8Bytes"),
        LexToString(OriginalCommandLineUtf8.Length()));
    Add(TEXT("process.originalCommandLineSha256"),
        OriginalCommandLineSha);

    static const TCHAR* KeyAffectingConsoleVariables[] = {
        TEXT("r.MeshStreaming"),
        TEXT("r.Nanite.CoarseMeshStreaming"),
        TEXT("r.Nanite.ForceEnableMeshes"),
        TEXT("r.RayTracing"),
        TEXT("r.RayTracing.EnableOnDemand"),
        TEXT("r.StaticMesh.KeepMobileMinLODSettingOnDesktop"),
        TEXT("r.StaticMesh.StripMinLodDataDuringCooking"),
        TEXT("r.StaticMesh.UpdateMeshLODGroupSettingsAtLoad"),
        TEXT("r.TriangleOrderOptimization"),
        TEXT("r.GenerateMeshDistanceFields"),
        TEXT("r.MeshCardRepresentation"),
        TEXT("r.MeshCardRepresentation.MinDensity"),
        TEXT("r.MeshCardRepresentation.NormalTreshold"),
        TEXT("r.MeshCardRepresentation.Debug"),
        TEXT("r.MeshCardRepresentation.Debug.SurfelDirection"),
        TEXT("r.MeshCardRepresentation.Async"),
        TEXT("r.DistanceFields.MaxPerMeshResolution"),
        TEXT("r.DistanceFields.DefaultVoxelDensity")};
    for (const TCHAR* ConsoleVariableName :
         KeyAffectingConsoleVariables)
    {
        IConsoleVariable* ConsoleVariable =
            IConsoleManager::Get().FindConsoleVariable(
                ConsoleVariableName);
        if (!ConsoleVariable)
        {
            OutError = TEXT("A required StaticMesh DDC-key console variable is unavailable: ") +
                FString(ConsoleVariableName);
            return false;
        }
        const FString Prefix = TEXT("cvar.") +
            FString(ConsoleVariableName);
        Add(Prefix + TEXT(".value"), ConsoleVariable->GetString());
        Add(Prefix + TEXT(".setBy"), LexToString(
            static_cast<uint32>(ConsoleVariable->GetFlags()) &
            static_cast<uint32>(ECVF_SetByMask)));
    }

    const UMeshBudgetProjectSettings* MeshBudgetProjectSettings =
        GetDefault<UMeshBudgetProjectSettings>();
    if (!MeshBudgetProjectSettings ||
        MeshBudgetProjectSettings->bEnableStaticMeshBudget)
    {
        OutError = TEXT("The protected StaticMesh prewarm contract requires the resolved mesh-budget auto-LOD assignment setting to exist and remain disabled.");
        return false;
    }
    Add(TEXT("project.meshBudget.enableStaticMeshBudget"), TEXT("false"));

    const auto CaptureConfigRouting =
        [&Add](
            FConfigCacheIni* ConfigSystem,
            const FString& Prefix,
            FString& Error) -> bool
    {
        if (!ConfigSystem)
        {
            Error = TEXT("A StaticMesh DDC-key config system is null: ") +
                Prefix;
            return false;
        }
        static const TCHAR* RelevantConfigBaseNames[] = {
            TEXT("Engine"),
            TEXT("Game"),
            TEXT("DeviceProfiles"),
            TEXT("Scalability"),
            TEXT("Hardware"),
            TEXT("RuntimeOptions")};
        for (const TCHAR* BaseName : RelevantConfigBaseNames)
        {
            const FString Filename =
                ConfigSystem->GetConfigFilename(BaseName);
            const FConfigFile* ConfigFile =
                ConfigSystem->FindConfigFile(Filename);
            const FString RecordPrefix = Prefix + TEXT(".") + BaseName;
            Add(RecordPrefix + TEXT(".filename"), Filename);
            Add(RecordPrefix + TEXT(".present"),
                ConfigFile ? TEXT("true") : TEXT("false"));
            // Whole FConfigFile content includes resolved runtime state that is
            // unrelated to StaticMesh key construction and can legitimately
            // differ between clean editor processes. Bind routing here; bind
            // every admitted effective key input explicitly above/below.
        }
        Error.Reset();
        return true;
    };
    if (!CaptureConfigRouting(
            GConfig, TEXT("hostConfig"), OutError) ||
        !CaptureConfigRouting(
            RunningTargetPlatform->GetConfigSystem(),
            TEXT("targetConfig"),
            OutError))
    {
        return false;
    }

    const auto AddReductionSettings =
        [&Add, &FloatBits](
            const FString& Prefix,
            const FMeshReductionSettings& Settings)
    {
        Add(Prefix + TEXT(".terminationCriterion"), LexToString(
            static_cast<uint8>(Settings.TerminationCriterion)));
        Add(Prefix + TEXT(".percentTrianglesBits"),
            FloatBits(Settings.PercentTriangles));
        Add(Prefix + TEXT(".maxNumTriangles"),
            LexToString(Settings.MaxNumOfTriangles));
        Add(Prefix + TEXT(".percentVerticesBits"),
            FloatBits(Settings.PercentVertices));
        Add(Prefix + TEXT(".maxNumVertices"),
            LexToString(Settings.MaxNumOfVerts));
        Add(Prefix + TEXT(".maxDeviationBits"),
            FloatBits(Settings.MaxDeviation));
        Add(Prefix + TEXT(".pixelErrorBits"),
            FloatBits(Settings.PixelError));
        Add(Prefix + TEXT(".weldingThresholdBits"),
            FloatBits(Settings.WeldingThreshold));
        Add(Prefix + TEXT(".hardAngleThresholdBits"),
            FloatBits(Settings.HardAngleThreshold));
        Add(Prefix + TEXT(".baseLodModel"),
            LexToString(Settings.BaseLODModel));
        Add(Prefix + TEXT(".silhouetteImportance"), LexToString(
            static_cast<uint8>(Settings.SilhouetteImportance)));
        Add(Prefix + TEXT(".textureImportance"), LexToString(
            static_cast<uint8>(Settings.TextureImportance)));
        Add(Prefix + TEXT(".shadingImportance"), LexToString(
            static_cast<uint8>(Settings.ShadingImportance)));
        Add(Prefix + TEXT(".recalculateNormals"),
            Settings.bRecalculateNormals ? TEXT("true") : TEXT("false"));
    };
    const FStaticMeshLODGroup& NoneLodGroup =
        RunningTargetPlatform->GetStaticMeshLODSettings().GetLODGroup(
            NAME_None);
    const FString NoneLodPrefix =
        TEXT("target.staticMeshLodGroup.None");
    Add(NoneLodPrefix + TEXT(".defaultNumLods"),
        LexToString(NoneLodGroup.GetDefaultNumLODs()));
    Add(NoneLodPrefix + TEXT(".defaultMaxStreamedLods"),
        LexToString(NoneLodGroup.GetDefaultMaxNumStreamedLODs()));
    Add(NoneLodPrefix + TEXT(".defaultMaxOptionalLods"),
        LexToString(NoneLodGroup.GetDefaultMaxNumOptionalLODs()));
    Add(NoneLodPrefix + TEXT(".defaultLightMapResolution"),
        LexToString(NoneLodGroup.GetDefaultLightMapResolution()));
    AddBool(NoneLodPrefix + TEXT(".supportsLodStreaming"),
        NoneLodGroup.IsLODStreamingSupported());
    FMeshReductionSettings ProbeSettings;
    ProbeSettings.PercentTriangles = 0.347f;
    ProbeSettings.PercentVertices = 0.419f;
    ProbeSettings.MaxDeviation = 17.25f;
    ProbeSettings.PixelError = 23.5f;
    ProbeSettings.WeldingThreshold = 0.067f;
    ProbeSettings.HardAngleThreshold = 41.25f;
    ProbeSettings.SilhouetteImportance = EMeshFeatureImportance::Normal;
    ProbeSettings.TextureImportance = EMeshFeatureImportance::Normal;
    ProbeSettings.ShadingImportance = EMeshFeatureImportance::Normal;
    for (int32 LodIndex = 0; LodIndex < MAX_STATIC_MESH_LODS; ++LodIndex)
    {
        AddReductionSettings(
            NoneLodPrefix + FString::Printf(
                TEXT(".defaultLod%d"), LodIndex),
            NoneLodGroup.GetDefaultSettings(LodIndex));
        AddReductionSettings(
            NoneLodPrefix + FString::Printf(
                TEXT(".biasProbeLod%d"), LodIndex),
            NoneLodGroup.GetSettings(ProbeSettings, LodIndex));
    }

    Rows.Sort();
    OutRows.Sort([](
        const FProtectedPrewarmReceiptEnvironmentRow& A,
        const FProtectedPrewarmReceiptEnvironmentRow& B)
    {
        return A.Name.Compare(B.Name) < 0;
    });
    OutRecordCount = Rows.Num();
    bool bRowsAreSortedAndUnique = OutRows.Num() == OutRecordCount;
    for (int32 Index = 0;
         bRowsAreSortedAndUnique && Index < OutRows.Num();
         ++Index)
    {
        bRowsAreSortedAndUnique =
            IsSafeStaticMeshEnvironmentRowName(OutRows[Index].Name) &&
            OutRows[Index].ValueSha256.Len() == 64 &&
            (Index == 0 ||
             OutRows[Index - 1].Name.Compare(OutRows[Index].Name) < 0);
    }
    if (OutRecordCount <= 0 || bRowDigestFailed ||
        !bRowsAreSortedAndUnique ||
        !HashUtf8StringSha256(
            FString::Join(Rows, TEXT("")), OutSha256))
    {
        OutError = TEXT("The exact running StaticMesh DDC-key environment manifest could not be bound.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool CaptureCurrentProtectedPrewarmReceiptState(
    const TArray<FProtectedFileDigest>& ProtectedSnapshot,
    const TMap<FString, FProtectedPrewarmDerivedDataProof>& DerivedDataProofs,
    FProtectedPrewarmReceipt& OutReceipt,
    FString& OutError)
{
    OutReceipt = FProtectedPrewarmReceipt();
    OutReceipt.Schema = ProtectedPrewarmReceiptSchema;
    FString ProjectFile = FPaths::GetProjectFilePath();
    if (ProjectFile.IsEmpty() ||
        !CaptureReceiptFile(
            ProjectFile,
            TEXT("Project descriptor"),
            OutReceipt.ProjectFile,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The current project descriptor path is unavailable.");
        }
        return false;
    }
    const FString EditorModuleFilename =
        FModuleManager::Get().GetModuleFilename(
            FName(TEXT("TRIADSensorFusionEditor")));
    if (!CaptureReceiptFile(
            EditorModuleFilename,
            TEXT("Loaded TRIADSensorFusionEditor module"),
            OutReceipt.EditorModule,
            OutError))
    {
        return false;
    }
    if (!CaptureReceiptFile(
            FPaths::Combine(
                FPaths::EngineDir(), TEXT("Build/Build.version")),
            TEXT("Engine build-version descriptor"),
            OutReceipt.EngineBuildVersionFile,
            OutError))
    {
        return false;
    }
    OutReceipt.EngineVersion = FEngineVersion::Current().ToString(
        EVersionComponent::Changelist);
    OutReceipt.EngineChangelist =
        LexToString(FEngineVersion::Current().GetChangelist());
    if (!CaptureReceiptFile(
            VegetationContractFilename(),
            TEXT("Frozen Explore V4 vegetation contract"),
            OutReceipt.VegetationContract,
            OutError) ||
        OutReceipt.VegetationContract.Bytes != VegetationContractBytes ||
        !OutReceipt.VegetationContract.Sha256.Equals(
            VegetationContractSha, ESearchCase::IgnoreCase) ||
        !CaptureProtectedSnapshotBinding(
            ProtectedSnapshot,
            OutReceipt.ProtectedSnapshotRecordCount,
            OutReceipt.ProtectedSnapshotSha256,
            OutError) ||
        !CaptureDdcZenIdentity(
            OutReceipt.DdcGraphName,
            OutReceipt.DdcDirectories,
            OutReceipt.DdcZenIdentitySha256,
            OutError) ||
        !CaptureStaticMeshKeyEnvironmentBinding(
            OutReceipt.StaticMeshKeyEnvironmentRecordCount,
            OutReceipt.StaticMeshKeyEnvironmentSha256,
            OutReceipt.StaticMeshKeyEnvironmentRows,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The current receipt-bound contract or DDC/Zen identity changed.");
        }
        return false;
    }
    if (ProtectedDerivativeSpecs().Num() != 2 ||
        DerivedDataProofs.Num() != 2)
    {
        OutError = TEXT("The protected prewarm receipt requires exactly two source/DDC records.");
        return false;
    }
    TSet<FString> UniqueDerivedDataKeys;
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        const FProtectedPrewarmDerivedDataProof* DerivedDataProof =
            DerivedDataProofs.Find(Spec.SourceObjectPath);
        FString PackageFilename;
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(Spec.SourceObjectPath);
        FProtectedPrewarmReceiptMesh Mesh;
        if (!DerivedDataProof ||
            DerivedDataProof->RenderDerivedDataKey.IsEmpty() ||
            UniqueDerivedDataKeys.Contains(
                DerivedDataProof->RenderDerivedDataKey) ||
            FindObject<UStaticMesh>(nullptr, *Spec.SourceObjectPath) ||
            FindPackage(nullptr, *PackageName) ||
            !ResolvePackageFileAndValidate(
                Spec.SourceObjectPath,
                Spec.SourcePackageBytes,
                Spec.SourcePackageSha256,
                PackageFilename,
                OutError) ||
            !ValidateStaticMeshDerivedDataRecord(
                DerivedDataProof->RenderDerivedDataKey,
                Spec.SourceObjectPath,
                Mesh.MeshDataRawHash,
                Mesh.MeshDataRawSize,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A protected receipt mesh is loaded, duplicated or no longer exact: ") +
                    Spec.SelectionId;
            }
            return false;
        }
        FProtectedPrewarmDerivedDataProof CurrentProof = *DerivedDataProof;
        if (!ValidateProtectedSecondaryDerivedDataProof(
                CurrentProof, Spec.SourceObjectPath, OutError) ||
            CurrentProof.DistanceFieldRawHash !=
                DerivedDataProof->DistanceFieldRawHash ||
            CurrentProof.DistanceFieldRawSize !=
                DerivedDataProof->DistanceFieldRawSize ||
            CurrentProof.CardRawHash != DerivedDataProof->CardRawHash ||
            CurrentProof.CardRawSize != DerivedDataProof->CardRawSize)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A protected receipt secondary CARD/DIST local-DDC record changed: ") +
                    Spec.SelectionId;
            }
            return false;
        }
        UniqueDerivedDataKeys.Add(
            DerivedDataProof->RenderDerivedDataKey);
        Mesh.ObjectPath = Spec.SourceObjectPath;
        Mesh.PackageName = PackageName;
        Mesh.PackageFile = NormalizeReceiptPath(PackageFilename);
        Mesh.PackageBytes = Spec.SourcePackageBytes;
        Mesh.PackageSha256 = Spec.SourcePackageSha256;
        Mesh.RenderDerivedDataKey =
            DerivedDataProof->RenderDerivedDataKey;
        Mesh.SecondaryMode = DerivedDataProof->SecondaryMode;
        Mesh.DistanceFieldDerivedDataKey =
            DerivedDataProof->DistanceFieldDerivedDataKey;
        Mesh.DistanceFieldRawHash =
            DerivedDataProof->DistanceFieldRawHash;
        Mesh.DistanceFieldRawSize =
            DerivedDataProof->DistanceFieldRawSize;
        Mesh.CardDerivedDataKey = DerivedDataProof->CardDerivedDataKey;
        Mesh.CardRawHash = DerivedDataProof->CardRawHash;
        Mesh.CardRawSize = DerivedDataProof->CardRawSize;
        OutReceipt.ProtectedMeshes.Add(MoveTemp(Mesh));
    }
    OutReceipt.V4AssetCountAtIssue = CountAssetsUnderV4Root();
    if (OutReceipt.ProtectedMeshes.Num() != 2 ||
        OutReceipt.V4AssetCountAtIssue != 0 ||
        !ValidateProtectedPackages(ProtectedSnapshot, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The prewarm receipt requires zero V4 assets and an exact complete protected snapshot.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

TSharedRef<FJsonObject> ReceiptFileToJson(
    const FProtectedPrewarmReceiptFile& File)
{
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("path"), File.Path);
    Json->SetNumberField(TEXT("bytes"), static_cast<double>(File.Bytes));
    Json->SetStringField(TEXT("sha256"), File.Sha256);
    return Json;
}

TSharedRef<FJsonObject> ProtectedPrewarmReceiptToJson(
    const FProtectedPrewarmReceipt& Receipt)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), Receipt.Schema);
    Root->SetStringField(TEXT("receiptId"), Receipt.ReceiptId);
    Root->SetStringField(TEXT("createdUtc"), Receipt.CreatedUtc);
    Root->SetStringField(
        TEXT("producerProcessId"), Receipt.ProducerProcessId);
    Root->SetStringField(
        TEXT("producerAppInstanceId"), Receipt.ProducerAppInstanceId);
    Root->SetObjectField(
        TEXT("projectFile"), ReceiptFileToJson(Receipt.ProjectFile));
    Root->SetObjectField(
        TEXT("editorModule"), ReceiptFileToJson(Receipt.EditorModule));
    Root->SetObjectField(
        TEXT("engineBuildVersionFile"),
        ReceiptFileToJson(Receipt.EngineBuildVersionFile));
    Root->SetStringField(TEXT("engineVersion"), Receipt.EngineVersion);
    Root->SetStringField(
        TEXT("engineChangelist"), Receipt.EngineChangelist);
    Root->SetObjectField(
        TEXT("vegetationContract"),
        ReceiptFileToJson(Receipt.VegetationContract));
    TSharedRef<FJsonObject> ProtectedSnapshot = MakeShared<FJsonObject>();
    ProtectedSnapshot->SetNumberField(
        TEXT("recordCount"), Receipt.ProtectedSnapshotRecordCount);
    ProtectedSnapshot->SetStringField(
        TEXT("sha256"), Receipt.ProtectedSnapshotSha256);
    Root->SetObjectField(TEXT("protectedSnapshot"), ProtectedSnapshot);
    TSharedRef<FJsonObject> DdcZen = MakeShared<FJsonObject>();
    DdcZen->SetStringField(TEXT("graphName"), Receipt.DdcGraphName);
    TArray<TSharedPtr<FJsonValue>> Directories;
    for (const FString& Directory : Receipt.DdcDirectories)
    {
        Directories.Add(MakeShared<FJsonValueString>(Directory));
    }
    DdcZen->SetArrayField(TEXT("directories"), Directories);
    DdcZen->SetStringField(
        TEXT("identitySha256"), Receipt.DdcZenIdentitySha256);
    DdcZen->SetStringField(TEXT("recordBucket"), TEXT("StaticMesh"));
    Root->SetObjectField(TEXT("ddcZen"), DdcZen);
    TSharedRef<FJsonObject> StaticMeshKeyEnvironment =
        MakeShared<FJsonObject>();
    StaticMeshKeyEnvironment->SetNumberField(
        TEXT("recordCount"),
        Receipt.StaticMeshKeyEnvironmentRecordCount);
    StaticMeshKeyEnvironment->SetStringField(
        TEXT("sha256"), Receipt.StaticMeshKeyEnvironmentSha256);
    TArray<TSharedPtr<FJsonValue>> EnvironmentRows;
    for (const FProtectedPrewarmReceiptEnvironmentRow& Row :
         Receipt.StaticMeshKeyEnvironmentRows)
    {
        TSharedRef<FJsonObject> RowJson = MakeShared<FJsonObject>();
        RowJson->SetStringField(TEXT("name"), Row.Name);
        RowJson->SetStringField(
            TEXT("valueSha256"), Row.ValueSha256);
        EnvironmentRows.Add(MakeShared<FJsonValueObject>(RowJson));
    }
    StaticMeshKeyEnvironment->SetArrayField(
        TEXT("rows"), EnvironmentRows);
    Root->SetObjectField(
        TEXT("staticMeshKeyEnvironment"), StaticMeshKeyEnvironment);
    TArray<TSharedPtr<FJsonValue>> Meshes;
    for (const FProtectedPrewarmReceiptMesh& Mesh : Receipt.ProtectedMeshes)
    {
        TSharedRef<FJsonObject> MeshJson = MakeShared<FJsonObject>();
        MeshJson->SetStringField(TEXT("objectPath"), Mesh.ObjectPath);
        MeshJson->SetStringField(TEXT("packageName"), Mesh.PackageName);
        MeshJson->SetStringField(TEXT("packageFile"), Mesh.PackageFile);
        MeshJson->SetNumberField(
            TEXT("packageBytes"), static_cast<double>(Mesh.PackageBytes));
        MeshJson->SetStringField(
            TEXT("packageSha256"), Mesh.PackageSha256);
        MeshJson->SetStringField(
            TEXT("renderDerivedDataKey"), Mesh.RenderDerivedDataKey);
        MeshJson->SetStringField(
            TEXT("meshDataRawHash"), Mesh.MeshDataRawHash);
        MeshJson->SetNumberField(
            TEXT("meshDataRawSize"),
            static_cast<double>(Mesh.MeshDataRawSize));
        MeshJson->SetStringField(
            TEXT("secondaryMode"), Mesh.SecondaryMode);
        MeshJson->SetStringField(
            TEXT("distanceFieldDerivedDataKey"),
            Mesh.DistanceFieldDerivedDataKey);
        MeshJson->SetStringField(
            TEXT("distanceFieldRawHash"), Mesh.DistanceFieldRawHash);
        MeshJson->SetNumberField(
            TEXT("distanceFieldRawSize"),
            static_cast<double>(Mesh.DistanceFieldRawSize));
        MeshJson->SetStringField(
            TEXT("cardDerivedDataKey"), Mesh.CardDerivedDataKey);
        MeshJson->SetStringField(
            TEXT("cardRawHash"), Mesh.CardRawHash);
        MeshJson->SetNumberField(
            TEXT("cardRawSize"),
            static_cast<double>(Mesh.CardRawSize));
        Meshes.Add(MakeShared<FJsonValueObject>(MeshJson));
    }
    Root->SetArrayField(TEXT("protectedMeshes"), Meshes);
    Root->SetNumberField(
        TEXT("v4AssetCountAtIssue"), Receipt.V4AssetCountAtIssue);
    Root->SetStringField(TEXT("bindingSha256"), Receipt.BindingSha256);
    return Root;
}

bool WriteProtectedPrewarmReceiptAtomically(
    const FProtectedPrewarmReceipt& Receipt,
    FString& OutError)
{
    const FString DestinationPath = ProtectedPrewarmReceiptFilename();
    const FString DestinationDirectory = FPaths::GetPath(DestinationPath);
    if (IFileManager::Get().FileExists(*DestinationPath) ||
        (!IFileManager::Get().DirectoryExists(*DestinationDirectory) &&
         !IFileManager::Get().MakeDirectory(*DestinationDirectory, true)))
    {
        OutError = TEXT("Atomic protected-prewarm receipt publication requires an absent final file and a writable exact Saved directory.");
        return false;
    }
    FString JsonText;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(
            &JsonText);
    if (!FJsonSerializer::Serialize(
            ProtectedPrewarmReceiptToJson(Receipt), Writer))
    {
        OutError = TEXT("The protected-prewarm receipt could not be serialized.");
        return false;
    }
    const FString TemporaryPath = DestinationPath + TEXT(".") +
        FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
    const auto DeleteTemporary = [&TemporaryPath]()
    {
        IFileManager::Get().Delete(*TemporaryPath, false, true, true);
    };
    const FTCHARToUTF8 Utf8Json(*JsonText);
    IPlatformFile& PlatformFile =
        FPlatformFileManager::Get().GetPlatformFile();
    TUniquePtr<IFileHandle> TemporaryHandle(
        PlatformFile.OpenWrite(*TemporaryPath, false, false));
    if (!TemporaryHandle ||
        !TemporaryHandle->Write(
            reinterpret_cast<const uint8*>(Utf8Json.Get()),
            static_cast<int64>(Utf8Json.Length())) ||
        !TemporaryHandle->Flush(true))
    {
        TemporaryHandle.Reset();
        DeleteTemporary();
        OutError = TEXT("The protected-prewarm receipt temporary file could not be written: ") +
            TemporaryPath;
        return false;
    }
    TemporaryHandle.Reset();
    FString ReadBackJson;
    FString ExpectedJsonSha;
    FString ActualJsonSha;
    int64 ActualJsonBytes = 0;
    TSharedPtr<FJsonObject> ReadBackRoot;
    if (!HashUtf8StringSha256(JsonText, ExpectedJsonSha) ||
        !HashFileSha256(
            TemporaryPath, ActualJsonSha, ActualJsonBytes) ||
        ActualJsonBytes != Utf8Json.Length() ||
        ActualJsonSha != ExpectedJsonSha ||
        !FFileHelper::LoadFileToString(ReadBackJson, *TemporaryPath))
    {
        DeleteTemporary();
        OutError = TEXT("The durable protected-prewarm receipt temporary file failed exact read-back validation: ") +
            TemporaryPath;
        return false;
    }
    const TSharedRef<TJsonReader<>> ReadBackReader =
        TJsonReaderFactory<>::Create(ReadBackJson);
    if (!FJsonSerializer::Deserialize(ReadBackReader, ReadBackRoot) ||
        !ReadBackRoot.IsValid())
    {
        DeleteTemporary();
        OutError = TEXT("The durable protected-prewarm receipt temporary JSON is invalid: ") +
            TemporaryPath;
        return false;
    }
    if (IFileManager::Get().FileExists(*DestinationPath) ||
        !IFileManager::Get().Move(
            *DestinationPath,
            *TemporaryPath,
            false,
            false,
            false,
            true))
    {
        DeleteTemporary();
        OutError = TEXT("The protected-prewarm receipt could not be atomically published without replacement: ") +
            DestinationPath;
        return false;
    }
    if (!IFileManager::Get().FileExists(*DestinationPath) ||
        IFileManager::Get().FileExists(*TemporaryPath))
    {
        OutError = TEXT("The protected-prewarm receipt atomic publication postcondition failed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateExactJsonFields(
    const FJsonObject& Object,
    std::initializer_list<const TCHAR*> ExpectedFields,
    const FString& Label,
    FString& OutError)
{
    if (Object.Values.Num() != static_cast<int32>(ExpectedFields.size()))
    {
        OutError = Label + TEXT(" has an unexpected JSON field count.");
        return false;
    }
    for (const TCHAR* Field : ExpectedFields)
    {
        if (!Object.Values.Contains(Field))
        {
            OutError = Label + TEXT(" is missing exact JSON field: ") + Field;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ReadExactJsonString(
    const FJsonObject& Object,
    const TCHAR* Field,
    FString& OutValue,
    FString& OutError)
{
    const TSharedPtr<FJsonValue>* JsonValue = Object.Values.Find(Field);
    if (!JsonValue || !JsonValue->IsValid() ||
        (*JsonValue)->Type != EJson::String ||
        !Object.TryGetStringField(Field, OutValue))
    {
        OutError = FString::Printf(
            TEXT("Protected-prewarm receipt field '%s' is not an exact JSON string."),
            Field);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ReadExactJsonInt64(
    const FJsonObject& Object,
    const TCHAR* Field,
    int64 Minimum,
    int64 Maximum,
    int64& OutValue,
    FString& OutError)
{
    const TSharedPtr<FJsonValue>* JsonValue = Object.Values.Find(Field);
    double Number = 0.0;
    constexpr double MaximumExactJsonInteger = 9007199254740991.0;
    if (!JsonValue || !JsonValue->IsValid() ||
        (*JsonValue)->Type != EJson::Number ||
        !Object.TryGetNumberField(Field, Number) ||
        !FMath::IsFinite(Number) ||
        FMath::TruncToDouble(Number) != Number ||
        Number < static_cast<double>(Minimum) ||
        Number > static_cast<double>(Maximum) ||
        FMath::Abs(Number) > MaximumExactJsonInteger)
    {
        OutError = FString::Printf(
            TEXT("Protected-prewarm receipt field '%s' is not an exact bounded JSON integer."),
            Field);
        return false;
    }
    OutValue = static_cast<int64>(Number);
    OutError.Reset();
    return true;
}

bool ReadExactJsonObject(
    const FJsonObject& Object,
    const TCHAR* Field,
    TSharedPtr<FJsonObject>& OutObject,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* JsonObject = nullptr;
    if (!Object.TryGetObjectField(Field, JsonObject) || !JsonObject ||
        !JsonObject->IsValid())
    {
        OutError = FString::Printf(
            TEXT("Protected-prewarm receipt field '%s' is not an exact JSON object."),
            Field);
        return false;
    }
    OutObject = *JsonObject;
    OutError.Reset();
    return true;
}

bool ReadExactJsonArray(
    const FJsonObject& Object,
    const TCHAR* Field,
    const TArray<TSharedPtr<FJsonValue>>*& OutArray,
    FString& OutError)
{
    OutArray = nullptr;
    if (!Object.TryGetArrayField(Field, OutArray) || !OutArray)
    {
        OutError = FString::Printf(
            TEXT("Protected-prewarm receipt field '%s' is not an exact JSON array."),
            Field);
        return false;
    }
    OutError.Reset();
    return true;
}

bool IsExactUpperHex(const FString& Value, int32 ExpectedLength)
{
    if (Value.Len() != ExpectedLength || Value.ToUpper() != Value)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsHexDigit(Character))
        {
            return false;
        }
    }
    return true;
}

bool ParseReceiptFileObject(
    const FJsonObject& Json,
    FProtectedPrewarmReceiptFile& OutFile,
    FString& OutError)
{
    if (!ValidateExactJsonFields(
            Json,
            {TEXT("path"), TEXT("bytes"), TEXT("sha256")},
            TEXT("Protected-prewarm receipt file binding"),
            OutError) ||
        !ReadExactJsonString(Json, TEXT("path"), OutFile.Path, OutError) ||
        !ReadExactJsonInt64(
            Json,
            TEXT("bytes"),
            1,
            MAX_int64,
            OutFile.Bytes,
            OutError) ||
        !ReadExactJsonString(
            Json, TEXT("sha256"), OutFile.Sha256, OutError) ||
        OutFile.Path.IsEmpty() ||
        NormalizeReceiptPath(OutFile.Path) != OutFile.Path ||
        !IsExactUpperHex(OutFile.Sha256, 64))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A protected-prewarm receipt file path/size/SHA binding is malformed.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ParseProtectedPrewarmReceipt(
    FProtectedPrewarmReceipt& OutReceipt,
    uint32& OutProducerProcessId,
    FString& OutError)
{
    OutReceipt = FProtectedPrewarmReceipt();
    OutProducerProcessId = 0;
    const FString ReceiptPath = ProtectedPrewarmReceiptFilename();
    const int64 ReceiptBytes = IFileManager::Get().FileSize(*ReceiptPath);
    FString JsonText;
    TSharedPtr<FJsonObject> Root;
    if (ReceiptBytes <= 0 || ReceiptBytes > 256 * 1024 ||
        !FFileHelper::LoadFileToString(JsonText, *ReceiptPath))
    {
        OutError = TEXT("The exact protected-prewarm receipt is absent, empty or over its bounded size limit.");
        return false;
    }
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
        !ValidateExactJsonFields(
            *Root,
            {
                TEXT("schema"),
                TEXT("receiptId"),
                TEXT("createdUtc"),
                TEXT("producerProcessId"),
                TEXT("producerAppInstanceId"),
                TEXT("projectFile"),
                TEXT("editorModule"),
                TEXT("engineBuildVersionFile"),
                TEXT("engineVersion"),
                TEXT("engineChangelist"),
                TEXT("vegetationContract"),
                TEXT("protectedSnapshot"),
                TEXT("ddcZen"),
                TEXT("staticMeshKeyEnvironment"),
                TEXT("protectedMeshes"),
                TEXT("v4AssetCountAtIssue"),
                TEXT("bindingSha256")
            },
            TEXT("Protected-prewarm receipt root"),
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The protected-prewarm receipt JSON root is malformed.");
        }
        return false;
    }
    TSharedPtr<FJsonObject> ProjectFile;
    TSharedPtr<FJsonObject> EditorModule;
    TSharedPtr<FJsonObject> EngineBuildVersionFile;
    TSharedPtr<FJsonObject> VegetationContract;
    TSharedPtr<FJsonObject> ProtectedSnapshot;
    TSharedPtr<FJsonObject> DdcZen;
    TSharedPtr<FJsonObject> StaticMeshKeyEnvironment;
    const TArray<TSharedPtr<FJsonValue>>* Directories = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* EnvironmentRows = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Meshes = nullptr;
    int64 SnapshotRecordCount = 0;
    int64 StaticMeshKeyEnvironmentRecordCount = 0;
    int64 V4AssetCount = -1;
    if (!ReadExactJsonString(
            *Root, TEXT("schema"), OutReceipt.Schema, OutError) ||
        !ReadExactJsonString(
            *Root, TEXT("receiptId"), OutReceipt.ReceiptId, OutError) ||
        !ReadExactJsonString(
            *Root, TEXT("createdUtc"), OutReceipt.CreatedUtc, OutError) ||
        !ReadExactJsonString(
            *Root,
            TEXT("producerProcessId"),
            OutReceipt.ProducerProcessId,
            OutError) ||
        !ReadExactJsonString(
            *Root,
            TEXT("producerAppInstanceId"),
            OutReceipt.ProducerAppInstanceId,
            OutError) ||
        !ReadExactJsonObject(
            *Root, TEXT("projectFile"), ProjectFile, OutError) ||
        !ReadExactJsonObject(
            *Root, TEXT("editorModule"), EditorModule, OutError) ||
        !ReadExactJsonObject(
            *Root,
            TEXT("engineBuildVersionFile"),
            EngineBuildVersionFile,
            OutError) ||
        !ReadExactJsonString(
            *Root,
            TEXT("engineVersion"),
            OutReceipt.EngineVersion,
            OutError) ||
        !ReadExactJsonString(
            *Root,
            TEXT("engineChangelist"),
            OutReceipt.EngineChangelist,
            OutError) ||
        !ReadExactJsonObject(
            *Root,
            TEXT("vegetationContract"),
            VegetationContract,
            OutError) ||
        !ReadExactJsonObject(
            *Root,
            TEXT("protectedSnapshot"),
            ProtectedSnapshot,
            OutError) ||
        !ReadExactJsonObject(*Root, TEXT("ddcZen"), DdcZen, OutError) ||
        !ReadExactJsonObject(
            *Root,
            TEXT("staticMeshKeyEnvironment"),
            StaticMeshKeyEnvironment,
            OutError) ||
        !ReadExactJsonArray(
            *Root, TEXT("protectedMeshes"), Meshes, OutError) ||
        !ReadExactJsonInt64(
            *Root,
            TEXT("v4AssetCountAtIssue"),
            0,
            0,
            V4AssetCount,
            OutError) ||
        !ReadExactJsonString(
            *Root,
            TEXT("bindingSha256"),
            OutReceipt.BindingSha256,
            OutError) ||
        !ParseReceiptFileObject(
            *ProjectFile, OutReceipt.ProjectFile, OutError) ||
        !ParseReceiptFileObject(
            *EditorModule, OutReceipt.EditorModule, OutError) ||
        !ParseReceiptFileObject(
            *EngineBuildVersionFile,
            OutReceipt.EngineBuildVersionFile,
            OutError) ||
        !ParseReceiptFileObject(
            *VegetationContract,
            OutReceipt.VegetationContract,
            OutError))
    {
        return false;
    }
    OutReceipt.V4AssetCountAtIssue = static_cast<int32>(V4AssetCount);
    if (!ValidateExactJsonFields(
            *ProtectedSnapshot,
            {TEXT("recordCount"), TEXT("sha256")},
            TEXT("Protected-prewarm receipt protected snapshot"),
            OutError) ||
        !ReadExactJsonInt64(
            *ProtectedSnapshot,
            TEXT("recordCount"),
            1,
            MAX_int32,
            SnapshotRecordCount,
            OutError) ||
        !ReadExactJsonString(
            *ProtectedSnapshot,
            TEXT("sha256"),
            OutReceipt.ProtectedSnapshotSha256,
            OutError) ||
        !IsExactUpperHex(OutReceipt.ProtectedSnapshotSha256, 64))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The protected-prewarm receipt protected snapshot binding is malformed.");
        }
        return false;
    }
    OutReceipt.ProtectedSnapshotRecordCount =
        static_cast<int32>(SnapshotRecordCount);
    if (!ValidateExactJsonFields(
            *StaticMeshKeyEnvironment,
            {TEXT("recordCount"), TEXT("sha256"), TEXT("rows")},
            TEXT("Protected-prewarm receipt StaticMesh key environment"),
            OutError) ||
        !ReadExactJsonInt64(
            *StaticMeshKeyEnvironment,
            TEXT("recordCount"),
            1,
            4096,
            StaticMeshKeyEnvironmentRecordCount,
            OutError) ||
        !ReadExactJsonString(
            *StaticMeshKeyEnvironment,
            TEXT("sha256"),
            OutReceipt.StaticMeshKeyEnvironmentSha256,
            OutError) ||
        !ReadExactJsonArray(
            *StaticMeshKeyEnvironment,
            TEXT("rows"),
            EnvironmentRows,
            OutError) ||
        !IsExactUpperHex(
            OutReceipt.StaticMeshKeyEnvironmentSha256, 64) ||
        !EnvironmentRows ||
        EnvironmentRows->Num() != StaticMeshKeyEnvironmentRecordCount)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The protected-prewarm receipt StaticMesh key environment binding is malformed.");
        }
        return false;
    }
    OutReceipt.StaticMeshKeyEnvironmentRecordCount =
        static_cast<int32>(StaticMeshKeyEnvironmentRecordCount);
    FString PreviousEnvironmentRowName;
    for (const TSharedPtr<FJsonValue>& RowValue : *EnvironmentRows)
    {
        const TSharedPtr<FJsonObject>* RowObject = nullptr;
        FProtectedPrewarmReceiptEnvironmentRow Row;
        if (!RowValue.IsValid() || RowValue->Type != EJson::Object ||
            !RowValue->TryGetObject(RowObject) || !RowObject ||
            !RowObject->IsValid() ||
            !ValidateExactJsonFields(
                **RowObject,
                {TEXT("name"), TEXT("valueSha256")},
                TEXT("Protected-prewarm receipt StaticMesh environment row"),
                OutError) ||
            !ReadExactJsonString(
                **RowObject, TEXT("name"), Row.Name, OutError) ||
            !ReadExactJsonString(
                **RowObject,
                TEXT("valueSha256"),
                Row.ValueSha256,
                OutError) ||
            !IsSafeStaticMeshEnvironmentRowName(Row.Name) ||
            !IsExactUpperHex(Row.ValueSha256, 64) ||
            (!PreviousEnvironmentRowName.IsEmpty() &&
             PreviousEnvironmentRowName.Compare(Row.Name) >= 0))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A protected-prewarm receipt StaticMesh environment row is malformed, unsorted or duplicated.");
            }
            return false;
        }
        PreviousEnvironmentRowName = Row.Name;
        OutReceipt.StaticMeshKeyEnvironmentRows.Add(MoveTemp(Row));
    }
    FString RecordBucket;
    if (!ValidateExactJsonFields(
            *DdcZen,
            {
                TEXT("graphName"),
                TEXT("directories"),
                TEXT("identitySha256"),
                TEXT("recordBucket")
            },
            TEXT("Protected-prewarm receipt DDC/Zen identity"),
            OutError) ||
        !ReadExactJsonString(
            *DdcZen,
            TEXT("graphName"),
            OutReceipt.DdcGraphName,
            OutError) ||
        !ReadExactJsonArray(
            *DdcZen, TEXT("directories"), Directories, OutError) ||
        !ReadExactJsonString(
            *DdcZen,
            TEXT("identitySha256"),
            OutReceipt.DdcZenIdentitySha256,
            OutError) ||
        !ReadExactJsonString(
            *DdcZen, TEXT("recordBucket"), RecordBucket, OutError) ||
        OutReceipt.DdcGraphName.IsEmpty() ||
        RecordBucket != TEXT("StaticMesh") ||
        !IsExactUpperHex(OutReceipt.DdcZenIdentitySha256, 64))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The protected-prewarm receipt DDC/Zen identity is malformed.");
        }
        return false;
    }
    FString PreviousDirectory;
    for (const TSharedPtr<FJsonValue>& DirectoryValue : *Directories)
    {
        FString Directory;
        if (!DirectoryValue.IsValid() ||
            DirectoryValue->Type != EJson::String ||
            !DirectoryValue->TryGetString(Directory) ||
            Directory.IsEmpty() ||
            Directory.Contains(TEXT("\\")) ||
            (!PreviousDirectory.IsEmpty() &&
             Directory.Compare(PreviousDirectory) <= 0))
        {
            OutError = TEXT("The protected-prewarm receipt DDC directory roster is not exact, normalized, sorted and unique.");
            return false;
        }
        OutReceipt.DdcDirectories.Add(Directory);
        PreviousDirectory = Directory;
    }
    if (Meshes->Num() != 2)
    {
        OutError = TEXT("The protected-prewarm receipt must contain exactly two mesh records.");
        return false;
    }
    TSet<FString> UniqueObjectPaths;
    TSet<FString> UniqueDerivedDataKeys;
    TSet<FString> UniqueCardDerivedDataKeys;
    for (const TSharedPtr<FJsonValue>& MeshValue : *Meshes)
    {
        const TSharedPtr<FJsonObject>* MeshObject = nullptr;
        if (!MeshValue.IsValid() || MeshValue->Type != EJson::Object ||
            !MeshValue->TryGetObject(MeshObject) || !MeshObject ||
            !MeshObject->IsValid())
        {
            OutError = TEXT("A protected-prewarm receipt mesh row is not an exact JSON object.");
            return false;
        }
        FProtectedPrewarmReceiptMesh Mesh;
        if (!ValidateExactJsonFields(
                **MeshObject,
                {
                    TEXT("objectPath"),
                    TEXT("packageName"),
                    TEXT("packageFile"),
                    TEXT("packageBytes"),
                    TEXT("packageSha256"),
                    TEXT("renderDerivedDataKey"),
                    TEXT("meshDataRawHash"),
                    TEXT("meshDataRawSize"),
                    TEXT("secondaryMode"),
                    TEXT("distanceFieldDerivedDataKey"),
                    TEXT("distanceFieldRawHash"),
                    TEXT("distanceFieldRawSize"),
                    TEXT("cardDerivedDataKey"),
                    TEXT("cardRawHash"),
                    TEXT("cardRawSize")
                },
                TEXT("Protected-prewarm receipt mesh row"),
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("objectPath"),
                Mesh.ObjectPath,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("packageName"),
                Mesh.PackageName,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("packageFile"),
                Mesh.PackageFile,
                OutError) ||
            !ReadExactJsonInt64(
                **MeshObject,
                TEXT("packageBytes"),
                1,
                MAX_int64,
                Mesh.PackageBytes,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("packageSha256"),
                Mesh.PackageSha256,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("renderDerivedDataKey"),
                Mesh.RenderDerivedDataKey,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("meshDataRawHash"),
                Mesh.MeshDataRawHash,
                OutError) ||
            !ReadExactJsonInt64(
                **MeshObject,
                TEXT("meshDataRawSize"),
                1,
                MAX_int64,
                Mesh.MeshDataRawSize,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("secondaryMode"),
                Mesh.SecondaryMode,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("distanceFieldDerivedDataKey"),
                Mesh.DistanceFieldDerivedDataKey,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("distanceFieldRawHash"),
                Mesh.DistanceFieldRawHash,
                OutError) ||
            !ReadExactJsonInt64(
                **MeshObject,
                TEXT("distanceFieldRawSize"),
                0,
                MAX_int64,
                Mesh.DistanceFieldRawSize,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("cardDerivedDataKey"),
                Mesh.CardDerivedDataKey,
                OutError) ||
            !ReadExactJsonString(
                **MeshObject,
                TEXT("cardRawHash"),
                Mesh.CardRawHash,
                OutError) ||
            !ReadExactJsonInt64(
                **MeshObject,
                TEXT("cardRawSize"),
                0,
                MAX_int64,
                Mesh.CardRawSize,
                OutError) ||
            Mesh.ObjectPath.IsEmpty() || Mesh.PackageName.IsEmpty() ||
            Mesh.PackageFile.IsEmpty() ||
            Mesh.RenderDerivedDataKey.IsEmpty() ||
            FPackageName::ObjectPathToPackageName(Mesh.ObjectPath) !=
                Mesh.PackageName ||
            NormalizeReceiptPath(Mesh.PackageFile) != Mesh.PackageFile ||
            !IsExactUpperHex(Mesh.PackageSha256, 64) ||
            !IsExactUpperHex(Mesh.MeshDataRawHash, 40) ||
            UniqueObjectPaths.Contains(Mesh.ObjectPath) ||
            UniqueDerivedDataKeys.Contains(Mesh.RenderDerivedDataKey))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A protected-prewarm receipt mesh identity/DDC row is malformed or duplicated.");
            }
            return false;
        }
        if (Mesh.SecondaryMode != TEXT("Card") ||
            !Mesh.DistanceFieldDerivedDataKey.IsEmpty() ||
            !Mesh.DistanceFieldRawHash.IsEmpty() ||
            Mesh.DistanceFieldRawSize != 0 ||
            Mesh.CardDerivedDataKey.IsEmpty() ||
            !IsExactUpperHex(Mesh.CardRawHash, 40) ||
            Mesh.CardRawSize <= 0 ||
            UniqueCardDerivedDataKeys.Contains(
                Mesh.CardDerivedDataKey))
        {
            OutError = TEXT("A protected-prewarm receipt secondary CARD/DIST key/hash/size row is malformed.");
            return false;
        }
        UniqueObjectPaths.Add(Mesh.ObjectPath);
        UniqueDerivedDataKeys.Add(Mesh.RenderDerivedDataKey);
        UniqueCardDerivedDataKeys.Add(Mesh.CardDerivedDataKey);
        OutReceipt.ProtectedMeshes.Add(MoveTemp(Mesh));
    }
    FGuid ReceiptGuid;
    FGuid ProducerAppGuid;
    FDateTime CreatedUtc;
    const uint64 ProducerProcessId64 = FCString::Strtoui64(
        *OutReceipt.ProducerProcessId, nullptr, 10);
    if (OutReceipt.Schema != ProtectedPrewarmReceiptSchema ||
        !FGuid::ParseExact(
            OutReceipt.ReceiptId,
            EGuidFormats::Digits,
            ReceiptGuid) ||
        !FDateTime::ParseIso8601(*OutReceipt.CreatedUtc, CreatedUtc) ||
        ProducerProcessId64 == 0 ||
        ProducerProcessId64 > MAX_uint32 ||
        LexToString(static_cast<uint32>(ProducerProcessId64)) !=
            OutReceipt.ProducerProcessId ||
        !FGuid::ParseExact(
            OutReceipt.ProducerAppInstanceId,
            EGuidFormats::Digits,
            ProducerAppGuid) ||
        OutReceipt.EngineVersion.IsEmpty() ||
        OutReceipt.EngineChangelist.IsEmpty() ||
        !IsExactUpperHex(OutReceipt.BindingSha256, 64))
    {
        OutError = TEXT("The protected-prewarm receipt schema/run/process/engine/binding identity is malformed.");
        return false;
    }
    OutProducerProcessId = static_cast<uint32>(ProducerProcessId64);
    FString ComputedBindingSha;
    if (!HashUtf8StringSha256(
            BuildProtectedPrewarmReceiptCanonical(OutReceipt),
            ComputedBindingSha) ||
        ComputedBindingSha != OutReceipt.BindingSha256)
    {
        OutError = TEXT("The protected-prewarm receipt canonical binding SHA is not exact.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool RemoveProtectedPrewarmReceiptExact(FString& OutError)
{
    const FString ReceiptPath = ProtectedPrewarmReceiptFilename();
    if (!IFileManager::Get().FileExists(*ReceiptPath))
    {
        OutError.Reset();
        return true;
    }
    if (!IFileManager::Get().Delete(*ReceiptPath, false, true, true) ||
        IFileManager::Get().FileExists(*ReceiptPath))
    {
        OutError = TEXT("Could not remove the one exact protected-prewarm receipt: ") +
            ReceiptPath;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedPrewarmSourcesUnloaded(FString& OutError)
{
    if (ProtectedDerivativeSpecs().Num() != 2)
    {
        OutError = TEXT("The protected prewarm source roster is not exactly two.");
        return false;
    }
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(Spec.SourceObjectPath);
        if (FindObject<UStaticMesh>(nullptr, *Spec.SourceObjectPath) ||
            FindPackage(nullptr, *PackageName))
        {
            OutError = TEXT("The protected prewarm receipt requires the exact source object and package to be absent in the consumer process: ") +
                Spec.SourceObjectPath;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedPrewarmReceipt(
    const TArray<FProtectedFileDigest>& ProtectedSnapshot,
    bool bRequireDifferentExitedProcess,
    bool bInvalidateOnDrift,
    FString& OutError)
{
    const FString ReceiptPath = ProtectedPrewarmReceiptFilename();
    if (!IFileManager::Get().FileExists(*ReceiptPath))
    {
        OutError = TEXT("The exact process-separated protected-prewarm receipt is absent. Run PrewarmIstanaExploreV4ProtectedReferencesForImport in a clean Entry-map editor process, close that process, then retry import in a new editor process.");
        return false;
    }
    const auto FailDrift =
        [bInvalidateOnDrift, &OutError](const FString& Failure) -> bool
    {
        OutError = Failure;
        if (bInvalidateOnDrift)
        {
            FString DeleteError;
            if (!RemoveProtectedPrewarmReceiptExact(DeleteError))
            {
                OutError += TEXT(" receiptInvalidation=FAILED: ") + DeleteError;
            }
            else
            {
                OutError += TEXT(" receiptInvalidation=complete");
            }
        }
        return false;
    };
    FProtectedPrewarmReceipt Parsed;
    uint32 ProducerProcessId = 0;
    FString ParseError;
    if (!ParseProtectedPrewarmReceipt(
            Parsed, ProducerProcessId, ParseError))
    {
        return FailDrift(
            TEXT("The protected-prewarm receipt is malformed or its canonical binding drifted: ") +
            ParseError);
    }
    if (bRequireDifferentExitedProcess)
    {
        const FString CurrentAppInstanceId =
            FApp::GetInstanceId().ToString(EGuidFormats::Digits);
        if (ProducerProcessId == FPlatformProcess::GetCurrentProcessId() ||
            Parsed.ProducerAppInstanceId == CurrentAppInstanceId)
        {
            OutError = TEXT("The protected-prewarm receipt was produced by this same editor process. Close this editor completely and run import from a new process; the valid receipt was preserved.");
            return false;
        }
        if (FPlatformProcess::IsApplicationRunning(ProducerProcessId))
        {
            OutError = TEXT("The protected-prewarm producer process is still running. Close it completely before import; the valid receipt was preserved.");
            return false;
        }
    }
    FString UnloadedError;
    if (!ValidateProtectedPrewarmSourcesUnloaded(UnloadedError))
    {
        OutError = UnloadedError +
            TEXT(" The valid receipt was preserved for retry from a clean consumer process.");
        return false;
    }
    TMap<FString, FProtectedPrewarmDerivedDataProof> DerivedDataProofs;
    for (const FProtectedPrewarmReceiptMesh& Mesh : Parsed.ProtectedMeshes)
    {
        FProtectedPrewarmDerivedDataProof Proof;
        Proof.RenderDerivedDataKey = Mesh.RenderDerivedDataKey;
        Proof.SecondaryMode = Mesh.SecondaryMode;
        Proof.DistanceFieldDerivedDataKey =
            Mesh.DistanceFieldDerivedDataKey;
        Proof.DistanceFieldRawHash = Mesh.DistanceFieldRawHash;
        Proof.DistanceFieldRawSize = Mesh.DistanceFieldRawSize;
        Proof.CardDerivedDataKey = Mesh.CardDerivedDataKey;
        Proof.CardRawHash = Mesh.CardRawHash;
        Proof.CardRawSize = Mesh.CardRawSize;
        DerivedDataProofs.Add(Mesh.ObjectPath, MoveTemp(Proof));
    }
    FProtectedPrewarmReceipt Current;
    FString CurrentError;
    if (DerivedDataProofs.Num() != 2 ||
        !CaptureCurrentProtectedPrewarmReceiptState(
            ProtectedSnapshot,
            DerivedDataProofs,
            Current,
            CurrentError))
    {
        return FailDrift(
            TEXT("The protected-prewarm receipt no longer matches the exact engine/project/contract/protected/local-DDC state: ") +
            CurrentError);
    }
    Current.ReceiptId = Parsed.ReceiptId;
    Current.CreatedUtc = Parsed.CreatedUtc;
    Current.ProducerProcessId = Parsed.ProducerProcessId;
    Current.ProducerAppInstanceId = Parsed.ProducerAppInstanceId;
    FString EnvironmentRowDifference;
    const bool bEnvironmentRowsMatch =
        CompareStaticMeshKeyEnvironmentRows(
            Parsed.StaticMeshKeyEnvironmentRows,
            Current.StaticMeshKeyEnvironmentRows,
            EnvironmentRowDifference);
    if (Current.StaticMeshKeyEnvironmentRecordCount !=
            Parsed.StaticMeshKeyEnvironmentRecordCount ||
        Current.StaticMeshKeyEnvironmentSha256 !=
            Parsed.StaticMeshKeyEnvironmentSha256 ||
        !bEnvironmentRowsMatch)
    {
        return FailDrift(FString::Printf(
            TEXT("The protected-prewarm receipt StaticMesh key environment drifted: producerRows=%d producerSha=%s currentRows=%d currentSha=%s rowDiffs=%s."),
            Parsed.StaticMeshKeyEnvironmentRecordCount,
            *Parsed.StaticMeshKeyEnvironmentSha256,
            Current.StaticMeshKeyEnvironmentRecordCount,
            *Current.StaticMeshKeyEnvironmentSha256,
            bEnvironmentRowsMatch
                ? TEXT("NONE_AGGREGATE_MISMATCH")
                : *EnvironmentRowDifference));
    }
    if (Current.DdcGraphName != Parsed.DdcGraphName ||
        Current.DdcDirectories != Parsed.DdcDirectories ||
        Current.DdcZenIdentitySha256 != Parsed.DdcZenIdentitySha256)
    {
        return FailDrift(FString::Printf(
            TEXT("The protected-prewarm receipt DDC/Zen identity drifted: producerGraph=%s producerSha=%s currentGraph=%s currentSha=%s."),
            *Parsed.DdcGraphName,
            *Parsed.DdcZenIdentitySha256,
            *Current.DdcGraphName,
            *Current.DdcZenIdentitySha256));
    }
    if (Current.ProtectedSnapshotRecordCount !=
            Parsed.ProtectedSnapshotRecordCount ||
        Current.ProtectedSnapshotSha256 !=
            Parsed.ProtectedSnapshotSha256)
    {
        return FailDrift(FString::Printf(
            TEXT("The protected-prewarm receipt protected snapshot drifted: producerRows=%d producerSha=%s currentRows=%d currentSha=%s."),
            Parsed.ProtectedSnapshotRecordCount,
            *Parsed.ProtectedSnapshotSha256,
            Current.ProtectedSnapshotRecordCount,
            *Current.ProtectedSnapshotSha256));
    }
    const auto SameReceiptFile = [](
        const FProtectedPrewarmReceiptFile& A,
        const FProtectedPrewarmReceiptFile& B) -> bool
    {
        return A.Path == B.Path && A.Bytes == B.Bytes &&
            A.Sha256 == B.Sha256;
    };
    if (!SameReceiptFile(Current.ProjectFile, Parsed.ProjectFile) ||
        !SameReceiptFile(Current.EditorModule, Parsed.EditorModule) ||
        !SameReceiptFile(
            Current.EngineBuildVersionFile,
            Parsed.EngineBuildVersionFile) ||
        !SameReceiptFile(
            Current.VegetationContract,
            Parsed.VegetationContract) ||
        Current.EngineVersion != Parsed.EngineVersion ||
        Current.EngineChangelist != Parsed.EngineChangelist)
    {
        return FailDrift(
            TEXT("The protected-prewarm receipt engine/project/module/contract file binding drifted."));
    }
    const auto SameMesh = [](
        const FProtectedPrewarmReceiptMesh& A,
        const FProtectedPrewarmReceiptMesh& B) -> bool
    {
        return A.ObjectPath == B.ObjectPath &&
            A.PackageName == B.PackageName &&
            A.PackageFile == B.PackageFile &&
            A.PackageBytes == B.PackageBytes &&
            A.PackageSha256 == B.PackageSha256 &&
            A.RenderDerivedDataKey == B.RenderDerivedDataKey &&
            A.MeshDataRawHash == B.MeshDataRawHash &&
            A.MeshDataRawSize == B.MeshDataRawSize &&
            A.SecondaryMode == B.SecondaryMode &&
            A.DistanceFieldDerivedDataKey ==
                B.DistanceFieldDerivedDataKey &&
            A.DistanceFieldRawHash == B.DistanceFieldRawHash &&
            A.DistanceFieldRawSize == B.DistanceFieldRawSize &&
            A.CardDerivedDataKey == B.CardDerivedDataKey &&
            A.CardRawHash == B.CardRawHash &&
            A.CardRawSize == B.CardRawSize;
    };
    if (Current.ProtectedMeshes.Num() != Parsed.ProtectedMeshes.Num())
    {
        return FailDrift(FString::Printf(
            TEXT("The protected-prewarm receipt mesh roster drifted: producerCount=%d currentCount=%d."),
            Parsed.ProtectedMeshes.Num(),
            Current.ProtectedMeshes.Num()));
    }
    for (int32 MeshIndex = 0;
         MeshIndex < Current.ProtectedMeshes.Num();
         ++MeshIndex)
    {
        if (!SameMesh(
                Current.ProtectedMeshes[MeshIndex],
                Parsed.ProtectedMeshes[MeshIndex]))
        {
            return FailDrift(FString::Printf(
                TEXT("The protected-prewarm receipt mesh/DDC proof drifted at index %d: producerObject=%s currentObject=%s."),
                MeshIndex,
                *Parsed.ProtectedMeshes[MeshIndex].ObjectPath,
                *Current.ProtectedMeshes[MeshIndex].ObjectPath));
        }
    }
    if (Current.V4AssetCountAtIssue != Parsed.V4AssetCountAtIssue)
    {
        return FailDrift(FString::Printf(
            TEXT("The protected-prewarm receipt V4 asset-count boundary drifted: producerCount=%d currentCount=%d."),
            Parsed.V4AssetCountAtIssue,
            Current.V4AssetCountAtIssue));
    }
    const FString ParsedCanonical =
        BuildProtectedPrewarmReceiptCanonical(Parsed);
    const FString CurrentCanonical =
        BuildProtectedPrewarmReceiptCanonical(Current);
    FString CurrentBindingSha;
    if (ParsedCanonical != CurrentCanonical ||
        !HashUtf8StringSha256(CurrentCanonical, CurrentBindingSha) ||
        CurrentBindingSha != Parsed.BindingSha256)
    {
        return FailDrift(
            TEXT("The protected-prewarm receipt canonical reconstruction drifted after all named state groups matched."));
    }
    OutError.Reset();
    return true;
}

bool ValidateCleanEntryProcessBoundary(
    int32 KnownAutoDirtyMaterialCount,
    FString& OutError)
{
    UWorld* EntryWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    if (!GEditor || GEditor->PlayWorld || KnownAutoDirtyMaterialCount != 0 ||
        !EntryWorld || !EntryWorld->GetOutermost() ||
        EntryWorld->GetOutermost()->GetName() != PrewarmEntryMapPackage ||
        EntryWorld->GetOutermost()->IsDirty())
    {
        OutError = TEXT("The process-separated protected-reference boundary requires no PIE, zero known auto-dirty protected materials, and the exact clean /Engine/Maps/Entry editor world.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool RevalidateProtectedPrewarmReceiptAfterExternalSeal(
    const TArray<FProtectedFileDigest>& ProtectedSnapshot,
    FString& OutError)
{
    const auto FailDrift = [&OutError](const FString& Failure) -> bool
    {
        OutError = Failure;
        FString DeleteError;
        if (!RemoveProtectedPrewarmReceiptExact(DeleteError))
        {
            OutError += TEXT(" receiptInvalidation=FAILED: ") + DeleteError;
        }
        else
        {
            OutError += TEXT(" receiptInvalidation=complete");
        }
        return false;
    };
    FProtectedPrewarmReceipt Parsed;
    uint32 ProducerProcessId = 0;
    FString ParseError;
    if (!ParseProtectedPrewarmReceipt(
            Parsed, ProducerProcessId, ParseError))
    {
        return FailDrift(
            TEXT("The post-seal protected-prewarm receipt is malformed or binding-drifted: ") +
            ParseError);
    }
    const FString CurrentAppInstanceId =
        FApp::GetInstanceId().ToString(EGuidFormats::Digits);
    if (ProducerProcessId == FPlatformProcess::GetCurrentProcessId() ||
        Parsed.ProducerAppInstanceId == CurrentAppInstanceId ||
        FPlatformProcess::IsApplicationRunning(ProducerProcessId) ||
        !ValidateProtectedPrewarmSourcesUnloaded(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The protected-prewarm producer boundary or protected snapshot changed after the external stage seal.");
        }
        return false;
    }
    FString ProtectedError;
    if (!ValidateProtectedPackages(ProtectedSnapshot, ProtectedError))
    {
        return FailDrift(
            TEXT("The exact protected snapshot changed after the external stage seal: ") +
            ProtectedError);
    }
    const auto SameFile = [](
        const FProtectedPrewarmReceiptFile& A,
        const FProtectedPrewarmReceiptFile& B) -> bool
    {
        return A.Path == B.Path && A.Bytes == B.Bytes &&
            A.Sha256 == B.Sha256;
    };
    FProtectedPrewarmReceiptFile CurrentProjectFile;
    FProtectedPrewarmReceiptFile CurrentEditorModule;
    FProtectedPrewarmReceiptFile CurrentEngineBuildVersionFile;
    FProtectedPrewarmReceiptFile CurrentVegetationContract;
    int32 CurrentSnapshotRecordCount = 0;
    FString CurrentSnapshotSha;
    FString CurrentDdcGraphName;
    TArray<FString> CurrentDdcDirectories;
    FString CurrentDdcIdentitySha;
    int32 CurrentStaticMeshKeyEnvironmentRecordCount = 0;
    FString CurrentStaticMeshKeyEnvironmentSha;
    TArray<FProtectedPrewarmReceiptEnvironmentRow>
        CurrentStaticMeshKeyEnvironmentRows;
    if (!CaptureReceiptFile(
            FPaths::GetProjectFilePath(),
            TEXT("Project descriptor"),
            CurrentProjectFile,
            OutError) ||
        !CaptureReceiptFile(
            FModuleManager::Get().GetModuleFilename(
                FName(TEXT("TRIADSensorFusionEditor"))),
            TEXT("Loaded TRIADSensorFusionEditor module"),
            CurrentEditorModule,
            OutError) ||
        !CaptureReceiptFile(
            FPaths::Combine(
                FPaths::EngineDir(), TEXT("Build/Build.version")),
            TEXT("Engine build-version descriptor"),
            CurrentEngineBuildVersionFile,
            OutError) ||
        !CaptureReceiptFile(
            VegetationContractFilename(),
            TEXT("Frozen Explore V4 vegetation contract"),
            CurrentVegetationContract,
            OutError) ||
        !CaptureProtectedSnapshotBinding(
            ProtectedSnapshot,
            CurrentSnapshotRecordCount,
            CurrentSnapshotSha,
            OutError) ||
        !CaptureDdcZenIdentity(
            CurrentDdcGraphName,
            CurrentDdcDirectories,
            CurrentDdcIdentitySha,
            OutError) ||
        !CaptureStaticMeshKeyEnvironmentBinding(
            CurrentStaticMeshKeyEnvironmentRecordCount,
            CurrentStaticMeshKeyEnvironmentSha,
            CurrentStaticMeshKeyEnvironmentRows,
            OutError) ||
        !SameFile(CurrentProjectFile, Parsed.ProjectFile) ||
        !SameFile(CurrentEditorModule, Parsed.EditorModule) ||
        !SameFile(
            CurrentEngineBuildVersionFile,
            Parsed.EngineBuildVersionFile) ||
        !SameFile(CurrentVegetationContract, Parsed.VegetationContract) ||
        FEngineVersion::Current().ToString(EVersionComponent::Changelist) !=
            Parsed.EngineVersion ||
        LexToString(FEngineVersion::Current().GetChangelist()) !=
            Parsed.EngineChangelist ||
        CurrentSnapshotRecordCount != Parsed.ProtectedSnapshotRecordCount ||
        CurrentSnapshotSha != Parsed.ProtectedSnapshotSha256 ||
        CurrentDdcGraphName != Parsed.DdcGraphName ||
        CurrentDdcDirectories != Parsed.DdcDirectories ||
        CurrentDdcIdentitySha != Parsed.DdcZenIdentitySha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact engine/project/contract/protected/DDC identity changed after the external stage seal.");
        }
        return FailDrift(OutError);
    }
    FString EnvironmentRowDifference;
    const bool bEnvironmentRowsMatch =
        CompareStaticMeshKeyEnvironmentRows(
            Parsed.StaticMeshKeyEnvironmentRows,
            CurrentStaticMeshKeyEnvironmentRows,
            EnvironmentRowDifference);
    if (CurrentStaticMeshKeyEnvironmentRecordCount !=
            Parsed.StaticMeshKeyEnvironmentRecordCount ||
        CurrentStaticMeshKeyEnvironmentSha !=
            Parsed.StaticMeshKeyEnvironmentSha256 ||
        !bEnvironmentRowsMatch)
    {
        return FailDrift(FString::Printf(
            TEXT("The post-seal protected-prewarm StaticMesh key environment drifted: producerRows=%d producerSha=%s currentRows=%d currentSha=%s rowDiffs=%s."),
            Parsed.StaticMeshKeyEnvironmentRecordCount,
            *Parsed.StaticMeshKeyEnvironmentSha256,
            CurrentStaticMeshKeyEnvironmentRecordCount,
            *CurrentStaticMeshKeyEnvironmentSha,
            bEnvironmentRowsMatch
                ? TEXT("NONE_AGGREGATE_MISMATCH")
                : *EnvironmentRowDifference));
    }
    if (Parsed.ProtectedMeshes.Num() != 2 ||
        ProtectedDerivativeSpecs().Num() != 2)
    {
        OutError = TEXT("The post-seal protected local-DDC roster is not exactly two.");
        return false;
    }
    for (int32 Index = 0; Index < Parsed.ProtectedMeshes.Num(); ++Index)
    {
        const FProtectedPrewarmReceiptMesh& Mesh =
            Parsed.ProtectedMeshes[Index];
        const FProtectedDerivativeSpec& Spec =
            ProtectedDerivativeSpecs()[Index];
        FString PackageFilename;
        FString MeshDataRawHash;
        int64 MeshDataRawSize = 0;
        if (Mesh.ObjectPath != Spec.SourceObjectPath ||
            Mesh.PackageName != FPackageName::ObjectPathToPackageName(
                Spec.SourceObjectPath) ||
            Mesh.PackageBytes != Spec.SourcePackageBytes ||
            Mesh.PackageSha256 != Spec.SourcePackageSha256 ||
            !ResolvePackageFileAndValidate(
                Spec.SourceObjectPath,
                Spec.SourcePackageBytes,
                Spec.SourcePackageSha256,
                PackageFilename,
                OutError) ||
            NormalizeReceiptPath(PackageFilename) != Mesh.PackageFile)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("An exact protected source identity changed after the external stage seal: ") +
                    Mesh.ObjectPath;
            }
            return FailDrift(OutError);
        }
        FProtectedPrewarmDerivedDataProof Proof;
        Proof.RenderDerivedDataKey = Mesh.RenderDerivedDataKey;
        Proof.SecondaryMode = Mesh.SecondaryMode;
        Proof.DistanceFieldDerivedDataKey =
            Mesh.DistanceFieldDerivedDataKey;
        Proof.DistanceFieldRawHash = Mesh.DistanceFieldRawHash;
        Proof.DistanceFieldRawSize = Mesh.DistanceFieldRawSize;
        Proof.CardDerivedDataKey = Mesh.CardDerivedDataKey;
        Proof.CardRawHash = Mesh.CardRawHash;
        Proof.CardRawSize = Mesh.CardRawSize;
        const FString ExpectedCardHash = Mesh.CardRawHash;
        const int64 ExpectedCardSize = Mesh.CardRawSize;
        if (!ValidateStaticMeshDerivedDataRecord(
                Mesh.RenderDerivedDataKey,
                Mesh.ObjectPath,
                MeshDataRawHash,
                MeshDataRawSize,
                OutError) ||
            !ValidateProtectedSecondaryDerivedDataProof(
                Proof, Mesh.ObjectPath, OutError) ||
            MeshDataRawHash != Mesh.MeshDataRawHash ||
            MeshDataRawSize != Mesh.MeshDataRawSize ||
            Proof.CardRawHash != ExpectedCardHash ||
            Proof.CardRawSize != ExpectedCardSize)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("An exact protected local StaticMesh MeshData/CARD DDC record changed after the external stage seal; the receipt was preserved for a bounded retry: ") +
                    Mesh.ObjectPath;
            }
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool IsProtectedRuntimeMeshReference(const UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return false;
    }
    const FString Path = Mesh->GetPathName();
    return Path == ProtectedHighForkSourceObjectPath ||
        Path == ProtectedColumnarSourceObjectPath;
}

bool IsProtectedWindRole(ETRIADIstanaExploreV4WindRole Role)
{
    switch (Role)
    {
    case ETRIADIstanaExploreV4WindRole::HighForkTrunk:
    case ETRIADIstanaExploreV4WindRole::HighForkBranch:
    case ETRIADIstanaExploreV4WindRole::HighForkLeaf:
    case ETRIADIstanaExploreV4WindRole::ColumnarTrunk:
    case ETRIADIstanaExploreV4WindRole::ColumnarBranch:
    case ETRIADIstanaExploreV4WindRole::ColumnarLeaf:
        return true;
    default:
        return false;
    }
}

int32 ExpectedSlotForWindRole(ETRIADIstanaExploreV4WindRole Role);

const FProtectedDerivativeSpec* ProtectedSpecForWindRole(
    ETRIADIstanaExploreV4WindRole Role)
{
    const FString* RuntimeAssetName = nullptr;
    switch (Role)
    {
    case ETRIADIstanaExploreV4WindRole::HighForkTrunk:
    case ETRIADIstanaExploreV4WindRole::HighForkBranch:
    case ETRIADIstanaExploreV4WindRole::HighForkLeaf:
        RuntimeAssetName = &HighForkMeshName;
        break;
    case ETRIADIstanaExploreV4WindRole::ColumnarTrunk:
    case ETRIADIstanaExploreV4WindRole::ColumnarBranch:
    case ETRIADIstanaExploreV4WindRole::ColumnarLeaf:
        RuntimeAssetName = &ColumnarMeshName;
        break;
    default:
        return nullptr;
    }
    return ProtectedDerivativeSpecs().FindByPredicate(
        [RuntimeAssetName](const FProtectedDerivativeSpec& Spec)
        {
            return RuntimeAssetName &&
                Spec.RuntimeAssetName == *RuntimeAssetName;
        });
}

bool IsExactProtectedReferenceForWindRole(
    const UStaticMesh* Mesh,
    ETRIADIstanaExploreV4WindRole Role)
{
    if (!Mesh)
    {
        return false;
    }
    switch (Role)
    {
    case ETRIADIstanaExploreV4WindRole::HighForkTrunk:
    case ETRIADIstanaExploreV4WindRole::HighForkBranch:
    case ETRIADIstanaExploreV4WindRole::HighForkLeaf:
        return Mesh->GetPathName() == ProtectedHighForkSourceObjectPath;
    case ETRIADIstanaExploreV4WindRole::ColumnarTrunk:
    case ETRIADIstanaExploreV4WindRole::ColumnarBranch:
    case ETRIADIstanaExploreV4WindRole::ColumnarLeaf:
        return Mesh->GetPathName() == ProtectedColumnarSourceObjectPath;
    default:
        return !IsProtectedRuntimeMeshReference(Mesh);
    }
}

bool ResolveProtectedMeshReferences(
    TMap<FString, UStaticMesh*>& InOutRuntimeMeshes,
    FString& OutError)
{
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        FString PackageFilename;
        UStaticMesh* Source = LoadExact<UStaticMesh>(Spec.SourceObjectPath);
        if (!ResolvePackageFileAndValidate(
                Spec.SourceObjectPath,
                Spec.SourcePackageBytes,
                Spec.SourcePackageSha256,
                PackageFilename,
                OutError) ||
            !ValidateProtectedMeshTopology(Source, Spec, OutError))
        {
            return false;
        }
        if (!Source || Source->GetOutermost()->IsDirty() ||
            FindObject<UStaticMesh>(
                nullptr, *RuntimeMeshObjectPath(Spec.RuntimeAssetName)) ||
            FPackageName::DoesPackageExist(
                MeshAssetPath + TEXT("/") + Spec.RuntimeAssetName))
        {
            OutError = TEXT("A protected read-only V4 mesh reference is dirty or shadowed by a forbidden V4-owned duplicate: ") +
                Spec.SelectionId;
            return false;
        }
        InOutRuntimeMeshes.Add(Spec.RuntimeAssetName, Source);
        Source->ClearMeshDescriptions();
        FMemory::Trim(true);
        if (Source->GetOutermost()->IsDirty())
        {
            OutError = TEXT("Clearing only transient MeshDescription caches unexpectedly dirtied a protected read-only mesh package: ") +
                Spec.SelectionId;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool UnloadResolvedProtectedMeshReferences(
    const TArray<FProtectedFileDigest>& ProtectedSnapshot,
    FString& OutError)
{
    if (ProtectedDerivativeSpecs().Num() != 2)
    {
        OutError = TEXT("Protected cold-boundary unload requires exactly two read-only source specifications.");
        return false;
    }
    int32 UnloadedCount = 0;
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        UStaticMesh* Source = FindObject<UStaticMesh>(
            nullptr, *Spec.SourceObjectPath);
        UPackage* SourcePackage = Source ? Source->GetOutermost() : nullptr;
        if (!Source || Source->GetPathName() != Spec.SourceObjectPath ||
            !SourcePackage || SourcePackage->IsDirty())
        {
            OutError = TEXT("A resolved protected mesh was absent, misresolved or dirty at the final cold boundary: ") +
                Spec.SelectionId;
            return false;
        }
        Source->ClearMeshDescriptions();
        Source = nullptr;
        if (!UnloadOneExactCleanPackage(
                Spec.SourceObjectPath,
                SourcePackage,
                false,
                TEXT("Protected cold-boundary ") + Spec.SelectionId,
                OutError))
        {
            return false;
        }
        FString PackageFilename;
        if (!ResolvePackageFileAndValidate(
                Spec.SourceObjectPath,
                Spec.SourcePackageBytes,
                Spec.SourcePackageSha256,
                PackageFilename,
                OutError))
        {
            return false;
        }
        ++UnloadedCount;
    }
    if (UnloadedCount != 2 ||
        !ValidateProtectedPackages(ProtectedSnapshot, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool DuplicateCloseTurfAndBlocker(
    IAssetTools& AssetTools,
    TMap<FString, UStaticMesh*>& InOutRuntimeMeshes,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    UStaticMesh* SourceTurf =
        LoadExact<UStaticMesh>(ProtectedCloseTurfSourceObjectPath);
    if (!SourceTurf || CountTriangles(SourceTurf, 0) != 8 ||
        SourceTurf->GetStaticMaterials().Num() != 1 ||
        ImportedSlotOrder(SourceTurf) !=
            TArray<FName>{FName(TEXT("M_IPVExploreV3_CloseTurf"))})
    {
        OutError = TEXT("The exact protected V3 eight-triangle close-turf source changed.");
        return false;
    }
    UStaticMesh* Turf = Cast<UStaticMesh>(AssetTools.DuplicateAsset(
        CloseTurfMeshName,
        MeshAssetPath,
        SourceTurf));
    if (!Turf || Turf->GetPathName() !=
            RuntimeMeshObjectPath(CloseTurfMeshName))
    {
        OutError = TEXT("Could not create the V4-owned close-turf derivative.");
        return false;
    }
    RemoveVisualCollision(Turf);
    StampDerivativeProvenance(
        Turf,
        ProtectedCloseTurfSourceObjectPath,
        TEXT("PROTECTED_V3_PACKAGE_BYTE_SNAPSHOT"),
        TEXT("exact_v3_close_turf_replacement"));
    const TArray<FLodCap> TurfCaps = {{8, 1.0f}, {4, 0.20f}};
    if (!ConfigureRuntimeDerivativeLods(Turf, 8, TurfCaps, OutError) ||
        Turf->GetMinLODIdx() != 1 || Turf->GetNumLODs() != 2 ||
        CountTriangles(Turf, 1) > 4)
    {
        return false;
    }

    UStaticMesh* EngineCylinder =
        LoadExact<UStaticMesh>(EngineCylinderObjectPath);
    UStaticMesh* Blocker = EngineCylinder
        ? Cast<UStaticMesh>(AssetTools.DuplicateAsset(
            HeritageBlockerMeshName,
            MeshAssetPath,
            EngineCylinder))
        : nullptr;
    const UBodySetup* BlockerBody = Blocker ? Blocker->GetBodySetup() : nullptr;
    if (!Blocker || Blocker->GetPathName() !=
            RuntimeMeshObjectPath(HeritageBlockerMeshName) ||
        !BlockerBody || BlockerBody->AggGeom.GetElementCount() <= 0)
    {
        OutError = TEXT("Could not create the V4-owned Heritage Pawn blocker cylinder with inherited simple collision.");
        return false;
    }
    StampDerivativeProvenance(
        Blocker,
        EngineCylinderObjectPath,
        TEXT("ENGINE_BASIC_SHAPE_SOURCE"),
        TEXT("heritage_pawn_only_blocker"));
    Blocker->MarkPackageDirty();
    InOutRuntimeMeshes.Add(CloseTurfMeshName, Turf);
    InOutRuntimeMeshes.Add(HeritageBlockerMeshName, Blocker);
    OutAssetsToSave.Add(Turf);
    OutAssetsToSave.Add(Blocker);
    OutError.Reset();
    return true;
}

const TArray<FName>& PorticoSlotOrder()
{
    // UE5.5's legacy OBJ path visits the generated group nodes in lexical
    // order.  This is the accepted imported FStaticMaterial order, not the
    // frozen OBJ/MTL first-use order.
    static const TArray<FName> Slots = {
        TEXT("M_IPV8_Portico_Trim"),
        TEXT("M_IPV8_Portico_Stone"),
        TEXT("M_IPV8_Portico_Render"),
        TEXT("M_IPV8_Portico_Soffit"),
        TEXT("M_IPV8_Portico_Recess"),
        TEXT("M_IPV8_Portico_Metal"),
        TEXT("M_IPV8_Portico_Louvre")};
    return Slots;
}

const TArray<int32>& PorticoTrianglesBySlot()
{
    static const TArray<int32> Triangles = {
        3100, 1704, 48, 1092, 36, 144, 468};
    return Triangles;
}

FString PorticoMeshObjectPath()
{
    return ObjectPath(PorticoAssetPath, PorticoMeshName);
}

bool ValidateSimpleMaterial(
    UMaterial* Material,
    const FSimpleMaterialSpec& Spec,
    FString& OutError)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const UMaterialExpressionConstant3Vector* Base = EditorOnly
        ? Cast<UMaterialExpressionConstant3Vector>(
            EditorOnly->BaseColor.Expression)
        : nullptr;
    const UMaterialExpressionConstant* Rough = EditorOnly
        ? Cast<UMaterialExpressionConstant>(
            EditorOnly->Roughness.Expression)
        : nullptr;
    const UMaterialExpressionConstant* Metal = EditorOnly
        ? Cast<UMaterialExpressionConstant>(
            EditorOnly->Metallic.Expression)
        : nullptr;
    if (!Material || !EditorOnly ||
        Material->GetPathName() != MaterialObjectPath(Spec.Name) ||
        Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5) ||
        EditorOnly->ExpressionCollection.Expressions.Num() != 3 ||
        !Base || !Rough || !Metal ||
        !Base->Constant.Equals(Spec.Color, 0.0001f) ||
        !FMath::IsNearlyEqual(Rough->R, Spec.Roughness, 0.0001f) ||
        !FMath::IsNearlyEqual(Metal->R, Spec.Metallic, 0.0001f) ||
        Material->GetBlendMode() != BLEND_Opaque ||
        Material->IsTwoSided() ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit))
    {
        OutError = TEXT("A V8 portico material graph/value changed: ") +
            Spec.Name;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidatePorticoMesh(
    UStaticMesh* Mesh,
    bool bRequireBoundMaterials,
    FString& OutError)
{
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    const UAssetImportData* ImportData = Mesh
        ? Mesh->GetAssetImportData()
        : nullptr;
    const TArray<FString> Sources = ImportData
        ? ImportData->ExtractFilenames()
        : TArray<FString>();
    FString ActualSource = Sources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(Sources[0])
        : FString();
    FString ExpectedSource = ProjectSourcePath(TEXT(
        "IstanaPublicViewV8Portico/Generated/SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live.obj"));
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(ExpectedSource);
    if (!Mesh || Mesh->GetPathName() != PorticoMeshObjectPath() ||
        Sources.Num() != 1 || !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        CountTriangles(Mesh, 0) != 6592 ||
        ImportedSlotOrder(Mesh) != PorticoSlotOrder() ||
        CountLodTrianglesByMaterialSlot(Mesh, 0) !=
            PorticoTrianglesBySlot() ||
        Mesh->NaniteSettings.bEnabled)
    {
        OutError = TEXT("V8 portico lost exact 6,592-triangle/seven-slot topology.");
        return false;
    }
    const UBodySetup* Body = Mesh->GetBodySetup();
    if (Body && (Body->AggGeom.GetElementCount() != 0 ||
                 Body->CollisionTraceFlag == CTF_UseComplexAsSimple))
    {
        OutError = TEXT("The identity V8 portico must remain render-only/NoCollision.");
        return false;
    }
    if (bRequireBoundMaterials)
    {
        for (int32 Slot = 0; Slot < PorticoSlotOrder().Num(); ++Slot)
        {
            const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
            if (!Material || Material->GetPathName() !=
                    MaterialObjectPath(PorticoSlotOrder()[Slot].ToString()))
            {
                OutError = TEXT("V8 portico lost an exact V4-owned material binding.");
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

UAssetImportTask* MakePorticoImportTask()
{
    UFbxImportUI* Options = NewObject<UFbxImportUI>();
    UFbxFactory* Factory = NewObject<UFbxFactory>();
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    if (!Options || !Factory || !Task || !Options->StaticMeshImportData)
    {
        return nullptr;
    }
    Options->bImportAsSkeletal = false;
    Options->MeshTypeToImport = FBXIT_StaticMesh;
    Options->bAutomatedImportShouldDetectType = false;
    Options->bImportMesh = true;
    Options->bImportMaterials = false;
    Options->bImportTextures = false;
    Options->StaticMeshImportData->bConvertScene = false;
    Options->StaticMeshImportData->bConvertSceneUnit = false;
    Options->StaticMeshImportData->ImportUniformScale = 1.0f;
    Options->StaticMeshImportData->bCombineMeshes = true;
    Options->StaticMeshImportData->bReorderMaterialToFbxOrder = true;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = true;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = false;
    Options->StaticMeshImportData->bRemoveDegenerates = true;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = ProjectSourcePath(TEXT(
        "IstanaPublicViewV8Portico/Generated/SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live.obj"));
    Task->DestinationPath = PorticoAssetPath;
    Task->DestinationName = PorticoMeshName;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

bool ValidatePorticoImportTask(
    const UAssetImportTask* Task,
    FString& OutError)
{
    const UFbxImportUI* Options = Task
        ? Cast<UFbxImportUI>(Task->Options)
        : nullptr;
    const UFbxStaticMeshImportData* Data = Options
        ? Options->StaticMeshImportData
        : nullptr;
    const FString ExpectedSource = ProjectSourcePath(TEXT(
        "IstanaPublicViewV8Portico/Generated/SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live.obj"));
    if (!Task || !Options || !Data ||
        !FPaths::IsSamePath(Task->Filename, ExpectedSource) ||
        Task->DestinationPath != PorticoAssetPath ||
        Task->DestinationName != PorticoMeshName ||
        Options->bImportAsSkeletal ||
        Options->MeshTypeToImport != FBXIT_StaticMesh ||
        Options->bAutomatedImportShouldDetectType ||
        !Options->bImportMesh || Options->bImportMaterials ||
        Options->bImportTextures || Data->bConvertScene ||
        Data->bConvertSceneUnit ||
        !FMath::IsNearlyEqual(Data->ImportUniformScale, 1.0f) ||
        !Data->bCombineMeshes || !Data->bReorderMaterialToFbxOrder ||
        Data->bImportMeshLODs ||
        !Data->bTransformVertexToAbsolute || Data->bBakePivotInVertex ||
        Data->bAutoGenerateCollision || !Data->bGenerateLightmapUVs ||
        Data->NormalImportMethod != EFBXNormalImportMethod::FBXNIM_ImportNormals ||
        Data->NormalGenerationMethod != EFBXNormalGenerationMethod::MikkTSpace ||
        Data->bBuildNanite || !Data->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The exact identity V8 portico import policy changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ImportAndBindPortico(
    IAssetTools& AssetTools,
    const TMap<FString, UMaterial*>& Materials,
    UStaticMesh*& OutPortico,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    UAssetImportTask* Task = MakePorticoImportTask();
    if (!Task || !ValidatePorticoImportTask(Task, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not allocate exact V8 portico import task.");
        }
        return false;
    }
    AssetTools.ImportAssetTasks({Task});
    OutPortico = nullptr;
    for (UObject* Object : Task->GetObjects())
    {
        UStaticMesh* Mesh = Cast<UStaticMesh>(Object);
        if (Mesh && Mesh->GetPathName() == PorticoMeshObjectPath())
        {
            OutPortico = Mesh;
        }
    }
    if (!OutPortico)
    {
        OutPortico = LoadExact<UStaticMesh>(PorticoMeshObjectPath());
    }
    RemoveVisualCollision(OutPortico);
    if (!ValidatePorticoMesh(OutPortico, false, OutError))
    {
        return false;
    }
    OutPortico->Modify();
    for (int32 Slot = 0; Slot < PorticoSlotOrder().Num(); ++Slot)
    {
        UMaterial* Material = Materials.FindRef(
            PorticoSlotOrder()[Slot].ToString());
        if (!Material)
        {
            OutError = TEXT("A V8 portico material asset is absent.");
            return false;
        }
        OutPortico->GetStaticMaterials()[Slot].MaterialInterface = Material;
    }
    OutPortico->PostEditChange();
    OutPortico->MarkPackageDirty();
    if (!ValidatePorticoMesh(OutPortico, true, OutError))
    {
        return false;
    }
    OutAssetsToSave.Add(OutPortico);
    OutError.Reset();
    return true;
}

FWindMaterialSpec MakeWindSpec(
    const TCHAR* AssetName,
    ETRIADIstanaExploreV4WindRole Role,
    const TCHAR* ImportedSlotName,
    const TCHAR* BaseColor,
    const TCHAR* Normal,
    const TCHAR* Roughness,
    const TCHAR* Opacity,
    float HeightCm,
    float Response,
    float MaximumWpoCm,
    bool bMasked,
    bool bTwoSided)
{
    FWindMaterialSpec Spec;
    Spec.AssetName = AssetName;
    Spec.Role = Role;
    Spec.ImportedSlotName = ImportedSlotName;
    Spec.BaseColorTexture = BaseColor;
    Spec.NormalTexture = Normal;
    Spec.RoughnessTexture = Roughness;
    Spec.OpacityTexture = Opacity;
    Spec.bMasked = bMasked;
    Spec.bTwoSidedFoliage = bTwoSided;
    Spec.HeightCm = HeightCm;
    Spec.ResponseScale = Response;
    Spec.MaximumWpoCm = MaximumWpoCm;
    return Spec;
}

double MeshHeightCm(const UStaticMesh* Mesh)
{
    return Mesh ? Mesh->GetBounds().BoxExtent.Z * 2.0 : 0.0;
}

bool BuildWindMaterialSpecs(
    const TMap<FString, UStaticMesh*>& Meshes,
    TArray<FWindMaterialSpec>& OutSpecs,
    FString& OutError)
{
    UStaticMesh* Umbrella = Meshes.FindRef(UmbrellaMeshName);
    UStaticMesh* Dome = Meshes.FindRef(DomeMeshName);
    UStaticMesh* Palm = Meshes.FindRef(PalmMeshName);
    UStaticMesh* Shrub = Meshes.FindRef(ShrubMeshName);
    UStaticMesh* Flower = Meshes.FindRef(FlowerMeshName);
    UStaticMesh* Understorey = Meshes.FindRef(UnderstoreyMeshName);
    UStaticMesh* GeometryGrass = Meshes.FindRef(GeometryGrassMeshName);
    UStaticMesh* CloseTurf = Meshes.FindRef(CloseTurfMeshName);
    UStaticMesh* HeritageBlocker =
        Meshes.FindRef(HeritageBlockerMeshName);
    const FProtectedDerivativeSpec* HighFork =
        ProtectedSpecForWindRole(
            ETRIADIstanaExploreV4WindRole::HighForkTrunk);
    const FProtectedDerivativeSpec* Columnar =
        ProtectedSpecForWindRole(
            ETRIADIstanaExploreV4WindRole::ColumnarTrunk);
    const TArray<UStaticMesh*> Required = {
        Umbrella, Dome, Palm, Shrub, Flower, Understorey,
        GeometryGrass, CloseTurf, HeritageBlocker};
    if (Required.Contains(nullptr) || !HighFork || !Columnar)
    {
        OutError = TEXT("The complete nine-owned V4 mesh roster and two frozen protected-reference fact rows are required before material authoring.");
        return false;
    }
    OutSpecs = {
        MakeWindSpec(TEXT("M_IPV4_IslandTree02_Trunk_Wind"), ETRIADIstanaExploreV4WindRole::UmbrellaTrunk, TEXT("island_tree_02"), TEXT("T_IPV4_IslandTree02_Trunk_BaseColor"), TEXT("T_IPV4_IslandTree02_Trunk_NormalDX"), TEXT("T_IPV4_IslandTree02_Trunk_Roughness"), TEXT(""), MeshHeightCm(Umbrella), 0.16f, 18.0f, false, false),
        MakeWindSpec(TEXT("M_IPV4_IslandTree02_Branches_Wind"), ETRIADIstanaExploreV4WindRole::UmbrellaBranch, TEXT("island_tree_02_branches"), TEXT("T_IPV4_IslandTree02_Branch_BaseColor"), TEXT("T_IPV4_IslandTree02_Branch_NormalDX"), TEXT("T_IPV4_IslandTree02_Branch_Roughness"), TEXT(""), MeshHeightCm(Umbrella), 0.48f, 38.0f, false, false),
        MakeWindSpec(TEXT("M_IPV4_IslandTree02_Leaves_Wind"), ETRIADIstanaExploreV4WindRole::UmbrellaLeaf, TEXT("island_tree_02_leaves"), TEXT("T_IPV4_IslandTree02_Leaf_BaseColor"), TEXT("T_IPV4_IslandTree02_Leaf_NormalDX"), TEXT("T_IPV4_IslandTree02_Leaf_Roughness"), TEXT("T_IPV4_IslandTree02_Leaf_Opacity"), MeshHeightCm(Umbrella), 1.0f, 58.0f, true, true),
        MakeWindSpec(TEXT("M_IPV4_TreeSmall02_Trunk_Wind"), ETRIADIstanaExploreV4WindRole::DomeTrunk, TEXT("tree_small_02_trunk"), TEXT("T_IPV4_TreeSmall02_Trunk_BaseColor"), TEXT("T_IPV4_TreeSmall02_Trunk_NormalDX"), TEXT("T_IPV4_TreeSmall02_Trunk_Roughness"), TEXT(""), MeshHeightCm(Dome), 0.16f, 18.0f, false, false),
        MakeWindSpec(TEXT("M_IPV4_TreeSmall02_Branches_Wind"), ETRIADIstanaExploreV4WindRole::DomeBranch, TEXT("tree_small_02_branches"), TEXT("T_IPV4_TreeSmall02_Branch_BaseColor"), TEXT("T_IPV4_TreeSmall02_Branch_NormalDX"), TEXT("T_IPV4_TreeSmall02_Branch_Roughness"), TEXT(""), MeshHeightCm(Dome), 0.48f, 38.0f, false, false),
        MakeWindSpec(TEXT("M_IPV4_TreeSmall02_Leaves_Wind"), ETRIADIstanaExploreV4WindRole::DomeLeaf, TEXT("tree_small_02_leaves"), TEXT("T_IPV4_TreeSmall02_Leaf_BaseColor"), TEXT("T_IPV4_TreeSmall02_Leaf_NormalDX"), TEXT("T_IPV4_TreeSmall02_Leaf_Roughness"), TEXT("T_IPV4_TreeSmall02_Leaf_Opacity"), MeshHeightCm(Dome), 1.0f, 58.0f, true, true),
        MakeWindSpec(TEXT("M_IPV4_IslandTree01_Trunk_Wind"), ETRIADIstanaExploreV4WindRole::HighForkTrunk, TEXT("island_tree_01"), TEXT(""), TEXT(""), TEXT(""), TEXT(""), static_cast<float>(HighFork->SourceZSpanCm), 0.16f, 18.0f, false, false),
        MakeWindSpec(TEXT("M_IPV4_IslandTree01_Branches_Wind"), ETRIADIstanaExploreV4WindRole::HighForkBranch, TEXT("island_tree_01_branches"), TEXT(""), TEXT(""), TEXT(""), TEXT(""), static_cast<float>(HighFork->SourceZSpanCm), 0.48f, 38.0f, false, false),
        MakeWindSpec(TEXT("M_IPV4_IslandTree01_Leaves_Wind"), ETRIADIstanaExploreV4WindRole::HighForkLeaf, TEXT("island_tree_01_leaves"), TEXT(""), TEXT(""), TEXT(""), TEXT(""), static_cast<float>(HighFork->SourceZSpanCm), 1.0f, 58.0f, true, true),
        MakeWindSpec(TEXT("M_IPV4_BroadleafTrunk_Wind"), ETRIADIstanaExploreV4WindRole::ColumnarTrunk, TEXT("jacaranda_tree_trunk"), TEXT(""), TEXT(""), TEXT(""), TEXT(""), static_cast<float>(Columnar->SourceZSpanCm), 0.16f, 18.0f, false, false),
        MakeWindSpec(TEXT("M_IPV4_BroadleafBranches_Wind"), ETRIADIstanaExploreV4WindRole::ColumnarBranch, TEXT("jacaranda_tree_branches"), TEXT(""), TEXT(""), TEXT(""), TEXT(""), static_cast<float>(Columnar->SourceZSpanCm), 0.48f, 38.0f, false, false),
        MakeWindSpec(TEXT("M_IPV4_BroadleafLeavesDark_Wind"), ETRIADIstanaExploreV4WindRole::ColumnarLeaf, TEXT("jacaranda_tree_leaves"), TEXT(""), TEXT(""), TEXT(""), TEXT(""), static_cast<float>(Columnar->SourceZSpanCm), 1.0f, 58.0f, true, true),
        MakeWindSpec(TEXT("M_IPV4_QuaterniusPalm_Atlas_Wind"), ETRIADIstanaExploreV4WindRole::PalmComposite, TEXT("Atlas"), TEXT("T_IPV4_QuaterniusPalm_Atlas"), TEXT(""), TEXT(""), TEXT(""), MeshHeightCm(Palm), 0.82f, 44.0f, false, false),
        MakeWindSpec(TEXT("M_IPV4_Shrub04_Wind"), ETRIADIstanaExploreV4WindRole::Shrub, TEXT("shrub_04"), TEXT("T_IPV4_Shrub04_BaseColor"), TEXT("T_IPV4_Shrub04_NormalDX"), TEXT("T_IPV4_Shrub04_Roughness"), TEXT("T_IPV4_Shrub04_Opacity"), MeshHeightCm(Shrub), 0.62f, 14.0f, true, true),
        MakeWindSpec(TEXT("M_IPV4_Periwinkle_Wind"), ETRIADIstanaExploreV4WindRole::Flower, TEXT("periwinkle_plant"), TEXT("T_IPV4_Periwinkle_BaseColor"), TEXT("T_IPV4_Periwinkle_NormalDX"), TEXT("T_IPV4_Periwinkle_Roughness"), TEXT("T_IPV4_Periwinkle_Opacity"), MeshHeightCm(Flower), 0.78f, 10.0f, true, true),
        MakeWindSpec(TEXT("M_IPV4_Calathea_Wind"), ETRIADIstanaExploreV4WindRole::Understorey, TEXT("calathea_orbifolia_01"), TEXT("T_IPV4_Calathea_BaseColor"), TEXT("T_IPV4_Calathea_NormalDX"), TEXT("T_IPV4_Calathea_Roughness"), TEXT("T_IPV4_Calathea_Opacity"), MeshHeightCm(Understorey), 0.72f, 12.0f, true, true),
        MakeWindSpec(TEXT("M_IPV4_GrassMedium_Wind"), ETRIADIstanaExploreV4WindRole::GeometryGrass, TEXT("grass_medium_01"), TEXT("T_IPV4_GrassMedium_BaseColor"), TEXT("T_IPV4_GrassMedium_NormalDX"), TEXT("T_IPV4_GrassMedium_Roughness"), TEXT("T_IPV4_GrassMedium_Opacity"), MeshHeightCm(GeometryGrass), 1.0f, 5.0f, true, true),
        MakeWindSpec(TEXT("M_IPV4_CloseTurf_Wind"), ETRIADIstanaExploreV4WindRole::CloseTurf, TEXT("M_IPVExploreV3_CloseTurf"), TEXT("T_IPV4_GrassMedium_BaseColor"), TEXT("T_IPV4_GrassMedium_NormalDX"), TEXT("T_IPV4_GrassMedium_Roughness"), TEXT("T_IPV4_GrassMedium_Opacity"), MeshHeightCm(CloseTurf), 0.32f, 1.2f, true, true)};

    struct FProtectedMaterial
    {
        ETRIADIstanaExploreV4WindRole Role;
        const TCHAR* ObjectPath;
        int64 Bytes;
        const TCHAR* Sha;
    };
    const TArray<FProtectedMaterial> Protected = {
        {ETRIADIstanaExploreV4WindRole::HighForkTrunk, TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Materials/M_IPVExploreV3_IslandTree01_Trunk_Wind.M_IPVExploreV3_IslandTree01_Trunk_Wind"), 20606, TEXT("40159AB09B45C4CE0A83C71EA54CF65CB442C6144D83BB7BD14D5FEF023C4955")},
        {ETRIADIstanaExploreV4WindRole::HighForkBranch, TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Materials/M_IPVExploreV3_IslandTree01_Branches_Wind.M_IPVExploreV3_IslandTree01_Branches_Wind"), 20389, TEXT("F7BCD17979B279D680796CB128052D7B8212DAE7CEFA8043D041F069CF413F31")},
        {ETRIADIstanaExploreV4WindRole::HighForkLeaf, TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Materials/M_IPVExploreV3_IslandTree01_Leaves_Wind.M_IPVExploreV3_IslandTree01_Leaves_Wind"), 23445, TEXT("F57969A6CB19D0EAA2F76145E58E775C9E04B4CF9078B1B1C074AAD5BE07FF83")},
        {ETRIADIstanaExploreV4WindRole::ColumnarTrunk, TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafTrunk_Wind.M_IPVExploreV2_BroadleafTrunk_Wind"), 19008, TEXT("EC8476CE12B435D2456C2EC4B06F817CAE154C17A65C911094B420F243328F8A")},
        {ETRIADIstanaExploreV4WindRole::ColumnarBranch, TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafBranches_Wind.M_IPVExploreV2_BroadleafBranches_Wind"), 19690, TEXT("A853C24BEF311FB872BEC90A64F7D9022A772C389A939ECE24BCB45104BEB28C")},
        {ETRIADIstanaExploreV4WindRole::ColumnarLeaf, TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Materials/M_IPVExploreV3_ColumnarLeavesDark_Wind.M_IPVExploreV3_ColumnarLeavesDark_Wind"), 30671, TEXT("686748D2AABBA1C45EF24F83CC9837FA08023E348CAD8BA9EA80D6FC0F317390")}};
    for (const FProtectedMaterial& Source : Protected)
    {
        FWindMaterialSpec* Spec = OutSpecs.FindByPredicate(
            [&Source](const FWindMaterialSpec& Candidate)
            {
                return Candidate.Role == Source.Role;
            });
        if (!Spec)
        {
            OutError = TEXT("A protected material role is absent from the exact 18-role roster.");
            return false;
        }
        Spec->ProtectedSourceMaterialObjectPath = Source.ObjectPath;
        Spec->ProtectedSourceMaterialBytes = Source.Bytes;
        Spec->ProtectedSourceMaterialSha256 = Source.Sha;
    }
    TSet<uint8> Roles;
    TSet<FString> Names;
    for (const FWindMaterialSpec& Spec : OutSpecs)
    {
        Roles.Add(static_cast<uint8>(Spec.Role));
        Names.Add(Spec.AssetName);
        if (!FMath::IsFinite(Spec.HeightCm) || Spec.HeightCm <= 0.01f)
        {
            OutError = TEXT("A V4 wind material was not calibrated to its exact mesh Z span.");
            return false;
        }
    }
    if (OutSpecs.Num() != 18 || Roles.Num() != 18 || Names.Num() != 18)
    {
        OutError = TEXT("V4 requires exactly one unique persistent base material per wind role.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool IsOldWindExpression(const UMaterialExpression* Expression)
{
    if (const UMaterialExpressionCustom* Custom =
            Cast<UMaterialExpressionCustom>(Expression))
    {
        return Custom->Description.Contains(TEXT("DIRECTIONAL_GUST_WPO")) ||
            Custom->Description.Contains(TEXT("INSTANCE_LOCAL_PIVOT_UNDERDAMPED_WPO"));
    }
    if (Cast<UMaterialExpressionWorldPosition>(Expression) ||
        Cast<UMaterialExpressionObjectPositionWS>(Expression) ||
        Cast<UMaterialExpressionTransformPosition>(Expression) ||
        Cast<UMaterialExpressionTime>(Expression))
    {
        return true;
    }
    if (const UMaterialExpressionScalarParameter* Scalar =
            Cast<UMaterialExpressionScalarParameter>(Expression))
    {
        return Scalar->ParameterName == TEXT("TRIAD_WindStrengthCm") ||
            Scalar->ParameterName == TEXT("TRIAD_WindSpeed") ||
            Scalar->ParameterName == TEXT("TRIAD_WindHeightCm") ||
            Scalar->ParameterName == TEXT("TRIAD_WindResponseScale") ||
            Scalar->ParameterName == TEXT("TRIAD_MaxWpoCm");
    }
    if (const UMaterialExpressionVectorParameter* Vector =
            Cast<UMaterialExpressionVectorParameter>(Expression))
    {
        return Vector->ParameterName == TEXT("TRIAD_WindDirection");
    }
    return false;
}

UMaterial* DuplicateAndUpgradeProtectedWindMaterial(
    IAssetTools& AssetTools,
    const FWindMaterialSpec& Spec,
    FString& OutError)
{
    FString SourceFilename;
    UMaterial* Source = LoadExact<UMaterial>(
        Spec.ProtectedSourceMaterialObjectPath);
    if (!ResolvePackageFileAndValidate(
            Spec.ProtectedSourceMaterialObjectPath,
            Spec.ProtectedSourceMaterialBytes,
            Spec.ProtectedSourceMaterialSha256,
            SourceFilename,
            OutError) ||
        !Source || Source->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A protected wind-material source is invalid: ") +
                Spec.AssetName;
        }
        return nullptr;
    }
    UMaterial* Material = Cast<UMaterial>(AssetTools.DuplicateAsset(
        Spec.AssetName,
        MaterialAssetPath,
        Source));
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        Material->GetPathName() != MaterialObjectPath(Spec.AssetName))
    {
        OutError = TEXT("Could not duplicate a protected material into the exact V4 namespace: ") +
            Spec.AssetName;
        return nullptr;
    }
    Material->Modify();
    UMaterialExpression* OriginalNormal = EditorOnly->Normal.Expression;
    const int32 OriginalNormalOutput = EditorOnly->Normal.OutputIndex;
    EditorOnly->WorldPositionOffset.Expression = nullptr;
    EditorOnly->WorldPositionOffset.OutputIndex = 0;
    TArray<UMaterialExpression*> OldWindExpressions;
    for (UMaterialExpression* Expression :
         EditorOnly->ExpressionCollection.Expressions)
    {
        if (IsOldWindExpression(Expression))
        {
            OldWindExpressions.Add(Expression);
        }
    }
    for (UMaterialExpression* Expression : OldWindExpressions)
    {
        Material->GetExpressionCollection().RemoveExpression(Expression);
    }
    if (Spec.bTwoSidedFoliage)
    {
        UMaterialExpression* Corrected = AddTwoSidedSignNormalCorrection(
            Material,
            OriginalNormal,
            OriginalNormalOutput,
            OutError);
        if (!Corrected)
        {
            return nullptr;
        }
        EditorOnly->Normal.Connect(0, Corrected);
    }
    if (!AddInstanceLocalWindGraph(Material, Spec, OutError))
    {
        return nullptr;
    }
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->MaxWorldPositionOffsetDisplacement = Spec.MaximumWpoCm;
    UMetaData* Metadata = Material->GetOutermost()->GetMetaData();
    Metadata->SetValue(Material, TEXT("TRIAD_IPV4_ProtectedMaterialSource"),
        *Spec.ProtectedSourceMaterialObjectPath);
    Metadata->SetValue(Material, TEXT("TRIAD_IPV4_ProtectedMaterialSha256"),
        *Spec.ProtectedSourceMaterialSha256);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateWindMaterial(Material, Spec, OutError))
    {
        return nullptr;
    }
    OutError.Reset();
    return Material;
}

UStaticMesh* MeshForWindRole(
    const TMap<FString, UStaticMesh*>& Meshes,
    ETRIADIstanaExploreV4WindRole Role)
{
    switch (Role)
    {
    case ETRIADIstanaExploreV4WindRole::UmbrellaTrunk:
    case ETRIADIstanaExploreV4WindRole::UmbrellaBranch:
    case ETRIADIstanaExploreV4WindRole::UmbrellaLeaf:
        return Meshes.FindRef(UmbrellaMeshName);
    case ETRIADIstanaExploreV4WindRole::DomeTrunk:
    case ETRIADIstanaExploreV4WindRole::DomeBranch:
    case ETRIADIstanaExploreV4WindRole::DomeLeaf:
        return Meshes.FindRef(DomeMeshName);
    case ETRIADIstanaExploreV4WindRole::HighForkTrunk:
    case ETRIADIstanaExploreV4WindRole::HighForkBranch:
    case ETRIADIstanaExploreV4WindRole::HighForkLeaf:
        return Meshes.FindRef(HighForkMeshName);
    case ETRIADIstanaExploreV4WindRole::ColumnarTrunk:
    case ETRIADIstanaExploreV4WindRole::ColumnarBranch:
    case ETRIADIstanaExploreV4WindRole::ColumnarLeaf:
        return Meshes.FindRef(ColumnarMeshName);
    case ETRIADIstanaExploreV4WindRole::PalmComposite:
        return Meshes.FindRef(PalmMeshName);
    case ETRIADIstanaExploreV4WindRole::Shrub:
        return Meshes.FindRef(ShrubMeshName);
    case ETRIADIstanaExploreV4WindRole::Flower:
        return Meshes.FindRef(FlowerMeshName);
    case ETRIADIstanaExploreV4WindRole::Understorey:
        return Meshes.FindRef(UnderstoreyMeshName);
    case ETRIADIstanaExploreV4WindRole::GeometryGrass:
        return Meshes.FindRef(GeometryGrassMeshName);
    case ETRIADIstanaExploreV4WindRole::CloseTurf:
        return Meshes.FindRef(CloseTurfMeshName);
    default:
        return nullptr;
    }
}

int32 FindExactImportedSlot(
    const UStaticMesh* Mesh,
    FName ImportedSlot)
{
    if (!Mesh)
    {
        return INDEX_NONE;
    }
    int32 Found = INDEX_NONE;
    for (int32 Index = 0; Index < Mesh->GetStaticMaterials().Num(); ++Index)
    {
        const FStaticMaterial& Slot = Mesh->GetStaticMaterials()[Index];
        if (Slot.MaterialSlotName != Slot.ImportedMaterialSlotName)
        {
            return INDEX_NONE;
        }
        if (Slot.ImportedMaterialSlotName == ImportedSlot)
        {
            if (Found != INDEX_NONE)
            {
                return INDEX_NONE;
            }
            Found = Index;
        }
    }
    return Found;
}

bool ValidateFrozenProtectedWindBinding(
    const FWindMaterialSpec& WindSpec,
    FString& OutError)
{
    const FProtectedDerivativeSpec* ProtectedSpec =
        ProtectedSpecForWindRole(WindSpec.Role);
    const int32 Slot = ExpectedSlotForWindRole(WindSpec.Role);
    if (!ProtectedSpec || Slot == INDEX_NONE ||
        !ProtectedSpec->SlotOrder.IsValidIndex(Slot) ||
        ProtectedSpec->SlotOrder[Slot] != WindSpec.ImportedSlotName ||
        ProtectedSpec->SourceObjectPath.IsEmpty() ||
        ProtectedSpec->SourcePackageBytes <= 0 ||
        ProtectedSpec->SourcePackageSha256.Len() != 64 ||
        ProtectedSpec->SourceZSpanCm <= 0.0 ||
        !FMath::IsNearlyEqual(
            WindSpec.HeightCm,
            static_cast<float>(ProtectedSpec->SourceZSpanCm),
            0.001f))
    {
        OutError = TEXT("A protected V1/V3 wind role no longer matches its frozen exact source path/hash/height/imported-slot binding: ") +
            WindSpec.AssetName;
        return false;
    }
    OutError.Reset();
    return true;
}

bool BindWindMaterialsToRuntimeMeshes(
    const TMap<FString, UStaticMesh*>& Meshes,
    const TArray<FWindMaterialSpec>& Specs,
    const TMap<FString, UMaterial*>& Materials,
    FString& OutError)
{
    TSet<uint64> BoundOwnedSlots;
    TSet<uint8> BoundRoles;
    TArray<UStaticMesh*> MutatedOwnedMeshes;
    for (const FWindMaterialSpec& Spec : Specs)
    {
        UMaterial* Material = Materials.FindRef(Spec.AssetName);
        const uint8 RoleKey = static_cast<uint8>(Spec.Role);
        if (!Material || BoundRoles.Contains(RoleKey))
        {
            OutError = TEXT("A V4 wind role could not bind one unique exact material role: ") +
                Spec.AssetName;
            return false;
        }
        if (IsProtectedWindRole(Spec.Role))
        {
            if (!ValidateFrozenProtectedWindBinding(Spec, OutError) ||
                MeshForWindRole(Meshes, Spec.Role) != nullptr)
            {
                if (OutError.IsEmpty())
                {
                    OutError = TEXT("Protected V1/V3 mesh objects must remain unloaded during V4 material authoring: ") +
                        Spec.AssetName;
                }
                return false;
            }
        }
        else
        {
            UStaticMesh* Mesh = MeshForWindRole(Meshes, Spec.Role);
            const int32 Slot =
                FindExactImportedSlot(Mesh, Spec.ImportedSlotName);
            const uint64 Key =
                (static_cast<uint64>(GetTypeHash(Mesh)) << 32u) |
                static_cast<uint32>(Slot);
            if (!Mesh || Slot == INDEX_NONE ||
                BoundOwnedSlots.Contains(Key) ||
                !IsExactProtectedReferenceForWindRole(Mesh, Spec.Role))
            {
                OutError = TEXT("An owned V4 wind role could not bind one unique exact imported slot: ") +
                    Spec.AssetName;
                return false;
            }
            Mesh->Modify();
            Mesh->GetStaticMaterials()[Slot].MaterialInterface = Material;
            Mesh->PostEditChange();
            Mesh->MarkPackageDirty();
            MutatedOwnedMeshes.AddUnique(Mesh);
            BoundOwnedSlots.Add(Key);
        }
        BoundRoles.Add(RoleKey);
    }
    if (BoundRoles.Num() != 18 || BoundOwnedSlots.Num() != 12)
    {
        OutError = TEXT("The exact 18 role plan and 12 owned role-to-imported-slot bindings were not persisted.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(MutatedOwnedMeshes);
    OutError.Reset();
    return true;
}

FString LawnBaseMaterialObjectPath()
{
    return MaterialObjectPath(LawnBaseMaterialName);
}

FString LawnMaterialInstanceObjectPath()
{
    return MaterialObjectPath(LawnMaterialInstanceName);
}

UMaterialInstanceConstant* CreateStaticLawnMaterial(
    IAssetTools& AssetTools,
    const TMap<FString, UTexture2D*>& Textures,
    UMaterial*& OutBase,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    UTexture2D* BaseColor = Textures.FindRef(TEXT("T_IPV4_Grass001_BaseColor"));
    UTexture2D* NormalDx = Textures.FindRef(TEXT("T_IPV4_Grass001_NormalDX"));
    UTexture2D* Roughness = Textures.FindRef(TEXT("T_IPV4_Grass001_Roughness"));
    UTexture2D* AmbientOcclusion =
        Textures.FindRef(TEXT("T_IPV4_Grass001_AmbientOcclusion"));
    UMaterialFactoryNew* BaseFactory = NewObject<UMaterialFactoryNew>();
    OutBase = BaseFactory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            LawnBaseMaterialName,
            MaterialAssetPath,
            UMaterial::StaticClass(),
            BaseFactory,
            FName(TEXT("TRIAD.ImportIstanaExploreV4Assets"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = OutBase
        ? OutBase->GetEditorOnlyData()
        : nullptr;
    if (!OutBase || !EditorOnly || !BaseColor || !NormalDx || !Roughness ||
        !AmbientOcclusion ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create the exact static ambientCG Grass001 lawn base.");
        return nullptr;
    }
    UMaterialExpressionWorldPosition* World =
        AddMaterialExpression<UMaterialExpressionWorldPosition>(
            OutBase, -1500, 420);
    UMaterialExpressionComponentMask* XY =
        AddMaterialExpression<UMaterialExpressionComponentMask>(
            OutBase, -1320, 420);
    UMaterialExpressionConstant* TileSizeCm =
        AddMaterialExpression<UMaterialExpressionConstant>(
            OutBase, -1320, 540);
    UMaterialExpressionDivide* DetailUv =
        AddMaterialExpression<UMaterialExpressionDivide>(
            OutBase, -1120, 380);
    UMaterialExpressionComponentMask* DetailU =
        AddMaterialExpression<UMaterialExpressionComponentMask>(
            OutBase, -920, 500);
    UMaterialExpressionComponentMask* DetailV =
        AddMaterialExpression<UMaterialExpressionComponentMask>(
            OutBase, -920, 580);
    UMaterialExpressionConstant* NegativeOne =
        AddMaterialExpression<UMaterialExpressionConstant>(
            OutBase, -920, 660);
    UMaterialExpressionMultiply* NegativeU =
        AddMaterialExpression<UMaterialExpressionMultiply>(
            OutBase, -720, 540);
    UMaterialExpressionAppendVector* RotatedUv =
        AddMaterialExpression<UMaterialExpressionAppendVector>(
            OutBase, -540, 500);
    UMaterialExpressionConstant2Vector* DetailOffset =
        AddMaterialExpression<UMaterialExpressionConstant2Vector>(
            OutBase, -540, 620);
    UMaterialExpressionAdd* RotatedOffsetUv =
        AddMaterialExpression<UMaterialExpressionAdd>(
            OutBase, -340, 520);
    UMaterialExpressionConstant* MacroTileSizeCm =
        AddMaterialExpression<UMaterialExpressionConstant>(
            OutBase, -1320, 700);
    UMaterialExpressionDivide* MacroUv =
        AddMaterialExpression<UMaterialExpressionDivide>(
            OutBase, -1120, 700);

    UMaterialExpressionTextureSampleParameter2D* BaseSample = AddTextureSample(
        OutBase, BaseColor, TEXT("BaseColorTexture"),
        SAMPLERTYPE_Color, -100, -260);
    UMaterialExpressionTextureSampleParameter2D* BaseRotatedSample =
        AddTextureSample(
            OutBase, BaseColor, TEXT("BaseColorTextureRotated"),
            SAMPLERTYPE_Color, -100, -100);
    UMaterialExpressionTextureSampleParameter2D* NormalSample = AddTextureSample(
        OutBase, NormalDx, TEXT("NormalDXTexture"),
        SAMPLERTYPE_Normal, -100, 80);
    UMaterialExpressionTextureSampleParameter2D* NormalRotatedSample =
        AddTextureSample(
            OutBase, NormalDx, TEXT("NormalDXTextureRotated"),
            SAMPLERTYPE_Normal, -100, 160);
    UMaterialExpressionTextureSampleParameter2D* RoughSample = AddTextureSample(
        OutBase, Roughness, TEXT("RoughnessTexture"),
        SAMPLERTYPE_Masks, -100, 250);
    UMaterialExpressionTextureSampleParameter2D* RoughRotatedSample =
        AddTextureSample(
            OutBase, Roughness, TEXT("RoughnessTextureRotated"),
            SAMPLERTYPE_Masks, -100, 410);
    UMaterialExpressionTextureSampleParameter2D* AoSample = AddTextureSample(
        OutBase, AmbientOcclusion, TEXT("AmbientOcclusionTexture"),
        SAMPLERTYPE_Masks, -100, 570);
    UMaterialExpressionTextureSampleParameter2D* AoRotatedSample =
        AddTextureSample(
            OutBase, AmbientOcclusion,
            TEXT("AmbientOcclusionTextureRotated"),
            SAMPLERTYPE_Masks, -100, 650);
    UMaterialExpressionTextureSampleParameter2D* MacroSample = AddTextureSample(
        OutBase, AmbientOcclusion, TEXT("MacroVariationTexture"),
        SAMPLERTYPE_Masks, -100, 730);

    UMaterialExpressionLinearInterpolate* DetailBlend =
        AddMaterialExpression<UMaterialExpressionLinearInterpolate>(
            OutBase, 140, -190);
    UMaterialExpressionConstant3Vector* TintLow =
        AddMaterialExpression<UMaterialExpressionConstant3Vector>(
            OutBase, 140, -20);
    UMaterialExpressionConstant3Vector* TintHigh =
        AddMaterialExpression<UMaterialExpressionConstant3Vector>(
            OutBase, 140, 60);
    UMaterialExpressionLinearInterpolate* TintBlend =
        AddMaterialExpression<UMaterialExpressionLinearInterpolate>(
            OutBase, 340, 10);
    UMaterialExpressionMultiply* FinalColor =
        AddMaterialExpression<UMaterialExpressionMultiply>(
            OutBase, 560, -150);
    UMaterialExpressionLinearInterpolate* RoughDetailBlend =
        AddMaterialExpression<UMaterialExpressionLinearInterpolate>(
            OutBase, 140, 300);
    UMaterialExpressionConstant* RoughScaleLow =
        AddMaterialExpression<UMaterialExpressionConstant>(
            OutBase, 140, 440);
    UMaterialExpressionConstant* RoughScaleHigh =
        AddMaterialExpression<UMaterialExpressionConstant>(
            OutBase, 140, 510);
    UMaterialExpressionLinearInterpolate* RoughScaleBlend =
        AddMaterialExpression<UMaterialExpressionLinearInterpolate>(
            OutBase, 340, 460);
    UMaterialExpressionMultiply* FinalRoughness =
        AddMaterialExpression<UMaterialExpressionMultiply>(
            OutBase, 560, 320);
    UMaterialExpressionComponentMask* RotatedNormalX =
        AddMaterialExpression<UMaterialExpressionComponentMask>(
            OutBase, 140, 590);
    UMaterialExpressionComponentMask* RotatedNormalY =
        AddMaterialExpression<UMaterialExpressionComponentMask>(
            OutBase, 140, 650);
    UMaterialExpressionComponentMask* RotatedNormalZ =
        AddMaterialExpression<UMaterialExpressionComponentMask>(
            OutBase, 140, 710);
    UMaterialExpressionMultiply* NegativeRotatedNormalY =
        AddMaterialExpression<UMaterialExpressionMultiply>(
            OutBase, 340, 620);
    UMaterialExpressionAppendVector* ReorientedNormalXY =
        AddMaterialExpression<UMaterialExpressionAppendVector>(
            OutBase, 520, 620);
    UMaterialExpressionAppendVector* ReorientedNormalXYZ =
        AddMaterialExpression<UMaterialExpressionAppendVector>(
            OutBase, 700, 620);
    UMaterialExpressionLinearInterpolate* NormalDetailBlend =
        AddMaterialExpression<UMaterialExpressionLinearInterpolate>(
            OutBase, 880, 600);
    UMaterialExpressionNormalize* NormalizedDetailNormal =
        AddMaterialExpression<UMaterialExpressionNormalize>(
            OutBase, 1080, 600);
    UMaterialExpressionLinearInterpolate* AoDetailBlend =
        AddMaterialExpression<UMaterialExpressionLinearInterpolate>(
            OutBase, 340, 760);
    if (!World || !XY || !TileSizeCm || !DetailUv || !DetailU ||
        !DetailV || !NegativeOne || !NegativeU || !RotatedUv ||
        !DetailOffset || !RotatedOffsetUv || !MacroTileSizeCm || !MacroUv ||
        !BaseSample || !BaseRotatedSample || !NormalSample ||
        !NormalRotatedSample || !RoughSample ||
        !RoughRotatedSample || !AoSample || !AoRotatedSample ||
        !MacroSample || !DetailBlend ||
        !TintLow || !TintHigh || !TintBlend || !FinalColor ||
        !RoughDetailBlend || !RoughScaleLow || !RoughScaleHigh ||
        !RoughScaleBlend || !FinalRoughness || !RotatedNormalX ||
        !RotatedNormalY || !RotatedNormalZ ||
        !NegativeRotatedNormalY || !ReorientedNormalXY ||
        !ReorientedNormalXYZ || !NormalDetailBlend ||
        !NormalizedDetailNormal || !AoDetailBlend)
    {
        OutError = TEXT("Could not allocate the exact static multi-scale anti-tiling lawn graph.");
        return nullptr;
    }
    World->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    XY->R = true;
    XY->G = true;
    XY->B = false;
    XY->A = false;
    XY->Input.Connect(0, World);
    TileSizeCm->R = 140.0f;
    DetailUv->A.Connect(0, XY);
    DetailUv->B.Connect(0, TileSizeCm);
    DetailU->R = true;
    DetailU->G = false;
    DetailU->B = false;
    DetailU->A = false;
    DetailU->Input.Connect(0, DetailUv);
    DetailV->R = false;
    DetailV->G = true;
    DetailV->B = false;
    DetailV->A = false;
    DetailV->Input.Connect(0, DetailUv);
    NegativeOne->R = -1.0f;
    NegativeU->A.Connect(0, DetailU);
    NegativeU->B.Connect(0, NegativeOne);
    RotatedUv->A.Connect(0, DetailV);
    RotatedUv->B.Connect(0, NegativeU);
    DetailOffset->R = 19.25f;
    DetailOffset->G = -31.75f;
    RotatedOffsetUv->A.Connect(0, RotatedUv);
    RotatedOffsetUv->B.Connect(0, DetailOffset);
    MacroTileSizeCm->R = 3200.0f;
    MacroUv->A.Connect(0, XY);
    MacroUv->B.Connect(0, MacroTileSizeCm);

    BaseSample->Coordinates.Connect(0, DetailUv);
    BaseRotatedSample->Coordinates.Connect(0, RotatedOffsetUv);
    NormalSample->Coordinates.Connect(0, DetailUv);
    NormalRotatedSample->Coordinates.Connect(0, RotatedOffsetUv);
    RoughSample->Coordinates.Connect(0, DetailUv);
    RoughRotatedSample->Coordinates.Connect(0, RotatedOffsetUv);
    AoSample->Coordinates.Connect(0, DetailUv);
    AoRotatedSample->Coordinates.Connect(0, RotatedOffsetUv);
    MacroSample->Coordinates.Connect(0, MacroUv);
    DetailBlend->A.Connect(0, BaseSample);
    DetailBlend->B.Connect(0, BaseRotatedSample);
    DetailBlend->Alpha.Connect(1, MacroSample);
    TintLow->Constant = FLinearColor(0.93f, 0.96f, 0.92f, 1.0f);
    TintHigh->Constant = FLinearColor(1.04f, 1.01f, 0.95f, 1.0f);
    TintBlend->A.Connect(0, TintLow);
    TintBlend->B.Connect(0, TintHigh);
    TintBlend->Alpha.Connect(1, MacroSample);
    FinalColor->A.Connect(0, DetailBlend);
    FinalColor->B.Connect(0, TintBlend);
    RoughDetailBlend->A.Connect(1, RoughSample);
    RoughDetailBlend->B.Connect(1, RoughRotatedSample);
    RoughDetailBlend->Alpha.Connect(1, MacroSample);
    RoughScaleLow->R = 0.92f;
    RoughScaleHigh->R = 1.0f;
    RoughScaleBlend->A.Connect(0, RoughScaleLow);
    RoughScaleBlend->B.Connect(0, RoughScaleHigh);
    RoughScaleBlend->Alpha.Connect(1, MacroSample);
    FinalRoughness->A.Connect(0, RoughDetailBlend);
    FinalRoughness->B.Connect(0, RoughScaleBlend);
    RotatedNormalX->R = true;
    RotatedNormalX->G = false;
    RotatedNormalX->B = false;
    RotatedNormalX->A = false;
    RotatedNormalX->Input.Connect(0, NormalRotatedSample);
    RotatedNormalY->R = false;
    RotatedNormalY->G = true;
    RotatedNormalY->B = false;
    RotatedNormalY->A = false;
    RotatedNormalY->Input.Connect(0, NormalRotatedSample);
    RotatedNormalZ->R = false;
    RotatedNormalZ->G = false;
    RotatedNormalZ->B = true;
    RotatedNormalZ->A = false;
    RotatedNormalZ->Input.Connect(0, NormalRotatedSample);
    // Rotated UV is (V,-U), so the decoded tangent normal is reoriented
    // back to the lawn tangent frame as (-Y,X,Z) before blending.
    NegativeRotatedNormalY->A.Connect(0, RotatedNormalY);
    NegativeRotatedNormalY->B.Connect(0, NegativeOne);
    ReorientedNormalXY->A.Connect(0, NegativeRotatedNormalY);
    ReorientedNormalXY->B.Connect(0, RotatedNormalX);
    ReorientedNormalXYZ->A.Connect(0, ReorientedNormalXY);
    ReorientedNormalXYZ->B.Connect(0, RotatedNormalZ);
    NormalDetailBlend->A.Connect(0, NormalSample);
    NormalDetailBlend->B.Connect(0, ReorientedNormalXYZ);
    NormalDetailBlend->Alpha.Connect(1, MacroSample);
    NormalizedDetailNormal->VectorInput.Connect(0, NormalDetailBlend);
    AoDetailBlend->A.Connect(1, AoSample);
    AoDetailBlend->B.Connect(1, AoRotatedSample);
    AoDetailBlend->Alpha.Connect(1, MacroSample);
    OutBase->Modify();
    OutBase->MaterialDomain = MD_Surface;
    OutBase->BlendMode = BLEND_Opaque;
    OutBase->TwoSided = false;
    OutBase->SetShadingModel(MSM_DefaultLit);
    OutBase->bUsedWithInstancedStaticMeshes = true;
    OutBase->MaxWorldPositionOffsetDisplacement = 0.0f;
    EditorOnly->BaseColor.Connect(0, FinalColor);
    EditorOnly->Normal.Connect(0, NormalizedDetailNormal);
    EditorOnly->Roughness.Connect(0, FinalRoughness);
    EditorOnly->AmbientOcclusion.Connect(0, AoDetailBlend);
    UMaterialEditingLibrary::RecompileMaterial(OutBase);
    OutBase->PostEditChange();
    OutBase->MarkPackageDirty();

    UMaterialInstanceConstantFactoryNew* InstanceFactory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    if (InstanceFactory)
    {
        InstanceFactory->InitialParent = OutBase;
    }
    UMaterialInstanceConstant* Instance = InstanceFactory
        ? Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
            LawnMaterialInstanceName,
            MaterialAssetPath,
            UMaterialInstanceConstant::StaticClass(),
            InstanceFactory,
            FName(TEXT("TRIAD.ImportIstanaExploreV4Assets"))))
        : nullptr;
    if (!Instance)
    {
        OutError = TEXT("Could not create the exact derived static lawn material instance.");
        return nullptr;
    }
    Instance->SetParentEditorOnly(OutBase);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    OutAssetsToSave.Add(OutBase);
    OutAssetsToSave.Add(Instance);
    OutError.Reset();
    return Instance;
}

bool ValidateStaticLawnMaterial(
    UMaterial* Base,
    UMaterialInstanceConstant* Instance,
    FString& OutError)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    const UMaterialEditorOnlyData* EditorOnly = Base
        ? Base->GetEditorOnlyData()
        : nullptr;
    TArray<const UMaterialExpressionWorldPosition*> Worlds;
    TArray<const UMaterialExpressionComponentMask*> Masks;
    TArray<const UMaterialExpressionConstant*> Scalars;
    TArray<const UMaterialExpressionConstant2Vector*> Vector2s;
    TArray<const UMaterialExpressionConstant3Vector*> Vector3s;
    TArray<const UMaterialExpressionDivide*> Divides;
    TArray<const UMaterialExpressionMultiply*> Multiplies;
    TArray<const UMaterialExpressionAppendVector*> Appends;
    TArray<const UMaterialExpressionAdd*> Adds;
    TArray<const UMaterialExpressionLinearInterpolate*> Lerps;
    TArray<const UMaterialExpressionNormalize*> Normalizes;
    TMap<FName, const UMaterialExpressionTextureSampleParameter2D*> Samples;
    int32 CustomCount = 0;
    int32 TransformCount = 0;
    if (EditorOnly)
    {
        for (const UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            if (const auto* Candidate =
                    Cast<UMaterialExpressionWorldPosition>(Expression))
                Worlds.Add(Candidate);
            if (const auto* Candidate =
                    Cast<UMaterialExpressionComponentMask>(Expression))
                Masks.Add(Candidate);
            if (const auto* Candidate =
                    Cast<UMaterialExpressionConstant>(Expression))
                Scalars.Add(Candidate);
            if (const auto* Candidate =
                    Cast<UMaterialExpressionConstant2Vector>(Expression))
                Vector2s.Add(Candidate);
            if (const auto* Candidate =
                    Cast<UMaterialExpressionConstant3Vector>(Expression))
                Vector3s.Add(Candidate);
            if (const auto* Candidate =
                    Cast<UMaterialExpressionDivide>(Expression))
                Divides.Add(Candidate);
            if (const auto* Candidate =
                    Cast<UMaterialExpressionMultiply>(Expression))
                Multiplies.Add(Candidate);
            if (const auto* Candidate =
                    Cast<UMaterialExpressionAppendVector>(Expression))
                Appends.Add(Candidate);
            if (const auto* Candidate = Cast<UMaterialExpressionAdd>(Expression))
                Adds.Add(Candidate);
            if (const auto* Candidate =
                    Cast<UMaterialExpressionLinearInterpolate>(Expression))
                Lerps.Add(Candidate);
            if (const auto* Candidate =
                    Cast<UMaterialExpressionNormalize>(Expression))
                Normalizes.Add(Candidate);
            if (const UMaterialExpressionTextureSampleParameter2D* Sample =
                    Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
            {
                Samples.Add(Sample->ParameterName, Sample);
            }
            CustomCount += Cast<UMaterialExpressionCustom>(Expression) ? 1 : 0;
            TransformCount +=
                Cast<UMaterialExpressionTransformPosition>(Expression) ? 1 : 0;
        }
    }
    const auto InputIs = [](
        const FExpressionInput& Input,
        const UMaterialExpression* Expression,
        int32 OutputIndex = 0)
    {
        return Input.Expression == Expression &&
            Input.OutputIndex == OutputIndex;
    };
    const auto FindScalar = [&Scalars](float Value)
    {
        for (const UMaterialExpressionConstant* Scalar : Scalars)
        {
            if (FMath::IsNearlyEqual(Scalar->R, Value, 0.0001f))
            {
                return Scalar;
            }
        }
        return static_cast<const UMaterialExpressionConstant*>(nullptr);
    };
    const auto FindVector3 = [&Vector3s](const FLinearColor& Value)
    {
        for (const UMaterialExpressionConstant3Vector* Vector : Vector3s)
        {
            if (Vector->Constant.Equals(Value, 0.0001f))
            {
                return Vector;
            }
        }
        return static_cast<const UMaterialExpressionConstant3Vector*>(nullptr);
    };
    const auto ExactSample = [&Samples](
        FName Name,
        const FString& TexturePath,
        EMaterialSamplerType Sampler)
        -> const UMaterialExpressionTextureSampleParameter2D*
    {
        const UMaterialExpressionTextureSampleParameter2D* const* Found =
            Samples.Find(Name);
        return Found && *Found && (*Found)->Texture &&
            (*Found)->Texture->GetPathName() == TexturePath &&
            (*Found)->SamplerType == Sampler
            ? *Found
            : nullptr;
    };

    const UMaterialExpressionWorldPosition* World =
        Worlds.Num() == 1 ? Worlds[0] : nullptr;
    const UMaterialExpressionConstant* DetailTile = FindScalar(140.0f);
    const UMaterialExpressionConstant* MacroTile = FindScalar(3200.0f);
    const UMaterialExpressionConstant* NegativeOne = FindScalar(-1.0f);
    const UMaterialExpressionConstant* RoughLow = FindScalar(0.92f);
    const UMaterialExpressionConstant* RoughHigh = FindScalar(1.0f);
    const UMaterialExpressionConstant2Vector* Offset =
        Vector2s.Num() == 1 &&
        FMath::IsNearlyEqual(Vector2s[0]->R, 19.25f, 0.0001f) &&
        FMath::IsNearlyEqual(Vector2s[0]->G, -31.75f, 0.0001f)
        ? Vector2s[0]
        : nullptr;
    const UMaterialExpressionConstant3Vector* TintLow = FindVector3(
        FLinearColor(0.93f, 0.96f, 0.92f, 1.0f));
    const UMaterialExpressionConstant3Vector* TintHigh = FindVector3(
        FLinearColor(1.04f, 1.01f, 0.95f, 1.0f));
    const UMaterialExpressionComponentMask* XY = nullptr;
    for (const UMaterialExpressionComponentMask* Mask : Masks)
    {
        if (Mask->R && Mask->G && !Mask->B && !Mask->A &&
            InputIs(Mask->Input, World))
        {
            XY = Mask;
        }
    }
    const UMaterialExpressionDivide* DetailUv = nullptr;
    const UMaterialExpressionDivide* MacroUv = nullptr;
    for (const UMaterialExpressionDivide* Divide : Divides)
    {
        if (InputIs(Divide->A, XY) && InputIs(Divide->B, DetailTile))
        {
            DetailUv = Divide;
        }
        if (InputIs(Divide->A, XY) && InputIs(Divide->B, MacroTile))
        {
            MacroUv = Divide;
        }
    }
    const UMaterialExpressionComponentMask* DetailU = nullptr;
    const UMaterialExpressionComponentMask* DetailV = nullptr;
    for (const UMaterialExpressionComponentMask* Mask : Masks)
    {
        if (Mask->R && !Mask->G && !Mask->B && !Mask->A &&
            InputIs(Mask->Input, DetailUv))
        {
            DetailU = Mask;
        }
        if (!Mask->R && Mask->G && !Mask->B && !Mask->A &&
            InputIs(Mask->Input, DetailUv))
        {
            DetailV = Mask;
        }
    }
    const UMaterialExpressionMultiply* NegativeU = nullptr;
    for (const UMaterialExpressionMultiply* Multiply : Multiplies)
    {
        if ((InputIs(Multiply->A, DetailU) &&
             InputIs(Multiply->B, NegativeOne)) ||
            (InputIs(Multiply->B, DetailU) &&
             InputIs(Multiply->A, NegativeOne)))
        {
            NegativeU = Multiply;
        }
    }
    const UMaterialExpressionAppendVector* RotatedUv = nullptr;
    for (const UMaterialExpressionAppendVector* Append : Appends)
    {
        if (InputIs(Append->A, DetailV) &&
            InputIs(Append->B, NegativeU))
        {
            RotatedUv = Append;
        }
    }
    const UMaterialExpressionAdd* RotatedOffsetUv =
        Adds.Num() == 1 &&
        ((InputIs(Adds[0]->A, RotatedUv) && InputIs(Adds[0]->B, Offset)) ||
         (InputIs(Adds[0]->B, RotatedUv) && InputIs(Adds[0]->A, Offset)))
        ? Adds[0]
        : nullptr;

    const UMaterialExpressionTextureSampleParameter2D* BaseSample =
        ExactSample(TEXT("BaseColorTexture"),
            TextureObjectPath(TEXT("T_IPV4_Grass001_BaseColor")),
            SAMPLERTYPE_Color);
    const UMaterialExpressionTextureSampleParameter2D* BaseRotatedSample =
        ExactSample(TEXT("BaseColorTextureRotated"),
            TextureObjectPath(TEXT("T_IPV4_Grass001_BaseColor")),
            SAMPLERTYPE_Color);
    const UMaterialExpressionTextureSampleParameter2D* NormalSample =
        ExactSample(TEXT("NormalDXTexture"),
            TextureObjectPath(TEXT("T_IPV4_Grass001_NormalDX")),
            SAMPLERTYPE_Normal);
    const UMaterialExpressionTextureSampleParameter2D* NormalRotatedSample =
        ExactSample(TEXT("NormalDXTextureRotated"),
            TextureObjectPath(TEXT("T_IPV4_Grass001_NormalDX")),
            SAMPLERTYPE_Normal);
    const UMaterialExpressionTextureSampleParameter2D* RoughSample =
        ExactSample(TEXT("RoughnessTexture"),
            TextureObjectPath(TEXT("T_IPV4_Grass001_Roughness")),
            SAMPLERTYPE_Masks);
    const UMaterialExpressionTextureSampleParameter2D* RoughRotatedSample =
        ExactSample(TEXT("RoughnessTextureRotated"),
            TextureObjectPath(TEXT("T_IPV4_Grass001_Roughness")),
            SAMPLERTYPE_Masks);
    const UMaterialExpressionTextureSampleParameter2D* AoSample =
        ExactSample(TEXT("AmbientOcclusionTexture"),
            TextureObjectPath(TEXT("T_IPV4_Grass001_AmbientOcclusion")),
            SAMPLERTYPE_Masks);
    const UMaterialExpressionTextureSampleParameter2D* AoRotatedSample =
        ExactSample(TEXT("AmbientOcclusionTextureRotated"),
            TextureObjectPath(TEXT("T_IPV4_Grass001_AmbientOcclusion")),
            SAMPLERTYPE_Masks);
    const UMaterialExpressionTextureSampleParameter2D* MacroSample =
        ExactSample(TEXT("MacroVariationTexture"),
            TextureObjectPath(TEXT("T_IPV4_Grass001_AmbientOcclusion")),
            SAMPLERTYPE_Masks);

    const UMaterialExpressionLinearInterpolate* DetailBlend = nullptr;
    const UMaterialExpressionLinearInterpolate* TintBlend = nullptr;
    const UMaterialExpressionLinearInterpolate* RoughDetailBlend = nullptr;
    const UMaterialExpressionLinearInterpolate* RoughScaleBlend = nullptr;
    const UMaterialExpressionLinearInterpolate* NormalDetailBlend = nullptr;
    const UMaterialExpressionLinearInterpolate* AoDetailBlend = nullptr;

    const UMaterialExpressionComponentMask* RotatedNormalX = nullptr;
    const UMaterialExpressionComponentMask* RotatedNormalY = nullptr;
    const UMaterialExpressionComponentMask* RotatedNormalZ = nullptr;
    for (const UMaterialExpressionComponentMask* Mask : Masks)
    {
        if (!InputIs(Mask->Input, NormalRotatedSample))
        {
            continue;
        }
        if (Mask->R && !Mask->G && !Mask->B && !Mask->A)
            RotatedNormalX = Mask;
        if (!Mask->R && Mask->G && !Mask->B && !Mask->A)
            RotatedNormalY = Mask;
        if (!Mask->R && !Mask->G && Mask->B && !Mask->A)
            RotatedNormalZ = Mask;
    }
    const UMaterialExpressionMultiply* NegativeRotatedNormalY = nullptr;
    for (const UMaterialExpressionMultiply* Multiply : Multiplies)
    {
        if ((InputIs(Multiply->A, RotatedNormalY) &&
             InputIs(Multiply->B, NegativeOne)) ||
            (InputIs(Multiply->B, RotatedNormalY) &&
             InputIs(Multiply->A, NegativeOne)))
        {
            NegativeRotatedNormalY = Multiply;
        }
    }
    const UMaterialExpressionAppendVector* ReorientedNormalXY = nullptr;
    const UMaterialExpressionAppendVector* ReorientedNormalXYZ = nullptr;
    for (const UMaterialExpressionAppendVector* Append : Appends)
    {
        if (InputIs(Append->A, NegativeRotatedNormalY) &&
            InputIs(Append->B, RotatedNormalX))
        {
            ReorientedNormalXY = Append;
        }
    }
    for (const UMaterialExpressionAppendVector* Append : Appends)
    {
        if (InputIs(Append->A, ReorientedNormalXY) &&
            InputIs(Append->B, RotatedNormalZ))
        {
            ReorientedNormalXYZ = Append;
        }
    }
    for (const UMaterialExpressionLinearInterpolate* Lerp : Lerps)
    {
        if (InputIs(Lerp->A, BaseSample) &&
            InputIs(Lerp->B, BaseRotatedSample) &&
            InputIs(Lerp->Alpha, MacroSample, 1))
            DetailBlend = Lerp;
        if (InputIs(Lerp->A, TintLow) && InputIs(Lerp->B, TintHigh) &&
            InputIs(Lerp->Alpha, MacroSample, 1))
            TintBlend = Lerp;
        if (InputIs(Lerp->A, RoughSample, 1) &&
            InputIs(Lerp->B, RoughRotatedSample, 1) &&
            InputIs(Lerp->Alpha, MacroSample, 1))
            RoughDetailBlend = Lerp;
        if (InputIs(Lerp->A, RoughLow) && InputIs(Lerp->B, RoughHigh) &&
            InputIs(Lerp->Alpha, MacroSample, 1))
            RoughScaleBlend = Lerp;
        if (InputIs(Lerp->A, NormalSample) &&
            InputIs(Lerp->B, ReorientedNormalXYZ) &&
            InputIs(Lerp->Alpha, MacroSample, 1))
            NormalDetailBlend = Lerp;
        if (InputIs(Lerp->A, AoSample, 1) &&
            InputIs(Lerp->B, AoRotatedSample, 1) &&
            InputIs(Lerp->Alpha, MacroSample, 1))
            AoDetailBlend = Lerp;
    }
    const UMaterialExpressionNormalize* NormalizedDetailNormal =
        Normalizes.Num() == 1 &&
        InputIs(Normalizes[0]->VectorInput, NormalDetailBlend)
        ? Normalizes[0]
        : nullptr;
    const UMaterialExpressionMultiply* FinalColor = nullptr;
    const UMaterialExpressionMultiply* FinalRoughness = nullptr;
    for (const UMaterialExpressionMultiply* Multiply : Multiplies)
    {
        if ((InputIs(Multiply->A, DetailBlend) &&
             InputIs(Multiply->B, TintBlend)) ||
            (InputIs(Multiply->B, DetailBlend) &&
             InputIs(Multiply->A, TintBlend)))
            FinalColor = Multiply;
        if ((InputIs(Multiply->A, RoughDetailBlend) &&
             InputIs(Multiply->B, RoughScaleBlend)) ||
            (InputIs(Multiply->B, RoughDetailBlend) &&
             InputIs(Multiply->A, RoughScaleBlend)))
            FinalRoughness = Multiply;
    }
    if (!Base || !Instance || !EditorOnly ||
        Base->GetPathName() != LawnBaseMaterialObjectPath() ||
        Instance->GetPathName() != LawnMaterialInstanceObjectPath() ||
        Instance->Parent != Base ||
        Base->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5) ||
        Base->MaterialDomain != MD_Surface ||
        Base->GetBlendMode() != BLEND_Opaque || Base->IsTwoSided() ||
        !Base->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !FMath::IsNearlyZero(Base->MaxWorldPositionOffsetDisplacement) ||
        EditorOnly->WorldPositionOffset.Expression ||
        EditorOnly->PixelDepthOffset.Expression ||
        CustomCount != 0 || TransformCount != 0 ||
        EditorOnly->ExpressionCollection.Expressions.Num() != 41 ||
        Worlds.Num() != 1 || Masks.Num() != 6 || Scalars.Num() != 5 ||
        Vector2s.Num() != 1 || Vector3s.Num() != 2 || Divides.Num() != 2 ||
        Multiplies.Num() != 4 || Appends.Num() != 3 || Adds.Num() != 1 ||
        Lerps.Num() != 6 || Normalizes.Num() != 1 || Samples.Num() != 9 ||
        !World || World->WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets ||
        !XY || !DetailTile || !MacroTile || !NegativeOne || !RoughLow ||
        !RoughHigh || !Offset || !TintLow || !TintHigh || !DetailUv ||
        !MacroUv || !DetailU || !DetailV || !NegativeU || !RotatedUv ||
        !RotatedOffsetUv || !BaseSample || !BaseRotatedSample ||
        !NormalSample || !NormalRotatedSample ||
        !RoughSample || !RoughRotatedSample || !AoSample ||
        !AoRotatedSample || !MacroSample ||
        !InputIs(BaseSample->Coordinates, DetailUv) ||
        !InputIs(BaseRotatedSample->Coordinates, RotatedOffsetUv) ||
        !InputIs(NormalSample->Coordinates, DetailUv) ||
        !InputIs(NormalRotatedSample->Coordinates, RotatedOffsetUv) ||
        !InputIs(RoughSample->Coordinates, DetailUv) ||
        !InputIs(RoughRotatedSample->Coordinates, RotatedOffsetUv) ||
        !InputIs(AoSample->Coordinates, DetailUv) ||
        !InputIs(AoRotatedSample->Coordinates, RotatedOffsetUv) ||
        !InputIs(MacroSample->Coordinates, MacroUv) ||
        !DetailBlend || !TintBlend || !RoughDetailBlend ||
        !RoughScaleBlend || !RotatedNormalX || !RotatedNormalY ||
        !RotatedNormalZ || !NegativeRotatedNormalY ||
        !ReorientedNormalXY || !ReorientedNormalXYZ ||
        !NormalDetailBlend || !NormalizedDetailNormal || !AoDetailBlend ||
        !FinalColor || !FinalRoughness ||
        !InputIs(EditorOnly->BaseColor, FinalColor) ||
        !InputIs(EditorOnly->Normal, NormalizedDetailNormal) ||
        !InputIs(EditorOnly->Roughness, FinalRoughness) ||
        !InputIs(EditorOnly->AmbientOcclusion, AoDetailBlend) ||
        LoadExact<UTexture2D>(TextureObjectPath(
            TEXT("T_IPV4_Grass001_NormalGL"))))
    {
        OutError = TEXT("The static NormalDX Grass001 lawn lost its exact 41-node two-detail/32 m macro graph, shared rotated Normal/AO path, (-Y,X,Z) tangent reorientation/normalize, or gained displacement.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateAndBindAllMaterials(
    IAssetTools& AssetTools,
    const TMap<FString, UTexture2D*>& Textures,
    const TMap<FString, UStaticMesh*>& Meshes,
    TMap<FString, UMaterial*>& OutWindAndPorticoMaterials,
    UMaterialInstanceConstant*& OutLawn,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    TArray<FWindMaterialSpec> WindSpecs;
    if (!BuildWindMaterialSpecs(Meshes, WindSpecs, OutError))
    {
        return false;
    }
    OutWindAndPorticoMaterials.Reset();
    for (const FWindMaterialSpec& Spec : WindSpecs)
    {
        UMaterial* Material = Spec.ProtectedSourceMaterialObjectPath.IsEmpty()
            ? CreateWindMaterial(AssetTools, Spec, Textures, OutError)
            : DuplicateAndUpgradeProtectedWindMaterial(
                AssetTools, Spec, OutError);
        if (!Material ||
            OutWindAndPorticoMaterials.Contains(Spec.AssetName))
        {
            return false;
        }
        OutWindAndPorticoMaterials.Add(Spec.AssetName, Material);
        OutAssetsToSave.Add(Material);
    }
    for (const FSimpleMaterialSpec& Spec : PorticoMaterialSpecs())
    {
        UMaterial* Material = CreateSimpleMaterial(
            AssetTools, Spec, OutError);
        if (!Material || !ValidateSimpleMaterial(Material, Spec, OutError) ||
            OutWindAndPorticoMaterials.Contains(Spec.Name))
        {
            return false;
        }
        OutWindAndPorticoMaterials.Add(Spec.Name, Material);
        OutAssetsToSave.Add(Material);
    }
    UMaterial* LawnBase = nullptr;
    OutLawn = CreateStaticLawnMaterial(
        AssetTools,
        Textures,
        LawnBase,
        OutAssetsToSave,
        OutError);
    if (!OutLawn || !ValidateStaticLawnMaterial(
            LawnBase, OutLawn, OutError) ||
        !BindWindMaterialsToRuntimeMeshes(
            Meshes, WindSpecs, OutWindAndPorticoMaterials, OutError))
    {
        return false;
    }
    if (OutWindAndPorticoMaterials.Num() != 25)
    {
        OutError = TEXT("The exact V4 base-material roster must be 18 wind plus seven portico materials.");
        return false;
    }
    OutError.Reset();
    return true;
}

int32 ExpectedSlotForWindRole(ETRIADIstanaExploreV4WindRole Role)
{
    switch (Role)
    {
    case ETRIADIstanaExploreV4WindRole::UmbrellaTrunk: return 0;
    case ETRIADIstanaExploreV4WindRole::UmbrellaBranch: return 2;
    case ETRIADIstanaExploreV4WindRole::UmbrellaLeaf: return 1;
    case ETRIADIstanaExploreV4WindRole::DomeTrunk: return 2;
    case ETRIADIstanaExploreV4WindRole::DomeBranch: return 0;
    case ETRIADIstanaExploreV4WindRole::DomeLeaf: return 1;
    case ETRIADIstanaExploreV4WindRole::HighForkTrunk: return 0;
    case ETRIADIstanaExploreV4WindRole::HighForkBranch: return 2;
    case ETRIADIstanaExploreV4WindRole::HighForkLeaf: return 1;
    case ETRIADIstanaExploreV4WindRole::ColumnarTrunk: return 1;
    case ETRIADIstanaExploreV4WindRole::ColumnarBranch: return 0;
    case ETRIADIstanaExploreV4WindRole::ColumnarLeaf: return 2;
    case ETRIADIstanaExploreV4WindRole::PalmComposite:
    case ETRIADIstanaExploreV4WindRole::Shrub:
    case ETRIADIstanaExploreV4WindRole::Flower:
    case ETRIADIstanaExploreV4WindRole::Understorey:
    case ETRIADIstanaExploreV4WindRole::GeometryGrass:
    case ETRIADIstanaExploreV4WindRole::CloseTurf:
        return 0;
    default:
        return INDEX_NONE;
    }
}

bool ValidateWindMaterialTextures(
    UMaterial* Material,
    const FWindMaterialSpec& Spec,
    FString& OutError)
{
    if (!Material)
    {
        OutError = TEXT("A V4 wind material is absent.");
        return false;
    }
    if (!Spec.ProtectedSourceMaterialObjectPath.IsEmpty())
    {
        UMetaData* Metadata = Material->GetOutermost()->GetMetaData();
        if (!Metadata ||
            FString(Metadata->GetValue(
                Material, TEXT("TRIAD_IPV4_ProtectedMaterialSource"))) !=
                Spec.ProtectedSourceMaterialObjectPath ||
            !FString(Metadata->GetValue(
                Material, TEXT("TRIAD_IPV4_ProtectedMaterialSha256"))).Equals(
                    Spec.ProtectedSourceMaterialSha256,
                    ESearchCase::IgnoreCase))
        {
            OutError = TEXT("A protected-source V4 material lost exact provenance: ") +
                Spec.AssetName;
            return false;
        }
        return true;
    }
    TMap<FName, const UMaterialExpressionTextureSampleParameter2D*> Samples;
    const UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
    if (EditorOnly)
    {
        for (const UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            if (const UMaterialExpressionTextureSampleParameter2D* Sample =
                    Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
            {
                Samples.Add(Sample->ParameterName, Sample);
            }
        }
    }
    const auto Exact = [&Samples](
        FName Parameter,
        const FString& TextureName,
        EMaterialSamplerType Sampler)
    {
        if (TextureName.IsEmpty())
        {
            return !Samples.Contains(Parameter);
        }
        const UMaterialExpressionTextureSampleParameter2D* const* Found =
            Samples.Find(Parameter);
        return Found && *Found && (*Found)->Texture &&
            (*Found)->Texture->GetPathName() ==
                TextureObjectPath(TextureName) &&
            (*Found)->SamplerType == Sampler;
    };
    if (!Exact(TEXT("BaseColorTexture"), Spec.BaseColorTexture,
            SAMPLERTYPE_Color) ||
        !Exact(TEXT("NormalTexture"), Spec.NormalTexture,
            SAMPLERTYPE_Normal) ||
        !Exact(TEXT("RoughnessTexture"), Spec.RoughnessTexture,
            SAMPLERTYPE_Masks) ||
        !Exact(TEXT("OpacityTexture"), Spec.OpacityTexture,
            SAMPLERTYPE_Masks))
    {
        OutError = TEXT("A V4 wind material lost an exact source-texture/sampler binding: ") +
            Spec.AssetName;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedRuntimeMeshReference(
    UStaticMesh* Reference,
    const FProtectedDerivativeSpec& Spec,
    FString& OutError)
{
    UStaticMesh* Source = LoadExact<UStaticMesh>(Spec.SourceObjectPath);
    FString PackageFilename;
    if (!ResolvePackageFileAndValidate(
            Spec.SourceObjectPath,
            Spec.SourcePackageBytes,
            Spec.SourcePackageSha256,
            PackageFilename,
            OutError) ||
        !ValidateProtectedMeshTopology(Source, Spec, OutError) ||
        !Reference || Reference != Source ||
        Reference->GetPathName() != Spec.SourceObjectPath ||
        Reference->GetOutermost()->IsDirty() ||
        FindObject<UStaticMesh>(
            nullptr, *RuntimeMeshObjectPath(Spec.RuntimeAssetName)) ||
        FPackageName::DoesPackageExist(
            MeshAssetPath + TEXT("/") + Spec.RuntimeAssetName) ||
        Reference->GetNumLODs() != Source->GetNumLODs() ||
        Reference->GetMinLODIdx() < 1 ||
        ImportedSlotOrder(Reference) != Spec.SlotOrder ||
        CountLodTrianglesByMaterialSlot(Reference, 0) !=
            Spec.Lod0TrianglesBySlot ||
        !FMath::IsNearlyEqual(
            Reference->GetBounds().BoxExtent.Z * 2.0,
            Spec.SourceZSpanCm,
            0.05))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A V4 protected topology read-only reference changed, became dirty, or gained a shadow duplicate: ") +
                Spec.SelectionId;
        }
        return false;
    }
    for (int32 Lod = 0; Lod < Source->GetNumLODs(); ++Lod)
    {
        if (CountTriangles(Reference, Lod) != CountTriangles(Source, Lod))
        {
            OutError = TEXT("A protected topology reference no longer has byte-source-equivalent LOD triangles: ") +
                Spec.SelectionId;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool LoadOwnedRuntimeMeshRoster(
    TMap<FString, UStaticMesh*>& OutMeshes,
    FString& OutError)
{
    const TArray<FString> Names = {
        UmbrellaMeshName, DomeMeshName,
        PalmMeshName, ShrubMeshName, FlowerMeshName, UnderstoreyMeshName,
        GeometryGrassMeshName, CloseTurfMeshName, HeritageBlockerMeshName};
    OutMeshes.Reset();
    for (const FString& Name : Names)
    {
        UStaticMesh* Mesh = LoadExact<UStaticMesh>(RuntimeMeshObjectPath(Name));
        if (!Mesh)
        {
            OutError = TEXT("A required V4 runtime mesh is absent: ") + Name;
            return false;
        }
        OutMeshes.Add(Name, Mesh);
    }
    if (OutMeshes.Num() != 9)
    {
        OutError = TEXT("The V4-owned runtime mesh roster must contain exactly nine meshes.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool LoadRuntimeMeshRoster(
    TMap<FString, UStaticMesh*>& OutMeshes,
    FString& OutError)
{
    if (!LoadOwnedRuntimeMeshRoster(OutMeshes, OutError))
    {
        return false;
    }
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        UStaticMesh* Reference = LoadExact<UStaticMesh>(Spec.SourceObjectPath);
        if (!ValidateProtectedRuntimeMeshReference(
                Reference, Spec, OutError))
        {
            return false;
        }
        OutMeshes.Add(Spec.RuntimeAssetName, Reference);
    }
    if (OutMeshes.Num() != 11)
    {
        OutError = TEXT("The V4 runtime mesh roster must contain nine V4-owned meshes plus two exact protected read-only references.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedReferenceFactsWithoutLoading(
    bool bRequireSourcesUnloaded,
    FString& OutError)
{
    if (ProtectedDerivativeSpecs().Num() != 2)
    {
        OutError = TEXT("The protected no-load fact roster must contain exactly two rows.");
        return false;
    }
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        FString PackageFilename;
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(Spec.SourceObjectPath);
        if (!ResolvePackageFileAndValidate(
                Spec.SourceObjectPath,
                Spec.SourcePackageBytes,
                Spec.SourcePackageSha256,
                PackageFilename,
                OutError) ||
            FindObject<UStaticMesh>(
                nullptr, *RuntimeMeshObjectPath(Spec.RuntimeAssetName)) ||
            FPackageName::DoesPackageExist(
                MeshAssetPath + TEXT("/") + Spec.RuntimeAssetName) ||
            (bRequireSourcesUnloaded &&
                (FindObject<UStaticMesh>(
                    nullptr, *Spec.SourceObjectPath) ||
                 FindPackage(nullptr, *PackageName))))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A protected exact source/hash/frozen fact row is missing, shadowed by a V4 duplicate, or unexpectedly loaded: ") +
                    Spec.SelectionId;
            }
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool FinishExactStaticMeshRosterMemoryBounded(
    const TArray<UStaticMesh*>& Meshes,
    const FString& Label,
    FString& OutError)
{
    TSet<UStaticMesh*> UniqueMeshes;
    for (UStaticMesh* Mesh : Meshes)
    {
        if (!Mesh || UniqueMeshes.Contains(Mesh) ||
            !Mesh->GetOutermost() || Mesh->GetOutermost()->IsDirty())
        {
            OutError = Label +
                TEXT(" contains a null, duplicate, outerless or dirty static mesh.");
            return false;
        }
        UniqueMeshes.Add(Mesh);

        if (Mesh->IsCompiling())
        {
            FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
        }
        const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
        if (Mesh->IsCompiling() || !RenderData ||
            RenderData->LODResources.IsEmpty() || Mesh->GetNumLODs() <= 0)
        {
            OutError = Label +
                TEXT(" did not reach exact synchronous render/LOD readiness: ") +
                Mesh->GetPathName();
            return false;
        }

        Mesh->ClearMeshDescriptions();
        if (Mesh->GetOutermost()->IsDirty())
        {
            OutError = Label +
                TEXT(" dirtied a static-mesh package while releasing editor MeshDescriptions: ") +
                Mesh->GetPathName();
            return false;
        }
        FMemory::Trim(true);
    }
    if (UniqueMeshes.Num() != Meshes.Num())
    {
        OutError = Label + TEXT(" exact static-mesh roster cardinality changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedRuntimeMeshReferencesMemoryBounded(
    FString& OutError)
{
    if (ProtectedDerivativeSpecs().Num() != 2)
    {
        OutError = TEXT("Memory-bounded protected validation requires exactly two source specifications.");
        return false;
    }
    int32 LoadedObjects = 0;
    int32 LoadedPackages = 0;
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        LoadedObjects += FindObject<UStaticMesh>(
            nullptr, *Spec.SourceObjectPath) ? 1 : 0;
        LoadedPackages += FindPackage(
            nullptr,
            *FPackageName::ObjectPathToPackageName(
                Spec.SourceObjectPath)) ? 1 : 0;
    }
    if (LoadedObjects == 2 && LoadedPackages == 2)
    {
        for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
        {
            UStaticMesh* Reference = FindObject<UStaticMesh>(
                nullptr, *Spec.SourceObjectPath);
            if (!FinishExactStaticMeshRosterMemoryBounded(
                    {Reference},
                    TEXT("Map-resident protected validation ") +
                        Spec.SelectionId,
                    OutError) ||
                !ValidateProtectedRuntimeMeshReference(
                    Reference, Spec, OutError))
            {
                return false;
            }
        }
        OutError.Reset();
        return true;
    }
    if (LoadedObjects != 0 || LoadedPackages != 0)
    {
        OutError = TEXT("Protected runtime mesh validation refused a partial preloaded source roster; require either both map-resident references or neither source package.");
        return false;
    }

    int32 Validated = 0;
    for (const FProtectedDerivativeSpec& Spec : ProtectedDerivativeSpecs())
    {
        for (const FProtectedDerivativeSpec& Other : ProtectedDerivativeSpecs())
        {
            if (Other.SelectionId != Spec.SelectionId &&
                (FindObject<UStaticMesh>(
                    nullptr, *Other.SourceObjectPath) ||
                 FindPackage(
                    nullptr,
                    *FPackageName::ObjectPathToPackageName(
                        Other.SourceObjectPath))))
            {
                OutError = TEXT("Memory-bounded protected validation found the other heavyweight source resident: ") +
                    Other.SelectionId;
                return false;
            }
        }
        FString PackageFilename;
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(
                Spec.SourceObjectPath);
        if (!ResolvePackageFileAndValidate(
                Spec.SourceObjectPath,
                Spec.SourcePackageBytes,
                Spec.SourcePackageSha256,
                PackageFilename,
                OutError))
        {
            return false;
        }
        UStaticMesh* Reference = LoadExact<UStaticMesh>(
            Spec.SourceObjectPath);
        UPackage* ReferencePackage = Reference
            ? Reference->GetOutermost()
            : FindPackage(nullptr, *PackageName);
        const auto FailAndRelease =
            [&Reference, ReferencePackage, &Spec](
                const FString& Failure,
                FString& Error) -> bool
        {
            if (Reference)
            {
                Reference->ClearMeshDescriptions();
            }
            Reference = nullptr;
            FString ReleaseError;
            const bool bReleased = ReferencePackage &&
                UnloadOneExactCleanPackage(
                    Spec.SourceObjectPath,
                    ReferencePackage,
                    false,
                    TEXT("Memory-bounded protected validation ") +
                        Spec.SelectionId,
                    ReleaseError);
            Error = Failure;
            if (!bReleased)
            {
                if (!Error.IsEmpty())
                {
                    Error += TEXT(" ");
                }
                Error += TEXT("release=") + ReleaseError;
            }
            return false;
        };
        if (!Reference || !ReferencePackage ||
            ReferencePackage->IsDirty())
        {
            return FailAndRelease(
                TEXT("Memory-bounded protected validation could not load one exact clean source: ") +
                    Spec.SelectionId,
                OutError);
        }
        if (!FinishExactStaticMeshRosterMemoryBounded(
                {Reference},
                TEXT("Memory-bounded protected validation ") +
                    Spec.SelectionId,
                OutError) ||
            !ValidateProtectedRuntimeMeshReference(
                Reference, Spec, OutError))
        {
            const FString Failure = OutError;
            return FailAndRelease(Failure, OutError);
        }
        Reference->ClearMeshDescriptions();
        if (ReferencePackage->IsDirty())
        {
            return FailAndRelease(
                TEXT("Memory-bounded protected validation dirtied a source package: ") +
                    Spec.SelectionId,
                OutError);
        }
        Reference = nullptr;
        if (!UnloadOneExactCleanPackage(
                Spec.SourceObjectPath,
                ReferencePackage,
                false,
                TEXT("Memory-bounded protected validation ") +
                    Spec.SelectionId,
                OutError))
        {
            return false;
        }
        if (!ResolvePackageFileAndValidate(
                Spec.SourceObjectPath,
                Spec.SourcePackageBytes,
                Spec.SourcePackageSha256,
                PackageFilename,
                OutError))
        {
            return false;
        }
        ++Validated;
    }
    if (Validated != 2 ||
        !ValidateProtectedPrewarmSourcesUnloaded(OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateV4StagingAssetsMemoryBounded(FString& OutError)
{
    const TArray<FString> ExactStagingPaths =
        ExactExternalStagingObjectPaths();
    if (ExactStagingPaths.Num() != 7)
    {
        OutError = TEXT("The memory-bounded staging validation path roster must contain exactly seven assets.");
        return false;
    }
    int32 ValidatedCount = 0;
    for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())
    {
        const FString ExactObjectPath = StagingMeshObjectPath(Spec);
        const FString ExactPackageName =
            FPackageName::ObjectPathToPackageName(ExactObjectPath);
        UStaticMesh* Staging = LoadExact<UStaticMesh>(ExactObjectPath);
        UPackage* StagingPackage = Staging
            ? Staging->GetOutermost()
            : FindPackage(nullptr, *ExactPackageName);
        const auto FailAndRelease =
            [&Staging, StagingPackage, &ExactObjectPath, &Spec](
                const FString& Failure,
                FString& Error) -> bool
        {
            if (Staging)
            {
                Staging->ClearMeshDescriptions();
            }
            Staging = nullptr;
            FString ReleaseError;
            const bool bReleased = !StagingPackage ||
                UnloadOneExactCleanPackage(
                    ExactObjectPath,
                    StagingPackage,
                    false,
                    TEXT("Cold staging validation ") + Spec.SelectionId,
                    ReleaseError);
            Error = Failure;
            if (!bReleased)
            {
                if (!Error.IsEmpty())
                {
                    Error += TEXT(" ");
                }
                Error += TEXT("release=") + ReleaseError;
            }
            return false;
        };
        if (!Staging)
        {
            return FailAndRelease(
                TEXT("A memory-bounded cold staging load failed: ") +
                    Spec.SelectionId,
                OutError);
        }
        if (!FinishExactStaticMeshRosterMemoryBounded(
                {Staging},
                TEXT("Cold staging validation ") + Spec.SelectionId,
                OutError) ||
            !ValidateStagingMesh(Staging, Spec, OutError))
        {
            const FString Failure = OutError;
            return FailAndRelease(Failure, OutError);
        }
        Staging->ClearMeshDescriptions();
        if (!StagingPackage || StagingPackage->IsDirty())
        {
            return FailAndRelease(
                TEXT("Memory-bounded staging validation dirtied or misresolved an exact persisted package: ") +
                    Spec.SelectionId,
                OutError);
        }
        Staging = nullptr;
        if (!UnloadOneExactCleanPackage(
                ExactObjectPath,
                StagingPackage,
                false,
                TEXT("Cold staging validation ") + Spec.SelectionId,
                OutError))
        {
            return false;
        }
        ++ValidatedCount;
    }
    if (ValidatedCount != 7)
    {
        OutError = TEXT("Memory-bounded staging validation did not prove all seven exact packages.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAllV4NonStagingAssetsInternal(FString& OutError)
{
    TSharedPtr<FJsonObject> Contract;
    if (!ValidateFrozenPublicDataContracts(OutError) ||
        !ValidateFrozenVegetationContract(Contract, OutError) ||
        !ValidateExternalMeshContractRows(Contract, OutError) ||
        !ValidateProtectedDerivativeContractRows(Contract, OutError))
    {
        return false;
    }
    TArray<FTextureSourceSpec> TextureSpecs;
    if (!BuildRequiredTextureSpecs(Contract, TextureSpecs, OutError))
    {
        return false;
    }
    TMap<FString, UStaticMesh*> Meshes;
    if (!ValidateProtectedReferenceFactsWithoutLoading(false, OutError) ||
        !LoadOwnedRuntimeMeshRoster(Meshes, OutError))
    {
        return false;
    }
    TArray<UStaticMesh*> CompileMeshes;
    for (const TPair<FString, UStaticMesh*>& Pair : Meshes)
    {
        CompileMeshes.Add(Pair.Value);
    }
    UStaticMesh* ExactPortico =
        LoadExact<UStaticMesh>(PorticoMeshObjectPath());
    CompileMeshes.Add(ExactPortico);
    if (CompileMeshes.Contains(nullptr))
    {
        OutError = TEXT("The exact V4 compile roster contains a missing runtime mesh or portico.");
        return false;
    }
    if (!FinishExactStaticMeshRosterMemoryBounded(
            CompileMeshes,
            TEXT("V4 non-staging validation"),
            OutError))
    {
        return false;
    }
    for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())
    {
        if (!ValidateRuntimeDerivative(
                Meshes.FindRef(Spec.RuntimeAssetName),
                Spec,
                OutError))
        {
            return false;
        }
    }
    UStaticMesh* Turf = Meshes.FindRef(CloseTurfMeshName);
    const UBodySetup* TurfBody = Turf ? Turf->GetBodySetup() : nullptr;
    UStaticMesh* Blocker = Meshes.FindRef(HeritageBlockerMeshName);
    const UBodySetup* BlockerBody = Blocker ? Blocker->GetBodySetup() : nullptr;
    if (!Turf || Turf->GetMinLODIdx() < 1 || Turf->GetNumLODs() < 2 ||
        CountTriangles(Turf, 0) > 8 || CountTriangles(Turf, 1) > 4 ||
        (TurfBody && (TurfBody->AggGeom.GetElementCount() != 0 ||
                      TurfBody->CollisionTraceFlag == CTF_UseComplexAsSimple)) ||
        !Blocker || !BlockerBody || BlockerBody->AggGeom.GetElementCount() <= 0)
    {
        OutError = TEXT("The exact V4 close-turf MinLOD1 derivative or separate Heritage blocker mesh changed.");
        return false;
    }
    for (const FTextureSourceSpec& Spec : TextureSpecs)
    {
        if (!ValidateImportedTexture(
                LoadExact<UTexture2D>(TextureObjectPath(Spec.AssetName)),
                Spec,
                OutError))
        {
            return false;
        }
    }

    TArray<FWindMaterialSpec> WindSpecs;
    if (!BuildWindMaterialSpecs(Meshes, WindSpecs, OutError))
    {
        return false;
    }
    for (const FWindMaterialSpec& Spec : WindSpecs)
    {
        UMaterial* Material = LoadExact<UMaterial>(
            MaterialObjectPath(Spec.AssetName));
        const bool bProtectedRole = IsProtectedWindRole(Spec.Role);
        UStaticMesh* Mesh = bProtectedRole
            ? nullptr
            : MeshForWindRole(Meshes, Spec.Role);
        const int32 Slot = bProtectedRole
            ? ExpectedSlotForWindRole(Spec.Role)
            : FindExactImportedSlot(Mesh, Spec.ImportedSlotName);
        if (!ValidateWindMaterial(Material, Spec, OutError) ||
            !ValidateWindMaterialTextures(Material, Spec, OutError) ||
            Slot != ExpectedSlotForWindRole(Spec.Role) ||
            (bProtectedRole &&
                !ValidateFrozenProtectedWindBinding(Spec, OutError)) ||
            (!bProtectedRole &&
                (!Mesh ||
                 !IsExactProtectedReferenceForWindRole(Mesh, Spec.Role) ||
                 Mesh->GetMaterial(Slot) != Material)))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("An exact V4 role-to-slot/material mapping changed: ") +
                    Spec.AssetName;
            }
            return false;
        }
    }
    for (const FSimpleMaterialSpec& Spec : PorticoMaterialSpecs())
    {
        if (!ValidateSimpleMaterial(
                LoadExact<UMaterial>(MaterialObjectPath(Spec.Name)),
                Spec,
                OutError))
        {
            return false;
        }
    }
    if (!ValidateStaticLawnMaterial(
            LoadExact<UMaterial>(LawnBaseMaterialObjectPath()),
            LoadExact<UMaterialInstanceConstant>(
                LawnMaterialInstanceObjectPath()),
            OutError) ||
        !ValidatePorticoMesh(
            ExactPortico,
            true,
            OutError))
    {
        return false;
    }
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())
    {
        TArray<FName> Referencers;
        RegistryModule.Get().GetReferencers(
            FName(*(StagingVegetationAssetPath + TEXT("/") +
                Spec.StagingAssetName)),
            Referencers,
            UE::AssetRegistry::EDependencyCategory::Package);
        for (const FName Referencer : Referencers)
        {
            if (Referencer.ToString().StartsWith(
                    VegetationAssetPath + TEXT("/")) ||
                Referencer.ToString() == DestinationMapPackage)
            {
                OutError = TEXT("A runtime/map package references editor-only raw LOD0 staging: ") +
                    Referencer.ToString();
                return false;
            }
        }
    }
    if (CountAssetsUnderV4Root() != 85)
    {
        OutError = FString::Printf(
            TEXT("The exact V4 asset roster must contain 85 V4-owned assets plus two protected read-only mesh references, found %d V4-owned assets."),
            CountAssetsUnderV4Root());
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAllV4AssetsInternal(
    bool bValidateProtectedMeshesLive,
    FString& OutError)
{
    if ((bValidateProtectedMeshesLive
            ? !ValidateProtectedRuntimeMeshReferencesMemoryBounded(OutError)
            : !ValidateProtectedReferenceFactsWithoutLoading(
                true, OutError)) ||
        !ValidateV4StagingAssetsMemoryBounded(OutError) ||
        !ValidateAllV4NonStagingAssetsInternal(OutError) ||
        (!bValidateProtectedMeshesLive &&
            !ValidateProtectedReferenceFactsWithoutLoading(
                true, OutError)))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool UnloadCleanV4ValidationPackagesAtEntry(FString& OutError)
{
    int32 KnownAutoDirtyMaterialCount = 0;
    if (HasDisallowedDirtyPackages(
            KnownAutoDirtyMaterialCount, OutError) ||
        !ValidateCleanEntryProcessBoundary(
            KnownAutoDirtyMaterialCount, OutError) ||
        CountAssetsUnderV4Root() != 85)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Entry-phase release requires the exact clean 85-asset V4 namespace.");
        }
        return false;
    }

    TArray<FAssetData> AssetData;
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    RegistryModule.Get().GetAssetsByPath(
        FName(*AssetRoot), AssetData, true, false);
    if (AssetData.Num() != 85)
    {
        OutError = TEXT("Entry-phase release did not discover exactly 85 V4-owned asset packages.");
        return false;
    }

    TSet<UPackage*> UniqueLoadedPackages;
    for (const FAssetData& Data : AssetData)
    {
        const FString PackageName = Data.PackageName.ToString();
        if (!PackageName.StartsWith(AssetRoot + TEXT("/")))
        {
            OutError = TEXT("Entry-phase release refused a package outside the exact V4 root: ") +
                PackageName;
            return false;
        }
        if (UPackage* Package = FindPackage(nullptr, *PackageName))
        {
            if (Package->IsDirty())
            {
                OutError = TEXT("Entry-phase release refused a dirty V4 package: ") +
                    PackageName;
                return false;
            }
            UniqueLoadedPackages.Add(Package);
        }
    }

    if (!UniqueLoadedPackages.IsEmpty())
    {
        TArray<UPackage*> LoadedPackages = UniqueLoadedPackages.Array();
        UPackageTools::FUnloadPackageParams UnloadParams(LoadedPackages);
        UnloadParams.bUnloadDirtyPackages = false;
        UnloadParams.bResetTransBuffer = true;
        if (!UPackageTools::UnloadPackages(UnloadParams))
        {
            OutError = TEXT("Entry-phase release could not unload the exact clean V4 package batch: ") +
                UnloadParams.OutErrorMessage.ToString();
            return false;
        }
    }
    CollectGarbage(RF_NoFlags);
    FMemory::Trim(true);

    for (const FAssetData& Data : AssetData)
    {
        const FString PackageName = Data.PackageName.ToString();
        if (FindPackage(nullptr, *PackageName) ||
            FindObject<UObject>(nullptr, *Data.GetObjectPathString()))
        {
            OutError = TEXT("Entry-phase release left a V4 validation package or object resident: ") +
                PackageName;
            return false;
        }
    }
    if (FindPackage(nullptr, *SourceMapPackage) ||
        FindObject<UWorld>(nullptr, *SourceMapObjectPath) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath) ||
        !ValidateProtectedPrewarmSourcesUnloaded(OutError) ||
        !ValidateCleanEntryProcessBoundary(0, OutError) ||
        CountAssetsUnderV4Root() != 85)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Entry-phase release did not reach the exact V4/protected/source/target absence boundary.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool RollBackNewV4Namespace(FString& OutError)
{
    TArray<FAssetData> AssetData;
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    RegistryModule.Get().GetAssetsByPath(
        FName(*AssetRoot), AssetData, true, false);
    AssetData.Sort([](const FAssetData& A, const FAssetData& B)
    {
        return A.GetObjectPathString() < B.GetObjectPathString();
    });
    TArray<UObject*> AlreadyLoadedObjects;
    TArray<FAssetData> UnloadedAssetData;
    for (const FAssetData& Data : AssetData)
    {
        const FString ObjectPathToDelete = Data.GetObjectPathString();
        if (!ObjectPathToDelete.StartsWith(AssetRoot + TEXT("/")))
        {
            OutError = TEXT("Rollback refused an object outside the exact new V4 namespace.");
            return false;
        }
        if (UObject* Loaded = FindObject<UObject>(
                nullptr, *ObjectPathToDelete))
        {
            AlreadyLoadedObjects.Add(Loaded);
        }
        else
        {
            UnloadedAssetData.Add(Data);
        }
    }
    bool bDeleteFailed = false;
    if (!AlreadyLoadedObjects.IsEmpty() &&
        ObjectTools::DeleteObjectsUnchecked(AlreadyLoadedObjects) !=
            AlreadyLoadedObjects.Num())
    {
        bDeleteFailed = true;
    }
    AlreadyLoadedObjects.Reset();
    CollectGarbage(RF_NoFlags);
    FMemory::Trim(true);
    for (const FAssetData& Data : UnloadedAssetData)
    {
        const FString ObjectPathToDelete = Data.GetObjectPathString();
        UObject* Object = FindObject<UObject>(
            nullptr, *ObjectPathToDelete);
        if (!Object)
        {
            Object = Data.GetAsset();
        }
        TArray<UObject*> OneObjectToDelete;
        if (Object)
        {
            OneObjectToDelete.Add(Object);
        }
        if (!Object || Object->GetPathName() != ObjectPathToDelete ||
            !ObjectPathToDelete.StartsWith(AssetRoot + TEXT("/")) ||
            ObjectTools::DeleteObjectsUnchecked(OneObjectToDelete) != 1)
        {
            bDeleteFailed = true;
        }
        Object = nullptr;
        CollectGarbage(RF_NoFlags);
        FMemory::Trim(true);
    }
    if (bDeleteFailed || CountAssetsUnderV4Root() != 0)
    {
        OutError = TEXT("Could not roll back every exact new V4 target package without bulk-loading sealed staging assets.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool DeleteExactV4TargetMapAfterFailure(
    UEditorAssetSubsystem* AssetSubsystem,
    FString& OutError)
{
    if (!AssetSubsystem)
    {
        OutError = TEXT("Target cleanup requires the editor asset subsystem.");
        return false;
    }
    FString SourceFilename;
    if (!FPackageName::DoesPackageExist(
            SourceMapPackage, &SourceFilename))
    {
        OutError = TEXT("Target cleanup could not resolve the protected V3 source map.");
        return false;
    }
    UWorld* CurrentWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    if (!CurrentWorld || !CurrentWorld->GetOutermost() ||
        CurrentWorld->GetOutermost()->GetName() != SourceMapPackage)
    {
        UWorld* RestoredSource =
            UEditorLoadingAndSavingUtils::LoadMap(SourceFilename);
        if (!RestoredSource || !RestoredSource->GetOutermost() ||
            RestoredSource->GetOutermost()->GetName() != SourceMapPackage)
        {
            OutError = TEXT("Target cleanup could not switch back to the protected V3 source map.");
            return false;
        }
    }

    const bool bTargetKnown =
        FPackageName::DoesPackageExist(DestinationMapPackage) ||
        AssetSubsystem->DoesAssetExist(DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath) != nullptr;
    if (bTargetKnown &&
        !AssetSubsystem->DeleteAsset(DestinationMapPackage))
    {
        OutError = TEXT("Could not delete the exact failed Explore V4 target map.");
        return false;
    }
    CollectGarbage(RF_NoFlags);
    if (FPackageName::DoesPackageExist(DestinationMapPackage) ||
        AssetSubsystem->DoesAssetExist(DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath) ||
        FindPackage(nullptr, *DestinationMapPackage))
    {
        OutError = TEXT("The exact failed Explore V4 target package remains loaded or on disk after cleanup.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool BuildRuntimeAssetRosterAndWindBindings(
    FTRIADIstanaExploreV4AssetRoster& OutRoster,
    TArray<FTRIADIstanaExploreV4WindMaterialBinding>& OutBindings,
    FString& OutError)
{
    TMap<FString, UStaticMesh*> Meshes;
    if (!LoadRuntimeMeshRoster(Meshes, OutError))
    {
        return false;
    }
    UStaticMesh* Portico = LoadExact<UStaticMesh>(PorticoMeshObjectPath());
    if (!Portico)
    {
        OutError = TEXT("The exact V8 portico asset is absent from the V4 roster.");
        return false;
    }
    OutRoster = FTRIADIstanaExploreV4AssetRoster();
    OutRoster.UmbrellaTree = Meshes.FindRef(UmbrellaMeshName);
    OutRoster.DomeTree = Meshes.FindRef(DomeMeshName);
    OutRoster.HighForkRoundedTree = Meshes.FindRef(HighForkMeshName);
    OutRoster.ColumnarNarrowTree = Meshes.FindRef(ColumnarMeshName);
    OutRoster.PalmTree = Meshes.FindRef(PalmMeshName);
    OutRoster.HeritagePawnBlockerCylinder =
        Meshes.FindRef(HeritageBlockerMeshName);
    OutRoster.Shrub = Meshes.FindRef(ShrubMeshName);
    OutRoster.Flower = Meshes.FindRef(FlowerMeshName);
    OutRoster.Understorey = Meshes.FindRef(UnderstoreyMeshName);
    OutRoster.GeometryGrass = Meshes.FindRef(GeometryGrassMeshName);
    OutRoster.CloseTurf = Meshes.FindRef(CloseTurfMeshName);
    OutRoster.PorticoV8 = Portico;

    TArray<FWindMaterialSpec> Specs;
    if (!BuildWindMaterialSpecs(Meshes, Specs, OutError))
    {
        return false;
    }
    OutBindings.Reset();
    TSet<uint8> Roles;
    for (const FWindMaterialSpec& Spec : Specs)
    {
        UMaterialInterface* Material = LoadExact<UMaterialInterface>(
            MaterialObjectPath(Spec.AssetName));
        const int32 Slot = ExpectedSlotForWindRole(Spec.Role);
        UStaticMesh* Mesh = MeshForWindRole(Meshes, Spec.Role);
        if (!Material || !Mesh || Slot == INDEX_NONE ||
            Slot != FindExactImportedSlot(Mesh, Spec.ImportedSlotName) ||
            !IsExactProtectedReferenceForWindRole(Mesh, Spec.Role) ||
            (!IsProtectedWindRole(Spec.Role) &&
                Mesh->GetMaterial(Slot) != Material) ||
            (IsProtectedWindRole(Spec.Role) &&
                Mesh->GetOutermost()->IsDirty()) ||
            Roles.Contains(static_cast<uint8>(Spec.Role)))
        {
            OutError = TEXT("A V4 runtime wind role/base-material/imported-slot binding changed: ") +
                Spec.AssetName;
            return false;
        }
        FTRIADIstanaExploreV4WindMaterialBinding Binding;
        Binding.Role = Spec.Role;
        Binding.MaterialSlotIndex = Slot;
        Binding.BaseMaterial = Material;
        Binding.bUsesPerInstanceLocalPosition = true;
        OutBindings.Add(MoveTemp(Binding));
        Roles.Add(static_cast<uint8>(Spec.Role));
    }
    if (OutBindings.Num() != 18 || Roles.Num() != 18)
    {
        OutError = TEXT("The runtime handoff requires exactly 18 unique instance-local wind bindings.");
        return false;
    }
    OutError.Reset();
    return true;
}

void AppendTransforms(
    TArray<FTransform>& Destination,
    TArray<FTransform>& Source)
{
    Destination.Append(Source);
    Source.Reset();
}

bool BuildV4PopulationInput(
    ATRIADIstanaExploreV2LandscapeActor* V2,
    ATRIADIstanaExploreV3SupplementActor* V3,
    ATRIADIstanaPublicViewSceneActor* Scene,
    UStaticMeshComponent* Terrain,
    FTRIADIstanaExploreV4PopulationInput& OutInput,
    FPlacementAudit& OutAudit,
    FString& OutError)
{
    FFrozenRasterGrid Grid;
    if (!LoadFrozenRasterGrid(Grid, OutError))
    {
        return false;
    }
    OutInput = FTRIADIstanaExploreV4PopulationInput();
    if (!GatherExactTreeUnionAndRasterCues(
            V2,
            V3,
            Grid,
            OutInput.InheritedV2TreeWorldTransforms,
            OutInput.RasterCues,
            OutError) ||
        !BuildSanitizedHeritageAnchors(
            Terrain, OutInput.HeritageAnchors, OutError) ||
        !GatherExactV3CloseTurfTransforms(
            V3, OutInput.CloseTurfTransforms, OutError))
    {
        return false;
    }

    FPlanarExclusionIndex ExclusionIndex;
    TArray<FVector2D> TreeClearanceCentersCm;
    if (!BuildInheritedPlanarExclusionIndex(
            Scene, V3, ExclusionIndex, OutError) ||
        !GatherInheritedTreeClearanceCenters(
            V2,
            V3,
            Scene,
            OutInput.HeritageAnchors,
            TreeClearanceCentersCm,
            OutError))
    {
        return false;
    }
    OutAudit = FPlacementAudit();
    OutAudit.ExclusionSourceComponentCount =
        ExclusionIndex.SourceComponentCount;
    OutAudit.ExclusionTriangleCount = ExclusionIndex.Triangles.Num();
    OutAudit.FormalPlantingTriangleCount =
        ExclusionIndex.FormalPlantingTriangleCount;
    TArray<FVector2D> AcceptedNewCentersCm;

    TArray<FTransform> Formal;
    TArray<FTransform> Flank;
    FPlacementRoleAudit ShrubFormalAudit;
    ShrubFormalAudit.Role = TEXT("ShrubFormal");
    FPlacementRoleAudit ShrubFlankAudit;
    ShrubFlankAudit.Role = TEXT("ShrubFlank");
    if (!BuildPairedFormalBedTransforms(
            Terrain, FormalBedShrubCount, 0x4757A601,
            4.0f, 7.5f,
            ExclusionIndex, TreeClearanceCentersCm,
            150.0f, 300.0f, 100.0f,
            AcceptedNewCentersCm, ShrubFormalAudit,
            Formal, OutError) ||
        !BuildTracedPlantingTransforms(
            Terrain, Grid,
            ExpectedShrubCount - FormalBedShrubCount,
            0x4757A611, 60.0f, 930.0f, 4.0f, 8.0f, 10,
            ExclusionIndex, TreeClearanceCentersCm,
            150.0f, 300.0f, 200.0f,
            AcceptedNewCentersCm, ShrubFlankAudit,
            Flank, OutError))
    {
        return false;
    }
    OutAudit.Roles.Add(ShrubFormalAudit);
    OutAudit.Roles.Add(ShrubFlankAudit);
    AppendTransforms(OutInput.ShrubTransforms, Formal);
    AppendTransforms(OutInput.ShrubTransforms, Flank);

    FPlacementRoleAudit FlowerFormalAudit;
    FlowerFormalAudit.Role = TEXT("FlowerFormal");
    FPlacementRoleAudit FlowerFlankAudit;
    FlowerFlankAudit.Role = TEXT("FlowerFlank");
    if (!BuildPairedFormalBedTransforms(
            Terrain, FormalBedFlowerCount, 0x4757A602,
            2.8f, 4.8f,
            ExclusionIndex, TreeClearanceCentersCm,
            75.0f, 200.0f, 60.0f,
            AcceptedNewCentersCm, FlowerFormalAudit,
            Formal, OutError) ||
        !BuildTracedPlantingTransforms(
            Terrain, Grid,
            ExpectedFlowerCount - FormalBedFlowerCount,
            0x4757A612, 55.0f, 760.0f, 2.6f, 5.0f, 10,
            ExclusionIndex, TreeClearanceCentersCm,
            75.0f, 200.0f, 60.0f,
            AcceptedNewCentersCm, FlowerFlankAudit,
            Flank, OutError))
    {
        return false;
    }
    OutAudit.Roles.Add(FlowerFormalAudit);
    OutAudit.Roles.Add(FlowerFlankAudit);
    AppendTransforms(OutInput.FlowerTransforms, Formal);
    AppendTransforms(OutInput.FlowerTransforms, Flank);

    FPlacementRoleAudit UnderstoreyFormalAudit;
    UnderstoreyFormalAudit.Role = TEXT("UnderstoreyFormal");
    FPlacementRoleAudit UnderstoreyFlankAudit;
    UnderstoreyFlankAudit.Role = TEXT("UnderstoreyFlank");
    if (!BuildPairedFormalBedTransforms(
            Terrain, FormalBedUnderstoreyCount, 0x4757A603,
            4.0f, 8.0f,
            ExclusionIndex, TreeClearanceCentersCm,
            100.0f, 250.0f, 90.0f,
            AcceptedNewCentersCm, UnderstoreyFormalAudit,
            Formal, OutError) ||
        !BuildTracedPlantingTransforms(
            Terrain, Grid,
            ExpectedUnderstoreyCount - FormalBedUnderstoreyCount,
            0x4757A613, 60.0f, 900.0f, 3.8f, 8.5f, 10,
            ExclusionIndex, TreeClearanceCentersCm,
            100.0f, 250.0f, 90.0f,
            AcceptedNewCentersCm, UnderstoreyFlankAudit,
            Flank, OutError))
    {
        return false;
    }
    OutAudit.Roles.Add(UnderstoreyFormalAudit);
    OutAudit.Roles.Add(UnderstoreyFlankAudit);
    AppendTransforms(OutInput.UnderstoreyTransforms, Formal);
    AppendTransforms(OutInput.UnderstoreyTransforms, Flank);

    FPlacementRoleAudit GeometryGrassAudit;
    GeometryGrassAudit.Role = TEXT("GeometryGrassFlank");
    if (!BuildTracedPlantingTransforms(
            Terrain, Grid, ExpectedGeometryGrassCount,
            0x4757A614, 45.0f, 930.0f, 1.8f, 4.8f, 30,
            ExclusionIndex, TreeClearanceCentersCm,
            50.0f, 200.0f, 100.0f,
            AcceptedNewCentersCm, GeometryGrassAudit,
            OutInput.GeometryGrassTransforms, OutError))
    {
        return false;
    }
    OutAudit.Roles.Add(GeometryGrassAudit);
    if (OutInput.InheritedV2TreeWorldTransforms.Num() !=
            ExpectedV2TreeUnionCount ||
        OutInput.RasterCues.Num() != ExpectedV2TreeUnionCount ||
        OutInput.HeritageAnchors.Num() != ExpectedHeritageAnchorCount ||
        OutInput.ShrubTransforms.Num() != ExpectedShrubCount ||
        OutInput.FlowerTransforms.Num() != ExpectedFlowerCount ||
        OutInput.UnderstoreyTransforms.Num() != ExpectedUnderstoreyCount ||
        OutInput.GeometryGrassTransforms.Num() != ExpectedGeometryGrassCount ||
        OutInput.CloseTurfTransforms.Num() != ExpectedCloseTurfCount)
    {
        OutError = TEXT("The exact V4 editor-to-runtime population census changed.");
        return false;
    }
    const int32 ExpectedNewPlantCount = ExpectedShrubCount +
        ExpectedFlowerCount + ExpectedUnderstoreyCount +
        ExpectedGeometryGrassCount;
    if (OutAudit.ExclusionSourceComponentCount != 8 ||
        OutAudit.ExclusionTriangleCount <= 0 ||
        OutAudit.FormalPlantingTriangleCount != 24 ||
        OutAudit.Roles.Num() != 7 ||
        AcceptedNewCentersCm.Num() != ExpectedNewPlantCount)
    {
        OutError = TEXT("The exact geometry-backed placement audit/census changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateFormalBedComponentPrefix(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    int32 ExpectedTotal,
    int32 ExpectedFormalPrefix,
    float MinimumUniformScale,
    float MaximumUniformScale,
    const TCHAR* Label,
    FString& OutError)
{
    if (!Component || Component->GetInstanceCount() != ExpectedTotal ||
        ExpectedFormalPrefix <= 0 || ExpectedFormalPrefix > ExpectedTotal)
    {
        OutError = FString::Printf(
            TEXT("The exact %s formal-bed/total census changed."), Label);
        return false;
    }
    int32 Left = 0;
    int32 Right = 0;
    for (int32 Index = 0; Index < ExpectedFormalPrefix; ++Index)
    {
        FTransform Transform;
        if (!Component->GetInstanceTransform(Index, Transform, true))
        {
            OutError = FString::Printf(
                TEXT("Could not read a persisted %s formal-bed transform."),
                Label);
            return false;
        }
        const FVector LocationMeters = Transform.GetLocation() / 100.0f;
        const FVector Scale = Transform.GetScale3D();
        const bool bInExactBed =
            FMath::Abs(LocationMeters.X) >= 20.75f &&
            FMath::Abs(LocationMeters.X) <= 47.25f &&
            LocationMeters.Y >= 79.75f &&
            LocationMeters.Y <= 96.25f;
        if (!bInExactBed || Transform.ContainsNaN() ||
            Scale.X < MinimumUniformScale - 0.001f ||
            Scale.X > MaximumUniformScale + 0.001f ||
            !FMath::IsNearlyEqual(Scale.X, Scale.Z, 0.001f) ||
            Scale.Y < Scale.X * 0.879f || Scale.Y > Scale.X * 1.121f)
        {
            OutError = FString::Printf(
                TEXT("A persisted %s formal-bed transform escaped its exact envelope/scale policy."),
                Label);
            return false;
        }
        Left += LocationMeters.X < 0.0f ? 1 : 0;
        Right += LocationMeters.X > 0.0f ? 1 : 0;
    }
    if (Left != ExpectedFormalPrefix / 2 ||
        Right != ExpectedFormalPrefix / 2)
    {
        OutError = FString::Printf(
            TEXT("The %s formal-bed prefix lost paired left/right coverage."),
            Label);
        return false;
    }
    int32 CoverageQuadrants[4] = {0, 0, 0, 0};
    for (int32 Pair = 0; Pair < ExpectedFormalPrefix / 2; ++Pair)
    {
        FTransform LeftTransform;
        FTransform RightTransform;
        if (!Component->GetInstanceTransform(
                Pair * 2, LeftTransform, true) ||
            !Component->GetInstanceTransform(
                Pair * 2 + 1, RightTransform, true) ||
            LeftTransform.GetLocation().X >= 0.0f ||
            RightTransform.GetLocation().X <= 0.0f ||
            !FMath::IsNearlyEqual(
                -LeftTransform.GetLocation().X,
                RightTransform.GetLocation().X,
                0.1f) ||
            !FMath::IsNearlyEqual(
                LeftTransform.GetLocation().Y,
                RightTransform.GetLocation().Y,
                0.1f))
        {
            OutError = FString::Printf(
                TEXT("A persisted %s formal-bed pair lost its mirrored planting-box layout."),
                Label);
            return false;
        }
        const FVector LocationMeters =
            LeftTransform.GetLocation() / 100.0f;
        const int32 Quadrant =
            (FMath::Abs(LocationMeters.X) >= 34.0f ? 1 : 0) |
            (LocationMeters.Y >= 88.0f ? 2 : 0);
        ++CoverageQuadrants[Quadrant];
    }
    for (const int32 QuadrantCount : CoverageQuadrants)
    {
        if (QuadrantCount != ExpectedFormalPrefix / 8)
        {
            OutError = FString::Printf(
                TEXT("The persisted %s formal-bed prefix lost exact four-quadrant planting-box coverage."),
                Label);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateFormalBedPopulation(
    const ATRIADIstanaExploreV4LandscapeActor* V4,
    FString& OutError)
{
    return V4 &&
        ValidateFormalBedComponentPrefix(
            V4->ShrubInstances, ExpectedShrubCount, FormalBedShrubCount,
            4.0f, 7.5f, TEXT("shrub"), OutError) &&
        ValidateFormalBedComponentPrefix(
            V4->FlowerInstances, ExpectedFlowerCount, FormalBedFlowerCount,
            2.8f, 4.8f, TEXT("flower"), OutError) &&
        ValidateFormalBedComponentPrefix(
            V4->UnderstoreyInstances, ExpectedUnderstoreyCount,
            FormalBedUnderstoreyCount,
            4.0f, 8.0f, TEXT("understorey"), OutError);
}

TArray<UHierarchicalInstancedStaticMeshComponent*> V4HismComponents(
    ATRIADIstanaExploreV4LandscapeActor* V4)
{
    if (!V4)
    {
        return {};
    }
    return {
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
        V4->HeritageAnchorPawnBlockers,
        V4->ShrubInstances,
        V4->FlowerInstances,
        V4->UnderstoreyInstances,
        V4->GeometryGrassInstances,
        V4->CloseTurfInstances};
}

bool ValidateV4HismTreesAfterPopulation(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    FString& OutError)
{
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components =
        V4HismComponents(V4);
    if (Components.Num() != 16 || Components.Contains(nullptr))
    {
        OutError = TEXT("The exact 16-component V4 HISM roster is absent.");
        return false;
    }
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (!Component->bAutoRebuildTreeOnInstanceChanges ||
            Component->IsAsyncBuilding() || !Component->IsTreeFullyBuilt())
        {
            OutError = TEXT("A V4 HISM cluster tree was not synchronously complete after the runtime population transaction.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

UHierarchicalInstancedStaticMeshComponent* MainTreeComponentForForm(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    ETRIADIstanaExploreV4TreeForm Form)
{
    if (!V4)
    {
        return nullptr;
    }
    switch (Form)
    {
    case ETRIADIstanaExploreV4TreeForm::Umbrella:
        return V4->UmbrellaTreeInstances;
    case ETRIADIstanaExploreV4TreeForm::Dome:
        return V4->DomeTreeInstances;
    case ETRIADIstanaExploreV4TreeForm::HighForkRounded:
        return V4->HighForkRoundedTreeInstances;
    case ETRIADIstanaExploreV4TreeForm::ColumnarNarrow:
        return V4->ColumnarNarrowTreeInstances;
    case ETRIADIstanaExploreV4TreeForm::Palm:
        return V4->PalmTreeInstances;
    default:
        return nullptr;
    }
}

bool ValidateExactReplacementBeforeHiding(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    const FTRIADIstanaExploreV4PopulationInput& Input,
    FString& OutError)
{
    const TArray<float> NativeTreeHeightsCm = {
        static_cast<float>(MeshHeightCm(
            V4 && V4->UmbrellaTreeInstances
                ? V4->UmbrellaTreeInstances->GetStaticMesh() : nullptr)),
        static_cast<float>(MeshHeightCm(
            V4 && V4->DomeTreeInstances
                ? V4->DomeTreeInstances->GetStaticMesh() : nullptr)),
        static_cast<float>(MeshHeightCm(
            V4 && V4->HighForkRoundedTreeInstances
                ? V4->HighForkRoundedTreeInstances->GetStaticMesh() : nullptr)),
        static_cast<float>(MeshHeightCm(
            V4 && V4->ColumnarNarrowTreeInstances
                ? V4->ColumnarNarrowTreeInstances->GetStaticMesh() : nullptr)),
        static_cast<float>(MeshHeightCm(
            V4 && V4->PalmTreeInstances
                ? V4->PalmTreeInstances->GetStaticMesh() : nullptr))};
    TArray<FTRIADIstanaExploreV4ClassifiedTreeRow> Rows;
    if (!V4 || !ATRIADIstanaExploreV4LandscapeActor::
            BuildDeterministicReclassifiedRows(
                Input.InheritedV2TreeWorldTransforms,
                Input.RasterCues,
                NativeTreeHeightsCm,
                0x4757A6A5,
                Rows,
                OutError))
    {
        return false;
    }
    int32 ReadIndices[5] = {0, 0, 0, 0, 0};
    for (const FTRIADIstanaExploreV4ClassifiedTreeRow& Row : Rows)
    {
        const int32 FormIndex = static_cast<int32>(Row.Form);
        const FTransform& SourceTransform =
            Input.InheritedV2TreeWorldTransforms[Row.SourceIndex];
        const bool bExactColumnarSourceIndex =
            Row.SourceIndex >= ExpectedV2UmbrellaCount &&
            Row.SourceIndex <
                ExpectedV2UmbrellaCount + ExpectedV2ColumnarCount;
        UHierarchicalInstancedStaticMeshComponent* Component =
            MainTreeComponentForForm(V4, Row.Form);
        FTransform Actual;
        if (!Component || !Component->GetInstanceTransform(
                ReadIndices[FormIndex]++, Actual, true) ||
            !Actual.Equals(Row.WorldTransform, 0.001f) ||
            !Actual.GetLocation().Equals(SourceTransform.GetLocation(), 0.001f) ||
            !Actual.GetRotation().Equals(SourceTransform.GetRotation(), 0.00001f) ||
            bExactColumnarSourceIndex !=
                (Row.Form == ETRIADIstanaExploreV4TreeForm::ColumnarNarrow) ||
            (bExactColumnarSourceIndex &&
                !Actual.Equals(SourceTransform, 0.001f)) ||
            (!bExactColumnarSourceIndex &&
                (Row.Form == ETRIADIstanaExploreV4TreeForm::ColumnarNarrow ||
                 Row.Form == ETRIADIstanaExploreV4TreeForm::Palm ||
                 !FMath::IsNearlyEqual(
                    Actual.GetScale3D().X,
                    Actual.GetScale3D().Y,
                    0.001f) ||
                 !FMath::IsNearlyEqual(
                    Actual.GetScale3D().X,
                    Actual.GetScale3D().Z,
                    0.001f))))
        {
            OutError = TEXT("The live V4 union failed strict 272/232/182/34 input order, exact full-transform columnar inheritance, or uniform-height non-columnar readback before old visuals were hidden.");
            return false;
        }
        if (!bExactColumnarSourceIndex)
        {
            const double HeightCm = NativeTreeHeightsCm[FormIndex] *
                Actual.GetScale3D().Z;
            const bool bHeightExact =
                (Row.Form == ETRIADIstanaExploreV4TreeForm::Umbrella &&
                    HeightCm >= 2000.0 - 0.01 &&
                    HeightCm <= 2428.810830713951 + 0.01) ||
                (Row.Form == ETRIADIstanaExploreV4TreeForm::Dome &&
                    FMath::IsNearlyEqual(HeightCm, 3650.0, 0.01)) ||
                (Row.Form ==
                    ETRIADIstanaExploreV4TreeForm::HighForkRounded &&
                    FMath::IsNearlyEqual(HeightCm, 2830.0, 0.01));
            if (!bHeightExact)
            {
                OutError = TEXT("A V4 non-columnar replacement escaped the frozen uniform-height policy.");
                return false;
            }
        }
    }
    int32 MainTotal = 0;
    for (int32 FormIndex = 0; FormIndex < 5; ++FormIndex)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            MainTreeComponentForForm(
                V4,
                static_cast<ETRIADIstanaExploreV4TreeForm>(FormIndex));
        if (!Component || Component->GetInstanceCount() != ReadIndices[FormIndex])
        {
            OutError = TEXT("A V4 main-tree HISM has an unexpected pre-hide form census.");
            return false;
        }
        MainTotal += Component->GetInstanceCount();
    }
    if (MainTotal != ExpectedV2TreeUnionCount ||
        ReadIndices[static_cast<int32>(
            ETRIADIstanaExploreV4TreeForm::ColumnarNarrow)] !=
            ExpectedV2ColumnarCount ||
        ReadIndices[static_cast<int32>(
            ETRIADIstanaExploreV4TreeForm::Palm)] != 0 ||
        !V4->HeritageAnchorPawnBlockers ||
        V4->HeritageAnchorPawnBlockers->GetInstanceCount() !=
            ExpectedHeritageAnchorCount ||
        !V4->CloseTurfInstances ||
        V4->CloseTurfInstances->GetInstanceCount() != ExpectedCloseTurfCount ||
        !V4->PorticoV8RenderOnlyComponent ||
        !V4->PorticoV8RenderOnlyComponent->GetComponentTransform().Equals(
            FTransform::Identity, 0.001f) ||
        V4->PorticoV8RenderOnlyComponent->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        !ValidateFormalBedPopulation(V4, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V4 pre-hide Heritage/turf/formal-bed/identity-portico validation failed.");
        }
        return false;
    }
    for (int32 Index = 0; Index < ExpectedCloseTurfCount; ++Index)
    {
        FTransform Actual;
        if (!V4->CloseTurfInstances->GetInstanceTransform(
                Index, Actual, true) ||
            !Actual.Equals(Input.CloseTurfTransforms[Index], 0.001f))
        {
            OutError = TEXT("The V4 close-turf HISM did not reproduce all 18,432 V3 world transforms before hiding V3 turf.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateTemplateWorldBeforeMutation(
    UWorld* TemplateWorld,
    const FString& ExpectedTemplatePackage,
    const FV3WorldSnapshot& Source,
    FString& OutError)
{
    ATRIADIstanaExploreV2LandscapeActor* V2 = FindV2Landscape(TemplateWorld);
    ATRIADIstanaExploreV3SupplementActor* V3 = FindV3Supplement(TemplateWorld);
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(TemplateWorld);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(TemplateWorld);
    const TArray<UHierarchicalInstancedStaticMeshComponent*> V2Live =
        V2Components(V2);
    const TArray<UStaticMeshComponent*> V3Live = V3Components(V3);
    const TArray<UStaticMeshComponent*> SceneLive = SceneComponents(Scene);
    if (!TemplateWorld || !TemplateWorld->GetOutermost() ||
        ExpectedTemplatePackage.IsEmpty() ||
        !FPackageName::IsTempPackage(ExpectedTemplatePackage) ||
        ExpectedTemplatePackage == SourceMapPackage ||
        ExpectedTemplatePackage == DestinationMapPackage ||
        TemplateWorld->GetOutermost()->GetName() != ExpectedTemplatePackage ||
        !V2 || !V3 || !Scene || !Policy || FindV4Landscape(TemplateWorld) ||
        Policy->bEnforceFixedPrimaryCamera ||
        !V2->GetActorTransform().Equals(Source.V2ActorTransform, 0.001f) ||
        !V3->GetActorTransform().Equals(Source.V3ActorTransform, 0.001f) ||
        V2Live.Num() != Source.V2Components.Num() ||
        V3Live.Num() != Source.V3Components.Num() ||
        SceneLive.Num() != Source.SceneComponents.Num() ||
        !TemplateWorld->GetWorldSettings() ||
        !TemplateWorld->GetWorldSettings()->DefaultGameMode ||
        TemplateWorld->GetWorldSettings()->DefaultGameMode.Get()->GetPathName() !=
            Source.GameModeClassPath)
    {
        OutError = TEXT("The sequential untitled V3 template world/actor/free-roam roster changed before V4 mutation.");
        return false;
    }
    for (int32 Index = 0; Index < V2Live.Num(); ++Index)
    {
        if (!MatchesComponentState(
                V2Live[Index], Source.V2Components[Index], false,
                FString(), OutError))
        {
            return false;
        }
    }
    for (int32 Index = 0; Index < V3Live.Num(); ++Index)
    {
        if (!MatchesComponentState(
                V3Live[Index], Source.V3Components[Index], false,
                FString(), OutError))
        {
            return false;
        }
    }
    for (int32 Index = 0; Index < SceneLive.Num(); ++Index)
    {
        if (!MatchesComponentState(
                SceneLive[Index], Source.SceneComponents[Index], false,
                FString(), OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

void HideInheritedVisual(UStaticMeshComponent* Component)
{
    check(Component);
    Component->Modify();
    Component->SetVisibility(false, true);
    Component->SetHiddenInGame(true, true);
}

bool AreV4HismTreesPersisted(
    ATRIADIstanaExploreV4LandscapeActor* V4,
    FString& OutError)
{
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components =
        V4HismComponents(V4);
    if (Components.Num() != 16 || Components.Contains(nullptr))
    {
        OutError = TEXT("The persisted V4 HISM roster is incomplete.");
        return false;
    }
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (!Component->bAutoRebuildTreeOnInstanceChanges ||
            Component->IsAsyncBuilding() || !Component->IsTreeFullyBuilt())
        {
            OutError = TEXT("A persisted V4 HISM cluster tree is asynchronous or outdated.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool MatchesExactWorldTransforms(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const TArray<FTransform>& Expected,
    const TCHAR* Label,
    FString& OutError)
{
    if (!Component || Component->GetInstanceCount() != Expected.Num())
    {
        OutError = FString::Printf(
            TEXT("The persisted %s placement census changed."), Label);
        return false;
    }
    for (int32 Index = 0; Index < Expected.Num(); ++Index)
    {
        FTransform Actual;
        if (!Component->GetInstanceTransform(Index, Actual, true) ||
            !Actual.Equals(Expected[Index], 0.001f))
        {
            OutError = FString::Printf(
                TEXT("Persisted %s transform %d escaped the exact geometry/raster/clearance build."),
                Label,
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateV4WorldInternal(
    UWorld* World,
    const FString& ExpectedWorldPackage,
    const FV3WorldSnapshot* PrecomputedSourceSnapshot,
    const FTRIADIstanaExploreV4PopulationInput* PrecomputedPopulation,
    const FPlacementAudit* PrecomputedPlacementAudit,
    bool bValidateAssets,
    FString& OutReport)
{
    const TStrongObjectPtr<UCesiumIonServer> CesiumIonServerKeepAlive(
        UCesiumIonServer::GetServerForNewObjects());
    if (!CesiumIonServerKeepAlive.IsValid() ||
        CesiumIonServerKeepAlive->GetOutermost()->IsDirty())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_MAP_INVALID_CESIUM_EDITOR_LIFETIME: the selected Cesium ion-server asset must resolve cleanly so inactive V3 snapshot unload garbage collection cannot invalidate CesiumEditor callbacks.");
        return false;
    }
    if ((PrecomputedPopulation == nullptr) !=
        (PrecomputedPlacementAudit == nullptr))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_MAP_INVALID: precomputed population and placement audit must be supplied together.");
        return false;
    }
    if (!World || !World->GetOutermost() || ExpectedWorldPackage.IsEmpty() ||
        World->GetOutermost()->GetName() != ExpectedWorldPackage)
    {
        OutReport = TEXT("The exact expected V4 build/validation world package is not active: ") +
            ExpectedWorldPackage;
        return false;
    }
    FString Error;
    if (bValidateAssets && !ValidateAllV4AssetsInternal(true, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_MAP_INVALID: complete exact V4 assets failed. ") + Error;
        return false;
    }
    FV3WorldSnapshot LoadedSourceSnapshot;
    const FV3WorldSnapshot* ExpectedSourceSnapshot =
        PrecomputedSourceSnapshot;
    if (!ExpectedSourceSnapshot)
    {
        if (!CaptureV3WorldSnapshotForV4Validation(
                LoadedSourceSnapshot, Error))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_MAP_INVALID: protected V3 inheritance failed. ") +
                Error;
            return false;
        }
        ExpectedSourceSnapshot = &LoadedSourceSnapshot;
    }
    if (!ValidateTargetInheritance(
            World,
            *ExpectedSourceSnapshot,
            LawnMaterialInstanceObjectPath(),
            Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_MAP_INVALID: protected V3 inheritance failed. ") + Error;
        return false;
    }

    ATRIADIstanaExploreV4LandscapeActor* V4 = FindV4Landscape(World);
    ATRIADIstanaExploreV2LandscapeActor* V2 = FindV2Landscape(World);
    ATRIADIstanaExploreV3SupplementActor* V3 = FindV3Supplement(World);
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(World);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(World);
    UClass* ExploreGameMode = StaticLoadClass(
        AGameModeBase::StaticClass(), nullptr, *ExploreGameModeClassPath);
    FString V4Report;
    FString SceneReport;
    if (!V4 || !V2 || !V3 || !Scene || !Policy ||
        Policy->bEnforceFixedPrimaryCamera || !ExploreGameMode ||
        !World->GetWorldSettings() ||
        World->GetWorldSettings()->DefaultGameMode.Get() != ExploreGameMode ||
        !Scene->ValidatePublicViewScene(SceneReport, false) ||
        !V4->ValidateExploreV4Landscape(V4Report))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_MAP_INVALID: free-roam/manual-exposure policy or base scene/landscape validation failed. ") +
            SceneReport + TEXT(" ") + V4Report + TEXT(" ") + Error;
        return false;
    }

    FTRIADIstanaExploreV4PopulationInput RecomputedPopulation;
    FPlacementAudit RecomputedPlacementAudit;
    const FTRIADIstanaExploreV4PopulationInput* ExpectedPopulationPtr =
        PrecomputedPopulation;
    const FPlacementAudit* ExpectedPlacementAuditPtr =
        PrecomputedPlacementAudit;
    if (!ExpectedPopulationPtr)
    {
        UWorld* CurrentEditorWorld = GEditor
            ? GEditor->GetEditorWorldContext().World()
            : nullptr;
        if (World != CurrentEditorWorld || !Scene->TerrainComponent ||
            Scene->TerrainComponent->GetWorld() != World)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_MAP_INVALID: terrain population may be recomputed only from the active loaded V4 editor world; inactive/template worlds require the precomputed source population.");
            return false;
        }
        if (!BuildV4PopulationInput(
            V2,
            V3,
            Scene,
            Scene->TerrainComponent,
            RecomputedPopulation,
            RecomputedPlacementAudit,
            Error))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V4_MAP_INVALID: active V4 terrain population recomputation failed. ") +
                Error;
            return false;
        }
        ExpectedPopulationPtr = &RecomputedPopulation;
        ExpectedPlacementAuditPtr = &RecomputedPlacementAudit;
    }
    const FTRIADIstanaExploreV4PopulationInput& ExpectedPopulation =
        *ExpectedPopulationPtr;
    const FPlacementAudit& ExpectedPlacementAudit =
        *ExpectedPlacementAuditPtr;
    if (ExpectedPlacementAudit.ExclusionSourceComponentCount != 8 ||
        ExpectedPlacementAudit.ExclusionTriangleCount <= 0 ||
        ExpectedPlacementAudit.FormalPlantingTriangleCount != 24 ||
        !V4->Tags.Contains(PlacementPolicyTag) ||
        !V4->Tags.Contains(FName(*ExpectedPlacementAudit.BuildPersistedTag())) ||
        !MatchesExactWorldTransforms(
            V4->ShrubInstances,
            ExpectedPopulation.ShrubTransforms,
            TEXT("shrub"),
            Error) ||
        !MatchesExactWorldTransforms(
            V4->FlowerInstances,
            ExpectedPopulation.FlowerTransforms,
            TEXT("flower"),
            Error) ||
        !MatchesExactWorldTransforms(
            V4->UnderstoreyInstances,
            ExpectedPopulation.UnderstoreyTransforms,
            TEXT("understorey"),
            Error) ||
        !MatchesExactWorldTransforms(
            V4->GeometryGrassInstances,
            ExpectedPopulation.GeometryGrassTransforms,
            TEXT("geometry grass"),
            Error) ||
        !ValidateFormalBedPopulation(V4, Error) ||
        !AreV4HismTreesPersisted(V4, Error) ||
        !Scene->TerrainComponent ||
        Scene->TerrainComponent->GetMaterial(0) !=
            LoadExact<UMaterialInstanceConstant>(
                LawnMaterialInstanceObjectPath()) ||
        !ValidateStaticLawnMaterial(
            LoadExact<UMaterial>(LawnBaseMaterialObjectPath()),
            LoadExact<UMaterialInstanceConstant>(
                LawnMaterialInstanceObjectPath()),
            Error) ||
        V2->TreeTrunkPawnBlockers->GetInstanceCount() !=
            ExpectedV2TreeUnionCount ||
        V3->PorticoV7RenderOnlyComponent->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        V4->PorticoV8RenderOnlyComponent->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_MAP_INVALID: free-roam/manual-exposure policy, geometry/raster/clearance population, formal beds, terrain lawn, blocker or portico validation failed. ") +
            SceneReport + TEXT(" ") + V4Report + TEXT(" ") + Error;
        return false;
    }
    OutReport = TEXT("Validated additive Explore V4: exact source-equivalent Explore V3 template inheritance; preserved terrain mesh/transform/QueryAndPhysics collision, HDB/OSM and all 720 V2 Pawn blockers; exact 720-row strict umbrella/dome/high-fork/columnar replacement with the frozen 232-row columnar subset and zero free-roam palms; nine published Heritage anchors with separate girth-sized Pawn blockers; exact geometry-triangle/raster/tree/mutual-clearance placement audit with 256/128/192 mirrored planting-box prefixes, class-10 flanks and 1,536 class-30 taller-grass clumps; exact 18,432-transform animated close-turf replacement; static 41-node two-detail/32 m macro-variation NormalDX/AO lawn with zero WPO; synchronous persisted HISM trees; identity no-collision V8 successor; free-roam Player0/manual-exposure policy. One-to-one, survey, botanical-inventory and sensor-truth claims remain false.");
    return true;
}

bool ValidateV4WorldAgainstPrecomputedPopulation(
    UWorld* World,
    const FString& ExpectedWorldPackage,
    const FV3WorldSnapshot& SourceSnapshot,
    const FTRIADIstanaExploreV4PopulationInput& Population,
    const FPlacementAudit& PlacementAudit,
    bool bValidateAssets,
    FString& OutReport)
{
    return ValidateV4WorldInternal(
        World,
        ExpectedWorldPackage,
        &SourceSnapshot,
        &Population,
        &PlacementAudit,
        bValidateAssets,
        OutReport);
}

bool ValidateV4World(UWorld* World, FString& OutReport)
{
    return ValidateV4WorldInternal(
        World,
        DestinationMapPackage,
        nullptr,
        nullptr,
        nullptr,
        true,
        OutReport);
}

bool GetLightweightAcceptedV4PlayState(
    UWorld*& OutPlayWorld,
    APlayerController*& OutPlayer,
    ATRIADIstanaFreeRoamPawn*& OutPawn,
    ATRIADIstanaExploreV4LandscapeActor*& OutV4,
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
        OutError = TEXT("The accepted exact V4 editor map is absent or became dirty; rerun explicit PIE validation.");
        return false;
    }
    OutPlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!OutPlayWorld || OutPlayWorld->WorldType != EWorldType::PIE ||
        !OutPlayWorld->IsGameWorld() || !OutPlayWorld->HasBegunPlay() ||
        UWorld::RemovePIEPrefix(
            OutPlayWorld->GetOutermost()->GetName()) != DestinationMapPackage)
    {
        OutError = TEXT("Exact begun Explore V4 PIE world is absent.");
        return false;
    }
    if (bRequirePriorFullAcceptance &&
        (FullyAcceptedQaEditorWorld.Get() != EditorWorld ||
         FullyAcceptedQaPlayWorld.Get() != OutPlayWorld))
    {
        OutError = TEXT("Run ValidateIstanaExploreV4PlayWorld once for this exact editor/PIE world before bounded QA actions.");
        return false;
    }
    OutPlayer = UGameplayStatics::GetPlayerController(OutPlayWorld, 0);
    OutPawn = OutPlayer
        ? Cast<ATRIADIstanaFreeRoamPawn>(OutPlayer->GetPawn())
        : nullptr;
    OutV4 = FindV4Landscape(OutPlayWorld);
    ATRIADIstanaExploreV2LandscapeActor* V2 = FindV2Landscape(OutPlayWorld);
    ATRIADIstanaExploreV3SupplementActor* V3 = FindV3Supplement(OutPlayWorld);
    if (!OutPlayer || !OutPawn ||
        !OutPawn->HasExpectedExploreCameraProfile() ||
        OutPlayer->GetViewTarget() != OutPawn ||
        !OutV4 || !OutV4->HasActorBegunPlay() ||
        !OutV4->IsWindRuntimeActive() ||
        !V2 || !V2->HasActorBegunPlay() || !V2->IsWindRuntimeActive() ||
        !V3 || !V3->HasActorBegunPlay() || !V3->IsWindRuntimeActive())
    {
        OutError = TEXT("V4 PIE lightweight Player0/free-roam/manual-exposure camera and inherited/V4 begun/wind identity failed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool GetValidatedV4PlayState(
    UWorld*& OutPlayWorld,
    APlayerController*& OutPlayer,
    ATRIADIstanaFreeRoamPawn*& OutPawn,
    ATRIADIstanaExploreV4LandscapeActor*& OutV4,
    FString& OutError)
{
    FullyAcceptedQaEditorWorld.Reset();
    FullyAcceptedQaPlayWorld.Reset();
    UWorld* EditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString EditorReport;
    if (!ValidateV4World(EditorWorld, EditorReport))
    {
        OutError = TEXT("V4 PIE editor-map validation failed: ") + EditorReport;
        return false;
    }
    if (!GetLightweightAcceptedV4PlayState(
            OutPlayWorld,
            OutPlayer,
            OutPawn,
            OutV4,
            false,
            OutError))
    {
        return false;
    }
    FString RuntimeReport;
    if (!OutV4->ValidateExploreV4Landscape(RuntimeReport))
    {
        OutError = TEXT("V4 PIE full runtime landscape acceptance failed: ") +
            RuntimeReport;
        return false;
    }
    FullyAcceptedQaEditorWorld = EditorWorld;
    FullyAcceptedQaPlayWorld = OutPlayWorld;
    OutError.Reset();
    return true;
}
}

bool UTRIADIstanaExploreV4EditorLibrary::
    PrewarmIstanaExploreV4ProtectedReferencesForImport(
        FString& OutMessage)
{
    FString Error;
    int32 KnownAutoDirtyMaterialCount = 0;
    if (HasDisallowedDirtyPackages(KnownAutoDirtyMaterialCount, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_REFUSED_DIRTY: ") + Error;
        return false;
    }
    if (!ValidateCleanEntryProcessBoundary(
            KnownAutoDirtyMaterialCount, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_REFUSED_ENTRY: ") + Error;
        return false;
    }
    if (!EnsureAssetRegistryDiscoveryComplete(Error))
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_REFUSED_ASSET_DISCOVERY: ") +
            Error;
        return false;
    }
    const TStrongObjectPtr<UCesiumIonServer> CesiumIonServerKeepAlive(
        UCesiumIonServer::GetServerForNewObjects());
    if (!CesiumIonServerKeepAlive.IsValid() ||
        CesiumIonServerKeepAlive->GetOutermost()->IsDirty())
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_REFUSED_CESIUM_EDITOR_LIFETIME: the selected Cesium ion-server asset must resolve cleanly so long-running package unloads cannot invalidate CesiumEditor callbacks.");
        return false;
    }
    if (CountAssetsUnderV4Root() != 0)
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_REFUSED_DESTINATION: the exact V4-owned namespace must be absent before receipt production.");
        return false;
    }
    TSharedPtr<FJsonObject> Contract;
    TArray<FTextureSourceSpec> TextureSpecs;
    if (!ValidateFrozenPublicDataContracts(Error) ||
        !ValidateFrozenVegetationContract(Contract, Error) ||
        !ValidateExternalMeshContractRows(Contract, Error) ||
        !ValidateProtectedDerivativeContractRows(Contract, Error) ||
        !BuildRequiredTextureSpecs(Contract, TextureSpecs, Error))
    {
        FString InvalidationError;
        const bool bInvalidated =
            RemoveProtectedPrewarmReceiptExact(InvalidationError);
        OutMessage = TEXT("EXPLORE_V4_PREWARM_REFUSED_CONTRACT_DRIFT: ") +
            Error + TEXT(" receiptInvalidation=") +
            (bInvalidated ? TEXT("complete") :
                TEXT("FAILED: ") + InvalidationError);
        return false;
    }
    TArray<FProtectedFileDigest> Protected;
    if (!CaptureProtectedPackages(Protected, Error))
    {
        FString InvalidationError;
        const bool bInvalidated =
            RemoveProtectedPrewarmReceiptExact(InvalidationError);
        OutMessage = TEXT("EXPLORE_V4_PREWARM_REFUSED_PROTECTED_DRIFT: ") +
            Error + TEXT(" receiptInvalidation=") +
            (bInvalidated ? TEXT("complete") :
                TEXT("FAILED: ") + InvalidationError);
        return false;
    }
    if (!ValidateProtectedPrewarmSourcesUnloaded(Error))
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_REFUSED_LOADED_SOURCE: ") +
            Error;
        return false;
    }
    if (IFileManager::Get().FileExists(
            *ProtectedPrewarmReceiptFilename()))
    {
        if (ValidateProtectedPrewarmReceipt(
                Protected, false, true, Error))
        {
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V4_PREWARM_RECEIPT_ALREADY_VALID: the exact engine/project/contract/protected/local-DDC receipt is ready. Close this editor process completely, then run ImportIstanaExploreV4Assets from a new clean editor process (Remote Control requires a rendering-capable off-screen editor, not -NullRHI).");
            return true;
        }
        if (IFileManager::Get().FileExists(
                *ProtectedPrewarmReceiptFilename()))
        {
            OutMessage = TEXT("EXPLORE_V4_PREWARM_REFUSED_EXISTING_RECEIPT: ") +
                Error;
            return false;
        }
    }
    int32 CleanStaticMeshKeyEnvironmentRecordCount = 0;
    FString CleanStaticMeshKeyEnvironmentSha;
    TArray<FProtectedPrewarmReceiptEnvironmentRow>
        CleanStaticMeshKeyEnvironmentRows;
    if (!CaptureStaticMeshKeyEnvironmentBinding(
            CleanStaticMeshKeyEnvironmentRecordCount,
            CleanStaticMeshKeyEnvironmentSha,
            CleanStaticMeshKeyEnvironmentRows,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_FAILED_CLEAN_ENVIRONMENT: no protected source was loaded and no receipt was published. ") +
            Error;
        return false;
    }
    TMap<FString, FProtectedPrewarmDerivedDataProof> DerivedDataProofs;
    if (!PrewarmAndUnloadProtectedMeshReferences(
            Protected, DerivedDataProofs, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_FAILED: exact one-at-a-time protected mesh load/topology/main-and-CARD-local-DDC/unload/hash gate failed. ") +
            Error;
        return false;
    }
    if (CountAssetsUnderV4Root() != 0 ||
        HasDisallowedDirtyPackages(KnownAutoDirtyMaterialCount, Error) ||
        !ValidateCleanEntryProcessBoundary(
            KnownAutoDirtyMaterialCount, Error) ||
        !ValidateProtectedPrewarmSourcesUnloaded(Error) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_FAILED_POSTCONDITION: no receipt was published. ") +
            Error;
        return false;
    }
    FProtectedPrewarmReceipt Receipt;
    if (!CaptureCurrentProtectedPrewarmReceiptState(
            Protected, DerivedDataProofs, Receipt, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_FAILED_RECEIPT_STATE: no receipt was published. ") +
            Error;
        return false;
    }
    FString EnvironmentRowDifference;
    const bool bEnvironmentRowsMatch =
        CompareStaticMeshKeyEnvironmentRows(
            CleanStaticMeshKeyEnvironmentRows,
            Receipt.StaticMeshKeyEnvironmentRows,
            EnvironmentRowDifference);
    if (Receipt.StaticMeshKeyEnvironmentRecordCount !=
            CleanStaticMeshKeyEnvironmentRecordCount ||
        Receipt.StaticMeshKeyEnvironmentSha256 !=
            CleanStaticMeshKeyEnvironmentSha ||
        !bEnvironmentRowsMatch)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V4_PREWARM_FAILED_ENVIRONMENT_DRIFT: protected prewarm changed the exact resolved StaticMesh key environment; no receipt was published. cleanRows=%d cleanSha=%s postRows=%d postSha=%s rowDiffs=%s"),
            CleanStaticMeshKeyEnvironmentRecordCount,
            *CleanStaticMeshKeyEnvironmentSha,
            Receipt.StaticMeshKeyEnvironmentRecordCount,
            *Receipt.StaticMeshKeyEnvironmentSha256,
            bEnvironmentRowsMatch
                ? TEXT("NONE_AGGREGATE_MISMATCH")
                : *EnvironmentRowDifference);
        return false;
    }
    // Bind the clean pre-load capture explicitly. The equal post-unload capture
    // above proves prewarm did not alter any resolved key-affecting state.
    Receipt.StaticMeshKeyEnvironmentRecordCount =
        CleanStaticMeshKeyEnvironmentRecordCount;
    Receipt.StaticMeshKeyEnvironmentSha256 =
        CleanStaticMeshKeyEnvironmentSha;
    Receipt.StaticMeshKeyEnvironmentRows =
        CleanStaticMeshKeyEnvironmentRows;
    const FGuid AppInstanceId = FApp::GetInstanceId();
    Receipt.ReceiptId =
        FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Receipt.CreatedUtc = FDateTime::UtcNow().ToIso8601();
    Receipt.ProducerProcessId = LexToString(
        FPlatformProcess::GetCurrentProcessId());
    Receipt.ProducerAppInstanceId =
        AppInstanceId.ToString(EGuidFormats::Digits);
    if (!AppInstanceId.IsValid() ||
        !HashUtf8StringSha256(
            BuildProtectedPrewarmReceiptCanonical(Receipt),
            Receipt.BindingSha256) ||
        !WriteProtectedPrewarmReceiptAtomically(Receipt, Error) ||
        !ValidateProtectedPrewarmReceipt(
            Protected, false, true, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_PREWARM_FAILED_RECEIPT_PUBLICATION: ") +
            Error;
        return false;
    }
    OutMessage = TEXT("EXPLORE_V4_PREWARM_RECEIPT_READY: exact two protected references were prewarmed and unloaded one at a time, their local StaticMesh MeshData and exact Card-only legacy-DDC records plus engine/project/target/config/CVar/contract/protected identities were durably receipt-bound, and zero V4 assets were created. The external launcher must retain the producer process handle, verify this receipt's producer PID equals that handle's PID, and WaitForExit/Wait-Process before starting a new clean consumer editor; IsApplicationRunning alone is not an OS-handle wait proof. Remote Control requires a rendering-capable off-screen editor; do not use -NullRHI for this UFUNCTION.");
    return true;
}

bool UTRIADIstanaExploreV4EditorLibrary::ImportIstanaExploreV4Assets(
    FString& OutMessage)
{
    FString Error;
    int32 KnownAutoDirtyMaterialCount = 0;
    if (HasDisallowedDirtyPackages(
            KnownAutoDirtyMaterialCount, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_IMPORT_REFUSED: ") + Error;
        return false;
    }
    TSharedPtr<FJsonObject> Contract;
    TArray<FTextureSourceSpec> TextureSpecs;
    if (!ValidateFrozenPublicDataContracts(Error) ||
        !ValidateFrozenVegetationContract(Contract, Error) ||
        !ValidateExternalMeshContractRows(Contract, Error) ||
        !ValidateProtectedDerivativeContractRows(Contract, Error) ||
        !BuildRequiredTextureSpecs(Contract, TextureSpecs, Error))
    {
        FString InvalidationError;
        const bool bInvalidated =
            RemoveProtectedPrewarmReceiptExact(InvalidationError);
        OutMessage = TEXT("EXPLORE_V4_IMPORT_REFUSED: frozen source/contract preflight failed before asset creation. ") +
            Error + TEXT(" receiptInvalidation=") +
            (bInvalidated ? TEXT("complete") :
                TEXT("FAILED: ") + InvalidationError);
        return false;
    }
    if (!EnsureAssetRegistryDiscoveryComplete(Error))
    {
        OutMessage = TEXT("EXPLORE_V4_IMPORT_REFUSED_ASSET_DISCOVERY: ") +
            Error;
        return false;
    }
    const TStrongObjectPtr<UCesiumIonServer> CesiumIonServerKeepAlive(
        UCesiumIonServer::GetServerForNewObjects());
    if (!CesiumIonServerKeepAlive.IsValid() ||
        CesiumIonServerKeepAlive->GetOutermost()->IsDirty())
    {
        OutMessage = TEXT("EXPLORE_V4_IMPORT_REFUSED_CESIUM_EDITOR_LIFETIME: the selected Cesium ion-server asset must resolve cleanly so V4 rollback and staged unload garbage collection cannot invalidate CesiumEditor callbacks.");
        return false;
    }
    const int32 Existing = CountAssetsUnderV4Root();
    if (Existing > 0)
    {
        FString ExistingError;
        if (Existing == 85 &&
            ValidateAllV4AssetsInternal(false, ExistingError))
        {
            if (HasDisallowedDirtyPackages(
                    KnownAutoDirtyMaterialCount, ExistingError))
            {
                OutMessage = TEXT("EXPLORE_V4_IMPORT_REFUSED_AFTER_IDEMPOTENT_VALIDATION: ") +
                    ExistingError;
                return false;
            }
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V4_ASSETS_ALREADY_VALID: exact complete 85-asset V4-owned namespace and two protected read-only mesh references are unchanged.");
            if (KnownAutoDirtyMaterialCount > 0)
            {
                OutMessage += TEXT(" WARNING: exact protected V1 HISM-usage materials remain intentionally dirty in memory; do not use Save All.");
            }
            FString ReceiptCleanupError;
            if (!RemoveProtectedPrewarmReceiptExact(ReceiptCleanupError))
            {
                OutMessage += TEXT(" WARNING: completed idempotent import could not remove the stale exact prewarm receipt: ") +
                    ReceiptCleanupError;
            }
            return true;
        }
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V4_IMPORT_REFUSED_PARTIAL_DESTINATION: expected either zero or 85 exact V4-owned assets under %s, found %d. No overwrite or partial repair is authorized. %s"),
            *AssetRoot,
            Existing,
            *ExistingError);
        return false;
    }

    TArray<FProtectedFileDigest> Protected;
    if (!CaptureProtectedPackages(Protected, Error))
    {
        FString InvalidationError;
        const bool bInvalidated =
            RemoveProtectedPrewarmReceiptExact(InvalidationError);
        OutMessage = TEXT("EXPLORE_V4_IMPORT_REFUSED: protected-package byte snapshot failed. ") +
            Error + TEXT(" receiptInvalidation=") +
            (bInvalidated ? TEXT("complete") :
                TEXT("FAILED: ") + InvalidationError);
        return false;
    }
    if (!ValidateCleanEntryProcessBoundary(
            KnownAutoDirtyMaterialCount, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_IMPORT_REFUSED_CLEAN_CONSUMER: ") +
            Error;
        return false;
    }
    if (!ValidateProtectedPrewarmReceipt(
            Protected, true, true, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_IMPORT_REFUSED_PREWARM_RECEIPT: ") +
            Error;
        return false;
    }
    const auto RollBackFailure = [&Protected, &OutMessage](
        const FString& Stage,
        const FString& Failure) -> bool
    {
        FString RollbackError;
        FString ProtectionError;
        const bool bRolledBack = RollBackNewV4Namespace(RollbackError);
        const bool bProtected = ValidateProtectedPackages(
            Protected, ProtectionError);
        OutMessage = Stage + TEXT(": ") + Failure +
            TEXT(" rollback=") + (bRolledBack ? TEXT("complete") :
                TEXT("FAILED: ") + RollbackError) +
            TEXT(" protectedSizeSha=") + (bProtected ? TEXT("exact") :
                TEXT("FAILED: ") + ProtectionError);
        return false;
    };

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    TArray<UObject*> AssetsToSave;
    TArray<FString> SealedStagingObjectPaths;
    TArray<FProtectedFileDigest> SealedStagingPackageFiles;
    TMap<FString, UTexture2D*> Textures;
    TMap<FString, UStaticMesh*> RuntimeMeshes;
    TMap<FString, UMaterial*> Materials;
    UMaterialInstanceConstant* Lawn = nullptr;
    UStaticMesh* Portico = nullptr;
    if (!AssetSubsystem || !RuntimeMeshes.IsEmpty() ||
        !ImportExternalMeshDerivatives(
            AssetTools, RuntimeMeshes, AssetsToSave, Error) ||
        RuntimeMeshes.Num() != 7 ||
        !FinishAndValidateExactExternalMeshRoster(
            RuntimeMeshes, AssetsToSave, Error) ||
        !SealAndUnloadExactExternalStagingPackages(
            AssetSubsystem,
            AssetsToSave,
            SealedStagingObjectPaths,
            SealedStagingPackageFiles,
            Error) ||
        AssetsToSave.Num() != 7)
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_EXTERNAL_PHASE_ROLLED_BACK"), Error);
    }
    if (!ValidateCleanEntryProcessBoundary(
            KnownAutoDirtyMaterialCount, Error) ||
        !RevalidateProtectedPrewarmReceiptAfterExternalSeal(
            Protected, Error))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_POST_SEAL_PREWARM_RECEIPT_ROLLED_BACK"),
            Error);
    }
    if (!ImportRequiredTextures(
            AssetTools, TextureSpecs, Textures, AssetsToSave, Error) ||
        Textures.Num() != 41 ||
        FTextureCompilingManager::Get().GetNumRemainingTextures() != 0 ||
        !DuplicateCloseTurfAndBlocker(
            AssetTools, RuntimeMeshes, AssetsToSave, Error) ||
        RuntimeMeshes.Num() != 9)
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_IMPORT_ROLLED_BACK"), Error);
    }
    TArray<UStaticMesh*> CompileMeshes;
    for (const TPair<FString, UStaticMesh*>& Pair : RuntimeMeshes)
    {
        if (!IsProtectedRuntimeMeshReference(Pair.Value))
        {
            CompileMeshes.Add(Pair.Value);
        }
    }
    if (CompileMeshes.Contains(nullptr))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_IMPORT_ROLLED_BACK"),
            TEXT("The exact nine-owned-runtime compile roster is incomplete."));
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(CompileMeshes);
    FAssetCompilingManager::Get().FinishAllCompilation();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    FMemory::Trim(true);
    if (RuntimeMeshes.Num() != 9 ||
        !ValidateProtectedReferenceFactsWithoutLoading(true, Error))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_IMPORT_ROLLED_BACK"), Error);
    }
    if (!CreateAndBindAllMaterials(
            AssetTools,
            Textures,
            RuntimeMeshes,
            Materials,
            Lawn,
            AssetsToSave,
            Error) ||
        !ImportAndBindPortico(
            AssetTools,
            Materials,
            Portico,
            AssetsToSave,
            Error))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_IMPORT_ROLLED_BACK"), Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TSet<FString> LoadedObjectPaths;
    for (UObject* Asset : AssetsToSave)
    {
        if (!Asset || !Asset->GetPathName().StartsWith(AssetRoot + TEXT("/")) ||
            LoadedObjectPaths.Contains(Asset->GetPathName()))
        {
            Error = TEXT("The exact V4 save roster contains a null, duplicate or out-of-namespace object.");
            return RollBackFailure(
                TEXT("EXPLORE_V4_IMPORT_ROLLED_BACK"), Error);
        }
        LoadedObjectPaths.Add(Asset->GetPathName());
    }
    TSet<FString> CombinedOwnedObjectPaths = LoadedObjectPaths;
    for (const FString& SealedObjectPath : SealedStagingObjectPaths)
    {
        if (!SealedObjectPath.StartsWith(AssetRoot + TEXT("/")) ||
            CombinedOwnedObjectPaths.Contains(SealedObjectPath) ||
            FindObject<UObject>(nullptr, *SealedObjectPath))
        {
            Error = TEXT("The sealed staging ledger contains an invalid, duplicate or still-loaded object path.");
            return RollBackFailure(
                TEXT("EXPLORE_V4_IMPORT_ROLLED_BACK"), Error);
        }
        CombinedOwnedObjectPaths.Add(SealedObjectPath);
    }
    FString InMemoryValidation;
    FString SealedValidation;
    if (AssetsToSave.Num() != 78 || LoadedObjectPaths.Num() != 78 ||
        SealedStagingObjectPaths.Num() != 7 ||
        CombinedOwnedObjectPaths.Num() != 85 ||
        CountAssetsUnderV4Root() != 85 ||
        !ValidateProtectedReferenceFactsWithoutLoading(true, Error) ||
        !ValidateExactPackageFileDigests(
            SealedStagingPackageFiles,
            7,
            TEXT("Sealed V4 staging"),
            SealedValidation) ||
        !ValidateAllV4NonStagingAssetsInternal(InMemoryValidation) ||
        !ValidateProtectedReferenceFactsWithoutLoading(true, Error))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_IMPORT_ROLLED_BACK"),
            TEXT("Exact in-memory sealed7-plus-loaded78 owned-asset and two read-only-reference gate failed: ") +
                (!Error.IsEmpty()
                    ? Error
                    : (!SealedValidation.IsEmpty()
                        ? SealedValidation
                        : InMemoryValidation)));
    }
    TArray<FString> PersistedObjectPaths =
        CombinedOwnedObjectPaths.Array();
    PersistedObjectPaths.Sort();
    TSet<UPackage*> UniquePackagesToReload;
    TSet<FString> CombinedOwnedPackageNames;
    for (UObject* Asset : AssetsToSave)
    {
        if (!Asset || !Asset->GetOutermost() ||
            Asset->GetOutermost()->GetName() == DestinationMapPackage)
        {
            return RollBackFailure(
                TEXT("EXPLORE_V4_SAVE_ROSTER_INVALID_AND_ROLLED_BACK"),
                TEXT("The exact V4 asset save roster has an invalid package."));
        }
        UniquePackagesToReload.Add(Asset->GetOutermost());
        CombinedOwnedPackageNames.Add(Asset->GetOutermost()->GetName());
    }
    for (const FString& SealedObjectPath : SealedStagingObjectPaths)
    {
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(SealedObjectPath);
        if (PackageName.IsEmpty() ||
            PackageName == DestinationMapPackage ||
            CombinedOwnedPackageNames.Contains(PackageName))
        {
            return RollBackFailure(
                TEXT("EXPLORE_V4_SAVE_ROSTER_INVALID_AND_ROLLED_BACK"),
                TEXT("The sealed staging package ledger collided with a live or map package."));
        }
        CombinedOwnedPackageNames.Add(PackageName);
    }
    if (PersistedObjectPaths.Num() != 85 ||
        UniquePackagesToReload.Num() != 78 ||
        CombinedOwnedPackageNames.Num() != 85)
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_SAVE_ROSTER_INVALID_AND_ROLLED_BACK"),
            TEXT("The exact V4 persistence ledger must be seven sealed staging plus 78 loaded objects in 85 V4-only packages; protected V1/V3 mesh references may never enter it."));
    }
    if (!AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_SAVE_FAILED_AND_ROLLED_BACK"),
            TEXT("The remaining exact 78-package target-only save batch did not finish after the seven-package staging seal."));
    }
    for (UPackage* Package : UniquePackagesToReload)
    {
        if (!Package || Package->IsDirty())
        {
            return RollBackFailure(
                TEXT("EXPLORE_V4_SAVE_FAILED_AND_ROLLED_BACK"),
                TEXT("A remaining V4-only package stayed dirty after the 78-package save batch."));
        }
    }
    if (!ValidateExactPackageFileDigests(
            SealedStagingPackageFiles,
            7,
            TEXT("Sealed V4 staging"),
            Error))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_SAVE_FAILED_AND_ROLLED_BACK"), Error);
    }

    TArray<UPackage*> PackagesToReload = UniquePackagesToReload.Array();
    UniquePackagesToReload.Reset();
    AssetsToSave.Reset();
    Textures.Reset();
    RuntimeMeshes.Reset();
    Materials.Reset();
    CompileMeshes.Reset();
    Lawn = nullptr;
    Portico = nullptr;
    FText UnloadError;
    if (!UPackageTools::UnloadPackages(
            PackagesToReload, UnloadError, false))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_COLD_UNLOAD_FAILED_AND_ROLLED_BACK"),
            TEXT("Could not unload all 78 remaining saved V4-only packages: ") +
                UnloadError.ToString());
    }
    PackagesToReload.Reset();
    FMemory::Trim(true);
    for (const FString& ObjectPathToReload : PersistedObjectPaths)
    {
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(ObjectPathToReload);
        if (FindObject<UObject>(nullptr, *ObjectPathToReload) ||
            FindPackage(nullptr, *PackageName))
        {
            return RollBackFailure(
                TEXT("EXPLORE_V4_COLD_UNLOAD_FAILED_AND_ROLLED_BACK"),
                TEXT("A saved V4 asset/package remained loaded before cold reload: ") +
                    ObjectPathToReload);
        }
    }
    if (!ValidateProtectedReferenceFactsWithoutLoading(true, Error) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_COLD_UNLOAD_FAILED_AND_ROLLED_BACK"),
            TEXT("The exact two protected mesh references did not remain unloaded and hash-exact before streamed staging validation: ") +
                Error);
    }
    FString PersistedValidation;
    FString PersistedSealedValidation;
    Error.Reset();
    if (CountAssetsUnderV4Root() != 85 ||
        !ValidateExactPackageFileDigests(
            SealedStagingPackageFiles,
            7,
            TEXT("Sealed V4 staging"),
            PersistedSealedValidation) ||
        !ValidateAllV4AssetsInternal(false, PersistedValidation) ||
        !ValidateExactPackageFileDigests(
            SealedStagingPackageFiles,
            7,
            TEXT("Sealed V4 staging"),
            PersistedSealedValidation) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_PERSISTENCE_FAILED_AND_ROLLED_BACK"),
            !Error.IsEmpty()
                ? Error
                : (!PersistedSealedValidation.IsEmpty()
                    ? PersistedSealedValidation
                    : PersistedValidation));
    }
    if (HasDisallowedDirtyPackages(
            KnownAutoDirtyMaterialCount, Error))
    {
        return RollBackFailure(
            TEXT("EXPLORE_V4_POST_RELOAD_DIRTY_GATE_FAILED_AND_ROLLED_BACK"),
            Error);
    }
    FString ReceiptCleanupError;
    const bool bReceiptRemoved =
        RemoveProtectedPrewarmReceiptExact(ReceiptCleanupError);
    OutMessage = TEXT("Imported and validated the exact complete 85-asset Explore V4-owned namespace plus two protected read-only mesh references: 41 byte-frozen textures; seven editor-only raw staging meshes; nine V4-owned runtime vegetation/blocker meshes plus exact V1/V3 columnar/high-fork references with MinLOD1; 18 role-exact component-local wind materials; seven V8 portico materials; a static 41-node NormalDX lawn graph with paired rotated/offset 140 cm color/roughness/normal/AO detail, shared 32 m macro blend, explicit (-Y,X,Z) tangent reorientation/normalize and zero WPO; one derived lawn instance; and the exact identity 6,592-triangle/seven-slot NoCollision V8 portico. Protected V1-V5, Explore V1-V3, HDB and OSM packages remain byte-exact and are never saved by V4.");
    if (!bReceiptRemoved)
    {
        OutMessage += TEXT(" WARNING: the completed import could not remove its exact protected-prewarm receipt; the complete 85-asset destination still prevents reuse. ") +
            ReceiptCleanupError;
    }
    if (KnownAutoDirtyMaterialCount > 0)
    {
        OutMessage += FString::Printf(
            TEXT(" WARNING: %d exact protected V1 materials remain intentionally dirty in memory after UE auto-enabled HISM usage; do not use Save All. They were excluded from the V4-only save roster and their on-disk size/SHA stayed exact."),
            KnownAutoDirtyMaterialCount);
    }
    return true;
}

bool UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Assets(
    FString& OutReport)
{
    const int32 Existing = CountAssetsUnderV4Root();
    FString Error;
    if (Existing != 85 ||
        !ValidateAllV4AssetsInternal(true, Error))
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V4_ASSETS_INVALID: expected exact complete 85-asset additive owned namespace plus two protected read-only mesh references, found %d owned assets. %s"),
            Existing,
            *Error);
        return false;
    }
    OutReport = TEXT("Validated exact 85-asset Explore V4-owned namespace plus two protected read-only mesh references: frozen source hashes/import policy, topology and slot order, source-provenance LOD caps, MinLOD1/no raw-LOD0 runtime selection, component-only material overrides on protected meshes, 18 role-to-slot wind graphs, animated close-turf cards, static 41-node paired color/roughness/NormalDX/AO two-detail/32 m macro lawn, and exact NoCollision 6,592-triangle V8 portico are intact.");
    return true;
}

bool UTRIADIstanaExploreV4EditorLibrary::BuildIstanaExploreV4Map(
    FString& OutMessage)
{
    FString Error;
    int32 KnownAutoDirtyMaterialCount = 0;
    const TStrongObjectPtr<UCesiumIonServer> CesiumIonServerKeepAlive(
        UCesiumIonServer::GetServerForNewObjects());
    if (!CesiumIonServerKeepAlive.IsValid() ||
        CesiumIonServerKeepAlive->GetOutermost()->IsDirty())
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED_CESIUM_EDITOR_LIFETIME: the selected Cesium ion-server asset must resolve cleanly so build-time validation unloads and target rollback garbage collection cannot invalidate CesiumEditor callbacks.");
        return false;
    }
    if (HasDisallowedDirtyPackages(
            KnownAutoDirtyMaterialCount, Error) ||
        !ValidateCleanEntryProcessBoundary(
            KnownAutoDirtyMaterialCount, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED_ENTRY_BOUNDARY: launch this map consumer on the exact clean /Engine/Maps/Entry world. ") +
            Error;
        return false;
    }
    LogV4BuildPhaseMemory(TEXT("ENTRY_BASELINE"));
    FString AssetReport;
    if (!ValidateIstanaExploreV4Assets(AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED: import the exact complete V4 assets first. ") +
            AssetReport;
        return false;
    }
    LogV4BuildPhaseMemory(TEXT("ENTRY_ASSET_VALIDATION_COMPLETE"));
    FString ExistingFilename;
    if (FPackageName::DoesPackageExist(
            DestinationMapPackage, &ExistingFilename))
    {
        UWorld* ExistingWorld =
            UEditorLoadingAndSavingUtils::LoadMap(ExistingFilename);
        FString ExistingReport;
        if (ExistingWorld && ValidateV4World(ExistingWorld, ExistingReport))
        {
            if (HasDisallowedDirtyPackages(
                    KnownAutoDirtyMaterialCount, Error))
            {
                OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED_AFTER_IDEMPOTENT_VALIDATION: ") +
                    Error;
                return false;
            }
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V4_MAP_ALREADY_VALID: ") +
                ExistingReport;
            if (KnownAutoDirtyMaterialCount > 0)
            {
                OutMessage += TEXT(" WARNING: exact protected V1 HISM-usage materials remain intentionally dirty in memory; do not use Save All.");
            }
            return true;
        }
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED_PARTIAL_DESTINATION: the target map exists but is not the exact cold-valid V4 map; overwrite/repair is refused. ") +
            ExistingReport;
        return false;
    }

    if (!UnloadCleanV4ValidationPackagesAtEntry(Error))
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED_ENTRY_RELEASE: ") +
            Error;
        return false;
    }
    LogV4BuildPhaseMemory(TEXT("ENTRY_VALIDATION_PACKAGES_RELEASED"));

    FString SourceFilename;
    UWorld* SourceWorld = FPackageName::DoesPackageExist(
            SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    LogV4BuildPhaseMemory(TEXT("V3_SOURCE_LOADED"));
    FString V3Report;
    if (!SourceWorld || !SourceWorld->GetOutermost() ||
        SourceWorld->GetOutermost()->GetName() != SourceMapPackage)
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED: the exact validated Explore V3 source map could not be loaded after the Entry validation phase. ") +
            V3Report;
        return false;
    }
    const bool bV3MapValid = UTRIADIstanaExploreV3EditorLibrary::
        ValidateIstanaExploreV3Map(V3Report);
    UWorld* V2ValidationWorld = FindObject<UWorld>(
        nullptr, *V2ValidationMapObjectPath);
    UPackage* V2ValidationPackage = V2ValidationWorld
        ? V2ValidationWorld->GetOutermost()
        : FindPackage(nullptr, *V2ValidationMapPackage);
    UWorld* CurrentEditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    if (!V2ValidationWorld || !V2ValidationPackage ||
        V2ValidationPackage->GetName() != V2ValidationMapPackage ||
        V2ValidationWorld == SourceWorld ||
        V2ValidationWorld == CurrentEditorWorld)
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED_V2_VALIDATION_LIFETIME: public V3 validation did not leave exactly one clean inactive Explore V2 validation world available for balanced unload. v3Validation=") +
            V3Report;
        return false;
    }
    V2ValidationWorld = nullptr;
    if (!UnloadOneExactCleanPackage(
            V2ValidationMapObjectPath,
            V2ValidationPackage,
            false,
            TEXT("Build-time public V3 validation source"),
            Error))
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED_V2_VALIDATION_LIFETIME: ") +
            Error + TEXT(" v3Validation=") + V3Report;
        return false;
    }
    if (!bV3MapValid || !ValidateExactV3SourceWorld(SourceWorld, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED: open the exact validated Explore V3 source map. ") +
            V3Report + TEXT(" ") + Error;
        return false;
    }
    FV3WorldSnapshot SourceSnapshot;
    TArray<FProtectedFileDigest> Protected;
    if (!CaptureV3WorldSnapshot(SourceWorld, SourceSnapshot, Error) ||
        !CaptureProtectedPackages(Protected, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED: ") + Error;
        return false;
    }
    ATRIADIstanaExploreV2LandscapeActor* SourceV2 =
        FindV2Landscape(SourceWorld);
    ATRIADIstanaExploreV3SupplementActor* SourceV3 =
        FindV3Supplement(SourceWorld);
    ATRIADIstanaPublicViewSceneActor* SourceScene = FindScene(SourceWorld);
    ATRIADIstanaPublicViewRuntimePolicyActor* SourcePolicy =
        FindPolicy(SourceWorld);
    FTRIADIstanaExploreV4PopulationInput Population;
    FPlacementAudit PlacementAudit;
    if (!SourceV2 || !SourceV3 || !SourceScene || !SourcePolicy ||
        SourcePolicy->bEnforceFixedPrimaryCamera ||
        !SourceScene->TerrainComponent ||
        SourceScene->TerrainComponent->GetWorld() != SourceWorld ||
        !BuildV4PopulationInput(
            SourceV2,
            SourceV3,
            SourceScene,
            SourceScene->TerrainComponent,
            Population,
            PlacementAudit,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED_PREFLIGHT: exact registered V3 source actors/grid/terrain population handoff failed before the template transition. ") +
            Error;
        return false;
    }
    if (!ValidateExactV3SourceWorld(SourceWorld, Error) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED_PREFLIGHT: source/protected-package state changed during the source-only population preflight. ") +
            Error;
        return false;
    }
    LogV4BuildPhaseMemory(TEXT("V3_SNAPSHOT_AND_POPULATION_CAPTURED"));
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V4_BUILD_REFUSED: editor asset subsystem is unavailable before the sequential template transition.");
        return false;
    }
    const auto FailAndDeleteExactTarget =
        [&Protected, AssetSubsystem, &OutMessage](
            const FString& Stage,
            const FString& Failure) -> bool
    {
        FString CleanupError;
        FString ProtectionError;
        const bool bCleaned = DeleteExactV4TargetMapAfterFailure(
            AssetSubsystem, CleanupError);
        const bool bProtected = ValidateProtectedPackages(
            Protected, ProtectionError);
        OutMessage = Stage + TEXT(": ") + Failure +
            TEXT(" targetCleanup=") +
            (bCleaned ? TEXT("complete") :
                TEXT("FAILED: ") + CleanupError) +
            TEXT(" protectedSizeSha=") +
            (bProtected ? TEXT("exact") :
                TEXT("FAILED: ") + ProtectionError);
        return false;
    };

    SourceV2 = nullptr;
    SourceV3 = nullptr;
    SourceScene = nullptr;
    SourcePolicy = nullptr;
    SourceWorld = nullptr;
    if (!FEditorFileUtils::LoadMap(
            SourceFilename,
            true,
            false))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_TEMPLATE_LOAD_FAILED_AND_TARGET_DELETED"),
            TEXT("The exact V3 source could not be reloaded sequentially as an untitled template. ") +
                Error);
    }
    UWorld* TargetWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    const FString TemplatePackageName =
        TargetWorld && TargetWorld->GetOutermost()
            ? TargetWorld->GetOutermost()->GetName()
            : FString();
    if (!TargetWorld || !FPackageName::IsTempPackage(TemplatePackageName) ||
        FindPackage(nullptr, *SourceMapPackage) ||
        FindObject<UWorld>(nullptr, *SourceMapObjectPath) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath) ||
        !CesiumIonServerKeepAlive.IsValid() ||
        CesiumIonServerKeepAlive->GetOutermost()->IsDirty() ||
        !ValidateTemplateWorldBeforeMutation(
            TargetWorld,
            TemplatePackageName,
            SourceSnapshot,
            Error) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_TEMPLATE_STATE_FAILED_AND_TARGET_DELETED"),
            TEXT("The sequential untitled V3 template/source/protected state gate failed before V4 asset loading or mutation. ") +
                Error);
    }
    LogV4BuildPhaseMemory(TEXT("V3_TEMPLATE_LOADED_WITH_SOURCE_UNLOADED"));

    FTRIADIstanaExploreV4AssetRoster Roster;
    TArray<FTRIADIstanaExploreV4WindMaterialBinding> WindBindings;
    if (!BuildRuntimeAssetRosterAndWindBindings(
            Roster, WindBindings, Error))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_RUNTIME_ROSTER_FAILED_AND_TARGET_DELETED"),
            Error);
    }
    const TArray<UStaticMesh*> CompileMeshes = {
        Roster.UmbrellaTree,
        Roster.DomeTree,
        Roster.HighForkRoundedTree,
        Roster.ColumnarNarrowTree,
        Roster.PalmTree,
        Roster.HeritagePawnBlockerCylinder,
        Roster.Shrub,
        Roster.Flower,
        Roster.Understorey,
        Roster.GeometryGrass,
        Roster.CloseTurf,
        Roster.PorticoV8};
    if (CompileMeshes.Contains(nullptr) ||
        !FinishExactStaticMeshRosterMemoryBounded(
            CompileMeshes,
            TEXT("V4 template runtime roster"),
            Error) ||
        !ValidateTemplateWorldBeforeMutation(
            TargetWorld,
            TemplatePackageName,
            SourceSnapshot,
            Error) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_RUNTIME_COMPILE_FAILED_AND_TARGET_DELETED"),
            TEXT("The exact post-template V4 runtime roster did not reach bounded synchronous render readiness. ") +
                Error);
    }
    LogV4BuildPhaseMemory(TEXT("TEMPLATE_RUNTIME_ROSTER_READY"));

    ATRIADIstanaExploreV2LandscapeActor* V2 = FindV2Landscape(TargetWorld);
    ATRIADIstanaExploreV3SupplementActor* V3 = FindV3Supplement(TargetWorld);
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(TargetWorld);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(TargetWorld);
    if (!V2 || !V3 || !Scene || !Policy ||
        Policy->bEnforceFixedPrimaryCamera)
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_HANDOFF_FAILED_AND_TARGET_DELETED"),
            TEXT("Exact untitled-template target actors changed after the source-only population and post-template asset preflight. ") +
                Error);
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIADIstanaExploreLandscapeV4");
    SpawnParameters.OverrideLevel = TargetWorld->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        TargetWorld->SpawnActor<ATRIADIstanaExploreV4LandscapeActor>(
            ATRIADIstanaExploreV4LandscapeActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!V4)
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_SPAWN_FAILED_AND_TARGET_DELETED"),
            TEXT("Identity V4 landscape actor could not be spawned."));
    }
    V4->Tags.AddUnique(V4LandscapeTag);
    V4->Tags.AddUnique(PlacementPolicyTag);
    V4->Tags.AddUnique(FName(*PlacementAudit.BuildPersistedTag()));
    V4->SetActorLabel(TEXT("TRIAD Istana Explore V4 Public-Data Landscape Approximation (Not One-to-One / Not Sensor Truth)"));
    if (!V4->ConfigureRequiredAssets(Roster, WindBindings, Error) ||
        !V4->PopulateDeterministicLandscape(Population, Error) ||
        !ValidateV4HismTreesAfterPopulation(V4, Error) ||
        !ValidateExactReplacementBeforeHiding(V4, Population, Error))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_PREHIDE_FAILED_AND_TARGET_DELETED"),
            TEXT("V4 population/HISM/pre-hide replacement validation failed; no inherited visuals were hidden. ") +
                Error);
    }

    const TArray<UHierarchicalInstancedStaticMeshComponent*> V2Live =
        V2Components(V2);
    const TArray<UStaticMeshComponent*> V3Live = V3Components(V3);
    UMaterialInstanceConstant* Lawn =
        LoadExact<UMaterialInstanceConstant>(LawnMaterialInstanceObjectPath());
    if (V2Live.Num() != SourceSnapshot.V2Components.Num() ||
        V3Live.Num() != SourceSnapshot.V3Components.Num() ||
        !Lawn || !Scene->TerrainComponent ||
        Scene->TerrainComponent->GetNumMaterials() <= 0 ||
        V2Live[10]->GetInstanceCount() != ExpectedV2TreeUnionCount ||
        V3Live[8]->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_HIDE_PREREQUISITE_FAILED_AND_TARGET_DELETED"),
            TEXT("Final inherited hide/lawn prerequisites changed; no presentation delta was applied."));
    }
    for (int32 Index = 0; Index < 4; ++Index)
    {
        HideInheritedVisual(V2Live[Index]);
    }
    HideInheritedVisual(V3Live[1]);
    HideInheritedVisual(V3Live[6]);
    HideInheritedVisual(V3Live[8]);
    Scene->TerrainComponent->Modify();
    Scene->TerrainComponent->SetMaterial(0, Lawn);

    if (!ValidateTargetInheritance(
            TargetWorld,
            SourceSnapshot,
            LawnMaterialInstanceObjectPath(),
            Error) ||
        !V4->RecordExternalTargetMapState(
            ExpectedV2TreeUnionCount,
            true,
            true,
            true,
            true,
            true,
            true,
            Error))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_POSTHIDE_FAILED_AND_TARGET_DELETED"),
            TEXT("Exact post-hide terrain/HDB/OSM/blocker preservation or external-state record failed. ") +
                Error);
    }
    LogV4BuildPhaseMemory(TEXT("TEMPLATE_POPULATION_AND_MUTATION_COMPLETE"));
    TargetWorld->MarkPackageDirty();
    FString BeforeSave;
    if (!ValidateV4WorldAgainstPrecomputedPopulation(
            TargetWorld,
            TemplatePackageName,
            SourceSnapshot,
            Population,
            PlacementAudit,
            false,
            BeforeSave) ||
        !UEditorLoadingAndSavingUtils::SaveMap(
            TargetWorld,
            DestinationMapPackage) ||
        !TargetWorld->GetOutermost() ||
        TargetWorld->GetOutermost()->GetName() != DestinationMapPackage ||
        FindPackage(nullptr, *SourceMapPackage) ||
        FindObject<UWorld>(nullptr, *SourceMapObjectPath) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_SAVE_FAILED_AND_TARGET_DELETED"),
            !Error.IsEmpty() ? Error : BeforeSave);
    }
    LogV4BuildPhaseMemory(TEXT("TEMPLATE_RENAMED_AND_SAVED_AS_V4"));
    FString SourceFilenameForColdReload;
    UWorld* ColdSourceWorld = FPackageName::DoesPackageExist(
            SourceMapPackage, &SourceFilenameForColdReload)
        ? UEditorLoadingAndSavingUtils::LoadMap(
            SourceFilenameForColdReload)
        : nullptr;
    if (!ColdSourceWorld || !ColdSourceWorld->GetOutermost() ||
        ColdSourceWorld->GetOutermost()->GetName() != SourceMapPackage ||
        !ValidateExactV3SourceWorld(ColdSourceWorld, Error) ||
        !ValidateProtectedPackages(Protected, Error) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath) ||
        FindPackage(nullptr, *DestinationMapPackage))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_COLD_UNLOAD_FAILED_AND_TARGET_DELETED"),
            TEXT("Saved target could not be unloaded by switching to the exact V3 source map."));
    }
    LogV4BuildPhaseMemory(TEXT("SAVED_V4_UNLOADED_AT_EXACT_V3"));
    FString DestinationFilename;
    UWorld* Reloaded = FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)
        : nullptr;
    LogV4BuildPhaseMemory(TEXT("SAVED_V4_COLD_RELOADED"));
    FString Persisted;
    if (!Reloaded || FindPackage(nullptr, *SourceMapPackage) ||
        FindObject<UWorld>(nullptr, *SourceMapObjectPath) ||
        !ValidateV4WorldAgainstPrecomputedPopulation(
            Reloaded,
            DestinationMapPackage,
            SourceSnapshot,
            Population,
            PlacementAudit,
            true,
            Persisted) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_COLD_RELOAD_FAILED_AND_TARGET_DELETED"),
            !Error.IsEmpty() ? Error : Persisted);
    }
    LogV4BuildPhaseMemory(TEXT("SAVED_V4_FULL_COLD_VALIDATION_COMPLETE"));
    if (HasDisallowedDirtyPackages(
            KnownAutoDirtyMaterialCount, Error))
    {
        return FailAndDeleteExactTarget(
            TEXT("EXPLORE_V4_POST_RELOAD_DIRTY_GATE_FAILED_AND_TARGET_DELETED"),
            Error);
    }
    OutMessage = TEXT("Created and cold-reload-validated /Game/Maps/Istana_PublicView_Explore_v4. It preserves the exact Explore V3 terrain/collision, HDB/OSM context, free-roam/manual-exposure setup, supplementary non-replaced vegetation and all 720 V2 Pawn blockers. It replaces exactly all 720 inherited tree translations and full quaternion rotations (including the unchanged full FTransforms for source indices 272..503), nine published Heritage silhouettes plus separate girth-sized Pawn blockers, and all 18,432 V3 close-turf transforms; adds deterministic paired formal-bed coverage and raster-guided understorey/taller grass; uses a static anti-tiling V4 lawn surface distinct from animated close-turf cards; and renders the identity NoCollision V8 portico only after hiding V7. ") +
        Persisted;
    if (KnownAutoDirtyMaterialCount > 0)
    {
        OutMessage += TEXT(" WARNING: exact protected V1 HISM-usage materials remain intentionally dirty in memory; do not use Save All. They were never part of the target-only map save and their on-disk size/SHA stayed exact.");
    }
    return true;
}

bool UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Map(
    FString& OutReport)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    return ValidateV4World(World, OutReport);
}

bool UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4PlayWorld(
    FString& OutReport)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    FString Error;
    if (!GetValidatedV4PlayState(
            PlayWorld, Player, Pawn, V4, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V4_PIE_INVALID: ") + Error;
        return false;
    }
    OutReport = TEXT("Explore V4 PIE is live: Player0 owns and views the expected free-roam/manual-exposure pawn; inherited supplementary wind plus 18-role V4 instance-local wind/gust/after-sway are active; the lawn surface remains static while close-turf cards and grass clumps move.");
    return true;
}

bool UTRIADIstanaExploreV4EditorLibrary::CaptureIstanaExploreV4PlayView(
    const FString& OutputFileName,
    FString& OutMessage)
{
    if (!OutputFileName.StartsWith(
            TEXT("explore_v4_"), ESearchCase::IgnoreCase) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName)
    {
        OutMessage = TEXT("Explore V4 PIE captures require one clean 'explore_v4_*.png' filename.");
        return false;
    }
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    FString Validation;
    if (!GetLightweightAcceptedV4PlayState(
            PlayWorld, Player, Pawn, V4, true, Validation))
    {
        OutMessage = TEXT("Explore V4 capture requires the validated live free-roam world. ") +
            Validation;
        return false;
    }
    UGameViewportClient* ViewportClient = PlayWorld
        ? PlayWorld->GetGameViewport()
        : nullptr;
    FSceneViewport* Viewport = ViewportClient
        ? ViewportClient->GetGameViewport()
        : nullptr;
    if (!Viewport)
    {
        OutMessage = TEXT("The validated Explore V4 Player0 viewport is unavailable.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("TRIAD/IstanaPreviews/ExploreV4"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    {
        OutMessage = TEXT("Could not create the Explore V4 QA capture directory.");
        return false;
    }
    const FString Destination = FPaths::Combine(
        Directory, OutputFileName);
    if (IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested())
    {
        OutMessage = TEXT("Explore V4 capture refused an overwrite or overlapping screenshot request.");
        return false;
    }
    FScreenshotRequest::RequestScreenshot(
        Destination, false, false, false);
    Viewport->Draw(false);
    const int64 WrittenBytes = IFileManager::Get().FileSize(*Destination);
    if (WrittenBytes > 0)
    {
        OutMessage = FString::Printf(
            TEXT("Wrote a positive-byte Explore V4 Player0 screenshot (%lld bytes) to '%s'. A nonwhite pixel-histogram check remains mandatory external visual acceptance."),
            WrittenBytes,
            *Destination);
    }
    else
    {
        OutMessage = TEXT("Queued asynchronous Explore V4 Player0 screenshot to '") +
            Destination +
            TEXT("'. This is not a capture-success claim: acceptance requires the file to appear with positive bytes and pass an external nonwhite pixel-histogram check.");
    }
    return true;
}

bool UTRIADIstanaExploreV4EditorLibrary::
    TriggerIstanaExploreV4PlayWindGust(
        float PeakStrengthCm,
        FString& OutMessage)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    FString Error;
    if (!FMath::IsFinite(PeakStrengthCm) || PeakStrengthCm < 6.0f ||
        PeakStrengthCm > 120.0f ||
        !GetLightweightAcceptedV4PlayState(
            PlayWorld, Player, Pawn, V4, true, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_QA_GUST_REFUSED: require exact V4 PIE and a finite 6-120 cm peak. ") +
            Error;
        return false;
    }
    V4->TriggerWindGust(PeakStrengthCm);
    OutMessage = FString::Printf(
        TEXT("Triggered deterministic bounded V4 QA gust peak %.3f cm; %s"),
        PeakStrengthCm,
        *V4->BuildWindRuntimeStateReport());
    return true;
}

bool UTRIADIstanaExploreV4EditorLibrary::MoveIstanaExploreV4PlayPawnForQa(
    FVector DeltaCentimeters,
    FString& OutMessage)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    FString Error;
    if (DeltaCentimeters.ContainsNaN() ||
        DeltaCentimeters.Size() < 1.0 || DeltaCentimeters.Size() > 5000.0 ||
        !GetLightweightAcceptedV4PlayState(
            PlayWorld, Player, Pawn, V4, true, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_QA_MOVE_REFUSED: require exact V4 PIE and a finite 1-5000 cm delta. ") +
            Error;
        return false;
    }
    const FVector Start = Pawn->GetActorLocation();
    const FVector Requested = Start + DeltaCentimeters;
    if (FVector2D(Requested.X, Requested.Y).Size() > 95000.0 ||
        Requested.Z < 50.0 || Requested.Z > 20000.0)
    {
        OutMessage = TEXT("EXPLORE_V4_QA_MOVE_REFUSED: requested pose is outside the bounded 950 m / 0.5-200 m QA envelope.");
        return false;
    }
    FHitResult Hit;
    const bool bMoved = Pawn->SetActorLocation(
        Requested, true, &Hit, ETeleportType::None);
    const FVector End = Pawn->GetActorLocation();
    if (!bMoved || End.Equals(Start, 0.1f))
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V4_QA_MOVE_BLOCKED: sweep hit '%s'; pawn remains at %s."),
            *GetNameSafe(Hit.GetActor()),
            *End.ToString());
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Moved validated Explore V4 Player0 pawn by sweep from %s to %s (requested %s); viewTargetMatchesPawn=%s."),
        *Start.ToString(),
        *End.ToString(),
        *Requested.ToString(),
        Player->GetViewTarget() == Pawn ? TEXT("true") : TEXT("false"));
    return Player->GetViewTarget() == Pawn;
}

bool UTRIADIstanaExploreV4EditorLibrary::
    TeleportIstanaExploreV4PlayPawnForQa(
        FVector WorldLocationCentimeters,
        FRotator WorldRotationDegrees,
        FString& OutMessage)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    FString Error;
    if (WorldLocationCentimeters.ContainsNaN() ||
        WorldRotationDegrees.ContainsNaN() ||
        FVector2D(
            WorldLocationCentimeters.X,
            WorldLocationCentimeters.Y).Size() > 95000.0 ||
        WorldLocationCentimeters.Z < 50.0 ||
        WorldLocationCentimeters.Z > 20000.0 ||
        !GetLightweightAcceptedV4PlayState(
            PlayWorld, Player, Pawn, V4, true, Error))
    {
        OutMessage = TEXT("EXPLORE_V4_QA_TELEPORT_REFUSED: require exact V4 PIE and a finite pose inside the bounded 950 m / 0.5-200 m envelope. ") +
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
        OutMessage = TEXT("EXPLORE_V4_QA_TELEPORT_FAILED: exact bounded pawn pose/view-target readback failed.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Teleported validated Explore V4 Player0 pawn to %s at %s; viewTargetMatchesPawn=true."),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString());
    return true;
}

bool UTRIADIstanaExploreV4EditorLibrary::
    GetIstanaExploreV4PlayStateReport(FString& OutReport)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    FString Error;
    if (!GetLightweightAcceptedV4PlayState(
            PlayWorld, Player, Pawn, V4, true, Error))
    {
        OutReport = TEXT("EXPLORE_V4_QA_STATE_INVALID: ") + Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("map=%s pawn=%s pawnLocationCm=%s pawnRotationDeg=%s viewTarget=%s viewTargetMatchesPawn=true staticLawnWpo=false animatedCloseTurf=true %s"),
        *UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName()),
        *Pawn->GetName(),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString(),
        *GetNameSafe(Player->GetViewTarget()),
        *V4->BuildWindRuntimeStateReport());
    return true;
}

bool UTRIADIstanaExploreV4EditorLibrary::
    QuiesceIstanaExploreV4PlayWorldForStop(FString& OutMessage)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    FString Readiness;
    if (!GetLightweightAcceptedV4PlayState(
            PlayWorld, Player, Pawn, V4, true, Readiness))
    {
        OutMessage = TEXT("Refusing scripted Explore V4 PIE stop because exact runtime identity failed. ") +
            Readiness;
        return false;
    }
    OutMessage = TEXT("Explore V4 PIE identity, Player0 free-roam/manual-exposure possession, inherited wind and V4 instance-local gust/after-sway runtime are exact; no fragile UEDPIE object path or console command is required, so scripted stop may proceed.");
    return true;
}
