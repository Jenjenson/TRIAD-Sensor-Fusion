#include "TRIADIstanaPublicViewHeroV5EditorLibrary.h"
#include "TRIADIstanaPublicViewHeroV4EditorLibrary.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
#include "Engine/GameViewportClient.h"
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
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "Kismet/GameplayStatics.h"
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
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/Archive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaPublicViewRuntimePolicyActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Slate/SceneViewport.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

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

const FString ProtectedV1MapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Exterior_v1"));
const FString ProtectedV2MapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Exterior_v2"));
const FString ProtectedV3MapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Exterior_v3"));
const FString SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Exterior_v4"));
const FString DestinationMapPackage(TEXT("/Game/Maps/Istana_PublicView_Exterior_v5"));
const FString SourceMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Exterior_v4.Istana_PublicView_Exterior_v4"));
const FString DestinationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Exterior_v5.Istana_PublicView_Exterior_v5"));
const FString HeroAssetRoot(TEXT("/Game/TRIAD/IstanaPublicViewV5"));
const FString HeroMeshPath(TEXT("/Game/TRIAD/IstanaPublicViewV5/Building"));
const FString HeroMeshName(TEXT("SM_IstanaPublicViewV5_Building_Hero"));
const FString HeroMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewV5/Building/SM_IstanaPublicViewV5_Building_Hero.SM_IstanaPublicViewV5_Building_Hero"));
const FString V4HeroMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewV4/Building/SM_IstanaPublicViewV4_Building_Hero.SM_IstanaPublicViewV4_Building_Hero"));
const FString MaterialAssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV5"));
const FString MaterialTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV3/Textures"));
const FString GeometrySourceRoot(TEXT("SourceAssets/IstanaPublicViewV5"));
const FString GeometryContractRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/istana_public_view_hero_v5.contract.json"));
const FString GeometryManifestRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/Generated/IstanaPublicViewV5Building.manifest.json"));
const FString GeometryObjRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/Generated/SM_IstanaPublicViewV5_Building_Hero.obj"));
const FString GeometryFreezeRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/hero_v5.freeze.json"));
const FString GeometryGeneratorRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/generate_hero_v5.py"));
const FString GeometryAuditRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/v5_geometry_audit.py"));
const FString GeometryWholeShellAuditRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/v5_whole_shell_audit.py"));
const FString GeometryValidatorRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/validate_hero_v5.py"));
const FString GeometryContractTestRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/tests/test_hero_v5_contract.py"));
const FString GeometryMutationTestRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV5/tests/test_v5_geometry_audit_mutations.py"));
const FString GeometryKernelRelativePath(
    TEXT("SourceAssets/IstanaPublicView/public_view_geometry.py"));
const FString GeometrySparseClosureAuditRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV3/facade_closure_audit.py"));
const FString GeometryDenseClosureAuditRelativePath(
    TEXT("SourceAssets/IstanaPublicViewV3/dense_facade_closure_audit.py"));
const FString FreezeRelativePath(
    TEXT("Plugins/TRIADSensorFusion/Resources/IstanaPublicViewHeroV5.integration.freeze.json"));
const FString MaterialSourceRoot(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5"));
const FString V3MaterialSourceRoot(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3"));
const FString MaterialContractRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/hero_materials_v5.contract.json"));
const FString MaterialInterfaceRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/unreal_asset_interface.v5.json"));
const FString MaterialFreezeRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/hero_materials_v5.freeze.json"));
const FString MaterialValidatorRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/validate_hero_materials_v5.py"));
const FString MaterialEvaluatorRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/evaluate_neutral_captures_v5.py"));
const FString MaterialReadmeRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/README.md"));
const FString MaterialContractTestRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/tests/test_hero_materials_v5.py"));
const FString MaterialSourceManifestRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/Source/source_manifest.json"));
const FString MaterialGeneratedManifestRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/Generated/manifest.json"));
const FString MaterialBuilderRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/build_hero_materials_v3.py"));
const FString V3MaterialContractRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/hero_materials_v3.contract.json"));
const FString V3MaterialInterfaceRelativePath(
    TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/unreal_asset_interface.v3.json"));
const FString VisualAcceptanceSettingsRelativePath(
    TEXT("Config/IstanaPublicViewHeroV3VisualAcceptance.settings.json"));
const FString V1CollisionObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Building/SM_IstanaPublicView_Building_Collision.SM_IstanaPublicView_Building_Collision"));
const FString PendingSha256(
    TEXT("0000000000000000000000000000000000000000000000000000000000000000"));
const FString FrozenGeometryClaimStatus(
    TEXT("PUBLIC_REFERENCE_VISUAL_RECONSTRUCTION_SELECTED_VIEW_NOT_SURVEY_CONTROLLED"));
const FString FrozenGeometrySourceClass(
    TEXT("ORIGINAL_PROCEDURAL_ADDITIVE_PUBLIC_ARCH_ALL_ELEVATION_UPPER_TOWER_AND_CENTRAL_ROOF_REMEDIATION_DERIVED_FROM_FROZEN_V4_PUBLIC_REFERENCE_BASELINE"));
const FString FrozenVisualAcceptanceSettingsSha256(
    TEXT("6120769876168fe972f11950b72550539ee1fa2ca3cd7e0fab2d0d1a2eabe64c"));
const FString FrozenFreezeSha256(
    TEXT("459f60e21da82422aeedd88325157ffc61d12f69846144f3f8f295f0b609ce75"));
const FString FrozenGeometryFreezeSha256(
    TEXT("653b5f6f73f9b8b6cd277ba1ba42d7863f4616b24d8e7300883e96bc21514928"));
const FString FrozenGeometryContractSha256(
    TEXT("842f4184137eca2a5e515ea0f339411735c10258e49d6673a8799d3b9e2ab9e0"));
const FString FrozenGeometryGeneratorSha256(
    TEXT("6526e7c22e1c2e9323bfe942a45bda39761de81fd902bf605c3f7327431ab676"));
const FString FrozenGeometryManifestSha256(
    TEXT("6a21cc263303b3b415fd6071e3a597a627ee2958e4e61ab81e4701308db64570"));
const FString FrozenGeometryManifestSemanticSha256(
    TEXT("2a7fe5fe9fc4847317d9fa05358bf9aecb0b0966bd6942805bda7372956dbf83"));
const FString FrozenGeometryObjSha256(
    TEXT("d9b37092401a70855d045bf28153293cd876ba748bc6e874096748c78d00eb4c"));
const FString FrozenMaterialSourceManifestSha256(
    TEXT("332f543036d0c9c677df46194ff93df7142a5179c79263639f0bf10600482d51"));
const FString FrozenMaterialContractSha256(
    TEXT("8ba78534eb7c91ba66815e5456d6b23b138d10961e84a4b35d81fcd3545bcc20"));
const FString FrozenMaterialInterfaceSha256(
    TEXT("f4f606d00d4e2fbf9033117c9e34c768d73287f3dce55ba234b2d905b3a7bb44"));
const FString FrozenMaterialFreezeSha256(
    TEXT("1a3ec7696e25eb7da83cd53e60eed67531f6e225c430f189eab51ca37d66d69a"));
const FString FrozenMaterialValidatorSha256(
    TEXT("db6590043aa192d420cdca80cce538cfe725b3a4be0c0b8d1cec70792587ee2d"));
const FString FrozenMaterialEvaluatorSha256(
    TEXT("c26e9eec7f8550185d5b70c8d3b07405f8fc66e9fcb62547a029b9be1cdc26ca"));
const FString FrozenMaterialReadmeSha256(
    TEXT("045ecdaaa02e5285037fb4e25f15a233c09bd9f865ca753319a0b0772a124816"));
const FString FrozenMaterialContractTestSha256(
    TEXT("a8cd9f8693a6a322f472460e6d754115399b914f14be4c3316ab7003e4ac5736"));
const FString FrozenMaterialBuilderSha256(
    TEXT("7c760144a6eccfde411204523fd2b641f6ecc0cd54f9c5d0fd400fbf4db14317"));
const FString FrozenMaterialGeneratedManifestSha256(
    TEXT("cff3a4a9c105af2bacd8bb8753d6fa5d62ff90c4dc51c899994308d01eca8a36"));
const FString FrozenV3MaterialContractSha256(
    TEXT("3e96192de4ca19d9e3bc1688472cba2b4d53fc9147379f06dba46adf9b6b4a4f"));
const FString FrozenV3MaterialInterfaceSha256(
    TEXT("932a0fc58f3106bf9c1b826ce99ed82940fc61a07c9aec400fbed7acc4d28f09"));
const FString FrozenGeometryAuditSha256(
    TEXT("02fef93ba2b49dc5ad78900adf3be73564a01e0db901366407d1e8313642247b"));
const FString FrozenGeometryWholeShellAuditSha256(
    TEXT("085d0726e79ba8a12d46378b2628ddf79606578c3e30c37e5ce8f785057bb721"));
const FString FrozenGeometryValidatorSha256(
    TEXT("92dff27ccc281dc3f64f28209f08c7a7693f728eb7937b2f2640989895be1b6d"));
const FString FrozenGeometryContractTestSha256(
    TEXT("cc563db01ff72c6e30ea10c5072a3c401c2b316d316a8996a5e3dc2fd8f108bb"));
const FString FrozenGeometryMutationTestSha256(
    TEXT("657af42e7930400e20e2fb9c14c008c6ca00aff23e86cfc229ef78de4e7811fc"));
const FString FrozenGeometryKernelSha256(
    TEXT("f46b05c5e5a53a3fcb4392735a9cab15fef9dc8f6e8e0d7fb30395b3f8b82023"));
const FString FrozenGeometrySparseClosureAuditSha256(
    TEXT("6bbd4dfb6f7245f97577a28318c204c99aa85fc4fa2f18f736ac66f7ba268839"));
const FString FrozenGeometryDenseClosureAuditSha256(
    TEXT("70d31d09d4532a81a7da70f95b7fac31abff1d5e315915846e52d0028cba58f0"));
const FName SceneActorTag(TEXT("TRIADIstanaPublicViewScene_v1"));
const FName RuntimePolicyTag(TEXT("TRIADIstanaPublicViewRuntimePolicy_v1"));
const FName PrimaryCameraTag(TEXT("TRIADIstanaPublicViewCamera_Primary"));
const FString IstanaAirSimGameModeClassPath(
    TEXT("/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode"));

// Exact independently reviewed and frozen V5 geometry, material, and
// integration authorities. The zero digest remains only as a fail-closed guard.
constexpr int32 ExpectedHeroTriangles = 831720;
constexpr int32 ExpectedHeroVertices = 2495160;
constexpr int32 ExpectedHeroGroups = 28505;
constexpr int64 ExpectedHeroBytes = 334592878;
constexpr int32 ExpectedMaterialTextureCount = 57;
constexpr int32 ExpectedPreservedTreeInstanceCount = 720;
constexpr double BoundsToleranceCentimeters = 0.1;

const TArray<FString>& ExpectedHeroSlots()
{
    static const TArray<FString> Slots = {
        TEXT("M_IPV5_DarkTimber"),
        TEXT("M_IPV5_Door"),
        TEXT("M_IPV5_Glass"),
        TEXT("M_IPV5_Metal"),
        TEXT("M_IPV5_Opaline"),
        TEXT("M_IPV5_Recess"),
        TEXT("M_IPV5_Render"),
        TEXT("M_IPV5_Shutter"),
        TEXT("M_IPV5_Slate"),
        TEXT("M_IPV5_Stone"),
        TEXT("M_IPV5_Trim")};
    return Slots;
}

const TMap<FString, FString>& ExpectedSlotInstances()
{
    static const TMap<FString, FString> Bindings = {
        {TEXT("M_IPV5_Render"), TEXT("MI_IPV_Hero_Render_V5")},
        {TEXT("M_IPV5_Trim"), TEXT("MI_IPV_Hero_Trim_V5")},
        {TEXT("M_IPV5_Slate"), TEXT("MI_IPV_Hero_Slate_V5")},
        {TEXT("M_IPV5_Shutter"), TEXT("MI_IPV_Hero_Louvre_V5")},
        {TEXT("M_IPV5_Stone"), TEXT("MI_IPV_Hero_Stone_V5")},
        {TEXT("M_IPV5_Door"), TEXT("MI_IPV_Hero_Door_V5")},
        {TEXT("M_IPV5_DarkTimber"), TEXT("MI_IPV_Hero_Timber_V5")},
        {TEXT("M_IPV5_Metal"), TEXT("MI_IPV_Hero_PaintedMetal_V5")},
        {TEXT("M_IPV5_Glass"), TEXT("MI_IPV_Hero_Glass_V5")},
        {TEXT("M_IPV5_Opaline"), TEXT("MI_IPV_Hero_Opaline_V5")},
        {TEXT("M_IPV5_Recess"), TEXT("MI_IPV_Hero_Recess_V5")}};
    return Bindings;
}

const TArray<FString>& ExpectedV3SourceSlots()
{
    static const TArray<FString> Slots = {
        TEXT("M_IPV3_DarkTimber"),
        TEXT("M_IPV3_Door"),
        TEXT("M_IPV3_Glass"),
        TEXT("M_IPV3_Metal"),
        TEXT("M_IPV3_Opaline"),
        TEXT("M_IPV3_Recess"),
        TEXT("M_IPV3_Render"),
        TEXT("M_IPV3_Shutter"),
        TEXT("M_IPV3_Slate"),
        TEXT("M_IPV3_Stone"),
        TEXT("M_IPV3_Trim")};
    return Slots;
}

const TMap<FString, FString>& ExpectedV3SourceSlotMaterials()
{
    static const TMap<FString, FString> Bindings = {
        {TEXT("M_IPV3_Render"), TEXT("MI_IPV_Hero_Render_V3")},
        {TEXT("M_IPV3_Trim"), TEXT("MI_IPV_Hero_Trim_V3")},
        {TEXT("M_IPV3_Slate"), TEXT("MI_IPV_Hero_Slate_V3")},
        {TEXT("M_IPV3_Shutter"), TEXT("MI_IPV_Hero_Louvre_V3")},
        {TEXT("M_IPV3_Stone"), TEXT("MI_IPV_Hero_Stone_V3")},
        {TEXT("M_IPV3_Door"), TEXT("MI_IPV_Hero_Timber_V3")},
        {TEXT("M_IPV3_DarkTimber"), TEXT("MI_IPV_Hero_Timber_V3")},
        {TEXT("M_IPV3_Metal"), TEXT("MI_IPV_Hero_PaintedMetal_V3")},
        {TEXT("M_IPV3_Glass"), TEXT("MI_IPV_Hero_Glass_V3")},
        {TEXT("M_IPV3_Opaline"), TEXT("MI_IPV_Hero_Opaline_V3")},
        {TEXT("M_IPV3_Recess"), TEXT("MI_IPV_Hero_Recess_V3")}};
    return Bindings;
}

FString V3MaterialObjectPath(const FString& AssetName)
{
    return FString::Printf(
        TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV3/%s.%s"),
        *AssetName,
        *AssetName);
}

const TArray<FString>& ExpectedV4SourceSlots()
{
    static const TArray<FString> Slots = {
        TEXT("M_IPV4_DarkTimber"),
        TEXT("M_IPV4_Door"),
        TEXT("M_IPV4_Glass"),
        TEXT("M_IPV4_Metal"),
        TEXT("M_IPV4_Opaline"),
        TEXT("M_IPV4_Recess"),
        TEXT("M_IPV4_Render"),
        TEXT("M_IPV4_Shutter"),
        TEXT("M_IPV4_Slate"),
        TEXT("M_IPV4_Stone"),
        TEXT("M_IPV4_Trim")};
    return Slots;
}

const TMap<FString, FString>& ExpectedV4SourceSlotMaterials()
{
    static const TMap<FString, FString> Bindings = {
        {TEXT("M_IPV4_Render"), TEXT("MI_IPV_Hero_Render_V4")},
        {TEXT("M_IPV4_Trim"), TEXT("MI_IPV_Hero_Trim_V4")},
        {TEXT("M_IPV4_Slate"), TEXT("MI_IPV_Hero_Slate_V4")},
        {TEXT("M_IPV4_Shutter"), TEXT("MI_IPV_Hero_Louvre_V4")},
        {TEXT("M_IPV4_Stone"), TEXT("MI_IPV_Hero_Stone_V4")},
        {TEXT("M_IPV4_Door"), TEXT("MI_IPV_Hero_Timber_V4")},
        {TEXT("M_IPV4_DarkTimber"), TEXT("MI_IPV_Hero_Timber_V4")},
        {TEXT("M_IPV4_Metal"), TEXT("MI_IPV_Hero_PaintedMetal_V4")},
        {TEXT("M_IPV4_Glass"), TEXT("MI_IPV_Hero_Glass_V4")},
        {TEXT("M_IPV4_Opaline"), TEXT("MI_IPV_Hero_Opaline_V4")},
        {TEXT("M_IPV4_Recess"), TEXT("MI_IPV_Hero_Recess_V4")}};
    return Bindings;
}

FString V4MaterialObjectPath(const FString& AssetName)
{
    return FString::Printf(
        TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV4/%s.%s"),
        *AssetName,
        *AssetName);
}

bool HasExactV3ToV5TextureBinding(
    const FString& TextureSurfaceIdV3,
    const TArray<FString>& V3Slots,
    const TArray<FString>& V5Slots)
{
    TSet<FString> ExpectedV5;
    for (const FString& V3Slot : V3Slots)
    {
        if (!ExpectedV3SourceSlots().Contains(V3Slot) ||
            !V3Slot.StartsWith(TEXT("M_IPV3_")))
        {
            return false;
        }
        // V5 deliberately gives Door its own instance and reuses the neutral
        // V3 Trim scan. Do not inherit the V3 Door->Timber texture alias.
        if (V3Slot != TEXT("M_IPV3_Door"))
        {
            ExpectedV5.Add(
                FString(TEXT("M_IPV5_")) + V3Slot.RightChop(7));
        }
    }
    if (TextureSurfaceIdV3 == TEXT("Trim"))
    {
        ExpectedV5.Add(TEXT("M_IPV5_Door"));
    }
    TSet<FString> ActualV5;
    for (const FString& V5Slot : V5Slots)
    {
        ActualV5.Add(V5Slot);
    }
    return ExpectedV5.Num() == V5Slots.Num() &&
        ActualV5.Num() == V5Slots.Num() &&
        ExpectedV5.Includes(ActualV5);
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

int32 CountCommandLineToken(
    const FString& CommandLine,
    const FString& Token)
{
    int32 Count = 0;
    int32 SearchFrom = 0;
    while (SearchFrom < CommandLine.Len())
    {
        const int32 Found = CommandLine.Find(
            Token,
            ESearchCase::IgnoreCase,
            ESearchDir::FromStart,
            SearchFrom);
        if (Found == INDEX_NONE)
        {
            break;
        }
        ++Count;
        SearchFrom = Found + Token.Len();
    }
    return Count;
}

bool ValidateHeroV5VisualAcceptanceProcessProfile(FString& OutReport)
{
    const FString CommandLine(FCommandLine::Get());
    FString SettingsArgument;
    if (!FParse::Param(
            FCommandLine::Get(),
            TEXT("TRIADIstanaVisualAcceptance")) ||
        CountCommandLineToken(
            CommandLine,
            TEXT("-TRIADIstanaVisualAcceptance")) != 1 ||
        CountCommandLineToken(CommandLine, TEXT("-settings=")) != 1 ||
        !FParse::Value(
            FCommandLine::Get(),
            TEXT("-settings="),
            SettingsArgument,
            false) ||
        SettingsArgument.IsEmpty())
    {
        OutReport = TEXT("Hero-V5 PIE requires exactly one -TRIADIstanaVisualAcceptance flag and exactly one canonical -settings argument.");
        return false;
    }

    SettingsArgument.TrimStartAndEndInline();
    SettingsArgument.TrimQuotesInline();
    FString ExpectedSettings = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectConfigDir(),
            TEXT("IstanaPublicViewHeroV3VisualAcceptance.settings.json")));
    FString ActualSettings = FPaths::ConvertRelativePathToFull(
        SettingsArgument);
    FPaths::NormalizeFilename(ExpectedSettings);
    FPaths::NormalizeFilename(ActualSettings);
    if (!FPaths::IsSamePath(ExpectedSettings, ActualSettings))
    {
        OutReport = FString::Printf(
            TEXT("Hero-V5 PIE settings path is '%s', expected exact project-owned '%s'."),
            *ActualSettings,
            *ExpectedSettings);
        return false;
    }

    FString Digest;
    int64 Bytes = 0;
    FString Error;
    if (!CalculateSha256(ExpectedSettings, Digest, Bytes, Error) ||
        Bytes != 1022 || Digest != FrozenVisualAcceptanceSettingsSha256)
    {
        OutReport = TEXT("Hero-V5 PIE requires the exact 1022-byte repository-owned shared V3/V5 no-lidar/RPC-off settings profile. ") + Error;
        return false;
    }

    FString SettingsText;
    TSharedPtr<FJsonObject> Settings;
    const TSharedPtr<FJsonObject>* Vehicles = nullptr;
    int64 ApiServerPort = 0;
    if (!FFileHelper::LoadFileToString(SettingsText, *ExpectedSettings) ||
        CountCommandLineToken(SettingsText, TEXT("\"RpcEnabled\"")) != 1 ||
        CountCommandLineToken(SettingsText, TEXT("\"EnableRpc\"")) != 1 ||
        !LoadJsonObject(ExpectedSettings, Settings, Error) ||
        !ReadExactString(
            Settings,
            TEXT("TRIADProfile"),
            TEXT("ISTANA_PUBLIC_VIEW_HERO_V3_VISUAL_ACCEPTANCE_ONLY"),
            Error) ||
        !ReadExactBool(
            Settings,
            TEXT("TRIADProductionSensorProfile"),
            false,
            Error) ||
        !ReadExactString(
            Settings,
            TEXT("SimMode"),
            TEXT("ComputerVision"),
            Error) ||
        !ReadExactBool(Settings, TEXT("RpcEnabled"), false, Error) ||
        !ReadExactBool(Settings, TEXT("EnableRpc"), false, Error) ||
        !ReadExactString(
            Settings,
            TEXT("LocalHostIp"),
            TEXT("127.0.0.1"),
            Error) ||
        !ReadInteger(Settings, TEXT("ApiServerPort"), ApiServerPort, Error) ||
        ApiServerPort != 41451 ||
        Settings->HasField(TEXT("DefaultSensors")) ||
        !Settings->TryGetObjectField(TEXT("Vehicles"), Vehicles) ||
        !Vehicles || !(*Vehicles).IsValid() ||
        (*Vehicles)->Values.Num() != 1)
    {
        OutReport = TEXT("Hero-V5 PIE no-lidar/RPC-off settings contract failed; both upstream RpcEnabled=false and deployed-AirSim-1.8.1 EnableRpc=false must occur exactly once, with loopback API address. ") + Error;
        return false;
    }

    TSharedPtr<FJsonObject> Vehicle;
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Vehicles)->Values)
    {
        Vehicle = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
    }
    const TSharedPtr<FJsonObject>* Sensors = nullptr;
    if (!Vehicle.IsValid() ||
        !ReadExactString(
            Vehicle,
            TEXT("VehicleType"),
            TEXT("ComputerVision"),
            Error) ||
        !ReadExactBool(Vehicle, TEXT("AutoCreate"), true, Error) ||
        !Vehicle->TryGetObjectField(TEXT("Sensors"), Sensors) ||
        !Sensors || !(*Sensors).IsValid() ||
        (*Sensors)->Values.Num() != 0)
    {
        OutReport = TEXT("Hero-V5 PIE requires one auto-created ComputerVision vehicle with an empty Sensors object. ") + Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Verified exact shared V3/V5 no-lidar/RPC-off process file and command line (%s): upstream RpcEnabled=false plus deployed-AirSim-1.8.1 EnableRpc=false. Direct AirSim RPC state is not introspected across the plugin module boundary."),
        *Digest);
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

bool ReadStringArray(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    TArray<FString>& OutValues,
    FString& OutError);

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
    TMap<FString, FString>& InOutRolePaths,
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
            InOutSeenPaths.Contains(RelativePath) ||
            InOutRolePaths.Contains(Role))
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
        InOutRolePaths.Add(Role, RelativePath);
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
    if (FrozenFreezeSha256 == PendingSha256)
    {
        OutReport = TEXT("V5_FREEZE_NOT_FROZEN: final closure-audited geometry and material artifacts have not both been bound.");
        return false;
    }
    const FString FreezePath = ProjectSourcePath(FreezeRelativePath);
    FString Error;
    FString Digest;
    int64 FreezeBytes = 0;
    if (!CalculateSha256(FreezePath, Digest, FreezeBytes, Error))
    {
        OutReport = TEXT("V5_FREEZE_MISSING: ") + Error;
        return false;
    }
    if (Digest != FrozenFreezeSha256)
    {
        OutReport = FString::Printf(
            TEXT("V5_FREEZE_INVALID: integration freeze SHA-256 is %s, expected %s."),
            *Digest,
            *FrozenFreezeSha256);
        return false;
    }
    TSharedPtr<FJsonObject> Freeze;
    int64 CreatedV5TexturePackageCount = -1;
    TArray<FString> QualityCapturePresets;
    if (!LoadJsonObject(FreezePath, Freeze, Error) ||
        !ReadExactString(
            Freeze,
            TEXT("schema"),
            TEXT("triad.istana_public_view_hero_v5.unreal_freeze.v1"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("heroAssetRoot"),
            TEXT("/Game/TRIAD/IstanaPublicViewV5"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("materialAssetRoot"),
            TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV5"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("sourceMap"),
            TEXT("/Game/Maps/Istana_PublicView_Exterior_v4"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("destinationMap"),
            TEXT("/Game/Maps/Istana_PublicView_Exterior_v5"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("geometryClaimStatus"),
            *FrozenGeometryClaimStatus,
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("materialClaimStatus"),
            TEXT("PUBLIC_REFERENCE_QUALITATIVE_LOOKDEV_NOT_SITE_MEASURED_OR_COLOR_CALIBRATED"),
            Error) ||
        !ReadExactString(
            Freeze,
            TEXT("sourceGeometryGeneration"),
            *FrozenGeometrySourceClass,
            Error) ||
        !ReadExactBool(
            Freeze, TEXT("oneToOneDigitalTwinClaimed"), false, Error) ||
        !ReadExactBool(Freeze, TEXT("surveyAccuracyClaimed"), false, Error) ||
        !ReadExactBool(Freeze, TEXT("photogrammetryClaimed"), false, Error) ||
        !ReadExactBool(Freeze, TEXT("naniteEnabled"), false, Error) ||
        !ReadExactBool(Freeze, TEXT("sm6Required"), false, Error) ||
        !ReadExactBool(Freeze, TEXT("reusesV1Collision"), true, Error) ||
        !ReadExactBool(
            Freeze,
            TEXT("reusesFrozenV3TextureAssetsInPlace"),
            true,
            Error) ||
        !ReadInteger(
            Freeze,
            TEXT("createdV5TexturePackageCount"),
            CreatedV5TexturePackageCount,
            Error) ||
        CreatedV5TexturePackageCount != 0 ||
        !ReadExactBool(
            Freeze,
            TEXT("preservesV1V2V3V4Bytes"),
            true,
            Error) ||
        !ReadExactBool(Freeze, TEXT("changesSurroundings"), false, Error) ||
        !ReadExactBool(Freeze, TEXT("changesWorldLighting"), false, Error) ||
        !ReadExactBool(
            Freeze,
            TEXT("changesRendererOrProjectSettings"),
            false,
            Error) ||
        !ReadStringArray(
            Freeze,
            TEXT("qualityCapturePresets"),
            QualityCapturePresets,
            Error) ||
        QualityCapturePresets != TArray<FString>({
            TEXT("HERO_FRONT"),
            TEXT("HERO_OBLIQUE_RIGHT"),
            TEXT("HERO_OBLIQUE_LEFT"),
            TEXT("HERO_FACADE_MACRO"),
            TEXT("HERO_GROUND_DETAIL"),
            TEXT("HERO_MATERIAL_DETAIL"),
            TEXT("HERO_ORBIT_RIGHT"),
            TEXT("HERO_ORBIT_LEFT")}))
    {
        OutReport = TEXT("V5_FREEZE_INVALID: ") + Error;
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* ProtectedV1 = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* RepositoryScripts = nullptr;
    if (!Freeze->TryGetArrayField(TEXT("files"), Files) ||
        !Freeze->TryGetArrayField(TEXT("protectedV1Files"), ProtectedV1) ||
        !Freeze->TryGetArrayField(
            TEXT("repositoryScripts"), RepositoryScripts) ||
        !RepositoryScripts || RepositoryScripts->Num() != 6)
    {
        OutReport = TEXT("V5_FREEZE_INVALID: files/protectedV1Files or the six source-only repository scripts are absent.");
        return false;
    }
    TSet<FString> SeenPaths;
    TMap<FString, FString> RolePaths;
    if (!ValidateFreezeRecordArray(
            Files, TEXT("files"), SeenPaths, RolePaths, Error) ||
        !ValidateFreezeRecordArray(
            ProtectedV1,
            TEXT("protectedV1Files"),
            SeenPaths,
            RolePaths,
            Error))
    {
        OutReport = TEXT("V5_FREEZE_INVALID: ") + Error;
        return false;
    }
    TMap<FString, int64> RepositoryScriptBytes;
    TMap<FString, FString> RepositoryScriptDigests;
    for (const TSharedPtr<FJsonValue>& Value : *RepositoryScripts)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FString RelativePath;
        FString Role;
        FString Sha256;
        int64 Bytes = 0;
        if (!Row.IsValid() || Row->Values.Num() != 4 ||
            !Row->TryGetStringField(TEXT("relativePath"), RelativePath) ||
            !Row->TryGetStringField(TEXT("role"), Role) ||
            !Row->TryGetStringField(TEXT("sha256"), Sha256) ||
            !ReadInteger(Row, TEXT("bytes"), Bytes, Error) ||
            !IsSafeRelativeSourcePath(RelativePath) ||
            !RelativePath.StartsWith(TEXT("scripts/")) ||
            Role.IsEmpty() || Bytes <= 0 || Sha256.Len() != 64 ||
            SeenPaths.Contains(RelativePath) || RolePaths.Contains(Role))
        {
            OutReport = TEXT("V5_FREEZE_INVALID: repositoryScripts contains an unsafe, malformed, or duplicate record. ") + Error;
            return false;
        }
        SeenPaths.Add(RelativePath);
        RolePaths.Add(Role, RelativePath);
        RepositoryScriptBytes.Add(Role, Bytes);
        RepositoryScriptDigests.Add(Role, Sha256);
    }
    struct FExpectedRepositoryScript
    {
        const TCHAR* Role;
        const TCHAR* RelativePath;
        int64 Bytes;
        const TCHAR* Sha256;
    };
    static const FExpectedRepositoryScript ExpectedRepositoryScripts[] = {
        {TEXT("V5_QUALITY_CAPTURE_SCRIPT"), TEXT("scripts/Capture-IstanaPublicViewHeroV5QualityPreviews.ps1"), 11893, TEXT("86f717ad8f76694b0a9a6b8588f68f665f372321476f6df4220e2cd58d879ef7")},
        {TEXT("V5_ASSET_IMPORT_SCRIPT"), TEXT("scripts/Import-IstanaPublicViewHeroV5Assets.ps1"), 12718, TEXT("7f10ddd7de06d121011e5c1a6bdecf3a9510d0250010fc722288ea6e5c37731a")},
        {TEXT("V5_DEVELOPMENT_ASSET_INSTALL_SCRIPT"), TEXT("scripts/Install-IstanaPublicViewHeroV5DevelopmentAssets.ps1"), 47209, TEXT("cbce9ed299061faf751ac8687ea03fd56dda0942520549dbc1e8fba8e9a34a4f")},
        {TEXT("V5_MAP_MIGRATION_SCRIPT"), TEXT("scripts/Migrate-IstanaPublicViewHeroV5Map.ps1"), 17254, TEXT("8222f361e4e134392bc71b4eb683d8886c1558aecdab680378d9799349d194f6")},
        {TEXT("V5_PIE_SETUP_SCRIPT"), TEXT("scripts/Set-IstanaPublicViewHeroV5PlayInEditor.ps1"), 14119, TEXT("d3a21c001f4097e3367e2b7b46efa8d72826a36e6916eb0ae55ae43b2825ab8b")},
        {TEXT("V5_EDITOR_START_SCRIPT"), TEXT("scripts/Start-IstanaPublicViewHeroV5Editor.ps1"), 13664, TEXT("cd48e04fd67bfacaaad623f227bf4a7b80f03dcdfc89cad37e98f76262da8851")}};
    for (const FExpectedRepositoryScript& Expected : ExpectedRepositoryScripts)
    {
        const int64* ActualBytes = RepositoryScriptBytes.Find(Expected.Role);
        const FString* ActualDigest = RepositoryScriptDigests.Find(Expected.Role);
        if (!ActualBytes || *ActualBytes != Expected.Bytes ||
            !ActualDigest || *ActualDigest != Expected.Sha256)
        {
            OutReport = FString::Printf(
                TEXT("V5_FREEZE_INVALID: source-only repository script role '%s' has stale byte/hash metadata."),
                Expected.Role);
            return false;
        }
    }
    const TMap<FString, FString> ExpectedRolePaths = {
        {TEXT("GEOMETRY_FREEZE"), TEXT("SourceAssets/IstanaPublicViewV5/hero_v5.freeze.json")},
        {TEXT("GEOMETRY_CONTRACT"), TEXT("SourceAssets/IstanaPublicViewV5/istana_public_view_hero_v5.contract.json")},
        {TEXT("GEOMETRY_GENERATOR"), TEXT("SourceAssets/IstanaPublicViewV5/generate_hero_v5.py")},
        {TEXT("GEOMETRY_AUDIT"), TEXT("SourceAssets/IstanaPublicViewV5/v5_geometry_audit.py")},
        {TEXT("GEOMETRY_WHOLE_SHELL_AUDIT"), TEXT("SourceAssets/IstanaPublicViewV5/v5_whole_shell_audit.py")},
        {TEXT("GEOMETRY_VALIDATOR"), TEXT("SourceAssets/IstanaPublicViewV5/validate_hero_v5.py")},
        {TEXT("GEOMETRY_README"), TEXT("SourceAssets/IstanaPublicViewV5/README.md")},
        {TEXT("GEOMETRY_CONTRACT_TEST"), TEXT("SourceAssets/IstanaPublicViewV5/tests/test_hero_v5_contract.py")},
        {TEXT("GEOMETRY_MUTATION_TEST"), TEXT("SourceAssets/IstanaPublicViewV5/tests/test_v5_geometry_audit_mutations.py")},
        {TEXT("PROTECTED_V3_SPARSE_CLOSURE_AUDIT"), TEXT("SourceAssets/IstanaPublicViewV3/facade_closure_audit.py")},
        {TEXT("PROTECTED_V3_DENSE_CLOSURE_AUDIT"), TEXT("SourceAssets/IstanaPublicViewV3/dense_facade_closure_audit.py")},
        {TEXT("GEOMETRY_MANIFEST"), TEXT("SourceAssets/IstanaPublicViewV5/Generated/IstanaPublicViewV5Building.manifest.json")},
        {TEXT("BUILDING_HERO_VISUAL_V5"), TEXT("SourceAssets/IstanaPublicViewV5/Generated/SM_IstanaPublicViewV5_Building_Hero.obj")},
        {TEXT("HERO_MATERIAL_FREEZE"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/hero_materials_v5.freeze.json")},
        {TEXT("HERO_MATERIAL_CONTRACT"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/hero_materials_v5.contract.json")},
        {TEXT("HERO_MATERIAL_UNREAL_INTERFACE"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/unreal_asset_interface.v5.json")},
        {TEXT("HERO_MATERIAL_VALIDATOR"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/validate_hero_materials_v5.py")},
        {TEXT("HERO_MATERIAL_EVALUATOR"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/evaluate_neutral_captures_v5.py")},
        {TEXT("HERO_MATERIAL_README"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/README.md")},
        {TEXT("HERO_MATERIAL_CONTRACT_TEST"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV5/tests/test_hero_materials_v5.py")},
        {TEXT("HERO_MATERIAL_SOURCE_MANIFEST_V3"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/Source/source_manifest.json")},
        {TEXT("HERO_MATERIAL_GENERATED_MANIFEST_V3"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/Generated/manifest.json")},
        {TEXT("HERO_MATERIAL_BUILDER_V3"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/build_hero_materials_v3.py")},
        {TEXT("PROTECTED_V3_MATERIAL_CONTRACT"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/hero_materials_v3.contract.json")},
        {TEXT("PROTECTED_V3_MATERIAL_UNREAL_INTERFACE"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV3/unreal_asset_interface.v3.json")},
        {TEXT("HERO_V3_V5_VISUAL_ACCEPTANCE_SETTINGS"), VisualAcceptanceSettingsRelativePath},
        {TEXT("PUBLIC_REFERENCE_MANIFEST"), TEXT("SourceAssets/IstanaDigitalTwin/IstanaPublicView/manifest.json")},
        {TEXT("PROTECTED_V4_GEOMETRY_FREEZE"), TEXT("SourceAssets/IstanaPublicViewV4/hero_v4.freeze.json")},
        {TEXT("PROTECTED_V4_MATERIAL_FREEZE"), TEXT("SourceAssets/IstanaPublicView/HeroMaterialsV4/hero_materials_v4.freeze.json")},
        {TEXT("PROTECTED_V4_INTEGRATION_FREEZE"), TEXT("Plugins/TRIADSensorFusion/Resources/IstanaPublicViewHeroV4.integration.freeze.json")},
        {TEXT("PROTECTED_V3_GEOMETRY_FREEZE"), TEXT("SourceAssets/IstanaPublicViewV3/hero_v3.freeze.json")},
        {TEXT("PROTECTED_V3_INTEGRATION_FREEZE"), TEXT("Plugins/TRIADSensorFusion/Resources/IstanaPublicViewHeroV3.integration.freeze.json")},
        {TEXT("PROTECTED_V2_GEOMETRY_FREEZE"), TEXT("SourceAssets/IstanaPublicViewV2/hero_v2.freeze.json")},
        {TEXT("PROTECTED_V2_INTEGRATION_FREEZE"), TEXT("Plugins/TRIADSensorFusion/Resources/IstanaPublicViewHeroV2.integration.freeze.json")},
        {TEXT("V5_QUALITY_CAPTURE_SCRIPT"), TEXT("scripts/Capture-IstanaPublicViewHeroV5QualityPreviews.ps1")},
        {TEXT("V5_ASSET_IMPORT_SCRIPT"), TEXT("scripts/Import-IstanaPublicViewHeroV5Assets.ps1")},
        {TEXT("V5_DEVELOPMENT_ASSET_INSTALL_SCRIPT"), TEXT("scripts/Install-IstanaPublicViewHeroV5DevelopmentAssets.ps1")},
        {TEXT("V5_MAP_MIGRATION_SCRIPT"), TEXT("scripts/Migrate-IstanaPublicViewHeroV5Map.ps1")},
        {TEXT("V5_PIE_SETUP_SCRIPT"), TEXT("scripts/Set-IstanaPublicViewHeroV5PlayInEditor.ps1")},
        {TEXT("V5_EDITOR_START_SCRIPT"), TEXT("scripts/Start-IstanaPublicViewHeroV5Editor.ps1")},
        {TEXT("PROTECTED_V1_GEOMETRY_KERNEL"), TEXT("SourceAssets/IstanaPublicView/public_view_geometry.py")},
        {TEXT("PROTECTED_V1_BUILDING_GENERATOR"), TEXT("SourceAssets/IstanaPublicView/generate_public_view_building.py")},
        {TEXT("PROTECTED_V1_CONTEXT_GENERATOR"), TEXT("SourceAssets/IstanaPublicView/generate_public_view_context.py")},
        {TEXT("PROTECTED_V1_CONTRACT"), TEXT("SourceAssets/IstanaPublicView/istana_public_view.contract.json")}};
    if (RolePaths.Num() != ExpectedRolePaths.Num())
    {
        OutReport = FString::Printf(
            TEXT("V5_FREEZE_INVALID: exact role roster changed (%d records, expected %d)."),
            RolePaths.Num(),
            ExpectedRolePaths.Num());
        return false;
    }
    for (const TPair<FString, FString>& Expected : ExpectedRolePaths)
    {
        const FString* ActualPath = RolePaths.Find(Expected.Key);
        if (!ActualPath || *ActualPath != Expected.Value)
        {
            OutReport = FString::Printf(
                TEXT("V5_FREEZE_INVALID: role '%s' is absent or bound to a different path."),
                *Expected.Key);
            return false;
        }
    }
    OutReport = FString::Printf(
        TEXT("Validated %d exact integration-freeze records, including six source-only script metadata records (%lld-byte freeze, SHA-256 %s)."),
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

bool HasExactObjectFields(
    const TSharedPtr<FJsonObject>& Object,
    const TArray<FString>& ExpectedFields,
    FString& OutError)
{
    if (!Object.IsValid() || Object->Values.Num() != ExpectedFields.Num())
    {
        OutError = TEXT("JSON object field roster changed.");
        return false;
    }
    TSet<FString> ExpectedSet;
    for (const FString& Field : ExpectedFields)
    {
        ExpectedSet.Add(Field);
    }
    if (ExpectedSet.Num() != ExpectedFields.Num())
    {
        OutError = TEXT("Internal expected JSON field roster contains duplicates.");
        return false;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
    {
        if (!ExpectedSet.Contains(Pair.Key))
        {
            OutError = FString::Printf(
                TEXT("JSON object contains obsolete or unknown field '%s'."),
                *Pair.Key);
            return false;
        }
    }
    return true;
}

bool ValidateGeometrySource(
    FHeroGeometryContract& OutContract,
    FString& OutReport)
{
    if (FrozenGeometryFreezeSha256 == PendingSha256 ||
        FrozenGeometryContractSha256 == PendingSha256 ||
        FrozenGeometryGeneratorSha256 == PendingSha256 ||
        FrozenGeometryManifestSha256 == PendingSha256 ||
        FrozenGeometryObjSha256 == PendingSha256 ||
        FrozenGeometryAuditSha256 == PendingSha256 ||
        FrozenGeometryWholeShellAuditSha256 == PendingSha256)
    {
        OutReport = TEXT("V5_GEOMETRY_NOT_FROZEN: the independently audited V5 geometry authority is not fully bound.");
        return false;
    }

    FString Error;
    TSharedPtr<FJsonObject> Contract;
    TSharedPtr<FJsonObject> Manifest;
    TSharedPtr<FJsonObject> GeometryFreeze;
    if (!ValidateFrozenFile(
            ProjectSourcePath(GeometryContractRelativePath),
            17665,
            FrozenGeometryContractSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryGeneratorRelativePath),
            92361,
            FrozenGeometryGeneratorSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryManifestRelativePath),
            35793,
            FrozenGeometryManifestSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryFreezeRelativePath),
            4377,
            FrozenGeometryFreezeSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryObjRelativePath),
            ExpectedHeroBytes,
            FrozenGeometryObjSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryAuditRelativePath),
            243136,
            FrozenGeometryAuditSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryWholeShellAuditRelativePath),
            4883,
            FrozenGeometryWholeShellAuditSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryValidatorRelativePath),
            59575,
            FrozenGeometryValidatorSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryContractTestRelativePath),
            33340,
            FrozenGeometryContractTestSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryMutationTestRelativePath),
            51484,
            FrozenGeometryMutationTestSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryKernelRelativePath),
            23852,
            FrozenGeometryKernelSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometrySparseClosureAuditRelativePath),
            7627,
            FrozenGeometrySparseClosureAuditSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(GeometryDenseClosureAuditRelativePath),
            7286,
            FrozenGeometryDenseClosureAuditSha256,
            Error) ||
        !LoadJsonObject(ProjectSourcePath(GeometryContractRelativePath), Contract, Error) ||
        !LoadJsonObject(ProjectSourcePath(GeometryManifestRelativePath), Manifest, Error) ||
        !LoadJsonObject(ProjectSourcePath(GeometryFreezeRelativePath), GeometryFreeze, Error) ||
        !ReadExactString(
            Contract,
            TEXT("schema"),
            TEXT("triad.istana_public_view_hero_contract.v5"),
            Error) ||
        !ReadExactString(
            Manifest,
            TEXT("schema"),
            TEXT("triad.istana_public_view_building_manifest.v5"),
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("schema"),
            TEXT("triad.istana_public_view_hero_freeze.v5"),
            Error))
    {
        OutReport = TEXT("V5_GEOMETRY_AUTHORITY_INVALID: ") + Error;
        return false;
    }

    const auto ValidateIdentity = [&Error](
        const TSharedPtr<FJsonObject>& Object,
        bool bHasSourceClass) -> bool
    {
        return ReadExactString(
                   Object,
                   TEXT("assetId"),
                   TEXT("istana-public-view-hero-v5"),
                   Error) &&
            ReadExactString(Object, TEXT("referenceEpoch"), TEXT("2024-04"), Error) &&
            ReadExactString(
                Object,
                TEXT("claimStatus"),
                *FrozenGeometryClaimStatus,
                Error) &&
            (!bHasSourceClass || ReadExactString(
                Object,
                TEXT("sourceClass"),
                *FrozenGeometrySourceClass,
                Error));
    };
    if (!ValidateIdentity(Contract, true) ||
        !ValidateIdentity(Manifest, true) ||
        !ValidateIdentity(GeometryFreeze, false))
    {
        OutReport = TEXT("V5_GEOMETRY_IDENTITY_INVALID: selected-view reconstruction claim or source class changed. ") + Error;
        return false;
    }

    const TSharedPtr<FJsonObject>* ContractScope = nullptr;
    const TSharedPtr<FJsonObject>* ManifestScope = nullptr;
    const TSharedPtr<FJsonObject>* Surface = nullptr;
    const TSharedPtr<FJsonObject>* UnrealImport = nullptr;
    const TSharedPtr<FJsonObject>* LegacyImport = nullptr;
    const TSharedPtr<FJsonObject>* BuildingEnvelope = nullptr;
    const TSharedPtr<FJsonObject>* ApproximationBoundary = nullptr;
    const TSharedPtr<FJsonObject>* ContractCensus = nullptr;
    const TSharedPtr<FJsonObject>* FreezeCensus = nullptr;
    const TSharedPtr<FJsonObject>* QualityGates = nullptr;
    const TSharedPtr<FJsonObject>* QualityAudit = nullptr;
    const TSharedPtr<FJsonObject>* RoofAudit = nullptr;
    const TSharedPtr<FJsonObject>* WholeShell = nullptr;
    const TSharedPtr<FJsonObject>* FeatureCounts = nullptr;
    const TSharedPtr<FJsonObject>* ContractAuditAuthority = nullptr;
    const TSharedPtr<FJsonObject>* FreezeAuditAuthority = nullptr;
    const TSharedPtr<FJsonObject>* FreezeAuditDigests = nullptr;
    const TSharedPtr<FJsonObject>* FreezeAuditEvidence = nullptr;
    const TSharedPtr<FJsonObject>* DeterminismEvidence = nullptr;
    if (!Contract->TryGetObjectField(TEXT("scope"), ContractScope) ||
        !ContractScope || !(*ContractScope).IsValid() ||
        !Manifest->TryGetObjectField(TEXT("scope"), ManifestScope) ||
        !ManifestScope || !(*ManifestScope).IsValid() ||
        !Contract->TryGetObjectField(TEXT("surfaceContract"), Surface) ||
        !Surface || !(*Surface).IsValid() ||
        !(*Surface)->TryGetObjectField(TEXT("unrealImport"), UnrealImport) ||
        !UnrealImport || !(*UnrealImport).IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("unrealLegacyObjImportContract"), LegacyImport) ||
        !LegacyImport || !(*LegacyImport).IsValid() ||
        !Contract->TryGetObjectField(TEXT("buildingEnvelope"), BuildingEnvelope) ||
        !BuildingEnvelope || !(*BuildingEnvelope).IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("approximationBoundary"), ApproximationBoundary) ||
        !ApproximationBoundary || !(*ApproximationBoundary).IsValid() ||
        !Contract->TryGetObjectField(TEXT("meshCensus"), ContractCensus) ||
        !ContractCensus || !(*ContractCensus).IsValid() ||
        !GeometryFreeze->TryGetObjectField(TEXT("meshCensus"), FreezeCensus) ||
        !FreezeCensus || !(*FreezeCensus).IsValid() ||
        !Contract->TryGetObjectField(TEXT("qualityGates"), QualityGates) ||
        !QualityGates || !(*QualityGates).IsValid() ||
        !Manifest->TryGetObjectField(TEXT("qualityAudit"), QualityAudit) ||
        !QualityAudit || !(*QualityAudit).IsValid() ||
        !(*QualityAudit)->TryGetObjectField(TEXT("roofAudit"), RoofAudit) ||
        !RoofAudit || !(*RoofAudit).IsValid() ||
        !(*QualityAudit)->TryGetObjectField(TEXT("wholeShellClosure"), WholeShell) ||
        !WholeShell || !(*WholeShell).IsValid() ||
        !Manifest->TryGetObjectField(TEXT("featureCounts"), FeatureCounts) ||
        !FeatureCounts || !(*FeatureCounts).IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("auditSourceAuthority"), ContractAuditAuthority) ||
        !ContractAuditAuthority || !(*ContractAuditAuthority).IsValid() ||
        !GeometryFreeze->TryGetObjectField(
            TEXT("auditSourceAuthority"), FreezeAuditAuthority) ||
        !FreezeAuditAuthority || !(*FreezeAuditAuthority).IsValid() ||
        !GeometryFreeze->TryGetObjectField(
            TEXT("auditDigests"), FreezeAuditDigests) ||
        !FreezeAuditDigests || !(*FreezeAuditDigests).IsValid() ||
        !GeometryFreeze->TryGetObjectField(
            TEXT("auditEvidence"), FreezeAuditEvidence) ||
        !FreezeAuditEvidence || !(*FreezeAuditEvidence).IsValid() ||
        !GeometryFreeze->TryGetObjectField(
            TEXT("determinismEvidence"), DeterminismEvidence) ||
        !DeterminismEvidence || !(*DeterminismEvidence).IsValid())
    {
        OutReport = TEXT("V5_GEOMETRY_SCHEMA_INVALID: a required final V5 contract/manifest/freeze object is absent.");
        return false;
    }

    const TArray<FString> ExactScopeFields = {
        TEXT("preservesPublicViewV1V2V3V4Bytes"),
        TEXT("preservesPublicViewV1Surroundings"),
        TEXT("replacesHeroVisualOnly"),
        TEXT("addsV5GeometryNamespace"),
        TEXT("addsV5MaterialNamespace"),
        TEXT("rebuildsAllElevationUpperTowerFromLawfulPublicProportions"),
        TEXT("rebuildsCentralRoofMansardDormersFromLawfulPublicProportions"),
        TEXT("changesTerrain"),
        TEXT("changesHardscape"),
        TEXT("changesVegetation"),
        TEXT("changesContextBuildings"),
        TEXT("changesWorldLighting"),
        TEXT("changesRendererSettings"),
        TEXT("changesFrozenTextureBytes"),
        TEXT("surveyAccuracyClaimed"),
        TEXT("oneToOneDigitalTwinClaimed"),
        TEXT("photogrammetryClaimed")};
    const auto ValidateScope = [&Error, &ExactScopeFields](
        const TSharedPtr<FJsonObject>& Scope) -> bool
    {
        return HasExactObjectFields(Scope, ExactScopeFields, Error) &&
            ReadExactBool(
                Scope,
                TEXT("preservesPublicViewV1V2V3V4Bytes"),
                true,
                Error) &&
            ReadExactBool(
                Scope,
                TEXT("preservesPublicViewV1Surroundings"),
                true,
                Error) &&
            ReadExactBool(Scope, TEXT("replacesHeroVisualOnly"), true, Error) &&
            ReadExactBool(Scope, TEXT("addsV5GeometryNamespace"), true, Error) &&
            ReadExactBool(Scope, TEXT("addsV5MaterialNamespace"), true, Error) &&
            ReadExactBool(
                Scope,
                TEXT("rebuildsAllElevationUpperTowerFromLawfulPublicProportions"),
                true,
                Error) &&
            ReadExactBool(
                Scope,
                TEXT("rebuildsCentralRoofMansardDormersFromLawfulPublicProportions"),
                true,
                Error) &&
            ReadExactBool(Scope, TEXT("changesTerrain"), false, Error) &&
            ReadExactBool(Scope, TEXT("changesHardscape"), false, Error) &&
            ReadExactBool(Scope, TEXT("changesVegetation"), false, Error) &&
            ReadExactBool(Scope, TEXT("changesContextBuildings"), false, Error) &&
            ReadExactBool(Scope, TEXT("changesWorldLighting"), false, Error) &&
            ReadExactBool(Scope, TEXT("changesRendererSettings"), false, Error) &&
            ReadExactBool(Scope, TEXT("changesFrozenTextureBytes"), false, Error) &&
            ReadExactBool(Scope, TEXT("surveyAccuracyClaimed"), false, Error) &&
            ReadExactBool(Scope, TEXT("oneToOneDigitalTwinClaimed"), false, Error) &&
            ReadExactBool(Scope, TEXT("photogrammetryClaimed"), false, Error);
    };
    FVector ImportScale;
    FVector LegacyImportScale;
    if (!ValidateScope(*ContractScope) ||
        !ValidateScope(*ManifestScope) ||
        !ReadExactString(
            *UnrealImport,
            TEXT("normalImportMethod"),
            TEXT("ImportNormals"),
            Error) ||
        !ReadExactBool(*UnrealImport, TEXT("recomputeNormals"), false, Error) ||
        !ReadExactBool(*UnrealImport, TEXT("recomputeTangents"), true, Error) ||
        !ReadExactString(
            *UnrealImport,
            TEXT("tangentSpace"),
            TEXT("MikkTSpace"),
            Error) ||
        !ReadExactBool(*UnrealImport, TEXT("removeDegenerates"), true, Error) ||
        !ReadVector3Meters(*UnrealImport, TEXT("buildScale3D"), ImportScale, Error) ||
        !ImportScale.Equals(FVector(1.0, 1.0, 1.0), 1.0e-12) ||
        !ReadExactString(
            *LegacyImport,
            TEXT("contractId"),
            TEXT("UE55_LEGACY_OBJ_Y_MIRROR_WINDING_NORMAL_AND_UV_V_PRECONDITION"),
            Error) ||
        !ReadExactBool(
            *LegacyImport,
            TEXT("negativeBuildScaleForbidden"),
            true,
            Error) ||
        !ReadVector3Meters(
            *LegacyImport, TEXT("buildScale3D"), LegacyImportScale, Error) ||
        !LegacyImportScale.Equals(FVector(1.0, 1.0, 1.0), 1.0e-12) ||
        !ReadExactBool(
            *ApproximationBoundary,
            TEXT("arbitraryViewIndistinguishabilityClaimed"),
            false,
            Error) ||
        !ReadExactBool(
            *ApproximationBoundary,
            TEXT("oneToOneModelClaimed"),
            false,
            Error) ||
        !ReadExactString(
            *ApproximationBoundary,
            TEXT("towerMansardDormerStatus"),
            TEXT("V4_ALL_ELEVATION_UPPER_TOWER_MANSARD_SLATE_DORMER_AND_DRAINAGE_STREAMS_REMOVED_AND_REBUILT_AS_BOUNDED_V5_PUBLIC_REFERENCE_INTERPRETATION"),
            Error))
    {
        OutReport = TEXT("V5_GEOMETRY_SCOPE_INVALID: exact additive scope, claim boundary, or UE5.5 import contract changed. ") + Error;
        return false;
    }

    const auto ReadExpectedCount = [&Error](
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field,
        int64 Expected) -> bool
    {
        int64 Actual = -1;
        return ReadInteger(Object, Field, Actual, Error) && Actual == Expected;
    };
    const auto ValidateCensus = [&Error, &ReadExpectedCount](
        const TSharedPtr<FJsonObject>& Census) -> bool
    {
        struct FCensusExpectation
        {
            const TCHAR* Field;
            int64 Groups;
            int64 Triangles;
        };
        const FCensusExpectation Expectations[] = {
            {TEXT("baselineV4"), 33395, 958836},
            {TEXT("removedV4"), 10844, 249352},
            {TEXT("retainedV4"), 22551, 709484},
            {TEXT("authoredV5"), 5954, 122236},
            {TEXT("finalHeroV5"), ExpectedHeroGroups, ExpectedHeroTriangles}};
        if (!Census.IsValid() || Census->Values.Num() != 5)
        {
            Error = TEXT("Mesh census roster changed.");
            return false;
        }
        for (const FCensusExpectation& Expected : Expectations)
        {
            const TSharedPtr<FJsonObject>* Row = nullptr;
            if (!Census->TryGetObjectField(Expected.Field, Row) ||
                !Row || !(*Row).IsValid() || (*Row)->Values.Num() != 2 ||
                !ReadExpectedCount(*Row, TEXT("groupCount"), Expected.Groups) ||
                !ReadExpectedCount(
                    *Row, TEXT("triangleCount"), Expected.Triangles))
            {
                Error = FString::Printf(
                    TEXT("Mesh census row '%s' changed."),
                    Expected.Field);
                return false;
            }
        }
        return true;
    };
    if (!ValidateCensus(*ContractCensus) || !ValidateCensus(*FreezeCensus))
    {
        OutReport = TEXT("V5_GEOMETRY_CENSUS_INVALID: ") + Error;
        return false;
    }

    FVector ContractBoundsMin;
    FVector ContractBoundsMax;
    FVector FreezeBoundsMin;
    FVector FreezeBoundsMax;
    const FVector ExpectedBoundsMin(-63.6, -56.0, 0.0);
    const FVector ExpectedBoundsMax(63.6, 45.8, 36.0);
    if (!ReadVector3Meters(
            *BuildingEnvelope,
            TEXT("boundsMinMeters"),
            ContractBoundsMin,
            Error) ||
        !ReadVector3Meters(
            *BuildingEnvelope,
            TEXT("boundsMaxMeters"),
            ContractBoundsMax,
            Error) ||
        !ReadVector3Meters(
            GeometryFreeze,
            TEXT("boundsMinMeters"),
            FreezeBoundsMin,
            Error) ||
        !ReadVector3Meters(
            GeometryFreeze,
            TEXT("boundsMaxMeters"),
            FreezeBoundsMax,
            Error) ||
        !ContractBoundsMin.Equals(ExpectedBoundsMin, 1.0e-9) ||
        !ContractBoundsMax.Equals(ExpectedBoundsMax, 1.0e-9) ||
        !FreezeBoundsMin.Equals(ExpectedBoundsMin, 1.0e-9) ||
        !FreezeBoundsMax.Equals(ExpectedBoundsMax, 1.0e-9) ||
        !ReadExactBool(
            *BuildingEnvelope,
            TEXT("registrationTransformChangedFromV4"),
            false,
            Error) ||
        !ReadExactBool(
            *BuildingEnvelope,
            TEXT("buildingEnvelopeChangedFromV4"),
            true,
            Error) ||
        !ReadExactString(
            *BuildingEnvelope,
            TEXT("buildingEnvelopeChangeReason"),
            TEXT("EXACT_AUTHORISED_FORWARD_PORTICO_STAIR_DUPLICATE_TOWER_FRONT_ALL_ELEVATION_GENERIC_UPPER_TOWER_AND_CENTRAL_ROOF_REPLACEMENT_PLUS_PUBLIC_ARCH_SQUARE_VOUSSOIR_REMOVAL"),
            Error))
    {
        OutReport = TEXT("V5_GEOMETRY_ENVELOPE_INVALID: exact registered bounds or authorised envelope explanation changed. ") + Error;
        return false;
    }

    const auto ValidateAuditAuthority = [&Error](
        const TSharedPtr<FJsonObject>& Authority) -> bool
    {
        return ReadExactString(
                   Authority,
                   TEXT("geometryAuditRelativePath"),
                   TEXT("v5_geometry_audit.py"),
                   Error) &&
            ReadExactString(
                Authority,
                TEXT("geometryAuditSha256"),
                *FrozenGeometryAuditSha256,
                Error) &&
            ReadExactString(
                Authority,
                TEXT("wholeShellAuditRelativePath"),
                TEXT("v5_whole_shell_audit.py"),
                Error) &&
            ReadExactString(
                Authority,
                TEXT("wholeShellAuditSha256"),
                *FrozenGeometryWholeShellAuditSha256,
                Error) &&
            ReadExactString(
                Authority,
                TEXT("v3SparseClosureAuditRelativePath"),
                TEXT("../IstanaPublicViewV3/facade_closure_audit.py"),
                Error) &&
            ReadExactString(
                Authority,
                TEXT("v3SparseClosureAuditSha256"),
                *FrozenGeometrySparseClosureAuditSha256,
                Error) &&
            ReadExactString(
                Authority,
                TEXT("v3DenseClosureAuditRelativePath"),
                TEXT("../IstanaPublicViewV3/dense_facade_closure_audit.py"),
                Error) &&
            ReadExactString(
                Authority,
                TEXT("v3DenseClosureAuditSha256"),
                *FrozenGeometryDenseClosureAuditSha256,
                Error);
    };
    if (!ValidateAuditAuthority(*ContractAuditAuthority) ||
        !ValidateAuditAuthority(*FreezeAuditAuthority) ||
        !ReadExactString(
            Manifest,
            TEXT("contractSha256"),
            *FrozenGeometryContractSha256,
            Error) ||
        !ReadExactString(
            Manifest,
            TEXT("generatorSha256"),
            *FrozenGeometryGeneratorSha256,
            Error) ||
        !ReadExactString(
            Manifest,
            TEXT("geometryAuditSha256"),
            *FrozenGeometryAuditSha256,
            Error) ||
        !ReadExactString(
            Manifest,
            TEXT("wholeShellAuditSha256"),
            *FrozenGeometryWholeShellAuditSha256,
            Error) ||
        !ReadExactString(
            Manifest,
            TEXT("v3SparseClosureAuditSha256"),
            *FrozenGeometrySparseClosureAuditSha256,
            Error) ||
        !ReadExactString(
            Manifest,
            TEXT("v3DenseClosureAuditSha256"),
            *FrozenGeometryDenseClosureAuditSha256,
            Error) ||
        !ReadExactString(
            Manifest,
            TEXT("geometryKernelSha256"),
            *FrozenGeometryKernelSha256,
            Error) ||
        !ReadExactString(
            Manifest,
            TEXT("semanticSha256"),
            *FrozenGeometryManifestSemanticSha256,
            Error))
    {
        OutReport = TEXT("V5_GEOMETRY_AUDIT_AUTHORITY_INVALID: exact audit source or manifest provenance changed. ") + Error;
        return false;
    }

    const TCHAR* DenseDigestField =
        TEXT("publicViewClosureNearestSurfaceSemanticDistanceSha256");
    const TCHAR* ForegroundDigestField =
        TEXT("intentionalForegroundCanonicalFullPathSha256");
    const TCHAR* BoundaryDigestField =
        TEXT("roofSilhouetteBoundarySemanticDistanceSha256");
    const TCHAR* DormerDigestField =
        TEXT("dormerRaisedObliqueLayeringRawPathSha256");
    const TCHAR* DrainageDigestField =
        TEXT("roofDrainageLiteralPathSha256");
    const FString DenseDigest(
        TEXT("299a4573a33adb8935e1381656c2d1d246470e07506889d8d2894943896e5696"));
    const FString ForegroundDigest(
        TEXT("d2da442a84c2d78fa4e83601e7362eede80ba50bfaa07a65d514ae25d211b42c"));
    const FString BoundaryDigest(
        TEXT("01c0e746709ffd06b48bd46294188de0dc92c3724fc1217a991431a2d94d26b9"));
    const FString DormerDigest(
        TEXT("2826ddfd3f2038edb01d1bd000fbaf338a27fea65a1e6d56685226c5701c3ff4"));
    const FString DrainageDigest(
        TEXT("eded80916a2b223ad77704559c799b4343a71341c5602264643f847569337877"));
    if (!ReadExactString(*QualityGates, DenseDigestField, *DenseDigest, Error) ||
        !ReadExactString(
            *QualityGates, ForegroundDigestField, *ForegroundDigest, Error) ||
        !ReadExactString(
            *QualityGates, BoundaryDigestField, *BoundaryDigest, Error) ||
        !ReadExactString(*QualityGates, DormerDigestField, *DormerDigest, Error) ||
        !ReadExactString(
            *QualityGates, DrainageDigestField, *DrainageDigest, Error) ||
        !ReadExactString(*QualityAudit, DenseDigestField, *DenseDigest, Error) ||
        !ReadExactString(
            *QualityAudit, ForegroundDigestField, *ForegroundDigest, Error) ||
        !ReadExactString(
            *QualityAudit, BoundaryDigestField, *BoundaryDigest, Error) ||
        !ReadExactString(*RoofAudit, DormerDigestField, *DormerDigest, Error) ||
        !ReadExactString(*RoofAudit, DrainageDigestField, *DrainageDigest, Error) ||
        !ReadExactString(*FreezeAuditDigests, DenseDigestField, *DenseDigest, Error) ||
        !ReadExactString(
            *FreezeAuditDigests, ForegroundDigestField, *ForegroundDigest, Error) ||
        !ReadExactString(
            *FreezeAuditDigests, BoundaryDigestField, *BoundaryDigest, Error) ||
        !ReadExactString(
            *FreezeAuditDigests, DormerDigestField, *DormerDigest, Error) ||
        !ReadExactString(
            *FreezeAuditDigests, DrainageDigestField, *DrainageDigest, Error))
    {
        OutReport = TEXT("V5_GEOMETRY_EVIDENCE_INVALID: an exact semantic/path evidence digest changed. ") + Error;
        return false;
    }

    struct FCountExpectation
    {
        const TCHAR* Field;
        int64 Expected;
    };
    const FCountExpectation GateCounts[] = {
        {TEXT("retainedDeepDoorClosureFieldGroupCount"), 93},
        {TEXT("retainedDeepDoorClosureFieldTriangleCount"), 1116},
        {TEXT("authoredOpalineTriangleCount"), 0},
        {TEXT("groundRectilinearPierAssemblyCount"), 4},
        {TEXT("upperIonicPairGroupCount"), 4},
        {TEXT("upperIonicShaftCount"), 8},
        {TEXT("louvreSillBalustradeRunCount"), 3},
        {TEXT("upperTripartiteScreenPanelCount"), 3},
        {TEXT("upperDenseHorizontalBladeCount"), 0},
        {TEXT("compactDormerCount"), 6},
        {TEXT("denseClosureProbeCount"), 26251},
        {TEXT("denseClosureWitnessCount"), 26267},
        {TEXT("denseClosureMissCount"), 0},
        {TEXT("denseClosureSemanticOrDistanceFailureCount"), 0},
        {TEXT("transparentFirstProbeCount"), 2065},
        {TEXT("transparentFirstOpaqueMissingCount"), 0},
        {TEXT("intentionalForegroundProbeCount"), 213},
        {TEXT("intentionalForegroundFailureCount"), 0},
        {TEXT("roofSilhouetteBoundaryWitnessCount"), 16},
        {TEXT("roofSilhouetteBoundaryFailureCount"), 0},
        {TEXT("wholeShellInheritedProbeCount"), 32960},
        {TEXT("wholeShellReplacementProbeCount"), 26251},
        {TEXT("wholeShellEquivalentProbeCount"), 59211},
        {TEXT("wholeShellClosureMissCount"), 0},
        {TEXT("authoredExactDuplicateFaceCount"), 0},
        {TEXT("authoredUvDegenerateTriangleCount"), 0},
        {TEXT("closedHardSurfaceGroupCount"), 2332},
        {TEXT("closedHardSurfaceTopologyFailureCount"), 0},
        {TEXT("closedHardSurfaceEdgeOrientationFailureCount"), 0},
        {TEXT("explicitGroundTransitionClosedGroupCount"), 12},
        {TEXT("explicitGroundTransitionTriangleCensusFailureCount"), 0},
        {TEXT("explicitGroundTransitionTopologyFailureCount"), 0},
        {TEXT("explicitGroundTransitionEdgeOrientationFailureCount"), 0},
        {TEXT("explicitRoofDormerDrainageClosedGroupCount"), 39},
        {TEXT("explicitRoofDormerDrainageTopologyFailureCount"), 0},
        {TEXT("explicitRoofDormerDrainageEdgeOrientationFailureCount"), 0},
        {TEXT("closedTaperedSegmentGroupCount"), 28},
        {TEXT("dormerNormalObliqueVisibilityProbeCount"), 96},
        {TEXT("dormerVisibilityFailureCount"), 0},
        {TEXT("dormerExactTargetBinProbeCount"), 96},
        {TEXT("dormerGlassExactTerminalProbeCount"), 24},
        {TEXT("dormerFrameBoxClosedGroupCount"), 60},
        {TEXT("dormerFrameBoxTriangleCount"), 720},
        {TEXT("dormerFrameBoxTriangleCensusFailureCount"), 0},
        {TEXT("dormerFrameBoxMaterialFailureCount"), 0},
        {TEXT("dormerFrameBoxTopologyFailureCount"), 0},
        {TEXT("dormerFrameBoxWindingFailureCount"), 0},
        {TEXT("individualSlateTileCount"), 2312},
        {TEXT("slateCrossCourseAxisSeparationProbeCount"), 4168},
        {TEXT("roofDrainageGroupCount"), 32},
        {TEXT("roofDrainageCenterlineProbeCount"), 14},
        {TEXT("roofDrainageContactProbeCount"), 2},
        {TEXT("roofDrainageEndpointRegistrationProbeCount"), 40},
        {TEXT("roofDrainageLiteralRawHitCount"), 152},
        {TEXT("roofDrainagePublicClearLineProbeCount"), 4},
        {TEXT("roofDrainagePublicClearLineRawHitCount"), 12},
        {TEXT("roofDrainageVisibilityFailureCount"), 0},
        {TEXT("porticoPublicEdgeBalustradeRunCount"), 5},
        {TEXT("porticoPublicEdgeBalustradeReturnRunCount"), 2},
        {TEXT("porticoPublicEdgeBalustradePedestalCount"), 8}};
    for (const FCountExpectation& Expected : GateCounts)
    {
        if (!ReadExpectedCount(*QualityGates, Expected.Field, Expected.Expected))
        {
            OutReport = FString::Printf(
                TEXT("V5_GEOMETRY_GATE_INVALID: exact contract gate '%s' changed. %s"),
                Expected.Field,
                *Error);
            return false;
        }
    }
    if (!ReadExactBool(
            *QualityGates,
            TEXT("upperTowerBalustradeIsSeparateSetbackTier"),
            true,
            Error) ||
        !ReadExactBool(
            *QualityGates,
            TEXT("deterministicIndependentRegenerationRequired"),
            true,
            Error) ||
        !ReadExactBool(*QualityAudit, TEXT("passed"), true, Error) ||
        !ReadExpectedCount(
            *QualityAudit, TEXT("triangleCount"), ExpectedHeroTriangles) ||
        !ReadExpectedCount(
            *QualityAudit, TEXT("publicViewClosureProbeCount"), 26251) ||
        !ReadExpectedCount(
            *QualityAudit,
            TEXT("publicViewClosureNearestSurfaceWitnessCount"),
            26267) ||
        !ReadExpectedCount(
            *QualityAudit, TEXT("publicViewClosureMissCount"), 0) ||
        !ReadExpectedCount(
            *QualityAudit,
            TEXT("publicViewClosureSemanticOrDistanceFailureCount"),
            0) ||
        !ReadExpectedCount(
            *QualityAudit, TEXT("transparentFirstOpaqueMissingCount"), 0) ||
        !ReadExpectedCount(
            *QualityAudit, TEXT("intentionalForegroundFailureCount"), 0) ||
        !ReadExpectedCount(
            *QualityAudit, TEXT("roofSilhouetteBoundaryFailureCount"), 0) ||
        !ReadExpectedCount(
            *QualityAudit, TEXT("wholeShellClosureMissCount"), 0) ||
        !ReadExactBool(*WholeShell, TEXT("passed"), true, Error) ||
        !ReadExpectedCount(
            *WholeShell, TEXT("retainedInheritedProbeCount"), 32960) ||
        !ReadExpectedCount(
            *WholeShell, TEXT("replacementCentralProbeCount"), 26251) ||
        !ReadExpectedCount(
            *WholeShell, TEXT("equivalentWholeShellProbeCount"), 59211) ||
        !ReadExpectedCount(*WholeShell, TEXT("missCount"), 0) ||
        !ReadExpectedCount(*RoofAudit, TEXT("compactDormerCount"), 6) ||
        !ReadExpectedCount(
            *RoofAudit, TEXT("dormerVisibilityFailureCount"), 0) ||
        !ReadExpectedCount(
            *RoofAudit, TEXT("roofDrainageVisibilityFailureCount"), 0))
    {
        OutReport = TEXT("V5_GEOMETRY_AUDIT_INVALID: final manifest audit evidence changed. ") + Error;
        return false;
    }

    if (!ReadExpectedCount(
            *FeatureCounts, TEXT("v5RemovedV4Groups"), 10844) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5RemovedV4Triangles"), 249352) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5RetainedV4Triangles"), 709484) ||
        !ReadExpectedCount(*FeatureCounts, TEXT("v5AddedGroups"), 5954) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5AuditAuthoredTriangleCount"), 122236) ||
        !ReadExpectedCount(
            *FeatureCounts,
            TEXT("v5AuditTriangleCount"),
            ExpectedHeroTriangles) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5TerminalRecessSeams"), 15) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5GroundRectilinearPierAssemblies"), 4) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5UpperIonicPairGroups"), 4) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5UpperIonicPairedShafts"), 8) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5UpperLouvreSillBalustradeRuns"), 3) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5UpperTowerTripartiteScreenPanels"), 3) ||
        !ReadExpectedCount(*FeatureCounts, TEXT("v5CompactDormers"), 6) ||
        !ReadExpectedCount(
            *FeatureCounts, TEXT("v5IndividualSlateTiles"), 2312))
    {
        OutReport = TEXT("V5_GEOMETRY_FEATURE_CENSUS_INVALID: final manifest architecture census changed. ") + Error;
        return false;
    }

    TArray<FString> FreezeSlots;
    TArray<FString> FrozenPriorNamespacesChanged;
    int64 FreezeHeroBytes = 0;
    int64 FreezeHeroGroups = 0;
    int64 FreezeHeroTriangles = 0;
    int64 FreezeHeroVertices = 0;
    int64 FreezeManifestBytes = 0;
    int64 RegenerationCount = 0;
    if (!ReadExactString(
            GeometryFreeze,
            TEXT("freezeStatus"),
            TEXT("FROZEN_AFTER_EXACT_TRUTH_PROVENANCE_DUAL_BYTE_DETERMINISM_66_ADVERSARIAL_MUTATIONS_AND_ZERO_GEOMETRY_AUDIT_FAILURES"),
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("contractSha256"),
            *FrozenGeometryContractSha256,
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("generatorSha256"),
            *FrozenGeometryGeneratorSha256,
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("manifestSha256"),
            *FrozenGeometryManifestSha256,
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("manifestSemanticSha256"),
            *FrozenGeometryManifestSemanticSha256,
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("heroSha256"),
            *FrozenGeometryObjSha256,
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("auditSha256"),
            *FrozenGeometryAuditSha256,
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("wholeShellAuditSha256"),
            *FrozenGeometryWholeShellAuditSha256,
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("geometryKernelSha256"),
            *FrozenGeometryKernelSha256,
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("v3SparseClosureAuditSha256"),
            *FrozenGeometrySparseClosureAuditSha256,
            Error) ||
        !ReadExactString(
            GeometryFreeze,
            TEXT("v3DenseClosureAuditSha256"),
            *FrozenGeometryDenseClosureAuditSha256,
            Error) ||
        !ReadInteger(
            GeometryFreeze, TEXT("heroBytes"), FreezeHeroBytes, Error) ||
        FreezeHeroBytes != ExpectedHeroBytes ||
        !ReadInteger(
            GeometryFreeze, TEXT("heroGroups"), FreezeHeroGroups, Error) ||
        FreezeHeroGroups != ExpectedHeroGroups ||
        !ReadInteger(
            GeometryFreeze, TEXT("heroTriangles"), FreezeHeroTriangles, Error) ||
        FreezeHeroTriangles != ExpectedHeroTriangles ||
        !ReadInteger(
            GeometryFreeze, TEXT("heroVertices"), FreezeHeroVertices, Error) ||
        FreezeHeroVertices != ExpectedHeroVertices ||
        !ReadInteger(
            GeometryFreeze, TEXT("manifestBytes"), FreezeManifestBytes, Error) ||
        FreezeManifestBytes != 35793 ||
        !ReadStringArray(
            GeometryFreeze, TEXT("materialSlots"), FreezeSlots, Error) ||
        FreezeSlots != ExpectedHeroSlots() ||
        !ReadStringArray(
            GeometryFreeze,
            TEXT("frozenPriorNamespacesChanged"),
            FrozenPriorNamespacesChanged,
            Error) ||
        FrozenPriorNamespacesChanged.Num() != 0 ||
        !ReadInteger(
            *DeterminismEvidence,
            TEXT("independentRegenerationCount"),
            RegenerationCount,
            Error) ||
        RegenerationCount != 2 ||
        !ReadExactBool(
            *DeterminismEvidence, TEXT("heroByteIdentical"), true, Error) ||
        !ReadExactBool(
            *DeterminismEvidence, TEXT("manifestByteIdentical"), true, Error) ||
        !ReadExpectedCount(
            *FreezeAuditEvidence, TEXT("denseProbeCount"), 26251) ||
        !ReadExpectedCount(
            *FreezeAuditEvidence, TEXT("denseWitnessCount"), 26267) ||
        !ReadExpectedCount(
            *FreezeAuditEvidence, TEXT("denseMissCount"), 0) ||
        !ReadExpectedCount(
            *FreezeAuditEvidence,
            TEXT("semanticOrDistanceFailureCount"),
            0) ||
        !ReadExpectedCount(
            *FreezeAuditEvidence,
            TEXT("transparentFirstOpaqueMissingCount"),
            0) ||
        !ReadExpectedCount(
            *FreezeAuditEvidence, TEXT("wholeShellEquivalentProbeCount"), 59211) ||
        !ReadExpectedCount(
            *FreezeAuditEvidence, TEXT("wholeShellClosureMissCount"), 0))
    {
        OutReport = TEXT("V5_GEOMETRY_FREEZE_INVALID: exact reviewed freeze binding changed. ") + Error;
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("files"), Files) ||
        !Files || Files->Num() != 1)
    {
        OutReport = TEXT("V5_GEOMETRY_MANIFEST_INVALID: exactly one hero file is required.");
        return false;
    }
    const TSharedPtr<FJsonObject> Record = (*Files)[0].IsValid()
        ? (*Files)[0]->AsObject()
        : nullptr;
    FString Path;
    FString Role;
    FString CoordinateExportContract;
    TArray<FString> ContractSlots;
    TArray<FString> ManifestSlots;
    TArray<FString> ObjFirstUseSlots;
    int64 TriangleCount = 0;
    int64 VertexCount = 0;
    int64 HardTriangleCount = 0;
    int64 SmoothTriangleCount = 0;
    if (!Record.IsValid() ||
        !ReadStringArray(
            Contract, TEXT("expectedMaterialSlots"), ContractSlots, Error) ||
        !ReadStringArray(
            Manifest, TEXT("materialSlots"), ManifestSlots, Error) ||
        !ReadStringArray(
            Manifest,
            TEXT("objFirstUseMaterialOrder"),
            ObjFirstUseSlots,
            Error) ||
        ContractSlots != ExpectedHeroSlots() ||
        ManifestSlots != ExpectedHeroSlots() ||
        ObjFirstUseSlots != ExpectedHeroSlots() ||
        !Record->TryGetStringField(TEXT("path"), Path) ||
        Path != TEXT("SM_IstanaPublicViewV5_Building_Hero.obj") ||
        !Record->TryGetStringField(TEXT("role"), Role) ||
        Role != TEXT("BUILDING_HERO_VISUAL_V5") ||
        !Record->TryGetStringField(TEXT("sha256"), OutContract.SourceSha256) ||
        OutContract.SourceSha256 != FrozenGeometryObjSha256 ||
        !Record->TryGetStringField(
            TEXT("coordinateExportContract"), CoordinateExportContract) ||
        CoordinateExportContract !=
            TEXT("UE55_LEGACY_OBJ_Y_MIRROR_WINDING_NORMAL_AND_UV_V_PRECONDITION") ||
        !ReadInteger(
            Record, TEXT("bytes"), OutContract.SourceBytes, Error) ||
        OutContract.SourceBytes != ExpectedHeroBytes ||
        !ReadInteger(Record, TEXT("triangles"), TriangleCount, Error) ||
        TriangleCount != ExpectedHeroTriangles ||
        !ReadInteger(Record, TEXT("vertices"), VertexCount, Error) ||
        VertexCount != ExpectedHeroVertices ||
        !ReadInteger(
            Record, TEXT("hardTriangleCount"), HardTriangleCount, Error) ||
        HardTriangleCount != 456032 ||
        !ReadInteger(
            Record, TEXT("smoothTriangleCount"), SmoothTriangleCount, Error) ||
        SmoothTriangleCount != 375688 ||
        HardTriangleCount + SmoothTriangleCount != TriangleCount ||
        !ReadVector3Meters(
            Record,
            TEXT("expectedUnrealImportedBoundsMinMeters"),
            OutContract.BoundsMinMeters,
            Error) ||
        !ReadVector3Meters(
            Record,
            TEXT("expectedUnrealImportedBoundsMaxMeters"),
            OutContract.BoundsMaxMeters,
            Error) ||
        !OutContract.BoundsMinMeters.Equals(ExpectedBoundsMin, 1.0e-9) ||
        !OutContract.BoundsMaxMeters.Equals(ExpectedBoundsMax, 1.0e-9) ||
        !ReadStringArray(
            Record, TEXT("materialSlots"), OutContract.MaterialSlots, Error) ||
        OutContract.MaterialSlots != ExpectedHeroSlots())
    {
        OutReport = TEXT("V5_GEOMETRY_MANIFEST_INVALID: exact hero file record, bounds, or ordered M_IPV5 roster changed. ") + Error;
        return false;
    }
    for (const FString& Slot : OutContract.MaterialSlots)
    {
        if (!Slot.StartsWith(TEXT("M_IPV5_")) || Slot.StartsWith(TEXT("M_IPV_")))
        {
            OutReport = TEXT("V5_GEOMETRY_MANIFEST_INVALID: legacy M_IPV_* slots are forbidden.");
            return false;
        }
    }

    OutContract.Triangles = static_cast<int32>(TriangleCount);
    OutReport = FString::Printf(
        TEXT("Validated exact frozen %lld-byte V5 hero OBJ (%d triangles, %d vertices, %d groups, eleven ordered M_IPV5 slots) plus contract, manifest, dual-determinism freeze, whole-shell audit, adversarial mutation suite, validator, and inherited closure/kernel authority. Claim remains selected-view/public-reference reconstruction, not survey or one-to-one."),
        OutContract.SourceBytes,
        OutContract.Triangles,
        ExpectedHeroVertices,
        ExpectedHeroGroups);
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
    FString TextureSurfaceIdV3;
    TArray<FString> V5Slots;
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
    if (Usage == EHeroTextureUsage::Normal ||
        Usage == EHeroTextureUsage::DetailNormal)
    {
        return TEXTUREGROUP_WorldNormalMap;
    }
    if (Usage == EHeroTextureUsage::PackedOrm)
    {
        return TEXTUREGROUP_WorldSpecular;
    }
    return TEXTUREGROUP_World;
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

FString ForbiddenV5TextureObjectPath(const FHeroTextureSpec& Spec)
{
    return FString::Printf(
        TEXT("%s/Textures/%s.%s"),
        *MaterialAssetRoot,
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
        ExpectedMapType == TEXT("ArchitecturalDirtMasks") ? 2048 :
        4096;
    const FString ExpectedMode =
        ExpectedMapType == TEXT("Height") ? TEXT("L") :
        (ExpectedMapType == TEXT("MacroVariation") ||
         ExpectedMapType == TEXT("SurfaceMasks") ||
         ExpectedMapType == TEXT("ArchitecturalDirtMasks"))
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
        FPaths::GetPath(MaterialGeneratedManifestRelativePath),
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

template <typename ValueType>
bool HasExactParameterNames(
    const TMap<FName, ValueType>& Actual,
    const TArray<FName>& Expected)
{
    if (Actual.Num() != Expected.Num())
    {
        return false;
    }
    for (const FName Name : Expected)
    {
        if (!Actual.Contains(Name))
        {
            return false;
        }
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

bool ValidateNormalStrengthOwnership(
    const TSharedPtr<FJsonObject>& Contract,
    const TSharedPtr<FJsonObject>& Interface,
    const TSharedPtr<FJsonObject>& GeneratedManifest,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* ContractOwnership = nullptr;
    const TSharedPtr<FJsonObject>* InterfaceOwnership = nullptr;
    const TSharedPtr<FJsonObject>* GeneratedOwnership = nullptr;
    if (!Contract.IsValid() || !Interface.IsValid() ||
        !GeneratedManifest.IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("normalStrengthOwnership"), ContractOwnership) ||
        !ContractOwnership || !(*ContractOwnership).IsValid() ||
        !Interface->TryGetObjectField(
            TEXT("normalStrengthOwnership"), InterfaceOwnership) ||
        !InterfaceOwnership || !(*InterfaceOwnership).IsValid() ||
        !GeneratedManifest->TryGetObjectField(
            TEXT("normalStrengthOwnership"), GeneratedOwnership) ||
        !GeneratedOwnership || !(*GeneratedOwnership).IsValid())
    {
        OutError = TEXT("Normal-strength ownership objects are absent.");
        return false;
    }

    const TArray<FString> ExpectedGenerationFields = {
        TEXT("sourceNormalMix"), TEXT("detailSourceMix")};
    const TArray<FString> ExpectedRuntimeFields = {
        TEXT("NormalStrength"), TEXT("DetailNormalStrength")};
    for (const TSharedPtr<FJsonObject>* Ownership : {
            ContractOwnership, InterfaceOwnership, GeneratedOwnership})
    {
        TArray<FString> GenerationFields;
        TArray<FString> RuntimeFields;
        if (!ReadStringArray(
                *Ownership,
                TEXT("mapGenerationFields"),
                GenerationFields,
                OutError) ||
            !ReadStringArray(
                *Ownership,
                TEXT("runtimeAmplitudeFields"),
                RuntimeFields,
                OutError) ||
            GenerationFields != ExpectedGenerationFields ||
            RuntimeFields != ExpectedRuntimeFields ||
            !ReadExactBool(
                *Ownership,
                TEXT("sameFieldMayDriveGenerationAndRuntime"),
                false,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Normal generation and runtime amplitude ownership changed.");
            }
            return false;
        }
    }

    int64 RuntimeApplicationCount = 0;
    if (!ReadInteger(
            *InterfaceOwnership,
            TEXT("runtimeApplicationCount"),
            RuntimeApplicationCount,
            OutError) ||
        RuntimeApplicationCount != 1)
    {
        OutError = TEXT("Normal strength must be applied exactly once by the Unreal runtime material.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ContractMaterials = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* GeneratedMaterials = nullptr;
    if (!Contract->TryGetArrayField(TEXT("materials"), ContractMaterials) ||
        !ContractMaterials || ContractMaterials->Num() != 8 ||
        !GeneratedManifest->TryGetArrayField(
            TEXT("materials"), GeneratedMaterials) ||
        !GeneratedMaterials || GeneratedMaterials->Num() != 8)
    {
        OutError = TEXT("Normal-strength ownership requires exactly eight material rows.");
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *ContractMaterials)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        double SourceNormalMix = 0.0;
        double DetailSourceMix = 0.0;
        if (!Row.IsValid() ||
            !Row->TryGetNumberField(TEXT("sourceNormalMix"), SourceNormalMix) ||
            !Row->TryGetNumberField(TEXT("detailSourceMix"), DetailSourceMix) ||
            SourceNormalMix != 1.0 || DetailSourceMix != 1.0)
        {
            OutError = TEXT("Generated normal maps must retain unit source/detail mix before runtime strength.");
            return false;
        }
    }
    for (const TSharedPtr<FJsonValue>& Value : *GeneratedMaterials)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        const TSharedPtr<FJsonObject>* NormalGeneration = nullptr;
        double SourceNormalMix = 0.0;
        double DetailSourceMix = 0.0;
        if (!Row.IsValid() ||
            !Row->TryGetObjectField(
                TEXT("normalGeneration"), NormalGeneration) ||
            !NormalGeneration || !(*NormalGeneration).IsValid() ||
            (*NormalGeneration)->Values.Num() != 4 ||
            !(*NormalGeneration)->TryGetNumberField(
                TEXT("sourceNormalMix"), SourceNormalMix) ||
            !(*NormalGeneration)->TryGetNumberField(
                TEXT("detailSourceMix"), DetailSourceMix) ||
            SourceNormalMix != 1.0 || DetailSourceMix != 1.0 ||
            !ReadExactBool(
                *NormalGeneration,
                TEXT("runtimeNormalStrengthAppliedDuringGeneration"),
                false,
                OutError) ||
            !ReadExactBool(
                *NormalGeneration,
                TEXT("runtimeDetailNormalStrengthAppliedDuringGeneration"),
                false,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A generated normal map already contains runtime strength.");
            }
            return false;
        }
    }
    return true;
}

bool ValidateV5NormalStrengthOwnership(
    const TSharedPtr<FJsonObject>& Contract,
    const TSharedPtr<FJsonObject>& Interface,
    const TSharedPtr<FJsonObject>& GeneratedManifest,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* ContractOwnership = nullptr;
    const TSharedPtr<FJsonObject>* InterfaceOwnership = nullptr;
    const TSharedPtr<FJsonObject>* GeneratedOwnership = nullptr;
    if (!Contract.IsValid() || !Interface.IsValid() ||
        !GeneratedManifest.IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("normalStrengthOwnership"), ContractOwnership) ||
        !ContractOwnership || !(*ContractOwnership).IsValid() ||
        !Interface->TryGetObjectField(
            TEXT("normalStrengthOwnership"), InterfaceOwnership) ||
        !InterfaceOwnership || !(*InterfaceOwnership).IsValid() ||
        !GeneratedManifest->TryGetObjectField(
            TEXT("normalStrengthOwnership"), GeneratedOwnership) ||
        !GeneratedOwnership || !(*GeneratedOwnership).IsValid())
    {
        OutError = TEXT("V5/V3 normal-strength ownership objects are absent.");
        return false;
    }

    const TArray<FString> GenerationFields = {
        TEXT("sourceNormalMix"), TEXT("detailSourceMix")};
    const TArray<FString> RuntimeFields = {
        TEXT("NormalStrength"), TEXT("DetailNormalStrength")};
    TArray<FString> ContractGeneration;
    TArray<FString> ContractRuntime;
    TArray<FString> InterfaceGeneration;
    TArray<FString> InterfaceRuntime;
    TArray<FString> GeneratedGeneration;
    TArray<FString> GeneratedRuntime;
    int64 RuntimeApplicationCount = 0;
    if (!ReadStringArray(
            *ContractOwnership,
            TEXT("generationOnlyFields"),
            ContractGeneration,
            OutError) ||
        !ReadStringArray(
            *ContractOwnership,
            TEXT("runtimeOnlyFields"),
            ContractRuntime,
            OutError) ||
        !ReadStringArray(
            *InterfaceOwnership,
            TEXT("mapGenerationFields"),
            InterfaceGeneration,
            OutError) ||
        !ReadStringArray(
            *InterfaceOwnership,
            TEXT("runtimeAmplitudeFields"),
            InterfaceRuntime,
            OutError) ||
        !ReadStringArray(
            *GeneratedOwnership,
            TEXT("mapGenerationFields"),
            GeneratedGeneration,
            OutError) ||
        !ReadStringArray(
            *GeneratedOwnership,
            TEXT("runtimeAmplitudeFields"),
            GeneratedRuntime,
            OutError) ||
        ContractGeneration != GenerationFields ||
        ContractRuntime != RuntimeFields ||
        InterfaceGeneration != GenerationFields ||
        InterfaceRuntime != RuntimeFields ||
        GeneratedGeneration != GenerationFields ||
        GeneratedRuntime != RuntimeFields ||
        !ReadExactBool(
            *ContractOwnership,
            TEXT("sameFieldMayDriveGenerationAndRuntime"),
            false,
            OutError) ||
        !ReadExactBool(
            *ContractOwnership,
            TEXT("inheritedGeneratedMapsHaveRuntimeStrengthApplied"),
            false,
            OutError) ||
        !ReadExactBool(
            *InterfaceOwnership,
            TEXT("sameFieldMayDriveGenerationAndRuntime"),
            false,
            OutError) ||
        !ReadExactBool(
            *InterfaceOwnership,
            TEXT("inheritedGeneratedMapsHaveRuntimeStrengthApplied"),
            false,
            OutError) ||
        !ReadExactBool(
            *GeneratedOwnership,
            TEXT("sameFieldMayDriveGenerationAndRuntime"),
            false,
            OutError) ||
        !ReadInteger(
            *InterfaceOwnership,
            TEXT("runtimeApplicationCount"),
            RuntimeApplicationCount,
            OutError) ||
        RuntimeApplicationCount != 1)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5 normal amplitude must be applied exactly once to runtime-neutral V3 maps.");
        }
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* GeneratedMaterials = nullptr;
    if (!GeneratedManifest->TryGetArrayField(
            TEXT("materials"), GeneratedMaterials) ||
        !GeneratedMaterials || GeneratedMaterials->Num() != 8)
    {
        OutError = TEXT("V3 generated normal ownership requires exactly eight material rows.");
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *GeneratedMaterials)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        const TSharedPtr<FJsonObject>* NormalGeneration = nullptr;
        double SourceNormalMix = 0.0;
        double DetailSourceMix = 0.0;
        if (!Row.IsValid() ||
            !Row->TryGetObjectField(
                TEXT("normalGeneration"), NormalGeneration) ||
            !NormalGeneration || !(*NormalGeneration).IsValid() ||
            !(*NormalGeneration)->TryGetNumberField(
                TEXT("sourceNormalMix"), SourceNormalMix) ||
            !(*NormalGeneration)->TryGetNumberField(
                TEXT("detailSourceMix"), DetailSourceMix) ||
            SourceNormalMix != 1.0 || DetailSourceMix != 1.0 ||
            !ReadExactBool(
                *NormalGeneration,
                TEXT("runtimeNormalStrengthAppliedDuringGeneration"),
                false,
                OutError) ||
            !ReadExactBool(
                *NormalGeneration,
                TEXT("runtimeDetailNormalStrengthAppliedDuringGeneration"),
                false,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("A referenced V3 normal map already contains V5 runtime strength.");
            }
            return false;
        }
    }
    return true;
}

bool ValidateNativeSourceChannelAudit(
    const TSharedPtr<FJsonObject>& Channel,
    const FString& SourceId,
    const FString& Role,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Audit = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* ChannelMinimums = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* ChannelMaximums = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* ChannelMeans = nullptr;
    FString OriginalMode;
    FString DecodedMode;
    FString NativeDtype;
    int64 SampleBits = 0;
    int64 NativeChannelCount = 0;
    double NativeMinimum = 0.0;
    double NativeMaximum = 0.0;
    double NativeMean = 0.0;
    if (!Channel.IsValid() ||
        !Channel->TryGetObjectField(TEXT("audit"), Audit) ||
        !Audit || !(*Audit).IsValid() ||
        !(*Audit)->TryGetStringField(TEXT("originalMode"), OriginalMode) ||
        OriginalMode.IsEmpty() ||
        !(*Audit)->TryGetStringField(TEXT("decodedMode"), DecodedMode) ||
        DecodedMode.IsEmpty() ||
        !(*Audit)->TryGetStringField(TEXT("nativeDtype"), NativeDtype) ||
        NativeDtype.IsEmpty() ||
        !ReadExactString(
            *Audit,
            TEXT("statisticsReductionPrecision"),
            TEXT("FLOAT64"),
            OutError) ||
        !ReadInteger(*Audit, TEXT("sampleBits"), SampleBits, OutError) ||
        (SampleBits != 8 && SampleBits != 16) ||
        !ReadInteger(
            *Audit,
            TEXT("nativeChannelCount"),
            NativeChannelCount,
            OutError) ||
        NativeChannelCount < 1 || NativeChannelCount > 4 ||
        !(*Audit)->TryGetNumberField(
            TEXT("nativeMinimumSample"), NativeMinimum) ||
        !(*Audit)->TryGetNumberField(
            TEXT("nativeMaximumSample"), NativeMaximum) ||
        !(*Audit)->TryGetNumberField(
            TEXT("nativeMeanSample"), NativeMean) ||
        !FMath::IsFinite(NativeMinimum) ||
        !FMath::IsFinite(NativeMaximum) ||
        !FMath::IsFinite(NativeMean) ||
        NativeMinimum > NativeMean || NativeMean > NativeMaximum ||
        !(*Audit)->TryGetArrayField(
            TEXT("nativeChannelMinimumSample"), ChannelMinimums) ||
        !(*Audit)->TryGetArrayField(
            TEXT("nativeChannelMaximumSample"), ChannelMaximums) ||
        !(*Audit)->TryGetArrayField(
            TEXT("nativeChannelMeanSample"), ChannelMeans) ||
        !ChannelMinimums || !ChannelMaximums || !ChannelMeans ||
        ChannelMinimums->Num() != NativeChannelCount ||
        ChannelMaximums->Num() != NativeChannelCount ||
        ChannelMeans->Num() != NativeChannelCount)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("Source channel '%s/%s' lacks an exact native scalar/FLOAT64 audit."),
                *SourceId,
                *Role);
        }
        return false;
    }
    for (int32 Index = 0; Index < NativeChannelCount; ++Index)
    {
        const TSharedPtr<FJsonValue>& MinimumValue = (*ChannelMinimums)[Index];
        const TSharedPtr<FJsonValue>& MaximumValue = (*ChannelMaximums)[Index];
        const TSharedPtr<FJsonValue>& MeanValue = (*ChannelMeans)[Index];
        if (!MinimumValue.IsValid() || !MaximumValue.IsValid() ||
            !MeanValue.IsValid() || MinimumValue->Type != EJson::Number ||
            MaximumValue->Type != EJson::Number ||
            MeanValue->Type != EJson::Number ||
            !FMath::IsFinite(MinimumValue->AsNumber()) ||
            !FMath::IsFinite(MaximumValue->AsNumber()) ||
            !FMath::IsFinite(MeanValue->AsNumber()) ||
            MinimumValue->AsNumber() > MeanValue->AsNumber() ||
            MeanValue->AsNumber() > MaximumValue->AsNumber())
        {
            OutError = FString::Printf(
                TEXT("Source channel '%s/%s' has unordered native channel statistics."),
                *SourceId,
                *Role);
            return false;
        }
    }
    if (SourceId == TEXT("Slate") && Role == TEXT("Height") &&
        (OriginalMode != TEXT("I;16") || DecodedMode != TEXT("I;16") ||
            NativeDtype != TEXT("uint16") || SampleBits != 16 ||
            NativeChannelCount != 1 || NativeMinimum != 17112.0 ||
            NativeMaximum != 36510.0))
    {
        OutError = TEXT("Slate Height must retain its exact native I;16 range instead of an 8-bit clipped decode.");
        return false;
    }
    return true;
}

bool ValidateSourceManifestRecords(
    const TSharedPtr<FJsonObject>& Manifest,
    const TSharedPtr<FJsonObject>& Contract,
    FString& OutError)
{
    if (!ReadExactString(
            Manifest,
            TEXT("schema"),
            TEXT("triad.istana_hero_material_sources.v3"),
            OutError) ||
        !ReadExactString(Manifest, TEXT("version"), TEXT("3.1.0"), OutError) ||
        !ReadExactString(Manifest, TEXT("provider"), TEXT("Poly Haven"), OutError) ||
        !ReadExactString(
            Manifest,
            TEXT("providerAssetLicense"),
            TEXT("CC0 1.0 Universal"),
            OutError) ||
        !ReadExactBool(Manifest, TEXT("exactRoster"), true, OutError) ||
        !ReadExactBool(Manifest, TEXT("siteMeasuredColor"), false, OutError) ||
        !ReadExactBool(Manifest, TEXT("siteMeasuredMaterialScan"), false, OutError) ||
        !ReadExactBool(Manifest, TEXT("sitePhotogrammetry"), false, OutError))
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* SourceInterface = nullptr;
    const TSharedPtr<FJsonObject>* NativeSampleAudit = nullptr;
    const TSharedPtr<FJsonObject>* SlateHeightInvariant = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* RequiredAssets = nullptr;
    TArray<FString> RequiredRoleArray;
    TArray<FString> OptionalRoleArray;
    TArray<FString> RequiredAuditFields;
    int64 SlateSampleBits = 0;
    int64 SlateMinimum = 0;
    int64 SlateMaximum = 0;
    if (!Contract.IsValid() ||
        !Contract->TryGetObjectField(TEXT("sourceInterface"), SourceInterface) ||
        !SourceInterface || !(*SourceInterface).IsValid() ||
        !(*SourceInterface)->TryGetArrayField(
            TEXT("requiredAssets"), RequiredAssets) ||
        !RequiredAssets || RequiredAssets->Num() < 1 ||
        !ReadStringArray(
            *SourceInterface,
            TEXT("requiredChannelRoles"),
            RequiredRoleArray,
            OutError) ||
        !ReadStringArray(
            *SourceInterface,
            TEXT("optionalChannelRoles"),
            OptionalRoleArray,
            OutError) ||
        !(*SourceInterface)->TryGetObjectField(
            TEXT("nativeSampleAudit"), NativeSampleAudit) ||
        !NativeSampleAudit || !(*NativeSampleAudit).IsValid() ||
        !ReadExactBool(
            *NativeSampleAudit,
            TEXT("preserveNativeScalarBitDepth"),
            true,
            OutError) ||
        !ReadExactString(
            *NativeSampleAudit,
            TEXT("statisticsReductionPrecision"),
            TEXT("FLOAT64"),
            OutError) ||
        !ReadExactBool(
            *NativeSampleAudit,
            TEXT("failUnlessEveryMeanWithinNativeExtrema"),
            true,
            OutError) ||
        !ReadStringArray(
            *NativeSampleAudit,
            TEXT("requiredAuditFields"),
            RequiredAuditFields,
            OutError) ||
        !(*NativeSampleAudit)->TryGetObjectField(
            TEXT("slateHeightInvariant"), SlateHeightInvariant) ||
        !SlateHeightInvariant || !(*SlateHeightInvariant).IsValid() ||
        !ReadExactString(
            *SlateHeightInvariant,
            TEXT("originalMode"),
            TEXT("I;16"),
            OutError) ||
        !ReadInteger(
            *SlateHeightInvariant,
            TEXT("sampleBits"),
            SlateSampleBits,
            OutError) || SlateSampleBits != 16 ||
        !ReadInteger(
            *SlateHeightInvariant,
            TEXT("nativeMinimumSample"),
            SlateMinimum,
            OutError) || SlateMinimum != 17112 ||
        !ReadInteger(
            *SlateHeightInvariant,
            TEXT("nativeMaximumSample"),
            SlateMaximum,
            OutError) || SlateMaximum != 36510 ||
        !ReadExactBool(
            *SlateHeightInvariant,
            TEXT("generatedHeightMustRespondToSourceHeightPerturbation"),
            true,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Hero material contract source interface is incomplete.");
        }
        return false;
    }
    if (!HasExactStringSet(
            RequiredAuditFields,
            TArray<FString>{
                TEXT("originalMode"), TEXT("sampleBits"),
                TEXT("nativeDtype"), TEXT("nativeMinimumSample"),
                TEXT("nativeMaximumSample"), TEXT("nativeMeanSample"),
                TEXT("nativeChannelMinimumSample"),
                TEXT("nativeChannelMaximumSample"),
                TEXT("nativeChannelMeanSample")}))
    {
        OutError = TEXT("Native source audit required-field roster changed.");
        return false;
    }
    const TSet<FString> RequiredChannelRoles(RequiredRoleArray);
    const TSet<FString> OptionalChannelRoles(OptionalRoleArray);
    if (RequiredChannelRoles.Num() != RequiredRoleArray.Num() ||
        OptionalChannelRoles.Num() != OptionalRoleArray.Num() ||
        !AreSetsEqual(
            RequiredChannelRoles,
            TSet<FString>{
                TEXT("Diffuse"), TEXT("NormalDX"),
                TEXT("Roughness"), TEXT("AO")}) ||
        !AreSetsEqual(
            OptionalChannelRoles,
            TSet<FString>{TEXT("Height")}))
    {
        OutError = TEXT("Hero material contract channel-role interface changed.");
        return false;
    }
    TMap<FString, FString> ExpectedProviders;
    for (const TSharedPtr<FJsonValue>& Value : *RequiredAssets)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FString Id;
        FString ProviderAssetId;
        if (!Row.IsValid() ||
            !Row->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty() ||
            !Row->TryGetStringField(
                TEXT("providerAssetId"), ProviderAssetId) ||
            ProviderAssetId.IsEmpty() || ExpectedProviders.Contains(Id))
        {
            OutError = TEXT("Hero material contract has an invalid or duplicate required source asset.");
            return false;
        }
        ExpectedProviders.Add(Id, ProviderAssetId);
    }
    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("assets"), Assets) ||
        !Assets || Assets->Num() != ExpectedProviders.Num())
    {
        OutError = TEXT("Hero material source manifest requires exactly four frozen Poly Haven assets.");
        return false;
    }
    TSet<FString> SeenIds;
    TSet<FString> SeenFiles;
    for (const TSharedPtr<FJsonValue>& Value : *Assets)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FString Id;
        FString ProviderAssetId;
        const TArray<TSharedPtr<FJsonValue>>* Channels = nullptr;
        if (!Row.IsValid() ||
            !Row->TryGetStringField(TEXT("id"), Id) ||
            !Row->TryGetStringField(TEXT("providerAssetId"), ProviderAssetId) ||
            ExpectedProviders.FindRef(Id) != ProviderAssetId ||
            SeenIds.Contains(Id) ||
            !Row->TryGetArrayField(TEXT("channels"), Channels) ||
            !Channels ||
            Channels->Num() < RequiredChannelRoles.Num() ||
            Channels->Num() >
                RequiredChannelRoles.Num() + OptionalChannelRoles.Num())
        {
            OutError = TEXT("Hero material source manifest contains an unsafe, duplicate, or unapproved asset row.");
            return false;
        }
        SeenIds.Add(Id);
        TSet<FString> SeenRoles;
        for (const TSharedPtr<FJsonValue>& ChannelValue : *Channels)
        {
            const TSharedPtr<FJsonObject> Channel = ChannelValue.IsValid()
                ? ChannelValue->AsObject()
                : nullptr;
            FString Role;
            FString Filename;
            FString Digest;
            int64 Bytes = 0;
            if (!Channel.IsValid() ||
                !Channel->TryGetStringField(TEXT("role"), Role) ||
                (!RequiredChannelRoles.Contains(Role) &&
                    !OptionalChannelRoles.Contains(Role)) ||
                SeenRoles.Contains(Role) ||
                !Channel->TryGetStringField(TEXT("file"), Filename) ||
                !Channel->TryGetStringField(TEXT("sha256"), Digest) ||
                !ReadInteger(Channel, TEXT("bytes"), Bytes, OutError) ||
                Filename != FPaths::GetCleanFilename(Filename) ||
                SeenFiles.Contains(Filename) || Digest.Len() != 64 ||
                !ValidateFrozenFile(
                    ProjectSourcePath(FPaths::Combine(
                        V3MaterialSourceRoot,
                        TEXT("Source/PolyHaven"),
                        Filename)),
                    Bytes,
                    Digest,
                    OutError) ||
                !ValidateNativeSourceChannelAudit(
                    Channel,
                    Id,
                    Role,
                    OutError))
            {
                if (OutError.IsEmpty())
                {
                    OutError = TEXT("Hero material source manifest contains an invalid channel record.");
                }
                return false;
            }
            SeenRoles.Add(Role);
            SeenFiles.Add(Filename);
        }
        if (!SeenRoles.Includes(RequiredChannelRoles) ||
            (SeenRoles.Num() != RequiredChannelRoles.Num() &&
                !SeenRoles.Includes(OptionalChannelRoles)))
        {
            OutError = FString::Printf(
                TEXT("Hero material source asset '%s' has the wrong exact channel roster."),
                *Id);
            return false;
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* ApiSnapshots = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("apiSnapshots"), ApiSnapshots) ||
        !ApiSnapshots || ApiSnapshots->Num() != ExpectedProviders.Num() * 2)
    {
        OutError = TEXT("Hero material source manifest requires exactly eight frozen provider API snapshots.");
        return false;
    }
    TSet<FString> SeenApiFiles;
    TSet<FString> SeenApiKeys;
    for (const TSharedPtr<FJsonValue>& Value : *ApiSnapshots)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FString ProviderAssetId;
        FString Role;
        FString Filename;
        FString Digest;
        int64 Bytes = 0;
        if (!Row.IsValid() ||
            !Row->TryGetStringField(TEXT("providerAssetId"), ProviderAssetId) ||
            !ExpectedProviders.FindKey(ProviderAssetId) ||
            !Row->TryGetStringField(TEXT("role"), Role) ||
            (Role != TEXT("files") && Role != TEXT("info")) ||
            SeenApiKeys.Contains(ProviderAssetId + TEXT("/") + Role) ||
            !Row->TryGetStringField(TEXT("file"), Filename) ||
            !Row->TryGetStringField(TEXT("sha256"), Digest) ||
            !ReadInteger(Row, TEXT("bytes"), Bytes, OutError) ||
            Filename != FPaths::GetCleanFilename(Filename) ||
            SeenApiFiles.Contains(Filename) || Digest.Len() != 64 ||
            !ValidateFrozenFile(
                ProjectSourcePath(FPaths::Combine(
                    V3MaterialSourceRoot,
                    TEXT("Provenance/PolyHavenApi"),
                    Filename)),
                Bytes,
                Digest,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Hero material source manifest contains an invalid provider API snapshot.");
            }
            return false;
        }
        SeenApiKeys.Add(ProviderAssetId + TEXT("/") + Role);
        SeenApiFiles.Add(Filename);
    }

    TArray<FString> DiskSources;
    IFileManager::Get().FindFiles(
        DiskSources,
        *ProjectSourcePath(FPaths::Combine(
            V3MaterialSourceRoot, TEXT("Source/PolyHaven/*"))),
        true,
        false);
    TArray<FString> DiskApiSnapshots;
    IFileManager::Get().FindFiles(
        DiskApiSnapshots,
        *ProjectSourcePath(FPaths::Combine(
            V3MaterialSourceRoot, TEXT("Provenance/PolyHavenApi/*"))),
        true,
        false);
    TSet<FString> DiskSourceSet;
    TSet<FString> DiskApiSnapshotSet;
    for (const FString& Filename : DiskSources)
    {
        DiskSourceSet.Add(Filename);
    }
    for (const FString& Filename : DiskApiSnapshots)
    {
        DiskApiSnapshotSet.Add(Filename);
    }
    if (!AreSetsEqual(DiskSourceSet, SeenFiles) ||
        !AreSetsEqual(DiskApiSnapshotSet, SeenApiFiles))
    {
        OutError = TEXT("Hero material frozen source/API directories differ from their exact manifest rosters.");
        return false;
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
            TEXT("triad.istana_hero_material_unreal_interface.v5"),
            OutError) ||
        !ReadExactString(Interface, TEXT("version"), TEXT("5.0.0"), OutError) ||
        !ReadExactString(
            Interface,
            TEXT("scope"),
            TEXT("BUILDING_HERO_VISUAL_V5_ONLY"),
            OutError) ||
        !ReadExactString(
            Interface,
            TEXT("contentRoot"),
            TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV5"),
            OutError) ||
        !ReadExactString(
            Interface,
            TEXT("textureContentRoot"),
            TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV3/Textures"),
            OutError) ||
        !ReadExactString(
            Interface,
            TEXT("integrationStatus"),
            TEXT("ADDITIVE_V5_MASTERS_AND_INSTANCES_REFERENCE_EXACT_FROZEN_V3_TEXTURE_PACKAGES_IN_PLACE_BOUND_TO_EXACT_FROZEN_V5_GEOMETRY"),
            OutError))
    {
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!Interface->TryGetArrayField(TEXT("instances"), Rows) ||
        !Rows || Rows->Num() != 9)
    {
        OutError = TEXT("Hero material interface requires exactly nine textured instances, including the distinct Door instance.");
        return false;
    }
    TSet<FString> SeenAssets;
    TSet<FString> SeenIds;
    TSet<FString> SeenSlots;
    TSet<FString> SeenTextureSurfaces;
    TMap<FString, int32> TextureSurfaceUseCounts;
    const TSet<FString> ExpectedTextureSurfaces = {
        TEXT("Render"), TEXT("Trim"), TEXT("Slate"), TEXT("Louvre"),
        TEXT("Stone"), TEXT("Timber"), TEXT("PaintedMetal"), TEXT("Glass")};
    const TMap<FString, FString> ExpectedInstanceTextureSurfaces = {
        {TEXT("Render"), TEXT("Render")},
        {TEXT("Trim"), TEXT("Trim")},
        {TEXT("Slate"), TEXT("Slate")},
        {TEXT("Louvre"), TEXT("Louvre")},
        {TEXT("Stone"), TEXT("Stone")},
        {TEXT("Timber"), TEXT("Timber")},
        {TEXT("Door"), TEXT("Trim")},
        {TEXT("PaintedMetal"), TEXT("PaintedMetal")},
        {TEXT("Glass"), TEXT("Glass")}};
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
            !Row->TryGetStringField(
                TEXT("textureSurfaceIdV3"), Spec.TextureSurfaceIdV3) ||
            ExpectedInstanceTextureSurfaces.FindRef(Spec.Id) !=
                Spec.TextureSurfaceIdV3 ||
            !ExpectedTextureSurfaces.Contains(Spec.TextureSurfaceIdV3) ||
            !ReadStringArray(Row, TEXT("replaceSlotsV5"), Spec.V5Slots, OutError) ||
            !Row->TryGetObjectField(TEXT("parameters"), Parameters) ||
            !Parameters || !(*Parameters).IsValid() ||
            !ParseParameters(*Parameters, Spec.Scalars, Spec.Vectors, OutError) ||
            Spec.Id.IsEmpty() || Spec.AssetName.IsEmpty() ||
            SeenIds.Contains(Spec.Id) ||
            SeenAssets.Contains(Spec.AssetName) || Spec.V5Slots.Num() < 1)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Hero material interface contains an invalid textured instance.");
            }
            return false;
        }
        SeenIds.Add(Spec.Id);
        SeenAssets.Add(Spec.AssetName);
        SeenTextureSurfaces.Add(Spec.TextureSurfaceIdV3);
        TextureSurfaceUseCounts.FindOrAdd(Spec.TextureSurfaceIdV3) += 1;
        for (const FString& Slot : Spec.V5Slots)
        {
            if (!ExpectedHeroSlots().Contains(Slot) ||
                SeenSlots.Contains(Slot) ||
                ExpectedSlotInstances().FindRef(Slot) != Spec.AssetName)
            {
                OutError = FString::Printf(
                    TEXT("Hero material interface has invalid or duplicate v5 slot alias '%s'."),
                    *Slot);
                return false;
            }
            SeenSlots.Add(Slot);
        }
        if (Spec.ParentAssetName != TEXT("M_IPV_HeroSurface_V5") &&
            Spec.ParentAssetName != TEXT("M_IPV_HeroGlass_V5"))
        {
            OutError = TEXT("Hero material instance parent escaped the two frozen v5 masters.");
            return false;
        }
        const TArray<FName> OpaqueScalarNames = {
            TEXT("TileMeters"), TEXT("DetailTileMeters"), TEXT("MacroTileMeters"),
            TEXT("NormalStrength"), TEXT("DetailNormalStrength"),
            TEXT("RoughnessBias"), TEXT("MacroAlbedoStrength"),
            TEXT("MacroRoughnessStrength"), TEXT("WeatheringStrength"),
            TEXT("SillDirtStrength"), TEXT("CorniceRunoffStrength"),
            TEXT("GroundContactDampStrength"), TEXT("CavityDirtStrength"),
            TEXT("HeightMillimetres"), TEXT("BumpOffsetStrength"),
            TEXT("ExposedMetalMaskStrength")};
        const TArray<FName> GlassScalarNames = {
            TEXT("TileMeters"), TEXT("DetailTileMeters"), TEXT("MacroTileMeters"),
            TEXT("NormalStrength"), TEXT("DetailNormalStrength"),
            TEXT("RoughnessBias"), TEXT("IOR"), TEXT("Opacity"),
            TEXT("DustStrength")};
        const TArray<FName> TintOnly = {TEXT("LookdevTint")};
        const bool bGlass = Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V5");
        if (!HasExactParameterNames(
                Spec.Scalars,
                bGlass ? GlassScalarNames : OpaqueScalarNames) ||
            !HasExactParameterNames(Spec.Vectors, TintOnly))
        {
            OutError = FString::Printf(
                TEXT("Hero material instance '%s' changed its exact v5 parameter roster."),
                *Spec.AssetName);
            return false;
        }
        Spec.StaticSwitches = {
            {TEXT("UseTextureSet"), true},
            {TEXT("UseBumpOffset"), false},
            {TEXT("UseArchitecturalDirtMasks"), false},
            {TEXT("UseExposedMetalMask"), false}};
        if (bGlass)
        {
            Spec.StaticSwitches.Reset();
        }
        OutInstances.Add(MoveTemp(Spec));
    }

    const TSharedPtr<FJsonObject>* SpecialBindings = nullptr;
    if (!Interface->TryGetObjectField(
            TEXT("specialSlotBindingsV5"), SpecialBindings) ||
        !SpecialBindings || !(*SpecialBindings).IsValid() ||
        (*SpecialBindings)->Values.Num() != 2)
    {
        OutError = TEXT("Hero material interface lacks special v5 slot bindings.");
        return false;
    }
    const TCHAR* SpecialRows[] = {
        TEXT("M_IPV5_Opaline"),
        TEXT("M_IPV5_Recess")};
    for (const TCHAR* SlotName : SpecialRows)
    {
        if (SeenSlots.Contains(SlotName))
        {
            OutError = FString::Printf(
                TEXT("Special v5 alias '%s' changed."),
                SlotName);
            return false;
        }
        const TSharedPtr<FJsonObject>* Binding = nullptr;
        FString AssetName;
        FString ParentAssetName;
        if (!(*SpecialBindings)->TryGetObjectField(SlotName, Binding) ||
            !Binding || !(*Binding).IsValid())
        {
            OutError = FString::Printf(
                TEXT("Special v5 binding '%s' is absent."),
                SlotName);
            return false;
        }
        FHeroMaterialInstanceSpec Spec;
        Spec.Id = SlotName;
        if (!(*Binding)->TryGetStringField(TEXT("asset"), AssetName) ||
            !(*Binding)->TryGetStringField(TEXT("parent"), ParentAssetName) ||
            AssetName != ExpectedSlotInstances().FindRef(SlotName) ||
            ParentAssetName != TEXT("M_IPV_HeroSurface_V5") ||
            SeenAssets.Contains(AssetName))
        {
            OutError = FString::Printf(
                TEXT("Special v5 binding '%s' changed its asset or parent."),
                SlotName);
            return false;
        }
        Spec.AssetName = AssetName;
        Spec.ParentAssetName = ParentAssetName;
        Spec.V5Slots = {SlotName};
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
                    SlotName);
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
        const TMap<FName, bool> RequiredSwitches = {
            {TEXT("UseTextureSet"), false},
            {TEXT("UseBumpOffset"), false},
            {TEXT("UseArchitecturalDirtMasks"), false},
            {TEXT("UseExposedMetalMask"), false}};
        if (Spec.StaticSwitches.OrderIndependentCompareEqual(RequiredSwitches) == false)
        {
            OutError = FString::Printf(
                TEXT("Special v5 binding '%s' changed its fail-closed static switches."),
                SlotName);
            return false;
        }
        SeenAssets.Add(AssetName);
        SeenSlots.Add(SlotName);
        OutInstances.Add(MoveTemp(Spec));
    }
    if (SeenSlots.Num() != ExpectedHeroSlots().Num() ||
        SeenIds.Num() != ExpectedInstanceTextureSurfaces.Num() ||
        SeenTextureSurfaces.Num() != ExpectedTextureSurfaces.Num() ||
        TextureSurfaceUseCounts.FindRef(TEXT("Trim")) != 2)
    {
        OutError = TEXT("Hero material interface does not bind all eleven M_IPV5 slots.");
        return false;
    }
    for (const FString& Surface : ExpectedTextureSurfaces)
    {
        const int32 ExpectedUseCount = Surface == TEXT("Trim") ? 2 : 1;
        if (TextureSurfaceUseCounts.FindRef(Surface) != ExpectedUseCount)
        {
            OutError = FString::Printf(
                TEXT("Hero material interface texture surface '%s' has an invalid V5 reuse count."),
                *Surface);
            return false;
        }
    }
    return true;
}

bool ValidateV5TextureReuseAndGraphContract(
    const TSharedPtr<FJsonObject>& Contract,
    const TSharedPtr<FJsonObject>& Interface,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* TextureContract = nullptr;
    const TSharedPtr<FJsonObject>* TextureReferences = nullptr;
    const TSharedPtr<FJsonObject>* GraphContract = nullptr;
    const TSharedPtr<FJsonObject>* Masters = nullptr;
    const TSharedPtr<FJsonObject>* Opaque = nullptr;
    const TSharedPtr<FJsonObject>* Glass = nullptr;
    int64 ContractReferenceCount = 0;
    int64 InterfaceReferenceCount = 0;
    int64 V5TexturePackageCount = -1;
    if (!Contract.IsValid() || !Interface.IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("textureReferenceContract"), TextureContract) ||
        !TextureContract || !(*TextureContract).IsValid() ||
        !Interface->TryGetObjectField(
            TEXT("textureReferences"), TextureReferences) ||
        !TextureReferences || !(*TextureReferences).IsValid() ||
        !Contract->TryGetObjectField(TEXT("graphContract"), GraphContract) ||
        !GraphContract || !(*GraphContract).IsValid() ||
        !Interface->TryGetObjectField(TEXT("masterMaterials"), Masters) ||
        !Masters || !(*Masters).IsValid() ||
        !(*Masters)->TryGetObjectField(TEXT("opaque"), Opaque) ||
        !Opaque || !(*Opaque).IsValid() ||
        !(*Masters)->TryGetObjectField(TEXT("glass"), Glass) ||
        !Glass || !(*Glass).IsValid() ||
        !ReadExactString(
            *TextureContract,
            TEXT("sourceContentRoot"),
            TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV3/Textures"),
            OutError) ||
        !ReadExactBool(
            *TextureContract,
            TEXT("destinationTexturePackagesAllowed"),
            false,
            OutError) ||
        !ReadInteger(
            *TextureContract,
            TEXT("referenceCount"),
            ContractReferenceCount,
            OutError) ||
        ContractReferenceCount != ExpectedMaterialTextureCount ||
        !ReadExactString(
            *TextureReferences,
            TEXT("sourceRoot"),
            TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV3/Textures"),
            OutError) ||
        !ReadInteger(
            *TextureReferences,
            TEXT("v5TexturePackageCount"),
            V5TexturePackageCount,
            OutError) ||
        V5TexturePackageCount != 0 ||
        !ReadInteger(
            *TextureReferences,
            TEXT("v3TextureReferenceCount"),
            InterfaceReferenceCount,
            OutError) ||
        InterfaceReferenceCount != ExpectedMaterialTextureCount ||
        !ReadExactBool(
            *TextureReferences,
            TEXT("noV5TextureCopy"),
            true,
            OutError) ||
        !ReadExactString(
            *GraphContract,
            TEXT("opaqueGraphSchema"),
            TEXT("triad.istana_hero_surface_graph.v5.1"),
            OutError) ||
        !ReadExactString(
            *GraphContract,
            TEXT("glassGraphSchema"),
            TEXT("triad.istana_hero_glass_graph.v5.2"),
            OutError) ||
        !ReadExactBool(
            *GraphContract,
            TEXT("glassScreenSpaceReflections"),
            true,
            OutError) ||
        !ReadExactBool(
            *GraphContract,
            TEXT("requiresGlobalRendererChange"),
            false,
            OutError) ||
        !ReadExactBool(
            *GraphContract,
            TEXT("requiresShaderModel6Change"),
            false,
            OutError) ||
        !ReadExactBool(
            *GraphContract,
            TEXT("requiresNaniteChange"),
            false,
            OutError) ||
        !ReadExactBool(
            *GraphContract,
            TEXT("graphSealMustIncludeScreenSpaceReflections"),
            true,
            OutError) ||
        !ReadExactString(
            *Opaque,
            TEXT("asset"),
            TEXT("M_IPV_HeroSurface_V5"),
            OutError) ||
        !ReadExactString(
            *Opaque,
            TEXT("graphSchema"),
            TEXT("triad.istana_hero_surface_graph.v5.1"),
            OutError) ||
        !ReadExactBool(
            *Opaque,
            TEXT("screenSpaceReflections"),
            false,
            OutError) ||
        !ReadExactString(
            *Glass,
            TEXT("asset"),
            TEXT("M_IPV_HeroGlass_V5"),
            OutError) ||
        !ReadExactString(
            *Glass,
            TEXT("graphSchema"),
            TEXT("triad.istana_hero_glass_graph.v5.2"),
            OutError) ||
        !ReadExactBool(
            *Glass,
            TEXT("screenSpaceReflections"),
            true,
            OutError) ||
        !ReadExactBool(
            *Glass,
            TEXT("requiresGlobalRendererChange"),
            false,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5 texture-reuse or material-local SSR graph contract changed.");
        }
        return false;
    }

    const TSharedPtr<FJsonValue> DestinationRoot =
        (*TextureReferences)->TryGetField(TEXT("destinationTextureRoot"));
    if (!DestinationRoot.IsValid() || DestinationRoot->Type != EJson::Null)
    {
        OutError = TEXT("V5 texture destination must remain JSON null; V5 texture packages are forbidden.");
        return false;
    }

    const TSharedPtr<FJsonObject>* Dependencies = nullptr;
    const TSharedPtr<FJsonObject>* Provenance = nullptr;
    if (!Contract->TryGetObjectField(
            TEXT("frozenV3Dependencies"), Dependencies) ||
        !Dependencies || !(*Dependencies).IsValid() ||
        !Interface->TryGetObjectField(TEXT("provenance"), Provenance) ||
        !Provenance || !(*Provenance).IsValid())
    {
        OutError = TEXT("V5 frozen-V3 dependency provenance is absent.");
        return false;
    }
    const auto DependencyHasDigest = [Dependencies, &OutError](
        const TCHAR* Field,
        const FString& ExpectedDigest)
    {
        const TSharedPtr<FJsonObject>* Record = nullptr;
        FString Digest;
        if (!(*Dependencies)->TryGetObjectField(Field, Record) ||
            !Record || !(*Record).IsValid() ||
            !(*Record)->TryGetStringField(TEXT("sha256"), Digest) ||
            Digest != ExpectedDigest)
        {
            OutError = FString::Printf(
                TEXT("V5 frozen dependency '%s' digest changed."), Field);
            return false;
        }
        return true;
    };
    if (!DependencyHasDigest(
            TEXT("materialContract"), FrozenV3MaterialContractSha256) ||
        !DependencyHasDigest(
            TEXT("unrealInterface"), FrozenV3MaterialInterfaceSha256) ||
        !DependencyHasDigest(
            TEXT("sourceManifest"), FrozenMaterialSourceManifestSha256) ||
        !DependencyHasDigest(
            TEXT("builder"), FrozenMaterialBuilderSha256) ||
        !DependencyHasDigest(
            TEXT("generatedManifest"),
            FrozenMaterialGeneratedManifestSha256) ||
        !ReadExactString(
            *Provenance,
            TEXT("frozenV3MaterialContractSha256"),
            *FrozenV3MaterialContractSha256,
            OutError) ||
        !ReadExactString(
            *Provenance,
            TEXT("frozenV3UnrealInterfaceSha256"),
            *FrozenV3MaterialInterfaceSha256,
            OutError) ||
        !ReadExactString(
            *Provenance,
            TEXT("frozenV3SourceManifestSha256"),
            *FrozenMaterialSourceManifestSha256,
            OutError) ||
        !ReadExactString(
            *Provenance,
            TEXT("frozenV3BuilderSha256"),
            *FrozenMaterialBuilderSha256,
            OutError) ||
        !ReadExactString(
            *Provenance,
            TEXT("frozenV3GeneratedManifestSha256"),
            *FrozenMaterialGeneratedManifestSha256,
            OutError))
    {
        return false;
    }
    return true;
}

bool ValidateFrozenV5GeometryDependencies(
    const TSharedPtr<FJsonObject>& Dependencies,
    FString& OutError)
{
    const TArray<FString> ExpectedFields = {
        TEXT("bindingStatus"),
        TEXT("geometryContract"),
        TEXT("geometryGenerator"),
        TEXT("geometryAudit"),
        TEXT("geometryManifest"),
        TEXT("geometryFreeze"),
        TEXT("heroObj")};
    if (!HasExactObjectFields(Dependencies, ExpectedFields, OutError) ||
        !ReadExactString(
            Dependencies,
            TEXT("bindingStatus"),
            TEXT("BOUND_FROZEN"),
            OutError))
    {
        return false;
    }
    struct FDependencyExpectation
    {
        const TCHAR* Field;
        const TCHAR* RelativePath;
        const FString* Sha256;
        int64 Bytes;
    };
    const FDependencyExpectation Expectations[] = {
        {TEXT("geometryContract"),
         TEXT("../../IstanaPublicViewV5/istana_public_view_hero_v5.contract.json"),
         &FrozenGeometryContractSha256,
         17665},
        {TEXT("geometryGenerator"),
         TEXT("../../IstanaPublicViewV5/generate_hero_v5.py"),
         &FrozenGeometryGeneratorSha256,
         92361},
        {TEXT("geometryAudit"),
         TEXT("../../IstanaPublicViewV5/v5_geometry_audit.py"),
         &FrozenGeometryAuditSha256,
         243136},
        {TEXT("geometryManifest"),
         TEXT("../../IstanaPublicViewV5/Generated/IstanaPublicViewV5Building.manifest.json"),
         &FrozenGeometryManifestSha256,
         35793},
        {TEXT("geometryFreeze"),
         TEXT("../../IstanaPublicViewV5/hero_v5.freeze.json"),
         &FrozenGeometryFreezeSha256,
         4377},
        {TEXT("heroObj"),
         TEXT("../../IstanaPublicViewV5/Generated/SM_IstanaPublicViewV5_Building_Hero.obj"),
         &FrozenGeometryObjSha256,
         ExpectedHeroBytes}};
    for (const FDependencyExpectation& Expected : Expectations)
    {
        const TSharedPtr<FJsonObject>* Row = nullptr;
        FString RelativePath;
        FString Sha256;
        int64 Bytes = 0;
        if (!Dependencies->TryGetObjectField(Expected.Field, Row) ||
            !Row || !(*Row).IsValid() ||
            !(*Row)->TryGetStringField(TEXT("relativePath"), RelativePath) ||
            RelativePath != Expected.RelativePath ||
            !(*Row)->TryGetStringField(TEXT("sha256"), Sha256) ||
            Sha256 != *Expected.Sha256 ||
            !ReadInteger(*Row, TEXT("bytes"), Bytes, OutError) ||
            Bytes != Expected.Bytes)
        {
            OutError = FString::Printf(
                TEXT("Frozen V5 geometry dependency '%s' changed."),
                Expected.Field);
            return false;
        }
    }
    const TSharedPtr<FJsonObject>* Manifest = nullptr;
    const TSharedPtr<FJsonObject>* Hero = nullptr;
    int64 Triangles = 0;
    int64 Vertices = 0;
    int64 Groups = 0;
    if (!Dependencies->TryGetObjectField(TEXT("geometryManifest"), Manifest) ||
        !Manifest || !(*Manifest).IsValid() ||
        !ReadExactString(
            *Manifest,
            TEXT("semanticSha256"),
            *FrozenGeometryManifestSemanticSha256,
            OutError) ||
        !Dependencies->TryGetObjectField(TEXT("heroObj"), Hero) ||
        !Hero || !(*Hero).IsValid() ||
        !ReadInteger(*Hero, TEXT("triangles"), Triangles, OutError) ||
        Triangles != ExpectedHeroTriangles ||
        !ReadInteger(*Hero, TEXT("vertices"), Vertices, OutError) ||
        Vertices != ExpectedHeroVertices ||
        !ReadInteger(*Hero, TEXT("groups"), Groups, OutError) ||
        Groups != ExpectedHeroGroups)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Frozen V5 geometry manifest semantic digest or mesh census changed.");
        }
        return false;
    }
    return true;
}

bool ValidateMaterialSource(
    FHeroMaterialPack& OutPack,
    FString& OutReport)
{
    if (FrozenMaterialFreezeSha256 == PendingSha256 ||
        FrozenMaterialSourceManifestSha256 == PendingSha256 ||
        FrozenMaterialContractSha256 == PendingSha256 ||
        FrozenMaterialInterfaceSha256 == PendingSha256 ||
        FrozenMaterialBuilderSha256 == PendingSha256 ||
        FrozenMaterialGeneratedManifestSha256 == PendingSha256)
    {
        OutReport = TEXT("V5_MATERIALS_NOT_FROZEN: final byte-identical HeroMaterialsV5 artifacts have not been bound.");
        return false;
    }
    FString Error;
    TSharedPtr<FJsonObject> Contract;
    TSharedPtr<FJsonObject> Interface;
    TSharedPtr<FJsonObject> MaterialFreeze;
    TSharedPtr<FJsonObject> V3Contract;
    TSharedPtr<FJsonObject> SourceManifest;
    TSharedPtr<FJsonObject> GeneratedManifest;
    TArray<FString> MaterialFreezeOrderedSlots;
    int64 MaterialFreezeReferencedV3TextureCount = -1;
    int64 MaterialFreezeCreatedV5TextureCount = -1;
    int64 MaterialFreezeTexturedInstanceCount = -1;
    int64 MaterialFreezeTotalInstanceCount = -1;
    if (!ValidateFrozenFile(
            ProjectSourcePath(MaterialContractRelativePath),
            30825,
            FrozenMaterialContractSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(MaterialInterfaceRelativePath),
            42172,
            FrozenMaterialInterfaceSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(MaterialFreezeRelativePath),
            5415,
            FrozenMaterialFreezeSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(MaterialValidatorRelativePath),
            71330,
            FrozenMaterialValidatorSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(MaterialEvaluatorRelativePath),
            19474,
            FrozenMaterialEvaluatorSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(MaterialReadmeRelativePath),
            11396,
            FrozenMaterialReadmeSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(MaterialContractTestRelativePath),
            18884,
            FrozenMaterialContractTestSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(V3MaterialContractRelativePath),
            15312,
            FrozenV3MaterialContractSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(V3MaterialInterfaceRelativePath),
            12710,
            FrozenV3MaterialInterfaceSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(MaterialSourceManifestRelativePath),
            31731,
            FrozenMaterialSourceManifestSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(MaterialBuilderRelativePath),
            93357,
            FrozenMaterialBuilderSha256,
            Error) ||
        !ValidateFrozenFile(
            ProjectSourcePath(MaterialGeneratedManifestRelativePath),
            213225,
            FrozenMaterialGeneratedManifestSha256,
            Error) ||
        !LoadJsonObject(ProjectSourcePath(MaterialContractRelativePath), Contract, Error) ||
        !LoadJsonObject(ProjectSourcePath(MaterialInterfaceRelativePath), Interface, Error) ||
        !LoadJsonObject(ProjectSourcePath(MaterialFreezeRelativePath), MaterialFreeze, Error) ||
        !LoadJsonObject(ProjectSourcePath(V3MaterialContractRelativePath), V3Contract, Error) ||
        !LoadJsonObject(ProjectSourcePath(MaterialSourceManifestRelativePath), SourceManifest, Error) ||
        !LoadJsonObject(ProjectSourcePath(MaterialGeneratedManifestRelativePath), GeneratedManifest, Error) ||
        !ReadExactString(
            Contract,
            TEXT("schema"),
            TEXT("triad.istana_hero_materials.v5"),
            Error) ||
        !ReadExactString(Contract, TEXT("version"), TEXT("5.0.0"), Error) ||
        !ReadExactString(
            MaterialFreeze,
            TEXT("schema"),
            TEXT("triad.istana_hero_materials_v5.freeze.v1"),
            Error) ||
        !ReadExactString(
            MaterialFreeze,
            TEXT("version"),
            TEXT("5.0.0"),
            Error) ||
        !ReadExactString(
            MaterialFreeze,
            TEXT("geometryBindingStatus"),
            TEXT("BOUND_FROZEN"),
            Error) ||
        !ReadExactString(
            MaterialFreeze,
            TEXT("freezeStatus"),
            TEXT("FROZEN_AFTER_EXACT_V5_GEOMETRY_V3_TEXTURE_SEMANTIC_AND_NEGATIVE_CONTROL_VALIDATION"),
            Error) ||
        !ReadExactString(
            MaterialFreeze,
            TEXT("claimStatus"),
            TEXT("PUBLIC_REFERENCE_QUALITATIVE_LOOKDEV_NOT_SITE_MEASURED_OR_COLOR_CALIBRATED"),
            Error) ||
        !ReadInteger(
            MaterialFreeze,
            TEXT("referencedFrozenV3UTexturePackageCount"),
            MaterialFreezeReferencedV3TextureCount,
            Error) ||
        MaterialFreezeReferencedV3TextureCount != ExpectedMaterialTextureCount ||
        !ReadInteger(
            MaterialFreeze,
            TEXT("createdV5TexturePackageCount"),
            MaterialFreezeCreatedV5TextureCount,
            Error) ||
        MaterialFreezeCreatedV5TextureCount != 0 ||
        !ReadInteger(
            MaterialFreeze,
            TEXT("texturedInstanceCount"),
            MaterialFreezeTexturedInstanceCount,
            Error) ||
        MaterialFreezeTexturedInstanceCount != 9 ||
        !ReadInteger(
            MaterialFreeze,
            TEXT("totalMaterialInstanceCount"),
            MaterialFreezeTotalInstanceCount,
            Error) ||
        MaterialFreezeTotalInstanceCount != ExpectedHeroSlots().Num() ||
        !ReadStringArray(
            MaterialFreeze,
            TEXT("requiredV5SlotOrder"),
            MaterialFreezeOrderedSlots,
            Error) ||
        MaterialFreezeOrderedSlots != ExpectedHeroSlots())
    {
        OutReport = TEXT("V5_MATERIAL_CONTRACT_INVALID: ") + Error;
        return false;
    }
    const TSharedPtr<FJsonObject>* Scope = nullptr;
    const TSharedPtr<FJsonObject>* RuntimeRequirements = nullptr;
    const TSharedPtr<FJsonObject>* GeometryPartition = nullptr;
    const TSharedPtr<FJsonObject>* SemanticRoleMapping = nullptr;
    const TSharedPtr<FJsonObject>* InterfaceGeometryBinding = nullptr;
    const TSharedPtr<FJsonObject>* ContractGeometryDependencies = nullptr;
    const TSharedPtr<FJsonObject>* FreezeGeometryDependencies = nullptr;
    const TSharedPtr<FJsonObject>* FreezeGeometryCensus = nullptr;
    const TSharedPtr<FJsonObject>* FreezeNegativeControl = nullptr;
    TArray<FString> ContractOrderedSlots;
    TArray<FString> InterfaceOrderedSlots;
    TArray<FString> InterfaceEffectiveSlots;
    TArray<FString> GeometryPartitionSlots;
    TArray<FString> InterfaceGeometrySlots;
    TArray<FString> NegativeControlFailedGates;
    int64 GeneratedOrCopiedTextureCount = -1;
    int64 ReferencedFrozenTextureCount = -1;
    int64 BoundHeroTriangles = 0;
    int64 BoundHeroVertices = 0;
    int64 BoundHeroGroups = 0;
    int64 FrozenHeroTriangles = 0;
    int64 FrozenHeroVertices = 0;
    int64 FrozenHeroGroups = 0;
    int64 FrozenV3PngPayloadBytes = 0;
    int64 ContractDoorClosureGroups = 0;
    int64 ContractDoorClosureTriangles = 0;
    int64 ContractTerminalSeams = 0;
    int64 ContractClosureMisses = -1;
    int64 ContractTransparentFirstMissing = -1;
    int64 BoundDoorClosureGroups = 0;
    int64 BoundDoorClosureTriangles = 0;
    int64 BoundTerminalSeams = 0;
    int64 BoundClosureMisses = -1;
    int64 BoundTransparentFirstMissing = -1;
    int64 ContractAuthoredOpalineTriangles = -1;
    int64 BoundAuthoredOpalineTriangles = -1;
    if (!Contract->TryGetObjectField(TEXT("scope"), Scope) ||
        !Scope || !(*Scope).IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("geometrySemanticPartition"), GeometryPartition) ||
        !GeometryPartition || !(*GeometryPartition).IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("semanticV4ToV5SlotMapping"), SemanticRoleMapping) ||
        !SemanticRoleMapping || !(*SemanticRoleMapping).IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("frozenV5GeometryDependencies"),
            ContractGeometryDependencies) ||
        !ContractGeometryDependencies ||
        !(*ContractGeometryDependencies).IsValid() ||
        !Interface->TryGetObjectField(
            TEXT("runtimeRequirements"), RuntimeRequirements) ||
        !RuntimeRequirements || !(*RuntimeRequirements).IsValid() ||
        !Interface->TryGetObjectField(
            TEXT("geometryBinding"), InterfaceGeometryBinding) ||
        !InterfaceGeometryBinding || !(*InterfaceGeometryBinding).IsValid() ||
        !MaterialFreeze->TryGetObjectField(
            TEXT("geometryDependencies"), FreezeGeometryDependencies) ||
        !FreezeGeometryDependencies || !(*FreezeGeometryDependencies).IsValid() ||
        !MaterialFreeze->TryGetObjectField(
            TEXT("geometryMeshCensus"), FreezeGeometryCensus) ||
        !FreezeGeometryCensus || !(*FreezeGeometryCensus).IsValid() ||
        !MaterialFreeze->TryGetObjectField(
            TEXT("neutralCaptureNegativeControl"), FreezeNegativeControl) ||
        !FreezeNegativeControl || !(*FreezeNegativeControl).IsValid() ||
        !ValidateFrozenV5GeometryDependencies(
            *ContractGeometryDependencies, Error) ||
        !ValidateFrozenV5GeometryDependencies(
            *FreezeGeometryDependencies, Error) ||
        !ReadInteger(
            MaterialFreeze,
            TEXT("frozenV3PngPayloadBytes"),
            FrozenV3PngPayloadBytes,
            Error) ||
        FrozenV3PngPayloadBytes != 551260402 ||
        !ReadExactString(
            MaterialFreeze,
            TEXT("geometryManifestSemanticSha256"),
            *FrozenGeometryManifestSemanticSha256,
            Error) ||
        !ReadInteger(
            *FreezeGeometryCensus,
            TEXT("triangles"),
            FrozenHeroTriangles,
            Error) ||
        FrozenHeroTriangles != ExpectedHeroTriangles ||
        !ReadInteger(
            *FreezeGeometryCensus,
            TEXT("vertices"),
            FrozenHeroVertices,
            Error) ||
        FrozenHeroVertices != ExpectedHeroVertices ||
        !ReadInteger(
            *FreezeGeometryCensus,
            TEXT("groups"),
            FrozenHeroGroups,
            Error) ||
        FrozenHeroGroups != ExpectedHeroGroups ||
        !ReadExactString(
            *FreezeNegativeControl,
            TEXT("method"),
            TEXT("FROZEN_V4_METRICS_AS_BASELINE_AND_CANDIDATE"),
            Error) ||
        !ReadExactBool(
            *FreezeNegativeControl,
            TEXT("baselineReproductionPassed"),
            true,
            Error) ||
        !ReadExactBool(
            *FreezeNegativeControl,
            TEXT("candidateAcceptancePassed"),
            false,
            Error) ||
        !ReadStringArray(
            *FreezeNegativeControl,
            TEXT("expectedFailedGates"),
            NegativeControlFailedGates,
            Error) ||
        NegativeControlFailedGates != TArray<FString>({
            TEXT("louvreBothMetricsImprovedInAtLeast5Views"),
            TEXT("louvreEdgeDensityAtLeast26"),
            TEXT("louvreLaplacianAtLeast0.025"),
            TEXT("macroMedianLumaBetween0.54And0.64"),
            TEXT("roofBothMetricsImprovedInAtLeast5Views"),
            TEXT("roofEdgeDensityAtLeast16"),
            TEXT("roofLaplacianAtLeast0.016")}) ||
        !ReadStringArray(
            Contract,
            TEXT("requiredV5SlotOrder"),
            ContractOrderedSlots,
            Error) ||
        !ReadStringArray(
            Interface,
            TEXT("orderedHeroSlotsV5"),
            InterfaceOrderedSlots,
            Error) ||
        !ReadStringArray(
            Interface,
            TEXT("effectiveV5SlotOrder"),
            InterfaceEffectiveSlots,
            Error) ||
        ContractOrderedSlots != ExpectedHeroSlots() ||
        InterfaceOrderedSlots != ExpectedHeroSlots() ||
        InterfaceEffectiveSlots != ExpectedHeroSlots() ||
        !ReadExactString(
            Contract,
            TEXT("claimStatus"),
            TEXT("PUBLIC_REFERENCE_QUALITATIVE_LOOKDEV_NOT_SITE_MEASURED_OR_COLOR_CALIBRATED"),
            Error) ||
        !ReadExactString(
            Interface,
            TEXT("claimStatus"),
            TEXT("PUBLIC_REFERENCE_QUALITATIVE_LOOKDEV_NOT_SITE_MEASURED_OR_COLOR_CALIBRATED"),
            Error) ||
        !ReadExactBool(*Scope, TEXT("heroBuildingVisualOnly"), true, Error) ||
        !ReadExactBool(
            *Scope,
            TEXT("additiveV5MaterialNamespaceOnly"),
            true,
            Error) ||
        !ReadExactString(
            *Scope,
            TEXT("contentRoot"),
            TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV5"),
            Error) ||
        !ReadExactBool(
            *Scope,
            TEXT("reusesFrozenV3TextureAssetsInPlace"),
            true,
            Error) ||
        !ReadInteger(
            *Scope,
            TEXT("generatedOrCopiedTextureCountV5"),
            GeneratedOrCopiedTextureCount,
            Error) ||
        GeneratedOrCopiedTextureCount != 0 ||
        !ReadInteger(
            *Scope,
            TEXT("referencedFrozenTextureCountV3"),
            ReferencedFrozenTextureCount,
            Error) ||
        ReferencedFrozenTextureCount != ExpectedMaterialTextureCount ||
        !ReadExactBool(
            *Scope, TEXT("changesFrozenV3TexturePixels"), false, Error) ||
        !ReadExactBool(
            *Scope, TEXT("changesFrozenV3TexturePackages"), false, Error) ||
        !ReadExactBool(
            *Scope, TEXT("changesV1V2V3V4MaterialPackages"), false, Error) ||
        !ReadExactBool(
            *Scope, TEXT("changesV1V2V3V4GeometryPackages"), false, Error) ||
        !ReadExactBool(*Scope, TEXT("changesSurroundings"), false, Error) ||
        !ReadExactBool(*Scope, TEXT("changesWorldLighting"), false, Error) ||
        !ReadExactBool(
            *Scope, TEXT("changesRendererOrProjectSettings"), false, Error) ||
        !ReadExactBool(
            *Scope,
            TEXT("changesCollisionOrSensorTruth"),
            false,
            Error) ||
        !ReadExactBool(*Scope, TEXT("siteMeasuredColourClaimed"), false, Error) ||
        !ReadExactBool(*Scope, TEXT("siteMeasuredMaterialClaimed"), false, Error) ||
        !ReadExactBool(*Scope, TEXT("photogrammetryClaimed"), false, Error) ||
        !ReadExactBool(*Scope, TEXT("oneToOneDigitalTwinClaimed"), false, Error) ||
        (*SemanticRoleMapping)->Values.Num() != ExpectedHeroSlots().Num() ||
        !ReadExactString(
            *GeometryPartition,
            TEXT("bindingStatus"),
            TEXT("BOUND_FROZEN"),
            Error) ||
        !ReadExactString(
            *GeometryPartition,
            TEXT("sourceGeometryGeneration"),
            *FrozenGeometrySourceClass,
            Error) ||
        !ReadStringArray(
            *GeometryPartition,
            TEXT("materialSlots"),
            GeometryPartitionSlots,
            Error) ||
        GeometryPartitionSlots != ExpectedHeroSlots() ||
        !ReadExactBool(
            *GeometryPartition,
            TEXT("roleOrderMapsOneToOneFromV4"),
            true,
            Error) ||
        !ReadExactString(
            *GeometryPartition,
            TEXT("retainedDeepDoorClosureFieldV5Material"),
            TEXT("M_IPV5_Door"),
            Error) ||
        !ReadExactString(
            *GeometryPartition,
            TEXT("authoredUpperTowerOpaqueScreenBackingRole"),
            TEXT("M_IPV5_Door"),
            Error) ||
        !ReadExactBool(
            *GeometryPartition,
            TEXT("broadAuthoredOpalineBackingAllowed"),
            false,
            Error) ||
        !ReadInteger(
            *GeometryPartition,
            TEXT("retainedDeepDoorClosureFieldGroupCount"),
            ContractDoorClosureGroups,
            Error) ||
        ContractDoorClosureGroups != 93 ||
        !ReadInteger(
            *GeometryPartition,
            TEXT("retainedDeepDoorClosureFieldTriangleCount"),
            ContractDoorClosureTriangles,
            Error) ||
        ContractDoorClosureTriangles != 1116 ||
        !ReadInteger(
            *GeometryPartition,
            TEXT("authoredOpalineTriangleCount"),
            ContractAuthoredOpalineTriangles,
            Error) ||
        ContractAuthoredOpalineTriangles != 0 ||
        !ReadInteger(
            *GeometryPartition,
            TEXT("v5TerminalRecessSeamFeatureCount"),
            ContractTerminalSeams,
            Error) ||
        ContractTerminalSeams != 15 ||
        !ReadInteger(
            *GeometryPartition,
            TEXT("publicViewClosureMissCount"),
            ContractClosureMisses,
            Error) ||
        ContractClosureMisses != 0 ||
        !ReadInteger(
            *GeometryPartition,
            TEXT("transparentFirstOpaqueMissingCount"),
            ContractTransparentFirstMissing,
            Error) ||
        ContractTransparentFirstMissing != 0 ||
        !ReadExactBool(
            *GeometryPartition,
            TEXT("newOpeningBackingsOpaque"),
            true,
            Error) ||
        !ReadExactBool(
            *GeometryPartition,
            TEXT("nearBlackOpeningSizedCardsAllowed"),
            false,
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("bindingStatus"),
            TEXT("BOUND_FROZEN"),
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("geometryContractSha256"),
            *FrozenGeometryContractSha256,
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("geometryGeneratorSha256"),
            *FrozenGeometryGeneratorSha256,
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("geometryAuditSha256"),
            *FrozenGeometryAuditSha256,
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("geometryManifestSha256"),
            *FrozenGeometryManifestSha256,
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("geometryManifestSemanticSha256"),
            *FrozenGeometryManifestSemanticSha256,
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("geometryFreezeSha256"),
            *FrozenGeometryFreezeSha256,
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("heroObjSha256"),
            *FrozenGeometryObjSha256,
            Error) ||
        !ReadInteger(
            *InterfaceGeometryBinding,
            TEXT("heroObjTriangles"),
            BoundHeroTriangles,
            Error) ||
        BoundHeroTriangles != ExpectedHeroTriangles ||
        !ReadInteger(
            *InterfaceGeometryBinding,
            TEXT("heroObjVertices"),
            BoundHeroVertices,
            Error) ||
        BoundHeroVertices != ExpectedHeroVertices ||
        !ReadInteger(
            *InterfaceGeometryBinding,
            TEXT("heroObjGroups"),
            BoundHeroGroups,
            Error) ||
        BoundHeroGroups != ExpectedHeroGroups ||
        !ReadInteger(
            *InterfaceGeometryBinding,
            TEXT("retainedDeepDoorClosureFieldGroupCount"),
            BoundDoorClosureGroups,
            Error) ||
        BoundDoorClosureGroups != ContractDoorClosureGroups ||
        !ReadInteger(
            *InterfaceGeometryBinding,
            TEXT("retainedDeepDoorClosureFieldTriangleCount"),
            BoundDoorClosureTriangles,
            Error) ||
        BoundDoorClosureTriangles != ContractDoorClosureTriangles ||
        !ReadInteger(
            *InterfaceGeometryBinding,
            TEXT("authoredOpalineTriangleCount"),
            BoundAuthoredOpalineTriangles,
            Error) ||
        BoundAuthoredOpalineTriangles != ContractAuthoredOpalineTriangles ||
        !ReadInteger(
            *InterfaceGeometryBinding,
            TEXT("v5TerminalRecessSeamFeatureCount"),
            BoundTerminalSeams,
            Error) ||
        BoundTerminalSeams != ContractTerminalSeams ||
        !ReadInteger(
            *InterfaceGeometryBinding,
            TEXT("publicViewClosureMissCount"),
            BoundClosureMisses,
            Error) ||
        BoundClosureMisses != ContractClosureMisses ||
        !ReadInteger(
            *InterfaceGeometryBinding,
            TEXT("transparentFirstOpaqueMissingCount"),
            BoundTransparentFirstMissing,
            Error) ||
        BoundTransparentFirstMissing != ContractTransparentFirstMissing ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("sourceGeometryGeneration"),
            *FrozenGeometrySourceClass,
            Error) ||
        !ReadStringArray(
            *InterfaceGeometryBinding,
            TEXT("materialSlots"),
            InterfaceGeometrySlots,
            Error) ||
        InterfaceGeometrySlots != ExpectedHeroSlots() ||
        !ReadExactBool(
            *InterfaceGeometryBinding,
            TEXT("roleOrderMapsOneToOneFromV4"),
            true,
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("retainedDeepDoorClosureFieldV5Material"),
            TEXT("M_IPV5_Door"),
            Error) ||
        !ReadExactString(
            *InterfaceGeometryBinding,
            TEXT("authoredUpperTowerOpaqueScreenBackingRole"),
            TEXT("M_IPV5_Door"),
            Error) ||
        !ReadExactBool(
            *InterfaceGeometryBinding,
            TEXT("broadAuthoredOpalineBackingAllowed"),
            false,
            Error) ||
        !ReadExactBool(
            *InterfaceGeometryBinding,
            TEXT("newOpeningBackingsOpaque"),
            true,
            Error) ||
        !ReadExactBool(
            *InterfaceGeometryBinding,
            TEXT("nearBlackOpeningSizedCardsAllowed"),
            false,
            Error) ||
        !ReadExactString(
            MaterialFreeze,
            TEXT("geometryBindingStatus"),
            TEXT("BOUND_FROZEN"),
            Error) ||
        !ReadExactString(
            *RuntimeRequirements,
            TEXT("minimumEngine"),
            TEXT("5.5"),
            Error) ||
        !ReadExactBool(
            *RuntimeRequirements,
            TEXT("requiresShaderModel6"),
            false,
            Error) ||
        !ReadExactBool(
            *RuntimeRequirements,
            TEXT("requiresNanite"),
            false,
            Error) ||
        !ReadExactBool(
            *RuntimeRequirements,
            TEXT("requiresVirtualTextures"),
            false,
            Error) ||
        !ReadExactBool(
            *RuntimeRequirements,
            TEXT("changesCollisionOrSensorTruth"),
            false,
            Error) ||
        !ReadExactBool(
            *RuntimeRequirements,
            TEXT("changesWorldLighting"),
            false,
            Error) ||
        !ReadExactBool(
            *RuntimeRequirements,
            TEXT("changesSurroundings"),
            false,
            Error) ||
        !ReadExactBool(
            *RuntimeRequirements,
            TEXT("changesGlobalRendererSettings"),
            false,
            Error))
    {
        OutReport = TEXT("V5_MATERIAL_CONTRACT_INVALID: hero-only scope or exact ordered slot roster changed. ") + Error;
        return false;
    }
    for (int32 Index = 0; Index < ExpectedHeroSlots().Num(); ++Index)
    {
        if (!ReadExactString(
                *SemanticRoleMapping,
                *ExpectedV4SourceSlots()[Index],
                *ExpectedHeroSlots()[Index],
                Error))
        {
            OutReport = TEXT("V5_MATERIAL_CONTRACT_INVALID: exact V4-to-V5 semantic slot mapping changed. ") + Error;
            return false;
        }
    }
    TArray<FString> GeneratedOrderedV3Slots;
    if (!ValidateV5TextureReuseAndGraphContract(
            Contract, Interface, Error) ||
        !ValidateSourceManifestRecords(SourceManifest, V3Contract, Error) ||
        !ParseMaterialInterface(Interface, OutPack.Instances, Error) ||
        !ValidateV5NormalStrengthOwnership(
            Contract,
            Interface,
            GeneratedManifest,
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("schema"),
            TEXT("triad.istana_hero_material_generated_pack.v3"),
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("version"),
            TEXT("3.2.0"),
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("algorithmVersion"),
            TEXT("triad.istana_hero_pbr_builder.v3.4.0"),
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("scope"),
            TEXT("BUILDING_HERO_VISUAL_ONLY"),
            Error) ||
        !ReadStringArray(
            GeneratedManifest,
            TEXT("orderedHeroSlotsV3"),
            GeneratedOrderedV3Slots,
            Error) ||
        GeneratedOrderedV3Slots != ExpectedV3SourceSlots() ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("sourceManifestSha256"),
            *FrozenMaterialSourceManifestSha256,
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("contractSha256"),
            *FrozenV3MaterialContractSha256,
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("unrealAssetInterfaceSha256"),
            *FrozenV3MaterialInterfaceSha256,
            Error) ||
        !ReadExactString(
            GeneratedManifest,
            TEXT("builderSha256"),
            *FrozenMaterialBuilderSha256,
            Error) ||
        !ReadExactBool(GeneratedManifest, TEXT("siteMeasuredMaterialScan"), false, Error) ||
        !ReadExactBool(GeneratedManifest, TEXT("siteMeasuredColor"), false, Error) ||
        !ReadExactBool(GeneratedManifest, TEXT("sitePhotogrammetry"), false, Error) ||
        !ReadExactBool(
            GeneratedManifest,
            TEXT("siteSpecificWeatheringPlacement"),
            false,
            Error))
    {
        OutReport = TEXT("V5_MATERIAL_PACK_INVALID: ") + Error;
        return false;
    }
    const TSharedPtr<FJsonObject>* Resolutions = nullptr;
    int64 TileResolution = 0;
    int64 DetailResolution = 0;
    int64 MacroResolution = 0;
    int64 MaskResolution = 0;
    int64 DirtResolution = 0;
    if (!GeneratedManifest->TryGetObjectField(TEXT("resolutions"), Resolutions) ||
        !Resolutions || !(*Resolutions).IsValid() ||
        !ReadInteger(*Resolutions, TEXT("tile"), TileResolution, Error) ||
        !ReadInteger(*Resolutions, TEXT("detail"), DetailResolution, Error) ||
        !ReadInteger(*Resolutions, TEXT("macro"), MacroResolution, Error) ||
        !ReadInteger(*Resolutions, TEXT("mask"), MaskResolution, Error) ||
        !ReadInteger(*Resolutions, TEXT("dirt"), DirtResolution, Error) ||
        TileResolution != 4096 || DetailResolution != 2048 ||
        MacroResolution != 1024 || MaskResolution != 2048 ||
        DirtResolution != 2048)
    {
        OutReport = TEXT("V5_MATERIAL_PACK_INVALID: exact generated resolutions changed. ") + Error;
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
        OutReport = TEXT("V5_MATERIAL_PACK_INVALID: generated material roster is incomplete.");
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
        TArray<FString> GeneratedV3Slots;
        const TSharedPtr<FJsonObject>* Outputs = nullptr;
        if (!Row.IsValid() ||
            !Row->TryGetStringField(TEXT("id"), Id) ||
            !ExpectedMaterialIds.Contains(Id) || SeenIds.Contains(Id) ||
            !Row->TryGetStringField(TEXT("targetInstance"), TargetInstance) ||
            !ReadStringArray(Row, TEXT("v3Slots"), GeneratedV3Slots, Error) ||
            !Row->TryGetObjectField(TEXT("outputs"), Outputs) ||
            !Outputs || !(*Outputs).IsValid() ||
            (*Outputs)->Values.Num() != ExpectedMapTypes.Num())
        {
            OutReport = TEXT("V5_MATERIAL_PACK_INVALID: generated material row is unsafe or duplicated.");
            return false;
        }
        TArray<FString> InterfaceV5Slots;
        int32 MatchingInterfaceSpecCount = 0;
        for (const FHeroMaterialInstanceSpec& Candidate : OutPack.Instances)
        {
            if (!Candidate.bSpecialParameterOnly &&
                Candidate.TextureSurfaceIdV3 == Id)
            {
                ++MatchingInterfaceSpecCount;
                InterfaceV5Slots.Append(Candidate.V5Slots);
            }
        }
        const int32 ExpectedMatchingSpecCount = Id == TEXT("Trim") ? 2 : 1;
        bool bTargetInstanceMatchesV3 = !GeneratedV3Slots.IsEmpty();
        for (const FString& V3Slot : GeneratedV3Slots)
        {
            bTargetInstanceMatchesV3 = bTargetInstanceMatchesV3 &&
                ExpectedV3SourceSlotMaterials().FindRef(V3Slot) == TargetInstance;
        }
        if (MatchingInterfaceSpecCount != ExpectedMatchingSpecCount ||
            !HasExactV3ToV5TextureBinding(
                Id,
                GeneratedV3Slots, InterfaceV5Slots) ||
            !bTargetInstanceMatchesV3)
        {
            OutReport = FString::Printf(
                TEXT("V5_MATERIAL_PACK_INVALID: generated/interface binding differs for '%s'."),
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
                OutReport = TEXT("V5_MATERIAL_PACK_INVALID: ") + Error;
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
            TEXT("ArchitecturalDirtMasks"), SharedRecord) ||
        !SharedRecord || !(*SharedRecord).IsValid() ||
        !ParseTextureRecord(
            TEXT("Shared"),
            TEXT("ArchitecturalDirtMasks"),
            *SharedRecord,
            SharedSpec,
            Error) ||
        SeenFiles.Contains(SharedSpec.SourceFilename))
    {
        OutReport = TEXT("V5_MATERIAL_PACK_INVALID: shared architectural-dirt receptivity map changed. ") + Error;
        return false;
    }
    SeenFiles.Add(SharedSpec.SourceFilename);
    OutPack.Textures.Add(MoveTemp(SharedSpec));
    int64 OutputCount = 0;
    if (!ReadInteger(GeneratedManifest, TEXT("outputCount"), OutputCount, Error) ||
        OutputCount != ExpectedMaterialTextureCount ||
        OutPack.Textures.Num() != ExpectedMaterialTextureCount)
    {
        OutReport = TEXT("V5_MATERIAL_PACK_INVALID: exactly 57 generated maps are required.");
        return false;
    }
    TArray<FString> DiskPngs;
    IFileManager::Get().FindFiles(
        DiskPngs,
        *ProjectSourcePath(FPaths::Combine(V3MaterialSourceRoot, TEXT("Generated/*.png"))),
        true,
        false);
    TSet<FString> DiskPngSet;
    for (const FString& Filename : DiskPngs)
    {
        DiskPngSet.Add(Filename);
    }
    if (!AreSetsEqual(DiskPngSet, SeenFiles))
    {
        OutReport = TEXT("V5_MATERIAL_PACK_INVALID: Generated PNG roster differs from the exact manifest.");
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Validated additive HeroMaterialsV5 interface and eleven distinct V5 instances (including a separate Door instance) referencing %d exact frozen HeroMaterialsV3 texture packages in place; no V5 texture payload is generated or copied, and no measured-material claim is made."),
        OutPack.Textures.Num());
    return true;
}

bool ValidateCompleteV5SourceSet(
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
        OutError = TEXT("Stop Play-In-Editor before running hero-v5 editor operations.");
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
        OutError = TEXT("Refusing hero-v5 mutation while packages are dirty: ") +
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
    TEXT("triad.istana_public_view_hero_v5.embedded_payload.v1");
const TCHAR* HeroPayloadSchemaMetadataKey = TEXT("TRIADHeroV5PayloadSchema");
const TCHAR* HeroPayloadKindMetadataKey = TEXT("TRIADHeroV5PayloadKind");
const TCHAR* HeroPayloadDigestMetadataKey = TEXT("TRIADHeroV5PayloadDigest");

bool SealEmbeddedPayload(
    UObject* Asset,
    const TCHAR* Kind,
    const FString& Digest,
    FString& OutError)
{
    if (!Asset || !Asset->GetOutermost() || Digest.IsEmpty())
    {
        OutError = TEXT("Cannot seal an absent or empty v5 embedded payload.");
        return false;
    }
    UMetaData* MetaData = Asset->GetOutermost()->GetMetaData();
    if (!MetaData)
    {
        OutError = TEXT("Could not allocate v5 embedded-payload metadata.");
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
            TEXT("V5 asset '%s' embedded-payload seal is absent or stale."),
            Asset ? *Asset->GetPathName() : TEXT("<null>"));
        return false;
    }
    return true;
}

bool ValidateFrozenV3TexturePayloadSeal(
    UTexture2D* Texture,
    FString& OutError)
{
    static const TCHAR* V3PayloadSchema =
        TEXT("triad.istana_public_view_hero_v3.embedded_payload.v1");
    static const TCHAR* V3SchemaKey = TEXT("TRIADHeroV3PayloadSchema");
    static const TCHAR* V3KindKey = TEXT("TRIADHeroV3PayloadKind");
    static const TCHAR* V3DigestKey = TEXT("TRIADHeroV3PayloadDigest");
    UMetaData* MetaData = Texture && Texture->GetOutermost()
        ? Texture->GetOutermost()->GetMetaData()
        : nullptr;
    const FString CurrentDigest = Texture
        ? Texture->Source.GetIdString()
        : FString();
    if (!MetaData || CurrentDigest.IsEmpty() ||
        MetaData->GetValue(Texture, V3SchemaKey) != V3PayloadSchema ||
        MetaData->GetValue(Texture, V3KindKey) != TEXT("texture_source_id") ||
        MetaData->GetValue(Texture, V3DigestKey) != CurrentDigest)
    {
        OutError = FString::Printf(
            TEXT("Frozen V3 texture '%s' has a missing/stale V3 payload seal."),
            Texture ? *Texture->GetPathName() : TEXT("<null>"));
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
        V3MaterialSourceRoot,
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
    if (!ValidateFrozenV3TexturePayloadSeal(Texture, OutError))
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
            TEXT("TRIAD_IPV5_NODE_%d_%d_%s"),
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
        Parameter->Group = TEXT("Istana Hero V5");
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
        Parameter->Group = TEXT("Istana Hero V5");
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
        Parameter->Group = TEXT("Istana Hero V5");
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
        Parameter->Group = TEXT("Istana Hero V5");
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
            TEXT("Could not connect v5 material graph edge '%s' -> '%s'."),
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
            TEXT("Could not connect v5 material property %d."),
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
            TEXT("M_IPV_HeroSurface_V5"),
            MaterialAssetRoot,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaPublicViewHeroV5Assets"))))
        : nullptr;
    if (!Material)
    {
        OutError = TEXT("Could not create M_IPV_HeroSurface_V5.");
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bScreenSpaceReflections = false;

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
    UMaterialExpressionTextureSampleParameter2D* ArchitecturalDirtMasks =
        CreateTextureParameter(Material, TEXT("Tex_ArchitecturalDirtMasks"), SAMPLERTYPE_Masks, -1250, 850);
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
    UMaterialExpressionStaticBoolParameter* UseArchitecturalDirt =
        CreateStaticBoolParameter(Material, TEXT("UseArchitecturalDirtMasks"), false, -650, 1150);
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
    UMaterialExpressionStaticSwitch* ArchitecturalDirtEnabledSwitch =
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
    UMaterialExpressionScalarParameter* SillDirtStrength =
        CreateScalarParameter(Material, TEXT("SillDirtStrength"), 0.0f, -950, 800);
    UMaterialExpressionScalarParameter* CorniceRunoffStrength =
        CreateScalarParameter(Material, TEXT("CorniceRunoffStrength"), 0.0f, -950, 950);
    UMaterialExpressionScalarParameter* GroundContactDampStrength =
        CreateScalarParameter(Material, TEXT("GroundContactDampStrength"), 0.0f, -950, 1100);
    UMaterialExpressionScalarParameter* CavityDirtStrength =
        CreateScalarParameter(Material, TEXT("CavityDirtStrength"), 0.0f, -950, 1250);
    UMaterialExpressionScalarParameter* HeightMillimetres =
        CreateScalarParameter(Material, TEXT("HeightMillimetres"), 0.0f, -2200, 850);
    UMaterialExpressionScalarParameter* BumpOffsetStrength =
        CreateScalarParameter(Material, TEXT("BumpOffsetStrength"), 0.0f, -2200, 1000);
    UMaterialExpressionCustom* HeightRatio = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV5_HeightRatio"),
        TEXT("return (max(HeightMillimetres, 0.0) * 0.001 / max(TileMeters, 0.001)) * max(BumpOffsetStrength, 0.0);"),
        CMOT_Float1,
        TArray<FName>{TEXT("HeightMillimetres"), TEXT("TileMeters"), TEXT("BumpOffsetStrength")},
        -1850,
        900);
    UMaterialExpressionBumpOffset* BumpOffset =
        CreateExpression<UMaterialExpressionBumpOffset>(Material, -1600, 200);
    UMaterialExpressionCustom* AlbedoResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV5_OpaqueAlbedo"),
        TEXT("float macroSigned = (MacroRgb.r - 0.5) * 2.0 + (MacroA - 0.5) * 0.7;\n")
        TEXT("float weatherMask = saturate(dot(SurfaceRgb, float3(0.45, 0.20, 0.35)) + SurfaceA * 0.15);\n")
        TEXT("float architecturalDirt = saturate(DirtRgb.r * max(SillDirtStrength, 0.0) + DirtRgb.g * max(CorniceRunoffStrength, 0.0) + DirtRgb.b * max(GroundContactDampStrength, 0.0) + DirtA * max(CavityDirtStrength, 0.0)) * DirtEnabled;\n")
        TEXT("float weather = saturate(weatherMask * WeatheringStrength + architecturalDirt);\n")
        TEXT("return saturate(TintedBase * max(0.0, 1.0 + macroSigned * MacroAlbedoStrength - weather * 0.18));"),
        CMOT_Float3,
        TArray<FName>{
            TEXT("TintedBase"), TEXT("MacroRgb"), TEXT("MacroA"),
            TEXT("SurfaceRgb"), TEXT("SurfaceA"), TEXT("DirtRgb"), TEXT("DirtA"),
            TEXT("MacroAlbedoStrength"), TEXT("WeatheringStrength"),
            TEXT("SillDirtStrength"), TEXT("CorniceRunoffStrength"),
            TEXT("GroundContactDampStrength"), TEXT("CavityDirtStrength"),
            TEXT("DirtEnabled")},
        -100,
        -650);
    UMaterialExpressionCustom* RoughnessResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV5_OpaqueRoughness"),
        TEXT("float macroSigned = (MacroRgb.g - 0.5) * 2.0 + (MacroA - 0.5) * 0.35;\n")
        TEXT("float weatherMask = saturate(dot(SurfaceRgb, float3(0.45, 0.20, 0.35)) + SurfaceA * 0.15);\n")
        TEXT("float architecturalDirt = saturate(DirtRgb.r * max(SillDirtStrength, 0.0) + DirtRgb.g * max(CorniceRunoffStrength, 0.0) + DirtRgb.b * max(GroundContactDampStrength, 0.0) + DirtA * max(CavityDirtStrength, 0.0)) * DirtEnabled;\n")
        TEXT("float weather = saturate(weatherMask * WeatheringStrength + architecturalDirt);\n")
        TEXT("return saturate(TextureRoughness + RoughnessBias + macroSigned * MacroRoughnessStrength + weather * 0.15);"),
        CMOT_Float1,
        TArray<FName>{
            TEXT("TextureRoughness"), TEXT("RoughnessBias"), TEXT("MacroRgb"), TEXT("MacroA"),
            TEXT("SurfaceRgb"), TEXT("SurfaceA"), TEXT("DirtRgb"), TEXT("DirtA"),
            TEXT("MacroRoughnessStrength"), TEXT("WeatheringStrength"),
            TEXT("SillDirtStrength"), TEXT("CorniceRunoffStrength"),
            TEXT("GroundContactDampStrength"), TEXT("CavityDirtStrength"),
            TEXT("DirtEnabled")},
        -100,
        -300);
    UMaterialExpressionCustom* MetallicResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV5_OpaqueMetallic"),
        TEXT("return saturate(TextureMetallic + ExposedEnabled * SurfaceEdgeMask * max(ExposedMetalMaskStrength, 0.0));"),
        CMOT_Float1,
        TArray<FName>{
            TEXT("TextureMetallic"), TEXT("SurfaceEdgeMask"),
            TEXT("ExposedMetalMaskStrength"), TEXT("ExposedEnabled")},
        -100,
        0);
    UMaterialExpressionCustom* NormalResponse = CreateCustomExpression(
        Material,
        TEXT("TRIAD_IPV5_NormalRNM"),
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
        !Height || !DetailNormal || !Macro || !SurfaceMasks || !ArchitecturalDirtMasks ||
        !Tint || !TintedBase || !RoughnessBias || !MetallicFallback ||
        !DetailStrength || !FlatNormal || !One || !Zero || !UseTextureSet ||
        !UseBumpOffset || !UseArchitecturalDirt || !UseExposedMetal ||
        !TileUvSwitch || !BaseSwitch || !RoughnessSwitch || !MetallicSwitch ||
        !NormalSwitch || !AoSwitch || !ArchitecturalDirtEnabledSwitch ||
        !ExposedEnabledSwitch || !NormalStrength || !MacroAlbedoStrength ||
        !MacroRoughnessStrength || !WeatheringStrength || !SillDirtStrength ||
        !CorniceRunoffStrength || !GroundContactDampStrength || !CavityDirtStrength ||
        !HeightMillimetres || !BumpOffsetStrength || !HeightRatio || !BumpOffset ||
        !AlbedoResponse || !RoughnessResponse || !MetallicResponse || !NormalResponse)
    {
        OutError = TEXT("Could not allocate the complete M_IPV_HeroSurface_V5 parameter graph.");
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
    ArchitecturalDirtMasks->Texture = FindTextureForMaterial(
        TEXT("Shared"), TEXT("ArchitecturalDirtMasks"), Pack, Textures);
    if (!BaseColor->Texture || !Normal->Texture || !Orm->Texture || !Height->Texture ||
        !DetailNormal->Texture || !Macro->Texture || !SurfaceMasks->Texture ||
        !ArchitecturalDirtMasks->Texture)
    {
        OutError = TEXT("Opaque v5 master lacks a frozen default texture binding.");
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
        !ConnectExpression(MacroUv, TEXT(""), ArchitecturalDirtMasks, TEXT("UVs"), OutError) ||
        !ConnectExpression(BaseColor, TEXT("RGB"), TintedBase, TEXT("A"), OutError) ||
        !ConnectExpression(Tint, TEXT(""), TintedBase, TEXT("B"), OutError) ||
        !ConnectExpression(One, TEXT(""), ArchitecturalDirtEnabledSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(Zero, TEXT(""), ArchitecturalDirtEnabledSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseArchitecturalDirt, TEXT(""), ArchitecturalDirtEnabledSwitch, TEXT("Value"), OutError) ||
        !ConnectExpression(One, TEXT(""), ExposedEnabledSwitch, TEXT("True"), OutError) ||
        !ConnectExpression(Zero, TEXT(""), ExposedEnabledSwitch, TEXT("False"), OutError) ||
        !ConnectExpression(UseExposedMetal, TEXT(""), ExposedEnabledSwitch, TEXT("Value"), OutError) ||
        !ConnectExpression(TintedBase, TEXT(""), AlbedoResponse, TEXT("TintedBase"), OutError) ||
        !ConnectExpression(Macro, TEXT("RGB"), AlbedoResponse, TEXT("MacroRgb"), OutError) ||
        !ConnectExpression(Macro, TEXT("A"), AlbedoResponse, TEXT("MacroA"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("RGB"), AlbedoResponse, TEXT("SurfaceRgb"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("A"), AlbedoResponse, TEXT("SurfaceA"), OutError) ||
        !ConnectExpression(ArchitecturalDirtMasks, TEXT("RGB"), AlbedoResponse, TEXT("DirtRgb"), OutError) ||
        !ConnectExpression(ArchitecturalDirtMasks, TEXT("A"), AlbedoResponse, TEXT("DirtA"), OutError) ||
        !ConnectExpression(MacroAlbedoStrength, TEXT(""), AlbedoResponse, TEXT("MacroAlbedoStrength"), OutError) ||
        !ConnectExpression(WeatheringStrength, TEXT(""), AlbedoResponse, TEXT("WeatheringStrength"), OutError) ||
        !ConnectExpression(SillDirtStrength, TEXT(""), AlbedoResponse, TEXT("SillDirtStrength"), OutError) ||
        !ConnectExpression(CorniceRunoffStrength, TEXT(""), AlbedoResponse, TEXT("CorniceRunoffStrength"), OutError) ||
        !ConnectExpression(GroundContactDampStrength, TEXT(""), AlbedoResponse, TEXT("GroundContactDampStrength"), OutError) ||
        !ConnectExpression(CavityDirtStrength, TEXT(""), AlbedoResponse, TEXT("CavityDirtStrength"), OutError) ||
        !ConnectExpression(ArchitecturalDirtEnabledSwitch, TEXT(""), AlbedoResponse, TEXT("DirtEnabled"), OutError) ||
        !ConnectExpression(Orm, TEXT("G"), RoughnessResponse, TEXT("TextureRoughness"), OutError) ||
        !ConnectExpression(RoughnessBias, TEXT(""), RoughnessResponse, TEXT("RoughnessBias"), OutError) ||
        !ConnectExpression(Macro, TEXT("RGB"), RoughnessResponse, TEXT("MacroRgb"), OutError) ||
        !ConnectExpression(Macro, TEXT("A"), RoughnessResponse, TEXT("MacroA"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("RGB"), RoughnessResponse, TEXT("SurfaceRgb"), OutError) ||
        !ConnectExpression(SurfaceMasks, TEXT("A"), RoughnessResponse, TEXT("SurfaceA"), OutError) ||
        !ConnectExpression(ArchitecturalDirtMasks, TEXT("RGB"), RoughnessResponse, TEXT("DirtRgb"), OutError) ||
        !ConnectExpression(ArchitecturalDirtMasks, TEXT("A"), RoughnessResponse, TEXT("DirtA"), OutError) ||
        !ConnectExpression(MacroRoughnessStrength, TEXT(""), RoughnessResponse, TEXT("MacroRoughnessStrength"), OutError) ||
        !ConnectExpression(WeatheringStrength, TEXT(""), RoughnessResponse, TEXT("WeatheringStrength"), OutError) ||
        !ConnectExpression(SillDirtStrength, TEXT(""), RoughnessResponse, TEXT("SillDirtStrength"), OutError) ||
        !ConnectExpression(CorniceRunoffStrength, TEXT(""), RoughnessResponse, TEXT("CorniceRunoffStrength"), OutError) ||
        !ConnectExpression(GroundContactDampStrength, TEXT(""), RoughnessResponse, TEXT("GroundContactDampStrength"), OutError) ||
        !ConnectExpression(CavityDirtStrength, TEXT(""), RoughnessResponse, TEXT("CavityDirtStrength"), OutError) ||
        !ConnectExpression(ArchitecturalDirtEnabledSwitch, TEXT(""), RoughnessResponse, TEXT("DirtEnabled"), OutError) ||
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
    Material->bUsedWithNanite = true;
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
            TEXT("M_IPV_HeroGlass_V5"),
            MaterialAssetRoot,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaPublicViewHeroV5Assets"))))
        : nullptr;
    if (!Material)
    {
        OutError = TEXT("Could not create M_IPV_HeroGlass_V5.");
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
    // UE5.5 material-local translucent SSR eligibility. This does not change
    // a renderer/project CVar and is sealed into the V5 graph fingerprint.
    Material->bScreenSpaceReflections = true;

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
        TEXT("TRIAD_IPV5_GlassAlbedo"),
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
        TEXT("TRIAD_IPV5_GlassRoughness"),
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
        TEXT("TRIAD_IPV5_GlassNormalRNM"),
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
        OutError = TEXT("Could not allocate the complete M_IPV_HeroGlass_V5 graph.");
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
        OutError = TEXT("Glass v5 master lacks a frozen default texture binding.");
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
    UMaterial* Parent = Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V5")
        ? GlassMaster
        : OpaqueMaster;
    UMaterialInstanceConstantFactoryNew* Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    if (!Factory || !Parent)
    {
        OutError = TEXT("Could not allocate a v5 material-instance factory or parent.");
        return nullptr;
    }
    Factory->InitialParent = Parent;
    UMaterialInstanceConstant* Instance =
        Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
            Spec.AssetName,
            MaterialAssetRoot,
            UMaterialInstanceConstant::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaPublicViewHeroV5Assets"))));
    if (!Instance)
    {
        OutError = FString::Printf(
            TEXT("Could not create v5 material instance '%s'."),
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
            if (Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V5") &&
                FString(Binding.Key) == TEXT("Height"))
            {
                continue;
            }
            UTexture2D* Texture = FindTextureForMaterial(
                Spec.TextureSurfaceIdV3, Binding.Key, Pack, Textures);
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
        if (Spec.ParentAssetName != TEXT("M_IPV_HeroGlass_V5"))
        {
            UTexture2D* Shared = FindTextureForMaterial(
                TEXT("Shared"), TEXT("ArchitecturalDirtMasks"),
                Pack, Textures);
            if (!Shared)
            {
                OutError = TEXT("Shared v5 architectural-dirt receptivity mask is absent.");
                return nullptr;
            }
            Instance->SetTextureParameterValueEditorOnly(
                FMaterialParameterInfo(TEXT("Tex_ArchitecturalDirtMasks")),
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

const TCHAR* HeroGraphSealSchema =
    TEXT("triad.istana_public_view_hero_v5.material_graph_seal.v1");
const TCHAR* HeroGraphSchemaMetadataKey = TEXT("TRIADHeroV5GraphSchema");
const TCHAR* HeroGraphShaMetadataKey = TEXT("TRIADHeroV5GraphSha256");

FString ExpectedHeroGraphRecipeSchema(const UMaterial* Material)
{
    if (Material && Material->GetName() == TEXT("M_IPV_HeroSurface_V5"))
    {
        return TEXT("triad.istana_hero_surface_graph.v5.1");
    }
    if (Material && Material->GetName() == TEXT("M_IPV_HeroGlass_V5"))
    {
        return TEXT("triad.istana_hero_glass_graph.v5.2");
    }
    return FString();
}

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
        OutError = TEXT("Cannot fingerprint a null v5 master material.");
        return false;
    }
    TArray<UMaterialExpression*> Expressions;
    TSet<FString> NodeIds;
    for (UMaterialExpression* Expression : Material->GetExpressions())
    {
        if (!Expression || Expression->Desc.IsEmpty() ||
            !Expression->Desc.StartsWith(TEXT("TRIAD_IPV5_NODE_")) ||
            NodeIds.Contains(Expression->Desc))
        {
            OutError = TEXT("V5 graph fingerprint requires unique stable node identifiers.");
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
    const FString RecipeSchema = ExpectedHeroGraphRecipeSchema(Material);
    if (RecipeSchema.IsEmpty())
    {
        OutError = TEXT("V5 graph fingerprint refuses an unknown master recipe.");
        return false;
    }
    AppendGraphToken(Canonical, HeroGraphSealSchema);
    AppendGraphToken(Canonical, RecipeSchema);
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
        Canonical, Material->bScreenSpaceReflections ? TEXT("1") : TEXT("0"));
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
        int32 InputCount = 0;
        for (FExpressionInputIterator It{Expression}; It; ++It)
        {
            ++InputCount;
        }
        AppendGraphToken(Canonical, FString::FromInt(InputCount));
        for (FExpressionInputIterator It{Expression}; It; ++It)
        {
            const FExpressionInput* Input = It.Input;
            AppendGraphToken(
                Canonical,
                Expression->GetInputName(It.Index).ToString());
            if (!Input || !Input->Expression)
            {
                AppendGraphToken(Canonical, TEXT("<null>"));
                continue;
            }
            if (!NodeIds.Contains(Input->Expression->Desc))
            {
                OutError = TEXT("V5 graph edge points outside its sealed master material.");
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
        OutError = TEXT("Could not calculate the v5 material graph SHA-256.");
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
            OutError = TEXT("V5 master package is absent while sealing its graph.");
        }
        return false;
    }
    UMetaData* MetaData = Material->GetOutermost()->GetMetaData();
    if (!MetaData)
    {
        OutError = TEXT("Could not allocate v5 master graph metadata.");
        return false;
    }
    MetaData->SetValue(
        Material,
        HeroGraphSchemaMetadataKey,
        *ExpectedHeroGraphRecipeSchema(Material));
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
    if (StoredSchema != ExpectedHeroGraphRecipeSchema(Material) ||
        StoredDigest.Len() != 64 || StoredDigest != ActualDigest)
    {
        OutError = FString::Printf(
            TEXT("V5 master '%s' graph seal is absent or stale (actual %s)."),
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
    for (FExpressionInputIterator It{Expression}; It; ++It)
    {
        FExpressionInput* Input = It.Input;
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
        !Expression->Description.StartsWith(TEXT("TRIAD_IPV5_")) ||
        Expression->Code.IsEmpty() || Expression->Inputs.Num() == 0)
    {
        OutError = TEXT("V5 material contains an unidentified or empty custom response node.");
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
                TEXT("V5 custom response '%s' input[%d] of %d is unnamed."),
                *Expression->Description,
                InputIndex,
                Expression->Inputs.Num());
            return false;
        }
        if (Names.Contains(Input.InputName))
        {
            OutError = FString::Printf(
                TEXT("V5 custom response '%s' input[%d] '%s' duplicates an earlier input name."),
                *Expression->Description,
                InputIndex,
                *InputName);
            return false;
        }
        if (!Input.Input.Expression)
        {
            OutError = FString::Printf(
                TEXT("V5 custom response '%s' input[%d] '%s' is directly disconnected."),
                *Expression->Description,
                InputIndex,
                *InputName);
            return false;
        }
        const FExpressionInput TracedInput = Input.Input.GetTracedInput();
        if (!TracedInput.Expression)
        {
            OutError = FString::Printf(
                TEXT("V5 custom response '%s' input[%d] '%s' traces to no source expression (direct source '%s')."),
                *Expression->Description,
                InputIndex,
                *InputName,
                *Input.Input.Expression->GetName());
            return false;
        }
        if (!CustomCodeUsesIdentifier(Expression->Code, InputName))
        {
            OutError = FString::Printf(
                TEXT("V5 custom response '%s' input[%d] '%s' is not referenced as an exact HLSL identifier."),
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
            TEXT("V5 master '%s' did not produce a valid regular SM5 shader map."),
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
    else if (!bGlass && Name == TEXT("Tex_ArchitecturalDirtMasks"))
    {
        MapType = TEXT("ArchitecturalDirtMasks");
        OutSamplerType = SAMPLERTYPE_Masks;
        const FString AssetName =
            TEXT("T_IPV_HeroV3_Shared_ArchitecturalDirtMasks");
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
        TEXT("T_IPV_HeroV3_%s_%s"),
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
        ? TEXT("M_IPV_HeroGlass_V5")
        : TEXT("M_IPV_HeroSurface_V5");
    if (!Material ||
        Material->GetPathName() != MaterialObjectPath(ExpectedName) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != (bGlass ? BLEND_Translucent : BLEND_Opaque) ||
        Material->TwoSided ||
        !Material->bTangentSpaceNormal ||
        Material->bUseMaterialAttributes ||
        Material->bScreenSpaceReflections != bGlass ||
        (!bGlass && !Material->GetUsageByFlag(MATUSAGE_Nanite)) ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        (bGlass &&
            (Material->TranslucencyLightingMode != TLM_Surface ||
             Material->RefractionMethod != RM_IndexOfRefraction)))
    {
        OutError = FString::Printf(
            TEXT("V5 master '%s' has invalid domain/blend/shading settings."),
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
                TEXT("V5 master '%s' has a disconnected required material property %d."),
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
                TEXT("V5 master '%s' contains a forbidden duplicate-prone StaticSwitchParameter node."),
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
                    TEXT("V5 master '%s' has an unnamed, duplicate, or output-unreachable parameter '%s'."),
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
                    TEXT("V5 master '%s' texture parameter '%s' has stale sampling semantics or default binding."),
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
                    TEXT("V5 master '%s' contains an incomplete True/False/Value static branch."),
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
                        TEXT("V5 master '%s' duplicates custom response '%s'."),
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
                OutError = TEXT("Opaque v5 bump-offset branch is disconnected or changed.");
                return false;
            }
        }
    }
    if (Reachable.Num() != Material->GetExpressions().Num())
    {
        OutError = FString::Printf(
            TEXT("V5 master '%s' contains an output-unreachable or extra expression node."),
            *ExpectedName);
        return false;
    }
    const TArray<const TCHAR*> OpaqueTextures = {
        TEXT("Tex_BaseColor"), TEXT("Tex_Normal"), TEXT("Tex_ORM"),
        TEXT("Tex_Height"), TEXT("Tex_DetailNormal"),
        TEXT("Tex_MacroVariation"), TEXT("Tex_SurfaceMasks"),
        TEXT("Tex_ArchitecturalDirtMasks")};
    const TArray<const TCHAR*> OpaqueScalars = {
        TEXT("TileMeters"), TEXT("DetailTileMeters"), TEXT("MacroTileMeters"),
        TEXT("NormalStrength"), TEXT("DetailNormalStrength"),
        TEXT("RoughnessBias"), TEXT("MacroAlbedoStrength"),
        TEXT("MacroRoughnessStrength"), TEXT("WeatheringStrength"),
        TEXT("SillDirtStrength"), TEXT("CorniceRunoffStrength"),
        TEXT("GroundContactDampStrength"), TEXT("CavityDirtStrength"),
        TEXT("HeightMillimetres"),
        TEXT("BumpOffsetStrength"), TEXT("ExposedMetalMaskStrength")};
    const TArray<const TCHAR*> OpaqueSwitches = {
        TEXT("UseTextureSet"), TEXT("UseBumpOffset"),
        TEXT("UseArchitecturalDirtMasks"), TEXT("UseExposedMetalMask")};
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
            TEXT("V5 master '%s' does not retain the exact interface parameter roster."),
            *ExpectedName);
        return false;
    }
    const TArray<FString> ExpectedCustomDescriptions = bGlass
        ? TArray<FString>{
            TEXT("TRIAD_IPV5_GlassAlbedo"),
            TEXT("TRIAD_IPV5_GlassRoughness"),
            TEXT("TRIAD_IPV5_GlassNormalRNM")}
        : TArray<FString>{
            TEXT("TRIAD_IPV5_HeightRatio"),
            TEXT("TRIAD_IPV5_OpaqueAlbedo"),
            TEXT("TRIAD_IPV5_OpaqueRoughness"),
            TEXT("TRIAD_IPV5_OpaqueMetallic"),
            TEXT("TRIAD_IPV5_NormalRNM")};
    if (CustomDescriptions.Num() != ExpectedCustomDescriptions.Num())
    {
        OutError = FString::Printf(
            TEXT("V5 master '%s' has the wrong response-node count."),
            *ExpectedName);
        return false;
    }
    for (const FString& Description : ExpectedCustomDescriptions)
    {
        if (!CustomDescriptions.Contains(Description))
        {
            OutError = FString::Printf(
                TEXT("V5 master '%s' lacks required response node '%s'."),
                *ExpectedName,
                *Description);
            return false;
        }
    }
    if (BumpOffsetCount != (bGlass ? 0 : 1))
    {
        OutError = FString::Printf(
            TEXT("V5 master '%s' has an invalid bump-offset node count."),
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
        TEXT("V5 material instance '%s' %s override roster differs: missing=[%s]; unexpected=[%s]; expected=[%s]; actual=[%s]."),
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
            TEXT("V5 material instance '%s' has an invalid path or parent."),
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
                TEXT("V5 material instance '%s' has an invalid or duplicate scalar override."),
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
                TEXT("V5 material instance '%s' has an invalid or duplicate vector override."),
                *Spec.AssetName);
            return false;
        }
        ActualVectorNames.Add(Value.ParameterInfo.Name);
    }
    TSet<FName> ExpectedTextureNames;
    if (!Spec.bSpecialParameterOnly)
    {
        ExpectedTextureNames = Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V5")
            ? TSet<FName>{
                TEXT("Tex_BaseColor"), TEXT("Tex_Normal"), TEXT("Tex_ORM"),
                TEXT("Tex_DetailNormal"), TEXT("Tex_MacroVariation"),
                TEXT("Tex_SurfaceMasks")}
            : TSet<FName>{
                TEXT("Tex_BaseColor"), TEXT("Tex_Normal"), TEXT("Tex_ORM"),
                TEXT("Tex_Height"), TEXT("Tex_DetailNormal"),
                TEXT("Tex_MacroVariation"), TEXT("Tex_SurfaceMasks"),
                TEXT("Tex_ArchitecturalDirtMasks")};
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
                TEXT("V5 material instance '%s' has an invalid or duplicate texture override."),
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
                TEXT("V5 material instance '%s' has an invalid or duplicate static override."),
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
                TEXT("V5 material instance '%s' has %d unexpected %s override(s)."),
                *Spec.AssetName,
                Family.Value,
                Family.Key);
            return false;
        }
    }
    if (StaticParameters.bHasMaterialLayers)
    {
        OutError = FString::Printf(
            TEXT("V5 material instance '%s' has an unexpected material-layer override."),
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
            TEXT("V5 material instance '%s' enables forbidden base-property override flag(s): [%s]."),
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
                TEXT("V5 material instance '%s' scalar '%s' changed."),
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
                TEXT("V5 material instance '%s' vector '%s' changed."),
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
                TEXT("V5 material instance '%s' static switch '%s' changed."),
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
            if (Spec.ParentAssetName == TEXT("M_IPV_HeroGlass_V5") &&
                FString(Binding.Key) == TEXT("Height"))
            {
                continue;
            }
            UTexture2D* Expected = FindTextureForMaterial(
                Spec.TextureSurfaceIdV3, Binding.Key, Pack, Textures);
            UTexture* Actual = nullptr;
            if (!Expected ||
                !Instance->GetTextureParameterValue(
                    FHashedMaterialParameterInfo(Binding.Value),
                    Actual,
                    true) ||
                Actual != Expected)
            {
                OutError = FString::Printf(
                    TEXT("V5 material instance '%s' texture '%s' changed."),
                    *Spec.AssetName,
                    Binding.Value);
                return false;
            }
        }
        if (Spec.ParentAssetName != TEXT("M_IPV_HeroGlass_V5"))
        {
            UTexture2D* Expected = FindTextureForMaterial(
                TEXT("Shared"),
                TEXT("ArchitecturalDirtMasks"),
                Pack,
                Textures);
            UTexture* Actual = nullptr;
            if (!Expected ||
                !Instance->GetTextureParameterValue(
                    FHashedMaterialParameterInfo(
                        TEXT("Tex_ArchitecturalDirtMasks")),
                    Actual,
                    true) ||
                Actual != Expected)
            {
                OutError = FString::Printf(
                    TEXT("V5 material instance '%s' shared architectural-dirt map changed."),
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
            TEXT("V5 material instance '%s' SM5 permutation invalid: ")
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
        OutError = TEXT("V5 hero lacks a stable render-data key or lighting GUID.");
        return false;
    }
    FString Canonical;
    AppendGraphToken(
        Canonical,
        TEXT("triad.istana_public_view_hero_v5.mesh_render_payload.v1"));
    AppendGraphToken(Canonical, Mesh->GetRenderData()->DerivedDataKey);
    AppendGraphToken(
        Canonical,
        Mesh->GetLightingGuid().ToString(EGuidFormats::Digits));
    FTCHARToUTF8 Utf8(*Canonical);
    if (!CalculateSha256Bytes(Utf8.Get(), Utf8.Length(), OutDigest))
    {
        OutError = TEXT("Could not hash the v5 hero render payload identity.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool NormalizeImportedHeroMaterialSlots(
    UStaticMesh* Mesh,
    FString& OutError)
{
    if (!Mesh || Mesh->GetStaticMaterials().Num() != ExpectedHeroSlots().Num() ||
        !Mesh->GetRenderData() ||
        Mesh->GetRenderData()->LODResources.Num() != 1)
    {
        OutError = TEXT("Legacy OBJ import did not produce the exact eleven-slot regular mesh surface.");
        return false;
    }

    const TArray<FStaticMaterial> ImportedMaterials =
        Mesh->GetStaticMaterials();
    TArray<FStaticMaterial> OrderedMaterials;
    OrderedMaterials.SetNum(ExpectedHeroSlots().Num());
    TArray<int32> ImportedToOrdered;
    ImportedToOrdered.Init(INDEX_NONE, ImportedMaterials.Num());
    TArray<bool> SeenOrderedSlots;
    SeenOrderedSlots.Init(false, ExpectedHeroSlots().Num());

    for (int32 ImportedIndex = 0;
         ImportedIndex < ImportedMaterials.Num();
         ++ImportedIndex)
    {
        const FStaticMaterial& Imported = ImportedMaterials[ImportedIndex];
        const FString Slot = Imported.MaterialSlotName.ToString();
        const FString ImportedSlot = Imported.ImportedMaterialSlotName.ToString();
        const int32 OrderedIndex = ExpectedHeroSlots().IndexOfByKey(Slot);
        if (OrderedIndex == INDEX_NONE || Slot != ImportedSlot ||
            SeenOrderedSlots[OrderedIndex])
        {
            OutError = FString::Printf(
                TEXT("Legacy OBJ material slots are not an exact unique v5 permutation at imported index %d: slot='%s' imported='%s'."),
                ImportedIndex,
                *Slot,
                *ImportedSlot);
            return false;
        }
        SeenOrderedSlots[OrderedIndex] = true;
        ImportedToOrdered[ImportedIndex] = OrderedIndex;
        OrderedMaterials[OrderedIndex] = Imported;
        OrderedMaterials[OrderedIndex].MaterialSlotName = FName(*Slot);
        OrderedMaterials[OrderedIndex].ImportedMaterialSlotName = FName(*Slot);
    }
    for (int32 OrderedIndex = 0;
         OrderedIndex < SeenOrderedSlots.Num();
         ++OrderedIndex)
    {
        if (!SeenOrderedSlots[OrderedIndex])
        {
            OutError = FString::Printf(
                TEXT("Legacy OBJ import omitted required v5 slot '%s'."),
                *ExpectedHeroSlots()[OrderedIndex]);
            return false;
        }
    }

    const FStaticMeshLODResources& ImportedLod =
        Mesh->GetRenderData()->LODResources[0];
    if (ImportedLod.Sections.Num() != ExpectedHeroSlots().Num())
    {
        OutError = FString::Printf(
            TEXT("Legacy OBJ import produced %d sections instead of the exact eleven semantic material groups."),
            ImportedLod.Sections.Num());
        return false;
    }
    for (int32 SectionIndex = 0;
         SectionIndex < ImportedLod.Sections.Num();
         ++SectionIndex)
    {
        const int32 RenderMaterialIndex =
            ImportedLod.Sections[SectionIndex].MaterialIndex;
        FMeshSectionInfo SectionInfo =
            Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo OriginalSectionInfo =
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (!ImportedToOrdered.IsValidIndex(RenderMaterialIndex) ||
            !ImportedToOrdered.IsValidIndex(SectionInfo.MaterialIndex) ||
            !ImportedToOrdered.IsValidIndex(OriginalSectionInfo.MaterialIndex) ||
            RenderMaterialIndex != SectionInfo.MaterialIndex ||
            RenderMaterialIndex != OriginalSectionInfo.MaterialIndex ||
            ImportedToOrdered[RenderMaterialIndex] == INDEX_NONE ||
            ImportedToOrdered[SectionInfo.MaterialIndex] == INDEX_NONE ||
            ImportedToOrdered[OriginalSectionInfo.MaterialIndex] == INDEX_NONE)
        {
            OutError = FString::Printf(
                TEXT("Legacy OBJ section %d references an invalid imported material index."),
                SectionIndex);
            return false;
        }
        SectionInfo.MaterialIndex =
            ImportedToOrdered[SectionInfo.MaterialIndex];
        OriginalSectionInfo.MaterialIndex =
            ImportedToOrdered[OriginalSectionInfo.MaterialIndex];
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, SectionInfo);
        Mesh->GetOriginalSectionInfoMap().Set(
            0, SectionIndex, OriginalSectionInfo);
    }

    Mesh->SetStaticMaterials(OrderedMaterials);
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
        OutError = TEXT("V5 hero mesh path/LOD/triangle/no-Nanite contract failed.");
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
            TEXT("V5 hero imported bounds changed: min=%s max=%s expected min=%s max=%s cm."),
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
        OutError = TEXT("V5 hero source-model normal/tangent/identity-scale contract failed.");
        return false;
    }
    const UBodySetup* BodySetup = Mesh->GetBodySetup();
    if (BodySetup &&
        (BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple ||
         BodySetup->AggGeom.GetElementCount() != 0))
    {
        OutError = TEXT("V5 visual hero must contain no simple or complex-as-simple collision.");
        return false;
    }
    if (Mesh->GetStaticMaterials().Num() != ExpectedHeroSlots().Num())
    {
        OutError = TEXT("V5 hero does not have exactly eleven material slots.");
        return false;
    }
    TSet<FString> SeenSlots;
    const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();
    for (int32 SlotIndex = 0; SlotIndex < StaticMaterials.Num(); ++SlotIndex)
    {
        const FStaticMaterial& StaticMaterial = StaticMaterials[SlotIndex];
        const FString Slot = StaticMaterial.MaterialSlotName.ToString();
        const FString ImportedSlot =
            StaticMaterial.ImportedMaterialSlotName.ToString();
        UMaterialInstanceConstant* const* Expected = SlotMaterials.Find(Slot);
        if (Slot != ExpectedHeroSlots()[SlotIndex] ||
            ImportedSlot != ExpectedHeroSlots()[SlotIndex] ||
            Slot.StartsWith(TEXT("M_IPV_")) ||
            SeenSlots.Contains(Slot) || !Expected ||
            StaticMaterial.MaterialInterface != *Expected)
        {
            OutError = FString::Printf(
                TEXT("V5 hero material binding/order is invalid at index %d: actual='%s' expected='%s'."),
                SlotIndex,
                *Slot,
                *ExpectedHeroSlots()[SlotIndex]);
            return false;
        }
        SeenSlots.Add(Slot);
    }
    if (SeenSlots.Num() != ExpectedHeroSlots().Num())
    {
        OutError = TEXT("V5 hero material slot roster is incomplete.");
        return false;
    }
    const FMeshDescription* MeshDescription = Mesh->GetMeshDescription(0);
    const FStaticMeshLODResources& Lod =
        Mesh->GetRenderData()->LODResources[0];
    if (!MeshDescription ||
        MeshDescription->PolygonGroups().Num() != ExpectedHeroSlots().Num() ||
        Lod.Sections.Num() != ExpectedHeroSlots().Num())
    {
        OutError = TEXT("V5 hero mesh-description polygon groups or render sections are incomplete.");
        return false;
    }
    FStaticMeshConstAttributes Attributes(*MeshDescription);
    const TPolygonGroupAttributesConstRef<FName> PolygonGroupSlots =
        Attributes.GetPolygonGroupMaterialSlotNames();
    TSet<FString> SeenSectionSlots;
    int32 SectionIndex = 0;
    for (const FPolygonGroupID PolygonGroupId :
         MeshDescription->PolygonGroups().GetElementIDs())
    {
        const FString Slot = PolygonGroupSlots[PolygonGroupId].ToString();
        const int32 ExpectedMaterialIndex =
            ExpectedHeroSlots().IndexOfByKey(Slot);
        if (ExpectedMaterialIndex == INDEX_NONE ||
            SeenSectionSlots.Contains(Slot) ||
            !Lod.Sections.IsValidIndex(SectionIndex) ||
            Lod.Sections[SectionIndex].MaterialIndex != ExpectedMaterialIndex ||
            Mesh->GetSectionInfoMap().Get(0, SectionIndex).MaterialIndex !=
                ExpectedMaterialIndex ||
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex).MaterialIndex !=
                ExpectedMaterialIndex)
        {
            OutError = FString::Printf(
                TEXT("V5 hero polygon-group/render-section material mapping is invalid at section %d for slot '%s'."),
                SectionIndex,
                *Slot);
            return false;
        }
        SeenSectionSlots.Add(Slot);
        ++SectionIndex;
    }
    if (SectionIndex != ExpectedHeroSlots().Num() ||
        SeenSectionSlots.Num() != ExpectedHeroSlots().Num())
    {
        OutError = TEXT("V5 hero render-section material roster is incomplete.");
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

enum class EHeroV5AssetState : uint8
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
    OutPaths.Add(MaterialObjectPath(TEXT("M_IPV_HeroSurface_V5")));
    OutPaths.Add(MaterialObjectPath(TEXT("M_IPV_HeroGlass_V5")));
    for (const FHeroMaterialInstanceSpec& Spec : Pack.Instances)
    {
        OutPaths.Add(MaterialObjectPath(Spec.AssetName));
    }
}

EHeroV5AssetState InspectHeroV5Assets(
    UEditorAssetSubsystem* AssetSubsystem,
    const FHeroGeometryContract& Geometry,
    const FHeroMaterialPack& Pack,
    UStaticMesh** OutHero,
    FString& OutReport)
{
    if (!AssetSubsystem)
    {
        OutReport = TEXT("Editor Asset Subsystem is unavailable.");
        return EHeroV5AssetState::PartialOrInvalid;
    }
    TSet<FString> ExpectedPaths;
    BuildExpectedAssetPaths(Pack, ExpectedPaths);
    if (ExpectedPaths.Num() != 14)
    {
        OutReport = FString::Printf(
            TEXT("The frozen V5 namespace contract must contain exactly 14 objects (mesh, two masters, and eleven distinct instances); found %d."),
            ExpectedPaths.Num());
        return EHeroV5AssetState::PartialOrInvalid;
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    const TArray<FString> NamespaceRoots = {
        HeroAssetRoot,
        MaterialAssetRoot};
    AssetRegistry.ScanPathsSynchronous(
        NamespaceRoots,
        true,
        false);
    TArray<FAssetData> NamespaceAssets;
    const TArray<FName> NamespaceRootNames = {
        FName(*HeroAssetRoot),
        FName(*MaterialAssetRoot)};
    if (!AssetRegistry.GetAssetsByPaths(
            NamespaceRootNames,
            NamespaceAssets,
            true,
            false))
    {
        OutReport = TEXT(
            "Asset Registry could not recursively census the exact V5 hero/material roots.");
        return EHeroV5AssetState::PartialOrInvalid;
    }
    TSet<FString> ActualPaths;
    for (const FAssetData& AssetData : NamespaceAssets)
    {
        if (!AssetData.IsValid())
        {
            OutReport = TEXT(
                "Asset Registry returned invalid data while recursively censusing the exact V5 roots.");
            return EHeroV5AssetState::PartialOrInvalid;
        }
        ActualPaths.Add(AssetData.GetObjectPathString());
    }
    if (ActualPaths.Num() == 0)
    {
        OutReport = TEXT("V5 hero/material namespace is absent.");
        return EHeroV5AssetState::Absent;
    }
    TArray<FString> MissingPaths;
    for (const FString& ExpectedPath : ExpectedPaths)
    {
        if (!ActualPaths.Contains(ExpectedPath))
        {
            MissingPaths.Add(ExpectedPath);
        }
    }
    TArray<FString> UnexpectedPaths;
    for (const FString& ActualPath : ActualPaths)
    {
        if (!ExpectedPaths.Contains(ActualPath))
        {
            UnexpectedPaths.Add(ActualPath);
        }
    }
    if (ActualPaths.Num() != ExpectedPaths.Num() ||
        MissingPaths.Num() != 0 || UnexpectedPaths.Num() != 0)
    {
        MissingPaths.Sort();
        UnexpectedPaths.Sort();
        OutReport = FString::Printf(
            TEXT("V5 Asset Registry namespace census differs from the exact expected object-path set (actual=%d expected=%d missing=[%s] unexpected=[%s]); refusing arbitrary V5 packages, textures, or repair-in-place."),
            ActualPaths.Num(),
            ExpectedPaths.Num(),
            *FString::Join(MissingPaths, TEXT(", ")),
            *FString::Join(UnexpectedPaths, TEXT(", ")));
        return EHeroV5AssetState::PartialOrInvalid;
    }

    FString Error;
    TMap<FName, UTexture2D*> Textures;
    for (const FHeroTextureSpec& Spec : Pack.Textures)
    {
        if (DoesObjectOrPackageExist(
                AssetSubsystem, ForbiddenV5TextureObjectPath(Spec)))
        {
            OutReport = FString::Printf(
                TEXT("V5 texture copy is forbidden but package/object '%s' exists."),
                *ForbiddenV5TextureObjectPath(Spec));
            return EHeroV5AssetState::PartialOrInvalid;
        }
        UTexture2D* Texture = LoadObject<UTexture2D>(
            nullptr, *TextureObjectPath(Spec));
        if (!ValidateHeroTexture(Texture, Spec, Error))
        {
            OutReport = TEXT("V5 frozen V3 texture dependency is absent or stale: ") +
                Error;
            return EHeroV5AssetState::PartialOrInvalid;
        }
        Textures.Add(FName(*Spec.AssetName), Texture);
    }
    int32 ExistingCount = 0;
    for (const FString& Path : ExpectedPaths)
    {
        if (DoesObjectOrPackageExist(AssetSubsystem, Path))
        {
            ++ExistingCount;
        }
    }
    if (ExistingCount != ExpectedPaths.Num())
    {
        OutReport = FString::Printf(
            TEXT("V5 namespace is partial: %d of %d exact assets exist; refusing overwrite or repair-in-place."),
            ExistingCount,
            ExpectedPaths.Num());
        return EHeroV5AssetState::PartialOrInvalid;
    }

    UMaterial* OpaqueMaster = LoadObject<UMaterial>(
        nullptr,
        *MaterialObjectPath(TEXT("M_IPV_HeroSurface_V5")));
    UMaterial* GlassMaster = LoadObject<UMaterial>(
        nullptr,
        *MaterialObjectPath(TEXT("M_IPV_HeroGlass_V5")));
    if (!ValidateHeroMaster(OpaqueMaster, false, Error) ||
        !ValidateHeroMaster(GlassMaster, true, Error))
    {
        OutReport = Error;
        return EHeroV5AssetState::PartialOrInvalid;
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
            return EHeroV5AssetState::PartialOrInvalid;
        }
        for (const FString& Slot : Spec.V5Slots)
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
        return EHeroV5AssetState::PartialOrInvalid;
    }
    if (OutHero)
    {
        *OutHero = Hero;
    }
    OutReport = FString::Printf(
        TEXT("Validated non-overwriting v5 asset set: one regular %d-triangle hero, 57 exact frozen V3 texture references with zero V5 texture packages, two additive V5 masters, %d instances, eleven M_IPV5 slot bindings, and no hero collision/Nanite."),
        Geometry.Triangles,
        Pack.Instances.Num());
    return EHeroV5AssetState::CompleteValid;
}

bool PrepareHeroObjAdapter(
    FString& OutAdaptedObjFilename,
    FString& OutError)
{
    const FString AdapterDirectory =
        FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectIntermediateDir(),
            TEXT("TRIAD/IstanaPublicViewV5/LegacyObjMaterialSlots")));
    if (!IFileManager::Get().MakeDirectory(*AdapterDirectory, true))
    {
        OutError = TEXT("Could not create the isolated v5 legacy-OBJ adapter directory.");
        return false;
    }
    const FString MtlFilename = TEXT("IstanaPublicViewV5Slots.mtl");
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
        OutError = TEXT("Could not write the isolated v5 material-slot adapter MTL.");
        return false;
    }

    const FString OriginalPath = ProjectSourcePath(GeometryObjRelativePath);
    OutAdaptedObjFilename = FPaths::Combine(
        AdapterDirectory,
        TEXT("SM_IstanaPublicViewV5_Building_Hero.import.obj"));
    TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*OriginalPath));
    TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*OutAdaptedObjFilename));
    if (!Reader || !Writer)
    {
        OutError = TEXT("Could not open the v5 source/adapter OBJ streams.");
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

bool ImportHeroV5AssetSet(
    UEditorAssetSubsystem* AssetSubsystem,
    const FHeroGeometryContract& Geometry,
    const FHeroMaterialPack& Pack,
    const FString& SourceReport,
    FString& OutMessage)
{
    FString ExistingReport;
    const EHeroV5AssetState ExistingState = InspectHeroV5Assets(
        AssetSubsystem, Geometry, Pack, nullptr, ExistingReport);
    if (ExistingState == EHeroV5AssetState::CompleteValid)
    {
        OutMessage = SourceReport + TEXT(" ") + ExistingReport +
            TEXT(" Existing complete v5 output was accepted idempotently; no package was saved or overwritten.");
        return true;
    }
    if (ExistingState != EHeroV5AssetState::Absent)
    {
        OutMessage = TEXT("V5_IMPORT_REFUSED: ") + ExistingReport;
        return false;
    }

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TArray<UObject*> AssetsToSave;
    FString Error;
    TMap<FName, UTexture2D*> Textures;
    for (const FHeroTextureSpec& Spec : Pack.Textures)
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(
            nullptr, *TextureObjectPath(Spec));
        if (!ValidateHeroTexture(Texture, Spec, Error))
        {
            OutMessage = TEXT("V5_IMPORT_REFUSED_FROZEN_V3_TEXTURE_DEPENDENCY: ") +
                Error;
            return false;
        }
        Textures.Add(FName(*Spec.AssetName), Texture);
    }

    UMaterial* OpaqueMaster = CreateOpaqueHeroMaster(Pack, Textures, AssetTools, Error);
    UMaterial* GlassMaster = CreateGlassHeroMaster(Pack, Textures, AssetTools, Error);
    if (!OpaqueMaster || !GlassMaster ||
        !ValidateHeroMaster(OpaqueMaster, false, Error) ||
        !ValidateHeroMaster(GlassMaster, true, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: ") + Error;
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
            OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: ") + Error;
            return false;
        }
        for (const FString& Slot : Spec.V5Slots)
        {
            SlotMaterials.Add(Slot, Instance);
        }
        AssetsToSave.Add(Instance);
    }
    if (SlotMaterials.Num() != ExpectedHeroSlots().Num())
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: not all eleven v5 slots have exact material instances.");
        return false;
    }

    FString AdaptedObjFilename;
    if (!PrepareHeroObjAdapter(AdaptedObjFilename, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: ") + Error;
        return false;
    }
    UFbxImportUI* ImportOptions = NewObject<UFbxImportUI>();
    UFbxFactory* ImportFactory = NewObject<UFbxFactory>();
    UAssetImportTask* HeroTask = NewObject<UAssetImportTask>();
    if (!ImportOptions || !ImportFactory || !HeroTask ||
        !ImportOptions->StaticMeshImportData)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: could not allocate deterministic regular-mesh OBJ import options.");
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
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: OBJ import produced no v5 hero mesh.");
        return false;
    }
    FStaticMeshCompilingManager::Get().FinishCompilation({Hero});
    Hero->Modify();
    UAssetImportData* ImportData = Hero->GetAssetImportData();
    if (!ImportData)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: v5 hero lacks import provenance.");
        return false;
    }
    ImportData->Update(ProjectSourcePath(GeometryObjRelativePath));
    if (Hero->GetNumSourceModels() != 1)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: v5 hero requires exactly one source LOD.");
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
    if (!NormalizeImportedHeroMaterialSlots(Hero, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: ") + Error;
        return false;
    }
    for (const FString& Slot : ExpectedHeroSlots())
    {
        const int32 MaterialIndex = Hero->GetMaterialIndex(FName(*Slot));
        UMaterialInstanceConstant* Material = SlotMaterials.FindRef(Slot);
        if (MaterialIndex == INDEX_NONE || !Material)
        {
            OutMessage = FString::Printf(
                TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: hero lacks exact slot/material '%s'."),
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
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_NAMESPACE: ") + Error;
        return false;
    }
    AssetsToSave.Add(Hero);

    if (!AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_V5_NAMESPACE: v5 assets validated in memory but did not all save. No V1/V2/V3/V4 package was touched; inspect/delete only the isolated V5 namespaces before retrying.");
        return false;
    }
    FString FinalReport;
    if (InspectHeroV5Assets(
            AssetSubsystem,
            Geometry,
            Pack,
            nullptr,
            FinalReport) != EHeroV5AssetState::CompleteValid)
    {
        OutMessage = TEXT("POST_SAVE_V5_VALIDATION_FAILED: ") + FinalReport;
        return false;
    }
    OutMessage = SourceReport + TEXT(" ") + FinalReport +
        TEXT(" Imported only /Game/TRIAD/IstanaPublicViewV5 and /Game/TRIAD/IstanaPublicView/HeroMaterialsV5; all 57 textures were referenced read-only from HeroMaterialsV3, and no V5 texture package was created. V1/V2/V3/V4 meshes, materials, collision, maps, surroundings, renderer settings, SM6, and project settings were not loaded for mutation or saved.");
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
    FPreservedComponentState HeroVisualComponent;
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
    if (!CapturePreservedComponent(
            Scene->BuildingHeroVisualComponent.Get(),
            OutSnapshot.HeroVisualComponent,
            OutError))
    {
        return false;
    }
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
    const FPreservedComponentState& Actual,
    bool bCompareMeshAndEffectiveMaterials = true)
{
    if (Expected.Name != Actual.Name ||
        (bCompareMeshAndEffectiveMaterials &&
            Expected.MeshPath != Actual.MeshPath) ||
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
        (bCompareMeshAndEffectiveMaterials &&
            Expected.EffectiveMaterialPaths != Actual.EffectiveMaterialPaths) ||
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
        OutError = TEXT("Hero-v5 preservation requires exactly nine collision/surroundings component snapshots.");
        return false;
    }
    if (!Expected.SceneActorTransform.Equals(
            Actual.SceneActorTransform, 0.000001f) ||
        Expected.SceneActorTags != Actual.SceneActorTags)
    {
        OutError = TEXT("Hero-v5 migration changed the public-view scene actor transform or tags.");
        return false;
    }
    if (!SamePreservedState(
            Expected.HeroVisualComponent,
            Actual.HeroVisualComponent,
            false))
    {
        OutError = TEXT("Hero-v5 migration changed a hero-visual property other than its mesh/effective materials.");
        return false;
    }
    for (int32 Index = 0; Index < Expected.Components.Num(); ++Index)
    {
        if (!SamePreservedState(
                Expected.Components[Index], Actual.Components[Index]))
        {
            OutError = FString::Printf(
                TEXT("Hero-v5 migration changed preserved component '%s'."),
                *Expected.Components[Index].Name);
            return false;
        }
    }
    if (Expected.Cameras.Num() != Actual.Cameras.Num())
    {
        OutError = TEXT("Hero-v5 migration changed the camera actor count.");
        return false;
    }
    for (int32 Index = 0; Index < Expected.Cameras.Num(); ++Index)
    {
        if (!SameCameraState(Expected.Cameras[Index], Actual.Cameras[Index]))
        {
            OutError = FString::Printf(
                TEXT("Hero-v5 migration changed preserved camera '%s'."),
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
        OutError = TEXT("Hero-v5 map does not reuse the exact v1 collision asset.");
        return false;
    }
    const int32 PreservedTreeCount =
        Snapshot.Components[6].InstanceTransforms.Num() +
        Snapshot.Components[7].InstanceTransforms.Num() +
        Snapshot.Components[8].InstanceTransforms.Num();
    if (PreservedTreeCount != ExpectedPreservedTreeInstanceCount ||
        Snapshot.Components[6].InstanceTransforms.IsEmpty() ||
        Snapshot.Components[7].InstanceTransforms.IsEmpty() ||
        Snapshot.Components[8].InstanceTransforms.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("Hero-v5 preservation requires exactly %d volumetric tree instances across all three exact archetypes; found %d."),
            ExpectedPreservedTreeInstanceCount,
            PreservedTreeCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool WriteMigrationInvariantReport(
    const FString& ProtectedV1Filename,
    const FString& ProtectedV1ShaBefore,
    const FString& ProtectedV1ShaAfter,
    int64 ProtectedV1Bytes,
    const FString& ProtectedV2Filename,
    const FString& ProtectedV2ShaBefore,
    const FString& ProtectedV2ShaAfter,
    int64 ProtectedV2Bytes,
    const FString& ProtectedV3Filename,
    const FString& ProtectedV3ShaBefore,
    const FString& ProtectedV3ShaAfter,
    int64 ProtectedV3Bytes,
    const FString& SourceV4Filename,
    const FString& SourceV4ShaBefore,
    const FString& SourceV4ShaAfter,
    int64 SourceV4Bytes,
    const FPublicViewSurroundingsSnapshot& Baseline,
    const FPublicViewSurroundingsSnapshot& Persisted,
    FString& OutReportPath,
    FString& OutError)
{
    if (ProtectedV1ShaBefore != ProtectedV1ShaAfter ||
        ProtectedV2ShaBefore != ProtectedV2ShaAfter ||
        ProtectedV3ShaBefore != ProtectedV3ShaAfter ||
        SourceV4ShaBefore != SourceV4ShaAfter ||
        !ValidateSurroundingsUnchanged(Baseline, Persisted, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A protected V1/V2/V3 map or the source V4 map SHA-256 changed during V5 migration.");
        }
        return false;
    }
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(
        TEXT("schema"),
        TEXT("triad.istana_public_view_hero_v5.component_invariants.v1"));
    Root->SetStringField(TEXT("protectedV1MapPackage"), ProtectedV1MapPackage);
    Root->SetStringField(TEXT("protectedV1MapFilename"), ProtectedV1Filename);
    Root->SetStringField(TEXT("protectedV1MapSha256Before"), ProtectedV1ShaBefore);
    Root->SetStringField(TEXT("protectedV1MapSha256After"), ProtectedV1ShaAfter);
    Root->SetNumberField(
        TEXT("protectedV1MapBytes"), static_cast<double>(ProtectedV1Bytes));
    Root->SetBoolField(TEXT("protectedV1MapByteIdentical"), true);
    Root->SetStringField(TEXT("protectedV2MapPackage"), ProtectedV2MapPackage);
    Root->SetStringField(TEXT("protectedV2MapFilename"), ProtectedV2Filename);
    Root->SetStringField(TEXT("protectedV2MapSha256Before"), ProtectedV2ShaBefore);
    Root->SetStringField(TEXT("protectedV2MapSha256After"), ProtectedV2ShaAfter);
    Root->SetNumberField(
        TEXT("protectedV2MapBytes"), static_cast<double>(ProtectedV2Bytes));
    Root->SetBoolField(TEXT("protectedV2MapByteIdentical"), true);
    Root->SetStringField(TEXT("protectedV3MapPackage"), ProtectedV3MapPackage);
    Root->SetStringField(TEXT("protectedV3MapFilename"), ProtectedV3Filename);
    Root->SetStringField(TEXT("protectedV3MapSha256Before"), ProtectedV3ShaBefore);
    Root->SetStringField(TEXT("protectedV3MapSha256After"), ProtectedV3ShaAfter);
    Root->SetNumberField(
        TEXT("protectedV3MapBytes"), static_cast<double>(ProtectedV3Bytes));
    Root->SetBoolField(TEXT("protectedV3MapByteIdentical"), true);
    Root->SetStringField(TEXT("sourceMapPackage"), SourceMapPackage);
    Root->SetStringField(TEXT("sourceMapFilename"), SourceV4Filename);
    Root->SetStringField(TEXT("sourceMapSha256Before"), SourceV4ShaBefore);
    Root->SetStringField(TEXT("sourceMapSha256After"), SourceV4ShaAfter);
    Root->SetNumberField(TEXT("sourceMapBytes"), static_cast<double>(SourceV4Bytes));
    Root->SetStringField(TEXT("destinationMapPackage"), DestinationMapPackage);
    Root->SetStringField(TEXT("heroMesh"), HeroMeshObjectPath);
    Root->SetBoolField(TEXT("sourceMapByteIdentical"), true);
    Root->SetBoolField(TEXT("onlyHeroVisualComponentChanged"), true);
    Root->SetBoolField(TEXT("heroVisualNonAssetStateExact"), true);
    Root->SetStringField(
        TEXT("sourceHeroMesh"), Baseline.HeroVisualComponent.MeshPath);
    Root->SetStringField(
        TEXT("destinationHeroMesh"), Persisted.HeroVisualComponent.MeshPath);
    Root->SetBoolField(TEXT("componentsExact"), true);
    Root->SetBoolField(TEXT("camerasExact"), true);
    Root->SetNumberField(
        TEXT("preservedTreeInstanceCount"),
        static_cast<double>(ExpectedPreservedTreeInstanceCount));
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
        OutError = TEXT("Could not serialize the hero-v5 invariant report.");
        return false;
    }
    OutReportPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPublicViewV5/Istana_PublicView_Exterior_v5.invariants.json")));
    if (!IFileManager::Get().MakeDirectory(
            *FPaths::GetPath(OutReportPath), true) ||
        !FFileHelper::SaveStringToFile(
            Json,
            *OutReportPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = TEXT("Could not persist the hero-v5 invariant report under Saved/TRIAD.");
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

bool ValidateEffectiveHeroV5Materials(
    UStaticMeshComponent* HeroComponent,
    FString& OutError)
{
    if (!HeroComponent || !HeroComponent->GetStaticMesh() ||
        HeroComponent->HasOverrideMaterials() ||
        HeroComponent->GetNumOverrideMaterials() != 0 ||
        HeroComponent->GetOverlayMaterial() != nullptr)
    {
        OutError = TEXT("V5 hero component has missing mesh, material overrides, or an overlay material.");
        return false;
    }
    const TArray<FStaticMaterial>& StaticMaterials =
        HeroComponent->GetStaticMesh()->GetStaticMaterials();
    if (StaticMaterials.Num() != ExpectedHeroSlots().Num() ||
        HeroComponent->GetNumMaterials() != ExpectedHeroSlots().Num())
    {
        OutError = TEXT("V5 hero component does not expose exactly eleven effective materials.");
        return false;
    }
    for (int32 Index = 0; Index < StaticMaterials.Num(); ++Index)
    {
        const FString Slot = StaticMaterials[Index].MaterialSlotName.ToString();
        const FString ExpectedInstance = ExpectedSlotInstances().FindRef(Slot);
        UMaterialInterface* Effective = HeroComponent->GetMaterial(Index);
        if (Slot != ExpectedHeroSlots()[Index] ||
            ExpectedInstance.IsEmpty() || !Effective ||
            Effective->GetPathName() != MaterialObjectPath(ExpectedInstance))
        {
            OutError = FString::Printf(
                TEXT("V5 hero effective material differs at slot '%s'."),
                *Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateExactV4SourceHeroComponent(
    UStaticMeshComponent* HeroComponent,
    FString& OutError)
{
    if (!HeroComponent || !HeroComponent->GetStaticMesh() ||
        HeroComponent->GetStaticMesh()->GetPathName() != V4HeroMeshObjectPath ||
        HeroComponent->HasOverrideMaterials() ||
        HeroComponent->GetNumOverrideMaterials() != 0 ||
        HeroComponent->GetOverlayMaterial() != nullptr ||
        HeroComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
    {
        OutError = TEXT("V5 source must retain the exact non-colliding V4 hero visual without overrides or an overlay.");
        return false;
    }
    const TArray<FStaticMaterial>& StaticMaterials =
        HeroComponent->GetStaticMesh()->GetStaticMaterials();
    if (StaticMaterials.Num() != ExpectedV4SourceSlots().Num() ||
        HeroComponent->GetNumMaterials() != ExpectedV4SourceSlots().Num())
    {
        OutError = TEXT("V5 source V4 hero does not expose the exact eleven-slot material roster.");
        return false;
    }
    TSet<FString> SeenSlots;
    for (int32 Index = 0; Index < StaticMaterials.Num(); ++Index)
    {
        const FString Slot = StaticMaterials[Index].MaterialSlotName.ToString();
        const FString ExpectedInstance =
            ExpectedV4SourceSlotMaterials().FindRef(Slot);
        const UMaterialInterface* Effective = HeroComponent->GetMaterial(Index);
        // UE5.5's legacy Autodesk OBJ importer is free to expose the exact
        // semantic slot roster in FBX-node order rather than source first-use
        // order. V4 canonicalized the exact semantic roster after import;
        // still validate by name at the effective physical index and never
        // reorder or otherwise mutate the protected V4 asset/map.
        if (!ExpectedV4SourceSlots().Contains(Slot) ||
            SeenSlots.Contains(Slot) ||
            ExpectedInstance.IsEmpty() || !Effective ||
            Effective->GetPathName() != V4MaterialObjectPath(ExpectedInstance))
        {
            OutError = FString::Printf(
                TEXT("V5 source V4 hero semantic material binding differs at imported index %d (slot '%s')."),
                Index,
                *Slot);
            return false;
        }
        SeenSlots.Add(Slot);
    }
    if (SeenSlots.Num() != ExpectedV4SourceSlots().Num())
    {
        OutError = TEXT("V5 source V4 hero semantic material roster is incomplete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateWorldForV5(
    UWorld* World,
    const FString& RequiredPackage,
    const FPublicViewSurroundingsSnapshot* ExpectedSurroundings,
    FString& OutReport)
{
    if (!ExpectedSurroundings)
    {
        OutReport = TEXT("HERO_V5_MAP_INVALID: exact V4 surroundings/camera baseline is required.");
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
        OutReport = TEXT("HERO_V5_MAP_INVALID: exact non-colliding v5 hero visual is absent. ") + Error;
        return false;
    }
    if (!ValidateEffectiveHeroV5Materials(
            Scene->BuildingHeroVisualComponent, Error))
    {
        OutReport = TEXT("HERO_V5_MAP_INVALID: ") + Error;
        return false;
    }
    FPublicViewSurroundingsSnapshot Actual;
    if (!CaptureSurroundings(Scene, Actual, Error) ||
        !ValidateKnownV1Surroundings(Actual, Error) ||
        !ValidateSurroundingsUnchanged(*ExpectedSurroundings, Actual, Error))
    {
        OutReport = TEXT("HERO_V5_MAP_INVALID: ") + Error;
        return false;
    }
    FString SceneReport;
    if (!Scene->ValidatePublicViewScene(SceneReport))
    {
        OutReport = TEXT("HERO_V5_MAP_INVALID: ") + SceneReport;
        return false;
    }
    OutReport = TEXT("Validated v5 map hero-only boundary against the loaded exact V4 baseline: BuildingHeroVisualComponent uses the exact v5 regular static mesh and eleven ordered effective V5 materials with no overrides; scene actor and all nine collision, terrain, hardscape, context, and vegetation components retain exact assets, transforms, effective/override materials, visibility, activation, mobility, shadow/custom-depth state, collision/overlap state, HISM cull settings, instance transforms, and per-instance custom data; every camera retains exact actor state plus a deterministic digest of its complete editable component state, including post-process settings. ") + SceneReport;
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
            TEXT("Open exact source map '%s' before v5 migration."),
            *SourceMapPackage);
        return false;
    }
    OutScene = FindOnlySceneActor(World, OutError);
    if (!OutScene ||
        !ValidateExactV4SourceHeroComponent(
            OutScene->BuildingHeroVisualComponent, OutError))
    {
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

void ConfigureHeroV5D65CaptureCamera(
    UCameraComponent* CameraComponent,
    float FieldOfView)
{
    if (!CameraComponent)
    {
        return;
    }
    CameraComponent->SetFieldOfView(FieldOfView);
    CameraComponent->SetAspectRatio(16.0f / 9.0f);
    CameraComponent->SetConstraintAspectRatio(true);
    CameraComponent->SetPostProcessBlendWeight(1.0f);
    CameraComponent->PostProcessSettings = FPostProcessSettings();
    FPostProcessSettings& Settings = CameraComponent->PostProcessSettings;
    Settings.bOverride_AutoExposureMethod = true;
    Settings.AutoExposureMethod = AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Settings.AutoExposureApplyPhysicalCameraExposure = true;
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = 0.0f;
    Settings.bOverride_DynamicGlobalIlluminationMethod = true;
    Settings.DynamicGlobalIlluminationMethod =
        EDynamicGlobalIlluminationMethod::ScreenSpace;
    Settings.bOverride_ReflectionMethod = true;
    Settings.ReflectionMethod = EReflectionMethod::ScreenSpace;
    Settings.bOverride_CameraShutterSpeed = true;
    Settings.CameraShutterSpeed = 125.0f;
    Settings.bOverride_CameraISO = true;
    Settings.CameraISO = 100.0f;
    Settings.bOverride_DepthOfFieldFstop = true;
    Settings.DepthOfFieldFstop = 8.0f;
    Settings.bOverride_DepthOfFieldScale = true;
    Settings.DepthOfFieldScale = 0.0f;
    Settings.bOverride_TemperatureType = true;
    Settings.TemperatureType = TEMP_WhiteBalance;
    Settings.bOverride_WhiteTemp = true;
    Settings.WhiteTemp = 6500.0f;
    Settings.bOverride_WhiteTint = true;
    Settings.WhiteTint = 0.0f;
    Settings.bOverride_LocalExposureHighlightContrastScale = true;
    Settings.LocalExposureHighlightContrastScale = 1.0f;
    Settings.bOverride_LocalExposureShadowContrastScale = true;
    Settings.LocalExposureShadowContrastScale = 1.0f;
    Settings.bOverride_LocalExposureDetailStrength = true;
    Settings.LocalExposureDetailStrength = 1.0f;
    Settings.bOverride_LocalExposureMiddleGreyBias = true;
    Settings.LocalExposureMiddleGreyBias = 0.0f;
    Settings.bOverride_LocalExposureHighlightContrastCurve = true;
    Settings.LocalExposureHighlightContrastCurve = nullptr;
    Settings.bOverride_LocalExposureShadowContrastCurve = true;
    Settings.LocalExposureShadowContrastCurve = nullptr;
    Settings.bOverride_ColorGradingLUT = true;
    Settings.ColorGradingLUT = nullptr;
    Settings.bOverride_ColorSaturation = true;
    Settings.ColorSaturation = FVector4(1.0, 1.0, 1.0, 1.0);
    Settings.bOverride_ColorContrast = true;
    Settings.ColorContrast = FVector4(1.0, 1.0, 1.0, 1.0);
    Settings.bOverride_ColorGamma = true;
    Settings.ColorGamma = FVector4(1.0, 1.0, 1.0, 1.0);
    Settings.bOverride_ColorGain = true;
    Settings.ColorGain = FVector4(1.0, 1.0, 1.0, 1.0);
    Settings.bOverride_ColorOffset = true;
    Settings.ColorOffset = FVector4(0.0, 0.0, 0.0, 0.0);
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = 0.0f;
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = 0.0f;
    Settings.bOverride_MotionBlurAmount = true;
    Settings.MotionBlurAmount = 0.0f;
    Settings.bOverride_SceneFringeIntensity = true;
    Settings.SceneFringeIntensity = 0.0f;
    Settings.bOverride_FilmGrainIntensity = true;
    Settings.FilmGrainIntensity = 0.0f;
}

bool HasHeroV5D65CaptureCamera(
    const UCameraComponent* CameraComponent,
    float ExpectedFieldOfView)
{
    if (!CameraComponent)
    {
        return false;
    }
    const FPostProcessSettings& Settings =
        CameraComponent->PostProcessSettings;
    const auto IsVector4NearlyEqual = [](
        const FVector4& Left,
        const FVector4& Right)
    {
        return FMath::IsNearlyEqual(Left.X, Right.X) &&
            FMath::IsNearlyEqual(Left.Y, Right.Y) &&
            FMath::IsNearlyEqual(Left.Z, Right.Z) &&
            FMath::IsNearlyEqual(Left.W, Right.W);
    };
    return FMath::IsNearlyEqual(
            CameraComponent->FieldOfView, ExpectedFieldOfView) &&
        CameraComponent->bConstrainAspectRatio &&
        FMath::IsNearlyEqual(CameraComponent->AspectRatio, 16.0f / 9.0f) &&
        FMath::IsNearlyEqual(CameraComponent->PostProcessBlendWeight, 1.0f) &&
        Settings.bOverride_AutoExposureMethod &&
        Settings.AutoExposureMethod == AEM_Manual &&
        Settings.bOverride_AutoExposureApplyPhysicalCameraExposure &&
        Settings.AutoExposureApplyPhysicalCameraExposure &&
        Settings.bOverride_AutoExposureBias &&
        FMath::IsNearlyZero(Settings.AutoExposureBias) &&
        Settings.bOverride_DynamicGlobalIlluminationMethod &&
        Settings.DynamicGlobalIlluminationMethod ==
            EDynamicGlobalIlluminationMethod::ScreenSpace &&
        Settings.bOverride_ReflectionMethod &&
        Settings.ReflectionMethod == EReflectionMethod::ScreenSpace &&
        Settings.bOverride_CameraShutterSpeed &&
        FMath::IsNearlyEqual(Settings.CameraShutterSpeed, 125.0f) &&
        Settings.bOverride_CameraISO &&
        FMath::IsNearlyEqual(Settings.CameraISO, 100.0f) &&
        Settings.bOverride_DepthOfFieldFstop &&
        FMath::IsNearlyEqual(Settings.DepthOfFieldFstop, 8.0f) &&
        Settings.bOverride_DepthOfFieldScale &&
        FMath::IsNearlyZero(Settings.DepthOfFieldScale) &&
        Settings.bOverride_TemperatureType &&
        Settings.TemperatureType == TEMP_WhiteBalance &&
        Settings.bOverride_WhiteTemp &&
        FMath::IsNearlyEqual(Settings.WhiteTemp, 6500.0f) &&
        Settings.bOverride_WhiteTint &&
        FMath::IsNearlyZero(Settings.WhiteTint) &&
        Settings.bOverride_LocalExposureHighlightContrastScale &&
        FMath::IsNearlyEqual(
            Settings.LocalExposureHighlightContrastScale, 1.0f) &&
        Settings.bOverride_LocalExposureShadowContrastScale &&
        FMath::IsNearlyEqual(
            Settings.LocalExposureShadowContrastScale, 1.0f) &&
        Settings.bOverride_LocalExposureDetailStrength &&
        FMath::IsNearlyEqual(Settings.LocalExposureDetailStrength, 1.0f) &&
        Settings.bOverride_LocalExposureMiddleGreyBias &&
        FMath::IsNearlyZero(Settings.LocalExposureMiddleGreyBias) &&
        Settings.bOverride_LocalExposureHighlightContrastCurve &&
        Settings.LocalExposureHighlightContrastCurve == nullptr &&
        Settings.bOverride_LocalExposureShadowContrastCurve &&
        Settings.LocalExposureShadowContrastCurve == nullptr &&
        Settings.bOverride_ColorGradingLUT &&
        Settings.ColorGradingLUT == nullptr &&
        Settings.bOverride_ColorSaturation &&
        IsVector4NearlyEqual(
            Settings.ColorSaturation, FVector4(1.0, 1.0, 1.0, 1.0)) &&
        Settings.bOverride_ColorContrast &&
        IsVector4NearlyEqual(
            Settings.ColorContrast, FVector4(1.0, 1.0, 1.0, 1.0)) &&
        Settings.bOverride_ColorGamma &&
        IsVector4NearlyEqual(
            Settings.ColorGamma, FVector4(1.0, 1.0, 1.0, 1.0)) &&
        Settings.bOverride_ColorGain &&
        IsVector4NearlyEqual(
            Settings.ColorGain, FVector4(1.0, 1.0, 1.0, 1.0)) &&
        Settings.bOverride_ColorOffset &&
        IsVector4NearlyEqual(
            Settings.ColorOffset, FVector4(0.0, 0.0, 0.0, 0.0)) &&
        Settings.bOverride_BloomIntensity &&
        FMath::IsNearlyZero(Settings.BloomIntensity) &&
        Settings.bOverride_VignetteIntensity &&
        FMath::IsNearlyZero(Settings.VignetteIntensity) &&
        Settings.bOverride_MotionBlurAmount &&
        FMath::IsNearlyZero(Settings.MotionBlurAmount) &&
        Settings.bOverride_SceneFringeIntensity &&
        FMath::IsNearlyZero(Settings.SceneFringeIntensity) &&
        Settings.bOverride_FilmGrainIntensity &&
        FMath::IsNearlyZero(Settings.FilmGrainIntensity);
}

class FScopedHeroV5TsrCaptureQuality final
{
public:
    bool Apply(FString& OutError)
    {
        AntiAliasing_ = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.AntiAliasingMethod"));
        ScreenPercentage_ = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.ScreenPercentage"));
        TsrHistoryPercentage_ = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.TSR.History.ScreenPercentage"));
        SsgiQuality_ = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.SSGI.Quality"));
        SsrQuality_ = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.SSR.Quality"));
        if (!AntiAliasing_ || !ScreenPercentage_ || !TsrHistoryPercentage_ ||
            !SsgiQuality_ || !SsrQuality_)
        {
            OutError = TEXT("Required UE5.5 TSR/SSGI/SSR quality console variables are unavailable; no capture override was applied.");
            return false;
        }
        OriginalAntiAliasing_ = AntiAliasing_->GetInt();
        OriginalScreenPercentage_ = ScreenPercentage_->GetFloat();
        OriginalTsrHistoryPercentage_ = TsrHistoryPercentage_->GetFloat();
        OriginalSsgiQuality_ = SsgiQuality_->GetInt();
        OriginalSsrQuality_ = SsrQuality_->GetInt();
        AntiAliasing_->SetWithCurrentPriority(4);
        ScreenPercentage_->SetWithCurrentPriority(100.0f);
        TsrHistoryPercentage_->SetWithCurrentPriority(200.0f);
        SsgiQuality_->SetWithCurrentPriority(4);
        SsrQuality_->SetWithCurrentPriority(4);
        bApplied_ = true;
        if (AntiAliasing_->GetInt() != 4 ||
            !FMath::IsNearlyEqual(ScreenPercentage_->GetFloat(), 100.0f) ||
            !FMath::IsNearlyEqual(
                TsrHistoryPercentage_->GetFloat(), 200.0f) ||
            SsgiQuality_->GetInt() != 4 ||
            SsrQuality_->GetInt() != 4)
        {
            FString RestoreError;
            Restore(RestoreError);
            OutError = TEXT("UE5.5 rejected the exact scoped TSR/SSGI/SSR hero-capture quality profile; the prior values were restored. ") +
                RestoreError;
            return false;
        }
        OutError.Reset();
        return true;
    }

    bool Restore(FString& OutError)
    {
        if (!bApplied_)
        {
            OutError.Reset();
            return true;
        }
        AntiAliasing_->SetWithCurrentPriority(OriginalAntiAliasing_);
        ScreenPercentage_->SetWithCurrentPriority(OriginalScreenPercentage_);
        TsrHistoryPercentage_->SetWithCurrentPriority(
            OriginalTsrHistoryPercentage_);
        SsgiQuality_->SetWithCurrentPriority(OriginalSsgiQuality_);
        SsrQuality_->SetWithCurrentPriority(OriginalSsrQuality_);
        bApplied_ = false;
        if (AntiAliasing_->GetInt() != OriginalAntiAliasing_ ||
            !FMath::IsNearlyEqual(
                ScreenPercentage_->GetFloat(), OriginalScreenPercentage_) ||
            !FMath::IsNearlyEqual(
                TsrHistoryPercentage_->GetFloat(),
                OriginalTsrHistoryPercentage_) ||
            SsgiQuality_->GetInt() != OriginalSsgiQuality_ ||
            SsrQuality_->GetInt() != OriginalSsrQuality_)
        {
            OutError = TEXT("The transient TSR/SSGI/SSR quality values did not read back to their exact pre-capture state.");
            return false;
        }
        OutError.Reset();
        return true;
    }

    ~FScopedHeroV5TsrCaptureQuality()
    {
        FString Ignored;
        Restore(Ignored);
    }

private:
    IConsoleVariable* AntiAliasing_ = nullptr;
    IConsoleVariable* ScreenPercentage_ = nullptr;
    IConsoleVariable* TsrHistoryPercentage_ = nullptr;
    IConsoleVariable* SsgiQuality_ = nullptr;
    IConsoleVariable* SsrQuality_ = nullptr;
    int32 OriginalAntiAliasing_ = 0;
    float OriginalScreenPercentage_ = 0.0f;
    float OriginalTsrHistoryPercentage_ = 0.0f;
    int32 OriginalSsgiQuality_ = 0;
    int32 OriginalSsrQuality_ = 0;
    bool bApplied_ = false;
};

bool MigrateMapToV5(
    UEditorAssetSubsystem* AssetSubsystem,
    UStaticMesh* Hero,
    FString& OutMessage)
{
    if (!AssetSubsystem || !Hero)
    {
        OutMessage = TEXT("V5 migration requires the editor asset subsystem and validated hero.");
        return false;
    }
    if (FPackageName::DoesPackageExist(DestinationMapPackage) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        DoesObjectOrPackageExist(AssetSubsystem, DestinationMapObjectPath))
    {
        FString ExistingReport;
        if (!UTRIADIstanaPublicViewHeroV5EditorLibrary::
                ValidateIstanaPublicViewHeroV5Map(ExistingReport))
        {
            OutMessage = FString::Printf(
                TEXT("V5_MIGRATION_REFUSED_EXISTING_INVALID: destination '%s' exists but is not the exact already-migrated V5 map. No package was overwritten. %s"),
                *DestinationMapPackage,
                *ExistingReport);
            return false;
        }
        OutMessage = FString::Printf(
            TEXT("IDEMPOTENT_V5_MIGRATION_ALREADY_COMPLETE: exact validated destination '%s' already exists; no asset, map, surroundings, camera, renderer, or project state was mutated. %s"),
            *DestinationMapPackage,
            *ExistingReport);
        return true;
    }
    UWorld* SourceWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    ATRIADIstanaPublicViewSceneActor* SourceScene = nullptr;
    FPublicViewSurroundingsSnapshot SourceSnapshot;
    FString Error;
    FString V4StructuralReport;
    if (!UTRIADIstanaPublicViewHeroV4EditorLibrary::
            ValidateIstanaPublicViewHeroV4Map(V4StructuralReport))
    {
        OutMessage = TEXT("V5_MIGRATION_REFUSED: exact V4 source validation failed. ") +
            V4StructuralReport;
        return false;
    }
    if (!ValidateSourceWorldForMigration(
            SourceWorld, SourceScene, SourceSnapshot, Error))
    {
        OutMessage = TEXT("V5_MIGRATION_REFUSED: ") + Error;
        return false;
    }
    FString ProtectedV1Filename;
    FString ProtectedV1ShaBefore;
    int64 ProtectedV1BytesBefore = 0;
    FString ProtectedV2Filename;
    FString ProtectedV2ShaBefore;
    int64 ProtectedV2BytesBefore = 0;
    FString ProtectedV3Filename;
    FString ProtectedV3ShaBefore;
    int64 ProtectedV3BytesBefore = 0;
    FString SourceV4Filename;
    FString SourceV4ShaBefore;
    int64 SourceV4BytesBefore = 0;
    if (!FPackageName::DoesPackageExist(
            ProtectedV1MapPackage, &ProtectedV1Filename) ||
        !CalculateSha256(
            ProtectedV1Filename,
            ProtectedV1ShaBefore,
            ProtectedV1BytesBefore,
            Error) ||
        !FPackageName::DoesPackageExist(
            ProtectedV2MapPackage, &ProtectedV2Filename) ||
        !CalculateSha256(
            ProtectedV2Filename,
            ProtectedV2ShaBefore,
            ProtectedV2BytesBefore,
            Error) ||
        !FPackageName::DoesPackageExist(
            ProtectedV3MapPackage, &ProtectedV3Filename) ||
        !CalculateSha256(
            ProtectedV3Filename,
            ProtectedV3ShaBefore,
            ProtectedV3BytesBefore,
            Error) ||
        !FPackageName::DoesPackageExist(SourceMapPackage, &SourceV4Filename) ||
        !CalculateSha256(
            SourceV4Filename,
            SourceV4ShaBefore,
            SourceV4BytesBefore,
            Error))
    {
        OutMessage = TEXT("V5_MIGRATION_REFUSED: could not freeze protected V1/V2/V3 and source V4 map bytes before duplication. ") + Error;
        return false;
    }

    UWorld* TargetWorld = Cast<UWorld>(AssetSubsystem->DuplicateLoadedAsset(
        SourceWorld,
        DestinationMapPackage));
    if (!TargetWorld || !TargetWorld->GetOutermost() ||
        TargetWorld->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_MAP: could not create the isolated in-memory v5 map duplicate.");
        return false;
    }
    ATRIADIstanaPublicViewSceneActor* TargetScene =
        FindOnlySceneActor(TargetWorld, Error);
    FPublicViewSurroundingsSnapshot BeforeSwap;
    if (!TargetScene || !CaptureSurroundings(TargetScene, BeforeSwap, Error) ||
        !ValidateSurroundingsUnchanged(SourceSnapshot, BeforeSwap, Error))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_MAP: duplicate did not preserve exact V4 surroundings. ") + Error;
        return false;
    }

    // This is intentionally the sole scene mutation in the v5 migration.
    // Collision, terrain, hardscape, context, and vegetation are read-only.
    TargetScene->Modify();
    TargetScene->BuildingHeroVisualComponent->Modify();
    TargetScene->BuildingHeroVisualComponent->SetStaticMesh(Hero);
    TargetScene->BuildingHeroVisualComponent->EmptyOverrideMaterials();
    TargetScene->MarkPackageDirty();

    FPublicViewSurroundingsSnapshot AfterSwap;
    FString PreSaveReport;
    if (!CaptureSurroundings(TargetScene, AfterSwap, Error) ||
        !ValidateSurroundingsUnchanged(BeforeSwap, AfterSwap, Error) ||
        !ValidateWorldForV5(
            TargetWorld,
            DestinationMapPackage,
            &SourceSnapshot,
            PreSaveReport))
    {
        OutMessage = TEXT("UNSAVED_PARTIAL_V5_MAP: hero-only pre-save validation failed. ") +
            (!Error.IsEmpty() ? Error : PreSaveReport);
        return false;
    }
    if (!AssetSubsystem->SaveLoadedAsset(TargetWorld, false))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_V5_MAP: validated duplicate could not be saved. The V1/V2/V3/V4 maps were not overwritten; inspect only the new V5 package before retrying.");
        return false;
    }
    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename))
    {
        OutMessage = TEXT("V5 map save returned success but no destination package exists.");
        return false;
    }
    UWorld* Reloaded = UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    FString PersistedReport;
    if (!ValidateWorldForV5(
            Reloaded,
            DestinationMapPackage,
            &SourceSnapshot,
            PersistedReport))
    {
        OutMessage = TEXT("V5 destination saved but persistence validation failed: ") +
            PersistedReport;
        return false;
    }
    if (!FPackageName::DoesPackageExist(ProtectedV1MapPackage) ||
        !FPackageName::DoesPackageExist(ProtectedV2MapPackage) ||
        !FPackageName::DoesPackageExist(ProtectedV3MapPackage) ||
        !FPackageName::DoesPackageExist(SourceMapPackage))
    {
        OutMessage = TEXT("CRITICAL: a protected V1/V2/V3 map or source V4 map disappeared during additive migration.");
        return false;
    }
    FString ProtectedV1ShaAfter;
    int64 ProtectedV1BytesAfter = 0;
    FString ProtectedV2ShaAfter;
    int64 ProtectedV2BytesAfter = 0;
    FString ProtectedV3ShaAfter;
    int64 ProtectedV3BytesAfter = 0;
    FString SourceV4ShaAfter;
    int64 SourceV4BytesAfter = 0;
    if (!CalculateSha256(
            ProtectedV1Filename,
            ProtectedV1ShaAfter,
            ProtectedV1BytesAfter,
            Error) ||
        ProtectedV1BytesBefore != ProtectedV1BytesAfter ||
        ProtectedV1ShaBefore != ProtectedV1ShaAfter ||
        !CalculateSha256(
            ProtectedV2Filename,
            ProtectedV2ShaAfter,
            ProtectedV2BytesAfter,
            Error) ||
        ProtectedV2BytesBefore != ProtectedV2BytesAfter ||
        ProtectedV2ShaBefore != ProtectedV2ShaAfter ||
        !CalculateSha256(
            ProtectedV3Filename,
            ProtectedV3ShaAfter,
            ProtectedV3BytesAfter,
            Error) ||
        ProtectedV3BytesBefore != ProtectedV3BytesAfter ||
        ProtectedV3ShaBefore != ProtectedV3ShaAfter ||
        !CalculateSha256(
            SourceV4Filename,
            SourceV4ShaAfter,
            SourceV4BytesAfter,
            Error) ||
        SourceV4BytesBefore != SourceV4BytesAfter ||
        SourceV4ShaBefore != SourceV4ShaAfter)
    {
        OutMessage = TEXT("CRITICAL: protected V1/V2/V3 or source V4 map bytes changed during additive migration. ") + Error;
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
        OutMessage = TEXT("V5 destination persistence snapshot failed: ") + Error;
        return false;
    }
    FString InvariantReportPath;
    if (!WriteMigrationInvariantReport(
            ProtectedV1Filename,
            ProtectedV1ShaBefore,
            ProtectedV1ShaAfter,
            ProtectedV1BytesBefore,
            ProtectedV2Filename,
            ProtectedV2ShaBefore,
            ProtectedV2ShaAfter,
            ProtectedV2BytesBefore,
            ProtectedV3Filename,
            ProtectedV3ShaBefore,
            ProtectedV3ShaAfter,
            ProtectedV3BytesBefore,
            SourceV4Filename,
            SourceV4ShaBefore,
            SourceV4ShaAfter,
            SourceV4BytesBefore,
            SourceSnapshot,
            PersistedSnapshot,
            InvariantReportPath,
            Error))
    {
        OutMessage = TEXT("V5 destination saved but invariant report failed: ") + Error;
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Created non-overwriting %s from exact V4 and swapped only BuildingHeroVisualComponent to %s. %s Protected V1 retained SHA-256 %s, protected V2 retained SHA-256 %s, protected V3 retained SHA-256 %s, and source V4 retained SHA-256 %s; collision, terrain/hardscape/context/trees, and cameras remained exact. Invariant report: %s. Nanite, SM6, renderer, project settings, and sensor truth were unchanged."),
        *DestinationMapPackage,
        *HeroMeshObjectPath,
        *PersistedReport,
        *ProtectedV1ShaAfter,
        *ProtectedV2ShaAfter,
        *ProtectedV3ShaAfter,
        *SourceV4ShaAfter,
        *InvariantReportPath);
    return true;
}
}

bool UTRIADIstanaPublicViewHeroV5EditorLibrary::
    ValidateIstanaPublicViewHeroV5RemoteControlProject(
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
            TEXT("Hero-v5 Remote Control project mismatch: connected '%s', expected '%s'. No operation ran."),
            *ActualDirectory,
            *ExpectedDirectory);
        return false;
    }
    OutReport = TEXT("Hero-v5 Remote Control project identity verified: ") +
        ActualDirectory;
    return true;
}

bool UTRIADIstanaPublicViewHeroV5EditorLibrary::
    ImportIstanaPublicViewHeroV5Assets(FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }
    FHeroGeometryContract Geometry;
    FHeroMaterialPack Materials;
    FString SourceReport;
    if (!ValidateCompleteV5SourceSet(
            Geometry, Materials, SourceReport))
    {
        OutMessage = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    return ImportHeroV5AssetSet(
        AssetSubsystem, Geometry, Materials, SourceReport, OutMessage);
}

bool UTRIADIstanaPublicViewHeroV5EditorLibrary::
    ValidateIstanaPublicViewHeroV5Assets(FString& OutReport)
{
    FHeroGeometryContract Geometry;
    FHeroMaterialPack Materials;
    FString SourceReport;
    if (!ValidateCompleteV5SourceSet(
            Geometry, Materials, SourceReport))
    {
        OutReport = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    FString AssetReport;
    if (InspectHeroV5Assets(
            AssetSubsystem,
            Geometry,
            Materials,
            nullptr,
            AssetReport) != EHeroV5AssetState::CompleteValid)
    {
        OutReport = AssetReport;
        return false;
    }
    OutReport = SourceReport + TEXT(" ") + AssetReport;
    return true;
}

bool UTRIADIstanaPublicViewHeroV5EditorLibrary::
    MigrateIstanaPublicViewExteriorMapToHeroV5(FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsEditorOperationSafe(OutMessage))
    {
        return false;
    }
    FHeroGeometryContract Geometry;
    FHeroMaterialPack Materials;
    FString SourceReport;
    if (!ValidateCompleteV5SourceSet(
            Geometry, Materials, SourceReport))
    {
        OutMessage = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    UStaticMesh* Hero = nullptr;
    FString AssetReport;
    if (InspectHeroV5Assets(
            AssetSubsystem,
            Geometry,
            Materials,
            &Hero,
            AssetReport) != EHeroV5AssetState::CompleteValid)
    {
        OutMessage = TEXT("ASSETS_MISSING_OR_INVALID: ") + AssetReport;
        return false;
    }
    return MigrateMapToV5(AssetSubsystem, Hero, OutMessage);
}

bool UTRIADIstanaPublicViewHeroV5EditorLibrary::
    ValidateIstanaPublicViewHeroV5Map(FString& OutReport)
{
    FHeroGeometryContract Geometry;
    FHeroMaterialPack Materials;
    FString SourceReport;
    if (!ValidateCompleteV5SourceSet(Geometry, Materials, SourceReport))
    {
        OutReport = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    FString AssetReport;
    if (InspectHeroV5Assets(
            AssetSubsystem,
            Geometry,
            Materials,
            nullptr,
            AssetReport) != EHeroV5AssetState::CompleteValid)
    {
        OutReport = TEXT("HERO_V5_MAP_INVALID: source/assets are stale. ") + AssetReport;
        return false;
    }
    FString V4AssetReport;
    if (!UTRIADIstanaPublicViewHeroV4EditorLibrary::
            ValidateIstanaPublicViewHeroV4Assets(V4AssetReport))
    {
        OutReport = TEXT("HERO_V5_MAP_INVALID: the protected V4 source asset set is stale. ") +
            V4AssetReport;
        return false;
    }
    if (UPackage* LoadedSource = FindPackage(nullptr, *SourceMapPackage))
    {
        if (LoadedSource->IsDirty())
        {
            OutReport = TEXT("HERO_V5_MAP_INVALID: loaded V4 source package is dirty; save/revert it before exact comparison.");
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
        OutReport = TEXT("HERO_V5_MAP_INVALID: exact V4 source could not be loaded. ") +
            BaselineError;
        return false;
    }
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString MapReport;
    if (!ValidateWorldForV5(
        World,
        DestinationMapPackage,
        &Baseline,
        MapReport))
    {
        OutReport = MapReport;
        return false;
    }
    OutReport = SourceReport + TEXT(" ") + AssetReport + TEXT(" ") +
        V4AssetReport + TEXT(" ") + MapReport;
    return true;
}

bool UTRIADIstanaPublicViewHeroV5EditorLibrary::
    ValidateIstanaPublicViewHeroV5PlayWorldReadiness(FString& OutReport)
{
    FString VisualProfileReport;
    if (!ValidateHeroV5VisualAcceptanceProcessProfile(VisualProfileReport))
    {
        OutReport = TEXT("Hero-V5 PIE readiness refuses an arbitrary sensor process profile. ") +
            VisualProfileReport;
        return false;
    }
    FString EditorStructuralReport;
    if (!ValidateIstanaPublicViewHeroV5Map(EditorStructuralReport))
    {
        OutReport = TEXT("Hero-V5 PIE readiness requires the exact validated V5 editor map. ") +
            EditorStructuralReport;
        return false;
    }

    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE ||
        !PlayWorld->IsGameWorld() || !PlayWorld->HasBegunPlay() ||
        (GEditor && GEditor->IsSimulatingInEditor()))
    {
        OutReport = TEXT("Start authoritative non-simulated PIE in Istana_PublicView_Exterior_v5 before Hero-V5 runtime validation.");
        return false;
    }
    const FString PlaySourcePackage = UWorld::RemovePIEPrefix(
        PlayWorld->GetOutermost()->GetName());
    if (PlaySourcePackage != DestinationMapPackage)
    {
        OutReport = FString::Printf(
            TEXT("Hero-V5 PIE source package is '%s', expected '%s'."),
            *PlaySourcePackage,
            *DestinationMapPackage);
        return false;
    }

    TArray<FString> Errors;
    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    int32 SceneCount = 0;
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(PlayWorld); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == PlayWorld &&
            It->ActorHasTag(SceneActorTag))
        {
            Scene = *It;
            ++SceneCount;
        }
    }
    FString RuntimeSceneReport;
    FString HeroError;
    if (SceneCount != 1 || !Scene || !Scene->HasActorBegunPlay() ||
        !Scene->ValidatePublicViewScene(RuntimeSceneReport) ||
        !ValidateEffectiveHeroV5Materials(
            Scene ? Scene->BuildingHeroVisualComponent.Get() : nullptr,
            HeroError))
    {
        Errors.Add(FString::Printf(
            TEXT("Expected one begun exact Hero-V5 scene (count=%d). Scene='%s' Hero='%s'."),
            SceneCount,
            *RuntimeSceneReport,
            *HeroError));
    }

    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = nullptr;
    int32 PolicyCount = 0;
    for (TActorIterator<ATRIADIstanaPublicViewRuntimePolicyActor> It(PlayWorld); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == PlayWorld &&
            It->ActorHasTag(RuntimePolicyTag))
        {
            Policy = *It;
            ++PolicyCount;
        }
    }
    int32 FogComponentCount = -1;
    FString PrimaryProfileReason;
    const bool bFogClear = Policy &&
        Policy->IsFogSuppressionActive(FogComponentCount);
    const bool bPrimaryProfileReady = Policy &&
        Policy->IsFixedPrimaryCameraProfileActive(PrimaryProfileReason);
    if (PolicyCount != 1 || !Policy || !Policy->HasActorBegunPlay() ||
        Policy->ClaimLabel !=
            ATRIADIstanaPublicViewRuntimePolicyActor::ExpectedClaimLabel() ||
        !Policy->bSuppressAirSimVisualWeather ||
        !Policy->bSuppressExponentialHeightFog ||
        !Policy->bRequireIstanaAirSimGameMode ||
        Policy->RequiredGameModeClassPath != IstanaAirSimGameModeClassPath ||
        !Policy->bEnforceFixedPrimaryCamera ||
        Policy->RequiredPrimaryCameraTag != PrimaryCameraTag ||
        !Policy->bRuntimePolicySettledAtRuntime ||
        !Policy->bGameModeOverrideVerifiedAtRuntime ||
        !Policy->bFogSuppressionVerifiedAtRuntime ||
        !Policy->bPrimaryCameraVerifiedAtRuntime ||
        !bFogClear || FogComponentCount != Policy->FogComponentCountAtRuntime ||
        !bPrimaryProfileReady)
    {
        Errors.Add(FString::Printf(
            TEXT("Exact clear-weather/fog/fixed-camera runtime policy is not settled (count=%d, fog=%d, reason='%s')."),
            PolicyCount,
            FogComponentCount,
            *PrimaryProfileReason));
    }

    AGameModeBase* ActiveGameMode = PlayWorld->GetAuthGameMode();
    if (!ActiveGameMode ||
        ActiveGameMode->GetClass()->GetPathName() !=
            IstanaAirSimGameModeClassPath)
    {
        Errors.Add(TEXT("Hero-V5 PIE does not use the exact TRIAD AirSim GameMode wrapper."));
    }

    ACameraActor* PrimaryCamera = nullptr;
    int32 PrimaryCameraCount = 0;
    for (TActorIterator<ACameraActor> It(PlayWorld); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == PlayWorld &&
            It->ActorHasTag(PrimaryCameraTag))
        {
            PrimaryCamera = *It;
            ++PrimaryCameraCount;
        }
    }
    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(PlayWorld, 0);
    if (PrimaryCameraCount != 1 || !PrimaryCamera || !PlayerController ||
        PlayerController->GetWorld() != PlayWorld ||
        !PlayerController->IsLocalController() ||
        PlayerController->GetViewTarget() != PrimaryCamera)
    {
        Errors.Add(FString::Printf(
            TEXT("Player 0 is not restored to the unique unchanged primary camera (count=%d)."),
            PrimaryCameraCount));
    }

    if (Errors.Num() > 0)
    {
        OutReport = TEXT("Hero-V5 PIE readiness failed:\n - ") +
            FString::Join(Errors, TEXT("\n - "));
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Hero-V5 PIE readiness passed for '%s': exact no-lidar visual profile, editor-map preservation, V5 hero/material bindings, begun scene/policy, clear weather/fog, exact GameMode, and restored primary view are verified. Capture cameras and quality overrides remain transient only. %s"),
        *DestinationMapPackage,
        *VisualProfileReport);
    return true;
}

bool UTRIADIstanaPublicViewHeroV5EditorLibrary::
    CaptureIstanaPublicViewHeroV5QualityFrame(
        const FString& CameraPreset,
        const FString& OutputFileName,
        int32 TemporalWarmupFrames,
        FString& OutMessage)
{
    OutMessage.Reset();
    FString ReadinessReport;
    if (!ValidateIstanaPublicViewHeroV5PlayWorldReadiness(ReadinessReport))
    {
        OutMessage = TEXT("Hero-V5 quality capture requires settled PIE readiness. ") +
            ReadinessReport;
        return false;
    }
    if (TemporalWarmupFrames < 8 || TemporalWarmupFrames > 64)
    {
        OutMessage = TEXT("TemporalWarmupFrames must be in [8,64].");
        return false;
    }

    const FString Preset = CameraPreset.ToUpper();
    FVector LocalCamera;
    FVector LocalTarget;
    float FieldOfView = 0.0f;
    if (Preset == TEXT("HERO_FRONT"))
    {
        LocalCamera = FVector(0.0, 29200.0, 5000.0);
        LocalTarget = FVector(0.0, 500.0, 1350.0);
        FieldOfView = 32.0f;
    }
    else if (Preset == TEXT("HERO_OBLIQUE_RIGHT"))
    {
        LocalCamera = FVector(17500.0, 26500.0, 4500.0);
        LocalTarget = FVector(0.0, 500.0, 1400.0);
        FieldOfView = 28.0f;
    }
    else if (Preset == TEXT("HERO_OBLIQUE_LEFT"))
    {
        LocalCamera = FVector(-17500.0, 26500.0, 4500.0);
        LocalTarget = FVector(0.0, 500.0, 1400.0);
        FieldOfView = 28.0f;
    }
    else if (Preset == TEXT("HERO_FACADE_MACRO"))
    {
        LocalCamera = FVector(0.0, 10000.0, 1800.0);
        LocalTarget = FVector(0.0, 5000.0, 600.0);
        FieldOfView = 58.0f;
    }
    else if (Preset == TEXT("HERO_GROUND_DETAIL"))
    {
        // Human-eye acceptance view: 1.70 m above exact scene-local grade,
        // retained as a wider qualitative ground-level context view.
        LocalCamera = FVector(0.0, 8200.0, 170.0);
        LocalTarget = FVector(0.0, 5200.0, 170.0);
        FieldOfView = 50.0f;
    }
    else if (Preset == TEXT("HERO_MATERIAL_DETAIL"))
    {
        // Playable-distance PBR/edge proof: scene-local public face is y=22.80 m
        // and the outer stair edge is y=27.60 m. This transient camera is
        // outside the stair at y=40.00 m, exactly 17.20 m from that face, with
        // a 1.70 m eye height and a narrow central arch/door/reveal composition.
        LocalCamera = FVector(0.0, 4000.0, 170.0);
        LocalTarget = FVector(0.0, 2280.0, 420.0);
        FieldOfView = 42.0f;
    }
    else if (Preset == TEXT("HERO_ORBIT_RIGHT"))
    {
        LocalCamera = FVector(27000.0, 12000.0, 4800.0);
        LocalTarget = FVector(0.0, 1000.0, 1400.0);
        FieldOfView = 30.0f;
    }
    else if (Preset == TEXT("HERO_ORBIT_LEFT"))
    {
        LocalCamera = FVector(-27000.0, 12000.0, 4800.0);
        LocalTarget = FVector(0.0, 1000.0, 1400.0);
        FieldOfView = 30.0f;
    }
    else
    {
        OutMessage = TEXT("CameraPreset must be HERO_FRONT, HERO_OBLIQUE_RIGHT, HERO_OBLIQUE_LEFT, HERO_FACADE_MACRO, HERO_GROUND_DETAIL, HERO_MATERIAL_DETAIL, HERO_ORBIT_RIGHT, or HERO_ORBIT_LEFT.");
        return false;
    }

    if (OutputFileName.IsEmpty() || OutputFileName.Len() > 128 ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        !OutputFileName.StartsWith(
            TEXT("ipv_v5_quality_"), ESearchCase::CaseSensitive) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::CaseSensitive))
    {
        OutMessage = TEXT("OutputFileName must be a leaf-only 'ipv_v5_quality_*.png' name of at most 128 characters.");
        return false;
    }
    for (const TCHAR Character : OutputFileName)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_') &&
            Character != TEXT('-') && Character != TEXT('.'))
        {
            OutMessage = TEXT("OutputFileName contains a character outside [A-Za-z0-9_.-].");
            return false;
        }
    }

    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld ||
        UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName()) !=
            DestinationMapPackage)
    {
        OutMessage = TEXT("The active PIE world is not the exact Hero-V5 map.");
        return false;
    }
    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    int32 SceneCount = 0;
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(PlayWorld); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == PlayWorld &&
            It->ActorHasTag(SceneActorTag))
        {
            Scene = *It;
            ++SceneCount;
        }
    }
    const FTransform SceneFrame = Scene
        ? Scene->GetActorTransform()
        : FTransform::Identity;
    if (SceneCount != 1 || !Scene || !Scene->HasActorBegunPlay() ||
        !SceneFrame.IsValid())
    {
        OutMessage = TEXT("Quality capture requires one begun tagged scene with a valid rebased transform.");
        return false;
    }
    const FVector WorldCamera = SceneFrame.TransformPosition(LocalCamera);
    const FVector WorldTarget = SceneFrame.TransformPosition(LocalTarget);
    const auto IsFiniteVector = [](const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    };
    if (!IsFiniteVector(WorldCamera) || !IsFiniteVector(WorldTarget) ||
        (WorldTarget - WorldCamera).IsNearlyZero())
    {
        OutMessage = TEXT("Scene-local quality-camera transform produced an invalid world-space view.");
        return false;
    }

    ACameraActor* PrimaryCamera = nullptr;
    int32 PrimaryCameraCount = 0;
    for (TActorIterator<ACameraActor> It(PlayWorld); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == PlayWorld &&
            It->ActorHasTag(PrimaryCameraTag))
        {
            PrimaryCamera = *It;
            ++PrimaryCameraCount;
        }
    }
    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(PlayWorld, 0);
    APlayerCameraManager* CameraManager = PlayerController
        ? PlayerController->PlayerCameraManager
        : nullptr;
    UGameViewportClient* ViewportClient = PlayWorld->GetGameViewport();
    FSceneViewport* Viewport = ViewportClient
        ? ViewportClient->GetGameViewport()
        : nullptr;
    if (PrimaryCameraCount != 1 || !PrimaryCamera || !PlayerController ||
        !CameraManager || !Viewport || !PlayerController->IsLocalController())
    {
        OutMessage = TEXT("The unique primary camera, Player 0 camera manager, or PIE viewport is unavailable.");
        return false;
    }
    if (FScreenshotRequest::IsScreenshotRequested())
    {
        OutMessage = TEXT("Another Unreal screenshot request is pending; no Hero-V5 capture was scheduled.");
        return false;
    }

    const FString OutputDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/IstanaPreviews/PublicViewHeroV5")));
    const FString DestinationPath = FPaths::Combine(
        OutputDirectory, OutputFileName);
    if (IFileManager::Get().FileExists(*DestinationPath))
    {
        OutMessage = FString::Printf(
            TEXT("Refusing to overwrite existing Hero-V5 capture '%s'."),
            *DestinationPath);
        return false;
    }
    if (!IFileManager::Get().DirectoryExists(*OutputDirectory) &&
        !IFileManager::Get().MakeDirectory(*OutputDirectory, true))
    {
        OutMessage = FString::Printf(
            TEXT("Could not create isolated Hero-V5 capture directory '%s'."),
            *OutputDirectory);
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.ObjectFlags |= RF_Transient;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACameraActor* CaptureCamera = PlayWorld->SpawnActor<ACameraActor>(
        ACameraActor::StaticClass(),
        WorldCamera,
        (WorldTarget - WorldCamera).Rotation(),
        SpawnParameters);
    UCameraComponent* CaptureComponent = CaptureCamera
        ? CaptureCamera->GetCameraComponent()
        : nullptr;
    if (!CaptureCamera || !CaptureComponent)
    {
        OutMessage = TEXT("Could not spawn the transient Hero-V5 QA camera.");
        return false;
    }
    CaptureCamera->Tags.AddUnique(
        FName(TEXT("TRIADIstanaPublicViewHeroV5QualityCaptureOnly")));
    ConfigureHeroV5D65CaptureCamera(CaptureComponent, FieldOfView);
    if (!HasHeroV5D65CaptureCamera(CaptureComponent, FieldOfView))
    {
        CaptureCamera->Destroy();
        OutMessage = TEXT("Transient camera rejected the exact D65/manual-exposure/no-LUT QA profile.");
        return false;
    }

    const bool bViewportWasFixed = Viewport->HasFixedSize();
    const FIntPoint OriginalViewportSize = Viewport->GetSizeXY();
    const auto RestoreViewport = [Viewport, bViewportWasFixed, OriginalViewportSize]()
    {
        if (bViewportWasFixed && OriginalViewportSize.X > 0 &&
            OriginalViewportSize.Y > 0)
        {
            Viewport->SetFixedViewportSize(
                OriginalViewportSize.X, OriginalViewportSize.Y);
        }
        else
        {
            Viewport->SetFixedViewportSize(0, 0);
        }
    };
    const auto RestorePrimaryAndDestroy =
        [PlayerController, CameraManager, PrimaryCamera, CaptureCamera]()
    {
        PlayerController->SetViewTargetWithBlend(PrimaryCamera, 0.0f);
        CameraManager->UpdateCamera(0.0f);
        if (IsValid(CaptureCamera))
        {
            CaptureCamera->Destroy();
        }
    };

    Viewport->SetFixedViewportSize(3840, 2160);
    if (Viewport->GetSizeXY() != FIntPoint(3840, 2160))
    {
        RestorePrimaryAndDestroy();
        RestoreViewport();
        OutMessage = TEXT("PIE viewport rejected the exact 3840x2160 Hero-V5 QA size.");
        return false;
    }
    PlayerController->SetViewTargetWithBlend(CaptureCamera, 0.0f);
    CameraManager->UpdateCamera(0.0f);
    FMinimalViewInfo ExpectedView;
    CaptureComponent->GetCameraView(0.0f, ExpectedView);
    const FMinimalViewInfo& CachedView = CameraManager->GetCameraCacheView();
    if (PlayerController->GetViewTarget() != CaptureCamera ||
        !CachedView.Location.Equals(ExpectedView.Location, 0.1) ||
        !CachedView.Rotation.Equals(ExpectedView.Rotation, 0.01) ||
        !FMath::IsNearlyEqual(CachedView.FOV, ExpectedView.FOV, 0.01f))
    {
        RestorePrimaryAndDestroy();
        RestoreViewport();
        OutMessage = TEXT("Player 0 cached POV did not match the transient Hero-V5 QA camera.");
        return false;
    }

    FScopedHeroV5TsrCaptureQuality Quality;
    FString QualityError;
    if (!Quality.Apply(QualityError))
    {
        RestorePrimaryAndDestroy();
        RestoreViewport();
        OutMessage = QualityError;
        return false;
    }
    for (int32 Frame = 0; Frame < TemporalWarmupFrames; ++Frame)
    {
        Viewport->Draw(false);
    }
    FScreenshotRequest::RequestScreenshot(
        DestinationPath, false, false, false);
    Viewport->Draw(false);

    const bool bRequestProcessed =
        !FScreenshotRequest::IsScreenshotRequested();
    const int64 CaptureBytes = IFileManager::Get().FileSize(*DestinationPath);
    const bool bFileWritten = CaptureBytes > 24;
    if (!bRequestProcessed || !bFileWritten)
    {
        FScreenshotRequest::Reset();
        FString RestoreQualityError;
        const bool bQualityRestored = Quality.Restore(RestoreQualityError);
        RestorePrimaryAndDestroy();
        RestoreViewport();
        OutMessage = FString::Printf(
            TEXT("Hero-V5 synchronous QA capture failed for '%s' (requestProcessed=%s, fileBytes=%lld, qualityRestored=%s). %s"),
            *Preset,
            bRequestProcessed ? TEXT("true") : TEXT("false"),
            CaptureBytes,
            bQualityRestored ? TEXT("true") : TEXT("false"),
            *RestoreQualityError);
        return false;
    }

    FString RestoreQualityError;
    const bool bQualityRestored = Quality.Restore(RestoreQualityError);
    RestorePrimaryAndDestroy();
    RestoreViewport();
    FString RestoredReadiness;
    const bool bRuntimeRestored =
        ValidateIstanaPublicViewHeroV5PlayWorldReadiness(RestoredReadiness);
    if (!bQualityRestored || !bRuntimeRestored)
    {
        OutMessage = FString::Printf(
            TEXT("Hero-V5 image '%s' was written, but transient-state restoration failed (quality=%s, runtime=%s). Quality='%s' Runtime='%s'."),
            *DestinationPath,
            bQualityRestored ? TEXT("true") : TEXT("false"),
            bRuntimeRestored ? TEXT("true") : TEXT("false"),
            *RestoreQualityError,
            *RestoredReadiness);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("Captured 3840x2160 Hero-V5 QA preset=%s path='%s' bytes=%lld after %d settled TSR frames. Camera-local profile: D65 6500K, manual ISO100 1/125s, exposure bias 0 EV, ScreenSpace GI/reflection, no LUT/bloom/vignette/motion blur/grain; UE project filmic tonemapper remains unmodified and external display/OCIO is not certified. Scoped capture overrides r.AntiAliasingMethod=TSR, r.ScreenPercentage=100, r.TSR.History.ScreenPercentage=200, r.SSGI.Quality=4, and r.SSR.Quality=4 were applied, then every CVar was restored exactly to its pre-capture value. The scene-local transient camera followed world-origin rebasing; V1/V2/V3/V4/V5 maps, surroundings, project settings, and renderer config were not saved or mutated. %s"),
        *Preset,
        *DestinationPath,
        CaptureBytes,
        TemporalWarmupFrames,
        *RestoredReadiness);
    return true;
}

bool UTRIADIstanaPublicViewHeroV5EditorLibrary::
    QuiesceIstanaPublicViewHeroV5PlayWorldForStop(FString& OutMessage)
{
    OutMessage.Reset();
    FString FreezeReport;
    if (!ValidateIntegrationFreeze(FreezeReport))
    {
        OutMessage = TEXT("Hero-V5 quiescence is disabled until the exact integration freeze is bound. ") +
            FreezeReport;
        return false;
    }
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE)
    {
        OutMessage = TEXT("No Hero-V5 PIE PlayWorld exists to quiesce.");
        return false;
    }
    const FString SourcePackage = UWorld::RemovePIEPrefix(
        PlayWorld->GetOutermost()->GetName());
    if (SourcePackage != DestinationMapPackage)
    {
        OutMessage = FString::Printf(
            TEXT("Refusing Hero-V5 quiescence for PIE source '%s'; expected '%s'."),
            *SourcePackage,
            *DestinationMapPackage);
        return false;
    }

    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = nullptr;
    int32 PolicyCount = 0;
    int32 SceneCount = 0;
    for (TActorIterator<ATRIADIstanaPublicViewRuntimePolicyActor> It(PlayWorld); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == PlayWorld &&
            It->ActorHasTag(RuntimePolicyTag))
        {
            Policy = *It;
            ++PolicyCount;
        }
    }
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(PlayWorld); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == PlayWorld &&
            It->ActorHasTag(SceneActorTag))
        {
            ++SceneCount;
        }
    }
    AGameModeBase* ActiveGameMode = PlayWorld->GetAuthGameMode();
    if (PolicyCount != 1 || SceneCount != 1 || !Policy ||
        !Policy->HasActorBegunPlay() || !ActiveGameMode ||
        ActiveGameMode->GetClass()->GetPathName() !=
            IstanaAirSimGameModeClassPath)
    {
        OutMessage = FString::Printf(
            TEXT("Exact Hero-V5 PIE identity failed before quiescence: policy=%d scene=%d gameMode='%s'. PIE was not stopped."),
            PolicyCount,
            SceneCount,
            ActiveGameMode
                ? *ActiveGameMode->GetClass()->GetPathName()
                : TEXT("<none>"));
        return false;
    }

    int32 SimModeCount = 0;
    if (!Policy->RequestAirSimQuiescenceForTeardown(SimModeCount))
    {
        OutMessage = FString::Printf(
            TEXT("Hero-V5 AirSim quiescence failed for %d discovered simulation mode(s); PIE was not stopped."),
            SimModeCount);
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Hero-V5 PIE is quiescent: exact V5 world/policy/scene/GameMode identity passed and %d AirSim simulation mode(s) are paused. No map or asset was saved."),
        SimModeCount);
    return true;
}
