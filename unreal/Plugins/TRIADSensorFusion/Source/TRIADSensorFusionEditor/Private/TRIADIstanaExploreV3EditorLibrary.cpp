#include "TRIADIstanaExploreV3EditorLibrary.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetCompilingManager.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
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
#include "Factories/TextureFactory.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInterface.h"
#include "MeshReductionSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Serialization/Archive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/SceneViewport.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV2EditorLibrary.h"
#include "TRIADIstanaExploreV2LandscapeActor.h"
#include "TRIADIstanaExploreV3SupplementActor.h"
#include "TRIADIstanaFreeRoamPawn.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "TRIADIstanaPublicViewRuntimePolicyActor.h"
#include "UObject/Package.h"
#include "UnrealClient.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace
{
const FString SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v2"));
const FString DestinationMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v3"));
const FString DestinationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v3.Istana_PublicView_Explore_v3"));
const FString SourceMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v2.Istana_PublicView_Explore_v2"));
const FString ExploreGameModeClassPath(
    TEXT("/Script/TRIADSensorFusion.TRIADIstanaExploreGameMode"));
const FString AssetRoot(TEXT("/Game/TRIAD/IstanaPublicViewExploreV3"));
const FString TextureAssetPath(AssetRoot + TEXT("/Textures"));
const FString VegetationAssetPath(AssetRoot + TEXT("/Vegetation"));
const FString PorticoAssetPath(AssetRoot + TEXT("/Portico"));
const FString ContextAssetPath(AssetRoot + TEXT("/Context"));
const FString MaterialAssetPath(AssetRoot + TEXT("/Materials"));

const FString IslandTreeName(TEXT("SM_IPVExploreV3_IslandTree01"));
const FString ShrubName(TEXT("SM_IPVExploreV3_Shrub02"));
const FString FernName(TEXT("SM_IPVExploreV3_Fern02"));
const FString MossName(TEXT("SM_IPVExploreV3_Moss01"));
const FString BermudaName(TEXT("SM_IPVExploreV3_BermudaGrass"));
const FString TurfCardName(TEXT("SM_IPVExploreV3_CloseTurfCards"));
const FString PorticoName(
    TEXT("SM_IstanaPublicViewV7_CentralPorticoRefinement_V5Live"));
const FString TerrainName(TEXT("SM_IPVExploreV3_TerrainLowFrequencyPrior"));
const FString OsmRoadsName(TEXT("SM_IPVExploreV3_OsmPublicRoads"));
const FString UraRoadsName(TEXT("SM_IPVExploreV3_UraIndicativeRoads"));
const FString OsmWaterName(TEXT("SM_IPVExploreV3_OsmWater"));

FString ObjectPath(const FString& PackagePath, const FString& AssetName)
{
    return PackagePath + TEXT("/") + AssetName + TEXT(".") + AssetName;
}

const FString IslandTreeObjectPath = ObjectPath(VegetationAssetPath, IslandTreeName);
const FString ShrubObjectPath = ObjectPath(VegetationAssetPath, ShrubName);
const FString FernObjectPath = ObjectPath(VegetationAssetPath, FernName);
const FString MossObjectPath = ObjectPath(VegetationAssetPath, MossName);
const FString BermudaObjectPath = ObjectPath(VegetationAssetPath, BermudaName);
const FString TurfCardObjectPath = ObjectPath(VegetationAssetPath, TurfCardName);
const FString PorticoObjectPath = ObjectPath(PorticoAssetPath, PorticoName);
const FString TerrainObjectPath = ObjectPath(ContextAssetPath, TerrainName);
const FString OsmRoadsObjectPath = ObjectPath(ContextAssetPath, OsmRoadsName);
const FString UraRoadsObjectPath = ObjectPath(ContextAssetPath, UraRoadsName);
const FString OsmWaterObjectPath = ObjectPath(ContextAssetPath, OsmWaterName);
const FString V2BroadleafObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_Broadleaf_A.SM_IstanaPublicViewExploreV1_Broadleaf_A"));
const FString V2TrunkWindMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafTrunk_Wind.M_IPVExploreV2_BroadleafTrunk_Wind"));
const FString V2BranchWindMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafBranches_Wind.M_IPVExploreV2_BroadleafBranches_Wind"));
const FString V1LeafDiffuseTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves_diff_1k.jacaranda_tree_leaves_diff_1k"));
const FString V1LeafNormalTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves_nor_gl_1k.jacaranda_tree_leaves_nor_gl_1k"));
const FString V1LeafRoughnessTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves_rough_1k.jacaranda_tree_leaves_rough_1k"));
const FString V1LeafOpacityTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves_alpha_1k.jacaranda_tree_leaves_alpha_1k"));
const FString DarkColumnarMaterialName(
    TEXT("M_IPVExploreV3_ColumnarLeavesDark_Wind"));
const FString DarkColumnarMaterialPath = ObjectPath(
    MaterialAssetPath, DarkColumnarMaterialName);
const FString FormalLawnMaterialPath = ObjectPath(
    MaterialAssetPath, TEXT("M_IPVExploreV3_FormalLawn"));
const FLinearColor DarkColumnarTint(0.26f, 0.43f, 0.22f, 1.0f);

const FString FrozenSourceManifestSha(
    TEXT("CA26023F45BAA1344ED0DC031766355D734EC9B89C8B25F089ADFA18FEA12290"));
const FString IslandLockSha(
    TEXT("79F7C4938B85580D7B958CCA4649BC98EE33D89188C7EBDBA15CF3AFCCB49735"));
const FString AdditionalLockSha(
    TEXT("B3470E974E0FA9D32D6A839D227E35B088B161EB0BFE3EAFC09DCE88AD21E766"));
const FString PreparedManifestSha(
    TEXT("46CA095EC6B200DD2C3E7A0BB89C377968FEF17C7A4C8EF07C0681366BCBE82D"));
const FString ContextManifestSha(
    TEXT("FBC1032E60591AA8CEA0C1B3E16BDB9E9776C84692533CABC11D8EB7A79D6A56"));
const FString ContextAcceptanceLockSha(
    TEXT("7A9A78F4E0E1AA6FF2A9FC9E7623B4E27C23DC57C706B8D5EFBB464A4D17BCAD"));
const FString PorticoBindingSha(
    TEXT("5955C9194B5A342D11054996BD759D54212A95FBEF8042F5D5111CF2ADC8AC46"));
const FString PorticoManifestSha(
    TEXT("10EE96E2A2D59F510102F9854A5079A2E51D6E79C6A795C524608AC86799685A"));
const FString PorticoObjSha(
    TEXT("164624CABCF0B093A7D53F87F0A3ACA5B009A001ABCA9CBD3950ECFC6336FFFD"));
const FString PorticoMtlSha(
    TEXT("F58223E1AB2676FE7EEC10D4C07BC3CB75D5FADDBC215D7B7F7F50EA2B9479A7"));

const FName V2LandscapeTag(TEXT("TRIADIstanaExploreLandscapeV2"));
const FName V3SupplementTag(TEXT("TRIADIstanaExploreSupplementV3"));
const FString WindCustomDescription(
    TEXT("TRIAD_EXPLORE_V3_INSTANCE_LOCAL_PIVOT_UNDERDAMPED_WPO_V1"));
const FString InheritedV2WindCustomDescription(
    TEXT("TRIAD_EXPLORE_V2_DIRECTIONAL_GUST_WPO_V1"));
constexpr float IslandImportedUpAxisSpanCm = 502.744640f;
constexpr float ShrubImportedUpAxisSpanCm = 138.346186f;
constexpr float FernImportedUpAxisSpanCm = 42.769471f;
constexpr float MossImportedUpAxisSpanCm = 4.036798f;
constexpr float BermudaImportedUpAxisSpanCm = 15.825569f;
const FString WindCustomCode(
    TEXT("float InstanceRandom = GetPerInstanceRandom(Parameters);\n")
    TEXT("float localHeight = max(InstanceLocalPosition.z, 0.0);\n")
    TEXT("float h = saturate(localHeight / max(HeightCm, 1.0));\n")
    TEXT("float phase = TimeSeconds * WindSpeed * 6.28318530718 + InstanceRandom * 6.28318530718 + dot(WorldPosition.xy, float2(0.0017, 0.0023));\n")
    TEXT("float wave = sin(phase) + 0.31 * sin(phase * 1.73 + 1.2);\n")
    TEXT("float2 direction = normalize(WindDirection.xy + float2(0.0001, 0.0));\n")
    TEXT("float bend = clamp(WindStrengthCm * ResponseScale * wave * h * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("float lift = clamp(abs(WindStrengthCm) * ResponseScale * 0.025 * sin(phase * 0.71) * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("return float3(direction * bend, lift);"));

struct FProtectedPackageBytes
{
    FString PackageName;
    FString Filename;
    TArray<uint8> Bytes;
};

struct FTextureSourceSpec
{
    FString SourcePath;
    FString AssetName;
    int64 Bytes = 0;
    FString Sha256;
    bool bSrgb = false;
    TextureCompressionSettings Compression = TC_Default;
    TextureGroup Group = TEXTUREGROUP_World;
    bool bFlipGreen = false;
};

struct FMaterialSpec
{
    FString AssetName;
    FString BaseColorTexture;
    FString NormalTexture;
    FString RoughnessTexture;
    FString OpacityTexture;
    FString SubsurfaceTexture;
    bool bMasked = false;
    bool bTwoSidedFoliage = false;
    float ResponseScale = 0.5f;
    float HeightCm = 100.0f;
    float MaximumWpoCm = 20.0f;
};

struct FDarkColumnarSourceTextures
{
    UTexture2D* Diffuse = nullptr;
    UTexture2D* Normal = nullptr;
    UTexture2D* Roughness = nullptr;
    UTexture2D* Opacity = nullptr;
};

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
    bool bGenerateOverlapEvents = true;
    TArray<FTransform> InstanceWorldTransforms;
};

struct FV2WorldSnapshot
{
    FTransform LandscapeTransform = FTransform::Identity;
    TArray<FComponentState> LandscapeComponents;
    FComponentState Terrain;
    FComponentState DistantContext;
    FString GameModeClassPath;
    float BaseWindStrengthCm = 0.0f;
    float GustPeakStrengthCm = 0.0f;
    float WindSpeed = 0.0f;
    FVector2D PrevailingWindDirection = FVector2D::ZeroVector;
    float RecoveryFrequencyHz = 0.0f;
    float RecoveryDampingRatio = 0.0f;
    TArray<FString> WindMaterialPaths;
    TArray<int32> WindMaterialSlots;
    bool bWindMaterialsConfigured = false;
};

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

// TRIAD_EXPLORE_V3_SHA256_HOST_BEGIN
namespace TriadExploreV3Sha256
{
using FDigest = std::array<std::uint8_t, 32>;
using FLowerHexDigest = std::array<char, 65>;

class FStreamingSha256 final
{
public:
    FStreamingSha256()
        : State_{
              0x6a09e667u,
              0xbb67ae85u,
              0x3c6ef372u,
              0xa54ff53au,
              0x510e527fu,
              0x9b05688cu,
              0x1f83d9abu,
              0x5be0cd19u}
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
            const std::size_t CopyBytes = Length < Available
                ? Length
                : Available;
            std::memcpy(
                Buffer_.data() + BufferedBytes_,
                Data,
                CopyBytes);
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
        FStreamingSha256 Copy(*this);
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
                RotateRight(E, 6u) ^ RotateRight(E, 11u) ^
                RotateRight(E, 25u);
            const std::uint32_t Choice = (E & F) ^ ((~E) & G);
            const std::uint32_t Temporary1 =
                H + Sum1 + Choice + Constants[Index] + Words[Index];
            const std::uint32_t Sum0 =
                RotateRight(A, 2u) ^ RotateRight(A, 13u) ^
                RotateRight(A, 22u);
            const std::uint32_t Majority =
                (A & B) ^ (A & C) ^ (B & C);
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
        for (std::size_t WordIndex = 0u; WordIndex < State_.size(); ++WordIndex)
        {
            const std::uint32_t Word = State_[WordIndex];
            const std::size_t Offset = WordIndex * 4u;
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

bool HasDigest(
    const FStreamingSha256& Hasher,
    const char* ExpectedLowerHex)
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
    FStreamingSha256 Empty;
    if (!Empty.Update(nullptr, 0u) ||
        !HasDigest(
            Empty,
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855"))
    {
        return false;
    }

    FStreamingSha256 Abc;
    const char* AbcText = "abc";
    if (!Abc.Update(
            reinterpret_cast<const std::uint8_t*>(AbcText), 1u) ||
        !Abc.Update(
            reinterpret_cast<const std::uint8_t*>(AbcText + 1), 2u) ||
        !HasDigest(
            Abc,
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad"))
    {
        return false;
    }

    const char* MultiBlockText =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    FStreamingSha256 MultiBlock;
    if (!MultiBlock.Update(
            reinterpret_cast<const std::uint8_t*>(MultiBlockText), 13u) ||
        !MultiBlock.Update(
            reinterpret_cast<const std::uint8_t*>(MultiBlockText + 13),
            43u) ||
        !HasDigest(
            MultiBlock,
            "248d6a61d20638b8e5c026930c3e6039"
            "a33ce45964ff2167f6ecedd419db06c1"))
    {
        return false;
    }
    std::array<std::uint8_t, 1024> LargeChunk{};
    LargeChunk.fill(static_cast<std::uint8_t>('a'));
    FStreamingSha256 GreaterThanOneMiB;
    for (std::size_t ChunkIndex = 0u; ChunkIndex < 1024u; ++ChunkIndex)
    {
        if (!GreaterThanOneMiB.Update(LargeChunk.data(), LargeChunk.size()))
        {
            return false;
        }
    }
    std::array<std::uint8_t, 17> LargeTail{};
    LargeTail.fill(static_cast<std::uint8_t>('a'));
    if (!GreaterThanOneMiB.Update(LargeTail.data(), LargeTail.size()) ||
        !HasDigest(
            GreaterThanOneMiB,
            "c26032d5154f96bd29c799447d715ab6"
            "81d8d0aa308ecc6f321a35d98f0672da"))
    {
        return false;
    }
    return true;
}
} // namespace TriadExploreV3Sha256
// TRIAD_EXPLORE_V3_SHA256_HOST_END

bool Sha256ImplementationIsValid()
{
    static const bool bKnownVectorsValid =
        TriadExploreV3Sha256::VerifyKnownVectors();
    return bKnownVectorsValid;
}

bool FinishSha256(
    const TriadExploreV3Sha256::FStreamingSha256& Hasher,
    FString& OutDigest)
{
    TriadExploreV3Sha256::FDigest Digest{};
    if (!Hasher.Finalize(Digest))
    {
        return false;
    }
    const TriadExploreV3Sha256::FLowerHexDigest Hex =
        TriadExploreV3Sha256::ToLowerHex(Digest);
    FString Candidate = UTF8_TO_TCHAR(Hex.data());
    Candidate.ToUpperInline();
    if (Candidate.Len() != 64)
    {
        return false;
    }
    OutDigest = MoveTemp(Candidate);
    return true;
}

bool HashFileSha256(const FString& Filename, FString& OutSha256, int64& OutBytes)
{
    OutSha256.Reset();
    OutBytes = -1;
    if (!Sha256ImplementationIsValid())
    {
        return false;
    }

    TUniquePtr<FArchive> Reader(
        IFileManager::Get().CreateFileReader(*Filename, FILEREAD_Silent));
    if (!Reader)
    {
        return false;
    }
    const int64 TotalBytes = Reader->TotalSize();
    if (TotalBytes < 0)
    {
        return false;
    }

    constexpr int64 ReadChunkBytes = 1024 * 1024;
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(static_cast<int32>(ReadChunkBytes));
    TriadExploreV3Sha256::FStreamingSha256 Hasher;
    int64 RemainingBytes = TotalBytes;
    while (RemainingBytes > 0)
    {
        const int64 ThisChunk = FMath::Min(RemainingBytes, ReadChunkBytes);
        Reader->Serialize(Buffer.GetData(), ThisChunk);
        if (Reader->IsError() ||
            !Hasher.Update(
                Buffer.GetData(),
                static_cast<std::size_t>(ThisChunk)))
        {
            return false;
        }
        RemainingBytes -= ThisChunk;
    }

    FString Digest;
    if (!FinishSha256(Hasher, Digest))
    {
        return false;
    }
    OutSha256 = MoveTemp(Digest);
    OutBytes = TotalBytes;
    return true;
}
bool ValidateExactFile(
    const FString& Filename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutError)
{
    FString ActualSha;
    int64 ActualBytes = -1;
    if (!HashFileSha256(Filename, ActualSha, ActualBytes) ||
        ActualBytes != ExpectedBytes || ActualSha != ExpectedSha256.ToUpper())
    {
        OutError = FString::Printf(
            TEXT("Frozen source file is absent or changed: %s"), *Filename);
        return false;
    }
    OutError.Reset();
    return true;
}

bool IsImageExtension(const FString& Path)
{
    const FString Extension = FPaths::GetExtension(Path).ToLower();
    return Extension == TEXT("png") || Extension == TEXT("jpg") ||
        Extension == TEXT("jpeg") || Extension == TEXT("exr");
}

FTextureSourceSpec MakeTextureSpec(
    const FString& AbsolutePath,
    const FString& RelativePath,
    int64 Bytes,
    const FString& Sha256)
{
    FTextureSourceSpec Spec;
    Spec.SourcePath = AbsolutePath;
    Spec.AssetName = FPaths::GetBaseFilename(RelativePath);
    Spec.Bytes = Bytes;
    Spec.Sha256 = Sha256.ToUpper();
    const FString Lower = Spec.AssetName.ToLower();
    Spec.bSrgb = Lower.Contains(TEXT("diff")) ||
        Lower.EndsWith(TEXT("_color"));
    if (Lower.Contains(TEXT("nor_gl")) || Lower.Contains(TEXT("normalgl")))
    {
        Spec.Compression = TC_Normalmap;
        Spec.Group = TEXTUREGROUP_WorldNormalMap;
        Spec.bFlipGreen = true;
        Spec.bSrgb = false;
    }
    else if (Lower.Contains(TEXT("rough")) ||
             Lower.Contains(TEXT("alpha")) ||
             Lower.Contains(TEXT("mask")) ||
             Lower.Contains(TEXT("opacity")) ||
             Lower.Contains(TEXT("ambientocclusion")) ||
             Lower.Contains(TEXT("displacement")) ||
             Lower.Contains(TEXT("scattering")))
    {
        Spec.Compression = TC_Masks;
        Spec.Group = TEXTUREGROUP_WorldSpecular;
        Spec.bSrgb = false;
    }
    return Spec;
}

bool AddLockedFileRoster(
    const FString& LockPath,
    const FString& ExpectedLockSha,
    const FString& RelativeFileRoot,
    int32 ExpectedAssetCount,
    TArray<FTextureSourceSpec>& InOutTextures,
    FString& OutError)
{
    int64 LockBytes = -1;
    FString LockSha;
    if (!HashFileSha256(LockPath, LockSha, LockBytes) ||
        LockSha != ExpectedLockSha)
    {
        OutError = TEXT("A frozen Poly Haven lock file changed: ") + LockPath;
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    if (!LoadJsonObject(LockPath, Root) ||
        Root->GetStringField(TEXT("schema")) != TEXT("triad.cc0_asset_lock.v1") ||
        Root->GetStringField(TEXT("provider")) != TEXT("Poly Haven") ||
        Root->GetStringField(TEXT("licence")) != TEXT("CC0-1.0"))
    {
        OutError = TEXT("A frozen Poly Haven lock has invalid identity/licence fields.");
        return false;
    }
    TArray<TSharedPtr<FJsonValue>> FileRows;
    if (Root->HasTypedField<EJson::Array>(TEXT("files")))
    {
        FileRows = Root->GetArrayField(TEXT("files"));
        if (ExpectedAssetCount != 1)
        {
            OutError = TEXT("Single-asset lock was used with a non-single expected count.");
            return false;
        }
    }
    else
    {
        const TArray<TSharedPtr<FJsonValue>>& Assets =
            Root->GetArrayField(TEXT("assets"));
        if (Assets.Num() != ExpectedAssetCount)
        {
            OutError = TEXT("Additional Poly Haven lock asset count changed.");
            return false;
        }
        for (const TSharedPtr<FJsonValue>& AssetValue : Assets)
        {
            FileRows.Append(AssetValue->AsObject()->GetArrayField(TEXT("files")));
        }
    }

    const FString NormalizedRoot = FPaths::ConvertRelativePathToFull(RelativeFileRoot);
    TSet<FString> UniqueRelativePaths;
    for (const TSharedPtr<FJsonValue>& Value : FileRows)
    {
        const TSharedPtr<FJsonObject> Row = Value->AsObject();
        const FString Relative = Row->GetStringField(TEXT("path"));
        const int64 Bytes = static_cast<int64>(Row->GetNumberField(TEXT("bytes")));
        const FString Sha = Row->GetStringField(TEXT("sha256")).ToUpper();
        FString Absolute = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(NormalizedRoot, Relative));
        FPaths::NormalizeFilename(Absolute);
        FString RootPrefix = NormalizedRoot;
        FPaths::NormalizeFilename(RootPrefix);
        if (UniqueRelativePaths.Contains(Relative) ||
            !(Absolute + TEXT("/")).StartsWith(RootPrefix + TEXT("/")) ||
            !ValidateExactFile(Absolute, Bytes, Sha, OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A lock path is duplicate or escaped its frozen root.");
            }
            return false;
        }
        UniqueRelativePaths.Add(Relative);
        if (IsImageExtension(Relative))
        {
            InOutTextures.Add(MakeTextureSpec(Absolute, Relative, Bytes, Sha));
        }
    }
    OutError.Reset();
    return true;
}

bool AddManifestFileRoster(
    const FString& ManifestPath,
    int64 ExpectedManifestBytes,
    const FString& ExpectedManifestSha,
    const FString& RelativeFileRoot,
    const TCHAR* ArrayField,
    TArray<FTextureSourceSpec>* InOutTextures,
    FString& OutError)
{
    if (!ValidateExactFile(
            ManifestPath, ExpectedManifestBytes, ExpectedManifestSha, OutError))
    {
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    if (!LoadJsonObject(ManifestPath, Root))
    {
        OutError = TEXT("Frozen prepared/generated manifest is invalid JSON.");
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>& Files =
        Root->GetArrayField(ArrayField);
    const FString NormalizedRoot = FPaths::ConvertRelativePathToFull(RelativeFileRoot);
    TSet<FString> Unique;
    for (const TSharedPtr<FJsonValue>& Value : Files)
    {
        const TSharedPtr<FJsonObject> Row = Value->AsObject();
        const FString Relative = Row->GetStringField(TEXT("path"));
        const int64 Bytes = static_cast<int64>(Row->GetNumberField(TEXT("bytes")));
        const FString Sha = Row->GetStringField(TEXT("sha256")).ToUpper();
        FString Absolute = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(NormalizedRoot, Relative));
        FPaths::NormalizeFilename(Absolute);
        FString RootPrefix = NormalizedRoot;
        FPaths::NormalizeFilename(RootPrefix);
        if (Unique.Contains(Relative) ||
            !(Absolute + TEXT("/")).StartsWith(RootPrefix + TEXT("/")) ||
            !ValidateExactFile(Absolute, Bytes, Sha, OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A prepared/generated manifest file is duplicate or escaped its root.");
            }
            return false;
        }
        Unique.Add(Relative);
        if (InOutTextures && IsImageExtension(Relative))
        {
            InOutTextures->Add(MakeTextureSpec(Absolute, Relative, Bytes, Sha));
        }
    }
    OutError.Reset();
    return true;
}

const TArray<FMaterialSpec>& VegetationMaterialSpecs()
{
    static const TArray<FMaterialSpec> Specs = {
        {TEXT("M_IPVExploreV3_IslandTree01_Trunk_Wind"),
         TEXT("island_tree_01_diff_1k"),
         TEXT("island_tree_01_nor_gl_1k"),
         TEXT("island_tree_01_rough_1k"), TEXT(""), TEXT(""),
         false, false, 0.10f, IslandImportedUpAxisSpanCm, 12.0f},
        {TEXT("M_IPVExploreV3_IslandTree01_Branches_Wind"),
         TEXT("island_tree_01_branches_diff_1k"),
         TEXT("island_tree_01_branches_nor_gl_1k"),
         TEXT("island_tree_01_branches_rough_1k"), TEXT(""), TEXT(""),
         false, false, 0.38f, IslandImportedUpAxisSpanCm, 35.0f},
        {TEXT("M_IPVExploreV3_IslandTree01_Leaves_Wind"),
         TEXT("island_tree_01_leaves_diff_1k"),
         TEXT("island_tree_01_leaves_nor_gl_1k"),
         TEXT("island_tree_01_leaves_rough_1k"),
         TEXT("island_tree_01_leaves_alpha_1k"),
         TEXT("island_tree_01_leaves_diff_1k"),
         true, true, 1.00f, IslandImportedUpAxisSpanCm, 70.0f},
        {TEXT("M_IPVExploreV3_Shrub02_Wind"),
         TEXT("shrub_02_diff_1k"), TEXT("shrub_02_nor_gl_1k"),
         TEXT("shrub_02_rough_1k"), TEXT("shrub_02_alpha_1k"),
         TEXT("shrub_02_diff_1k"), true, true, 0.82f, ShrubImportedUpAxisSpanCm, 28.0f},
        {TEXT("M_IPVExploreV3_Fern02_Wind"),
         TEXT("fern_02_diff_1k"), TEXT("fern_02_nor_gl_1k"),
         TEXT("fern_02_rough_1k"), TEXT("fern_02_alpha_1k"),
         TEXT("fern_02_diff_1k"), true, true, 0.92f, FernImportedUpAxisSpanCm, 18.0f},
        {TEXT("M_IPVExploreV3_Moss01_Wind"),
         TEXT("moss_01_diff_1k"), TEXT("moss_01_nor_gl_1k"),
         TEXT("moss_01_rough_1k"), TEXT("moss_01_alpha_1k"),
         TEXT("moss_01_diff_1k"), true, true, 0.24f, MossImportedUpAxisSpanCm, 0.8f},
        {TEXT("M_IPVExploreV3_BermudaGrass_Wind"),
         TEXT("grass_bermuda_01_diff_1k"),
         TEXT("grass_bermuda_01_nor_gl_1k"),
         TEXT("grass_bermuda_01_rough_1k"), TEXT(""), TEXT(""),
         false, false, 0.34f, BermudaImportedUpAxisSpanCm, 1.2f},
        {TEXT("M_IPVExploreV3_CloseTurf_Wind"),
         TEXT("Foliage008_Color"), TEXT("Foliage008_NormalGL"),
         TEXT("Foliage008_Roughness"), TEXT("Foliage008_Opacity"),
         TEXT("Foliage008_Scattering"),
         true, true, 0.72f, 6.0f, 7.0f}};
    return Specs;
}

FMaterialSpec DarkColumnarMaterialSpec()
{
    FMaterialSpec Spec;
    Spec.AssetName = DarkColumnarMaterialName;
    Spec.BaseColorTexture = TEXT("V1JacarandaLeafDiffuse");
    Spec.NormalTexture = TEXT("V1JacarandaLeafNormal");
    Spec.RoughnessTexture = TEXT("V1JacarandaLeafRoughness");
    Spec.OpacityTexture = TEXT("V1JacarandaLeafOpacity");
    Spec.SubsurfaceTexture = TEXT("V1JacarandaLeafSubsurface");
    Spec.bMasked = true;
    Spec.bTwoSidedFoliage = true;
    Spec.ResponseScale = 1.0f;
    Spec.HeightCm = 2400.0f;
    Spec.MaximumWpoCm = 70.0f;
    return Spec;
}

bool MatchesExactLeafSourceTexture(
    const UTexture2D* Texture,
    const FString& ExactPath,
    const TCHAR* RelativeSourcePath,
    int64 ExpectedSourceBytes,
    const TCHAR* ExpectedSourceMd5,
    bool bExpectedSrgb,
    TextureCompressionSettings ExpectedCompression,
    TextureGroup ExpectedGroup,
    bool bExpectedGreenFlip)
{
    FString ExpectedSource = ProjectSourcePath(RelativeSourcePath);
    ExpectedSource = FPaths::ConvertRelativePathToFull(ExpectedSource);
    FPaths::NormalizeFilename(ExpectedSource);
    TArray<FString> ImportedFilenames = Texture && Texture->AssetImportData
        ? Texture->AssetImportData->ExtractFilenames()
        : TArray<FString>();
    FString ImportedSource = ImportedFilenames.Num() == 1
        ? FPaths::ConvertRelativePathToFull(ImportedFilenames[0])
        : FString();
    FPaths::NormalizeFilename(ImportedSource);
    const FMD5Hash CurrentHash = FMD5Hash::HashFile(*ExpectedSource);
    const FMD5Hash* ImportedHash = Texture && Texture->AssetImportData &&
        Texture->AssetImportData->GetSourceData().SourceFiles.Num() == 1
        ? &Texture->AssetImportData->GetSourceData().SourceFiles[0].FileHash
        : nullptr;
    return Texture && Texture->GetPathName() == ExactPath &&
        IFileManager::Get().FileSize(*ExpectedSource) == ExpectedSourceBytes &&
        CurrentHash.IsValid() &&
        LexToString(CurrentHash).Equals(ExpectedSourceMd5, ESearchCase::IgnoreCase) &&
        ImportedHash && ImportedHash->IsValid() && CurrentHash == *ImportedHash &&
        ImportedFilenames.Num() == 1 &&
        FPaths::IsSamePath(ImportedSource, ExpectedSource) &&
        Texture->Source.GetSizeX() == 1024 &&
        Texture->Source.GetSizeY() == 1024 &&
        Texture->SRGB == bExpectedSrgb &&
        Texture->CompressionSettings == ExpectedCompression &&
        Texture->LODGroup == ExpectedGroup &&
        Texture->MipGenSettings == TMGS_FromTextureGroup &&
        Texture->AddressX == TA_Wrap && Texture->AddressY == TA_Wrap &&
        Texture->bFlipGreenChannel == bExpectedGreenFlip &&
        !Texture->VirtualTextureStreaming &&
        !Texture->GetOutermost()->IsDirty();
}

bool ValidateDarkColumnarSourceTextures(
    FDarkColumnarSourceTextures* OutTextures,
    FString& OutError)
{
    UTexture2D* Diffuse = LoadExact<UTexture2D>(V1LeafDiffuseTexturePath);
    UTexture2D* Normal = LoadExact<UTexture2D>(V1LeafNormalTexturePath);
    UTexture2D* Roughness = LoadExact<UTexture2D>(V1LeafRoughnessTexturePath);
    UTexture2D* Opacity = LoadExact<UTexture2D>(V1LeafOpacityTexturePath);
    if (!MatchesExactLeafSourceTexture(
            Diffuse, V1LeafDiffuseTexturePath,
            TEXT("IstanaPublicViewExploreV1/Sources/PolyHaven/jacaranda_tree_1k/textures/jacaranda_tree_leaves_diff_1k.png"),
            2364526, TEXT("879EF43ACB3443E247A2D32713FEC86C"),
            true, TC_Default,
            TEXTUREGROUP_World, false) ||
        !MatchesExactLeafSourceTexture(
            Normal, V1LeafNormalTexturePath,
            TEXT("IstanaPublicViewExploreV1/Sources/PolyHaven/jacaranda_tree_1k/textures/jacaranda_tree_leaves_nor_gl_1k.png"),
            4154090, TEXT("C968C79960303470E124875A111BD5A4"),
            false, TC_Normalmap,
            TEXTUREGROUP_WorldNormalMap, true) ||
        !MatchesExactLeafSourceTexture(
            Roughness, V1LeafRoughnessTexturePath,
            TEXT("IstanaPublicViewExploreV1/Sources/PolyHaven/jacaranda_tree_1k/textures/jacaranda_tree_leaves_rough_1k.png"),
            992522, TEXT("4CBFFD1596F7AD63DE8E170AC54FB907"),
            false, TC_Masks,
            TEXTUREGROUP_WorldSpecular, false) ||
        !MatchesExactLeafSourceTexture(
            Opacity, V1LeafOpacityTexturePath,
            TEXT("IstanaPublicViewExploreV1/Sources/PolyHaven/jacaranda_tree_1k/textures/jacaranda_tree_leaves_alpha_1k.png"),
            506043, TEXT("740214DE7B900E30737009FF4EE2C01A"),
            false, TC_Masks,
            TEXTUREGROUP_WorldSpecular, false))
    {
        OutError = TEXT("The exact locked V1 jacaranda leaf textures or their sRGB/linear/OpenGL-normal import policy changed.");
        return false;
    }
    if (OutTextures)
    {
        OutTextures->Diffuse = Diffuse;
        OutTextures->Normal = Normal;
        OutTextures->Roughness = Roughness;
        OutTextures->Opacity = Opacity;
    }
    OutError.Reset();
    return true;
}

FString TextureObjectPath(const FString& AssetName)
{
    return ObjectPath(TextureAssetPath, AssetName);
}

FString MaterialObjectPath(const FString& AssetName)
{
    return ObjectPath(MaterialAssetPath, AssetName);
}

bool ValidatePreparedMaterialPlan(
    const FString& PreparedRoot,
    FString& OutError)
{
    const FString PlanPath = FPaths::Combine(
        PreparedRoot, TEXT("ExploreV3.material-plan.json"));
    TSharedPtr<FJsonObject> Plan;
    if (!ValidateExactFile(
            PlanPath,
            2685,
            TEXT("A5413E3CB2E8F81EDB710DBB249E0BC4CE12CFFCC61B790141701982910AC3F1"),
            OutError) ||
        !LoadJsonObject(PlanPath, Plan) ||
        Plan->GetStringField(TEXT("schema")) !=
            TEXT("triad.istana_explore_v3.material_plan.v1"))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Prepared V3 material plan identity changed.");
        }
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>& Materials =
        Plan->GetArrayField(TEXT("materials"));
    if (Materials.Num() != 2)
    {
        OutError = TEXT("Prepared material plan must contain exact lawn and close-turf rows.");
        return false;
    }
    bool bLawnValidated = false;
    bool bTurfValidated = false;
    for (const TSharedPtr<FJsonValue>& Value : Materials)
    {
        const TSharedPtr<FJsonObject> Row = Value->AsObject();
        const FString Name = Row->GetStringField(TEXT("assetName"));
        const TSharedPtr<FJsonObject> Policies =
            Row->GetObjectField(TEXT("texturePolicies"));
        const TSharedPtr<FJsonObject> Base =
            Policies->GetObjectField(TEXT("baseColor"));
        const TSharedPtr<FJsonObject> Normal =
            Policies->GetObjectField(TEXT("normalOpenGL"));
        const TSharedPtr<FJsonObject> Rough =
            Policies->GetObjectField(TEXT("roughness"));
        const bool bCommon = Base->GetBoolField(TEXT("sRgb")) &&
            !Normal->GetBoolField(TEXT("sRgb")) &&
            Normal->GetStringField(TEXT("compression")) == TEXT("NORMALMAP") &&
            Normal->GetBoolField(TEXT("unrealGreenChannelFlipRequired")) &&
            !Rough->GetBoolField(TEXT("sRgb"));
        if (Name == TEXT("M_IPVExploreV3_FormalLawn"))
        {
            bLawnValidated = bCommon &&
                !Policies->GetObjectField(TEXT("ambientOcclusion"))->GetBoolField(TEXT("sRgb")) &&
                !Policies->GetObjectField(TEXT("height"))->GetBoolField(TEXT("sRgb"));
        }
        else if (Name == TEXT("M_IPVExploreV3_CloseTurf_Wind"))
        {
            const TSharedPtr<FJsonObject> Wind = Row->GetObjectField(TEXT("wind"));
            bTurfValidated = bCommon &&
                !Policies->GetObjectField(TEXT("opacity"))->GetBoolField(TEXT("sRgb")) &&
                Policies->GetObjectField(TEXT("opacity"))->GetStringField(TEXT("compression")) == TEXT("MASK") &&
                !Policies->GetObjectField(TEXT("subsurface"))->GetBoolField(TEXT("sRgb")) &&
                Wind->GetBoolField(TEXT("instanceLocalPivotRequired")) &&
                Wind->GetBoolField(TEXT("afterSwayRequired")) &&
                FMath::IsNearlyEqual(
                    Wind->GetNumberField(TEXT("maximumWorldPositionOffsetCentimetres")),
                    7.0);
        }
    }
    if (!bLawnValidated || !bTurfValidated)
    {
        OutError = TEXT("Prepared V3 linear/sRGB/mask/OpenGL-normal green-flip or pivot-safe 7 cm wind policy changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool BuildFrozenTextureSpecs(
    TArray<FTextureSourceSpec>& OutTextures,
    FString& OutError)
{
    OutTextures.Reset();
    const FString V3Root = ProjectSourcePath(TEXT("IstanaPublicViewExploreV3"));
    const FString SourceRoot = FPaths::Combine(V3Root, TEXT("Sources"));
    const FString Manifest = FPaths::Combine(SourceRoot, TEXT("source-manifest.json"));
    if (!ValidateExactFile(
            Manifest, 36624, FrozenSourceManifestSha, OutError) ||
        !ValidatePreparedMaterialPlan(
            FPaths::Combine(V3Root, TEXT("Prepared")), OutError) ||
        !AddLockedFileRoster(
            FPaths::Combine(
                SourceRoot,
                TEXT("polyhaven-assets/island_tree_01/island_tree_01_1k.lock.json")),
            IslandLockSha,
            FPaths::Combine(SourceRoot, TEXT("polyhaven-assets/island_tree_01")),
            1,
            OutTextures,
            OutError) ||
        !AddLockedFileRoster(
            FPaths::Combine(
                SourceRoot,
                TEXT("polyhaven-assets/polyhaven_additional_1k.lock.json")),
            AdditionalLockSha,
            FPaths::Combine(SourceRoot, TEXT("polyhaven-assets")),
            4,
            OutTextures,
            OutError) ||
        !AddManifestFileRoster(
            FPaths::Combine(
                V3Root,
                TEXT("Prepared/ExploreV3.prepared-render-assets.manifest.json")),
            4584,
            PreparedManifestSha,
            FPaths::Combine(V3Root, TEXT("Prepared")),
            TEXT("files"),
            &OutTextures,
            OutError))
    {
        return false;
    }

    TSet<FString> UniqueNames;
    for (const FTextureSourceSpec& Spec : OutTextures)
    {
        if (UniqueNames.Contains(Spec.AssetName) ||
            !ValidateExactFile(
                Spec.SourcePath, Spec.Bytes, Spec.Sha256, OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("The V3 texture source roster has a duplicate name.");
            }
            return false;
        }
        UniqueNames.Add(Spec.AssetName);
    }
    if (OutTextures.Num() != 36 || UniqueNames.Num() != 36)
    {
        OutError = FString::Printf(
            TEXT("Frozen V3 texture roster changed: expected 36, found %d."),
            OutTextures.Num());
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateFrozenContext(FString& OutError)
{
    const FString V3Root = ProjectSourcePath(TEXT("IstanaPublicViewExploreV3"));
    const FString GeneratedRoot =
        FPaths::Combine(V3Root, TEXT("Generated/GeospatialContext"));
    const FString AcceptanceLock =
        FPaths::Combine(V3Root, TEXT("geospatial_context.acceptance.lock.json"));
    if (!AddManifestFileRoster(
            AcceptanceLock,
            4334,
            ContextAcceptanceLockSha,
            GeneratedRoot,
            TEXT("acceptedOutputs"),
            nullptr,
            OutError))
    {
        return false;
    }
    TSharedPtr<FJsonObject> Manifest;
    const FString ManifestPath =
        FPaths::Combine(GeneratedRoot, TEXT("geospatial_context.manifest.json"));
    if (!ValidateExactFile(
            ManifestPath, 7830, ContextManifestSha, OutError) ||
        !LoadJsonObject(ManifestPath, Manifest) ||
        Manifest->GetStringField(TEXT("schema")) !=
            TEXT("triad.istana_explore_v3_geospatial_context_manifest.v1") ||
        Manifest->GetStringField(TEXT("status")) !=
            TEXT("SOURCE_ONLY_VISUAL_PROTOTYPE_NOT_SURVEY_OR_SENSOR_TRUTH") ||
        Manifest->GetObjectField(TEXT("scope"))->GetBoolField(
            TEXT("collisionProduced")) ||
        Manifest->GetObjectField(TEXT("scope"))->GetBoolField(
            TEXT("sensorOcclusionProduced")) ||
        Manifest->GetObjectField(TEXT("claimBoundary"))->GetBoolField(
            TEXT("googleOrOneMapPixelsUsed")) ||
        Manifest->GetObjectField(TEXT("sanitization"))->GetBoolField(
            TEXT("securityMetadataInOutputs")) ||
        Manifest->GetObjectField(TEXT("sanitization"))->GetBoolField(
            TEXT("barrierFenceGateGeometryInOutputs")))
    {
        OutError = TEXT("Generated context truth/sanitization contract changed.");
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>& Attribution =
        Manifest->GetArrayField(TEXT("requiredAttribution"));
    const TArray<FString> ExpectedAttribution = {
        TEXT("© OpenStreetMap contributors; https://www.openstreetmap.org/copyright"),
        TEXT("Contains information from Heritage Trees and Master Plan 2019 Road Graphic accessed on 25 August 2026 from data.gov.sg under the Singapore Open Data Licence 1.0."),
        TEXT("Terrain derivative produced using Copernicus WorldDEM-30 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights reserved."),
        TEXT("The organisations in charge of the Copernicus programme by law or by delegation do not incur any liability for any use of the Copernicus WorldDEM-30.")};
    if (Attribution.Num() != ExpectedAttribution.Num())
    {
        OutError = TEXT("Generated context attribution roster count changed.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedAttribution.Num(); ++Index)
    {
        if (Attribution[Index]->AsString() != ExpectedAttribution[Index])
        {
            OutError = TEXT("Generated context exact provider attribution changed.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateFrozenPortico(FString& OutError)
{
    const FString V7Root = ProjectSourcePath(TEXT("IstanaPublicViewV7Portico"));
    const FString BindingPath =
        FPaths::Combine(V7Root, TEXT("v5_live_receiver_binding_v7.json"));
    const FString GeneratedRoot = FPaths::Combine(V7Root, TEXT("GeneratedV5Live"));
    const FString ManifestPath = FPaths::Combine(
        GeneratedRoot,
        TEXT("IstanaPublicViewV7CentralPorticoRefinementV5Live.manifest.json"));
    if (!ValidateExactFile(BindingPath, 4613, PorticoBindingSha, OutError) ||
        !AddManifestFileRoster(
            ManifestPath,
            7599,
            PorticoManifestSha,
            GeneratedRoot,
            TEXT("files"),
            nullptr,
            OutError))
    {
        return false;
    }
    TSharedPtr<FJsonObject> Manifest;
    if (!LoadJsonObject(ManifestPath, Manifest))
    {
        OutError = TEXT("V5-native portico manifest is invalid JSON.");
        return false;
    }
    const TSharedPtr<FJsonObject> Integration =
        Manifest->GetObjectField(TEXT("integration"));
    const TSharedPtr<FJsonObject> Mesh = Manifest->GetObjectField(TEXT("mesh"));
    const TSharedPtr<FJsonObject> Claims =
        Manifest->GetObjectField(TEXT("claimBoundary"));
    const TArray<TSharedPtr<FJsonValue>>& Location =
        Integration->GetArrayField(TEXT("requiredRelativeLocationCentimetres"));
    const TArray<TSharedPtr<FJsonValue>>& Rotation =
        Integration->GetArrayField(TEXT("requiredRelativeRotationDegrees"));
    const TArray<TSharedPtr<FJsonValue>>& Scale =
        Integration->GetArrayField(TEXT("requiredRelativeScale3D"));
    const auto IsTriple = [](const TArray<TSharedPtr<FJsonValue>>& Values,
                             double Expected)
    {
        return Values.Num() == 3 &&
            FMath::IsNearlyEqual(Values[0]->AsNumber(), Expected) &&
            FMath::IsNearlyEqual(Values[1]->AsNumber(), Expected) &&
            FMath::IsNearlyEqual(Values[2]->AsNumber(), Expected);
    };
    const FString ObjPath = FPaths::Combine(
        GeneratedRoot,
        TEXT("SM_IstanaPublicViewV7_CentralPorticoRefinement_V5Live.obj"));
    const FString MtlPath = FPaths::Combine(
        GeneratedRoot,
        TEXT("SM_IstanaPublicViewV7_CentralPorticoRefinement_V5Live.mtl"));
    if (Mesh->GetIntegerField(TEXT("triangleCount")) != 5816 ||
        !IsTriple(Location, 0.0) || !IsTriple(Rotation, 0.0) ||
        !IsTriple(Scale, 1.0) ||
        Integration->GetBoolField(TEXT("collisionEnabled")) ||
        Integration->GetBoolField(TEXT("liveInstallAuthorized")) ||
        !Integration->GetBoolField(TEXT("addAsSiblingOfFrozenV5Hero")) ||
        Integration->GetBoolField(TEXT("replaceFrozenV5Hero")) ||
        Claims->GetBoolField(TEXT("collisionAuthority")) ||
        Claims->GetBoolField(TEXT("liveInstallAuthorized")) ||
        Claims->GetBoolField(TEXT("oneToOneClaimed")) ||
        !ValidateExactFile(ObjPath, 1220068, PorticoObjSha, OutError) ||
        !ValidateExactFile(MtlPath, 1372, PorticoMtlSha, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5-native portico identity/render-only contract changed.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateUe55MeshImportCensus(
    const TSharedPtr<FJsonObject>& IntegrationContract,
    FString& OutError);

bool ValidatePorticoMaterialTopologyContract(
    const TSharedPtr<FJsonObject>& IntegrationContract,
    FString& OutError);

bool ValidateVegetationWindPivotBoundsContract(
    const TSharedPtr<FJsonObject>& IntegrationContract,
    FString& OutError);

bool ValidateFrozenSourceSet(
    TArray<FTextureSourceSpec>& OutTextures,
    FString& OutError)
{
    const FString ContractPath = ProjectSourcePath(
        TEXT("IstanaPublicViewExploreV3/explore_v3.contract.json"));
    const FString IntegrationContractPath = ProjectSourcePath(
        TEXT("IstanaPublicViewExploreV3Unreal/explore_v3_unreal_integration.contract.json"));
    TSharedPtr<FJsonObject> Contract;
    TSharedPtr<FJsonObject> IntegrationContract;
    if (!BuildFrozenTextureSpecs(OutTextures, OutError) ||
        !ValidateFrozenContext(OutError) ||
        !ValidateFrozenPortico(OutError) ||
        !ValidateExactFile(
            ContractPath,
            1771,
            TEXT("B0F96EEEB90824FBA7594EC3185A2EE2094290D13C3F9497E85FAA085BCB8CC2"),
            OutError) ||
        !LoadJsonObject(ContractPath, Contract) ||
        !ValidateExactFile(
            IntegrationContractPath,
            11892,
            TEXT("8F287CFF182A121D62CF617AD4E3BA758092D00F3F07A6D0C342007793216250"),
            OutError) ||
        !LoadJsonObject(IntegrationContractPath, IntegrationContract) ||
        Contract->GetStringField(TEXT("sourceMap")) != SourceMapPackage ||
        Contract->GetStringField(TEXT("destinationMap")) != DestinationMapPackage ||
        IntegrationContract->GetStringField(TEXT("sourceMap")) != SourceMapPackage ||
        IntegrationContract->GetStringField(TEXT("destinationMap")) != DestinationMapPackage ||
        IntegrationContract->GetStringField(TEXT("destinationAssetNamespace")) != AssetRoot ||
        !ValidateUe55MeshImportCensus(IntegrationContract, OutError) ||
        !ValidatePorticoMaterialTopologyContract(IntegrationContract, OutError) ||
        !ValidateVegetationWindPivotBoundsContract(
            IntegrationContract, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Explore V3 source contract/map chain changed.");
        }
        return false;
    }
    const TSharedPtr<FJsonObject> PorticoAuthorization =
        IntegrationContract->GetObjectField(TEXT("porticoAuthorization"));
    const TSharedPtr<FJsonObject> ContextPolicy =
        IntegrationContract->GetObjectField(TEXT("generatedContext"));
    const TSharedPtr<FJsonObject> LawnPolicy =
        IntegrationContract->GetObjectField(TEXT("lawnPresentationUpgrade"));
    const TSharedPtr<FJsonObject> IntegrationClaims =
        IntegrationContract->GetObjectField(TEXT("claimBoundary"));
    if (!PorticoAuthorization->GetBoolField(
            TEXT("developmentExploreV3PrototypeImportAuthorized")) ||
        PorticoAuthorization->GetBoolField(TEXT("productionPromotionAuthorized")) ||
        PorticoAuthorization->GetBoolField(TEXT("replaceV5")) ||
        PorticoAuthorization->GetBoolField(TEXT("collisionAuthority")) ||
        PorticoAuthorization->GetBoolField(TEXT("sensorAuthority")) ||
        ContextPolicy->GetStringField(TEXT("defaultVisibility")) !=
            TEXT("HIDDEN_DIAGNOSTIC") ||
        ContextPolicy->GetBoolField(TEXT("publicDistributionReady")) ||
        !LawnPolicy->GetBoolField(TEXT("targetMapOnly")) ||
        !LawnPolicy->GetBoolField(
            TEXT("inheritedTerrainMeshTransformAndQueryPhysicsCollisionPreserved")) ||
        LawnPolicy->GetIntegerField(TEXT("preparedCloseTurfCardInstances")) != 18432 ||
        LawnPolicy->GetIntegerField(TEXT("trianglesPerCard")) != 8 ||
        LawnPolicy->GetIntegerField(TEXT("totalSourceTriangles")) != 147456 ||
        LawnPolicy->GetIntegerField(TEXT("endCullCentimeters")) != 10000 ||
        !LawnPolicy->GetBoolField(TEXT("centralCeremonialAxisAllowsLowTurf")) ||
        !LawnPolicy->GetBoolField(
            TEXT("buildingHardscapeFountainExclusionsRequired")) ||
        IntegrationClaims->GetBoolField(TEXT("hyperrealClaimed")) ||
        IntegrationClaims->GetBoolField(TEXT("productionReady")))
    {
        OutError = TEXT("Explore V3 development-only portico/context/claim authorization boundary changed.");
        return false;
    }
    const TSharedPtr<FJsonObject> Claims =
        Contract->GetObjectField(TEXT("claimBoundary"));
    if (Claims->GetBoolField(TEXT("oneToOneOneKilometerClaimed")) ||
        Claims->GetBoolField(TEXT("sensorTruthAuthority")) ||
        Claims->GetBoolField(TEXT("surveyAccurateTerrainClaimed")) ||
        Claims->GetBoolField(TEXT("googleOrOneMapPixelsUsed")))
    {
        OutError = TEXT("Explore V3 source contract promoted an unsupported claim.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool HasDirtyPackages(FString& OutError)
{
    TArray<UPackage*> DirtyMaps;
    TArray<UPackage*> DirtyContent;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContent);
    if (!DirtyMaps.IsEmpty() || !DirtyContent.IsEmpty())
    {
        OutError = TEXT("Close or save every dirty map/content package before V3 import/build.");
        return true;
    }
    OutError.Reset();
    return false;
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
        SourceMapPackage};
    return Packages;
}

const TArray<FString>& ProtectedV2AssetPackages()
{
    static const TArray<FString> Packages = {
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_Broadleaf_A"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves_diff_1k"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves_nor_gl_1k"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves_rough_1k"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/jacaranda_tree_leaves_alpha_1k"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafTrunk_Wind"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafBranches_Wind"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafLeaves_Wind")};
    return Packages;
}

bool CaptureProtectedPackages(
    TArray<FProtectedPackageBytes>& OutRecords,
    FString& OutError)
{
    OutRecords.Reset();
    for (const FString& PackageName : ProtectedMapPackages())
    {
        FProtectedPackageBytes Record;
        Record.PackageName = PackageName;
        if (!FPackageName::DoesPackageExist(PackageName, &Record.Filename) ||
            !FFileHelper::LoadFileToArray(Record.Bytes, *Record.Filename) ||
            Record.Bytes.IsEmpty())
        {
            OutError = TEXT("Could not snapshot protected V1-V6/Explore V1-V2 map: ") +
                PackageName;
            return false;
        }
        OutRecords.Add(MoveTemp(Record));
    }
    for (const FString& PackageName : ProtectedV2AssetPackages())
    {
        FProtectedPackageBytes Record;
        Record.PackageName = PackageName;
        if (!FPackageName::DoesPackageExist(PackageName, &Record.Filename) ||
            !FFileHelper::LoadFileToArray(Record.Bytes, *Record.Filename) ||
            Record.Bytes.IsEmpty())
        {
            OutError = TEXT("Could not snapshot a protected inherited V1/V2 mesh/texture/wind asset: ") +
                PackageName;
            return false;
        }
        OutRecords.Add(MoveTemp(Record));
    }
    // Exterior V6 is protected when staged, but it is not mandatory in the
    // current live inventory (which ends at V5).  This avoids a false refusal
    // while still byte-protecting a future/staged V6 package if present.
    const FString OptionalV6(TEXT("/Game/Maps/Istana_PublicView_Exterior_v6"));
    FProtectedPackageBytes OptionalRecord;
    OptionalRecord.PackageName = OptionalV6;
    if (FPackageName::DoesPackageExist(OptionalV6, &OptionalRecord.Filename))
    {
        if (!FFileHelper::LoadFileToArray(
                OptionalRecord.Bytes, *OptionalRecord.Filename) ||
            OptionalRecord.Bytes.IsEmpty())
        {
            OutError = TEXT("Could not snapshot optional protected Exterior V6 map.");
            return false;
        }
        OutRecords.Add(MoveTemp(OptionalRecord));
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedPackages(
    const TArray<FProtectedPackageBytes>& Records,
    FString& OutError)
{
    const FString OptionalV6(TEXT("/Game/Maps/Istana_PublicView_Exterior_v6"));
    const bool bOptionalV6Exists = FPackageName::DoesPackageExist(OptionalV6);
    if (Records.Num() != ProtectedMapPackages().Num() +
            ProtectedV2AssetPackages().Num() +
            (bOptionalV6Exists ? 1 : 0))
    {
        OutError = TEXT("Protected map snapshot roster changed.");
        return false;
    }
    for (const FProtectedPackageBytes& Record : Records)
    {
        FString Filename;
        TArray<uint8> CurrentBytes;
        if (!(ProtectedMapPackages().Contains(Record.PackageName) ||
              ProtectedV2AssetPackages().Contains(Record.PackageName) ||
              Record.PackageName == OptionalV6) ||
            !FPackageName::DoesPackageExist(Record.PackageName, &Filename) ||
            !FPaths::IsSamePath(Filename, Record.Filename) ||
            !FFileHelper::LoadFileToArray(CurrentBytes, *Filename) ||
            CurrentBytes != Record.Bytes)
        {
            OutError = TEXT("A protected V1-V6/Explore V1-V2 package changed: ") +
                Record.PackageName;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

template <typename T>
T* AddMaterialExpression(UMaterial* Material, int32 EditorX, int32 EditorY)
{
    T* Expression = Material ? NewObject<T>(Material) : nullptr;
    if (Expression)
    {
        Material->GetExpressionCollection().AddExpression(Expression);
        Expression->MaterialExpressionEditorX = EditorX;
        Expression->MaterialExpressionEditorY = EditorY;
    }
    return Expression;
}

void ConnectCustomInput(
    UMaterialExpressionCustom* Custom,
    const FName Name,
    UMaterialExpression* Expression,
    int32 OutputIndex = 0)
{
    FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
    Input.InputName = Name;
    Input.Input.Connect(OutputIndex, Expression);
}

UTexture2D* ResolveImportedTexture(
    UAssetImportTask* Task,
    const FString& ExactObjectPath)
{
    if (Task)
    {
        for (UObject* Object : Task->GetObjects())
        {
            if (UTexture2D* Texture = Cast<UTexture2D>(Object))
            {
                if (Texture->GetPathName() == ExactObjectPath)
                {
                    return Texture;
                }
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
    const FString ExpectedObject = TextureObjectPath(Spec.AssetName);
    TArray<FString> ImportedSources = Texture && Texture->AssetImportData
        ? Texture->AssetImportData->ExtractFilenames()
        : TArray<FString>();
    FString ActualSource = ImportedSources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(ImportedSources[0])
        : FString();
    FString ExpectedSource = FPaths::ConvertRelativePathToFull(Spec.SourcePath);
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(ExpectedSource);
    FString SourceSha;
    int64 SourceBytes = -1;
    if (!Texture || Texture->GetPathName() != ExpectedObject ||
        Texture->Source.GetSizeX() != 1024 ||
        Texture->Source.GetSizeY() != 1024 ||
        Texture->SRGB != Spec.bSrgb ||
        Texture->CompressionSettings != Spec.Compression ||
        Texture->LODGroup != Spec.Group ||
        Texture->MipGenSettings != TMGS_FromTextureGroup ||
        Texture->AddressX != TA_Wrap || Texture->AddressY != TA_Wrap ||
        Texture->bFlipGreenChannel != Spec.bFlipGreen ||
        Texture->VirtualTextureStreaming || ImportedSources.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        !HashFileSha256(ExpectedSource, SourceSha, SourceBytes) ||
        SourceBytes != Spec.Bytes || SourceSha != Spec.Sha256)
    {
        OutError = TEXT("Imported V3 texture/source/policy is invalid: ") +
            Spec.AssetName;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ImportTextures(
    IAssetTools& AssetTools,
    const TArray<FTextureSourceSpec>& Specs,
    TMap<FString, UTexture2D*>& OutTextures,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    OutTextures.Reset();
    TArray<UAssetImportTask*> Tasks;
    TMap<FString, UAssetImportTask*> TasksByName;
    for (const FTextureSourceSpec& Spec : Specs)
    {
        UTextureFactory* Factory = NewObject<UTextureFactory>();
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        if (!Factory || !Task || TasksByName.Contains(Spec.AssetName))
        {
            OutError = TEXT("Could not allocate the exact unique V3 texture import roster.");
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
        Tasks.Add(Task);
        TasksByName.Add(Spec.AssetName, Task);
    }
    if (Tasks.Num() != 36)
    {
        OutError = TEXT("V3 import requires the exact 36-texture source roster.");
        return false;
    }
    AssetTools.ImportAssetTasks(Tasks);

    for (const FTextureSourceSpec& Spec : Specs)
    {
        UTexture2D* Texture = ResolveImportedTexture(
            TasksByName.FindRef(Spec.AssetName),
            TextureObjectPath(Spec.AssetName));
        if (!Texture)
        {
            OutError = TEXT("Texture import produced no exact V3 object: ") +
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
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (!ValidateImportedTexture(Texture, Spec, OutError))
        {
            return false;
        }
        OutTextures.Add(Spec.AssetName, Texture);
        OutAssetsToSave.Add(Texture);
    }
    OutError.Reset();
    return true;
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
    const FMaterialSpec& Spec,
    FString& OutError)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly || EditorOnly->WorldPositionOffset.Expression)
    {
        OutError = TEXT("V3 wind graph requires one material with no prior WPO.");
        return false;
    }
    UMaterialExpressionWorldPosition* WorldPosition =
        AddMaterialExpression<UMaterialExpressionWorldPosition>(Material, -1300, 520);
    UMaterialExpressionTransformPosition* InstanceLocalPosition =
        AddMaterialExpression<UMaterialExpressionTransformPosition>(Material, -1050, 520);
    UMaterialExpressionTime* Time =
        AddMaterialExpression<UMaterialExpressionTime>(Material, -1050, 720);
    UMaterialExpressionScalarParameter* Strength =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -800, 520);
    UMaterialExpressionScalarParameter* Speed =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -800, 620);
    UMaterialExpressionScalarParameter* Height =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -800, 720);
    UMaterialExpressionScalarParameter* Response =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -800, 820);
    UMaterialExpressionScalarParameter* MaximumWpo =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(Material, -800, 920);
    UMaterialExpressionVectorParameter* Direction =
        AddMaterialExpression<UMaterialExpressionVectorParameter>(Material, -800, 1020);
    UMaterialExpressionCustom* Wind =
        AddMaterialExpression<UMaterialExpressionCustom>(Material, -420, 650);
    if (!WorldPosition || !InstanceLocalPosition || !Time ||
        !Strength || !Speed || !Height || !Response || !MaximumWpo ||
        !Direction || !Wind)
    {
        OutError = TEXT("Could not allocate complete V3 instance-local wind graph.");
        return false;
    }
    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    InstanceLocalPosition->TransformSourceType = TRANSFORMPOSSOURCE_World;
    InstanceLocalPosition->TransformType = TRANSFORMPOSSOURCE_Instance;
    InstanceLocalPosition->Input.Connect(0, WorldPosition);
    Strength->ParameterName = TEXT("TRIAD_WindStrengthCm");
    Strength->DefaultValue = 5.0f;
    Speed->ParameterName = TEXT("TRIAD_WindSpeed");
    Speed->DefaultValue = 1.42f;
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
    const FMaterialSpec& Spec,
    FString& OutError)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const UMaterialExpressionCustom* Wind = nullptr;
    const UMaterialExpressionWorldPosition* WorldPosition = nullptr;
    const UMaterialExpressionTransformPosition* InstanceTransform = nullptr;
    const UMaterialExpressionTime* Time = nullptr;
    const UMaterialExpressionScalarParameter* Strength = nullptr;
    const UMaterialExpressionScalarParameter* Speed = nullptr;
    const UMaterialExpressionScalarParameter* Height = nullptr;
    const UMaterialExpressionScalarParameter* Response = nullptr;
    const UMaterialExpressionScalarParameter* MaximumWpo = nullptr;
    const UMaterialExpressionVectorParameter* Direction = nullptr;
    int32 CustomNodes = 0;
    int32 WorldPositionNodes = 0;
    int32 InstanceTransformNodes = 0;
    int32 TimeNodes = 0;
    int32 ScalarNodes = 0;
    int32 VectorNodes = 0;
    if (EditorOnly)
    {
        for (const UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            if (const UMaterialExpressionCustom* Custom =
                    Cast<UMaterialExpressionCustom>(Expression))
            {
                ++CustomNodes;
                if (Custom->Description == WindCustomDescription &&
                    Custom->Code == WindCustomCode)
                {
                    Wind = Custom;
                }
            }
            if (const UMaterialExpressionWorldPosition* Position =
                    Cast<UMaterialExpressionWorldPosition>(Expression))
            {
                ++WorldPositionNodes;
                WorldPosition = Position;
            }
            if (const UMaterialExpressionTransformPosition* Transform =
                    Cast<UMaterialExpressionTransformPosition>(Expression))
            {
                ++InstanceTransformNodes;
                InstanceTransform = Transform;
            }
            if (const UMaterialExpressionTime* TimeExpression =
                    Cast<UMaterialExpressionTime>(Expression))
            {
                ++TimeNodes;
                Time = TimeExpression;
            }
            if (const UMaterialExpressionScalarParameter* Scalar =
                    Cast<UMaterialExpressionScalarParameter>(Expression))
            {
                ++ScalarNodes;
                if (Scalar->ParameterName == TEXT("TRIAD_WindStrengthCm"))
                {
                    Strength = Scalar;
                }
                else if (Scalar->ParameterName == TEXT("TRIAD_WindSpeed"))
                {
                    Speed = Scalar;
                }
                else if (Scalar->ParameterName == TEXT("TRIAD_WindHeightCm"))
                {
                    Height = Scalar;
                }
                else if (Scalar->ParameterName == TEXT("TRIAD_WindResponseScale"))
                {
                    Response = Scalar;
                }
                else if (Scalar->ParameterName == TEXT("TRIAD_MaxWpoCm"))
                {
                    MaximumWpo = Scalar;
                }
            }
            if (const UMaterialExpressionVectorParameter* Vector =
                    Cast<UMaterialExpressionVectorParameter>(Expression))
            {
                ++VectorNodes;
                if (Vector->ParameterName == TEXT("TRIAD_WindDirection"))
                {
                    Direction = Vector;
                }
            }
        }
    }
    bool bExactInputs = Wind && WorldPosition && InstanceTransform && Time &&
        Strength && Speed && Direction && Height && Response && MaximumWpo &&
        Wind->Inputs.Num() == 9;
    if (bExactInputs)
    {
        const FName ExpectedNames[9] = {
            TEXT("WorldPosition"), TEXT("InstanceLocalPosition"),
            TEXT("TimeSeconds"), TEXT("WindStrengthCm"), TEXT("WindSpeed"),
            TEXT("WindDirection"), TEXT("HeightCm"),
            TEXT("ResponseScale"), TEXT("MaxWpoCm")};
        const UMaterialExpression* ExpectedExpressions[9] = {
            WorldPosition, InstanceTransform, Time, Strength, Speed, Direction,
            Height, Response, MaximumWpo};
        for (int32 Index = 0; Index < 9; ++Index)
        {
            bExactInputs &= Wind->Inputs[Index].InputName == ExpectedNames[Index] &&
                Wind->Inputs[Index].Input.Expression == ExpectedExpressions[Index] &&
                Wind->Inputs[Index].Input.OutputIndex == 0;
        }
    }
    if (!Material || Material->GetPathName() != MaterialObjectPath(Spec.AssetName) ||
        !EditorOnly || !Material->bUsedWithInstancedStaticMeshes ||
        Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5) ||
        Material->MaterialDomain != MD_Surface || Material->bUseMaterialAttributes ||
        !FMath::IsNearlyEqual(
            Material->MaxWorldPositionOffsetDisplacement,
            Spec.MaximumWpoCm,
            0.001f) ||
        CustomNodes != 1 || WorldPositionNodes != 1 ||
        InstanceTransformNodes != 1 || TimeNodes != 1 || ScalarNodes != 5 ||
        VectorNodes != 1 ||
        !Wind || Wind->OutputType != CMOT_Float3 || !bExactInputs ||
        EditorOnly->WorldPositionOffset.Expression != Wind ||
        EditorOnly->WorldPositionOffset.OutputIndex != 0 ||
        WorldPosition->WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets ||
        InstanceTransform->TransformSourceType != TRANSFORMPOSSOURCE_World ||
        InstanceTransform->TransformType != TRANSFORMPOSSOURCE_Instance ||
        InstanceTransform->Input.Expression != WorldPosition ||
        InstanceTransform->Input.OutputIndex != 0 ||
        !FMath::IsNearlyEqual(Strength->DefaultValue, 5.0f, 0.001f) ||
        !FMath::IsNearlyEqual(Speed->DefaultValue, 1.42f, 0.001f) ||
        !FMath::IsNearlyEqual(Height->DefaultValue, Spec.HeightCm, 0.001f) ||
        !FMath::IsNearlyEqual(Response->DefaultValue, Spec.ResponseScale, 0.001f) ||
        !FMath::IsNearlyEqual(MaximumWpo->DefaultValue, Spec.MaximumWpoCm, 0.001f) ||
        !FMath::IsNearlyEqual(Direction->DefaultValue.R, 0.93f, 0.001f) ||
        !FMath::IsNearlyEqual(Direction->DefaultValue.G, 0.37f, 0.001f) ||
        !FMath::IsNearlyZero(Direction->DefaultValue.B, 0.001f) ||
        !FMath::IsNearlyZero(Direction->DefaultValue.A, 0.001f) ||
        Material->GetBlendMode() != (Spec.bMasked ? BLEND_Masked : BLEND_Opaque) ||
        (Spec.bMasked && !FMath::IsNearlyEqual(
            Material->GetOpacityMaskClipValue(), 0.333f, 0.001f)) ||
        Material->IsTwoSided() != Spec.bTwoSidedFoliage ||
        !Material->GetShadingModels().HasOnlyShadingModel(
            Spec.bTwoSidedFoliage ? MSM_TwoSidedFoliage : MSM_DefaultLit))
    {
        OutError = TEXT("Saved V3 HISM material lost its PBR/instance-local wind contract: ") +
            Spec.AssetName;
        return false;
    }
    OutError.Reset();
    return true;
}

bool MatchesMaterialTextureInput(
    const FExpressionInput& Input,
    const FName ParameterName,
    const FString& ExactTexturePath,
    int32 ExpectedOutputIndex,
    EMaterialSamplerType ExpectedSampler)
{
    const UMaterialExpressionTextureSampleParameter2D* Sample =
        Cast<UMaterialExpressionTextureSampleParameter2D>(Input.Expression);
    return Sample && Sample->ParameterName == ParameterName && Sample->Texture &&
        Sample->Texture->GetPathName() == ExactTexturePath &&
        Sample->SamplerType == ExpectedSampler &&
        Sample->SamplerSource == SSM_FromTextureAsset &&
        Sample->Coordinates.Expression == nullptr &&
        Input.OutputIndex == ExpectedOutputIndex;
}

bool ValidateVegetationMaterial(
    UMaterial* Material,
    const FMaterialSpec& Spec,
    FString& OutError)
{
    if (!ValidateWindMaterial(Material, Spec, OutError))
    {
        return false;
    }
    const UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
    const int32 ExpectedExpressionCount = 13 +
        (Spec.bMasked ? 1 : 0) + (Spec.bTwoSidedFoliage ? 1 : 0);
    const EMaterialSamplerType SubsurfaceSampler =
        Spec.SubsurfaceTexture == TEXT("Foliage008_Scattering")
        ? SAMPLERTYPE_Masks
        : SAMPLERTYPE_Color;
    const bool bOpacityExact = Spec.bMasked
        ? MatchesMaterialTextureInput(
            EditorOnly->OpacityMask, TEXT("OpacityTexture"),
            TextureObjectPath(Spec.OpacityTexture), 1, SAMPLERTYPE_Masks)
        : EditorOnly->OpacityMask.Expression == nullptr;
    const bool bSubsurfaceExact = Spec.bTwoSidedFoliage
        ? MatchesMaterialTextureInput(
            EditorOnly->SubsurfaceColor, TEXT("SubsurfaceTexture"),
            TextureObjectPath(Spec.SubsurfaceTexture), 0, SubsurfaceSampler)
        : EditorOnly->SubsurfaceColor.Expression == nullptr;
    if (!EditorOnly ||
        EditorOnly->ExpressionCollection.Expressions.Num() !=
            ExpectedExpressionCount ||
        !MatchesMaterialTextureInput(
            EditorOnly->BaseColor, TEXT("BaseColorTexture"),
            TextureObjectPath(Spec.BaseColorTexture), 0, SAMPLERTYPE_Color) ||
        !MatchesMaterialTextureInput(
            EditorOnly->Normal, TEXT("NormalTexture"),
            TextureObjectPath(Spec.NormalTexture), 0, SAMPLERTYPE_Normal) ||
        !MatchesMaterialTextureInput(
            EditorOnly->Roughness, TEXT("RoughnessTexture"),
            TextureObjectPath(Spec.RoughnessTexture), 1, SAMPLERTYPE_Masks) ||
        !bOpacityExact || !bSubsurfaceExact ||
        EditorOnly->Opacity.Expression || EditorOnly->Metallic.Expression ||
        EditorOnly->Specular.Expression || EditorOnly->EmissiveColor.Expression ||
        EditorOnly->AmbientOcclusion.Expression ||
        EditorOnly->PixelDepthOffset.Expression)
    {
        OutError = TEXT("Saved V3 vegetation material lost an exact PBR texture/sampler/output connection: ") +
            Spec.AssetName;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateDarkColumnarLeafMaterial(
    UMaterial* Material,
    FString& OutError)
{
    const FMaterialSpec Spec = DarkColumnarMaterialSpec();
    if (!ValidateWindMaterial(Material, Spec, OutError))
    {
        return false;
    }
    const UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
    const UMaterialExpressionMultiply* BaseTint = EditorOnly
        ? Cast<UMaterialExpressionMultiply>(EditorOnly->BaseColor.Expression)
        : nullptr;
    const UMaterialExpressionMultiply* SubsurfaceTint = EditorOnly
        ? Cast<UMaterialExpressionMultiply>(EditorOnly->SubsurfaceColor.Expression)
        : nullptr;
    const UMaterialExpressionConstant3Vector* Tint = BaseTint
        ? Cast<UMaterialExpressionConstant3Vector>(BaseTint->B.Expression)
        : nullptr;
    const UMaterialExpressionCustom* ConnectedWind = EditorOnly
        ? Cast<UMaterialExpressionCustom>(
            EditorOnly->WorldPositionOffset.Expression)
        : nullptr;
    const UMaterialExpressionCustom* ExactWindNode = nullptr;
    int32 V3WindNodes = 0;
    int32 InheritedV2WindNodes = 0;
    if (EditorOnly)
    {
        for (const UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            if (const UMaterialExpressionCustom* Custom =
                    Cast<UMaterialExpressionCustom>(Expression))
            {
                if (Custom->Description == WindCustomDescription &&
                    Custom->Code == WindCustomCode)
                {
                    ++V3WindNodes;
                    ExactWindNode = Custom;
                }
                InheritedV2WindNodes +=
                    Custom->Description == InheritedV2WindCustomDescription
                    ? 1
                    : 0;
            }
        }
    }
    const bool bTintExact = Tint &&
        FMath::IsNearlyEqual(Tint->Constant.R, DarkColumnarTint.R, 0.0001f) &&
        FMath::IsNearlyEqual(Tint->Constant.G, DarkColumnarTint.G, 0.0001f) &&
        FMath::IsNearlyEqual(Tint->Constant.B, DarkColumnarTint.B, 0.0001f) &&
        FMath::IsNearlyEqual(Tint->Constant.A, DarkColumnarTint.A, 0.0001f);
    if (!EditorOnly || !BaseTint || !SubsurfaceTint ||
        BaseTint != SubsurfaceTint || !bTintExact ||
        BaseTint->B.Expression != Tint || BaseTint->B.OutputIndex != 0 ||
        EditorOnly->BaseColor.OutputIndex != 0 ||
        EditorOnly->SubsurfaceColor.OutputIndex != 0 ||
        EditorOnly->ExpressionCollection.Expressions.Num() != 16 ||
        !MatchesMaterialTextureInput(
            BaseTint->A, TEXT("BaseColorTexture"),
            V1LeafDiffuseTexturePath, 0, SAMPLERTYPE_Color) ||
        !MatchesMaterialTextureInput(
            EditorOnly->Normal, TEXT("NormalTexture"),
            V1LeafNormalTexturePath, 0, SAMPLERTYPE_Normal) ||
        !MatchesMaterialTextureInput(
            EditorOnly->Roughness, TEXT("RoughnessTexture"),
            V1LeafRoughnessTexturePath, 1, SAMPLERTYPE_Masks) ||
        !MatchesMaterialTextureInput(
            EditorOnly->OpacityMask, TEXT("OpacityTexture"),
            V1LeafOpacityTexturePath, 1, SAMPLERTYPE_Masks) ||
        !ConnectedWind || ConnectedWind != ExactWindNode ||
        ConnectedWind->Description != WindCustomDescription ||
        ConnectedWind->Code != WindCustomCode || V3WindNodes != 1 ||
        InheritedV2WindNodes != 0)
    {
        OutError = TEXT("The V3-owned dark-columnar material lost its exact fresh-texture/tint/single-pivot-safe-WPO contract.");
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterial* CreateVegetationMaterial(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
    const TMap<FString, UTexture2D*>& Textures,
    FString& OutError)
{
    UTexture2D* Base = Textures.FindRef(Spec.BaseColorTexture);
    UTexture2D* Normal = Textures.FindRef(Spec.NormalTexture);
    UTexture2D* Roughness = Textures.FindRef(Spec.RoughnessTexture);
    UTexture2D* Opacity = Spec.OpacityTexture.IsEmpty()
        ? nullptr
        : Textures.FindRef(Spec.OpacityTexture);
    UTexture2D* Subsurface = Spec.SubsurfaceTexture.IsEmpty()
        ? nullptr
        : Textures.FindRef(Spec.SubsurfaceTexture);
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            Spec.AssetName,
            MaterialAssetPath,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaExploreV3Assets"))))
        : nullptr;
    if (!Material || !Base || !Normal || !Roughness ||
        (Spec.bMasked && !Opacity) ||
        (Spec.bTwoSidedFoliage && !Subsurface))
    {
        OutError = TEXT("Could not create complete V3 PBR material: ") +
            Spec.AssetName;
        return nullptr;
    }
    UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
    if (!EditorOnly || !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("New V3 material did not begin with an empty graph.");
        return nullptr;
    }
    UMaterialExpressionTextureSampleParameter2D* BaseSample = AddTextureSample(
        Material, Base, TEXT("BaseColorTexture"), SAMPLERTYPE_Color, -720, -180);
    UMaterialExpressionTextureSampleParameter2D* NormalSample = AddTextureSample(
        Material, Normal, TEXT("NormalTexture"), SAMPLERTYPE_Normal, -720, 20);
    UMaterialExpressionTextureSampleParameter2D* RoughnessSample = AddTextureSample(
        Material, Roughness, TEXT("RoughnessTexture"), SAMPLERTYPE_Masks, -720, 220);
    UMaterialExpressionTextureSampleParameter2D* OpacitySample = Spec.bMasked
        ? AddTextureSample(
            Material, Opacity, TEXT("OpacityTexture"), SAMPLERTYPE_Masks, -720, 400)
        : nullptr;
    UMaterialExpressionTextureSampleParameter2D* SubsurfaceSample =
        Spec.bTwoSidedFoliage
        ? AddTextureSample(
            Material,
            Subsurface,
            TEXT("SubsurfaceTexture"),
            SamplerTypeForTexture(Subsurface),
            -720,
            580)
        : nullptr;
    if (!BaseSample || !NormalSample || !RoughnessSample ||
        (Spec.bMasked && !OpacitySample) ||
        (Spec.bTwoSidedFoliage && !SubsurfaceSample) ||
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
    EditorOnly->Normal.Connect(0, NormalSample);
    EditorOnly->Roughness.Connect(1, RoughnessSample);
    if (OpacitySample)
    {
        EditorOnly->OpacityMask.Connect(1, OpacitySample);
    }
    if (SubsurfaceSample)
    {
        EditorOnly->SubsurfaceColor.Connect(0, SubsurfaceSample);
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateVegetationMaterial(Material, Spec, OutError))
    {
        return nullptr;
    }
    OutError.Reset();
    return Material;
}

UMaterial* CreateFormalLawnMaterial(
    IAssetTools& AssetTools,
    const TMap<FString, UTexture2D*>& Textures,
    FString& OutError)
{
    const FString Name(TEXT("M_IPVExploreV3_FormalLawn"));
    UTexture2D* Base = Textures.FindRef(TEXT("Grass004_Color"));
    UTexture2D* Normal = Textures.FindRef(TEXT("Grass004_NormalGL"));
    UTexture2D* Roughness = Textures.FindRef(TEXT("Grass004_Roughness"));
    UTexture2D* Ao = Textures.FindRef(TEXT("Grass004_AmbientOcclusion"));
    UTexture2D* Height = Textures.FindRef(TEXT("Grass004_Displacement"));
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            Name,
            MaterialAssetPath,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaExploreV3Assets"))))
        : nullptr;
    if (!Material || !Base || !Normal || !Roughness || !Ao || !Height)
    {
        OutError = TEXT("Could not create the complete ambientCG Grass004 lawn material.");
        return nullptr;
    }
    UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
    if (!EditorOnly || !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("New FormalLawn material did not begin with an empty graph.");
        return nullptr;
    }
    UMaterialExpressionTextureSampleParameter2D* BaseSample = AddTextureSample(
        Material, Base, TEXT("BaseColorTexture"), SamplerTypeForTexture(Base), -620, -220);
    UMaterialExpressionTextureSampleParameter2D* NormalSample = AddTextureSample(
        Material, Normal, TEXT("NormalTexture"), SamplerTypeForTexture(Normal), -620, -40);
    UMaterialExpressionTextureSampleParameter2D* RoughnessSample = AddTextureSample(
        Material, Roughness, TEXT("RoughnessTexture"), SamplerTypeForTexture(Roughness), -620, 140);
    UMaterialExpressionTextureSampleParameter2D* AoSample = AddTextureSample(
        Material, Ao, TEXT("AmbientOcclusionTexture"), SamplerTypeForTexture(Ao), -620, 320);
    UMaterialExpressionTextureSampleParameter2D* HeightSample = AddTextureSample(
        Material, Height, TEXT("HeightTexture"), SamplerTypeForTexture(Height), -620, 500);
    if (!BaseSample || !NormalSample || !RoughnessSample ||
        !AoSample || !HeightSample)
    {
        OutError = TEXT("Could not allocate all five Grass004 material inputs.");
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->bUseMaterialAttributes = false;
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = false;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    EditorOnly->BaseColor.Connect(0, BaseSample);
    EditorOnly->Normal.Connect(0, NormalSample);
    EditorOnly->Roughness.Connect(1, RoughnessSample);
    EditorOnly->AmbientOcclusion.Connect(1, AoSample);
    // Prepared height is a shallow pixel-depth cue only; it is never terrain
    // geometry, collision, or an elevation/survey source.
    EditorOnly->PixelDepthOffset.Connect(1, HeightSample);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutError.Reset();
    return Material;
}

bool ValidateFormalLawnMaterial(UMaterial* Material, FString& OutError)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || Material->GetPathName() != FormalLawnMaterialPath ||
        !EditorOnly ||
        Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5) ||
        Material->MaterialDomain != MD_Surface || Material->bUseMaterialAttributes ||
        Material->GetBlendMode() != BLEND_Opaque || Material->IsTwoSided() ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !Material->bUsedWithInstancedStaticMeshes ||
        EditorOnly->ExpressionCollection.Expressions.Num() != 5 ||
        !MatchesMaterialTextureInput(
            EditorOnly->BaseColor, TEXT("BaseColorTexture"),
            TextureObjectPath(TEXT("Grass004_Color")), 0, SAMPLERTYPE_Color) ||
        !MatchesMaterialTextureInput(
            EditorOnly->Normal, TEXT("NormalTexture"),
            TextureObjectPath(TEXT("Grass004_NormalGL")), 0,
            SAMPLERTYPE_Normal) ||
        !MatchesMaterialTextureInput(
            EditorOnly->Roughness, TEXT("RoughnessTexture"),
            TextureObjectPath(TEXT("Grass004_Roughness")), 1,
            SAMPLERTYPE_Masks) ||
        !MatchesMaterialTextureInput(
            EditorOnly->AmbientOcclusion, TEXT("AmbientOcclusionTexture"),
            TextureObjectPath(TEXT("Grass004_AmbientOcclusion")), 1,
            SAMPLERTYPE_Masks) ||
        !MatchesMaterialTextureInput(
            EditorOnly->PixelDepthOffset, TEXT("HeightTexture"),
            TextureObjectPath(TEXT("Grass004_Displacement")), 1,
            SAMPLERTYPE_Masks) ||
        !FMath::IsNearlyZero(
            Material->MaxWorldPositionOffsetDisplacement, 0.001f) ||
        EditorOnly->WorldPositionOffset.Expression ||
        EditorOnly->Opacity.Expression || EditorOnly->OpacityMask.Expression ||
        EditorOnly->Metallic.Expression || EditorOnly->Specular.Expression ||
        EditorOnly->EmissiveColor.Expression ||
        EditorOnly->SubsurfaceColor.Expression)
    {
        OutError = TEXT("The exact five-map Grass004 FormalLawn PBR/no-WPO material contract changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSimpleMaterial(
    UMaterial* Material,
    const FString& Name,
    const FLinearColor& ExpectedColor,
    float ExpectedRoughness,
    float ExpectedMetallic,
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
        ? Cast<UMaterialExpressionConstant>(EditorOnly->Roughness.Expression)
        : nullptr;
    const UMaterialExpressionConstant* Metal = EditorOnly
        ? Cast<UMaterialExpressionConstant>(EditorOnly->Metallic.Expression)
        : nullptr;
    const bool bColorExact = Base &&
        FMath::IsNearlyEqual(Base->Constant.R, ExpectedColor.R, 0.0001f) &&
        FMath::IsNearlyEqual(Base->Constant.G, ExpectedColor.G, 0.0001f) &&
        FMath::IsNearlyEqual(Base->Constant.B, ExpectedColor.B, 0.0001f) &&
        FMath::IsNearlyEqual(Base->Constant.A, ExpectedColor.A, 0.0001f);
    if (!Material || Material->GetPathName() != MaterialObjectPath(Name) ||
        !EditorOnly ||
        Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5) ||
        Material->MaterialDomain != MD_Surface || Material->bUseMaterialAttributes ||
        Material->GetBlendMode() != BLEND_Opaque || Material->IsTwoSided() ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !FMath::IsNearlyZero(
            Material->MaxWorldPositionOffsetDisplacement, 0.001f) ||
        EditorOnly->ExpressionCollection.Expressions.Num() != 3 ||
        !bColorExact || !Rough || !Metal ||
        EditorOnly->BaseColor.OutputIndex != 0 ||
        EditorOnly->Roughness.OutputIndex != 0 ||
        EditorOnly->Metallic.OutputIndex != 0 ||
        !FMath::IsNearlyEqual(Rough->R, ExpectedRoughness, 0.0001f) ||
        !FMath::IsNearlyEqual(Metal->R, ExpectedMetallic, 0.0001f) ||
        EditorOnly->Normal.Expression || EditorOnly->Specular.Expression ||
        EditorOnly->EmissiveColor.Expression || EditorOnly->Opacity.Expression ||
        EditorOnly->OpacityMask.Expression ||
        EditorOnly->SubsurfaceColor.Expression ||
        EditorOnly->AmbientOcclusion.Expression ||
        EditorOnly->WorldPositionOffset.Expression ||
        EditorOnly->PixelDepthOffset.Expression)
    {
        OutError = TEXT("A V3 portico/context material lost its exact constant graph/value/shader contract: ") +
            Name;
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterial* CreateSimpleMaterial(
    IAssetTools& AssetTools,
    const FString& Name,
    const FLinearColor& BaseColor,
    float Roughness,
    float Metallic,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            Name,
            MaterialAssetPath,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaExploreV3Assets"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("New simple V3 material did not begin with an empty graph: ") +
            Name;
        return nullptr;
    }
    UMaterialExpressionConstant3Vector* Base =
        AddMaterialExpression<UMaterialExpressionConstant3Vector>(Material, -420, -80);
    UMaterialExpressionConstant* Rough =
        AddMaterialExpression<UMaterialExpressionConstant>(Material, -420, 100);
    UMaterialExpressionConstant* Metal =
        AddMaterialExpression<UMaterialExpressionConstant>(Material, -420, 260);
    if (!Base || !Rough || !Metal)
    {
        OutError = TEXT("Could not create simple V3 material: ") + Name;
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->bUseMaterialAttributes = false;
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = false;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Base->Constant = BaseColor;
    Rough->R = Roughness;
    Metal->R = Metallic;
    EditorOnly->BaseColor.Connect(0, Base);
    EditorOnly->Roughness.Connect(0, Rough);
    EditorOnly->Metallic.Connect(0, Metal);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateSimpleMaterial(
            Material, Name, BaseColor, Roughness, Metallic, OutError))
    {
        return nullptr;
    }
    OutError.Reset();
    return Material;
}

struct FSimpleMaterialSpec
{
    const TCHAR* Name;
    FLinearColor Color;
    float Roughness;
    float Metallic;
};

const TArray<FSimpleMaterialSpec>& SimpleMaterialSpecs()
{
    static const TArray<FSimpleMaterialSpec> Specs = {
        {TEXT("M_IPV7_Portico_Render"), FLinearColor(0.86f, 0.84f, 0.76f), 0.62f, 0.0f},
        {TEXT("M_IPV7_Portico_Trim"), FLinearColor(0.95f, 0.94f, 0.88f), 0.62f, 0.0f},
        {TEXT("M_IPV7_Portico_Louvre"), FLinearColor(0.35f, 0.36f, 0.31f), 0.62f, 0.0f},
        {TEXT("M_IPV7_Portico_Recess"), FLinearColor(0.055f, 0.060f, 0.055f), 0.72f, 0.0f},
        {TEXT("M_IPV7_Portico_Glass"), FLinearColor(0.10f, 0.14f, 0.15f), 0.24f, 0.0f},
        {TEXT("M_IPV7_Portico_Metal"), FLinearColor(0.16f, 0.17f, 0.15f), 0.42f, 0.15f},
        {TEXT("M_IPV7_Portico_Stone"), FLinearColor(0.78f, 0.75f, 0.66f), 0.68f, 0.0f},
        {TEXT("M_IPV7_Portico_Soffit"), FLinearColor(0.74f, 0.72f, 0.66f), 0.72f, 0.0f},
        {TEXT("M_IPVExploreV3_ContextRoad"), FLinearColor(0.075f, 0.080f, 0.085f), 0.86f, 0.0f},
        {TEXT("M_IPVExploreV3_ContextWater"), FLinearColor(0.055f, 0.16f, 0.19f), 0.18f, 0.0f}};
    return Specs;
}

UMaterial* CreateDarkColumnarLeafMaterial(
    IAssetTools& AssetTools,
    FString& OutError)
{
    // Author a clean V3-owned graph from the exact locked V1 texture assets.
    // Never duplicate the V2 leaf material: it already contains a WPO graph,
    // and adding another graph would either fail import or create double sway.
    const FMaterialSpec Spec = DarkColumnarMaterialSpec();
    FDarkColumnarSourceTextures Textures;
    if (!ValidateDarkColumnarSourceTextures(&Textures, OutError))
    {
        return nullptr;
    }
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            DarkColumnarMaterialName,
            MaterialAssetPath,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaExploreV3Assets"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        Material->GetPathName() != DarkColumnarMaterialPath ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty() ||
        EditorOnly->WorldPositionOffset.Expression)
    {
        OutError = TEXT("Could not create the fresh V3-owned dark-columnar material from exact locked leaf textures.");
        return nullptr;
    }

    Material->Modify();
    UMaterialExpressionTextureSampleParameter2D* BaseSample = AddTextureSample(
        Material, Textures.Diffuse, TEXT("BaseColorTexture"),
        SAMPLERTYPE_Color, -720, -220);
    UMaterialExpressionTextureSampleParameter2D* NormalSample = AddTextureSample(
        Material, Textures.Normal, TEXT("NormalTexture"),
        SAMPLERTYPE_Normal, -720, -40);
    UMaterialExpressionTextureSampleParameter2D* RoughnessSample = AddTextureSample(
        Material, Textures.Roughness, TEXT("RoughnessTexture"),
        SAMPLERTYPE_Masks, -720, 140);
    UMaterialExpressionTextureSampleParameter2D* OpacitySample = AddTextureSample(
        Material, Textures.Opacity, TEXT("OpacityTexture"),
        SAMPLERTYPE_Masks, -720, 320);
    UMaterialExpressionMultiply* Darken =
        AddMaterialExpression<UMaterialExpressionMultiply>(Material, 460, -260);
    UMaterialExpressionConstant3Vector* Tint =
        AddMaterialExpression<UMaterialExpressionConstant3Vector>(Material, 250, -140);
    if (!BaseSample || !NormalSample || !RoughnessSample || !OpacitySample ||
        !Darken || !Tint ||
        !AddInstanceLocalWindGraph(Material, Spec, OutError))
    {
        OutError = TEXT("Could not author the V3 dark-columnar tint graph.");
        return nullptr;
    }
    Material->MaterialDomain = MD_Surface;
    Material->bUseMaterialAttributes = false;
    Material->BlendMode = BLEND_Masked;
    Material->TwoSided = true;
    Material->OpacityMaskClipValue = 0.333f;
    Material->SetShadingModel(MSM_TwoSidedFoliage);
    Darken->A.Connect(0, BaseSample);
    Tint->Constant = DarkColumnarTint;
    Darken->B.Connect(0, Tint);
    EditorOnly->BaseColor.Connect(0, Darken);
    EditorOnly->SubsurfaceColor.Connect(0, Darken);
    EditorOnly->Normal.Connect(0, NormalSample);
    EditorOnly->Roughness.Connect(1, RoughnessSample);
    EditorOnly->OpacityMask.Connect(1, OpacitySample);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateDarkColumnarLeafMaterial(Material, OutError))
    {
        return nullptr;
    }
    OutError.Reset();
    return Material;
}

bool CreateAllMaterials(
    IAssetTools& AssetTools,
    const TMap<FString, UTexture2D*>& Textures,
    TMap<FString, UMaterial*>& OutMaterials,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    OutMaterials.Reset();
    for (const FMaterialSpec& Spec : VegetationMaterialSpecs())
    {
        UMaterial* Material = CreateVegetationMaterial(
            AssetTools, Spec, Textures, OutError);
        if (!Material || OutMaterials.Contains(Spec.AssetName))
        {
            return false;
        }
        OutMaterials.Add(Spec.AssetName, Material);
        OutAssetsToSave.Add(Material);
    }
    UMaterial* Lawn = CreateFormalLawnMaterial(AssetTools, Textures, OutError);
    if (!Lawn || !ValidateFormalLawnMaterial(Lawn, OutError))
    {
        return false;
    }
    OutMaterials.Add(TEXT("M_IPVExploreV3_FormalLawn"), Lawn);
    OutAssetsToSave.Add(Lawn);
    UMaterial* DarkColumnar = CreateDarkColumnarLeafMaterial(
        AssetTools, OutError);
    if (!DarkColumnar)
    {
        return false;
    }
    OutMaterials.Add(DarkColumnarMaterialName, DarkColumnar);
    OutAssetsToSave.Add(DarkColumnar);
    for (const FSimpleMaterialSpec& Spec : SimpleMaterialSpecs())
    {
        UMaterial* Material = CreateSimpleMaterial(
            AssetTools,
            Spec.Name,
            Spec.Color,
            Spec.Roughness,
            Spec.Metallic,
            OutError);
        if (!Material || OutMaterials.Contains(Spec.Name))
        {
            return false;
        }
        OutMaterials.Add(Spec.Name, Material);
        OutAssetsToSave.Add(Material);
    }
    if (OutMaterials.Num() != 20)
    {
        OutError = TEXT("The V3 material roster must contain exactly 20 assets.");
        return false;
    }
    OutError.Reset();
    return true;
}

UAssetImportTask* MakeMeshImportTask(
    const FString& Filename,
    const FString& DestinationPath,
    const FString& DestinationName,
    bool bConvertSceneUnit)
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
    Options->StaticMeshImportData->bConvertScene = bConvertSceneUnit;
    Options->StaticMeshImportData->bConvertSceneUnit = bConvertSceneUnit;
    Options->StaticMeshImportData->bForceFrontXAxis = false;
    Options->StaticMeshImportData->bCombineMeshes = true;
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
    Task->Filename = Filename;
    Task->DestinationPath = DestinationPath;
    Task->DestinationName = DestinationName;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

UStaticMesh* ResolveImportedMesh(
    UAssetImportTask* Task,
    const FString& ExactObjectPath)
{
    if (Task)
    {
        for (UObject* Object : Task->GetObjects())
        {
            if (UStaticMesh* Mesh = Cast<UStaticMesh>(Object))
            {
                if (Mesh->GetPathName() == ExactObjectPath)
                {
                    return Mesh;
                }
            }
        }
    }
    return LoadExact<UStaticMesh>(ExactObjectPath);
}

void RemoveVisualMeshCollision(UStaticMesh* Mesh)
{
    Mesh->Modify();
    Mesh->NaniteSettings.bEnabled = false;
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseDefault;
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
}

struct FLodSpec
{
    float PercentTriangles;
    uint32 MaximumTriangles;
    float ScreenSize;
};

bool ConfigureRuntimeLods(
    UStaticMesh* Mesh,
    const TArray<FLodSpec>& GeneratedLods,
    int32 RuntimeMinLod,
    FString& OutError)
{
    if (!Mesh || Mesh->GetNumSourceModels() != 1 || GeneratedLods.IsEmpty())
    {
        OutError = TEXT("Runtime LOD generation requires one imported source model.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    Mesh->Modify();
    Mesh->SetNumSourceModels(GeneratedLods.Num() + 1);
    Mesh->bAutoComputeLODScreenSize = false;
    Mesh->GetSourceModel(0).ScreenSize = FPerPlatformFloat(1.0f);
    for (int32 Index = 0; Index < GeneratedLods.Num(); ++Index)
    {
        const FLodSpec& Spec = GeneratedLods[Index];
        FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(Index + 1);
        SourceModel.ScreenSize = FPerPlatformFloat(Spec.ScreenSize);
        FMeshReductionSettings& Reduction = SourceModel.ReductionSettings;
        Reduction.TerminationCriterion =
            EStaticMeshReductionTerimationCriterion::Triangles;
        Reduction.PercentTriangles = Spec.PercentTriangles;
        Reduction.MaxNumOfTriangles = Spec.MaximumTriangles;
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
    Mesh->SetMinLODIdx(RuntimeMinLod);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
    if (!RenderData || RenderData->LODResources.Num() != GeneratedLods.Num() + 1 ||
        Mesh->GetMinLODIdx() != RuntimeMinLod)
    {
        OutError = TEXT("Generated runtime LOD resources or MinLOD did not persist.");
        return false;
    }
    int32 PreviousTriangles = RenderData->LODResources[0].GetNumTriangles();
    for (int32 Index = 0; Index < GeneratedLods.Num(); ++Index)
    {
        const int32 Triangles =
            RenderData->LODResources[Index + 1].GetNumTriangles();
        if (Triangles <= 0 ||
            Triangles > static_cast<int32>(GeneratedLods[Index].MaximumTriangles) ||
            Triangles >= PreviousTriangles)
        {
            OutError = FString::Printf(
                TEXT("Generated LOD%d exceeds its cap or did not reduce (triangles=%d)."),
                Index + 1,
                Triangles);
            return false;
        }
        PreviousTriangles = Triangles;
    }
    OutError.Reset();
    return true;
}

bool BindSingleMaterial(
    UStaticMesh* Mesh,
    UMaterialInterface* Material,
    const FName CanonicalSlot,
    FString& OutError)
{
    if (!Mesh || !Material || Mesh->GetStaticMaterials().IsEmpty())
    {
        OutError = TEXT("A V3 mesh cannot bind its required single material.");
        return false;
    }
    Mesh->Modify();
    for (FStaticMaterial& Slot : Mesh->GetStaticMaterials())
    {
        Slot.MaterialInterface = Material;
    }
    if (Mesh->GetStaticMaterials().Num() == 1)
    {
        Mesh->GetStaticMaterials()[0].MaterialSlotName = CanonicalSlot;
        Mesh->GetStaticMaterials()[0].ImportedMaterialSlotName = CanonicalSlot;
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    OutError.Reset();
    return true;
}

bool BindIslandTreeMaterials(
    UStaticMesh* Mesh,
    const TMap<FString, UMaterial*>& Materials,
    FString& OutError)
{
    struct FBinding
    {
        FName Slot;
        FString MaterialName;
    };
    const TArray<FBinding> Bindings = {
        {TEXT("island_tree_01"), TEXT("M_IPVExploreV3_IslandTree01_Trunk_Wind")},
        {TEXT("island_tree_01_branches"), TEXT("M_IPVExploreV3_IslandTree01_Branches_Wind")},
        {TEXT("island_tree_01_leaves"), TEXT("M_IPVExploreV3_IslandTree01_Leaves_Wind")}};
    if (!Mesh || Mesh->GetStaticMaterials().Num() != 3)
    {
        OutError = TEXT("Island Tree 01 must retain exactly three imported material slots.");
        return false;
    }
    TSet<int32> Bound;
    Mesh->Modify();
    for (const FBinding& Binding : Bindings)
    {
        int32 SlotIndex = Mesh->GetMaterialIndexFromImportedMaterialSlotName(
            Binding.Slot);
        if (SlotIndex == INDEX_NONE)
        {
            SlotIndex = Mesh->GetMaterialIndex(Binding.Slot);
        }
        UMaterial* Material = Materials.FindRef(Binding.MaterialName);
        if (SlotIndex == INDEX_NONE || Bound.Contains(SlotIndex) || !Material)
        {
            OutError = TEXT("Island Tree imported trunk/branch/leaf slot identity changed.");
            return false;
        }
        Bound.Add(SlotIndex);
        FStaticMaterial& Slot = Mesh->GetStaticMaterials()[SlotIndex];
        Slot.MaterialSlotName = Binding.Slot;
        Slot.ImportedMaterialSlotName = Binding.Slot;
        Slot.MaterialInterface = Material;
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    OutError.Reset();
    return true;
}

struct FPorticoMaterialUseSpec
{
    FName MaterialName;
    int32 TriangleFaces;
};

const TArray<FPorticoMaterialUseSpec>& PorticoUe55ImportedSlotSpecs()
{
    // UE5.5 deterministically orders the six face-assigned OBJ materials this
    // way. Render and Stone are zero-face MTL palette definitions, so Unreal
    // correctly does not fabricate FStaticMaterial entries for them.
    static const TArray<FPorticoMaterialUseSpec> Specs = {
        {TEXT("M_IPV7_Portico_Soffit"), 948},
        {TEXT("M_IPV7_Portico_Trim"), 3176},
        {TEXT("M_IPV7_Portico_Recess"), 72},
        {TEXT("M_IPV7_Portico_Metal"), 252},
        {TEXT("M_IPV7_Portico_Glass"), 576},
        {TEXT("M_IPV7_Portico_Louvre"), 792}};
    return Specs;
}

const TArray<FPorticoMaterialUseSpec>& PorticoObjFaceAssignmentSpecs()
{
    static const TArray<FPorticoMaterialUseSpec> Specs = {
        {TEXT("M_IPV7_Portico_Trim"), 3176},
        {TEXT("M_IPV7_Portico_Soffit"), 948},
        {TEXT("M_IPV7_Portico_Recess"), 72},
        {TEXT("M_IPV7_Portico_Glass"), 576},
        {TEXT("M_IPV7_Portico_Metal"), 252},
        {TEXT("M_IPV7_Portico_Louvre"), 792}};
    return Specs;
}

const TArray<FName>& PorticoMtlDeclaredDefinitions()
{
    static const TArray<FName> Names = {
        TEXT("M_IPV7_Portico_Render"),
        TEXT("M_IPV7_Portico_Trim"),
        TEXT("M_IPV7_Portico_Louvre"),
        TEXT("M_IPV7_Portico_Recess"),
        TEXT("M_IPV7_Portico_Glass"),
        TEXT("M_IPV7_Portico_Metal"),
        TEXT("M_IPV7_Portico_Stone"),
        TEXT("M_IPV7_Portico_Soffit")};
    return Names;
}

const TArray<FName>& PorticoStandaloneUnusedDefinitions()
{
    static const TArray<FName> Names = {
        TEXT("M_IPV7_Portico_Render"),
        TEXT("M_IPV7_Portico_Stone")};
    return Names;
}

bool ValidatePorticoMeshMaterialTopology(
    const UStaticMesh* Mesh,
    const bool bRequireExactV3Bindings,
    FString& OutError)
{
    const TArray<FPorticoMaterialUseSpec>& Specs =
        PorticoUe55ImportedSlotSpecs();
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || !RenderData || RenderData->LODResources.IsEmpty() ||
        Mesh->GetStaticMaterials().Num() != Specs.Num() ||
        RenderData->LODResources[0].Sections.Num() != Specs.Num())
    {
        OutError = TEXT("V7 portico must retain exactly six UE5.5 imported face-assigned material slots and six LOD0 sections; zero-face Render/Stone definitions remain standalone assets only.");
        return false;
    }

    for (int32 SlotIndex = 0; SlotIndex < Specs.Num(); ++SlotIndex)
    {
        const FPorticoMaterialUseSpec& Spec = Specs[SlotIndex];
        const FStaticMaterial& StaticMaterial =
            Mesh->GetStaticMaterials()[SlotIndex];
        const bool bImportedNameExact =
            StaticMaterial.ImportedMaterialSlotName == Spec.MaterialName;
        const bool bDisplayNameExact =
            StaticMaterial.MaterialSlotName == Spec.MaterialName;
        if (!bImportedNameExact || !bDisplayNameExact)
        {
            OutError = TEXT("V7 portico UE5.5 imported FStaticMaterial order/name changed.");
            return false;
        }
        if (bRequireExactV3Bindings)
        {
            const UMaterialInterface* Material = Mesh->GetMaterial(SlotIndex);
            if (!Material || Material->GetPathName() !=
                    MaterialObjectPath(Spec.MaterialName.ToString()))
            {
                OutError = TEXT("V7 portico six used/imported slots lost an exact V3-owned base-material binding.");
                return false;
            }
        }
    }

    TSet<int32> SeenMaterialIndices;
    int32 TriangleTotal = 0;
    for (const FStaticMeshSection& Section :
         RenderData->LODResources[0].Sections)
    {
        const int32 MaterialIndex = Section.MaterialIndex;
        if (!Specs.IsValidIndex(MaterialIndex) ||
            SeenMaterialIndices.Contains(MaterialIndex) ||
            Section.NumTriangles != Specs[MaterialIndex].TriangleFaces)
        {
            OutError = TEXT("V7 portico six-section material-index/triangle-face census changed.");
            return false;
        }
        SeenMaterialIndices.Add(MaterialIndex);
        TriangleTotal += Section.NumTriangles;
    }
    if (SeenMaterialIndices.Num() != Specs.Num() || TriangleTotal != 5816)
    {
        OutError = TEXT("V7 portico six-section triangle-face total or material coverage changed.");
        return false;
    }
    for (const FName UnusedDefinition : PorticoStandaloneUnusedDefinitions())
    {
        if (Mesh->GetMaterialIndex(UnusedDefinition) != INDEX_NONE ||
            Mesh->GetMaterialIndexFromImportedMaterialSlotName(
                UnusedDefinition) != INDEX_NONE)
        {
            OutError = TEXT("V7 portico fabricated a UE slot for zero-face Render or Stone MTL definitions.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool BindPorticoMaterials(
    UStaticMesh* Mesh,
    const TMap<FString, UMaterial*>& Materials,
    FString& OutError)
{
    if (!ValidatePorticoMeshMaterialTopology(Mesh, false, OutError))
    {
        return false;
    }
    for (const FName StandaloneName : PorticoStandaloneUnusedDefinitions())
    {
        UMaterial* Standalone = Materials.FindRef(StandaloneName.ToString());
        if (!Standalone || Standalone->GetPathName() !=
                MaterialObjectPath(StandaloneName.ToString()))
        {
            OutError = TEXT("V7 portico zero-face Render/Stone definitions must remain exact standalone V3 material assets.");
            return false;
        }
    }

    Mesh->Modify();
    const TArray<FPorticoMaterialUseSpec>& Specs =
        PorticoUe55ImportedSlotSpecs();
    for (int32 SlotIndex = 0; SlotIndex < Specs.Num(); ++SlotIndex)
    {
        const FName SlotName = Specs[SlotIndex].MaterialName;
        UMaterial* Material = Materials.FindRef(SlotName.ToString());
        if (!Material || Material->GetPathName() !=
                MaterialObjectPath(SlotName.ToString()))
        {
            OutError = TEXT("V7 portico used/imported material asset is absent or moved.");
            return false;
        }
        FStaticMaterial& Slot = Mesh->GetStaticMaterials()[SlotIndex];
        Slot.MaterialSlotName = SlotName;
        Slot.ImportedMaterialSlotName = SlotName;
        Slot.MaterialInterface = Material;
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    return ValidatePorticoMeshMaterialTopology(Mesh, true, OutError);
}

int32 CountTriangles(const UStaticMesh* Mesh, int32 LodIndex)
{
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    if (!RenderData || !RenderData->LODResources.IsValidIndex(LodIndex))
    {
        return INDEX_NONE;
    }
    return Mesh->GetNumTriangles(LodIndex);
}

struct FMeshImportSpec
{
    FString SourcePath;
    FString DestinationPath;
    FString AssetName;
    FString ExactObjectPath;
    int64 SourceBytes;
    FString SourceSha256;
    FString SourceFormat;
    FString SourceAssetId;
    int32 CatalogPolycount;
    int32 BinaryFbxPolygonCount;
    int32 ExpectedImportedLod0Triangles;
    bool bConvertSceneUnit;
};

const TArray<FMeshImportSpec>& MeshImportSpecs()
{
    const FString V3Root = ProjectSourcePath(TEXT("IstanaPublicViewExploreV3"));
    const FString Poly = FPaths::Combine(V3Root, TEXT("Sources/polyhaven-assets"));
    const FString Prepared = FPaths::Combine(V3Root, TEXT("Prepared"));
    const FString Geo = FPaths::Combine(V3Root, TEXT("Generated/GeospatialContext"));
    const FString Portico = ProjectSourcePath(
        TEXT("IstanaPublicViewV7Portico/GeneratedV5Live/SM_IstanaPublicViewV7_CentralPorticoRefinement_V5Live.obj"));
    static const TArray<FMeshImportSpec> Specs = {
        {FPaths::Combine(Poly, TEXT("island_tree_01/island_tree_01_1k.fbx")), VegetationAssetPath, IslandTreeName, IslandTreeObjectPath, 48055820, TEXT("3F193C0CA27977B001682AA4BA3EEF654B31C1AF60A166AF4024A11CC19A2948"), TEXT("FBX"), TEXT("island_tree_01"), 3729692, 812532, 1599403, true},
        {FPaths::Combine(Poly, TEXT("shrub_02/shrub_02_1k.fbx")), VegetationAssetPath, ShrubName, ShrubObjectPath, 832300, TEXT("7BD42E635ED83A9341A9953280845DC2685615A4A4A5441E31EEDD39D5431EC8"), TEXT("FBX"), TEXT("shrub_02"), 52317, 13834, 27254, true},
        {FPaths::Combine(Poly, TEXT("fern_02/fern_02_1k.fbx")), VegetationAssetPath, FernName, FernObjectPath, 223596, TEXT("6D8238B9CC1BBB5F5766396C2DB26F2219C0D0E5CA226FB81DCC44BDB35045E8"), TEXT("FBX"), TEXT("fern_02"), 6232, 3116, 6232, true},
        {FPaths::Combine(Poly, TEXT("moss_01/moss_01_1k.fbx")), VegetationAssetPath, MossName, MossObjectPath, 51484, TEXT("77DC310CBF59893B0374993B58B6AE438035331CAFCE8FEAA16757CC11797A84"), TEXT("FBX"), TEXT("moss_01"), 246170, 138, 204, true},
        {FPaths::Combine(Poly, TEXT("grass_bermuda_01/grass_bermuda_01_1k.fbx")), VegetationAssetPath, BermudaName, BermudaObjectPath, 107212, TEXT("EC188EDF028E1A76ED2E63F7483165EA765F0212B317E4B73FDC3D64610500CD"), TEXT("FBX"), TEXT("grass_bermuda_01"), 223596, 552, 941, true},
        {FPaths::Combine(Prepared, TEXT("SM_IPVExploreV3_CloseTurfCards.obj")), VegetationAssetPath, TurfCardName, TurfCardObjectPath, 3057, TEXT("D97FE4289F90B924E575D8D7C5C263021BDC447DA87141962570ED115BD1429D"), TEXT("OBJ"), TEXT(""), INDEX_NONE, INDEX_NONE, 8, false},
        {Portico, PorticoAssetPath, PorticoName, PorticoObjectPath, 1220068, PorticoObjSha, TEXT("OBJ"), TEXT(""), INDEX_NONE, INDEX_NONE, 5816, false},
        {FPaths::Combine(Geo, TEXT("terrain_low_frequency_prior.obj")), ContextAssetPath, TerrainName, TerrainObjectPath, 824147, TEXT("9FA8F5D62878521F05E55626EAE25E6C5F9E4EDBBBC9AB8AB62E41AA219F6828"), TEXT("OBJ"), TEXT(""), INDEX_NONE, INDEX_NONE, 8064, false},
        {FPaths::Combine(Geo, TEXT("osm_public_road_ribbons.obj")), ContextAssetPath, OsmRoadsName, OsmRoadsObjectPath, 1107676, TEXT("3D2F51506585EA7A47AF6D9C1ED77A3C1D7BB6B38E354BAD92AC68A6BBA31EB0"), TEXT("OBJ"), TEXT(""), INDEX_NONE, INDEX_NONE, 3972, false},
        {FPaths::Combine(Geo, TEXT("ura_indicative_road_ribbons.obj")), ContextAssetPath, UraRoadsName, UraRoadsObjectPath, 1426017, TEXT("724F47B10B3FA739CE96E234E37A0F110C547724D1F5DEAFA9C65929EE369264"), TEXT("OBJ"), TEXT(""), INDEX_NONE, INDEX_NONE, 5250, false},
        {FPaths::Combine(Geo, TEXT("osm_water_surfaces.obj")), ContextAssetPath, OsmWaterName, OsmWaterObjectPath, 58290, TEXT("D88A5A26AC72CF0522C4F9B10E6AA795CCC911F0C326A98D120088BA8C7CFD3D"), TEXT("OBJ"), TEXT(""), INDEX_NONE, INDEX_NONE, 254, false}};
    return Specs;
}

bool HasExactJsonKeys(
    const TSharedPtr<FJsonObject>& Object,
    const TArray<FString>& ExactKeys)
{
    if (!Object || Object->Values.Num() != ExactKeys.Num())
    {
        return false;
    }
    for (const FString& Key : ExactKeys)
    {
        if (!Object->Values.Contains(Key))
        {
            return false;
        }
    }
    return true;
}

bool ValidatePorticoMaterialTopologyContract(
    const TSharedPtr<FJsonObject>& IntegrationContract,
    FString& OutError)
{
    if (!IntegrationContract ||
        !IntegrationContract->HasTypedField<EJson::Object>(
            TEXT("porticoAuthorization")))
    {
        OutError = TEXT("The V3 portico authorization contract is absent.");
        return false;
    }
    const TSharedPtr<FJsonObject> Authorization =
        IntegrationContract->GetObjectField(TEXT("porticoAuthorization"));
    if (!Authorization ||
        !Authorization->HasTypedField<EJson::Object>(TEXT("materialTopology")))
    {
        OutError = TEXT("The exact V7 portico material topology is absent.");
        return false;
    }
    const TSharedPtr<FJsonObject> Topology =
        Authorization->GetObjectField(TEXT("materialTopology"));
    const TArray<FString> ExactKeys = {
        TEXT("sourceManifestMaterialSlotsMeaning"),
        TEXT("sourceMtlDeclaredDefinitionCount"),
        TEXT("sourceMtlDeclaredDefinitions"),
        TEXT("objFaceAssignedMaterialCount"),
        TEXT("objFaceAssignedMaterials"),
        TEXT("objTriangleFaceTotal"),
        TEXT("ue55ImportedSlotCount"),
        TEXT("ue55ImportedSlotOrder"),
        TEXT("ue55Lod0SectionCount"),
        TEXT("standaloneUnusedDefinitionCount"),
        TEXT("standaloneUnusedDefinitions"),
        TEXT("zeroFaceDefinitionsFabricatedAsSlots"),
        TEXT("allEightStandaloneMaterialAssetsRequired")};
    if (!HasExactJsonKeys(Topology, ExactKeys) ||
        !Topology->HasTypedField<EJson::String>(
            TEXT("sourceManifestMaterialSlotsMeaning")) ||
        !Topology->HasTypedField<EJson::Number>(
            TEXT("sourceMtlDeclaredDefinitionCount")) ||
        !Topology->HasTypedField<EJson::Array>(
            TEXT("sourceMtlDeclaredDefinitions")) ||
        !Topology->HasTypedField<EJson::Number>(
            TEXT("objFaceAssignedMaterialCount")) ||
        !Topology->HasTypedField<EJson::Array>(
            TEXT("objFaceAssignedMaterials")) ||
        !Topology->HasTypedField<EJson::Number>(
            TEXT("objTriangleFaceTotal")) ||
        !Topology->HasTypedField<EJson::Number>(
            TEXT("ue55ImportedSlotCount")) ||
        !Topology->HasTypedField<EJson::Array>(
            TEXT("ue55ImportedSlotOrder")) ||
        !Topology->HasTypedField<EJson::Number>(
            TEXT("ue55Lod0SectionCount")) ||
        !Topology->HasTypedField<EJson::Number>(
            TEXT("standaloneUnusedDefinitionCount")) ||
        !Topology->HasTypedField<EJson::Array>(
            TEXT("standaloneUnusedDefinitions")) ||
        !Topology->HasTypedField<EJson::Boolean>(
            TEXT("zeroFaceDefinitionsFabricatedAsSlots")) ||
        !Topology->HasTypedField<EJson::Boolean>(
            TEXT("allEightStandaloneMaterialAssetsRequired")))
    {
        OutError = TEXT("The V7 portico material-topology field roster or types changed.");
        return false;
    }

    const auto ExactNameArray = [](
        const TArray<TSharedPtr<FJsonValue>>& Values,
        const TArray<FName>& Expected) -> bool
    {
        if (Values.Num() != Expected.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < Expected.Num(); ++Index)
        {
            FString Actual;
            if (!Values[Index].IsValid() ||
                !Values[Index]->TryGetString(Actual) ||
                Actual != Expected[Index].ToString())
            {
                return false;
            }
        }
        return true;
    };

    TArray<FName> ImportedNames;
    for (const FPorticoMaterialUseSpec& Spec :
         PorticoUe55ImportedSlotSpecs())
    {
        ImportedNames.Add(Spec.MaterialName);
    }
    if (Topology->GetStringField(
            TEXT("sourceManifestMaterialSlotsMeaning")) !=
            TEXT("MTL_DECLARATION_PALETTE_NOT_IMPORTED_SLOT_CENSUS") ||
        Topology->GetIntegerField(
            TEXT("sourceMtlDeclaredDefinitionCount")) != 8 ||
        !ExactNameArray(
            Topology->GetArrayField(TEXT("sourceMtlDeclaredDefinitions")),
            PorticoMtlDeclaredDefinitions()) ||
        Topology->GetIntegerField(
            TEXT("objFaceAssignedMaterialCount")) != 6 ||
        Topology->GetIntegerField(TEXT("objTriangleFaceTotal")) != 5816 ||
        Topology->GetIntegerField(TEXT("ue55ImportedSlotCount")) != 6 ||
        !ExactNameArray(
            Topology->GetArrayField(TEXT("ue55ImportedSlotOrder")),
            ImportedNames) ||
        Topology->GetIntegerField(TEXT("ue55Lod0SectionCount")) != 6 ||
        Topology->GetIntegerField(
            TEXT("standaloneUnusedDefinitionCount")) != 2 ||
        !ExactNameArray(
            Topology->GetArrayField(TEXT("standaloneUnusedDefinitions")),
            PorticoStandaloneUnusedDefinitions()) ||
        Topology->GetBoolField(
            TEXT("zeroFaceDefinitionsFabricatedAsSlots")) ||
        !Topology->GetBoolField(
            TEXT("allEightStandaloneMaterialAssetsRequired")))
    {
        OutError = TEXT("The exact 8-palette/6-face-assigned/6-imported/2-standalone V7 portico material semantics changed.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>& FaceRows =
        Topology->GetArrayField(TEXT("objFaceAssignedMaterials"));
    const TArray<FPorticoMaterialUseSpec>& ExpectedFaceRows =
        PorticoObjFaceAssignmentSpecs();
    int32 FaceTotal = 0;
    if (FaceRows.Num() != ExpectedFaceRows.Num())
    {
        OutError = TEXT("The V7 portico exact OBJ face-assigned material roster changed.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedFaceRows.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Row =
            FaceRows[Index].IsValid() &&
                FaceRows[Index]->Type == EJson::Object
            ? FaceRows[Index]->AsObject()
            : nullptr;
        if (!HasExactJsonKeys(
                Row,
                TArray<FString>{TEXT("material"), TEXT("triangleFaces")}) ||
            !Row->HasTypedField<EJson::String>(TEXT("material")) ||
            !Row->HasTypedField<EJson::Number>(TEXT("triangleFaces")) ||
            Row->GetStringField(TEXT("material")) !=
                ExpectedFaceRows[Index].MaterialName.ToString() ||
            Row->GetIntegerField(TEXT("triangleFaces")) !=
                ExpectedFaceRows[Index].TriangleFaces)
        {
            OutError = TEXT("The V7 portico OBJ usemtl order or per-material triangle-face census changed.");
            return false;
        }
        FaceTotal += Row->GetIntegerField(TEXT("triangleFaces"));
    }
    if (FaceTotal != 5816)
    {
        OutError = TEXT("The V7 portico OBJ material triangle-face total changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateVegetationWindPivotBoundsContract(
    const TSharedPtr<FJsonObject>& IntegrationContract,
    FString& OutError)
{
    if (!IntegrationContract ||
        !IntegrationContract->HasTypedField<EJson::Object>(
            TEXT("vegetationWindPivotBoundsContract")))
    {
        OutError = TEXT("The exact vegetation imported-bounds/wind-pivot contract is absent.");
        return false;
    }
    const TSharedPtr<FJsonObject> Contract =
        IntegrationContract->GetObjectField(
            TEXT("vegetationWindPivotBoundsContract"));
    const TArray<FString> ContractKeys = {
        TEXT("derivationAuthority"), TEXT("boundsMeasurementApi"),
        TEXT("measurementStage"),
        TEXT("windHeightMustEqualImportedUpAxisSpan"),
        TEXT("toleranceCentimeters"), TEXT("meshes"),
        TEXT("bermudaBinaryFbxTransformEvidence"),
        TEXT("bermudaRuntimePlacement")};
    if (!HasExactJsonKeys(Contract, ContractKeys) ||
        !Contract->HasTypedField<EJson::String>(TEXT("derivationAuthority")) ||
        !Contract->HasTypedField<EJson::String>(TEXT("boundsMeasurementApi")) ||
        !Contract->HasTypedField<EJson::String>(TEXT("measurementStage")) ||
        !Contract->HasTypedField<EJson::Boolean>(
            TEXT("windHeightMustEqualImportedUpAxisSpan")) ||
        !Contract->HasTypedField<EJson::Number>(
            TEXT("toleranceCentimeters")) ||
        !Contract->HasTypedField<EJson::Array>(TEXT("meshes")) ||
        !Contract->HasTypedField<EJson::Object>(
            TEXT("bermudaBinaryFbxTransformEvidence")) ||
        !Contract->HasTypedField<EJson::Object>(
            TEXT("bermudaRuntimePlacement")) ||
        Contract->GetStringField(TEXT("derivationAuthority")) !=
            TEXT("FROZEN_BINARY_FBX_ABSOLUTE_MODEL_TRANSFORMS_PLUS_UE55_SCENE_AXIS_CONVERSION") ||
        Contract->GetStringField(TEXT("boundsMeasurementApi")) !=
            TEXT("UStaticMesh::GetBounds().BoxExtent.Z * 2.0") ||
        Contract->GetStringField(TEXT("measurementStage")) !=
            TEXT("POST_SYNCHRONOUS_IMPORT_COMPILE_BEFORE_GENERATED_LODS") ||
        !Contract->GetBoolField(
            TEXT("windHeightMustEqualImportedUpAxisSpan")) ||
        !FMath::IsNearlyEqual(
            Contract->GetNumberField(TEXT("toleranceCentimeters")),
            0.05,
            0.0000001))
    {
        OutError = TEXT("The vegetation imported-bounds derivation authority or measurement policy changed.");
        return false;
    }

    const TArray<double> ExpectedSpans = {
        502.744640, 138.346186, 42.769471, 4.036798, 15.825568523};
    const TArray<TSharedPtr<FJsonValue>>& MeshRows =
        Contract->GetArrayField(TEXT("meshes"));
    if (MeshRows.Num() != ExpectedSpans.Num())
    {
        OutError = TEXT("The exact five-mesh vegetation wind-pivot bounds roster changed.");
        return false;
    }
    for (int32 Index = 0; Index < MeshRows.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Row =
            MeshRows[Index].IsValid() &&
                MeshRows[Index]->Type == EJson::Object
            ? MeshRows[Index]->AsObject()
            : nullptr;
        const FMeshImportSpec& SourceSpec = MeshImportSpecs()[Index];
        if (!HasExactJsonKeys(
                Row,
                TArray<FString>{
                    TEXT("assetName"), TEXT("sourceAssetId"),
                    TEXT("sourceSha256"),
                    TEXT("expectedImportedUpAxisSpanCentimeters"),
                    TEXT("windHeightParameterCentimeters")}) ||
            !Row->HasTypedField<EJson::String>(TEXT("assetName")) ||
            !Row->HasTypedField<EJson::String>(TEXT("sourceAssetId")) ||
            !Row->HasTypedField<EJson::String>(TEXT("sourceSha256")) ||
            !Row->HasTypedField<EJson::Number>(
                TEXT("expectedImportedUpAxisSpanCentimeters")) ||
            !Row->HasTypedField<EJson::Number>(
                TEXT("windHeightParameterCentimeters")) ||
            Row->GetStringField(TEXT("assetName")) != SourceSpec.AssetName ||
            Row->GetStringField(TEXT("sourceAssetId")) !=
                SourceSpec.SourceAssetId ||
            Row->GetStringField(TEXT("sourceSha256")) !=
                SourceSpec.SourceSha256 ||
            !FMath::IsNearlyEqual(
                Row->GetNumberField(
                    TEXT("expectedImportedUpAxisSpanCentimeters")),
                ExpectedSpans[Index],
                0.0000001) ||
            !FMath::IsNearlyEqual(
                Row->GetNumberField(
                    TEXT("windHeightParameterCentimeters")),
                ExpectedSpans[Index],
                0.000001))
        {
            OutError = TEXT("A frozen vegetation source/imported-up-axis/wind-height binding changed.");
            return false;
        }
    }

    const auto ExactNumbers = [](
        const TArray<TSharedPtr<FJsonValue>>& Values,
        const TArray<double>& Expected,
        const double Tolerance) -> bool
    {
        if (Values.Num() != Expected.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < Expected.Num(); ++Index)
        {
            double Actual = 0.0;
            if (!Values[Index].IsValid() ||
                !Values[Index]->TryGetNumber(Actual) ||
                !FMath::IsNearlyEqual(Actual, Expected[Index], Tolerance))
            {
                return false;
            }
        }
        return true;
    };

    const TSharedPtr<FJsonObject> Evidence = Contract->GetObjectField(
        TEXT("bermudaBinaryFbxTransformEvidence"));
    const TArray<FString> EvidenceKeys = {
        TEXT("binaryFbxVersion"), TEXT("upAxis"), TEXT("upAxisSign"),
        TEXT("frontAxis"), TEXT("frontAxisSign"), TEXT("coordAxis"),
        TEXT("coordAxisSign"), TEXT("unitScaleFactorCentimeters"),
        TEXT("originalUnitScaleFactorCentimeters"),
        TEXT("geometryNodeCount"), TEXT("rootModelNodeCount"),
        TEXT("geometryToModelOneToOneOoLinks"),
        TEXT("modelToModelParentLinks"), TEXT("eachModelRotationDegrees"),
        TEXT("eachModelScale3D"), TEXT("modelTranslationsVariable"),
        TEXT("modelTransformPivotDeclarationsPresent"),
        TEXT("sourceWorldBoundsCentimeters"), TEXT("ueAxisConversion"),
        TEXT("expectedUeSpanCentimeters")};
    const TSharedPtr<FJsonObject> SourceBounds = Evidence &&
        Evidence->HasTypedField<EJson::Object>(
            TEXT("sourceWorldBoundsCentimeters"))
        ? Evidence->GetObjectField(TEXT("sourceWorldBoundsCentimeters"))
        : nullptr;
    if (!HasExactJsonKeys(Evidence, EvidenceKeys) ||
        Evidence->GetIntegerField(TEXT("binaryFbxVersion")) != 7400 ||
        Evidence->GetIntegerField(TEXT("upAxis")) != 1 ||
        Evidence->GetIntegerField(TEXT("upAxisSign")) != 1 ||
        Evidence->GetIntegerField(TEXT("frontAxis")) != 2 ||
        Evidence->GetIntegerField(TEXT("frontAxisSign")) != 1 ||
        Evidence->GetIntegerField(TEXT("coordAxis")) != 0 ||
        Evidence->GetIntegerField(TEXT("coordAxisSign")) != 1 ||
        !FMath::IsNearlyEqual(
            Evidence->GetNumberField(TEXT("unitScaleFactorCentimeters")),
            1.0) ||
        !FMath::IsNearlyEqual(
            Evidence->GetNumberField(
                TEXT("originalUnitScaleFactorCentimeters")),
            1.0) ||
        Evidence->GetIntegerField(TEXT("geometryNodeCount")) != 21 ||
        Evidence->GetIntegerField(TEXT("rootModelNodeCount")) != 21 ||
        Evidence->GetIntegerField(
            TEXT("geometryToModelOneToOneOoLinks")) != 21 ||
        Evidence->GetIntegerField(TEXT("modelToModelParentLinks")) != 0 ||
        !ExactNumbers(
            Evidence->GetArrayField(TEXT("eachModelRotationDegrees")),
            TArray<double>{-90.00000933466734, 0.0, 0.0},
            0.000000001) ||
        !ExactNumbers(
            Evidence->GetArrayField(TEXT("eachModelScale3D")),
            TArray<double>{100.0, 100.0, 100.0},
            0.0000001) ||
        !Evidence->GetBoolField(TEXT("modelTranslationsVariable")) ||
        Evidence->GetBoolField(
            TEXT("modelTransformPivotDeclarationsPresent")) ||
        !HasExactJsonKeys(
            SourceBounds,
            TArray<FString>{TEXT("minimum"), TEXT("maximum"), TEXT("span")}) ||
        !ExactNumbers(
            SourceBounds->GetArrayField(TEXT("minimum")),
            TArray<double>{-75.89022308, -1.14697891, -7.01069162},
            0.00000001) ||
        !ExactNumbers(
            SourceBounds->GetArrayField(TEXT("maximum")),
            TArray<double>{92.75741302, 14.67858961, 7.20876051},
            0.00000001) ||
        !ExactNumbers(
            SourceBounds->GetArrayField(TEXT("span")),
            TArray<double>{168.64763610, 15.825568523, 14.21945213},
            0.00000001) ||
        Evidence->GetStringField(TEXT("ueAxisConversion")) !=
            TEXT("Y_UP_TO_Z_UP") ||
        !ExactNumbers(
            Evidence->GetArrayField(TEXT("expectedUeSpanCentimeters")),
            TArray<double>{168.647636, 14.219452, 15.825569},
            0.000001))
    {
        OutError = TEXT("The frozen Bermuda binary-FBX transform/axis/bounds derivation evidence changed.");
        return false;
    }

    const TSharedPtr<FJsonObject> Placement = Contract->GetObjectField(
        TEXT("bermudaRuntimePlacement"));
    if (!HasExactJsonKeys(
            Placement,
            TArray<FString>{
                TEXT("targetHeightCentimeters"), TEXT("zScaleFormula"),
                TEXT("expectedZScaleRange"),
                TEXT("everyInstanceReadbackRequired")}) ||
        !ExactNumbers(
            Placement->GetArrayField(TEXT("targetHeightCentimeters")),
            TArray<double>{3.0, 6.0},
            0.0000001) ||
        Placement->GetStringField(TEXT("zScaleFormula")) !=
            TEXT("TARGET_HEIGHT_CM / EXPECTED_IMPORTED_UP_AXIS_SPAN_CM") ||
        !ExactNumbers(
            Placement->GetArrayField(TEXT("expectedZScaleRange")),
            TArray<double>{0.189566649415, 0.379133298831},
            0.000000000001) ||
        !Placement->GetBoolField(TEXT("everyInstanceReadbackRequired")))
    {
        OutError = TEXT("The Bermuda exact 3-6 cm runtime placement/readback contract changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateUe55MeshImportCensus(
    const TSharedPtr<FJsonObject>& IntegrationContract,
    FString& OutError)
{
    if (!IntegrationContract ||
        !IntegrationContract->HasTypedField<EJson::Object>(
            TEXT("ue55MeshImportCensus")))
    {
        OutError = TEXT("The exact UE5.5 mesh-import census is absent.");
        return false;
    }
    const TSharedPtr<FJsonObject> Census =
        IntegrationContract->GetObjectField(TEXT("ue55MeshImportCensus"));
    const TArray<FString> CensusKeys = {
        TEXT("engineVersion"),
        TEXT("triangleMeasurementApi"),
        TEXT("measurementStage"),
        TEXT("catalogPolycountRole"),
        TEXT("binaryFbxPolygonCountRole"),
        TEXT("importedLod0TrianglesRole"),
        TEXT("sourceBytesAndSha256RemainAuthoritative"),
        TEXT("importer"),
        TEXT("fbxMeshes"),
        TEXT("objMeshes")};
    if (!HasExactJsonKeys(Census, CensusKeys) ||
        !Census->HasTypedField<EJson::Object>(TEXT("engineVersion")) ||
        !Census->HasTypedField<EJson::Object>(TEXT("importer")) ||
        !Census->HasTypedField<EJson::Array>(TEXT("fbxMeshes")) ||
        !Census->HasTypedField<EJson::Array>(TEXT("objMeshes")))
    {
        OutError = TEXT("The UE5.5 mesh-import census field roster changed.");
        return false;
    }

    const TSharedPtr<FJsonObject> Engine =
        Census->GetObjectField(TEXT("engineVersion"));
    const TSharedPtr<FJsonObject> Importer =
        Census->GetObjectField(TEXT("importer"));
    const TArray<FString> ImporterKeys = {
        TEXT("factoryClass"), TEXT("importUiClass"), TEXT("meshType"),
        TEXT("automatedImportShouldDetectType"), TEXT("importAsSkeletal"),
        TEXT("importMesh"), TEXT("importMaterials"), TEXT("importTextures"),
        TEXT("combineMeshes"), TEXT("importMeshLods"),
        TEXT("transformVertexToAbsolute"), TEXT("bakePivotInVertex"),
        TEXT("autoGenerateCollision"), TEXT("buildNanite"),
        TEXT("removeDegenerates"), TEXT("generateLightmapUvs"),
        TEXT("normalImportMethod"), TEXT("normalGenerationMethod"),
        TEXT("forceFrontXAxis"), TEXT("fbxConvertScene"),
        TEXT("fbxConvertSceneUnit"), TEXT("objConvertScene"),
        TEXT("objConvertSceneUnit"), TEXT("automated"),
        TEXT("replaceExisting"), TEXT("replaceExistingSettings"),
        TEXT("saveDuringImport"), TEXT("async")};
    if (!HasExactJsonKeys(
            Engine,
            TArray<FString>{TEXT("major"), TEXT("minor")}) ||
        Engine->GetIntegerField(TEXT("major")) != 5 ||
        Engine->GetIntegerField(TEXT("minor")) != 5 ||
        ENGINE_MAJOR_VERSION != 5 || ENGINE_MINOR_VERSION != 5 ||
        Census->GetStringField(TEXT("triangleMeasurementApi")) !=
            TEXT("UStaticMesh::GetNumTriangles(0)") ||
        Census->GetStringField(TEXT("measurementStage")) !=
            TEXT("POST_SYNCHRONOUS_IMPORT_COMPILE_BEFORE_GENERATED_LODS") ||
        Census->GetStringField(TEXT("catalogPolycountRole")) !=
            TEXT("UPSTREAM_POLY_HAVEN_METADATA_PROVENANCE_ONLY") ||
        Census->GetStringField(TEXT("binaryFbxPolygonCountRole")) !=
            TEXT("EXACT_FROZEN_SOURCE_TOPOLOGY_EVIDENCE") ||
        Census->GetStringField(TEXT("importedLod0TrianglesRole")) !=
            TEXT("FAIL_CLOSED_UE_RENDER_TRIANGLE_VALIDATION_AUTHORITY") ||
        !Census->GetBoolField(
            TEXT("sourceBytesAndSha256RemainAuthoritative")) ||
        !HasExactJsonKeys(Importer, ImporterKeys) ||
        Importer->GetStringField(TEXT("factoryClass")) !=
            UFbxFactory::StaticClass()->GetPathName() ||
        Importer->GetStringField(TEXT("importUiClass")) !=
            UFbxImportUI::StaticClass()->GetPathName() ||
        Importer->GetStringField(TEXT("meshType")) !=
            TEXT("FBXIT_StaticMesh") ||
        Importer->GetBoolField(TEXT("automatedImportShouldDetectType")) ||
        Importer->GetBoolField(TEXT("importAsSkeletal")) ||
        !Importer->GetBoolField(TEXT("importMesh")) ||
        Importer->GetBoolField(TEXT("importMaterials")) ||
        Importer->GetBoolField(TEXT("importTextures")) ||
        !Importer->GetBoolField(TEXT("combineMeshes")) ||
        Importer->GetBoolField(TEXT("importMeshLods")) ||
        !Importer->GetBoolField(TEXT("transformVertexToAbsolute")) ||
        Importer->GetBoolField(TEXT("bakePivotInVertex")) ||
        Importer->GetBoolField(TEXT("autoGenerateCollision")) ||
        Importer->GetBoolField(TEXT("buildNanite")) ||
        !Importer->GetBoolField(TEXT("removeDegenerates")) ||
        !Importer->GetBoolField(TEXT("generateLightmapUvs")) ||
        Importer->GetStringField(TEXT("normalImportMethod")) !=
            TEXT("FBXNIM_ImportNormals") ||
        Importer->GetStringField(TEXT("normalGenerationMethod")) !=
            TEXT("MikkTSpace") ||
        Importer->GetBoolField(TEXT("forceFrontXAxis")) ||
        !Importer->GetBoolField(TEXT("fbxConvertScene")) ||
        !Importer->GetBoolField(TEXT("fbxConvertSceneUnit")) ||
        Importer->GetBoolField(TEXT("objConvertScene")) ||
        Importer->GetBoolField(TEXT("objConvertSceneUnit")) ||
        !Importer->GetBoolField(TEXT("automated")) ||
        Importer->GetBoolField(TEXT("replaceExisting")) ||
        Importer->GetBoolField(TEXT("replaceExistingSettings")) ||
        Importer->GetBoolField(TEXT("saveDuringImport")) ||
        Importer->GetBoolField(TEXT("async")))
    {
        OutError = TEXT("The UE5.5 engine/importer/triangle-authority semantics changed.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>& FbxRows =
        Census->GetArrayField(TEXT("fbxMeshes"));
    const TArray<TSharedPtr<FJsonValue>>& ObjRows =
        Census->GetArrayField(TEXT("objMeshes"));
    if (FbxRows.Num() != 5 || ObjRows.Num() != 6 ||
        MeshImportSpecs().Num() != 11)
    {
        OutError = TEXT("The exact five-FBX/six-OBJ UE5.5 census roster changed.");
        return false;
    }

    TSet<FString> SeenAssets;
    auto ValidateRows = [&SeenAssets](
        const TArray<TSharedPtr<FJsonValue>>& Rows,
        const FString& ExpectedFormat) -> bool
    {
        const bool bFbx = ExpectedFormat == TEXT("FBX");
        const TArray<FString> RowKeys = bFbx
            ? TArray<FString>{
                TEXT("assetName"), TEXT("sourceAssetId"),
                TEXT("catalogPolycount"), TEXT("binaryFbxPolygonCount"),
                TEXT("importedLod0Triangles")}
            : TArray<FString>{
                TEXT("assetName"), TEXT("importedLod0Triangles")};
        for (const TSharedPtr<FJsonValue>& Value : Rows)
        {
            const TSharedPtr<FJsonObject> Row = Value.IsValid()
                ? Value->AsObject()
                : nullptr;
            if (!HasExactJsonKeys(Row, RowKeys))
            {
                return false;
            }
            const FString AssetName = Row->GetStringField(TEXT("assetName"));
            const FMeshImportSpec* Spec = MeshImportSpecs().FindByPredicate(
                [&AssetName, &ExpectedFormat](const FMeshImportSpec& Candidate)
                {
                    return Candidate.AssetName == AssetName &&
                        Candidate.SourceFormat == ExpectedFormat;
                });
            if (!Spec || SeenAssets.Contains(AssetName) ||
                Row->GetIntegerField(TEXT("importedLod0Triangles")) !=
                    Spec->ExpectedImportedLod0Triangles)
            {
                return false;
            }
            if (bFbx &&
                (Row->GetStringField(TEXT("sourceAssetId")) !=
                     Spec->SourceAssetId ||
                 Row->GetIntegerField(TEXT("catalogPolycount")) !=
                     Spec->CatalogPolycount ||
                 Row->GetIntegerField(TEXT("binaryFbxPolygonCount")) !=
                     Spec->BinaryFbxPolygonCount))
            {
                return false;
            }
            SeenAssets.Add(AssetName);
        }
        return true;
    };
    if (!ValidateRows(FbxRows, TEXT("FBX")) ||
        !ValidateRows(ObjRows, TEXT("OBJ")) ||
        SeenAssets.Num() != MeshImportSpecs().Num())
    {
        OutError = TEXT("The catalog/source-topology/imported-triangle census changed or substituted authorities.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMeshImportTaskPolicy(
    const UAssetImportTask* Task,
    const FMeshImportSpec& Spec,
    FString& OutError)
{
    const UFbxFactory* Factory = Task
        ? Cast<UFbxFactory>(Task->Factory)
        : nullptr;
    const UFbxImportUI* Options = Task
        ? Cast<UFbxImportUI>(Task->Options)
        : nullptr;
    const UFbxStaticMeshImportData* StaticMeshData = Options
        ? Options->StaticMeshImportData
        : nullptr;
    if (!Task || !Factory || !Options || !StaticMeshData ||
        Factory->ImportUI != Options ||
        Options->bImportAsSkeletal ||
        Options->MeshTypeToImport != FBXIT_StaticMesh ||
        Options->bAutomatedImportShouldDetectType ||
        !Options->bImportMesh || Options->bImportMaterials ||
        Options->bImportTextures ||
        StaticMeshData->bConvertScene != Spec.bConvertSceneUnit ||
        StaticMeshData->bConvertSceneUnit != Spec.bConvertSceneUnit ||
        StaticMeshData->bForceFrontXAxis ||
        !StaticMeshData->bCombineMeshes ||
        StaticMeshData->bImportMeshLODs ||
        !StaticMeshData->bTransformVertexToAbsolute ||
        StaticMeshData->bBakePivotInVertex ||
        StaticMeshData->bAutoGenerateCollision ||
        !StaticMeshData->bGenerateLightmapUVs ||
        StaticMeshData->NormalImportMethod !=
            EFBXNormalImportMethod::FBXNIM_ImportNormals ||
        StaticMeshData->NormalGenerationMethod !=
            EFBXNormalGenerationMethod::MikkTSpace ||
        StaticMeshData->bBuildNanite ||
        !StaticMeshData->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The in-memory UE5.5 mesh import task diverged from the exact census importer policy: ") +
            Spec.AssetName;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMeshSourceAndCollision(
    UStaticMesh* Mesh,
    const FMeshImportSpec& Spec,
    FString& OutError)
{
    UAssetImportData* ImportData = Mesh ? Mesh->GetAssetImportData() : nullptr;
    TArray<FString> Sources = ImportData
        ? ImportData->ExtractFilenames()
        : TArray<FString>();
    FString ActualSource = Sources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(Sources[0])
        : FString();
    FString ExpectedSource = FPaths::ConvertRelativePathToFull(Spec.SourcePath);
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(ExpectedSource);
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    const int32 ActualImportedLod0Triangles = CountTriangles(Mesh, 0);
    if (!Mesh || Mesh->GetPathName() != Spec.ExactObjectPath ||
        Sources.Num() != 1 || !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        !ValidateExactFile(ExpectedSource, Spec.SourceBytes, Spec.SourceSha256, OutError) ||
        Mesh->NaniteSettings.bEnabled ||
        (Body && (Body->AggGeom.GetElementCount() != 0 ||
                  Body->CollisionTraceFlag == CTF_UseComplexAsSimple)))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A V3 visual mesh source/triangle/no-collision contract changed: ") +
                Spec.AssetName;
        }
        return false;
    }
    if (ActualImportedLod0Triangles !=
        Spec.ExpectedImportedLod0Triangles)
    {
        OutError = FString::Printf(
            TEXT("V3 mesh LOD0 triangle census changed for %s: expected %d from the UE5.5 contract, actual %d."),
            *Spec.AssetName,
            Spec.ExpectedImportedLod0Triangles,
            ActualImportedLod0Triangles);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMeshLodCaps(UStaticMesh* Mesh, FString& OutError)
{
    if (!Mesh)
    {
        OutError = TEXT("A V3 mesh is absent.");
        return false;
    }
    struct FCap { int32 Lod; int32 Cap; };
    TArray<FCap> Caps;
    int32 RequiredMinLod = 0;
    if (Mesh->GetPathName() == IslandTreeObjectPath)
    {
        Caps = {{1, 150000}, {2, 60000}, {3, 20000}};
        RequiredMinLod = 1;
    }
    else if (Mesh->GetPathName() == MossObjectPath)
    {
        Caps = {{1, 20000}, {2, 6000}};
        RequiredMinLod = 1;
    }
    else if (Mesh->GetPathName() == BermudaObjectPath)
    {
        Caps = {{1, 12000}, {2, 3000}};
        RequiredMinLod = 1;
    }
    else if (Mesh->GetPathName() == ShrubObjectPath)
    {
        Caps = {{1, 16000}, {2, 5000}};
    }
    else if (Mesh->GetPathName() == FernObjectPath)
    {
        Caps = {{1, 2500}, {2, 700}};
    }
    if (Mesh->GetMinLODIdx() != RequiredMinLod)
    {
        OutError = TEXT("A V3 source-heavy mesh runtime MinLOD changed.");
        return false;
    }
    int32 Previous = CountTriangles(Mesh, 0);
    for (const FCap& Cap : Caps)
    {
        const int32 Current = CountTriangles(Mesh, Cap.Lod);
        if (Current <= 0 || Current > Cap.Cap || Current >= Previous)
        {
            OutError = TEXT("A V3 generated mesh LOD exceeds its bounded triangle cap.");
            return false;
        }
        Previous = Current;
    }
    OutError.Reset();
    return true;
}

bool MeshImportedSlotHasMaterial(
    const UStaticMesh* Mesh,
    const FName SlotName,
    const FString& ExactMaterialPath)
{
    int32 Slot = Mesh
        ? Mesh->GetMaterialIndexFromImportedMaterialSlotName(SlotName)
        : INDEX_NONE;
    if (Slot == INDEX_NONE && Mesh)
    {
        Slot = Mesh->GetMaterialIndex(SlotName);
    }
    const UMaterialInterface* Material = Slot != INDEX_NONE
        ? Mesh->GetMaterial(Slot)
        : nullptr;
    return Material && Material->GetPathName() == ExactMaterialPath;
}

bool MeshAllSlotsHaveMaterial(
    const UStaticMesh* Mesh,
    const FString& ExactMaterialPath)
{
    if (!Mesh || Mesh->GetStaticMaterials().IsEmpty())
    {
        return false;
    }
    for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
    {
        const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
        if (!Material || Material->GetPathName() != ExactMaterialPath)
        {
            return false;
        }
    }
    return true;
}

bool ValidateAllV3AssetsInternal(FString& OutError)
{
    TArray<FTextureSourceSpec> TextureSpecs;
    if (!ValidateFrozenSourceSet(TextureSpecs, OutError))
    {
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
    FAssetCompilingManager::Get().FinishAllCompilation();
    for (const FMaterialSpec& Spec : VegetationMaterialSpecs())
    {
        if (!ValidateVegetationMaterial(
                LoadExact<UMaterial>(MaterialObjectPath(Spec.AssetName)),
                Spec,
                OutError))
        {
            return false;
        }
    }
    UMaterial* FormalLawn = LoadExact<UMaterial>(FormalLawnMaterialPath);
    if (!ValidateDarkColumnarSourceTextures(nullptr, OutError) ||
        !ValidateDarkColumnarLeafMaterial(
            LoadExact<UMaterial>(DarkColumnarMaterialPath), OutError) ||
        !ValidateFormalLawnMaterial(FormalLawn, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact V3 dark-columnar or lawn material is absent.");
        }
        return false;
    }
    for (const FSimpleMaterialSpec& Spec : SimpleMaterialSpecs())
    {
        if (!ValidateSimpleMaterial(
                LoadExact<UMaterial>(MaterialObjectPath(Spec.Name)),
                Spec.Name,
                Spec.Color,
                Spec.Roughness,
                Spec.Metallic,
                OutError))
        {
            return false;
        }
    }
    TArray<UStaticMesh*> CompiledMeshes;
    for (const FMeshImportSpec& Spec : MeshImportSpecs())
    {
        UStaticMesh* Mesh = LoadExact<UStaticMesh>(Spec.ExactObjectPath);
        if (!Mesh)
        {
            OutError = TEXT("A V3 imported visual mesh is absent: ") + Spec.AssetName;
            return false;
        }
        CompiledMeshes.Add(Mesh);
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(CompiledMeshes);
    for (int32 Index = 0; Index < MeshImportSpecs().Num(); ++Index)
    {
        UStaticMesh* Mesh = CompiledMeshes[Index];
        if (Mesh->IsCompiling() ||
            !ValidateMeshSourceAndCollision(Mesh, MeshImportSpecs()[Index], OutError) ||
            !ValidateMeshLodCaps(Mesh, OutError))
        {
            return false;
        }
    }
    UStaticMesh* Island = LoadExact<UStaticMesh>(IslandTreeObjectPath);
    UStaticMesh* Shrub = LoadExact<UStaticMesh>(ShrubObjectPath);
    UStaticMesh* Fern = LoadExact<UStaticMesh>(FernObjectPath);
    UStaticMesh* Moss = LoadExact<UStaticMesh>(MossObjectPath);
    UStaticMesh* Bermuda = LoadExact<UStaticMesh>(BermudaObjectPath);
    UStaticMesh* Turf = LoadExact<UStaticMesh>(TurfCardObjectPath);
    UStaticMesh* Portico = LoadExact<UStaticMesh>(PorticoObjectPath);
    struct FWindPivotBinding
    {
        UStaticMesh* Mesh;
        float ExpectedImportedUpAxisSpanCm;
        TArray<FString> MaterialNames;
    };
    const TArray<FWindPivotBinding> WindPivotBindings = {
        {Island, IslandImportedUpAxisSpanCm,
            {TEXT("M_IPVExploreV3_IslandTree01_Trunk_Wind"),
             TEXT("M_IPVExploreV3_IslandTree01_Branches_Wind"),
             TEXT("M_IPVExploreV3_IslandTree01_Leaves_Wind")}},
        {Shrub, ShrubImportedUpAxisSpanCm,
            {TEXT("M_IPVExploreV3_Shrub02_Wind")}},
        {Fern, FernImportedUpAxisSpanCm,
            {TEXT("M_IPVExploreV3_Fern02_Wind")}},
        {Moss, MossImportedUpAxisSpanCm,
            {TEXT("M_IPVExploreV3_Moss01_Wind")}},
        {Bermuda, BermudaImportedUpAxisSpanCm,
            {TEXT("M_IPVExploreV3_BermudaGrass_Wind")}}};
    for (const FWindPivotBinding& Binding : WindPivotBindings)
    {
        const double ActualImportedUpAxisSpanCm = Binding.Mesh
            ? Binding.Mesh->GetBounds().BoxExtent.Z * 2.0
            : 0.0;
        if (!Binding.Mesh || !FMath::IsNearlyEqual(
                ActualImportedUpAxisSpanCm,
                static_cast<double>(Binding.ExpectedImportedUpAxisSpanCm),
                0.05))
        {
            OutError = FString::Printf(
                TEXT("A frozen vegetation UE5.5 imported up-axis span changed: mesh=%s expected=%.9fcm actual=%.9fcm."),
                Binding.Mesh ? *Binding.Mesh->GetPathName() : TEXT("<null>"),
                static_cast<double>(Binding.ExpectedImportedUpAxisSpanCm),
                ActualImportedUpAxisSpanCm);
            return false;
        }
        for (const FString& MaterialName : Binding.MaterialNames)
        {
            const FMaterialSpec* WindSpec =
                VegetationMaterialSpecs().FindByPredicate(
                    [&MaterialName](const FMaterialSpec& Spec)
                    {
                        return Spec.AssetName == MaterialName;
                    });
            if (!WindSpec || !FMath::IsNearlyEqual(
                    WindSpec->HeightCm,
                    Binding.ExpectedImportedUpAxisSpanCm,
                    0.0001f))
            {
                OutError = TEXT("A vegetation wind HeightCm no longer equals its exact imported up-axis span: ") + MaterialName;
                return false;
            }
        }
    }
    const FMaterialSpec* MossWindSpec =
        VegetationMaterialSpecs().FindByPredicate(
            [](const FMaterialSpec& Spec)
            {
                return Spec.AssetName == TEXT("M_IPVExploreV3_Moss01_Wind");
            });
    const FMaterialSpec* BermudaWindSpec =
        VegetationMaterialSpecs().FindByPredicate(
            [](const FMaterialSpec& Spec)
            {
                return Spec.AssetName == TEXT("M_IPVExploreV3_BermudaGrass_Wind");
            });
    if (!MossWindSpec || MossWindSpec->MaximumWpoCm > 0.8f ||
        !BermudaWindSpec || BermudaWindSpec->MaximumWpoCm > 1.2f)
    {
        OutError = TEXT("Moss/Bermuda wind WPO caps must remain <=0.8/1.2 cm after exact imported pivot-height calibration.");
        return false;
    }
    FString PorticoTopologyError;
    const bool bPorticoMaterialsExact = ValidatePorticoMeshMaterialTopology(
        Portico, true, PorticoTopologyError);
    if (!MeshImportedSlotHasMaterial(
            Island, TEXT("island_tree_01"),
            MaterialObjectPath(TEXT("M_IPVExploreV3_IslandTree01_Trunk_Wind"))) ||
        !MeshImportedSlotHasMaterial(
            Island, TEXT("island_tree_01_branches"),
            MaterialObjectPath(TEXT("M_IPVExploreV3_IslandTree01_Branches_Wind"))) ||
        !MeshImportedSlotHasMaterial(
            Island, TEXT("island_tree_01_leaves"),
            MaterialObjectPath(TEXT("M_IPVExploreV3_IslandTree01_Leaves_Wind"))) ||
        !MeshAllSlotsHaveMaterial(
            Shrub, MaterialObjectPath(TEXT("M_IPVExploreV3_Shrub02_Wind"))) ||
        !MeshAllSlotsHaveMaterial(
            Fern, MaterialObjectPath(TEXT("M_IPVExploreV3_Fern02_Wind"))) ||
        !MeshAllSlotsHaveMaterial(
            Moss, MaterialObjectPath(TEXT("M_IPVExploreV3_Moss01_Wind"))) ||
        !MeshAllSlotsHaveMaterial(
            Bermuda, MaterialObjectPath(TEXT("M_IPVExploreV3_BermudaGrass_Wind"))) ||
        !MeshAllSlotsHaveMaterial(
            Turf, MaterialObjectPath(TEXT("M_IPVExploreV3_CloseTurf_Wind"))) ||
        !MeshAllSlotsHaveMaterial(
            LoadExact<UStaticMesh>(TerrainObjectPath), FormalLawnMaterialPath) ||
        !MeshAllSlotsHaveMaterial(
            LoadExact<UStaticMesh>(OsmRoadsObjectPath),
            MaterialObjectPath(TEXT("M_IPVExploreV3_ContextRoad"))) ||
        !MeshAllSlotsHaveMaterial(
            LoadExact<UStaticMesh>(UraRoadsObjectPath),
            MaterialObjectPath(TEXT("M_IPVExploreV3_ContextRoad"))) ||
        !MeshAllSlotsHaveMaterial(
            LoadExact<UStaticMesh>(OsmWaterObjectPath),
            MaterialObjectPath(TEXT("M_IPVExploreV3_ContextWater"))) ||
        !bPorticoMaterialsExact)
    {
        OutError = bPorticoMaterialsExact
            ? TEXT("A V3 mesh lost its exact vegetation/lawn/context material-slot binding.")
            : PorticoTopologyError;
        return false;
    }
    if (CompiledMeshes.Num() != 11 || TextureSpecs.Num() != 36)
    {
        OutError = TEXT("The exact 67-asset V3 roster changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

int32 CountExistingV3Assets()
{
    TArray<FAssetData> Assets;
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    RegistryModule.Get().GetAssetsByPath(
        FName(*AssetRoot), Assets, true, false);
    return Assets.Num();
}

bool ImportAllMeshes(
    IAssetTools& AssetTools,
    const TMap<FString, UMaterial*>& Materials,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    TArray<UAssetImportTask*> Tasks;
    TMap<FString, UAssetImportTask*> TasksByName;
    for (const FMeshImportSpec& Spec : MeshImportSpecs())
    {
        if (!ValidateExactFile(
                Spec.SourcePath, Spec.SourceBytes, Spec.SourceSha256, OutError))
        {
            return false;
        }
        UAssetImportTask* Task = MakeMeshImportTask(
            Spec.SourcePath,
            Spec.DestinationPath,
            Spec.AssetName,
            Spec.bConvertSceneUnit);
        if (!Task || TasksByName.Contains(Spec.AssetName))
        {
            OutError = TEXT("Could not allocate the exact unique V3 mesh import roster.");
            return false;
        }
        if (!ValidateMeshImportTaskPolicy(Task, Spec, OutError))
        {
            return false;
        }
        Tasks.Add(Task);
        TasksByName.Add(Spec.AssetName, Task);
    }
    if (Tasks.Num() != 11)
    {
        OutError = TEXT("V3 import requires exactly eleven source-bound visual meshes.");
        return false;
    }
    AssetTools.ImportAssetTasks(Tasks);
    TMap<FString, UStaticMesh*> Meshes;
    for (const FMeshImportSpec& Spec : MeshImportSpecs())
    {
        UStaticMesh* Mesh = ResolveImportedMesh(
            TasksByName.FindRef(Spec.AssetName), Spec.ExactObjectPath);
        if (!Mesh)
        {
            OutError = TEXT("Mesh import produced no exact V3 object: ") + Spec.AssetName;
            return false;
        }
        RemoveVisualMeshCollision(Mesh);
        Meshes.Add(Spec.AssetName, Mesh);
        OutAssetsToSave.Add(Mesh);
    }
    TArray<UStaticMesh*> CompileMeshes;
    Meshes.GenerateValueArray(CompileMeshes);
    FStaticMeshCompilingManager::Get().FinishCompilation(CompileMeshes);
    for (const FMeshImportSpec& Spec : MeshImportSpecs())
    {
        UStaticMesh* Mesh = Meshes.FindRef(Spec.AssetName);
        const int32 ActualImportedLod0Triangles = CountTriangles(Mesh, 0);
        if (!Mesh ||
            ActualImportedLod0Triangles !=
                Spec.ExpectedImportedLod0Triangles)
        {
            OutError = FString::Printf(
                TEXT("Imported V3 mesh LOD0 triangle census changed for %s: expected %d from the UE5.5 contract, actual %d."),
                *Spec.AssetName,
                Spec.ExpectedImportedLod0Triangles,
                ActualImportedLod0Triangles);
            return false;
        }
    }

    if (!ConfigureRuntimeLods(Meshes.FindRef(IslandTreeName),
            {{0.04f, 150000, 0.65f}, {0.015f, 60000, 0.25f}, {0.005f, 20000, 0.08f}}, 1, OutError) ||
        !ConfigureRuntimeLods(Meshes.FindRef(ShrubName),
            {{0.30f, 16000, 0.45f}, {0.09f, 5000, 0.12f}}, 0, OutError) ||
        !ConfigureRuntimeLods(Meshes.FindRef(FernName),
            {{0.38f, 2500, 0.40f}, {0.10f, 700, 0.10f}}, 0, OutError) ||
        !ConfigureRuntimeLods(Meshes.FindRef(MossName),
            {{0.08f, 20000, 0.55f}, {0.024f, 6000, 0.16f}}, 1, OutError) ||
        !ConfigureRuntimeLods(Meshes.FindRef(BermudaName),
            {{0.05f, 12000, 0.50f}, {0.012f, 3000, 0.14f}}, 1, OutError))
    {
        return false;
    }

    if (!BindIslandTreeMaterials(Meshes.FindRef(IslandTreeName), Materials, OutError) ||
        !BindSingleMaterial(Meshes.FindRef(ShrubName), Materials.FindRef(TEXT("M_IPVExploreV3_Shrub02_Wind")), TEXT("shrub_02"), OutError) ||
        !BindSingleMaterial(Meshes.FindRef(FernName), Materials.FindRef(TEXT("M_IPVExploreV3_Fern02_Wind")), TEXT("fern_02"), OutError) ||
        !BindSingleMaterial(Meshes.FindRef(MossName), Materials.FindRef(TEXT("M_IPVExploreV3_Moss01_Wind")), TEXT("moss_01"), OutError) ||
        !BindSingleMaterial(Meshes.FindRef(BermudaName), Materials.FindRef(TEXT("M_IPVExploreV3_BermudaGrass_Wind")), TEXT("grass_bermuda_01"), OutError) ||
        !BindSingleMaterial(Meshes.FindRef(TurfCardName), Materials.FindRef(TEXT("M_IPVExploreV3_CloseTurf_Wind")), TEXT("M_IPVExploreV3_CloseTurf"), OutError) ||
        !BindPorticoMaterials(Meshes.FindRef(PorticoName), Materials, OutError) ||
        !BindSingleMaterial(Meshes.FindRef(TerrainName), Materials.FindRef(TEXT("M_IPVExploreV3_FormalLawn")), TEXT("M_IPVExploreV3_FormalLawn"), OutError) ||
        !BindSingleMaterial(Meshes.FindRef(OsmRoadsName), Materials.FindRef(TEXT("M_IPVExploreV3_ContextRoad")), TEXT("M_IPVExploreV3_ContextRoad"), OutError) ||
        !BindSingleMaterial(Meshes.FindRef(UraRoadsName), Materials.FindRef(TEXT("M_IPVExploreV3_ContextRoad")), TEXT("M_IPVExploreV3_ContextRoad"), OutError) ||
        !BindSingleMaterial(Meshes.FindRef(OsmWaterName), Materials.FindRef(TEXT("M_IPVExploreV3_ContextWater")), TEXT("M_IPVExploreV3_ContextWater"), OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

ATRIADIstanaPublicViewSceneActor* FindScene(UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }
    ATRIADIstanaPublicViewSceneActor* Result = nullptr;
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
    if (!World)
    {
        return nullptr;
    }
    ATRIADIstanaPublicViewRuntimePolicyActor* Result = nullptr;
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
    if (!World)
    {
        return nullptr;
    }
    ATRIADIstanaExploreV2LandscapeActor* Result = nullptr;
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
    if (!World)
    {
        return nullptr;
    }
    ATRIADIstanaExploreV3SupplementActor* Result = nullptr;
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

bool CaptureComponentState(
    UStaticMeshComponent* Component,
    FComponentState& OutState,
    FString& OutError)
{
    if (!Component || !Component->GetStaticMesh())
    {
        OutError = TEXT("A required inherited V2 static-mesh component is absent.");
        return false;
    }
    OutState = FComponentState();
    OutState.MeshPath = Component->GetStaticMesh()->GetPathName();
    for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
    {
        const UMaterialInterface* Material = Component->GetMaterial(Slot);
        if (!Material)
        {
            OutError = TEXT("An inherited V2 component has a null material slot.");
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
        for (int32 Instance = 0; Instance < Hism->GetInstanceCount(); ++Instance)
        {
            FTransform Transform;
            if (!Hism->GetInstanceTransform(Instance, Transform, true))
            {
                OutError = TEXT("Could not snapshot an inherited V2 HISM transform.");
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
    bool bAllowExactHiddenVisibilityDelta,
    FString& OutError)
{
    FComponentState Current;
    if (!CaptureComponentState(Component, Current, OutError))
    {
        return false;
    }
    const bool bVisibilityExact = bAllowExactHiddenVisibilityDelta
        ? Expected.bVisible && !Expected.bHiddenInGame &&
            !Current.bVisible && Current.bHiddenInGame
        : Current.bVisible == Expected.bVisible &&
            Current.bHiddenInGame == Expected.bHiddenInGame;
    if (Current.MeshPath != Expected.MeshPath ||
        Current.MaterialPaths != Expected.MaterialPaths ||
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
        OutError = TEXT("An inherited V2 component changed outside the approved target-only columnar/near-turf/meadow visibility deltas.");
        return false;
    }
    for (int32 Index = 0; Index < Current.InstanceWorldTransforms.Num(); ++Index)
    {
        if (!Current.InstanceWorldTransforms[Index].Equals(
                Expected.InstanceWorldTransforms[Index], 0.001f))
        {
            OutError = TEXT("An inherited V2 instance world transform changed.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool CaptureV2WorldSnapshot(
    UWorld* World,
    FV2WorldSnapshot& OutSnapshot,
    FString& OutError)
{
    ATRIADIstanaExploreV2LandscapeActor* Landscape = FindV2Landscape(World);
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(World);
    if (!World || !Landscape || !Scene || !World->GetWorldSettings() ||
        !World->GetWorldSettings()->DefaultGameMode)
    {
        OutError = TEXT("The exact V2 world/landscape/scene/game mode is absent.");
        return false;
    }
    OutSnapshot = FV2WorldSnapshot();
    OutSnapshot.LandscapeTransform = Landscape->GetActorTransform();
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         V2Components(Landscape))
    {
        FComponentState State;
        if (!CaptureComponentState(Component, State, OutError))
        {
            return false;
        }
        OutSnapshot.LandscapeComponents.Add(MoveTemp(State));
    }
    if (!CaptureComponentState(
            Scene->TerrainComponent.Get(),
            OutSnapshot.Terrain,
            OutError) ||
        !CaptureComponentState(
            Scene->OSMContextBuildingsComponent.Get(),
            OutSnapshot.DistantContext,
            OutError))
    {
        return false;
    }
    OutSnapshot.GameModeClassPath =
        World->GetWorldSettings()->DefaultGameMode.Get()->GetPathName();
    OutSnapshot.BaseWindStrengthCm = Landscape->BaseWindStrengthCm;
    OutSnapshot.GustPeakStrengthCm = Landscape->GustPeakStrengthCm;
    OutSnapshot.WindSpeed = Landscape->WindSpeed;
    OutSnapshot.PrevailingWindDirection = Landscape->PrevailingWindDirection;
    OutSnapshot.RecoveryFrequencyHz = Landscape->RecoveryFrequencyHz;
    OutSnapshot.RecoveryDampingRatio = Landscape->RecoveryDampingRatio;
    for (const UMaterialInterface* Material : {
             Landscape->TreeTrunkWindMaterial.Get(),
             Landscape->TreeBranchWindMaterial.Get(),
             Landscape->TreeLeafWindMaterial.Get(),
             Landscape->GrassWindMaterial.Get()})
    {
        if (!Material)
        {
            OutError = TEXT("An inherited V2 saved wind base material is absent.");
            return false;
        }
        OutSnapshot.WindMaterialPaths.Add(Material->GetPathName());
    }
    OutSnapshot.WindMaterialSlots = {
        Landscape->TreeTrunkWindSlotIndex,
        Landscape->TreeBranchWindSlotIndex,
        Landscape->TreeLeafWindSlotIndex,
        Landscape->GrassWindSlotIndex};
    OutSnapshot.bWindMaterialsConfigured = Landscape->bWindMaterialsConfigured;
    OutError.Reset();
    return true;
}

bool ValidateExactV2SourceWorld(UWorld* World, FString& OutError)
{
    ATRIADIstanaExploreV2LandscapeActor* Landscape = FindV2Landscape(World);
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(World);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(World);
    FString LandscapeReport;
    FString SceneReport;
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != SourceMapPackage ||
        !Landscape || !Scene || !Policy || Policy->bEnforceFixedPrimaryCamera ||
        !Scene->ValidatePublicViewScene(SceneReport, false) ||
        !Landscape->ValidateExploreV2Landscape(LandscapeReport))
    {
        OutError = TEXT("Exact Explore V2 source validation failed. ") +
            SceneReport + TEXT(" ") + LandscapeReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateTargetV2AgainstSnapshot(
    UWorld* TargetWorld,
    const FV2WorldSnapshot& SourceSnapshot,
    FString& OutError)
{
    ATRIADIstanaExploreV2LandscapeActor* Target = FindV2Landscape(TargetWorld);
    TArray<UHierarchicalInstancedStaticMeshComponent*> Components =
        V2Components(Target);
    if (!Target || Components.Num() != SourceSnapshot.LandscapeComponents.Num() ||
        !Target->GetActorTransform().Equals(
            SourceSnapshot.LandscapeTransform, 0.001f))
    {
        OutError = TEXT("The duplicated V2 landscape actor/component roster changed.");
        return false;
    }
    for (int32 Index = 0; Index < Components.Num(); ++Index)
    {
        const bool bApprovedTargetVisibilityDelta =
            Index == 1 || Index == 8 || Index == 9;
        if (!MatchesComponentState(
                Components[Index],
                SourceSnapshot.LandscapeComponents[Index],
                bApprovedTargetVisibilityDelta,
                OutError))
        {
            return false;
        }
    }
    if (Components[1]->GetInstanceCount() != 232 ||
        Components[10]->GetInstanceCount() != 720 ||
        Target->ClaimLabel !=
            TEXT("ISTANA_PUBLIC_REFERENCE_EXPLORE_V2_QUALITATIVE_COMPOSITION_NOT_BOTANICAL_INVENTORY_NOT_SURVEY_CONTROLLED_NOT_ONE_TO_ONE_1KM_REPLICA") ||
        Target->bExactSpeciesOrCultivarsClaimed ||
        Target->bExactIndividualTreePlacementClaimed ||
        Target->bOneToOneKilometerReplicaClaimed ||
        Target->bVegetationUsedForSensorTruth ||
        Target->DeterministicPlacementSeed != 0x2757A6A5 ||
        !FMath::IsNearlyEqual(
            Target->BaseWindStrengthCm,
            SourceSnapshot.BaseWindStrengthCm,
            0.001f) ||
        !FMath::IsNearlyEqual(
            Target->GustPeakStrengthCm,
            SourceSnapshot.GustPeakStrengthCm,
            0.001f) ||
        !FMath::IsNearlyEqual(
            Target->WindSpeed, SourceSnapshot.WindSpeed, 0.001f) ||
        !Target->PrevailingWindDirection.Equals(
            SourceSnapshot.PrevailingWindDirection, 0.001f) ||
        !FMath::IsNearlyEqual(
            Target->RecoveryFrequencyHz,
            SourceSnapshot.RecoveryFrequencyHz,
            0.001f) ||
        !FMath::IsNearlyEqual(
            Target->RecoveryDampingRatio,
            SourceSnapshot.RecoveryDampingRatio,
            0.001f) ||
        !Target->bWindMaterialsConfigured ||
        !SourceSnapshot.bWindMaterialsConfigured)
    {
        OutError = TEXT("V2 tree census, aligned Pawn blockers, seed or truthful claims changed in the V3 target.");
        return false;
    }
    const TArray<const UMaterialInterface*> TargetWindMaterials = {
        Target->TreeTrunkWindMaterial.Get(),
        Target->TreeBranchWindMaterial.Get(),
        Target->TreeLeafWindMaterial.Get(),
        Target->GrassWindMaterial.Get()};
    const TArray<int32> TargetWindSlots = {
        Target->TreeTrunkWindSlotIndex,
        Target->TreeBranchWindSlotIndex,
        Target->TreeLeafWindSlotIndex,
        Target->GrassWindSlotIndex};
    if (TargetWindMaterials.Num() != SourceSnapshot.WindMaterialPaths.Num() ||
        TargetWindSlots != SourceSnapshot.WindMaterialSlots)
    {
        OutError = TEXT("Inherited V2 wind material-slot bindings changed.");
        return false;
    }
    for (int32 Index = 0; Index < TargetWindMaterials.Num(); ++Index)
    {
        if (!TargetWindMaterials[Index] ||
            TargetWindMaterials[Index]->GetPathName() !=
                SourceSnapshot.WindMaterialPaths[Index])
        {
            OutError = TEXT("An inherited V2 wind base-material path changed.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateWorld(UWorld* World, FString& OutReport)
{
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutReport = TEXT("Open exact additive map /Game/Maps/Istana_PublicView_Explore_v3.");
        return false;
    }
    UWorld* SourceWorld = LoadExact<UWorld>(SourceMapObjectPath);
    FV2WorldSnapshot SourceSnapshot;
    FString Error;
    if (!ValidateExactV2SourceWorld(SourceWorld, Error) ||
        !CaptureV2WorldSnapshot(SourceWorld, SourceSnapshot, Error) ||
        !ValidateTargetV2AgainstSnapshot(World, SourceSnapshot, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_MAP_INVALID: protected/inherited V2 state failed. ") + Error;
        return false;
    }
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(World);
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = FindPolicy(World);
    ATRIADIstanaExploreV3SupplementActor* Supplement = FindV3Supplement(World);
    UClass* ExploreGameMode = StaticLoadClass(
        AGameModeBase::StaticClass(), nullptr, *ExploreGameModeClassPath);
    FString SceneReport;
    FString SupplementReport;
    FComponentState ExpectedTerrain = SourceSnapshot.Terrain;
    if (ExpectedTerrain.MaterialPaths.IsEmpty())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_MAP_INVALID: inherited terrain has no slot 0 for the target-only lawn presentation override.");
        return false;
    }
    ExpectedTerrain.MaterialPaths[0] = FormalLawnMaterialPath;
    if (!Scene || !Scene->ValidatePublicViewScene(SceneReport, false) ||
        !Policy || Policy->bEnforceFixedPrimaryCamera ||
        !ExploreGameMode || !World->GetWorldSettings() ||
        World->GetWorldSettings()->DefaultGameMode.Get() != ExploreGameMode ||
        !Supplement ||
        !Supplement->ValidateExploreV3Supplement(SupplementReport) ||
        !Supplement->ValidatePreservedDistantContext(
            Scene->OSMContextBuildingsComponent.Get(), Error) ||
        !MatchesComponentState(
            Scene->TerrainComponent.Get(),
            ExpectedTerrain,
            false,
            Error) ||
        !MatchesComponentState(
            Scene->OSMContextBuildingsComponent.Get(),
            SourceSnapshot.DistantContext,
            false,
            Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_MAP_INVALID: scene/free-roam/V3/OSM-HDB validation failed. ") +
            SceneReport + TEXT(" ") + SupplementReport + TEXT(" ") + Error;
        return false;
    }
    OutReport = TEXT("Validated Explore V3 additive map: exact Explore V2 free-roam, terrain mesh/transform/QueryAndPhysics collision, OSM/HDB, component transforms/censuses and aligned 720 Pawn blockers are preserved. Target-only presentation deltas are exactly: terrain slot 0 uses the V3 Grass004 PBR lawn material; V2 NearTurf and MeadowSedge visuals are hidden without changing their meshes/counts/materials/collision; and the 232-instance columnar visual HISM is hidden/replaced at exact transforms by the V3-owned darker wind HISM. Exactly 18,432 prepared 8-triangle, 3-6 cm close-turf cards provide the bounded camera-near grass layer. Other V3 vegetation and identity V7 portico are render-only; coarse geospatial assets remain hidden diagnostics. This is not hyperreal, one-to-one, survey, botanical-inventory, production or sensor truth.");
    return true;
}

bool GetValidatedPlayState(
    UWorld*& OutPlayWorld,
    APlayerController*& OutPlayer,
    ATRIADIstanaFreeRoamPawn*& OutPawn,
    ATRIADIstanaExploreV3SupplementActor*& OutSupplement,
    FString& OutError)
{
    UWorld* EditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString EditorReport;
    if (!ValidateWorld(EditorWorld, EditorReport))
    {
        OutError = TEXT("V3 PIE editor-map validation failed: ") + EditorReport;
        return false;
    }
    OutPlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!OutPlayWorld || OutPlayWorld->WorldType != EWorldType::PIE ||
        !OutPlayWorld->IsGameWorld() || !OutPlayWorld->HasBegunPlay() ||
        UWorld::RemovePIEPrefix(OutPlayWorld->GetOutermost()->GetName()) !=
            DestinationMapPackage)
    {
        OutError = TEXT("Exact begun Explore V3 PIE world is absent.");
        return false;
    }
    OutPlayer = UGameplayStatics::GetPlayerController(OutPlayWorld, 0);
    OutPawn = OutPlayer
        ? Cast<ATRIADIstanaFreeRoamPawn>(OutPlayer->GetPawn())
        : nullptr;
    OutSupplement = FindV3Supplement(OutPlayWorld);
    FString SupplementReport;
    if (!OutPlayer || !OutPawn || !OutPawn->HasExpectedExploreCameraProfile() ||
        OutPlayer->GetViewTarget() != OutPawn ||
        !OutSupplement || !OutSupplement->HasActorBegunPlay() ||
        !OutSupplement->IsWindRuntimeActive() ||
        !OutSupplement->ValidateExploreV3Supplement(SupplementReport))
    {
        OutError = TEXT("V3 PIE Player0/free-roam/manual-exposure camera/wind identity failed. ") +
            SupplementReport;
        return false;
    }
    OutError.Reset();
    return true;
}
}

bool UTRIADIstanaExploreV3EditorLibrary::ImportIstanaExploreV3Assets(
    FString& OutMessage)
{
    FString Error;
    if (HasDirtyPackages(Error))
    {
        OutMessage = TEXT("EXPLORE_V3_IMPORT_REFUSED: ") + Error;
        return false;
    }
    TArray<FTextureSourceSpec> TextureSpecs;
    if (!ValidateFrozenSourceSet(TextureSpecs, Error))
    {
        OutMessage = TEXT("EXPLORE_V3_IMPORT_REFUSED: frozen lawful source validation failed. ") + Error;
        return false;
    }
    if (!ValidateDarkColumnarSourceTextures(nullptr, Error))
    {
        OutMessage = TEXT("EXPLORE_V3_IMPORT_REFUSED: exact locked jacaranda texture preflight failed before any V3 asset creation. ") + Error;
        return false;
    }
    const int32 ExistingAssetCount = CountExistingV3Assets();
    if (ExistingAssetCount > 0)
    {
        FString ExistingReport;
        if (ExistingAssetCount == 67 &&
            ValidateAllV3AssetsInternal(ExistingReport))
        {
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V3_ASSETS_ALREADY_VALID: exact 67-asset V3 namespace is complete.");
            return true;
        }
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V3_IMPORT_REFUSED_PARTIAL_DESTINATION: expected either zero or 67 exact assets under %s, found %d. No overwrite or repair is authorized. %s"),
            *AssetRoot,
            ExistingAssetCount,
            *ExistingReport);
        return false;
    }

    TArray<FProtectedPackageBytes> Protected;
    if (!CaptureProtectedPackages(Protected, Error))
    {
        OutMessage = TEXT("EXPLORE_V3_IMPORT_REFUSED: ") + Error;
        return false;
    }
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    TArray<UObject*> AssetsToSave;
    TMap<FString, UTexture2D*> Textures;
    TMap<FString, UMaterial*> Materials;
    if (!Assets ||
        !ImportTextures(
            AssetTools, TextureSpecs, Textures, AssetsToSave, Error) ||
        !CreateAllMaterials(
            AssetTools, Textures, Materials, AssetsToSave, Error) ||
        !ImportAllMeshes(
            AssetTools, Materials, AssetsToSave, Error) ||
        AssetsToSave.Num() != 67)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_ASSETS: import/configuration stopped fail-closed. Close without saving the V3 namespace. ") + Error;
        return false;
    }
    if (!Assets->SaveLoadedAssets(AssetsToSave, false))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_EXPLORE_V3_ASSETS: exact 67-asset save did not complete.");
        return false;
    }
    FString Validation;
    if (CountExistingV3Assets() != 67 ||
        !ValidateAllV3AssetsInternal(Validation) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        OutMessage = TEXT("EXPLORE_V3_ASSET_PERSISTENCE_FAILED: ") +
            (!Error.IsEmpty() ? Error : Validation);
        return false;
    }
    OutMessage = TEXT("Imported and validated exactly 67 additive Explore V3 assets: 36 frozen 1K textures, 20 HISM-safe/PBR/render materials, and 11 no-collision meshes. Island Tree, moss and Bermuda source LOD0 remain provenance-only through asset/component MinLOD1; close turf is the exact 8-triangle near-camera path. Generated 1 km context is imported as hidden diagnostic only, not a visible terrain replacement. Required OSM, Singapore Open Data and Copernicus attribution is retained; public distribution remains false until credits are surfaced.");
    return true;
}

bool UTRIADIstanaExploreV3EditorLibrary::ValidateIstanaExploreV3Assets(
    FString& OutReport)
{
    const int32 Existing = CountExistingV3Assets();
    FString Error;
    if (Existing != 67 || !ValidateAllV3AssetsInternal(Error))
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V3_ASSETS_INVALID: expected exact complete 67-asset additive namespace; found %d. %s"),
            Existing,
            *Error);
        return false;
    }
    OutReport = TEXT("Validated exact complete 67-asset Explore V3 namespace: source hashes/import policy, 36 textures, 20 materials, 11 meshes, no visual collision, MinLOD/caps, 8-triangle close turf, 5,816-triangle identity portico, hidden generated context and attribution boundary are intact.");
    return true;
}

bool UTRIADIstanaExploreV3EditorLibrary::BuildIstanaExploreV3Map(
    FString& OutMessage)
{
    FString Error;
    if (HasDirtyPackages(Error))
    {
        OutMessage = TEXT("EXPLORE_V3_BUILD_REFUSED: ") + Error;
        return false;
    }
    FString AssetReport;
    if (!ValidateIstanaExploreV3Assets(AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V3_BUILD_REFUSED: exact V3 assets must be imported first. ") + AssetReport;
        return false;
    }
    if (FPackageName::DoesPackageExist(DestinationMapPackage))
    {
        FString Existing;
        if (ValidateIstanaExploreV3Map(Existing))
        {
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V3_MAP_ALREADY_VALID: ") + Existing;
            return true;
        }
        OutMessage = TEXT("EXPLORE_V3_BUILD_REFUSED_PARTIAL_DESTINATION: destination exists but is not the exact validated V3 map; overwrite/repair is refused. ") + Existing;
        return false;
    }

    UWorld* SourceWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString SourceReport;
    if (!SourceWorld || !SourceWorld->GetOutermost() ||
        SourceWorld->GetOutermost()->GetName() != SourceMapPackage ||
        !UTRIADIstanaExploreV2EditorLibrary::ValidateIstanaExploreV2Map(
            SourceReport) ||
        !ValidateExactV2SourceWorld(SourceWorld, Error))
    {
        OutMessage = TEXT("EXPLORE_V3_BUILD_REFUSED: open the exact validated Explore V2 source map. ") +
            SourceReport + TEXT(" ") + Error;
        return false;
    }
    FV2WorldSnapshot SourceSnapshot;
    TArray<FProtectedPackageBytes> Protected;
    if (!CaptureV2WorldSnapshot(SourceWorld, SourceSnapshot, Error) ||
        !CaptureProtectedPackages(Protected, Error))
    {
        OutMessage = TEXT("EXPLORE_V3_BUILD_REFUSED: ") + Error;
        return false;
    }
    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    UWorld* TargetWorld = Assets
        ? Cast<UWorld>(Assets->DuplicateLoadedAsset(
            SourceWorld, DestinationMapPackage))
        : nullptr;
    if (!TargetWorld || !TargetWorld->GetOutermost() ||
        TargetWorld->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: exact V2 duplication failed.");
        return false;
    }
    ATRIADIstanaExploreV2LandscapeActor* V2Landscape =
        FindV2Landscape(TargetWorld);
    ATRIADIstanaPublicViewSceneActor* Scene = FindScene(TargetWorld);
    TArray<UHierarchicalInstancedStaticMeshComponent*> TargetV2Components =
        V2Components(V2Landscape);
    if (!V2Landscape || !Scene ||
        TargetV2Components.Num() != SourceSnapshot.LandscapeComponents.Num())
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: duplicated V2 actor/scene roster changed.");
        return false;
    }
    for (int32 Index = 0; Index < TargetV2Components.Num(); ++Index)
    {
        if (!MatchesComponentState(
                TargetV2Components[Index],
                SourceSnapshot.LandscapeComponents[Index],
                false,
                Error))
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: duplication was not byte/state-equivalent before V3 edits. ") + Error;
            return false;
        }
    }
    if (!MatchesComponentState(
            Scene->TerrainComponent.Get(),
            SourceSnapshot.Terrain,
            false,
            Error) ||
        !MatchesComponentState(
            Scene->OSMContextBuildingsComponent.Get(),
            SourceSnapshot.DistantContext,
            false,
            Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: duplicated terrain or OSM/HDB state changed before V3 presentation edits. ") + Error;
        return false;
    }
    UHierarchicalInstancedStaticMeshComponent* Columnar =
        V2Landscape->ColumnarBroadleafInstances;
    TArray<FTransform> ColumnarWorldTransforms;
    for (int32 Index = 0; Columnar && Index < Columnar->GetInstanceCount(); ++Index)
    {
        FTransform Transform;
        if (!Columnar->GetInstanceTransform(Index, Transform, true))
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: could not capture every V2 columnar world transform.");
            return false;
        }
        ColumnarWorldTransforms.Add(Transform);
    }
    if (!Columnar || ColumnarWorldTransforms.Num() != 232)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: exact 232-instance V2 columnar source is absent.");
        return false;
    }
    Columnar->Modify();
    V2Landscape->ColumnarBroadleafInstances->SetVisibility(false, true);
    V2Landscape->ColumnarBroadleafInstances->SetHiddenInGame(true, true);
    V2Landscape->NearTurfInstances->Modify();
    V2Landscape->NearTurfInstances->SetVisibility(false, true);
    V2Landscape->NearTurfInstances->SetHiddenInGame(true, true);
    V2Landscape->MeadowSedgeInstances->Modify();
    V2Landscape->MeadowSedgeInstances->SetVisibility(false, true);
    V2Landscape->MeadowSedgeInstances->SetHiddenInGame(true, true);
    UMaterialInterface* FormalLawn =
        LoadExact<UMaterialInterface>(FormalLawnMaterialPath);
    if (!FormalLawn || Scene->TerrainComponent->GetNumMaterials() <= 0)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: exact V3 FormalLawn material or inherited terrain slot 0 is absent.");
        return false;
    }
    Scene->TerrainComponent->Modify();
    Scene->TerrainComponent->SetMaterial(0, FormalLawn);

    TArray<UStaticMesh*> RequiredMeshes = {
        LoadExact<UStaticMesh>(IslandTreeObjectPath),
        LoadExact<UStaticMesh>(ShrubObjectPath),
        LoadExact<UStaticMesh>(FernObjectPath),
        LoadExact<UStaticMesh>(MossObjectPath),
        LoadExact<UStaticMesh>(BermudaObjectPath),
        LoadExact<UStaticMesh>(TurfCardObjectPath),
        LoadExact<UStaticMesh>(V2BroadleafObjectPath),
        LoadExact<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")),
        LoadExact<UStaticMesh>(PorticoObjectPath),
        LoadExact<UStaticMesh>(TerrainObjectPath),
        LoadExact<UStaticMesh>(OsmRoadsObjectPath),
        LoadExact<UStaticMesh>(UraRoadsObjectPath),
        LoadExact<UStaticMesh>(OsmWaterObjectPath)};
    if (RequiredMeshes.Contains(nullptr))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: an exact V3/V2 mesh binding is absent.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(RequiredMeshes);
    FActorSpawnParameters Parameters;
    Parameters.Name = TEXT("TRIADIstanaExploreSupplementV3");
    Parameters.OverrideLevel = TargetWorld->PersistentLevel;
    Parameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADIstanaExploreV3SupplementActor* Supplement =
        TargetWorld->SpawnActor<ATRIADIstanaExploreV3SupplementActor>(
            ATRIADIstanaExploreV3SupplementActor::StaticClass(),
            FTransform::Identity,
            Parameters);
    if (!Supplement)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: V3 supplement actor could not be spawned.");
        return false;
    }
    Supplement->Tags.AddUnique(V3SupplementTag);
    Supplement->SetActorLabel(TEXT("TRIAD Istana Explore V3 Lawful-Source Visual Supplement (Not Hyperreal / Not Sensor Truth)"));
    if (!Supplement->ConfigureRequiredAssets(
            RequiredMeshes[0], RequiredMeshes[1], RequiredMeshes[2],
            RequiredMeshes[3], RequiredMeshes[4], RequiredMeshes[5],
            RequiredMeshes[6],
            LoadExact<UMaterialInterface>(V2TrunkWindMaterialPath),
            LoadExact<UMaterialInterface>(V2BranchWindMaterialPath),
            LoadExact<UMaterialInterface>(DarkColumnarMaterialPath),
            RequiredMeshes[7], RequiredMeshes[8], Error) ||
        !Supplement->ConfigureOptionalVisualContext(
            RequiredMeshes[9], RequiredMeshes[10], RequiredMeshes[11],
            RequiredMeshes[12], ContextManifestSha, Error) ||
        !Supplement->RecordPreservedDistantContext(
            Scene->OSMContextBuildingsComponent.Get(), Error) ||
        !Supplement->PopulateDeterministicSupplement(
            ColumnarWorldTransforms, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_EXPLORE_V3_MAP: ") + Error;
        return false;
    }
    TargetWorld->MarkPackageDirty();
    FString BeforeSave;
    if (!ValidateWorld(TargetWorld, BeforeSave) ||
        !Assets->SaveLoadedAsset(TargetWorld, false))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_EXPLORE_V3_MAP: ") + BeforeSave;
        return false;
    }
    FString Filename;
    UWorld* Reloaded = FPackageName::DoesPackageExist(
            DestinationMapPackage, &Filename)
        ? UEditorLoadingAndSavingUtils::LoadMap(Filename)
        : nullptr;
    FString Persisted;
    if (!Reloaded || !ValidateWorld(Reloaded, Persisted) ||
        !ValidateProtectedPackages(Protected, Error))
    {
        OutMessage = TEXT("EXPLORE_V3_PERSISTENCE_FAILED: ") +
            (!Error.IsEmpty() ? Error : Persisted);
        return false;
    }
    OutMessage = TEXT("Created additive /Game/Maps/Istana_PublicView_Explore_v3 as an exact Explore V2 duplicate plus bounded lawful-source presentation deltas: target terrain slot 0 receives Grass004 PBR while its mesh/transform/QueryAndPhysics collision stay exact; only duplicated V2 NearTurf and MeadowSedge visuals are hidden with their census/assets/state preserved; 18,432 prepared 8-triangle, 3-6 cm close-turf cards supply camera-near grass; and the exact 232-instance columnar HISM is hidden/replaced at identical transforms by a darker V3-owned leaf-wind HISM while V2 Pawn blockers remain aligned. V2 free-roam and OSM/HDB are preserved. Generated 1 km context is hidden diagnostic only. The result is not hyperreal, one-to-one, survey, botanical-inventory, production or sensor truth. ") + Persisted;
    return true;
}

bool UTRIADIstanaExploreV3EditorLibrary::ValidateIstanaExploreV3Map(
    FString& OutReport)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    return ValidateWorld(World, OutReport);
}

bool UTRIADIstanaExploreV3EditorLibrary::ValidateIstanaExploreV3PlayWorld(
    FString& OutReport)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV3SupplementActor* Supplement = nullptr;
    FString Error;
    if (!GetValidatedPlayState(
            PlayWorld, Player, Pawn, Supplement, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_PIE_INVALID: ") + Error;
        return false;
    }
    ATRIADIstanaExploreV2LandscapeActor* V2Landscape =
        FindV2Landscape(PlayWorld);
    if (!V2Landscape || !V2Landscape->HasActorBegunPlay() ||
        !V2Landscape->IsWindRuntimeActive())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_PIE_INVALID: inherited V2 wind runtime is not active.");
        return false;
    }
    OutReport = TEXT("Explore V3 PIE is live: Player0 owns/views the free-roam pawn; inherited V2 wind and V3 instance-local underdamped gust/after-sway are active; hidden diagnostic context remains no-collision and non-sensor-authoritative.");
    return true;
}

bool UTRIADIstanaExploreV3EditorLibrary::CaptureIstanaExploreV3PlayView(
    const FString& OutputFileName,
    FString& OutMessage)
{
    if (!OutputFileName.StartsWith(TEXT("explore_v3_"), ESearchCase::IgnoreCase) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName)
    {
        OutMessage = TEXT("Explore V3 PIE captures require one clean 'explore_v3_*.png' filename.");
        return false;
    }
    FString Validation;
    if (!ValidateIstanaExploreV3PlayWorld(Validation))
    {
        OutMessage = TEXT("Explore V3 capture requires the validated live free-roam world. ") + Validation;
        return false;
    }
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    UGameViewportClient* ViewportClient = PlayWorld
        ? PlayWorld->GetGameViewport()
        : nullptr;
    FSceneViewport* Viewport = ViewportClient
        ? ViewportClient->GetGameViewport()
        : nullptr;
    if (!Viewport)
    {
        OutMessage = TEXT("The validated Explore V3 Player0 viewport is unavailable.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("TRIAD/IstanaPreviews/ExploreV3"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    {
        OutMessage = TEXT("Could not create the Explore V3 QA capture directory.");
        return false;
    }
    const FString Destination = FPaths::Combine(Directory, OutputFileName);
    if (IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested())
    {
        OutMessage = TEXT("Explore V3 capture refused an overwrite or overlapping screenshot request.");
        return false;
    }
    FScreenshotRequest::RequestScreenshot(Destination, false, false, false);
    Viewport->Draw(false);
    OutMessage = TEXT("Captured validated Explore V3 Player0 view to '") +
        Destination + TEXT("'.");
    return true;
}

bool UTRIADIstanaExploreV3EditorLibrary::TriggerIstanaExploreV3PlayWindGust(
    float PeakStrengthCm,
    FString& OutMessage)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV3SupplementActor* Supplement = nullptr;
    FString Error;
    if (!FMath::IsFinite(PeakStrengthCm) || PeakStrengthCm < 6.0f ||
        PeakStrengthCm > 120.0f ||
        !GetValidatedPlayState(
            PlayWorld, Player, Pawn, Supplement, Error))
    {
        OutMessage = TEXT("EXPLORE_V3_QA_GUST_REFUSED: require exact V3 PIE and a finite 6-120 cm peak. ") + Error;
        return false;
    }
    Supplement->TriggerWindGust(PeakStrengthCm);
    OutMessage = FString::Printf(
        TEXT("Triggered deterministic bounded V3 QA gust peak %.3f cm; %s"),
        PeakStrengthCm,
        *Supplement->BuildWindRuntimeStateReport());
    return true;
}

bool UTRIADIstanaExploreV3EditorLibrary::MoveIstanaExploreV3PlayPawnForQa(
    FVector DeltaCentimeters,
    FString& OutMessage)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV3SupplementActor* Supplement = nullptr;
    FString Error;
    if (DeltaCentimeters.ContainsNaN() ||
        DeltaCentimeters.Size() < 1.0 ||
        DeltaCentimeters.Size() > 5000.0 ||
        !GetValidatedPlayState(
            PlayWorld, Player, Pawn, Supplement, Error))
    {
        OutMessage = TEXT("EXPLORE_V3_QA_MOVE_REFUSED: require exact V3 PIE and a finite 1-5000 cm delta. ") + Error;
        return false;
    }
    const FVector Start = Pawn->GetActorLocation();
    const FVector Requested = Start + DeltaCentimeters;
    if (FVector2D(Requested.X, Requested.Y).Size() > 95000.0 ||
        Requested.Z < 50.0 || Requested.Z > 20000.0)
    {
        OutMessage = TEXT("EXPLORE_V3_QA_MOVE_REFUSED: requested pose is outside the bounded 950 m / 0.5-200 m QA envelope.");
        return false;
    }
    FHitResult Hit;
    const bool bMoved = Pawn->SetActorLocation(
        Requested, true, &Hit, ETeleportType::None);
    const FVector End = Pawn->GetActorLocation();
    if (!bMoved || End.Equals(Start, 0.1f))
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V3_QA_MOVE_BLOCKED: sweep hit '%s'; pawn remains at %s."),
            *GetNameSafe(Hit.GetActor()),
            *End.ToString());
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Moved validated Explore V3 Player0 pawn by sweep from %s to %s (requested %s); viewTargetMatchesPawn=%s."),
        *Start.ToString(),
        *End.ToString(),
        *Requested.ToString(),
        Player->GetViewTarget() == Pawn ? TEXT("true") : TEXT("false"));
    return Player->GetViewTarget() == Pawn;
}

bool UTRIADIstanaExploreV3EditorLibrary::GetIstanaExploreV3PlayStateReport(
    FString& OutReport)
{
    UWorld* PlayWorld = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaFreeRoamPawn* Pawn = nullptr;
    ATRIADIstanaExploreV3SupplementActor* Supplement = nullptr;
    FString Error;
    if (!GetValidatedPlayState(
            PlayWorld, Player, Pawn, Supplement, Error))
    {
        OutReport = TEXT("EXPLORE_V3_QA_STATE_INVALID: ") + Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("map=%s pawn=%s pawnLocationCm=%s pawnRotationDeg=%s viewTarget=%s viewTargetMatchesPawn=true %s"),
        *UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName()),
        *Pawn->GetName(),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString(),
        *GetNameSafe(Player->GetViewTarget()),
        *Supplement->BuildWindRuntimeStateReport());
    return true;
}

bool UTRIADIstanaExploreV3EditorLibrary::
    QuiesceIstanaExploreV3PlayWorldForStop(FString& OutMessage)
{
    FString Readiness;
    if (!ValidateIstanaExploreV3PlayWorld(Readiness))
    {
        OutMessage = TEXT("Refusing scripted Explore V3 PIE stop because exact runtime identity failed. ") + Readiness;
        return false;
    }
    OutMessage = TEXT("Explore V3 PIE identity, Player0 free-roam possession, inherited V2 wind and V3 pivot-safe gust/after-sway runtime are exact; no fragile UEDPIE object path or console command is required, so scripted stop may proceed.");
    return true;
}
