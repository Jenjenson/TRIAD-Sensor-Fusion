#include "TRIADIstanaPublicViewHeroV2EditorLibrary.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Scene.h"
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
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionBumpOffset.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/Archive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace
{
// TRIAD_HERO_SHA256_HOST_BEGIN
namespace TriadHeroSha256
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
    return true;
}
} // namespace TriadHeroSha256
// TRIAD_HERO_SHA256_HOST_END

const FString SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Exterior_v1"));
const FString DestinationMapPackage(TEXT("/Game/Maps/Istana_PublicView_Exterior_v2"));
const FString SourceMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Exterior_v1.Istana_PublicView_Exterior_v1"));
const FString DestinationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Exterior_v2.Istana_PublicView_Exterior_v2"));
const FString HeroAssetRoot(TEXT("/Game/TRIAD/IstanaPublicViewV2"));
const FString HeroMeshPath(TEXT("/Game/TRIAD/IstanaPublicViewV2/Building"));
const FString HeroMeshName(TEXT("SM_IstanaPublicViewV2_Building_Hero"));
const FString HeroMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewV2/Building/SM_IstanaPublicViewV2_Building_Hero.SM_IstanaPublicViewV2_Building_Hero"));
const FString MaterialAssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV2"));
const FString MaterialTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV2/Textures"));
const FString GeometrySourceRoot(TEXT("SourceAssets/IstanaPublicViewV2"));
const FString GeometryContractRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV2/istana_public_view_hero_v2.contract.json"));
const FString GeometryManifestRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV2/Generated/IstanaPublicViewV2Building.manifest.json"));
const FString GeometryObjRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV2/Generated/SM_IstanaPublicViewV2_Building_Hero.obj"));
const FString FreezeRelativePath(
    TEXT("Plugins/TRIADSensorFusion/Resources/IstanaPublicViewHeroV2.integration.freeze.json"));
const FString MaterialSourceRoot(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV2"));
const FString MaterialContractRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV2/hero_materials_v2.contract.json"));
const FString MaterialInterfaceRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV2/unreal_asset_interface.v2.json"));
const FString MaterialSourceManifestRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV2/Source/source_manifest.json"));
const FString MaterialGeneratedManifestRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV2/Generated/manifest.json"));
const FString V1CollisionObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Building/SM_IstanaPublicView_Building_Collision.SM_IstanaPublicView_Building_Collision"));
const FString FrozenFreezeSha256(
    TEXT("9b23203d94845e93a7d92b1710cad56964bcd51517153d4bd92a72ed55444501"));
const FName SceneActorTag(TEXT("TRIADIstanaPublicViewScene_v1"));

constexpr int32 ExpectedHeroTriangles = 496644;
constexpr int32 ExpectedMaterialTextureCount = 57;
constexpr double BoundsToleranceCentimeters = 0.1;

const TArray<FString>& ExpectedHeroSlots()
{
    static const TArray<FString> Slots = {
        TEXT("M_IPV2_Render"),
        TEXT("M_IPV2_Trim"),
        TEXT("M_IPV2_Slate"),
        TEXT("M_IPV2_Shutter"),
        TEXT("M_IPV2_Stone"),
        TEXT("M_IPV2_Door"),
        TEXT("M_IPV2_DarkTimber"),
        TEXT("M_IPV2_Metal"),
        TEXT("M_IPV2_Glass"),
        TEXT("M_IPV2_Opaline"),
        TEXT("M_IPV2_Recess")};
    return Slots;
}

const TMap<FString, FString>& ExpectedSlotInstances()
{
    static const TMap<FString, FString> Bindings = {
        {TEXT("M_IPV2_Render"), TEXT("MI_IPV_Hero_Render_V2")},
        {TEXT("M_IPV2_Trim"), TEXT("MI_IPV_Hero_Trim_V2")},
        {TEXT("M_IPV2_Slate"), TEXT("MI_IPV_Hero_Slate_V2")},
        {TEXT("M_IPV2_Shutter"), TEXT("MI_IPV_Hero_Louvre_V2")},
        {TEXT("M_IPV2_Stone"), TEXT("MI_IPV_Hero_Stone_V2")},
        {TEXT("M_IPV2_Door"), TEXT("MI_IPV_Hero_Timber_V2")},
        {TEXT("M_IPV2_DarkTimber"), TEXT("MI_IPV_Hero_Timber_V2")},
        {TEXT("M_IPV2_Metal"), TEXT("MI_IPV_Hero_PaintedMetal_V2")},
        {TEXT("M_IPV2_Glass"), TEXT("MI_IPV_Hero_Glass_V2")},
        {TEXT("M_IPV2_Opaline"), TEXT("MI_IPV_Hero_Opaline_V2")},
        {TEXT("M_IPV2_Recess"), TEXT("MI_IPV_Hero_Recess_V2")}};
    return Bindings;
}

FString ProjectSourcePath(const FString& RelativePath)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(), RelativePath));
}

FString MaterialObjectPath(const FString& AssetName)
{
    return FString::Printf(
        TEXT("%s/%s.%s"),
        *MaterialAssetRoot,
        *AssetName,
        *AssetName);
}

bool IsSafeRelativeSourcePath(const FString& RelativePath)
{
    return !RelativePath.IsEmpty() &&
        FPaths::IsRelative(RelativePath) &&
        !RelativePath.Contains(TEXT("..")) &&
        !RelativePath.Contains(TEXT(":"));
}

bool Sha256ImplementationIsValid()
{
    static const bool bKnownVectorsValid =
        TriadHeroSha256::VerifyKnownVectors();
    return bKnownVectorsValid;
}

bool FinishSha256(
    const TriadHeroSha256::FStreamingSha256& Hasher,
    FString& OutDigest)
{
    TriadHeroSha256::FDigest Digest{};
    if (!Hasher.Finalize(Digest))
    {
        return false;
    }
    const TriadHeroSha256::FLowerHexDigest Hex =
        TriadHeroSha256::ToLowerHex(Digest);
    OutDigest = UTF8_TO_TCHAR(Hex.data());
    return OutDigest.Len() == 64;
}

bool CalculateSha256Bytes(
    const void* Data,
    int64 ByteCount,
    FString& OutDigest)
{
    if (!Sha256ImplementationIsValid() || ByteCount < 0 ||
        (ByteCount > 0 && !Data))
    {
        return false;
    }
    TriadHeroSha256::FStreamingSha256 Hasher;
    if (!Hasher.Update(
            static_cast<const std::uint8_t*>(Data),
            static_cast<std::size_t>(ByteCount)))
    {
        return false;
    }
    return FinishSha256(Hasher, OutDigest);
}

bool CalculateSha256(
    const FString& Filename,
    FString& OutDigest,
    int64& OutByteCount,
    FString& OutError)
{
    TUniquePtr<FArchive> Reader(
        IFileManager::Get().CreateFileReader(*Filename, FILEREAD_Silent));
    if (!Reader)
    {
        OutError = FString::Printf(
            TEXT("Could not open frozen file '%s' for SHA-256."),
            *Filename);
        return false;
    }
    const int64 TotalBytes = Reader->TotalSize();
    if (TotalBytes <= 0 || !Sha256ImplementationIsValid())
    {
        OutError = FString::Printf(
            TEXT("Could not initialize SHA-256 for '%s'."),
            *Filename);
        return false;
    }

    constexpr int64 ReadChunkBytes = 1024 * 1024;
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(ReadChunkBytes);
    TriadHeroSha256::FStreamingSha256 Hasher;
    int64 RemainingBytes = TotalBytes;
    while (RemainingBytes > 0)
    {
        const int64 ThisChunk = FMath::Min(RemainingBytes, ReadChunkBytes);
        Reader->Serialize(Buffer.GetData(), ThisChunk);
        if (Reader->IsError())
        {
            OutError = FString::Printf(
                TEXT("Could not read all bytes from frozen file '%s'."),
                *Filename);
            return false;
        }
        if (!Hasher.Update(
                Buffer.GetData(),
                static_cast<std::size_t>(ThisChunk)))
        {
            OutError = FString::Printf(
                TEXT("Frozen file '%s' exceeds the SHA-256 length limit."),
                *Filename);
            return false;
        }
        RemainingBytes -= ThisChunk;
    }
    if (!FinishSha256(Hasher, OutDigest))
    {
        OutError = FString::Printf(
            TEXT("Could not finalize SHA-256 for '%s'."),
            *Filename);
        return false;
    }
    OutByteCount = TotalBytes;
    OutError.Reset();
    return true;
}

bool ValidateFrozenFile(
    const FString& Filename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutError)
{
    FString ActualDigest;
    int64 ActualBytes = -1;
    if (!CalculateSha256(Filename, ActualDigest, ActualBytes, OutError))
    {
        return false;
    }
    if (ActualBytes != ExpectedBytes ||
        ActualDigest != ExpectedSha256.ToLower())
    {
        OutError = FString::Printf(
            TEXT("Frozen file '%s' is %lld bytes/SHA-256 %s; expected %lld/%s."),
            *Filename,
            ActualBytes,
            *ActualDigest,
            ExpectedBytes,
            *ExpectedSha256);
        return false;
    }
    OutError.Reset();
    return true;
}

bool LoadJsonObject(
    const FString& Filename,
    TSharedPtr<FJsonObject>& OutObject,
    FString& OutError)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Filename))
    {
        OutError = FString::Printf(TEXT("Could not read JSON '%s'."), *Filename);
        return false;
    }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
    {
        OutError = FString::Printf(TEXT("Could not parse JSON '%s'."), *Filename);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ReadExactString(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const TCHAR* Expected,
    FString& OutError)
{
    FString Value;
    if (!Object.IsValid() ||
        !Object->TryGetStringField(Field, Value) ||
        Value != Expected)
    {
        OutError = FString::Printf(
            TEXT("JSON field '%s' must be exactly '%s'."),
            Field,
            Expected);
        return false;
    }
    return true;
}

bool ReadExactBool(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    bool Expected,
    FString& OutError)
{
    bool Value = !Expected;
    if (!Object.IsValid() ||
        !Object->TryGetBoolField(Field, Value) ||
        Value != Expected)
    {
        OutError = FString::Printf(
            TEXT("JSON field '%s' must be exactly %s."),
            Field,
            Expected ? TEXT("true") : TEXT("false"));
        return false;
    }
    return true;
}

bool ReadInteger(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    int64& OutValue,
    FString& OutError)
{
    double Value = 0.0;
    if (!Object.IsValid() ||
        !Object->TryGetNumberField(Field, Value) ||
        !FMath::IsFinite(Value) ||
        Value < 0.0 ||
        Value != FMath::FloorToDouble(Value) ||
        Value > 9007199254740991.0)
    {
        OutError = FString::Printf(
            TEXT("JSON field '%s' must be an exact non-negative integer."),
            Field);
        return false;
    }
    OutValue = static_cast<int64>(Value);
    return true;
}

bool ReadVector3Meters(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FVector& OutValue,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() ||
        !Object->TryGetArrayField(Field, Values) ||
        !Values || Values->Num() != 3)
    {
        OutError = FString::Printf(
            TEXT("JSON field '%s' must be a three-number array."),
            Field);
        return false;
    }
    double Components[3] = {0.0, 0.0, 0.0};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (!(*Values)[Index].IsValid() ||
            (*Values)[Index]->Type != EJson::Number)
        {
            OutError = FString::Printf(
                TEXT("JSON field '%s' contains a non-number."),
                Field);
            return false;
        }
        Components[Index] = (*Values)[Index]->AsNumber();
        if (!FMath::IsFinite(Components[Index]))
        {
            OutError = FString::Printf(
                TEXT("JSON field '%s' contains a non-finite number."),
                Field);
            return false;
        }
    }
    OutValue = FVector(Components[0], Components[1], Components[2]);
    return true;
}

struct FFrozenFileRecord
{
    FString RelativePath;
    FString Role;
    int64 Bytes = 0;
    FString Sha256;
};

bool ValidateFreezeRecordArray(
    const TArray<TSharedPtr<FJsonValue>>* Records,
    const TCHAR* ArrayName,
    TSet<FString>& InOutSeenPaths,
    FString& OutError)
{
    if (!Records || Records->Num() < 1)
    {
        OutError = FString::Printf(
            TEXT("Integration freeze array '%s' is empty."),
            ArrayName);
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Records)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FString RelativePath;
        FString Role;
        FString Sha256;
        int64 Bytes = 0;
        if (!Row.IsValid() ||
            !Row->TryGetStringField(TEXT("relativePath"), RelativePath) ||
            !Row->TryGetStringField(TEXT("role"), Role) ||
            !Row->TryGetStringField(TEXT("sha256"), Sha256) ||
            !ReadInteger(Row, TEXT("bytes"), Bytes, OutError) ||
            !IsSafeRelativeSourcePath(RelativePath) ||
            Role.IsEmpty() || Sha256.Len() != 64 ||
            InOutSeenPaths.Contains(RelativePath))
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("Integration freeze has an unsafe or duplicate record in '%s'."),
                    ArrayName);
            }
            return false;
        }
        InOutSeenPaths.Add(RelativePath);
        if (!ValidateFrozenFile(
                ProjectSourcePath(RelativePath),
                Bytes,
                Sha256,
                OutError))
        {
            return false;
        }
    }
    return true;
}

bool ValidateIntegrationFreeze(FString& OutReport)
{
    const FString FreezePath = ProjectSourcePath(FreezeRelativePath);
    FString Error;
    FString Digest;
    int64 FreezeBytes = 0;
    if (!CalculateSha256(FreezePath, Digest, FreezeBytes, Error))
    {
        OutReport = TEXT("V2_FREEZE_MISSING: ") + Error;
        return false;
    }
    if (Digest != FrozenFreezeSha256)
    {
        OutReport = FString::Printf(
            TEXT("V2_FREEZE_INVALID: integration freeze SHA-256 is %s, expected %s."),
            *Digest,
            *FrozenFreezeSha256);
        return false;
    }
    TSharedPtr<FJsonObject> Freeze;
    if (!LoadJsonObject(FreezePath, Freeze, Error) ||
        !ReadExactString(
            Freeze,
            TEXT("schema"),
            TEXT("triad.istana_public_view_hero_v2.unreal_freeze.v1"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("heroAssetRoot"),
            TEXT("/Game/TRIAD/IstanaPublicViewV2"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("materialAssetRoot"),
            TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV2"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("sourceMap"),
            TEXT("/Game/Maps/Istana_PublicView_Exterior_v1"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("destinationMap"),
            TEXT("/Game/Maps/Istana_PublicView_Exterior_v2"),
            Error) ||
        !ReadExactBool(Freeze, TEXT("naniteEnabled"), false, Error) ||
        !ReadExactBool(Freeze, TEXT("sm6Required"), false, Error) ||
        !ReadExactBool(Freeze, TEXT("reusesV1Collision"), true, Error) ||
        !ReadExactBool(Freeze, TEXT("changesSurroundings"), false, Error))
    {
        OutReport = TEXT("V2_FREEZE_INVALID: ") + Error;
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* ProtectedV1 = nullptr;
    if (!Freeze->TryGetArrayField(TEXT("files"), Files) ||
        !Freeze->TryGetArrayField(TEXT("protectedV1Files"), ProtectedV1))
    {
        OutReport = TEXT("V2_FREEZE_INVALID: files/protectedV1Files are absent.");
        return false;
    }
    TSet<FString> SeenPaths;
    if (!ValidateFreezeRecordArray(Files, TEXT("files"), SeenPaths, Error) ||
        !ValidateFreezeRecordArray(
            ProtectedV1,
            TEXT("protectedV1Files"),
            SeenPaths,
            Error))
    {
        OutReport = TEXT("V2_FREEZE_INVALID: ") + Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Validated %d exact integration-freeze records (%lld-byte freeze, SHA-256 %s)."),
        SeenPaths.Num(),
        FreezeBytes,
        *Digest);
    return true;
}

struct FHeroGeometryContract
{
    int64 SourceBytes = 0;
    FString SourceSha256;
    int32 Triangles = 0;
    FVector BoundsMinMeters = FVector::ZeroVector;
    FVector BoundsMaxMeters = FVector::ZeroVector;
    TArray<FString> MaterialSlots;
};

template <typename ElementType>
bool AreSetsEqual(
    const TSet<ElementType>& Left,
    const TSet<ElementType>& Right)
{
    return Left.Num() == Right.Num() && Left.Includes(Right);
}

bool HasExactStringSet(
    const TArray<FString>& Actual,
    const TArray<FString>& Expected)
{
    if (Actual.Num() != Expected.Num())
    {
        return false;
    }
    TSet<FString> ActualSet;
    TSet<FString> ExpectedSet;
    for (const FString& Value : Actual)
    {
        ActualSet.Add(Value);
    }
    for (const FString& Value : Expected)
    {
        ExpectedSet.Add(Value);
    }
    return ActualSet.Num() == Actual.Num() &&
        AreSetsEqual(ActualSet, ExpectedSet);
}

bool ReadStringArray(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    TArray<FString>& OutValues,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() ||
        !Object->TryGetArrayField(Field, Values) || !Values)
    {
        OutError = FString::Printf(TEXT("JSON array '%s' is absent."), Field);
        return false;
    }
    OutValues.Reset();
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        if (!Value.IsValid() || Value->Type != EJson::String)
        {
            OutError = FString::Printf(
                TEXT("JSON array '%s' contains a non-string."),
                Field);
            return false;
        }
        OutValues.Add(Value->AsString());
    }
    return true;
}

bool ValidateGeometrySource(
    FHeroGeometryContract& OutContract,
    FString& OutReport)
{
    FString Error;
    TSharedPtr<FJsonObject> Contract;
    TSharedPtr<FJsonObject> Manifest;
    if (!LoadJsonObject(ProjectSourcePath(GeometryContractRelativePath), Contract, Error) ||
        !LoadJsonObject(ProjectSourcePath(GeometryManifestRelativePath), Manifest, Error) ||
        !ReadExactString(
            Contract,
            TEXT("schema"),
            TEXT("triad.istana_public_view_hero_contract.v2"),
            Error) ||
        !ReadExactString(
            Manifest,
            TEXT("schema"),
            TEXT("triad.istana_public_view_building_manifest.v2"),
            Error))
    {
        OutReport = TEXT("V2_GEOMETRY_CONTRACT_INVALID: ") + Error;
        return false;
    }
    const TSharedPtr<FJsonObject>* Scope = nullptr;
    const TSharedPtr<FJsonObject>* Surface = nullptr;
    const TSharedPtr<FJsonObject>* UnrealImport = nullptr;
    if (!Contract->TryGetObjectField(TEXT("scope"), Scope) || !Scope ||
        !(*Scope).IsValid() ||
        !Contract->TryGetObjectField(TEXT("surfaceContract"), Surface) ||
        !Surface || !(*Surface).IsValid() ||
        !(*Surface)->TryGetObjectField(TEXT("unrealImport"), UnrealImport) ||
        !UnrealImport || !(*UnrealImport).IsValid() ||
        !ReadExactBool(*Scope, TEXT("heroBuildingVisualOnly"), true, Error) ||
        !ReadExactBool(*Scope, TEXT("replacesPublicViewV1HeroVisualOnly"), true, Error) ||
        !ReadExactBool(*Scope, TEXT("reusesPublicViewV1Collision"), true, Error) ||
        !ReadExactBool(*Scope, TEXT("changesTerrain"), false, Error) ||
        !ReadExactBool(*Scope, TEXT("changesHardscape"), false, Error) ||
        !ReadExactBool(*Scope, TEXT("changesVegetation"), false, Error) ||
        !ReadExactBool(*Scope, TEXT("changesContextBuildings"), false, Error) ||
        !ReadExactBool(*UnrealImport, TEXT("naniteEnabled"), false, Error) ||
        !ReadExactString(*UnrealImport, TEXT("normalImportMethod"), TEXT("ImportNormals"), Error) ||
        !ReadExactBool(*UnrealImport, TEXT("recomputeNormals"), false, Error) ||
        !ReadExactBool(*UnrealImport, TEXT("recomputeTangents"), true, Error))
    {
        OutReport = TEXT("V2_GEOMETRY_CONTRACT_INVALID: hero-only/no-Nanite scope changed. ") + Error;
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("files"), Files) ||
        !Files || Files->Num() != 1)
    {
        OutReport = TEXT("V2_GEOMETRY_MANIFEST_INVALID: exactly one hero file is required.");
        return false;
    }
    const TSharedPtr<FJsonObject> Record = (*Files)[0].IsValid()
        ? (*Files)[0]->AsObject()
        : nullptr;
    FString Path;
    FString Role;
    if (!Record.IsValid() ||
        !Record->TryGetStringField(TEXT("path"), Path) ||
        Path != TEXT("SM_IstanaPublicViewV2_Building_Hero.obj") ||
        !Record->TryGetStringField(TEXT("role"), Role) ||
        Role != TEXT("BUILDING_HERO_VISUAL_V2") ||
        !Record->TryGetStringField(TEXT("sha256"), OutContract.SourceSha256) ||
        !ReadInteger(Record, TEXT("bytes"), OutContract.SourceBytes, Error))
    {
        OutReport = TEXT("V2_GEOMETRY_MANIFEST_INVALID: hero file record changed. ") + Error;
        return false;
    }
    int64 TriangleCount = 0;
    if (!ReadInteger(Record, TEXT("triangles"), TriangleCount, Error) ||
        TriangleCount != ExpectedHeroTriangles ||
        !ReadVector3Meters(
            Record, TEXT("expectedUnrealImportedBoundsMinMeters"),
            OutContract.BoundsMinMeters, Error) ||
        !ReadVector3Meters(
            Record, TEXT("expectedUnrealImportedBoundsMaxMeters"),
            OutContract.BoundsMaxMeters, Error) ||
        !ReadStringArray(Record, TEXT("materialSlots"), OutContract.MaterialSlots, Error) ||
        !HasExactStringSet(OutContract.MaterialSlots, ExpectedHeroSlots()))
    {
        OutReport = TEXT("V2_GEOMETRY_MANIFEST_INVALID: metrics or exact M_IPV2 slot roster changed. ") + Error;
        return false;
    }
    for (const FString& Slot : OutContract.MaterialSlots)
    {
        if (!Slot.StartsWith(TEXT("M_IPV2_")) || Slot.StartsWith(TEXT("M_IPV_")))
        {
            OutReport = TEXT("V2_GEOMETRY_MANIFEST_INVALID: legacy M_IPV_* slots are forbidden.");
            return false;
        }
    }
    OutContract.Triangles = static_cast<int32>(TriangleCount);
    if (!ValidateFrozenFile(
            ProjectSourcePath(GeometryObjRelativePath),
            OutContract.SourceBytes,
            OutContract.SourceSha256,
            Error))
    {
        OutReport = TEXT("V2_GEOMETRY_SOURCE_INVALID: ") + Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Validated exact %lld-byte v2 hero OBJ (%d triangles, eleven M_IPV2 slots, regular static mesh/no Nanite)."),
        OutContract.SourceBytes,
        OutContract.Triangles);
    return true;
}

enum class EHeroTextureUsage : uint8
{
    BaseColor,
    Normal,
    PackedOrm,
    Height,
    DetailNormal,
    Masks
};

struct FHeroTextureSpec
{
    FString MaterialId;
    FString MapType;
    FString SourceFilename;
    FString AssetName;
    int64 Bytes = 0;
    FString Sha256;
    int32 Width = 0;
    int32 Height = 0;
    EHeroTextureUsage Usage = EHeroTextureUsage::Masks;
};

struct FHeroMaterialInstanceSpec
{
    FString Id;
    FString AssetName;
    FString ParentAssetName;
    TArray<FString> V2Slots;
    TMap<FName, float> Scalars;
    TMap<FName, FLinearColor> Vectors;
    TMap<FName, bool> StaticSwitches;
    bool bSpecialParameterOnly = false;
};

struct FHeroMaterialPack
{
    TArray<FHeroTextureSpec> Textures;
    TArray<FHeroMaterialInstanceSpec> Instances;
};

EHeroTextureUsage TextureUsageFromMapType(const FString& MapType)
{
    if (MapType == TEXT("BaseColor"))
    {
        return EHeroTextureUsage::BaseColor;
    }
    if (MapType == TEXT("Normal"))
    {
        return EHeroTextureUsage::Normal;
    }
    if (MapType == TEXT("ORM"))
    {
        return EHeroTextureUsage::PackedOrm;
    }
    if (MapType == TEXT("Height"))
    {
        return EHeroTextureUsage::Height;
    }
    if (MapType == TEXT("DetailNormal"))
    {
        return EHeroTextureUsage::DetailNormal;
    }
    return EHeroTextureUsage::Masks;
}

TextureCompressionSettings TextureCompression(EHeroTextureUsage Usage)
{
    if (Usage == EHeroTextureUsage::Normal ||
        Usage == EHeroTextureUsage::DetailNormal)
    {
        return TC_Normalmap;
    }
    if (Usage == EHeroTextureUsage::Height)
    {
        return TC_Grayscale;
    }
    if (Usage == EHeroTextureUsage::PackedOrm ||
        Usage == EHeroTextureUsage::Masks)
    {
        return TC_Masks;
    }
    return TC_Default;
}

TextureGroup TextureLodGroup(EHeroTextureUsage Usage)
{
    return Usage == EHeroTextureUsage::Normal ||
        Usage == EHeroTextureUsage::DetailNormal
        ? TEXTUREGROUP_WorldNormalMap
        : TEXTUREGROUP_World;
}

bool TextureIsSrgb(EHeroTextureUsage Usage)
{
    return Usage == EHeroTextureUsage::BaseColor;
}

FString TextureObjectPath(const FHeroTextureSpec& Spec)
{
    return FString::Printf(
        TEXT("%s/%s.%s"),
        *MaterialTexturePath,
        *Spec.AssetName,
        *Spec.AssetName);
}

bool ParseTextureRecord(
    const FString& MaterialId,
    const FString& ExpectedMapType,
    const TSharedPtr<FJsonObject>& Record,
    FHeroTextureSpec& OutSpec,
    FString& OutError)
{
    int64 Width = 0;
    int64 Height = 0;
    int64 EdgeError = -1;
    FString MapType;
    FString Mode;
    const int32 ExpectedResolution =
        ExpectedMapType == TEXT("DetailNormal") ? 2048 :
        ExpectedMapType == TEXT("MacroVariation") ? 1024 :
        ExpectedMapType == TEXT("SurfaceMasks") ? 2048 :
        ExpectedMapType == TEXT("SharedWeatheringDecalMasks") ? 2048 :
        4096;
    const FString ExpectedMode =
        ExpectedMapType == TEXT("Height") ? TEXT("L") :
        (ExpectedMapType == TEXT("MacroVariation") ||
         ExpectedMapType == TEXT("SurfaceMasks") ||
         ExpectedMapType == TEXT("SharedWeatheringDecalMasks"))
            ? TEXT("RGBA")
            : TEXT("RGB");
    if (!Record.IsValid() ||
        !Record->TryGetStringField(TEXT("mapType"), MapType) ||
        MapType != ExpectedMapType ||
        !Record->TryGetStringField(TEXT("mode"), Mode) ||
        Mode != ExpectedMode ||
        !Record->TryGetStringField(TEXT("file"), OutSpec.SourceFilename) ||
        !Record->TryGetStringField(TEXT("sha256"), OutSpec.Sha256) ||
        !ReadInteger(Record, TEXT("bytes"), OutSpec.Bytes, OutError) ||
        !ReadInteger(Record, TEXT("widthPixels"), Width, OutError) ||
        !ReadInteger(Record, TEXT("heightPixels"), Height, OutError) ||
        !ReadInteger(Record, TEXT("edgeErrorMaximumByte"), EdgeError, OutError) ||
        OutSpec.SourceFilename != FPaths::GetCleanFilename(OutSpec.SourceFilename) ||
        OutSpec.SourceFilename.Contains(TEXT("..")) ||
        !OutSpec.SourceFilename.EndsWith(TEXT(".png"), ESearchCase::CaseSensitive) ||
        OutSpec.Sha256.Len() != 64 ||
        Width != ExpectedResolution || Height != ExpectedResolution ||
        EdgeError != 0)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("Generated material record '%s/%s' is invalid."),
                *MaterialId,
                *ExpectedMapType);
        }
        return false;
    }
    OutSpec.MaterialId = MaterialId;
    OutSpec.MapType = ExpectedMapType;
    OutSpec.AssetName = FPaths::GetBaseFilename(OutSpec.SourceFilename);
    OutSpec.Width = static_cast<int32>(Width);
    OutSpec.Height = static_cast<int32>(Height);
    OutSpec.Usage = TextureUsageFromMapType(ExpectedMapType);
    const FString FullPath = ProjectSourcePath(FPaths::Combine(
        MaterialSourceRoot,
        TEXT("Generated"),
        OutSpec.SourceFilename));
    return ValidateFrozenFile(
        FullPath,
        OutSpec.Bytes,
        OutSpec.Sha256,
        OutError);
}

bool ParseParameters(
    const TSharedPtr<FJsonObject>& Parameters,
    TMap<FName, float>& OutScalars,
    TMap<FName, FLinearColor>& OutVectors,
    FString& OutError)
{
    if (!Parameters.IsValid())
    {
        OutError = TEXT("Material instance parameters object is absent.");
        return false;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Parameters->Values)
    {
        if (!Pair.Value.IsValid())
        {
            OutError = TEXT("Material instance contains a null parameter value.");
            return false;
        }
        if (Pair.Value->Type == EJson::Number)
        {
            const double Value = Pair.Value->AsNumber();
            if (!FMath::IsFinite(Value))
            {
                OutError = TEXT("Material instance contains a non-finite scalar.");
                return false;
            }
            OutScalars.Add(FName(*Pair.Key), static_cast<float>(Value));
            continue;
        }
        if (Pair.Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Values = Pair.Value->AsArray();
            if (Values.Num() != 4)
            {
                OutError = FString::Printf(
                    TEXT("Material vector parameter '%s' must have four numbers."),
                    *Pair.Key);
                return false;
            }
            double Components[4] = {0.0, 0.0, 0.0, 0.0};
            for (int32 Index = 0; Index < 4; ++Index)
            {
                if (!Values[Index].IsValid() ||
                    Values[Index]->Type != EJson::Number ||
                    !FMath::IsFinite(Values[Index]->AsNumber()))
                {
                    OutError = FString::Printf(
                        TEXT("Material vector parameter '%s' contains a non-number."),
                        *Pair.Key);
                    return false;
                }
                Components[Index] = Values[Index]->AsNumber();
            }
            OutVectors.Add(
                FName(*Pair.Key),
                FLinearColor(
                    Components[0], Components[1],
                    Components[2], Components[3]));
            continue;
        }
        OutError = FString::Printf(
            TEXT("Material parameter '%s' has unsupported JSON type."),
            *Pair.Key);
        return false;
    }
    return true;
}

bool ParseStaticSwitches(
    const TSharedPtr<FJsonObject>& Switches,
    TMap<FName, bool>& OutSwitches,
    FString& OutError)
{
    if (!Switches.IsValid())
    {
        OutError = TEXT("Material static-switch object is absent.");
        return false;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Switches->Values)
    {
        if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Boolean)
        {
            OutError = FString::Printf(
                TEXT("Static switch '%s' must be Boolean."),
                *Pair.Key);
            return false;
        }
        OutSwitches.Add(FName(*Pair.Key), Pair.Value->AsBool());
    }
    return true;
}

bool ValidateSourceManifestRecords(
    const TSharedPtr<FJsonObject>& Manifest,
    FString& OutError)
{
    if (!ReadExactString(
            Manifest,
            TEXT("schema"),
            TEXT("triad.istana_hero_material_sources.v2"),
            OutError) ||
        !ReadExactBool(Manifest, TEXT("exactRoster"), true, OutError))
    {
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("sources"), Rows) ||
        !Rows || Rows->Num() != 5)
    {
        OutError = TEXT("Hero material source manifest requires exactly five sources.");
        return false;
    }
    TSet<FString> SeenFiles;
    for (const TSharedPtr<FJsonValue>& Value : *Rows)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FString Filename;
        FString Digest;
        int64 Bytes = 0;
        if (!Row.IsValid() ||
            !Row->TryGetStringField(TEXT("file"), Filename) ||
            !Row->TryGetStringField(TEXT("sha256"), Digest) ||
            !ReadInteger(Row, TEXT("bytes"), Bytes, OutError) ||
            Filename != FPaths::GetCleanFilename(Filename) ||
            SeenFiles.Contains(Filename) || Digest.Len() != 64)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Hero material source manifest contains an unsafe or duplicate record.");
            }
            return false;
        }
        SeenFiles.Add(Filename);
        if (!ValidateFrozenFile(
                ProjectSourcePath(FPaths::Combine(
                    MaterialSourceRoot, TEXT("Source"), Filename)),
                Bytes,
                Digest,
                OutError))
        {
            return false;
        }
    }
    return true;
}

bool ParseMaterialInterface(
    const TSharedPtr<FJsonObject>& Interface,
    TArray<FHeroMaterialInstanceSpec>& OutInstances,
    FString& OutError)
{
    if (!ReadExactString(
            Interface,
            TEXT("schema"),
            TEXT("triad.istana_hero_material_unreal_interface.v2"),
            OutError) ||
        !ReadExactString(
            Interface,
            TEXT("scope"),
            TEXT("BUILDING_HERO_VISUAL_ONLY"),
            OutError) ||
        !ReadExactString(
            Interface,
            TEXT("contentRoot"),
            TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV2"),
            OutError))
    {
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!Interface->TryGetArrayField(TEXT("instances"), Rows) ||
        !Rows || Rows->Num() != 8)
    {
        OutError = TEXT("Hero material interface requires exactly eight textured instances.");
        return false;
    }
    TSet<FString> SeenAssets;
    TSet<FString> SeenSlots;
    OutInstances.Reset();
    for (const TSharedPtr<FJsonValue>& Value : *Rows)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FHeroMaterialInstanceSpec Spec;
        const TSharedPtr<FJsonObject>* Parameters = nullptr;
        if (!Row.IsValid() ||
            !Row->TryGetStringField(TEXT("id"), Spec.Id) ||
            !Row->TryGetStringField(TEXT("asset"), Spec.AssetName) ||
            !Row->TryGetStringField(TEXT("parent"), Spec.ParentAssetName) ||
            !ReadStringArray(Row, TEXT("replaceSlotsV2"), Spec.V2Slots, OutError) ||
            !Row->TryGetObjectField(TEXT("parameters"), Parameters) ||
            !Parameters || !(*Parameters).IsValid() ||
            !ParseParameters(*Parameters, Spec.Scalars, Spec.Vectors, OutError) ||
            Spec.Id.IsEmpty() || Spec.AssetName.IsEmpty() ||
            SeenAssets.Contains(Spec.AssetName) || Spec.V2Slots.Num() < 1)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Hero material interface contains an invalid textured instance.");
            }
            return false;
        }
        SeenAssets.Add(Spec.AssetName);
        for (const FString& Slot : Spec.V2Slots)
        {
            if (!ExpectedHeroSlots().Contains(Slot) ||
                SeenSlots.Contains(Slot) ||
                ExpectedSlotInstances().FindRef(Slot) != Spec.AssetName)
            {
                OutError = FString::Printf(
                    TEXT("Hero material interface has invalid or duplicate v2 slot alias '%s'."),
                    *Slot);
                return false;
            }
            SeenSlots.Add(Slot);
        }
        if (Spec.ParentAssetName != TEXT("M_IPV_HeroSurface_V2") &&
            Spec.ParentAssetName != TEXT("M_IPV_HeroGlass_V2"))
        {
            OutError = TEXT("Hero material instance parent escaped the two frozen v2 masters.");
            return false;
        }
        Spec.StaticSwitches = {
            {TEXT("UseTextureSet"), true},
            {TEXT("UseBumpOffset"), false},
            {TEXT("UseGenericWeatheringDecals"), false},
            {TEXT("UseExposedMetalMask"), false}};
        if (Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V2"))
        {
            Spec.StaticSwitches.Reset();
        }
        OutInstances.Add(MoveTemp(Spec));
    }

    const TSharedPtr<FJsonObject>* V2SpecialAliases = nullptr;
    const TSharedPtr<FJsonObject>* SpecialBindings = nullptr;
    if (!Interface->TryGetObjectField(
            TEXT("v2SpecialSlotBindings"), V2SpecialAliases) ||
        !V2SpecialAliases || !(*V2SpecialAliases).IsValid() ||
        !Interface->TryGetObjectField(
            TEXT("specialSlotBindings"), SpecialBindings) ||
        !SpecialBindings || !(*SpecialBindings).IsValid())
    {
        OutError = TEXT("Hero material interface lacks special v2 slot bindings.");
        return false;
    }
    const TPair<const TCHAR*, const TCHAR*> SpecialRows[] = {
        {TEXT("M_IPV2_Opaline"), TEXT("M_IPV_Opaline")},
        {TEXT("M_IPV2_Recess"), TEXT("M_IPV_Recess")}};
    for (const TPair<const TCHAR*, const TCHAR*>& Pair : SpecialRows)
    {
        FString AssetName;
        if (!(*V2SpecialAliases)->TryGetStringField(Pair.Key, AssetName) ||
            AssetName != ExpectedSlotInstances().FindRef(Pair.Key) ||
            SeenSlots.Contains(Pair.Key))
        {
            OutError = FString::Printf(
                TEXT("Special v2 alias '%s' changed."),
                Pair.Key);
            return false;
        }
        const TSharedPtr<FJsonObject>* Binding = nullptr;
        if (!(*SpecialBindings)->TryGetObjectField(Pair.Value, Binding) ||
            !Binding || !(*Binding).IsValid())
        {
            OutError = FString::Printf(
                TEXT("Special source binding '%s' is absent."),
                Pair.Value);
            return false;
        }
        FHeroMaterialInstanceSpec Spec;
        Spec.Id = Pair.Key;
        Spec.AssetName = AssetName;
        Spec.ParentAssetName = TEXT("M_IPV_HeroSurface_V2");
        Spec.V2Slots = {Pair.Key};
        Spec.bSpecialParameterOnly = true;
        const TArray<TSharedPtr<FJsonValue>>* BaseColor = nullptr;
        double Roughness = 0.0;
        double Metallic = 0.0;
        const TSharedPtr<FJsonObject>* Switches = nullptr;
        if (!(*Binding)->TryGetArrayField(TEXT("baseColorLinear"), BaseColor) ||
            !BaseColor || BaseColor->Num() != 4 ||
            !(*Binding)->TryGetNumberField(TEXT("roughness"), Roughness) ||
            !(*Binding)->TryGetNumberField(TEXT("metallic"), Metallic) ||
            !FMath::IsFinite(Roughness) || !FMath::IsFinite(Metallic) ||
            !(*Binding)->TryGetObjectField(TEXT("staticSwitches"), Switches) ||
            !Switches ||
            !ParseStaticSwitches(*Switches, Spec.StaticSwitches, OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("Special material binding '%s' is invalid."),
                    Pair.Value);
            }
            return false;
        }
        double Components[4] = {0.0, 0.0, 0.0, 0.0};
        for (int32 Index = 0; Index < 4; ++Index)
        {
            if (!(*BaseColor)[Index].IsValid() ||
                (*BaseColor)[Index]->Type != EJson::Number ||
                !FMath::IsFinite((*BaseColor)[Index]->AsNumber()))
            {
                OutError = TEXT("Special material base colour contains a non-number.");
                return false;
            }
            Components[Index] = (*BaseColor)[Index]->AsNumber();
        }
        Spec.Vectors.Add(
            TEXT("LookdevTint"),
            FLinearColor(
                Components[0], Components[1],
                Components[2], Components[3]));
        // The opaque master uses these two frozen scalar inputs as its
        // texture-disabled roughness and metallic values.
        Spec.Scalars.Add(TEXT("RoughnessBias"), Roughness);
        Spec.Scalars.Add(TEXT("ExposedMetalMaskStrength"), Metallic);
        SeenSlots.Add(Pair.Key);
        OutInstances.Add(MoveTemp(Spec));
    }
    if (SeenSlots.Num() != ExpectedHeroSlots().Num())
    {
        OutError = TEXT("Hero material interface does not bind all eleven M_IPV2 slots.");
        return false;
    }
    return true;
}

bool ValidateMaterialSource(
    FHeroMaterialPack& OutPack,
    FString& OutReport)
{
    FString Error;
    TSharedPtr<FJsonObject> Contract;
    TSharedPtr<FJsonObject> Interface;
    TSharedPtr<FJsonObject> SourceManifest;
    TSharedPtr<FJsonObject> GeneratedManifest;
    if (!LoadJsonObject(ProjectSourcePath(MaterialContractRelativePath), Contract, Error) ||
        !LoadJsonObject(ProjectSourcePath(MaterialInterfaceRelativePath), Interface, Error) ||
        !LoadJsonObject(ProjectSourcePath(MaterialSourceManifestRelativePath), SourceManifest, Error) ||
        !LoadJsonObject(ProjectSourcePath(MaterialGeneratedManifestRelativePath), GeneratedManifest, Error) ||
        !ReadExactString(
            Contract,
            TEXT("schema"),
            TEXT("triad.istana_hero_materials.v2"),
            Error))
    {
        OutReport = TEXT("V2_MATERIAL_CONTRACT_INVALID: ") + Error;
        return false;
    }
    const TSharedPtr<FJsonObject>* Scope = nullptr;
    if (!Contract->TryGetObjectField(TEXT("scope"), Scope) ||
        !Scope || !(*Scope).IsValid() ||
        !ReadExactBool(*Scope, TEXT("buildingHeroOnly"), true, Error) ||
        !ReadExactBool(*Scope, TEXT("changesCollision"), false, Error) ||
        !ReadExactBool(*Scope, TEXT("changesSensorTruth"), false, Error))
    {
        OutReport = TEXT("V2_MATERIAL_CONTRACT_INVALID: hero-only scope changed. ") + Error;
        return false;
    }
    TArray<FString> RequiredV2;
    TArray<FString> RequiredSpecialV2;
    if (!ReadStringArray(*Scope, TEXT("requiredV2Slots"), RequiredV2, Error) ||
        !ReadStringArray(*Scope, TEXT("requiredV2SpecialSlots"), RequiredSpecialV2, Error))
    {
        OutReport = TEXT("V2_MATERIAL_CONTRACT_INVALID: ") + Error;
        return false;
    }
    RequiredV2.Append(RequiredSpecialV2);
    if (!HasExactStringSet(RequiredV2, ExpectedHeroSlots()) ||
        !ValidateSourceManifestRecords(SourceManifest, Error) ||
        !ParseMaterialInterface(Interface, OutPack.Instances, Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("schema"),
            TEXT("triad.istana_hero_material_generated_pack.v2"),
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("scope"),
            TEXT("BUILDING_HERO_VISUAL_ONLY"),
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("sourceManifestSha256"),
            TEXT("9d9046ceb8cfee17e368e4832ffeaf0ba71194e2ac6797a68c20549409721c83"),
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("contractSha256"),
            TEXT("65deb423a0e1d7ffda981a4aad3fb80a8cff840a3ca573eab290076f95c49b2c"),
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("unrealAssetInterfaceSha256"),
            TEXT("7a0df5e20c47ebed6c5b2299f44f9658aeaeed9747f740038edce6d2e3c46277"),
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("builderSha256"),
            TEXT("d429b1fcfac20a3cc96f0c64fb82e67adad85b54ee0ba06befbc914e6d69e500"),
            Error) ||
        !ReadExactBool(GeneratedManifest, TEXT("measuredMaterialScan"), false, Error) ||
        !ReadExactBool(GeneratedManifest, TEXT("measuredColor"), false, Error) ||
        !ReadExactBool(GeneratedManifest, TEXT("photogrammetry"), false, Error) ||
        !ReadExactBool(
            GeneratedManifest,
            TEXT("siteSpecificWeatheringPlacement"),
            false,
            Error))
    {
        OutReport = TEXT("V2_MATERIAL_PACK_INVALID: ") + Error;
        return false;
    }
    const TSharedPtr<FJsonObject>* Resolutions = nullptr;
    int64 TileResolution = 0;
    int64 DetailResolution = 0;
    int64 MacroResolution = 0;
    int64 MaskResolution = 0;
    int64 DecalResolution = 0;
    if (!GeneratedManifest->TryGetObjectField(TEXT("resolutions"), Resolutions) ||
        !Resolutions || !(*Resolutions).IsValid() ||
        !ReadInteger(*Resolutions, TEXT("tile"), TileResolution, Error) ||
        !ReadInteger(*Resolutions, TEXT("detail"), DetailResolution, Error) ||
        !ReadInteger(*Resolutions, TEXT("macro"), MacroResolution, Error) ||
        !ReadInteger(*Resolutions, TEXT("mask"), MaskResolution, Error) ||
        !ReadInteger(*Resolutions, TEXT("decal"), DecalResolution, Error) ||
        TileResolution != 4096 || DetailResolution != 2048 ||
        MacroResolution != 1024 || MaskResolution != 2048 ||
        DecalResolution != 2048)
    {
        OutReport = TEXT("V2_MATERIAL_PACK_INVALID: exact generated resolutions changed. ") + Error;
        return false;
    }

    const TArray<FString> ExpectedMapTypes = {
        TEXT("BaseColor"), TEXT("Normal"), TEXT("ORM"), TEXT("Height"),
        TEXT("DetailNormal"), TEXT("MacroVariation"), TEXT("SurfaceMasks")};
    const TSet<FString> ExpectedMaterialIds = {
        TEXT("Render"), TEXT("Trim"), TEXT("Slate"), TEXT("Louvre"),
        TEXT("Stone"), TEXT("Timber"), TEXT("PaintedMetal"), TEXT("Glass")};
    const TArray<TSharedPtr<FJsonValue>>* MaterialRows = nullptr;
    if (!GeneratedManifest->TryGetArrayField(TEXT("materials"), MaterialRows) ||
        !MaterialRows || MaterialRows->Num() != ExpectedMaterialIds.Num())
    {
        OutReport = TEXT("V2_MATERIAL_PACK_INVALID: generated material roster is incomplete.");
        return false;
    }
    TSet<FString> SeenIds;
    TSet<FString> SeenFiles;
    OutPack.Textures.Reset();
    for (const TSharedPtr<FJsonValue>& Value : *MaterialRows)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FString Id;
        FString TargetInstance;
        TArray<FString> GeneratedV2Slots;
        const TSharedPtr<FJsonObject>* Outputs = nullptr;
        if (!Row.IsValid() ||
            !Row->TryGetStringField(TEXT("id"), Id) ||
            !ExpectedMaterialIds.Contains(Id) || SeenIds.Contains(Id) ||
            !Row->TryGetStringField(TEXT("targetInstance"), TargetInstance) ||
            !ReadStringArray(Row, TEXT("v2Slots"), GeneratedV2Slots, Error) ||
            !Row->TryGetObjectField(TEXT("outputs"), Outputs) ||
            !Outputs || !(*Outputs).IsValid() ||
            (*Outputs)->Values.Num() != ExpectedMapTypes.Num())
        {
            OutReport = TEXT("V2_MATERIAL_PACK_INVALID: generated material row is unsafe or duplicated.");
            return false;
        }
        const FHeroMaterialInstanceSpec* InterfaceSpec =
            OutPack.Instances.FindByPredicate(
                [&Id](const FHeroMaterialInstanceSpec& Candidate)
                {
                    return !Candidate.bSpecialParameterOnly && Candidate.Id == Id;
                });
        if (!InterfaceSpec || InterfaceSpec->AssetName != TargetInstance ||
            !HasExactStringSet(GeneratedV2Slots, InterfaceSpec->V2Slots))
        {
            OutReport = FString::Printf(
                TEXT("V2_MATERIAL_PACK_INVALID: generated/interface binding differs for '%s'."),
                *Id);
            return false;
        }
        SeenIds.Add(Id);
        for (const FString& MapType : ExpectedMapTypes)
        {
            const TSharedPtr<FJsonObject>* Record = nullptr;
            FHeroTextureSpec Spec;
            if (!(*Outputs)->TryGetObjectField(MapType, Record) ||
                !Record || !(*Record).IsValid() ||
                !ParseTextureRecord(Id, MapType, *Record, Spec, Error) ||
                SeenFiles.Contains(Spec.SourceFilename))
            {
                OutReport = TEXT("V2_MATERIAL_PACK_INVALID: ") + Error;
                return false;
            }
            SeenFiles.Add(Spec.SourceFilename);
            OutPack.Textures.Add(MoveTemp(Spec));
        }
    }
    const TSharedPtr<FJsonObject>* SharedOutputs = nullptr;
    const TSharedPtr<FJsonObject>* SharedRecord = nullptr;
    FHeroTextureSpec SharedSpec;
    if (!GeneratedManifest->TryGetObjectField(TEXT("sharedOutputs"), SharedOutputs) ||
        !SharedOutputs || !(*SharedOutputs).IsValid() ||
        !(*SharedOutputs)->TryGetObjectField(
            TEXT("SharedWeatheringDecalMasks"), SharedRecord) ||
        !SharedRecord || !(*SharedRecord).IsValid() ||
        !ParseTextureRecord(
            TEXT("Shared"),
            TEXT("SharedWeatheringDecalMasks"),
            *SharedRecord,
            SharedSpec,
            Error) ||
        SeenFiles.Contains(SharedSpec.SourceFilename))
    {
        OutReport = TEXT("V2_MATERIAL_PACK_INVALID: shared weathering map changed. ") + Error;
        return false;
    }
    SeenFiles.Add(SharedSpec.SourceFilename);
    OutPack.Textures.Add(MoveTemp(SharedSpec));
    int64 OutputCount = 0;
    if (!ReadInteger(GeneratedManifest, TEXT("outputCount"), OutputCount, Error) ||
        OutputCount != ExpectedMaterialTextureCount ||
        OutPack.Textures.Num() != ExpectedMaterialTextureCount)
    {
        OutReport = TEXT("V2_MATERIAL_PACK_INVALID: exactly 57 generated maps are required.");
        return false;
    }
    TArray<FString> DiskPngs;
    IFileManager::Get().FindFiles(
        DiskPngs,
        *ProjectSourcePath(FPaths::Combine(MaterialSourceRoot, TEXT("Generated/*.png"))),
        true,
        false);
    TSet<FString> DiskPngSet;
    for (const FString& Filename : DiskPngs)
    {
        DiskPngSet.Add(Filename);
    }
    if (!AreSetsEqual(DiskPngSet, SeenFiles))
    {
        OutReport = TEXT("V2_MATERIAL_PACK_INVALID: Generated PNG roster differs from the exact manifest.");
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Validated exact HeroMaterialsV2 source, interface, ten instances, and %d generated maps without measured-material claims."),
        OutPack.Textures.Num());
    return true;
}

bool ValidateCompleteV2SourceSet(
    FHeroGeometryContract& OutGeometry,
    FHeroMaterialPack& OutMaterials,
    FString& OutReport)
{
    FString FreezeReport;
    FString GeometryReport;
    FString MaterialReport;
    if (!ValidateIntegrationFreeze(FreezeReport) ||
        !ValidateGeometrySource(OutGeometry, GeometryReport) ||
        !ValidateMaterialSource(OutMaterials, MaterialReport))
    {
        OutReport = !FreezeReport.IsEmpty() &&
            !FreezeReport.StartsWith(TEXT("Validated"))
            ? FreezeReport
            : (!GeometryReport.IsEmpty() &&
                !GeometryReport.StartsWith(TEXT("Validated"))
                ? GeometryReport
                : MaterialReport);
        return false;
    }
    OutReport = FreezeReport + TEXT(" ") + GeometryReport + TEXT(" ") + MaterialReport;
    return true;
}

bool IsEditorOperationSafe(FString& OutError)
{
    if (!GEditor)
    {
        OutError = TEXT("Unreal Editor is unavailable.");
        return false;
    }
    if (GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
    {
        OutError = TEXT("Stop Play-In-Editor before running hero-v2 editor operations.");
        return false;
    }
    TArray<UPackage*> DirtyMaps;
    TArray<UPackage*> DirtyContent;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContent);
    if (DirtyMaps.Num() > 0 || DirtyContent.Num() > 0)
    {
        TArray<FString> Names;
        for (UPackage* Package : DirtyMaps)
        {
            Names.Add(Package ? Package->GetName() : TEXT("<unknown map>"));
        }
        for (UPackage* Package : DirtyContent)
        {
            Names.Add(Package ? Package->GetName() : TEXT("<unknown content>"));
        }
        Names.Sort();
        OutError = TEXT("Refusing hero-v2 mutation while packages are dirty: ") +
            FString::Join(Names, TEXT(", "));
        return false;
    }
    OutError.Reset();
    return true;
}

bool DoesObjectOrPackageExist(
    UEditorAssetSubsystem* AssetSubsystem,
    const FString& ObjectPath)
{
    return AssetSubsystem &&
        (AssetSubsystem->DoesAssetExist(ObjectPath) ||
         FPackageName::DoesPackageExist(
             FPackageName::ObjectPathToPackageName(ObjectPath)));
}

bool ValidateSingleSourceProvenance(
    const UAssetImportData* ImportData,
    const FString& ExpectedFilename,
    const FString& AssetLabel,
    FString& OutError)
{
    FString ExpectedSource = FPaths::ConvertRelativePathToFull(ExpectedFilename);
    FPaths::NormalizeFilename(ExpectedSource);
    const TArray<FString> Filenames = ImportData
        ? ImportData->ExtractFilenames()
        : TArray<FString>();
    if (!ImportData || Filenames.Num() != 1 ||
        ImportData->GetSourceData().SourceFiles.Num() != 1)
    {
        OutError = FString::Printf(
            TEXT("%s must retain exactly one frozen source provenance record."),
            *AssetLabel);
        return false;
    }
    FString ActualSource = FPaths::ConvertRelativePathToFull(Filenames[0]);
    FPaths::NormalizeFilename(ActualSource);
    const FMD5Hash CurrentHash = FMD5Hash::HashFile(*ExpectedSource);
    const FMD5Hash& ImportedHash =
        ImportData->GetSourceData().SourceFiles[0].FileHash;
    if (!FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        !CurrentHash.IsValid() || !ImportedHash.IsValid() ||
        CurrentHash != ImportedHash)
    {
        OutError = FString::Printf(
            TEXT("%s import path or stored source-content MD5 is stale."),
            *AssetLabel);
        return false;
    }
    return true;
}

const TCHAR* HeroPayloadSealSchema =
    TEXT("triad.istana_public_view_hero_v2.embedded_payload.v1");
const TCHAR* HeroPayloadSchemaMetadataKey = TEXT("TRIADHeroV2PayloadSchema");
const TCHAR* HeroPayloadKindMetadataKey = TEXT("TRIADHeroV2PayloadKind");
const TCHAR* HeroPayloadDigestMetadataKey = TEXT("TRIADHeroV2PayloadDigest");

bool SealEmbeddedPayload(
    UObject* Asset,
    const TCHAR* Kind,
    const FString& Digest,
    FString& OutError)
{
    if (!Asset || !Asset->GetOutermost() || Digest.IsEmpty())
    {
        OutError = TEXT("Cannot seal an absent or empty v2 embedded payload.");
        return false;
    }
    UMetaData* MetaData = Asset->GetOutermost()->GetMetaData();
    if (!MetaData)
    {
        OutError = TEXT("Could not allocate v2 embedded-payload metadata.");
        return false;
    }
    MetaData->SetValue(
        Asset, HeroPayloadSchemaMetadataKey, HeroPayloadSealSchema);
    MetaData->SetValue(Asset, HeroPayloadKindMetadataKey, Kind);
    MetaData->SetValue(Asset, HeroPayloadDigestMetadataKey, *Digest);
    return true;
}

bool ValidateEmbeddedPayloadSeal(
    UObject* Asset,
    const TCHAR* ExpectedKind,
    const FString& CurrentDigest,
    FString& OutError)
{
    UMetaData* MetaData = Asset && Asset->GetOutermost()
        ? Asset->GetOutermost()->GetMetaData()
        : nullptr;
    const FString StoredSchema = MetaData
        ? MetaData->GetValue(Asset, HeroPayloadSchemaMetadataKey)
        : FString();
    const FString StoredKind = MetaData
        ? MetaData->GetValue(Asset, HeroPayloadKindMetadataKey)
        : FString();
    const FString StoredDigest = MetaData
        ? MetaData->GetValue(Asset, HeroPayloadDigestMetadataKey)
        : FString();
    if (StoredSchema != HeroPayloadSealSchema ||
        StoredKind != ExpectedKind || CurrentDigest.IsEmpty() ||
        StoredDigest != CurrentDigest)
    {
        OutError = FString::Printf(
            TEXT("V2 asset '%s' embedded-payload seal is absent or stale."),
            Asset ? *Asset->GetPathName() : TEXT("<null>"));
        return false;
    }
    return true;
}

bool ValidateHeroTexture(
    UTexture2D* Texture,
    const FHeroTextureSpec& Spec,
    FString& OutError)
{
    if (!Texture ||
        Texture->GetPathName() != TextureObjectPath(Spec) ||
        Texture->Source.GetSizeX() != Spec.Width ||
        Texture->Source.GetSizeY() != Spec.Height ||
        static_cast<bool>(Texture->SRGB) != TextureIsSrgb(Spec.Usage) ||
        Texture->CompressionSettings != TextureCompression(Spec.Usage) ||
        Texture->LODGroup != TextureLodGroup(Spec.Usage) ||
        Texture->MipGenSettings != TMGS_FromTextureGroup ||
        Texture->AddressX != TA_Wrap || Texture->AddressY != TA_Wrap ||
        // UE 5.5 has no TF_Anisotropic texture-asset enum. TF_Default is the
        // engine-supported route to the texture group's anisotropic sampler.
        Texture->Filter != TF_Default ||
        Texture->bFlipGreenChannel ||
        Texture->VirtualTextureStreaming ||
        Texture->LODBias != 0 || Texture->MaxTextureSize != 0 ||
        Texture->NeverStream ||
        Texture->PowerOfTwoMode != ETexturePowerOfTwoSetting::None ||
        Texture->CompressionNoAlpha)
    {
        OutError = FString::Printf(
            TEXT("Texture '%s' violates the frozen resolution/color-space/compression/streaming/alpha contract."),
            *TextureObjectPath(Spec));
        return false;
    }
    const FString FrozenPng = ProjectSourcePath(FPaths::Combine(
        MaterialSourceRoot,
        TEXT("Generated"),
        Spec.SourceFilename));
    if (!ValidateSingleSourceProvenance(
            Texture->AssetImportData,
            FrozenPng,
            TextureObjectPath(Spec),
            OutError))
    {
        return false;
    }
    if (!ValidateEmbeddedPayloadSeal(
            Texture,
            TEXT("texture_source_id"),
            Texture->Source.GetIdString(),
            OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

template <typename TExpression>
TExpression* CreateExpression(
    UMaterial* Material,
    int32 X,
    int32 Y)
{
    TExpression* Expression = Cast<TExpression>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material,
            TExpression::StaticClass(),
            X,
            Y));
    if (Expression)
    {
        Expression->Desc = FString::Printf(
            TEXT("TRIAD_IPV2_NODE_%d_%d_%s"),
            X,
            Y,
            *TExpression::StaticClass()->GetName());
    }
    return Expression;
}

UMaterialExpressionScalarParameter* CreateScalarParameter(
    UMaterial* Material,
    const FName& Name,
    float DefaultValue,
    int32 X,
    int32 Y)
{
    UMaterialExpressionScalarParameter* Parameter =
        CreateExpression<UMaterialExpressionScalarParameter>(Material, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = Name;
        Parameter->DefaultValue = DefaultValue;
        Parameter->Group = TEXT("Istana Hero V2");
    }
    return Parameter;
}

UMaterialExpressionVectorParameter* CreateVectorParameter(
    UMaterial* Material,
    const FName& Name,
    const FLinearColor& DefaultValue,
    int32 X,
    int32 Y)
{
    UMaterialExpressionVectorParameter* Parameter =
        CreateExpression<UMaterialExpressionVectorParameter>(Material, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = Name;
        Parameter->DefaultValue = DefaultValue;
        Parameter->Group = TEXT("Istana Hero V2");
    }
    return Parameter;
}

UMaterialExpressionStaticBoolParameter* CreateStaticBoolParameter(
    UMaterial* Material,
    const FName& Name,
    bool DefaultValue,
    int32 X,
    int32 Y)
{
    UMaterialExpressionStaticBoolParameter* Parameter =
        CreateExpression<UMaterialExpressionStaticBoolParameter>(Material, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = Name;
        Parameter->DefaultValue = DefaultValue;
        Parameter->Group = TEXT("Istana Hero V2");
    }
    return Parameter;
}

UMaterialExpressionStaticSwitch* CreateStaticSwitch(
    UMaterial* Material,
    int32 X,
    int32 Y)
{
    return CreateExpression<UMaterialExpressionStaticSwitch>(Material, X, Y);
}

bool CustomCodeUsesIdentifier(
    const FString& Code,
    const FString& Identifier)
{
    if (Code.IsEmpty() || Identifier.IsEmpty())
    {
        return false;
    }
    int32 SearchFrom = 0;
    while (SearchFrom < Code.Len())
    {
        const int32 Found = Code.Find(
            Identifier,
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            SearchFrom);
        if (Found == INDEX_NONE)
        {
            return false;
        }
        const int32 After = Found + Identifier.Len();
        const bool bStartsAtIdentifierBoundary =
            Found == 0 ||
            (!FChar::IsAlnum(Code[Found - 1]) && Code[Found - 1] != TEXT('_'));
        const bool bEndsAtIdentifierBoundary =
            After == Code.Len() ||
            (!FChar::IsAlnum(Code[After]) && Code[After] != TEXT('_'));
        if (bStartsAtIdentifierBoundary && bEndsAtIdentifierBoundary)
        {
            return true;
        }
        SearchFrom = Found + 1;
    }
    return false;
}

UMaterialExpressionCustom* CreateCustomExpression(
    UMaterial* Material,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    const TArray<FName>& InputNames,
    int32 X,
    int32 Y)
{
    UMaterialExpressionCustom* Expression =
        CreateExpression<UMaterialExpressionCustom>(Material, X, Y);
    if (!Expression)
    {
        return nullptr;
    }
    Expression->Description = Description;
    Expression->Code = Code;
    Expression->OutputType = OutputType;
    // UE 5.5's UMaterialExpressionCustom constructor seeds one unnamed input.
    // Replace that editor placeholder; appending leaves a disconnected input 0
    // even though ConnectMaterialExpressions succeeds for every named pin.
    Expression->Inputs.Reset(InputNames.Num());
    for (const FName& InputName : InputNames)
    {
        FCustomInput& Input = Expression->Inputs.AddDefaulted_GetRef();
        Input.InputName = InputName;
    }
    return Expression;
}

UMaterialExpressionTextureSampleParameter2D* CreateTextureParameter(
    UMaterial* Material,
    const FName& Name,
    EMaterialSamplerType Sampler,
    int32 X,
    int32 Y)
{
    UMaterialExpressionTextureSampleParameter2D* Parameter =
        CreateExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = Name;
        Parameter->Group = TEXT("Istana Hero V2");
        Parameter->SamplerType = Sampler;
        Parameter->MipValueMode = TMVM_None;
        Parameter->SamplerSource = SSM_FromTextureAsset;
        Parameter->ConstMipValue = 0;
        Parameter->AutomaticViewMipBias = true;
    }
    return Parameter;
}

bool ConnectExpression(
    UMaterialExpression* From,
    const TCHAR* Output,
    UMaterialExpression* To,
    const TCHAR* Input,
    FString& OutError)
{
    if (!From || !To ||
        !UMaterialEditingLibrary::ConnectMaterialExpressions(
            From, FString(Output), To, FString(Input)))
    {
        OutError = FString::Printf(
            TEXT("Could not connect v2 material graph edge '%s' -> '%s'."),
            Output,
            Input);
        return false;
    }
    return true;
}

bool ConnectProperty(
    UMaterialExpression* From,
    const TCHAR* Output,
    EMaterialProperty Property,
    FString& OutError)
{
    if (!From ||
        !UMaterialEditingLibrary::ConnectMaterialProperty(
            From, FString(Output), Property))
    {
        OutError = FString::Printf(
            TEXT("Could not connect v2 material property %d."),
            static_cast<int32>(Property));
        return false;
    }
    return true;
}

UTexture2D* FindTextureForMaterial(
    const FString& MaterialId,
    const FString& MapType,
    const FHeroMaterialPack& Pack,
    const TMap<FName, UTexture2D*>& Textures);
bool SealHeroMaterialGraph(UMaterial* Material, FString& OutError);

UMaterial* CreateOpaqueHeroMaster(
    const FHeroMaterialPack& Pack,
    const TMap<FName, UTexture2D*>& Textures,
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            TEXT("M_IPV_HeroSurface_V2"),
            MaterialAssetRoot,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaPublicViewHeroV2Assets"))))
        : nullptr;
    if (!Material)
    {
        OutError = TEXT("Could not create M_IPV_HeroSurface_V2.");
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;

    UMaterialExpressionTextureCoordinate* Uv0 =
        CreateExpression<UMaterialExpressionTextureCoordinate>(Material, -2200, 0);
    UMaterialExpressionScalarParameter* TileMeters =
        CreateScalarParameter(Material, TEXT("TileMeters"), 1.0f, -2200, 150);
    UMaterialExpressionScalarParameter* DetailTileMeters =
        CreateScalarParameter(Material, TEXT("DetailTileMeters"), 0.15f, -2200, 300);
    UMaterialExpressionScalarParameter* MacroTileMeters =
        CreateScalarParameter(Material, TEXT("MacroTileMeters"), 8.0f, -2200, 450);
    UMaterialExpressionDivide* TileUv =
        CreateExpression<UMaterialExpressionDivide>(Material, -1950, 0);
    UMaterialExpressionDivide* DetailUv =
        CreateExpression<UMaterialExpressionDivide>(Material, -1950, 250);
    UMaterialExpressionDivide* MacroUv =
        CreateExpression<UMaterialExpressionDivide>(Material, -1950, 500);
    UMaterialExpressionTextureSampleParameter2D* BaseColor =
        CreateTextureParameter(Material, TEXT("Tex_BaseColor"), SAMPLERTYPE_Color, -1250, -600);
    UMaterialExpressionTextureSampleParameter2D* Normal =
        CreateTextureParameter(Material, TEXT("Tex_Normal"), SAMPLERTYPE_Normal, -1250, -400);
    UMaterialExpressionTextureSampleParameter2D* Orm =
        CreateTextureParameter(Material, TEXT("Tex_ORM"), SAMPLERTYPE_Masks, -1250, -200);
    UMaterialExpressionTextureSampleParameter2D* Height =
        CreateTextureParameter(Material, TEXT("Tex_Height"), SAMPLERTYPE_LinearGrayscale, -1700, 50);
    UMaterialExpressionTextureSampleParameter2D* DetailNormal =
        CreateTextureParameter(Material, TEXT("Tex_DetailNormal"), SAMPLERTYPE_Normal, -1250, 250);
    UMaterialExpressionTextureSampleParameter2D* Macro =
        CreateTextureParameter(Material, TEXT("Tex_MacroVariation"), SAMPLERTYPE_Masks, -1250, 450);
    UMaterialExpressionTextureSampleParameter2D* SurfaceMasks =
        CreateTextureParameter(Material, TEXT("Tex_SurfaceMasks"), SAMPLERTYPE_Masks, -1250, 650);
    UMaterialExpressionTextureSampleParameter2D* SharedMasks =
        CreateTextureParameter(Material, TEXT("Tex_SharedWeatheringDecalMasks"), SAMPLERTYPE_Masks, -1250, 850);
    UMaterialExpressionVectorParameter* Tint =
        CreateVectorParameter(Material, TEXT("LookdevTint"), FLinearColor::White, -950, -650);
    UMaterialExpressionMultiply* TintedBase =
        CreateExpression<UMaterialExpressionMultiply>(Material, -650, -600);
    UMaterialExpressionScalarParameter* RoughnessBias =
        CreateScalarParameter(Material, TEXT("RoughnessBias"), 0.0f, -950, -200);
    UMaterialExpressionScalarParameter* MetallicFallback =
        CreateScalarParameter(Material, TEXT("ExposedMetalMaskStrength"), 0.0f, -950, -50);
    UMaterialExpressionScalarParameter* DetailStrength =
        CreateScalarParameter(Material, TEXT("DetailNormalStrength"), 0.5f, -950, 200);
    UMaterialExpressionConstant3Vector* FlatNormal =
        CreateExpression<UMaterialExpressionConstant3Vector>(Material, -350, 350);
    UMaterialExpressionConstant* One =
        CreateExpression<UMaterialExpressionConstant>(Material, -950, 1050);
    UMaterialExpressionConstant* Zero =
        CreateExpression<UMaterialExpressionConstant>(Material, -950, 1150);
    UMaterialExpressionStaticBoolParameter* UseTextureSet =
        CreateStaticBoolParameter(Material, TEXT("UseTextureSet"), true, -650, 1050);
    UMaterialExpressionStaticBoolParameter* UseBumpOffset =
        CreateStaticBoolParameter(Material, TEXT("UseBumpOffset"), false, -2200, 700);
    UMaterialExpressionStaticBoolParameter* UseGenericWeathering =
        CreateStaticBoolParameter(Material, TEXT("UseGenericWeatheringDecals"), false, -650, 1150);
    UMaterialExpressionStaticBoolParameter* UseExposedMetal =
        CreateStaticBoolParameter(Material, TEXT("UseExposedMetalMask"), false, -650, 1250);
    UMaterialExpressionStaticSwitch* TileUvSwitch =
        CreateStaticSwitch(Material, -1450, 0);
    UMaterialExpressionStaticSwitch* BaseSwitch =
        CreateStaticSwitch(Material, 250, -550);
    UMaterialExpressionStaticSwitch* RoughnessSwitch =
        CreateStaticSwitch(Material, 250, -250);
    UMaterialExpressionStaticSwitch* MetallicSwitch =
        CreateStaticSwitch(Material, 250, 0);
    UMaterialExpressionStaticSwitch* NormalSwitch =
        CreateStaticSwitch(Material, 250, 300);
    UMaterialExpressionStaticSwitch* AoSwitch =
        CreateStaticSwitch(Material, 250, 550);
    UMaterialExpressionStaticSwitch* GenericEnabledSwitch =
        CreateStaticSwitch(Material, -350, 1100);
    UMaterialExpressionStaticSwitch* ExposedEnabledSwitch =
        CreateStaticSwitch(Material, -350, 1250);
    UMaterialExpressionScalarParameter* NormalStrength =
        CreateScalarParameter(Material, TEXT("NormalStrength"), 1.0f, -950, 100);
    UMaterialExpressionScalarParameter* MacroAlbedoStrength =
        CreateScalarParameter(Material, TEXT("MacroAlbedoStrength"), 0.0f, -950, 350);
    UMaterialExpressionScalarParameter* MacroRoughnessStrength =
        CreateScalarParameter(Material, TEXT("MacroRoughnessStrength"), 0.0f, -950, 500);
    UMaterialExpressionScalarParameter* WeatheringStrength =
        CreateScalarParameter(Material, TEXT("WeatheringStrength"), 0.0f, -950, 650);
    UMaterialExpressionScalarParameter* GenericDecalStrength =
        CreateScalarParameter(Material, TEXT("GenericDecalStrength"), 0.0f, -950, 800);
    UMaterialExpressionScalarParameter* HeightMillimetres =
        CreateScalarParameter(Material, TEXT("HeightMillimetres"), 0.0f, -2200, 850);
    UMaterialExpressionScalarParameter* BumpOffsetStrength =
        CreateScalarParameter(Material, TEXT("BumpOffsetStrength"), 0.0f, -2200, 1000);
    UMaterialExpressionCustom* HeightRatio = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV2_HeightRatio"),
        TEXT("return (max(HeightMillimetres, 0.0) * 0.001 / max(TileMeters, 0.001)) * max(BumpOffsetStrength, 0.0);"),
        CMOT_Float1,
        TArray<FName>{TEXT("HeightMillimetres"), TEXT("TileMeters"), TEXT("BumpOffsetStrength")},
        -1850,
        900);
    UMaterialExpressionBumpOffset* BumpOffset =
        CreateExpression<UMaterialExpressionBumpOffset>(Material, -1600, 200);
    UMaterialExpressionCustom* AlbedoResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV2_OpaqueAlbedo"),
        TEXT("float macroSigned = (MacroRgb.r - 0.5) * 2.0 + (MacroA - 0.5) * 0.7;\n")
        TEXT("float weatherMask = saturate(dot(SurfaceRgb, float3(0.45, 0.20, 0.35)) + SurfaceA * 0.15);\n")
        TEXT("float genericMask = saturate(dot(SharedRgb, float3(0.40, 0.25, 0.20)) + SharedA * 0.15) * GenericEnabled;\n")
        TEXT("float weather = saturate(weatherMask * WeatheringStrength + genericMask * GenericDecalStrength);\n")
        TEXT("return saturate(TintedBase * max(0.0, 1.0 + macroSigned * MacroAlbedoStrength - weather * 0.18));"),
        CMOT_Float3,
        TArray<FName>{
            TEXT("TintedBase"), TEXT("MacroRgb"), TEXT("MacroA"),
            TEXT("SurfaceRgb"), TEXT("SurfaceA"), TEXT("SharedRgb"), TEXT("SharedA"),
            TEXT("MacroAlbedoStrength"), TEXT("WeatheringStrength"),
            TEXT("GenericDecalStrength"), TEXT("GenericEnabled")},
        -100,
        -650);
    UMaterialExpressionCustom* RoughnessResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV2_OpaqueRoughness"),
        TEXT("float macroSigned = (MacroRgb.g - 0.5) * 2.0 + (MacroA - 0.5) * 0.35;\n")
        TEXT("float weatherMask = saturate(dot(SurfaceRgb, float3(0.45, 0.20, 0.35)) + SurfaceA * 0.15);\n")
        TEXT("float genericMask = saturate(dot(SharedRgb, float3(0.40, 0.25, 0.20)) + SharedA * 0.15) * GenericEnabled;\n")
        TEXT("float weather = saturate(weatherMask * WeatheringStrength + genericMask * GenericDecalStrength);\n")
        TEXT("return saturate(TextureRoughness + RoughnessBias + macroSigned * MacroRoughnessStrength + weather * 0.15);"),
        CMOT_Float1,
        TArray<FName>{
            TEXT("TextureRoughness"), TEXT("RoughnessBias"), TEXT("MacroRgb"), TEXT("MacroA"),
            TEXT("SurfaceRgb"), TEXT("SurfaceA"), TEXT("SharedRgb"), TEXT("SharedA"),
            TEXT("MacroRoughnessStrength"), TEXT("WeatheringStrength"),
            TEXT("GenericDecalStrength"), TEXT("GenericEnabled")},
        -100,
        -300);
    UMaterialExpressionCustom* MetallicResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV2_OpaqueMetallic"),
        TEXT("return saturate(TextureMetallic + ExposedEnabled * SurfaceEdgeMask * max(ExposedMetalMaskStrength, 0.0));"),
        CMOT_Float1,
        TArray<FName>{
            TEXT("TextureMetallic"), TEXT("SurfaceEdgeMask"),
            TEXT("ExposedMetalMaskStrength"), TEXT("ExposedEnabled")},
        -100,
        0);
    UMaterialExpressionCustom* NormalResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV2_NormalRNM"),
        TEXT("float baseScale = max(BaseStrength, 0.0);\n")
        TEXT("float detailScale = max(DetailStrength, 0.0);\n")
        TEXT("float3 n1 = normalize(float3(BaseNormal.xy * baseScale, lerp(1.0, BaseNormal.z, saturate(baseScale))));\n")
        TEXT("float3 n2 = normalize(float3(DetailNormal.xy * detailScale, lerp(1.0, DetailNormal.z, saturate(detailScale))));\n")
        TEXT("float3 t = n1 + float3(0.0, 0.0, 1.0);\n")
        TEXT("float3 u = n2 * float3(-1.0, -1.0, 1.0);\n")
        TEXT("return normalize(t * dot(t, u) / max(t.z, 0.0001) - u);"),
        CMOT_Float3,
        TArray<FName>{TEXT("BaseNormal"), TEXT("DetailNormal"), TEXT("BaseStrength"), TEXT("DetailStrength")},
        -100,
        300);

    if (!Uv0 || !TileMeters || !DetailTileMeters || !MacroTileMeters ||
        !TileUv || !DetailUv || !MacroUv || !BaseColor || !Normal || !Orm ||
        !Height || !DetailNormal || !Macro || !SurfaceMasks || !SharedMasks ||
        !Tint || !TintedBase || !RoughnessBias || !MetallicFallback ||
        !DetailStrength || !FlatNormal || !One || !Zero || !UseTextureSet ||
        !UseBumpOffset || !UseGenericWeathering || !UseExposedMetal ||
        !TileUvSwitch || !BaseSwitch || !RoughnessSwitch || !MetallicSwitch ||
        !NormalSwitch || !AoSwitch || !GenericEnabledSwitch ||
        !ExposedEnabledSwitch || !NormalStrength || !MacroAlbedoStrength ||
        !MacroRoughnessStrength || !WeatheringStrength || !GenericDecalStrength ||
        !HeightMillimetres || !BumpOffsetStrength || !HeightRatio || !BumpOffset ||
        !AlbedoResponse || !RoughnessResponse || !MetallicResponse || !NormalResponse)
    {
        OutError = TEXT("Could not allocate the complete M_IPV_HeroSurface_V2 parameter graph.");
        return nullptr;
    }
    Uv0->CoordinateIndex = 0;
    FlatNormal->Constant = FLinearColor(0.0f, 0.0f, 1.0f);
    One->R = 1.0f;
    Zero->R = 0.0f;
    BumpOffset->ReferencePlane = 0.5f;
    BaseColor->Texture = FindTextureForMaterial(TEXT("Render"), TEXT("BaseColor"), Pack, Textures);
    Normal->Texture = FindTextureForMaterial(TEXT("Render"), TEXT("Normal"), Pack, Textures);
    Orm->Texture = FindTextureForMaterial(TEXT("Render"), TEXT("ORM"), Pack, Textures);
    Height->Texture = FindTextureForMaterial(TEXT("Render"), TEXT("Height"), Pack, Textures);
    DetailNormal->Texture = FindTextureForMaterial(TEXT("Render"), TEXT("DetailNormal"), Pack, Textures);
    Macro->Texture = FindTextureForMaterial(TEXT("Render"), TEXT("MacroVariation"), Pack, Textures);
    SurfaceMasks->Texture = FindTextureForMaterial(TEXT("Render"), TEXT("SurfaceMasks"), Pack, Textures);
    SharedMasks->Texture = FindTextureForMaterial(
        TEXT("Shared"), TEXT("SharedWeatheringDecalMasks"), Pack, Textures);
    if (!BaseColor->Texture || !Normal->Texture || !Orm->Texture || !Height->Texture ||
        !DetailNormal->Texture || !Macro->Texture || !SurfaceMasks->Texture ||
        !SharedMasks->Texture)
    {
        OutError = TEXT("Opaque v2 master lacks a frozen default texture binding.");
        return nullptr;
    }
    if (!ConnectExpression(Uv0, TEXT(""), TileUv, TEXT("A"), OutError) ||
        !ConnectExpression(TileMeters, TEXT(""), TileUv, TEXT("B"), OutError) ||
        !ConnectExpression(Uv0, TEXT(""), DetailUv, TEXT("A"), OutError) ||
        !ConnectExpression(DetailTileMeters, TEXT(""), DetailUv, TEXT("B"), OutError) ||
        !ConnectExpression(Uv0, TEXT(""), MacroUv, TEXT("A"), OutError) ||
        !ConnectExpression(MacroTileMeters, TEXT(""), MacroUv, TEXT("B"), OutError) ||
        !ConnectExpression(TileUv, TEXT(""), Height, TEXT("UVs"), OutError) ||
        !ConnectExpression(HeightMillimetres, TEXT(""), HeightRatio, TEXT("HeightMillimetres"), OutError) ||
        !ConnectExpression(TileMeters, TEXT(""), HeightRatio, TEXT("TileMeters"), OutError) ||
        !ConnectExpression(BumpOffsetStrength, TEXT(""), HeightRatio, TEXT("BumpOffsetStrength"), OutError) ||
        !ConnectExpression(TileUv, TEXT(""), BumpOffset, TEXT("Coordinate"), OutError) ||
        !ConnectExpression(Height, TEXT("R"), BumpOffset, TEXT("Height"), OutError) ||
        !ConnectExpression(HeightRatio, TEXT(""), BumpOffset, TEXT("HeightRatioInput"), OutError) ||
        !ConnectExpression(BumpOffset, TEXT(""), TileUvSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(TileUv, TEXT(""), TileUvSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseBumpOffset, TEXT(""), TileUvSwitch, TEXT("Value"), OutError) ||
        !ConnectExpression(TileUvSwitch, TEXT(""), BaseColor, TEXT("UVs"), OutError) ||
        !ConnectExpression(TileUvSwitch, TEXT(""), Normal, TEXT("UVs"), OutError) ||
        !ConnectExpression(TileUvSwitch, TEXT(""), Orm, TEXT("UVs"), OutError) ||
        !ConnectExpression(DetailUv, TEXT(""), DetailNormal, TEXT("UVs"), OutError) ||
        !ConnectExpression(MacroUv, TEXT(""), Macro, TEXT("UVs"), OutError) ||
        !ConnectExpression(TileUvSwitch, TEXT(""), SurfaceMasks, TEXT("UVs"), OutError) ||
        !ConnectExpression(MacroUv, TEXT(""), SharedMasks, TEXT("UVs"), OutError) ||
        !ConnectExpression(BaseColor, TEXT("RGB"), TintedBase, TEXT("A"), OutError) ||
        !ConnectExpression(Tint, TEXT(""), TintedBase, TEXT("B"), OutError) ||
        !ConnectExpression(One, TEXT(""), GenericEnabledSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(Zero, TEXT(""), GenericEnabledSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseGenericWeathering, TEXT(""), GenericEnabledSwitch, TEXT("Value"), OutError) ||
        !ConnectExpression(One, TEXT(""), ExposedEnabledSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(Zero, TEXT(""), ExposedEnabledSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseExposedMetal, TEXT(""), ExposedEnabledSwitch, TEXT("Value"), OutError) ||
        !ConnectExpression(TintedBase, TEXT(""), AlbedoResponse, TEXT("TintedBase"), OutError) ||
        !ConnectExpression(Macro, TEXT("RGB"), AlbedoResponse, TEXT("MacroRgb"), OutError) ||
        !ConnectExpression(Macro, TEXT("A"), AlbedoResponse, TEXT("MacroA"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("RGB"), AlbedoResponse, TEXT("SurfaceRgb"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("A"), AlbedoResponse, TEXT("SurfaceA"), OutError) ||
        !ConnectExpression(SharedMasks, TEXT("RGB"), AlbedoResponse, TEXT("SharedRgb"), OutError) ||
        !ConnectExpression(SharedMasks, TEXT("A"), AlbedoResponse, TEXT("SharedA"), OutError) ||
        !ConnectExpression(MacroAlbedoStrength, TEXT(""), AlbedoResponse, TEXT("MacroAlbedoStrength"), OutError) ||
        !ConnectExpression(WeatheringStrength, TEXT(""), AlbedoResponse, TEXT("WeatheringStrength"), OutError) ||
        !ConnectExpression(GenericDecalStrength, TEXT(""), AlbedoResponse, TEXT("GenericDecalStrength"), OutError) ||
        !ConnectExpression(GenericEnabledSwitch, TEXT(""), AlbedoResponse, TEXT("GenericEnabled"), OutError) ||
        !ConnectExpression(Orm, TEXT("G"), RoughnessResponse, TEXT("TextureRoughness"), OutError) ||
        !ConnectExpression(RoughnessBias, TEXT(""), RoughnessResponse, TEXT("RoughnessBias"), OutError) ||
        !ConnectExpression(Macro, TEXT("RGB"), RoughnessResponse, TEXT("MacroRgb"), OutError) ||
        !ConnectExpression(Macro, TEXT("A"), RoughnessResponse, TEXT("MacroA"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("RGB"), RoughnessResponse, TEXT("SurfaceRgb"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("A"), RoughnessResponse, TEXT("SurfaceA"), OutError) ||
        !ConnectExpression(SharedMasks, TEXT("RGB"), RoughnessResponse, TEXT("SharedRgb"), OutError) ||
        !ConnectExpression(SharedMasks, TEXT("A"), RoughnessResponse, TEXT("SharedA"), OutError) ||
        !ConnectExpression(MacroRoughnessStrength, TEXT(""), RoughnessResponse, TEXT("MacroRoughnessStrength"), OutError) ||
        !ConnectExpression(WeatheringStrength, TEXT(""), RoughnessResponse, TEXT("WeatheringStrength"), OutError) ||
        !ConnectExpression(GenericDecalStrength, TEXT(""), RoughnessResponse, TEXT("GenericDecalStrength"), OutError) ||
        !ConnectExpression(GenericEnabledSwitch, TEXT(""), RoughnessResponse, TEXT("GenericEnabled"), OutError) ||
        !ConnectExpression(Orm, TEXT("B"), MetallicResponse, TEXT("TextureMetallic"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("G"), MetallicResponse, TEXT("SurfaceEdgeMask"), OutError) ||
        !ConnectExpression(MetallicFallback, TEXT(""), MetallicResponse, TEXT("ExposedMetalMaskStrength"), OutError) ||
        !ConnectExpression(ExposedEnabledSwitch, TEXT(""), MetallicResponse, TEXT("ExposedEnabled"), OutError) ||
        !ConnectExpression(Normal, TEXT("RGB"), NormalResponse, TEXT("BaseNormal"), OutError) ||
        !ConnectExpression(DetailNormal, TEXT("RGB"), NormalResponse, TEXT("DetailNormal"), OutError) ||
        !ConnectExpression(NormalStrength, TEXT(""), NormalResponse, TEXT("BaseStrength"), OutError) ||
        !ConnectExpression(DetailStrength, TEXT(""), NormalResponse, TEXT("DetailStrength"), OutError) ||
        !ConnectExpression(AlbedoResponse, TEXT(""), BaseSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(Tint, TEXT(""), BaseSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseTextureSet, TEXT(""), BaseSwitch, TEXT("Value"), OutError) ||
        !ConnectExpression(RoughnessResponse, TEXT(""), RoughnessSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(RoughnessBias, TEXT(""), RoughnessSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseTextureSet, TEXT(""), RoughnessSwitch, TEXT("Value"), OutError) ||
        !ConnectExpression(MetallicResponse, TEXT(""), MetallicSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(MetallicFallback, TEXT(""), MetallicSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseTextureSet, TEXT(""), MetallicSwitch, TEXT("Value"), OutError) ||
        !ConnectExpression(NormalResponse, TEXT(""), NormalSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(FlatNormal, TEXT(""), NormalSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseTextureSet, TEXT(""), NormalSwitch, TEXT("Value"), OutError) ||
        !ConnectExpression(Orm, TEXT("R"), AoSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(One, TEXT(""), AoSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseTextureSet, TEXT(""), AoSwitch, TEXT("Value"), OutError) ||
        !ConnectProperty(BaseSwitch, TEXT(""), MP_BaseColor, OutError) ||
        !ConnectProperty(RoughnessSwitch, TEXT(""), MP_Roughness, OutError) ||
        !ConnectProperty(MetallicSwitch, TEXT(""), MP_Metallic, OutError) ||
        !ConnectProperty(NormalSwitch, TEXT(""), MP_Normal, OutError) ||
        !ConnectProperty(AoSwitch, TEXT(""), MP_AmbientOcclusion, OutError))
    {
        return nullptr;
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    if (!SealHeroMaterialGraph(Material, OutError))
    {
        return nullptr;
    }
    Material->MarkPackageDirty();
    return Material;
}

UMaterial* CreateGlassHeroMaster(
    const FHeroMaterialPack& Pack,
    const TMap<FName, UTexture2D*>& Textures,
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            TEXT("M_IPV_HeroGlass_V2"),
            MaterialAssetRoot,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaPublicViewHeroV2Assets"))))
        : nullptr;
    if (!Material)
    {
        OutError = TEXT("Could not create M_IPV_HeroGlass_V2.");
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Translucent;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->TranslucencyLightingMode = TLM_Surface;
    Material->RefractionMethod = RM_IndexOfRefraction;

    UMaterialExpressionTextureCoordinate* Uv0 =
        CreateExpression<UMaterialExpressionTextureCoordinate>(Material, -1400, 0);
    UMaterialExpressionScalarParameter* TileMeters =
        CreateScalarParameter(Material, TEXT("TileMeters"), 2.0f, -1400, 150);
    UMaterialExpressionScalarParameter* DetailTileMeters =
        CreateScalarParameter(Material, TEXT("DetailTileMeters"), 0.22f, -1400, 300);
    UMaterialExpressionScalarParameter* MacroTileMeters =
        CreateScalarParameter(Material, TEXT("MacroTileMeters"), 5.0f, -1400, 450);
    UMaterialExpressionDivide* TileUv =
        CreateExpression<UMaterialExpressionDivide>(Material, -1150, 0);
    UMaterialExpressionDivide* DetailUv =
        CreateExpression<UMaterialExpressionDivide>(Material, -1150, 250);
    UMaterialExpressionDivide* MacroUv =
        CreateExpression<UMaterialExpressionDivide>(Material, -1150, 500);
    UMaterialExpressionTextureSampleParameter2D* BaseColor =
        CreateTextureParameter(Material, TEXT("Tex_BaseColor"), SAMPLERTYPE_Color, -900, -400);
    UMaterialExpressionTextureSampleParameter2D* Normal =
        CreateTextureParameter(Material, TEXT("Tex_Normal"), SAMPLERTYPE_Normal, -900, -200);
    UMaterialExpressionTextureSampleParameter2D* Orm =
        CreateTextureParameter(Material, TEXT("Tex_ORM"), SAMPLERTYPE_Masks, -900, 0);
    UMaterialExpressionTextureSampleParameter2D* DetailNormal =
        CreateTextureParameter(Material, TEXT("Tex_DetailNormal"), SAMPLERTYPE_Normal, -900, 200);
    UMaterialExpressionTextureSampleParameter2D* Macro =
        CreateTextureParameter(Material, TEXT("Tex_MacroVariation"), SAMPLERTYPE_Masks, -900, 400);
    UMaterialExpressionTextureSampleParameter2D* SurfaceMasks =
        CreateTextureParameter(Material, TEXT("Tex_SurfaceMasks"), SAMPLERTYPE_Masks, -900, 600);
    UMaterialExpressionVectorParameter* Tint =
        CreateVectorParameter(Material, TEXT("LookdevTint"), FLinearColor::White, -650, -400);
    UMaterialExpressionMultiply* TintedBase =
        CreateExpression<UMaterialExpressionMultiply>(Material, -400, -350);
    UMaterialExpressionScalarParameter* RoughnessBias =
        CreateScalarParameter(Material, TEXT("RoughnessBias"), 0.0f, -650, 0);
    UMaterialExpressionScalarParameter* DetailStrength =
        CreateScalarParameter(Material, TEXT("DetailNormalStrength"), 0.18f, -650, 200);
    UMaterialExpressionScalarParameter* NormalStrength =
        CreateScalarParameter(Material, TEXT("NormalStrength"), 1.0f, -650, 350);
    UMaterialExpressionScalarParameter* Ior =
        CreateScalarParameter(Material, TEXT("IOR"), 1.52f, -400, 450);
    UMaterialExpressionScalarParameter* Opacity =
        CreateScalarParameter(Material, TEXT("Opacity"), 0.32f, -400, 550);
    UMaterialExpressionScalarParameter* DustStrength =
        CreateScalarParameter(Material, TEXT("DustStrength"), 0.035f, -400, 650);
    UMaterialExpressionCustom* AlbedoResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV2_GlassAlbedo"),
        TEXT("float macroSigned = (MacroRgb.r - 0.5) * 2.0 + (MacroA - 0.5) * 0.5;\n")
        TEXT("float dust = saturate((dot(SurfaceRgb, float3(0.55, 0.15, 0.30)) + SurfaceA * 0.2) * DustStrength);\n")
        TEXT("return saturate(TintedBase * (1.0 + macroSigned * 0.025) + dust * float3(0.025, 0.023, 0.020));"),
        CMOT_Float3,
        TArray<FName>{
            TEXT("TintedBase"), TEXT("MacroRgb"), TEXT("MacroA"),
            TEXT("SurfaceRgb"), TEXT("SurfaceA"), TEXT("DustStrength")},
        -100,
        -350);
    UMaterialExpressionCustom* RoughnessResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV2_GlassRoughness"),
        TEXT("float macroSigned = (MacroRgb.g - 0.5) * 2.0;\n")
        TEXT("float dust = saturate((dot(SurfaceRgb, float3(0.55, 0.15, 0.30)) + SurfaceA * 0.2) * DustStrength);\n")
        TEXT("return saturate(TextureRoughness + RoughnessBias + macroSigned * 0.025 + dust * 0.45);"),
        CMOT_Float1,
        TArray<FName>{
            TEXT("TextureRoughness"), TEXT("RoughnessBias"), TEXT("MacroRgb"),
            TEXT("SurfaceRgb"), TEXT("SurfaceA"), TEXT("DustStrength")},
        -100,
        0);
    UMaterialExpressionCustom* NormalResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV2_GlassNormalRNM"),
        TEXT("float baseScale = max(BaseStrength, 0.0);\n")
        TEXT("float detailScale = max(DetailStrength, 0.0);\n")
        TEXT("float3 n1 = normalize(float3(BaseNormal.xy * baseScale, lerp(1.0, BaseNormal.z, saturate(baseScale))));\n")
        TEXT("float3 n2 = normalize(float3(DetailNormal.xy * detailScale, lerp(1.0, DetailNormal.z, saturate(detailScale))));\n")
        TEXT("float3 t = n1 + float3(0.0, 0.0, 1.0);\n")
        TEXT("float3 u = n2 * float3(-1.0, -1.0, 1.0);\n")
        TEXT("return normalize(t * dot(t, u) / max(t.z, 0.0001) - u);"),
        CMOT_Float3,
        TArray<FName>{TEXT("BaseNormal"), TEXT("DetailNormal"), TEXT("BaseStrength"), TEXT("DetailStrength")},
        -100,
        250);
    if (!Uv0 || !TileMeters || !DetailTileMeters || !MacroTileMeters ||
        !TileUv || !DetailUv || !MacroUv || !BaseColor || !Normal || !Orm ||
        !DetailNormal || !Macro || !SurfaceMasks || !Tint || !TintedBase ||
        !RoughnessBias || !DetailStrength || !NormalStrength || !Ior || !Opacity ||
        !DustStrength || !AlbedoResponse || !RoughnessResponse || !NormalResponse)
    {
        OutError = TEXT("Could not allocate the complete M_IPV_HeroGlass_V2 graph.");
        return nullptr;
    }
    Uv0->CoordinateIndex = 0;
    BaseColor->Texture = FindTextureForMaterial(TEXT("Glass"), TEXT("BaseColor"), Pack, Textures);
    Normal->Texture = FindTextureForMaterial(TEXT("Glass"), TEXT("Normal"), Pack, Textures);
    Orm->Texture = FindTextureForMaterial(TEXT("Glass"), TEXT("ORM"), Pack, Textures);
    DetailNormal->Texture = FindTextureForMaterial(TEXT("Glass"), TEXT("DetailNormal"), Pack, Textures);
    Macro->Texture = FindTextureForMaterial(TEXT("Glass"), TEXT("MacroVariation"), Pack, Textures);
    SurfaceMasks->Texture = FindTextureForMaterial(TEXT("Glass"), TEXT("SurfaceMasks"), Pack, Textures);
    if (!BaseColor->Texture || !Normal->Texture || !Orm->Texture ||
        !DetailNormal->Texture || !Macro->Texture || !SurfaceMasks->Texture)
    {
        OutError = TEXT("Glass v2 master lacks a frozen default texture binding.");
        return nullptr;
    }
    if (!ConnectExpression(Uv0, TEXT(""), TileUv, TEXT("A"), OutError) ||
        !ConnectExpression(TileMeters, TEXT(""), TileUv, TEXT("B"), OutError) ||
        !ConnectExpression(Uv0, TEXT(""), DetailUv, TEXT("A"), OutError) ||
        !ConnectExpression(DetailTileMeters, TEXT(""), DetailUv, TEXT("B"), OutError) ||
        !ConnectExpression(Uv0, TEXT(""), MacroUv, TEXT("A"), OutError) ||
        !ConnectExpression(MacroTileMeters, TEXT(""), MacroUv, TEXT("B"), OutError) ||
        !ConnectExpression(TileUv, TEXT(""), BaseColor, TEXT("UVs"), OutError) ||
        !ConnectExpression(TileUv, TEXT(""), Normal, TEXT("UVs"), OutError) ||
        !ConnectExpression(TileUv, TEXT(""), Orm, TEXT("UVs"), OutError) ||
        !ConnectExpression(DetailUv, TEXT(""), DetailNormal, TEXT("UVs"), OutError) ||
        !ConnectExpression(MacroUv, TEXT(""), Macro, TEXT("UVs"), OutError) ||
        !ConnectExpression(TileUv, TEXT(""), SurfaceMasks, TEXT("UVs"), OutError) ||
        !ConnectExpression(BaseColor, TEXT("RGB"), TintedBase, TEXT("A"), OutError) ||
        !ConnectExpression(Tint, TEXT(""), TintedBase, TEXT("B"), OutError) ||
        !ConnectExpression(TintedBase, TEXT(""), AlbedoResponse, TEXT("TintedBase"), OutError) ||
        !ConnectExpression(Macro, TEXT("RGB"), AlbedoResponse, TEXT("MacroRgb"), OutError) ||
        !ConnectExpression(Macro, TEXT("A"), AlbedoResponse, TEXT("MacroA"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("RGB"), AlbedoResponse, TEXT("SurfaceRgb"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("A"), AlbedoResponse, TEXT("SurfaceA"), OutError) ||
        !ConnectExpression(DustStrength, TEXT(""), AlbedoResponse, TEXT("DustStrength"), OutError) ||
        !ConnectExpression(Orm, TEXT("G"), RoughnessResponse, TEXT("TextureRoughness"), OutError) ||
        !ConnectExpression(RoughnessBias, TEXT(""), RoughnessResponse, TEXT("RoughnessBias"), OutError) ||
        !ConnectExpression(Macro, TEXT("RGB"), RoughnessResponse, TEXT("MacroRgb"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("RGB"), RoughnessResponse, TEXT("SurfaceRgb"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("A"), RoughnessResponse, TEXT("SurfaceA"), OutError) ||
        !ConnectExpression(DustStrength, TEXT(""), RoughnessResponse, TEXT("DustStrength"), OutError) ||
        !ConnectExpression(Normal, TEXT("RGB"), NormalResponse, TEXT("BaseNormal"), OutError) ||
        !ConnectExpression(DetailNormal, TEXT("RGB"), NormalResponse, TEXT("DetailNormal"), OutError) ||
        !ConnectExpression(NormalStrength, TEXT(""), NormalResponse, TEXT("BaseStrength"), OutError) ||
        !ConnectExpression(DetailStrength, TEXT(""), NormalResponse, TEXT("DetailStrength"), OutError) ||
        !ConnectProperty(AlbedoResponse, TEXT(""), MP_BaseColor, OutError) ||
        !ConnectProperty(RoughnessResponse, TEXT(""), MP_Roughness, OutError) ||
        !ConnectProperty(NormalResponse, TEXT(""), MP_Normal, OutError) ||
        !ConnectProperty(Ior, TEXT(""), MP_Refraction, OutError) ||
        !ConnectProperty(Opacity, TEXT(""), MP_Opacity, OutError))
    {
        return nullptr;
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    if (!SealHeroMaterialGraph(Material, OutError))
    {
        return nullptr;
    }
    Material->MarkPackageDirty();
    return Material;
}

UTexture2D* FindTextureForMaterial(
    const FString& MaterialId,
    const FString& MapType,
    const FHeroMaterialPack& Pack,
    const TMap<FName, UTexture2D*>& Textures)
{
    const FHeroTextureSpec* Spec = Pack.Textures.FindByPredicate(
        [&MaterialId, &MapType](const FHeroTextureSpec& Candidate)
        {
            return Candidate.MaterialId == MaterialId &&
                Candidate.MapType == MapType;
        });
    return Spec ? Textures.FindRef(FName(*Spec->AssetName)) : nullptr;
}

UMaterialInstanceConstant* CreateHeroMaterialInstance(
    const FHeroMaterialInstanceSpec& Spec,
    const FHeroMaterialPack& Pack,
    const TMap<FName, UTexture2D*>& Textures,
    UMaterial* OpaqueMaster,
    UMaterial* GlassMaster,
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterial* Parent = Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V2")
        ? GlassMaster
        : OpaqueMaster;
    UMaterialInstanceConstantFactoryNew* Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    if (!Factory || !Parent)
    {
        OutError = TEXT("Could not allocate a v2 material-instance factory or parent.");
        return nullptr;
    }
    Factory->InitialParent = Parent;
    UMaterialInstanceConstant* Instance =
        Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
            Spec.AssetName,
            MaterialAssetRoot,
            UMaterialInstanceConstant::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaPublicViewHeroV2Assets"))));
    if (!Instance)
    {
        OutError = FString::Printf(
            TEXT("Could not create v2 material instance '%s'."),
            *Spec.AssetName);
        return nullptr;
    }
    Instance->Modify();
    Instance->SetParentEditorOnly(Parent, false);
    for (const TPair<FName, float>& Pair : Spec.Scalars)
    {
        Instance->SetScalarParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    for (const TPair<FName, FLinearColor>& Pair : Spec.Vectors)
    {
        Instance->SetVectorParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    for (const TPair<FName, bool>& Pair : Spec.StaticSwitches)
    {
        Instance->SetStaticSwitchParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    if (!Spec.bSpecialParameterOnly)
    {
        const TPair<const TCHAR*, const TCHAR*> TextureBindings[] = {
            {TEXT("BaseColor"), TEXT("Tex_BaseColor")},
            {TEXT("Normal"), TEXT("Tex_Normal")},
            {TEXT("ORM"), TEXT("Tex_ORM")},
            {TEXT("Height"), TEXT("Tex_Height")},
            {TEXT("DetailNormal"), TEXT("Tex_DetailNormal")},
            {TEXT("MacroVariation"), TEXT("Tex_MacroVariation")},
            {TEXT("SurfaceMasks"), TEXT("Tex_SurfaceMasks")}};
        for (const TPair<const TCHAR*, const TCHAR*>& Binding : TextureBindings)
        {
            if (Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V2") &&
                FString(Binding.Key) == TEXT("Height"))
            {
                continue;
            }
            UTexture2D* Texture = FindTextureForMaterial(
                Spec.Id, Binding.Key, Pack, Textures);
            if (!Texture)
            {
                OutError = FString::Printf(
                    TEXT("Material instance '%s' lacks generated map '%s'."),
                    *Spec.AssetName,
                    Binding.Key);
                return nullptr;
            }
            Instance->SetTextureParameterValueEditorOnly(
                FMaterialParameterInfo(Binding.Value), Texture);
        }
        if (Spec.ParentAssetName != TEXT("M_IPV_HeroGlass_V2"))
        {
            UTexture2D* Shared = FindTextureForMaterial(
                TEXT("Shared"), TEXT("SharedWeatheringDecalMasks"),
                Pack, Textures);
            if (!Shared)
            {
                OutError = TEXT("Shared v2 weathering decal mask is absent.");
                return nullptr;
            }
            Instance->SetTextureParameterValueEditorOnly(
                FMaterialParameterInfo(TEXT("Tex_SharedWeatheringDecalMasks")),
                Shared);
        }
    }
    if (Spec.StaticSwitches.Num() > 0)
    {
        // SetStaticSwitchParameterValueEditorOnly mutates the parameter set but
        // does not itself establish or compile the MIC-owned permutation.
        // Use the documented forced update before the generic edit callback.
        Instance->UpdateStaticPermutation();
    }
    Instance->PostEditChange();
    // UpdateStaticPermutation creates the correct resource with precompile mode
    // None. EnsureIsComplete explicitly submits and finishes those shader jobs
    // before the fail-closed validator observes the instance.
    Instance->EnsureIsComplete();
    Instance->MarkPackageDirty();
    return Instance;
}

bool SetEquals(
    const TSet<FName>& Actual,
    const TArray<const TCHAR*>& Expected)
{
    if (Actual.Num() != Expected.Num())
    {
        return false;
    }
    for (const TCHAR* Name : Expected)
    {
        if (!Actual.Contains(FName(Name)))
        {
            return false;
        }
    }
    return true;
}

const TCHAR* HeroGraphSchema =
    TEXT("triad.istana_public_view_hero_v2.material_graph.v1");
// This schema is an external recipe epoch, not merely descriptive metadata.
// Any material recipe/node/default/edge change must increment material_graph.vN;
// no earlier released integration asset used the v1 seal.
const TCHAR* HeroGraphSchemaMetadataKey = TEXT("TRIADHeroV2GraphSchema");
const TCHAR* HeroGraphShaMetadataKey = TEXT("TRIADHeroV2GraphSha256");

void AppendGraphToken(FString& InOut, const FString& Value)
{
    InOut += FString::Printf(TEXT("%d:"), Value.Len());
    InOut += Value;
}

FString FloatBits(float Value)
{
    return FString::Printf(TEXT("%08x"), FPlatformMath::AsUInt(Value));
}

bool ComputeHeroMaterialGraphSha256(
    UMaterial* Material,
    FString& OutDigest,
    FString& OutError)
{
    if (!Material)
    {
        OutError = TEXT("Cannot fingerprint a null v2 master material.");
        return false;
    }
    TArray<UMaterialExpression*> Expressions;
    TSet<FString> NodeIds;
    for (UMaterialExpression* Expression : Material->GetExpressions())
    {
        if (!Expression || Expression->Desc.IsEmpty() ||
            !Expression->Desc.StartsWith(TEXT("TRIAD_IPV2_NODE_")) ||
            NodeIds.Contains(Expression->Desc))
        {
            OutError = TEXT("V2 graph fingerprint requires unique stable node identifiers.");
            return false;
        }
        NodeIds.Add(Expression->Desc);
        Expressions.Add(Expression);
    }
    Expressions.Sort(
        [](const UMaterialExpression& A, const UMaterialExpression& B)
        {
            return A.Desc < B.Desc;
        });

    FString Canonical;
    AppendGraphToken(Canonical, HeroGraphSchema);
    AppendGraphToken(Canonical, Material->GetName());
    AppendGraphToken(
        Canonical, FString::FromInt(static_cast<int32>(Material->MaterialDomain)));
    AppendGraphToken(
        Canonical, FString::FromInt(static_cast<int32>(Material->BlendMode)));
    AppendGraphToken(
        Canonical,
        FString::FromInt(Material->GetShadingModels().GetShadingModelField()));
    AppendGraphToken(Canonical, Material->TwoSided ? TEXT("1") : TEXT("0"));
    AppendGraphToken(
        Canonical, Material->bTangentSpaceNormal ? TEXT("1") : TEXT("0"));
    AppendGraphToken(
        Canonical, Material->bUseMaterialAttributes ? TEXT("1") : TEXT("0"));
    AppendGraphToken(
        Canonical,
        FString::FromInt(
            static_cast<int32>(Material->TranslucencyLightingMode)));
    AppendGraphToken(
        Canonical,
        FString::FromInt(static_cast<int32>(Material->RefractionMethod)));
    AppendGraphToken(Canonical, FString::FromInt(Expressions.Num()));
    for (UMaterialExpression* Expression : Expressions)
    {
        AppendGraphToken(Canonical, Expression->Desc);
        AppendGraphToken(Canonical, Expression->GetClass()->GetName());
        if (const UMaterialExpressionParameter* Parameter =
                Cast<UMaterialExpressionParameter>(Expression))
        {
            AppendGraphToken(Canonical, Parameter->ParameterName.ToString());
            AppendGraphToken(Canonical, Parameter->Group.ToString());
        }
        if (const UMaterialExpressionScalarParameter* Parameter =
                Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            AppendGraphToken(Canonical, FloatBits(Parameter->DefaultValue));
        }
        if (const UMaterialExpressionVectorParameter* Parameter =
                Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            AppendGraphToken(Canonical, FloatBits(Parameter->DefaultValue.R));
            AppendGraphToken(Canonical, FloatBits(Parameter->DefaultValue.G));
            AppendGraphToken(Canonical, FloatBits(Parameter->DefaultValue.B));
            AppendGraphToken(Canonical, FloatBits(Parameter->DefaultValue.A));
        }
        if (const UMaterialExpressionStaticBoolParameter* Parameter =
                Cast<UMaterialExpressionStaticBoolParameter>(Expression))
        {
            AppendGraphToken(Canonical, Parameter->DefaultValue ? TEXT("1") : TEXT("0"));
            AppendGraphToken(Canonical, Parameter->DynamicBranch ? TEXT("1") : TEXT("0"));
        }
        if (const UMaterialExpressionTextureSampleParameter2D* Parameter =
                Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            AppendGraphToken(
                Canonical, FString::FromInt(static_cast<int32>(Parameter->SamplerType)));
            AppendGraphToken(
                Canonical,
                FString::FromInt(static_cast<int32>(Parameter->MipValueMode)));
            AppendGraphToken(
                Canonical,
                FString::FromInt(static_cast<int32>(Parameter->SamplerSource)));
            AppendGraphToken(
                Canonical, FString::FromInt(Parameter->ConstMipValue));
            AppendGraphToken(
                Canonical, Parameter->AutomaticViewMipBias ? TEXT("1") : TEXT("0"));
            AppendGraphToken(
                Canonical,
                Parameter->Texture ? Parameter->Texture->GetPathName() : TEXT("<null>"));
        }
        if (const UMaterialExpressionTextureCoordinate* Coordinate =
                Cast<UMaterialExpressionTextureCoordinate>(Expression))
        {
            AppendGraphToken(Canonical, FString::FromInt(Coordinate->CoordinateIndex));
            AppendGraphToken(Canonical, FloatBits(Coordinate->UTiling));
            AppendGraphToken(Canonical, FloatBits(Coordinate->VTiling));
            AppendGraphToken(Canonical, Coordinate->UnMirrorU ? TEXT("1") : TEXT("0"));
            AppendGraphToken(Canonical, Coordinate->UnMirrorV ? TEXT("1") : TEXT("0"));
        }
        if (const UMaterialExpressionConstant* Constant =
                Cast<UMaterialExpressionConstant>(Expression))
        {
            AppendGraphToken(Canonical, FloatBits(Constant->R));
        }
        if (const UMaterialExpressionConstant3Vector* Constant =
                Cast<UMaterialExpressionConstant3Vector>(Expression))
        {
            AppendGraphToken(Canonical, FloatBits(Constant->Constant.R));
            AppendGraphToken(Canonical, FloatBits(Constant->Constant.G));
            AppendGraphToken(Canonical, FloatBits(Constant->Constant.B));
        }
        if (const UMaterialExpressionStaticSwitch* Switch =
                Cast<UMaterialExpressionStaticSwitch>(Expression))
        {
            AppendGraphToken(Canonical, Switch->DefaultValue ? TEXT("1") : TEXT("0"));
        }
        if (const UMaterialExpressionBumpOffset* Bump =
                Cast<UMaterialExpressionBumpOffset>(Expression))
        {
            AppendGraphToken(Canonical, FloatBits(Bump->HeightRatio));
            AppendGraphToken(Canonical, FloatBits(Bump->ReferencePlane));
            AppendGraphToken(Canonical, FString::FromInt(Bump->ConstCoordinate));
        }
        if (const UMaterialExpressionCustom* Custom =
                Cast<UMaterialExpressionCustom>(Expression))
        {
            AppendGraphToken(Canonical, Custom->Description);
            AppendGraphToken(Canonical, Custom->Code);
            AppendGraphToken(
                Canonical, FString::FromInt(static_cast<int32>(Custom->OutputType)));
            AppendGraphToken(Canonical, FString::FromInt(Custom->AdditionalOutputs.Num()));
            for (const FCustomOutput& Output : Custom->AdditionalOutputs)
            {
                AppendGraphToken(Canonical, Output.OutputName.ToString());
                AppendGraphToken(
                    Canonical, FString::FromInt(static_cast<int32>(Output.OutputType)));
            }
            AppendGraphToken(Canonical, FString::FromInt(Custom->AdditionalDefines.Num()));
            for (const FCustomDefine& Define : Custom->AdditionalDefines)
            {
                AppendGraphToken(Canonical, Define.DefineName);
                AppendGraphToken(Canonical, Define.DefineValue);
            }
            AppendGraphToken(Canonical, FString::FromInt(Custom->IncludeFilePaths.Num()));
            for (const FString& Include : Custom->IncludeFilePaths)
            {
                AppendGraphToken(Canonical, Include);
            }
        }
        const TArrayView<FExpressionInput*> Inputs = Expression->GetInputsView();
        AppendGraphToken(Canonical, FString::FromInt(Inputs.Num()));
        for (int32 Index = 0; Index < Inputs.Num(); ++Index)
        {
            const FExpressionInput* Input = Inputs[Index];
            AppendGraphToken(Canonical, Expression->GetInputName(Index).ToString());
            if (!Input || !Input->Expression)
            {
                AppendGraphToken(Canonical, TEXT("<null>"));
                continue;
            }
            if (!NodeIds.Contains(Input->Expression->Desc))
            {
                OutError = TEXT("V2 graph edge points outside its sealed master material.");
                return false;
            }
            AppendGraphToken(Canonical, Input->Expression->Desc);
            AppendGraphToken(Canonical, FString::FromInt(Input->OutputIndex));
            AppendGraphToken(Canonical, FString::FromInt(Input->Mask));
            AppendGraphToken(Canonical, FString::FromInt(Input->MaskR));
            AppendGraphToken(Canonical, FString::FromInt(Input->MaskG));
            AppendGraphToken(Canonical, FString::FromInt(Input->MaskB));
            AppendGraphToken(Canonical, FString::FromInt(Input->MaskA));
        }
    }
    for (int32 Property = 0; Property < MP_MAX; ++Property)
    {
        FExpressionInput* Input = Material->GetExpressionInputForProperty(
            static_cast<EMaterialProperty>(Property));
        if (!Input || !Input->Expression)
        {
            continue;
        }
        AppendGraphToken(Canonical, FString::Printf(TEXT("PROPERTY_%d"), Property));
        AppendGraphToken(Canonical, Input->Expression->Desc);
        AppendGraphToken(Canonical, FString::FromInt(Input->OutputIndex));
        AppendGraphToken(Canonical, FString::FromInt(Input->Mask));
        AppendGraphToken(Canonical, FString::FromInt(Input->MaskR));
        AppendGraphToken(Canonical, FString::FromInt(Input->MaskG));
        AppendGraphToken(Canonical, FString::FromInt(Input->MaskB));
        AppendGraphToken(Canonical, FString::FromInt(Input->MaskA));
    }
    FTCHARToUTF8 Utf8(*Canonical);
    if (!CalculateSha256Bytes(Utf8.Get(), Utf8.Length(), OutDigest))
    {
        OutError = TEXT("Could not calculate the v2 material graph SHA-256.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool SealHeroMaterialGraph(UMaterial* Material, FString& OutError)
{
    FString Digest;
    if (!ComputeHeroMaterialGraphSha256(Material, Digest, OutError) ||
        !Material->GetOutermost())
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V2 master package is absent while sealing its graph.");
        }
        return false;
    }
    UMetaData* MetaData = Material->GetOutermost()->GetMetaData();
    if (!MetaData)
    {
        OutError = TEXT("Could not allocate v2 master graph metadata.");
        return false;
    }
    MetaData->SetValue(Material, HeroGraphSchemaMetadataKey, HeroGraphSchema);
    MetaData->SetValue(Material, HeroGraphShaMetadataKey, *Digest);
    return true;
}

bool ValidateHeroMaterialGraphSeal(
    UMaterial* Material,
    FString& OutError)
{
    FString ActualDigest;
    if (!Material || !Material->GetOutermost() ||
        !ComputeHeroMaterialGraphSha256(Material, ActualDigest, OutError))
    {
        return false;
    }
    UMetaData* MetaData = Material->GetOutermost()->GetMetaData();
    const FString StoredSchema = MetaData
        ? MetaData->GetValue(Material, HeroGraphSchemaMetadataKey)
        : FString();
    const FString StoredDigest = MetaData
        ? MetaData->GetValue(Material, HeroGraphShaMetadataKey)
        : FString();
    if (StoredSchema != HeroGraphSchema ||
        StoredDigest.Len() != 64 || StoredDigest != ActualDigest)
    {
        OutError = FString::Printf(
            TEXT("V2 master '%s' graph seal is absent or stale (actual %s)."),
            *Material->GetName(),
            *ActualDigest);
        return false;
    }
    return true;
}

void GatherReachableMaterialExpressions(
    UMaterialExpression* Expression,
    TSet<const UMaterialExpression*>& OutReachable)
{
    if (!Expression || OutReachable.Contains(Expression))
    {
        return;
    }
    OutReachable.Add(Expression);
    for (FExpressionInput* Input : Expression->GetInputsView())
    {
        if (Input && Input->Expression)
        {
            GatherReachableMaterialExpressions(Input->Expression, OutReachable);
        }
    }
}

bool ValidateCustomExpressionTopology(
    const UMaterialExpressionCustom* Expression,
    FString& OutError)
{
    if (!Expression || Expression->Description.IsEmpty() ||
        !Expression->Description.StartsWith(TEXT("TRIAD_IPV2_")) ||
        Expression->Code.IsEmpty() || Expression->Inputs.Num() == 0)
    {
        OutError = TEXT("V2 material contains an unidentified or empty custom response node.");
        return false;
    }
    TSet<FName> Names;
    for (int32 InputIndex = 0;
         InputIndex < Expression->Inputs.Num();
         ++InputIndex)
    {
        const FCustomInput& Input = Expression->Inputs[InputIndex];
        const FString InputName = Input.InputName.ToString();
        if (Input.InputName.IsNone())
        {
            OutError = FString::Printf(
                TEXT("V2 custom response '%s' input[%d] of %d is unnamed."),
                *Expression->Description,
                InputIndex,
                Expression->Inputs.Num());
            return false;
        }
        if (Names.Contains(Input.InputName))
        {
            OutError = FString::Printf(
                TEXT("V2 custom response '%s' input[%d] '%s' duplicates an earlier input name."),
                *Expression->Description,
                InputIndex,
                *InputName);
            return false;
        }
        if (!Input.Input.Expression)
        {
            OutError = FString::Printf(
                TEXT("V2 custom response '%s' input[%d] '%s' is directly disconnected."),
                *Expression->Description,
                InputIndex,
                *InputName);
            return false;
        }
        const FExpressionInput TracedInput = Input.Input.GetTracedInput();
        if (!TracedInput.Expression)
        {
            OutError = FString::Printf(
                TEXT("V2 custom response '%s' input[%d] '%s' traces to no source expression (direct source '%s')."),
                *Expression->Description,
                InputIndex,
                *InputName,
                *Input.Input.Expression->GetName());
            return false;
        }
        if (!CustomCodeUsesIdentifier(Expression->Code, InputName))
        {
            OutError = FString::Printf(
                TEXT("V2 custom response '%s' input[%d] '%s' is not referenced as an exact HLSL identifier."),
                *Expression->Description,
                InputIndex,
                *InputName);
            return false;
        }
        Names.Add(Input.InputName);
    }
    return true;
}

bool FinishAndValidateMaterialCompilation(
    UMaterial* Material,
    FString& OutError)
{
    FMaterialResource* Resource = Material
        ? Material->GetMaterialResource(ERHIFeatureLevel::SM5)
        : nullptr;
    if (Resource)
    {
        Resource->FinishCompilation();
    }
    if (!Material ||
        Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5))
    {
        OutError = FString::Printf(
            TEXT("V2 master '%s' did not produce a valid regular SM5 shader map."),
            Material ? *Material->GetName() : TEXT("<null>"));
        return false;
    }
    return true;
}

bool ResolveExpectedMasterTextureBinding(
    bool bGlass,
    const FName& ParameterName,
    FString& OutTexturePath,
    EMaterialSamplerType& OutSamplerType)
{
    const FString Name = ParameterName.ToString();
    FString MapType;
    if (Name == TEXT("Tex_BaseColor"))
    {
        MapType = TEXT("BaseColor");
        OutSamplerType = SAMPLERTYPE_Color;
    }
    else if (Name == TEXT("Tex_Normal"))
    {
        MapType = TEXT("Normal");
        OutSamplerType = SAMPLERTYPE_Normal;
    }
    else if (Name == TEXT("Tex_ORM"))
    {
        MapType = TEXT("ORM");
        OutSamplerType = SAMPLERTYPE_Masks;
    }
    else if (!bGlass && Name == TEXT("Tex_Height"))
    {
        MapType = TEXT("Height");
        OutSamplerType = SAMPLERTYPE_LinearGrayscale;
    }
    else if (Name == TEXT("Tex_DetailNormal"))
    {
        MapType = TEXT("DetailNormal");
        OutSamplerType = SAMPLERTYPE_Normal;
    }
    else if (Name == TEXT("Tex_MacroVariation"))
    {
        MapType = TEXT("MacroVariation");
        OutSamplerType = SAMPLERTYPE_Masks;
    }
    else if (Name == TEXT("Tex_SurfaceMasks"))
    {
        MapType = TEXT("SurfaceMasks");
        OutSamplerType = SAMPLERTYPE_Masks;
    }
    else if (!bGlass && Name == TEXT("Tex_SharedWeatheringDecalMasks"))
    {
        MapType = TEXT("WeatheringDecalMasks");
        OutSamplerType = SAMPLERTYPE_Masks;
        const FString AssetName =
            TEXT("T_IPV_HeroV2_Shared_WeatheringDecalMasks");
        OutTexturePath = FString::Printf(
            TEXT("%s/%s.%s"),
            *MaterialTexturePath,
            *AssetName,
            *AssetName);
        return true;
    }
    else
    {
        return false;
    }
    const FString AssetName = FString::Printf(
        TEXT("T_IPV_HeroV2_%s_%s"),
        bGlass ? TEXT("Glass") : TEXT("Render"),
        *MapType);
    OutTexturePath = FString::Printf(
        TEXT("%s/%s.%s"),
        *MaterialTexturePath,
        *AssetName,
        *AssetName);
    return true;
}

bool ValidateHeroMaster(
    UMaterial* Material,
    bool bGlass,
    FString& OutError)
{
    const FString ExpectedName = bGlass
        ? TEXT("M_IPV_HeroGlass_V2")
        : TEXT("M_IPV_HeroSurface_V2");
    if (!Material ||
        Material->GetPathName() != MaterialObjectPath(ExpectedName) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != (bGlass ? BLEND_Translucent : BLEND_Opaque) ||
        Material->TwoSided ||
        !Material->bTangentSpaceNormal ||
        Material->bUseMaterialAttributes ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        (bGlass &&
            (Material->TranslucencyLightingMode != TLM_Surface ||
             Material->RefractionMethod != RM_IndexOfRefraction)))
    {
        OutError = FString::Printf(
            TEXT("V2 master '%s' has invalid domain/blend/shading settings."),
            *ExpectedName);
        return false;
    }
    TSet<FName> TextureParameters;
    TSet<FName> ScalarParameters;
    TSet<FName> VectorParameters;
    TSet<FName> StaticSwitches;
    TMap<FName, int32> ParameterNameCounts;
    TSet<const UMaterialExpression*> Reachable;
    const TArray<EMaterialProperty> Properties = bGlass
        ? TArray<EMaterialProperty>{
            MP_BaseColor, MP_Roughness, MP_Normal, MP_Refraction, MP_Opacity}
        : TArray<EMaterialProperty>{
            MP_BaseColor, MP_Roughness, MP_Metallic, MP_Normal,
            MP_AmbientOcclusion};
    for (const EMaterialProperty Property : Properties)
    {
        FExpressionInput* Input = Material->GetExpressionInputForProperty(Property);
        if (!Input || !Input->Expression)
        {
            OutError = FString::Printf(
                TEXT("V2 master '%s' has a disconnected required material property %d."),
                *ExpectedName,
                static_cast<int32>(Property));
            return false;
        }
        GatherReachableMaterialExpressions(Input->Expression, Reachable);
    }
    TSet<FString> CustomDescriptions;
    int32 BumpOffsetCount = 0;
    for (UMaterialExpression* Expression : Material->GetExpressions())
    {
        if (Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
        {
            OutError = FString::Printf(
                TEXT("V2 master '%s' contains a forbidden duplicate-prone StaticSwitchParameter node."),
                *ExpectedName);
            return false;
        }
        if (const UMaterialExpressionParameter* Parameter =
                Cast<UMaterialExpressionParameter>(Expression))
        {
            ParameterNameCounts.FindOrAdd(Parameter->ParameterName) += 1;
            if (Parameter->ParameterName.IsNone() ||
                ParameterNameCounts[Parameter->ParameterName] != 1 ||
                !Reachable.Contains(Expression))
            {
                OutError = FString::Printf(
                    TEXT("V2 master '%s' has an unnamed, duplicate, or output-unreachable parameter '%s'."),
                    *ExpectedName,
                    *Parameter->ParameterName.ToString());
                return false;
            }
        }
        if (const UMaterialExpressionStaticBoolParameter* StaticBoolParameter =
                Cast<UMaterialExpressionStaticBoolParameter>(Expression))
        {
            StaticSwitches.Add(StaticBoolParameter->ParameterName);
        }
        else if (const UMaterialExpressionTextureSampleParameter2D* TextureParameter =
                Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            FString ExpectedTexturePath;
            EMaterialSamplerType ExpectedSampler = SAMPLERTYPE_Color;
            if (!ResolveExpectedMasterTextureBinding(
                    bGlass,
                    TextureParameter->ParameterName,
                    ExpectedTexturePath,
                    ExpectedSampler) ||
                !TextureParameter->Texture ||
                TextureParameter->Texture->GetPathName() != ExpectedTexturePath ||
                TextureParameter->SamplerType != ExpectedSampler ||
                TextureParameter->MipValueMode != TMVM_None ||
                TextureParameter->SamplerSource != SSM_FromTextureAsset ||
                TextureParameter->ConstMipValue != 0 ||
                !TextureParameter->AutomaticViewMipBias)
            {
                OutError = FString::Printf(
                    TEXT("V2 master '%s' texture parameter '%s' has stale sampling semantics or default binding."),
                    *ExpectedName,
                    *TextureParameter->ParameterName.ToString());
                return false;
            }
            TextureParameters.Add(TextureParameter->ParameterName);
        }
        else if (const UMaterialExpressionScalarParameter* ScalarParameter =
                Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            ScalarParameters.Add(ScalarParameter->ParameterName);
        }
        else if (const UMaterialExpressionVectorParameter* VectorParameter =
                Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            VectorParameters.Add(VectorParameter->ParameterName);
        }
        if (const UMaterialExpressionStaticSwitch* Switch =
                Cast<UMaterialExpressionStaticSwitch>(Expression))
        {
            if (!Switch->A.Expression || !Switch->B.Expression ||
                !Switch->Value.Expression)
            {
                OutError = FString::Printf(
                    TEXT("V2 master '%s' contains an incomplete True/False/Value static branch."),
                    *ExpectedName);
                return false;
            }
        }
        if (const UMaterialExpressionCustom* Custom =
                Cast<UMaterialExpressionCustom>(Expression))
        {
            if (!ValidateCustomExpressionTopology(Custom, OutError) ||
                CustomDescriptions.Contains(Custom->Description))
            {
                if (OutError.IsEmpty())
                {
                    OutError = FString::Printf(
                        TEXT("V2 master '%s' duplicates custom response '%s'."),
                        *ExpectedName,
                        *Custom->Description);
                }
                return false;
            }
            CustomDescriptions.Add(Custom->Description);
        }
        if (const UMaterialExpressionBumpOffset* Bump =
                Cast<UMaterialExpressionBumpOffset>(Expression))
        {
            ++BumpOffsetCount;
            if (!Bump->Coordinate.Expression || !Bump->Height.Expression ||
                !Bump->HeightRatioInput.Expression ||
                !FMath::IsNearlyEqual(Bump->ReferencePlane, 0.5f))
            {
                OutError = TEXT("Opaque v2 bump-offset branch is disconnected or changed.");
                return false;
            }
        }
    }
    if (Reachable.Num() != Material->GetExpressions().Num())
    {
        OutError = FString::Printf(
            TEXT("V2 master '%s' contains an output-unreachable or extra expression node."),
            *ExpectedName);
        return false;
    }
    const TArray<const TCHAR*> OpaqueTextures = {
        TEXT("Tex_BaseColor"), TEXT("Tex_Normal"), TEXT("Tex_ORM"),
        TEXT("Tex_Height"), TEXT("Tex_DetailNormal"),
        TEXT("Tex_MacroVariation"), TEXT("Tex_SurfaceMasks"),
        TEXT("Tex_SharedWeatheringDecalMasks")};
    const TArray<const TCHAR*> OpaqueScalars = {
        TEXT("TileMeters"), TEXT("DetailTileMeters"), TEXT("MacroTileMeters"),
        TEXT("NormalStrength"), TEXT("DetailNormalStrength"),
        TEXT("RoughnessBias"), TEXT("MacroAlbedoStrength"),
        TEXT("MacroRoughnessStrength"), TEXT("WeatheringStrength"),
        TEXT("GenericDecalStrength"), TEXT("HeightMillimetres"),
        TEXT("BumpOffsetStrength"), TEXT("ExposedMetalMaskStrength")};
    const TArray<const TCHAR*> OpaqueSwitches = {
        TEXT("UseTextureSet"), TEXT("UseBumpOffset"),
        TEXT("UseGenericWeatheringDecals"), TEXT("UseExposedMetalMask")};
    const TArray<const TCHAR*> GlassTextures = {
        TEXT("Tex_BaseColor"), TEXT("Tex_Normal"), TEXT("Tex_ORM"),
        TEXT("Tex_DetailNormal"), TEXT("Tex_MacroVariation"),
        TEXT("Tex_SurfaceMasks")};
    const TArray<const TCHAR*> GlassScalars = {
        TEXT("TileMeters"), TEXT("DetailTileMeters"), TEXT("MacroTileMeters"),
        TEXT("NormalStrength"), TEXT("DetailNormalStrength"),
        TEXT("RoughnessBias"), TEXT("IOR"), TEXT("Opacity"),
        TEXT("DustStrength")};
    const TArray<const TCHAR*> OneVector = {TEXT("LookdevTint")};
    if (!SetEquals(TextureParameters, bGlass ? GlassTextures : OpaqueTextures) ||
        !SetEquals(ScalarParameters, bGlass ? GlassScalars : OpaqueScalars) ||
        !SetEquals(VectorParameters, OneVector) ||
        !SetEquals(StaticSwitches, bGlass
            ? TArray<const TCHAR*>()
            : OpaqueSwitches))
    {
        OutError = FString::Printf(
            TEXT("V2 master '%s' does not retain the exact interface parameter roster."),
            *ExpectedName);
        return false;
    }
    const TArray<FString> ExpectedCustomDescriptions = bGlass
        ? TArray<FString>{
            TEXT("TRIAD_IPV2_GlassAlbedo"),
            TEXT("TRIAD_IPV2_GlassRoughness"),
            TEXT("TRIAD_IPV2_GlassNormalRNM")}
        : TArray<FString>{
            TEXT("TRIAD_IPV2_HeightRatio"),
            TEXT("TRIAD_IPV2_OpaqueAlbedo"),
            TEXT("TRIAD_IPV2_OpaqueRoughness"),
            TEXT("TRIAD_IPV2_OpaqueMetallic"),
            TEXT("TRIAD_IPV2_NormalRNM")};
    if (CustomDescriptions.Num() != ExpectedCustomDescriptions.Num())
    {
        OutError = FString::Printf(
            TEXT("V2 master '%s' has the wrong response-node count."),
            *ExpectedName);
        return false;
    }
    for (const FString& Description : ExpectedCustomDescriptions)
    {
        if (!CustomDescriptions.Contains(Description))
        {
            OutError = FString::Printf(
                TEXT("V2 master '%s' lacks required response node '%s'."),
                *ExpectedName,
                *Description);
            return false;
        }
    }
    if (BumpOffsetCount != (bGlass ? 0 : 1))
    {
        OutError = FString::Printf(
            TEXT("V2 master '%s' has an invalid bump-offset node count."),
            *ExpectedName);
        return false;
    }
    if (!ValidateHeroMaterialGraphSeal(Material, OutError))
    {
        return false;
    }
    if (!FinishAndValidateMaterialCompilation(Material, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

FString FormatMaterialOverrideNames(const TSet<FName>& Names)
{
    TArray<FString> SortedNames;
    SortedNames.Reserve(Names.Num());
    for (const FName& Name : Names)
    {
        SortedNames.Add(Name.ToString());
    }
    SortedNames.Sort();
    return SortedNames.Num() > 0
        ? FString::Join(SortedNames, TEXT(","))
        : TEXT("<none>");
}

bool ValidateExactMaterialOverrideNames(
    const FString& AssetName,
    const TCHAR* OverrideKind,
    const TSet<FName>& Expected,
    const TSet<FName>& Actual,
    FString& OutError)
{
    if (AreSetsEqual(Expected, Actual))
    {
        return true;
    }
    TSet<FName> Missing;
    TSet<FName> Unexpected;
    for (const FName& Name : Expected)
    {
        if (!Actual.Contains(Name))
        {
            Missing.Add(Name);
        }
    }
    for (const FName& Name : Actual)
    {
        if (!Expected.Contains(Name))
        {
            Unexpected.Add(Name);
        }
    }
    OutError = FString::Printf(
        TEXT("V2 material instance '%s' %s override roster differs: missing=[%s]; unexpected=[%s]; expected=[%s]; actual=[%s]."),
        *AssetName,
        OverrideKind,
        *FormatMaterialOverrideNames(Missing),
        *FormatMaterialOverrideNames(Unexpected),
        *FormatMaterialOverrideNames(Expected),
        *FormatMaterialOverrideNames(Actual));
    return false;
}

FString EnabledBasePropertyOverrideNames(
    const FMaterialInstanceBasePropertyOverrides& Overrides)
{
    TArray<FString> Names;
    if (Overrides.bOverride_OpacityMaskClipValue)
    {
        Names.Add(TEXT("bOverride_OpacityMaskClipValue"));
    }
    if (Overrides.bOverride_BlendMode)
    {
        Names.Add(TEXT("bOverride_BlendMode"));
    }
    if (Overrides.bOverride_ShadingModel)
    {
        Names.Add(TEXT("bOverride_ShadingModel"));
    }
    if (Overrides.bOverride_DitheredLODTransition)
    {
        Names.Add(TEXT("bOverride_DitheredLODTransition"));
    }
    if (Overrides.bOverride_CastDynamicShadowAsMasked)
    {
        Names.Add(TEXT("bOverride_CastDynamicShadowAsMasked"));
    }
    if (Overrides.bOverride_TwoSided)
    {
        Names.Add(TEXT("bOverride_TwoSided"));
    }
    if (Overrides.bOverride_bIsThinSurface)
    {
        Names.Add(TEXT("bOverride_bIsThinSurface"));
    }
    if (Overrides.bOverride_OutputTranslucentVelocity)
    {
        Names.Add(TEXT("bOverride_OutputTranslucentVelocity"));
    }
    if (Overrides.bOverride_bHasPixelAnimation)
    {
        Names.Add(TEXT("bOverride_bHasPixelAnimation"));
    }
    if (Overrides.bOverride_bEnableTessellation)
    {
        Names.Add(TEXT("bOverride_bEnableTessellation"));
    }
    if (Overrides.bOverride_DisplacementScaling)
    {
        Names.Add(TEXT("bOverride_DisplacementScaling"));
    }
    if (Overrides.bOverride_bEnableDisplacementFade)
    {
        Names.Add(TEXT("bOverride_bEnableDisplacementFade"));
    }
    if (Overrides.bOverride_DisplacementFadeRange)
    {
        Names.Add(TEXT("bOverride_DisplacementFadeRange"));
    }
    if (Overrides.bOverride_MaxWorldPositionOffsetDisplacement)
    {
        Names.Add(TEXT("bOverride_MaxWorldPositionOffsetDisplacement"));
    }
    Names.Sort();
    return FString::Join(Names, TEXT(","));
}

bool ValidateHeroMaterialInstance(
    UMaterialInstanceConstant* Instance,
    const FHeroMaterialInstanceSpec& Spec,
    const FHeroMaterialPack& Pack,
    const TMap<FName, UTexture2D*>& Textures,
    FString& OutError)
{
    const FString ExpectedParentPath = MaterialObjectPath(Spec.ParentAssetName);
    if (!Instance ||
        Instance->GetPathName() != MaterialObjectPath(Spec.AssetName) ||
        !Instance->Parent ||
        Instance->Parent->GetPathName() != ExpectedParentPath)
    {
        OutError = FString::Printf(
            TEXT("V2 material instance '%s' has an invalid path or parent."),
            *Spec.AssetName);
        return false;
    }
    TSet<FName> ExpectedScalarNames;
    TSet<FName> ActualScalarNames;
    for (const TPair<FName, float>& Pair : Spec.Scalars)
    {
        ExpectedScalarNames.Add(Pair.Key);
    }
    for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualScalarNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = FString::Printf(
                TEXT("V2 material instance '%s' has an invalid or duplicate scalar override."),
                *Spec.AssetName);
            return false;
        }
        ActualScalarNames.Add(Value.ParameterInfo.Name);
    }
    TSet<FName> ExpectedVectorNames;
    TSet<FName> ActualVectorNames;
    for (const TPair<FName, FLinearColor>& Pair : Spec.Vectors)
    {
        ExpectedVectorNames.Add(Pair.Key);
    }
    for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualVectorNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = FString::Printf(
                TEXT("V2 material instance '%s' has an invalid or duplicate vector override."),
                *Spec.AssetName);
            return false;
        }
        ActualVectorNames.Add(Value.ParameterInfo.Name);
    }
    TSet<FName> ExpectedTextureNames;
    if (!Spec.bSpecialParameterOnly)
    {
        ExpectedTextureNames = Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V2")
            ? TSet<FName>{
                TEXT("Tex_BaseColor"), TEXT("Tex_Normal"), TEXT("Tex_ORM"),
                TEXT("Tex_DetailNormal"), TEXT("Tex_MacroVariation"),
                TEXT("Tex_SurfaceMasks")}
            : TSet<FName>{
                TEXT("Tex_BaseColor"), TEXT("Tex_Normal"), TEXT("Tex_ORM"),
                TEXT("Tex_Height"), TEXT("Tex_DetailNormal"),
                TEXT("Tex_MacroVariation"), TEXT("Tex_SurfaceMasks"),
                TEXT("Tex_SharedWeatheringDecalMasks")};
    }
    TSet<FName> ActualTextureNames;
    for (const FTextureParameterValue& Value : Instance->TextureParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualTextureNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = FString::Printf(
                TEXT("V2 material instance '%s' has an invalid or duplicate texture override."),
                *Spec.AssetName);
            return false;
        }
        ActualTextureNames.Add(Value.ParameterInfo.Name);
    }
    const FStaticParameterSet StaticParameters = Instance->GetStaticParameters();
    TSet<FName> ExpectedStaticNames;
    TSet<FName> ActualStaticNames;
    for (const TPair<FName, bool>& Pair : Spec.StaticSwitches)
    {
        ExpectedStaticNames.Add(Pair.Key);
    }
    for (const FStaticSwitchParameter& Value :
         StaticParameters.StaticSwitchParameters)
    {
        if (!Value.IsOverride() || Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualStaticNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = FString::Printf(
                TEXT("V2 material instance '%s' has an invalid or duplicate static override."),
                *Spec.AssetName);
            return false;
        }
        ActualStaticNames.Add(Value.ParameterInfo.Name);
    }
    if (!ValidateExactMaterialOverrideNames(
            Spec.AssetName, TEXT("scalar"),
            ExpectedScalarNames, ActualScalarNames, OutError) ||
        !ValidateExactMaterialOverrideNames(
            Spec.AssetName, TEXT("vector"),
            ExpectedVectorNames, ActualVectorNames, OutError) ||
        !ValidateExactMaterialOverrideNames(
            Spec.AssetName, TEXT("texture"),
            ExpectedTextureNames, ActualTextureNames, OutError) ||
        !ValidateExactMaterialOverrideNames(
            Spec.AssetName, TEXT("static-switch"),
            ExpectedStaticNames, ActualStaticNames, OutError))
    {
        return false;
    }
    const TPair<const TCHAR*, int32> UnexpectedOverrideFamilies[] = {
        {TEXT("double-vector"), Instance->DoubleVectorParameterValues.Num()},
        {TEXT("texture-collection"), Instance->TextureCollectionParameterValues.Num()},
        {TEXT("runtime-virtual-texture"), Instance->RuntimeVirtualTextureParameterValues.Num()},
        {TEXT("sparse-volume-texture"), Instance->SparseVolumeTextureParameterValues.Num()},
        {TEXT("font"), Instance->FontParameterValues.Num()},
        {TEXT("user-scene-texture"), Instance->UserSceneTextureOverrides.Num()},
        {TEXT("static-component-mask"),
            StaticParameters.EditorOnly.StaticComponentMaskParameters.Num()},
        {TEXT("terrain-layer-weight"),
            StaticParameters.EditorOnly.TerrainLayerWeightParameters.Num()}};
    for (const TPair<const TCHAR*, int32>& Family : UnexpectedOverrideFamilies)
    {
        if (Family.Value != 0)
        {
            OutError = FString::Printf(
                TEXT("V2 material instance '%s' has %d unexpected %s override(s)."),
                *Spec.AssetName,
                Family.Value,
                Family.Key);
            return false;
        }
    }
    if (StaticParameters.bHasMaterialLayers)
    {
        OutError = FString::Printf(
            TEXT("V2 material instance '%s' has an unexpected material-layer override."),
            *Spec.AssetName);
        return false;
    }
    // UE5.5 UpdateOverridableBaseProperties deliberately copies inherited
    // parent values into the inactive value fields of BasePropertyOverrides.
    // Those cache values are not overrides and can differ from the struct's
    // constructor defaults (notably 0.3333f versus 0.333333f opacity clip).
    // Check every enable flag directly so an explicit equal-to-parent override
    // still fails closed while benign inherited cache values remain valid.
    const FString EnabledBaseOverrides =
        EnabledBasePropertyOverrideNames(Instance->BasePropertyOverrides);
    if (!EnabledBaseOverrides.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("V2 material instance '%s' enables forbidden base-property override flag(s): [%s]."),
            *Spec.AssetName,
            *EnabledBaseOverrides);
        return false;
    }
    for (const TPair<FName, float>& Pair : Spec.Scalars)
    {
        float Actual = 0.0f;
        if (!Instance->GetScalarParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Actual, true) ||
            !FMath::IsNearlyEqual(Actual, Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("V2 material instance '%s' scalar '%s' changed."),
                *Spec.AssetName,
                *Pair.Key.ToString());
            return false;
        }
    }
    for (const TPair<FName, FLinearColor>& Pair : Spec.Vectors)
    {
        FLinearColor Actual;
        if (!Instance->GetVectorParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Actual, true) ||
            !Actual.Equals(Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("V2 material instance '%s' vector '%s' changed."),
                *Spec.AssetName,
                *Pair.Key.ToString());
            return false;
        }
    }
    for (const TPair<FName, bool>& Pair : Spec.StaticSwitches)
    {
        bool Actual = !Pair.Value;
        FGuid ExpressionGuid;
        if (!Instance->GetStaticSwitchParameterValue(
                FHashedMaterialParameterInfo(Pair.Key),
                Actual,
                ExpressionGuid,
                true) ||
            Actual != Pair.Value)
        {
            OutError = FString::Printf(
                TEXT("V2 material instance '%s' static switch '%s' changed."),
                *Spec.AssetName,
                *Pair.Key.ToString());
            return false;
        }
    }
    if (!Spec.bSpecialParameterOnly)
    {
        const TPair<const TCHAR*, const TCHAR*> TextureBindings[] = {
            {TEXT("BaseColor"), TEXT("Tex_BaseColor")},
            {TEXT("Normal"), TEXT("Tex_Normal")},
            {TEXT("ORM"), TEXT("Tex_ORM")},
            {TEXT("Height"), TEXT("Tex_Height")},
            {TEXT("DetailNormal"), TEXT("Tex_DetailNormal")},
            {TEXT("MacroVariation"), TEXT("Tex_MacroVariation")},
            {TEXT("SurfaceMasks"), TEXT("Tex_SurfaceMasks")}};
        for (const TPair<const TCHAR*, const TCHAR*>& Binding : TextureBindings)
        {
            if (Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V2") &&
                FString(Binding.Key) == TEXT("Height"))
            {
                continue;
            }
            UTexture2D* Expected = FindTextureForMaterial(
                Spec.Id, Binding.Key, Pack, Textures);
            UTexture* Actual = nullptr;
            if (!Expected ||
                !Instance->GetTextureParameterValue(
                    FHashedMaterialParameterInfo(Binding.Value),
                    Actual,
                    true) ||
                Actual != Expected)
            {
                OutError = FString::Printf(
                    TEXT("V2 material instance '%s' texture '%s' changed."),
                    *Spec.AssetName,
                    Binding.Value);
                return false;
            }
        }
        if (Spec.ParentAssetName != TEXT("M_IPV_HeroGlass_V2"))
        {
            UTexture2D* Expected = FindTextureForMaterial(
                TEXT("Shared"),
                TEXT("SharedWeatheringDecalMasks"),
                Pack,
                Textures);
            UTexture* Actual = nullptr;
            if (!Expected ||
                !Instance->GetTextureParameterValue(
                    FHashedMaterialParameterInfo(
                        TEXT("Tex_SharedWeatheringDecalMasks")),
                    Actual,
                    true) ||
                Actual != Expected)
            {
                OutError = FString::Printf(
                    TEXT("V2 material instance '%s' shared weathering map changed."),
                    *Spec.AssetName);
                return false;
            }
        }
    }
    // FinishCompilation alone only waits for jobs already submitted. EnsureIsComplete
    // first submits any prepared shader jobs for every active feature level and then
    // waits, which makes this validation deterministic immediately after import.
    Instance->EnsureIsComplete();
    FMaterialResource* InstanceResource =
        Instance->GetMaterialResource(ERHIFeatureLevel::SM5);
    const bool bRequiresMicOwnedStaticResource = Spec.StaticSwitches.Num() > 0;
    const bool bResourcePresent = InstanceResource != nullptr;
    const bool bOwnedByInstance =
        InstanceResource && InstanceResource->GetMaterialInstance() == Instance;
    const bool bInstanceIsCompiling = Instance->IsCompiling();
    const bool bInstanceIsComplete = Instance->IsComplete();
    const bool bResourceCompilationFinished =
        InstanceResource && InstanceResource->IsCompilationFinished();
    const bool bShaderMapPresent =
        InstanceResource && InstanceResource->GetGameThreadShaderMap() != nullptr;
    const bool bShaderMapComplete =
        InstanceResource && InstanceResource->IsGameThreadShaderMapComplete();
    const bool bValidGameThreadShaderMap =
        InstanceResource && InstanceResource->HasValidGameThreadShaderMap();
    const TArray<FString> CompileErrors = InstanceResource
        ? InstanceResource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    const FString CompileErrorText = CompileErrors.Num() > 0
        ? FString::Join(CompileErrors, TEXT(" | "))
        : TEXT("none");
    if (!bResourcePresent ||
        (bRequiresMicOwnedStaticResource && !bOwnedByInstance) ||
        bInstanceIsCompiling ||
        !bInstanceIsComplete ||
        !bResourceCompilationFinished ||
        !bShaderMapPresent ||
        !bShaderMapComplete ||
        CompileErrors.Num() != 0)
    {
        const auto BoolText = [](const bool bValue)
        {
            return bValue ? TEXT("true") : TEXT("false");
        };
        OutError = FString::Printf(
            TEXT("V2 material instance '%s' SM5 permutation invalid: ")
            TEXT("resourcePresent=%s; requiresMicOwnedStaticResource=%s; ")
            TEXT("ownedByInstance=%s; instanceIsCompiling=%s; ")
            TEXT("instanceIsComplete=%s; resourceCompilationFinished=%s; ")
            TEXT("shaderMapPresent=%s; shaderMapComplete=%s; ")
            TEXT("validGameThreadShaderMap=%s; compileErrors=[%s]."),
            *Spec.AssetName,
            BoolText(bResourcePresent),
            BoolText(bRequiresMicOwnedStaticResource),
            BoolText(bOwnedByInstance),
            BoolText(bInstanceIsCompiling),
            BoolText(bInstanceIsComplete),
            BoolText(bResourceCompilationFinished),
            BoolText(bShaderMapPresent),
            BoolText(bShaderMapComplete),
            BoolText(bValidGameThreadShaderMap),
            *CompileErrorText);
        return false;
    }
    // HasValidGameThreadShaderMap additionally requires the shader map's
    // bCompilationFinalized flag. UE5.5's editor ODSC/material-map-DDC-disabled
    // path deliberately installs a complete frozen map without setting that flag
    // so missing permutations can be requested on demand. The renderer itself
    // selects material versus fallback using IsRenderingThreadShaderMapComplete.
    // Keep this value in the diagnostic above, but do not reject that supported
    // editor state after all renderer-readiness and compile-error gates pass.
    OutError.Reset();
    return true;
}

bool ComputeHeroMeshPayloadDigest(
    UStaticMesh* Mesh,
    FString& OutDigest,
    FString& OutError)
{
    if (!Mesh || !Mesh->GetRenderData() ||
        Mesh->GetRenderData()->DerivedDataKey.IsEmpty() ||
        !Mesh->GetLightingGuid().IsValid())
    {
        OutError = TEXT("V2 hero lacks a stable render-data key or lighting GUID.");
        return false;
    }
    FString Canonical;
    AppendGraphToken(
        Canonical,
        TEXT("triad.istana_public_view_hero_v2.mesh_render_payload.v1"));
    AppendGraphToken(Canonical, Mesh->GetRenderData()->DerivedDataKey);
    AppendGraphToken(
        Canonical,
        Mesh->GetLightingGuid().ToString(EGuidFormats::Digits));
    FTCHARToUTF8 Utf8(*Canonical);
    if (!CalculateSha256Bytes(Utf8.Get(), Utf8.Length(), OutDigest))
    {
        OutError = TEXT("Could not hash the v2 hero render payload identity.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateHeroMesh(
    UStaticMesh* Mesh,
    const FHeroGeometryContract& Geometry,
    const TMap<FString, UMaterialInstanceConstant*>& SlotMaterials,
    FString& OutError)
{
    if (!Mesh || Mesh->GetPathName() != HeroMeshObjectPath ||
        Mesh->GetNumSourceModels() != 1 || !Mesh->GetRenderData() ||
        Mesh->GetRenderData()->LODResources.Num() != 1 ||
        Mesh->GetRenderData()->LODResources[0].GetNumTriangles() !=
            Geometry.Triangles ||
        Mesh->NaniteSettings.bEnabled)
    {
        OutError = TEXT("V2 hero mesh path/LOD/triangle/no-Nanite contract failed.");
        return false;
    }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector ExpectedMin = Geometry.BoundsMinMeters * 100.0;
    const FVector ExpectedMax = Geometry.BoundsMaxMeters * 100.0;
    const FVector ActualMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector ActualMax = Bounds.Origin + Bounds.BoxExtent;
    if (!ActualMin.Equals(ExpectedMin, BoundsToleranceCentimeters) ||
        !ActualMax.Equals(ExpectedMax, BoundsToleranceCentimeters))
    {
        OutError = FString::Printf(
            TEXT("V2 hero imported bounds changed: min=%s max=%s expected min=%s max=%s cm."),
            *ActualMin.ToString(),
            *ActualMax.ToString(),
            *ExpectedMin.ToString(),
            *ExpectedMax.ToString());
        return false;
    }
    const FMeshBuildSettings& Build = Mesh->GetSourceModel(0).BuildSettings;
    if (Build.bRecomputeNormals || !Build.bRecomputeTangents ||
        !Build.bUseMikkTSpace || !Build.bRemoveDegenerates ||
        !Build.bUseFullPrecisionUVs ||
        !Build.BuildScale3D.Equals(FVector::OneVector, 0.000001))
    {
        OutError = TEXT("V2 hero source-model normal/tangent/identity-scale contract failed.");
        return false;
    }
    const UBodySetup* BodySetup = Mesh->GetBodySetup();
    if (BodySetup &&
        (BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple ||
         BodySetup->AggGeom.GetElementCount() != 0))
    {
        OutError = TEXT("V2 visual hero must contain no simple or complex-as-simple collision.");
        return false;
    }
    if (Mesh->GetStaticMaterials().Num() != ExpectedHeroSlots().Num())
    {
        OutError = TEXT("V2 hero does not have exactly eleven material slots.");
        return false;
    }
    TSet<FString> SeenSlots;
    for (const FStaticMaterial& StaticMaterial : Mesh->GetStaticMaterials())
    {
        const FString Slot = StaticMaterial.MaterialSlotName.ToString();
        UMaterialInstanceConstant* const* Expected = SlotMaterials.Find(Slot);
        if (!ExpectedHeroSlots().Contains(Slot) ||
            Slot.StartsWith(TEXT("M_IPV_")) ||
            SeenSlots.Contains(Slot) || !Expected ||
            StaticMaterial.MaterialInterface != *Expected)
        {
            OutError = FString::Printf(
                TEXT("V2 hero material binding is invalid for slot '%s'."),
                *Slot);
            return false;
        }
        SeenSlots.Add(Slot);
    }
    if (SeenSlots.Num() != ExpectedHeroSlots().Num())
    {
        OutError = TEXT("V2 hero material slot roster is incomplete.");
        return false;
    }
    if (!ValidateSingleSourceProvenance(
            Mesh->GetAssetImportData(),
            ProjectSourcePath(GeometryObjRelativePath),
            HeroMeshObjectPath,
            OutError))
    {
        return false;
    }
    FString PayloadDigest;
    if (!ComputeHeroMeshPayloadDigest(Mesh, PayloadDigest, OutError) ||
        !ValidateEmbeddedPayloadSeal(
            Mesh,
            TEXT("mesh_render_ddc_and_lighting"),
            PayloadDigest,
            OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

enum class EHeroV2AssetState : uint8
{
    Absent,
    CompleteValid,
    PartialOrInvalid
};

void BuildExpectedAssetPaths(
    const FHeroMaterialPack& Pack,
    TSet<FString>& OutPaths)
{
    OutPaths.Reset();
    OutPaths.Add(HeroMeshObjectPath);
    OutPaths.Add(MaterialObjectPath(TEXT("M_IPV_HeroSurface_V2")));
    OutPaths.Add(MaterialObjectPath(TEXT("M_IPV_HeroGlass_V2")));
    for (const FHeroTextureSpec& Spec : Pack.Textures)
    {
        OutPaths.Add(TextureObjectPath(Spec));
    }
    for (const FHeroMaterialInstanceSpec& Spec : Pack.Instances)
    {
        OutPaths.Add(MaterialObjectPath(Spec.AssetName));
    }
}

EHeroV2AssetState InspectHeroV2Assets(
    UEditorAssetSubsystem* AssetSubsystem,
    const FHeroGeometryContract& Geometry,
    const FHeroMaterialPack& Pack,
    UStaticMesh** OutHero,
    FString& OutReport)
{
    if (!AssetSubsystem)
    {
        OutReport = TEXT("Editor Asset Subsystem is unavailable.");
        return EHeroV2AssetState::PartialOrInvalid;
    }
    TSet<FString> ExpectedPaths;
    BuildExpectedAssetPaths(Pack, ExpectedPaths);
    int32 ExistingCount = 0;
    for (const FString& Path : ExpectedPaths)
    {
        if (DoesObjectOrPackageExist(AssetSubsystem, Path))
        {
            ++ExistingCount;
        }
    }
    if (ExistingCount == 0)
    {
        OutReport = TEXT("V2 hero/material namespace is absent.");
        return EHeroV2AssetState::Absent;
    }
    if (ExistingCount != ExpectedPaths.Num())
    {
        OutReport = FString::Printf(
            TEXT("V2 namespace is partial: %d of %d exact assets exist; refusing overwrite or repair-in-place."),
            ExistingCount,
            ExpectedPaths.Num());
        return EHeroV2AssetState::PartialOrInvalid;
    }

    FString Error;
    TMap<FName, UTexture2D*> Textures;
    for (const FHeroTextureSpec& Spec : Pack.Textures)
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(
            nullptr, *TextureObjectPath(Spec));
        if (!ValidateHeroTexture(Texture, Spec, Error))
        {
            OutReport = Error;
            return EHeroV2AssetState::PartialOrInvalid;
        }
        Textures.Add(FName(*Spec.AssetName), Texture);
    }
    UMaterial* OpaqueMaster = LoadObject<UMaterial>(
        nullptr,
        *MaterialObjectPath(TEXT("M_IPV_HeroSurface_V2")));
    UMaterial* GlassMaster = LoadObject<UMaterial>(
        nullptr,
        *MaterialObjectPath(TEXT("M_IPV_HeroGlass_V2")));
    if (!ValidateHeroMaster(OpaqueMaster, false, Error) ||
        !ValidateHeroMaster(GlassMaster, true, Error))
    {
        OutReport = Error;
        return EHeroV2AssetState::PartialOrInvalid;
    }
    TMap<FString, UMaterialInstanceConstant*> SlotMaterials;
    for (const FHeroMaterialInstanceSpec& Spec : Pack.Instances)
    {
        UMaterialInstanceConstant* Instance =
            LoadObject<UMaterialInstanceConstant>(
                nullptr, *MaterialObjectPath(Spec.AssetName));
        if (!ValidateHeroMaterialInstance(
                Instance, Spec, Pack, Textures, Error))
        {
            OutReport = Error;
            return EHeroV2AssetState::PartialOrInvalid;
        }
        for (const FString& Slot : Spec.V2Slots)
        {
            SlotMaterials.Add(Slot, Instance);
        }
    }
    UStaticMesh* Hero = LoadObject<UStaticMesh>(nullptr, *HeroMeshObjectPath);
    if (Hero)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Hero});
    }
    if (!ValidateHeroMesh(Hero, Geometry, SlotMaterials, Error))
    {
        OutReport = Error;
        return EHeroV2AssetState::PartialOrInvalid;
    }
    if (OutHero)
    {
        *OutHero = Hero;
    }
    OutReport = FString::Printf(
        TEXT("Validated non-overwriting v2 asset set: one regular %d-triangle hero, 57 exact generated textures, two masters, ten instances, eleven M_IPV2 slot bindings, and no hero collision/Nanite."),
        Geometry.Triangles);
    return EHeroV2AssetState::CompleteValid;
}

bool PrepareHeroObjAdapter(
    FString& OutAdaptedObjFilename,
    FString& OutError)
{
    const FString AdapterDirectory =
        FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectIntermediateDir(),
            TEXT("TRIAD/IstanaPublicViewV2/LegacyObjMaterialSlots")));
    if (!IFileManager::Get().MakeDirectory(*AdapterDirectory, true))
    {
        OutError = TEXT("Could not create the isolated v2 legacy-OBJ adapter directory.");
        return false;
    }
    const FString MtlFilename = TEXT("IstanaPublicViewV2Slots.mtl");
    const FString MtlPath = FPaths::Combine(AdapterDirectory, MtlFilename);
    FString MtlText;
    for (const FString& Slot : ExpectedHeroSlots())
    {
        MtlText += FString::Printf(
            TEXT("newmtl %s\nKd 1.000000 1.000000 1.000000\n\n"),
            *Slot);
    }
    if (!FFileHelper::SaveStringToFile(
            MtlText,
            *MtlPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = TEXT("Could not write the isolated v2 material-slot adapter MTL.");
        return false;
    }

    const FString OriginalPath = ProjectSourcePath(GeometryObjRelativePath);
    OutAdaptedObjFilename = FPaths::Combine(
        AdapterDirectory,
        TEXT("SM_IstanaPublicViewV2_Building_Hero.import.obj"));
    TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*OriginalPath));
    TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*OutAdaptedObjFilename));
    if (!Reader || !Writer)
    {
        OutError = TEXT("Could not open the v2 source/adapter OBJ streams.");
        return false;
    }
    const FString Header = FString::Printf(TEXT("mtllib %s\n"), *MtlFilename);
    FTCHARToUTF8 HeaderUtf8(*Header);
    Writer->Serialize(
        const_cast<ANSICHAR*>(HeaderUtf8.Get()),
        HeaderUtf8.Length());
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(4 * 1024 * 1024);
    int64 Remaining = Reader->TotalSize();
    while (Remaining > 0 && !Reader->IsError() && !Writer->IsError())
    {
        const int64 Chunk = FMath::Min<int64>(Remaining, Buffer.Num());
        Reader->Serialize(Buffer.GetData(), Chunk);
        Writer->Serialize(Buffer.GetData(), Chunk);
        Remaining -= Chunk;
    }
    Reader->Close();
    Writer->Close();
    if (Remaining != 0 || Reader->IsError() || Writer->IsError())
    {
        OutError = TEXT("Could not stream the complete frozen OBJ into its import-only adapter.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ImportHeroV2AssetSet(
    UEditorAssetSubsystem* AssetSubsystem,
    const FHeroGeometryContract& Geometry,
    const FHeroMaterialPack& Pack,
    const FString& SourceReport,
    FString& OutMessage)
{
    FString ExistingReport;
    const EHeroV2AssetState ExistingState = InspectHeroV2Assets(
        AssetSubsystem, Geometry, Pack, nullptr, ExistingReport);
    if (ExistingState == EHeroV2AssetState::CompleteValid)
    {
        OutMessage = SourceReport + TEXT(" ") + ExistingReport +
            TEXT(" Existing complete v2 output was accepted idempotently; no package was saved or overwritten.");
        return true;
    }
    if (ExistingState != EHeroV2AssetState::Absent)
    {
        OutMessage = TEXT("V2_IMPORT_REFUSED: ") + ExistingReport;
        return false;
    }

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TArray<UObject*> AssetsToSave;
    TArray<UAssetImportTask*> TextureTasks;
    TMap<FName, UAssetImportTask*> TextureTaskByName;
    for (const FHeroTextureSpec& Spec : Pack.Textures)
    {
        UTextureFactory* Factory = NewObject<UTextureFactory>();
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        if (!Factory || !Task)
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: could not allocate texture import tasks.");
            return false;
        }
        Factory->bCreateMaterial = false;
        Factory->CompressionSettings = TextureCompression(Spec.Usage);
        Factory->LODGroup = TextureLodGroup(Spec.Usage);
        Factory->MipGenSettings = TMGS_FromTextureGroup;
        Factory->bFlipNormalMapGreenChannel = false;
        Factory->ColorSpaceMode = TextureIsSrgb(Spec.Usage)
            ? ETextureSourceColorSpace::SRGB
            : ETextureSourceColorSpace::Linear;
        Task->Filename = ProjectSourcePath(FPaths::Combine(
            MaterialSourceRoot, TEXT("Generated"), Spec.SourceFilename));
        Task->DestinationPath = MaterialTexturePath;
        Task->DestinationName = Spec.AssetName;
        Task->bReplaceExisting = false;
        Task->bReplaceExistingSettings = false;
        Task->bAutomated = true;
        Task->bSave = false;
        Task->bAsync = false;
        Task->Factory = Factory;
        Task->Options = Factory;
        TextureTasks.Add(Task);
        TextureTaskByName.Add(FName(*Spec.AssetName), Task);
    }
    AssetTools.ImportAssetTasks(TextureTasks);

    FString Error;
    TMap<FName, UTexture2D*> Textures;
    for (const FHeroTextureSpec& Spec : Pack.Textures)
    {
        UTexture2D* Texture = nullptr;
        if (UAssetImportTask* Task = TextureTaskByName.FindRef(FName(*Spec.AssetName)))
        {
            for (UObject* Object : Task->GetObjects())
            {
                Texture = Cast<UTexture2D>(Object);
                if (Texture)
                {
                    break;
                }
            }
        }
        if (!Texture)
        {
            Texture = LoadObject<UTexture2D>(nullptr, *TextureObjectPath(Spec));
        }
        if (!Texture)
        {
            OutMessage = FString::Printf(
                TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: no imported texture '%s'."),
                *Spec.AssetName);
            return false;
        }
        Texture->Modify();
        Texture->SRGB = TextureIsSrgb(Spec.Usage);
        Texture->CompressionSettings = TextureCompression(Spec.Usage);
        Texture->LODGroup = TextureLodGroup(Spec.Usage);
        Texture->MipGenSettings = TMGS_FromTextureGroup;
        Texture->AddressX = TA_Wrap;
        Texture->AddressY = TA_Wrap;
        // TF_Default delegates filtering to the texture group, whose UE 5.5
        // sampler path supplies anisotropy; TF_Anisotropic is not an asset enum.
        Texture->Filter = TF_Default;
        Texture->bFlipGreenChannel = false;
        Texture->VirtualTextureStreaming = false;
        Texture->LODBias = 0;
        Texture->MaxTextureSize = 0;
        Texture->NeverStream = false;
        Texture->PowerOfTwoMode = ETexturePowerOfTwoSetting::None;
        Texture->CompressionNoAlpha = false;
        if (!Texture->AssetImportData)
        {
            OutMessage = FString::Printf(
                TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: texture '%s' lacks import provenance."),
                *Spec.AssetName);
            return false;
        }
        Texture->AssetImportData->Update(ProjectSourcePath(FPaths::Combine(
            MaterialSourceRoot,
            TEXT("Generated"),
            Spec.SourceFilename)));
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (!SealEmbeddedPayload(
                Texture,
                TEXT("texture_source_id"),
                Texture->Source.GetIdString(),
                Error) ||
            !ValidateHeroTexture(Texture, Spec, Error))
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: ") + Error;
            return false;
        }
        Textures.Add(FName(*Spec.AssetName), Texture);
        AssetsToSave.Add(Texture);
    }

    UMaterial* OpaqueMaster = CreateOpaqueHeroMaster(Pack, Textures, AssetTools, Error);
    UMaterial* GlassMaster = CreateGlassHeroMaster(Pack, Textures, AssetTools, Error);
    if (!OpaqueMaster || !GlassMaster ||
        !ValidateHeroMaster(OpaqueMaster, false, Error) ||
        !ValidateHeroMaster(GlassMaster, true, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: ") + Error;
        return false;
    }
    AssetsToSave.Add(OpaqueMaster);
    AssetsToSave.Add(GlassMaster);

    TMap<FString, UMaterialInstanceConstant*> SlotMaterials;
    for (const FHeroMaterialInstanceSpec& Spec : Pack.Instances)
    {
        UMaterialInstanceConstant* Instance = CreateHeroMaterialInstance(
            Spec, Pack, Textures, OpaqueMaster, GlassMaster,
            AssetTools, Error);
        if (!Instance ||
            !ValidateHeroMaterialInstance(
                Instance, Spec, Pack, Textures, Error))
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: ") + Error;
            return false;
        }
        for (const FString& Slot : Spec.V2Slots)
        {
            SlotMaterials.Add(Slot, Instance);
        }
        AssetsToSave.Add(Instance);
    }
    if (SlotMaterials.Num() != ExpectedHeroSlots().Num())
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: not all eleven v2 slots have exact material instances.");
        return false;
    }

    FString AdaptedObjFilename;
    if (!PrepareHeroObjAdapter(AdaptedObjFilename, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: ") + Error;
        return false;
    }
    UFbxImportUI* ImportOptions = NewObject<UFbxImportUI>();
    UFbxFactory* ImportFactory = NewObject<UFbxFactory>();
    UAssetImportTask* HeroTask = NewObject<UAssetImportTask>();
    if (!ImportOptions || !ImportFactory || !HeroTask ||
        !ImportOptions->StaticMeshImportData)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: could not allocate deterministic regular-mesh OBJ import options.");
        return false;
    }
    ImportOptions->bImportAsSkeletal = false;
    ImportOptions->MeshTypeToImport = FBXIT_StaticMesh;
    ImportOptions->bAutomatedImportShouldDetectType = false;
    ImportOptions->bImportMesh = true;
    ImportOptions->bImportMaterials = false;
    ImportOptions->bImportTextures = false;
    ImportOptions->StaticMeshImportData->bConvertScene = false;
    ImportOptions->StaticMeshImportData->bConvertSceneUnit = false;
    ImportOptions->StaticMeshImportData->bForceFrontXAxis = false;
    ImportOptions->StaticMeshImportData->ImportTranslation = FVector::ZeroVector;
    ImportOptions->StaticMeshImportData->ImportRotation = FRotator::ZeroRotator;
    ImportOptions->StaticMeshImportData->ImportUniformScale = 1.0f;
    ImportOptions->StaticMeshImportData->bCombineMeshes = true;
    ImportOptions->StaticMeshImportData->bAutoGenerateCollision = false;
    ImportOptions->StaticMeshImportData->bGenerateLightmapUVs = true;
    ImportOptions->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormals;
    ImportOptions->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    ImportOptions->StaticMeshImportData->bBuildNanite = false;
    ImportOptions->StaticMeshImportData->bRemoveDegenerates = true;
    ImportFactory->ImportUI = ImportOptions;
    ImportFactory->SetDetectImportTypeOnImport(false);
    HeroTask->Filename = AdaptedObjFilename;
    HeroTask->DestinationPath = HeroMeshPath;
    HeroTask->DestinationName = HeroMeshName;
    HeroTask->bReplaceExisting = false;
    HeroTask->bReplaceExistingSettings = false;
    HeroTask->bAutomated = true;
    HeroTask->bSave = false;
    HeroTask->bAsync = false;
    HeroTask->Factory = ImportFactory;
    HeroTask->Options = ImportOptions;
    TArray<UAssetImportTask*> HeroTasks = {HeroTask};
    AssetTools.ImportAssetTasks(HeroTasks);

    UStaticMesh* Hero = nullptr;
    for (UObject* Object : HeroTask->GetObjects())
    {
        Hero = Cast<UStaticMesh>(Object);
        if (Hero)
        {
            break;
        }
    }
    if (!Hero)
    {
        Hero = LoadObject<UStaticMesh>(nullptr, *HeroMeshObjectPath);
    }
    if (!Hero)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: OBJ import produced no v2 hero mesh.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Hero});
    Hero->Modify();
    UAssetImportData* ImportData = Hero->GetAssetImportData();
    if (!ImportData)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: v2 hero lacks import provenance.");
        return false;
    }
    ImportData->Update(ProjectSourcePath(GeometryObjRelativePath));
    if (Hero->GetNumSourceModels() != 1)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: v2 hero requires exactly one source LOD.");
        return false;
    }
    FMeshBuildSettings& Build = Hero->GetSourceModel(0).BuildSettings;
    Build.bRecomputeNormals = false;
    Build.bRecomputeTangents = true;
    Build.bUseMikkTSpace = true;
    Build.bRemoveDegenerates = true;
    Build.bUseFullPrecisionUVs = true;
    Build.BuildScale3D = FVector::OneVector;
    Hero->NaniteSettings.bEnabled = false;
    Hero->CreateBodySetup();
    if (UBodySetup* BodySetup = Hero->GetBodySetup())
    {
        BodySetup->Modify();
        BodySetup->RemoveSimpleCollision();
        BodySetup->CollisionTraceFlag = CTF_UseDefault;
        BodySetup->InvalidatePhysicsData();
    }
    for (const FString& Slot : ExpectedHeroSlots())
    {
        const int32 MaterialIndex = Hero->GetMaterialIndex(FName(*Slot));
        UMaterialInstanceConstant* Material = SlotMaterials.FindRef(Slot);
        if (MaterialIndex == INDEX_NONE || !Material)
        {
            OutMessage = FString::Printf(
                TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: hero lacks exact slot/material '%s'."),
                *Slot);
            return false;
        }
        Hero->SetMaterial(MaterialIndex, Material);
    }
    Hero->PostEditChange();
    if (!Hero->GetLightingGuid().IsValid())
    {
        Hero->SetLightingGuid();
    }
    Hero->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Hero});
    FString MeshPayloadDigest;
    if (!ComputeHeroMeshPayloadDigest(Hero, MeshPayloadDigest, Error) ||
        !SealEmbeddedPayload(
            Hero,
            TEXT("mesh_render_ddc_and_lighting"),
            MeshPayloadDigest,
            Error) ||
        !ValidateHeroMesh(Hero, Geometry, SlotMaterials, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_NAMESPACE: ") + Error;
        return false;
    }
    AssetsToSave.Add(Hero);

    if (!AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_V2_NAMESPACE: v2 assets validated in memory but did not all save. No v1 package was touched; inspect/delete only the isolated v2 namespaces before retrying.");
        return false;
    }
    FString FinalReport;
    if (InspectHeroV2Assets(
            AssetSubsystem,
            Geometry,
            Pack,
            nullptr,
            FinalReport) != EHeroV2AssetState::CompleteValid)
    {
        OutMessage = TEXT("POST_SAVE_V2_VALIDATION_FAILED: ") + FinalReport;
        return false;
    }
    OutMessage = SourceReport + TEXT(" ") + FinalReport +
        TEXT(" Imported only /Game/TRIAD/IstanaPublicViewV2 and /Game/TRIAD/IstanaPublicView/HeroMaterialsV2; v1 meshes, materials, collision, maps, surroundings, renderer settings, SM6, and project settings were not loaded for mutation or saved.");
    return true;
}

struct FPreservedComponentState
{
    FString Name;
    FString MeshPath;
    FTransform RelativeTransform;
    ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
    FCollisionResponseContainer CollisionResponses;
    bool bVisible = false;
    bool bHiddenInGame = false;
    bool bActive = false;
    bool bAutoActivate = false;
    bool bGenerateOverlapEvents = false;
    bool bVisibleInEditor = false;
    bool bTemporarilyHiddenInEditor = false;
    bool bCastShadow = false;
    bool bRenderCustomDepth = false;
    bool bVisibleInSceneCaptureOnly = false;
    bool bHiddenInSceneCapture = false;
    int32 CustomDepthStencilValue = 0;
    EComponentMobility::Type Mobility = EComponentMobility::Static;
    TArray<FString> EffectiveMaterialPaths;
    TArray<FString> OverrideMaterialPaths;
    FString OverlayMaterialPath;
    TArray<FTransform> InstanceTransforms;
    int32 InstanceStartCullDistance = 0;
    int32 InstanceEndCullDistance = 0;
    int32 NumCustomDataFloats = 0;
    int32 InstancingRandomSeed = 0;
    float InstanceLodDistanceScale = 1.0f;
    TArray<float> PerInstanceCustomData;
};

struct FPublicViewSurroundingsSnapshot
{
    FTransform SceneActorTransform;
    TArray<FName> SceneActorTags;
    TArray<FPreservedComponentState> Components;
    struct FCameraState
    {
        FString Name;
        FString ClassPath;
        FTransform ActorTransform;
        FTransform ComponentRelativeTransform;
        float FieldOfView = 0.0f;
        float AspectRatio = 0.0f;
        float OrthoWidth = 0.0f;
        ECameraProjectionMode::Type ProjectionMode = ECameraProjectionMode::Perspective;
        float PostProcessBlendWeight = 0.0f;
        bool bConstrainAspectRatio = false;
        bool bActive = false;
        bool bAutoActivate = false;
        bool bHiddenInGame = false;
        bool bHiddenInEditor = false;
        FString EditableComponentStateSha256;
        TArray<FName> Tags;
    };
    TArray<FCameraState> Cameras;
};

bool CaptureEditableCameraComponentDigest(
    const UCameraComponent* Camera,
    FString& OutDigest,
    FString& OutError)
{
    if (!Camera)
    {
        OutError = TEXT("Cannot snapshot a null camera component.");
        return false;
    }
    TArray<const FProperty*> Properties;
    for (TFieldIterator<FProperty> It(
             Camera->GetClass(), EFieldIteratorFlags::IncludeSuper);
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
    Properties.Sort(
        [](const FProperty& A, const FProperty& B)
        {
            return A.GetFullName() < B.GetFullName();
        });
    FString Canonical;
    AppendGraphToken(Canonical, Camera->GetClass()->GetPathName());
    for (const FProperty* Property : Properties)
    {
        AppendGraphToken(Canonical, Property->GetFullName());
        AppendGraphToken(Canonical, FString::FromInt(Property->ArrayDim));
        for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
        {
            const void* ValueAddress =
                Property->ContainerPtrToValuePtr<void>(Camera, Index);
            int32 DynamicArrayNum = INDEX_NONE;
            bool bHardObjectReferenceCollection = false;
            if (const FArrayProperty* ArrayProperty =
                    CastField<FArrayProperty>(Property))
            {
                FScriptArrayHelper ArrayHelper(
                    ArrayProperty, const_cast<void*>(ValueAddress));
                DynamicArrayNum = ArrayHelper.Num();
                bHardObjectReferenceCollection =
                    CastField<FObjectPropertyBase>(ArrayProperty->Inner) !=
                    nullptr;
                AppendGraphToken(
                    Canonical,
                    bHardObjectReferenceCollection
                        ? TEXT("dynamic-array:hard-object-references")
                        : TEXT("dynamic-array:values"));
                AppendGraphToken(
                    Canonical,
                    FString::Printf(
                        TEXT("dynamic-array-count:%d"), DynamicArrayNum));
            }
            // A reference export seals the reference identity, not the editable
            // fields inside an instanced object such as UAssetUserData.  Refuse
            // non-empty editable hard-reference collections rather than claim a
            // deep camera-state invariant that this digest cannot prove.
            if (bHardObjectReferenceCollection && DynamicArrayNum > 0)
            {
                OutError = FString::Printf(
                    TEXT("Camera property '%s' contains %d editable hard-object reference(s); deep referenced-object state is not supported by the exact migration invariant."),
                    *Property->GetFullName(),
                    DynamicArrayNum);
                return false;
            }
            FString Value;
            // ExportText_InContainer's bool means "different from Delta", not
            // "serialization succeeded".  With a null Delta, valid zero/default
            // values (notably an empty ActorComponent::AssetUserData array) return
            // false.  Export every selected property unconditionally so an empty
            // value contributes its own length-delimited token and any later edit
            // still changes the digest.
            Property->ExportTextItem_Direct(
                Value,
                ValueAddress,
                nullptr,
                const_cast<UCameraComponent*>(Camera),
                PPF_Delimited,
                nullptr);
            if (DynamicArrayNum > 0 && Value.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("Could not canonicalize non-empty camera array property '%s'."),
                    *Property->GetFullName());
                return false;
            }
            Value.ReplaceInline(*SourceMapPackage, TEXT("<ISTANA_MAP>"));
            Value.ReplaceInline(*DestinationMapPackage, TEXT("<ISTANA_MAP>"));
            AppendGraphToken(Canonical, Value);
        }
    }
    FString PostProcessText;
    FPostProcessSettings::StaticStruct()->ExportText(
        PostProcessText,
        &Camera->PostProcessSettings,
        nullptr,
        const_cast<UCameraComponent*>(Camera),
        PPF_Delimited,
        nullptr);
    PostProcessText.ReplaceInline(*SourceMapPackage, TEXT("<ISTANA_MAP>"));
    PostProcessText.ReplaceInline(
        *DestinationMapPackage, TEXT("<ISTANA_MAP>"));
    AppendGraphToken(Canonical, TEXT("FPostProcessSettings"));
    AppendGraphToken(Canonical, PostProcessText);

    FTCHARToUTF8 Utf8(*Canonical);
    if (!CalculateSha256Bytes(Utf8.Get(), Utf8.Length(), OutDigest))
    {
        OutError = TEXT("Could not hash the complete editable camera state.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool CapturePreservedComponent(
    const UStaticMeshComponent* Component,
    FPreservedComponentState& OutState,
    FString& OutError)
{
    if (!Component || !Component->GetStaticMesh())
    {
        OutError = TEXT("A required v1 collision/surroundings component is absent.");
        return false;
    }
    OutState.Name = Component->GetName();
    OutState.MeshPath = Component->GetStaticMesh()->GetPathName();
    OutState.RelativeTransform = Component->GetRelativeTransform();
    OutState.CollisionEnabled = Component->GetCollisionEnabled();
    OutState.CollisionResponses = Component->GetCollisionResponseToChannels();
    OutState.bVisible = Component->IsVisible();
    OutState.bHiddenInGame = Component->bHiddenInGame;
    OutState.bActive = Component->IsActive();
    OutState.bAutoActivate = Component->bAutoActivate;
    OutState.bGenerateOverlapEvents = Component->GetGenerateOverlapEvents();
    OutState.bVisibleInEditor = Component->IsVisibleInEditor();
    OutState.bTemporarilyHiddenInEditor =
        Component->IsTemporarilyHiddenInEditor(false);
    OutState.bCastShadow = Component->CastShadow;
    OutState.bRenderCustomDepth = Component->bRenderCustomDepth;
    OutState.bVisibleInSceneCaptureOnly =
        Component->bVisibleInSceneCaptureOnly;
    OutState.bHiddenInSceneCapture = Component->bHiddenInSceneCapture;
    OutState.CustomDepthStencilValue = Component->CustomDepthStencilValue;
    OutState.Mobility = Component->Mobility;
    OutState.EffectiveMaterialPaths.Reset();
    for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
    {
        const UMaterialInterface* Material = Component->GetMaterial(Index);
        OutState.EffectiveMaterialPaths.Add(
            Material ? Material->GetPathName() : TEXT("<null>"));
    }
    OutState.OverrideMaterialPaths.Reset();
    for (const TObjectPtr<UMaterialInterface>& Material :
         Component->OverrideMaterials)
    {
        OutState.OverrideMaterialPaths.Add(
            Material ? Material->GetPathName() : TEXT("<null>"));
    }
    const UMaterialInterface* Overlay = Component->GetOverlayMaterial();
    OutState.OverlayMaterialPath = Overlay
        ? Overlay->GetPathName()
        : FString();
    if (const UHierarchicalInstancedStaticMeshComponent* Instances =
            Cast<UHierarchicalInstancedStaticMeshComponent>(Component))
    {
        Instances->GetCullDistances(
            OutState.InstanceStartCullDistance,
            OutState.InstanceEndCullDistance);
        OutState.NumCustomDataFloats = Instances->NumCustomDataFloats;
        OutState.InstancingRandomSeed = Instances->InstancingRandomSeed;
        OutState.InstanceLodDistanceScale = Instances->InstanceLODDistanceScale;
        OutState.PerInstanceCustomData = Instances->PerInstanceSMCustomData;
        OutState.InstanceTransforms.Reserve(Instances->GetInstanceCount());
        for (int32 Index = 0; Index < Instances->GetInstanceCount(); ++Index)
        {
            FTransform Transform;
            if (!Instances->GetInstanceTransform(Index, Transform, false))
            {
                OutError = FString::Printf(
                    TEXT("Could not read preserved instance %d from '%s'."),
                    Index,
                    *OutState.Name);
                return false;
            }
            OutState.InstanceTransforms.Add(Transform);
        }
    }
    return true;
}

bool CaptureSurroundings(
    const ATRIADIstanaPublicViewSceneActor* Scene,
    FPublicViewSurroundingsSnapshot& OutSnapshot,
    FString& OutError)
{
    if (!Scene)
    {
        OutError = TEXT("Public-view scene actor is absent.");
        return false;
    }
    OutSnapshot.SceneActorTransform = Scene->GetActorTransform();
    OutSnapshot.SceneActorTags = Scene->Tags;
    OutSnapshot.SceneActorTags.Sort(FNameLexicalLess());
    const UStaticMeshComponent* Components[] = {
        Scene->BuildingCollisionComponent.Get(),
        Scene->TerrainComponent.Get(),
        Scene->TerrainSkirtComponent.Get(),
        Scene->HardscapeComponent.Get(),
        Scene->ContextBuildingsComponent.Get(),
        Scene->OSMContextBuildingsComponent.Get(),
        Scene->RainTreeInstances.Get(),
        Scene->PalmTreeInstances.Get(),
        Scene->FramingTreeInstances.Get()};
    OutSnapshot.Components.Reset();
    for (const UStaticMeshComponent* Component : Components)
    {
        FPreservedComponentState State;
        if (!CapturePreservedComponent(Component, State, OutError))
        {
            return false;
        }
        OutSnapshot.Components.Add(MoveTemp(State));
    }
    OutSnapshot.Cameras.Reset();
    for (TActorIterator<ACameraActor> It(Scene->GetWorld()); It; ++It)
    {
        const ACameraActor* CameraActor = *It;
        const UCameraComponent* Camera = CameraActor
            ? CameraActor->GetCameraComponent()
            : nullptr;
        if (!CameraActor || !Camera)
        {
            OutError = TEXT("A camera actor in the public-view map lacks its camera component.");
            return false;
        }
        FPublicViewSurroundingsSnapshot::FCameraState State;
        State.Name = CameraActor->GetName();
        State.ClassPath = CameraActor->GetClass()->GetPathName();
        State.ActorTransform = CameraActor->GetActorTransform();
        State.ComponentRelativeTransform = Camera->GetRelativeTransform();
        State.FieldOfView = Camera->FieldOfView;
        State.AspectRatio = Camera->AspectRatio;
        State.OrthoWidth = Camera->OrthoWidth;
        State.ProjectionMode = Camera->ProjectionMode;
        State.PostProcessBlendWeight = Camera->PostProcessBlendWeight;
        State.bConstrainAspectRatio = Camera->bConstrainAspectRatio;
        State.bActive = Camera->IsActive();
        State.bAutoActivate = Camera->bAutoActivate;
        State.bHiddenInGame = CameraActor->IsHidden();
        State.bHiddenInEditor = CameraActor->IsHiddenEd();
        if (!CaptureEditableCameraComponentDigest(
                Camera, State.EditableComponentStateSha256, OutError))
        {
            return false;
        }
        State.Tags = CameraActor->Tags;
        State.Tags.Sort(FNameLexicalLess());
        OutSnapshot.Cameras.Add(MoveTemp(State));
    }
    OutSnapshot.Cameras.Sort(
        [](const FPublicViewSurroundingsSnapshot::FCameraState& A,
           const FPublicViewSurroundingsSnapshot::FCameraState& B)
        {
            return A.Name < B.Name;
        });
    return true;
}

bool SameCameraState(
    const FPublicViewSurroundingsSnapshot::FCameraState& Expected,
    const FPublicViewSurroundingsSnapshot::FCameraState& Actual)
{
    return Expected.Name == Actual.Name &&
        Expected.ClassPath == Actual.ClassPath &&
        Expected.ActorTransform.Equals(Actual.ActorTransform, 0.000001f) &&
        Expected.ComponentRelativeTransform.Equals(
            Actual.ComponentRelativeTransform, 0.000001f) &&
        FMath::IsNearlyEqual(Expected.FieldOfView, Actual.FieldOfView, 0.000001f) &&
        FMath::IsNearlyEqual(Expected.AspectRatio, Actual.AspectRatio, 0.000001f) &&
        FMath::IsNearlyEqual(Expected.OrthoWidth, Actual.OrthoWidth, 0.000001f) &&
        Expected.ProjectionMode == Actual.ProjectionMode &&
        FMath::IsNearlyEqual(
            Expected.PostProcessBlendWeight,
            Actual.PostProcessBlendWeight,
            0.000001f) &&
        Expected.bConstrainAspectRatio == Actual.bConstrainAspectRatio &&
        Expected.bActive == Actual.bActive &&
        Expected.bAutoActivate == Actual.bAutoActivate &&
        Expected.bHiddenInGame == Actual.bHiddenInGame &&
        Expected.bHiddenInEditor == Actual.bHiddenInEditor &&
        Expected.EditableComponentStateSha256 ==
            Actual.EditableComponentStateSha256 &&
        Expected.Tags == Actual.Tags;
}

bool SamePreservedState(
    const FPreservedComponentState& Expected,
    const FPreservedComponentState& Actual)
{
    if (Expected.Name != Actual.Name ||
        Expected.MeshPath != Actual.MeshPath ||
        !Expected.RelativeTransform.Equals(Actual.RelativeTransform, 0.000001f) ||
        Expected.CollisionEnabled != Actual.CollisionEnabled ||
        Expected.CollisionResponses != Actual.CollisionResponses ||
        Expected.bVisible != Actual.bVisible ||
        Expected.bHiddenInGame != Actual.bHiddenInGame ||
        Expected.bActive != Actual.bActive ||
        Expected.bAutoActivate != Actual.bAutoActivate ||
        Expected.bGenerateOverlapEvents != Actual.bGenerateOverlapEvents ||
        Expected.bVisibleInEditor != Actual.bVisibleInEditor ||
        Expected.bTemporarilyHiddenInEditor !=
            Actual.bTemporarilyHiddenInEditor ||
        Expected.bCastShadow != Actual.bCastShadow ||
        Expected.bRenderCustomDepth != Actual.bRenderCustomDepth ||
        Expected.bVisibleInSceneCaptureOnly !=
            Actual.bVisibleInSceneCaptureOnly ||
        Expected.bHiddenInSceneCapture != Actual.bHiddenInSceneCapture ||
        Expected.CustomDepthStencilValue != Actual.CustomDepthStencilValue ||
        Expected.Mobility != Actual.Mobility ||
        Expected.EffectiveMaterialPaths != Actual.EffectiveMaterialPaths ||
        Expected.OverrideMaterialPaths != Actual.OverrideMaterialPaths ||
        Expected.OverlayMaterialPath != Actual.OverlayMaterialPath ||
        Expected.InstanceStartCullDistance != Actual.InstanceStartCullDistance ||
        Expected.InstanceEndCullDistance != Actual.InstanceEndCullDistance ||
        Expected.NumCustomDataFloats != Actual.NumCustomDataFloats ||
        Expected.InstancingRandomSeed != Actual.InstancingRandomSeed ||
        !FMath::IsNearlyEqual(
            Expected.InstanceLodDistanceScale,
            Actual.InstanceLodDistanceScale,
            0.000001f) ||
        Expected.PerInstanceCustomData != Actual.PerInstanceCustomData ||
        Expected.InstanceTransforms.Num() != Actual.InstanceTransforms.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < Expected.InstanceTransforms.Num(); ++Index)
    {
        if (!Expected.InstanceTransforms[Index].Equals(
                Actual.InstanceTransforms[Index], 0.000001f))
        {
            return false;
        }
    }
    return true;
}

bool ValidateSurroundingsUnchanged(
    const FPublicViewSurroundingsSnapshot& Expected,
    const FPublicViewSurroundingsSnapshot& Actual,
    FString& OutError)
{
    if (Expected.Components.Num() != 9 || Actual.Components.Num() != 9)
    {
        OutError = TEXT("Hero-v2 preservation requires exactly nine collision/surroundings component snapshots.");
        return false;
    }
    if (!Expected.SceneActorTransform.Equals(
            Actual.SceneActorTransform, 0.000001f) ||
        Expected.SceneActorTags != Actual.SceneActorTags)
    {
        OutError = TEXT("Hero-v2 migration changed the public-view scene actor transform or tags.");
        return false;
    }
    for (int32 Index = 0; Index < Expected.Components.Num(); ++Index)
    {
        if (!SamePreservedState(
                Expected.Components[Index], Actual.Components[Index]))
        {
            OutError = FString::Printf(
                TEXT("Hero-v2 migration changed preserved component '%s'."),
                *Expected.Components[Index].Name);
            return false;
        }
    }
    if (Expected.Cameras.Num() != Actual.Cameras.Num())
    {
        OutError = TEXT("Hero-v2 migration changed the camera actor count.");
        return false;
    }
    for (int32 Index = 0; Index < Expected.Cameras.Num(); ++Index)
    {
        if (!SameCameraState(Expected.Cameras[Index], Actual.Cameras[Index]))
        {
            OutError = FString::Printf(
                TEXT("Hero-v2 migration changed preserved camera '%s'."),
                *Expected.Cameras[Index].Name);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateKnownV1Surroundings(
    const FPublicViewSurroundingsSnapshot& Snapshot,
    FString& OutError)
{
    const TCHAR* ExpectedPaths[] = {
        TEXT("/Game/TRIAD/IstanaPublicView/Building/SM_IstanaPublicView_Building_Collision.SM_IstanaPublicView_Building_Collision"),
        TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Terrain.SM_IstanaPublicView_Terrain"),
        TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_TerrainSkirt.SM_IstanaPublicView_TerrainSkirt"),
        TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Hardscape.SM_IstanaPublicView_Hardscape"),
        TEXT("/Game/TRIAD/IstanaPublicView/Context/SM_IstanaPublicView_ContextBuildings.SM_IstanaPublicView_ContextBuildings"),
        TEXT("/Game/TRIAD/IstanaPublicView/Context/SM_IstanaPublicView_OSMContextBuildings.SM_IstanaPublicView_OSMContextBuildings"),
        TEXT("/Game/TRIAD/IstanaPublicView/Vegetation/SM_IstanaPublicView_Tree_Rain.SM_IstanaPublicView_Tree_Rain"),
        TEXT("/Game/TRIAD/IstanaPublicView/Vegetation/SM_IstanaPublicView_Tree_Palm.SM_IstanaPublicView_Tree_Palm"),
        TEXT("/Game/TRIAD/IstanaPublicView/Vegetation/SM_IstanaPublicView_Tree_Framing.SM_IstanaPublicView_Tree_Framing")};
    if (Snapshot.Components.Num() != UE_ARRAY_COUNT(ExpectedPaths))
    {
        OutError = TEXT("Known v1 surroundings snapshot is incomplete.");
        return false;
    }
    for (int32 Index = 0; Index < Snapshot.Components.Num(); ++Index)
    {
        if (Snapshot.Components[Index].MeshPath != ExpectedPaths[Index])
        {
            OutError = FString::Printf(
                TEXT("Preserved component '%s' no longer references exact v1 asset '%s'."),
                *Snapshot.Components[Index].Name,
                ExpectedPaths[Index]);
            return false;
        }
    }
    if (Snapshot.Components[0].MeshPath != V1CollisionObjectPath)
    {
        OutError = TEXT("Hero-v2 map does not reuse the exact v1 collision asset.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool WriteMigrationInvariantReport(
    const FString& SourceFilename,
    const FString& SourceShaBefore,
    const FString& SourceShaAfter,
    int64 SourceBytes,
    const FPublicViewSurroundingsSnapshot& Baseline,
    const FPublicViewSurroundingsSnapshot& Persisted,
    FString& OutReportPath,
    FString& OutError)
{
    if (SourceShaBefore != SourceShaAfter ||
        !ValidateSurroundingsUnchanged(Baseline, Persisted, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The v1 map SHA-256 changed during v2 migration.");
        }
        return false;
    }
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(
        TEXT("schema"),
        TEXT("triad.istana_public_view_hero_v2.component_invariants.v1"));
    Root->SetStringField(TEXT("sourceMapPackage"), SourceMapPackage);
    Root->SetStringField(TEXT("sourceMapFilename"), SourceFilename);
    Root->SetStringField(TEXT("sourceMapSha256Before"), SourceShaBefore);
    Root->SetStringField(TEXT("sourceMapSha256After"), SourceShaAfter);
    Root->SetNumberField(TEXT("sourceMapBytes"), static_cast<double>(SourceBytes));
    Root->SetStringField(TEXT("destinationMapPackage"), DestinationMapPackage);
    Root->SetStringField(TEXT("heroMesh"), HeroMeshObjectPath);
    Root->SetBoolField(TEXT("sourceMapByteIdentical"), true);
    Root->SetBoolField(TEXT("componentsExact"), true);
    Root->SetBoolField(TEXT("camerasExact"), true);
    Root->SetBoolField(TEXT("changesSurroundings"), false);
    Root->SetBoolField(TEXT("requiresNanite"), false);
    Root->SetBoolField(TEXT("requiresShaderModel6"), false);
    Root->SetStringField(
        TEXT("sceneActorTransform"),
        Persisted.SceneActorTransform.ToString());
    TArray<TSharedPtr<FJsonValue>> SceneTags;
    for (const FName& Tag : Persisted.SceneActorTags)
    {
        SceneTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    }
    Root->SetArrayField(TEXT("sceneActorTags"), SceneTags);

    TArray<TSharedPtr<FJsonValue>> ComponentRows;
    for (int32 Index = 0; Index < Persisted.Components.Num(); ++Index)
    {
        const FPreservedComponentState& State = Persisted.Components[Index];
        TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), State.Name);
        Row->SetStringField(TEXT("mesh"), State.MeshPath);
        Row->SetStringField(
            TEXT("relativeTransform"), State.RelativeTransform.ToString());
        Row->SetNumberField(
            TEXT("collisionEnabled"),
            static_cast<double>(State.CollisionEnabled));
        FString Responses;
        for (int32 Channel = 0;
             Channel <= ECollisionChannel::ECC_GameTraceChannel18;
             ++Channel)
        {
            Responses += FString::FromInt(static_cast<int32>(
                State.CollisionResponses.GetResponse(
                    static_cast<ECollisionChannel>(Channel))));
        }
        Row->SetStringField(TEXT("collisionResponses"), Responses);
        Row->SetBoolField(TEXT("visible"), State.bVisible);
        Row->SetBoolField(TEXT("hiddenInGame"), State.bHiddenInGame);
        Row->SetBoolField(TEXT("active"), State.bActive);
        Row->SetBoolField(TEXT("autoActivate"), State.bAutoActivate);
        Row->SetBoolField(
            TEXT("generateOverlapEvents"), State.bGenerateOverlapEvents);
        Row->SetBoolField(TEXT("visibleInEditor"), State.bVisibleInEditor);
        Row->SetBoolField(
            TEXT("temporarilyHiddenInEditor"),
            State.bTemporarilyHiddenInEditor);
        Row->SetBoolField(TEXT("castShadow"), State.bCastShadow);
        Row->SetBoolField(
            TEXT("renderCustomDepth"), State.bRenderCustomDepth);
        Row->SetBoolField(
            TEXT("visibleInSceneCaptureOnly"),
            State.bVisibleInSceneCaptureOnly);
        Row->SetBoolField(
            TEXT("hiddenInSceneCapture"), State.bHiddenInSceneCapture);
        Row->SetNumberField(
            TEXT("customDepthStencilValue"),
            State.CustomDepthStencilValue);
        Row->SetNumberField(
            TEXT("mobility"), static_cast<double>(State.Mobility));
        TArray<TSharedPtr<FJsonValue>> EffectiveMaterials;
        for (const FString& Path : State.EffectiveMaterialPaths)
        {
            EffectiveMaterials.Add(MakeShared<FJsonValueString>(Path));
        }
        Row->SetArrayField(TEXT("effectiveMaterials"), EffectiveMaterials);
        TArray<TSharedPtr<FJsonValue>> OverrideMaterials;
        for (const FString& Path : State.OverrideMaterialPaths)
        {
            OverrideMaterials.Add(MakeShared<FJsonValueString>(Path));
        }
        Row->SetArrayField(TEXT("overrideMaterials"), OverrideMaterials);
        Row->SetStringField(TEXT("overlayMaterial"), State.OverlayMaterialPath);
        Row->SetNumberField(
            TEXT("instanceCount"),
            static_cast<double>(State.InstanceTransforms.Num()));
        Row->SetNumberField(
            TEXT("instanceStartCullDistance"),
            State.InstanceStartCullDistance);
        Row->SetNumberField(
            TEXT("instanceEndCullDistance"),
            State.InstanceEndCullDistance);
        Row->SetNumberField(
            TEXT("numCustomDataFloats"),
            State.NumCustomDataFloats);
        Row->SetNumberField(
            TEXT("instancingRandomSeed"),
            State.InstancingRandomSeed);
        Row->SetNumberField(
            TEXT("instanceLodDistanceScale"),
            State.InstanceLodDistanceScale);
        TArray<TSharedPtr<FJsonValue>> CustomData;
        for (const float Value : State.PerInstanceCustomData)
        {
            CustomData.Add(MakeShared<FJsonValueNumber>(Value));
        }
        Row->SetArrayField(TEXT("perInstanceCustomData"), CustomData);
        TArray<TSharedPtr<FJsonValue>> Instances;
        for (const FTransform& Transform : State.InstanceTransforms)
        {
            Instances.Add(MakeShared<FJsonValueString>(Transform.ToString()));
        }
        Row->SetArrayField(TEXT("instanceTransforms"), Instances);
        ComponentRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("preservedComponents"), ComponentRows);

    TArray<TSharedPtr<FJsonValue>> CameraRows;
    for (const FPublicViewSurroundingsSnapshot::FCameraState& State :
         Persisted.Cameras)
    {
        TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), State.Name);
        Row->SetStringField(TEXT("class"), State.ClassPath);
        Row->SetStringField(TEXT("actorTransform"), State.ActorTransform.ToString());
        Row->SetStringField(
            TEXT("componentRelativeTransform"),
            State.ComponentRelativeTransform.ToString());
        Row->SetNumberField(TEXT("fieldOfView"), State.FieldOfView);
        Row->SetNumberField(TEXT("aspectRatio"), State.AspectRatio);
        Row->SetNumberField(TEXT("orthoWidth"), State.OrthoWidth);
        Row->SetNumberField(
            TEXT("projectionMode"), static_cast<double>(State.ProjectionMode));
        Row->SetNumberField(
            TEXT("postProcessBlendWeight"), State.PostProcessBlendWeight);
        Row->SetBoolField(
            TEXT("constrainAspectRatio"), State.bConstrainAspectRatio);
        Row->SetBoolField(TEXT("active"), State.bActive);
        Row->SetBoolField(TEXT("autoActivate"), State.bAutoActivate);
        Row->SetBoolField(TEXT("hiddenInGame"), State.bHiddenInGame);
        Row->SetBoolField(TEXT("hiddenInEditor"), State.bHiddenInEditor);
        Row->SetStringField(
            TEXT("editableComponentStateSha256"),
            State.EditableComponentStateSha256);
        TArray<TSharedPtr<FJsonValue>> Tags;
        for (const FName& Tag : State.Tags)
        {
            Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        }
        Row->SetArrayField(TEXT("tags"), Tags);
        CameraRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("preservedCameras"), CameraRows);

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = TEXT("Could not serialize the hero-v2 invariant report.");
        return false;
    }
    OutReportPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPublicViewV2/Istana_PublicView_Exterior_v2.invariants.json")));
    if (!IFileManager::Get().MakeDirectory(
            *FPaths::GetPath(OutReportPath), true) ||
        !FFileHelper::SaveStringToFile(
            Json,
            *OutReportPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = TEXT("Could not persist the hero-v2 invariant report under Saved/TRIAD.");
        return false;
    }
    OutError.Reset();
    return true;
}

ATRIADIstanaPublicViewSceneActor* FindOnlySceneActor(
    UWorld* World,
    FString& OutError)
{
    if (!World)
    {
        OutError = TEXT("World is absent.");
        return nullptr;
    }
    TArray<ATRIADIstanaPublicViewSceneActor*> Scenes;
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(World); It; ++It)
    {
        Scenes.Add(*It);
    }
    if (Scenes.Num() != 1 || !Scenes[0] ||
        !Scenes[0]->Tags.Contains(SceneActorTag))
    {
        OutError = FString::Printf(
            TEXT("Expected exactly one tagged public-view scene actor; found %d."),
            Scenes.Num());
        return nullptr;
    }
    OutError.Reset();
    return Scenes[0];
}

bool ValidateEffectiveHeroV2Materials(
    UStaticMeshComponent* HeroComponent,
    FString& OutError)
{
    if (!HeroComponent || !HeroComponent->GetStaticMesh() ||
        HeroComponent->HasOverrideMaterials() ||
        HeroComponent->GetNumOverrideMaterials() != 0 ||
        HeroComponent->GetOverlayMaterial() != nullptr)
    {
        OutError = TEXT("V2 hero component has missing mesh, material overrides, or an overlay material.");
        return false;
    }
    const TArray<FStaticMaterial>& StaticMaterials =
        HeroComponent->GetStaticMesh()->GetStaticMaterials();
    if (StaticMaterials.Num() != ExpectedHeroSlots().Num() ||
        HeroComponent->GetNumMaterials() != ExpectedHeroSlots().Num())
    {
        OutError = TEXT("V2 hero component does not expose exactly eleven effective materials.");
        return false;
    }
    for (int32 Index = 0; Index < StaticMaterials.Num(); ++Index)
    {
        const FString Slot = StaticMaterials[Index].MaterialSlotName.ToString();
        const FString ExpectedInstance = ExpectedSlotInstances().FindRef(Slot);
        UMaterialInterface* Effective = HeroComponent->GetMaterial(Index);
        if (ExpectedInstance.IsEmpty() || !Effective ||
            Effective->GetPathName() != MaterialObjectPath(ExpectedInstance))
        {
            OutError = FString::Printf(
                TEXT("V2 hero effective material differs at slot '%s'."),
                *Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateWorldForV2(
    UWorld* World,
    const FString& RequiredPackage,
    const FPublicViewSurroundingsSnapshot* ExpectedSurroundings,
    FString& OutReport)
{
    if (!ExpectedSurroundings)
    {
        OutReport = TEXT("HERO_V2_MAP_INVALID: exact v1 surroundings/camera baseline is required.");
        return false;
    }
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != RequiredPackage)
    {
        OutReport = FString::Printf(
            TEXT("Open exact map '%s'; current package is '%s'."),
            *RequiredPackage,
            World && World->GetOutermost()
                ? *World->GetOutermost()->GetName()
                : TEXT("<none>"));
        return false;
    }
    FString Error;
    ATRIADIstanaPublicViewSceneActor* Scene = FindOnlySceneActor(World, Error);
    if (!Scene || !Scene->BuildingHeroVisualComponent ||
        !Scene->BuildingHeroVisualComponent->GetStaticMesh() ||
        Scene->BuildingHeroVisualComponent->GetStaticMesh()->GetPathName() !=
            HeroMeshObjectPath ||
        Scene->BuildingHeroVisualComponent->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision)
    {
        OutReport = TEXT("HERO_V2_MAP_INVALID: exact non-colliding v2 hero visual is absent. ") + Error;
        return false;
    }
    if (!ValidateEffectiveHeroV2Materials(
            Scene->BuildingHeroVisualComponent, Error))
    {
        OutReport = TEXT("HERO_V2_MAP_INVALID: ") + Error;
        return false;
    }
    FPublicViewSurroundingsSnapshot Actual;
    if (!CaptureSurroundings(Scene, Actual, Error) ||
        !ValidateKnownV1Surroundings(Actual, Error) ||
        !ValidateSurroundingsUnchanged(*ExpectedSurroundings, Actual, Error))
    {
        OutReport = TEXT("HERO_V2_MAP_INVALID: ") + Error;
        return false;
    }
    FString SceneReport;
    if (!Scene->ValidatePublicViewScene(SceneReport))
    {
        OutReport = TEXT("HERO_V2_MAP_INVALID: ") + SceneReport;
        return false;
    }
    OutReport = TEXT("Validated v2 map hero-only boundary against the loaded exact v1 baseline: BuildingHeroVisualComponent uses the exact v2 regular static mesh and eleven effective V2 materials with no overrides; scene actor and all nine collision, terrain, hardscape, context, and vegetation components retain exact assets, transforms, effective/override materials, visibility, activation, mobility, shadow/custom-depth state, collision/overlap state, HISM cull settings, instance transforms, and per-instance custom data; every camera retains exact actor state plus a deterministic digest of its complete editable component state, including post-process settings. ") + SceneReport;
    return true;
}

bool ValidateSourceWorldForMigration(
    UWorld* World,
    ATRIADIstanaPublicViewSceneActor*& OutScene,
    FPublicViewSurroundingsSnapshot& OutSnapshot,
    FString& OutError)
{
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != SourceMapPackage)
    {
        OutError = FString::Printf(
            TEXT("Open exact source map '%s' before v2 migration."),
            *SourceMapPackage);
        return false;
    }
    OutScene = FindOnlySceneActor(World, OutError);
    const FString V1HeroPath =
        TEXT("/Game/TRIAD/IstanaPublicView/Building/SM_IstanaPublicView_Building_Hero.SM_IstanaPublicView_Building_Hero");
    if (!OutScene || !OutScene->BuildingHeroVisualComponent ||
        !OutScene->BuildingHeroVisualComponent->GetStaticMesh() ||
        OutScene->BuildingHeroVisualComponent->GetStaticMesh()->GetPathName() !=
            V1HeroPath ||
        OutScene->BuildingHeroVisualComponent->HasOverrideMaterials() ||
        OutScene->BuildingHeroVisualComponent->GetNumOverrideMaterials() != 0 ||
        OutScene->BuildingHeroVisualComponent->GetOverlayMaterial() != nullptr)
    {
        OutError = TEXT("Source map does not retain the exact v1 hero visual.");
        return false;
    }
    FString SceneReport;
    if (!OutScene->ValidatePublicViewScene(SceneReport) ||
        !CaptureSurroundings(OutScene, OutSnapshot, OutError) ||
        !ValidateKnownV1Surroundings(OutSnapshot, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = SceneReport;
        }
        return false;
    }
    return true;
}

bool MigrateMapToV2(
    UEditorAssetSubsystem* AssetSubsystem,
    UStaticMesh* Hero,
    FString& OutMessage)
{
    if (!AssetSubsystem || !Hero)
    {
        OutMessage = TEXT("V2 migration requires the editor asset subsystem and validated hero.");
        return false;
    }
    if (FPackageName::DoesPackageExist(DestinationMapPackage) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        DoesObjectOrPackageExist(AssetSubsystem, DestinationMapObjectPath))
    {
        OutMessage = FString::Printf(
            TEXT("Refusing to overwrite existing v2 destination map '%s'."),
            *DestinationMapPackage);
        return false;
    }
    UWorld* SourceWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    ATRIADIstanaPublicViewSceneActor* SourceScene = nullptr;
    FPublicViewSurroundingsSnapshot SourceSnapshot;
    FString Error;
    if (!ValidateSourceWorldForMigration(
            SourceWorld, SourceScene, SourceSnapshot, Error))
    {
        OutMessage = TEXT("V2_MIGRATION_REFUSED: ") + Error;
        return false;
    }
    FString SourceFilename;
    FString SourceShaBefore;
    int64 SourceBytesBefore = 0;
    if (!FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename) ||
        !CalculateSha256(
            SourceFilename,
            SourceShaBefore,
            SourceBytesBefore,
            Error))
    {
        OutMessage = TEXT("V2_MIGRATION_REFUSED: could not freeze the v1 map before duplication. ") + Error;
        return false;
    }

    UWorld* TargetWorld = Cast<UWorld>(AssetSubsystem->DuplicateLoadedAsset(
        SourceWorld,
        DestinationMapPackage));
    if (!TargetWorld || !TargetWorld->GetOutermost() ||
        TargetWorld->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_MAP: could not create the isolated in-memory v2 map duplicate.");
        return false;
    }
    ATRIADIstanaPublicViewSceneActor* TargetScene =
        FindOnlySceneActor(TargetWorld, Error);
    FPublicViewSurroundingsSnapshot BeforeSwap;
    if (!TargetScene || !CaptureSurroundings(TargetScene, BeforeSwap, Error) ||
        !ValidateSurroundingsUnchanged(SourceSnapshot, BeforeSwap, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_MAP: duplicate did not preserve v1 surroundings. ") + Error;
        return false;
    }

    // This is intentionally the sole scene mutation in the v2 migration.
    // Collision, terrain, hardscape, context, and vegetation are read-only.
    TargetScene->Modify();
    TargetScene->BuildingHeroVisualComponent->Modify();
    TargetScene->BuildingHeroVisualComponent->SetStaticMesh(Hero);
    TargetScene->BuildingHeroVisualComponent->EmptyOverrideMaterials();
    TargetScene->BuildingHeroVisualComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);
    TargetScene->MarkPackageDirty();

    FPublicViewSurroundingsSnapshot AfterSwap;
    FString PreSaveReport;
    if (!CaptureSurroundings(TargetScene, AfterSwap, Error) ||
        !ValidateSurroundingsUnchanged(BeforeSwap, AfterSwap, Error) ||
        !ValidateWorldForV2(
            TargetWorld,
            DestinationMapPackage,
            &SourceSnapshot,
            PreSaveReport))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V2_MAP: hero-only pre-save validation failed. ") +
            (!Error.IsEmpty() ? Error : PreSaveReport);
        return false;
    }
    if (!AssetSubsystem->SaveLoadedAsset(TargetWorld, false))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_V2_MAP: validated duplicate could not be saved. The v1 map was not overwritten; inspect only the new v2 package before retrying.");
        return false;
    }
    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename))
    {
        OutMessage = TEXT("V2 map save returned success but no destination package exists.");
        return false;
    }
    UWorld* Reloaded = UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    FString PersistedReport;
    if (!ValidateWorldForV2(
            Reloaded,
            DestinationMapPackage,
            &SourceSnapshot,
            PersistedReport))
    {
        OutMessage = TEXT("V2 destination saved but persistence validation failed: ") +
            PersistedReport;
        return false;
    }
    if (!FPackageName::DoesPackageExist(SourceMapPackage))
    {
        OutMessage = TEXT("CRITICAL: v1 source map disappeared during additive migration.");
        return false;
    }
    FString SourceShaAfter;
    int64 SourceBytesAfter = 0;
    if (!CalculateSha256(
            SourceFilename,
            SourceShaAfter,
            SourceBytesAfter,
            Error) ||
        SourceBytesBefore != SourceBytesAfter ||
        SourceShaBefore != SourceShaAfter)
    {
        OutMessage = TEXT("CRITICAL: v1 source map bytes changed during additive migration. ") + Error;
        return false;
    }
    ATRIADIstanaPublicViewSceneActor* ReloadedScene =
        FindOnlySceneActor(Reloaded, Error);
    FPublicViewSurroundingsSnapshot PersistedSnapshot;
    if (!ReloadedScene ||
        !CaptureSurroundings(ReloadedScene, PersistedSnapshot, Error) ||
        !ValidateSurroundingsUnchanged(
            SourceSnapshot, PersistedSnapshot, Error))
    {
        OutMessage = TEXT("V2 destination persistence snapshot failed: ") + Error;
        return false;
    }
    FString InvariantReportPath;
    if (!WriteMigrationInvariantReport(
            SourceFilename,
            SourceShaBefore,
            SourceShaAfter,
            SourceBytesBefore,
            SourceSnapshot,
            PersistedSnapshot,
            InvariantReportPath,
            Error))
    {
        OutMessage = TEXT("V2 destination saved but invariant report failed: ") + Error;
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Created non-overwriting %s from exact v1 and swapped only BuildingHeroVisualComponent to %s. %s The v1 .umap retained SHA-256 %s; collision, terrain/hardscape/context/trees, and cameras remained exact. Invariant report: %s. Nanite, SM6, renderer, project settings, and sensor truth were unchanged."),
        *DestinationMapPackage,
        *HeroMeshObjectPath,
        *PersistedReport,
        *SourceShaAfter,
        *InvariantReportPath);
    return true;
}
}

bool UTRIADIstanaPublicViewHeroV2EditorLibrary::
    ValidateIstanaPublicViewHeroV2RemoteControlProject(
        const FString& ExpectedProjectPath,
        FString& OutReport)
{
    if (ExpectedProjectPath.IsEmpty())
    {
        OutReport = TEXT("ExpectedProjectPath is required.");
        return false;
    }
    FString ExpectedDirectory =
        FPaths::ConvertRelativePathToFull(ExpectedProjectPath);
    FPaths::NormalizeDirectoryName(ExpectedDirectory);
    if (ExpectedDirectory.EndsWith(TEXT(".uproject"), ESearchCase::IgnoreCase))
    {
        ExpectedDirectory = FPaths::GetPath(ExpectedDirectory);
        FPaths::NormalizeDirectoryName(ExpectedDirectory);
    }
    FString ActualDirectory =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ActualDirectory);
    if (!FPaths::IsSamePath(ExpectedDirectory, ActualDirectory))
    {
        OutReport = FString::Printf(
            TEXT("Hero-v2 Remote Control project mismatch: connected '%s', expected '%s'. No operation ran."),
            *ActualDirectory,
            *ExpectedDirectory);
        return false;
    }
    OutReport = TEXT("Hero-v2 Remote Control project identity verified: ") +
        ActualDirectory;
    return true;
}

bool UTRIADIstanaPublicViewHeroV2EditorLibrary::
    ImportIstanaPublicViewHeroV2Assets(FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }
    FHeroGeometryContract Geometry;
    FHeroMaterialPack Materials;
    FString SourceReport;
    if (!ValidateCompleteV2SourceSet(
            Geometry, Materials, SourceReport))
    {
        OutMessage = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    return ImportHeroV2AssetSet(
        AssetSubsystem, Geometry, Materials, SourceReport, OutMessage);
}

bool UTRIADIstanaPublicViewHeroV2EditorLibrary::
    ValidateIstanaPublicViewHeroV2Assets(FString& OutReport)
{
    FHeroGeometryContract Geometry;
    FHeroMaterialPack Materials;
    FString SourceReport;
    if (!ValidateCompleteV2SourceSet(
            Geometry, Materials, SourceReport))
    {
        OutReport = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    FString AssetReport;
    if (InspectHeroV2Assets(
            AssetSubsystem,
            Geometry,
            Materials,
            nullptr,
            AssetReport) != EHeroV2AssetState::CompleteValid)
    {
        OutReport = AssetReport;
        return false;
    }
    OutReport = SourceReport + TEXT(" ") + AssetReport;
    return true;
}

bool UTRIADIstanaPublicViewHeroV2EditorLibrary::
    MigrateIstanaPublicViewExteriorMapToHeroV2(FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }
    FHeroGeometryContract Geometry;
    FHeroMaterialPack Materials;
    FString SourceReport;
    if (!ValidateCompleteV2SourceSet(
            Geometry, Materials, SourceReport))
    {
        OutMessage = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    UStaticMesh* Hero = nullptr;
    FString AssetReport;
    if (InspectHeroV2Assets(
            AssetSubsystem,
            Geometry,
            Materials,
            &Hero,
            AssetReport) != EHeroV2AssetState::CompleteValid)
    {
        OutMessage = TEXT("ASSETS_MISSING_OR_INVALID: ") + AssetReport;
        return false;
    }
    return MigrateMapToV2(AssetSubsystem, Hero, OutMessage);
}

bool UTRIADIstanaPublicViewHeroV2EditorLibrary::
    ValidateIstanaPublicViewHeroV2Map(FString& OutReport)
{
    FHeroGeometryContract Geometry;
    FHeroMaterialPack Materials;
    FString SourceReport;
    if (!ValidateCompleteV2SourceSet(Geometry, Materials, SourceReport))
    {
        OutReport = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    FString AssetReport;
    if (InspectHeroV2Assets(
            AssetSubsystem,
            Geometry,
            Materials,
            nullptr,
            AssetReport) != EHeroV2AssetState::CompleteValid)
    {
        OutReport = TEXT("HERO_V2_MAP_INVALID: source/assets are stale. ") + AssetReport;
        return false;
    }
    if (UPackage* LoadedSource = FindPackage(nullptr, *SourceMapPackage))
    {
        if (LoadedSource->IsDirty())
        {
            OutReport = TEXT("HERO_V2_MAP_INVALID: loaded v1 baseline package is dirty; save/revert it before exact comparison.");
            return false;
        }
    }
    UWorld* SourceWorld = LoadObject<UWorld>(nullptr, *SourceMapObjectPath);
    ATRIADIstanaPublicViewSceneActor* SourceScene = nullptr;
    FPublicViewSurroundingsSnapshot Baseline;
    FString BaselineError;
    if (!ValidateSourceWorldForMigration(
            SourceWorld, SourceScene, Baseline, BaselineError))
    {
        OutReport = TEXT("HERO_V2_MAP_INVALID: exact v1 baseline could not be loaded. ") +
            BaselineError;
        return false;
    }
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString MapReport;
    if (!ValidateWorldForV2(
        World,
        DestinationMapPackage,
        &Baseline,
        MapReport))
    {
        OutReport = MapReport;
        return false;
    }
    OutReport = SourceReport + TEXT(" ") + AssetReport + TEXT(" ") + MapReport;
    return true;
}
