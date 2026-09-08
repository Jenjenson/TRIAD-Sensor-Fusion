#include "TRIADIstanaPublicViewEditorLibrary.h"
#include "TRIADIstanaPublicViewHeroV2EditorLibrary.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EditorFramework/AssetImportData.h"
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
#include "IAssetTools.h"
#include "Kismet/GameplayStatics.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Slate/SceneViewport.h"
#include "StaticMeshCompiler.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaPublicViewRuntimePolicyActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealClient.h"

namespace
{
const FString DestinationMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Exterior_v1"));
const FString HeroV2DestinationMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Exterior_v2"));
const FString PublicViewAssetRoot(TEXT("/Game/TRIAD/IstanaPublicView"));
const FString PublicViewMaterialPath(TEXT("/Game/TRIAD/IstanaPublicView/Materials"));
const FString PublicViewTexturePath(TEXT("/Game/TRIAD/IstanaPublicView/Textures"));
const FString GeneratedSourceRelativeDirectory(
    TEXT("SourceAssets/IstanaPublicView/Generated"));
const FString OsmContextSourceRelativeDirectory(
    TEXT("SourceAssets/IstanaPublicView/GeneratedOptional/OSMContext"));
const FString OsmContextContractRelativePath(
    TEXT("SourceAssets/IstanaPublicView/istana_public_view_osm_context.contract.json"));
const FString OsmContextManifestRelativePath(
    TEXT("SourceAssets/IstanaPublicView/GeneratedOptional/OSMContext/IstanaPublicViewOSMContext.manifest.json"));
const FString TextureSourceRelativeDirectory(
    TEXT("SourceAssets/IstanaPublicView/Textures/Generated"));
const FString TextureManifestRelativePath(
    TEXT("SourceAssets/IstanaPublicView/Textures/Generated/manifest.json"));
const FString MaterialSlotMappingRelativePath(
    TEXT("SourceAssets/IstanaPublicView/Textures/material_slot_mapping.json"));
const FString PublicViewClaimLabel(
    TEXT("PUBLIC_REFERENCE_VISUAL_APPROXIMATION_NOT_SURVEY_CONTROLLED"));
const FString PublicViewMaterialTier(
    TEXT("ORIGINAL_AI_ASSISTED_PBR_TEXTURES_INTEGRATED_PHOTO_QA_NOT_ACCEPTED"));
const FString PublicViewDisplayPolicy(
    TEXT("NEUTRAL_CAMERA_LOCAL_EXPOSURE_DISABLED_EXTERNAL_SRGB_REC709_DISPLAY_NOT_OCIO_VALIDATED"));
const FString LegacyObjExportContract(
    TEXT("UE55_LEGACY_OBJ_Y_MIRROR_WINDING_NORMAL_AND_UV_V_PRECONDITION"));
const FString SurfaceUvEncoding(
    TEXT("continuous source-metre projections; one UV unit equals one metre before material scaling"));
const FString SurfacePlanarProjection(
    TEXT("dominant-axis world-oriented source metres"));
const FString SurfaceCurvedProjection(
    TEXT("primitive-local metre-scaled arc length and axial distance"));
const FString SurfaceTerrainProjection(TEXT("local XY source metres"));
const FString SurfaceEncodedUvPrecondition(
    TEXT("disk vt=(logicalU,1-logicalV); UE5.5 legacy importer V flip restores logical UV0 exactly"));
const FString SurfaceHardEdges(
    TEXT("face normals for planar architecture, primitive caps, simple collision, planar hardscape and anonymous context massing"));
const FString SurfaceSmoothEdges(
    TEXT("area-weighted shared-position normals within named groups for terrain, curved details, cylindrical vegetation and crown surfaces"));
const FString FrozenSourceContractSha256(
    TEXT("c068cf3ceb731c85b28e3c8e6292fe92bb5463e792742dd4a0b81525bd84baee"));
const FString FrozenBuildingManifestSha256(
    TEXT("46856ad01439fda15abd1c6e21bc74d0d7ec4a763fc89a221ebe405e79c6302f"));
const FString FrozenContextManifestSha256(
    TEXT("887267787dcc716ed07b2468e39d5202b6b032ca7704791567302ceac81e4774"));
const FString FrozenTextureManifestSha256(
    TEXT("2b59b328a4e5f4963d4fb3b006eba7d12f42316b981e88b8aeebd0718643a87a"));
const FString FrozenMaterialSlotMappingSha256(
    TEXT("35f1ab2db3251133fafe7c4aae94cc750e198a0b1f3a33a80dab7effb393cea6"));
const FString FrozenOsmContextContractSha256(
    TEXT("413c9c215da8d48503e5d39fa5793c98fd02fb9ebde131bd5c99353bf9140b96"));
const FString FrozenOsmContextManifestSha256(
    TEXT("2733484bbc4a0598452e122f6bdc2a98a62bef50c6f84def15850d0d2d8c957c"));
const FString FrozenOsmContextMeshSha256(
    TEXT("4026a4a99170eade125156b7b52e5b92d3e61215dbf0df38011a5432f93f1d77"));
const FString OsmContextStatus(
    TEXT("OPTIONAL_MAPPING_GRADE_VISUAL_CONTEXT_NOT_ACCEPTED"));
const FString OsmContextAlignmentStatus(
    TEXT("COORDINATE_CONTRACT_ALIGNED_NOT_VISUALLY_ACCEPTED"));
const FString OsmContextSceneStatus(
    TEXT("COORDINATE_CONTRACT_ALIGNED_MAPPING_GRADE_NOT_VISUALLY_OR_PHOTO_ACCEPTED"));
const FString OsmAttribution(TEXT("© OpenStreetMap contributors"));
const FString OsmLicenceUrl(TEXT("https://www.openstreetmap.org/copyright"));
const FString ExternalAirSimGameModeClassPath(
    TEXT("/Script/AirSimTriadRuntime.AirSimGameMode"));
const FString IstanaAirSimGameModeClassPath(
    TEXT("/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode"));
const FString InstanceSchema(TEXT("triad.istana_public_view_instances.v1"));
const FString InstanceCoordinateSystem(
    TEXT("right-handed Z-up local metres; ceremonial approach +Y"));
const FName SceneActorTag(TEXT("TRIADIstanaPublicViewScene_v1"));
const FName RuntimePolicyTag(TEXT("TRIADIstanaPublicViewRuntimePolicy_v1"));
const FName PrimaryCameraTag(TEXT("TRIADIstanaPublicViewCamera_Primary"));
const FName LightingTag(TEXT("TRIADIstanaPublicViewLighting_v1"));
constexpr int32 RequiredTreeInstanceCount = 600;
constexpr int32 RequiredHeroTreeInstanceCount = 120;
constexpr double HeroTreeRadiusMeters = 250.0;
constexpr double ContextRadiusMeters = 1000.0;

class FPublicViewSha256
{
public:
    FPublicViewSha256()
    {
        State[0] = 0x6a09e667u;
        State[1] = 0xbb67ae85u;
        State[2] = 0x3c6ef372u;
        State[3] = 0xa54ff53au;
        State[4] = 0x510e527fu;
        State[5] = 0x9b05688cu;
        State[6] = 0x1f83d9abu;
        State[7] = 0x5be0cd19u;
    }

    void Update(const uint8* Bytes, int64 ByteCount)
    {
        for (int64 Index = 0; Index < ByteCount; ++Index)
        {
            Block[BlockLength++] = Bytes[Index];
            if (BlockLength == 64)
            {
                Transform();
                BitLength += 512;
                BlockLength = 0;
            }
        }
    }

    FString Final()
    {
        uint32 Index = BlockLength;
        Block[Index++] = 0x80;
        if (Index > 56)
        {
            while (Index < 64)
            {
                Block[Index++] = 0;
            }
            Transform();
            Index = 0;
        }
        while (Index < 56)
        {
            Block[Index++] = 0;
        }
        BitLength += static_cast<uint64>(BlockLength) * 8u;
        for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
        {
            Block[63 - ByteIndex] = static_cast<uint8>(
                BitLength >> (ByteIndex * 8));
        }
        Transform();

        FString Digest;
        Digest.Reserve(64);
        for (int32 StateIndex = 0; StateIndex < 8; ++StateIndex)
        {
            for (int32 ByteIndex = 3; ByteIndex >= 0; --ByteIndex)
            {
                Digest += FString::Printf(
                    TEXT("%02x"),
                    static_cast<uint8>(State[StateIndex] >> (ByteIndex * 8)));
            }
        }
        return Digest;
    }

private:
    static uint32 RotateRight(uint32 Value, uint32 Shift)
    {
        return (Value >> Shift) | (Value << (32u - Shift));
    }

    void Transform()
    {
        static const uint32 RoundConstants[64] = {
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
        uint32 Words[64];
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const int32 Offset = Index * 4;
            Words[Index] =
                (static_cast<uint32>(Block[Offset]) << 24) |
                (static_cast<uint32>(Block[Offset + 1]) << 16) |
                (static_cast<uint32>(Block[Offset + 2]) << 8) |
                static_cast<uint32>(Block[Offset + 3]);
        }
        for (int32 Index = 16; Index < 64; ++Index)
        {
            const uint32 Sigma0 =
                RotateRight(Words[Index - 15], 7) ^
                RotateRight(Words[Index - 15], 18) ^
                (Words[Index - 15] >> 3);
            const uint32 Sigma1 =
                RotateRight(Words[Index - 2], 17) ^
                RotateRight(Words[Index - 2], 19) ^
                (Words[Index - 2] >> 10);
            Words[Index] = Words[Index - 16] + Sigma0 +
                Words[Index - 7] + Sigma1;
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
            const uint32 Choice = (E & F) ^ ((~E) & G);
            const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
            const uint32 BigSigma0 =
                RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
            const uint32 BigSigma1 =
                RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
            const uint32 Temp1 = H + BigSigma1 + Choice +
                RoundConstants[Index] + Words[Index];
            const uint32 Temp2 = BigSigma0 + Majority;
            H = G;
            G = F;
            F = E;
            E = D + Temp1;
            D = C;
            C = B;
            B = A;
            A = Temp1 + Temp2;
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

    uint8 Block[64] = {0};
    uint32 BlockLength = 0;
    uint64 BitLength = 0;
    uint32 State[8] = {0};
};

struct FPublicViewMeshSpec
{
    const TCHAR* SourceFile;
    const TCHAR* DestinationPath;
    const TCHAR* AssetName;
    FVector MinimumBoxExtentCentimeters;
    bool bUseComplexCollision;
    bool bBuildNanite;
    TArray<FName> RequiredMaterialSlots;
    bool bUsesOptionalOsmSource = false;
};

const TArray<FPublicViewMeshSpec>& GetMeshSpecs()
{
    static const TArray<FPublicViewMeshSpec> Specs = {
        {
            TEXT("SM_IstanaPublicView_Building_Hero.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Building"),
            TEXT("SM_IstanaPublicView_Building_Hero"),
            FVector(5000.0, 5000.0, 1000.0),
            false,
            false,
            {TEXT("M_IPV_Render"), TEXT("M_IPV_Trim"), TEXT("M_IPV_Slate"),
             TEXT("M_IPV_Shutter"), TEXT("M_IPV_Glass"), TEXT("M_IPV_Stone"),
             TEXT("M_IPV_Metal"), TEXT("M_IPV_Door"), TEXT("M_IPV_DarkTimber"),
             TEXT("M_IPV_Opaline")}
        },
        {
            TEXT("SM_IstanaPublicView_Building_Collision.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Building"),
            TEXT("SM_IstanaPublicView_Building_Collision"),
            FVector(5000.0, 5000.0, 800.0),
            true,
            false,
            {TEXT("M_IPV_Collision")}
        },
        {
            TEXT("SM_IstanaPublicView_Terrain.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Ground"),
            TEXT("SM_IstanaPublicView_Terrain"),
            FVector(97500.0, 97500.0, 50.0),
            true,
            true,
            {TEXT("M_IPV_Lawn")}
        },
        {
            TEXT("SM_IstanaPublicView_TerrainSkirt.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Ground"),
            TEXT("SM_IstanaPublicView_TerrainSkirt"),
            FVector(97500.0, 97500.0, 1000.0),
            false,
            false,
            {TEXT("M_IPV_TerrainSkirt")}
        },
        {
            TEXT("SM_IstanaPublicView_Hardscape.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Ground"),
            TEXT("SM_IstanaPublicView_Hardscape"),
            FVector(4500.0, 1500.0, 500.0),
            true,
            false,
            {TEXT("M_IPV_Stone"), TEXT("M_IPV_Water"), TEXT("M_IPV_Metal"),
             TEXT("M_IPV_Planting")}
        },
        {
            TEXT("SM_IstanaPublicView_ContextBuildings.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Context"),
            TEXT("SM_IstanaPublicView_ContextBuildings"),
            FVector(20000.0, 20000.0, 100.0),
            false,
            false,
            {TEXT("M_IPV_ContextRender"), TEXT("M_IPV_ContextGlass"),
             TEXT("M_IPV_ContextRoof")}
        },
        {
            TEXT("SM_IstanaPublicView_OSMContextBuildings.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Context"),
            TEXT("SM_IstanaPublicView_OSMContextBuildings"),
            FVector(95000.0, 95000.0, 5000.0),
            false,
            true,
            {TEXT("M_IPV_ContextRender"), TEXT("M_IPV_ContextRoof")},
            true
        },
        {
            TEXT("SM_IstanaPublicView_Tree_Rain.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Vegetation"),
            TEXT("SM_IstanaPublicView_Tree_Rain"),
            FVector(10.0, 10.0, 10.0),
            false,
            true,
            {TEXT("M_IPV_Bark"), TEXT("M_IPV_LeafDark"), TEXT("M_IPV_LeafMid"),
             TEXT("M_IPV_LeafLight")}
        },
        {
            TEXT("SM_IstanaPublicView_Tree_Palm.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Vegetation"),
            TEXT("SM_IstanaPublicView_Tree_Palm"),
            FVector(10.0, 10.0, 10.0),
            false,
            true,
            {TEXT("M_IPV_Bark"), TEXT("M_IPV_LeafDark"), TEXT("M_IPV_LeafMid"),
             TEXT("M_IPV_LeafLight")}
        },
        {
            TEXT("SM_IstanaPublicView_Tree_Framing.obj"),
            TEXT("/Game/TRIAD/IstanaPublicView/Vegetation"),
            TEXT("SM_IstanaPublicView_Tree_Framing"),
            FVector(10.0, 10.0, 10.0),
            false,
            true,
            {TEXT("M_IPV_Bark"), TEXT("M_IPV_LeafDark"), TEXT("M_IPV_LeafMid"),
             TEXT("M_IPV_LeafLight")}
        }};
    return Specs;
}

struct FPublicViewExpectedImportedBounds
{
    const TCHAR* AssetName;
    FVector OriginCentimeters;
    FVector ExtentCentimeters;
};

const TArray<FPublicViewExpectedImportedBounds>& GetExpectedImportedBounds()
{
    // Exact source-manifest AABBs in centimetres. Asymmetric origins are a
    // fail-closed orientation sentinel: an axis flip cannot silently pass.
    static const TArray<FPublicViewExpectedImportedBounds> Bounds = {
        {TEXT("SM_IstanaPublicView_Building_Hero"), FVector(0.0, 645.25, 1800.0), FVector(6360.0, 6245.25, 1800.0)},
        {TEXT("SM_IstanaPublicView_Building_Collision"), FVector(0.0, 75.0, 1400.0), FVector(6250.0, 5675.0, 1400.0)},
        {TEXT("SM_IstanaPublicView_Terrain"), FVector(0.0, 0.0, 26.076140), FVector(100000.0, 100000.0, 160.117335)},
        {TEXT("SM_IstanaPublicView_TerrainSkirt"), FVector(0.0, 0.0, -1532.661539), FVector(100000.0, 100000.0, 1718.855014)},
        {TEXT("SM_IstanaPublicView_Hardscape"), FVector(0.0, 9321.5, 715.219473), FVector(4800.0, 1528.5, 700.0)},
        {TEXT("SM_IstanaPublicView_ContextBuildings"), FVector(-303.654400, -375.491750, 5062.002400), FVector(94772.251150, 94839.041450, 5196.244100)},
        {TEXT("SM_IstanaPublicView_OSMContextBuildings"), FVector(724.501314, 382.382451, 5734.645422), FVector(99156.518506, 99020.356853, 5868.321182)},
        {TEXT("SM_IstanaPublicView_Tree_Rain"), FVector(-67.8, -52.646107, 892.4), FVector(1124.2, 1119.950389, 892.4)},
        {TEXT("SM_IstanaPublicView_Tree_Palm"), FVector(-19.964621, -14.489765, 809.470174), FVector(583.106321, 601.142949, 809.470174)},
        {TEXT("SM_IstanaPublicView_Tree_Framing"), FVector(15.490730, 17.836572, 836.6), FVector(826.775491, 844.615961, 836.6)}};
    return Bounds;
}

enum class EPublicViewShaderKind : uint8
{
    PbrTextured,
    SpecialParameter,
    HiddenCollisionOnly
};

enum class EPublicViewTextureUsage : uint8
{
    BaseColor,
    Normal,
    PackedOrm
};

struct FPublicViewMaterialSpec
{
    const TCHAR* AssetName;
    EPublicViewShaderKind ShaderKind;
    const TCHAR* TextureSet;
    float UvMetersPerTile;
    FLinearColor TintOrBaseColor;
    float Roughness;
    float Metallic;
    bool bTranslucent = false;
    float Opacity = 1.0f;
};

struct FPublicViewTextureSpec
{
    const TCHAR* TextureSet;
    const TCHAR* AssetName;
    EPublicViewTextureUsage Usage;
    int64 FrozenBytes;
    const TCHAR* FrozenSha256;
};

struct FPublicViewFrozenSourceFile
{
    const TCHAR* RelativePath;
    const TCHAR* Role;
    int64 FrozenBytes;
    const TCHAR* FrozenSha256;
};

const TArray<FPublicViewMaterialSpec>& GetMaterialSpecs()
{
    static const TArray<FPublicViewMaterialSpec> Specs = {
        {TEXT("M_IPV_Render"), EPublicViewShaderKind::PbrTextured, TEXT("Plaster"), 2.0f, FLinearColor(1.0f, 1.0f, 1.0f), 0.0f, 0.0f},
        {TEXT("M_IPV_Trim"), EPublicViewShaderKind::PbrTextured, TEXT("Plaster"), 1.5f, FLinearColor(1.015f, 1.01f, 1.0f), 0.0f, 0.0f},
        {TEXT("M_IPV_Slate"), EPublicViewShaderKind::PbrTextured, TEXT("Slate"), 1.2f, FLinearColor(1.0f, 1.0f, 1.0f), 0.0f, 0.0f},
        {TEXT("M_IPV_Shutter"), EPublicViewShaderKind::PbrTextured, TEXT("Shutter"), 0.8f, FLinearColor(1.0f, 1.0f, 1.0f), 0.0f, 0.0f},
        {TEXT("M_IPV_Stone"), EPublicViewShaderKind::PbrTextured, TEXT("Stone"), 1.5f, FLinearColor(1.0f, 1.0f, 1.0f), 0.0f, 0.0f},
        {TEXT("M_IPV_Door"), EPublicViewShaderKind::PbrTextured, TEXT("DarkTimber"), 1.0f, FLinearColor(0.82f, 0.74f, 0.66f), 0.0f, 0.0f},
        {TEXT("M_IPV_DarkTimber"), EPublicViewShaderKind::PbrTextured, TEXT("DarkTimber"), 1.0f, FLinearColor(0.72f, 0.66f, 0.60f), 0.0f, 0.0f},
        {TEXT("M_IPV_Bark"), EPublicViewShaderKind::PbrTextured, TEXT("Bark"), 2.0f, FLinearColor(1.0f, 1.0f, 1.0f), 0.0f, 0.0f},
        {TEXT("M_IPV_LeafDark"), EPublicViewShaderKind::PbrTextured, TEXT("Foliage"), 1.6f, FLinearColor(0.50f, 0.66f, 0.45f), 0.0f, 0.0f},
        {TEXT("M_IPV_LeafMid"), EPublicViewShaderKind::PbrTextured, TEXT("Foliage"), 1.6f, FLinearColor(0.68f, 0.82f, 0.58f), 0.0f, 0.0f},
        {TEXT("M_IPV_LeafLight"), EPublicViewShaderKind::PbrTextured, TEXT("Foliage"), 1.6f, FLinearColor(0.82f, 0.92f, 0.70f), 0.0f, 0.0f},
        {TEXT("M_IPV_Lawn"), EPublicViewShaderKind::PbrTextured, TEXT("Lawn"), 4.0f, FLinearColor(0.72f, 0.82f, 0.62f), 0.0f, 0.0f},
        {TEXT("M_IPV_ContextRender"), EPublicViewShaderKind::PbrTextured, TEXT("Plaster"), 4.0f, FLinearColor(0.76f, 0.80f, 0.84f), 0.0f, 0.0f},
        {TEXT("M_IPV_ContextRoof"), EPublicViewShaderKind::PbrTextured, TEXT("Slate"), 3.0f, FLinearColor(0.74f, 0.76f, 0.80f), 0.0f, 0.0f},
        {TEXT("M_IPV_Glass"), EPublicViewShaderKind::SpecialParameter, nullptr, 0.0f, FLinearColor(0.07f, 0.12f, 0.14f), 0.14f, 0.0f, true, 0.28f},
        {TEXT("M_IPV_Metal"), EPublicViewShaderKind::SpecialParameter, nullptr, 0.0f, FLinearColor(0.16f, 0.17f, 0.18f), 0.28f, 0.82f},
        {TEXT("M_IPV_Opaline"), EPublicViewShaderKind::SpecialParameter, nullptr, 0.0f, FLinearColor(0.92f, 0.89f, 0.76f), 0.22f, 0.0f},
        {TEXT("M_IPV_Collision"), EPublicViewShaderKind::HiddenCollisionOnly, nullptr, 0.0f, FLinearColor(0.05f, 0.05f, 0.05f), 1.0f, 0.0f},
        {TEXT("M_IPV_Water"), EPublicViewShaderKind::SpecialParameter, nullptr, 0.0f, FLinearColor(0.015f, 0.11f, 0.13f), 0.05f, 0.0f, true, 0.55f},
        {TEXT("M_IPV_Planting"), EPublicViewShaderKind::SpecialParameter, nullptr, 0.0f, FLinearColor(0.11f, 0.065f, 0.035f), 0.92f, 0.0f},
        {TEXT("M_IPV_ContextGlass"), EPublicViewShaderKind::SpecialParameter, nullptr, 0.0f, FLinearColor(0.09f, 0.15f, 0.19f), 0.18f, 0.08f, true, 0.42f},
        {TEXT("M_IPV_TerrainSkirt"), EPublicViewShaderKind::SpecialParameter, nullptr, 0.0f, FLinearColor(0.035f, 0.055f, 0.025f), 0.95f, 0.0f}};
    return Specs;
}

bool RequiresNaniteMaterialUsage(const FPublicViewMaterialSpec& Spec)
{
    return FCString::Strcmp(Spec.AssetName, TEXT("M_IPV_Bark")) == 0 ||
        FCString::Strcmp(Spec.AssetName, TEXT("M_IPV_LeafDark")) == 0 ||
        FCString::Strcmp(Spec.AssetName, TEXT("M_IPV_LeafMid")) == 0 ||
        FCString::Strcmp(Spec.AssetName, TEXT("M_IPV_LeafLight")) == 0 ||
        FCString::Strcmp(Spec.AssetName, TEXT("M_IPV_Lawn")) == 0;
}

const TArray<FPublicViewTextureSpec>& GetTextureSpecs()
{
    static const TArray<FPublicViewTextureSpec> Specs = {
        {TEXT("Bark"), TEXT("T_IPV_Bark_BaseColor"), EPublicViewTextureUsage::BaseColor, 9962202, TEXT("48ab071d49e17f9f68e4132b04e0fadd0f9e4f18e7883ff3ca117eb298f29a93")},
        {TEXT("Bark"), TEXT("T_IPV_Bark_Normal"), EPublicViewTextureUsage::Normal, 5359508, TEXT("810fe420f8fb4c460a7adcf83836e39180411ba7b50ec1aaf0ec17c8191863e1")},
        {TEXT("Bark"), TEXT("T_IPV_Bark_ORM"), EPublicViewTextureUsage::PackedOrm, 1898287, TEXT("16312147ba6801e4cb8d4808d07830c83520e6e413f5b2610812568ea84c77d6")},
        {TEXT("DarkTimber"), TEXT("T_IPV_DarkTimber_BaseColor"), EPublicViewTextureUsage::BaseColor, 6478048, TEXT("df4f895f54951977e76a07ea29d9976c141c65b3b280889e76e78c52b6173da0")},
        {TEXT("DarkTimber"), TEXT("T_IPV_DarkTimber_Normal"), EPublicViewTextureUsage::Normal, 2106696, TEXT("5d69019948dd757fefaf1aede1f9d26f3a4d480f68dc66767c1911a04abfdfdc")},
        {TEXT("DarkTimber"), TEXT("T_IPV_DarkTimber_ORM"), EPublicViewTextureUsage::PackedOrm, 836813, TEXT("54a2d68d2459271365b45ad36ef761e752c386883b0fe5f52306992170bbc4cc")},
        {TEXT("Foliage"), TEXT("T_IPV_Foliage_BaseColor"), EPublicViewTextureUsage::BaseColor, 9326803, TEXT("e08023d14f67c6a7c5926c424bbcf8d39c5ef65bc7a0ab664f1a75f1b21c0eae")},
        {TEXT("Foliage"), TEXT("T_IPV_Foliage_Normal"), EPublicViewTextureUsage::Normal, 5252004, TEXT("db9fd0fb18e5519db99d9714a3b620b14d5330cf45fe5890964af2fa3317f7cb")},
        {TEXT("Foliage"), TEXT("T_IPV_Foliage_ORM"), EPublicViewTextureUsage::PackedOrm, 2189459, TEXT("0e82a9657787449f54a6cdd9cdb2e0827058f4c75f0a9947dca484598ff2c711")},
        {TEXT("Lawn"), TEXT("T_IPV_Lawn_BaseColor"), EPublicViewTextureUsage::BaseColor, 10459920, TEXT("09cb71e3a93ad4da7b36f1cbf31dc9bf7ee7bdf2c203340de87735978b90f48b")},
        {TEXT("Lawn"), TEXT("T_IPV_Lawn_Normal"), EPublicViewTextureUsage::Normal, 6471831, TEXT("e73d5cd7a09622dc5670389d130779236eb72c70e040dba93fdd6e97b49307d0")},
        {TEXT("Lawn"), TEXT("T_IPV_Lawn_ORM"), EPublicViewTextureUsage::PackedOrm, 2261495, TEXT("8f19e1614680543ecd9090d579a512dc2acf9749cee19934fe69676c699a780d")},
        {TEXT("Plaster"), TEXT("T_IPV_Plaster_BaseColor"), EPublicViewTextureUsage::BaseColor, 7890921, TEXT("7c05a613c000ddccba5df7d5f9a370e4f6a22721260d0cab8f2da151a098adfc")},
        {TEXT("Plaster"), TEXT("T_IPV_Plaster_Normal"), EPublicViewTextureUsage::Normal, 2500685, TEXT("37ea9e604129d6180819dac7f20d128ca062d2e4aeecf3d4868fd933c4f217f1")},
        {TEXT("Plaster"), TEXT("T_IPV_Plaster_ORM"), EPublicViewTextureUsage::PackedOrm, 869018, TEXT("f11318dd9b8cf6b05045ade2f8bec80179c90aa170be0fb4b2cd37aaeeabbb9f")},
        {TEXT("Shutter"), TEXT("T_IPV_Shutter_BaseColor"), EPublicViewTextureUsage::BaseColor, 4570409, TEXT("8444d4e6900786afc27e02d0c0b643a0433177497a8f27080f87b0c3634af63c")},
        {TEXT("Shutter"), TEXT("T_IPV_Shutter_Normal"), EPublicViewTextureUsage::Normal, 1462578, TEXT("4e5b5e946e923ee9589ebfd45d7ff6fbfab540fd08ea352eff8fd02f8c0dd519")},
        {TEXT("Shutter"), TEXT("T_IPV_Shutter_ORM"), EPublicViewTextureUsage::PackedOrm, 402069, TEXT("9182691e038cb90ddaf00f46c3e3f4d7ba95a5fdc905a6dd47a28bb3055340ea")},
        {TEXT("Slate"), TEXT("T_IPV_Slate_BaseColor"), EPublicViewTextureUsage::BaseColor, 7255179, TEXT("80731216a8dfeaaf7974630d4e8592492007308028d16bc2470b9a28c356b957")},
        {TEXT("Slate"), TEXT("T_IPV_Slate_Normal"), EPublicViewTextureUsage::Normal, 3249242, TEXT("4eed13b4f055f25910e9f02a5f7695a154cf44b475fa8877cde5ca75f616821b")},
        {TEXT("Slate"), TEXT("T_IPV_Slate_ORM"), EPublicViewTextureUsage::PackedOrm, 1053293, TEXT("b20d11d3e6b6be5406faaf5b0cff3802da4666bae14f363a534f42d748ffa29b")},
        {TEXT("Stone"), TEXT("T_IPV_Stone_BaseColor"), EPublicViewTextureUsage::BaseColor, 9259452, TEXT("15866b6cfcb7cc1fb2d5ed89fafa72eed9fe87c8d531638b88b5e9f62015108a")},
        {TEXT("Stone"), TEXT("T_IPV_Stone_Normal"), EPublicViewTextureUsage::Normal, 3945419, TEXT("fdf23decc3699928217a0b4d8f3b4a189ab16e4791520f9b94838dd65868b101")},
        {TEXT("Stone"), TEXT("T_IPV_Stone_ORM"), EPublicViewTextureUsage::PackedOrm, 1513197, TEXT("a54cac5abb3fb783d707a2d43854ba29d296f9a9cbf9b447d389b04ea426f943")}};
    return Specs;
}

const TArray<FPublicViewFrozenSourceFile>& GetFrozenSourceFiles()
{
    static const TArray<FPublicViewFrozenSourceFile> Files = {
        {TEXT("SM_IstanaPublicView_Building_Hero.obj"), TEXT("BUILDING_HERO_VISUAL"), 12881265, TEXT("e543ae2919e41b6b97805980c068980bdd192bc6381a66a50c80921d7e63b9f0")},
        {TEXT("SM_IstanaPublicView_Building_Collision.obj"), TEXT("BUILDING_SIMPLE_COLLISION"), 31095, TEXT("d52d1d4a3e6f55927ed7d0abda344a7589ff916c0b669faa2c608992b35498dd")},
        {TEXT("SM_IstanaPublicView_Terrain.obj"), TEXT("TERRAIN_VISUAL_COLLISION"), 3909911, TEXT("78af53572ef53bacb684b5b2103d7ba427999c8223bddd5c0de76deb8b16dd23")},
        {TEXT("SM_IstanaPublicView_TerrainSkirt.obj"), TEXT("TERRAIN_BOUNDARY_SKIRT_VISUAL_ONLY"), 96378, TEXT("28601fcda4d046bedb324ac3c734074e55ce8b4ac695aa45171e02a8807febd2")},
        {TEXT("SM_IstanaPublicView_Hardscape.obj"), TEXT("PUBLIC_FORECOURT_HARDSCAPE"), 588366, TEXT("41937b244913d9cda360c010c2e2d337033774ad20bc4109bf0fad61bdf362c5")},
        {TEXT("SM_IstanaPublicView_ContextBuildings.obj"), TEXT("ANONYMOUS_CONTEXT_BUILDINGS"), 2556816, TEXT("6baca540296064fa1532111f93017a8b6ca0ffcaf7a53420a2662006e5fd6071")},
        {TEXT("SM_IstanaPublicView_Tree_Rain.obj"), TEXT("VOLUMETRIC_TREE_ARCHETYPE"), 1425165, TEXT("ebb0474c5449eecb7f9ec8b5b193cf0939a1e8eb252ca0c0c5d688830b97be71")},
        {TEXT("SM_IstanaPublicView_Tree_Palm.obj"), TEXT("VOLUMETRIC_TREE_ARCHETYPE"), 1417803, TEXT("ac16394d95cdab6f54ad2675f742a22de185b196807a4bc2448c9250aa80f84e")},
        {TEXT("SM_IstanaPublicView_Tree_Framing.obj"), TEXT("VOLUMETRIC_TREE_ARCHETYPE"), 1107675, TEXT("3b687ddb56102cea5a3fe775fb3aaef680ed0f228ca90fbcfe067e24001c5b5e")},
        {TEXT("IstanaPublicView.instances.json"), TEXT("INSTANCE_PLACEMENTS"), 273412, TEXT("84ad850367da04a48f95efea591773e496adf6726b076965970796c3b01be789")}};
    return Files;
}

FString MeshObjectPath(const FPublicViewMeshSpec& Spec)
{
    return FString::Printf(
        TEXT("%s/%s.%s"),
        Spec.DestinationPath,
        Spec.AssetName,
        Spec.AssetName);
}

FString MeshSourceFilename(const FPublicViewMeshSpec& Spec)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        Spec.bUsesOptionalOsmSource
            ? OsmContextSourceRelativeDirectory
            : GeneratedSourceRelativeDirectory,
        Spec.SourceFile));
}

bool PrepareLegacyObjMaterialSlotAdapter(
    const FPublicViewMeshSpec& Spec,
    FString& OutAdaptedObjFilename,
    FString& OutError)
{
    // Autodesk's legacy OBJ reader discards usemtl groups that have no
    // mtllib-resolved material definitions. Keep the frozen source OBJ
    // self-contained and immutable, but give the reader a deterministic
    // import-only copy whose minimal MTL declares the exact slot names. The
    // caller keeps material/texture import disabled and restores provenance
    // to the frozen OBJ after import, so this adapter cannot create content
    // assets or become the recorded source file.
    const FString OriginalObjFilename = MeshSourceFilename(Spec);
    FString OriginalObjText;
    if (!FFileHelper::LoadFileToString(
            OriginalObjText,
            *OriginalObjFilename))
    {
        OutError = FString::Printf(
            TEXT("Could not read frozen OBJ '%s' for legacy material-slot adaptation."),
            *OriginalObjFilename);
        return false;
    }
    if (OriginalObjText.ToLower().Contains(TEXT("mtllib ")))
    {
        OutError = FString::Printf(
            TEXT("Frozen OBJ '%s' unexpectedly contains mtllib; refusing to layer a second material contract."),
            *OriginalObjFilename);
        return false;
    }

    const FString AdapterDirectory =
        FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectIntermediateDir(),
            TEXT("TRIAD/IstanaPublicView/LegacyObjMaterialSlots")));
    if (!IFileManager::Get().MakeDirectory(*AdapterDirectory, true))
    {
        OutError = FString::Printf(
            TEXT("Could not create isolated legacy OBJ adapter directory '%s'."),
            *AdapterDirectory);
        return false;
    }

    const FString MaterialLibraryFilename = FString(Spec.AssetName) + TEXT(".mtl");
    const FString MaterialLibraryPath = FPaths::Combine(
        AdapterDirectory,
        MaterialLibraryFilename);
    FString MaterialLibraryText(
        TEXT("# TRIAD deterministic import-only material slot declarations\n"));
    for (const FName& SlotName : Spec.RequiredMaterialSlots)
    {
        MaterialLibraryText += FString::Printf(
            TEXT("newmtl %s\nKa 0.000000 0.000000 0.000000\nKd 0.800000 0.800000 0.800000\nKs 0.000000 0.000000 0.000000\nd 1.000000\nillum 1\n\n"),
            *SlotName.ToString());
    }

    OutAdaptedObjFilename = FPaths::Combine(
        AdapterDirectory,
        FString(Spec.AssetName) + TEXT(".obj"));
    const FString AdaptedObjText = FString::Printf(
        TEXT("mtllib %s\n"),
        *MaterialLibraryFilename) + OriginalObjText;
    if (!FFileHelper::SaveStringToFile(
            MaterialLibraryText,
            *MaterialLibraryPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
        !FFileHelper::SaveStringToFile(
            AdaptedObjText,
            *OutAdaptedObjFilename,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(
            TEXT("Could not write deterministic legacy OBJ material-slot adapter for '%s'."),
            Spec.AssetName);
        OutAdaptedObjFilename.Reset();
        return false;
    }

    FString MaterialLibraryReadback;
    FString AdaptedObjReadback;
    if (!FFileHelper::LoadFileToString(
            MaterialLibraryReadback,
            *MaterialLibraryPath) ||
        !FFileHelper::LoadFileToString(
            AdaptedObjReadback,
            *OutAdaptedObjFilename) ||
        AdaptedObjReadback != AdaptedObjText ||
        MaterialLibraryReadback != MaterialLibraryText)
    {
        OutError = FString::Printf(
            TEXT("Legacy OBJ material-slot adapter readback differed for '%s'."),
            Spec.AssetName);
        OutAdaptedObjFilename.Reset();
        return false;
    }
    for (const FName& SlotName : Spec.RequiredMaterialSlots)
    {
        if (!MaterialLibraryReadback.Contains(
                TEXT("newmtl ") + SlotName.ToString(),
                ESearchCase::CaseSensitive))
        {
            OutError = FString::Printf(
                TEXT("Legacy OBJ adapter for '%s' omitted exact slot declaration '%s'."),
                Spec.AssetName,
                *SlotName.ToString());
            OutAdaptedObjFilename.Reset();
            return false;
        }
    }
    OutError.Reset();
    return true;
}

FString MaterialObjectPath(const FName& SlotName)
{
    return FString::Printf(
        TEXT("%s/%s.%s"),
        *PublicViewMaterialPath,
        *SlotName.ToString(),
        *SlotName.ToString());
}

FString TextureObjectPath(const FPublicViewTextureSpec& Spec)
{
    return FString::Printf(
        TEXT("%s/%s.%s"),
        *PublicViewTexturePath,
        Spec.AssetName,
        Spec.AssetName);
}

TextureCompressionSettings GetTextureCompression(
    EPublicViewTextureUsage Usage)
{
    if (Usage == EPublicViewTextureUsage::Normal)
    {
        return TC_Normalmap;
    }
    if (Usage == EPublicViewTextureUsage::PackedOrm)
    {
        return TC_Masks;
    }
    return TC_Default;
}

TextureGroup GetTextureGroup(EPublicViewTextureUsage Usage)
{
    return Usage == EPublicViewTextureUsage::Normal
        ? TEXTUREGROUP_WorldNormalMap
        : TEXTUREGROUP_World;
}

bool IsTextureSrgb(EPublicViewTextureUsage Usage)
{
    return Usage == EPublicViewTextureUsage::BaseColor;
}

const TCHAR* ShaderKindName(EPublicViewShaderKind Kind)
{
    if (Kind == EPublicViewShaderKind::PbrTextured)
    {
        return TEXT("PBR_TEXTURED");
    }
    if (Kind == EPublicViewShaderKind::SpecialParameter)
    {
        return TEXT("SPECIAL_PARAMETER");
    }
    return TEXT("HIDDEN_COLLISION_ONLY");
}

FString TextureAssetName(
    const TCHAR* TextureSet,
    EPublicViewTextureUsage Usage)
{
    const TCHAR* Suffix = Usage == EPublicViewTextureUsage::BaseColor
        ? TEXT("BaseColor")
        : (Usage == EPublicViewTextureUsage::Normal
            ? TEXT("Normal")
            : TEXT("ORM"));
    return FString::Printf(TEXT("T_IPV_%s_%s"), TextureSet, Suffix);
}

bool IsPublicViewEditorOperationSafe(FString& OutError)
{
    if (!GEditor)
    {
        OutError = TEXT("Unreal Editor is unavailable.");
        return false;
    }
    if (GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
    {
        OutError = TEXT("Stop Play-In-Editor before running public-view editor operations.");
        return false;
    }
    TArray<UPackage*> DirtyMaps;
    TArray<UPackage*> DirtyContent;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContent);
    if (DirtyMaps.Num() > 0 || DirtyContent.Num() > 0)
    {
        TArray<FString> DirtyNames;
        for (UPackage* Package : DirtyMaps)
        {
            DirtyNames.Add(Package ? Package->GetName() : TEXT("<unknown map>"));
        }
        for (UPackage* Package : DirtyContent)
        {
            DirtyNames.Add(Package ? Package->GetName() : TEXT("<unknown content>"));
        }
        DirtyNames.Sort();
        OutError = TEXT("Refusing to switch or save maps while packages are dirty: ") +
            FString::Join(DirtyNames, TEXT(", "));
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

enum class EPublicViewAssetState : uint8
{
    Absent,
    CompleteValid,
    PartialOrInvalid
};

bool ValidateStaticMesh(
    const UStaticMesh* Mesh,
    const FPublicViewMeshSpec& Spec,
    bool bRequireAssignedMaterials,
    FString& OutError)
{
    const FString ExpectedPath = MeshObjectPath(Spec);
    if (!Mesh || Mesh->GetPathName() != ExpectedPath ||
        !Mesh->GetPathName().StartsWith(PublicViewAssetRoot + TEXT("/")))
    {
        OutError = FString::Printf(
            TEXT("Required isolated static mesh is absent or not at '%s'."),
            *ExpectedPath);
        return false;
    }
    const FVector Extent = Mesh->GetBounds().BoxExtent;
    if (Extent.ContainsNaN() ||
        Extent.X < Spec.MinimumBoxExtentCentimeters.X ||
        Extent.Y < Spec.MinimumBoxExtentCentimeters.Y ||
        Extent.Z < Spec.MinimumBoxExtentCentimeters.Z)
    {
        OutError = FString::Printf(
            TEXT("Mesh '%s' has invalid/non-volumetric bounds (extent %.1f, %.1f, %.1f cm)."),
            *ExpectedPath,
            Extent.X,
            Extent.Y,
            Extent.Z);
        return false;
    }
    if (Mesh->GetNumSourceModels() < 1 || Mesh->GetNumTexCoords(0) < 1)
    {
        OutError = FString::Printf(
            TEXT("Mesh '%s' lacks source LOD0 or the required imported continuous-metre UV0 channel."),
            *ExpectedPath);
        return false;
    }
    const FMeshBuildSettings& BuildSettings =
        Mesh->GetSourceModel(0).BuildSettings;
    if (BuildSettings.bRecomputeNormals ||
        !BuildSettings.bRecomputeTangents ||
        !BuildSettings.bUseMikkTSpace ||
        !BuildSettings.bRemoveDegenerates ||
        !BuildSettings.bUseFullPrecisionUVs ||
        !BuildSettings.BuildScale3D.Equals(FVector::OneVector, 0.000001))
    {
        OutError = FString::Printf(
            TEXT("Mesh '%s' does not retain ImportNormals + recomputed MikkTSpace tangents + remove-degenerates + full-precision metre UVs + identity build scale."),
            *ExpectedPath);
        return false;
    }
    const FPublicViewExpectedImportedBounds* ExpectedBounds =
        GetExpectedImportedBounds().FindByPredicate(
            [&Spec](const FPublicViewExpectedImportedBounds& Candidate)
            {
                return FString(Spec.AssetName) == Candidate.AssetName;
            });
    const FBoxSphereBounds& ImportedBounds = Mesh->GetBounds();
    constexpr double BoundsToleranceCentimeters = 2.0;
    if (!ExpectedBounds ||
        !ImportedBounds.Origin.Equals(
            ExpectedBounds->OriginCentimeters,
            BoundsToleranceCentimeters) ||
        !ImportedBounds.BoxExtent.Equals(
            ExpectedBounds->ExtentCentimeters,
            BoundsToleranceCentimeters))
    {
        OutError = FString::Printf(
            TEXT("Mesh '%s' does not retain exact centimetre scale and +Y ceremonial-approach source bounds; got origin (%.3f, %.3f, %.3f), extent (%.3f, %.3f, %.3f)."),
            *ExpectedPath,
            ImportedBounds.Origin.X,
            ImportedBounds.Origin.Y,
            ImportedBounds.Origin.Z,
            ImportedBounds.BoxExtent.X,
            ImportedBounds.BoxExtent.Y,
            ImportedBounds.BoxExtent.Z);
        return false;
    }
    for (const FName& SlotName : Spec.RequiredMaterialSlots)
    {
        const int32 MaterialIndex = Mesh->GetMaterialIndex(SlotName);
        if (MaterialIndex == INDEX_NONE)
        {
            OutError = FString::Printf(
                TEXT("Mesh '%s' is missing required isolated slot '%s'."),
                *ExpectedPath,
                *SlotName.ToString());
            return false;
        }
        if (bRequireAssignedMaterials)
        {
            UMaterialInterface* Material = Mesh->GetMaterial(MaterialIndex);
            const FString ExpectedMaterial = MaterialObjectPath(SlotName);
            if (!Material || Material->GetPathName() != ExpectedMaterial)
            {
                OutError = FString::Printf(
                    TEXT("Mesh '%s' slot '%s' is not assigned exact isolated material '%s'."),
                    *ExpectedPath,
                    *SlotName.ToString(),
                    *ExpectedMaterial);
                return false;
            }
        }
    }
    if (Spec.bUseComplexCollision &&
        (!Mesh->GetBodySetup() ||
         Mesh->GetBodySetup()->CollisionTraceFlag != CTF_UseComplexAsSimple))
    {
        OutError = FString::Printf(
            TEXT("Mesh '%s' lacks its required explicit complex-as-simple collision policy."),
            *ExpectedPath);
        return false;
    }
    OutError.Reset();
    return true;
}

struct FPublicViewTreePlacement
{
    FName Archetype;
    FTransform LocalTransform;
};

bool LoadJsonObjectFromFile(
    const FString& Filename,
    TSharedPtr<FJsonObject>& OutObject,
    FString& OutError)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Filename))
    {
        OutError = FString::Printf(TEXT("Required JSON file is missing or unreadable: '%s'."), *Filename);
        return false;
    }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
    {
        OutError = FString::Printf(TEXT("Required JSON file is malformed: '%s'."), *Filename);
        return false;
    }
    OutError.Reset();
    return true;
}

bool HasExactJsonFields(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* const* ExpectedFields,
    int32 ExpectedFieldCount,
    FString& OutError)
{
    if (!Object.IsValid() || Object->Values.Num() != ExpectedFieldCount)
    {
        OutError = TEXT("JSON object has missing or unknown fields.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedFieldCount; ++Index)
    {
        if (!Object->Values.Contains(ExpectedFields[Index]))
        {
            OutError = FString::Printf(
                TEXT("JSON object is missing exact field '%s'."),
                ExpectedFields[Index]);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool TryGetExactString(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const TCHAR* Expected,
    FString& OutError)
{
    FString Value;
    if (!Object.IsValid() || !Object->TryGetStringField(Field, Value) || Value != Expected)
    {
        OutError = FString::Printf(
            TEXT("JSON field '%s' must be exactly '%s'."),
            Field,
            Expected);
        return false;
    }
    return true;
}

bool TryGetFiniteNumber(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    double& OutValue,
    FString& OutError)
{
    if (!Object.IsValid() || !Object->TryGetNumberField(Field, OutValue) ||
        !FMath::IsFinite(OutValue))
    {
        OutError = FString::Printf(TEXT("JSON field '%s' must be finite."), Field);
        return false;
    }
    return true;
}

bool ReadFiniteVector3(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FVector& OutValue,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) ||
        !Values || Values->Num() != 3)
    {
        OutError = FString::Printf(TEXT("JSON field '%s' must be a three-number array."), Field);
        return false;
    }
    double Components[3] = {0.0, 0.0, 0.0};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (!(*Values)[Index].IsValid() ||
            (*Values)[Index]->Type != EJson::Number)
        {
            OutError = FString::Printf(TEXT("JSON field '%s' contains a non-number."), Field);
            return false;
        }
        Components[Index] = (*Values)[Index]->AsNumber();
        if (!FMath::IsFinite(Components[Index]))
        {
            OutError = FString::Printf(TEXT("JSON field '%s' contains a non-finite number."), Field);
            return false;
        }
    }
    OutValue = FVector(Components[0], Components[1], Components[2]);
    return true;
}

bool CalculateSha256(const FString& Filename, FString& OutDigest, FString& OutError)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() <= 0)
    {
        OutError = FString::Printf(TEXT("Could not load non-empty source file '%s'."), *Filename);
        return false;
    }
    FPublicViewSha256 Hasher;
    Hasher.Update(Bytes.GetData(), Bytes.Num());
    OutDigest = Hasher.Final();
    OutError.Reset();
    return true;
}

bool ValidateFrozenFile(
    const FString& Filename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutError)
{
    const int64 ActualBytes = IFileManager::Get().FileSize(*Filename);
    FString ActualDigest;
    if (ActualBytes != ExpectedBytes ||
        !CalculateSha256(Filename, ActualDigest, OutError) ||
        ActualDigest != ExpectedSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("Frozen source '%s' does not match exact %lld-byte SHA-256 contract '%s'."),
                *Filename,
                ExpectedBytes,
                *ExpectedSha256);
        }
        return false;
    }
    OutError.Reset();
    return true;
}

TMap<FString, FString> GetExpectedSourceRoles()
{
    TMap<FString, FString> Roles;
    for (const FPublicViewFrozenSourceFile& File : GetFrozenSourceFiles())
    {
        Roles.Add(File.RelativePath, File.Role);
    }
    return Roles;
}

const FPublicViewFrozenSourceFile* FindFrozenSourceFile(
    const FString& RelativePath)
{
    return GetFrozenSourceFiles().FindByPredicate(
        [&RelativePath](const FPublicViewFrozenSourceFile& Candidate)
        {
            return RelativePath == Candidate.RelativePath;
        });
}

const FPublicViewMeshSpec* FindMeshSpecBySourceFile(const FString& SourceFile)
{
    for (const FPublicViewMeshSpec& Spec : GetMeshSpecs())
    {
        if (SourceFile == Spec.SourceFile)
        {
            return &Spec;
        }
    }
    return nullptr;
}

bool ValidateObjSlotContract(
    const FString& Filename,
    const FPublicViewMeshSpec& Spec,
    FString& OutError)
{
    FString ObjText;
    if (!FFileHelper::LoadFileToString(ObjText, *Filename))
    {
        OutError = FString::Printf(TEXT("Could not read required OBJ '%s'."), *Filename);
        return false;
    }
    const FString LowerText = ObjText.ToLower();
    if (LowerText.Contains(TEXT("mtllib ")) ||
        !LowerText.Contains(TEXT("# positions: centimetres")) ||
        !ObjText.Contains(
            TEXT("# export contract: UE5.5 legacy OBJ preconditioned; disk v=(x,-y,z), vn=(nx,-ny,nz)"),
            ESearchCase::CaseSensitive) ||
        !ObjText.Contains(
            TEXT("# export contract: each face-token order is reversed; disk vt=(u,1-v)"),
            ESearchCase::CaseSensitive) ||
        !ObjText.Contains(
            TEXT("# expected importer readback: logical positions, explicit normals, winding, and UV0"),
            ESearchCase::CaseSensitive) ||
        !ObjText.Contains(
            TEXT("# uv0: continuous source-metre projections; one UV unit equals one metre before material scaling"),
            ESearchCase::CaseSensitive) ||
        !ObjText.Contains(
            TEXT("# normals: explicit; hard faces split, curved/terrain surfaces area-weighted smooth"),
            ESearchCase::CaseSensitive) ||
        !ObjText.Contains(TEXT("\nvt "), ESearchCase::CaseSensitive) ||
        !ObjText.Contains(TEXT("\nvn "), ESearchCase::CaseSensitive))
    {
        OutError = FString::Printf(
            TEXT("OBJ '%s' must be self-contained, centimetre-position encoded, and carry explicit continuous-metre UV0 plus vertex normals."),
            *Filename);
        return false;
    }
    const TCHAR* ForbiddenMarkers[] = {
        TEXT("billboard"), TEXT("image_plane"), TEXT("imageplane"),
        TEXT("panorama_card"), TEXT("depth_card"), TEXT("interior_room"),
        TEXT("security_equipment"), TEXT("guard_position"),
        TEXT("access_control"), TEXT("service_route"), TEXT("security_route")};
    for (const TCHAR* Marker : ForbiddenMarkers)
    {
        if (LowerText.Contains(Marker))
        {
            OutError = FString::Printf(
                TEXT("OBJ '%s' contains forbidden public-view marker '%s'."),
                *Filename,
                Marker);
            return false;
        }
    }
    for (const FName& Slot : Spec.RequiredMaterialSlots)
    {
        const FString Declaration = TEXT("usemtl ") + Slot.ToString();
        if (!ObjText.Contains(Declaration, ESearchCase::CaseSensitive))
        {
            OutError = FString::Printf(
                TEXT("OBJ '%s' is missing exact material declaration '%s'."),
                *Filename,
                *Declaration);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateOsmContextSourceContract(FString& OutError)
{
    const FString ContractPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        OsmContextContractRelativePath));
    const FString ManifestPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        OsmContextManifestRelativePath));
    const FString MeshPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        OsmContextSourceRelativeDirectory,
        TEXT("SM_IstanaPublicView_OSMContextBuildings.obj")));
    if (!ValidateFrozenFile(
            ContractPath,
            4109,
            FrozenOsmContextContractSha256,
            OutError) ||
        !ValidateFrozenFile(
            ManifestPath,
            7536,
            FrozenOsmContextManifestSha256,
            OutError) ||
        !ValidateFrozenFile(
            MeshPath,
            11784053,
            FrozenOsmContextMeshSha256,
            OutError))
    {
        return false;
    }

    TSharedPtr<FJsonObject> Contract;
    TSharedPtr<FJsonObject> Manifest;
    if (!LoadJsonObjectFromFile(ContractPath, Contract, OutError) ||
        !LoadJsonObjectFromFile(ManifestPath, Manifest, OutError) ||
        !TryGetExactString(
            Contract,
            TEXT("schema"),
            TEXT("triad.istana_public_view_osm_context_contract.v1"),
            OutError) ||
        !TryGetExactString(Contract, TEXT("status"), *OsmContextStatus, OutError) ||
        !TryGetExactString(Contract, TEXT("claimStatus"), *PublicViewClaimLabel, OutError) ||
        !TryGetExactString(
            Contract,
            TEXT("sourceClass"),
            TEXT("LICENSED_ODBL_SNAPSHOT_DERIVED"),
            OutError) ||
        !TryGetExactString(
            Manifest,
            TEXT("schema"),
            TEXT("triad.istana_public_view_osm_context_manifest.v1"),
            OutError) ||
        !TryGetExactString(Manifest, TEXT("status"), *OsmContextStatus, OutError) ||
        !TryGetExactString(Manifest, TEXT("claimStatus"), *PublicViewClaimLabel, OutError) ||
        !TryGetExactString(
            Manifest,
            TEXT("sourceClass"),
            TEXT("LICENSED_ODBL_SNAPSHOT_DERIVED"),
            OutError) ||
        !TryGetExactString(
            Manifest,
            TEXT("contractSha256"),
            *FrozenOsmContextContractSha256,
            OutError))
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* LocalFrame = nullptr;
    const TSharedPtr<FJsonObject>* ContractScope = nullptr;
    const TSharedPtr<FJsonObject>* Output = nullptr;
    const TSharedPtr<FJsonObject>* ManifestScope = nullptr;
    const TSharedPtr<FJsonObject>* Snapshot = nullptr;
    FVector ContractBuildScale = FVector::ZeroVector;
    FVector ManifestBuildScale = FVector::ZeroVector;
    bool bContractNegativeScaleForbidden = false;
    bool bManifestNegativeScaleForbidden = false;
    bool bContainsRoutes = true;
    bool bCollisionEnabled = true;
    bool bSensorOcclusionEnabled = true;
    bool bConsumedByFrozenMainImport = true;
    if (!Contract->TryGetObjectField(TEXT("localFrame"), LocalFrame) ||
        !LocalFrame || !LocalFrame->IsValid() ||
        !TryGetExactString(
            *LocalFrame,
            TEXT("sourceCoordinateSystem"),
            TEXT("local tangent ENU metres; +X east, +Y north"),
            OutError) ||
        !TryGetExactString(
            *LocalFrame,
            TEXT("coordinateSystem"),
            TEXT("right-handed hero-local metres; +X east, +Y ceremonial/south, +Z up"),
            OutError) ||
        !TryGetExactString(
            *LocalFrame,
            TEXT("enuToHeroTransform"),
            TEXT("hero (x,y,z) = ENU (east,-north,up)"),
            OutError) ||
        !TryGetExactString(
            *LocalFrame,
            TEXT("heroFrameAlignmentStatus"),
            *OsmContextAlignmentStatus,
            OutError) ||
        !TryGetExactString(
            *LocalFrame,
            TEXT("unrealLegacyObjExportContract"),
            *LegacyObjExportContract,
            OutError) ||
        !ReadFiniteVector3(*LocalFrame, TEXT("buildScale3D"), ContractBuildScale, OutError) ||
        !ContractBuildScale.Equals(FVector::OneVector, 0.000001) ||
        !(*LocalFrame)->TryGetBoolField(
            TEXT("negativeBuildScaleForbidden"),
            bContractNegativeScaleForbidden) ||
        !bContractNegativeScaleForbidden ||
        !ReadFiniteVector3(Manifest, TEXT("buildScale3D"), ManifestBuildScale, OutError) ||
        !ManifestBuildScale.Equals(FVector::OneVector, 0.000001) ||
        !Manifest->TryGetBoolField(
            TEXT("negativeBuildScaleForbidden"),
            bManifestNegativeScaleForbidden) ||
        !bManifestNegativeScaleForbidden ||
        !TryGetExactString(
            Manifest,
            TEXT("coordinateSystem"),
            TEXT("right-handed hero-local metres; +X east, +Y ceremonial/south, +Z up"),
            OutError) ||
        !TryGetExactString(
            Manifest,
            TEXT("enuToHeroTransform"),
            TEXT("hero (x,y,z) = ENU (east,-north,up)"),
            OutError) ||
        !TryGetExactString(
            Manifest,
            TEXT("heroFrameAlignmentStatus"),
            *OsmContextAlignmentStatus,
            OutError) ||
        !TryGetExactString(
            Manifest,
            TEXT("unrealLegacyObjExportContract"),
            *LegacyObjExportContract,
            OutError) ||
        !Contract->TryGetObjectField(TEXT("scope"), ContractScope) ||
        !ContractScope || !ContractScope->IsValid() ||
        !Contract->TryGetObjectField(TEXT("output"), Output) ||
        !Output || !Output->IsValid() ||
        !Manifest->TryGetObjectField(TEXT("scope"), ManifestScope) ||
        !ManifestScope || !ManifestScope->IsValid() ||
        !Manifest->TryGetObjectField(TEXT("sourceSnapshot"), Snapshot) ||
        !Snapshot || !Snapshot->IsValid())
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Optional OSM context coordinate/import contract changed or is incomplete.");
        }
        return false;
    }

    double HeroExclusionRadius = 0.0;
    double OuterRadius = 0.0;
    if (!TryGetFiniteNumber(
            *ManifestScope,
            TEXT("heroExclusionRadiusMeters"),
            HeroExclusionRadius,
            OutError) ||
        !FMath::IsNearlyEqual(HeroExclusionRadius, 300.0, 0.000001) ||
        !TryGetFiniteNumber(
            *ManifestScope,
            TEXT("outerRadiusMeters"),
            OuterRadius,
            OutError) ||
        !FMath::IsNearlyEqual(OuterRadius, 1000.0, 0.000001) ||
        !(*ManifestScope)->TryGetBoolField(TEXT("containsRoadsOrRoutes"), bContainsRoutes) ||
        bContainsRoutes ||
        !(*ManifestScope)->TryGetBoolField(TEXT("collisionEnabled"), bCollisionEnabled) ||
        bCollisionEnabled ||
        !(*ManifestScope)->TryGetBoolField(
            TEXT("sensorOcclusionEnabled"),
            bSensorOcclusionEnabled) ||
        bSensorOcclusionEnabled ||
        !(*ManifestScope)->TryGetBoolField(
            TEXT("consumedByFrozenMainImport"),
            bConsumedByFrozenMainImport) ||
        bConsumedByFrozenMainImport ||
        !TryGetExactString(
            *Output,
            TEXT("mesh"),
            TEXT("SM_IstanaPublicView_OSMContextBuildings.obj"),
            OutError) ||
        !TryGetExactString(
            *Output,
            TEXT("role"),
            TEXT("OPTIONAL_ODBL_CONTEXT_BUILDINGS_VISUAL_ONLY"),
            OutError) ||
        !TryGetExactString(*Snapshot, TEXT("databaseLicence"), TEXT("ODbL-1.0"), OutError) ||
        !TryGetExactString(*Snapshot, TEXT("attribution"), *OsmAttribution, OutError) ||
        !TryGetExactString(*Snapshot, TEXT("licenceUrl"), *OsmLicenceUrl, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Optional OSM scope, role, attribution, or no-sensor-truth contract changed.");
        }
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("files"), Records) || !Records ||
        Records->Num() != 1 || !(*Records)[0].IsValid())
    {
        OutError = TEXT("Optional OSM manifest must bind exactly one frozen mesh record.");
        return false;
    }
    const TSharedPtr<FJsonObject> Record = (*Records)[0]->AsObject();
    double Bytes = 0.0;
    double Vertices = 0.0;
    double Triangles = 0.0;
    if (!Record.IsValid() ||
        !TryGetExactString(
            Record,
            TEXT("path"),
            TEXT("SM_IstanaPublicView_OSMContextBuildings.obj"),
            OutError) ||
        !TryGetExactString(
            Record,
            TEXT("role"),
            TEXT("OPTIONAL_ODBL_CONTEXT_BUILDINGS_VISUAL_ONLY"),
            OutError) ||
        !TryGetExactString(Record, TEXT("sha256"), *FrozenOsmContextMeshSha256, OutError) ||
        !TryGetExactString(
            Record,
            TEXT("coordinateExportContract"),
            *LegacyObjExportContract,
            OutError) ||
        !TryGetExactString(
            Record,
            TEXT("normalContract"),
            TEXT("EXPLICIT_HARD_AND_AREA_WEIGHTED_SMOOTH_NORMALS"),
            OutError) ||
        !TryGetExactString(
            Record,
            TEXT("uvContract"),
            TEXT("UV0_CONTINUOUS_SOURCE_METRES"),
            OutError) ||
        !TryGetFiniteNumber(Record, TEXT("bytes"), Bytes, OutError) ||
        !FMath::IsNearlyEqual(Bytes, 11784053.0, 0.1) ||
        !TryGetFiniteNumber(Record, TEXT("vertices"), Vertices, OutError) ||
        !FMath::IsNearlyEqual(Vertices, 89619.0, 0.1) ||
        !TryGetFiniteNumber(Record, TEXT("triangles"), Triangles, OutError) ||
        !FMath::IsNearlyEqual(Triangles, 29873.0, 0.1))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Optional OSM frozen mesh record changed.");
        }
        return false;
    }

    const FPublicViewMeshSpec* Spec = FindMeshSpecBySourceFile(
        TEXT("SM_IstanaPublicView_OSMContextBuildings.obj"));
    if (!Spec || !Spec->bUsesOptionalOsmSource ||
        Spec->RequiredMaterialSlots.Num() != 2 ||
        !Spec->RequiredMaterialSlots.Contains(FName(TEXT("M_IPV_ContextRender"))) ||
        !Spec->RequiredMaterialSlots.Contains(FName(TEXT("M_IPV_ContextRoof"))) ||
        !ValidateObjSlotContract(MeshPath, *Spec, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Optional OSM Unreal mesh/material import specification changed.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSurfaceContract(
    const TSharedPtr<FJsonObject>& Container,
    FString& OutError)
{
    OutError.Reset();
    const TSharedPtr<FJsonObject>* Surface = nullptr;
    const TSharedPtr<FJsonObject>* UnrealImport = nullptr;
    bool bRecomputeNormals = true;
    bool bRecomputeTangents = false;
    bool bRemoveDegenerates = false;
    if (!Container.IsValid() ||
        !Container->TryGetObjectField(TEXT("surfaceContract"), Surface) ||
        !Surface || !Surface->IsValid() ||
        !TryGetExactString(*Surface, TEXT("uvSet"), TEXT("UV0"), OutError) ||
        !TryGetExactString(*Surface, TEXT("uvEncoding"), *SurfaceUvEncoding, OutError) ||
        !TryGetExactString(
            *Surface,
            TEXT("encodedObjUvPrecondition"),
            *SurfaceEncodedUvPrecondition,
            OutError) ||
        !TryGetExactString(*Surface, TEXT("planarProjection"), *SurfacePlanarProjection, OutError) ||
        !TryGetExactString(*Surface, TEXT("curvedProjection"), *SurfaceCurvedProjection, OutError) ||
        !TryGetExactString(*Surface, TEXT("terrainProjection"), *SurfaceTerrainProjection, OutError) ||
        !TryGetExactString(*Surface, TEXT("normalEncoding"), TEXT("explicit OBJ vertex normals"), OutError) ||
        !TryGetExactString(*Surface, TEXT("hardEdges"), *SurfaceHardEdges, OutError) ||
        !TryGetExactString(*Surface, TEXT("smoothEdges"), *SurfaceSmoothEdges, OutError) ||
        !(*Surface)->TryGetObjectField(TEXT("unrealImport"), UnrealImport) ||
        !UnrealImport || !UnrealImport->IsValid() ||
        !TryGetExactString(*UnrealImport, TEXT("normalImportMethod"), TEXT("ImportNormals"), OutError) ||
        !TryGetExactString(*UnrealImport, TEXT("tangentSpace"), TEXT("MikkTSpace"), OutError) ||
        !(*UnrealImport)->TryGetBoolField(TEXT("recomputeNormals"), bRecomputeNormals) ||
        bRecomputeNormals ||
        !(*UnrealImport)->TryGetBoolField(TEXT("recomputeTangents"), bRecomputeTangents) ||
        !bRecomputeTangents ||
        !(*UnrealImport)->TryGetBoolField(TEXT("removeDegenerates"), bRemoveDegenerates) ||
        !bRemoveDegenerates)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("PBR surface contract changed: exact UV0 metre projections, imported normals, recomputed MikkTSpace tangents, and degenerate removal are required.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateLegacyObjImportContract(
    const TSharedPtr<FJsonObject>& Container,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Contract = nullptr;
    const TSharedPtr<FJsonObject>* Encoded = nullptr;
    const TSharedPtr<FJsonObject>* Conversion = nullptr;
    const TSharedPtr<FJsonObject>* Sentinel = nullptr;
    bool bAppliesToEveryMesh = false;
    bool bPlacementsExcluded = false;
    bool bNegativeBuildScaleForbidden = false;
    FVector BuildScale = FVector::ZeroVector;
    if (!Container.IsValid() ||
        !Container->TryGetObjectField(TEXT("unrealLegacyObjImportContract"), Contract) ||
        !Contract || !Contract->IsValid() ||
        !TryGetExactString(*Contract, TEXT("contractId"), *LegacyObjExportContract, OutError) ||
        !TryGetExactString(
            *Contract,
            TEXT("engineTarget"),
            TEXT("Unreal Engine 5.5 legacy OBJ importer"),
            OutError) ||
        !TryGetExactString(
            *Contract,
            TEXT("logicalCoordinates"),
            TEXT("right-handed Z-up local metres; ceremonial approach +Y"),
            OutError) ||
        !(*Contract)->TryGetBoolField(
            TEXT("appliesToEveryGeneratedObjMesh"),
            bAppliesToEveryMesh) || !bAppliesToEveryMesh ||
        !(*Contract)->TryGetBoolField(
            TEXT("doesNotApplyToInstancePlacementJson"),
            bPlacementsExcluded) || !bPlacementsExcluded ||
        !(*Contract)->TryGetObjectField(TEXT("encodedObjPrecondition"), Encoded) ||
        !Encoded || !Encoded->IsValid() ||
        !TryGetExactString(
            *Encoded, TEXT("position"),
            TEXT("disk (x,y,z) = logical (x,-y,z)"), OutError) ||
        !TryGetExactString(
            *Encoded, TEXT("normal"),
            TEXT("disk (nx,ny,nz) = logical (nx,-ny,nz)"), OutError) ||
        !TryGetExactString(
            *Encoded, TEXT("faceTokens"),
            TEXT("reverse each complete v/vt/vn token sequence"), OutError) ||
        !TryGetExactString(
            *Encoded, TEXT("uv0"),
            TEXT("disk (u,v) = logical (u,1-v)"), OutError) ||
        !(*Contract)->TryGetObjectField(TEXT("legacyImporterConversion"), Conversion) ||
        !Conversion || !Conversion->IsValid() ||
        !TryGetExactString(
            *Conversion, TEXT("position"),
            TEXT("imported (x,y,z) = disk (x,-y,z)"), OutError) ||
        !TryGetExactString(
            *Conversion, TEXT("normal"),
            TEXT("imported (nx,ny,nz) = disk (nx,-ny,nz)"), OutError) ||
        !TryGetExactString(
            *Conversion, TEXT("faceTokens"),
            TEXT("preserved in disk order"), OutError) ||
        !TryGetExactString(
            *Conversion, TEXT("uv0"),
            TEXT("imported (u,v) = disk (u,1-v)"), OutError) ||
        !TryGetExactString(
            *Contract,
            TEXT("expectedReadback"),
            TEXT("logical positions, explicit normals, front winding, and continuous-metre UV0"),
            OutError) ||
        !ReadFiniteVector3(*Contract, TEXT("buildScale3D"), BuildScale, OutError) ||
        !BuildScale.Equals(FVector::OneVector, 0.000001) ||
        !(*Contract)->TryGetBoolField(
            TEXT("negativeBuildScaleForbidden"),
            bNegativeBuildScaleForbidden) || !bNegativeBuildScaleForbidden ||
        !(*Contract)->TryGetObjectField(
            TEXT("ceremonialPositiveYSentinel"), Sentinel) ||
        !Sentinel || !Sentinel->IsValid() ||
        !TryGetExactString(
            *Sentinel,
            TEXT("mesh"),
            TEXT("SM_IstanaPublicView_Hardscape.obj"),
            OutError) ||
        !TryGetExactString(
            *Sentinel,
            TEXT("group"),
            TEXT("FountainPaverRing"),
            OutError) ||
        !TryGetExactString(*Sentinel, TEXT("requiredImportedSide"), TEXT("+Y"), OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("UE5.5 legacy OBJ Y/winding/normal/UV preconditioning contract changed or is incomplete.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

const FPublicViewTextureSpec* FindTextureSpec(
    const FString& TextureSet,
    EPublicViewTextureUsage Usage)
{
    return GetTextureSpecs().FindByPredicate(
        [&TextureSet, Usage](const FPublicViewTextureSpec& Candidate)
        {
            return TextureSet == Candidate.TextureSet && Usage == Candidate.Usage;
        });
}

bool ReadLinearColor4(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FLinearColor& OutColor,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    double Components[4] = {0.0, 0.0, 0.0, 0.0};
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) ||
        !Values || Values->Num() != 4)
    {
        OutError = FString::Printf(
            TEXT("JSON field '%s' must be a four-number linear color."),
            Field);
        return false;
    }
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (!(*Values)[Index].IsValid() ||
            !(*Values)[Index]->TryGetNumber(Components[Index]) ||
            !FMath::IsFinite(Components[Index]))
        {
            OutError = FString::Printf(
                TEXT("JSON field '%s' contains a non-finite color component."),
                Field);
            return false;
        }
    }
    OutColor = FLinearColor(
        Components[0], Components[1], Components[2], Components[3]);
    OutError.Reset();
    return true;
}

bool ValidateMaterialSlotMappingContract(
    const FString& MappingPath,
    FString& OutError)
{
    if (!ValidateFrozenFile(
            MappingPath,
            3509,
            FrozenMaterialSlotMappingSha256,
            OutError))
    {
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    if (!LoadJsonObjectFromFile(MappingPath, Root, OutError) ||
        !TryGetExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.istana_public_view_material_slot_mapping.v1"),
            OutError) ||
        !TryGetExactString(Root, TEXT("claimStatus"), *PublicViewClaimLabel, OutError) ||
        !TryGetExactString(Root, TEXT("textureManifest"), TEXT("Generated/manifest.json"), OutError) ||
        !TryGetExactString(
            Root,
            TEXT("pbrChannelConvention"),
            TEXT("BaseColor=sRGB; Normal=DirectX tangent; ORM=R AO/G roughness/B metallic"),
            OutError))
    {
        return false;
    }
    bool bPhotorealAcceptance = true;
    const TSharedPtr<FJsonObject>* Slots = nullptr;
    if (!Root->TryGetBoolField(
            TEXT("photorealMaterialAcceptance"),
            bPhotorealAcceptance) ||
        bPhotorealAcceptance ||
        !Root->TryGetObjectField(TEXT("slots"), Slots) || !Slots ||
        !Slots->IsValid() || (*Slots)->Values.Num() != GetMaterialSpecs().Num())
    {
        OutError = TEXT("Material-slot mapping must retain 22 exact slots and photorealMaterialAcceptance=false pending photo QA.");
        return false;
    }

    for (const FPublicViewMaterialSpec& Spec : GetMaterialSpecs())
    {
        const TSharedPtr<FJsonObject>* Slot = nullptr;
        if (!(*Slots)->TryGetObjectField(Spec.AssetName, Slot) || !Slot ||
            !Slot->IsValid() ||
            !TryGetExactString(
                *Slot,
                TEXT("shaderKind"),
                ShaderKindName(Spec.ShaderKind),
                OutError))
        {
            return false;
        }
        const TSharedPtr<FJsonValue>* TextureSetValue =
            (*Slot)->Values.Find(TEXT("textureSet"));
        if (!TextureSetValue || !TextureSetValue->IsValid())
        {
            OutError = FString::Printf(
                TEXT("Material mapping '%s' lacks textureSet field."),
                Spec.AssetName);
            return false;
        }
        if (Spec.ShaderKind == EPublicViewShaderKind::PbrTextured)
        {
            FString TextureSet;
            double MetersPerTile = 0.0;
            FLinearColor Tint;
            if (!(*Slot)->TryGetStringField(TEXT("textureSet"), TextureSet) ||
                TextureSet != Spec.TextureSet ||
                !TryGetFiniteNumber(*Slot, TEXT("uvMetersPerTile"), MetersPerTile, OutError) ||
                !FMath::IsNearlyEqual(MetersPerTile, Spec.UvMetersPerTile, 0.000001) ||
                !ReadLinearColor4(*Slot, TEXT("tintLinear"), Tint, OutError) ||
                !Tint.Equals(Spec.TintOrBaseColor, 0.000001f))
            {
                if (OutError.IsEmpty())
                {
                    OutError = FString::Printf(
                        TEXT("Textured material mapping '%s' changed texture set, metres-per-tile, or tint."),
                        Spec.AssetName);
                }
                return false;
            }
        }
        else if ((*TextureSetValue)->Type != EJson::Null)
        {
            OutError = FString::Printf(
                TEXT("Non-textured material mapping '%s' must declare textureSet=null."),
                Spec.AssetName);
            return false;
        }
        else if (Spec.ShaderKind == EPublicViewShaderKind::SpecialParameter)
        {
            FLinearColor BaseColor;
            double Roughness = 0.0;
            double Metallic = 0.0;
            if (!ReadLinearColor4(*Slot, TEXT("baseColorLinear"), BaseColor, OutError) ||
                !BaseColor.Equals(Spec.TintOrBaseColor, 0.000001f) ||
                !TryGetFiniteNumber(*Slot, TEXT("roughness"), Roughness, OutError) ||
                !TryGetFiniteNumber(*Slot, TEXT("metallic"), Metallic, OutError) ||
                !FMath::IsNearlyEqual(Roughness, Spec.Roughness, 0.000001) ||
                !FMath::IsNearlyEqual(Metallic, Spec.Metallic, 0.000001))
            {
                if (OutError.IsEmpty())
                {
                    OutError = FString::Printf(
                        TEXT("Special material mapping '%s' changed its exact parameters."),
                        Spec.AssetName);
                }
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ValidatePbrTextureSourceContract(
    const FString& ManifestPath,
    const FString& TextureDirectory,
    FString& OutError)
{
    if (!ValidateFrozenFile(
            ManifestPath,
            7952,
            FrozenTextureManifestSha256,
            OutError))
    {
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    double OutputSize = 0.0;
    bool bMeasuredMaterialScan = true;
    if (!LoadJsonObjectFromFile(ManifestPath, Root, OutError) ||
        !TryGetExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.istana_public_view_pbr_manifest.v1"),
            OutError) ||
        !TryGetExactString(Root, TEXT("claimStatus"), *PublicViewClaimLabel, OutError) ||
        !TryGetExactString(
            Root,
            TEXT("mapConvention"),
            TEXT("BaseColor=sRGB; Normal=DirectX tangent; ORM=R AO/G roughness/B metallic"),
            OutError) ||
        !TryGetFiniteNumber(Root, TEXT("outputSizePixels"), OutputSize, OutError) ||
        !FMath::IsNearlyEqual(OutputSize, 2048.0) ||
        !Root->TryGetBoolField(TEXT("measuredMaterialScan"), bMeasuredMaterialScan) ||
        bMeasuredMaterialScan)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("PBR texture manifest must retain the exact 2048px sRGB/DirectX-normal/packed-ORM, non-measured contract.");
        }
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
    if (!Root->TryGetArrayField(TEXT("records"), Records) || !Records ||
        Records->Num() != 8)
    {
        OutError = TEXT("PBR texture manifest must contain exactly eight surface records.");
        return false;
    }
    TSet<FString> SeenTextures;
    for (const TSharedPtr<FJsonValue>& RecordValue : *Records)
    {
        const TSharedPtr<FJsonObject> Record = RecordValue.IsValid()
            ? RecordValue->AsObject()
            : nullptr;
        FString TextureSet;
        const TSharedPtr<FJsonObject>* Outputs = nullptr;
        if (!Record.IsValid() ||
            !Record->TryGetStringField(TEXT("id"), TextureSet) ||
            !Record->TryGetObjectField(TEXT("outputs"), Outputs) || !Outputs ||
            !Outputs->IsValid())
        {
            OutError = TEXT("PBR texture manifest contains an incomplete surface record.");
            return false;
        }
        const EPublicViewTextureUsage Usages[] = {
            EPublicViewTextureUsage::BaseColor,
            EPublicViewTextureUsage::Normal,
            EPublicViewTextureUsage::PackedOrm};
        const TCHAR* OutputFields[] = {TEXT("baseColor"), TEXT("normal"), TEXT("orm")};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FPublicViewTextureSpec* Spec =
                FindTextureSpec(TextureSet, Usages[Index]);
            const TSharedPtr<FJsonObject>* Output = nullptr;
            FString Filename;
            FString Digest;
            double ByteCount = -1.0;
            double EdgeError = -1.0;
            if (!Spec ||
                !(*Outputs)->TryGetObjectField(OutputFields[Index], Output) ||
                !Output || !Output->IsValid() ||
                !(*Output)->TryGetStringField(TEXT("file"), Filename) ||
                !(*Output)->TryGetStringField(TEXT("sha256"), Digest) ||
                !TryGetFiniteNumber(*Output, TEXT("bytes"), ByteCount, OutError) ||
                !TryGetFiniteNumber(*Output, TEXT("edgeErrorMaximumByte"), EdgeError, OutError) ||
                Filename != FString(Spec->AssetName) + TEXT(".png") ||
                Digest.ToLower() != Spec->FrozenSha256 ||
                static_cast<double>(Spec->FrozenBytes) != ByteCount ||
                !FMath::IsNearlyZero(EdgeError) ||
                SeenTextures.Contains(Spec->AssetName))
            {
                if (OutError.IsEmpty())
                {
                    OutError = FString::Printf(
                        TEXT("PBR output contract changed for texture set '%s' field '%s'."),
                        *TextureSet,
                        OutputFields[Index]);
                }
                return false;
            }
            const FString SourcePath = FPaths::Combine(TextureDirectory, Filename);
            if (!ValidateFrozenFile(
                    SourcePath,
                    Spec->FrozenBytes,
                    Spec->FrozenSha256,
                    OutError))
            {
                return false;
            }
            SeenTextures.Add(Spec->AssetName);
        }
    }
    if (SeenTextures.Num() != GetTextureSpecs().Num())
    {
        OutError = FString::Printf(
            TEXT("PBR manifest validated %d/%d exact texture outputs."),
            SeenTextures.Num(),
            GetTextureSpecs().Num());
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateManifestInventory(
    const FString& ManifestPath,
    const TCHAR* ExpectedSchema,
    const FString& ContractDigest,
    const FString& GeneratedDirectory,
    TSet<FString>& InOutSeenFiles,
    FString& OutError)
{
    TSharedPtr<FJsonObject> Manifest;
    if (!LoadJsonObjectFromFile(ManifestPath, Manifest, OutError) ||
        !TryGetExactString(Manifest, TEXT("schema"), ExpectedSchema, OutError) ||
        !TryGetExactString(Manifest, TEXT("referenceEpoch"), TEXT("2024-04"), OutError) ||
        !TryGetExactString(Manifest, TEXT("claimStatus"), *PublicViewClaimLabel, OutError) ||
        !TryGetExactString(Manifest, TEXT("sourceClass"), TEXT("ORIGINAL_PROCEDURAL"), OutError) ||
        !TryGetExactString(Manifest, TEXT("contractSha256"), *ContractDigest, OutError) ||
        !ValidateSurfaceContract(Manifest, OutError))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("files"), Records) || !Records)
    {
        OutError = FString::Printf(TEXT("Manifest '%s' has no file inventory."), *ManifestPath);
        return false;
    }
    const TMap<FString, FString> ExpectedRoles = GetExpectedSourceRoles();
    for (const TSharedPtr<FJsonValue>& RecordValue : *Records)
    {
        const TSharedPtr<FJsonObject> Record = RecordValue.IsValid()
            ? RecordValue->AsObject()
            : nullptr;
        FString Role;
        FString RelativePath;
        FString Digest;
        double ByteCount = -1.0;
        if (!Record.IsValid() ||
            !Record->TryGetStringField(TEXT("role"), Role) ||
            !Record->TryGetStringField(TEXT("path"), RelativePath) ||
            !Record->TryGetStringField(TEXT("sha256"), Digest) ||
            !Record->TryGetNumberField(TEXT("bytes"), ByteCount) ||
            !FMath::IsFinite(ByteCount))
        {
            OutError = FString::Printf(TEXT("Manifest '%s' contains an incomplete file record."), *ManifestPath);
            return false;
        }
        const FString* ExpectedRole = ExpectedRoles.Find(RelativePath);
        const FPublicViewFrozenSourceFile* Frozen =
            FindFrozenSourceFile(RelativePath);
        if (!ExpectedRole || !Frozen || Role != *ExpectedRole ||
            RelativePath != FPaths::GetCleanFilename(RelativePath) ||
            RelativePath.Contains(TEXT("..")) || InOutSeenFiles.Contains(RelativePath) ||
            static_cast<double>(Frozen->FrozenBytes) != ByteCount ||
            Digest.ToLower() != Frozen->FrozenSha256)
        {
            OutError = FString::Printf(
                TEXT("Manifest '%s' contains an unexpected, unsafe, or duplicate role/path '%s' / '%s'."),
                *ManifestPath,
                *Role,
                *RelativePath);
            return false;
        }
        const FString FullPath = FPaths::Combine(GeneratedDirectory, RelativePath);
        const int64 ActualSize = IFileManager::Get().FileSize(*FullPath);
        FString ActualDigest;
        if (ActualSize <= 0 ||
            ActualSize != Frozen->FrozenBytes ||
            !CalculateSha256(FullPath, ActualDigest, OutError) ||
            ActualDigest != Frozen->FrozenSha256)
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("Generated source '%s' does not match its byte-count/SHA-256 manifest record."),
                    *FullPath);
            }
            return false;
        }
        if (const FPublicViewMeshSpec* MeshSpec = FindMeshSpecBySourceFile(RelativePath))
        {
            if (!TryGetExactString(
                    Record,
                    TEXT("uvContract"),
                    TEXT("UV0_CONTINUOUS_SOURCE_METRES"),
                    OutError) ||
                !TryGetExactString(
                    Record,
                    TEXT("normalContract"),
                    TEXT("EXPLICIT_HARD_AND_AREA_WEIGHTED_SMOOTH_NORMALS"),
                    OutError) ||
                !TryGetExactString(
                    Record,
                    TEXT("coordinateExportContract"),
                    *LegacyObjExportContract,
                    OutError) ||
                !ValidateObjSlotContract(FullPath, *MeshSpec, OutError))
            {
                return false;
            }
        }
        InOutSeenFiles.Add(RelativePath);
    }
    return true;
}

bool ValidateTreePlacementRow(
    const TSharedPtr<FJsonObject>& Row,
    TSet<FString>& InOutIdentifiers,
    TArray<FPublicViewTreePlacement>& OutPlacements,
    int32& InOutHeroCount,
    int32* InOutRadialBandCounts,
    FString& OutError)
{
    const TCHAR* const ExpectedFields[] = {
        TEXT("id"), TEXT("archetypeAssetName"), TEXT("positionMeters"),
        TEXT("yawDegrees"), TEXT("uniformScale"), TEXT("zone")};
    if (!HasExactJsonFields(Row, ExpectedFields, UE_ARRAY_COUNT(ExpectedFields), OutError))
    {
        return false;
    }
    FString Identifier;
    FString AssetName;
    FString Zone;
    FVector PositionMeters;
    double YawDegrees = 0.0;
    double UniformScale = 0.0;
    if (!Row->TryGetStringField(TEXT("id"), Identifier) || Identifier.IsEmpty() ||
        InOutIdentifiers.Contains(Identifier) ||
        !Row->TryGetStringField(TEXT("archetypeAssetName"), AssetName) ||
        !Row->TryGetStringField(TEXT("zone"), Zone) || Zone.IsEmpty() ||
        !ReadFiniteVector3(Row, TEXT("positionMeters"), PositionMeters, OutError) ||
        !TryGetFiniteNumber(Row, TEXT("yawDegrees"), YawDegrees, OutError) ||
        !TryGetFiniteNumber(Row, TEXT("uniformScale"), UniformScale, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Tree placement has an absent/duplicate ID, archetype, or zone.");
        }
        return false;
    }

    FName Archetype;
    if (AssetName == TEXT("SM_IstanaPublicView_Tree_Rain"))
    {
        Archetype = TEXT("RAIN");
    }
    else if (AssetName == TEXT("SM_IstanaPublicView_Tree_Palm"))
    {
        Archetype = TEXT("PALM");
    }
    else if (AssetName == TEXT("SM_IstanaPublicView_Tree_Framing"))
    {
        Archetype = TEXT("FRAMING");
    }
    else
    {
        OutError = FString::Printf(TEXT("Tree '%s' uses unknown isolated archetype '%s'."), *Identifier, *AssetName);
        return false;
    }
    const double Radius = FVector2D(PositionMeters.X, PositionMeters.Y).Size();
    if (Radius < 90.0 || Radius > ContextRadiusMeters + 0.000001 ||
        UniformScale < 0.1 || UniformScale > 5.0)
    {
        OutError = FString::Printf(TEXT("Tree '%s' violates radius/scale limits."), *Identifier);
        return false;
    }
    if (Radius <= HeroTreeRadiusMeters)
    {
        ++InOutHeroCount;
        ++InOutRadialBandCounts[0];
    }
    else if (Radius <= 500.0)
    {
        ++InOutRadialBandCounts[1];
    }
    else if (Radius <= 750.0)
    {
        ++InOutRadialBandCounts[2];
    }
    else
    {
        ++InOutRadialBandCounts[3];
    }

    FPublicViewTreePlacement Placement;
    Placement.Archetype = Archetype;
    Placement.LocalTransform = FTransform(
        FRotator(0.0, YawDegrees, 0.0),
        PositionMeters * 100.0,
        FVector(UniformScale));
    OutPlacements.Add(Placement);
    InOutIdentifiers.Add(Identifier);
    return true;
}

bool ValidateContextPlacementRow(
    const TSharedPtr<FJsonObject>& Row,
    TSet<FString>& InOutIdentifiers,
    FString& OutError)
{
    const TCHAR* const ExpectedFields[] = {
        TEXT("id"), TEXT("archetypeIndex"), TEXT("positionMeters"),
        TEXT("yawDegrees"), TEXT("scaleMeters"), TEXT("zone")};
    if (!HasExactJsonFields(Row, ExpectedFields, UE_ARRAY_COUNT(ExpectedFields), OutError))
    {
        return false;
    }
    FString Identifier;
    FString Zone;
    FVector PositionMeters;
    FVector ScaleMeters;
    double ArchetypeIndex = -1.0;
    double YawDegrees = 0.0;
    if (!Row->TryGetStringField(TEXT("id"), Identifier) || Identifier.IsEmpty() ||
        InOutIdentifiers.Contains(Identifier) ||
        !Row->TryGetStringField(TEXT("zone"), Zone) || Zone.IsEmpty() ||
        !ReadFiniteVector3(Row, TEXT("positionMeters"), PositionMeters, OutError) ||
        !ReadFiniteVector3(Row, TEXT("scaleMeters"), ScaleMeters, OutError) ||
        !TryGetFiniteNumber(Row, TEXT("archetypeIndex"), ArchetypeIndex, OutError) ||
        !TryGetFiniteNumber(Row, TEXT("yawDegrees"), YawDegrees, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Anonymous context placement has an absent/duplicate ID or zone.");
        }
        return false;
    }
    const double Radius = FVector2D(PositionMeters.X, PositionMeters.Y).Size();
    if (Radius < 300.0 || Radius > ContextRadiusMeters + 0.000001 ||
        ArchetypeIndex < 0.0 || ArchetypeIndex != FMath::FloorToDouble(ArchetypeIndex) ||
        ScaleMeters.GetMin() <= 0.0 || ScaleMeters.GetMax() > 200.0)
    {
        OutError = FString::Printf(TEXT("Anonymous context placement '%s' violates its public-view bounds."), *Identifier);
        return false;
    }
    InOutIdentifiers.Add(Identifier);
    return true;
}

bool ParsePublicViewPlacements(
    const FString& InstancePath,
    TArray<FPublicViewTreePlacement>& OutPlacements,
    FString& OutReport)
{
    OutPlacements.Reset();
    TSharedPtr<FJsonObject> Root;
    if (!LoadJsonObjectFromFile(InstancePath, Root, OutReport))
    {
        return false;
    }
    const TCHAR* const ExpectedFields[] = {
        TEXT("schema"), TEXT("sceneId"), TEXT("referenceEpoch"),
        TEXT("claimStatus"), TEXT("sourceClass"), TEXT("coordinateSystem"),
        TEXT("unrealTransform"), TEXT("contextRadiusMeters"),
        TEXT("containsRouteGraph"), TEXT("treeInstances"),
        TEXT("contextBuildingInstances")};
    if (!HasExactJsonFields(Root, ExpectedFields, UE_ARRAY_COUNT(ExpectedFields), OutReport) ||
        !TryGetExactString(Root, TEXT("schema"), *InstanceSchema, OutReport) ||
        !TryGetExactString(Root, TEXT("sceneId"), TEXT("istana-public-view-exterior-v1"), OutReport) ||
        !TryGetExactString(Root, TEXT("referenceEpoch"), TEXT("2024-04"), OutReport) ||
        !TryGetExactString(Root, TEXT("claimStatus"), *PublicViewClaimLabel, OutReport) ||
        !TryGetExactString(Root, TEXT("sourceClass"), TEXT("ORIGINAL_PROCEDURAL"), OutReport) ||
        !TryGetExactString(Root, TEXT("coordinateSystem"), *InstanceCoordinateSystem, OutReport) ||
        !TryGetExactString(
            Root,
            TEXT("unrealTransform"),
            TEXT("position metres multiplied by 100; yaw about +Z; uniform scale"),
            OutReport))
    {
        return false;
    }
    double Radius = 0.0;
    bool bContainsRouteGraph = true;
    if (!TryGetFiniteNumber(Root, TEXT("contextRadiusMeters"), Radius, OutReport) ||
        !FMath::IsNearlyEqual(Radius, ContextRadiusMeters, 0.000001) ||
        !Root->TryGetBoolField(TEXT("containsRouteGraph"), bContainsRouteGraph) ||
        bContainsRouteGraph)
    {
        OutReport = TEXT("Instance document must retain a 1000 m radius and containsRouteGraph=false.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Trees = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* ContextBuildings = nullptr;
    if (!Root->TryGetArrayField(TEXT("treeInstances"), Trees) || !Trees ||
        !Root->TryGetArrayField(TEXT("contextBuildingInstances"), ContextBuildings) ||
        !ContextBuildings)
    {
        OutReport = TEXT("Instance document lacks tree/context arrays.");
        return false;
    }
    if (Trees->Num() < RequiredTreeInstanceCount || ContextBuildings->Num() < 160)
    {
        OutReport = FString::Printf(
            TEXT("ASSETS_MISSING: require at least %d trees and 160 anonymous context buildings; found %d and %d."),
            RequiredTreeInstanceCount,
            Trees->Num(),
            ContextBuildings->Num());
        return false;
    }

    TSet<FString> Identifiers;
    int32 HeroCount = 0;
    int32 RadialBandCounts[4] = {0, 0, 0, 0};
    TSet<FName> Archetypes;
    for (const TSharedPtr<FJsonValue>& TreeValue : *Trees)
    {
        const TSharedPtr<FJsonObject> Tree = TreeValue.IsValid()
            ? TreeValue->AsObject()
            : nullptr;
        const int32 PreviousCount = OutPlacements.Num();
        if (!Tree.IsValid() ||
            !ValidateTreePlacementRow(
                Tree,
                Identifiers,
                OutPlacements,
                HeroCount,
                RadialBandCounts,
                OutReport))
        {
            return false;
        }
        Archetypes.Add(OutPlacements[PreviousCount].Archetype);
    }
    for (const TSharedPtr<FJsonValue>& ContextValue : *ContextBuildings)
    {
        const TSharedPtr<FJsonObject> Context = ContextValue.IsValid()
            ? ContextValue->AsObject()
            : nullptr;
        if (!Context.IsValid() ||
            !ValidateContextPlacementRow(Context, Identifiers, OutReport))
        {
            return false;
        }
    }
    if (HeroCount < RequiredHeroTreeInstanceCount || Archetypes.Num() != 3 ||
        RadialBandCounts[0] <= 0 || RadialBandCounts[1] <= 0 ||
        RadialBandCounts[2] <= 0 || RadialBandCounts[3] <= 0)
    {
        OutReport = FString::Printf(
            TEXT("Volumetric tree placement contract failed: hero=%d/%d, archetypes=%d/3, radial bands=%d/%d/%d/%d."),
            HeroCount,
            RequiredHeroTreeInstanceCount,
            Archetypes.Num(),
            RadialBandCounts[0],
            RadialBandCounts[1],
            RadialBandCounts[2],
            RadialBandCounts[3]);
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Validated %d full-volume tree placements (%d within 250 m) and %d anonymous context placements."),
        OutPlacements.Num(),
        HeroCount,
        ContextBuildings->Num());
    return true;
}

bool ValidatePublicViewSourceSet(
    TArray<FPublicViewTreePlacement>* OutPlacements,
    FString& OutReport)
{
    const FString SourceRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/IstanaPublicView")));
    const FString GeneratedDirectory = FPaths::Combine(SourceRoot, TEXT("Generated"));
    const FString ContractPath = FPaths::Combine(
        SourceRoot,
        TEXT("istana_public_view.contract.json"));
    const FString BuildingManifestPath = FPaths::Combine(
        GeneratedDirectory,
        TEXT("IstanaPublicViewBuilding.manifest.json"));
    const FString ContextManifestPath = FPaths::Combine(
        GeneratedDirectory,
        TEXT("IstanaPublicViewContext.manifest.json"));
    const FString InstancePath = FPaths::Combine(
        GeneratedDirectory,
        TEXT("IstanaPublicView.instances.json"));
    const FString TextureDirectory = FPaths::Combine(
        SourceRoot,
        TEXT("Textures/Generated"));
    const FString TextureManifestPath = FPaths::Combine(
        TextureDirectory,
        TEXT("manifest.json"));
    const FString MaterialSlotMappingPath = FPaths::Combine(
        SourceRoot,
        TEXT("Textures/material_slot_mapping.json"));

    TSharedPtr<FJsonObject> Contract;
    FString Error;
    if (!ValidateFrozenFile(
            ContractPath,
            5036,
            FrozenSourceContractSha256,
            Error) ||
        !ValidateFrozenFile(
            BuildingManifestPath,
            7121,
            FrozenBuildingManifestSha256,
            Error) ||
        !ValidateFrozenFile(
            ContextManifestPath,
            15611,
            FrozenContextManifestSha256,
            Error) ||
        !ValidateMaterialSlotMappingContract(MaterialSlotMappingPath, Error) ||
        !ValidatePbrTextureSourceContract(
            TextureManifestPath,
            TextureDirectory,
            Error) ||
        !ValidateOsmContextSourceContract(Error) ||
        !LoadJsonObjectFromFile(ContractPath, Contract, Error))
    {
        OutReport = TEXT("ASSETS_MISSING: ") + Error;
        return false;
    }
    if (!TryGetExactString(
            Contract,
            TEXT("schema"),
            TEXT("triad.istana_public_view_contract.v1"),
            Error) ||
        !TryGetExactString(Contract, TEXT("sceneId"), TEXT("istana-public-view-exterior-v1"), Error) ||
        !TryGetExactString(Contract, TEXT("referenceEpoch"), TEXT("2024-04"), Error) ||
        !TryGetExactString(Contract, TEXT("claimStatus"), *PublicViewClaimLabel, Error) ||
        !TryGetExactString(Contract, TEXT("sourceClass"), TEXT("ORIGINAL_PROCEDURAL"), Error) ||
        !ValidateLegacyObjImportContract(Contract, Error) ||
        !ValidateSurfaceContract(Contract, Error))
    {
        OutReport = TEXT("SOURCE_CONTRACT_INVALID: ") + Error;
        return false;
    }

    const TSharedPtr<FJsonObject>* ScopeField = nullptr;
    const TSharedPtr<FJsonObject>* ContextRequirementsField = nullptr;
    if (!Contract->TryGetObjectField(TEXT("scope"), ScopeField) || !ScopeField ||
        !ScopeField->IsValid() ||
        !Contract->TryGetObjectField(
            TEXT("contextRequirements"),
            ContextRequirementsField) ||
        !ContextRequirementsField || !ContextRequirementsField->IsValid())
    {
        OutReport = TEXT("SOURCE_CONTRACT_INVALID: scope/context requirements are absent.");
        return false;
    }
    bool bPublicExteriorOnly = false;
    bool bArbitraryClaim = true;
    double ContractRadius = 0.0;
    double ContractHeroRadius = 0.0;
    if (!(*ScopeField)->TryGetBoolField(
            TEXT("publicExteriorOnly"),
            bPublicExteriorOnly) ||
        !bPublicExteriorOnly ||
        !(*ScopeField)->TryGetBoolField(
            TEXT("arbitraryViewIndistinguishabilityClaimed"),
            bArbitraryClaim) ||
        bArbitraryClaim ||
        !TryGetFiniteNumber(
            *ScopeField,
            TEXT("localContextRadiusMeters"),
            ContractRadius,
            Error) ||
        ContractRadius < ContextRadiusMeters ||
        !TryGetFiniteNumber(
            *ScopeField,
            TEXT("heroDetailRadiusMeters"),
            ContractHeroRadius,
            Error) ||
        !FMath::IsNearlyEqual(ContractHeroRadius, HeroTreeRadiusMeters, 0.000001))
    {
        OutReport = TEXT("SOURCE_CONTRACT_INVALID: public scope, claim, or radii changed.");
        return false;
    }

    double ContractTreeMinimum = 0.0;
    double ContractHeroMinimum = 0.0;
    double ContractContextBuildingMinimum = 0.0;
    bool bBillboardsAllowed = true;
    bool bImagePlanesAllowed = true;
    bool bPanoramaCardsAllowed = true;
    bool bRouteGraphsAllowed = true;
    FString TerrainBoundaryTreatment;
    if (!TryGetFiniteNumber(
            *ContextRequirementsField,
            TEXT("treeInstanceMinimum"),
            ContractTreeMinimum,
            Error) ||
        ContractTreeMinimum < RequiredTreeInstanceCount ||
        !TryGetFiniteNumber(
            *ContextRequirementsField,
            TEXT("heroTreeInstanceMinimum"),
            ContractHeroMinimum,
            Error) ||
        ContractHeroMinimum < RequiredHeroTreeInstanceCount ||
        !TryGetFiniteNumber(
            *ContextRequirementsField,
            TEXT("anonymousContextBuildingMinimum"),
            ContractContextBuildingMinimum,
            Error) ||
        ContractContextBuildingMinimum < 160.0 ||
        !(*ContextRequirementsField)->TryGetBoolField(
            TEXT("wholeTreeBillboardsAllowed"),
            bBillboardsAllowed) || bBillboardsAllowed ||
        !(*ContextRequirementsField)->TryGetBoolField(
            TEXT("imagePlanesAllowed"),
            bImagePlanesAllowed) || bImagePlanesAllowed ||
        !(*ContextRequirementsField)->TryGetBoolField(
            TEXT("panoramaCardsAllowed"),
            bPanoramaCardsAllowed) || bPanoramaCardsAllowed ||
        !(*ContextRequirementsField)->TryGetBoolField(
            TEXT("routeGraphsAllowed"),
            bRouteGraphsAllowed) || bRouteGraphsAllowed ||
        !(*ContextRequirementsField)->TryGetStringField(
            TEXT("terrainBoundaryTreatment"),
            TerrainBoundaryTreatment) ||
        TerrainBoundaryTreatment !=
            TEXT("SYNTHETIC_DOWNWARD_VISUAL_ONLY_SKIRT_NO_COLLISION"))
    {
        OutReport = TEXT("SOURCE_CONTRACT_INVALID: volumetric context minimums/exclusions changed.");
        return false;
    }

    FString ContractDigest;
    if (!CalculateSha256(ContractPath, ContractDigest, Error))
    {
        OutReport = TEXT("SOURCE_CONTRACT_INVALID: ") + Error;
        return false;
    }
    if (ContractDigest != FrozenSourceContractSha256)
    {
        OutReport = TEXT("SOURCE_CONTRACT_INVALID: source-contract digest is not the frozen PBR geometry contract.");
        return false;
    }
    TSet<FString> SeenFiles;
    if (!ValidateManifestInventory(
            BuildingManifestPath,
            TEXT("triad.istana_public_view_building_manifest.v1"),
            ContractDigest,
            GeneratedDirectory,
            SeenFiles,
            Error) ||
        !ValidateManifestInventory(
            ContextManifestPath,
            TEXT("triad.istana_public_view_context_manifest.v1"),
            ContractDigest,
            GeneratedDirectory,
            SeenFiles,
            Error))
    {
        OutReport = TEXT("ASSETS_MISSING_OR_INVALID: ") + Error;
        return false;
    }
    const TMap<FString, FString> ExpectedRoles = GetExpectedSourceRoles();
    if (SeenFiles.Num() != ExpectedRoles.Num())
    {
        OutReport = FString::Printf(
            TEXT("ASSETS_MISSING_OR_INVALID: exact generated inventory requires %d files; manifests bind %d."),
            ExpectedRoles.Num(),
            SeenFiles.Num());
        return false;
    }
    for (const TPair<FString, FString>& Expected : ExpectedRoles)
    {
        if (!SeenFiles.Contains(Expected.Key))
        {
            OutReport = TEXT("ASSETS_MISSING: generated manifest inventory lacks '") +
                Expected.Key + TEXT("'.");
            return false;
        }
    }

    TArray<FPublicViewTreePlacement> Placements;
    FString PlacementReport;
    if (!ParsePublicViewPlacements(InstancePath, Placements, PlacementReport))
    {
        OutReport = TEXT("ASSETS_MISSING_OR_INVALID: ") + PlacementReport;
        return false;
    }
    if (OutPlacements)
    {
        *OutPlacements = MoveTemp(Placements);
    }
    OutReport = FString::Printf(
        TEXT("Validated frozen ten-mesh SHA-256 inventory: nine original meshes plus the exact ODbL-derived mapping-grade outer-context mesh. All retain identity BuildScale3D, continuous-metre UV0, imported normals, and recomputed MikkTSpace tangents; the PBR pack has 24 exact textures and 22 mapped materials. OSM alignment is coordinate-contract validated, not visually/photo accepted, and never sensor truth. %s Attribution=%s (ODbL-1.0). Claim=%s; photoreal material acceptance remains false pending photo QA."),
        *PlacementReport,
        *OsmAttribution,
        *PublicViewClaimLabel);
    return true;
}

bool ValidatePublicViewTexture(
    UTexture2D* Texture,
    const FPublicViewTextureSpec& Spec,
    FString& OutError)
{
    const FString ExpectedPath = TextureObjectPath(Spec);
    if (!Texture || Texture->GetPathName() != ExpectedPath ||
        !Texture->GetPathName().StartsWith(PublicViewAssetRoot + TEXT("/")) ||
        Texture->Source.GetSizeX() != 2048 ||
        Texture->Source.GetSizeY() != 2048 ||
        static_cast<bool>(Texture->SRGB) != IsTextureSrgb(Spec.Usage) ||
        Texture->CompressionSettings != GetTextureCompression(Spec.Usage) ||
        Texture->LODGroup != GetTextureGroup(Spec.Usage) ||
        Texture->MipGenSettings != TMGS_FromTextureGroup ||
        Texture->AddressX != TA_Wrap || Texture->AddressY != TA_Wrap ||
        Texture->bFlipGreenChannel)
    {
        OutError = FString::Printf(
            TEXT("Texture '%s' does not retain exact 2048px color-space/compression/mip/wrap/DirectX-normal settings."),
            *ExpectedPath);
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterialExpressionTextureSampleParameter2D* CreatePublicViewTextureParameter(
    UMaterial* Material,
    UTexture2D* Texture,
    const FName& ParameterName,
    EMaterialSamplerType SamplerType,
    int32 X,
    int32 Y)
{
    UMaterialExpressionTextureSampleParameter2D* Expression =
        Cast<UMaterialExpressionTextureSampleParameter2D>(
            UMaterialEditingLibrary::CreateMaterialExpressionEx(
                Material,
                nullptr,
                UMaterialExpressionTextureSampleParameter2D::StaticClass(),
                Texture,
                X,
                Y));
    if (Expression)
    {
        Expression->Texture = Texture;
        Expression->ParameterName = ParameterName;
        Expression->Group = FName(TEXT("Istana Public View PBR"));
        Expression->SamplerType = SamplerType;
        Expression->AutoSetSampleType();
    }
    return Expression;
}

bool ConnectPublicViewExpression(
    UMaterialExpression* From,
    const TCHAR* FromOutput,
    UMaterialExpression* To,
    const TCHAR* ToInput,
    const TCHAR* Description,
    FString& OutError)
{
    if (!UMaterialEditingLibrary::ConnectMaterialExpressions(
            From,
            FString(FromOutput),
            To,
            FString(ToInput)))
    {
        OutError = FString::Printf(
            TEXT("Could not connect public-view PBR graph edge '%s'."),
            Description);
        return false;
    }
    return true;
}

bool ConnectPublicViewProperty(
    UMaterialExpression* From,
    const TCHAR* FromOutput,
    EMaterialProperty Property,
    const TCHAR* Description,
    FString& OutError)
{
    if (!UMaterialEditingLibrary::ConnectMaterialProperty(
            From,
            FString(FromOutput),
            Property))
    {
        OutError = FString::Printf(
            TEXT("Could not connect public-view material property '%s'."),
            Description);
        return false;
    }
    return true;
}

UTexture2D* FindMaterialTexture(
    const FPublicViewMaterialSpec& MaterialSpec,
    EPublicViewTextureUsage Usage,
    const TMap<FName, UTexture2D*>& Textures)
{
    if (!MaterialSpec.TextureSet)
    {
        return nullptr;
    }
    return Textures.FindRef(
        FName(*TextureAssetName(MaterialSpec.TextureSet, Usage)));
}

bool ValidateDirectTextureBinding(
    UMaterial* Material,
    EMaterialProperty Property,
    const TCHAR* OutputName,
    UTexture2D* ExpectedTexture,
    const FName& ExpectedParameter,
    EMaterialSamplerType ExpectedSamplerType,
    FString& OutError)
{
    const UMaterialExpressionTextureSampleParameter2D* Sample =
        Cast<UMaterialExpressionTextureSampleParameter2D>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(
                Material,
                Property));
    if (!Sample || Sample->Texture != ExpectedTexture ||
        Sample->ParameterName != ExpectedParameter ||
        Sample->SamplerType != ExpectedSamplerType ||
        UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(
            Material,
            Property) != OutputName)
    {
        OutError = FString::Printf(
            TEXT("Material '%s' property %d is not bound to exact texture '%s' output '%s'."),
            *Material->GetPathName(),
            static_cast<int32>(Property),
            ExpectedTexture ? *ExpectedTexture->GetPathName() : TEXT("<null>"),
            OutputName);
        return false;
    }
    return true;
}

bool ValidatePbrPublicViewMaterial(
    UMaterial* Material,
    const FPublicViewMaterialSpec& Spec,
    const TMap<FName, UTexture2D*>& Textures,
    FString& OutError)
{
    UTexture2D* BaseColor = FindMaterialTexture(
        Spec, EPublicViewTextureUsage::BaseColor, Textures);
    UTexture2D* Normal = FindMaterialTexture(
        Spec, EPublicViewTextureUsage::Normal, Textures);
    UTexture2D* PackedOrm = FindMaterialTexture(
        Spec, EPublicViewTextureUsage::PackedOrm, Textures);
    if (!BaseColor || !Normal || !PackedOrm ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->GetShadingModels().HasShadingModel(MSM_DefaultLit) ||
        !ValidateDirectTextureBinding(
            Material, MP_Normal, TEXT("RGB"), Normal,
            TEXT("NormalTexture"), SAMPLERTYPE_Normal, OutError) ||
        !ValidateDirectTextureBinding(
            Material, MP_AmbientOcclusion, TEXT("R"), PackedOrm,
            TEXT("PackedORMTexture"), SAMPLERTYPE_Masks, OutError) ||
        !ValidateDirectTextureBinding(
            Material, MP_Roughness, TEXT("G"), PackedOrm,
            TEXT("PackedORMTexture"), SAMPLERTYPE_Masks, OutError) ||
        !ValidateDirectTextureBinding(
            Material, MP_Metallic, TEXT("B"), PackedOrm,
            TEXT("PackedORMTexture"), SAMPLERTYPE_Masks, OutError))
    {
        return false;
    }

    UMaterialExpressionTextureCoordinate* TextureCoordinate = nullptr;
    UMaterialExpressionTextureSampleParameter2D* BaseSample = nullptr;
    UMaterialExpressionTextureSampleParameter2D* NormalSample = nullptr;
    UMaterialExpressionTextureSampleParameter2D* OrmSample = nullptr;
    UMaterialExpressionMultiply* TintMultiply = nullptr;
    UMaterialExpressionConstant3Vector* Tint = nullptr;
    int32 TextureSampleCount = 0;
    for (UMaterialExpression* Expression : Material->GetExpressions())
    {
        if (UMaterialExpressionTextureCoordinate* Candidate =
                Cast<UMaterialExpressionTextureCoordinate>(Expression))
        {
            if (TextureCoordinate)
            {
                OutError = TEXT("Textured public-view material has duplicate texture-coordinate nodes.");
                return false;
            }
            TextureCoordinate = Candidate;
        }
        if (UMaterialExpressionTextureSampleParameter2D* Sample =
                Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            ++TextureSampleCount;
            if (Sample->ParameterName == TEXT("BaseColorTexture"))
            {
                BaseSample = Sample;
            }
            else if (Sample->ParameterName == TEXT("NormalTexture"))
            {
                NormalSample = Sample;
            }
            else if (Sample->ParameterName == TEXT("PackedORMTexture"))
            {
                OrmSample = Sample;
            }
        }
        TintMultiply = TintMultiply
            ? TintMultiply
            : Cast<UMaterialExpressionMultiply>(Expression);
        Tint = Tint ? Tint : Cast<UMaterialExpressionConstant3Vector>(Expression);
    }
    const float ExpectedCoordinateScale = 1.0f / Spec.UvMetersPerTile;
    const TArray<UMaterialExpression*> BaseInputs = BaseSample
        ? UMaterialEditingLibrary::GetInputsForMaterialExpression(Material, BaseSample)
        : TArray<UMaterialExpression*>();
    const TArray<UMaterialExpression*> NormalInputs = NormalSample
        ? UMaterialEditingLibrary::GetInputsForMaterialExpression(Material, NormalSample)
        : TArray<UMaterialExpression*>();
    const TArray<UMaterialExpression*> OrmInputs = OrmSample
        ? UMaterialEditingLibrary::GetInputsForMaterialExpression(Material, OrmSample)
        : TArray<UMaterialExpression*>();
    const TArray<UMaterialExpression*> MultiplyInputs = TintMultiply
        ? UMaterialEditingLibrary::GetInputsForMaterialExpression(Material, TintMultiply)
        : TArray<UMaterialExpression*>();
    if (!TextureCoordinate || TextureCoordinate->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(TextureCoordinate->UTiling, ExpectedCoordinateScale, 0.000001f) ||
        !FMath::IsNearlyEqual(TextureCoordinate->VTiling, ExpectedCoordinateScale, 0.000001f) ||
        TextureSampleCount != 3 || !BaseSample || !NormalSample || !OrmSample ||
        BaseSample->Texture != BaseColor ||
        BaseSample->SamplerType != SAMPLERTYPE_Color ||
        !BaseInputs.Contains(TextureCoordinate) ||
        !NormalInputs.Contains(TextureCoordinate) ||
        !OrmInputs.Contains(TextureCoordinate) ||
        !TintMultiply || !Tint ||
        !Tint->Constant.Equals(Spec.TintOrBaseColor, 0.000001f) ||
        !MultiplyInputs.Contains(BaseSample) || !MultiplyInputs.Contains(Tint) ||
        UMaterialEditingLibrary::GetMaterialPropertyInputNode(
            Material,
            MP_BaseColor) != TintMultiply)
    {
        OutError = FString::Printf(
            TEXT("Material '%s' does not retain exact UV0 metres-per-tile, three-map PBR wiring, and base-color tint graph."),
            *Material->GetPathName());
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateConstantPublicViewMaterial(
    UMaterial* Material,
    const FPublicViewMaterialSpec& Spec,
    FString& OutError)
{
    if (Material->MaterialDomain != MD_Surface ||
        Material->BlendMode !=
            (Spec.bTranslucent ? BLEND_Translucent : BLEND_Opaque) ||
        Material->TwoSided ||
        !Material->GetShadingModels().HasShadingModel(MSM_DefaultLit) ||
        (Spec.bTranslucent &&
         Material->TranslucencyLightingMode != TLM_SurfacePerPixelLighting))
    {
        OutError = FString::Printf(
            TEXT("Purpose-built material '%s' has the wrong surface/blend/shading contract."),
            *Material->GetPathName());
        return false;
    }
    const UMaterialExpressionConstant3Vector* BaseColor =
        Cast<UMaterialExpressionConstant3Vector>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(
                Material, MP_BaseColor));
    const UMaterialExpressionConstant* Roughness =
        Cast<UMaterialExpressionConstant>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(
                Material, MP_Roughness));
    const UMaterialExpressionConstant* Metallic =
        Cast<UMaterialExpressionConstant>(
            UMaterialEditingLibrary::GetMaterialPropertyInputNode(
                Material, MP_Metallic));
    for (UMaterialExpression* Expression : Material->GetExpressions())
    {
        if (Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            OutError = FString::Printf(
                TEXT("Purpose-built non-textured material '%s' unexpectedly samples a bitmap set."),
                *Material->GetPathName());
            return false;
        }
    }
    if (!BaseColor || !Roughness || !Metallic ||
        !BaseColor->Constant.Equals(Spec.TintOrBaseColor, 0.000001f) ||
        !FMath::IsNearlyEqual(Roughness->R, Spec.Roughness, 0.000001f) ||
        !FMath::IsNearlyEqual(Metallic->R, Spec.Metallic, 0.000001f))
    {
        OutError = FString::Printf(
            TEXT("Purpose-built material '%s' does not retain exact parameter graph."),
            *Material->GetPathName());
        return false;
    }
    if (Spec.bTranslucent)
    {
        const UMaterialExpressionConstant* Opacity =
            Cast<UMaterialExpressionConstant>(
                UMaterialEditingLibrary::GetMaterialPropertyInputNode(
                    Material, MP_Opacity));
        if (!Opacity ||
            !FMath::IsNearlyEqual(Opacity->R, Spec.Opacity, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("Translucent material '%s' lacks its exact opacity graph."),
                *Material->GetPathName());
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidatePublicViewMaterial(
    UMaterial* Material,
    const FPublicViewMaterialSpec& Spec,
    const TMap<FName, UTexture2D*>& Textures,
    FString& OutError)
{
    const FString ExpectedPath = FString::Printf(
        TEXT("%s/%s.%s"),
        *PublicViewMaterialPath,
        Spec.AssetName,
        Spec.AssetName);
    if (!Material || Material->GetPathName() != ExpectedPath ||
        !Material->GetPathName().StartsWith(PublicViewAssetRoot + TEXT("/")) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode !=
            (Spec.bTranslucent ? BLEND_Translucent : BLEND_Opaque) ||
        (RequiresNaniteMaterialUsage(Spec) &&
         !Material->GetUsageByFlag(MATUSAGE_Nanite)))
    {
        OutError = FString::Printf(
            TEXT("Required isolated material '%s' is absent or has the wrong domain/blend mode."),
            *ExpectedPath);
        return false;
    }
    return Spec.ShaderKind == EPublicViewShaderKind::PbrTextured
        ? ValidatePbrPublicViewMaterial(Material, Spec, Textures, OutError)
        : ValidateConstantPublicViewMaterial(Material, Spec, OutError);
}

UMaterial* CreatePbrPublicViewMaterial(
    const FPublicViewMaterialSpec& Spec,
    const TMap<FName, UTexture2D*>& Textures,
    IAssetTools& AssetTools,
    FString& OutError)
{
    UTexture2D* BaseColor = FindMaterialTexture(
        Spec, EPublicViewTextureUsage::BaseColor, Textures);
    UTexture2D* Normal = FindMaterialTexture(
        Spec, EPublicViewTextureUsage::Normal, Textures);
    UTexture2D* PackedOrm = FindMaterialTexture(
        Spec, EPublicViewTextureUsage::PackedOrm, Textures);
    if (!BaseColor || !Normal || !PackedOrm)
    {
        OutError = FString::Printf(
            TEXT("PBR material '%s' is missing its exact three-map texture set '%s'."),
            Spec.AssetName,
            Spec.TextureSet ? Spec.TextureSet : TEXT("<null>"));
        return nullptr;
    }
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            Spec.AssetName,
            PublicViewMaterialPath,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaPublicViewAssets"))))
        : nullptr;
    if (!Material)
    {
        OutError = FString::Printf(
            TEXT("Could not create isolated PBR material '%s'."),
            Spec.AssetName);
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;

    UMaterialExpressionTextureCoordinate* TextureCoordinate =
        Cast<UMaterialExpressionTextureCoordinate>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material,
                UMaterialExpressionTextureCoordinate::StaticClass(),
                -1200,
                0));
    UMaterialExpressionTextureSampleParameter2D* BaseSample =
        CreatePublicViewTextureParameter(
            Material, BaseColor, TEXT("BaseColorTexture"),
            SAMPLERTYPE_Color, -650, -300);
    UMaterialExpressionTextureSampleParameter2D* NormalSample =
        CreatePublicViewTextureParameter(
            Material, Normal, TEXT("NormalTexture"),
            SAMPLERTYPE_Normal, -350, 0);
    UMaterialExpressionTextureSampleParameter2D* OrmSample =
        CreatePublicViewTextureParameter(
            Material, PackedOrm, TEXT("PackedORMTexture"),
            SAMPLERTYPE_Masks, -350, 300);
    UMaterialExpressionConstant3Vector* Tint =
        Cast<UMaterialExpressionConstant3Vector>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material,
                UMaterialExpressionConstant3Vector::StaticClass(),
                -650,
                -100));
    UMaterialExpressionMultiply* TintMultiply =
        Cast<UMaterialExpressionMultiply>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material,
                UMaterialExpressionMultiply::StaticClass(),
                -250,
                -250));
    if (!TextureCoordinate || !BaseSample || !NormalSample || !OrmSample ||
        !Tint || !TintMultiply)
    {
        OutError = FString::Printf(
            TEXT("Could not allocate exact PBR graph for '%s'."),
            Spec.AssetName);
        return nullptr;
    }
    const float CoordinateScale = 1.0f / Spec.UvMetersPerTile;
    TextureCoordinate->CoordinateIndex = 0;
    TextureCoordinate->UTiling = CoordinateScale;
    TextureCoordinate->VTiling = CoordinateScale;
    Tint->Constant = Spec.TintOrBaseColor;

    if (!ConnectPublicViewExpression(TextureCoordinate, TEXT(""), BaseSample, TEXT("UVs"), TEXT("UV0 to base color"), OutError) ||
        !ConnectPublicViewExpression(TextureCoordinate, TEXT(""), NormalSample, TEXT("UVs"), TEXT("UV0 to normal"), OutError) ||
        !ConnectPublicViewExpression(TextureCoordinate, TEXT(""), OrmSample, TEXT("UVs"), TEXT("UV0 to packed ORM"), OutError) ||
        !ConnectPublicViewExpression(BaseSample, TEXT("RGB"), TintMultiply, TEXT("A"), TEXT("base color to tint multiply"), OutError) ||
        !ConnectPublicViewExpression(Tint, TEXT(""), TintMultiply, TEXT("B"), TEXT("linear tint to base color"), OutError) ||
        !ConnectPublicViewProperty(TintMultiply, TEXT(""), MP_BaseColor, TEXT("tinted Base Color"), OutError) ||
        !ConnectPublicViewProperty(NormalSample, TEXT("RGB"), MP_Normal, TEXT("DirectX Normal RGB"), OutError) ||
        !ConnectPublicViewProperty(OrmSample, TEXT("R"), MP_AmbientOcclusion, TEXT("ORM R Ambient Occlusion"), OutError) ||
        !ConnectPublicViewProperty(OrmSample, TEXT("G"), MP_Roughness, TEXT("ORM G Roughness"), OutError) ||
        !ConnectPublicViewProperty(OrmSample, TEXT("B"), MP_Metallic, TEXT("ORM B Metallic"), OutError))
    {
        return nullptr;
    }
    if (RequiresNaniteMaterialUsage(Spec))
    {
        Material->bUsedWithNanite = true;
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return Material;
}

UMaterial* CreateConstantPublicViewMaterial(
    const FPublicViewMaterialSpec& Spec,
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            Spec.AssetName,
            PublicViewMaterialPath,
            UMaterial::StaticClass(),
            Factory,
            FName(TEXT("TRIAD.ImportIstanaPublicViewAssets"))))
        : nullptr;
    if (!Material)
    {
        OutError = FString::Printf(
            TEXT("Could not create isolated purpose-built material '%s'."),
            Spec.AssetName);
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = Spec.bTranslucent ? BLEND_Translucent : BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    if (Spec.bTranslucent)
    {
        Material->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
    }
    UMaterialExpressionConstant3Vector* BaseColor =
        Cast<UMaterialExpressionConstant3Vector>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material,
                UMaterialExpressionConstant3Vector::StaticClass(),
                -400,
                -150));
    UMaterialExpressionConstant* Roughness =
        Cast<UMaterialExpressionConstant>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material,
                UMaterialExpressionConstant::StaticClass(),
                -400,
                0));
    UMaterialExpressionConstant* Metallic =
        Cast<UMaterialExpressionConstant>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material,
                UMaterialExpressionConstant::StaticClass(),
                -400,
                150));
    if (!BaseColor || !Roughness || !Metallic)
    {
        OutError = FString::Printf(
            TEXT("Could not allocate purpose-built graph for '%s'."),
            Spec.AssetName);
        return nullptr;
    }
    BaseColor->Constant = Spec.TintOrBaseColor;
    Roughness->R = Spec.Roughness;
    Metallic->R = Spec.Metallic;
    if (!ConnectPublicViewProperty(BaseColor, TEXT(""), MP_BaseColor, TEXT("special Base Color"), OutError) ||
        !ConnectPublicViewProperty(Roughness, TEXT(""), MP_Roughness, TEXT("special Roughness"), OutError) ||
        !ConnectPublicViewProperty(Metallic, TEXT(""), MP_Metallic, TEXT("special Metallic"), OutError))
    {
        return nullptr;
    }
    if (Spec.bTranslucent)
    {
        UMaterialExpressionConstant* Opacity =
            Cast<UMaterialExpressionConstant>(
                UMaterialEditingLibrary::CreateMaterialExpression(
                    Material,
                    UMaterialExpressionConstant::StaticClass(),
                    -400,
                    300));
        if (!Opacity)
        {
            OutError = FString::Printf(
                TEXT("Could not allocate opacity graph for '%s'."),
                Spec.AssetName);
            return nullptr;
        }
        Opacity->R = Spec.Opacity;
        if (!ConnectPublicViewProperty(
                Opacity,
                TEXT(""),
                MP_Opacity,
                TEXT("special Opacity"),
                OutError))
        {
            return nullptr;
        }
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return Material;
}

UMaterial* CreatePublicViewMaterial(
    const FPublicViewMaterialSpec& Spec,
    const TMap<FName, UTexture2D*>& Textures,
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterial* Material = Spec.ShaderKind == EPublicViewShaderKind::PbrTextured
        ? CreatePbrPublicViewMaterial(Spec, Textures, AssetTools, OutError)
        : CreateConstantPublicViewMaterial(Spec, AssetTools, OutError);
    if (!Material ||
        !ValidatePublicViewMaterial(Material, Spec, Textures, OutError))
    {
        return nullptr;
    }
    return Material;
}

EPublicViewAssetState InspectPublicViewAssetSet(
    UEditorAssetSubsystem* AssetSubsystem,
    TMap<FString, UStaticMesh*>* OutMeshes,
    TMap<FName, UMaterialInterface*>* OutMaterials,
    FString& OutReport)
{
    if (OutMeshes)
    {
        OutMeshes->Reset();
    }
    if (OutMaterials)
    {
        OutMaterials->Reset();
    }
    if (!AssetSubsystem)
    {
        OutReport = TEXT("Editor Asset Subsystem is unavailable.");
        return EPublicViewAssetState::PartialOrInvalid;
    }

    int32 ExistingCount = 0;
    const int32 ExpectedCount = GetMeshSpecs().Num() +
        GetMaterialSpecs().Num() + GetTextureSpecs().Num();
    for (const FPublicViewMeshSpec& Spec : GetMeshSpecs())
    {
        ExistingCount += DoesObjectOrPackageExist(AssetSubsystem, MeshObjectPath(Spec)) ? 1 : 0;
    }
    for (const FPublicViewMaterialSpec& Spec : GetMaterialSpecs())
    {
        const FString Path = FString::Printf(
            TEXT("%s/%s.%s"),
            *PublicViewMaterialPath,
            Spec.AssetName,
            Spec.AssetName);
        ExistingCount += DoesObjectOrPackageExist(AssetSubsystem, Path) ? 1 : 0;
    }
    for (const FPublicViewTextureSpec& Spec : GetTextureSpecs())
    {
        ExistingCount += DoesObjectOrPackageExist(
            AssetSubsystem,
            TextureObjectPath(Spec)) ? 1 : 0;
    }
    if (ExistingCount == 0)
    {
        OutReport = TEXT("ASSETS_MISSING: isolated /Game/TRIAD/IstanaPublicView asset set is absent.");
        return EPublicViewAssetState::Absent;
    }
    if (ExistingCount != ExpectedCount)
    {
        OutReport = FString::Printf(
            TEXT("PARTIAL_ASSET_SET: found %d/%d isolated mesh/material/texture assets; refusing overwrite, mixed provenance, or constant-placeholder upgrade."),
            ExistingCount,
            ExpectedCount);
        return EPublicViewAssetState::PartialOrInvalid;
    }

    TMap<FName, UTexture2D*> Textures;
    FString Error;
    for (const FPublicViewTextureSpec& Spec : GetTextureSpecs())
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(
            nullptr,
            *TextureObjectPath(Spec));
        if (!ValidatePublicViewTexture(Texture, Spec, Error))
        {
            OutReport = TEXT("INVALID_ASSET_SET: ") + Error;
            return EPublicViewAssetState::PartialOrInvalid;
        }
        Textures.Add(FName(Spec.AssetName), Texture);
    }

    TMap<FName, UMaterialInterface*> Materials;
    for (const FPublicViewMaterialSpec& Spec : GetMaterialSpecs())
    {
        const FString Path = FString::Printf(
            TEXT("%s/%s.%s"),
            *PublicViewMaterialPath,
            Spec.AssetName,
            Spec.AssetName);
        UMaterial* Material = LoadObject<UMaterial>(nullptr, *Path);
        if (!ValidatePublicViewMaterial(Material, Spec, Textures, Error))
        {
            OutReport = TEXT("INVALID_ASSET_SET: ") + Error;
            return EPublicViewAssetState::PartialOrInvalid;
        }
        Materials.Add(FName(Spec.AssetName), Material);
    }

    TMap<FString, UStaticMesh*> Meshes;
    TArray<UStaticMesh*> MeshesToFinish;
    for (const FPublicViewMeshSpec& Spec : GetMeshSpecs())
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshObjectPath(Spec));
        if (!Mesh)
        {
            OutReport = TEXT("INVALID_ASSET_SET: could not load ") + MeshObjectPath(Spec);
            return EPublicViewAssetState::PartialOrInvalid;
        }
        Meshes.Add(Spec.AssetName, Mesh);
        MeshesToFinish.Add(Mesh);
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
    for (const FPublicViewMeshSpec& Spec : GetMeshSpecs())
    {
        UStaticMesh* Mesh = Meshes.FindRef(Spec.AssetName);
        if (!ValidateStaticMesh(Mesh, Spec, true, Error) ||
            Mesh->NaniteSettings.bEnabled != Spec.bBuildNanite)
        {
            if (Error.IsEmpty())
            {
                Error = FString::Printf(
                    TEXT("Mesh '%s' does not retain exact Nanite policy."),
                    *MeshObjectPath(Spec));
            }
            OutReport = TEXT("INVALID_ASSET_SET: ") + Error;
            return EPublicViewAssetState::PartialOrInvalid;
        }
    }
    if (OutMeshes)
    {
        *OutMeshes = MoveTemp(Meshes);
    }
    if (OutMaterials)
    {
        *OutMaterials = MoveTemp(Materials);
    }
    OutReport = FString::Printf(
        TEXT("Validated %d isolated static meshes, %d exact project-owned materials, and %d exact 2048px PBR textures under %s; all 14 mapped visible slots retain UV-scale/tint/normal/ORM graph readback and no constant-placeholder success is accepted."),
        GetMeshSpecs().Num(),
        GetMaterialSpecs().Num(),
        GetTextureSpecs().Num(),
        *PublicViewAssetRoot);
    return EPublicViewAssetState::CompleteValid;
}

bool ImportPublicViewAssetSet(
    UEditorAssetSubsystem* AssetSubsystem,
    FString& OutMessage)
{
    FString SourceReport;
    if (!ValidatePublicViewSourceSet(nullptr, SourceReport))
    {
        OutMessage = SourceReport;
        return false;
    }
    FString ExistingReport;
    const EPublicViewAssetState ExistingState = InspectPublicViewAssetSet(
        AssetSubsystem,
        nullptr,
        nullptr,
        ExistingReport);
    if (ExistingState == EPublicViewAssetState::CompleteValid)
    {
        OutMessage = TEXT("Existing non-overwritten public-view asset set is complete. ") +
            SourceReport + TEXT(" ") + ExistingReport;
        return true;
    }
    if (ExistingState != EPublicViewAssetState::Absent)
    {
        OutMessage = ExistingReport +
            TEXT(" Restart the editor and inspect/remove only the isolated new namespace deliberately before retrying; automation will not delete or overwrite it.");
        return false;
    }

    for (const FPublicViewMeshSpec& Spec : GetMeshSpecs())
    {
        if (DoesObjectOrPackageExist(AssetSubsystem, MeshObjectPath(Spec)))
        {
            OutMessage = TEXT("Refusing to overwrite ") + MeshObjectPath(Spec);
            return false;
        }
    }
    for (const FPublicViewMaterialSpec& Spec : GetMaterialSpecs())
    {
        const FString Path = FString::Printf(
            TEXT("%s/%s.%s"),
            *PublicViewMaterialPath,
            Spec.AssetName,
            Spec.AssetName);
        if (DoesObjectOrPackageExist(AssetSubsystem, Path))
        {
            OutMessage = TEXT("Refusing to overwrite ") + Path;
            return false;
        }
    }
    for (const FPublicViewTextureSpec& Spec : GetTextureSpecs())
    {
        if (DoesObjectOrPackageExist(AssetSubsystem, TextureObjectPath(Spec)))
        {
            OutMessage = TEXT("Refusing to overwrite ") + TextureObjectPath(Spec);
            return false;
        }
    }

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TArray<UObject*> AssetsToSave;
    const FString TextureSourceDirectory =
        FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectDir(),
            TextureSourceRelativeDirectory));
    TArray<UAssetImportTask*> TextureImportTasks;
    TMap<FName, UAssetImportTask*> TextureTaskByName;
    for (const FPublicViewTextureSpec& Spec : GetTextureSpecs())
    {
        UTextureFactory* Factory = NewObject<UTextureFactory>();
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        if (!Factory || !Task)
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_IMPORT: could not allocate deterministic PBR texture import tasks.");
            return false;
        }
        Factory->bCreateMaterial = false;
        Factory->CompressionSettings = GetTextureCompression(Spec.Usage);
        Factory->LODGroup = GetTextureGroup(Spec.Usage);
        Factory->MipGenSettings = TMGS_FromTextureGroup;
        Factory->bFlipNormalMapGreenChannel = false;
        Factory->ColorSpaceMode = IsTextureSrgb(Spec.Usage)
            ? ETextureSourceColorSpace::SRGB
            : ETextureSourceColorSpace::Linear;

        Task->Filename = FPaths::Combine(
            TextureSourceDirectory,
            FString(Spec.AssetName) + TEXT(".png"));
        Task->DestinationPath = PublicViewTexturePath;
        Task->DestinationName = Spec.AssetName;
        Task->bReplaceExisting = false;
        Task->bReplaceExistingSettings = false;
        Task->bAutomated = true;
        Task->bSave = false;
        Task->bAsync = false;
        Task->Factory = Factory;
        Task->Options = Factory;
        TextureImportTasks.Add(Task);
        TextureTaskByName.Add(FName(Spec.AssetName), Task);
    }
    AssetTools.ImportAssetTasks(TextureImportTasks);

    TMap<FName, UTexture2D*> Textures;
    FString Error;
    for (const FPublicViewTextureSpec& Spec : GetTextureSpecs())
    {
        UTexture2D* Texture = nullptr;
        UAssetImportTask* Task = TextureTaskByName.FindRef(FName(Spec.AssetName));
        if (Task)
        {
            for (UObject* ImportedObject : Task->GetObjects())
            {
                Texture = Cast<UTexture2D>(ImportedObject);
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
                TEXT("UNSAVED_PARTIAL_IMPORT: PBR texture import produced no asset for '%s'."),
                Spec.AssetName);
            return false;
        }
        Texture->Modify();
        Texture->SRGB = IsTextureSrgb(Spec.Usage);
        Texture->CompressionSettings = GetTextureCompression(Spec.Usage);
        Texture->LODGroup = GetTextureGroup(Spec.Usage);
        Texture->MipGenSettings = TMGS_FromTextureGroup;
        Texture->AddressX = TA_Wrap;
        Texture->AddressY = TA_Wrap;
        Texture->bFlipGreenChannel = false;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (!ValidatePublicViewTexture(Texture, Spec, Error))
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_IMPORT: ") + Error;
            return false;
        }
        Textures.Add(FName(Spec.AssetName), Texture);
        AssetsToSave.Add(Texture);
    }

    TMap<FName, UMaterialInterface*> Materials;
    for (const FPublicViewMaterialSpec& Spec : GetMaterialSpecs())
    {
        UMaterial* Material = CreatePublicViewMaterial(
            Spec,
            Textures,
            AssetTools,
            Error);
        if (!Material)
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_IMPORT: ") + Error;
            return false;
        }
        Materials.Add(FName(Spec.AssetName), Material);
        AssetsToSave.Add(Material);
    }

    TArray<UAssetImportTask*> ImportTasks;
    TMap<FString, UAssetImportTask*> TasksByAssetName;
    for (const FPublicViewMeshSpec& Spec : GetMeshSpecs())
    {
        FString AdaptedObjFilename;
        if (!PrepareLegacyObjMaterialSlotAdapter(
                Spec,
                AdaptedObjFilename,
                Error))
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_IMPORT: ") + Error;
            return false;
        }
        UFbxImportUI* ImportOptions = NewObject<UFbxImportUI>();
        UFbxFactory* ImportFactory = NewObject<UFbxFactory>();
        UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
        if (!ImportOptions || !ImportFactory || !ImportTask ||
            !ImportOptions->StaticMeshImportData)
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_IMPORT: could not allocate deterministic OBJ import options.");
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
        ImportOptions->StaticMeshImportData->bBuildNanite = Spec.bBuildNanite;
        ImportOptions->StaticMeshImportData->bRemoveDegenerates = true;
        ImportFactory->ImportUI = ImportOptions;
        ImportFactory->SetDetectImportTypeOnImport(false);

        ImportTask->Filename = AdaptedObjFilename;
        ImportTask->DestinationPath = Spec.DestinationPath;
        ImportTask->DestinationName = Spec.AssetName;
        ImportTask->bReplaceExisting = false;
        ImportTask->bReplaceExistingSettings = false;
        ImportTask->bAutomated = true;
        ImportTask->bSave = false;
        ImportTask->bAsync = false;
        ImportTask->Factory = ImportFactory;
        ImportTask->Options = ImportOptions;
        ImportTasks.Add(ImportTask);
        TasksByAssetName.Add(Spec.AssetName, ImportTask);
    }
    AssetTools.ImportAssetTasks(ImportTasks);

    TArray<UStaticMesh*> ImportedMeshes;
    for (const FPublicViewMeshSpec& Spec : GetMeshSpecs())
    {
        UStaticMesh* Mesh = nullptr;
        UAssetImportTask* Task = TasksByAssetName.FindRef(Spec.AssetName);
        if (Task)
        {
            for (UObject* ImportedObject : Task->GetObjects())
            {
                Mesh = Cast<UStaticMesh>(ImportedObject);
                if (Mesh)
                {
                    break;
                }
            }
        }
        if (!Mesh)
        {
            Mesh = LoadObject<UStaticMesh>(nullptr, *MeshObjectPath(Spec));
        }
        if (!Mesh)
        {
            OutMessage = FString::Printf(
                TEXT("UNSAVED_PARTIAL_IMPORT: OBJ import produced no mesh for '%s'."),
                Spec.AssetName);
            return false;
        }
        ImportedMeshes.Add(Mesh);
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(ImportedMeshes);

    for (int32 MeshIndex = 0; MeshIndex < GetMeshSpecs().Num(); ++MeshIndex)
    {
        const FPublicViewMeshSpec& Spec = GetMeshSpecs()[MeshIndex];
        UStaticMesh* Mesh = ImportedMeshes[MeshIndex];
        Mesh->Modify();
        const FString OriginalSourceFilename = MeshSourceFilename(Spec);
        UAssetImportData* AssetImportData = Mesh->GetAssetImportData();
        if (!AssetImportData)
        {
            OutMessage = FString::Printf(
                TEXT("UNSAVED_PARTIAL_IMPORT: mesh '%s' has no import provenance data to restore to the frozen OBJ."),
                Spec.AssetName);
            return false;
        }
        AssetImportData->Update(OriginalSourceFilename);
        if (Mesh->GetNumSourceModels() < 1)
        {
            OutMessage = FString::Printf(
                TEXT("UNSAVED_PARTIAL_IMPORT: mesh '%s' has no source LOD0 for the frozen normal/tangent contract."),
                Spec.AssetName);
            return false;
        }
        FMeshBuildSettings& BuildSettings =
            Mesh->GetSourceModel(0).BuildSettings;
        BuildSettings.bRecomputeNormals = false;
        BuildSettings.bRecomputeTangents = true;
        BuildSettings.bUseMikkTSpace = true;
        BuildSettings.bRemoveDegenerates = true;
        BuildSettings.bUseFullPrecisionUVs = true;
        BuildSettings.BuildScale3D = FVector(1.0, 1.0, 1.0);
        Mesh->NaniteSettings.bEnabled = Spec.bBuildNanite;
        if (Spec.bUseComplexCollision)
        {
            Mesh->CreateBodySetup();
            UBodySetup* BodySetup = Mesh->GetBodySetup();
            if (!BodySetup)
            {
                OutMessage = FString::Printf(
                    TEXT("UNSAVED_PARTIAL_IMPORT: could not create collision body for '%s'."),
                    Spec.AssetName);
                return false;
            }
            BodySetup->Modify();
            BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
            BodySetup->InvalidatePhysicsData();
            BodySetup->CreatePhysicsMeshes();
        }
        for (const FName& SlotName : Spec.RequiredMaterialSlots)
        {
            const int32 MaterialIndex = Mesh->GetMaterialIndex(SlotName);
            UMaterialInterface* Material = Materials.FindRef(SlotName);
            if (MaterialIndex == INDEX_NONE || !Material)
            {
                OutMessage = FString::Printf(
                    TEXT("UNSAVED_PARTIAL_IMPORT: mesh '%s' lacks exact slot/material '%s'."),
                    Spec.AssetName,
                    *SlotName.ToString());
                return false;
            }
            Mesh->SetMaterial(MaterialIndex, Material);
        }
        Mesh->PostEditChange();
        Mesh->MarkPackageDirty();
        TArray<UStaticMesh*> MeshToFinish;
        MeshToFinish.Add(Mesh);
        FStaticMeshCompilingManager::Get().FinishCompilation(MeshToFinish);
        if (!ValidateStaticMesh(Mesh, Spec, true, Error) ||
            Mesh->NaniteSettings.bEnabled != Spec.bBuildNanite)
        {
            OutMessage = TEXT("UNSAVED_PARTIAL_IMPORT: ") + Error;
            return false;
        }
        AssetsToSave.Add(Mesh);
    }

    if (!AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        OutMessage = TEXT("SAVE_FAILED_PARTIAL_ISOLATED_SET: all assets validated in memory, but one or more new isolated packages did not save. Inspect the new namespace before retrying; no legacy package was touched.");
        return false;
    }
    FString FinalReport;
    if (InspectPublicViewAssetSet(
            AssetSubsystem,
            nullptr,
            nullptr,
            FinalReport) != EPublicViewAssetState::CompleteValid)
    {
        OutMessage = TEXT("POST_SAVE_VALIDATION_FAILED: ") + FinalReport;
        return false;
    }
    OutMessage = SourceReport + TEXT(" ") + FinalReport +
        TEXT(" MATERIAL_ACCEPTANCE_BLOCKED_PENDING_PHOTO_QA: the exact original AI-assisted PBR pack is integrated with deterministic graph readback, but it is not a measured scan and does not claim photoreal acceptance. No existing Istana/DigitalTwin package was loaded into Unreal, overwritten, or saved.");
    return true;
}

struct FPublicViewCameraSpec
{
    const TCHAR* ActorName;
    const TCHAR* ActorLabel;
    FName RequiredTag;
    FVector Location;
    FVector LookAt;
    float FieldOfView;
    bool bPrimary;
};

const TArray<FPublicViewCameraSpec>& GetCameraSpecs()
{
    static const TArray<FPublicViewCameraSpec> Specs = {
        {
            TEXT("TRIAD_IPV_Camera_CeremonialFront"),
            TEXT("Istana Public View - Ceremonial Front"),
            TEXT("TRIADIstanaPublicViewCamera_Primary"),
            FVector(0.0, 21000.0, 2400.0),
            FVector(0.0, 0.0, 1300.0),
            52.0f,
            true
        },
        {
            TEXT("TRIAD_IPV_Camera_FrontOblique"),
            TEXT("Istana Public View - Front Oblique"),
            TEXT("TRIADIstanaPublicViewCamera_FrontOblique"),
            FVector(16000.0, 16000.0, 1700.0),
            FVector(0.0, 0.0, 1200.0),
            48.0f,
            false
        },
        {
            TEXT("TRIAD_IPV_Camera_Arcade"),
            TEXT("Istana Public View - Public Arcade"),
            TEXT("TRIADIstanaPublicViewCamera_Arcade"),
            FVector(-4500.0, 6500.0, 950.0),
            FVector(0.0, 0.0, 900.0),
            60.0f,
            false
        },
        {
            TEXT("TRIAD_IPV_Camera_Tower"),
            TEXT("Istana Public View - Tower Long Lens"),
            TEXT("TRIADIstanaPublicViewCamera_Tower"),
            FVector(0.0, 30000.0, 1800.0),
            FVector(0.0, 0.0, 2200.0),
            24.0f,
            false
        }};
    return Specs;
}

template <typename TActor>
TActor* SpawnPublicViewActor(
    UWorld* World,
    const FName& Name,
    const TCHAR* Label,
    const FName& Folder)
{
    if (!World || !World->PersistentLevel)
    {
        return nullptr;
    }
    FActorSpawnParameters Parameters;
    Parameters.Name = Name;
    Parameters.OverrideLevel = World->PersistentLevel;
    Parameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TActor* Actor = World->SpawnActor<TActor>(
        TActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        Parameters);
#if WITH_EDITOR
    if (Actor)
    {
        Actor->SetActorLabel(Label, false);
        Actor->SetFolderPath(Folder);
    }
#endif
    return Actor;
}

bool SetCameraAutoActivateForPlayer(
    ACameraActor* Camera,
    EAutoReceiveInput::Type Player,
    FString& OutError)
{
    FByteProperty* Property = Camera
        ? FindFProperty<FByteProperty>(
            ACameraActor::StaticClass(),
            TEXT("AutoActivateForPlayer"))
        : nullptr;
    if (!Camera || !Property)
    {
        OutError = TEXT("UE 5.5 camera auto-activation property is unavailable.");
        return false;
    }
    Property->SetPropertyValue_InContainer(Camera, static_cast<uint8>(Player));
    OutError.Reset();
    return true;
}

bool HasExpectedCameraAutoActivation(
    const ACameraActor* Camera,
    EAutoReceiveInput::Type Expected)
{
    FByteProperty* Property = Camera
        ? FindFProperty<FByteProperty>(
            ACameraActor::StaticClass(),
            TEXT("AutoActivateForPlayer"))
        : nullptr;
    return Property &&
        Property->GetPropertyValue_InContainer(Camera) == static_cast<uint8>(Expected);
}

void ConfigureNeutralCameraProfile(
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
    FPostProcessSettings& Settings = CameraComponent->PostProcessSettings;
    Settings.bOverride_AutoExposureMethod = true;
    Settings.AutoExposureMethod = AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Settings.AutoExposureApplyPhysicalCameraExposure = true;
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = 0.0f;
    Settings.bOverride_CameraShutterSpeed = true;
    Settings.CameraShutterSpeed = 125.0f;
    Settings.bOverride_CameraISO = true;
    Settings.CameraISO = 100.0f;
    Settings.bOverride_DepthOfFieldFstop = true;
    Settings.DepthOfFieldFstop = 8.0f;
    Settings.bOverride_DepthOfFieldScale = true;
    Settings.DepthOfFieldScale = 0.0f;
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
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = 0.0f;
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = 0.0f;
    Settings.bOverride_MotionBlurAmount = true;
    Settings.MotionBlurAmount = 0.0f;
    Settings.bOverride_SceneFringeIntensity = true;
    Settings.SceneFringeIntensity = 0.0f;
}

bool HasNeutralCameraProfile(
    const UCameraComponent* CameraComponent,
    float ExpectedFieldOfView)
{
    if (!CameraComponent)
    {
        return false;
    }
    const FPostProcessSettings& Settings = CameraComponent->PostProcessSettings;
    return FMath::IsNearlyEqual(CameraComponent->FieldOfView, ExpectedFieldOfView) &&
        CameraComponent->bConstrainAspectRatio &&
        FMath::IsNearlyEqual(CameraComponent->AspectRatio, 16.0f / 9.0f) &&
        FMath::IsNearlyEqual(CameraComponent->PostProcessBlendWeight, 1.0f) &&
        Settings.bOverride_AutoExposureMethod &&
        Settings.AutoExposureMethod == AEM_Manual &&
        Settings.bOverride_AutoExposureApplyPhysicalCameraExposure &&
        Settings.AutoExposureApplyPhysicalCameraExposure &&
        Settings.bOverride_AutoExposureBias &&
        FMath::IsNearlyZero(Settings.AutoExposureBias) &&
        Settings.bOverride_CameraShutterSpeed &&
        FMath::IsNearlyEqual(Settings.CameraShutterSpeed, 125.0f) &&
        Settings.bOverride_CameraISO &&
        FMath::IsNearlyEqual(Settings.CameraISO, 100.0f) &&
        Settings.bOverride_DepthOfFieldFstop &&
        FMath::IsNearlyEqual(Settings.DepthOfFieldFstop, 8.0f) &&
        Settings.bOverride_DepthOfFieldScale &&
        FMath::IsNearlyZero(Settings.DepthOfFieldScale) &&
        Settings.bOverride_WhiteTemp &&
        FMath::IsNearlyEqual(Settings.WhiteTemp, 6500.0f) &&
        Settings.bOverride_WhiteTint && FMath::IsNearlyZero(Settings.WhiteTint) &&
        Settings.bOverride_LocalExposureHighlightContrastScale &&
        FMath::IsNearlyEqual(Settings.LocalExposureHighlightContrastScale, 1.0f) &&
        Settings.bOverride_LocalExposureShadowContrastScale &&
        FMath::IsNearlyEqual(Settings.LocalExposureShadowContrastScale, 1.0f) &&
        Settings.bOverride_LocalExposureDetailStrength &&
        FMath::IsNearlyEqual(Settings.LocalExposureDetailStrength, 1.0f) &&
        Settings.bOverride_LocalExposureMiddleGreyBias &&
        FMath::IsNearlyZero(Settings.LocalExposureMiddleGreyBias) &&
        Settings.bOverride_LocalExposureHighlightContrastCurve &&
        Settings.LocalExposureHighlightContrastCurve == nullptr &&
        Settings.bOverride_LocalExposureShadowContrastCurve &&
        Settings.LocalExposureShadowContrastCurve == nullptr &&
        Settings.bOverride_BloomIntensity && FMath::IsNearlyZero(Settings.BloomIntensity) &&
        Settings.bOverride_VignetteIntensity && FMath::IsNearlyZero(Settings.VignetteIntensity) &&
        Settings.bOverride_MotionBlurAmount && FMath::IsNearlyZero(Settings.MotionBlurAmount) &&
        Settings.bOverride_SceneFringeIntensity && FMath::IsNearlyZero(Settings.SceneFringeIntensity);
}

ACameraActor* SpawnFixedCamera(
    UWorld* World,
    const FPublicViewCameraSpec& Spec,
    FString& OutError)
{
    ACameraActor* Camera = SpawnPublicViewActor<ACameraActor>(
        World,
        FName(Spec.ActorName),
        Spec.ActorLabel,
        TEXT("TRIAD/Istana Public View/Cameras"));
    if (!Camera)
    {
        OutError = FString::Printf(TEXT("Could not spawn fixed camera '%s'."), Spec.ActorName);
        return nullptr;
    }
    Camera->Modify();
    Camera->Tags.AddUnique(Spec.RequiredTag);
    Camera->SetActorLocation(Spec.Location);
    Camera->SetActorRotation((Spec.LookAt - Spec.Location).Rotation());
    ConfigureNeutralCameraProfile(Camera->GetCameraComponent(), Spec.FieldOfView);
    if (!SetCameraAutoActivateForPlayer(
            Camera,
            Spec.bPrimary ? EAutoReceiveInput::Player0 : EAutoReceiveInput::Disabled,
            OutError))
    {
        return nullptr;
    }
    return Camera;
}

bool SetPersistentPublicViewAirSimGameMode(
    UWorld* World,
    FString& OutError)
{
    if (!World || !World->PersistentLevel)
    {
        OutError = TEXT("Public-view world or persistent level is unavailable.");
        return false;
    }

    UClass* ExternalAirSimGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *ExternalAirSimGameModeClassPath);
    UClass* RequiredGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *IstanaAirSimGameModeClassPath);
    AWorldSettings* WorldSettings = World->GetWorldSettings();
    FClassProperty* DefaultGameModeProperty = FindFProperty<FClassProperty>(
        AWorldSettings::StaticClass(),
        GET_MEMBER_NAME_CHECKED(AWorldSettings, DefaultGameMode));
    if (!ExternalAirSimGameModeClass ||
        !RequiredGameModeClass ||
        RequiredGameModeClass->GetPathName() != IstanaAirSimGameModeClassPath ||
        !RequiredGameModeClass->IsChildOf(ExternalAirSimGameModeClass) ||
        !WorldSettings ||
        !DefaultGameModeProperty)
    {
        OutError = FString::Printf(
            TEXT("Required TRIAD AirSim GameMode '%s', its base '%s', or the UE 5.5 World Settings override is unavailable."),
            *IstanaAirSimGameModeClassPath,
            *ExternalAirSimGameModeClassPath);
        return false;
    }

    WorldSettings->Modify();
    WorldSettings->PreEditChange(DefaultGameModeProperty);
    DefaultGameModeProperty->SetObjectPropertyValue_InContainer(
        WorldSettings,
        RequiredGameModeClass);
    FPropertyChangedEvent ChangedEvent(
        DefaultGameModeProperty,
        EPropertyChangeType::ValueSet);
    WorldSettings->PostEditChangeProperty(ChangedEvent);
    WorldSettings->MarkPackageDirty();
    World->PersistentLevel->MarkPackageDirty();
    World->MarkPackageDirty();

    const UClass* PersistedClass = WorldSettings->DefaultGameMode.Get();
    if (!PersistedClass ||
        PersistedClass->GetPathName() != IstanaAirSimGameModeClassPath ||
        !World->GetOutermost()->IsDirty())
    {
        OutError = TEXT("World Settings rejected or failed to dirty the exact TRIAD AirSim GameMode override.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ConfigurePublicViewLighting(UWorld* World, FString& OutError)
{
    ADirectionalLight* Sun = SpawnPublicViewActor<ADirectionalLight>(
        World,
        TEXT("TRIAD_IPV_DirectionalSun"),
        TEXT("Istana Public View - Clear Day Sun"),
        TEXT("TRIAD/Istana Public View/Lighting"));
    ASkyLight* Sky = SpawnPublicViewActor<ASkyLight>(
        World,
        TEXT("TRIAD_IPV_SkyLight"),
        TEXT("Istana Public View - Realtime Skylight"),
        TEXT("TRIAD/Istana Public View/Lighting"));
    ASkyAtmosphere* Atmosphere = SpawnPublicViewActor<ASkyAtmosphere>(
        World,
        TEXT("TRIAD_IPV_SkyAtmosphere"),
        TEXT("Istana Public View - Sky Atmosphere"),
        TEXT("TRIAD/Istana Public View/Lighting"));
    UDirectionalLightComponent* SunComponent = Sun ? Sun->GetComponent() : nullptr;
    USkyLightComponent* SkyComponent = Sky ? Sky->GetLightComponent() : nullptr;
    if (!Sun || !Sky || !Atmosphere || !SunComponent || !SkyComponent)
    {
        OutError = TEXT("Could not create complete deterministic clear-day lighting.");
        return false;
    }
    Sun->Modify();
    Sun->Tags.AddUnique(LightingTag);
    Sun->SetActorRotation(FRotator(-32.0, -145.0, 0.0));
    SunComponent->SetMobility(EComponentMobility::Movable);
    SunComponent->SetIntensity(80000.0f);
    SunComponent->SetLightColor(FLinearColor(1.0f, 0.94f, 0.84f), false);
    SunComponent->SetAtmosphereSunLight(true);
    SunComponent->SetAtmosphereSunLightIndex(0);

    Sky->Modify();
    Sky->Tags.AddUnique(LightingTag);
    SkyComponent->SetMobility(EComponentMobility::Movable);
    SkyComponent->SetRealTimeCapture(true);
    SkyComponent->SetIntensity(1.0f);

    Atmosphere->Modify();
    Atmosphere->Tags.AddUnique(LightingTag);
    OutError.Reset();
    return true;
}

bool ValidatePublicViewWorld(
    UWorld* World,
    bool bRequireExactDestinationPackage,
    FString& OutReport)
{
    TArray<FString> Errors;
    if (!World || !World->PersistentLevel)
    {
        OutReport = TEXT("Public-view world or persistent level is unavailable.");
        return false;
    }
    if (bRequireExactDestinationPackage &&
        World->GetOutermost()->GetName() != DestinationMapPackage)
    {
        Errors.Add(FString::Printf(
            TEXT("Loaded package is '%s', expected exact destination '%s'."),
            *World->GetOutermost()->GetName(),
            *DestinationMapPackage));
    }

    UClass* ExternalAirSimGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *ExternalAirSimGameModeClassPath);
    UClass* RequiredGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *IstanaAirSimGameModeClassPath);
    const AWorldSettings* WorldSettings = World->GetWorldSettings();
    const UClass* ConfiguredGameMode = WorldSettings
        ? WorldSettings->DefaultGameMode.Get()
        : nullptr;
    if (!ExternalAirSimGameModeClass ||
        !RequiredGameModeClass ||
        RequiredGameModeClass->GetPathName() != IstanaAirSimGameModeClassPath ||
        !RequiredGameModeClass->IsChildOf(ExternalAirSimGameModeClass) ||
        !ConfiguredGameMode ||
        ConfiguredGameMode->GetPathName() != IstanaAirSimGameModeClassPath)
    {
        Errors.Add(FString::Printf(
            TEXT("World Settings must persist exact TRIAD AirSim GameMode '%s' with expected AirSim lineage."),
            *IstanaAirSimGameModeClassPath));
    }

    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy = nullptr;
    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    int32 FogActorCount = 0;
    int32 MaterialBillboardCount = 0;
    int32 DirectionalLightCount = 0;
    int32 SkyLightCount = 0;
    int32 AtmosphereCount = 0;
    int32 TaggedLightingCount = 0;
    ADirectionalLight* TaggedSun = nullptr;
    ASkyLight* TaggedSky = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor) || Actor->GetWorld() != World)
        {
            continue;
        }
        if (Actor->ActorHasTag(SceneActorTag))
        {
            Scene = Cast<ATRIADIstanaPublicViewSceneActor>(Actor);
            ++SceneCount;
        }
        if (Actor->ActorHasTag(RuntimePolicyTag))
        {
            Policy = Cast<ATRIADIstanaPublicViewRuntimePolicyActor>(Actor);
            ++PolicyCount;
        }
        if (Cast<AExponentialHeightFog>(Actor))
        {
            ++FogActorCount;
        }
        DirectionalLightCount += Cast<ADirectionalLight>(Actor) ? 1 : 0;
        SkyLightCount += Cast<ASkyLight>(Actor) ? 1 : 0;
        AtmosphereCount += Cast<ASkyAtmosphere>(Actor) ? 1 : 0;
        if (Actor->ActorHasTag(LightingTag))
        {
            ++TaggedLightingCount;
            if (ADirectionalLight* Sun = Cast<ADirectionalLight>(Actor))
            {
                TaggedSun = Sun;
            }
            if (ASkyLight* Sky = Cast<ASkyLight>(Actor))
            {
                TaggedSky = Sky;
            }
        }
        TArray<UMaterialBillboardComponent*> MaterialBillboards;
        Actor->GetComponents<UMaterialBillboardComponent>(MaterialBillboards);
        MaterialBillboardCount += MaterialBillboards.Num();
    }
    if (SceneCount != 1 || !Scene)
    {
        Errors.Add(FString::Printf(TEXT("Expected exactly one tagged public-view scene actor; found %d."), SceneCount));
    }
    else
    {
        FString SceneReport;
        if (!Scene->ValidatePublicViewScene(SceneReport))
        {
            Errors.Add(SceneReport);
        }
        if (!Scene->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
            Scene->ClaimLabel != PublicViewClaimLabel ||
            Scene->ReferenceEpoch != TEXT("2024-04") ||
            Scene->bSurveyControlled ||
            Scene->bArbitraryViewIndistinguishabilityClaimed ||
            Scene->bPhotorealMaterialAcceptanceClaimed ||
            !Scene->bPbrTexturePackIntegrated ||
            Scene->MaterialImplementationTier != PublicViewMaterialTier ||
            Scene->DisplayColorPolicy != PublicViewDisplayPolicy ||
            Scene->bExternalOcioDisplayValidated ||
            Scene->PhotoQaEvidenceStatus !=
                TEXT("NOT_READY_UNCALIBRATED_COMMONS_EVIDENCE_METADATA_ONLY") ||
            Scene->bPhotoQaEvidenceConsumedByMap ||
            Scene->bContainsWholeTreeBillboards ||
            Scene->SensorOcclusionPolicy !=
                TEXT("SYNTHETIC_CONTEXT_AND_VEGETATION_VISUAL_ONLY_NO_COLLISION") ||
            Scene->TerrainBoundaryPolicy !=
                TEXT("SYNTHETIC_DOWNWARD_VISUAL_ONLY_SKIRT_NO_COLLISION") ||
            Scene->OSMContextIntegrationStatus != OsmContextSceneStatus ||
            Scene->OSMContextAttribution !=
                TEXT("© OpenStreetMap contributors; ODbL-1.0") ||
            Scene->bOSMContextUsedForSensorTruth ||
            Scene->bSyntheticContextBuildingsEnabled ||
            Scene->MinimumRequiredTreeInstances != RequiredTreeInstanceCount ||
            Scene->MinimumRequiredHeroTreeInstances != RequiredHeroTreeInstanceCount ||
            !FMath::IsNearlyEqual(Scene->HeroTreeRadiusMeters, 250.0f) ||
            !FMath::IsNearlyEqual(Scene->ContextRadiusMeters, 1000.0f))
        {
            Errors.Add(TEXT("Scene transform, claim boundary, or volumetric-context thresholds changed."));
        }
        struct FExpectedMeshBinding
        {
            const UStaticMeshComponent* Component;
            const TCHAR* ObjectPath;
        };
        const FExpectedMeshBinding StaticBindings[] = {
            {Scene->BuildingHeroVisualComponent.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Building/SM_IstanaPublicView_Building_Hero.SM_IstanaPublicView_Building_Hero")},
            {Scene->BuildingCollisionComponent.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Building/SM_IstanaPublicView_Building_Collision.SM_IstanaPublicView_Building_Collision")},
            {Scene->TerrainComponent.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Terrain.SM_IstanaPublicView_Terrain")},
            {Scene->TerrainSkirtComponent.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_TerrainSkirt.SM_IstanaPublicView_TerrainSkirt")},
            {Scene->HardscapeComponent.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Hardscape.SM_IstanaPublicView_Hardscape")},
            {Scene->ContextBuildingsComponent.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Context/SM_IstanaPublicView_ContextBuildings.SM_IstanaPublicView_ContextBuildings")},
            {Scene->OSMContextBuildingsComponent.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Context/SM_IstanaPublicView_OSMContextBuildings.SM_IstanaPublicView_OSMContextBuildings")}};
        for (const FExpectedMeshBinding& Binding : StaticBindings)
        {
            const UStaticMesh* Mesh = Binding.Component
                ? Binding.Component->GetStaticMesh()
                : nullptr;
            if (!Mesh || Mesh->GetPathName() != Binding.ObjectPath)
            {
                Errors.Add(FString::Printf(TEXT("Scene mesh binding changed: expected '%s'."), Binding.ObjectPath));
            }
        }
        const FExpectedMeshBinding TreeBindings[] = {
            {Scene->RainTreeInstances.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Vegetation/SM_IstanaPublicView_Tree_Rain.SM_IstanaPublicView_Tree_Rain")},
            {Scene->PalmTreeInstances.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Vegetation/SM_IstanaPublicView_Tree_Palm.SM_IstanaPublicView_Tree_Palm")},
            {Scene->FramingTreeInstances.Get(), TEXT("/Game/TRIAD/IstanaPublicView/Vegetation/SM_IstanaPublicView_Tree_Framing.SM_IstanaPublicView_Tree_Framing")}};
        for (const FExpectedMeshBinding& Binding : TreeBindings)
        {
            const UStaticMesh* Mesh = Binding.Component
                ? Binding.Component->GetStaticMesh()
                : nullptr;
            if (!Mesh || Mesh->GetPathName() != Binding.ObjectPath)
            {
                Errors.Add(FString::Printf(TEXT("Volumetric tree binding changed: expected '%s'."), Binding.ObjectPath));
            }
        }
    }
    if (PolicyCount != 1 || !Policy ||
        (Policy && (Policy->ClaimLabel != PublicViewClaimLabel ||
                    Policy->ReferenceEpoch != TEXT("2024-04") ||
                    Policy->DisplayColorPolicy != PublicViewDisplayPolicy ||
                    Policy->bExternalOcioDisplayValidated ||
                    !Policy->bSuppressAirSimVisualWeather ||
                    !Policy->bSuppressExponentialHeightFog ||
                    !Policy->bRequireIstanaAirSimGameMode ||
                    Policy->RequiredGameModeClassPath != IstanaAirSimGameModeClassPath ||
                    !Policy->bEnforceFixedPrimaryCamera ||
                    Policy->RequiredPrimaryCameraTag != PrimaryCameraTag ||
                    Policy->MaximumEnforcementAttempts < 1 ||
                    Policy->MaximumEnforcementAttempts > 12 ||
                    !FMath::IsFinite(Policy->EnforcementRetrySeconds) ||
                    Policy->EnforcementRetrySeconds < 0.1f ||
                    Policy->EnforcementRetrySeconds > 5.0f)))
    {
        Errors.Add(FString::Printf(TEXT("Expected one exact clear-weather/fixed-camera runtime policy; found %d."), PolicyCount));
    }
    if (FogActorCount != 0)
    {
        Errors.Add(FString::Printf(TEXT("Authored public-view map must contain zero exponential-height-fog actors; found %d."), FogActorCount));
    }
    if (MaterialBillboardCount != 0)
    {
        Errors.Add(FString::Printf(TEXT("Public-view map contains %d forbidden material-billboard component(s)."), MaterialBillboardCount));
    }

    int32 CameraActorCount = 0;
    for (TActorIterator<ACameraActor> It(World); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == World)
        {
            ++CameraActorCount;
        }
    }
    if (CameraActorCount != GetCameraSpecs().Num())
    {
        Errors.Add(FString::Printf(
            TEXT("Expected exactly %d fixed cameras; found %d."),
            GetCameraSpecs().Num(),
            CameraActorCount));
    }
    for (const FPublicViewCameraSpec& Spec : GetCameraSpecs())
    {
        ACameraActor* MatchingCamera = nullptr;
        int32 MatchingCount = 0;
        for (TActorIterator<ACameraActor> It(World); It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == World &&
                It->ActorHasTag(Spec.RequiredTag))
            {
                MatchingCamera = *It;
                ++MatchingCount;
            }
        }
        const FVector ExpectedDirection = (Spec.LookAt - Spec.Location).GetSafeNormal();
        if (MatchingCount != 1 || !MatchingCamera ||
            !MatchingCamera->GetActorLocation().Equals(Spec.Location, 0.1) ||
            FVector::DotProduct(
                MatchingCamera->GetActorForwardVector(),
                ExpectedDirection) < 0.99999 ||
            !HasNeutralCameraProfile(
                MatchingCamera->GetCameraComponent(),
                Spec.FieldOfView) ||
            !HasExpectedCameraAutoActivation(
                MatchingCamera,
                Spec.bPrimary ? EAutoReceiveInput::Player0 : EAutoReceiveInput::Disabled))
        {
            Errors.Add(FString::Printf(TEXT("Fixed camera contract failed for tag '%s'."), *Spec.RequiredTag.ToString()));
        }
    }
    if (TaggedLightingCount != 3 || DirectionalLightCount != 1 ||
        SkyLightCount != 1 || AtmosphereCount != 1)
    {
        Errors.Add(FString::Printf(
            TEXT("Clear-day lighting requires one sun, one skylight, and one sky atmosphere; tagged counts are %d/%d/%d of %d."),
            DirectionalLightCount,
            SkyLightCount,
            AtmosphereCount,
            TaggedLightingCount));
    }
    const UDirectionalLightComponent* TaggedSunComponent = TaggedSun
        ? TaggedSun->GetComponent()
        : nullptr;
    const USkyLightComponent* TaggedSkyComponent = TaggedSky
        ? TaggedSky->GetLightComponent()
        : nullptr;
    if (!TaggedSunComponent || !TaggedSkyComponent ||
        TaggedSunComponent->Mobility != EComponentMobility::Movable ||
        !FMath::IsNearlyEqual(TaggedSunComponent->Intensity, 80000.0f, 0.1f) ||
        !TaggedSunComponent->IsUsedAsAtmosphereSunLight() ||
        TaggedSunComponent->GetAtmosphereSunLightIndex() != 0 ||
        !TaggedSun->GetActorRotation().Equals(
            FRotator(-32.0, -145.0, 0.0),
            0.01) ||
        TaggedSkyComponent->Mobility != EComponentMobility::Movable ||
        !TaggedSkyComponent->bRealTimeCapture ||
        !FMath::IsNearlyEqual(TaggedSkyComponent->Intensity, 1.0f))
    {
        Errors.Add(TEXT("Tagged clear-day light components do not retain exact movable/intensity/atmosphere/realtime-capture settings."));
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    FString AssetReport;
    if (InspectPublicViewAssetSet(
            AssetSubsystem,
            nullptr,
            nullptr,
            AssetReport) != EPublicViewAssetState::CompleteValid)
    {
        Errors.Add(AssetReport);
    }
    if (Errors.Num() > 0)
    {
        OutReport = TEXT("Istana public-view map validation failed:\n - ") +
            FString::Join(Errors, TEXT("\n - "));
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Validated isolated PBR public-view map package '%s': exact TRIAD AirSim GameMode, ten exact meshes including a non-colliding terrain-edge skirt and coordinate-aligned ODbL mapping-grade outer context, %d project-owned materials, %d exact 2048px PBR textures, %d volumetric trees minimum, hidden synthetic context fallback, non-colliding/no-sensor-truth visual context, four fixed 16:9 neutral physical-exposure/local-exposure-disabled cameras, clear-day lighting, no fog/material billboards, and claim=%s. OSM_CONTEXT_NOT_VISUALLY_OR_PHOTO_ACCEPTED; attribution © OpenStreetMap contributors (ODbL-1.0). MATERIAL_ACCEPTANCE_BLOCKED_PENDING_PHOTO_QA: PBR integration is complete, but the texture pack is not a measured scan and external sRGB/Rec.709 display/OCIO calibration has not been validated."),
        *World->GetOutermost()->GetName(),
        GetMaterialSpecs().Num(),
        GetTextureSpecs().Num(),
        RequiredTreeInstanceCount,
        *PublicViewClaimLabel);
    return true;
}

bool ReloadAndValidatePublicViewMap(FString& OutReport)
{
    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(DestinationMapPackage, &DestinationFilename))
    {
        OutReport = FString::Printf(
            TEXT("Saved destination '%s' cannot be resolved for persistence validation."),
            *DestinationMapPackage);
        return false;
    }
    UWorld* ReloadedWorld = UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (!ReloadedWorld ||
        ReloadedWorld->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutReport = FString::Printf(TEXT("Could not reload exact destination '%s'."), *DestinationMapPackage);
        return false;
    }
    if (!ValidatePublicViewWorld(ReloadedWorld, true, OutReport))
    {
        return false;
    }
    TArray<UPackage*> DirtyMaps;
    TArray<UPackage*> DirtyContent;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMaps);
    UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContent);
    if (DirtyMaps.Num() > 0 || DirtyContent.Num() > 0)
    {
        OutReport = TEXT("Reloaded destination validated structurally but left dirty packages.");
        return false;
    }
    return true;
}
}

bool UTRIADIstanaPublicViewEditorLibrary::ValidateIstanaPublicViewRemoteControlProject(
    const FString& ExpectedProjectPath,
    FString& OutReport)
{
    if (ExpectedProjectPath.IsEmpty())
    {
        OutReport = TEXT("ExpectedProjectPath is required.");
        return false;
    }
    FString ExpectedDirectory = FPaths::ConvertRelativePathToFull(ExpectedProjectPath);
    FPaths::NormalizeDirectoryName(ExpectedDirectory);
    if (ExpectedDirectory.EndsWith(TEXT(".uproject"), ESearchCase::IgnoreCase))
    {
        ExpectedDirectory = FPaths::GetPath(ExpectedDirectory);
        FPaths::NormalizeDirectoryName(ExpectedDirectory);
    }
    FString ActualDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ActualDirectory);
    if (!FPaths::IsSamePath(ExpectedDirectory, ActualDirectory))
    {
        OutReport = FString::Printf(
            TEXT("Remote Control project mismatch: connected editor project is '%s', expected '%s'. No public-view operation ran."),
            *ActualDirectory,
            *ExpectedDirectory);
        return false;
    }
    OutReport = FString::Printf(
        TEXT("Remote Control project identity verified for isolated public-view automation: '%s'."),
        *ActualDirectory);
    return true;
}

bool UTRIADIstanaPublicViewEditorLibrary::ImportIstanaPublicViewAssets(
    FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsPublicViewEditorOperationSafe(OutMessage))
    {
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("Editor Asset Subsystem is unavailable.");
        return false;
    }
    return ImportPublicViewAssetSet(AssetSubsystem, OutMessage);
}

bool UTRIADIstanaPublicViewEditorLibrary::ValidateIstanaPublicViewAssets(
    FString& OutReport)
{
    FString SourceReport;
    if (!ValidatePublicViewSourceSet(nullptr, SourceReport))
    {
        OutReport = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    FString AssetReport;
    if (InspectPublicViewAssetSet(
            AssetSubsystem,
            nullptr,
            nullptr,
            AssetReport) != EPublicViewAssetState::CompleteValid)
    {
        OutReport = AssetReport;
        return false;
    }
    OutReport = SourceReport + TEXT(" ") + AssetReport;
    return true;
}

bool UTRIADIstanaPublicViewEditorLibrary::BuildIstanaPublicViewExteriorMap(
    FString& OutMessage)
{
    OutMessage.Reset();
    if (!IsPublicViewEditorOperationSafe(OutMessage))
    {
        return false;
    }
    if (FPackageName::DoesPackageExist(DestinationMapPackage) ||
        FindPackage(nullptr, *DestinationMapPackage))
    {
        OutMessage = FString::Printf(
            TEXT("Refusing to overwrite existing destination map '%s'."),
            *DestinationMapPackage);
        return false;
    }

    TArray<FPublicViewTreePlacement> Placements;
    FString SourceReport;
    if (!ValidatePublicViewSourceSet(&Placements, SourceReport))
    {
        OutMessage = SourceReport;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    TMap<FString, UStaticMesh*> Meshes;
    FString AssetReport;
    if (InspectPublicViewAssetSet(
            AssetSubsystem,
            &Meshes,
            nullptr,
            AssetReport) != EPublicViewAssetState::CompleteValid)
    {
        OutMessage = TEXT("ASSETS_MISSING: build requires the complete pre-imported public-view set. ") +
            AssetReport;
        return false;
    }

    UWorld* PreviousWorld = GEditor->GetEditorWorldContext().World();
    UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!World || !World->PersistentLevel || World == PreviousWorld ||
        World->GetOutermost()->GetName() == DestinationMapPackage)
    {
        OutMessage = TEXT("Could not create a distinct unsaved blank public-view world.");
        return false;
    }
    World->Modify();
    World->PersistentLevel->Modify();

    FString GameModeError;
    if (!SetPersistentPublicViewAirSimGameMode(World, GameModeError))
    {
        OutMessage = GameModeError;
        return false;
    }

    ATRIADIstanaPublicViewSceneActor* Scene =
        SpawnPublicViewActor<ATRIADIstanaPublicViewSceneActor>(
            World,
            TEXT("TRIAD_IPV_PublicExteriorScene"),
            TEXT("Istana Public View - Exterior Scene (Non-Survey)"),
            TEXT("TRIAD/Istana Public View/Scene"));
    ATRIADIstanaPublicViewRuntimePolicyActor* Policy =
        SpawnPublicViewActor<ATRIADIstanaPublicViewRuntimePolicyActor>(
            World,
            TEXT("TRIAD_IPV_RuntimePolicy"),
            TEXT("Istana Public View - Clear Weather / Fixed Camera Policy"),
            TEXT("TRIAD/Istana Public View/Runtime"));
    if (!Scene || !Policy)
    {
        OutMessage = TEXT("Could not spawn isolated public-view scene/runtime actors.");
        return false;
    }
    Scene->Modify();
    Scene->Tags.AddUnique(SceneActorTag);
    Scene->MinimumRequiredTreeInstances = RequiredTreeInstanceCount;
    Scene->MinimumRequiredHeroTreeInstances = RequiredHeroTreeInstanceCount;
    Scene->HeroTreeRadiusMeters = static_cast<float>(HeroTreeRadiusMeters);
    Scene->ContextRadiusMeters = static_cast<float>(ContextRadiusMeters);
    FString SceneError;
    if (!Scene->ConfigurePublicViewAssets(
            Meshes.FindRef(TEXT("SM_IstanaPublicView_Building_Hero")),
            Meshes.FindRef(TEXT("SM_IstanaPublicView_Building_Collision")),
            Meshes.FindRef(TEXT("SM_IstanaPublicView_Terrain")),
            Meshes.FindRef(TEXT("SM_IstanaPublicView_TerrainSkirt")),
            Meshes.FindRef(TEXT("SM_IstanaPublicView_Hardscape")),
            Meshes.FindRef(TEXT("SM_IstanaPublicView_ContextBuildings")),
            Meshes.FindRef(TEXT("SM_IstanaPublicView_OSMContextBuildings")),
            Meshes.FindRef(TEXT("SM_IstanaPublicView_Tree_Rain")),
            Meshes.FindRef(TEXT("SM_IstanaPublicView_Tree_Palm")),
            Meshes.FindRef(TEXT("SM_IstanaPublicView_Tree_Framing")),
            SceneError))
    {
        OutMessage = TEXT("ASSETS_MISSING: ") + SceneError;
        return false;
    }
    Scene->ClearVolumetricTreeInstances();
    for (const FPublicViewTreePlacement& Placement : Placements)
    {
        if (!Scene->AddVolumetricTreeInstance(
                Placement.Archetype,
                Placement.LocalTransform,
                SceneError))
        {
            OutMessage = TEXT("Invalid volumetric placement: ") + SceneError;
            return false;
        }
    }
    FString SceneReport;
    if (!Scene->ValidatePublicViewScene(SceneReport))
    {
        OutMessage = SceneReport;
        return false;
    }

    Policy->Modify();
    Policy->Tags.AddUnique(RuntimePolicyTag);
    Policy->ClaimLabel = PublicViewClaimLabel;
    Policy->ReferenceEpoch = TEXT("2024-04");
    Policy->DisplayColorPolicy = PublicViewDisplayPolicy;
    Policy->bExternalOcioDisplayValidated = false;
    Policy->bSuppressAirSimVisualWeather = true;
    Policy->bSuppressExponentialHeightFog = true;
    Policy->bRequireIstanaAirSimGameMode = true;
    Policy->RequiredGameModeClassPath = IstanaAirSimGameModeClassPath;
    Policy->bEnforceFixedPrimaryCamera = true;
    Policy->RequiredPrimaryCameraTag = PrimaryCameraTag;

    FString CameraError;
    for (const FPublicViewCameraSpec& CameraSpec : GetCameraSpecs())
    {
        if (!SpawnFixedCamera(World, CameraSpec, CameraError))
        {
            OutMessage = CameraError;
            return false;
        }
    }
    FString LightingError;
    if (!ConfigurePublicViewLighting(World, LightingError))
    {
        OutMessage = LightingError;
        return false;
    }

    FString PreSaveReport;
    if (!ValidatePublicViewWorld(World, false, PreSaveReport))
    {
        OutMessage = TEXT("Refusing to save invalid blank-map output. ") + PreSaveReport;
        return false;
    }
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, DestinationMapPackage))
    {
        OutMessage = FString::Printf(
            TEXT("Validated public-view world could not be saved to exact destination '%s'."),
            *DestinationMapPackage);
        return false;
    }
    FString PersistedReport;
    if (!ReloadAndValidatePublicViewMap(PersistedReport))
    {
        OutMessage = TEXT("Destination was saved but persistence validation failed: ") +
            PersistedReport;
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Created and reopened non-overwriting destination %s from a new blank map. %s %s %s No existing Istana/DigitalTwin map or asset was loaded into Unreal, copied, mutated, or saved."),
        *DestinationMapPackage,
        *SourceReport,
        *SceneReport,
        *PersistedReport);
    return true;
}

bool UTRIADIstanaPublicViewEditorLibrary::ValidateIstanaPublicViewExteriorMap(
    FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || World->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutReport = FString::Printf(
            TEXT("Open exact map '%s' before validation; current package is '%s'."),
            *DestinationMapPackage,
            World ? *World->GetOutermost()->GetName() : TEXT("<none>"));
        return false;
    }
    return ValidatePublicViewWorld(World, true, OutReport);
}

bool UTRIADIstanaPublicViewEditorLibrary::ValidateIstanaPublicViewPlayWorldReadiness(
    FString& OutReport)
{
    UWorld* EditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString EditorStructuralReport;
    if (!ValidatePublicViewWorld(EditorWorld, true, EditorStructuralReport))
    {
        OutReport = TEXT("PIE public-view readiness failed because the editor map is structurally invalid:\n") +
            EditorStructuralReport;
        return false;
    }

    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE)
    {
        OutReport = TEXT("Start Play-In-Editor in Istana_PublicView_Exterior_v1 before runtime validation.");
        return false;
    }
    if (!PlayWorld->IsGameWorld() || !PlayWorld->HasBegunPlay() ||
        (GEditor && GEditor->IsSimulatingInEditor()))
    {
        OutReport = TEXT("PIE public-view readiness failed: PlayWorld has not begun authoritative non-simulated play.");
        return false;
    }
    const FString SourcePackage = UWorld::RemovePIEPrefix(
        PlayWorld->GetOutermost()->GetName());
    if (SourcePackage != DestinationMapPackage)
    {
        OutReport = FString::Printf(
            TEXT("PIE public-view readiness failed: PlayWorld source package is '%s', expected exact destination '%s'."),
            *SourcePackage,
            *DestinationMapPackage);
        return false;
    }

    TArray<FString> Errors;
    TArray<FString> Notes;
    UClass* ExternalAirSimGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *ExternalAirSimGameModeClassPath);
    UClass* RequiredGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *IstanaAirSimGameModeClassPath);
    AGameModeBase* ActiveGameMode = PlayWorld->GetAuthGameMode();
    if (!ExternalAirSimGameModeClass || !RequiredGameModeClass ||
        RequiredGameModeClass->GetPathName() != IstanaAirSimGameModeClassPath ||
        !RequiredGameModeClass->IsChildOf(ExternalAirSimGameModeClass))
    {
        Errors.Add(TEXT("The TRIAD-owned AirSim GameMode wrapper or its expected AirSim lineage is unavailable."));
    }
    else if (!ActiveGameMode || ActiveGameMode->GetWorld() != PlayWorld ||
        ActiveGameMode->GetClass() != RequiredGameModeClass)
    {
        Errors.Add(FString::Printf(
            TEXT("PIE instantiated GameMode '%s' instead of exact wrapper '%s'."),
            ActiveGameMode
                ? *ActiveGameMode->GetClass()->GetPathName()
                : TEXT("<none>"),
            *IstanaAirSimGameModeClassPath));
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
    if (SceneCount != 1 || !Scene || !Scene->HasActorBegunPlay())
    {
        Errors.Add(FString::Printf(
            TEXT("PIE requires exactly one begun tagged public-view scene actor; found %d."),
            SceneCount));
    }
    else
    {
        FString RuntimeSceneReport;
        if (!Scene->ValidatePublicViewScene(RuntimeSceneReport))
        {
            Errors.Add(TEXT("PIE public-view scene runtime invariants failed: ") +
                RuntimeSceneReport);
        }
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

    int32 TotalRuntimeCameraCount = 0;
    for (TActorIterator<ACameraActor> It(PlayWorld); It; ++It)
    {
        if (IsValid(*It) && It->GetWorld() == PlayWorld)
        {
            ++TotalRuntimeCameraCount;
        }
    }

    ACameraActor* PrimaryCamera = nullptr;
    TArray<ACameraActor*> UniqueTaggedCameras;
    for (const FPublicViewCameraSpec& Spec : GetCameraSpecs())
    {
        ACameraActor* MatchingCamera = nullptr;
        int32 MatchingCameraCount = 0;
        for (TActorIterator<ACameraActor> It(PlayWorld); It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == PlayWorld &&
                It->ActorHasTag(Spec.RequiredTag))
            {
                MatchingCamera = *It;
                ++MatchingCameraCount;
            }
        }
        if (MatchingCameraCount != 1 || !MatchingCamera)
        {
            Errors.Add(FString::Printf(
                TEXT("PIE requires exactly one public-view camera with tag '%s'; found %d."),
                *Spec.RequiredTag.ToString(),
                MatchingCameraCount));
            continue;
        }
        if (UniqueTaggedCameras.Contains(MatchingCamera))
        {
            Errors.Add(FString::Printf(
                TEXT("PIE public-view camera tag '%s' is not bound to a distinct camera actor."),
                *Spec.RequiredTag.ToString()));
            continue;
        }
        UniqueTaggedCameras.Add(MatchingCamera);
        if (!HasNeutralCameraProfile(
                MatchingCamera->GetCameraComponent(),
                Spec.FieldOfView))
        {
            Errors.Add(FString::Printf(
                TEXT("PIE public-view camera profile changed for tag '%s'."),
                *Spec.RequiredTag.ToString()));
        }
        if (Spec.bPrimary)
        {
            PrimaryCamera = MatchingCamera;
        }
    }
    if (UniqueTaggedCameras.Num() != GetCameraSpecs().Num())
    {
        Errors.Add(FString::Printf(
            TEXT("PIE requires four distinct tagged public-view cameras; resolved %d."),
            UniqueTaggedCameras.Num()));
    }

    int32 FogComponentCount = -1;
    FString CameraReason;
    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(PlayWorld, 0);
    const bool bLiveWeatherAndFogClear = Policy &&
        Policy->IsFogSuppressionActive(FogComponentCount);
    const bool bLivePrimaryCameraProfile = Policy &&
        Policy->IsFixedPrimaryCameraProfileActive(CameraReason);
    if (PolicyCount != 1 || !Policy)
    {
        Errors.Add(FString::Printf(
            TEXT("PIE requires exactly one tagged public-view runtime policy; found %d."),
            PolicyCount));
    }
    else
    {
        if (!Policy->HasActorBegunPlay() ||
            Policy->ClaimLabel != PublicViewClaimLabel ||
            Policy->ReferenceEpoch != TEXT("2024-04") ||
            Policy->DisplayColorPolicy != PublicViewDisplayPolicy ||
            Policy->bExternalOcioDisplayValidated ||
            !Policy->bSuppressAirSimVisualWeather ||
            !Policy->bSuppressExponentialHeightFog ||
            !Policy->bRequireIstanaAirSimGameMode ||
            Policy->RequiredGameModeClassPath != IstanaAirSimGameModeClassPath ||
            !Policy->bEnforceFixedPrimaryCamera ||
            Policy->RequiredPrimaryCameraTag != PrimaryCameraTag ||
            !Policy->bRuntimePolicySettledAtRuntime ||
            !Policy->bGameModeOverrideVerifiedAtRuntime)
        {
            Errors.Add(TEXT("PIE public-view runtime policy identity, exact configuration, or bounded settle state is invalid."));
        }
        if (!Policy->bFogSuppressionVerifiedAtRuntime ||
            !bLiveWeatherAndFogClear ||
            FogComponentCount != Policy->FogComponentCountAtRuntime)
        {
            Errors.Add(FString::Printf(
                TEXT("PIE AirSim weather/fog suppression is not live and stable: live=%d fog component(s), last-verified=%d; all eight weather scalars must read zero with weather disabled, and every runtime fog component must have exact zero density/opacity with volumetric fog disabled."),
                FogComponentCount,
                Policy->FogComponentCountAtRuntime));
        }
        else
        {
            Notes.Add(FString::Printf(
                TEXT("Verified live clear-weather readback and exact suppression across %d AirSim/runtime fog component(s)."),
                FogComponentCount));
        }
        if (!Policy->bPrimaryCameraVerifiedAtRuntime ||
            !bLivePrimaryCameraProfile)
        {
            Errors.Add(TEXT("PIE fixed primary-camera profile is not live and verified: ") +
                CameraReason);
        }
    }

    if (!PrimaryCamera || !PlayerController ||
        PlayerController->GetWorld() != PlayWorld ||
        !PlayerController->IsLocalController() ||
        PlayerController->GetViewTarget() != PrimaryCamera)
    {
        Errors.Add(FString::Printf(
            TEXT("PIE Player 0 does not use the unique tagged primary public-view camera (camera=%s, controller=%s, view=%s)."),
            PrimaryCamera ? TEXT("true") : TEXT("false"),
            PlayerController ? TEXT("true") : TEXT("false"),
            PlayerController && PlayerController->GetViewTarget() == PrimaryCamera
                ? TEXT("exact")
                : TEXT("mismatch")));
    }

    if (Errors.Num() > 0)
    {
        OutReport = TEXT("PIE public-view readiness failed:\n - ") +
            FString::Join(Errors, TEXT("\n - ")) +
            TEXT("\nEditor-map structural validation passed separately; PlayWorld checks intentionally allowed AirSim-added cameras and fog actors and did not compare absolute transforms or total actor counts.");
        if (Notes.Num() > 0)
        {
            OutReport += TEXT("\nNotes:\n - ") +
                FString::Join(Notes, TEXT("\n - "));
        }
        return false;
    }

    Notes.Add(FString::Printf(
        TEXT("Resolved four distinct tagged public-view cameras among %d total runtime camera actor(s); AirSim-added untagged cameras are allowed."),
        TotalRuntimeCameraCount));
    OutReport = FString::Printf(
        TEXT("PIE PBR public-view readiness passed: editor-map structure was validated separately at exact destination '%s'; PlayWorld source identity, exact TRIAD AirSim GameMode, begun public-view scene/runtime policy, live clear-weather/fog readback, four unique tagged cameras, and Player 0 primary view are ready. No PlayWorld absolute-transform or total camera/fog actor-count equality was required.\n - %s\nClaim=%s. OSM_CONTEXT_NOT_VISUALLY_OR_PHOTO_ACCEPTED; © OpenStreetMap contributors (ODbL-1.0). MATERIAL_ACCEPTANCE_BLOCKED_PENDING_PHOTO_QA: exact PBR assets are integrated, but external sRGB/Rec.709 display/OCIO calibration and photo-match acceptance are not validated."),
        *DestinationMapPackage,
        *FString::Join(Notes, TEXT("\n - ")),
        *PublicViewClaimLabel);
    return true;
}

bool UTRIADIstanaPublicViewEditorLibrary::
    ValidateIstanaPublicViewHeroV2PlayWorldReadiness(FString& OutReport)
{
    UWorld* EditorWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    FString EditorStructuralReport;
    if (!EditorWorld ||
        !UTRIADIstanaPublicViewHeroV2EditorLibrary::
            ValidateIstanaPublicViewHeroV2Map(EditorStructuralReport))
    {
        OutReport = TEXT("Hero-v2 PIE readiness failed because the exact v2 editor map or its hero-only preservation contract is invalid:\n") +
            EditorStructuralReport;
        return false;
    }

    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE ||
        !PlayWorld->IsGameWorld() || !PlayWorld->HasBegunPlay() ||
        (GEditor && GEditor->IsSimulatingInEditor()))
    {
        OutReport = TEXT("Start authoritative non-simulated PIE in Istana_PublicView_Exterior_v2 before hero-v2 runtime validation.");
        return false;
    }
    const FString SourcePackage = UWorld::RemovePIEPrefix(
        PlayWorld->GetOutermost()->GetName());
    if (SourcePackage != HeroV2DestinationMapPackage)
    {
        OutReport = FString::Printf(
            TEXT("Hero-v2 PIE readiness failed: PlayWorld source package is '%s', expected exact destination '%s'."),
            *SourcePackage,
            *HeroV2DestinationMapPackage);
        return false;
    }

    TArray<FString> Errors;
    TArray<FString> Notes;
    UClass* ExternalAirSimGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *ExternalAirSimGameModeClassPath);
    UClass* RequiredGameModeClass = StaticLoadClass(
        AGameModeBase::StaticClass(),
        nullptr,
        *IstanaAirSimGameModeClassPath);
    AGameModeBase* ActiveGameMode = PlayWorld->GetAuthGameMode();
    if (!ExternalAirSimGameModeClass || !RequiredGameModeClass ||
        RequiredGameModeClass->GetPathName() != IstanaAirSimGameModeClassPath ||
        !RequiredGameModeClass->IsChildOf(ExternalAirSimGameModeClass) ||
        !ActiveGameMode || ActiveGameMode->GetWorld() != PlayWorld ||
        ActiveGameMode->GetClass() != RequiredGameModeClass)
    {
        Errors.Add(TEXT("Hero-v2 PIE did not instantiate the exact TRIAD-owned AirSim GameMode wrapper and lineage."));
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
    if (SceneCount != 1 || !Scene || !Scene->HasActorBegunPlay())
    {
        Errors.Add(FString::Printf(
            TEXT("Hero-v2 PIE requires exactly one begun tagged public-view scene actor; found %d."),
            SceneCount));
    }
    else
    {
        FString RuntimeSceneReport;
        if (!Scene->ValidatePublicViewScene(RuntimeSceneReport))
        {
            Errors.Add(TEXT("Hero-v2 runtime scene invariants failed: ") +
                RuntimeSceneReport);
        }
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

    ACameraActor* PrimaryCamera = nullptr;
    TArray<ACameraActor*> UniqueTaggedCameras;
    for (const FPublicViewCameraSpec& Spec : GetCameraSpecs())
    {
        ACameraActor* MatchingCamera = nullptr;
        int32 MatchingCameraCount = 0;
        for (TActorIterator<ACameraActor> It(PlayWorld); It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == PlayWorld &&
                It->ActorHasTag(Spec.RequiredTag))
            {
                MatchingCamera = *It;
                ++MatchingCameraCount;
            }
        }
        if (MatchingCameraCount != 1 || !MatchingCamera ||
            UniqueTaggedCameras.Contains(MatchingCamera) ||
            !HasNeutralCameraProfile(
                MatchingCamera->GetCameraComponent(),
                Spec.FieldOfView))
        {
            Errors.Add(FString::Printf(
                TEXT("Hero-v2 PIE camera contract failed for tag '%s' (count=%d)."),
                *Spec.RequiredTag.ToString(),
                MatchingCameraCount));
            continue;
        }
        UniqueTaggedCameras.Add(MatchingCamera);
        if (Spec.bPrimary)
        {
            PrimaryCamera = MatchingCamera;
        }
    }
    if (UniqueTaggedCameras.Num() != GetCameraSpecs().Num())
    {
        Errors.Add(FString::Printf(
            TEXT("Hero-v2 PIE requires four distinct unchanged tagged cameras; resolved %d."),
            UniqueTaggedCameras.Num()));
    }

    int32 FogComponentCount = -1;
    FString CameraReason;
    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(PlayWorld, 0);
    const bool bLiveWeatherAndFogClear = Policy &&
        Policy->IsFogSuppressionActive(FogComponentCount);
    const bool bLivePrimaryCameraProfile = Policy &&
        Policy->IsFixedPrimaryCameraProfileActive(CameraReason);
    if (PolicyCount != 1 || !Policy || !Policy->HasActorBegunPlay() ||
        Policy->ClaimLabel != PublicViewClaimLabel ||
        Policy->ReferenceEpoch != TEXT("2024-04") ||
        Policy->DisplayColorPolicy != PublicViewDisplayPolicy ||
        Policy->bExternalOcioDisplayValidated ||
        !Policy->bSuppressAirSimVisualWeather ||
        !Policy->bSuppressExponentialHeightFog ||
        !Policy->bRequireIstanaAirSimGameMode ||
        Policy->RequiredGameModeClassPath != IstanaAirSimGameModeClassPath ||
        !Policy->bEnforceFixedPrimaryCamera ||
        Policy->RequiredPrimaryCameraTag != PrimaryCameraTag ||
        !Policy->bRuntimePolicySettledAtRuntime ||
        !Policy->bGameModeOverrideVerifiedAtRuntime ||
        !Policy->bFogSuppressionVerifiedAtRuntime ||
        !bLiveWeatherAndFogClear ||
        FogComponentCount != Policy->FogComponentCountAtRuntime ||
        !Policy->bPrimaryCameraVerifiedAtRuntime ||
        !bLivePrimaryCameraProfile)
    {
        Errors.Add(TEXT("Hero-v2 PIE runtime policy identity, clear-weather/fog readback, or fixed-primary-camera settle state is invalid. ") +
            CameraReason);
    }
    else
    {
        Notes.Add(FString::Printf(
            TEXT("Verified exact clear-weather suppression across %d runtime fog component(s)."),
            FogComponentCount));
    }

    if (!PrimaryCamera || !PlayerController ||
        PlayerController->GetWorld() != PlayWorld ||
        !PlayerController->IsLocalController() ||
        PlayerController->GetViewTarget() != PrimaryCamera)
    {
        Errors.Add(TEXT("Hero-v2 PIE Player 0 is not restored to the unique tagged primary public-view camera."));
    }

    if (Errors.Num() > 0)
    {
        OutReport = TEXT("Hero-v2 PIE readiness failed:\n - ") +
            FString::Join(Errors, TEXT("\n - ")) +
            TEXT("\nThe exact v2 editor map passed its independent hero-only preservation validator before these runtime checks.");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("Hero-v2 PIE readiness passed for exact destination '%s': independent hero-only map preservation, exact AirSim GameMode, begun public-view scene/policy, clear weather and fog suppression, four unchanged tagged cameras, and Player 0 primary view are verified. Capture-only cameras are transient PIE actors and are never saved.\n - %s"),
        *HeroV2DestinationMapPackage,
        *FString::Join(Notes, TEXT("\n - ")));
    return true;
}

bool UTRIADIstanaPublicViewEditorLibrary::CaptureIstanaPublicViewPlayCamera(
    const FString& CameraPreset,
    const FString& OutputFileName,
    FString& OutMessage)
{
    OutMessage.Reset();
    FString ReadinessReport;
    if (!ValidateIstanaPublicViewPlayWorldReadiness(ReadinessReport))
    {
        OutMessage = TEXT("Public-view PIE camera capture requires settled runtime readiness. ") +
            ReadinessReport;
        return false;
    }

    const FString NormalizedPreset = CameraPreset.ToUpper();
    FName RequiredTag;
    if (NormalizedPreset == TEXT("CEREMONIAL_FRONT"))
    {
        RequiredTag = PrimaryCameraTag;
    }
    else if (NormalizedPreset == TEXT("FRONT_OBLIQUE"))
    {
        RequiredTag = TEXT("TRIADIstanaPublicViewCamera_FrontOblique");
    }
    else if (NormalizedPreset == TEXT("ARCADE"))
    {
        RequiredTag = TEXT("TRIADIstanaPublicViewCamera_Arcade");
    }
    else if (NormalizedPreset == TEXT("TOWER"))
    {
        RequiredTag = TEXT("TRIADIstanaPublicViewCamera_Tower");
    }
    else
    {
        OutMessage = TEXT("CameraPreset must be CEREMONIAL_FRONT, FRONT_OBLIQUE, ARCADE, or TOWER.");
        return false;
    }

    if (OutputFileName.IsEmpty() || OutputFileName.Len() > 128 ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        !OutputFileName.StartsWith(TEXT("ipv_play_"), ESearchCase::CaseSensitive) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::CaseSensitive))
    {
        OutMessage = TEXT("OutputFileName must be a leaf-only lowercase 'ipv_play_*.png' name of at most 128 characters.");
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

    const FPublicViewCameraSpec* RequestedSpec = nullptr;
    for (const FPublicViewCameraSpec& Spec : GetCameraSpecs())
    {
        if (Spec.RequiredTag == RequiredTag)
        {
            RequestedSpec = &Spec;
            break;
        }
    }
    if (!RequestedSpec)
    {
        OutMessage = TEXT("The requested preset has no frozen public-view camera specification.");
        return false;
    }

    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE ||
        UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName()) !=
            DestinationMapPackage)
    {
        OutMessage = TEXT("The active PIE world is not the exact public-view destination map.");
        return false;
    }

    ACameraActor* RequestedCamera = nullptr;
    ACameraActor* PrimaryCamera = nullptr;
    int32 RequestedCameraCount = 0;
    int32 PrimaryCameraCount = 0;
    for (TActorIterator<ACameraActor> It(PlayWorld); It; ++It)
    {
        ACameraActor* Camera = *It;
        if (!IsValid(Camera) || Camera->GetWorld() != PlayWorld)
        {
            continue;
        }
        if (Camera->ActorHasTag(RequiredTag))
        {
            RequestedCamera = Camera;
            ++RequestedCameraCount;
        }
        if (Camera->ActorHasTag(PrimaryCameraTag))
        {
            PrimaryCamera = Camera;
            ++PrimaryCameraCount;
        }
    }
    UCameraComponent* RequestedCameraComponent = RequestedCamera
        ? RequestedCamera->GetCameraComponent()
        : nullptr;
    if (RequestedCameraCount != 1 || PrimaryCameraCount != 1 ||
        !RequestedCamera || !PrimaryCamera || !RequestedCameraComponent ||
        !HasNeutralCameraProfile(
            RequestedCameraComponent,
            RequestedSpec->FieldOfView))
    {
        OutMessage = FString::Printf(
            TEXT("Exact camera/profile lookup failed for preset '%s' (requested=%d, primary=%d)."),
            *NormalizedPreset,
            RequestedCameraCount,
            PrimaryCameraCount);
        return false;
    }

    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(PlayWorld, 0);
    APlayerCameraManager* CameraManager = PlayerController
        ? PlayerController->PlayerCameraManager
        : nullptr;
    UGameViewportClient* GameViewportClient = PlayWorld->GetGameViewport();
    FSceneViewport* GameViewport = GameViewportClient
        ? GameViewportClient->GetGameViewport()
        : nullptr;
    if (!PlayerController || PlayerController->GetWorld() != PlayWorld ||
        !PlayerController->IsLocalController() || !CameraManager || !GameViewport)
    {
        OutMessage = TEXT("Player 0, its camera manager, or the active PIE game viewport is unavailable.");
        return false;
    }
    if (FScreenshotRequest::IsScreenshotRequested())
    {
        OutMessage = TEXT("Another Unreal screenshot request is already pending; no capture was scheduled.");
        return false;
    }

    const FString OutputDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/IstanaPreviews/PublicView")));
    const FString DestinationPath = FPaths::Combine(
        OutputDirectory,
        OutputFileName);
    if (IFileManager::Get().FileExists(*DestinationPath))
    {
        OutMessage = FString::Printf(
            TEXT("Refusing to overwrite existing public-view capture '%s'."),
            *DestinationPath);
        return false;
    }
    if (!IFileManager::Get().DirectoryExists(*OutputDirectory) &&
        !IFileManager::Get().MakeDirectory(*OutputDirectory, true))
    {
        OutMessage = FString::Printf(
            TEXT("Could not create isolated public-view capture directory '%s'."),
            *OutputDirectory);
        return false;
    }

    const bool bViewportWasFixed = GameViewport->HasFixedSize();
    const FIntPoint OriginalViewportSize = GameViewport->GetSizeXY();
    const auto RestoreViewportSize = [GameViewport, bViewportWasFixed, OriginalViewportSize]()
    {
        if (bViewportWasFixed && OriginalViewportSize.X > 0 &&
            OriginalViewportSize.Y > 0)
        {
            GameViewport->SetFixedViewportSize(
                OriginalViewportSize.X,
                OriginalViewportSize.Y);
        }
        else
        {
            GameViewport->SetFixedViewportSize(0, 0);
        }
    };
    const auto RestorePrimaryCamera = [PlayerController, CameraManager, PrimaryCamera]()
    {
        PlayerController->SetViewTargetWithBlend(PrimaryCamera, 0.0f);
        CameraManager->UpdateCamera(0.0f);
    };

    GameViewport->SetFixedViewportSize(1920, 1080);
    if (GameViewport->GetSizeXY() != FIntPoint(1920, 1080))
    {
        RestoreViewportSize();
        OutMessage = TEXT("PIE game viewport rejected the exact 1920x1080 acceptance size.");
        return false;
    }

    PlayerController->SetViewTargetWithBlend(RequestedCamera, 0.0f);
    CameraManager->UpdateCamera(0.0f);
    FMinimalViewInfo ExpectedView;
    RequestedCameraComponent->GetCameraView(0.0f, ExpectedView);
    const FMinimalViewInfo& CachedView = CameraManager->GetCameraCacheView();
    const bool bCachedViewMatches =
        PlayerController->GetViewTarget() == RequestedCamera &&
        CachedView.Location.Equals(ExpectedView.Location, 0.1) &&
        CachedView.Rotation.Equals(ExpectedView.Rotation, 0.01) &&
        FMath::IsNearlyEqual(CachedView.FOV, ExpectedView.FOV, 0.01f) &&
        FMath::IsNearlyEqual(
            CachedView.AspectRatio,
            ExpectedView.AspectRatio,
            0.0001f);
    if (!bCachedViewMatches)
    {
        RestorePrimaryCamera();
        RestoreViewportSize();
        OutMessage = FString::Printf(
            TEXT("Player 0 cached POV did not match exact camera '%s'; no screenshot was requested."),
            *RequiredTag.ToString());
        return false;
    }

    // Draw one settled frame at the exact requested size before arming the
    // global one-shot screenshot request, then capture without Slate UI.
    GameViewport->Draw(false);
    FScreenshotRequest::RequestScreenshot(DestinationPath, false, false, false);
    GameViewport->Draw(false);

    RestorePrimaryCamera();
    RestoreViewportSize();
    FString RestoredReadiness;
    if (!ValidateIstanaPublicViewPlayWorldReadiness(RestoredReadiness))
    {
        OutMessage = FString::Printf(
            TEXT("Screenshot for '%s' was requested at '%s', but primary-camera restoration failed: %s"),
            *NormalizedPreset,
            *DestinationPath,
            *RestoredReadiness);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("Requested exact 1920x1080 public-view PIE capture preset=%s tag=%s path='%s'; Player 0 was restored to the primary camera. %s"),
        *NormalizedPreset,
        *RequiredTag.ToString(),
        *DestinationPath,
        *RestoredReadiness);
    return true;
}

bool UTRIADIstanaPublicViewEditorLibrary::
    CaptureIstanaPublicViewHeroV2PlayCamera(
        const FString& CameraPreset,
        const FString& OutputFileName,
        FString& OutMessage)
{
    OutMessage.Reset();
    FString ReadinessReport;
    if (!ValidateIstanaPublicViewHeroV2PlayWorldReadiness(ReadinessReport))
    {
        OutMessage = TEXT("Hero-v2 PIE camera capture requires settled runtime readiness. ") +
            ReadinessReport;
        return false;
    }

    const FString NormalizedPreset = CameraPreset.ToUpper();
    FVector LocalCaptureLocation;
    FVector LocalCaptureLookAt;
    float CaptureFieldOfView = 0.0f;
    if (NormalizedPreset == TEXT("HERO_FRONT_CLOSE"))
    {
        LocalCaptureLocation = FVector(0.0, 29200.0, 5000.0);
        LocalCaptureLookAt = FVector(0.0, 500.0, 1350.0);
        CaptureFieldOfView = 32.0f;
    }
    else if (NormalizedPreset == TEXT("HERO_FRONT_OBLIQUE_CLOSE"))
    {
        LocalCaptureLocation = FVector(17500.0, 26500.0, 4500.0);
        LocalCaptureLookAt = FVector(0.0, 500.0, 1400.0);
        CaptureFieldOfView = 28.0f;
    }
    else if (NormalizedPreset == TEXT("HERO_FACADE_MACRO"))
    {
        LocalCaptureLocation = FVector(0.0, 10000.0, 1800.0);
        LocalCaptureLookAt = FVector(0.0, 5000.0, 600.0);
        CaptureFieldOfView = 58.0f;
    }
    else
    {
        OutMessage = TEXT("CameraPreset must be HERO_FRONT_CLOSE, HERO_FRONT_OBLIQUE_CLOSE, or HERO_FACADE_MACRO.");
        return false;
    }

    if (OutputFileName.IsEmpty() || OutputFileName.Len() > 128 ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        !OutputFileName.StartsWith(TEXT("ipv_v2_play_"), ESearchCase::CaseSensitive) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::CaseSensitive))
    {
        OutMessage = TEXT("OutputFileName must be a leaf-only lowercase 'ipv_v2_play_*.png' name of at most 128 characters.");
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
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE ||
        UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName()) !=
            HeroV2DestinationMapPackage)
    {
        OutMessage = TEXT("The active PIE world is not the exact hero-v2 public-view destination map.");
        return false;
    }

    ATRIADIstanaPublicViewSceneActor* SceneActor = nullptr;
    int32 SceneActorCount = 0;
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(PlayWorld); It; ++It)
    {
        ATRIADIstanaPublicViewSceneActor* Candidate = *It;
        if (IsValid(Candidate) && Candidate->GetWorld() == PlayWorld &&
            Candidate->ActorHasTag(SceneActorTag))
        {
            SceneActor = Candidate;
            ++SceneActorCount;
        }
    }
    const bool bSceneHasBegunPlay = SceneActor && SceneActor->HasActorBegunPlay();
    const FTransform SceneFrame = SceneActor
        ? SceneActor->GetActorTransform()
        : FTransform::Identity;
    if (SceneActorCount != 1 || !SceneActor || !bSceneHasBegunPlay ||
        !SceneFrame.IsValid())
    {
        OutMessage = FString::Printf(
            TEXT("Hero-v2 close capture requires exactly one begun, valid tagged public-view scene frame; found %d (begun=%s, transformValid=%s)."),
            SceneActorCount,
            bSceneHasBegunPlay ? TEXT("true") : TEXT("false"),
            SceneFrame.IsValid() ? TEXT("true") : TEXT("false"));
        return false;
    }

    // AirSim rebases the PIE world origin. Treat acceptance-camera presets as
    // scene-local coordinates so the camera and target follow the begun scene
    // actor's rebased transform instead of silently reverting to map-space.
    const FVector CaptureLocation =
        SceneFrame.TransformPosition(LocalCaptureLocation);
    const FVector CaptureLookAt =
        SceneFrame.TransformPosition(LocalCaptureLookAt);
    const auto IsFiniteVector = [](const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    };
    if (!IsFiniteVector(CaptureLocation) || !IsFiniteVector(CaptureLookAt) ||
        (CaptureLookAt - CaptureLocation).IsNearlyZero())
    {
        OutMessage = FString::Printf(
            TEXT("Hero-v2 scene-local camera transform produced an invalid world-space view for preset '%s'."),
            *NormalizedPreset);
        return false;
    }

    ACameraActor* PrimaryCamera = nullptr;
    int32 PrimaryCameraCount = 0;
    for (TActorIterator<ACameraActor> It(PlayWorld); It; ++It)
    {
        ACameraActor* Camera = *It;
        if (IsValid(Camera) && Camera->GetWorld() == PlayWorld &&
            Camera->ActorHasTag(PrimaryCameraTag))
        {
            PrimaryCamera = Camera;
            ++PrimaryCameraCount;
        }
    }
    if (PrimaryCameraCount != 1 || !PrimaryCamera ||
        !HasNeutralCameraProfile(PrimaryCamera->GetCameraComponent(), 52.0f))
    {
        OutMessage = FString::Printf(
            TEXT("Hero-v2 close capture requires one unchanged primary public-view camera; found %d."),
            PrimaryCameraCount);
        return false;
    }

    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(PlayWorld, 0);
    APlayerCameraManager* CameraManager = PlayerController
        ? PlayerController->PlayerCameraManager
        : nullptr;
    UGameViewportClient* GameViewportClient = PlayWorld->GetGameViewport();
    FSceneViewport* GameViewport = GameViewportClient
        ? GameViewportClient->GetGameViewport()
        : nullptr;
    if (!PlayerController || PlayerController->GetWorld() != PlayWorld ||
        !PlayerController->IsLocalController() || !CameraManager || !GameViewport)
    {
        OutMessage = TEXT("Player 0, its camera manager, or the active PIE game viewport is unavailable.");
        return false;
    }
    if (FScreenshotRequest::IsScreenshotRequested())
    {
        OutMessage = TEXT("Another Unreal screenshot request is already pending; no hero-v2 capture was scheduled.");
        return false;
    }

    const FString OutputDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/IstanaPreviews/PublicViewHeroV2")));
    const FString DestinationPath = FPaths::Combine(
        OutputDirectory,
        OutputFileName);
    if (IFileManager::Get().FileExists(*DestinationPath))
    {
        OutMessage = FString::Printf(
            TEXT("Refusing to overwrite existing hero-v2 public-view capture '%s'."),
            *DestinationPath);
        return false;
    }
    if (!IFileManager::Get().DirectoryExists(*OutputDirectory) &&
        !IFileManager::Get().MakeDirectory(*OutputDirectory, true))
    {
        OutMessage = FString::Printf(
            TEXT("Could not create isolated hero-v2 capture directory '%s'."),
            *OutputDirectory);
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.ObjectFlags |= RF_Transient;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACameraActor* CaptureCamera = PlayWorld->SpawnActor<ACameraActor>(
        ACameraActor::StaticClass(),
        CaptureLocation,
        (CaptureLookAt - CaptureLocation).Rotation(),
        SpawnParameters);
    UCameraComponent* CaptureCameraComponent = CaptureCamera
        ? CaptureCamera->GetCameraComponent()
        : nullptr;
    if (!CaptureCamera || !CaptureCameraComponent)
    {
        OutMessage = TEXT("Could not spawn the transient hero-v2 detail camera in PIE.");
        return false;
    }
    CaptureCamera->Tags.AddUnique(
        FName(TEXT("TRIADIstanaPublicViewHeroV2CaptureOnly")));
    ConfigureNeutralCameraProfile(CaptureCameraComponent, CaptureFieldOfView);

    const bool bViewportWasFixed = GameViewport->HasFixedSize();
    const FIntPoint OriginalViewportSize = GameViewport->GetSizeXY();
    const auto RestoreViewportSize = [GameViewport, bViewportWasFixed, OriginalViewportSize]()
    {
        if (bViewportWasFixed && OriginalViewportSize.X > 0 &&
            OriginalViewportSize.Y > 0)
        {
            GameViewport->SetFixedViewportSize(
                OriginalViewportSize.X,
                OriginalViewportSize.Y);
        }
        else
        {
            GameViewport->SetFixedViewportSize(0, 0);
        }
    };
    const auto RestorePrimaryAndDestroyCapture =
        [PlayerController, CameraManager, PrimaryCamera, CaptureCamera]()
    {
        PlayerController->SetViewTargetWithBlend(PrimaryCamera, 0.0f);
        CameraManager->UpdateCamera(0.0f);
        if (IsValid(CaptureCamera))
        {
            CaptureCamera->Destroy();
        }
    };

    GameViewport->SetFixedViewportSize(3840, 2160);
    if (GameViewport->GetSizeXY() != FIntPoint(3840, 2160))
    {
        RestorePrimaryAndDestroyCapture();
        RestoreViewportSize();
        OutMessage = TEXT("PIE game viewport rejected the exact 3840x2160 hero-detail acceptance size.");
        return false;
    }

    PlayerController->SetViewTargetWithBlend(CaptureCamera, 0.0f);
    CameraManager->UpdateCamera(0.0f);
    FMinimalViewInfo ExpectedView;
    CaptureCameraComponent->GetCameraView(0.0f, ExpectedView);
    const FMinimalViewInfo& CachedView = CameraManager->GetCameraCacheView();
    const bool bCachedViewMatches =
        PlayerController->GetViewTarget() == CaptureCamera &&
        CachedView.Location.Equals(ExpectedView.Location, 0.1) &&
        CachedView.Rotation.Equals(ExpectedView.Rotation, 0.01) &&
        FMath::IsNearlyEqual(CachedView.FOV, ExpectedView.FOV, 0.01f) &&
        FMath::IsNearlyEqual(
            CachedView.AspectRatio,
            ExpectedView.AspectRatio,
            0.0001f);
    if (!bCachedViewMatches)
    {
        RestorePrimaryAndDestroyCapture();
        RestoreViewportSize();
        OutMessage = FString::Printf(
            TEXT("Player 0 cached POV did not match transient hero-v2 preset '%s'; no screenshot was requested."),
            *NormalizedPreset);
        return false;
    }

    // The transient camera and fixed viewport affect only this PIE world and
    // are restored immediately after the synchronous UI-free screenshot draw.
    GameViewport->Draw(false);
    FScreenshotRequest::RequestScreenshot(DestinationPath, false, false, false);
    GameViewport->Draw(false);

    // FViewport::Draw normally processes and saves screenshot requests before
    // returning. Fail closed if this viewport did not do so: resetting the
    // request prevents a later frame from capturing the restored primary view.
    const bool bScreenshotRequestProcessed =
        !FScreenshotRequest::IsScreenshotRequested();
    const bool bScreenshotFileWritten =
        IFileManager::Get().FileExists(*DestinationPath);
    if (!bScreenshotRequestProcessed || !bScreenshotFileWritten)
    {
        FScreenshotRequest::Reset();
        RestorePrimaryAndDestroyCapture();
        RestoreViewportSize();
        OutMessage = FString::Printf(
            TEXT("Hero-v2 screenshot draw did not complete synchronously for preset '%s' at '%s' (requestProcessed=%s, fileWritten=%s); the pending request was cleared before restoring Player 0."),
            *NormalizedPreset,
            *DestinationPath,
            bScreenshotRequestProcessed ? TEXT("true") : TEXT("false"),
            bScreenshotFileWritten ? TEXT("true") : TEXT("false"));
        return false;
    }

    RestorePrimaryAndDestroyCapture();
    RestoreViewportSize();
    FString RestoredReadiness;
    if (!ValidateIstanaPublicViewHeroV2PlayWorldReadiness(RestoredReadiness))
    {
        OutMessage = FString::Printf(
            TEXT("Hero-v2 screenshot for '%s' was requested at '%s', but primary-camera/runtime restoration failed: %s"),
            *NormalizedPreset,
            *DestinationPath,
            *RestoredReadiness);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("Captured exact 3840x2160 transient hero-v2 PIE preset=%s path='%s' in the begun public-view scene frame (localCamera=%s, worldCamera=%s); no map actor or surrounding asset was changed, and Player 0 was restored to the primary camera. %s"),
        *NormalizedPreset,
        *DestinationPath,
        *LocalCaptureLocation.ToCompactString(),
        *CaptureLocation.ToCompactString(),
        *RestoredReadiness);
    return true;
}

bool UTRIADIstanaPublicViewEditorLibrary::QuiesceIstanaPublicViewPlayWorldForStop(
    FString& OutMessage)
{
    OutMessage.Reset();
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE)
    {
        OutMessage = TEXT("No PIE PlayWorld exists to quiesce.");
        return false;
    }
    const FString SourcePackage =
        UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName());
    if (SourcePackage != DestinationMapPackage)
    {
        OutMessage = FString::Printf(
            TEXT("Refusing public-view quiescence for PIE source '%s'; expected '%s'."),
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
            TEXT("Exact public-view PIE identity failed before quiescence: policy=%d scene=%d gameMode='%s'. PIE was not stopped."),
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
            TEXT("AirSim quiescence could not be verified across %d begun simulation mode(s). PIE was not stopped."),
            SimModeCount);
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Paused and verified %d begun AirSim simulation mode(s) in exact public-view PIE. Keep PIE alive for the caller's bounded worker-drain interval before explicit stop."),
        SimModeCount);
    return true;
}

bool UTRIADIstanaPublicViewEditorLibrary::
    QuiesceIstanaPublicViewHeroV2PlayWorldForStop(FString& OutMessage)
{
    OutMessage.Reset();
    UWorld* PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE)
    {
        OutMessage = TEXT("No PIE PlayWorld exists to quiesce.");
        return false;
    }
    const FString SourcePackage =
        UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName());
    if (SourcePackage != HeroV2DestinationMapPackage)
    {
        OutMessage = FString::Printf(
            TEXT("Refusing hero-v2 public-view quiescence for PIE source '%s'; expected '%s'."),
            *SourcePackage,
            *HeroV2DestinationMapPackage);
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
            TEXT("Exact hero-v2 public-view PIE identity failed before quiescence: policy=%d scene=%d gameMode='%s'. PIE was not stopped."),
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
            TEXT("AirSim quiescence could not be verified across %d begun simulation mode(s). Hero-v2 PIE was not stopped."),
            SimModeCount);
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Paused and verified %d begun AirSim simulation mode(s) in exact hero-v2 public-view PIE. Keep PIE alive for the caller's bounded worker-drain interval before explicit stop."),
        SimModeCount);
    return true;
}
