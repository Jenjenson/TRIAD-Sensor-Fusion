#include "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "IAssetTools.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Ssl.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5CSurroundingsAssetFactory.h"
#include "TRIADIstanaExploreV5DCurrentSurroundingsProvenance.h"
#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.h"
#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <cfloat>

#if WITH_SSL
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#include <openssl/sha.h>
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#endif

namespace
{
const FString SuppressedAssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LocalFallbackSuppressionV1"));
const FString SuppressedMeshName(
    TEXT("SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v1"));
const FString SuppressedMeshObjectPath(
    SuppressedAssetRoot + TEXT("/") + SuppressedMeshName + TEXT(".") +
    SuppressedMeshName);

const FString V5COfficialWallPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OfficialContextRender.M_IPV5C_OfficialContextRender"));
const FString V5COfficialRoofPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OfficialContextRoof.M_IPV5C_OfficialContextRoof"));
const FString V5CFallbackWallPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OsmFallbackContextRender.M_IPV5C_OsmFallbackContextRender"));
const FString V5CFallbackRoofPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/"
         "M_IPV5C_OsmFallbackContextRoof.M_IPV5C_OsmFallbackContextRoof"));

constexpr int32 SuppressedExpectedAssetCount = 1;
constexpr int32 SuppressedExpectedSourceVertexLines = 24522;
constexpr int32 SuppressedExpectedSourceTextureCoordinateLines = 130632;
constexpr int32 SuppressedExpectedImportedVertexCount = 24492;
constexpr int32 SuppressedExpectedVertexInstanceCount = 130476;
constexpr int32 SuppressedExpectedTriangleCount = 43492;
constexpr int32 SuppressedExpectedMaterialCount = 17;

constexpr int64 SuppressedExpectedObjBytes = 6356433;
constexpr int64 SuppressedExpectedMtlBytes = 2645;
constexpr int64 SuppressedExpectedMetadataBytes = 93879;
constexpr int64 SuppressedExpectedManifestBytes = 1761;
constexpr int64 SuppressedExpectedContractBytes = 6284;

const FString SuppressedExpectedObjSha256(
    TEXT("C4781C95EBE88387A57260BD2D8BBC4CD132BA38D26AE007F862BA81FD9F31E9"));
const FString SuppressedExpectedMtlSha256(
    TEXT("751DE195642892F781731EBD0F9EB3C05F23731CC674F76E7EF335E4F909342A"));
const FString SuppressedExpectedMetadataSha256(
    TEXT("154BE543F3E7398900F39658FDE41604ABE4642A11EFE097DF1C9C3F8BF1CC61"));
const FString SuppressedExpectedManifestSha256(
    TEXT("D8627FECAC184B9B658E8C36048544A026811D56E130336441792FA997A9B88D"));
const FString SuppressedExpectedContractSha256(
    TEXT("68D68B4D906076A49AA070C7341D38245100A7577C4AF7C66C2D62C9A12D0BB5"));
const FString SuppressedExpectedOutputSetSha256(
    TEXT("DD61A0746D68899217E87470F3C07BA29FB37334DC11120833E15A59732AE56C"));

// V2 is deliberately separate from the live V1 asset. Importing it creates a
// new additive asset root and never rewrites either the canonical source or V1.
const FString SuppressedV2AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LocalFallbackSuppressionV2"));
const FString SuppressedV2MeshName(
    TEXT("SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2"));
const FString SuppressedV2MeshObjectPath(
    SuppressedV2AssetRoot + TEXT("/") + SuppressedV2MeshName + TEXT(".") +
    SuppressedV2MeshName);
constexpr int32 SuppressedV2ExpectedImportedVertexCount = 24468;
constexpr int32 SuppressedV2ExpectedVertexInstanceCount = 130344;
constexpr int32 SuppressedV2ExpectedTriangleCount = 43448;
constexpr int64 SuppressedV2ExpectedObjBytes = 6354063;
constexpr int64 SuppressedV2ExpectedMetadataBytes = 94841;
constexpr int64 SuppressedV2ExpectedManifestBytes = 1763;
constexpr int64 SuppressedV2ExpectedContractBytes = 6952;
const FString SuppressedV2ExpectedObjSha256(
    TEXT("99175681A1F307D02D8FD01E09850AD017B782BCD4A56043F64B0EA285703110"));
const FString SuppressedV2ExpectedMetadataSha256(
    TEXT("31A32BCB8DAED756E0B8D90D0EE795A43B389BFB3148322A0FAC761A9BD73477"));
const FString SuppressedV2ExpectedManifestSha256(
    TEXT("7D455FE8C1E057F2380BE4941026F511AE5D5A1817795F238496FCEAD77EDE04"));
const FString SuppressedV2ExpectedContractSha256(
    TEXT("EAA570EC3F6DCA0B47CD4F346E73EE879E9C94AC951CE5FE456E3C4DFCFAB6A6"));
const FString SuppressedV2ExpectedOutputSetSha256(
    TEXT("FBE8F7D0C8BB935A3DFC2AE2953B9480902B4CCBFBE7765170FC09AF49081120"));

constexpr int64 CanonicalRenderObjBytes = 6359246;
constexpr int64 CanonicalRfObjBytes = 1796000;
constexpr int64 CanonicalRfMtlBytes = 216;
const FString CanonicalRenderObjSha256(
    TEXT("1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3"));
const FString CanonicalRfObjSha256(
    TEXT("2B329516E24C081C7773DB984510EBD1E0C87CDB522CB490170B702D5F564324"));
const FString CanonicalRfMtlSha256(
    TEXT("107E25A2EEB7DFF92356CFBF8E1DA99C3329CE66F75CD1AE15A6EDD8D74F1A47"));

enum class ESuppressedV5CMaterialRole : uint8
{
    OfficialWall = 0,
    OfficialRoof = 1,
    FallbackWall = 2,
    FallbackRoof = 3,
};

struct FSuppressedMaterialSpec
{
    const TCHAR* Name;
    int32 Triangles;
    ESuppressedV5CMaterialRole Role;
};

const FSuppressedMaterialSpec SuppressedMaterialSpecs[] = {
    {TEXT("MAT_BOTTOM_HIDDEN"), 9446, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_COMMERCIAL_HINT"), 2166, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_GENERIC_BUILDING_HINT"), 14696, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_HEALTHCARE_HINT"), 278, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_HOTEL_HINT"), 414, ESuppressedV5CMaterialRole::OfficialWall},
    {TEXT("MAT_INDUSTRIAL_HINT"), 8, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_RELIGIOUS_HINT"), 150, ESuppressedV5CMaterialRole::OfficialWall},
    {TEXT("MAT_RESIDENTIAL_HINT"), 6340, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_ROOF_COMMERCIAL_HINT"), 1029, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"), 5452, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_HEALTHCARE_HINT"), 131, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_HOTEL_HINT"), 169, ESuppressedV5CMaterialRole::OfficialRoof},
    {TEXT("MAT_ROOF_INDUSTRIAL_HINT"), 2, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_RELIGIOUS_HINT"), 53, ESuppressedV5CMaterialRole::OfficialRoof},
    {TEXT("MAT_ROOF_RESIDENTIAL_HINT"), 2694, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_TRANSPORT_HINT"), 132, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_TRANSPORT_HINT"), 332, ESuppressedV5CMaterialRole::FallbackWall},
};
static_assert(
    UE_ARRAY_COUNT(SuppressedMaterialSpecs) ==
    SuppressedExpectedMaterialCount);

const FSuppressedMaterialSpec SuppressedV2MaterialSpecs[] = {
    {TEXT("MAT_BOTTOM_HIDDEN"), 9436, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_COMMERCIAL_HINT"), 2166, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_GENERIC_BUILDING_HINT"), 14696, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_HEALTHCARE_HINT"), 278, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_HOTEL_HINT"), 414, ESuppressedV5CMaterialRole::OfficialWall},
    {TEXT("MAT_INDUSTRIAL_HINT"), 8, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_RELIGIOUS_HINT"), 150, ESuppressedV5CMaterialRole::OfficialWall},
    {TEXT("MAT_RESIDENTIAL_HINT"), 6316, ESuppressedV5CMaterialRole::FallbackWall},
    {TEXT("MAT_ROOF_COMMERCIAL_HINT"), 1029, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"), 5452, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_HEALTHCARE_HINT"), 131, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_HOTEL_HINT"), 169, ESuppressedV5CMaterialRole::OfficialRoof},
    {TEXT("MAT_ROOF_INDUSTRIAL_HINT"), 2, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_RELIGIOUS_HINT"), 53, ESuppressedV5CMaterialRole::OfficialRoof},
    {TEXT("MAT_ROOF_RESIDENTIAL_HINT"), 2684, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_ROOF_TRANSPORT_HINT"), 132, ESuppressedV5CMaterialRole::FallbackRoof},
    {TEXT("MAT_TRANSPORT_HINT"), 332, ESuppressedV5CMaterialRole::FallbackWall},
};
static_assert(
    UE_ARRAY_COUNT(SuppressedV2MaterialSpecs) ==
    SuppressedExpectedMaterialCount);

FString CurrentSourcePath(const TCHAR* Filename)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent"),
        Filename));
}

FString SuppressionSourcePath(const TCHAR* Filename)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/"
             "LocalFallbackSuppressionV1"),
        Filename));
}

FString SuppressedObjSourcePath()
{
    return SuppressionSourcePath(
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render_"
             "LocalFallbackSuppressed_v1.obj"));
}

FString SuppressedMtlSourcePath()
{
    return SuppressionSourcePath(
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render.mtl"));
}

FString SuppressedMetadataSourcePath()
{
    return SuppressionSourcePath(
        TEXT("IstanaPublicViewV5DLocalFallbackSuppression.v1.metadata.json"));
}

FString SuppressedManifestSourcePath()
{
    return SuppressionSourcePath(
        TEXT("IstanaPublicViewV5DLocalFallbackSuppression.v1.manifest.json"));
}

FString SuppressedContractSourcePath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/"
             "local_fallback_suppression_v1.contract.json")));
}

FString SuppressionV2SourcePath(const TCHAR* Filename)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/"
             "LocalFallbackSuppressionV2"),
        Filename));
}

FString SuppressedV2ObjSourcePath()
{
    return SuppressionV2SourcePath(
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render_"
             "LocalFallbackSuppressed_v2.obj"));
}

FString SuppressedV2MtlSourcePath()
{
    return SuppressionV2SourcePath(
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render.mtl"));
}

FString SuppressedV2MetadataSourcePath()
{
    return SuppressionV2SourcePath(
        TEXT("IstanaPublicViewV5DLocalFallbackSuppression.v2.metadata.json"));
}

FString SuppressedV2ManifestSourcePath()
{
    return SuppressionV2SourcePath(
        TEXT("IstanaPublicViewV5DLocalFallbackSuppression.v2.manifest.json"));
}

FString SuppressedV2ContractSourcePath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/"
             "local_fallback_suppression_v2.contract.json")));
}

template <typename T>
T* LoadSuppressedExact(const FString& ObjectPath)
{
    T* Object = LoadObject<T>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
}

bool ValidateSuppressedSourceHash(
    const FString& Filename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutError)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != ExpectedBytes)
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression source byte guard failed for '%s': expected=%lld actual=%d."),
            *Filename,
            ExpectedBytes,
            Bytes.Num());
        return false;
    }

    FString ActualSha256;
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(),
            static_cast<size_t>(Bytes.Num()),
            Digest) == nullptr)
    {
        OutError = TEXT("V5D local-fallback suppression SHA-256 computation failed for '") +
            Filename + TEXT("'.");
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    ActualSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("V5D local-fallback suppression SHA-256 admission requires WITH_SSL for '") +
        Filename + TEXT("'.");
    return false;
#endif
    if (ActualSha256 != ExpectedSha256)
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression SHA-256 guard failed for '%s': expected=%s actual=%s."),
            *Filename,
            *ExpectedSha256,
            *ActualSha256);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedSourceHashes(FString& OutError)
{
    // The three parent sources below are re-admitted only to prove that the
    // render successor never replaced its canonical render or RF inputs.
    return ValidateSuppressedSourceHash(
               CurrentSourcePath(
                   TEXT("SM_IPV5D_OSMCurrentSurroundings_Render.obj")),
               CanonicalRenderObjBytes,
               CanonicalRenderObjSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               CurrentSourcePath(
                   TEXT("SM_IPV5D_OSMCurrentSurroundings_RFShell.obj")),
               CanonicalRfObjBytes,
               CanonicalRfObjSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               CurrentSourcePath(
                   TEXT("SM_IPV5D_OSMCurrentSurroundings_RFShell.mtl")),
               CanonicalRfMtlBytes,
               CanonicalRfMtlSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedObjSourcePath(),
               SuppressedExpectedObjBytes,
               SuppressedExpectedObjSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedMtlSourcePath(),
               SuppressedExpectedMtlBytes,
               SuppressedExpectedMtlSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedMetadataSourcePath(),
               SuppressedExpectedMetadataBytes,
               SuppressedExpectedMetadataSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedManifestSourcePath(),
               SuppressedExpectedManifestBytes,
               SuppressedExpectedManifestSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedContractSourcePath(),
               SuppressedExpectedContractBytes,
               SuppressedExpectedContractSha256,
               OutError);
}

bool ValidateSuppressedV2SourceHashes(FString& OutError)
{
    // V2 remains a render-only derivative. Re-admit the unchanged canonical
    // render/RF inputs and the exact independent V2 output set before import.
    return ValidateSuppressedSourceHash(
               CurrentSourcePath(
                   TEXT("SM_IPV5D_OSMCurrentSurroundings_Render.obj")),
               CanonicalRenderObjBytes,
               CanonicalRenderObjSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               CurrentSourcePath(
                   TEXT("SM_IPV5D_OSMCurrentSurroundings_RFShell.obj")),
               CanonicalRfObjBytes,
               CanonicalRfObjSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               CurrentSourcePath(
                   TEXT("SM_IPV5D_OSMCurrentSurroundings_RFShell.mtl")),
               CanonicalRfMtlBytes,
               CanonicalRfMtlSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedV2ObjSourcePath(),
               SuppressedV2ExpectedObjBytes,
               SuppressedV2ExpectedObjSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedV2MtlSourcePath(),
               SuppressedExpectedMtlBytes,
               SuppressedExpectedMtlSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedV2MetadataSourcePath(),
               SuppressedV2ExpectedMetadataBytes,
               SuppressedV2ExpectedMetadataSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedV2ManifestSourcePath(),
               SuppressedV2ExpectedManifestBytes,
               SuppressedV2ExpectedManifestSha256,
               OutError) &&
        ValidateSuppressedSourceHash(
               SuppressedV2ContractSourcePath(),
               SuppressedV2ExpectedContractBytes,
               SuppressedV2ExpectedContractSha256,
               OutError);
}

bool LoadSuppressedJson(
    const FString& Filename,
    TSharedPtr<FJsonObject>& OutObject,
    FString& OutError)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Filename))
    {
        OutError = TEXT("Could not read V5D local-fallback suppression JSON: ") +
            Filename;
        return false;
    }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, OutObject) ||
        !OutObject.IsValid())
    {
        OutError = TEXT("Could not parse V5D local-fallback suppression JSON: ") +
            Filename;
        return false;
    }
    OutError.Reset();
    return true;
}

bool HasExactString(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const TCHAR* Expected)
{
    FString Actual;
    return Object.IsValid() && Object->TryGetStringField(Field, Actual) &&
        Actual == Expected;
}

bool HasExactNumber(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    double Expected)
{
    double Actual = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(Field, Actual) &&
        Actual == Expected;
}

bool HasExactBool(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    bool Expected)
{
    bool Actual = false;
    return Object.IsValid() && Object->TryGetBoolField(Field, Actual) &&
        Actual == Expected;
}

bool ValidateSuppressedAuthority(
    const TSharedPtr<FJsonObject>& Object)
{
    return HasExactBool(Object, TEXT("renderOnly"), true) &&
        HasExactBool(Object, TEXT("localFallbackOnly"), true) &&
        HasExactBool(Object, TEXT("providerOverlapResolved"), false) &&
        HasExactBool(Object, TEXT("collisionEnabled"), false) &&
        HasExactBool(Object, TEXT("navigationAuthority"), false) &&
        HasExactBool(Object, TEXT("sensorOcclusionAuthority"), false) &&
        HasExactBool(Object, TEXT("rfGeometryAuthority"), false) &&
        HasExactBool(Object, TEXT("rfMaterialAuthority"), false) &&
        HasExactBool(Object, TEXT("surveyOrAsBuiltAuthority"), false) &&
        HasExactBool(Object, TEXT("facadeOrApertureAuthority"), false) &&
        HasExactBool(Object, TEXT("measuredHeightClaimed"), false);
}

bool ValidateSuppressedJsonContracts(FString& OutError)
{
    TSharedPtr<FJsonObject> Metadata;
    TSharedPtr<FJsonObject> Manifest;
    if (!LoadSuppressedJson(
            SuppressedMetadataSourcePath(), Metadata, OutError) ||
        !LoadSuppressedJson(
            SuppressedManifestSourcePath(), Manifest, OutError))
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* MetadataAuthority = nullptr;
    const TSharedPtr<FJsonObject>* ManifestAuthority = nullptr;
    const TSharedPtr<FJsonObject>* Suppression = nullptr;
    const TSharedPtr<FJsonObject>* CanonicalCounts = nullptr;
    const TSharedPtr<FJsonObject>* FilteredCounts = nullptr;
    const TSharedPtr<FJsonObject>* Preservation = nullptr;
    if (!HasExactString(
            Metadata,
            TEXT("schema"),
            TEXT("triad.istana_explore_v5d.local_fallback_suppression_metadata.v1")) ||
        !HasExactString(Metadata, TEXT("sourceEpoch"), TEXT("2026-08-31")) ||
        !HasExactString(
            Manifest,
            TEXT("schema"),
            TEXT("triad.istana_explore_v5d.local_fallback_suppression_manifest.v1")) ||
        !HasExactString(
            Manifest,
            TEXT("outputSetSha256"),
            *SuppressedExpectedOutputSetSha256) ||
        !Metadata->TryGetObjectField(TEXT("authorityPolicy"), MetadataAuthority) ||
        !Manifest->TryGetObjectField(TEXT("authorityPolicy"), ManifestAuthority) ||
        !ValidateSuppressedAuthority(*MetadataAuthority) ||
        !ValidateSuppressedAuthority(*ManifestAuthority) ||
        !Metadata->TryGetObjectField(TEXT("suppression"), Suppression) ||
        !Metadata->TryGetObjectField(TEXT("canonicalCounts"), CanonicalCounts) ||
        !Metadata->TryGetObjectField(TEXT("filteredCounts"), FilteredCounts) ||
        !Metadata->TryGetObjectField(TEXT("preservation"), Preservation) ||
        !HasExactNumber(*CanonicalCounts, TEXT("triangles"), 43544.0) ||
        !HasExactNumber(*FilteredCounts, TEXT("triangles"), 43492.0) ||
        !HasExactNumber(*FilteredCounts, TEXT("vertexLines"), 24522.0) ||
        !HasExactNumber(
            *FilteredCounts,
            TEXT("textureCoordinateLines"),
            130632.0) ||
        !HasExactBool(
            *Preservation,
            TEXT("sourceInputsMatchedBeforeAndAfter"),
            true) ||
        !HasExactBool(
            *Preservation,
            TEXT("rfObjUnchanged"),
            true) ||
        !HasExactBool(
            *Preservation,
            TEXT("rfMtlUnchanged"),
            true) ||
        !HasExactBool(
            *Preservation,
            TEXT("allRemainingFaceLinesRetainedByteForByteAndInSourceOrder"),
            true))
    {
        OutError = TEXT("V5D local-fallback suppression metadata/manifest root contract changed.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* SourceKeys = nullptr;
    if (!(*Suppression)->TryGetArrayField(TEXT("sourceKeys"), SourceKeys) ||
        !SourceKeys || SourceKeys->Num() != 2)
    {
        OutError = TEXT("V5D local-fallback suppression must contain exactly two source keys.");
        return false;
    }
    const TCHAR* ExpectedKeys[] = {
        TEXT("OSM:way:46521250"),
        TEXT("OSM:way:1551538490"),
    };
    const TCHAR* ExpectedGroups[] = {
        TEXT("OSM_way_46521250_P00"),
        TEXT("OSM_way_1551538490_P00"),
    };
    const int32 ExpectedTriangleStarts[] = {868, 41976};
    const int32 ExpectedTriangleCounts[] = {12, 40};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const TSharedPtr<FJsonObject> Row = (*SourceKeys)[Index].IsValid()
            ? (*SourceKeys)[Index]->AsObject()
            : nullptr;
        const TArray<TSharedPtr<FJsonValue>>* TriangleRange = nullptr;
        if (!HasExactString(Row, TEXT("sourceKey"), ExpectedKeys[Index]) ||
            !HasExactString(Row, TEXT("objGroup"), ExpectedGroups[Index]) ||
            !Row->TryGetArrayField(TEXT("triangleRange"), TriangleRange) ||
            !TriangleRange || TriangleRange->Num() != 2 ||
            (*TriangleRange)[0]->AsNumber() !=
                static_cast<double>(ExpectedTriangleStarts[Index]) ||
            (*TriangleRange)[1]->AsNumber() !=
                static_cast<double>(ExpectedTriangleCounts[Index]))
        {
            OutError = TEXT("V5D local-fallback suppression exact two-key roster changed.");
            return false;
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("outputs"), Outputs) || !Outputs ||
        Outputs->Num() != 3)
    {
        OutError = TEXT("V5D local-fallback suppression manifest output roster changed.");
        return false;
    }
    const TCHAR* ExpectedOutputFiles[] = {
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v1.obj"),
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render.mtl"),
        TEXT("IstanaPublicViewV5DLocalFallbackSuppression.v1.metadata.json"),
    };
    const int64 ExpectedOutputBytes[] = {
        SuppressedExpectedObjBytes,
        SuppressedExpectedMtlBytes,
        SuppressedExpectedMetadataBytes,
    };
    const FString ExpectedOutputHashes[] = {
        SuppressedExpectedObjSha256,
        SuppressedExpectedMtlSha256,
        SuppressedExpectedMetadataSha256,
    };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const TSharedPtr<FJsonObject> Row = (*Outputs)[Index].IsValid()
            ? (*Outputs)[Index]->AsObject()
            : nullptr;
        if (!HasExactString(Row, TEXT("file"), ExpectedOutputFiles[Index]) ||
            !HasExactNumber(
                Row,
                TEXT("bytes"),
                static_cast<double>(ExpectedOutputBytes[Index])) ||
            !HasExactString(
                Row,
                TEXT("sha256"),
                *ExpectedOutputHashes[Index]))
        {
            OutError = TEXT("V5D local-fallback suppression manifest file record changed.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedV2JsonContracts(FString& OutError)
{
    TSharedPtr<FJsonObject> Metadata;
    TSharedPtr<FJsonObject> Manifest;
    if (!LoadSuppressedJson(
            SuppressedV2MetadataSourcePath(), Metadata, OutError) ||
        !LoadSuppressedJson(
            SuppressedV2ManifestSourcePath(), Manifest, OutError))
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* MetadataAuthority = nullptr;
    const TSharedPtr<FJsonObject>* ManifestAuthority = nullptr;
    const TSharedPtr<FJsonObject>* Suppression = nullptr;
    const TSharedPtr<FJsonObject>* CanonicalCounts = nullptr;
    const TSharedPtr<FJsonObject>* FilteredCounts = nullptr;
    const TSharedPtr<FJsonObject>* Preservation = nullptr;
    if (!HasExactString(
            Metadata,
            TEXT("schema"),
            TEXT("triad.istana_explore_v5d.local_fallback_suppression_metadata.v2")) ||
        !HasExactString(Metadata, TEXT("sourceEpoch"), TEXT("2026-08-31")) ||
        !HasExactString(
            Manifest,
            TEXT("schema"),
            TEXT("triad.istana_explore_v5d.local_fallback_suppression_manifest.v2")) ||
        !HasExactString(
            Manifest,
            TEXT("outputSetSha256"),
            *SuppressedV2ExpectedOutputSetSha256) ||
        !Metadata->TryGetObjectField(TEXT("authorityPolicy"), MetadataAuthority) ||
        !Manifest->TryGetObjectField(TEXT("authorityPolicy"), ManifestAuthority) ||
        !ValidateSuppressedAuthority(*MetadataAuthority) ||
        !ValidateSuppressedAuthority(*ManifestAuthority) ||
        !Metadata->TryGetObjectField(TEXT("suppression"), Suppression) ||
        !Metadata->TryGetObjectField(TEXT("canonicalCounts"), CanonicalCounts) ||
        !Metadata->TryGetObjectField(TEXT("filteredCounts"), FilteredCounts) ||
        !Metadata->TryGetObjectField(TEXT("preservation"), Preservation) ||
        !HasExactNumber(*CanonicalCounts, TEXT("triangles"), 43544.0) ||
        !HasExactNumber(*FilteredCounts, TEXT("triangles"), 43448.0) ||
        !HasExactNumber(*FilteredCounts, TEXT("vertexLines"), 24522.0) ||
        !HasExactNumber(
            *FilteredCounts,
            TEXT("textureCoordinateLines"),
            130632.0) ||
        !HasExactBool(
            *Preservation,
            TEXT("sourceInputsMatchedBeforeAndAfter"),
            true) ||
        !HasExactBool(*Preservation, TEXT("rfObjUnchanged"), true) ||
        !HasExactBool(*Preservation, TEXT("rfMtlUnchanged"), true) ||
        !HasExactBool(
            *Preservation,
            TEXT("allRemainingFaceLinesRetainedByteForByteAndInSourceOrder"),
            true))
    {
        OutError = TEXT("V5D local-fallback suppression V2 metadata/manifest root contract changed.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* SourceKeys = nullptr;
    if (!(*Suppression)->TryGetArrayField(TEXT("sourceKeys"), SourceKeys) ||
        !SourceKeys || SourceKeys->Num() != 3)
    {
        OutError = TEXT("V5D local-fallback suppression V2 must contain exactly three source keys.");
        return false;
    }
    const TCHAR* ExpectedKeys[] = {
        TEXT("OSM:way:46521250"),
        TEXT("OSM:way:1551538490"),
        TEXT("OSM:way:429681826"),
    };
    const TCHAR* ExpectedGroups[] = {
        TEXT("OSM_way_46521250_P00"),
        TEXT("OSM_way_1551538490_P00"),
        TEXT("OSM_way_429681826_P00"),
    };
    const int32 ExpectedTriangleStarts[] = {868, 41976, 27082};
    const int32 ExpectedTriangleCounts[] = {12, 40, 44};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const TSharedPtr<FJsonObject> Row = (*SourceKeys)[Index].IsValid()
            ? (*SourceKeys)[Index]->AsObject()
            : nullptr;
        const TArray<TSharedPtr<FJsonValue>>* TriangleRange = nullptr;
        if (!HasExactString(Row, TEXT("sourceKey"), ExpectedKeys[Index]) ||
            !HasExactString(Row, TEXT("objGroup"), ExpectedGroups[Index]) ||
            !Row->TryGetArrayField(TEXT("triangleRange"), TriangleRange) ||
            !TriangleRange || TriangleRange->Num() != 2 ||
            (*TriangleRange)[0]->AsNumber() !=
                static_cast<double>(ExpectedTriangleStarts[Index]) ||
            (*TriangleRange)[1]->AsNumber() !=
                static_cast<double>(ExpectedTriangleCounts[Index]))
        {
            OutError = TEXT("V5D local-fallback suppression V2 exact three-key roster changed.");
            return false;
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("outputs"), Outputs) || !Outputs ||
        Outputs->Num() != 3)
    {
        OutError = TEXT("V5D local-fallback suppression V2 manifest output roster changed.");
        return false;
    }
    const TCHAR* ExpectedOutputFiles[] = {
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.obj"),
        TEXT("SM_IPV5D_OSMCurrentSurroundings_Render.mtl"),
        TEXT("IstanaPublicViewV5DLocalFallbackSuppression.v2.metadata.json"),
    };
    const int64 ExpectedOutputBytes[] = {
        SuppressedV2ExpectedObjBytes,
        SuppressedExpectedMtlBytes,
        SuppressedV2ExpectedMetadataBytes,
    };
    const FString ExpectedOutputHashes[] = {
        SuppressedV2ExpectedObjSha256,
        SuppressedExpectedMtlSha256,
        SuppressedV2ExpectedMetadataSha256,
    };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const TSharedPtr<FJsonObject> Row = (*Outputs)[Index].IsValid()
            ? (*Outputs)[Index]->AsObject()
            : nullptr;
        if (!HasExactString(Row, TEXT("file"), ExpectedOutputFiles[Index]) ||
            !HasExactNumber(
                Row,
                TEXT("bytes"),
                static_cast<double>(ExpectedOutputBytes[Index])) ||
            !HasExactString(
                Row,
                TEXT("sha256"),
                *ExpectedOutputHashes[Index]))
        {
            OutError = TEXT("V5D local-fallback suppression V2 manifest file record changed.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

const TArray<FString>& ExpectedSuppressedV5CMaterialPaths()
{
    static const TArray<FString> Paths = {
        V5COfficialWallPath,
        V5COfficialRoofPath,
        V5CFallbackWallPath,
        V5CFallbackRoofPath,
    };
    return Paths;
}

bool ValidateSuppressedV5CMaterialDependencies(
    TArray<UMaterialInstanceConstant*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    if (TRIADIstanaExploreV5CSurroundingsAssetFactory::
            GetOrderedMaterialObjectPaths() !=
        ExpectedSuppressedV5CMaterialPaths())
    {
        OutError = TEXT("The V5C surroundings material dependency order changed for the local-fallback suppression asset.");
        return false;
    }
    for (const FString& Path : ExpectedSuppressedV5CMaterialPaths())
    {
        UMaterialInstanceConstant* Material =
            LoadSuppressedExact<UMaterialInstanceConstant>(Path);
        if (!Material ||
            Material->GetClass() != UMaterialInstanceConstant::StaticClass() ||
            !Material->GetOutermost() ||
            !FPackageName::DoesPackageExist(Material->GetOutermost()->GetName()) ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("An exact V5C material dependency is invalid for the local-fallback suppression asset: ") +
                Path;
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    if (OutMaterials.Num() != 4 || OutMaterials.Contains(nullptr))
    {
        OutError = TEXT("The local-fallback suppression asset requires exactly four V5C materials.");
        OutMaterials.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool GatherSuppressedRootAssets(
    TArray<FAssetData>& OutAssets,
    FString& OutError)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    if (Registry.Get().IsLoadingAssets())
    {
        Registry.Get().WaitForCompletion();
    }
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(
        FName(*SuppressedAssetRoot), OutAssets, true, false);
    if (Registry.Get().IsLoadingAssets())
    {
        OutError = TEXT("V5D local-fallback suppression Asset Registry discovery did not complete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedRootRoster(bool bRequireSaved, FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherSuppressedRootAssets(Assets, OutError))
    {
        return false;
    }
    TArray<FString> Actual;
    for (const FAssetData& Asset : Assets)
    {
        Actual.Add(Asset.GetObjectPathString());
    }
    Actual.Sort();
    const TArray<FString> Expected = {SuppressedMeshObjectPath};
    if (Actual != Expected)
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression root must contain exactly one additive mesh; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        UStaticMesh* Mesh =
            LoadSuppressedExact<UStaticMesh>(SuppressedMeshObjectPath);
        if (!Mesh || !Mesh->GetOutermost() ||
            !FPackageName::DoesPackageExist(Mesh->GetOutermost()->GetName()) ||
            Mesh->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The exact additive local-fallback suppression mesh is not persisted and clean.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

class FScopedSuppressedFreshRollback final
{
public:
    explicit FScopedSuppressedFreshRollback(FString& InError)
        : Error(InError)
    {
    }

    ~FScopedSuppressedFreshRollback()
    {
        if (bCommitted)
        {
            return;
        }
        TArray<FAssetData> Data;
        FString Ignored;
        GatherSuppressedRootAssets(Data, Ignored);
        TArray<UObject*> Disposable;
        for (const FAssetData& Row : Data)
        {
            UObject* Object = Row.GetAsset();
            UPackage* Package = Object ? Object->GetOutermost() : nullptr;
            if (Object && Package &&
                Package->HasAnyPackageFlags(PKG_NewlyCreated) &&
                !FPackageName::DoesPackageExist(Package->GetName()))
            {
                Disposable.AddUnique(Object);
            }
        }
        if (!Disposable.IsEmpty() &&
            ObjectTools::DeleteObjectsUnchecked(Disposable) !=
                Disposable.Num())
        {
            Error += TEXT(" V5D_LOCAL_FALLBACK_SUPPRESSION_ROLLBACK_INCOMPLETE");
        }
    }

    void Commit() { bCommitted = true; }

private:
    FString& Error;
    bool bCommitted = false;
};

bool GatherSuppressedV2RootAssets(
    TArray<FAssetData>& OutAssets,
    FString& OutError)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    if (Registry.Get().IsLoadingAssets())
    {
        Registry.Get().WaitForCompletion();
    }
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(
        FName(*SuppressedV2AssetRoot), OutAssets, true, false);
    if (Registry.Get().IsLoadingAssets())
    {
        OutError = TEXT("V5D local-fallback suppression V2 Asset Registry discovery did not complete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedV2RootRoster(bool bRequireSaved, FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherSuppressedV2RootAssets(Assets, OutError))
    {
        return false;
    }
    TArray<FString> Actual;
    for (const FAssetData& Asset : Assets)
    {
        Actual.Add(Asset.GetObjectPathString());
    }
    Actual.Sort();
    const TArray<FString> Expected = {SuppressedV2MeshObjectPath};
    if (Actual != Expected)
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression V2 root must contain exactly one additive mesh; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        UStaticMesh* Mesh =
            LoadSuppressedExact<UStaticMesh>(SuppressedV2MeshObjectPath);
        if (!Mesh || !Mesh->GetOutermost() ||
            !FPackageName::DoesPackageExist(Mesh->GetOutermost()->GetName()) ||
            Mesh->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The exact additive local-fallback suppression V2 mesh is not persisted and clean.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

class FScopedSuppressedV2FreshRollback final
{
public:
    explicit FScopedSuppressedV2FreshRollback(FString& InError)
        : Error(InError)
    {
    }

    ~FScopedSuppressedV2FreshRollback()
    {
        if (bCommitted)
        {
            return;
        }
        TArray<FAssetData> Data;
        FString Ignored;
        GatherSuppressedV2RootAssets(Data, Ignored);
        TArray<UObject*> Disposable;
        for (const FAssetData& Row : Data)
        {
            UObject* Object = Row.GetAsset();
            UPackage* Package = Object ? Object->GetOutermost() : nullptr;
            if (Object && Package &&
                Package->HasAnyPackageFlags(PKG_NewlyCreated) &&
                !FPackageName::DoesPackageExist(Package->GetName()))
            {
                Disposable.AddUnique(Object);
            }
        }
        if (!Disposable.IsEmpty() &&
            ObjectTools::DeleteObjectsUnchecked(Disposable) != Disposable.Num())
        {
            Error += TEXT(" V5D_LOCAL_FALLBACK_SUPPRESSION_V2_ROLLBACK_INCOMPLETE");
        }
    }

    void Commit() { bCommitted = true; }

private:
    FString& Error;
    bool bCommitted = false;
};

UAssetImportTask* MakeSuppressedImportTask()
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
    Options->StaticMeshImportData->bForceFrontXAxis = false;
    Options->StaticMeshImportData->ImportUniformScale = 100.0f;
    Options->StaticMeshImportData->bCombineMeshes = true;
    Options->StaticMeshImportData->bReorderMaterialToFbxOrder = true;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = false;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ComputeNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = true;
    Options->StaticMeshImportData->bRemoveDegenerates = true;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = SuppressedObjSourcePath();
    Task->DestinationPath = SuppressedAssetRoot;
    Task->DestinationName = SuppressedMeshName;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

UAssetImportTask* MakeSuppressedV2ImportTask()
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
    Options->StaticMeshImportData->bForceFrontXAxis = false;
    Options->StaticMeshImportData->ImportUniformScale = 100.0f;
    Options->StaticMeshImportData->bCombineMeshes = true;
    Options->StaticMeshImportData->bReorderMaterialToFbxOrder = true;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = false;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ComputeNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = true;
    Options->StaticMeshImportData->bRemoveDegenerates = true;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = SuppressedV2ObjSourcePath();
    Task->DestinationPath = SuppressedV2AssetRoot;
    Task->DestinationName = SuppressedV2MeshName;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

bool ValidateSuppressedImportTask(
    const UAssetImportTask* Task,
    FString& OutError)
{
    const UFbxImportUI* Options =
        Task ? Cast<UFbxImportUI>(Task->Options) : nullptr;
    const UFbxStaticMeshImportData* Data =
        Options ? Options->StaticMeshImportData : nullptr;
    if (!Task || !Options || !Data ||
        !FPaths::IsSamePath(Task->Filename, SuppressedObjSourcePath()) ||
        Task->DestinationPath != SuppressedAssetRoot ||
        Task->DestinationName != SuppressedMeshName ||
        Options->bImportAsSkeletal ||
        Options->MeshTypeToImport != FBXIT_StaticMesh ||
        Options->bAutomatedImportShouldDetectType || !Options->bImportMesh ||
        Options->bImportMaterials || Options->bImportTextures ||
        Data->bConvertScene || Data->bConvertSceneUnit ||
        Data->bForceFrontXAxis ||
        !FMath::IsNearlyEqual(Data->ImportUniformScale, 100.0f) ||
        !Data->bCombineMeshes || !Data->bReorderMaterialToFbxOrder ||
        Data->bImportMeshLODs || !Data->bTransformVertexToAbsolute ||
        Data->bBakePivotInVertex || Data->bAutoGenerateCollision ||
        Data->bGenerateLightmapUVs ||
        Data->NormalImportMethod !=
            EFBXNormalImportMethod::FBXNIM_ComputeNormals ||
        Data->NormalGenerationMethod !=
            EFBXNormalGenerationMethod::MikkTSpace ||
        !Data->bBuildNanite || !Data->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The V5D local-fallback suppression OBJ import policy changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedV2ImportTask(
    const UAssetImportTask* Task,
    FString& OutError)
{
    const UFbxImportUI* Options =
        Task ? Cast<UFbxImportUI>(Task->Options) : nullptr;
    const UFbxStaticMeshImportData* Data =
        Options ? Options->StaticMeshImportData : nullptr;
    if (!Task || !Options || !Data ||
        !FPaths::IsSamePath(Task->Filename, SuppressedV2ObjSourcePath()) ||
        Task->DestinationPath != SuppressedV2AssetRoot ||
        Task->DestinationName != SuppressedV2MeshName ||
        Options->bImportAsSkeletal ||
        Options->MeshTypeToImport != FBXIT_StaticMesh ||
        Options->bAutomatedImportShouldDetectType || !Options->bImportMesh ||
        Options->bImportMaterials || Options->bImportTextures ||
        Data->bConvertScene || Data->bConvertSceneUnit ||
        Data->bForceFrontXAxis ||
        !FMath::IsNearlyEqual(Data->ImportUniformScale, 100.0f) ||
        !Data->bCombineMeshes || !Data->bReorderMaterialToFbxOrder ||
        Data->bImportMeshLODs || !Data->bTransformVertexToAbsolute ||
        Data->bBakePivotInVertex || Data->bAutoGenerateCollision ||
        Data->bGenerateLightmapUVs ||
        Data->NormalImportMethod !=
            EFBXNormalImportMethod::FBXNIM_ComputeNormals ||
        Data->NormalGenerationMethod !=
            EFBXNormalGenerationMethod::MikkTSpace ||
        !Data->bBuildNanite || !Data->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The V5D local-fallback suppression V2 OBJ import policy changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

void MakeSuppressedRenderOnly(UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return;
    }
    Mesh->Modify();
    if (Mesh->GetNumSourceModels() == 1)
    {
        if (FMeshDescription* Description = Mesh->GetMeshDescription(0))
        {
            const FTransform RestoreIstanaLocalHandedness(
                FQuat::Identity,
                FVector::ZeroVector,
                FVector(1.0, -1.0, 1.0));
            FStaticMeshOperations::ApplyTransform(
                *Description,
                RestoreIstanaLocalHandedness,
                true);
            Mesh->CommitMeshDescription(0);
        }
        FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(0);
        SourceModel.BuildSettings.bGenerateLightmapUVs = false;
        SourceModel.BuildSettings.bUseFullPrecisionUVs = true;
    }
    Mesh->SetLightMapCoordinateIndex(0);
    Mesh->NaniteSettings.bEnabled = true;
    Mesh->NaniteSettings.KeepPercentTriangles = 1.0f;
    Mesh->NaniteSettings.TrimRelativeError = 0.0f;
    Mesh->NaniteSettings.FallbackTarget =
        ENaniteFallbackTarget::PercentTriangles;
    Mesh->NaniteSettings.FallbackPercentTriangles = 1.0f;
    Mesh->NaniteSettings.FallbackRelativeError = 0.0f;
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
    }
    Mesh->MarkAsNotHavingNavigationData();
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
}

int32 CountSuppressedProvenanceObjects(const UStaticMesh* Mesh)
{
    const TArray<UAssetUserData*>* UserData =
        Mesh ? Mesh->GetAssetUserDataArray() : nullptr;
    int32 Count = 0;
    if (UserData)
    {
        for (const UAssetUserData* Datum : *UserData)
        {
            Count += Datum && Datum->IsA(
                UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
                    StaticClass());
        }
    }
    return Count;
}

bool ValidateSuppressedProvenance(
    const UStaticMesh* Mesh,
    FString& OutError)
{
    const TArray<UAssetUserData*>* UserData =
        Mesh ? Mesh->GetAssetUserDataArray() : nullptr;
    const UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance*
        Provenance = nullptr;
    int32 MatchingCount = 0;
    int32 LegacyCanonicalCount = 0;
    if (UserData)
    {
        for (const UAssetUserData* Datum : *UserData)
        {
            if (Datum && Datum->IsA(
                    UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
                        StaticClass()))
            {
                ++MatchingCount;
                Provenance = Cast<
                    UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance>(
                        Datum);
            }
            LegacyCanonicalCount += Datum && Datum->IsA(
                UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
                    StaticClass());
        }
    }
    if (!Mesh || MatchingCount != 1 || LegacyCanonicalCount != 0 ||
        !Provenance || Provenance->GetClass() !=
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
                StaticClass() ||
        Provenance->GetOuter() != Mesh ||
        Provenance->HasAnyFlags(RF_Transient) ||
        !Provenance->IsCanonicalContract())
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression mesh must own exactly one V1 cooked provenance contract and no legacy canonical contract; v1=%d legacy=%d."),
            MatchingCount,
            LegacyCanonicalCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool StampSuppressedProvenance(UStaticMesh* Mesh, FString& OutError)
{
    if (!Mesh)
    {
        OutError = TEXT("Cannot stamp local-fallback suppression provenance on a null mesh.");
        return false;
    }
    Mesh->Modify();
    const int32 ExistingCount = CountSuppressedProvenanceObjects(Mesh);
    for (int32 Index = 0; Index < ExistingCount; ++Index)
    {
        Mesh->RemoveUserDataOfClass(
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
                StaticClass());
    }
    if (CountSuppressedProvenanceObjects(Mesh) != 0)
    {
        OutError = TEXT("Could not remove every prior local-fallback suppression provenance object.");
        return false;
    }
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance* Provenance =
        NewObject<
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance>(
                Mesh, NAME_None, RF_Transactional);
    if (!Provenance)
    {
        OutError = TEXT("Could not allocate local-fallback suppression provenance.");
        return false;
    }
    Provenance->SetCanonicalContract();
    Mesh->AddAssetUserData(Provenance);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    return ValidateSuppressedProvenance(Mesh, OutError);
}

int32 CountSuppressedV2ProvenanceObjects(const UStaticMesh* Mesh)
{
    const TArray<UAssetUserData*>* UserData =
        Mesh ? Mesh->GetAssetUserDataArray() : nullptr;
    int32 Count = 0;
    if (UserData)
    {
        for (const UAssetUserData* Datum : *UserData)
        {
            Count += Datum && Datum->IsA(
                UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance::
                    StaticClass());
        }
    }
    return Count;
}

bool ValidateSuppressedV2Provenance(
    const UStaticMesh* Mesh,
    FString& OutError)
{
    const TArray<UAssetUserData*>* UserData =
        Mesh ? Mesh->GetAssetUserDataArray() : nullptr;
    const UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance*
        Provenance = nullptr;
    int32 MatchingCount = 0;
    int32 V1Count = 0;
    int32 LegacyCanonicalCount = 0;
    if (UserData)
    {
        for (const UAssetUserData* Datum : *UserData)
        {
            if (Datum && Datum->IsA(
                    UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance::
                        StaticClass()))
            {
                ++MatchingCount;
                Provenance = Cast<
                    UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance>(
                        Datum);
            }
            V1Count += Datum && Datum->IsA(
                UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
                    StaticClass());
            LegacyCanonicalCount += Datum && Datum->IsA(
                UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
                    StaticClass());
        }
    }
    if (!Mesh || MatchingCount != 1 || V1Count != 0 ||
        LegacyCanonicalCount != 0 || !Provenance ||
        Provenance->GetClass() !=
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance::
                StaticClass() ||
        Provenance->GetOuter() != Mesh ||
        Provenance->HasAnyFlags(RF_Transient) ||
        !Provenance->IsCanonicalContract())
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression V2 mesh must own exactly one V2 cooked provenance contract and no V1/legacy contract; v2=%d v1=%d legacy=%d."),
            MatchingCount,
            V1Count,
            LegacyCanonicalCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool StampSuppressedV2Provenance(UStaticMesh* Mesh, FString& OutError)
{
    if (!Mesh)
    {
        OutError = TEXT("Cannot stamp local-fallback suppression V2 provenance on a null mesh.");
        return false;
    }
    Mesh->Modify();
    const int32 ExistingV2Count = CountSuppressedV2ProvenanceObjects(Mesh);
    for (int32 Index = 0; Index < ExistingV2Count; ++Index)
    {
        Mesh->RemoveUserDataOfClass(
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance::
                StaticClass());
    }
    // A newly imported V2 asset may never inherit either prior contract.
    if (CountSuppressedV2ProvenanceObjects(Mesh) != 0 ||
        CountSuppressedProvenanceObjects(Mesh) != 0)
    {
        OutError = TEXT("Could not establish a clean V2-only local-fallback suppression provenance roster.");
        return false;
    }
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance* Provenance =
        NewObject<
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance>(
                Mesh, NAME_None, RF_Transactional);
    if (!Provenance)
    {
        OutError = TEXT("Could not allocate local-fallback suppression V2 provenance.");
        return false;
    }
    Provenance->SetCanonicalContract();
    Mesh->AddAssetUserData(Provenance);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    return ValidateSuppressedV2Provenance(Mesh, OutError);
}

bool NormalizeSuppressedMaterials(
    UStaticMesh* Mesh,
    const TArray<UMaterialInstanceConstant*>& V5CMaterials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || V5CMaterials.Num() != 4 || V5CMaterials.Contains(nullptr) ||
        Mesh->GetStaticMaterials().Num() != SuppressedExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() !=
            SuppressedExpectedMaterialCount)
    {
        OutError = TEXT("V5D local-fallback suppression normalization requires the exact 17-slot/17-section mesh and four V5C materials.");
        return false;
    }

    TMap<FName, int32> CanonicalOrder;
    for (int32 CanonicalIndex = 0;
         CanonicalIndex < SuppressedExpectedMaterialCount;
         ++CanonicalIndex)
    {
        const FName Name(SuppressedMaterialSpecs[CanonicalIndex].Name);
        if (Name.IsNone() || CanonicalOrder.Contains(Name))
        {
            OutError = TEXT("The local-fallback suppression material roster is ambiguous.");
            return false;
        }
        CanonicalOrder.Add(Name, CanonicalIndex);
    }

    const TArray<FStaticMaterial> ImportedMaterials =
        Mesh->GetStaticMaterials();
    TArray<FStaticMaterial> OrderedMaterials;
    OrderedMaterials.SetNum(SuppressedExpectedMaterialCount);
    TArray<int32> ImportedToCanonical;
    ImportedToCanonical.Init(INDEX_NONE, SuppressedExpectedMaterialCount);
    TArray<bool> SeenCanonical;
    SeenCanonical.Init(false, SuppressedExpectedMaterialCount);
    for (int32 ImportedIndex = 0;
         ImportedIndex < SuppressedExpectedMaterialCount;
         ++ImportedIndex)
    {
        const FStaticMaterial& Imported = ImportedMaterials[ImportedIndex];
        const int32* CanonicalIndex =
            CanonicalOrder.Find(Imported.MaterialSlotName);
        if (!CanonicalIndex || Imported.MaterialSlotName.IsNone() ||
            Imported.MaterialSlotName != Imported.ImportedMaterialSlotName ||
            SeenCanonical[*CanonicalIndex])
        {
            OutError = TEXT("A local-fallback suppression material slot is not an exact unique semantic-name permutation.");
            return false;
        }
        const int32 DependencyIndex = static_cast<int32>(
            SuppressedMaterialSpecs[*CanonicalIndex].Role);
        if (!V5CMaterials.IsValidIndex(DependencyIndex))
        {
            OutError = TEXT("A local-fallback suppression semantic role has no V5C material binding.");
            return false;
        }
        SeenCanonical[*CanonicalIndex] = true;
        ImportedToCanonical[ImportedIndex] = *CanonicalIndex;
        OrderedMaterials[*CanonicalIndex] = Imported;
        OrderedMaterials[*CanonicalIndex].MaterialSlotName =
            FName(SuppressedMaterialSpecs[*CanonicalIndex].Name);
        OrderedMaterials[*CanonicalIndex].ImportedMaterialSlotName =
            FName(SuppressedMaterialSpecs[*CanonicalIndex].Name);
        OrderedMaterials[*CanonicalIndex].MaterialInterface =
            V5CMaterials[DependencyIndex];
    }

    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    TSet<int32> SeenSections;
    for (int32 SectionIndex = 0; SectionIndex < Lod.Sections.Num(); ++SectionIndex)
    {
        const FStaticMeshSection& RenderSection = Lod.Sections[SectionIndex];
        const int32 ImportedIndex = RenderSection.MaterialIndex;
        FMeshSectionInfo Section =
            Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original =
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (!ImportedToCanonical.IsValidIndex(ImportedIndex) ||
            ImportedToCanonical[ImportedIndex] == INDEX_NONE ||
            Section.MaterialIndex != ImportedIndex ||
            Original.MaterialIndex != ImportedIndex)
        {
            OutError = TEXT("A local-fallback suppression section references an invalid imported material index.");
            return false;
        }
        const int32 CanonicalIndex = ImportedToCanonical[ImportedIndex];
        if (SeenSections.Contains(CanonicalIndex) ||
            RenderSection.NumTriangles !=
                SuppressedMaterialSpecs[CanonicalIndex].Triangles)
        {
            OutError = FString::Printf(
                TEXT("Local-fallback suppression section census changed for '%s': expected=%d actual=%u."),
                SuppressedMaterialSpecs[CanonicalIndex].Name,
                SuppressedMaterialSpecs[CanonicalIndex].Triangles,
                RenderSection.NumTriangles);
            return false;
        }
        SeenSections.Add(CanonicalIndex);
        Section.MaterialIndex = CanonicalIndex;
        Original.MaterialIndex = CanonicalIndex;
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Mesh->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }
    if (SeenSections.Num() != SuppressedExpectedMaterialCount)
    {
        OutError = TEXT("The local-fallback suppression import omitted a semantic section.");
        return false;
    }

    Mesh->Modify();
    Mesh->SetStaticMaterials(OrderedMaterials);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    OutError.Reset();
    return true;
}

bool NormalizeSuppressedV2Materials(
    UStaticMesh* Mesh,
    const TArray<UMaterialInstanceConstant*>& V5CMaterials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || V5CMaterials.Num() != 4 || V5CMaterials.Contains(nullptr) ||
        Mesh->GetStaticMaterials().Num() != SuppressedExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() !=
            SuppressedExpectedMaterialCount)
    {
        OutError = TEXT("V5D local-fallback suppression V2 normalization requires the exact 17-slot/17-section mesh and four V5C materials.");
        return false;
    }

    TMap<FName, int32> CanonicalOrder;
    for (int32 CanonicalIndex = 0;
         CanonicalIndex < SuppressedExpectedMaterialCount;
         ++CanonicalIndex)
    {
        const FName Name(SuppressedV2MaterialSpecs[CanonicalIndex].Name);
        if (Name.IsNone() || CanonicalOrder.Contains(Name))
        {
            OutError = TEXT("The local-fallback suppression V2 material roster is ambiguous.");
            return false;
        }
        CanonicalOrder.Add(Name, CanonicalIndex);
    }

    const TArray<FStaticMaterial> ImportedMaterials =
        Mesh->GetStaticMaterials();
    TArray<FStaticMaterial> OrderedMaterials;
    OrderedMaterials.SetNum(SuppressedExpectedMaterialCount);
    TArray<int32> ImportedToCanonical;
    ImportedToCanonical.Init(INDEX_NONE, SuppressedExpectedMaterialCount);
    TArray<bool> SeenCanonical;
    SeenCanonical.Init(false, SuppressedExpectedMaterialCount);
    for (int32 ImportedIndex = 0;
         ImportedIndex < SuppressedExpectedMaterialCount;
         ++ImportedIndex)
    {
        const FStaticMaterial& Imported = ImportedMaterials[ImportedIndex];
        const int32* CanonicalIndex =
            CanonicalOrder.Find(Imported.MaterialSlotName);
        if (!CanonicalIndex || Imported.MaterialSlotName.IsNone() ||
            Imported.MaterialSlotName != Imported.ImportedMaterialSlotName ||
            SeenCanonical[*CanonicalIndex])
        {
            OutError = TEXT("A local-fallback suppression V2 material slot is not an exact unique semantic-name permutation.");
            return false;
        }
        const int32 DependencyIndex = static_cast<int32>(
            SuppressedV2MaterialSpecs[*CanonicalIndex].Role);
        if (!V5CMaterials.IsValidIndex(DependencyIndex))
        {
            OutError = TEXT("A local-fallback suppression V2 semantic role has no V5C material binding.");
            return false;
        }
        SeenCanonical[*CanonicalIndex] = true;
        ImportedToCanonical[ImportedIndex] = *CanonicalIndex;
        OrderedMaterials[*CanonicalIndex] = Imported;
        OrderedMaterials[*CanonicalIndex].MaterialSlotName =
            FName(SuppressedV2MaterialSpecs[*CanonicalIndex].Name);
        OrderedMaterials[*CanonicalIndex].ImportedMaterialSlotName =
            FName(SuppressedV2MaterialSpecs[*CanonicalIndex].Name);
        OrderedMaterials[*CanonicalIndex].MaterialInterface =
            V5CMaterials[DependencyIndex];
    }

    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    TSet<int32> SeenSections;
    for (int32 SectionIndex = 0; SectionIndex < Lod.Sections.Num(); ++SectionIndex)
    {
        const FStaticMeshSection& RenderSection = Lod.Sections[SectionIndex];
        const int32 ImportedIndex = RenderSection.MaterialIndex;
        FMeshSectionInfo Section =
            Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original =
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (!ImportedToCanonical.IsValidIndex(ImportedIndex) ||
            ImportedToCanonical[ImportedIndex] == INDEX_NONE ||
            Section.MaterialIndex != ImportedIndex ||
            Original.MaterialIndex != ImportedIndex)
        {
            OutError = TEXT("A local-fallback suppression V2 section references an invalid imported material index.");
            return false;
        }
        const int32 CanonicalIndex = ImportedToCanonical[ImportedIndex];
        if (SeenSections.Contains(CanonicalIndex) ||
            RenderSection.NumTriangles !=
                SuppressedV2MaterialSpecs[CanonicalIndex].Triangles)
        {
            OutError = FString::Printf(
                TEXT("Local-fallback suppression V2 section census changed for '%s': expected=%d actual=%u."),
                SuppressedV2MaterialSpecs[CanonicalIndex].Name,
                SuppressedV2MaterialSpecs[CanonicalIndex].Triangles,
                RenderSection.NumTriangles);
            return false;
        }
        SeenSections.Add(CanonicalIndex);
        Section.MaterialIndex = CanonicalIndex;
        Original.MaterialIndex = CanonicalIndex;
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Mesh->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }
    if (SeenSections.Num() != SuppressedExpectedMaterialCount)
    {
        OutError = TEXT("The local-fallback suppression V2 import omitted a semantic section.");
        return false;
    }

    Mesh->Modify();
    Mesh->SetStaticMaterials(OrderedMaterials);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    OutError.Reset();
    return true;
}

bool ValidateSuppressedUv0(
    const FMeshDescription& Description,
    const FStaticMeshLODResources& Lod,
    FString& OutError)
{
    const FStaticMeshConstAttributes Attributes(Description);
    const TVertexInstanceAttributesConstRef<FVector2f> Uvs =
        Attributes.GetVertexInstanceUVs();
    const FStaticMeshVertexBuffer& RenderBuffer =
        Lod.VertexBuffers.StaticMeshVertexBuffer;
    if (Description.VertexInstances().Num() !=
            SuppressedExpectedVertexInstanceCount ||
        Uvs.GetNumElements() != SuppressedExpectedVertexInstanceCount ||
        Uvs.GetNumChannels() != 1 || RenderBuffer.GetNumVertices() == 0 ||
        RenderBuffer.GetNumTexCoords() != 1)
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression UV0 structure changed: vertexInstances=%d uvElements=%d uvChannels=%d renderUvChannels=%d."),
            Description.VertexInstances().Num(),
            Uvs.GetNumElements(),
            Uvs.GetNumChannels(),
            RenderBuffer.GetNumTexCoords());
        return false;
    }
    FVector2f Minimum(FLT_MAX, FLT_MAX);
    FVector2f Maximum(-FLT_MAX, -FLT_MAX);
    for (const FVertexInstanceID VertexInstanceId :
         Description.VertexInstances().GetElementIDs())
    {
        const FVector2f Uv = Uvs.Get(VertexInstanceId, 0);
        if (!FMath::IsFinite(Uv.X) || !FMath::IsFinite(Uv.Y))
        {
            OutError = TEXT("V5D local-fallback suppression UV0 contains a non-finite value.");
            return false;
        }
        Minimum.X = FMath::Min(Minimum.X, Uv.X);
        Minimum.Y = FMath::Min(Minimum.Y, Uv.Y);
        Maximum.X = FMath::Max(Maximum.X, Uv.X);
        Maximum.Y = FMath::Max(Maximum.Y, Uv.Y);
    }
    if (!FMath::IsNearlyEqual(Minimum.X, -984.383058f, 0.002f) ||
        !FMath::IsNearlyEqual(Minimum.Y, -992.398856f, 0.002f) ||
        !FMath::IsNearlyEqual(Maximum.X, 998.751074f, 0.002f) ||
        !FMath::IsNearlyEqual(Maximum.Y, 999.319761f, 0.002f))
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression source-metre UV0 bounds drifted: min=(%.6f,%.6f) max=(%.6f,%.6f)."),
            Minimum.X,
            Minimum.Y,
            Maximum.X,
            Maximum.Y);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedV2Uv0(
    const FMeshDescription& Description,
    const FStaticMeshLODResources& Lod,
    FString& OutError)
{
    const FStaticMeshConstAttributes Attributes(Description);
    const TVertexInstanceAttributesConstRef<FVector2f> Uvs =
        Attributes.GetVertexInstanceUVs();
    const FStaticMeshVertexBuffer& RenderBuffer =
        Lod.VertexBuffers.StaticMeshVertexBuffer;
    if (Description.VertexInstances().Num() !=
            SuppressedV2ExpectedVertexInstanceCount ||
        Uvs.GetNumElements() != SuppressedV2ExpectedVertexInstanceCount ||
        Uvs.GetNumChannels() != 1 || RenderBuffer.GetNumVertices() == 0 ||
        RenderBuffer.GetNumTexCoords() != 1)
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression V2 UV0 structure changed: vertexInstances=%d uvElements=%d uvChannels=%d renderUvChannels=%d."),
            Description.VertexInstances().Num(),
            Uvs.GetNumElements(),
            Uvs.GetNumChannels(),
            RenderBuffer.GetNumTexCoords());
        return false;
    }
    FVector2f Minimum(FLT_MAX, FLT_MAX);
    FVector2f Maximum(-FLT_MAX, -FLT_MAX);
    for (const FVertexInstanceID VertexInstanceId :
         Description.VertexInstances().GetElementIDs())
    {
        const FVector2f Uv = Uvs.Get(VertexInstanceId, 0);
        if (!FMath::IsFinite(Uv.X) || !FMath::IsFinite(Uv.Y))
        {
            OutError = TEXT("V5D local-fallback suppression V2 UV0 contains a non-finite value.");
            return false;
        }
        Minimum.X = FMath::Min(Minimum.X, Uv.X);
        Minimum.Y = FMath::Min(Minimum.Y, Uv.Y);
        Maximum.X = FMath::Max(Maximum.X, Uv.X);
        Maximum.Y = FMath::Max(Maximum.Y, Uv.Y);
    }
    if (!FMath::IsNearlyEqual(Minimum.X, -984.383058f, 0.002f) ||
        !FMath::IsNearlyEqual(Minimum.Y, -992.398856f, 0.002f) ||
        !FMath::IsNearlyEqual(Maximum.X, 998.751074f, 0.002f) ||
        !FMath::IsNearlyEqual(Maximum.Y, 999.319761f, 0.002f))
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression V2 source-metre UV0 bounds drifted: min=(%.6f,%.6f) max=(%.6f,%.6f)."),
            Minimum.X,
            Minimum.Y,
            Maximum.X,
            Maximum.Y);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedMeshDescription(
    const FMeshDescription& Description,
    FString& OutError)
{
    if (Description.Vertices().Num() !=
            SuppressedExpectedImportedVertexCount ||
        Description.VertexInstances().Num() !=
            SuppressedExpectedVertexInstanceCount ||
        Description.Triangles().Num() != SuppressedExpectedTriangleCount ||
        Description.PolygonGroups().Num() != SuppressedExpectedMaterialCount)
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression mesh-description census changed: vertices=%d vertexInstances=%d triangles=%d polygonGroups=%d."),
            Description.Vertices().Num(),
            Description.VertexInstances().Num(),
            Description.Triangles().Num(),
            Description.PolygonGroups().Num());
        return false;
    }

    const FStaticMeshConstAttributes Attributes(Description);
    const TPolygonGroupAttributesConstRef<FName> PolygonGroupSlots =
        Attributes.GetPolygonGroupMaterialSlotNames();
    TMap<FName, int32> CanonicalIndices;
    for (int32 Index = 0; Index < SuppressedExpectedMaterialCount; ++Index)
    {
        CanonicalIndices.Add(
            FName(SuppressedMaterialSpecs[Index].Name), Index);
    }
    TArray<int32> Counts;
    Counts.Init(0, SuppressedExpectedMaterialCount);
    TSet<FName> SeenGroups;
    for (const FPolygonGroupID GroupId :
         Description.PolygonGroups().GetElementIDs())
    {
        const FName Slot = PolygonGroupSlots[GroupId];
        const int32* Index = CanonicalIndices.Find(Slot);
        if (!Index || SeenGroups.Contains(Slot))
        {
            OutError = TEXT("Local-fallback suppression polygon-group names are incomplete or ambiguous.");
            return false;
        }
        SeenGroups.Add(Slot);
    }
    for (const FTriangleID TriangleId :
         Description.Triangles().GetElementIDs())
    {
        const FPolygonGroupID GroupId =
            Description.GetTrianglePolygonGroup(TriangleId);
        const int32* Index = CanonicalIndices.Find(PolygonGroupSlots[GroupId]);
        if (!Index)
        {
            OutError = TEXT("A local-fallback suppression triangle has no semantic material group.");
            return false;
        }
        ++Counts[*Index];
    }
    for (int32 Index = 0; Index < SuppressedExpectedMaterialCount; ++Index)
    {
        if (Counts[Index] != SuppressedMaterialSpecs[Index].Triangles)
        {
            OutError = FString::Printf(
                TEXT("Local-fallback suppression source triangle census drifted for '%s': expected=%d actual=%d."),
                SuppressedMaterialSpecs[Index].Name,
                SuppressedMaterialSpecs[Index].Triangles,
                Counts[Index]);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedV2MeshDescription(
    const FMeshDescription& Description,
    FString& OutError)
{
    if (Description.Vertices().Num() !=
            SuppressedV2ExpectedImportedVertexCount ||
        Description.VertexInstances().Num() !=
            SuppressedV2ExpectedVertexInstanceCount ||
        Description.Triangles().Num() != SuppressedV2ExpectedTriangleCount ||
        Description.PolygonGroups().Num() != SuppressedExpectedMaterialCount)
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression V2 mesh-description census changed: vertices=%d vertexInstances=%d triangles=%d polygonGroups=%d."),
            Description.Vertices().Num(),
            Description.VertexInstances().Num(),
            Description.Triangles().Num(),
            Description.PolygonGroups().Num());
        return false;
    }

    const FStaticMeshConstAttributes Attributes(Description);
    const TPolygonGroupAttributesConstRef<FName> PolygonGroupSlots =
        Attributes.GetPolygonGroupMaterialSlotNames();
    TMap<FName, int32> CanonicalIndices;
    for (int32 Index = 0; Index < SuppressedExpectedMaterialCount; ++Index)
    {
        CanonicalIndices.Add(
            FName(SuppressedV2MaterialSpecs[Index].Name), Index);
    }
    TArray<int32> Counts;
    Counts.Init(0, SuppressedExpectedMaterialCount);
    TSet<FName> SeenGroups;
    for (const FPolygonGroupID GroupId :
         Description.PolygonGroups().GetElementIDs())
    {
        const FName Slot = PolygonGroupSlots[GroupId];
        const int32* Index = CanonicalIndices.Find(Slot);
        if (!Index || SeenGroups.Contains(Slot))
        {
            OutError = TEXT("Local-fallback suppression V2 polygon-group names are incomplete or ambiguous.");
            return false;
        }
        SeenGroups.Add(Slot);
    }
    for (const FTriangleID TriangleId :
         Description.Triangles().GetElementIDs())
    {
        const FPolygonGroupID GroupId =
            Description.GetTrianglePolygonGroup(TriangleId);
        const int32* Index = CanonicalIndices.Find(PolygonGroupSlots[GroupId]);
        if (!Index)
        {
            OutError = TEXT("A local-fallback suppression V2 triangle has no semantic material group.");
            return false;
        }
        ++Counts[*Index];
    }
    for (int32 Index = 0; Index < SuppressedExpectedMaterialCount; ++Index)
    {
        if (Counts[Index] != SuppressedV2MaterialSpecs[Index].Triangles)
        {
            OutError = FString::Printf(
                TEXT("Local-fallback suppression V2 source triangle census drifted for '%s': expected=%d actual=%d."),
                SuppressedV2MaterialSpecs[Index].Name,
                SuppressedV2MaterialSpecs[Index].Triangles,
                Counts[Index]);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedMesh(
    UStaticMesh* Mesh,
    const TArray<UMaterialInstanceConstant*>& V5CMaterials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const UAssetImportData* ImportData =
        Mesh ? Mesh->GetAssetImportData() : nullptr;
    const TArray<FString> Sources =
        ImportData ? ImportData->ExtractFilenames() : TArray<FString>();
    FString ActualSource = Sources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(Sources[0])
        : FString();
    FString ExpectedSource = SuppressedObjSourcePath();
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(ExpectedSource);
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        Mesh->GetPathName() != SuppressedMeshObjectPath ||
        Sources.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        Mesh->GetNumSourceModels() != 1 || !Description ||
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D !=
            FVector::OneVector ||
        Mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs ||
        Mesh->GetLightMapCoordinateIndex() != 0 ||
        Mesh->GetStaticMaterials().Num() != SuppressedExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() !=
            SuppressedExpectedTriangleCount ||
        RenderData->LODResources[0].Sections.Num() !=
            SuppressedExpectedMaterialCount ||
        !Mesh->NaniteSettings.bEnabled ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) ||
        Mesh->NaniteSettings.FallbackTarget !=
            ENaniteFallbackTarget::PercentTriangles ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError) ||
        !Mesh->HasValidNaniteData())
    {
        OutError = TEXT("V5D local-fallback suppression mesh lost its exact source, identity scale, one-source/one-LOD structure, UV0 policy, 43,492-triangle/17-slot census, or full-fidelity Nanite/raster-fallback state.");
        return false;
    }

    const UBodySetup* Body = Mesh->GetBodySetup();
    if (!Body || Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision())
    {
        OutError = TEXT("V5D local-fallback suppression mesh must have zero collision and no navigation data.");
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(
            FVector(-98438.3058, -99831.9761, 0.0), 0.25) ||
        !BoundsMax.Equals(
            FVector(99875.1074, 99339.8856, 15200.0), 0.25))
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression centimetre bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            BoundsMin.X,
            BoundsMin.Y,
            BoundsMin.Z,
            BoundsMax.X,
            BoundsMax.Y,
            BoundsMax.Z);
        return false;
    }
    if (!ValidateSuppressedMeshDescription(*Description, OutError) ||
        !ValidateSuppressedUv0(
            *Description, RenderData->LODResources[0], OutError) ||
        !ValidateSuppressedProvenance(Mesh, OutError))
    {
        return false;
    }

    const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
    TSet<int32> SeenSectionMaterials;
    for (int32 SectionIndex = 0;
         SectionIndex < RenderData->LODResources[0].Sections.Num();
         ++SectionIndex)
    {
        const FStaticMeshSection& Section =
            RenderData->LODResources[0].Sections[SectionIndex];
        if (Section.MaterialIndex < 0 ||
            Section.MaterialIndex >= SuppressedExpectedMaterialCount ||
            SeenSectionMaterials.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                SuppressedMaterialSpecs[Section.MaterialIndex].Triangles ||
            Mesh->GetSectionInfoMap().Get(0, SectionIndex).MaterialIndex !=
                Section.MaterialIndex ||
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex).MaterialIndex !=
                Section.MaterialIndex)
        {
            OutError = TEXT("V5D local-fallback suppression render section census/mapping drifted.");
            return false;
        }
        SeenSectionMaterials.Add(Section.MaterialIndex);
    }
    if (SeenSectionMaterials.Num() != SuppressedExpectedMaterialCount)
    {
        OutError = TEXT("V5D local-fallback suppression render section roster is incomplete.");
        return false;
    }
    for (int32 Slot = 0; Slot < SuppressedExpectedMaterialCount; ++Slot)
    {
        const FStaticMaterial& StaticMaterial = Materials[Slot];
        const int32 DependencyIndex = static_cast<int32>(
            SuppressedMaterialSpecs[Slot].Role);
        if (!V5CMaterials.IsValidIndex(DependencyIndex) ||
            StaticMaterial.MaterialSlotName !=
                FName(SuppressedMaterialSpecs[Slot].Name) ||
            StaticMaterial.ImportedMaterialSlotName !=
                FName(SuppressedMaterialSpecs[Slot].Name) ||
            StaticMaterial.MaterialInterface != V5CMaterials[DependencyIndex])
        {
            OutError = FString::Printf(
                TEXT("V5D local-fallback suppression material binding drifted at slot %d ('%s')."),
                Slot,
                SuppressedMaterialSpecs[Slot].Name);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedV2Mesh(
    UStaticMesh* Mesh,
    const TArray<UMaterialInstanceConstant*>& V5CMaterials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const UAssetImportData* ImportData =
        Mesh ? Mesh->GetAssetImportData() : nullptr;
    const TArray<FString> Sources =
        ImportData ? ImportData->ExtractFilenames() : TArray<FString>();
    FString ActualSource = Sources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(Sources[0])
        : FString();
    FString ExpectedSource = SuppressedV2ObjSourcePath();
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(ExpectedSource);
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        Mesh->GetPathName() != SuppressedV2MeshObjectPath ||
        Sources.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        Mesh->GetNumSourceModels() != 1 || !Description ||
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D !=
            FVector::OneVector ||
        Mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs ||
        Mesh->GetLightMapCoordinateIndex() != 0 ||
        Mesh->GetStaticMaterials().Num() != SuppressedExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() !=
            SuppressedV2ExpectedTriangleCount ||
        RenderData->LODResources[0].Sections.Num() !=
            SuppressedExpectedMaterialCount ||
        !Mesh->NaniteSettings.bEnabled ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) ||
        Mesh->NaniteSettings.FallbackTarget !=
            ENaniteFallbackTarget::PercentTriangles ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError) ||
        !Mesh->HasValidNaniteData())
    {
        OutError = TEXT("V5D local-fallback suppression V2 mesh lost its exact source, identity scale, one-source/one-LOD structure, UV0 policy, 43,448-triangle/17-slot census, or full-fidelity Nanite/raster-fallback state.");
        return false;
    }

    const UBodySetup* Body = Mesh->GetBodySetup();
    if (!Body || Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision())
    {
        OutError = TEXT("V5D local-fallback suppression V2 mesh must have zero collision and no navigation data.");
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(
            FVector(-98438.3058, -99831.9761, 0.0), 0.25) ||
        !BoundsMax.Equals(
            FVector(99875.1074, 99339.8856, 15200.0), 0.25))
    {
        OutError = FString::Printf(
            TEXT("V5D local-fallback suppression V2 centimetre bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            BoundsMin.X,
            BoundsMin.Y,
            BoundsMin.Z,
            BoundsMax.X,
            BoundsMax.Y,
            BoundsMax.Z);
        return false;
    }
    if (!ValidateSuppressedV2MeshDescription(*Description, OutError) ||
        !ValidateSuppressedV2Uv0(
            *Description, RenderData->LODResources[0], OutError) ||
        !ValidateSuppressedV2Provenance(Mesh, OutError))
    {
        return false;
    }

    const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
    TSet<int32> SeenSectionMaterials;
    for (int32 SectionIndex = 0;
         SectionIndex < RenderData->LODResources[0].Sections.Num();
         ++SectionIndex)
    {
        const FStaticMeshSection& Section =
            RenderData->LODResources[0].Sections[SectionIndex];
        if (Section.MaterialIndex < 0 ||
            Section.MaterialIndex >= SuppressedExpectedMaterialCount ||
            SeenSectionMaterials.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                SuppressedV2MaterialSpecs[Section.MaterialIndex].Triangles ||
            Mesh->GetSectionInfoMap().Get(0, SectionIndex).MaterialIndex !=
                Section.MaterialIndex ||
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex).MaterialIndex !=
                Section.MaterialIndex)
        {
            OutError = TEXT("V5D local-fallback suppression V2 render section census/mapping drifted.");
            return false;
        }
        SeenSectionMaterials.Add(Section.MaterialIndex);
    }
    if (SeenSectionMaterials.Num() != SuppressedExpectedMaterialCount)
    {
        OutError = TEXT("V5D local-fallback suppression V2 render section roster is incomplete.");
        return false;
    }
    for (int32 Slot = 0; Slot < SuppressedExpectedMaterialCount; ++Slot)
    {
        const FStaticMaterial& StaticMaterial = Materials[Slot];
        const int32 DependencyIndex = static_cast<int32>(
            SuppressedV2MaterialSpecs[Slot].Role);
        if (!V5CMaterials.IsValidIndex(DependencyIndex) ||
            StaticMaterial.MaterialSlotName !=
                FName(SuppressedV2MaterialSpecs[Slot].Name) ||
            StaticMaterial.ImportedMaterialSlotName !=
                FName(SuppressedV2MaterialSpecs[Slot].Name) ||
            StaticMaterial.MaterialInterface != V5CMaterials[DependencyIndex])
        {
            OutError = FString::Printf(
                TEXT("V5D local-fallback suppression V2 material binding drifted at slot %d ('%s')."),
                Slot,
                SuppressedV2MaterialSpecs[Slot].Name);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateSuppressedInternal(bool bRequireSaved, FString& OutReport)
{
    FString CanonicalReport;
    TArray<UMaterialInstanceConstant*> V5CMaterials;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::ValidateAsset(
            CanonicalReport))
    {
        OutReport = TEXT("The original 43,544-triangle canonical surroundings asset must remain valid before admitting its local-fallback successor: ") +
            CanonicalReport;
        return false;
    }
    if (!ValidateSuppressedSourceHashes(OutReport) ||
        !ValidateSuppressedJsonContracts(OutReport) ||
        !ValidateSuppressedV5CMaterialDependencies(
            V5CMaterials, OutReport) ||
        !ValidateSuppressedRootRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateSuppressedMesh(
            LoadSuppressedExact<UStaticMesh>(SuppressedMeshObjectPath),
            V5CMaterials,
            OutReport))
    {
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V1_ASSET_VALID assets=%d sourceDate=2026-08-31 canonicalMeshStillValid=true sourceVertexLines=%d sourceTextureCoordinateLines=%d importedVertices=%d importedVertexInstances=%d meshTriangles=%d suppressedTriangles=52 visibleFeatures=1387 visiblePolygonParts=1389 materialSlots=%d wallTriangles=24384 roofTriangles=9662 bottomTriangles=9446 objBytes=%lld objSha256=%s mtlSha256=%s metadataSha256=%s manifestSha256=%s contractSha256=%s outputSetSha256=%s exactSuppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490 canonicalRenderAndRfInputsHashPinnedUnchanged=true canonicalLexicalSlots=true sourceUv0Channels=1 generatedLightmapUv=false fullPrecisionUv=true naniteFullMesh=true rasterFallbackFullMesh=true cookedProvenanceObjects=1 renderOnly=true localFallbackOnly=true providerOverlapResolved=false zeroCollision=true noNavigationData=true noActorOrMapChange=true notSurveyAsBuiltOrHyperreal=true noSensorPropagationRfMaterialOrRfOcclusionAuthority=true."),
        SuppressedExpectedAssetCount,
        SuppressedExpectedSourceVertexLines,
        SuppressedExpectedSourceTextureCoordinateLines,
        SuppressedExpectedImportedVertexCount,
        SuppressedExpectedVertexInstanceCount,
        SuppressedExpectedTriangleCount,
        SuppressedExpectedMaterialCount,
        SuppressedExpectedObjBytes,
        *SuppressedExpectedObjSha256,
        *SuppressedExpectedMtlSha256,
        *SuppressedExpectedMetadataSha256,
        *SuppressedExpectedManifestSha256,
        *SuppressedExpectedContractSha256,
        *SuppressedExpectedOutputSetSha256);
    return true;
}

bool ValidateSuppressedV2Internal(bool bRequireSaved, FString& OutReport)
{
    FString CanonicalReport;
    TArray<UMaterialInstanceConstant*> V5CMaterials;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::ValidateAsset(
            CanonicalReport))
    {
        OutReport = TEXT("The original 43,544-triangle canonical surroundings asset must remain valid before admitting its V2 local-fallback successor: ") +
            CanonicalReport;
        return false;
    }
    if (!ValidateSuppressedV2SourceHashes(OutReport) ||
        !ValidateSuppressedV2JsonContracts(OutReport) ||
        !ValidateSuppressedV5CMaterialDependencies(V5CMaterials, OutReport) ||
        !ValidateSuppressedV2RootRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateSuppressedV2Mesh(
            LoadSuppressedExact<UStaticMesh>(SuppressedV2MeshObjectPath),
            V5CMaterials,
            OutReport))
    {
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_ASSET_VALID assets=%d sourceDate=2026-08-31 canonicalMeshStillValid=true sourceVertexLines=%d sourceTextureCoordinateLines=%d importedVertices=%d importedVertexInstances=%d meshTriangles=%d suppressedTriangles=96 visibleFeatures=1386 visiblePolygonParts=1388 materialSlots=%d wallTriangles=24360 roofTriangles=9652 bottomTriangles=9436 objBytes=%lld objSha256=%s mtlSha256=%s metadataSha256=%s manifestSha256=%s contractSha256=%s outputSetSha256=%s exactSuppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490+OSM:way:429681826 canonicalRenderAndRfInputsHashPinnedUnchanged=true canonicalLexicalSlots=true sourceUv0Channels=1 generatedLightmapUv=false fullPrecisionUv=true naniteFullMesh=true rasterFallbackFullMesh=true cookedV2ProvenanceObjects=1 renderOnly=true localFallbackOnly=true providerOverlapResolved=false zeroCollision=true noNavigationData=true noActorOrMapChange=true notSurveyAsBuiltOrHyperreal=true noSensorPropagationRfMaterialOrRfOcclusionAuthority=true."),
        SuppressedExpectedAssetCount,
        SuppressedExpectedSourceVertexLines,
        SuppressedExpectedSourceTextureCoordinateLines,
        SuppressedV2ExpectedImportedVertexCount,
        SuppressedV2ExpectedVertexInstanceCount,
        SuppressedV2ExpectedTriangleCount,
        SuppressedExpectedMaterialCount,
        SuppressedV2ExpectedObjBytes,
        *SuppressedV2ExpectedObjSha256,
        *SuppressedExpectedMtlSha256,
        *SuppressedV2ExpectedMetadataSha256,
        *SuppressedV2ExpectedManifestSha256,
        *SuppressedV2ExpectedContractSha256,
        *SuppressedV2ExpectedOutputSetSha256);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory
{
const FString& GetLocalFallbackSuppressedAssetRootPath()
{
    return SuppressedAssetRoot;
}

const FString& GetLocalFallbackSuppressedMeshObjectPath()
{
    return SuppressedMeshObjectPath;
}

bool CreateFreshLocalFallbackSuppressedAsset(
    UStaticMesh*& OutAsset,
    FString& OutError)
{
    OutAsset = nullptr;
    FString ExistingReport;
    if (ValidateLocalFallbackSuppressedAsset(ExistingReport))
    {
        OutAsset =
            LoadSuppressedExact<UStaticMesh>(SuppressedMeshObjectPath);
        OutError = ExistingReport;
        return OutAsset != nullptr;
    }

    FString CanonicalReport;
    if (!ValidateAsset(CanonicalReport))
    {
        OutError = TEXT("Local-fallback suppression import requires the unchanged canonical surroundings asset: ") +
            CanonicalReport;
        return false;
    }
    if (!ValidateSuppressedSourceHashes(OutError) ||
        !ValidateSuppressedJsonContracts(OutError))
    {
        return false;
    }
    TArray<UMaterialInstanceConstant*> V5CMaterials;
    if (!ValidateSuppressedV5CMaterialDependencies(
            V5CMaterials, OutError))
    {
        return false;
    }
    TArray<FAssetData> Existing;
    if (!GatherSuppressedRootAssets(Existing, OutError) ||
        !Existing.IsEmpty())
    {
        OutError = TEXT("Local-fallback suppression import refuses an invalid or non-empty additive asset root.");
        return false;
    }

    FScopedSuppressedFreshRollback Rollback(OutError);
    UAssetImportTask* Task = MakeSuppressedImportTask();
    if (!Task || !ValidateSuppressedImportTask(Task, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not allocate the exact local-fallback suppression OBJ import task.");
        }
        return false;
    }
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    AssetTools.ImportAssetTasks({Task});
    for (UObject* Object : Task->GetObjects())
    {
        if (Object && Object->GetPathName() == SuppressedMeshObjectPath)
        {
            OutAsset = Cast<UStaticMesh>(Object);
        }
    }
    if (!OutAsset)
    {
        OutAsset =
            LoadSuppressedExact<UStaticMesh>(SuppressedMeshObjectPath);
    }
    MakeSuppressedRenderOnly(OutAsset);
    if (!NormalizeSuppressedMaterials(
            OutAsset, V5CMaterials, OutError) ||
        !StampSuppressedProvenance(OutAsset, OutError) ||
        !ValidateSuppressedMesh(OutAsset, V5CMaterials, OutError))
    {
        OutAsset = nullptr;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString PreSaveReport;
    if (!ValidateSuppressedInternal(false, PreSaveReport))
    {
        OutError = TEXT("Fresh local-fallback suppression validation failed before save: ") +
            PreSaveReport;
        OutAsset = nullptr;
        return false;
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool ValidateLocalFallbackSuppressedAsset(FString& OutReport)
{
    return ValidateSuppressedInternal(true, OutReport);
}

const FString& GetLocalFallbackSuppressedV2AssetRootPath()
{
    return SuppressedV2AssetRoot;
}

const FString& GetLocalFallbackSuppressedV2MeshObjectPath()
{
    return SuppressedV2MeshObjectPath;
}

bool CreateFreshLocalFallbackSuppressedV2Asset(
    UStaticMesh*& OutAsset,
    FString& OutError)
{
    OutAsset = nullptr;
    FString ExistingReport;
    if (ValidateLocalFallbackSuppressedV2Asset(ExistingReport))
    {
        OutAsset =
            LoadSuppressedExact<UStaticMesh>(SuppressedV2MeshObjectPath);
        OutError = ExistingReport;
        return OutAsset != nullptr;
    }

    FString CanonicalReport;
    if (!ValidateAsset(CanonicalReport))
    {
        OutError = TEXT("Local-fallback suppression V2 import requires the unchanged canonical surroundings asset: ") +
            CanonicalReport;
        return false;
    }
    if (!ValidateSuppressedV2SourceHashes(OutError) ||
        !ValidateSuppressedV2JsonContracts(OutError))
    {
        return false;
    }
    TArray<UMaterialInstanceConstant*> V5CMaterials;
    if (!ValidateSuppressedV5CMaterialDependencies(V5CMaterials, OutError))
    {
        return false;
    }
    TArray<FAssetData> Existing;
    if (!GatherSuppressedV2RootAssets(Existing, OutError) ||
        !Existing.IsEmpty())
    {
        OutError = TEXT("Local-fallback suppression V2 import refuses an invalid or non-empty additive asset root.");
        return false;
    }

    FScopedSuppressedV2FreshRollback Rollback(OutError);
    UAssetImportTask* Task = MakeSuppressedV2ImportTask();
    if (!Task || !ValidateSuppressedV2ImportTask(Task, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not allocate the exact local-fallback suppression V2 OBJ import task.");
        }
        return false;
    }
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    AssetTools.ImportAssetTasks({Task});
    for (UObject* Object : Task->GetObjects())
    {
        if (Object && Object->GetPathName() == SuppressedV2MeshObjectPath)
        {
            OutAsset = Cast<UStaticMesh>(Object);
        }
    }
    if (!OutAsset)
    {
        OutAsset =
            LoadSuppressedExact<UStaticMesh>(SuppressedV2MeshObjectPath);
    }
    MakeSuppressedRenderOnly(OutAsset);
    if (!NormalizeSuppressedV2Materials(
            OutAsset, V5CMaterials, OutError) ||
        !StampSuppressedV2Provenance(OutAsset, OutError) ||
        !ValidateSuppressedV2Mesh(OutAsset, V5CMaterials, OutError))
    {
        OutAsset = nullptr;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString PreSaveReport;
    if (!ValidateSuppressedV2Internal(false, PreSaveReport))
    {
        OutError = TEXT("Fresh local-fallback suppression V2 validation failed before save: ") +
            PreSaveReport;
        OutAsset = nullptr;
        return false;
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool ValidateLocalFallbackSuppressedV2Asset(FString& OutReport)
{
    return ValidateSuppressedV2Internal(true, OutReport);
}
} // namespace TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory
