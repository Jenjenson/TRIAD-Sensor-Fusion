#include "TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary.h"

#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
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
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

#include <openssl/sha.h>

namespace
{
constexpr int32 VariantCount = 3;
constexpr int32 LodCount = 3;
constexpr int32 MaterialCount = 3;
constexpr int32 OutputAssetCount = 6;
constexpr int32 StagingAssetCount = VariantCount * LodCount;
constexpr int64 MaximumPinnedInputBytes = 8 * 1024 * 1024;
constexpr int64 CandidateContractBytes = 31330;
constexpr int64 TreeRealismContractBytes = 3842;
constexpr int64 GeometrySelectorBytes = 227267;
constexpr int64 SourceIdentityManifestBytes = 3193;

// A valid caller hash cannot cross this independent source gate. The two
// compiled receipt sentinels are also deliberately invalid and distinct.
constexpr bool bMaterializationAndSwapCompiled = false;
static_assert(!bMaterializationAndSwapCompiled);
const FString TrustedAcceptedCurrentReceiptSha256(
    TEXT("UNSET_EDITOR_CURRENT_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));
const FString TrustedFutureAuthorizationReceiptSha256(
    TEXT("UNSET_EDITOR_FUTURE_TRUST_ANCHOR_REQUIRES_SEPARATE_REVIEWED_SOURCE_CHANGE"));

const FString CandidateContractSha256(
    TEXT("504E94CCC21E5C671E290181A14DDA6339A0D7FED365EE822ABD54E90CC63130"));
const FString TreeRealismContractSha256(
    TEXT("D86971F2EE859A911C1891C314729B2F871A79B6956BD40D81256D476BDDCC05"));
const FString GeometrySelectorSha256(
    TEXT("59BA4B21B81A3546CBE76E1108D6EB39C2DEA1DD0CBB309791515F9AC5B59E51"));
const FString SourceIdentityManifestSha256(
    TEXT("8E522EA9B237480E12CF9FDF6AA6EB24738768F48B9E7BF04033A45A826D3B7F"));
const FString MaterialLibrarySha256(
    TEXT("FC57508A2D2B3A4C9EEE2521201884E3BEF3E485933869384B9F86F043433810"));
constexpr int64 MaterialLibraryBytes = 477;

const FString AcceptedCurrentSchema(
    TEXT("triad.istana_explore_v5d.r33_player0_capture.v1"));
const FString AcceptedCurrentTransactionSchema(
    TEXT("triad.istana_explore_v5d.r33_cesium_world_terrain_reference.native_transaction.v1"));
const FString FutureAuthorizationSchema(
    TEXT("triad.istana_public_view_explore_v5d.tree_realism.tropical_umbrella_hero.future_transaction_authority.v1"));
const FString FutureOperation(
    TEXT("MATERIALIZE_THREE_TRUE_LOD_VARIANTS_AND_ATOMICALLY_SWAP_EXACT_SIX_UMBRELLA_VISUALS"));
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration"));
const FString StagingRoot(
    TEXT("/Temp/TRIAD/TropicalUmbrellaHeroIntegrationSourceStaging"));

const TCHAR* const VariantNames[VariantCount] = {
    TEXT("A"), TEXT("B"), TEXT("C")};
const TCHAR* const MaterialNames[MaterialCount] = {
    TEXT("Bark"), TEXT("LeafLive"), TEXT("LeafDry")};
const TCHAR* const MaterialAssetNames[MaterialCount] = {
    TEXT("M_IPV5D_TropicalUmbrella_Bark"),
    TEXT("M_IPV5D_TropicalUmbrella_LeafLive"),
    TEXT("M_IPV5D_TropicalUmbrella_LeafDry")};
const TCHAR* const MaterialParentObjectPaths[MaterialCount] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Trunk_Response.M_IPV5D_Tree_Umbrella_Trunk_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Leaves_Response.M_IPV5D_Tree_Umbrella_Leaves_Response"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Materials/M_IPV5D_Tree_Umbrella_Leaves_Response.M_IPV5D_Tree_Umbrella_Leaves_Response")};

// LeafLive deliberately inherits the admitted TreeRealism response unchanged.
// LeafDry retains that same masked/two-sided foliage, opacity and WPO graph but
// has this exact, bounded response-only override set. These values make the
// restrained dry-leaf section visibly warm/low-chroma without introducing a
// new texture, opacity, wind, species, health or current-season claim.
struct FScalarOverride
{
    const TCHAR* Name;
    float Value;
};

struct FVectorOverride
{
    const TCHAR* Name;
    FLinearColor Value;
};

const FScalarOverride LeafDryScalarOverrides[] = {
    {TEXT("TRIAD_TreeResponseLumaLow"), 0.82f},
    {TEXT("TRIAD_TreeResponseLumaHigh"), 0.96f},
    {TEXT("TRIAD_TreeResponseDesaturationFraction"), 0.28f},
    {TEXT("TRIAD_TreeResponseRoughnessMin"), 0.72f},
    {TEXT("TRIAD_TreeResponseRoughnessMax"), 0.90f},
    {TEXT("TRIAD_TreeResponseBaseColorMax"), 0.72f}};
const FVectorOverride LeafDryVectorOverrides[] = {
    {TEXT("TRIAD_TreeResponseTintLow"),
     FLinearColor(1.30f, 0.79f, 0.38f, 1.0f)},
    {TEXT("TRIAD_TreeResponseTintHigh"),
     FLinearColor(1.12f, 0.70f, 0.34f, 1.0f)},
    {TEXT("TRIAD_TreeResponseSubsurfaceTint"),
     FLinearColor(0.84f, 0.58f, 0.26f, 1.0f)}};
const TCHAR* const InheritedWindScalarNames[] = {
    TEXT("TRIAD_WindStrengthCm"),
    TEXT("TRIAD_WindSpeed"),
    TEXT("TRIAD_WindHeightCm"),
    TEXT("TRIAD_WindResponseScale"),
    TEXT("TRIAD_MaxWpoCm")};
const TCHAR* const InheritedWindVectorName = TEXT("TRIAD_WindDirection");
constexpr int32 LeafLiveMaterialSlot = 1;
constexpr int32 LeafDryMaterialSlot = 2;
constexpr float InheritedLeafOpacityMaskClip = 0.333f;
static_assert(UE_ARRAY_COUNT(LeafDryScalarOverrides) == 6);
static_assert(UE_ARRAY_COUNT(LeafDryVectorOverrides) == 3);

struct FSourceFilePin
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourceFilePin ObjPins[VariantCount][LodCount] = {
    {
        {TEXT("Generated/SM_IPV5D_TropicalUmbrellaHero_A_LOD0.obj"), 6473399, TEXT("2687325EFBE2480F73BF6915C5FFF19F9BDB6D9CEC06CADBB1D66D9F0BEE7B95")},
        {TEXT("Generated/SM_IPV5D_TropicalUmbrellaHero_A_LOD1.obj"), 2423157, TEXT("BDBCBEE321D0BDC1EA0FBCD15583C192843D397DD6568A8F918107A7F7D76B89")},
        {TEXT("Generated/SM_IPV5D_TropicalUmbrellaHero_A_LOD2.obj"), 509062, TEXT("362E7253A1245B656D12E7B880BCA4EF39B54DCC858B7A38ABF542B82054236A")},
    },
    {
        {TEXT("Generated/SM_IPV5D_TropicalUmbrellaHero_B_LOD0.obj"), 6481507, TEXT("61D3C809572BC047244F110C7C66043A447456FF24DF13E60BA54929BAA58D07")},
        {TEXT("Generated/SM_IPV5D_TropicalUmbrellaHero_B_LOD1.obj"), 2424750, TEXT("AB235CC3F098AA9927A86B56B677FAC00328AADA2F6DE55D2C121199B757F3B3")},
        {TEXT("Generated/SM_IPV5D_TropicalUmbrellaHero_B_LOD2.obj"), 510164, TEXT("586BF47C69DCBBC661F177E51FBF592FC00290A4EA3B729A1D1C909572EAB077")},
    },
    {
        {TEXT("Generated/SM_IPV5D_TropicalUmbrellaHero_C_LOD0.obj"), 6457747, TEXT("446963351C41765322CE0A6D7D0D3EAA1CFC0EA8782C6D508B2C65DCEC242549")},
        {TEXT("Generated/SM_IPV5D_TropicalUmbrellaHero_C_LOD1.obj"), 2414573, TEXT("4F0C13E92DBA1FE0B58D236BD64827DDDE6D2F2DFBA2DBB1BA6FE86759631E6A")},
        {TEXT("Generated/SM_IPV5D_TropicalUmbrellaHero_C_LOD2.obj"), 507824, TEXT("ACFCFD8DB0ABE4ADB4521477C7444D2DCE311710E75FAEB93082F255202833D4")},
    }};

const int32 LodVertices[VariantCount][LodCount] = {
    {37136, 14372, 3156},
    {37136, 14372, 3156},
    {37136, 14372, 3156}};
const int32 LodTriangles[VariantCount][LodCount] = {
    {54828, 21352, 4776},
    {54828, 21352, 4776},
    {54828, 21352, 4776}};
const int32 LodRootVertices[VariantCount][LodCount] = {
    {61, 47, 33}, {61, 47, 33}, {61, 47, 33}};
const int32 LodTrianglesByMaterial[VariantCount][LodCount][MaterialCount] = {
    {{18540, 35544, 744}, {7912, 13104, 336}, {2088, 2648, 40}},
    {{18540, 35584, 704}, {7912, 13184, 256}, {2088, 2640, 48}},
    {{18540, 35392, 896}, {7912, 13144, 296}, {2088, 2632, 56}}};
const FVector3f ExpectedUnrealBoundsMinCm[VariantCount][LodCount] = {
    {FVector3f(-1153.7861f, -1028.6197f, 0.0f), FVector3f(-1173.6045f, -994.5727f, 0.0f), FVector3f(-979.6528f, -959.4625f, 0.0f)},
    {FVector3f(-1172.4090f, -1280.5380f, 0.0f), FVector3f(-1116.5429f, -1227.4144f, 0.0f), FVector3f(-1117.9435f, -1140.6434f, 0.0f)},
    {FVector3f(-1294.6117f, -1028.4023f, 0.0f), FVector3f(-1310.5309f, -1084.1193f, 0.0f), FVector3f(-1223.2171f, -991.3024f, 0.0f)}};
const FVector3f ExpectedUnrealBoundsMaxCm[VariantCount][LodCount] = {
    {FVector3f(1360.6162f, 1024.7927f, 1208.6435f), FVector3f(1347.8084f, 987.0468f, 1225.2170f), FVector3f(1357.0960f, 826.2744f, 1184.0472f)},
    {FVector3f(1074.8558f, 1200.1115f, 1374.9297f), FVector3f(1038.6270f, 1159.2829f, 1336.1616f), FVector3f(817.4451f, 926.1799f, 1283.4635f)},
    {FVector3f(1260.0605f, 999.4995f, 1139.1916f), FVector3f(1170.2789f, 1060.1997f, 1148.6171f), FVector3f(1145.5205f, 772.1522f, 1090.9971f)}};

static_assert(UE_ARRAY_COUNT(ObjPins) == VariantCount);
static_assert(UE_ARRAY_COUNT(MaterialNames) == MaterialCount);
static_assert(StagingAssetCount == 9);

struct FAdmission
{
    FString CandidateRoot;
    FString AcceptedCurrentSha256;
    FString FutureAuthorizationSha256;
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

FString AbsoluteNormalized(const FString& Path)
{
    FString Result = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Result);
    return Result;
}

FString CandidateSourcePath(
    const FString& CandidateRoot,
    const TCHAR* RelativePath)
{
    return AbsoluteNormalized(FPaths::Combine(CandidateRoot, RelativePath));
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
        !IsSha256(ExpectedSha256) || ExpectedBytes < 2 ||
        ExpectedBytes > MaximumPinnedInputBytes ||
        IFileManager::Get().FileSize(*AbsolutePath) != ExpectedBytes ||
        !FFileHelper::LoadFileToArray(OutBytes, *AbsolutePath) ||
        OutBytes.Num() != ExpectedBytes)
    {
        OutError = TEXT("Tropical umbrella input is absent or lost its exact absolute-path/byte-count contract.");
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
        OutError = TEXT("Tropical umbrella input failed its immutable SHA-256 pin.");
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
    if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
    {
        OutError = TEXT("Tropical umbrella pinned JSON is not a root object.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ExactString(
    const TSharedPtr<FJsonObject>& Root,
    const TCHAR* Field,
    const FString& Expected)
{
    FString Actual;
    return Root.IsValid() && Root->TryGetStringField(Field, Actual) &&
        Actual == Expected;
}

bool ExactBool(
    const TSharedPtr<FJsonObject>& Root,
    const TCHAR* Field,
    bool Expected)
{
    bool Actual = !Expected;
    return Root.IsValid() && Root->TryGetBoolField(Field, Actual) &&
        Actual == Expected;
}

bool ExactInteger(
    const TSharedPtr<FJsonObject>& Root,
    const TCHAR* Field,
    int32 Expected)
{
    double Actual = 0.0;
    return Root.IsValid() && Root->TryGetNumberField(Field, Actual) &&
        Actual == static_cast<double>(Expected);
}

bool ValidateCandidateContract(
    const FString& CandidateRoot,
    FString& OutError)
{
    TSharedPtr<FJsonObject> Root;
    if (!ParsePinnedJson(
            CandidateSourcePath(
                CandidateRoot,
                TEXT("tropical_umbrella_hero_candidate.contract.json")),
            CandidateContractBytes,
            CandidateContractSha256,
            Root,
            OutError) ||
        !ExactString(
            Root,
            TEXT("status"),
            TEXT("UNADMITTED_SOURCE_ONLY_VISUAL_CANDIDATE")) ||
        !ExactString(Root, TEXT("schemaVersion"), TEXT("1.1.0")))
    {
        OutError = TEXT("Tropical umbrella candidate contract hash or source-only status drifted.");
        return false;
    }
    const TSharedPtr<FJsonObject>* Geometry = nullptr;
    const TSharedPtr<FJsonObject>* Gates = nullptr;
    if (!Root->TryGetObjectField(TEXT("geometry"), Geometry) ||
        !Root->TryGetObjectField(TEXT("futureIntegrationBoundary"), Gates) ||
        !Geometry || !Gates ||
        !ExactInteger(*Geometry, TEXT("candidateObjCount"), 9) ||
        !ExactInteger(*Geometry, TEXT("lodsPerVariant"), 3) ||
        !ExactBool(
            *Gates,
            TEXT("candidateMayOnlyReplaceNativeClassifiedUmbrellaFormVisuals"),
            true) ||
        !ExactBool(*Gates, TEXT("compiledTrustAnchorsPopulated"), false) ||
        !ExactBool(*Gates, TEXT("materializationReachable"), false) ||
        !ExactBool(*Gates, TEXT("runtimeSelectionCompiledFalse"), true) ||
        !ExactBool(*Gates, TEXT("runtimeActivationCompiledFalse"), true))
    {
        OutError = TEXT("Tropical umbrella candidate semantic admission gates drifted.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidatePinnedSourceClosure(
    const FString& CandidateRoot,
    FString& OutError)
{
    if (!ValidateCandidateContract(CandidateRoot, OutError))
    {
        return false;
    }
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        for (int32 Lod = 0; Lod < LodCount; ++Lod)
        {
            const FSourceFilePin& Pin = ObjPins[Variant][Lod];
            TArray<uint8> Bytes;
            if (!LoadPinnedBytes(
                    CandidateSourcePath(CandidateRoot, Pin.RelativePath),
                    Pin.Bytes,
                    Pin.Sha256,
                    Bytes,
                    OutError))
            {
                return false;
            }
        }
    }
    TArray<uint8> MtlBytes;
    if (!LoadPinnedBytes(
            CandidateSourcePath(
                CandidateRoot,
                TEXT("Generated/M_IPV5D_TropicalUmbrellaHeroCandidate.mtl")),
            MaterialLibraryBytes,
            MaterialLibrarySha256,
            MtlBytes,
            OutError))
    {
        return false;
    }
    const FString TreeRoot = AbsoluteNormalized(
        FPaths::GetPath(CandidateRoot));
    TSharedPtr<FJsonObject> TreeContract;
    TSharedPtr<FJsonObject> Selector;
    TSharedPtr<FJsonObject> Identities;
    if (!ParsePinnedJson(
            FPaths::Combine(
                TreeRoot,
                TEXT("istana_public_view_v5d_tree_realism.contract.json")),
            TreeRealismContractBytes,
            TreeRealismContractSha256,
            TreeContract,
            OutError) ||
        !ParsePinnedJson(
            FPaths::Combine(
                TreeRoot,
                TEXT("GeometryVariationCandidateV4/tree_geometry_instance_selector.v4.json")),
            GeometrySelectorBytes,
            GeometrySelectorSha256,
            Selector,
            OutError) ||
        !ParsePinnedJson(
            FPaths::Combine(
                TreeRoot,
                TEXT("TropicalUmbrellaHeroIntegration/tropical_umbrella_hero_source_instances.v1.json")),
            SourceIdentityManifestBytes,
            SourceIdentityManifestSha256,
            Identities,
            OutError) ||
        !ExactString(
            Identities,
            TEXT("status"),
            TEXT("SOURCE_IDENTITIES_ONLY_NATIVE_WORLD_TRANSFORMS_NOT_CAPTURED")))
    {
        OutError = TEXT("TreeRealism, GV4 selector, or six-identity source pin drifted.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAcceptedCurrentReceipt(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    if (!ExactString(Root, TEXT("Schema"), AcceptedCurrentSchema) ||
        !ExactString(Root, TEXT("Status"), TEXT("COMMITTED")) ||
        !ExactString(
            Root,
            TEXT("NativeTransactionSchema"),
            AcceptedCurrentTransactionSchema) ||
        !ExactBool(Root, TEXT("HumanVisualReviewAccepted"), true) ||
        !ExactBool(Root, TEXT("ExplicitHumanReviewAcceptance"), true) ||
        !ExactBool(Root, TEXT("ConfirmedEightImagesReviewed"), true) ||
        !ExactBool(Root, TEXT("MapModifiedByCapture"), false) ||
        !ExactBool(
            Root,
            TEXT("SimulationCollisionNavigationSensorRfModified"),
            false))
    {
        OutError = TEXT("Accepted-current receipt is not the exact human-accepted R33 state.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateFutureAuthorization(
    const TSharedPtr<FJsonObject>& Root,
    const FString& AcceptedCurrentSha256,
    FString& OutError)
{
    if (!ExactString(Root, TEXT("Schema"), FutureAuthorizationSchema) ||
        !ExactString(Root, TEXT("Status"), TEXT("AUTHORIZED_NOT_EXECUTED")) ||
        !ExactString(Root, TEXT("Operation"), FutureOperation) ||
        !ExactString(Root, TEXT("TargetMapPackage"), TargetMapPackage) ||
        !ExactString(
            Root,
            TEXT("AcceptedCurrentReceiptSha256"),
            AcceptedCurrentSha256.ToUpper()) ||
        !ExactString(
            Root,
            TEXT("CandidateContractSha256"),
            CandidateContractSha256) ||
        !ExactString(
            Root,
            TEXT("TreeRealismContractSha256"),
            TreeRealismContractSha256) ||
        !ExactString(
            Root,
            TEXT("GeometrySelectorSha256"),
            GeometrySelectorSha256) ||
        !ExactString(
            Root,
            TEXT("SourceIdentityManifestSha256"),
            SourceIdentityManifestSha256) ||
        !ExactInteger(Root, TEXT("ExactSourceInstanceCount"), 6) ||
        !ExactInteger(Root, TEXT("ExactVariantCount"), 3) ||
        !ExactInteger(Root, TEXT("ExactLodsPerVariant"), 3) ||
        !ExactInteger(Root, TEXT("ExactOutputAssetCount"), OutputAssetCount) ||
        !ExactBool(Root, TEXT("ExplicitExecutionAuthorized"), true) ||
        !ExactBool(Root, TEXT("ExactSixUmbrellaVisualSwapOnly"), true) ||
        !ExactBool(Root, TEXT("ExistingFallbackPreserved"), true) ||
        !ExactBool(Root, TEXT("NewOrDeletedPlacementsAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceTransformMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("MapSaveAuthorized"), false) ||
        !ExactBool(
            Root,
            TEXT("CollisionNavigationLosRfSensorTerrainMutationAuthorized"),
            false) ||
        !ExactBool(Root, TEXT("NativeWriteOrUnrealLaunchPerformed"), false))
    {
        OutError = TEXT("Future tropical-umbrella authorization is absent, unpinned, or broader than the exact six-source appearance swap.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool BuildAdmission(
    const FString& CandidateRoot,
    const FString& AcceptedCurrentReceiptPath,
    const FString& ExpectedAcceptedCurrentReceiptSha256,
    const FString& FutureAuthorizationReceiptPath,
    const FString& ExpectedFutureAuthorizationReceiptSha256,
    bool bRequireCompiledExecution,
    FAdmission& OutAdmission,
    FString& OutError)
{
    OutAdmission = FAdmission{};
    if (CandidateRoot.IsEmpty() || FPaths::IsRelative(CandidateRoot) ||
        AcceptedCurrentReceiptPath.IsEmpty() ||
        FPaths::IsRelative(AcceptedCurrentReceiptPath) ||
        FutureAuthorizationReceiptPath.IsEmpty() ||
        FPaths::IsRelative(FutureAuthorizationReceiptPath) ||
        !IsSha256(ExpectedAcceptedCurrentReceiptSha256) ||
        !IsSha256(ExpectedFutureAuthorizationReceiptSha256))
    {
        OutError = TEXT("Inspection requires one absolute candidate root, two absolute distinct receipt paths, and two SHA-256 caller pins.");
        return false;
    }
    const FString FullRoot = AbsoluteNormalized(CandidateRoot);
    const FString FullCurrent = AbsoluteNormalized(AcceptedCurrentReceiptPath);
    const FString FullFuture = AbsoluteNormalized(FutureAuthorizationReceiptPath);
    if (FPaths::IsSamePath(FullCurrent, FullFuture) ||
        ExpectedAcceptedCurrentReceiptSha256.Equals(
            ExpectedFutureAuthorizationReceiptSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Accepted-current and future-authorization receipts must be different files with different hashes.");
        return false;
    }
    if (bRequireCompiledExecution &&
        (!bMaterializationAndSwapCompiled ||
         !IsSha256(TrustedAcceptedCurrentReceiptSha256) ||
         !IsSha256(TrustedFutureAuthorizationReceiptSha256) ||
         TrustedAcceptedCurrentReceiptSha256.Equals(
             TrustedFutureAuthorizationReceiptSha256,
             ESearchCase::IgnoreCase) ||
         !ExpectedAcceptedCurrentReceiptSha256.Equals(
             TrustedAcceptedCurrentReceiptSha256,
             ESearchCase::IgnoreCase) ||
         !ExpectedFutureAuthorizationReceiptSha256.Equals(
             TrustedFutureAuthorizationReceiptSha256,
             ESearchCase::IgnoreCase)))
    {
        OutError = TEXT("Materialization and six-source swap are intentionally unreachable: their compile gate is false and distinct compiled trust anchors are unset.");
        return false;
    }
    TSharedPtr<FJsonObject> Current;
    TSharedPtr<FJsonObject> Future;
    const int64 CurrentBytes = IFileManager::Get().FileSize(*FullCurrent);
    const int64 FutureBytes = IFileManager::Get().FileSize(*FullFuture);
    if (!ValidatePinnedSourceClosure(FullRoot, OutError) ||
        CurrentBytes < 2 || CurrentBytes > MaximumPinnedInputBytes ||
        FutureBytes < 2 || FutureBytes > MaximumPinnedInputBytes ||
        !ParsePinnedJson(
            FullCurrent,
            CurrentBytes,
            ExpectedAcceptedCurrentReceiptSha256,
            Current,
            OutError) ||
        !ValidateAcceptedCurrentReceipt(Current, OutError) ||
        !ParsePinnedJson(
            FullFuture,
            FutureBytes,
            ExpectedFutureAuthorizationReceiptSha256,
            Future,
            OutError) ||
        !ValidateFutureAuthorization(
            Future,
            ExpectedAcceptedCurrentReceiptSha256,
            OutError))
    {
        return false;
    }
    OutAdmission.CandidateRoot = FullRoot;
    OutAdmission.AcceptedCurrentSha256 =
        ExpectedAcceptedCurrentReceiptSha256.ToUpper();
    OutAdmission.FutureAuthorizationSha256 =
        ExpectedFutureAuthorizationReceiptSha256.ToUpper();
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
        ? FPaths::Combine(
              OutputRoot,
              TEXT("Materials"),
              MaterialAssetNames[Slot])
        : FString();
}

FString MaterialObjectPath(int32 Slot)
{
    return Slot >= 0 && Slot < MaterialCount
        ? MaterialPackagePath(Slot) + TEXT(".") + MaterialAssetNames[Slot]
        : FString();
}

FString FinalMeshAssetName(int32 Variant)
{
    return Variant >= 0 && Variant < VariantCount
        ? FString::Printf(
              TEXT("SM_IPV5D_TropicalUmbrellaHero_%s"),
              VariantNames[Variant])
        : FString();
}

FString FinalMeshPackagePath(int32 Variant)
{
    return FPaths::Combine(
        OutputRoot,
        TEXT("Meshes"),
        FinalMeshAssetName(Variant));
}

FString FinalMeshObjectPath(int32 Variant)
{
    return FinalMeshPackagePath(Variant) + TEXT(".") +
        FinalMeshAssetName(Variant);
}

FString StagingAssetName(int32 Variant, int32 Lod)
{
    return FString::Printf(
        TEXT("SM_IPV5D_TropicalUmbrellaHeroSource_%s_LOD%d"),
        VariantNames[Variant],
        Lod);
}

FString StagingPackagePath(int32 Variant, int32 Lod)
{
    return FPaths::Combine(
        StagingRoot,
        StagingAssetName(Variant, Lod));
}

FString StagingObjectPath(int32 Variant, int32 Lod)
{
    return StagingPackagePath(Variant, Lod) + TEXT(".") +
        StagingAssetName(Variant, Lod);
}

void GetExpectedOutputObjectPaths(TArray<FString>& OutPaths)
{
    OutPaths.Reset();
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        OutPaths.Add(FinalMeshObjectPath(Variant));
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        OutPaths.Add(MaterialObjectPath(Slot));
    }
}

void GetExpectedStagingObjectPaths(TArray<FString>& OutPaths)
{
    OutPaths.Reset();
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        for (int32 Lod = 0; Lod < LodCount; ++Lod)
        {
            OutPaths.Add(StagingObjectPath(Variant, Lod));
        }
    }
}

bool IsPackageWithinNamespace(
    const FString& PackageName,
    const FString& NamespaceRoot)
{
    return PackageName == NamespaceRoot ||
        PackageName.StartsWith(NamespaceRoot + TEXT("/"));
}

struct FNamespaceState
{
    TSet<FString> ObjectPaths;
    TSet<FString> PackagePaths;
    TSet<FString> PhysicalFiles;
    TSet<FString> PhysicalDirectories;
    FString PhysicalRoot;
    bool bPhysicalRootResolved = false;
};

void CollectNamespaceState(
    const FString& NamespaceRoot,
    FNamespaceState& OutState)
{
    OutState = FNamespaceState{};
    TArray<FAssetData> RegistryAssets;
    FAssetRegistryModule::GetRegistry().GetAssetsByPath(
        FName(NamespaceRoot), RegistryAssets, true, false);
    for (const FAssetData& Asset : RegistryAssets)
    {
        const FString PackageName = Asset.PackageName.ToString();
        if (IsPackageWithinNamespace(PackageName, NamespaceRoot))
        {
            OutState.ObjectPaths.Add(Asset.GetObjectPathString());
            OutState.PackagePaths.Add(PackageName);
        }
    }
    for (TObjectIterator<UObject> It; It; ++It)
    {
        UObject* Object = *It;
        UPackage* Package = Object ? Object->GetOutermost() : nullptr;
        if (Object && IsValid(Object) && Object->IsAsset() && Package &&
            IsPackageWithinNamespace(Package->GetName(), NamespaceRoot))
        {
            OutState.ObjectPaths.Add(Object->GetPathName());
            OutState.PackagePaths.Add(Package->GetName());
        }
    }
    for (TObjectIterator<UPackage> It; It; ++It)
    {
        UPackage* Package = *It;
        if (Package && IsValid(Package) &&
            IsPackageWithinNamespace(Package->GetName(), NamespaceRoot))
        {
            OutState.PackagePaths.Add(Package->GetName());
        }
    }
    if (FPackageName::TryConvertLongPackageNameToFilename(
            NamespaceRoot, OutState.PhysicalRoot, FString()))
    {
        OutState.bPhysicalRootResolved = true;
        FPaths::NormalizeDirectoryName(OutState.PhysicalRoot);
        if (IFileManager::Get().DirectoryExists(*OutState.PhysicalRoot))
        {
            OutState.PhysicalDirectories.Add(OutState.PhysicalRoot);
        }
        TArray<FString> Files;
        TArray<FString> Directories;
        IFileManager::Get().FindFilesRecursive(
            Files,
            *OutState.PhysicalRoot,
            TEXT("*"),
            true,
            false,
            true);
        IFileManager::Get().FindFilesRecursive(
            Directories,
            *OutState.PhysicalRoot,
            TEXT("*"),
            false,
            true,
            true);
        for (FString Filename : Files)
        {
            FPaths::NormalizeFilename(Filename);
            OutState.PhysicalFiles.Add(Filename);
            FString PackageName;
            const FString Extension = FPaths::GetExtension(Filename, true);
            if ((Extension == FPackageName::GetAssetPackageExtension() ||
                 Extension == FPackageName::GetMapPackageExtension()) &&
                FPackageName::TryConvertFilenameToLongPackageName(
                    Filename, PackageName) &&
                IsPackageWithinNamespace(PackageName, NamespaceRoot))
            {
                OutState.PackagePaths.Add(PackageName);
            }
        }
        for (FString Directory : Directories)
        {
            FPaths::NormalizeDirectoryName(Directory);
            OutState.PhysicalDirectories.Add(Directory);
        }
    }
}

bool BuildExpectedNamespaceState(
    const FString& NamespaceRoot,
    const TArray<FString>& ExpectedObjectPaths,
    TSet<FString>& OutObjects,
    TSet<FString>& OutPackages,
    TSet<FString>& OutPhysicalFiles,
    TSet<FString>& OutPhysicalDirectories,
    FString& OutPhysicalRoot,
    FString& OutError)
{
    OutObjects.Reset();
    OutPackages.Reset();
    OutPhysicalFiles.Reset();
    OutPhysicalDirectories.Reset();
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            NamespaceRoot, OutPhysicalRoot, FString()))
    {
        OutError = TEXT("Tropical umbrella namespace has no resolvable package-root filesystem path.");
        return false;
    }
    FPaths::NormalizeDirectoryName(OutPhysicalRoot);
    OutPhysicalDirectories.Add(OutPhysicalRoot);
    for (const FString& ObjectPath : ExpectedObjectPaths)
    {
        if (ObjectPath.IsEmpty() || OutObjects.Contains(ObjectPath))
        {
            OutError = TEXT("Tropical umbrella expected object roster is empty or duplicated.");
            return false;
        }
        OutObjects.Add(ObjectPath);
        const FString PackagePath =
            FPackageName::ObjectPathToPackageName(ObjectPath);
        if (!IsPackageWithinNamespace(PackagePath, NamespaceRoot) ||
            OutPackages.Contains(PackagePath))
        {
            OutError = TEXT("Tropical umbrella expected package escaped the isolated namespace or duplicated a package.");
            return false;
        }
        OutPackages.Add(PackagePath);
        FString PhysicalFile;
        if (!FPackageName::TryConvertLongPackageNameToFilename(
                PackagePath,
                PhysicalFile,
                FPackageName::GetAssetPackageExtension()))
        {
            OutError = TEXT("A tropical umbrella output package has no exact .uasset filename.");
            return false;
        }
        FPaths::NormalizeFilename(PhysicalFile);
        OutPhysicalFiles.Add(PhysicalFile);
        FString Directory = FPaths::GetPath(PhysicalFile);
        FPaths::NormalizeDirectoryName(Directory);
        while (!Directory.IsEmpty() &&
               (FPaths::IsSamePath(Directory, OutPhysicalRoot) ||
                Directory.StartsWith(OutPhysicalRoot + TEXT("/"))))
        {
            OutPhysicalDirectories.Add(Directory);
            if (FPaths::IsSamePath(Directory, OutPhysicalRoot))
            {
                break;
            }
            Directory = FPaths::GetPath(Directory);
            FPaths::NormalizeDirectoryName(Directory);
        }
    }
    OutError.Reset();
    return true;
}

bool NamespaceMatchesExactAssets(
    const FString& NamespaceRoot,
    const TArray<FString>& ExpectedObjectPaths,
    bool bRequireExactRoster,
    bool bRequirePhysicalAssetFiles,
    FString& OutError)
{
    TSet<FString> ExpectedObjects;
    TSet<FString> ExpectedPackages;
    TSet<FString> ExpectedPhysicalFiles;
    TSet<FString> ExpectedPhysicalDirectories;
    FString ExpectedPhysicalRoot;
    if (!BuildExpectedNamespaceState(
            NamespaceRoot,
            ExpectedObjectPaths,
            ExpectedObjects,
            ExpectedPackages,
            ExpectedPhysicalFiles,
            ExpectedPhysicalDirectories,
            ExpectedPhysicalRoot,
            OutError))
    {
        return false;
    }
    FNamespaceState Actual;
    CollectNamespaceState(NamespaceRoot, Actual);
    if (!Actual.bPhysicalRootResolved ||
        !FPaths::IsSamePath(Actual.PhysicalRoot, ExpectedPhysicalRoot))
    {
        OutError = TEXT("Tropical umbrella physical namespace resolution drifted.");
        return false;
    }
    for (const FString& ObjectPath : Actual.ObjectPaths)
    {
        if (!ExpectedObjects.Contains(ObjectPath))
        {
            OutError = TEXT("Tropical umbrella namespace contains an uncontracted asset: ") +
                ObjectPath;
            return false;
        }
    }
    for (const FString& PackagePath : Actual.PackagePaths)
    {
        if (!ExpectedPackages.Contains(PackagePath))
        {
            OutError = TEXT("Tropical umbrella namespace contains an uncontracted loaded or physical package: ") +
                PackagePath;
            return false;
        }
    }
    for (const FString& PhysicalFile : Actual.PhysicalFiles)
    {
        if (!ExpectedPhysicalFiles.Contains(PhysicalFile))
        {
            OutError = TEXT("Tropical umbrella namespace contains an uncontracted physical file: ") +
                PhysicalFile;
            return false;
        }
    }
    for (const FString& PhysicalDirectory : Actual.PhysicalDirectories)
    {
        if (!ExpectedPhysicalDirectories.Contains(PhysicalDirectory))
        {
            OutError = TEXT("Tropical umbrella namespace contains an uncontracted physical directory: ") +
                PhysicalDirectory;
            return false;
        }
    }
    if (bRequireExactRoster &&
        (Actual.ObjectPaths.Num() != ExpectedObjects.Num() ||
         Actual.PackagePaths.Num() != ExpectedPackages.Num() ||
         Actual.ObjectPaths.Difference(ExpectedObjects).Num() != 0 ||
         ExpectedObjects.Difference(Actual.ObjectPaths).Num() != 0 ||
         Actual.PackagePaths.Difference(ExpectedPackages).Num() != 0 ||
         ExpectedPackages.Difference(Actual.PackagePaths).Num() != 0))
    {
        OutError = TEXT("Tropical umbrella namespace lacks the exact expected object/package roster.");
        return false;
    }
    if (bRequirePhysicalAssetFiles &&
        (Actual.PhysicalFiles.Num() != ExpectedPhysicalFiles.Num() ||
         Actual.PhysicalFiles.Difference(ExpectedPhysicalFiles).Num() != 0 ||
         ExpectedPhysicalFiles.Difference(Actual.PhysicalFiles).Num() != 0 ||
         Actual.PhysicalDirectories.Difference(ExpectedPhysicalDirectories).Num() != 0 ||
         ExpectedPhysicalDirectories.Difference(Actual.PhysicalDirectories).Num() != 0))
    {
        OutError = TEXT("Saved tropical umbrella namespace lacks the exact physical .uasset files or directory closure.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool DeleteFreshNamespaceContents(
    const FString& NamespaceRoot,
    FString& OutError)
{
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutError = TEXT("Fresh namespace rollback requires EditorAssetSubsystem.");
        return false;
    }
    FAssetRegistryModule::GetRegistry().ScanPathsSynchronous(
        {NamespaceRoot}, true, true);
    FNamespaceState Before;
    CollectNamespaceState(NamespaceRoot, Before);
    TArray<UObject*> LoadedAssets;
    for (const FString& ObjectPath : Before.ObjectPaths)
    {
        if (UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath))
        {
            LoadedAssets.AddUnique(Object);
        }
    }
    const bool bLoadedDeleteReportedSuccess = LoadedAssets.IsEmpty() ||
        AssetSubsystem->DeleteLoadedAssets(LoadedAssets);
    const bool bDirectoryKnown =
        AssetSubsystem->DoesDirectoryExist(NamespaceRoot) ||
        (Before.bPhysicalRootResolved &&
         IFileManager::Get().DirectoryExists(*Before.PhysicalRoot));
    const bool bDirectoryDeleteReportedSuccess = !bDirectoryKnown ||
        AssetSubsystem->DeleteDirectory(NamespaceRoot);
    FAssetRegistryModule::GetRegistry().ScanPathsSynchronous(
        {NamespaceRoot}, true, true);
    FNamespaceState After;
    CollectNamespaceState(NamespaceRoot, After);
    const bool bPhysicalRootAbsent = After.bPhysicalRootResolved &&
        !IFileManager::Get().DirectoryExists(*After.PhysicalRoot);
    const bool bEmpty = After.ObjectPaths.IsEmpty() &&
        After.PackagePaths.IsEmpty() && After.PhysicalFiles.IsEmpty() &&
        After.PhysicalDirectories.IsEmpty();
    if (!bLoadedDeleteReportedSuccess ||
        !bDirectoryDeleteReportedSuccess || !bPhysicalRootAbsent || !bEmpty)
    {
        OutError = FString::Printf(
            TEXT("Atomic fresh-namespace rollback failed for '%s' (DeleteLoadedAssets=%s DeleteDirectory=%s physicalRootAbsent=%s registryLoadedPackageAndPhysicalNamespaceEmpty=%s)."),
            *NamespaceRoot,
            bLoadedDeleteReportedSuccess ? TEXT("true") : TEXT("false"),
            bDirectoryDeleteReportedSuccess ? TEXT("true") : TEXT("false"),
            bPhysicalRootAbsent ? TEXT("true") : TEXT("false"),
            bEmpty ? TEXT("true") : TEXT("false"));
        return false;
    }
    OutError.Reset();
    return true;
}

struct FFreshNamespaceBaseline
{
    bool bOutputProvenEmptyAtEntry = false;
    bool bStagingProvenEmptyAtEntry = false;
};

bool RollbackFreshNamespaces(
    const FFreshNamespaceBaseline& Baseline,
    FString& InOutError)
{
    if (!Baseline.bOutputProvenEmptyAtEntry ||
        !Baseline.bStagingProvenEmptyAtEntry)
    {
        InOutError += TEXT(" ROLLBACK_REFUSED_NAMESPACES_NOT_PROVEN_FRESH");
        return false;
    }
    FString OutputError;
    FString StagingError;
    const bool bOutputClean = DeleteFreshNamespaceContents(
        OutputRoot,
        OutputError);
    // Both scopes are always attempted; no short-circuit can strand assets.
    const bool bStagingClean = DeleteFreshNamespaceContents(
        StagingRoot,
        StagingError);
    if (!bOutputClean || !bStagingClean)
    {
        InOutError += FString::Printf(
            TEXT(" ROLLBACK_FAILED output='%s' staging='%s'"),
            *OutputError,
            *StagingError);
        return false;
    }
    InOutError += TEXT(" ROLLBACK_COMPLETE outputAndStagingRegistryLivePackagePhysicalFilesDirectories=0 physicalRootsAbsent=true");
    return true;
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
    int32 Variant,
    int32 Lod,
    FString& OutError)
{
    if (Variant < 0 || Variant >= VariantCount || Lod < 0 || Lod >= LodCount ||
        Description.NeedsCompact() ||
        Description.Vertices().Num() != LodVertices[Variant][Lod] ||
        Description.Triangles().Num() != LodTriangles[Variant][Lod] ||
        Description.PolygonGroups().Num() != MaterialCount)
    {
        OutError = TEXT("Tropical umbrella MeshDescription lost exact compact LOD topology or three polygon groups.");
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
        OutError = TEXT("Tropical umbrella import must retain exactly source UV0.");
        return false;
    }
    FBox3f Bounds(ForceInit);
    int32 RootVertices = 0;
    for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
    {
        const FVector3f Position = Positions.Get(Vertex);
        if (Position.ContainsNaN())
        {
            OutError = TEXT("Tropical umbrella import contains a non-finite position.");
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
            OutError = TEXT("Tropical umbrella import contains non-finite UV0 or normals.");
            return false;
        }
    }
    TSet<int32> SeenSlots;
    for (const FPolygonGroupID Group :
         Description.PolygonGroups().GetElementIDs())
    {
        const int32 Slot = MaterialSlotIndex(GroupNames.Get(Group));
        if (Slot == INDEX_NONE || SeenSlots.Contains(Slot))
        {
            OutError = TEXT("Tropical umbrella polygon groups lost unique Bark/LeafLive/LeafDry semantics.");
            return false;
        }
        SeenSlots.Add(Slot);
    }
    int32 TrianglesByMaterial[MaterialCount] = {0, 0, 0};
    for (const FTriangleID Triangle :
         Description.Triangles().GetElementIDs())
    {
        const int32 Slot = MaterialSlotIndex(GroupNames.Get(
            Description.GetTrianglePolygonGroup(Triangle)));
        if (Slot == INDEX_NONE)
        {
            OutError = TEXT("Tropical umbrella triangle references an unknown polygon group.");
            return false;
        }
        ++TrianglesByMaterial[Slot];
    }
    if (!Bounds.IsValid || RootVertices != LodRootVertices[Variant][Lod] ||
        !Bounds.Min.Equals(
            ExpectedUnrealBoundsMinCm[Variant][Lod],
            0.05f) ||
        !Bounds.Max.Equals(
            ExpectedUnrealBoundsMaxCm[Variant][Lod],
            0.05f))
    {
        OutError = TEXT("Tropical umbrella import lost exact X,-Y,Z centimetre bounds or root plane.");
        return false;
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        if (TrianglesByMaterial[Slot] !=
            LodTrianglesByMaterial[Variant][Lod][Slot])
        {
            OutError = TEXT("Tropical umbrella imported triangle/material route drifted.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateMeshSlots(
    const UStaticMesh* Mesh,
    const TArray<UMaterialInstanceConstant*>& Materials,
    bool bRequireBindings,
    FString& OutError)
{
    if (!Mesh || Mesh->GetStaticMaterials().Num() != MaterialCount ||
        (bRequireBindings &&
         (Materials.Num() != MaterialCount || Materials.Contains(nullptr))))
    {
        OutError = TEXT("Tropical umbrella mesh lost its exact three material slots.");
        return false;
    }
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        const FStaticMaterial& Binding = Mesh->GetStaticMaterials()[Slot];
        if (Binding.MaterialSlotName != FName(MaterialNames[Slot]) ||
            Binding.ImportedMaterialSlotName != FName(MaterialNames[Slot]) ||
            (bRequireBindings &&
             (Mesh->GetMaterial(Slot) != Materials[Slot] ||
              Materials[Slot]->GetPathName() != MaterialObjectPath(Slot))))
        {
            OutError = TEXT("Tropical umbrella Bark/LeafLive/LeafDry slot order or binding drifted.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

void WriteAdmissionMetadata(
    UObject* Asset,
    const FAdmission& Admission,
    const FString& Role)
{
    UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
    UMetaData* Metadata = Package ? Package->GetMetaData() : nullptr;
    if (!Asset || !Metadata)
    {
        return;
    }
    Metadata->SetValue(Asset, TEXT("TRIAD_TropicalUmbrellaRole"), *Role);
    Metadata->SetValue(Asset, TEXT("TRIAD_CandidateContractSha256"), *CandidateContractSha256);
    Metadata->SetValue(Asset, TEXT("TRIAD_TreeRealismContractSha256"), *TreeRealismContractSha256);
    Metadata->SetValue(Asset, TEXT("TRIAD_GeometrySelectorSha256"), *GeometrySelectorSha256);
    Metadata->SetValue(Asset, TEXT("TRIAD_SourceIdentityManifestSha256"), *SourceIdentityManifestSha256);
    Metadata->SetValue(Asset, TEXT("TRIAD_AcceptedCurrentReceiptSha256"), *Admission.AcceptedCurrentSha256);
    Metadata->SetValue(Asset, TEXT("TRIAD_FutureAuthorizationSha256"), *Admission.FutureAuthorizationSha256);
    Metadata->SetValue(Asset, TEXT("TRIAD_Authority"), TEXT("APPEARANCE_ONLY_NO_GEOGRAPHY_COLLISION_NAVIGATION_LOS_RF_SENSOR_TERRAIN_AUTHORITY"));
    Metadata->SetValue(Asset, TEXT("TRIAD_ExistingFallback"), *ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::ExistingFallbackMeshObjectPath());
}

bool ExactAssetMetadata(
    const UObject* Asset,
    const TCHAR* Key,
    const FString& Expected,
    FString& OutError)
{
    UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
    if (!Asset || !Package || !Package->HasMetaData())
    {
        OutError = TEXT("Tropical umbrella isolated asset has no retained admission metadata.");
        return false;
    }
    UMetaData* Metadata = Package->GetMetaData();
    if (!Metadata || Metadata->GetValue(Asset, Key) != Expected)
    {
        OutError = TEXT("Tropical umbrella isolated asset metadata drifted: ") +
            FString(Key);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAdmissionMetadata(
    const UObject* Asset,
    const FAdmission& Admission,
    const FString& Role,
    FString& OutError)
{
    return ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_TropicalUmbrellaRole"),
               Role,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_CandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_TreeRealismContractSha256"),
               TreeRealismContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GeometrySelectorSha256"),
               GeometrySelectorSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_SourceIdentityManifestSha256"),
               SourceIdentityManifestSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_AcceptedCurrentReceiptSha256"),
               Admission.AcceptedCurrentSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_FutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_Authority"),
               TEXT("APPEARANCE_ONLY_NO_GEOGRAPHY_COLLISION_NAVIGATION_LOS_RF_SENSOR_TERRAIN_AUTHORITY"),
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_ExistingFallback"),
               ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                   ExistingFallbackMeshObjectPath(),
               OutError);
}

template <typename TObjectType>
TObjectType* GetOnlyExactImportedObject(
    const UAssetImportTask* Task,
    const FString& ExpectedObjectPath,
    FString& OutError)
{
    if (!Task || Task->ImportedObjectPaths.Num() != 1 ||
        Task->ImportedObjectPaths[0] != ExpectedObjectPath ||
        Task->GetObjects().Num() != 1)
    {
        OutError = TEXT("OBJ importer returned an unexpected object roster.");
        return nullptr;
    }
    TObjectType* Object = Cast<TObjectType>(Task->GetObjects()[0]);
    if (!Object || Object->GetPathName() != ExpectedObjectPath)
    {
        OutError = TEXT("OBJ importer returned an unexpected object type or path.");
        return nullptr;
    }
    OutError.Reset();
    return Object;
}

UAssetImportTask* MakeObjImportTask(
    const FAdmission& Admission,
    int32 Variant,
    int32 Lod,
    FString& OutError)
{
    UFbxImportUI* Options = NewObject<UFbxImportUI>();
    UFbxFactory* Factory = NewObject<UFbxFactory>();
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    if (!Options || !Factory || !Task || !Options->StaticMeshImportData)
    {
        OutError = TEXT("Could not allocate deterministic tropical umbrella OBJ import options.");
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
    Task->Filename = CandidateSourcePath(
        Admission.CandidateRoot,
        ObjPins[Variant][Lod].RelativePath);
    Task->DestinationPath = StagingRoot;
    Task->DestinationName = StagingAssetName(Variant, Lod);
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

bool RevalidateImporterSourcePinsImmediatelyAfterImport(
    const FAdmission& Admission,
    FString& OutError)
{
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        for (int32 Lod = 0; Lod < LodCount; ++Lod)
        {
            const FSourceFilePin& Pin = ObjPins[Variant][Lod];
            TArray<uint8> Bytes;
            if (!LoadPinnedBytes(
                    CandidateSourcePath(
                        Admission.CandidateRoot,
                        Pin.RelativePath),
                    Pin.Bytes,
                    Pin.Sha256,
                    Bytes,
                    OutError))
            {
                OutError = TEXT("POST_IMPORT_OBJ_TOCTOU_PIN_CLOSED: ") +
                    OutError;
                return false;
            }
        }
    }
    TArray<uint8> MtlBytes;
    if (!LoadPinnedBytes(
            CandidateSourcePath(
                Admission.CandidateRoot,
                TEXT("Generated/M_IPV5D_TropicalUmbrellaHeroCandidate.mtl")),
            MaterialLibraryBytes,
            MaterialLibrarySha256,
            MtlBytes,
            OutError))
    {
        OutError = TEXT("POST_IMPORT_MTL_TOCTOU_WINDOW_CLOSED: ") +
            OutError;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ImportStagingLods(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    TArray<UStaticMesh*>& OutStagingMeshes,
    FString& OutError)
{
    OutStagingMeshes.Reset();
    TArray<UAssetImportTask*> Tasks;
    Tasks.Reserve(StagingAssetCount);
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        for (int32 Lod = 0; Lod < LodCount; ++Lod)
        {
            UAssetImportTask* Task = MakeObjImportTask(
                Admission,
                Variant,
                Lod,
                OutError);
            if (!Task)
            {
                return false;
            }
            Tasks.Add(Task);
        }
    }
    AssetTools.ImportAssetTasks(Tasks);
    if (!RevalidateImporterSourcePinsImmediatelyAfterImport(
            Admission,
            OutError))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        for (int32 Lod = 0; Lod < LodCount; ++Lod)
        {
            const int32 Flat = Variant * LodCount + Lod;
            UStaticMesh* Mesh = GetOnlyExactImportedObject<UStaticMesh>(
                Tasks[Flat],
                StagingObjectPath(Variant, Lod),
                OutError);
            FMeshDescription Description;
            const TArray<UMaterialInstanceConstant*> NoBindings;
            const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
            if (!Mesh || Mesh->GetNumSourceModels() != 1 ||
                Mesh->GetNumLODs() != 1 || Mesh->IsNaniteEnabled() ||
                !ValidateMeshSlots(Mesh, NoBindings, false, OutError) ||
                !Mesh->CloneMeshDescription(0, Description) ||
                !ValidateMeshDescription(
                    Description,
                    Variant,
                    Lod,
                    OutError) ||
                (Body && Body->AggGeom.GetElementCount() != 0))
            {
                if (OutError.IsEmpty())
                {
                    OutError = TEXT("Imported staging OBJ lost its exact single-LOD render-only source policy.");
                }
                return false;
            }
            OutStagingMeshes.Add(Mesh);
        }
    }
    TArray<FString> Expected;
    GetExpectedStagingObjectPaths(Expected);
    if (OutStagingMeshes.Num() != StagingAssetCount ||
        !NamespaceMatchesExactAssets(
            StagingRoot, Expected, true, false, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool SameNames(const TSet<FName>& Expected, const TSet<FName>& Actual)
{
    if (Expected.Num() != Actual.Num())
    {
        return false;
    }
    for (const FName& Name : Expected)
    {
        if (!Actual.Contains(Name))
        {
            return false;
        }
    }
    return true;
}

bool ValidateCandidateMaterial(
    UMaterialInstanceConstant* Material,
    int32 Slot,
    FString& OutError)
{
    UMaterialInterface* Parent = Slot >= 0 && Slot < MaterialCount
        ? LoadExact<UMaterialInterface>(MaterialParentObjectPaths[Slot])
        : nullptr;
    const FStaticParameterSet StaticParameters = Material
        ? Material->GetStaticParameters()
        : FStaticParameterSet{};
    if (!Material || !Parent || Slot < 0 || Slot >= MaterialCount ||
        Material->GetClass() != UMaterialInstanceConstant::StaticClass() ||
        Material->GetPathName() != MaterialObjectPath(Slot) ||
        Material->Parent != Parent ||
        !Material->TextureParameterValues.IsEmpty() ||
        !Material->DoubleVectorParameterValues.IsEmpty() ||
        !Material->TextureCollectionParameterValues.IsEmpty() ||
        !Material->RuntimeVirtualTextureParameterValues.IsEmpty() ||
        !Material->SparseVolumeTextureParameterValues.IsEmpty() ||
        !Material->FontParameterValues.IsEmpty() ||
        !Material->UserSceneTextureOverrides.IsEmpty() ||
        !StaticParameters.StaticSwitchParameters.IsEmpty() ||
        !StaticParameters.EditorOnly.StaticComponentMaskParameters.IsEmpty() ||
        !StaticParameters.EditorOnly.TerrainLayerWeightParameters.IsEmpty() ||
        StaticParameters.bHasMaterialLayers)
    {
        OutError = TEXT("Tropical umbrella MIC lost its exact parent, path, class, or response-only override policy.");
        return false;
    }

    TSet<FName> ExpectedScalarNames;
    TSet<FName> ExpectedVectorNames;
    if (Slot == LeafDryMaterialSlot)
    {
        for (const FScalarOverride& Spec : LeafDryScalarOverrides)
        {
            ExpectedScalarNames.Add(FName(Spec.Name));
        }
        for (const FVectorOverride& Spec : LeafDryVectorOverrides)
        {
            ExpectedVectorNames.Add(FName(Spec.Name));
        }
    }
    TSet<FName> ActualScalarNames;
    for (const FScalarParameterValue& Value : Material->ScalarParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualScalarNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = TEXT("Tropical umbrella MIC has an invalid or duplicate scalar override.");
            return false;
        }
        ActualScalarNames.Add(Value.ParameterInfo.Name);
    }
    TSet<FName> ActualVectorNames;
    for (const FVectorParameterValue& Value : Material->VectorParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualVectorNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = TEXT("Tropical umbrella MIC has an invalid or duplicate vector override.");
            return false;
        }
        ActualVectorNames.Add(Value.ParameterInfo.Name);
    }
    if (!SameNames(ExpectedScalarNames, ActualScalarNames) ||
        !SameNames(ExpectedVectorNames, ActualVectorNames))
    {
        OutError = TEXT("Tropical umbrella MIC response override roster drifted; only LeafDry may own the exact six-scalar/three-vector set.");
        return false;
    }
    if (Slot == LeafDryMaterialSlot)
    {
        for (const FScalarOverride& Spec : LeafDryScalarOverrides)
        {
            float Actual = 0.0f;
            if (!Material->GetScalarParameterValue(
                    FMaterialParameterInfo(FName(Spec.Name)),
                    Actual) ||
                !FMath::IsNearlyEqual(Actual, Spec.Value, 0.000001f))
            {
                OutError = TEXT("Tropical umbrella LeafDry scalar response drifted: ") +
                    FString(Spec.Name);
                return false;
            }
        }
        for (const FVectorOverride& Spec : LeafDryVectorOverrides)
        {
            FLinearColor Actual;
            if (!Material->GetVectorParameterValue(
                    FMaterialParameterInfo(FName(Spec.Name)),
                    Actual) ||
                !Actual.Equals(Spec.Value, 0.000001f))
            {
                OutError = TEXT("Tropical umbrella LeafDry vector response drifted: ") +
                    FString(Spec.Name);
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateLeafOpacityWindInheritance(
    UMaterialInstanceConstant* LeafLive,
    UMaterialInstanceConstant* LeafDry,
    FString& OutError)
{
    UMaterialInterface* Parent = LoadExact<UMaterialInterface>(
        MaterialParentObjectPaths[LeafLiveMaterialSlot]);
    if (!LeafLive || !LeafDry || !Parent ||
        LeafLive->Parent != Parent || LeafDry->Parent != Parent ||
        LeafLive->GetBlendMode() != BLEND_Masked ||
        LeafDry->GetBlendMode() != BLEND_Masked ||
        !LeafLive->IsTwoSided() || !LeafDry->IsTwoSided() ||
        !LeafLive->GetShadingModels().HasOnlyShadingModel(
            MSM_TwoSidedFoliage) ||
        !LeafDry->GetShadingModels().HasOnlyShadingModel(
            MSM_TwoSidedFoliage) ||
        !FMath::IsNearlyEqual(
            LeafLive->GetOpacityMaskClipValue(),
            InheritedLeafOpacityMaskClip,
            0.000001f) ||
        !FMath::IsNearlyEqual(
            LeafDry->GetOpacityMaskClipValue(),
            InheritedLeafOpacityMaskClip,
            0.000001f))
    {
        OutError = TEXT("Closed tropical umbrella leaflets lost the exact inherited masked, two-sided foliage/opacity policy.");
        return false;
    }
    for (const TCHAR* Name : InheritedWindScalarNames)
    {
        float ParentValue = 0.0f;
        float LiveValue = 0.0f;
        float DryValue = 0.0f;
        const FMaterialParameterInfo Parameter{FName(Name)};
        if (!Parent->GetScalarParameterValue(Parameter, ParentValue) ||
            !LeafLive->GetScalarParameterValue(Parameter, LiveValue) ||
            !LeafDry->GetScalarParameterValue(Parameter, DryValue) ||
            !FMath::IsFinite(ParentValue) ||
            !FMath::IsNearlyEqual(LiveValue, ParentValue, 0.000001f) ||
            !FMath::IsNearlyEqual(DryValue, ParentValue, 0.000001f))
        {
            OutError = TEXT("Closed tropical umbrella leaflets do not inherit one exact finite TreeRealism wind scalar: ") +
                FString(Name);
            return false;
        }
    }
    FLinearColor ParentDirection;
    FLinearColor LiveDirection;
    FLinearColor DryDirection;
    const FMaterialParameterInfo WindDirection{
        FName(InheritedWindVectorName)};
    if (!Parent->GetVectorParameterValue(WindDirection, ParentDirection) ||
        !LeafLive->GetVectorParameterValue(WindDirection, LiveDirection) ||
        !LeafDry->GetVectorParameterValue(WindDirection, DryDirection) ||
        !LiveDirection.Equals(ParentDirection, 0.000001f) ||
        !DryDirection.Equals(ParentDirection, 0.000001f))
    {
        OutError = TEXT("Closed tropical umbrella leaflets do not inherit the exact TreeRealism wind direction.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateCandidateMaterials(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    TArray<UMaterialInstanceConstant*>& OutMaterials,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        UMaterialInterface* Parent = LoadExact<UMaterialInterface>(
            MaterialParentObjectPaths[Slot]);
        UMaterialInstanceConstantFactoryNew* Factory =
            NewObject<UMaterialInstanceConstantFactoryNew>();
        if (!Parent || !Factory)
        {
            OutError = TEXT("Exact TreeRealism response parent is unavailable for isolated candidate material creation.");
            return false;
        }
        Factory->InitialParent = Parent;
        UMaterialInstanceConstant* Material =
            Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
                MaterialAssetNames[Slot],
                FPaths::Combine(OutputRoot, TEXT("Materials")),
                UMaterialInstanceConstant::StaticClass(),
                Factory));
        if (!Material || Material->GetPathName() != MaterialObjectPath(Slot) ||
            Material->Parent != Parent)
        {
            OutError = TEXT("Could not create an exact isolated candidate material instance.");
            return false;
        }
        Material->Modify();
        if (Slot == LeafDryMaterialSlot)
        {
            for (const FScalarOverride& Spec : LeafDryScalarOverrides)
            {
                Material->SetScalarParameterValueEditorOnly(
                    FMaterialParameterInfo(FName(Spec.Name)),
                    Spec.Value);
            }
            for (const FVectorOverride& Spec : LeafDryVectorOverrides)
            {
                Material->SetVectorParameterValueEditorOnly(
                    FMaterialParameterInfo(FName(Spec.Name)),
                    Spec.Value);
            }
        }
        WriteAdmissionMetadata(
            Material,
            Admission,
            FString::Printf(TEXT("MATERIAL_%s"), MaterialNames[Slot]));
        if (UPackage* Package = Material->GetOutermost())
        {
            if (UMetaData* Metadata = Package->GetMetaData())
            {
                Metadata->SetValue(
                    Material,
                    TEXT("TRIAD_ClosedLeafletOpacityWindPolicy"),
                    TEXT("EXACT_TREE_REALISM_INHERITANCE_SOURCE_SANITY_ONLY_NATIVE_LOD_WIND_AND_ALPHA_ACCEPTANCE_REQUIRED"));
                Metadata->SetValue(
                    Material,
                    TEXT("TRIAD_LeafDryResponseOverrides"),
                    Slot == LeafDryMaterialSlot
                        ? TEXT("TintLow=1.30,0.79,0.38 TintHigh=1.12,0.70,0.34 Subsurface=0.84,0.58,0.26 Luma=0.82..0.96 Desaturation=0.28 Roughness=0.72..0.90 BaseColorMax=0.72")
                        : TEXT("NONE"));
            }
        }
        Material->PostEditChange();
        Material->MarkPackageDirty();
        if (!ValidateCandidateMaterial(Material, Slot, OutError) ||
            !ValidateAdmissionMetadata(
                Material,
                Admission,
                FString::Printf(
                    TEXT("MATERIAL_%s"),
                    MaterialNames[Slot]),
                OutError) ||
            !ExactAssetMetadata(
                Material,
                TEXT("TRIAD_ClosedLeafletOpacityWindPolicy"),
                TEXT("EXACT_TREE_REALISM_INHERITANCE_SOURCE_SANITY_ONLY_NATIVE_LOD_WIND_AND_ALPHA_ACCEPTANCE_REQUIRED"),
                OutError) ||
            !ExactAssetMetadata(
                Material,
                TEXT("TRIAD_LeafDryResponseOverrides"),
                Slot == LeafDryMaterialSlot
                    ? TEXT("TintLow=1.30,0.79,0.38 TintHigh=1.12,0.70,0.34 Subsurface=0.84,0.58,0.26 Luma=0.82..0.96 Desaturation=0.28 Roughness=0.72..0.90 BaseColorMax=0.72")
                    : TEXT("NONE"),
                OutError))
        {
            return false;
        }
        OutMaterials.Add(Material);
        OutAssetsToSave.Add(Material);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (OutMaterials.Num() != MaterialCount ||
        !ValidateLeafOpacityWindInheritance(
            OutMaterials[LeafLiveMaterialSlot],
            OutMaterials[LeafDryMaterialSlot],
            OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateFinalMesh(
    const FAdmission& Admission,
    int32 Variant,
    const TArray<UStaticMesh*>& StagingMeshes,
    const TArray<UMaterialInstanceConstant*>& Materials,
    UStaticMesh*& OutMesh,
    FString& OutError)
{
    OutMesh = nullptr;
    if (Variant < 0 || Variant >= VariantCount ||
        StagingMeshes.Num() != StagingAssetCount ||
        StagingMeshes.Contains(nullptr) ||
        Materials.Num() != MaterialCount || Materials.Contains(nullptr))
    {
        OutError = TEXT("Final tropical umbrella mesh requires all nine staging LODs and three materials.");
        return false;
    }
    UStaticMesh* SourceLod0 = StagingMeshes[Variant * LodCount];
    UPackage* Package = CreatePackage(*FinalMeshPackagePath(Variant));
    UStaticMesh* Mesh = Package
        ? Cast<UStaticMesh>(StaticDuplicateObject(
              SourceLod0,
              Package,
              FName(*FinalMeshAssetName(Variant)),
              RF_Public | RF_Standalone | RF_Transactional))
        : nullptr;
    if (!Mesh || Mesh == SourceLod0 ||
        Mesh->GetPathName() != FinalMeshObjectPath(Variant))
    {
        OutError = TEXT("Could not allocate the isolated final tropical umbrella mesh package.");
        return false;
    }
    Mesh->Modify();
    Mesh->SetNumSourceModels(LodCount);
    Mesh->bAutoComputeLODScreenSize = false;
    const float ScreenSizes[LodCount] = {1.0f, 0.42f, 0.16f};
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        FMeshDescription Description;
        if (!StagingMeshes[Variant * LodCount + Lod]->CloneMeshDescription(
                0,
                Description) ||
            !ValidateMeshDescription(
                Description,
                Variant,
                Lod,
                OutError))
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
            OutError = TEXT("Could not install an exact candidate source LOD MeshDescription.");
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
    WriteAdmissionMetadata(
        Mesh,
        Admission,
        FString::Printf(TEXT("MESH_VARIANT_%s"), VariantNames[Variant]));
    if (UMetaData* Metadata = Package->GetMetaData())
    {
        for (int32 Lod = 0; Lod < LodCount; ++Lod)
        {
            Metadata->SetValue(
                Mesh,
                *FString::Printf(TEXT("TRIAD_LOD%dObjSha256"), Lod),
                ObjPins[Variant][Lod].Sha256);
        }
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Mesh);
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (Mesh->GetNumLODs() != LodCount || Mesh->IsNaniteEnabled() ||
        !ValidateMeshSlots(Mesh, Materials, true, OutError) ||
        !ValidateAdmissionMetadata(
            Mesh,
            Admission,
            FString::Printf(
                TEXT("MESH_VARIANT_%s"),
                VariantNames[Variant]),
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Assembled candidate mesh failed exact LOD/material policy.");
        }
        return false;
    }
    for (int32 Lod = 0; Lod < LodCount; ++Lod)
    {
        FMeshDescription Description;
        if (!Mesh->CloneMeshDescription(Lod, Description) ||
            !ValidateMeshDescription(
                Description,
                Variant,
                Lod,
                OutError) ||
            !ExactAssetMetadata(
                Mesh,
                *FString::Printf(TEXT("TRIAD_LOD%dObjSha256"), Lod),
                ObjPins[Variant][Lod].Sha256,
                OutError))
        {
            return false;
        }
    }
    OutMesh = Mesh;
    OutError.Reset();
    return true;
}

bool CreateFinalMeshes(
    const FAdmission& Admission,
    const TArray<UStaticMesh*>& StagingMeshes,
    const TArray<UMaterialInstanceConstant*>& Materials,
    TArray<UStaticMesh*>& OutMeshes,
    TArray<UObject*>& OutAssetsToSave,
    FString& OutError)
{
    OutMeshes.Reset();
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        UStaticMesh* Mesh = nullptr;
        if (!CreateFinalMesh(
                Admission,
                Variant,
                StagingMeshes,
                Materials,
                Mesh,
                OutError))
        {
            return false;
        }
        OutMeshes.Add(Mesh);
        OutAssetsToSave.Add(Mesh);
    }
    OutError.Reset();
    return OutMeshes.Num() == VariantCount;
}

bool BuildRuntimeAssetRoster(
    const TArray<UStaticMesh*>& CandidateMeshes,
    FTRIADIstanaExploreV5DTropicalUmbrellaAssets& OutAssets,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DTropicalUmbrellaAssets{};
    if (CandidateMeshes.Num() != VariantCount ||
        CandidateMeshes.Contains(nullptr))
    {
        OutError = TEXT("Runtime candidate roster requires exact A/B/C meshes.");
        return false;
    }
    for (UStaticMesh* Mesh : CandidateMeshes)
    {
        OutAssets.CandidateVariantMeshes.Add(Mesh);
    }
    OutAssets.ExistingTreeRealismUmbrellaFallback = LoadExact<UStaticMesh>(
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            ExistingFallbackMeshObjectPath());
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        OutAssets.ExistingFallbackMaterials.Add(
            LoadExact<UMaterialInterface>(
                ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                    ExistingFallbackMaterialObjectPath(Slot)));
    }
    return ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
        ValidateAssetRoster(OutAssets, OutError);
}

bool LoadAndValidateOutputs(
    const FAdmission& Admission,
    FTRIADIstanaExploreV5DTropicalUmbrellaAssets& OutAssets,
    FString& OutError)
{
    TArray<UMaterialInstanceConstant*> Materials;
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        UMaterialInstanceConstant* Material =
            LoadExact<UMaterialInstanceConstant>(MaterialObjectPath(Slot));
        if (!ValidateCandidateMaterial(Material, Slot, OutError) ||
            !ValidateAdmissionMetadata(
                Material,
                Admission,
                FString::Printf(
                    TEXT("MATERIAL_%s"),
                    MaterialNames[Slot]),
                OutError) ||
            !ExactAssetMetadata(
                Material,
                TEXT("TRIAD_ClosedLeafletOpacityWindPolicy"),
                TEXT("EXACT_TREE_REALISM_INHERITANCE_SOURCE_SANITY_ONLY_NATIVE_LOD_WIND_AND_ALPHA_ACCEPTANCE_REQUIRED"),
                OutError) ||
            !ExactAssetMetadata(
                Material,
                TEXT("TRIAD_LeafDryResponseOverrides"),
                Slot == LeafDryMaterialSlot
                    ? TEXT("TintLow=1.30,0.79,0.38 TintHigh=1.12,0.70,0.34 Subsurface=0.84,0.58,0.26 Luma=0.82..0.96 Desaturation=0.28 Roughness=0.72..0.90 BaseColorMax=0.72")
                    : TEXT("NONE"),
                OutError))
        {
            return false;
        }
        Materials.Add(Material);
    }
    if (!ValidateLeafOpacityWindInheritance(
            Materials[LeafLiveMaterialSlot],
            Materials[LeafDryMaterialSlot],
            OutError))
    {
        return false;
    }
    TArray<UStaticMesh*> CandidateMeshes;
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        UStaticMesh* Mesh =
            LoadExact<UStaticMesh>(FinalMeshObjectPath(Variant));
        if (!ValidateAdmissionMetadata(
                Mesh,
                Admission,
                FString::Printf(
                    TEXT("MESH_VARIANT_%s"),
                    VariantNames[Variant]),
                OutError))
        {
            return false;
        }
        for (int32 Lod = 0; Lod < LodCount; ++Lod)
        {
            if (!ExactAssetMetadata(
                    Mesh,
                    *FString::Printf(
                        TEXT("TRIAD_LOD%dObjSha256"),
                        Lod),
                    ObjPins[Variant][Lod].Sha256,
                    OutError))
            {
                return false;
            }
        }
        CandidateMeshes.Add(Mesh);
    }
    return BuildRuntimeAssetRoster(CandidateMeshes, OutAssets, OutError);
}

enum class EExactSourceOwnerKind : uint8
{
    V4Landscape,
    V5DLandmarkVegetation,
};

bool ValidateExactTargetPersistentEditorWorld(
    const ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor* CandidateActor,
    UWorld*& OutWorld,
    FString& OutError)
{
    OutWorld = CandidateActor ? CandidateActor->GetWorld() : nullptr;
    UPackage* WorldPackage = OutWorld ? OutWorld->GetOutermost() : nullptr;
    if (!CandidateActor ||
        CandidateActor->GetClass() !=
            ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::StaticClass() ||
        !OutWorld || OutWorld->WorldType != EWorldType::Editor ||
        OutWorld->HasBegunPlay() || !OutWorld->PersistentLevel ||
        !WorldPackage || WorldPackage->GetName() != TargetMapPackage ||
        OutWorld->PersistentLevel->GetOutermost() != WorldPackage ||
        CandidateActor->GetLevel() != OutWorld->PersistentLevel)
    {
        OutWorld = nullptr;
        OutError = TEXT("Candidate actor is not the exact class in the persistent level of /Game/Maps/Istana_PublicView_Explore_v5d_hybrid editor world.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateExactSourceOwnerBinding(
    UHierarchicalInstancedStaticMeshComponent* Component,
    EExactSourceOwnerKind OwnerKind,
    UWorld* ExpectedWorld,
    const AActor* ExpectedOwner,
    FString& OutError)
{
    AActor* Owner = Component ? Component->GetOwner() : nullptr;
    if (!Component || !Owner || !ExpectedWorld ||
        Owner->GetWorld() != ExpectedWorld ||
        Owner->GetLevel() != ExpectedWorld->PersistentLevel ||
        (ExpectedOwner && Owner != ExpectedOwner) ||
        !Component->GetAttachChildren().IsEmpty())
    {
        OutError = TEXT("Exact source component owner/world/persistent-level identity drifted or the component has attached children.");
        return false;
    }
    if (OwnerKind == EExactSourceOwnerKind::V4Landscape)
    {
        const ATRIADIstanaExploreV4LandscapeActor* Typed =
            Cast<ATRIADIstanaExploreV4LandscapeActor>(Owner);
        if (!Typed ||
            Owner->GetClass() !=
                ATRIADIstanaExploreV4LandscapeActor::StaticClass() ||
            Typed->HeritageUmbrellaInstances.Get() != Component)
        {
            OutError = TEXT("V4 umbrella source is not the exact landscape-owner HeritageUmbrellaInstances property.");
            return false;
        }
    }
    else
    {
        const ATRIADIstanaExploreV5DLandmarkVegetationActor* Typed =
            Cast<ATRIADIstanaExploreV5DLandmarkVegetationActor>(Owner);
        if (!Typed ||
            Owner->GetClass() !=
                ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass() ||
            Typed->UmbrellaTreeInstances.Get() != Component)
        {
            OutError = TEXT("R29 umbrella source is not the exact landmark-vegetation-owner UmbrellaTreeInstances property.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

struct FExactSourceComponentSnapshot
{
    UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
    AActor* ExactOwner = nullptr;
    UWorld* ExactWorld = nullptr;
    EExactSourceOwnerKind OwnerKind =
        EExactSourceOwnerKind::V4Landscape;
    FName ExpectedComponentName = NAME_None;
    int32 ExpectedInstanceCount = 0;
    bool bVisible = false;
    bool bHiddenInGame = true;
    ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
    bool bCanEverAffectNavigation = false;
    UStaticMesh* StaticMesh = nullptr;
    TArray<UMaterialInterface*> Materials;
    TArray<FTransform> WorldTransforms;
};

bool CaptureExactSourceComponent(
    UHierarchicalInstancedStaticMeshComponent* Component,
    EExactSourceOwnerKind OwnerKind,
    const FName ExpectedComponentName,
    int32 ExpectedInstanceCount,
    UWorld* ExpectedWorld,
    FExactSourceComponentSnapshot& OutSnapshot,
    FString& OutError)
{
    OutSnapshot = FExactSourceComponentSnapshot{};
    UStaticMesh* ExactFallback = LoadExact<UStaticMesh>(
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            ExistingFallbackMeshObjectPath());
    if (!ValidateExactSourceOwnerBinding(
            Component,
            OwnerKind,
            ExpectedWorld,
            nullptr,
            OutError))
    {
        return false;
    }
    if (!ExactFallback ||
        Component->GetFName() != ExpectedComponentName ||
        Component->GetInstanceCount() != ExpectedInstanceCount ||
        Component->GetStaticMesh() != ExactFallback ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->CanEverAffectNavigation() ||
        Component->GetNumMaterials() != MaterialCount ||
        !Component->IsVisible() || Component->bHiddenInGame)
    {
        OutError = FString::Printf(
            TEXT("Exact visible source HISM '%s' lost its native count, TreeRealism umbrella fallback, render-only policy, or world identity."),
            *ExpectedComponentName.ToString());
        return false;
    }
    OutSnapshot.Component = Component;
    OutSnapshot.ExactOwner = Component->GetOwner();
    OutSnapshot.ExactWorld = ExpectedWorld;
    OutSnapshot.OwnerKind = OwnerKind;
    OutSnapshot.ExpectedComponentName = ExpectedComponentName;
    OutSnapshot.ExpectedInstanceCount = ExpectedInstanceCount;
    OutSnapshot.bVisible = Component->IsVisible();
    OutSnapshot.bHiddenInGame = Component->bHiddenInGame;
    OutSnapshot.CollisionEnabled = Component->GetCollisionEnabled();
    OutSnapshot.bCanEverAffectNavigation =
        Component->CanEverAffectNavigation();
    OutSnapshot.StaticMesh = Component->GetStaticMesh();
    for (int32 Slot = 0; Slot < MaterialCount; ++Slot)
    {
        UMaterialInterface* Material = Component->GetMaterial(Slot);
        if (!Material)
        {
            OutSnapshot = FExactSourceComponentSnapshot{};
            OutError = TEXT("An exact source umbrella HISM has a null effective material.");
            return false;
        }
        OutSnapshot.Materials.Add(Material);
    }
    OutSnapshot.WorldTransforms.Reserve(ExpectedInstanceCount);
    for (int32 LocalIndex = 0; LocalIndex < ExpectedInstanceCount;
         ++LocalIndex)
    {
        FTransform WorldTransform;
        if (!Component->GetInstanceTransform(
                LocalIndex,
                WorldTransform,
                true) ||
            WorldTransform.ContainsNaN() ||
            !WorldTransform.GetRotation().IsNormalized())
        {
            OutSnapshot = FExactSourceComponentSnapshot{};
            OutError = TEXT("An exact source umbrella instance has no finite normalized native world transform.");
            return false;
        }
        OutSnapshot.WorldTransforms.Add(WorldTransform);
    }
    OutError.Reset();
    return true;
}

bool ValidateExactSourceComponentUnchanged(
    const FExactSourceComponentSnapshot& Snapshot,
    bool bRequireSuppressed,
    FString& OutError)
{
    UHierarchicalInstancedStaticMeshComponent* Component =
        Snapshot.Component;
    if (!ValidateExactSourceOwnerBinding(
            Component,
            Snapshot.OwnerKind,
            Snapshot.ExactWorld,
            Snapshot.ExactOwner,
            OutError))
    {
        return false;
    }
    if (
        Component->GetFName() != Snapshot.ExpectedComponentName ||
        Component->GetInstanceCount() != Snapshot.ExpectedInstanceCount ||
        Component->GetStaticMesh() != Snapshot.StaticMesh ||
        Component->GetCollisionEnabled() != Snapshot.CollisionEnabled ||
        Component->CanEverAffectNavigation() !=
            Snapshot.bCanEverAffectNavigation ||
        Component->GetNumMaterials() != Snapshot.Materials.Num() ||
        Snapshot.WorldTransforms.Num() != Snapshot.ExpectedInstanceCount ||
        (bRequireSuppressed &&
         (Component->IsVisible() || !Component->bHiddenInGame)))
    {
        OutError = TEXT("Exact source umbrella HISM changed outside its bounded presentation state.");
        return false;
    }
    for (int32 Slot = 0; Slot < Snapshot.Materials.Num(); ++Slot)
    {
        if (Component->GetMaterial(Slot) != Snapshot.Materials[Slot])
        {
            OutError = TEXT("Exact source umbrella HISM effective material changed during presentation swap.");
            return false;
        }
    }
    for (int32 LocalIndex = 0;
         LocalIndex < Snapshot.ExpectedInstanceCount;
         ++LocalIndex)
    {
        FTransform Actual;
        if (!Component->GetInstanceTransform(LocalIndex, Actual, true) ||
            !ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                ExactSourceTransformBitsMatch(
                    Actual,
                    Snapshot.WorldTransforms[LocalIndex]))
        {
            OutError = TEXT("Exact source umbrella instance count/order/world-transform bits changed.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

void AppendTransformScalarBits(TArray<uint8>& Bytes, double Value)
{
    static_assert(sizeof(double) == 8);
    const int32 Offset = Bytes.AddUninitialized(sizeof(double));
    FMemory::Memcpy(Bytes.GetData() + Offset, &Value, sizeof(double));
}

FString ExactOrderedTransformBitHash(
    const TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>& Anchors)
{
    TArray<uint8> Bytes;
    Bytes.Reserve(Anchors.Num() * 10 * sizeof(double));
    for (const FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor& Anchor :
         Anchors)
    {
        const FVector Translation =
            Anchor.SourceWorldTransform.GetTranslation();
        const FQuat Rotation = Anchor.SourceWorldTransform.GetRotation();
        const FVector Scale = Anchor.SourceWorldTransform.GetScale3D();
        AppendTransformScalarBits(Bytes, Translation.X);
        AppendTransformScalarBits(Bytes, Translation.Y);
        AppendTransformScalarBits(Bytes, Translation.Z);
        AppendTransformScalarBits(Bytes, Rotation.X);
        AppendTransformScalarBits(Bytes, Rotation.Y);
        AppendTransformScalarBits(Bytes, Rotation.Z);
        AppendTransformScalarBits(Bytes, Rotation.W);
        AppendTransformScalarBits(Bytes, Scale.X);
        AppendTransformScalarBits(Bytes, Scale.Y);
        AppendTransformScalarBits(Bytes, Scale.Z);
    }
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (Bytes.IsEmpty() ||
        !SHA256(
            Bytes.GetData(),
            static_cast<size_t>(Bytes.Num()),
            Digest))
    {
        return FString();
    }
    return BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
}

bool BuildExactNativeAnchorCopy(
    const TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>&
        RequestedAnchors,
    const FExactSourceComponentSnapshot& V4Heritage,
    const FExactSourceComponentSnapshot& R29Landmark,
    TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>&
        OutNativeAnchors,
    FString& OutTransformBitHash,
    FString& OutError)
{
    OutNativeAnchors.Reset();
    OutTransformBitHash.Reset();
    if (RequestedAnchors.Num() != 6 ||
        V4Heritage.WorldTransforms.Num() != 4 ||
        R29Landmark.WorldTransforms.Num() != 2)
    {
        OutError = TEXT("Exact native umbrella source extraction requires ordered component-local counts 4+2.");
        return false;
    }
    OutNativeAnchors = RequestedAnchors;
    for (int32 Ordinal = 0; Ordinal < RequestedAnchors.Num(); ++Ordinal)
    {
        const FTransform& Native = Ordinal < 4
            ? V4Heritage.WorldTransforms[Ordinal]
            : R29Landmark.WorldTransforms[Ordinal - 4];
        if (!ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                ExactSourceTransformBitsMatch(
                    RequestedAnchors[Ordinal].SourceWorldTransform,
                    Native))
        {
            OutNativeAnchors.Reset();
            OutError = FString::Printf(
                TEXT("Caller anchor %d is not the bit-exact native component-local world transform."),
                Ordinal);
            return false;
        }
        // Copy the native readback value itself; never reconstruct a pose.
        OutNativeAnchors[Ordinal].SourceWorldTransform = Native;
    }
    FTRIADIstanaExploreV5DTropicalUmbrellaLayout Layout;
    if (!ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            BuildDeterministicLayout(
                OutNativeAnchors,
                Layout,
                OutError))
    {
        OutNativeAnchors.Reset();
        return false;
    }
    OutTransformBitHash = ExactOrderedTransformBitHash(OutNativeAnchors);
    if (!IsSha256(OutTransformBitHash))
    {
        OutNativeAnchors.Reset();
        OutError = TEXT("Could not hash the exact ordered native transform bits.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreExactSourcePresentation(
    const FExactSourceComponentSnapshot& V4Heritage,
    const FExactSourceComponentSnapshot& R29Landmark,
    FString& OutError)
{
    for (const FExactSourceComponentSnapshot* Snapshot :
         {&V4Heritage, &R29Landmark})
    {
        if (Snapshot->Component)
        {
            Snapshot->Component->SetVisibility(
                Snapshot->bVisible,
                false);
            Snapshot->Component->SetHiddenInGame(
                Snapshot->bHiddenInGame,
                false);
        }
    }
    FString V4Error;
    FString R29Error;
    const bool bV4Unchanged = ValidateExactSourceComponentUnchanged(
        V4Heritage,
        false,
        V4Error) &&
        V4Heritage.Component->IsVisible() == V4Heritage.bVisible &&
        V4Heritage.Component->bHiddenInGame == V4Heritage.bHiddenInGame;
    const bool bR29Unchanged = ValidateExactSourceComponentUnchanged(
        R29Landmark,
        false,
        R29Error) &&
        R29Landmark.Component->IsVisible() == R29Landmark.bVisible &&
        R29Landmark.Component->bHiddenInGame == R29Landmark.bHiddenInGame;
    if (!bV4Unchanged || !bR29Unchanged)
    {
        OutError = FString::Printf(
            TEXT("Exact source presentation rollback failed: v4='%s' r29='%s'."),
            *V4Error,
            *R29Error);
        return false;
    }
    OutError.Reset();
    return true;
}

bool SuppressExactSourcePresentation(
    const FExactSourceComponentSnapshot& V4Heritage,
    const FExactSourceComponentSnapshot& R29Landmark,
    FString& OutError)
{
    // The two native form-partitioned components contain exactly 4+2 source
    // instances. Suppress only these components; no instance is added,
    // removed, reordered or transformed.
    V4Heritage.Component->SetVisibility(false, false);
    V4Heritage.Component->SetHiddenInGame(true, false);
    R29Landmark.Component->SetVisibility(false, false);
    R29Landmark.Component->SetHiddenInGame(true, false);
    FString V4Error;
    FString R29Error;
    if (!ValidateExactSourceComponentUnchanged(
            V4Heritage,
            true,
            V4Error) ||
        !ValidateExactSourceComponentUnchanged(
            R29Landmark,
            true,
            R29Error))
    {
        FString RestoreError;
        RestoreExactSourcePresentation(
            V4Heritage,
            R29Landmark,
            RestoreError);
        OutError = FString::Printf(
            TEXT("Compensating-atomic exact six-source suppression failed: v4='%s' r29='%s' restore='%s'."),
            *V4Error,
            *R29Error,
            *RestoreError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateActivatedCandidatePresentation(
    const ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor* CandidateActor,
    const TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>&
        NativeAnchors,
    FString& OutError)
{
    if (!CandidateActor || NativeAnchors.Num() != 6 ||
        CandidateActor->CandidateComponents.Num() != VariantCount)
    {
        OutError = TEXT("Activated tropical umbrella presentation lost its actor/component/source census.");
        return false;
    }
    TArray<FTransform> ExpectedByVariant[VariantCount];
    for (int32 Ordinal = 0; Ordinal < NativeAnchors.Num(); ++Ordinal)
    {
        const int32 Variant =
            ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                ExpectedVariantSelector(Ordinal) - TEXT('A');
        if (Variant < 0 || Variant >= VariantCount)
        {
            OutError = TEXT("Activated tropical umbrella selector is invalid.");
            return false;
        }
        ExpectedByVariant[Variant].Add(
            NativeAnchors[Ordinal].SourceWorldTransform);
    }
    const int32 ExpectedCounts[VariantCount] = {1, 4, 1};
    for (int32 Variant = 0; Variant < VariantCount; ++Variant)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            CandidateActor->CandidateComponents[Variant];
        if (!Component || !Component->IsVisible() ||
            Component->bHiddenInGame ||
            Component->GetStaticMesh() == nullptr ||
            Component->GetStaticMesh()->GetPathName() !=
                ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                    CandidateMeshObjectPath(Variant) ||
            Component->GetInstanceCount() != ExpectedCounts[Variant] ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            Component->CanEverAffectNavigation())
        {
            OutError = TEXT("Activated tropical umbrella candidate component lost exact A/B/C render-only presentation state.");
            return false;
        }
        for (int32 LocalIndex = 0;
             LocalIndex < ExpectedByVariant[Variant].Num();
             ++LocalIndex)
        {
            FTransform Actual;
            if (!Component->GetInstanceTransform(
                    LocalIndex,
                    Actual,
                    true) ||
                !ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                    ExactSourceTransformBitsMatch(
                        Actual,
                        ExpectedByVariant[Variant][LocalIndex]))
            {
                OutError = TEXT("Activated tropical umbrella candidate transform is not the exact routed native source value.");
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary::
    InspectTropicalUmbrellaHeroReceipts(
        const FString& CandidateRoot,
        const FString& AcceptedCurrentReceiptPath,
        const FString& ExpectedAcceptedCurrentReceiptSha256,
        const FString& FutureAuthorizationReceiptPath,
        const FString& ExpectedFutureAuthorizationReceiptSha256,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            CandidateRoot,
            AcceptedCurrentReceiptPath,
            ExpectedAcceptedCurrentReceiptSha256,
            FutureAuthorizationReceiptPath,
            ExpectedFutureAuthorizationReceiptSha256,
            false,
            Admission,
            Error))
    {
        OutReport =
            TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_RECEIPT_INSPECTION_DENIED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_RECEIPTS_INSPECTED_ONLY exactCandidateContractAndNineObjsPinned=true exactMtlPinned=true treeRealismAndGv4SelectorPinned=true sixSourceIdentityManifestPinned=true callerHashesNeverAuthorizeExecution=true privateMaterializerInvoked=false assetsWritten=false mapModified=false sourceVisibilityModified=false sourceTransformsModified=false collisionNavigationLosRfSensorTerrainAuthorityModified=false selectionCompiled=false activationCompiled=false UnrealLaunched=false nativeCompileImportRuntimeLodWindOpacityCapturePerformanceOrHumanAcceptanceClaimed=false");
    return true;
}

bool UTRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary::
    MaterializeAndStageTrustedTropicalUmbrellaHeroInternal(
        const FString& CandidateRoot,
        const FString& AcceptedCurrentReceiptPath,
        const FString& ExpectedAcceptedCurrentReceiptSha256,
        const FString& FutureAuthorizationReceiptPath,
        const FString& ExpectedFutureAuthorizationReceiptSha256,
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor* CandidateActor,
        UHierarchicalInstancedStaticMeshComponent*
            ExistingV4HeritageUmbrellaComponent,
        UHierarchicalInstancedStaticMeshComponent*
            ExistingR29LandmarkUmbrellaComponent,
        const TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>&
            ExactOrderedNativeAnchors,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            CandidateRoot,
            AcceptedCurrentReceiptPath,
            ExpectedAcceptedCurrentReceiptSha256,
            FutureAuthorizationReceiptPath,
            ExpectedFutureAuthorizationReceiptSha256,
            true,
            Admission,
            Error))
    {
        OutReport =
            TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_MATERIALIZE_SWAP_DENIED: ") +
            Error;
        return false;
    }

    UWorld* World = nullptr;
    if (!ValidateExactTargetPersistentEditorWorld(
            CandidateActor,
            World,
            Error))
    {
        OutReport =
            TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_EXACT_TARGET_MAP_DENIED: ") +
            Error;
        return false;
    }

    const TArray<FString> EmptyExpected;
    if (!NamespaceMatchesExactAssets(
            OutputRoot, EmptyExpected, true, false, Error) ||
        !NamespaceMatchesExactAssets(
            StagingRoot, EmptyExpected, true, false, Error))
    {
        OutReport =
            TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_FRESH_NAMESPACE_REQUIRED: ") +
            Error;
        return false;
    }
    FFreshNamespaceBaseline Baseline;
    Baseline.bOutputProvenEmptyAtEntry = true;
    Baseline.bStagingProvenEmptyAtEntry = true;

    FString DormantReport;
    FExactSourceComponentSnapshot V4Heritage;
    FExactSourceComponentSnapshot R29Landmark;
    if (!CandidateActor->ValidateDormantScaffold(DormantReport) ||
        !CaptureExactSourceComponent(
            ExistingV4HeritageUmbrellaComponent,
            EExactSourceOwnerKind::V4Landscape,
            FName(TEXT("V4HeritageUmbrellaSilhouetteProxies")),
            4,
            World,
            V4Heritage,
            Error) ||
        !CaptureExactSourceComponent(
            ExistingR29LandmarkUmbrellaComponent,
            EExactSourceOwnerKind::V5DLandmarkVegetation,
            FName(TEXT("V5DLandmarkTreesUmbrella")),
            2,
            World,
            R29Landmark,
            Error))
    {
        OutReport =
            TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_EXACT_SOURCE_SNAPSHOT_DENIED: ") +
            (Error.IsEmpty() ? DormantReport : Error);
        return false;
    }
    TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>
        NativeAnchors;
    FString BeforeTransformBitHash;
    if (!BuildExactNativeAnchorCopy(
            ExactOrderedNativeAnchors,
            V4Heritage,
            R29Landmark,
            NativeAnchors,
            BeforeTransformBitHash,
            Error))
    {
        OutReport =
            TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_NATIVE_TRANSFORM_COPY_DENIED: ") +
            Error;
        return false;
    }

    const auto FailAndRollback = [&](const TCHAR* Stage) -> bool
    {
        const FString PrimaryError = Error;
        CandidateActor->RollbackPresentationToMandatoryFallbackInternal();
        FString SourceRestoreError;
        const bool bSourcesRestored = RestoreExactSourcePresentation(
            V4Heritage,
            R29Landmark,
            SourceRestoreError);
        FString RollbackError = PrimaryError;
        const bool bNamespacesRolledBack =
            RollbackFreshNamespaces(Baseline, RollbackError);
        OutReport = FString::Printf(
            TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_%s_FAILED primary='%s' sourcePresentationRestored=%s sourceRestore='%s' namespacesRolledBack=%s rollback='%s'"),
            Stage,
            *PrimaryError,
            bSourcesRestored ? TEXT("true") : TEXT("false"),
            *SourceRestoreError,
            bNamespacesRolledBack ? TEXT("true") : TEXT("false"),
            *RollbackError);
        return false;
    };

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
            .Get();
    TArray<UObject*> AssetsToSave;
    TArray<UMaterialInstanceConstant*> Materials;
    TArray<UStaticMesh*> StagingMeshes;
    TArray<UStaticMesh*> CandidateMeshes;
    if (!CreateCandidateMaterials(
            AssetTools,
            Admission,
            Materials,
            AssetsToSave,
            Error) ||
        !ImportStagingLods(
            AssetTools,
            Admission,
            StagingMeshes,
            Error) ||
        !CreateFinalMeshes(
            Admission,
            StagingMeshes,
            Materials,
            CandidateMeshes,
            AssetsToSave,
            Error))
    {
        return FailAndRollback(TEXT("CREATE_BEFORE_SAVE"));
    }
    TArray<FString> ExpectedOutputPaths;
    TArray<FString> ExpectedStagingPaths;
    GetExpectedOutputObjectPaths(ExpectedOutputPaths);
    GetExpectedStagingObjectPaths(ExpectedStagingPaths);
    TSet<FString> UniqueAssetsToSave;
    for (UObject* Asset : AssetsToSave)
    {
        UniqueAssetsToSave.Add(Asset ? Asset->GetPathName() : FString());
    }
    FTRIADIstanaExploreV5DTropicalUmbrellaAssets RuntimeAssets;
    if (AssetsToSave.Num() != OutputAssetCount ||
        UniqueAssetsToSave.Num() != OutputAssetCount ||
        UniqueAssetsToSave.Contains(FString()) ||
        !NamespaceMatchesExactAssets(
            OutputRoot,
            ExpectedOutputPaths,
            true,
            false,
            Error) ||
        !NamespaceMatchesExactAssets(
            StagingRoot,
            ExpectedStagingPaths,
            true,
            false,
            Error) ||
        !LoadAndValidateOutputs(Admission, RuntimeAssets, Error) ||
        !ValidateExactSourceComponentUnchanged(
            V4Heritage,
            false,
            Error) ||
        !ValidateExactSourceComponentUnchanged(
            R29Landmark,
            false,
            Error))
    {
        return FailAndRollback(TEXT("VALIDATE_BEFORE_SAVE"));
    }
    if (!DeleteFreshNamespaceContents(StagingRoot, Error) ||
        !NamespaceMatchesExactAssets(
            StagingRoot,
            EmptyExpected,
            true,
            false,
            Error) ||
        !NamespaceMatchesExactAssets(
            OutputRoot,
            ExpectedOutputPaths,
            true,
            false,
            Error))
    {
        return FailAndRollback(TEXT("STAGING_CLEANUP_BEFORE_SAVE"));
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem ||
        !AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        Error = TEXT("Exact six candidate output assets failed to save.");
        return FailAndRollback(TEXT("SAVE"));
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!LoadAndValidateOutputs(Admission, RuntimeAssets, Error) ||
        !NamespaceMatchesExactAssets(
            OutputRoot,
            ExpectedOutputPaths,
            true,
            true,
            Error) ||
        !NamespaceMatchesExactAssets(
            StagingRoot,
            EmptyExpected,
            true,
            false,
            Error))
    {
        return FailAndRollback(TEXT("POST_SAVE_VALIDATE"));
    }
    if (!CandidateActor->ConfigureCandidateSelectionInternal(
            RuntimeAssets,
            NativeAnchors,
            Admission.AcceptedCurrentSha256,
            Admission.FutureAuthorizationSha256,
            Error))
    {
        return FailAndRollback(TEXT("RUNTIME_SELECTION"));
    }
    if (!SuppressExactSourcePresentation(V4Heritage, R29Landmark, Error))
    {
        return FailAndRollback(TEXT("SOURCE_SUPPRESSION"));
    }
    if (!CandidateActor->ActivateAfterAtomicExactSourceSuppressionInternal(
            Admission.AcceptedCurrentSha256,
            Admission.FutureAuthorizationSha256,
            true,
            Error))
    {
        return FailAndRollback(TEXT("RUNTIME_ACTIVATION"));
    }

    TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor>
        AfterNativeAnchors;
    FString AfterTransformBitHash;
    FExactSourceComponentSnapshot V4After = V4Heritage;
    FExactSourceComponentSnapshot R29After = R29Landmark;
    V4After.WorldTransforms.Reset();
    R29After.WorldTransforms.Reset();
    for (int32 LocalIndex = 0; LocalIndex < 4; ++LocalIndex)
    {
        FTransform Transform;
        if (!V4Heritage.Component->GetInstanceTransform(
                LocalIndex,
                Transform,
                true))
        {
            Error = TEXT("Could not reread the V4 source transform after swap.");
            return FailAndRollback(TEXT("FINAL_TRANSFORM_PROOF"));
        }
        V4After.WorldTransforms.Add(Transform);
    }
    for (int32 LocalIndex = 0; LocalIndex < 2; ++LocalIndex)
    {
        FTransform Transform;
        if (!R29Landmark.Component->GetInstanceTransform(
                LocalIndex,
                Transform,
                true))
        {
            Error = TEXT("Could not reread the R29 source transform after swap.");
            return FailAndRollback(TEXT("FINAL_TRANSFORM_PROOF"));
        }
        R29After.WorldTransforms.Add(Transform);
    }
    if (!BuildExactNativeAnchorCopy(
            NativeAnchors,
            V4After,
            R29After,
            AfterNativeAnchors,
            AfterTransformBitHash,
            Error) ||
        BeforeTransformBitHash != AfterTransformBitHash ||
        !ValidateExactSourceComponentUnchanged(
            V4Heritage,
            true,
            Error) ||
        !ValidateExactSourceComponentUnchanged(
            R29Landmark,
            true,
            Error) ||
        !ValidateActivatedCandidatePresentation(
            CandidateActor,
            NativeAnchors,
            Error) ||
        !CandidateActor->bCandidateSelectionConfigured ||
        !CandidateActor->bPresentationActivated ||
        !CandidateActor->bExactSourcePresentationSuppressionProven ||
        !CandidateActor->bExistingTreeRealismMeshAndMaterialsFallbackPreserved ||
        CandidateActor->bSourceKeysTransformsOrCensusModified ||
        CandidateActor->
            bGeospatialCollisionNavigationLosRfSensorOrTerrainAuthority)
    {
        if (Error.IsEmpty())
        {
            Error = TEXT("Final exact transform/presentation/authority truth proof failed.");
        }
        return FailAndRollback(TEXT("FINAL_ATOMIC_PROOF"));
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_MATERIALIZED_AND_TRANSIENTLY_SWAPPED exactSourceInstances=6 sourceDistribution=V4Heritage4_R29Landmark2 candidateDistribution=A1_B4_C1 trueLodsPerVariant=3 outputAssets=6 stagingAssetsRemaining=0 bitExactBeforeAfterTransformSha256=%s sourceInstancesAddedRemovedOrMoved=false sourcePresentationSuppressed=true candidatePresentationActivated=true exactTreeRealismFallbackPreserved=true LeafDryExactOverrides=6scalar_3vector LeafLiveOverrides=0 inheritedClosedLeafletOpacityMask=0.333 inheritedWindParameters=6 nativeWindOpacityLodTransitionPerformanceFixedViewAndHumanAcceptanceStillRequired=true mapSaved=false geospatialCollisionNavigationLosRfSensorTerrainAuthorityModified=false"),
        *BeforeTransformBitHash);
    return true;
}
