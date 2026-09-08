#include "TRIADIstanaExploreV5DPalmHeroEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "Memory/SharedBuffer.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DPalmHeroSourceLibrary.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

#include <openssl/sha.h>

namespace
{
constexpr int32 LodCount = 3;
constexpr int32 MaterialCount = 3;
constexpr int32 TextureCount = 4;
constexpr int32 OutputAssetCount = 8;
constexpr int64 MaximumInputBytes = 16 * 1024 * 1024;
constexpr int64 CandidateContractBytes = 8922;
constexpr int64 ProvenanceManifestBytes = 4851;
// The immutable JSON uses decimal doubles while the pinned UE constants are
// FVector3f.  This bound is just above the largest IEEE-754 float32 rounding
// delta in the 18 pinned bound coordinates (8.244628908471e-7 metres).  The
// file hash must already match before this semantic comparison is reached.
constexpr double ContractBoundsFloat32ToleranceMeters = 0.000001;
constexpr float BoundsToleranceCentimetres = 0.05f;

const FString CandidateContractSha256(
    TEXT("A856DF661D8FD06B7F0BBF5AE55BF711FC4196EA3FBF97E5C68F1031F54CFFC4"));
const FString ProvenanceManifestSha256(
    TEXT("DC7F1CF47E118D008FEF4A32BED721FA7518802860091871AB291DD661AFB504"));

// Deliberately invalid SHA-256 sentinels. Caller-provided paths and hashes can
// inspect receipt contents, but cannot bootstrap execution authority. A future
// reviewed source change must replace both sentinels with exact trusted hashes
// and recompile before the private materializer can pass BuildAdmission.
const FString TrustedAcceptedR33ReceiptSha256(
    TEXT("UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));
const FString TrustedFutureAuthorizationSha256(
    TEXT("UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));
const FString AcceptedR33Schema(
    TEXT("triad.istana_explore_v5d.r33_player0_capture.v1"));
const FString R33TransactionSchema(
    TEXT("triad.istana_explore_v5d.r33_cesium_world_terrain_reference.native_transaction.v1"));
const FString FutureAuthorizationSchema(
    TEXT("triad.istana_public_view_explore_v5d_tree_realism.palm_hero.future_transaction_authority.v1"));
const FString FutureOperation(
    TEXT("MATERIALIZE_PALM_HERO_CANDIDATE_ASSETS_ONLY_AND_EXPOSE_OPTIONAL_SOURCE"));
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration"));
const FString StagingRoot(TEXT("/Temp/TRIAD/PalmHeroIntegrationSourceStaging"));

const TCHAR* const LodRelativePaths[] = {
    TEXT("Generated/SM_IPV5D_PalmHeroCandidate_LOD0.obj"),
    TEXT("Generated/SM_IPV5D_PalmHeroCandidate_LOD1.obj"),
    TEXT("Generated/SM_IPV5D_PalmHeroCandidate_LOD2.obj")};
const TCHAR* const LodSha256[] = {
    TEXT("119F1D8D9AD9E3A8F98BF3E94E32F4B88B221CA6F022DD899BDD276D4376BF97"),
    TEXT("53DBD68149CE2E361EAF2E466B1A6B4CCCE5E9A4C294AD286FAC06E274720D57"),
    TEXT("7E94F23E604B40346D9FE8DF91AB5F8A2651CFF13E37F2E1509F04A2A5082C5F")};
const int64 LodBytes[] = {11120505, 4007254, 1010979};
const int32 LodVertices[] = {57768, 21676, 5918};
const int32 LodTriangles[] = {104244, 37968, 9864};
const int32 LodRootVertices[] = {73, 53, 37};
const FVector3f LodSourceBoundsMinMeters[] = {
    FVector3f(-4.588270f, -5.099290f, 0.0f),
    FVector3f(-4.524105f, -4.885492f, 0.0f),
    FVector3f(-4.711650f, -4.809668f, 0.0f)};
const FVector3f LodSourceBoundsMaxMeters[] = {
    FVector3f(7.059138f, 6.303532f, 17.793116f),
    FVector3f(6.948630f, 6.108454f, 17.764737f),
    FVector3f(6.798521f, 6.397235f, 17.741958f)};
// UE5.5's legacy FBX/OBJ static-mesh path always calls Converter.ConvertPos
// after the import TotalMatrix. ConvertPos maps (X,Y,Z) to (X,-Y,Z); the
// uniform scale above then expresses pinned source metres as Unreal centimetres.
const FVector3f LodExpectedUnrealBoundsMinCentimetres[] = {
    FVector3f(-458.8270f, -630.3532f, 0.0f),
    FVector3f(-452.4105f, -610.8454f, 0.0f),
    FVector3f(-471.1650f, -639.7235f, 0.0f)};
const FVector3f LodExpectedUnrealBoundsMaxCentimetres[] = {
    FVector3f(705.9138f, 509.9290f, 1779.3116f),
    FVector3f(694.8630f, 488.5492f, 1776.4737f),
    FVector3f(679.8521f, 480.9668f, 1774.1958f)};
const int32 LodTrianglesByMaterial[LodCount][MaterialCount] = {
    {5984, 90832, 7428},
    {2400, 33308, 2260},
    {880, 8576, 408}};

const TCHAR* const MaterialNames[] = {
    TEXT("Bark"), TEXT("FrondLive"), TEXT("FrondDry")};
const TCHAR* const MaterialAssetNames[] = {
    TEXT("M_IPV5D_PalmHero_Bark"),
    TEXT("M_IPV5D_PalmHero_FrondLive"),
    TEXT("M_IPV5D_PalmHero_FrondDry")};
const TCHAR* const TextureRoles[] = {
    TEXT("Diffuse"), TEXT("NormalDX"), TEXT("Roughness"),
    TEXT("AmbientOcclusion")};
const TCHAR* const TextureRelativePaths[] = {
    TEXT("SourceTextures/palm_tree_bark_diff_2k.jpg"),
    TEXT("SourceTextures/palm_tree_bark_nor_dx_2k.jpg"),
    TEXT("SourceTextures/palm_tree_bark_rough_2k.jpg"),
    TEXT("SourceTextures/palm_tree_bark_ao_2k.jpg")};
const TCHAR* const TextureSha256[] = {
    TEXT("808B70B2B1F94D292B689E71CA761B99E2C1593706883495C041B6BF36B6BAB4"),
    TEXT("CE3EECA1617E23851DFD86E541ED0F1FDF2D88BC526EFA7FD0F35CF8E9EF58F0"),
    TEXT("25B7B82A0218DF0A3B778004855CF217665FC8B4E6874800819C296907FE7B23"),
    TEXT("8B720C90A32BE2665295B3F9D250282108937B3B4F58BF07F7E607341B92F963")};
const int64 TextureBytes[] = {7622678, 6833446, 5158632, 5242101};
const ETextureSourceFormat TextureSourceFormats[] = {
    TSF_BGRA8, TSF_BGRA8, TSF_BGRA8, TSF_G8};
const TCHAR* const TextureAssetNames[] = {
    TEXT("T_IPV5D_PalmHero_Bark_BaseColor"),
    TEXT("T_IPV5D_PalmHero_Bark_NormalDX"),
    TEXT("T_IPV5D_PalmHero_Bark_Roughness"),
    TEXT("T_IPV5D_PalmHero_Bark_AmbientOcclusion")};

struct FSourceFilePin
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourceFilePin AdditionalSourcePins[] = {
    {TEXT("Generated/M_IPV5D_PalmHeroCandidate.mtl"), 697,
     TEXT("57D3D23017EF5E8C2A70BA6E71509ED3D3B8C364B44034DF3D4FEF89589C4936")},
    {TEXT("Provenance/api/palm_tree_bark.files.json"), 34767,
     TEXT("3ED4BA21C0D223B3370902536AA1389CDAE90062CEA125E6A5BDB8468C56AADF")},
    {TEXT("Provenance/api/palm_tree_bark.info.json"), 1153,
     TEXT("D323726E1E96BDF6B6085B6D8912349E6B5992D890B770786084649FB18EC607")},
    {TEXT("Provenance/source-page/palm_tree_bark.html"), 192582,
     TEXT("1D28E0B9FC07D0868C8257436158CA05B8BDBFDF50B1240B018FE16876F5CB8C")},
    {TEXT("Provenance/license/polyhaven-license.html"), 72295,
     TEXT("6ED195C17E59E0404BCFC79AB1943C0B63307BB777D8A874B6650E048C6BCE80")}};

static_assert(UE_ARRAY_COUNT(LodRelativePaths) == LodCount);
static_assert(UE_ARRAY_COUNT(LodSha256) == LodCount);
static_assert(UE_ARRAY_COUNT(LodBytes) == LodCount);
static_assert(UE_ARRAY_COUNT(LodVertices) == LodCount);
static_assert(UE_ARRAY_COUNT(LodTriangles) == LodCount);
static_assert(UE_ARRAY_COUNT(LodRootVertices) == LodCount);
static_assert(UE_ARRAY_COUNT(LodSourceBoundsMinMeters) == LodCount);
static_assert(UE_ARRAY_COUNT(LodSourceBoundsMaxMeters) == LodCount);
static_assert(
    UE_ARRAY_COUNT(LodExpectedUnrealBoundsMinCentimetres) == LodCount);
static_assert(
    UE_ARRAY_COUNT(LodExpectedUnrealBoundsMaxCentimetres) == LodCount);
static_assert(UE_ARRAY_COUNT(MaterialNames) == MaterialCount);
static_assert(UE_ARRAY_COUNT(TextureRelativePaths) == TextureCount);
static_assert(UE_ARRAY_COUNT(TextureSourceFormats) == TextureCount);
static_assert(UE_ARRAY_COUNT(TextureAssetNames) == TextureCount);

struct FAdmission
{
    FString CandidateRoot;
    FString AcceptedR33Sha256;
    FString FutureAuthorizationSha256;
    TArray<TArray<uint8>> TextureSourceBytes;
};

bool IsSha256(const FString& Value)
{
    if (Value.Len() != 64)
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

FString SourcePath(const FString& CandidateRoot, const TCHAR* RelativePath)
{
    FString Result = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(CandidateRoot, RelativePath));
    FPaths::NormalizeFilename(Result);
    return Result;
}

bool LoadPinnedBytes(
    const FString& AbsolutePath,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    TArray<uint8>& OutBytes,
    FString& OutError)
{
    OutBytes.Reset();
    if (AbsolutePath.IsEmpty() || FPaths::IsRelative(AbsolutePath) ||
        !IsSha256(ExpectedSha256))
    {
        OutError = TEXT("Every PalmHero input requires an absolute path and explicit SHA-256 pin.");
        return false;
    }
    const int64 ActualBytes = IFileManager::Get().FileSize(*AbsolutePath);
    if (ActualBytes != ExpectedBytes || ActualBytes < 2 ||
        ActualBytes > MaximumInputBytes ||
        !FFileHelper::LoadFileToArray(OutBytes, *AbsolutePath) ||
        OutBytes.Num() != ActualBytes)
    {
        OutError = TEXT("A PalmHero source file is absent or its exact byte count drifted.");
        return false;
    }
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (!SHA256(
            OutBytes.GetData(),
            static_cast<size_t>(OutBytes.Num()),
            Digest) ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper() !=
            ExpectedSha256.ToUpper())
    {
        OutError = TEXT("A PalmHero source file failed its immutable SHA-256 pin.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ParsePinnedJson(
    const FString& AbsolutePath,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    TSharedPtr<FJsonObject>& OutRoot,
    FString& OutError)
{
    TArray<uint8> Bytes;
    OutRoot.Reset();
    if (!LoadPinnedBytes(
            AbsolutePath,
            ExpectedBytes,
            ExpectedSha256,
            Bytes,
            OutError))
    {
        return false;
    }
    FString Text;
    FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (Text.IsEmpty() || !FJsonSerializer::Deserialize(Reader, OutRoot) ||
        !OutRoot.IsValid())
    {
        OutError = TEXT("A hash-pinned PalmHero JSON document could not be decoded and parsed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ExactString(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const FString& Expected)
{
    FString Actual;
    return Object.IsValid() && Object->TryGetStringField(Field, Actual) &&
        Actual == Expected;
}

bool ExactBool(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    bool Expected)
{
    bool Actual = !Expected;
    return Object.IsValid() && Object->TryGetBoolField(Field, Actual) &&
        Actual == Expected;
}

bool ExactInteger(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    int32 Expected)
{
    double Actual = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(Field, Actual) &&
        Actual == static_cast<double>(Expected);
}

bool ExactNumber(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    double Expected)
{
    double Actual = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(Field, Actual) &&
        Actual == Expected;
}

bool ExactStringArray(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const TCHAR* const* Expected,
    int32 Count)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) ||
        !Values || Values->Num() != Count)
    {
        return false;
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if ((*Values)[Index]->AsString() != Expected[Index])
        {
            return false;
        }
    }
    return true;
}

bool ExactVector3(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const FVector3f& Expected)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    return Object.IsValid() && Object->TryGetArrayField(Field, Values) &&
        Values && Values->Num() == 3 &&
        FMath::IsNearlyEqual(
            (*Values)[0]->AsNumber(),
            static_cast<double>(Expected.X),
            ContractBoundsFloat32ToleranceMeters) &&
        FMath::IsNearlyEqual(
            (*Values)[1]->AsNumber(),
            static_cast<double>(Expected.Y),
            ContractBoundsFloat32ToleranceMeters) &&
        FMath::IsNearlyEqual(
            (*Values)[2]->AsNumber(),
            static_cast<double>(Expected.Z),
            ContractBoundsFloat32ToleranceMeters);
}

bool ValidateCandidateContract(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Lods = nullptr;
    const TSharedPtr<FJsonObject>* Collision = nullptr;
    const TSharedPtr<FJsonObject>* RootPlane = nullptr;
    const TSharedPtr<FJsonObject>* MaterialLibrary = nullptr;
    const TSharedPtr<FJsonObject>* Provenance = nullptr;
    if (!ExactString(Root, TEXT("assetId"), TEXT("PalmHeroCandidate")) ||
        !ExactString(Root, TEXT("schemaVersion"), TEXT("1.0.0")) ||
        !ExactString(
            Root,
            TEXT("status"),
            TEXT("UNADMITTED_SOURCE_ONLY_CANDIDATE")) ||
        !ExactString(
            Root,
            TEXT("coordinateSystem"),
            TEXT("right-handed Z-up")) ||
        !ExactString(Root, TEXT("linearUnit"), TEXT("meter")) ||
        !ExactStringArray(
            Root,
            TEXT("materialSlots"),
            MaterialNames,
            MaterialCount) ||
        !Root->TryGetObjectField(TEXT("collision"), Collision) ||
        !Collision || !Collision->IsValid() ||
        !ExactBool(*Collision, TEXT("authored"), false) ||
        !ExactBool(*Collision, TEXT("engineCollisionConfigured"), false) ||
        !Root->TryGetObjectField(TEXT("rootPlane"), RootPlane) ||
        !RootPlane || !RootPlane->IsValid() ||
        !ExactBool(*RootPlane, TEXT("allLodsVerified"), true) ||
        !ExactNumber(*RootPlane, TEXT("worldZ"), 0.0) ||
        !Root->TryGetObjectField(TEXT("materialLibrary"), MaterialLibrary) ||
        !MaterialLibrary || !MaterialLibrary->IsValid() ||
        !ExactString(
            *MaterialLibrary,
            TEXT("path"),
            TEXT("Generated/M_IPV5D_PalmHeroCandidate.mtl")) ||
        !ExactInteger(*MaterialLibrary, TEXT("bytes"), 697) ||
        !ExactString(
            *MaterialLibrary,
            TEXT("sha256"),
            TEXT("57d3d23017ef5e8c2a70ba6e71509ed3d3b8c364b44034df3d4fef89589c4936")) ||
        !Root->TryGetObjectField(TEXT("provenanceManifest"), Provenance) ||
        !Provenance || !Provenance->IsValid() ||
        !ExactString(
            *Provenance,
            TEXT("path"),
            TEXT("Provenance/palm_tree_bark.provenance.json")) ||
        !ExactInteger(
            *Provenance,
            TEXT("bytes"),
            static_cast<int32>(ProvenanceManifestBytes)) ||
        !ExactString(
            *Provenance,
            TEXT("sha256"),
            ProvenanceManifestSha256.ToLower()) ||
        !Root->TryGetArrayField(TEXT("lods"), Lods) || !Lods ||
        Lods->Num() != LodCount)
    {
        OutError = TEXT("PalmHero candidate contract identity, source-only status, material route, or provenance drifted.");
        return false;
    }

    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        const TSharedPtr<FJsonObject> Row = (*Lods)[Lod]->AsObject();
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        const TSharedPtr<FJsonObject>* RootContact = nullptr;
        const TSharedPtr<FJsonObject>* Bounds = nullptr;
        const TSharedPtr<FJsonObject>* TriangleRoute = nullptr;
        if (!Row.IsValid() ||
            !ExactInteger(Row, TEXT("vertices"), LodVertices[Lod]) ||
            !ExactInteger(Row, TEXT("triangles"), LodTriangles[Lod]) ||
            !ExactStringArray(
                Row,
                TEXT("materialSlotsInFirstUseOrder"),
                MaterialNames,
                MaterialCount) ||
            !Row->TryGetObjectField(TEXT("obj"), Obj) || !Obj ||
            !Obj->IsValid() ||
            !ExactString(*Obj, TEXT("path"), LodRelativePaths[Lod]) ||
            !ExactInteger(
                *Obj,
                TEXT("bytes"),
                static_cast<int32>(LodBytes[Lod])) ||
            !ExactString(
                *Obj,
                TEXT("sha256"),
                FString(LodSha256[Lod]).ToLower()) ||
            !Row->TryGetObjectField(TEXT("rootContact"), RootContact) ||
            !RootContact || !RootContact->IsValid() ||
            !ExactBool(*RootContact, TEXT("pass"), true) ||
            !ExactNumber(*RootContact, TEXT("minimumZ"), 0.0) ||
            !ExactInteger(
                *RootContact,
                TEXT("verticesExactlyOnZ0"),
                LodRootVertices[Lod]) ||
            !Row->TryGetObjectField(TEXT("boundsMeters"), Bounds) ||
            !Bounds || !Bounds->IsValid() ||
            !ExactVector3(*Bounds, TEXT("min"), LodSourceBoundsMinMeters[Lod]) ||
            !ExactVector3(*Bounds, TEXT("max"), LodSourceBoundsMaxMeters[Lod]) ||
            !Row->TryGetObjectField(
                TEXT("trianglesByMaterial"),
                TriangleRoute) ||
            !TriangleRoute || !TriangleRoute->IsValid())
        {
            OutError = FString::Printf(
                TEXT("PalmHero contract LOD%d lost its exact OBJ, root, bounds, slot, or count declaration."),
                Lod);
            return false;
        }
        for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
        {
            if (!ExactInteger(
                    *TriangleRoute,
                    MaterialNames[Slot],
                    LodTrianglesByMaterial[Lod][Slot]))
            {
                OutError = FString::Printf(
                    TEXT("PalmHero contract LOD%d material triangle route drifted."),
                    Lod);
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateProvenanceManifest(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Asset = nullptr;
    const TSharedPtr<FJsonObject>* License = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Maps = nullptr;
    if (!ExactString(Root, TEXT("schemaVersion"), TEXT("1.0.0")) ||
        !Root->TryGetObjectField(TEXT("asset"), Asset) || !Asset ||
        !Asset->IsValid() ||
        !ExactString(*Asset, TEXT("provider"), TEXT("Poly Haven")) ||
        !ExactString(
            *Asset,
            TEXT("providerAssetId"),
            TEXT("palm_tree_bark")) ||
        !Root->TryGetObjectField(TEXT("license"), License) || !License ||
        !License->IsValid() ||
        !ExactString(
            *License,
            TEXT("providerDeclaration"),
            TEXT("CC0 1.0 Universal for Poly Haven assets")) ||
        !Root->TryGetArrayField(TEXT("textureMaps"), Maps) || !Maps ||
        Maps->Num() != TextureCount)
    {
        OutError = TEXT("PalmHero provenance identity, license boundary, or texture roster drifted.");
        return false;
    }
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        const TSharedPtr<FJsonObject> Map = (*Maps)[Index]->AsObject();
        const FString ExpectedColor = Index == 0
            ? TEXT("sRGB")
            : TEXT("linear/non-color");
        if (!Map.IsValid() ||
            !ExactString(Map, TEXT("role"), TextureRoles[Index]) ||
            !ExactString(Map, TEXT("path"), TextureRelativePaths[Index]) ||
            !ExactString(Map, TEXT("format"), TEXT("JPEG")) ||
            !ExactString(Map, TEXT("resolution"), TEXT("2K")) ||
            !ExactString(Map, TEXT("colorSpace"), ExpectedColor) ||
            !ExactInteger(
                Map,
                TEXT("actualBytes"),
                static_cast<int32>(TextureBytes[Index])) ||
            !ExactString(
                Map,
                TEXT("actualSha256"),
                FString(TextureSha256[Index]).ToLower()) ||
            !ExactInteger(
                Map,
                TEXT("publishedBytes"),
                static_cast<int32>(TextureBytes[Index])))
        {
            OutError = FString::Printf(
                TEXT("PalmHero provenance texture row %d drifted."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateMtl(const FString& CandidateRoot, FString& OutError)
{
    TArray<uint8> Bytes;
    if (!LoadPinnedBytes(
            SourcePath(
                CandidateRoot,
                TEXT("Generated/M_IPV5D_PalmHeroCandidate.mtl")),
            697,
            TEXT("57D3D23017EF5E8C2A70BA6E71509ED3D3B8C364B44034DF3D4FEF89589C4936"),
            Bytes,
            OutError))
    {
        return false;
    }
    FString Text;
    FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, true);
    TArray<FString> MaterialOrder;
    for (const FString& Line : Lines)
    {
        if (Line.StartsWith(TEXT("newmtl ")))
        {
            MaterialOrder.Add(Line.RightChop(7));
        }
    }
    if (MaterialOrder.Num() != MaterialCount ||
        MaterialOrder[0] != MaterialNames[0] ||
        MaterialOrder[1] != MaterialNames[1] ||
        MaterialOrder[2] != MaterialNames[2] ||
        !Text.Contains(TEXT("map_Kd ../SourceTextures/palm_tree_bark_diff_2k.jpg")) ||
        !Text.Contains(TEXT("map_Bump -bm 1.000000 ../SourceTextures/palm_tree_bark_nor_dx_2k.jpg")) ||
        !Text.Contains(TEXT("map_Pr ../SourceTextures/palm_tree_bark_rough_2k.jpg")) ||
        !Text.Contains(TEXT("map_Ka ../SourceTextures/palm_tree_bark_ao_2k.jpg")))
    {
        OutError = TEXT("PalmHero MTL lost the exact Bark/FrondLive/FrondDry route.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateObj(
    const FString& CandidateRoot,
    int32 Lod,
    FString& OutError)
{
    TArray<uint8> Bytes;
    if (Lod < 0 || Lod >= LodCount ||
        !LoadPinnedBytes(
            SourcePath(CandidateRoot, LodRelativePaths[Lod]),
            LodBytes[Lod],
            LodSha256[Lod],
            Bytes,
            OutError))
    {
        return false;
    }
    FString Text;
    FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, true);
    int32 Vertices = 0;
    int32 Uvs = 0;
    int32 Normals = 0;
    int32 Triangles = 0;
    int32 RootVertices = 0;
    int32 ObjectDeclarations = 0;
    int32 MaterialIndex = INDEX_NONE;
    int32 UseMaterialCount = 0;
    int32 TrianglesByMaterial[MaterialCount] = {0, 0, 0};
    FBox3d Bounds(ForceInit);
    bool bExactMtlReference = false;

    for (const FString& Line : Lines)
    {
        if (Line.StartsWith(TEXT("v ")))
        {
            TArray<FString> Values;
            Line.ParseIntoArrayWS(Values);
            if (Values.Num() != 4)
            {
                OutError = TEXT("PalmHero OBJ contains a non-XYZ vertex record.");
                return false;
            }
            const FVector3d Position(
                FCString::Atod(*Values[1]),
                FCString::Atod(*Values[2]),
                FCString::Atod(*Values[3]));
            if (!FMath::IsFinite(Position.X) ||
                !FMath::IsFinite(Position.Y) ||
                !FMath::IsFinite(Position.Z))
            {
                OutError = TEXT("PalmHero OBJ contains a non-finite vertex.");
                return false;
            }
            Bounds += Position;
            RootVertices += Position.Z == 0.0 ? 1 : 0;
            ++Vertices;
        }
        else if (Line.StartsWith(TEXT("vt ")))
        {
            ++Uvs;
        }
        else if (Line.StartsWith(TEXT("vn ")))
        {
            ++Normals;
        }
        else if (Line.StartsWith(TEXT("f ")))
        {
            TArray<FString> Values;
            Line.ParseIntoArrayWS(Values);
            if (Values.Num() != 4 || MaterialIndex == INDEX_NONE)
            {
                OutError = TEXT("PalmHero OBJ contains a non-triangle or unmaterialed face.");
                return false;
            }
            ++Triangles;
            ++TrianglesByMaterial[MaterialIndex];
        }
        else if (Line.StartsWith(TEXT("usemtl ")))
        {
            const FString Name = Line.RightChop(7);
            MaterialIndex = INDEX_NONE;
            for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
            {
                if (Name == MaterialNames[Slot])
                {
                    MaterialIndex = Slot;
                    break;
                }
            }
            if (MaterialIndex != UseMaterialCount ||
                UseMaterialCount >= MaterialCount)
            {
                OutError = TEXT("PalmHero OBJ material first-use order is not Bark/FrondLive/FrondDry.");
                return false;
            }
            ++UseMaterialCount;
        }
        else if (Line.StartsWith(TEXT("mtllib ")))
        {
            bExactMtlReference =
                Line == TEXT("mtllib M_IPV5D_PalmHeroCandidate.mtl") &&
                !bExactMtlReference;
        }
        else if (Line.StartsWith(TEXT("o ")))
        {
            ++ObjectDeclarations;
            if (Line != FString::Printf(
                    TEXT("o SM_IPV5D_PalmHeroCandidate_LOD%d"),
                    Lod))
            {
                OutError = TEXT("PalmHero OBJ object identity drifted.");
                return false;
            }
        }
    }

    if (!Bounds.IsValid || !bExactMtlReference ||
        ObjectDeclarations != 1 || UseMaterialCount != MaterialCount ||
        Vertices != LodVertices[Lod] || Uvs != LodVertices[Lod] ||
        Normals != LodVertices[Lod] || Triangles != LodTriangles[Lod] ||
        RootVertices != LodRootVertices[Lod] || Bounds.Min.Z != 0.0 ||
        !FVector3d(LodSourceBoundsMinMeters[Lod]).Equals(Bounds.Min, 0.000001) ||
        !FVector3d(LodSourceBoundsMaxMeters[Lod]).Equals(Bounds.Max, 0.000001))
    {
        OutError = FString::Printf(
            TEXT("PalmHero OBJ LOD%d failed its exact root, bounds, vertex/normal/UV, triangle, material, or MTL contract."),
            Lod);
        return false;
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        if (TrianglesByMaterial[Slot] !=
            LodTrianglesByMaterial[Lod][Slot])
        {
            OutError = FString::Printf(
                TEXT("PalmHero OBJ LOD%d material triangle count drifted at slot %d."),
                Lod,
                Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateSourceRoster(
    const FString& CandidateRoot,
    TArray<TArray<uint8>>& OutTextureSourceBytes,
    FString& OutError)
{
    OutTextureSourceBytes.Reset();
    if (CandidateRoot.IsEmpty() || FPaths::IsRelative(CandidateRoot))
    {
        OutError = TEXT("PalmHero candidate root must be an explicit absolute directory.");
        return false;
    }
    TSharedPtr<FJsonObject> Candidate;
    TSharedPtr<FJsonObject> Provenance;
    if (!ParsePinnedJson(
            SourcePath(CandidateRoot, TEXT("palm_hero_candidate.contract.json")),
            CandidateContractBytes,
            CandidateContractSha256,
            Candidate,
            OutError) ||
        !ValidateCandidateContract(Candidate, OutError) ||
        !ParsePinnedJson(
            SourcePath(
                CandidateRoot,
                TEXT("Provenance/palm_tree_bark.provenance.json")),
            ProvenanceManifestBytes,
            ProvenanceManifestSha256,
            Provenance,
            OutError) ||
        !ValidateProvenanceManifest(Provenance, OutError) ||
        !ValidateMtl(CandidateRoot, OutError))
    {
        return false;
    }
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        if (!ValidateObj(CandidateRoot, Lod, OutError))
        {
            return false;
        }
    }
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        TArray<uint8> Bytes;
        if (!LoadPinnedBytes(
                SourcePath(CandidateRoot, TextureRelativePaths[Index]),
                TextureBytes[Index],
                TextureSha256[Index],
                Bytes,
                OutError))
        {
            return false;
        }
        OutTextureSourceBytes.Add(MoveTemp(Bytes));
    }
    for (const FSourceFilePin& Pin : AdditionalSourcePins)
    {
        if (FString(Pin.RelativePath).EndsWith(TEXT(".mtl")))
        {
            continue;
        }
        TArray<uint8> Bytes;
        if (!LoadPinnedBytes(
                SourcePath(CandidateRoot, Pin.RelativePath),
                Pin.Bytes,
                Pin.Sha256,
                Bytes,
                OutError))
        {
            return false;
        }
    }
    if (OutTextureSourceBytes.Num() != TextureCount)
    {
        OutError = TEXT("PalmHero admission did not retain all four exact pinned texture byte buffers.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAcceptedR33Receipt(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Commit = nullptr;
    if (!ExactString(Root, TEXT("Schema"), AcceptedR33Schema) ||
        !ExactString(Root, TEXT("Status"), TEXT("COMMITTED")) ||
        !ExactInteger(Root, TEXT("ExactPoseCount"), 4) ||
        !ExactInteger(Root, TEXT("ExactPresentationStateCount"), 2) ||
        !ExactInteger(Root, TEXT("ExactCoreCaptureCount"), 8) ||
        !ExactBool(Root, TEXT("MechanicalCaptureValidationPassed"), true) ||
        !ExactBool(Root, TEXT("ExplicitHumanReviewAcceptance"), true) ||
        !ExactBool(Root, TEXT("ConfirmedEightImagesReviewed"), true) ||
        !ExactBool(Root, TEXT("HumanVisualReviewAttested"), true) ||
        !ExactBool(Root, TEXT("AutomaticVisualAcceptanceAllowed"), false) ||
        !ExactBool(Root, TEXT("VisualReviewRequired"), false) ||
        !ExactBool(Root, TEXT("VisualReviewAccepted"), true) ||
        !ExactBool(Root, TEXT("R33CesiumWorldTerrainVisualQaAccepted"), true) ||
        !ExactBool(Root, TEXT("GooglePrimaryVisualQaAccepted"), true) ||
        !ExactBool(Root, TEXT("CwtPresentedVisualQaAccepted"), true) ||
        !ExactBool(Root, TEXT("TreeMaterialResponseV3Preserved"), true) ||
        !ExactBool(Root, TEXT("MapModifiedByCapture"), false) ||
        !ExactBool(
            Root,
            TEXT("SimulationCollisionNavigationSensorRfModified"),
            false) ||
        !Root->TryGetObjectField(TEXT("R33CommitAdmission"), Commit) ||
        !Commit || !Commit->IsValid() ||
        !ExactString(*Commit, TEXT("Schema"), R33TransactionSchema) ||
        !ExactString(*Commit, TEXT("Status"), TEXT("COMMITTED")))
    {
        OutError = TEXT("R33 receipt is not the exact human-accepted no-mutation eight-image receipt.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateFutureAuthorization(
    const TSharedPtr<FJsonObject>& Root,
    const FString& AcceptedR33Sha256,
    FString& OutError)
{
    const TCHAR* const ExactFields[] = {
        TEXT("Schema"), TEXT("Status"), TEXT("Operation"),
        TEXT("TargetMapPackage"), TEXT("AcceptedR33ReceiptSha256"),
        TEXT("PalmHeroContractSha256"), TEXT("PalmHeroProvenanceSha256"),
        TEXT("ExactLodCount"), TEXT("ExactOutputAssetCount"),
        TEXT("UnnumberedSuccessor"), TEXT("ExplicitExecutionAuthorized"),
        TEXT("AssetsOnlyEndpoint"), TEXT("OptionalPalmSourceOnly"),
        TEXT("ExistingPalmFallbackPreserved"),
        TEXT("MapMutationAuthorized"),
        TEXT("SourceMeshMutationAuthorized"),
        TEXT("SourceTransformMutationAuthorized"),
        TEXT("CollisionNavigationLosRfSensorTerrainMutationAuthorized"),
        TEXT("NativeWriteOrUnrealLaunchPerformed")};
    if (!Root.IsValid() ||
        Root->Values.Num() != UE_ARRAY_COUNT(ExactFields))
    {
        OutError = TEXT("Future PalmHero authorization must contain exactly the narrow trusted field roster and no extensions.");
        return false;
    }
    for (const TCHAR* Field : ExactFields)
    {
        if (!Root->HasField(Field))
        {
            OutError = TEXT("Future PalmHero authorization omitted a field from the exact narrow roster.");
            return false;
        }
    }
    if (!ExactString(Root, TEXT("Schema"), FutureAuthorizationSchema) ||
        !ExactString(
            Root,
            TEXT("Status"),
            TEXT("AUTHORIZED_NOT_EXECUTED")) ||
        !ExactString(Root, TEXT("Operation"), FutureOperation) ||
        !ExactString(Root, TEXT("TargetMapPackage"), TargetMapPackage) ||
        !ExactString(
            Root,
            TEXT("AcceptedR33ReceiptSha256"),
            AcceptedR33Sha256.ToUpper()) ||
        !ExactString(
            Root,
            TEXT("PalmHeroContractSha256"),
            CandidateContractSha256) ||
        !ExactString(
            Root,
            TEXT("PalmHeroProvenanceSha256"),
            ProvenanceManifestSha256) ||
        !ExactInteger(Root, TEXT("ExactLodCount"), LodCount) ||
        !ExactInteger(
            Root,
            TEXT("ExactOutputAssetCount"),
            OutputAssetCount) ||
        !ExactBool(Root, TEXT("UnnumberedSuccessor"), true) ||
        !ExactBool(Root, TEXT("ExplicitExecutionAuthorized"), true) ||
        !ExactBool(Root, TEXT("AssetsOnlyEndpoint"), true) ||
        !ExactBool(Root, TEXT("OptionalPalmSourceOnly"), true) ||
        !ExactBool(Root, TEXT("ExistingPalmFallbackPreserved"), true) ||
        !ExactBool(Root, TEXT("MapMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceMeshMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceTransformMutationAuthorized"), false) ||
        !ExactBool(
            Root,
            TEXT("CollisionNavigationLosRfSensorTerrainMutationAuthorized"),
            false) ||
        !ExactBool(
            Root,
            TEXT("NativeWriteOrUnrealLaunchPerformed"),
            false))
    {
        OutError = TEXT("Future PalmHero authorization is absent, unpinned, non-explicit, or broader than isolated optional-source asset materialization.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool BuildAdmission(
    const FString& CandidateRoot,
    const FString& AcceptedR33ReceiptPath,
    const FString& ExpectedAcceptedR33ReceiptSha256,
    const FString& FutureTransactionAuthorizationPath,
    const FString& ExpectedFutureTransactionAuthorizationSha256,
    bool bRequireCompiledTrustAnchors,
    FAdmission& OutAdmission,
    FString& OutError)
{
    OutAdmission = FAdmission{};
    if (CandidateRoot.IsEmpty() || FPaths::IsRelative(CandidateRoot) ||
        AcceptedR33ReceiptPath.IsEmpty() ||
        FPaths::IsRelative(AcceptedR33ReceiptPath) ||
        FutureTransactionAuthorizationPath.IsEmpty() ||
        FPaths::IsRelative(FutureTransactionAuthorizationPath) ||
        !IsSha256(ExpectedAcceptedR33ReceiptSha256) ||
        !IsSha256(ExpectedFutureTransactionAuthorizationSha256))
    {
        OutError = TEXT("PalmHero inspection requires an absolute candidate root, two absolute receipt paths, and two explicit SHA-256 receipt pins.");
        return false;
    }
    if (bRequireCompiledTrustAnchors)
    {
        if (!IsSha256(TrustedAcceptedR33ReceiptSha256) ||
            !IsSha256(TrustedFutureAuthorizationSha256) ||
            TrustedAcceptedR33ReceiptSha256.Equals(
                TrustedFutureAuthorizationSha256,
                ESearchCase::IgnoreCase))
        {
            OutError = TEXT("PalmHero execution is intentionally unreachable: compiled trusted receipt anchors are unset and require a separately reviewed source change plus recompile.");
            return false;
        }
        if (!ExpectedAcceptedR33ReceiptSha256.Equals(
                TrustedAcceptedR33ReceiptSha256,
                ESearchCase::IgnoreCase) ||
            !ExpectedFutureTransactionAuthorizationSha256.Equals(
                TrustedFutureAuthorizationSha256,
                ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Caller-supplied PalmHero receipt hashes do not match the separately compiled trusted anchors.");
            return false;
        }
    }
    FString FullRoot = FPaths::ConvertRelativePathToFull(CandidateRoot);
    FString FullR33 = FPaths::ConvertRelativePathToFull(
        AcceptedR33ReceiptPath);
    FString FullAuthorization = FPaths::ConvertRelativePathToFull(
        FutureTransactionAuthorizationPath);
    FPaths::NormalizeDirectoryName(FullRoot);
    FPaths::NormalizeFilename(FullR33);
    FPaths::NormalizeFilename(FullAuthorization);
    if (FPaths::IsSamePath(FullR33, FullAuthorization) ||
        ExpectedAcceptedR33ReceiptSha256.Equals(
            ExpectedFutureTransactionAuthorizationSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("PalmHero inspection requires independent R33 and future-authorization receipts with different SHA-256 pins.");
        return false;
    }
    TSharedPtr<FJsonObject> AcceptedR33;
    TSharedPtr<FJsonObject> FutureAuthorization;
    if (!ValidateSourceRoster(
            FullRoot,
            OutAdmission.TextureSourceBytes,
            OutError) ||
        !ParsePinnedJson(
            FullR33,
            IFileManager::Get().FileSize(*FullR33),
            ExpectedAcceptedR33ReceiptSha256,
            AcceptedR33,
            OutError) ||
        !ValidateAcceptedR33Receipt(AcceptedR33, OutError) ||
        !ParsePinnedJson(
            FullAuthorization,
            IFileManager::Get().FileSize(
                *FullAuthorization),
            ExpectedFutureTransactionAuthorizationSha256,
            FutureAuthorization,
            OutError) ||
        !ValidateFutureAuthorization(
            FutureAuthorization,
            ExpectedAcceptedR33ReceiptSha256,
            OutError))
    {
        return false;
    }
    OutAdmission.CandidateRoot = FullRoot;
    OutAdmission.AcceptedR33Sha256 =
        ExpectedAcceptedR33ReceiptSha256.ToUpper();
    OutAdmission.FutureAuthorizationSha256 =
        ExpectedFutureTransactionAuthorizationSha256.ToUpper();
    OutError.Reset();
    return true;
}

template <typename TObjectType>
TObjectType* LoadExact(const FString& ObjectPath)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
}

FString MaterialPackagePath(int32 Slot)
{
    return Slot >= 0 && Slot < MaterialCount
        ? FPaths::Combine(OutputRoot, TEXT("Materials"), MaterialAssetNames[Slot])
        : FString();
}

FString MaterialObjectPath(int32 Slot)
{
    return Slot >= 0 && Slot < MaterialCount
        ? MaterialPackagePath(Slot) + TEXT(".") + MaterialAssetNames[Slot]
        : FString();
}

FString TexturePackagePath(int32 Index)
{
    return Index >= 0 && Index < TextureCount
        ? FPaths::Combine(OutputRoot, TEXT("Textures"), TextureAssetNames[Index])
        : FString();
}

FString TextureObjectPath(int32 Index)
{
    return Index >= 0 && Index < TextureCount
        ? TexturePackagePath(Index) + TEXT(".") + TextureAssetNames[Index]
        : FString();
}

const FString FinalMeshPackagePath =
    FPaths::Combine(OutputRoot, TEXT("Meshes/SM_IPV5D_PalmHero"));
const FString FinalMeshObjectPath =
    FinalMeshPackagePath + TEXT(".SM_IPV5D_PalmHero");

void GetOutputPackagePaths(TArray<FString>& OutPaths)
{
    OutPaths.Reset();
    OutPaths.Add(FinalMeshPackagePath);
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        OutPaths.Add(TexturePackagePath(Index));
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        OutPaths.Add(MaterialPackagePath(Slot));
    }
}

void GetOutputObjectPaths(TArray<FString>& OutPaths);

bool IsPackageWithinNamespace(
    const FString& PackageName,
    const FString& NamespaceRoot)
{
    return PackageName == NamespaceRoot ||
        PackageName.StartsWith(NamespaceRoot + TEXT("/"));
}

void CollectNamespaceState(
    const FString& NamespaceRoot,
    TSet<FString>& OutObjectPaths,
    TSet<FString>& OutPackagePaths)
{
    OutObjectPaths.Reset();
    OutPackagePaths.Reset();

    TArray<FAssetData> RegistryAssets;
    FAssetRegistryModule::GetRegistry().GetAssetsByPath(
        FName(NamespaceRoot),
        RegistryAssets,
        true,
        false);
    for (const FAssetData& Asset : RegistryAssets)
    {
        const FString PackageName = Asset.PackageName.ToString();
        if (IsPackageWithinNamespace(PackageName, NamespaceRoot))
        {
            OutObjectPaths.Add(Asset.GetObjectPathString());
            OutPackagePaths.Add(PackageName);
        }
    }

    // Unsaved imports may not yet have a stable registry view. Include every
    // live top-level asset and even empty loaded package beneath the isolated
    // roots so an in-memory extra cannot evade a post-create or rollback scan.
    for (TObjectIterator<UObject> It; It; ++It)
    {
        UObject* Object = *It;
        UPackage* Package = Object ? Object->GetOutermost() : nullptr;
        if (Object && IsValid(Object) && Object->IsAsset() && Package &&
            IsPackageWithinNamespace(Package->GetName(), NamespaceRoot))
        {
            OutObjectPaths.Add(Object->GetPathName());
            OutPackagePaths.Add(Package->GetName());
        }
    }
    for (TObjectIterator<UPackage> It; It; ++It)
    {
        UPackage* Package = *It;
        if (Package && IsValid(Package) &&
            IsPackageWithinNamespace(Package->GetName(), NamespaceRoot))
        {
            OutPackagePaths.Add(Package->GetName());
        }
    }
}

bool NamespaceContainsOnlyExpectedAssets(
    const FString& NamespaceRoot,
    const TArray<FString>& ExpectedObjectPaths,
    FString& OutError)
{
    TSet<FString> ExpectedObjects;
    TSet<FString> ExpectedPackages;
    for (const FString& ObjectPath : ExpectedObjectPaths)
    {
        ExpectedObjects.Add(ObjectPath);
        ExpectedPackages.Add(
            FPackageName::ObjectPathToPackageName(ObjectPath));
    }
    TSet<FString> ActualObjects;
    TSet<FString> ActualPackages;
    CollectNamespaceState(
        NamespaceRoot,
        ActualObjects,
        ActualPackages);
    for (const FString& ObjectPath : ActualObjects)
    {
        if (!ExpectedObjects.Contains(ObjectPath))
        {
            OutError = FString::Printf(
                TEXT("PalmHero isolated namespace '%s' contains uncontracted asset '%s'."),
                *NamespaceRoot,
                *ObjectPath);
            return false;
        }
    }
    for (const FString& PackagePath : ActualPackages)
    {
        if (!ExpectedPackages.Contains(PackagePath))
        {
            OutError = FString::Printf(
                TEXT("PalmHero isolated namespace '%s' contains uncontracted loaded package '%s'."),
                *NamespaceRoot,
                *PackagePath);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool NamespaceMatchesExactAssets(
    const FString& NamespaceRoot,
    const TArray<FString>& ExpectedObjectPaths,
    FString& OutError)
{
    if (!NamespaceContainsOnlyExpectedAssets(
            NamespaceRoot,
            ExpectedObjectPaths,
            OutError))
    {
        return false;
    }
    TSet<FString> ActualObjects;
    TSet<FString> ActualPackages;
    CollectNamespaceState(
        NamespaceRoot,
        ActualObjects,
        ActualPackages);
    if (ActualObjects.Num() != ExpectedObjectPaths.Num())
    {
        OutError = FString::Printf(
            TEXT("PalmHero isolated namespace '%s' has %d assets; expected exactly %d."),
            *NamespaceRoot,
            ActualObjects.Num(),
            ExpectedObjectPaths.Num());
        return false;
    }
    for (const FString& ExpectedObjectPath : ExpectedObjectPaths)
    {
        if (!ActualObjects.Contains(ExpectedObjectPath))
        {
            OutError = FString::Printf(
                TEXT("PalmHero isolated namespace '%s' is missing exact asset '%s'."),
                *NamespaceRoot,
                *ExpectedObjectPath);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

int32 ExistingOutputPackageCount()
{
    TArray<FString> Paths;
    GetOutputPackagePaths(Paths);
    int32 Count = 0;
    for (const FString& Path : Paths)
    {
        if (FindPackage(nullptr, *Path) || FPackageName::DoesPackageExist(Path))
        {
            ++Count;
        }
    }
    return Count;
}

bool HasUnexpectedOutputNamespaceAssets(FString& OutError)
{
    TArray<FString> ExpectedPaths;
    GetOutputObjectPaths(ExpectedPaths);
    return !NamespaceContainsOnlyExpectedAssets(
        OutputRoot,
        ExpectedPaths,
        OutError);
}

int32 MaterialSlotIndex(const FName& Name)
{
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        if (Name == FName(MaterialNames[Slot]))
        {
            return Slot;
        }
    }
    return INDEX_NONE;
}

bool ValidateMeshDescription(
    const FMeshDescription& Description,
    int32 Lod,
    FString& OutError)
{
    if (Lod < 0 || Lod >= LodCount || Description.NeedsCompact() ||
        Description.Vertices().Num() != LodVertices[Lod] ||
        Description.Triangles().Num() != LodTriangles[Lod] ||
        Description.PolygonGroups().Num() != MaterialCount)
    {
        OutError = TEXT("PalmHero MeshDescription lost compact exact LOD topology or polygon groups.");
        return false;
    }
    const FStaticMeshConstAttributes Attributes(Description);
    const TVertexAttributesConstRef<FVector3f> Positions =
        Attributes.GetVertexPositions();
    const TVertexInstanceAttributesConstRef<FVector2f> Uvs =
        Attributes.GetVertexInstanceUVs();
    const TVertexInstanceAttributesConstRef<FVector3f> Normals =
        Attributes.GetVertexInstanceNormals();
    const TPolygonGroupAttributesConstRef<FName> GroupNames =
        Attributes.GetPolygonGroupMaterialSlotNames();
    if (Uvs.GetNumChannels() != 1)
    {
        OutError = TEXT("PalmHero imported MeshDescription must retain exactly source UV0.");
        return false;
    }
    FBox3f Bounds(ForceInit);
    int32 RootVertices = 0;
    for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
    {
        const FVector3f Position = Positions.Get(Vertex);
        if (Position.ContainsNaN())
        {
            OutError = TEXT("PalmHero imported MeshDescription contains a non-finite position.");
            return false;
        }
        Bounds += Position;
        RootVertices += Position.Z == 0.0f ? 1 : 0;
    }
    for (const FVertexInstanceID Instance :
         Description.VertexInstances().GetElementIDs())
    {
        if (Uvs.Get(Instance, 0).ContainsNaN() ||
            Normals.Get(Instance).ContainsNaN())
        {
            OutError = TEXT("PalmHero imported MeshDescription contains non-finite UV0 or normal data.");
            return false;
        }
    }
    int32 TrianglesByMaterial[MaterialCount] = {0, 0, 0};
    TSet<int32> SeenSlots;
    for (const FPolygonGroupID Group :
         Description.PolygonGroups().GetElementIDs())
    {
        const int32 Slot = MaterialSlotIndex(GroupNames.Get(Group));
        if (Slot == INDEX_NONE || SeenSlots.Contains(Slot))
        {
            OutError = TEXT("PalmHero MeshDescription polygon groups lost unique Bark/FrondLive/FrondDry semantics.");
            return false;
        }
        SeenSlots.Add(Slot);
    }
    for (const FTriangleID Triangle :
         Description.Triangles().GetElementIDs())
    {
        const int32 Slot = MaterialSlotIndex(
            GroupNames.Get(Description.GetTrianglePolygonGroup(Triangle)));
        if (Slot == INDEX_NONE)
        {
            OutError = TEXT("PalmHero MeshDescription triangle references an unknown material group.");
            return false;
        }
        ++TrianglesByMaterial[Slot];
    }
    if (!Bounds.IsValid || RootVertices != LodRootVertices[Lod] ||
        !Bounds.Min.Equals(
            LodExpectedUnrealBoundsMinCentimetres[Lod],
            BoundsToleranceCentimetres) ||
        !Bounds.Max.Equals(
            LodExpectedUnrealBoundsMaxCentimetres[Lod],
            BoundsToleranceCentimetres))
    {
        OutError = FString::Printf(
            TEXT("PalmHero imported LOD%d lost its exact 100-centimetre scale, root, or bounds."),
            Lod);
        return false;
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        if (TrianglesByMaterial[Slot] !=
            LodTrianglesByMaterial[Lod][Slot])
        {
            OutError = FString::Printf(
                TEXT("PalmHero imported LOD%d triangle/material route drifted at slot %d."),
                Lod,
                Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateMaterialSlots(
    const UStaticMesh* Mesh,
    const TArray<UMaterial*>& Materials,
    bool bRequireBindings,
    FString& OutError)
{
    if (!Mesh || Mesh->GetStaticMaterials().Num() != MaterialCount ||
        (bRequireBindings && Materials.Num() != MaterialCount))
    {
        OutError = TEXT("PalmHero mesh lost its exact three material slots.");
        return false;
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        const FStaticMaterial& Binding = Mesh->GetStaticMaterials()[Slot];
        if (Binding.MaterialSlotName != FName(MaterialNames[Slot]) ||
            Binding.ImportedMaterialSlotName !=
                FName(MaterialNames[Slot]) ||
            (bRequireBindings &&
             (!Materials[Slot] ||
              Mesh->GetMaterial(Slot) != Materials[Slot] ||
              Materials[Slot]->GetPathName() != MaterialObjectPath(Slot))))
        {
            OutError = TEXT("PalmHero mesh Bark/FrondLive/FrondDry slot order or binding drifted.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ExactAssetMetadata(
    const UObject* Asset,
    const TCHAR* Key,
    const FString& Expected,
    FString& OutError)
{
    UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
    // HasMetaData is checked first so validating an old unbound package never
    // creates metadata as a side effect merely to reject it.
    UMetaData* Metadata = Package && Package->HasMetaData()
        ? Package->GetMetaData()
        : nullptr;
    if (!Asset || !Metadata || Metadata->GetValue(Asset, Key) != Expected)
    {
        OutError = FString::Printf(
            TEXT("PalmHero asset '%s' lost exact metadata binding '%s'."),
            Asset ? *Asset->GetPathName() : TEXT("<null>"),
            Key);
        return false;
    }
    OutError.Reset();
    return true;
}

bool WriteCommonOutputMetadata(
    UObject* Asset,
    const FAdmission& Admission,
    const TCHAR* Authority,
    FString& OutError)
{
    UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
    UMetaData* Metadata = Package ? Package->GetMetaData() : nullptr;
    if (!Asset || !Metadata)
    {
        OutError = TEXT("PalmHero output requires writable package metadata.");
        return false;
    }
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_PalmHeroCandidateContractSha256"),
        *CandidateContractSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_PalmHeroProvenanceSha256"),
        *ProvenanceManifestSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_PalmHeroAcceptedR33ReceiptSha256"),
        *Admission.AcceptedR33Sha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_PalmHeroFutureAuthorizationSha256"),
        *Admission.FutureAuthorizationSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_PalmHeroAuthority"),
        Authority);
    return ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroCandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroProvenanceSha256"),
               ProvenanceManifestSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroAuthority"),
               Authority,
               OutError);
}

bool ValidateCommonOutputMetadata(
    const UObject* Asset,
    const FAdmission& Admission,
    const TCHAR* Authority,
    FString& OutError)
{
    return ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroCandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroProvenanceSha256"),
               ProvenanceManifestSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_PalmHeroAuthority"),
               Authority,
               OutError);
}

bool ValidateTextureImportProvenance(
    const UTexture2D* Texture,
    int32 Index,
    const FAdmission& Admission,
    FString& OutError)
{
#if WITH_EDITORONLY_DATA
    if (!Texture || Index < 0 || Index >= TextureCount ||
        Admission.TextureSourceBytes.Num() != TextureCount ||
        Admission.TextureSourceBytes[Index].Num() != TextureBytes[Index] ||
        !Texture->AssetImportData ||
        Texture->AssetImportData->GetSourceFileCount() != 1)
    {
        OutError = TEXT("PalmHero texture lost its exact retained source bytes or single AssetImportData record.");
        return false;
    }
    const FAssetImportInfo& SourceData =
        Texture->AssetImportData->GetSourceData();
    if (SourceData.SourceFiles.Num() != 1 ||
        !SourceData.SourceFiles[0].FileHash.IsValid())
    {
        OutError = TEXT("PalmHero texture AssetImportData lacks one valid source-file hash.");
        return false;
    }

    FString ExpectedFilename = SourcePath(
        Admission.CandidateRoot,
        TextureRelativePaths[Index]);
    FString FirstFilename = Texture->AssetImportData->GetFirstFilename();
    FString ResolvedRecordFilename = UAssetImportData::ResolveImportFilename(
        SourceData.SourceFiles[0].RelativeFilename,
        Texture->GetOutermost());
    FPaths::NormalizeFilename(ExpectedFilename);
    FPaths::NormalizeFilename(FirstFilename);
    FPaths::NormalizeFilename(ResolvedRecordFilename);
    if (!FPaths::IsSamePath(FirstFilename, ExpectedFilename) ||
        !FPaths::IsSamePath(ResolvedRecordFilename, ExpectedFilename))
    {
        OutError = TEXT("PalmHero texture AssetImportData source filename is not the exact admitted file.");
        return false;
    }

    const TArray<uint8>& PinnedBytes = Admission.TextureSourceBytes[Index];
    FMD5 ExpectedMd5Builder;
    ExpectedMd5Builder.Update(
        PinnedBytes.GetData(),
        static_cast<uint64>(PinnedBytes.Num()));
    FMD5Hash ExpectedMd5;
    ExpectedMd5.Set(ExpectedMd5Builder);
    if (SourceData.SourceFiles[0].FileHash != ExpectedMd5)
    {
        OutError = TEXT("PalmHero texture AssetImportData MD5 does not match the SHA-256-admitted source bytes.");
        return false;
    }
    OutError.Reset();
    return true;
#else
    OutError = TEXT("PalmHero texture provenance validation requires editor-only import data.");
    return false;
#endif
}

bool ValidateTextureSourcePayloadFingerprint(
    UTexture2D* Texture,
    int32 Index,
    const FAdmission& Admission,
    FString& OutError)
{
#if WITH_EDITORONLY_DATA
    if (!Texture || Index < 0 || Index >= TextureCount ||
        Admission.TextureSourceBytes.Num() != TextureCount ||
        !Texture->Source.IsValid() || Texture->Source.GetNumBlocks() != 1 ||
        Texture->Source.GetNumLayers() != 1 ||
        Texture->Source.GetNumSlices() != 1 ||
        Texture->Source.GetNumMips() != 1 ||
        Texture->Source.GetSizeX() != 2048 ||
        Texture->Source.GetSizeY() != 4096 ||
        Texture->Source.GetFormat() != TextureSourceFormats[Index] ||
        Texture->Source.GetSourceCompression() !=
            ETextureSourceCompressionFormat::TSCF_JPEG)
    {
        OutError = TEXT("PalmHero texture source art lost its exact retained-JPEG format, one-block, one-layer, one-slice, one-mip dimensions.");
        return false;
    }
    const TArray<uint8>& PinnedBytes = Admission.TextureSourceBytes[Index];
    const FSharedBuffer SourcePayload =
        Texture->Source.GetBulkDataPayload();
    if (PinnedBytes.IsEmpty() || !SourcePayload ||
        SourcePayload.GetSize() != static_cast<uint64>(PinnedBytes.Num()) ||
        !SourcePayload.GetData() ||
        FMemory::Memcmp(
            SourcePayload.GetData(),
            PinnedBytes.GetData(),
            PinnedBytes.Num()) != 0)
    {
        OutError = TEXT("PalmHero Texture->Source retained payload is not byte-exact to the SHA-256-admitted JPEG.");
        return false;
    }
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (!SHA256(
            static_cast<const uint8*>(SourcePayload.GetData()),
            static_cast<size_t>(SourcePayload.GetSize()),
            Digest) ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper() !=
            FString(TextureSha256[Index]))
    {
        OutError = TEXT("PalmHero Texture->Source raw payload SHA-256 does not match the immutable source pin.");
        return false;
    }
    OutError.Reset();
    return true;
#else
    OutError = TEXT("PalmHero texture source-payload fingerprint validation requires editor-only source art.");
    return false;
#endif
}

bool ValidateTexture(
    UTexture2D* Texture,
    int32 Index,
    const FAdmission& Admission,
    FString& OutError)
{
    const bool bSrgb = Index == 0;
    const TextureCompressionSettings Compression = Index == 0
        ? TC_Default
        : (Index == 1 ? TC_Normalmap : TC_Masks);
    if (!Texture || Index < 0 || Index >= TextureCount ||
        Texture->GetPathName() != TextureObjectPath(Index) ||
        Texture->GetSizeX() != 2048 || Texture->GetSizeY() != 4096 ||
        Texture->SRGB != bSrgb ||
        Texture->CompressionSettings != Compression ||
        Texture->LODGroup != TEXTUREGROUP_World ||
        Texture->MipGenSettings != TMGS_FromTextureGroup ||
        Texture->bFlipGreenChannel ||
        Texture->VirtualTextureStreaming || Texture->NeverStream)
    {
        OutError = FString::Printf(
            TEXT("PalmHero texture %d lost its exact dimensions, color/compression, streaming, or path policy."),
            Index);
        return false;
    }
    return ValidateTextureImportProvenance(
               Texture,
               Index,
               Admission,
               OutError) &&
        ValidateTextureSourcePayloadFingerprint(
               Texture,
               Index,
               Admission,
               OutError);
}

bool ValidateMaterial(
    const UMaterial* Material,
    int32 Slot,
    const TArray<UTexture2D*>& Textures,
    FString& OutError)
{
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly || Slot < 0 || Slot >= MaterialCount ||
        Material->GetPathName() != MaterialObjectPath(Slot) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque ||
        Material->bUseMaterialAttributes ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Material->TwoSided != (Slot != 0) ||
        !Material->GetShadingModels().HasOnlyShadingModel(
            Slot == 0 ? MSM_DefaultLit : MSM_TwoSidedFoliage) ||
        EditorOnly->WorldPositionOffset.Expression != nullptr)
    {
        OutError = TEXT("PalmHero material lost its exact static appearance-only surface policy.");
        return false;
    }
    if (Slot == 0)
    {
        if (Textures.Num() != TextureCount ||
            Material->GetExpressions().Num() != TextureCount)
        {
            OutError = TEXT("PalmHero Bark material requires exactly four pinned texture parameters.");
            return false;
        }
        UMaterialExpressionTextureSampleParameter2D* Samples[TextureCount] = {
            nullptr, nullptr, nullptr, nullptr};
        for (UMaterialExpression* Expression : Material->GetExpressions())
        {
            UMaterialExpressionTextureSampleParameter2D* Sample =
                Cast<UMaterialExpressionTextureSampleParameter2D>(Expression);
            if (!Sample)
            {
                OutError = TEXT("PalmHero Bark material contains an uncontracted expression.");
                return false;
            }
            for (int32 Index = 0; Index < TextureCount; ++Index)
            {
                if (Sample->ParameterName == FName(TextureRoles[Index]))
                {
                    if (Samples[Index])
                    {
                        OutError = TEXT("PalmHero Bark material duplicates a texture parameter.");
                        return false;
                    }
                    Samples[Index] = Sample;
                }
            }
        }
        for (int32 Index = 0; Index < TextureCount; ++Index)
        {
            const EMaterialSamplerType Sampler = Index == 0
                ? SAMPLERTYPE_Color
                : (Index == 1 ? SAMPLERTYPE_Normal : SAMPLERTYPE_Masks);
            if (!Samples[Index] || Samples[Index]->Texture != Textures[Index] ||
                Samples[Index]->SamplerType != Sampler ||
                Samples[Index]->SamplerSource != SSM_FromTextureAsset)
            {
                OutError = TEXT("PalmHero Bark material texture parameter route drifted.");
                return false;
            }
        }
        if (EditorOnly->BaseColor.Expression != Samples[0] ||
            EditorOnly->Normal.Expression != Samples[1] ||
            EditorOnly->Roughness.Expression != Samples[2] ||
            EditorOnly->AmbientOcclusion.Expression != Samples[3] ||
            EditorOnly->Roughness.OutputIndex != 1 ||
            EditorOnly->AmbientOcclusion.OutputIndex != 1)
        {
            OutError = TEXT("PalmHero Bark material output wiring drifted.");
            return false;
        }
    }
    else
    {
        UMaterialExpressionVectorParameter* Color = nullptr;
        UMaterialExpressionScalarParameter* Roughness = nullptr;
        UMaterialExpressionScalarParameter* Specular = nullptr;
        for (UMaterialExpression* Expression : Material->GetExpressions())
        {
            if (UMaterialExpressionVectorParameter* Candidate =
                    Cast<UMaterialExpressionVectorParameter>(Expression))
            {
                Color = Candidate->ParameterName == FName(TEXT("FrondColor"))
                    ? Candidate
                    : Color;
            }
            if (UMaterialExpressionScalarParameter* Candidate =
                    Cast<UMaterialExpressionScalarParameter>(Expression))
            {
                Roughness = Candidate->ParameterName ==
                        FName(TEXT("FrondRoughness"))
                    ? Candidate
                    : Roughness;
                Specular = Candidate->ParameterName ==
                        FName(TEXT("FrondSpecular"))
                    ? Candidate
                    : Specular;
            }
        }
        const FLinearColor ExpectedColor = Slot == 1
            ? FLinearColor(0.105f, 0.360f, 0.095f, 1.0f)
            : FLinearColor(0.410f, 0.245f, 0.085f, 1.0f);
        const float ExpectedRoughness = Slot == 1 ? 0.65f : 0.80f;
        const float ExpectedSpecular = Slot == 1 ? 0.15f : 0.10f;
        if (Material->GetExpressions().Num() != 3 || !Color || !Roughness ||
            !Specular || Color->DefaultValue != ExpectedColor ||
            Roughness->DefaultValue != ExpectedRoughness ||
            Specular->DefaultValue != ExpectedSpecular ||
            EditorOnly->BaseColor.Expression != Color ||
            EditorOnly->SubsurfaceColor.Expression != Color ||
            EditorOnly->Roughness.Expression != Roughness ||
            EditorOnly->Specular.Expression != Specular)
        {
            OutError = TEXT("PalmHero frond color, roughness, specular, subsurface, or output route drifted.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool LoadAndValidateOutputAssets(
    const FAdmission& Admission,
    TArray<UObject*>* OutAssets,
    FString& OutError)
{
    if (OutAssets)
    {
        OutAssets->Reset();
    }
    TArray<UTexture2D*> Textures;
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        UTexture2D* Texture = LoadExact<UTexture2D>(TextureObjectPath(Index));
        if (!ValidateTexture(Texture, Index, Admission, OutError) ||
            !ValidateCommonOutputMetadata(
                Texture,
                Admission,
                TEXT("APPEARANCE_ONLY_NO_SIMULATION_AUTHORITY"),
                OutError) ||
            !ExactAssetMetadata(
                Texture,
                TEXT("TRIAD_PalmHeroSourceSha256"),
                TextureSha256[Index],
                OutError))
        {
            return false;
        }
        Textures.Add(Texture);
        if (OutAssets)
        {
            OutAssets->Add(Texture);
        }
    }
    TArray<UMaterial*> Materials;
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        UMaterial* Material = LoadExact<UMaterial>(MaterialObjectPath(Slot));
        if (!ValidateMaterial(Material, Slot, Textures, OutError) ||
            !ValidateCommonOutputMetadata(
                Material,
                Admission,
                TEXT("APPEARANCE_ONLY_NO_SIMULATION_AUTHORITY"),
                OutError) ||
            !ExactAssetMetadata(
                Material,
                TEXT("TRIAD_PalmHeroMaterialSlot"),
                MaterialNames[Slot],
                OutError))
        {
            return false;
        }
        Materials.Add(Material);
        if (OutAssets)
        {
            OutAssets->Add(Material);
        }
    }
    UStaticMesh* Mesh = LoadExact<UStaticMesh>(FinalMeshObjectPath);
    if (!Mesh || Mesh->GetNumSourceModels() != LodCount ||
        !ValidateMaterialSlots(Mesh, Materials, true, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("PalmHero final mesh is absent or lost its exact source LOD count.");
        }
        return false;
    }
    if (!ValidateCommonOutputMetadata(
            Mesh,
            Admission,
            TEXT("APPEARANCE_ONLY_NO_COLLISION_NAVIGATION_LOS_RF_SENSOR_TERRAIN_AUTHORITY"),
            OutError) ||
        !ExactAssetMetadata(
            Mesh,
            TEXT("TRIAD_PalmHeroOptionalFallback"),
            UTRIADIstanaExploreV5DPalmHeroSourceLibrary::
                ExistingPalmFallbackObjectPath(),
            OutError))
    {
        return false;
    }
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        const FString Key = FString::Printf(
            TEXT("TRIAD_PalmHeroLod%dSourceSha256"),
            Lod);
        if (!ExactAssetMetadata(
                Mesh,
                *Key,
                LodSha256[Lod],
                OutError))
        {
            return false;
        }
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (Mesh->GetNumLODs() != LodCount || Mesh->IsNaniteEnabled())
    {
        OutError = TEXT("PalmHero final mesh lost its exact non-Nanite render LOD policy.");
        return false;
    }
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        FMeshDescription Description;
        if (!Mesh->CloneMeshDescription(Lod, Description) ||
            !ValidateMeshDescription(Description, Lod, OutError))
        {
            return false;
        }
    }
    const UBodySetup* Body = Mesh->GetBodySetup();
    if (Body && (Body->AggGeom.GetElementCount() != 0 ||
                 Body->GetCollisionTraceFlag() == CTF_UseComplexAsSimple))
    {
        OutError = TEXT("PalmHero final mesh acquired uncontracted collision geometry or complexity.");
        return false;
    }
    FTRIADIstanaExploreV5DPalmHeroAssets RuntimeAssets;
    RuntimeAssets.PalmMesh = Mesh;
    for (UMaterial* Material : Materials)
    {
        RuntimeAssets.Materials.Add(Material);
    }
    for (UTexture2D* Texture : Textures)
    {
        RuntimeAssets.BarkTextures.Add(Texture);
    }
    if (!UTRIADIstanaExploreV5DPalmHeroSourceLibrary::ValidateAssetRoster(
            RuntimeAssets,
            OutError))
    {
        return false;
    }
    if (OutAssets)
    {
        OutAssets->Insert(Mesh, 0);
        if (OutAssets->Num() != OutputAssetCount)
        {
            OutError = TEXT("PalmHero output asset roster is not exactly eight unique assets.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

template <typename TObjectType>
TObjectType* GetOnlyExactImportedObject(
    const UAssetImportTask* Task,
    const FString& ExpectedObjectPath,
    FString& OutError)
{
    if (!Task)
    {
        OutError = TEXT("PalmHero import task is absent.");
        return nullptr;
    }
    const TArray<UObject*>& ReturnedObjects = Task->GetObjects();
    if (Task->ImportedObjectPaths.Num() != 1 ||
        Task->ImportedObjectPaths[0] != ExpectedObjectPath ||
        ReturnedObjects.Num() != 1 || !ReturnedObjects[0] ||
        ReturnedObjects[0]->GetPathName() != ExpectedObjectPath)
    {
        OutError = FString::Printf(
            TEXT("PalmHero import task must return exactly one object at '%s'; nonmatching or companion outputs are denied."),
            *ExpectedObjectPath);
        return nullptr;
    }
    TObjectType* Result = Cast<TObjectType>(ReturnedObjects[0]);
    if (!Result)
    {
        OutError = FString::Printf(
            TEXT("PalmHero import task returned the wrong class at '%s'."),
            *ExpectedObjectPath);
        return nullptr;
    }
    OutError.Reset();
    return Result;
}

bool ImportTextures(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    TArray<UTexture2D*>& OutTextures,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    OutTextures.Reset();
    TArray<UAssetImportTask*> Tasks;
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        UTextureFactory* Factory = NewObject<UTextureFactory>();
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        if (!Factory || !Task)
        {
            OutError = TEXT("Could not allocate all four deterministic PalmHero texture import tasks.");
            return false;
        }
        const bool bSrgb = Index == 0;
        Factory->bCreateMaterial = false;
        Factory->NoAlpha = false;
        Factory->bDeferCompression = false;
        Factory->CompressionSettings = Index == 0
            ? TC_Default
            : (Index == 1 ? TC_Normalmap : TC_Masks);
        Factory->LODGroup = TEXTUREGROUP_World;
        Factory->MipGenSettings = TMGS_FromTextureGroup;
        Factory->bFlipNormalMapGreenChannel = false;
        Factory->ColorSpaceMode = bSrgb
            ? ETextureSourceColorSpace::SRGB
            : ETextureSourceColorSpace::Linear;
        Task->Filename = SourcePath(
            Admission.CandidateRoot,
            TextureRelativePaths[Index]);
        Task->DestinationPath = FPaths::Combine(OutputRoot, TEXT("Textures"));
        Task->DestinationName = TextureAssetNames[Index];
        Task->bReplaceExisting = false;
        Task->bReplaceExistingSettings = false;
        Task->bAutomated = true;
        Task->bSave = false;
        Task->bAsync = false;
        Task->Factory = Factory;
        Task->Options = Factory;
        Tasks.Add(Task);
    }
    AssetTools.ImportAssetTasks(Tasks);
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        UTexture2D* Texture = GetOnlyExactImportedObject<UTexture2D>(
            Tasks[Index],
            TextureObjectPath(Index),
            OutError);
        if (!Texture)
        {
            return false;
        }
        Texture->Modify();
        Texture->SRGB = Index == 0;
        Texture->CompressionSettings = Index == 0
            ? TC_Default
            : (Index == 1 ? TC_Normalmap : TC_Masks);
        Texture->LODGroup = TEXTUREGROUP_World;
        Texture->MipGenSettings = TMGS_FromTextureGroup;
        Texture->bFlipGreenChannel = false;
        Texture->AddressX = TA_Wrap;
        Texture->AddressY = TA_Wrap;
        Texture->Filter = TF_Default;
        Texture->VirtualTextureStreaming = false;
        Texture->NeverStream = false;
        if (!WriteCommonOutputMetadata(
                Texture,
                Admission,
                TEXT("APPEARANCE_ONLY_NO_SIMULATION_AUTHORITY"),
                OutError))
        {
            return false;
        }
        if (UMetaData* Metadata = Texture->GetOutermost()->GetMetaData())
        {
            Metadata->SetValue(
                Texture,
                TEXT("TRIAD_PalmHeroSourceSha256"),
                TextureSha256[Index]);
        }
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (!ValidateTexture(Texture, Index, Admission, OutError))
        {
            return false;
        }
        OutTextures.Add(Texture);
        OutAssetsToSave.Add(Texture);
    }
    TArray<FString> ExpectedTexturePaths;
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        ExpectedTexturePaths.Add(TextureObjectPath(Index));
    }
    if (!NamespaceMatchesExactAssets(
            OutputRoot,
            ExpectedTexturePaths,
            OutError))
    {
        return false;
    }
    OutError.Reset();
    return OutTextures.Num() == TextureCount;
}

template <typename TExpression>
TExpression* AddExpression(
    UMaterial* Material,
    int32 X,
    int32 Y)
{
    TExpression* Expression = Material
        ? NewObject<TExpression>(Material)
        : nullptr;
    if (Expression)
    {
        Material->GetExpressionCollection().AddExpression(Expression);
        Expression->MaterialExpressionEditorX = X;
        Expression->MaterialExpressionEditorY = Y;
    }
    return Expression;
}

UMaterialExpressionTextureSampleParameter2D* AddTextureParameter(
    UMaterial* Material,
    UTexture2D* Texture,
    const TCHAR* ParameterName,
    EMaterialSamplerType SamplerType,
    int32 Y)
{
    UMaterialExpressionTextureSampleParameter2D* Sample =
        AddExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material,
            -520,
            Y);
    if (Sample)
    {
        Sample->ParameterName = FName(ParameterName);
        Sample->Texture = Texture;
        Sample->SamplerType = SamplerType;
        Sample->SamplerSource = SSM_FromTextureAsset;
    }
    return Sample;
}

UMaterial* CreateBarkMaterial(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    const TArray<UTexture2D*>& Textures,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              MaterialAssetNames[0],
              FPaths::Combine(OutputRoot, TEXT("Materials")),
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.MaterializePalmHeroCandidateAssets"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly || Textures.Num() != TextureCount ||
        Material->GetPathName() != MaterialObjectPath(0) ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create an exact empty PalmHero Bark material package.");
        return nullptr;
    }
    UMaterialExpressionTextureSampleParameter2D* Diffuse =
        AddTextureParameter(
            Material,
            Textures[0],
            TextureRoles[0],
            SAMPLERTYPE_Color,
            -260);
    UMaterialExpressionTextureSampleParameter2D* Normal =
        AddTextureParameter(
            Material,
            Textures[1],
            TextureRoles[1],
            SAMPLERTYPE_Normal,
            -60);
    UMaterialExpressionTextureSampleParameter2D* Roughness =
        AddTextureParameter(
            Material,
            Textures[2],
            TextureRoles[2],
            SAMPLERTYPE_Masks,
            140);
    UMaterialExpressionTextureSampleParameter2D* AmbientOcclusion =
        AddTextureParameter(
            Material,
            Textures[3],
            TextureRoles[3],
            SAMPLERTYPE_Masks,
            340);
    if (!Diffuse || !Normal || !Roughness || !AmbientOcclusion)
    {
        OutError = TEXT("Could not allocate the exact four-node PalmHero Bark graph.");
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = false;
    Material->bUseMaterialAttributes = false;
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->SetShadingModel(MSM_DefaultLit);
    EditorOnly->BaseColor.Connect(0, Diffuse);
    EditorOnly->Normal.Connect(0, Normal);
    EditorOnly->Roughness.Connect(1, Roughness);
    EditorOnly->AmbientOcclusion.Connect(1, AmbientOcclusion);
    if (!WriteCommonOutputMetadata(
            Material,
            Admission,
            TEXT("APPEARANCE_ONLY_NO_SIMULATION_AUTHORITY"),
            OutError))
    {
        return nullptr;
    }
    if (UMetaData* Metadata = Material->GetOutermost()->GetMetaData())
    {
        Metadata->SetValue(
            Material,
            TEXT("TRIAD_PalmHeroMaterialSlot"),
            MaterialNames[0]);
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    if (!ValidateMaterial(Material, 0, Textures, OutError))
    {
        return nullptr;
    }
    OutError.Reset();
    return Material;
}

UMaterial* CreateFrondMaterial(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    int32 Slot,
    const TArray<UTexture2D*>& Textures,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Slot > 0 && Slot < MaterialCount && Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              MaterialAssetNames[Slot],
              FPaths::Combine(OutputRoot, TEXT("Materials")),
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.MaterializePalmHeroCandidateAssets"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        Material->GetPathName() != MaterialObjectPath(Slot) ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create an exact empty PalmHero frond material package.");
        return nullptr;
    }
    UMaterialExpressionVectorParameter* Color =
        AddExpression<UMaterialExpressionVectorParameter>(
            Material,
            -420,
            -120);
    UMaterialExpressionScalarParameter* Roughness =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material,
            -420,
            80);
    UMaterialExpressionScalarParameter* Specular =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material,
            -420,
            220);
    if (!Color || !Roughness || !Specular)
    {
        OutError = TEXT("Could not allocate the exact three-node PalmHero frond graph.");
        return nullptr;
    }
    Color->ParameterName = FName(TEXT("FrondColor"));
    Color->DefaultValue = Slot == 1
        ? FLinearColor(0.105f, 0.360f, 0.095f, 1.0f)
        : FLinearColor(0.410f, 0.245f, 0.085f, 1.0f);
    Roughness->ParameterName = FName(TEXT("FrondRoughness"));
    Roughness->DefaultValue = Slot == 1 ? 0.65f : 0.80f;
    Specular->ParameterName = FName(TEXT("FrondSpecular"));
    Specular->DefaultValue = Slot == 1 ? 0.15f : 0.10f;
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = true;
    Material->bUseMaterialAttributes = false;
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->SetShadingModel(MSM_TwoSidedFoliage);
    EditorOnly->BaseColor.Connect(0, Color);
    EditorOnly->SubsurfaceColor.Connect(0, Color);
    EditorOnly->Roughness.Connect(0, Roughness);
    EditorOnly->Specular.Connect(0, Specular);
    if (!WriteCommonOutputMetadata(
            Material,
            Admission,
            TEXT("APPEARANCE_ONLY_NO_SIMULATION_AUTHORITY"),
            OutError))
    {
        return nullptr;
    }
    if (UMetaData* Metadata = Material->GetOutermost()->GetMetaData())
    {
        Metadata->SetValue(
            Material,
            TEXT("TRIAD_PalmHeroMaterialSlot"),
            MaterialNames[Slot]);
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    if (!ValidateMaterial(Material, Slot, Textures, OutError))
    {
        return nullptr;
    }
    OutError.Reset();
    return Material;
}

bool CreateMaterials(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    const TArray<UTexture2D*>& Textures,
    TArray<UMaterial*>& OutMaterials,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    OutMaterials.Reset();
    UMaterial* Bark = CreateBarkMaterial(
        AssetTools,
        Admission,
        Textures,
        OutError);
    if (!Bark)
    {
        return false;
    }
    OutMaterials.Add(Bark);
    OutAssetsToSave.Add(Bark);
    for (int32 Slot = 1; Slot < MaterialCount; ++Slot)
    {
        UMaterial* Frond = CreateFrondMaterial(
            AssetTools,
            Admission,
            Slot,
            Textures,
            OutError);
        if (!Frond)
        {
            return false;
        }
        OutMaterials.Add(Frond);
        OutAssetsToSave.Add(Frond);
    }
    OutError.Reset();
    return OutMaterials.Num() == MaterialCount;
}

FString StagingAssetName(int32 Lod)
{
    return FString::Printf(
        TEXT("SM_IPV5D_PalmHeroSourceStaging_LOD%d"),
        Lod);
}

FString StagingPackagePath(int32 Lod)
{
    return FPaths::Combine(StagingRoot, StagingAssetName(Lod));
}

FString StagingObjectPath(int32 Lod)
{
    return StagingPackagePath(Lod) + TEXT(".") + StagingAssetName(Lod);
}

UAssetImportTask* MakeObjImportTask(
    const FAdmission& Admission,
    int32 Lod,
    FString& OutError)
{
    UFbxImportUI* Options = NewObject<UFbxImportUI>();
    UFbxFactory* Factory = NewObject<UFbxFactory>();
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    if (!Options || !Factory || !Task || !Options->StaticMeshImportData)
    {
        OutError = TEXT("Could not allocate deterministic PalmHero OBJ import options.");
        return nullptr;
    }
    Options->bImportAsSkeletal = false;
    Options->MeshTypeToImport = FBXIT_StaticMesh;
    Options->bAutomatedImportShouldDetectType = false;
    Options->bImportMesh = true;
    Options->bImportMaterials = false;
    Options->bImportTextures = false;
    Options->StaticMeshImportData->ImportUniformScale = 100.0f;
    Options->StaticMeshImportData->bConvertScene = false;
    Options->StaticMeshImportData->bConvertSceneUnit = false;
    Options->StaticMeshImportData->bForceFrontXAxis = false;
    Options->StaticMeshImportData->bCombineMeshes = true;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = false;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = false;
    Options->StaticMeshImportData->bRemoveDegenerates = false;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = SourcePath(Admission.CandidateRoot, LodRelativePaths[Lod]);
    Task->DestinationPath = StagingRoot;
    Task->DestinationName = StagingAssetName(Lod);
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    OutError.Reset();
    return Task;
}

bool ImportStagingLods(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    TArray<UStaticMesh*>& OutLods,
    FString& OutError)
{
    OutLods.Reset();
    TArray<UAssetImportTask*> Tasks;
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        if (FindPackage(nullptr, *StagingPackagePath(Lod)) ||
            FPackageName::DoesPackageExist(StagingPackagePath(Lod)))
        {
            OutError = TEXT("PalmHero temporary staging namespace is not empty; restart the editor before a fail-closed retry.");
            return false;
        }
        UAssetImportTask* Task = MakeObjImportTask(
            Admission,
            Lod,
            OutError);
        if (!Task)
        {
            return false;
        }
        Tasks.Add(Task);
    }
    AssetTools.ImportAssetTasks(Tasks);
    FAssetCompilingManager::Get().FinishAllCompilation();
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        UStaticMesh* Mesh = GetOnlyExactImportedObject<UStaticMesh>(
            Tasks[Lod],
            StagingObjectPath(Lod),
            OutError);
        FMeshDescription Description;
        const TArray<UMaterial*> NoBindings;
        if (!Mesh || Mesh->GetNumSourceModels() != 1 ||
            Mesh->GetNumLODs() != 1 || Mesh->IsNaniteEnabled() ||
            !ValidateMaterialSlots(Mesh, NoBindings, false, OutError) ||
            !Mesh->CloneMeshDescription(0, Description) ||
            !ValidateMeshDescription(Description, Lod, OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("PalmHero imported staging LOD%d failed its exact single-LOD source policy."),
                    Lod);
            }
            return false;
        }
        const UBodySetup* Body = Mesh->GetBodySetup();
        if (Body && Body->AggGeom.GetElementCount() != 0)
        {
            OutError = TEXT("PalmHero OBJ importer generated uncontracted collision.");
            return false;
        }
        OutLods.Add(Mesh);
    }
    TArray<FString> ExpectedStagingPaths;
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        ExpectedStagingPaths.Add(StagingObjectPath(Lod));
    }
    if (!NamespaceMatchesExactAssets(
            StagingRoot,
            ExpectedStagingPaths,
            OutError))
    {
        return false;
    }
    OutError.Reset();
    return OutLods.Num() == LodCount;
}

bool CreateFinalMesh(
    const FAdmission& Admission,
    const TArray<UStaticMesh*>& StagingLods,
    const TArray<UMaterial*>& Materials,
    UStaticMesh*& OutMesh,
    FString& OutError)
{
    OutMesh = nullptr;
    if (StagingLods.Num() != LodCount ||
        StagingLods.Contains(nullptr) || Materials.Num() != MaterialCount ||
        Materials.Contains(nullptr))
    {
        OutError = TEXT("PalmHero final mesh requires all three validated staging LODs and materials.");
        return false;
    }
    UPackage* Package = CreatePackage(*FinalMeshPackagePath);
    UStaticMesh* Mesh = Package
        ? Cast<UStaticMesh>(StaticDuplicateObject(
              StagingLods[0],
              Package,
              FName(TEXT("SM_IPV5D_PalmHero")),
              RF_Public | RF_Standalone | RF_Transactional))
        : nullptr;
    if (!Mesh || Mesh == StagingLods[0] ||
        Mesh->GetPathName() != FinalMeshObjectPath)
    {
        OutError = TEXT("Could not allocate the isolated PalmHero final mesh package.");
        return false;
    }
    Mesh->Modify();
    Mesh->SetNumSourceModels(LodCount);
    Mesh->bAutoComputeLODScreenSize = false;
    const float ScreenSizes[LodCount] = {1.0f, 0.40f, 0.15f};
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        FMeshDescription Description;
        if (!StagingLods[Lod]->CloneMeshDescription(0, Description) ||
            !ValidateMeshDescription(Description, Lod, OutError))
        {
            return false;
        }
        const int32 Vertices = Description.Vertices().Num();
        const int32 Triangles = Description.Triangles().Num();
        FMeshDescription* Installed = Mesh->CreateMeshDescription(
            Lod,
            MoveTemp(Description));
        if (!Installed || Installed->Vertices().Num() != Vertices ||
            Installed->Triangles().Num() != Triangles)
        {
            OutError = TEXT("Could not install an exact PalmHero source LOD MeshDescription.");
            return false;
        }
        FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(Lod);
        SourceModel.ScreenSize = FPerPlatformFloat(ScreenSizes[Lod]);
        SourceModel.BuildSettings.bRecomputeNormals = false;
        SourceModel.BuildSettings.bRecomputeTangents = true;
        SourceModel.BuildSettings.bUseMikkTSpace = true;
        SourceModel.BuildSettings.bRemoveDegenerates = false;
        SourceModel.BuildSettings.bGenerateLightmapUVs = false;
        SourceModel.BuildSettings.bBuildReversedIndexBuffer = false;
        SourceModel.ReductionSettings.PercentTriangles = 1.0f;
        SourceModel.ReductionSettings.PercentVertices = 1.0f;
        SourceModel.ReductionSettings.MaxDeviation = 0.0f;
        SourceModel.ReductionSettings.BaseLODModel = Lod;
        UStaticMesh::FCommitMeshDescriptionParams CommitParams;
        CommitParams.bMarkPackageDirty = false;
        CommitParams.bUseHashAsGuid = true;
        Mesh->CommitMeshDescription(Lod, CommitParams);
    }
    TArray<FStaticMaterial> Bindings;
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        Bindings.Emplace(
            Materials[Slot],
            FName(MaterialNames[Slot]),
            FName(MaterialNames[Slot]));
    }
    Mesh->SetStaticMaterials(Bindings);
    Mesh->NaniteSettings.bEnabled = false;
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseDefault;
        Body->InvalidatePhysicsData();
    }
    if (!WriteCommonOutputMetadata(
            Mesh,
            Admission,
            TEXT("APPEARANCE_ONLY_NO_COLLISION_NAVIGATION_LOS_RF_SENSOR_TERRAIN_AUTHORITY"),
            OutError))
    {
        return false;
    }
    if (UMetaData* Metadata = Package->GetMetaData())
    {
        Metadata->SetValue(
            Mesh,
            TEXT("TRIAD_PalmHeroOptionalFallback"),
            *UTRIADIstanaExploreV5DPalmHeroSourceLibrary::
                ExistingPalmFallbackObjectPath());
        for (int32 Lod = 0; Lod < LodCount; ++Lod)
        {
            const FString Key = FString::Printf(
                TEXT("TRIAD_PalmHeroLod%dSourceSha256"),
                Lod);
            Metadata->SetValue(Mesh, *Key, LodSha256[Lod]);
        }
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Mesh);
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UMaterial*> MutableMaterials = Materials;
    if (Mesh->GetNumLODs() != LodCount || Mesh->IsNaniteEnabled() ||
        !ValidateMaterialSlots(Mesh, MutableMaterials, true, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("PalmHero assembled mesh failed its exact LOD or material policy.");
        }
        return false;
    }
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        FMeshDescription Description;
        if (!Mesh->CloneMeshDescription(Lod, Description) ||
            !ValidateMeshDescription(Description, Lod, OutError))
        {
            return false;
        }
    }
    OutMesh = Mesh;
    OutError.Reset();
    return true;
}

void GetStagingObjectPaths(TArray<FString>& OutPaths)
{
    OutPaths.Reset();
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        OutPaths.Add(StagingObjectPath(Lod));
    }
}

void GetOutputObjectPaths(TArray<FString>& OutPaths)
{
    OutPaths.Reset();
    OutPaths.Add(FinalMeshObjectPath);
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        OutPaths.Add(TextureObjectPath(Index));
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        OutPaths.Add(MaterialObjectPath(Slot));
    }
}

bool DeleteExactFreshAssets(
    const TArray<FString>& ExactObjectPaths,
    FString& OutError)
{
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutError = TEXT("Fresh PalmHero rollback requires EditorAssetSubsystem.");
        return false;
    }
    TArray<UObject*> Loaded;
    for (const FString& ObjectPath : ExactObjectPaths)
    {
        if (UObject* Object = FindObject<UObject>(nullptr, *ObjectPath))
        {
            if (!Object->GetOutermost() ||
                Object->GetPathName() != ObjectPath)
            {
                OutError = TEXT("Fresh PalmHero rollback encountered an object outside its exact allowlist.");
                return false;
            }
            Loaded.AddUnique(Object);
        }
    }
    if (!Loaded.IsEmpty() && !AssetSubsystem->DeleteLoadedAssets(Loaded))
    {
        OutError = TEXT("Fresh PalmHero rollback could not delete every exact loaded asset.");
        return false;
    }
    for (const FString& ObjectPath : ExactObjectPaths)
    {
        const FString PackagePath = FPackageName::ObjectPathToPackageName(
            ObjectPath);
        if (FindObject<UObject>(nullptr, *ObjectPath) ||
            FindPackage(nullptr, *PackagePath) ||
            FPackageName::DoesPackageExist(PackagePath))
        {
            OutError = FString::Printf(
                TEXT("Fresh PalmHero rollback left exact asset or package '%s'."),
                *ObjectPath);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

struct FFreshNamespaceBaseline
{
    bool bOutputRootProvenEmptyAtEntry = false;
    bool bStagingRootProvenEmptyAtEntry = false;
};

bool DeleteFreshNamespaceContents(
    const FString& NamespaceRoot,
    FString& OutError)
{
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutError = TEXT("Fresh PalmHero namespace rollback requires EditorAssetSubsystem.");
        return false;
    }

    TArray<UObject*> LoadedAssets;
    for (TObjectIterator<UObject> It; It; ++It)
    {
        UObject* Object = *It;
        UPackage* Package = Object ? Object->GetOutermost() : nullptr;
        if (Object && IsValid(Object) && Object->IsAsset() && Package &&
            IsPackageWithinNamespace(Package->GetName(), NamespaceRoot))
        {
            LoadedAssets.AddUnique(Object);
        }
    }
    const bool bLoadedDeleteReportedSuccess = LoadedAssets.IsEmpty() ||
        AssetSubsystem->DeleteLoadedAssets(LoadedAssets);
    const bool bDirectoryDeleteReportedSuccess =
        !AssetSubsystem->DoesDirectoryExist(NamespaceRoot) ||
        AssetSubsystem->DeleteDirectory(NamespaceRoot);
    const TArray<FString> EmptyExpected;
    FString ResidualStateError;
    const bool bNamespaceProvenEmpty = NamespaceMatchesExactAssets(
            NamespaceRoot,
            EmptyExpected,
            ResidualStateError);
    if (!bLoadedDeleteReportedSuccess ||
        !bDirectoryDeleteReportedSuccess ||
        !bNamespaceProvenEmpty)
    {
        OutError = FString::Printf(
            TEXT("Fresh PalmHero rollback could not prove cleanup of namespace '%s' (DeleteLoadedAssets=%s DeleteDirectory=%s NamespaceEmpty=%s): %s"),
            *NamespaceRoot,
            bLoadedDeleteReportedSuccess ? TEXT("true") : TEXT("false"),
            bDirectoryDeleteReportedSuccess ? TEXT("true") : TEXT("false"),
            bNamespaceProvenEmpty ? TEXT("true") : TEXT("false"),
            *ResidualStateError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool RollbackFreshMaterialization(
    const FFreshNamespaceBaseline& Baseline,
    FString& InOutError)
{
    if (!Baseline.bOutputRootProvenEmptyAtEntry ||
        !Baseline.bStagingRootProvenEmptyAtEntry)
    {
        if (!InOutError.IsEmpty())
        {
            InOutError += TEXT(" ");
        }
        InOutError += TEXT("ROLLBACK_REFUSED: isolated namespaces were not both proven empty at transaction entry.");
        return false;
    }

    FString OutputRollbackError;
    FString StagingRollbackError;
    const bool bOutputClean = DeleteFreshNamespaceContents(
        OutputRoot,
        OutputRollbackError);
    // Always attempt both roots, even if cleanup of the first one reports a
    // failure, so a texture/mesh failure cannot strand the other namespace.
    const bool bStagingClean = DeleteFreshNamespaceContents(
        StagingRoot,
        StagingRollbackError);
    if (!bOutputClean || !bStagingClean)
    {
        if (!InOutError.IsEmpty())
        {
            InOutError += TEXT(" ");
        }
        InOutError += FString::Printf(
            TEXT("ROLLBACK_FAILED output='%s' staging='%s'"),
            *OutputRollbackError,
            *StagingRollbackError);
        return false;
    }
    if (!InOutError.IsEmpty())
    {
        InOutError += TEXT(" ");
    }
    InOutError += TEXT("ROLLBACK_COMPLETE recursivelyDeletedFreshOutputAndStagingNamespaces=true registryAssets=0 liveAssets=0 loadedPackages=0");
    return true;
}

bool RemoveFreshStagingAssets(FString& OutError)
{
    TArray<FString> Paths;
    GetStagingObjectPaths(Paths);
    return DeleteExactFreshAssets(Paths, OutError);
}
} // namespace

bool UTRIADIstanaExploreV5DPalmHeroEditorLibrary::
    InspectPalmHeroReceipts(
        const FString& PalmHeroCandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            PalmHeroCandidateRoot,
            AcceptedR33ReceiptPath,
            ExpectedAcceptedR33ReceiptSha256,
            FutureTransactionAuthorizationPath,
            ExpectedFutureTransactionAuthorizationSha256,
            false,
            Admission,
            Error))
    {
        OutReport = TEXT("ISTANA_PALM_HERO_RECEIPT_INSPECTION_DENIED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_PALM_HERO_RECEIPTS_INSPECTED_ONLY executionAuthorized=false callerSuppliedHashesNeverAuthorizeExecution=true privateMaterializerInvoked=false sourceOnlyRead=true acceptedR33ReceiptHashMatchedCallerPinAndHumanAcceptedFields=true futureExplicitTransactionHashMatchedCallerPinAndExactNarrowFields=true candidateContractProvenanceObjMtlAndTexturesHashPinned=true assetsWritten=false mapModified=false existingPalmFallbackReplaced=false collisionNavigationLosRfSensorTerrainAuthorityModified=false UnrealLaunched=false nativeCompileImportRuntimeCaptureOrVisualAcceptanceClaimed=false");
    return true;
}

bool UTRIADIstanaExploreV5DPalmHeroEditorLibrary::
    MaterializeTrustedPalmHeroAssetsInternal(
        const FString& PalmHeroCandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            PalmHeroCandidateRoot,
            AcceptedR33ReceiptPath,
            ExpectedAcceptedR33ReceiptSha256,
            FutureTransactionAuthorizationPath,
            ExpectedFutureTransactionAuthorizationSha256,
            true,
            Admission,
            Error))
    {
        OutReport = TEXT("ISTANA_PALM_HERO_MATERIALIZATION_DENIED: ") +
            Error;
        return false;
    }

    if (HasUnexpectedOutputNamespaceAssets(Error))
    {
        OutReport = TEXT("ISTANA_PALM_HERO_UNEXPECTED_NAMESPACE_ASSET_DENIED: ") +
            Error;
        return false;
    }
    TArray<FString> ExpectedOutputPaths;
    TArray<FString> ExpectedStagingPaths;
    const TArray<FString> EmptyExpectedPaths;
    GetOutputObjectPaths(ExpectedOutputPaths);
    GetStagingObjectPaths(ExpectedStagingPaths);
    if (!NamespaceMatchesExactAssets(
            StagingRoot,
            EmptyExpectedPaths,
            Error))
    {
        OutReport = TEXT("ISTANA_PALM_HERO_STAGING_NAMESPACE_NOT_EMPTY_DENIED: ") +
            Error;
        return false;
    }
    const int32 ExistingCount = ExistingOutputPackageCount();
    if (ExistingCount == OutputAssetCount)
    {
        if (!LoadAndValidateOutputAssets(Admission, nullptr, Error) ||
            !NamespaceMatchesExactAssets(
                OutputRoot,
                ExpectedOutputPaths,
                Error))
        {
            OutReport = TEXT("ISTANA_PALM_HERO_EXISTING_ASSETS_INVALID: ") +
                Error;
            return false;
        }
        OutReport = TEXT("ISTANA_PALM_HERO_ASSETS_VALID existing=8 created=0 lods=3 exactPerAssetSourceProvenanceAndAuthorizationMetadata=true recursiveNamespaceExact=true materialSlots=Bark,FrondLive,FrondDry optionalSourceAvailable=true existingPalmFallbackPreserved=true mapModified=false sourceTransformsModified=false collisionNavigationLosRfSensorTerrainAuthorityModified=false nativeColdReloadRuntimeCaptureOrVisualAcceptanceClaimed=false");
        return true;
    }
    if (ExistingCount != 0)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_PALM_HERO_PARTIAL_NAMESPACE_DENIED existing=%d expectedEither=0_or_8"),
            ExistingCount);
        return false;
    }
    if (!NamespaceMatchesExactAssets(
            OutputRoot,
            EmptyExpectedPaths,
            Error))
    {
        OutReport = TEXT("ISTANA_PALM_HERO_OUTPUT_NAMESPACE_NOT_EMPTY_DENIED: ") +
            Error;
        return false;
    }
    FFreshNamespaceBaseline FreshBaseline;
    FreshBaseline.bOutputRootProvenEmptyAtEntry = true;
    FreshBaseline.bStagingRootProvenEmptyAtEntry = true;

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
            .Get();
    TArray<UObject*> AssetsToSave;
    TArray<UTexture2D*> Textures;
    TArray<UMaterial*> Materials;
    TArray<UStaticMesh*> StagingLods;
    UStaticMesh* FinalMesh = nullptr;
    if (!ImportTextures(
            AssetTools,
            Admission,
            Textures,
            AssetsToSave,
            Error) ||
        !CreateMaterials(
            AssetTools,
            Admission,
            Textures,
            Materials,
            AssetsToSave,
            Error) ||
        !ImportStagingLods(
            AssetTools,
            Admission,
            StagingLods,
            Error) ||
        !CreateFinalMesh(
            Admission,
            StagingLods,
            Materials,
            FinalMesh,
            Error))
    {
        RollbackFreshMaterialization(FreshBaseline, Error);
        OutReport = TEXT("ISTANA_PALM_HERO_CREATE_FAILED_BEFORE_SAVE: ") +
            Error;
        return false;
    }
    AssetsToSave.Add(FinalMesh);
    TSet<FString> ExactPaths;
    for (const UObject* Asset : AssetsToSave)
    {
        ExactPaths.Add(Asset ? Asset->GetPathName() : FString());
    }
    TArray<UObject*> ValidatedAssets;
    if (AssetsToSave.Num() != OutputAssetCount ||
        ExactPaths.Num() != OutputAssetCount ||
        ExactPaths.Contains(FString()) ||
        !NamespaceMatchesExactAssets(
            OutputRoot,
            ExpectedOutputPaths,
            Error) ||
        !NamespaceMatchesExactAssets(
            StagingRoot,
            ExpectedStagingPaths,
            Error) ||
        !LoadAndValidateOutputAssets(
            Admission,
            &ValidatedAssets,
            Error))
    {
        RollbackFreshMaterialization(FreshBaseline, Error);
        OutReport = TEXT("ISTANA_PALM_HERO_VALIDATION_FAILED_BEFORE_SAVE: ") +
            Error;
        return false;
    }
    if (!RemoveFreshStagingAssets(Error))
    {
        RollbackFreshMaterialization(FreshBaseline, Error);
        OutReport = TEXT("ISTANA_PALM_HERO_STAGING_CLEANUP_FAILED_BEFORE_SAVE: ") +
            Error;
        return false;
    }
    if (!NamespaceMatchesExactAssets(
            StagingRoot,
            EmptyExpectedPaths,
            Error) ||
        !NamespaceMatchesExactAssets(
            OutputRoot,
            ExpectedOutputPaths,
            Error))
    {
        RollbackFreshMaterialization(FreshBaseline, Error);
        OutReport = TEXT("ISTANA_PALM_HERO_NAMESPACE_VALIDATION_FAILED_BEFORE_SAVE: ") +
            Error;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem ||
        !AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        Error = TEXT("ISTANA_PALM_HERO_EXACT_EIGHT_ASSET_SAVE_FAILED");
        RollbackFreshMaterialization(FreshBaseline, Error);
        OutReport = Error;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!LoadAndValidateOutputAssets(Admission, nullptr, Error) ||
        !NamespaceMatchesExactAssets(
            OutputRoot,
            ExpectedOutputPaths,
            Error) ||
        !NamespaceMatchesExactAssets(
            StagingRoot,
            EmptyExpectedPaths,
            Error))
    {
        RollbackFreshMaterialization(FreshBaseline, Error);
        OutReport = TEXT("ISTANA_PALM_HERO_POST_SAVE_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_PALM_HERO_ASSETS_MATERIALIZED created=8 lods=3 exactObjHashes=true exactSourceMeshDescriptions=true sourceToUnrealCoordinateConversion=X,-Y,Z exactRootAndConvertedBounds=true materialSlots=Bark,FrondLive,FrondDry barkTextures=4 exactPerAssetProvenanceAndAuthorizationMetadata=true outputNamespaceComplete=true recursiveRegistryLiveObjectAndPackageScan=true stagingNamespaceEmpty=true failureRollbackScope=bothIsolatedNamespacesProvenEmptyAtEntry optionalSourceAvailable=true existingPalmFallbackPreserved=true mapModified=false sourceMeshesModified=false sourceTransformsModified=false collisionNavigationLosRfSensorTerrainAuthorityModified=false nativeColdReloadRuntimeCapturePerformanceOrVisualAcceptanceStillRequired=true");
    return true;
}
