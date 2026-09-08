#include "TRIADIstanaExploreV5DBuildingOpticsContactCompositionEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHIFeatureLevel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Ssl.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary.h"
#include "TRIADIstanaExploreV5DR31BroadShellAssetFactory.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

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
constexpr int32 SlotCount = 17;
constexpr int32 OutputAssetCount = 18;
constexpr int32 ExpectedMasterExpressionCount = 42;
constexpr int32 ExpectedR31SurfaceInputCount = 23;
constexpr int32 ExpectedCandidateSurfaceInputCount = 26;
constexpr int32 ExpectedCandidateAdditionalOutputCount = 6;
constexpr int32 ExpectedCandidateSurfaceOutputCount = 7;
constexpr int64 MaximumInputBytes = 16ll * 1024ll * 1024ll;

const FString CompositionContractSha256(
    TEXT("B4D7A4555B98A5A6A8B6EFB20EB5F711F2CBB9666B625586C51849AF8F9AE8C1"));
const FString OpticsCandidateContractSha256(
    TEXT("3DDE345EADF05AAB69AEF4244220543F59E442FFA36571B66DC18D493246B3EE"));
const FString OpticsMaterialIntentSha256(
    TEXT("1F5639C8D85EDBBE47B5C9F93F76A43D4CA0E42CBDDB971A9694A2E8CDA3702E"));
const FString OpticsOfflineAuditSha256(
    TEXT("4161B0EB861CABFD8D23AC3980023D4DEC68995DC71C789C4EC9142276081AED"));
const FString FootContactCandidateContractSha256(
    TEXT("81BB2A8F81A5C0DEF753898255335C5DAF57209704197297AECCA927DAF7B63E"));
const FString FootContactMaterialIntentSha256(
    TEXT("C396D13A2B1509868DA2EC528354043038AA2631B2299BFD896AD682008FEB77"));
const FString FootContactGeneratedAuditSha256(
    TEXT("0D94CBEE0FFDF62B1C588E621D1183AAE6EB3A6E763131BC94E2287F7C4A4D1C"));
const FString OpticsIntegrationContractSha256(
    TEXT("69094A6C6C3B7B69B98A52B1755EA5B1A973A6BDAE286B73D8719AD2F9AF9476"));
const FString FootContactIntegrationContractSha256(
    TEXT("A0E56C626243F4A38B262D4C6B0CD00BA293B82B54C16FBD3DDAED76FEC67AFC"));
const FString R31ContractSha256(
    TEXT("5ED127F1072A127A9ECEE51D1BD9496CE05E43E57FCB90F92A560840D6E97F27"));

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
    TEXT("triad.istana_explore_v5d.r33_cesium_world_terrain.native_transaction.v1"));
const FString FutureAuthorizationSchema(
    TEXT("triad.istana_public_view_explore_v5d.building_optics_contact_composition.future_transaction_authority.v1"));
const FString FutureOperation(
    TEXT("MATERIALIZE_BUILDING_OPTICS_CONTACT_COMPOSITION_ASSETS_ONLY_AND_EXPOSE_OPTIONAL_SEVENTEEN_SLOT_PRESENTATION_ROSTER"));
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingOpticsContactComposition"));
const FString MaterialRoot(OutputRoot + TEXT("/Materials"));
const FString MasterName(TEXT("M_IPV5D_BuildingOpticsContact_Master"));
const FString MasterObjectPath(
    MaterialRoot + TEXT("/") + MasterName + TEXT(".") + MasterName);

const TCHAR* const SlotNames[] = {
    TEXT("MAT_BOTTOM_HIDDEN"), TEXT("MAT_COMMERCIAL_HINT"),
    TEXT("MAT_GENERIC_BUILDING_HINT"), TEXT("MAT_HEALTHCARE_HINT"),
    TEXT("MAT_HOTEL_HINT"), TEXT("MAT_INDUSTRIAL_HINT"),
    TEXT("MAT_RELIGIOUS_HINT"), TEXT("MAT_RESIDENTIAL_HINT"),
    TEXT("MAT_ROOF_COMMERCIAL_HINT"),
    TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"),
    TEXT("MAT_ROOF_HEALTHCARE_HINT"), TEXT("MAT_ROOF_HOTEL_HINT"),
    TEXT("MAT_ROOF_INDUSTRIAL_HINT"), TEXT("MAT_ROOF_RELIGIOUS_HINT"),
    TEXT("MAT_ROOF_RESIDENTIAL_HINT"), TEXT("MAT_ROOF_TRANSPORT_HINT"),
    TEXT("MAT_TRANSPORT_HINT")};

const TCHAR* const CandidateNames[] = {
    TEXT("MI_IPV5D_BOC_00_MAT_BOTTOM_HIDDEN"),
    TEXT("MI_IPV5D_BOC_01_MAT_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BOC_02_MAT_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BOC_03_MAT_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BOC_04_MAT_HOTEL_HINT"),
    TEXT("MI_IPV5D_BOC_05_MAT_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BOC_06_MAT_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BOC_07_MAT_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BOC_08_MAT_ROOF_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BOC_09_MAT_ROOF_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BOC_10_MAT_ROOF_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BOC_11_MAT_ROOF_HOTEL_HINT"),
    TEXT("MI_IPV5D_BOC_12_MAT_ROOF_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BOC_13_MAT_ROOF_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BOC_14_MAT_ROOF_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BOC_15_MAT_ROOF_TRANSPORT_HINT"),
    TEXT("MI_IPV5D_BOC_16_MAT_TRANSPORT_HINT")};

const TCHAR* const OpticsFallbackNames[] = {
    TEXT("MI_IPV5D_BSO_00_MAT_BOTTOM_HIDDEN"),
    TEXT("MI_IPV5D_BSO_01_MAT_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BSO_02_MAT_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BSO_03_MAT_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BSO_04_MAT_HOTEL_HINT"),
    TEXT("MI_IPV5D_BSO_05_MAT_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BSO_06_MAT_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BSO_07_MAT_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BSO_08_MAT_ROOF_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BSO_09_MAT_ROOF_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BSO_10_MAT_ROOF_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BSO_11_MAT_ROOF_HOTEL_HINT"),
    TEXT("MI_IPV5D_BSO_12_MAT_ROOF_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BSO_13_MAT_ROOF_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BSO_14_MAT_ROOF_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BSO_15_MAT_ROOF_TRANSPORT_HINT"),
    TEXT("MI_IPV5D_BSO_16_MAT_TRANSPORT_HINT")};
const FString OpticsMaterialRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingSurfaceOpticsIntegration/Materials"));
const FString OpticsMasterObjectPath(
    OpticsMaterialRoot +
    TEXT("/M_IPV5D_BuildingSurfaceOptics_Master.M_IPV5D_BuildingSurfaceOptics_Master"));

const TCHAR* const R31OfficialWall =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialWall.MI_IPV5D_R31_OfficialWall");
const TCHAR* const R31OfficialRoof =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialRoof.MI_IPV5D_R31_OfficialRoof");
const TCHAR* const R31FallbackWall =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall");
const TCHAR* const R31FallbackRoof =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof");
const TCHAR* const R31FallbackPaths[] = {
    R31FallbackRoof, R31FallbackWall, R31FallbackWall, R31FallbackWall,
    R31OfficialWall, R31FallbackWall, R31OfficialWall, R31FallbackWall,
    R31FallbackRoof, R31FallbackRoof, R31FallbackRoof, R31OfficialRoof,
    R31FallbackRoof, R31OfficialRoof, R31FallbackRoof, R31FallbackRoof,
    R31FallbackWall};

const TCHAR* const R31SurfaceInputNames[] = {
    TEXT("BaseSurface"), TEXT("PackedOrm"), TEXT("UV0"),
    TEXT("WorldPositionCm"), TEXT("CameraPositionCm"),
    TEXT("SurfaceRoughness"), TEXT("RoughnessTextureWeight"),
    TEXT("VariationCellMeters"), TEXT("WeatheringStrength"),
    TEXT("WallVerticalWeatherMask"), TEXT("BayMeters"),
    TEXT("StoreyMeters"), TEXT("ApertureWidthFraction"),
    TEXT("ApertureHeightFraction"), TEXT("ApertureSillFraction"),
    TEXT("ApertureHintStrength"), TEXT("ApertureFadeStartCm"),
    TEXT("ApertureFadeEndCm"), TEXT("ApertureTint"),
    TEXT("AtmosphereStartCm"), TEXT("AtmosphereEndCm"),
    TEXT("AtmosphereStrength"), TEXT("AtmosphereTint")};

const TCHAR* const CandidateAdditionalOutputNames[] = {
    TEXT("CandidateTangentNormal"), TEXT("CandidateRoughness"),
    TEXT("CandidateMetallic"), TEXT("CandidateAmbientOcclusion"),
    TEXT("CandidateClearCoat"), TEXT("CandidateClearCoatRoughness")};
const TCHAR* const CandidateSurfaceOutputNames[] = {
    TEXT("return"), TEXT("CandidateTangentNormal"),
    TEXT("CandidateRoughness"), TEXT("CandidateMetallic"),
    TEXT("CandidateAmbientOcclusion"), TEXT("CandidateClearCoat"),
    TEXT("CandidateClearCoatRoughness")};
const ECustomMaterialOutputType CandidateAdditionalOutputTypes[] = {
    CMOT_Float3, CMOT_Float1, CMOT_Float1,
    CMOT_Float1, CMOT_Float1, CMOT_Float1};

static_assert(UE_ARRAY_COUNT(SlotNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(CandidateNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(OpticsFallbackNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(R31FallbackPaths) == SlotCount);
static_assert(
    UE_ARRAY_COUNT(R31SurfaceInputNames) == ExpectedR31SurfaceInputCount);
static_assert(
    UE_ARRAY_COUNT(CandidateAdditionalOutputNames) ==
        ExpectedCandidateAdditionalOutputCount);
static_assert(
    UE_ARRAY_COUNT(CandidateAdditionalOutputTypes) ==
        ExpectedCandidateAdditionalOutputCount);
static_assert(
    UE_ARRAY_COUNT(CandidateSurfaceOutputNames) ==
        ExpectedCandidateSurfaceOutputCount);
static_assert(
    ExpectedCandidateSurfaceOutputCount ==
        ExpectedCandidateAdditionalOutputCount + 1);

struct FSourcePin
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourcePin CompositionSourcePins[] = {
    {TEXT("building_optics_contact_composition_candidate.contract.v1.json"), 7087,
     TEXT("B4D7A4555B98A5A6A8B6EFB20EB5F711F2CBB9666B625586C51849AF8F9AE8C1")}};

const FSourcePin OpticsSourcePins[] = {
    {TEXT("building_surface_optics_candidate.contract.json"), 8382,
     TEXT("3DDE345EADF05AAB69AEF4244220543F59E442FFA36571B66DC18D493246B3EE")},
    {TEXT("building_surface_optics.material_intent.hlsl"), 6639,
     TEXT("1F5639C8D85EDBBE47B5C9F93F76A43D4CA0E42CBDDB971A9694A2E8CDA3702E")},
    {TEXT("OfflineAudit/building_surface_optics_audit.json"), 11419,
     TEXT("4161B0EB861CABFD8D23AC3980023D4DEC68995DC71C789C4EC9142276081AED")},
    {TEXT("render_building_surface_optics_candidate.py"), 49876,
     TEXT("C2F19177D9479BC2F6C16ED1A5943BEC152F391A89FD966080D04BB29184359F")},
    {TEXT("OfflineAudit/building_surface_optics_comparison.png"), 1574985,
     TEXT("7B0690035277B433F34601FAE82A3ACAA70E97F15F8ABFC23007A773ACDD6C8D")}};

const FSourcePin FootContactSourcePins[] = {
    {TEXT("building_foot_contact_candidate.contract.v1.json"), 4823,
     TEXT("81BB2A8F81A5C0DEF753898255335C5DAF57209704197297AECCA927DAF7B63E")},
    {TEXT("building_foot_contact.material_intent.hlsl"), 1779,
     TEXT("C396D13A2B1509868DA2EC528354043038AA2631B2299BFD896AD682008FEB77")},
    {TEXT("Generated/building_foot_contact_source_audit.v1.json"), 6462,
     TEXT("0D94CBEE0FFDF62B1C588E621D1183AAE6EB3A6E763131BC94E2287F7C4A4D1C")},
    {TEXT("build_building_foot_contact_candidate.py"), 17327,
     TEXT("5C2F1EE8DB84AF42146E780D4934A51288B1CD186EE161EDFA9281619E1D5BA2")}};

struct FAdmission
{
    FString CompositionCandidateRoot;
    FString OpticsCandidateRoot;
    FString FootContactCandidateRoot;
    FString AcceptedR33Sha256;
    FString FutureAuthorizationSha256;
    FString OpticsMaterialIntentText;
    FString FootContactMaterialIntentText;
};

FString BytesToHex(const uint8* Bytes, int32 Count)
{
    FString Result;
    Result.Reserve(Count * 2);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Result += FString::Printf(TEXT("%02X"), Bytes[Index]);
    }
    return Result;
}

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
        ExpectedBytes < 2 || ExpectedBytes > MaximumInputBytes ||
        !IsSha256(ExpectedSha256))
    {
        OutError = TEXT("Building-surface source admission requires an absolute path, bounded byte count, and explicit SHA-256 pin.");
        return false;
    }
    const int64 ActualBytes = IFileManager::Get().FileSize(*AbsolutePath);
    if (ActualBytes != ExpectedBytes ||
        !FFileHelper::LoadFileToArray(OutBytes, *AbsolutePath) ||
        OutBytes.Num() != ActualBytes)
    {
        OutError = TEXT("A building-optics/contact composition source file is absent or its exact byte count drifted.");
        return false;
    }
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (!SHA256(
            OutBytes.GetData(),
            static_cast<size_t>(OutBytes.Num()),
            Digest) ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper() !=
            ExpectedSha256.ToUpper())
    {
        OutError = TEXT("A building-optics/contact composition source file failed its immutable SHA-256 pin.");
        return false;
    }
#else
    OutError = TEXT("Building-surface source admission requires WITH_SSL SHA-256 support.");
    return false;
#endif
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
            AbsolutePath, ExpectedBytes, ExpectedSha256, Bytes, OutError))
    {
        return false;
    }
    FString Text;
    FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (Text.IsEmpty() || !FJsonSerializer::Deserialize(Reader, OutRoot) ||
        !OutRoot.IsValid())
    {
        OutError = TEXT("A hash-pinned building-optics/contact composition JSON document could not be parsed.");
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
    int32 ExpectedCount)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() ||
        !Object->TryGetArrayField(Field, Values) || !Values ||
        Values->Num() != ExpectedCount)
    {
        return false;
    }
    for (int32 Index = 0; Index < ExpectedCount; ++Index)
    {
        FString Actual;
        if (!(*Values)[Index].IsValid() ||
            !(*Values)[Index]->TryGetString(Actual) ||
            Actual != Expected[Index])
        {
            return false;
        }
    }
    return true;
}

bool ValidateCompositionCandidateContract(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Channels = nullptr;
    const TSharedPtr<FJsonObject>* CustomSurface = nullptr;
    const TSharedPtr<FJsonObject>* Contact = nullptr;
    const TSharedPtr<FJsonObject>* Geometry = nullptr;
    const TSharedPtr<FJsonObject>* Fallbacks = nullptr;
    const TSharedPtr<FJsonObject>* Authority = nullptr;
    const TSharedPtr<FJsonObject>* NativeClaims = nullptr;
    const TCHAR* const CompositionOrder[] = {
        TEXT("EXACT_R31_BROAD_SHELL_PRESENTATION"),
        TEXT("BUILDING_SURFACE_OPTICS"),
        TEXT("BOUNDED_BUILDING_FOOT_CONTACT")};
    const TCHAR* const InputChannels[] = {
        TEXT("CandidateBaseColor"), TEXT("CandidateTangentNormal"),
        TEXT("CandidateRoughness"), TEXT("CandidateMetallic"),
        TEXT("CandidateAmbientOcclusion"), TEXT("CandidateClearCoat"),
        TEXT("CandidateClearCoatRoughness")};
    const TCHAR* const AdjustedChannels[] = {
        TEXT("CandidateBaseColor"), TEXT("CandidateRoughness"),
        TEXT("CandidateAmbientOcclusion")};
    const TCHAR* const PassThroughChannels[] = {
        TEXT("CandidateTangentNormal"), TEXT("CandidateMetallic"),
        TEXT("CandidateClearCoat"),
        TEXT("CandidateClearCoatRoughness")};
    const TCHAR* const ExtraInputNames[] = {
        TEXT("R31TangentNormalInput"), TEXT("R31MetallicInput"),
        TEXT("R31AmbientOcclusionInput")};
    const TCHAR* const FalseGeometryFields[] = {
        TEXT("meshAssetCreatedDuplicatedOrModified"),
        TEXT("meshTopologyOrIndexMutation"), TEXT("vertexOrUvMutation"),
        TEXT("slotRosterOrOrderMutation"),
        TEXT("footprintHeightSilhouetteOrMassingMutation"),
        TEXT("buildingAdditionOrRemoval")};
    const TCHAR* const FalseAuthorityFields[] = {
        TEXT("mapOrComponentBinding"), TEXT("geography"), TEXT("cesium"),
        TEXT("terrain"), TEXT("collision"), TEXT("navigation"),
        TEXT("lineOfSight"), TEXT("rf"), TEXT("sensor"),
        TEXT("simulation"), TEXT("physicalMaterial"),
        TEXT("physicalMaterialMask"),
        TEXT("privacyOrSecurity")};
    const TCHAR* const FalseNativeFields[] = {
        TEXT("unrealLaunchedOrBuilt"), TEXT("compiledOrLinked"),
        TEXT("materialShaderCompiled"), TEXT("assetsCreatedOrSaved"),
        TEXT("coldReloaded"), TEXT("mapOrComponentModified"),
        TEXT("runtimeCaptured"), TEXT("performanceAccepted"),
        TEXT("humanVisuallyAccepted")};

    bool bValid =
        ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.istana_public_view_explore_v5d.building_optics_contact_composition_candidate.v1")) &&
        ExactString(
            Root,
            TEXT("status"),
            TEXT("POST_R33_UNNUMBERED_UNADMITTED_SOURCE_ONLY_CANDIDATE")) &&
        ExactBool(Root, TEXT("sourceOnly"), true) &&
        ExactStringArray(
            Root,
            TEXT("compositionOrder"),
            CompositionOrder,
            UE_ARRAY_COUNT(CompositionOrder)) &&
        Root->TryGetObjectField(TEXT("exactOpticsChannelPreservation"), Channels) &&
        Channels && Channels->IsValid() &&
        ExactStringArray(
            *Channels,
            TEXT("inputChannels"),
            InputChannels,
            UE_ARRAY_COUNT(InputChannels)) &&
        ExactStringArray(
            *Channels,
            TEXT("contactAdjustedChannels"),
            AdjustedChannels,
            UE_ARRAY_COUNT(AdjustedChannels)) &&
        ExactStringArray(
            *Channels,
            TEXT("bitExactPassThroughChannels"),
            PassThroughChannels,
            UE_ARRAY_COUNT(PassThroughChannels)) &&
        ExactBool(*Channels, TEXT("clearCoatShadingModelPreserved"), true) &&
        ExactBool(
            *Channels,
            TEXT("allExistingOpticsPresentationChannelsRemainConnected"),
            true) &&
        ExactBool(
            *Channels,
            TEXT("opticsMasksPaneIdentityMetricCoordinatesAndSignalsReused"),
            true) &&
        ExactBool(
            *Channels,
            TEXT("independentWindowLayoutOrFacadeClassificationIntroduced"),
            false) &&
        Root->TryGetObjectField(
            TEXT("customSurfaceInvariant"), CustomSurface) &&
        CustomSurface && CustomSurface->IsValid() &&
        ExactInteger(
            *CustomSurface, TEXT("exactR31InheritedInputCount"),
            ExpectedR31SurfaceInputCount) &&
        ExactInteger(
            *CustomSurface, TEXT("composedInputCount"),
            ExpectedCandidateSurfaceInputCount) &&
        ExactStringArray(
            *CustomSurface,
            TEXT("extraInputNames"),
            ExtraInputNames,
            UE_ARRAY_COUNT(ExtraInputNames)) &&
        ExactString(
            *CustomSurface,
            TEXT("extraInputExpressionInputName"),
            TEXT("None")) &&
        ExactBool(
            *CustomSurface,
            TEXT("extraInputMasksMatchSelectedSourceOutputs"),
            true) &&
        ExactStringArray(
            *CustomSurface,
            TEXT("outputNames"),
            CandidateSurfaceOutputNames,
            UE_ARRAY_COUNT(CandidateSurfaceOutputNames)) &&
        ExactBool(
            *CustomSurface, TEXT("allOutputMasksZero"), true) &&
        ExactBool(
            *CustomSurface, TEXT("showOutputNameOnPin"), true) &&
        Root->TryGetObjectField(TEXT("boundedFootContactResponse"), Contact) &&
        Contact && Contact->IsValid() &&
        ExactString(*Contact, TEXT("applicationOrder"), TEXT("AFTER_OPTICS_OUTPUTS")) &&
        ExactNumber(*Contact, TEXT("fullWeightThroughSourceZMetres"), 0.1) &&
        ExactNumber(*Contact, TEXT("zeroWeightAtAndAboveSourceZMetres"), 1.25) &&
        ExactNumber(*Contact, TEXT("maximumBaseColorDarkening"), 0.08) &&
        ExactNumber(*Contact, TEXT("maximumRoughnessDelta"), 0.05) &&
        ExactNumber(*Contact, TEXT("maximumAmbientOcclusionDarkening"), 0.07) &&
        ExactBool(*Contact, TEXT("existingGlassMaskExcludesGlazing"), true) &&
        ExactBool(*Contact, TEXT("wallVerticalWeatherMaskRequired"), true) &&
        ExactBool(*Contact, TEXT("syntheticSourcePlaneIsMeasuredGrade"), false) &&
        ExactBool(
            *Contact,
            TEXT("sourcePlaneAlignmentMustBeVisuallyAcceptedBeforeActivation"),
            true) &&
        Root->TryGetObjectField(TEXT("geometryInvariant"), Geometry) &&
        Geometry && Geometry->IsValid() &&
        ExactInteger(*Geometry, TEXT("sourceTriangles"), 43448) &&
        ExactInteger(*Geometry, TEXT("materialSlots"), SlotCount) &&
        ExactInteger(*Geometry, TEXT("retainedGroups"), 1388) &&
        ExactString(*Geometry, TEXT("componentTransform"), TEXT("IDENTITY")) &&
        Root->TryGetObjectField(TEXT("fallbackBoundary"), Fallbacks) &&
        Fallbacks && Fallbacks->IsValid() &&
        ExactBool(*Fallbacks, TEXT("exactOpticsRosterRetained"), true) &&
        ExactBool(*Fallbacks, TEXT("exactR31RosterRetained"), true) &&
        ExactBool(*Fallbacks, TEXT("opticsPreferredFallback"), true) &&
        ExactBool(*Fallbacks, TEXT("r31UltimateFallback"), true) &&
        ExactBool(*Fallbacks, TEXT("fallbackMutation"), false) &&
        Root->TryGetObjectField(TEXT("authorityBoundary"), Authority) &&
        Authority && Authority->IsValid() &&
        Root->TryGetObjectField(TEXT("nativeClaims"), NativeClaims) &&
        NativeClaims && NativeClaims->IsValid();

    if (Geometry && Geometry->IsValid())
    {
        for (const TCHAR* Field : FalseGeometryFields)
        {
            bValid = bValid && ExactBool(*Geometry, Field, false);
        }
    }
    if (Authority && Authority->IsValid())
    {
        for (const TCHAR* Field : FalseAuthorityFields)
        {
            bValid = bValid && ExactBool(*Authority, Field, false);
        }
    }
    if (NativeClaims && NativeClaims->IsValid())
    {
        for (const TCHAR* Field : FalseNativeFields)
        {
            bValid = bValid && ExactBool(*NativeClaims, Field, false);
        }
    }
    if (!bValid)
    {
        OutError = TEXT("The composition contract lost its exact optics-first, bounded-contact, immutable-geometry, dual-fallback, or no-authority boundary.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateFootContactCandidateContract(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Facts = nullptr;
    const TSharedPtr<FJsonObject>* Response = nullptr;
    const TSharedPtr<FJsonObject>* Preservation = nullptr;
    const TSharedPtr<FJsonObject>* Authority = nullptr;
    const TCHAR* const FalsePreservationFields[] = {
        TEXT("buildingFootprintMutation"), TEXT("buildingHeightMutation"),
        TEXT("buildingSilhouetteMutation"), TEXT("componentTransformMutation"),
        TEXT("meshIndexMutation"), TEXT("meshPositionMutation"),
        TEXT("meshTopologyMutation"), TEXT("opacityOrCoverageMutation"),
        TEXT("pixelDepthOffsetMutation"),
        TEXT("terrainHeightOrTransformMutation"), TEXT("uvMutation"),
        TEXT("worldPositionOffsetMutation")};
    const TCHAR* const FalseAuthorityFields[] = {
        TEXT("asBuiltOrCurrentSiteTruth"), TEXT("collision"),
        TEXT("geospatial"), TEXT("lineOfSight"), TEXT("navigation"),
        TEXT("physicalMaterial"), TEXT("rf"), TEXT("sensor"),
        TEXT("surveyOrVerticalDatum"), TEXT("terrain")};
    bool bValid =
        ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.istana.building_foot_contact_candidate.v1")) &&
        ExactString(
            Root,
            TEXT("status"),
            TEXT("POST_R33_UNNUMBERED_UNADMITTED_SOURCE_ONLY_CANDIDATE")) &&
        ExactBool(Root, TEXT("sourceOnly"), true) &&
        ExactBool(Root, TEXT("nativeProjectApplied"), false) &&
        ExactBool(Root, TEXT("newNumberedStageAuthorized"), false) &&
        ExactBool(Root, TEXT("targetMapMutated"), false) &&
        ExactBool(Root, TEXT("unrealLaunched"), false) &&
        Root->TryGetObjectField(TEXT("knownSourceFacts"), Facts) && Facts &&
        Facts->IsValid() &&
        ExactInteger(*Facts, TEXT("sourceTriangleCount"), 43448) &&
        ExactInteger(*Facts, TEXT("materialSlotCount"), SlotCount) &&
        ExactInteger(*Facts, TEXT("retainedGroupCount"), 1388) &&
        ExactBool(*Facts, TEXT("wallUvVEqualsSourceAbsoluteZExactly"), true) &&
        Root->TryGetObjectField(TEXT("materialResponse"), Response) && Response &&
        Response->IsValid() &&
        ExactNumber(*Response, TEXT("ambientOcclusionDarkeningMaximum"), 0.07) &&
        ExactNumber(*Response, TEXT("baseColorDarkeningMaximum"), 0.08) &&
        ExactNumber(*Response, TEXT("roughnessDeltaMaximum"), 0.05) &&
        ExactNumber(*Response, TEXT("fullWeightThroughSourceZMetres"), 0.1) &&
        ExactNumber(*Response, TEXT("zeroWeightAtAndAboveSourceZMetres"), 1.25) &&
        ExactBool(*Response, TEXT("existingGlassMaskExcludesGlazing"), true) &&
        ExactBool(*Response, TEXT("wallVerticalWeatherMaskRequired"), true) &&
        ExactBool(*Response, TEXT("worldXyBreakupIsContinuous"), true) &&
        ExactBool(*Response, TEXT("hardWorldGridOrBuildingSelector"), false) &&
        Root->TryGetObjectField(TEXT("preservationBoundary"), Preservation) &&
        Preservation && Preservation->IsValid() &&
        Root->TryGetObjectField(TEXT("authorityBoundary"), Authority) &&
        Authority && Authority->IsValid();
    if (Preservation && Preservation->IsValid())
    {
        for (const TCHAR* Field : FalsePreservationFields)
        {
            bValid = bValid && ExactBool(*Preservation, Field, false);
        }
    }
    if (Authority && Authority->IsValid())
    {
        for (const TCHAR* Field : FalseAuthorityFields)
        {
            bValid = bValid && ExactBool(*Authority, Field, false);
        }
    }
    if (!bValid)
    {
        OutError = TEXT("The foot-contact predecessor lost its exact bounded source-plane response or immutable/no-authority boundary.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateOpticsCandidateContract(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Claims = nullptr;
    const TSharedPtr<FJsonObject>* Geometry = nullptr;
    const TSharedPtr<FJsonObject>* Appearance = nullptr;
    const TSharedPtr<FJsonObject>* Glazing = nullptr;
    const TSharedPtr<FJsonObject>* Authority = nullptr;
    const TSharedPtr<FJsonObject>* Admission = nullptr;
    const TSharedPtr<FJsonObject>* Audit = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* RenderChannels = nullptr;
    const TCHAR* const FalseClaimFields[] = {
        TEXT("unrealAsset"), TEXT("nativeIntegrated"),
        TEXT("nativeCompiled"), TEXT("nativeCaptured"),
        TEXT("humanVisualAcceptance"), TEXT("hyperrealClaim"),
        TEXT("actualSiteMaterialTruth"), TEXT("surveyOrAsBuiltTruth"),
        TEXT("currentComplete")};
    const TCHAR* const FalseGeometryFields[] = {
        TEXT("meshPackageMutation"), TEXT("vertexMutation"),
        TEXT("topologyMutation"), TEXT("uvMutation"),
        TEXT("materialSlotRosterOrOrderMutation"),
        TEXT("footprintMutation"), TEXT("heightMutation"),
        TEXT("silhouetteMutation"), TEXT("componentTransformMutation"),
        TEXT("buildingAdditionOrRemoval")};
    const TCHAR* const FalseAuthorityFields[] = {
        TEXT("geography"), TEXT("cesium"), TEXT("terrain"),
        TEXT("collision"), TEXT("navigation"), TEXT("lineOfSight"),
        TEXT("simulation"), TEXT("sensor"), TEXT("rfMaterial"),
        TEXT("rfOcclusion"), TEXT("privacyOrSecurityInference")};

    bool bValid =
        ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.istana_explore_v5d.building_surface_optics_candidate.v1")) &&
        ExactString(Root, TEXT("status"), TEXT("SOURCE_ONLY_UNADMITTED")) &&
        Root->TryGetObjectField(TEXT("claimBoundary"), Claims) && Claims &&
        Claims->IsValid() &&
        ExactBool(*Claims, TEXT("sourceLookdevOnly"), true) &&
        Root->TryGetObjectField(TEXT("immutableGeometry"), Geometry) &&
        Geometry && Geometry->IsValid() &&
        ExactInteger(*Geometry, TEXT("sourceObjTriangles"), 43448) &&
        ExactInteger(*Geometry, TEXT("materialSlots"), SlotCount) &&
        ExactInteger(*Geometry, TEXT("canonicalSourceGroups"), 1391) &&
        ExactInteger(*Geometry, TEXT("retainedSuppressionGroups"), 1388) &&
        ExactString(*Geometry, TEXT("componentTransform"), TEXT("identity")) &&
        Root->TryGetObjectField(TEXT("appearanceScope"), Appearance) &&
        Appearance && Appearance->IsValid() &&
        ExactBool(*Appearance, TEXT("newWindowLayout"), false) &&
        ExactBool(*Appearance, TEXT("newFacadeClassification"), false) &&
        ExactBool(*Appearance, TEXT("newRoofBoundaryOrEquipment"), false) &&
        ExactBool(*Appearance, TEXT("newBalconyOrParapetGeometry"), false) &&
        ExactBool(*Appearance, TEXT("worldPositionOffset"), false) &&
        ExactBool(*Appearance, TEXT("pixelDepthOffset"), false) &&
        ExactBool(*Appearance, TEXT("opacityOrMaskMutation"), false) &&
        ExactBool(*Appearance, TEXT("emissive"), false) &&
        ExactBool(*Appearance, TEXT("transparentBlend"), false) &&
        ExactBool(*Appearance, TEXT("twoSided"), false) &&
        ExactNumber(*Appearance, TEXT("ordinaryDistanceFullResponseMetres"), 75.0) &&
        ExactNumber(*Appearance, TEXT("fineDetailFadeEndMetres"), 220.0) &&
        ExactNumber(*Appearance, TEXT("macroResponseFadeEndMetres"), 950.0) &&
        (*Appearance)->TryGetArrayField(TEXT("renderChannels"), RenderChannels) &&
        RenderChannels && RenderChannels->Num() == 7 &&
        Root->TryGetObjectField(TEXT("opticalResponse"), Glazing) &&
        Glazing && Glazing->IsValid() &&
        Root->TryGetObjectField(TEXT("authority"), Authority) && Authority &&
        Authority->IsValid() && ExactBool(*Authority, TEXT("renderOnly"), true) &&
        Root->TryGetObjectField(TEXT("futureAdmission"), Admission) &&
        Admission && Admission->IsValid() &&
        ExactBool(*Admission, TEXT("allowedNow"), false) &&
        ExactBool(*Admission, TEXT("acceptedR33ReceiptRequired"), true) &&
        ExactBool(*Admission, TEXT("acceptedR33HumanReviewRequired"), true) &&
        ExactBool(
            *Admission,
            TEXT("separateExplicitAuthorizationReceiptRequired"),
            true) &&
        ExactBool(*Admission, TEXT("assetsOnlyMaterializationRequired"), true) &&
        Root->TryGetObjectField(TEXT("offlineAudit"), Audit) && Audit &&
        Audit->IsValid() && ExactBool(*Audit, TEXT("sourceOnly"), true) &&
        ExactBool(*Audit, TEXT("usesExactBroadShellObj"), true) &&
        ExactBool(*Audit, TEXT("usesAll17SemanticSlots"), true) &&
        ExactBool(
            *Audit,
            TEXT("sameMeshCropCameraTexturesAndLight"),
            true) &&
        ExactBool(
            *Audit,
            TEXT("sourceOrOfflineAuditMayAuthorizeNativeAcceptance"),
            false);

    if (!Claims || !Claims->IsValid() || !Geometry || !Geometry->IsValid() ||
        !Authority || !Authority->IsValid())
    {
        OutError = TEXT("The building-optics/contact composition candidate contract omitted a required boundary object.");
        return false;
    }
    for (const TCHAR* Field : FalseClaimFields)
    {
        bValid = bValid && ExactBool(*Claims, Field, false);
    }
    for (const TCHAR* Field : FalseGeometryFields)
    {
        bValid = bValid && ExactBool(*Geometry, Field, false);
    }
    for (const TCHAR* Field : FalseAuthorityFields)
    {
        bValid = bValid && ExactBool(*Authority, Field, false);
    }

    const TSharedPtr<FJsonObject>* GlazingObject = nullptr;
    const bool bOpticsValid = Glazing && Glazing->IsValid() &&
        (*Glazing)->TryGetObjectField(TEXT("glazing"), GlazingObject) &&
        GlazingObject && GlazingObject->IsValid() &&
        ExactBool(*GlazingObject, TEXT("clearCoatShadingModelIntent"), true) &&
        ExactBool(
            *GlazingObject,
            TEXT("clearCoatMaskUsesExistingGlassOnly"),
            true) &&
        ExactBool(
            *GlazingObject,
            TEXT("paneIdentityConstantWithinExistingApertureCell"),
            true) &&
        ExactBool(
            *GlazingObject,
            TEXT("independentFixedFacadeGridUsed"),
            false) &&
        ExactBool(*GlazingObject, TEXT("opaqueBlackWindowAllowed"), false);

    if (!bValid || !bOpticsValid)
    {
        OutError = TEXT("The building-optics/contact composition candidate contract lost its exact source-only, geometry-preserving, R31-mask-derived, Clear Coat, or authority boundary.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateOpticsOfflineAudit(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Authority = nullptr;
    const TSharedPtr<FJsonObject>* Metrics = nullptr;
    const TSharedPtr<FJsonObject>* Geometry = nullptr;
    const TSharedPtr<FJsonObject>* PaneProbe = nullptr;
    double GradientGain = 0.0;
    double LuminanceGain = 0.0;
    double MeanDelta = 0.0;
    double ChangedFraction = 0.0;
    double WithinCellRange = -1.0;
    double AdjacentDelta = 0.0;
    bool bValid =
        ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.istana_explore_v5d.building_surface_optics_candidate.offline_audit.v1")) &&
        ExactString(Root, TEXT("status"), TEXT("PASS_SOURCE_LOOKDEV_ONLY")) &&
        ExactBool(Root, TEXT("sourceOnly"), true) &&
        ExactBool(Root, TEXT("nativeIntegrated"), false) &&
        ExactBool(Root, TEXT("visualAcceptance"), false) &&
        Root->TryGetObjectField(TEXT("authority"), Authority) && Authority &&
        Authority->IsValid() &&
        ExactBool(*Authority, TEXT("geometryOrSimulationAuthorityChanged"), false) &&
        ExactBool(*Authority, TEXT("mayAuthorizeNativeAcceptance"), false) &&
        ExactBool(*Authority, TEXT("nativeOrProviderClaimed"), false) &&
        Root->TryGetObjectField(TEXT("comparisonMetrics"), Metrics) && Metrics &&
        Metrics->IsValid() &&
        (*Metrics)->TryGetNumberField(TEXT("meanNeighbourGradientGainFraction"), GradientGain) &&
        (*Metrics)->TryGetNumberField(TEXT("shellLuminanceStandardDeviationGainFraction"), LuminanceGain) &&
        (*Metrics)->TryGetNumberField(TEXT("meanAbsoluteChannelDelta255"), MeanDelta) &&
        (*Metrics)->TryGetNumberField(TEXT("pixelFractionMaximumChannelDeltaAbove4"), ChangedFraction) &&
        GradientGain >= 0.10 && LuminanceGain >= 0.08 &&
        MeanDelta >= 0.65 && ChangedFraction >= 0.08 &&
        Root->TryGetObjectField(TEXT("geometryEvidence"), Geometry) &&
        Geometry && Geometry->IsValid() &&
        ExactInteger(*Geometry, TEXT("candidateGeometryFileCount"), 0) &&
        ExactInteger(*Geometry, TEXT("materialSlotCount"), SlotCount) &&
        ExactInteger(*Geometry, TEXT("sourceGroupCount"), 1388) &&
        ExactInteger(*Geometry, TEXT("sourceTriangleCount"), 43448) &&
        ExactString(*Geometry, TEXT("componentTransform"), TEXT("identity")) &&
        ExactBool(
            *Geometry,
            TEXT("meshFootprintHeightTransformSilhouetteMutation"),
            false) &&
        Root->TryGetObjectField(TEXT("paneIdentityProbe"), PaneProbe) &&
        PaneProbe && PaneProbe->IsValid() &&
        ExactBool(
            *PaneProbe,
            TEXT("fixedThreeByThreePointTwoMetreFacadeGridRemoved"),
            true) &&
        ExactBool(
            *PaneProbe,
            TEXT("noPaneIdentityDiscontinuityInsideExistingApertureCell"),
            true) &&
        ExactInteger(*PaneProbe, TEXT("qualifiedExistingApertureCellCount"), 126) &&
        ExactInteger(*PaneProbe, TEXT("adjacentCellPairCount"), 16) &&
        ExactInteger(
            *PaneProbe,
            TEXT("adjacentCellPairsWithDifferentSignal"),
            16) &&
        (*PaneProbe)->TryGetNumberField(
            TEXT("maximumWithinCellPaneSignalRange"), WithinCellRange) &&
        (*PaneProbe)->TryGetNumberField(
            TEXT("minimumAdjacentCellPaneSignalDelta"), AdjacentDelta) &&
        WithinCellRange == 0.0 && AdjacentDelta >= 0.02;
    if (!bValid)
    {
        OutError = TEXT("The building-optics/contact composition offline audit no longer proves its bounded source-lookdev geometry, contrast, or exact R31 pane-cell invariants.");
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
        OutError = TEXT("R33 receipt is not the exact human-accepted, no-mutation, eight-image receipt.");
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
        TEXT("CompositionContractSha256"),
        TEXT("OpticsCandidateContractSha256"),
        TEXT("OpticsMaterialIntentSha256"),
        TEXT("OpticsOfflineAuditSha256"),
        TEXT("OpticsIntegrationContractSha256"),
        TEXT("FootContactCandidateContractSha256"),
        TEXT("FootContactMaterialIntentSha256"),
        TEXT("FootContactGeneratedAuditSha256"),
        TEXT("FootContactIntegrationContractSha256"),
        TEXT("R31ContractSha256"), TEXT("CompositionOrder"),
        TEXT("ExactMaterialSlotCount"), TEXT("ExactOutputAssetCount"),
        TEXT("OutputNamespace"), TEXT("UnnumberedSuccessor"),
        TEXT("ExplicitExecutionAuthorized"), TEXT("AssetsOnlyEndpoint"),
        TEXT("OptionalPresentationMaterialSetOnly"),
        TEXT("ExactOpticsAndR31FallbacksPreserved"),
        TEXT("MapMutationAuthorized"),
        TEXT("SourceMeshMutationAuthorized"),
        TEXT("SourceMaterialMutationAuthorized"),
        TEXT("SourceTransformMutationAuthorized"),
        TEXT("GeographyTerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"),
        TEXT("NativeWriteOrUnrealLaunchPerformed")};
    if (!Root.IsValid() ||
        Root->Values.Num() != UE_ARRAY_COUNT(ExactFields))
    {
        OutError = TEXT("Future building-optics/contact composition authorization must contain exactly the narrow trusted field roster and no extensions.");
        return false;
    }
    for (const TCHAR* Field : ExactFields)
    {
        if (!Root->HasField(Field))
        {
            OutError = TEXT("Future building-optics/contact composition authorization omitted a field from the exact narrow roster.");
            return false;
        }
    }
    if (!ExactString(Root, TEXT("Schema"), FutureAuthorizationSchema) ||
        !ExactString(Root, TEXT("Status"), TEXT("AUTHORIZED_NOT_EXECUTED")) ||
        !ExactString(Root, TEXT("Operation"), FutureOperation) ||
        !ExactString(Root, TEXT("TargetMapPackage"), TargetMapPackage) ||
        !ExactString(
            Root,
            TEXT("AcceptedR33ReceiptSha256"),
            AcceptedR33Sha256.ToUpper()) ||
        !ExactString(
            Root, TEXT("CompositionContractSha256"), CompositionContractSha256) ||
        !ExactString(
            Root,
            TEXT("OpticsCandidateContractSha256"),
            OpticsCandidateContractSha256) ||
        !ExactString(
            Root, TEXT("OpticsMaterialIntentSha256"), OpticsMaterialIntentSha256) ||
        !ExactString(
            Root, TEXT("OpticsOfflineAuditSha256"), OpticsOfflineAuditSha256) ||
        !ExactString(
            Root,
            TEXT("OpticsIntegrationContractSha256"),
            OpticsIntegrationContractSha256) ||
        !ExactString(
            Root,
            TEXT("FootContactCandidateContractSha256"),
            FootContactCandidateContractSha256) ||
        !ExactString(
            Root,
            TEXT("FootContactMaterialIntentSha256"),
            FootContactMaterialIntentSha256) ||
        !ExactString(
            Root,
            TEXT("FootContactGeneratedAuditSha256"),
            FootContactGeneratedAuditSha256) ||
        !ExactString(
            Root,
            TEXT("FootContactIntegrationContractSha256"),
            FootContactIntegrationContractSha256) ||
        !ExactString(Root, TEXT("R31ContractSha256"), R31ContractSha256) ||
        !ExactString(
            Root,
            TEXT("CompositionOrder"),
            TEXT("EXACT_R31_THEN_BUILDING_SURFACE_OPTICS_THEN_BOUNDED_BUILDING_FOOT_CONTACT")) ||
        !ExactInteger(Root, TEXT("ExactMaterialSlotCount"), SlotCount) ||
        !ExactInteger(Root, TEXT("ExactOutputAssetCount"), OutputAssetCount) ||
        !ExactString(Root, TEXT("OutputNamespace"), OutputRoot) ||
        !ExactBool(Root, TEXT("UnnumberedSuccessor"), true) ||
        !ExactBool(Root, TEXT("ExplicitExecutionAuthorized"), true) ||
        !ExactBool(Root, TEXT("AssetsOnlyEndpoint"), true) ||
        !ExactBool(
            Root, TEXT("OptionalPresentationMaterialSetOnly"), true) ||
        !ExactBool(
            Root, TEXT("ExactOpticsAndR31FallbacksPreserved"), true) ||
        !ExactBool(Root, TEXT("MapMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceMeshMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceMaterialMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceTransformMutationAuthorized"), false) ||
        !ExactBool(
            Root,
            TEXT("GeographyTerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"),
            false) ||
        !ExactBool(
            Root, TEXT("NativeWriteOrUnrealLaunchPerformed"), false))
    {
        OutError = TEXT("Future building-optics/contact composition authorization is absent, unpinned, non-explicit, or broader than isolated optional material-set creation.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ExtractFunctionBody(
    const FString& MaterialIntentText,
    const FString& Signature,
    const TCHAR* const* RequiredTokens,
    int32 RequiredTokenCount,
    FString& OutBody,
    FString& OutError)
{
    const int32 SignatureIndex = MaterialIntentText.Find(
        Signature, ESearchCase::CaseSensitive);
    const int32 OpenBrace = SignatureIndex == INDEX_NONE
        ? INDEX_NONE
        : MaterialIntentText.Find(
              TEXT("{"),
              ESearchCase::CaseSensitive,
              ESearchDir::FromStart,
              SignatureIndex + Signature.Len());
    const int32 CloseBrace = MaterialIntentText.Find(
        TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (SignatureIndex == INDEX_NONE || OpenBrace == INDEX_NONE ||
        CloseBrace <= OpenBrace ||
        !MaterialIntentText.Mid(CloseBrace + 1).TrimStartAndEnd().IsEmpty())
    {
        OutError = TEXT("The hash-pinned candidate HLSL has no single extractable function body.");
        return false;
    }
    OutBody = MaterialIntentText.Mid(
        OpenBrace + 1, CloseBrace - OpenBrace - 1);
    for (int32 Index = 0; Index < RequiredTokenCount; ++Index)
    {
        if (!OutBody.Contains(RequiredTokens[Index], ESearchCase::CaseSensitive))
        {
            OutError = TEXT("A hash-pinned composition input lost a required bounded presentation token.");
            return false;
        }
    }
    if (OutBody.Contains(TEXT("WorldPositionOffset")) ||
        OutBody.Contains(TEXT("PixelDepthOffset")) ||
        OutBody.Contains(TEXT("Opacity")))
    {
        OutError = TEXT("The candidate HLSL unexpectedly requests geometry, depth, or opacity authority.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ExtractOpticsFunctionBody(
    const FString& MaterialIntentText,
    FString& OutBody,
    FString& OutError)
{
    const TCHAR* const RequiredTokens[] = {
        TEXT("R31PaneCellIndex"),
        TEXT("const float PaneSignal = frac(sin(dot("),
        TEXT("CandidateClearCoat ="),
        TEXT("CandidateClearCoatRoughness ="),
        TEXT("CandidateTangentNormal ="),
        TEXT("CandidateMetallic ="),
        TEXT("CandidateAmbientOcclusion ="),
        TEXT("const float WallSurfaceMask"),
        TEXT("const float RoofMicro")};
    return ExtractFunctionBody(
        MaterialIntentText,
        TEXT("void TRIADBuildingSurfaceOpticsCandidate("),
        RequiredTokens,
        UE_ARRAY_COUNT(RequiredTokens),
        OutBody,
        OutError);
}

bool ExtractFootContactFunctionBody(
    const FString& MaterialIntentText,
    FString& OutBody,
    FString& OutError)
{
    const TCHAR* const RequiredTokens[] = {
        TEXT("const float SourcePlaneEnvelope = 1.0 - smoothstep("),
        TEXT("0.10,"), TEXT("1.25,"), TEXT("const float BroadSignal"),
        TEXT("const float OpaqueWallMask"),
        TEXT("ContactBaseColor = ExistingBaseColor * lerp(1.0, 0.92"),
        TEXT("ContactRoughness = saturate(ExistingRoughness + 0.05"),
        TEXT("ExistingAmbientOcclusion * (1.0 - 0.07")};
    return ExtractFunctionBody(
        MaterialIntentText,
        TEXT("void TRIADBuildingFootContactResponse("),
        RequiredTokens,
        UE_ARRAY_COUNT(RequiredTokens),
        OutBody,
        OutError);
}

bool BuildAdmission(
    const FString& CompositionCandidateRoot,
    const FString& OpticsCandidateRoot,
    const FString& FootContactCandidateRoot,
    const FString& AcceptedR33ReceiptPath,
    const FString& ExpectedAcceptedR33ReceiptSha256,
    const FString& FutureTransactionAuthorizationPath,
    const FString& ExpectedFutureTransactionAuthorizationSha256,
    bool bRequireCompiledTrustAnchors,
    FAdmission& OutAdmission,
    FString& OutError)
{
    OutAdmission = FAdmission{};
    if (CompositionCandidateRoot.IsEmpty() ||
        FPaths::IsRelative(CompositionCandidateRoot) ||
        OpticsCandidateRoot.IsEmpty() ||
        FPaths::IsRelative(OpticsCandidateRoot) ||
        FootContactCandidateRoot.IsEmpty() ||
        FPaths::IsRelative(FootContactCandidateRoot) ||
        AcceptedR33ReceiptPath.IsEmpty() ||
        FPaths::IsRelative(AcceptedR33ReceiptPath) ||
        FutureTransactionAuthorizationPath.IsEmpty() ||
        FPaths::IsRelative(FutureTransactionAuthorizationPath) ||
        !IsSha256(ExpectedAcceptedR33ReceiptSha256) ||
        !IsSha256(ExpectedFutureTransactionAuthorizationSha256))
    {
        OutError = TEXT("Composition admission requires three absolute candidate roots, two absolute receipt paths, and two explicit SHA-256 receipt pins.");
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
            OutError = TEXT("Composition execution is intentionally unreachable: distinct compiled trusted receipt anchors are unset and require a separately reviewed source change plus recompile.");
            return false;
        }
        if (!ExpectedAcceptedR33ReceiptSha256.Equals(
                TrustedAcceptedR33ReceiptSha256,
                ESearchCase::IgnoreCase) ||
            !ExpectedFutureTransactionAuthorizationSha256.Equals(
                TrustedFutureAuthorizationSha256,
                ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Caller-supplied building-optics/contact composition receipt hashes do not match the separately compiled trusted anchors.");
            return false;
        }
    }
    FString FullCompositionRoot =
        FPaths::ConvertRelativePathToFull(CompositionCandidateRoot);
    FString FullOpticsRoot =
        FPaths::ConvertRelativePathToFull(OpticsCandidateRoot);
    FString FullFootContactRoot =
        FPaths::ConvertRelativePathToFull(FootContactCandidateRoot);
    FString FullR33 = FPaths::ConvertRelativePathToFull(
        AcceptedR33ReceiptPath);
    FString FullAuthorization = FPaths::ConvertRelativePathToFull(
        FutureTransactionAuthorizationPath);
    FPaths::NormalizeDirectoryName(FullCompositionRoot);
    FPaths::NormalizeDirectoryName(FullOpticsRoot);
    FPaths::NormalizeDirectoryName(FullFootContactRoot);
    FPaths::NormalizeFilename(FullR33);
    FPaths::NormalizeFilename(FullAuthorization);
    if (FPaths::GetCleanFilename(FullCompositionRoot) !=
            TEXT("BuildingOpticsContactCompositionCandidate") ||
        FPaths::GetCleanFilename(FullOpticsRoot) !=
            TEXT("BuildingSurfaceOpticsCandidate") ||
        FPaths::GetCleanFilename(FullFootContactRoot) !=
            TEXT("BuildingFootContactCandidate") ||
        FPaths::IsSamePath(FullCompositionRoot, FullOpticsRoot) ||
        FPaths::IsSamePath(FullCompositionRoot, FullFootContactRoot) ||
        FPaths::IsSamePath(FullOpticsRoot, FullFootContactRoot) ||
        FPaths::IsSamePath(FullR33, FullAuthorization) ||
        ExpectedAcceptedR33ReceiptSha256.Equals(
            ExpectedFutureTransactionAuthorizationSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Composition admission requires three exact distinct named roots and independent R33/authorization receipts.");
        return false;
    }

    TArray<uint8> OpticsIntentBytes;
    TArray<uint8> FootContactIntentBytes;
    TSharedPtr<FJsonObject> CompositionContract;
    TSharedPtr<FJsonObject> OpticsContract;
    TSharedPtr<FJsonObject> OpticsOfflineAudit;
    TSharedPtr<FJsonObject> FootContactContract;
    if (!ParsePinnedJson(
            SourcePath(
                FullCompositionRoot,
                TEXT("building_optics_contact_composition_candidate.contract.v1.json")),
            7087,
            CompositionContractSha256,
            CompositionContract,
            OutError) ||
        !ParsePinnedJson(
            SourcePath(
                FullOpticsRoot,
                TEXT("building_surface_optics_candidate.contract.json")),
            8382,
            OpticsCandidateContractSha256,
            OpticsContract,
            OutError) ||
        !ParsePinnedJson(
            SourcePath(
                FullOpticsRoot,
                TEXT("OfflineAudit/building_surface_optics_audit.json")),
            11419,
            OpticsOfflineAuditSha256,
            OpticsOfflineAudit,
            OutError) ||
        !ParsePinnedJson(
            SourcePath(
                FullFootContactRoot,
                TEXT("building_foot_contact_candidate.contract.v1.json")),
            4823,
            FootContactCandidateContractSha256,
            FootContactContract,
            OutError) ||
        !ValidateCompositionCandidateContract(CompositionContract, OutError) ||
        !ValidateOpticsCandidateContract(OpticsContract, OutError) ||
        !ValidateOpticsOfflineAudit(OpticsOfflineAudit, OutError) ||
        !ValidateFootContactCandidateContract(FootContactContract, OutError))
    {
        return false;
    }

    for (const FSourcePin& Pin : CompositionSourcePins)
    {
        TArray<uint8> Bytes;
        if (!LoadPinnedBytes(
                SourcePath(FullCompositionRoot, Pin.RelativePath),
                Pin.Bytes,
                Pin.Sha256,
                Bytes,
                OutError))
        {
            return false;
        }
    }
    for (const FSourcePin& Pin : OpticsSourcePins)
    {
        TArray<uint8> Bytes;
        if (!LoadPinnedBytes(
                SourcePath(FullOpticsRoot, Pin.RelativePath),
                Pin.Bytes,
                Pin.Sha256,
                Bytes,
                OutError))
        {
            return false;
        }
        if (FString(Pin.RelativePath) ==
            TEXT("building_surface_optics.material_intent.hlsl"))
        {
            OpticsIntentBytes = MoveTemp(Bytes);
        }
    }
    for (const FSourcePin& Pin : FootContactSourcePins)
    {
        TArray<uint8> Bytes;
        if (!LoadPinnedBytes(
                SourcePath(FullFootContactRoot, Pin.RelativePath),
                Pin.Bytes,
                Pin.Sha256,
                Bytes,
                OutError))
        {
            return false;
        }
        if (FString(Pin.RelativePath) ==
            TEXT("building_foot_contact.material_intent.hlsl"))
        {
            FootContactIntentBytes = MoveTemp(Bytes);
        }
    }
    FFileHelper::BufferToString(
        OutAdmission.OpticsMaterialIntentText,
        OpticsIntentBytes.GetData(),
        OpticsIntentBytes.Num());
    FFileHelper::BufferToString(
        OutAdmission.FootContactMaterialIntentText,
        FootContactIntentBytes.GetData(),
        FootContactIntentBytes.Num());
    FString OpticsBody;
    FString FootContactBody;
    if (OutAdmission.OpticsMaterialIntentText.IsEmpty() ||
        OutAdmission.FootContactMaterialIntentText.IsEmpty() ||
        !ExtractOpticsFunctionBody(
            OutAdmission.OpticsMaterialIntentText, OpticsBody, OutError) ||
        !ExtractFootContactFunctionBody(
            OutAdmission.FootContactMaterialIntentText,
            FootContactBody,
            OutError))
    {
        return false;
    }

    FString SurroundingsRoot = FPaths::GetPath(FullCompositionRoot);
    FPaths::NormalizeDirectoryName(SurroundingsRoot);
    FString OpticsParent = FPaths::GetPath(FullOpticsRoot);
    FString FootContactParent = FPaths::GetPath(FullFootContactRoot);
    FPaths::NormalizeDirectoryName(OpticsParent);
    FPaths::NormalizeDirectoryName(FootContactParent);
    TArray<uint8> IntegrationContractBytes;
    if (!FPaths::IsSamePath(SurroundingsRoot, OpticsParent) ||
        !FPaths::IsSamePath(SurroundingsRoot, FootContactParent) ||
        !LoadPinnedBytes(
            SourcePath(
                SurroundingsRoot,
                TEXT("BuildingSurfaceOpticsIntegration/building_surface_optics_post_r33.source_contract.v1.json")),
            13397,
            OpticsIntegrationContractSha256,
            IntegrationContractBytes,
            OutError) ||
        !LoadPinnedBytes(
            SourcePath(
                SurroundingsRoot,
                TEXT("BuildingFootContactIntegration/building_foot_contact_post_r33.source_contract.v1.json")),
            15166,
            FootContactIntegrationContractSha256,
            IntegrationContractBytes,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Composition roots do not share one exact Surroundings source closure.");
        }
        return false;
    }

    FString R31Report;
    if (!TRIADIstanaExploreV5DR31BroadShellAssetFactory::ValidateAssets(
            R31Report))
    {
        OutError = TEXT("The exact native R31 broad-shell material/geometry predecessor is unavailable or invalid: ") +
            R31Report;
        return false;
    }
    const int64 R33Bytes = IFileManager::Get().FileSize(*FullR33);
    const int64 AuthorizationBytes =
        IFileManager::Get().FileSize(*FullAuthorization);
    if (R33Bytes < 2 || R33Bytes > MaximumInputBytes ||
        AuthorizationBytes < 2 || AuthorizationBytes > MaximumInputBytes)
    {
        OutError = TEXT("A building-optics/contact composition gate receipt is absent or outside the bounded input size.");
        return false;
    }
    TSharedPtr<FJsonObject> AcceptedR33;
    TSharedPtr<FJsonObject> FutureAuthorization;
    if (!ParsePinnedJson(
            FullR33,
            R33Bytes,
            ExpectedAcceptedR33ReceiptSha256,
            AcceptedR33,
            OutError) ||
        !ValidateAcceptedR33Receipt(AcceptedR33, OutError) ||
        !ParsePinnedJson(
            FullAuthorization,
            AuthorizationBytes,
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

    OutAdmission.CompositionCandidateRoot = FullCompositionRoot;
    OutAdmission.OpticsCandidateRoot = FullOpticsRoot;
    OutAdmission.FootContactCandidateRoot = FullFootContactRoot;
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
    return IsValid(Object) && Object->GetPathName() == ObjectPath
        ? Object
        : nullptr;
}

FString CandidateObjectPath(int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < SlotCount
        ? MaterialRoot + TEXT("/") + CandidateNames[SlotIndex] +
              TEXT(".") + CandidateNames[SlotIndex]
        : FString();
}

FString OpticsFallbackObjectPath(int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < SlotCount
        ? OpticsMaterialRoot + TEXT("/") + OpticsFallbackNames[SlotIndex] +
              TEXT(".") + OpticsFallbackNames[SlotIndex]
        : FString();
}

bool SameReferencedTextures(
    const UMaterialInterface* Left,
    const UMaterialInterface* Right);

bool HasNoInstancePhysicalAuthority(
    const UMaterialInstanceConstant* Instance)
{
    if (!Instance || Instance->PhysMaterial || Instance->PhysMaterialMask)
    {
        return false;
    }
    for (const TObjectPtr<UPhysicalMaterial>& PhysicalMaterial :
         Instance->PhysicalMaterialMap)
    {
        if (PhysicalMaterial)
        {
            return false;
        }
    }
    return true;
}

bool HasExactInstanceOverrideState(
    const UMaterialInstanceConstant* Left,
    const UMaterialInstanceConstant* Right)
{
    return Left && Right &&
        Left->ScalarParameterValues == Right->ScalarParameterValues &&
        Left->VectorParameterValues == Right->VectorParameterValues &&
        Left->DoubleVectorParameterValues ==
            Right->DoubleVectorParameterValues &&
        Left->TextureParameterValues == Right->TextureParameterValues &&
        Left->TextureCollectionParameterValues ==
            Right->TextureCollectionParameterValues &&
        Left->RuntimeVirtualTextureParameterValues ==
            Right->RuntimeVirtualTextureParameterValues &&
        Left->SparseVolumeTextureParameterValues ==
            Right->SparseVolumeTextureParameterValues &&
        Left->FontParameterValues == Right->FontParameterValues &&
        Left->UserSceneTextureOverrides.IsEmpty() &&
        Right->UserSceneTextureOverrides.IsEmpty() &&
        Left->GetStaticParameters().Equivalent(Right->GetStaticParameters()) &&
        Left->BasePropertyOverrides == Right->BasePropertyOverrides;
}

bool ValidateExactOpticsFallbackAssets(FString& OutError)
{
    TArray<TObjectPtr<UMaterialInterface>> OpticsFallbacks;
    OpticsFallbacks.Reserve(SlotCount);
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        UMaterialInstanceConstant* Material =
            LoadExact<UMaterialInstanceConstant>(
                OpticsFallbackObjectPath(Index));
        UMaterialInstanceConstant* ExactR31Fallback =
            LoadExact<UMaterialInstanceConstant>(R31FallbackPaths[Index]);
        if (!Material || !ExactR31Fallback || !Material->Parent ||
            Material->Parent->GetPathName() != OpticsMasterObjectPath ||
            !ExactR31Fallback->Parent ||
            ExactR31Fallback->Parent->GetPathName() !=
                TRIADIstanaExploreV5DR31BroadShellAssetFactory::
                    GetMasterMaterialObjectPath() ||
            !HasExactInstanceOverrideState(Material, ExactR31Fallback) ||
            !HasNoInstancePhysicalAuthority(Material) ||
            !HasNoInstancePhysicalAuthority(ExactR31Fallback) ||
            Material->NaniteOverrideMaterial.bEnableOverride !=
                ExactR31Fallback->NaniteOverrideMaterial.bEnableOverride ||
            Material->NaniteOverrideMaterial.GetOverrideMaterial() !=
                ExactR31Fallback->NaniteOverrideMaterial.GetOverrideMaterial() ||
            !SameReferencedTextures(Material, ExactR31Fallback))
        {
            OutError = FString::Printf(
                TEXT("Exact BuildingSurfaceOptics fallback %d is absent, retains physical-material or physical-material-mask authority, or its actual scalar/vector/double-vector/texture/collection/RVT/sparse-volume/font/static/base override state differs from exact R31."),
                Index);
            return false;
        }
        UPackage* Package = Material->GetOutermost();
        UMetaData* Metadata = Package && Package->HasMetaData()
            ? Package->GetMetaData()
            : nullptr;
        if (!Metadata ||
            Metadata->GetValue(
                Material,
                TEXT("TRIAD_BuildingSurfaceOpticsCandidateContractSha256")) !=
                OpticsCandidateContractSha256 ||
            Metadata->GetValue(
                Material,
                TEXT("TRIAD_BuildingSurfaceOpticsMaterialIntentSha256")) !=
                OpticsMaterialIntentSha256 ||
            Metadata->GetValue(
                Material,
                TEXT("TRIAD_BuildingSurfaceOpticsOfflineAuditSha256")) !=
                OpticsOfflineAuditSha256 ||
            Metadata->GetValue(
                Material,
                TEXT("TRIAD_BuildingSurfaceOpticsR31ContractSha256")) !=
                R31ContractSha256 ||
            Metadata->GetValue(
                Material,
                TEXT("TRIAD_BuildingSurfaceOpticsSlotIndex")) !=
                FString::FromInt(Index) ||
            Metadata->GetValue(
                Material,
                TEXT("TRIAD_BuildingSurfaceOpticsSlotName")) !=
                SlotNames[Index] ||
            Metadata->GetValue(
                Material,
                TEXT("TRIAD_BuildingSurfaceOpticsExactR31Fallback")) !=
                R31FallbackPaths[Index])
        {
            OutError = FString::Printf(
                TEXT("BuildingSurfaceOptics fallback %d lacks the exact source and R31 provenance metadata."),
                Index);
            return false;
        }
        OpticsFallbacks.Add(Material);
    }
    if (!UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
            ValidateExactOpticsFallbackRoster(OpticsFallbacks, OutError))
    {
        return false;
    }
    UMaterial* OpticsMaster = LoadExact<UMaterial>(OpticsMasterObjectPath);
    UPackage* MasterPackage = OpticsMaster ? OpticsMaster->GetOutermost() : nullptr;
    UMetaData* MasterMetadata = MasterPackage && MasterPackage->HasMetaData()
        ? MasterPackage->GetMetaData()
        : nullptr;
    if (!OpticsMaster || !MasterMetadata ||
        MasterMetadata->GetValue(
            OpticsMaster,
            TEXT("TRIAD_BuildingSurfaceOpticsCandidateContractSha256")) !=
            OpticsCandidateContractSha256 ||
        MasterMetadata->GetValue(
            OpticsMaster,
            TEXT("TRIAD_BuildingSurfaceOpticsMaterialIntentSha256")) !=
            OpticsMaterialIntentSha256 ||
        MasterMetadata->GetValue(
            OpticsMaster,
            TEXT("TRIAD_BuildingSurfaceOpticsGeometryInvariant")) !=
            TEXT("43448_TRIANGLES_17_SLOTS_1388_GROUPS_IDENTITY_TRANSFORM_NO_MESH_BINDING"))
    {
        OutError = TEXT("Exact BuildingSurfaceOptics master provenance is absent or drifted.");
        return false;
    }
    OutError.Reset();
    return true;
}

void GetOutputObjectPaths(TArray<FString>& OutPaths)
{
    OutPaths.Reset(OutputAssetCount);
    OutPaths.Add(MasterObjectPath);
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        OutPaths.Add(CandidateObjectPath(Index));
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
            Files, *OutState.PhysicalRoot, TEXT("*"), true, false, true);
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
        OutError = TEXT("Building-surface namespace has no resolvable package-root filesystem path.");
        return false;
    }
    FPaths::NormalizeDirectoryName(OutPhysicalRoot);
    OutPhysicalDirectories.Add(OutPhysicalRoot);
    for (const FString& ObjectPath : ExpectedObjectPaths)
    {
        if (ObjectPath.IsEmpty() || OutObjects.Contains(ObjectPath))
        {
            OutError = TEXT("Building-surface expected object roster contains an empty or duplicate path.");
            return false;
        }
        OutObjects.Add(ObjectPath);
        const FString PackagePath =
            FPackageName::ObjectPathToPackageName(ObjectPath);
        if (!IsPackageWithinNamespace(PackagePath, NamespaceRoot) ||
            OutPackages.Contains(PackagePath))
        {
            OutError = TEXT("Building-surface expected package roster escaped the isolated namespace or duplicated a package.");
            return false;
        }
        OutPackages.Add(PackagePath);
        FString PhysicalFile;
        if (!FPackageName::TryConvertLongPackageNameToFilename(
                PackagePath,
                PhysicalFile,
                FPackageName::GetAssetPackageExtension()))
        {
            OutError = TEXT("A building-optics/contact composition output package has no exact .uasset filename.");
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

bool NamespaceMatchesExpected(
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
        OutError = TEXT("Building-surface physical namespace resolution drifted.");
        return false;
    }
    for (const FString& ObjectPath : Actual.ObjectPaths)
    {
        if (!ExpectedObjects.Contains(ObjectPath))
        {
            OutError = TEXT("Building-surface namespace contains an uncontracted asset: ") +
                ObjectPath;
            return false;
        }
    }
    for (const FString& PackagePath : Actual.PackagePaths)
    {
        if (!ExpectedPackages.Contains(PackagePath))
        {
            OutError = TEXT("Building-surface namespace contains an uncontracted loaded or physical package: ") +
                PackagePath;
            return false;
        }
    }
    for (const FString& PhysicalFile : Actual.PhysicalFiles)
    {
        if (!ExpectedPhysicalFiles.Contains(PhysicalFile))
        {
            OutError = TEXT("Building-surface namespace contains an uncontracted physical file: ") +
                PhysicalFile;
            return false;
        }
    }
    for (const FString& PhysicalDirectory : Actual.PhysicalDirectories)
    {
        if (!ExpectedPhysicalDirectories.Contains(PhysicalDirectory))
        {
            OutError = TEXT("Building-surface namespace contains an uncontracted physical directory: ") +
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
        OutError = TEXT("Building-surface namespace does not contain the exact eighteen-object/package roster.");
        return false;
    }
    if (bRequirePhysicalAssetFiles &&
        (Actual.PhysicalFiles.Num() != ExpectedPhysicalFiles.Num() ||
         Actual.PhysicalFiles.Difference(ExpectedPhysicalFiles).Num() != 0 ||
         ExpectedPhysicalFiles.Difference(Actual.PhysicalFiles).Num() != 0 ||
         Actual.PhysicalDirectories.Difference(ExpectedPhysicalDirectories).Num() != 0 ||
         ExpectedPhysicalDirectories.Difference(Actual.PhysicalDirectories).Num() != 0))
    {
        OutError = TEXT("Saved building-optics/contact composition namespace lacks the exact eighteen physical .uasset files or directory closure.");
        return false;
    }
    OutError.Reset();
    return true;
}

int32 ExistingOutputPackageCount()
{
    TArray<FString> Paths;
    GetOutputObjectPaths(Paths);
    int32 Count = 0;
    for (const FString& Path : Paths)
    {
        if (FPackageName::DoesPackageExist(
                FPackageName::ObjectPathToPackageName(Path)))
        {
            ++Count;
        }
    }
    return Count;
}

bool DeleteFreshNamespaceContents(FString& OutError)
{
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutError = TEXT("Editor asset subsystem unavailable during building-optics/contact composition rollback.");
        return false;
    }
    FNamespaceState Before;
    CollectNamespaceState(OutputRoot, Before);
    TArray<UObject*> LoadedAssets;
    for (const FString& ObjectPath : Before.ObjectPaths)
    {
        if (UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath))
        {
            LoadedAssets.AddUnique(Object);
        }
    }
    const bool bLoadedDeleteSucceeded =
        LoadedAssets.IsEmpty() || AssetSubsystem->DeleteLoadedAssets(LoadedAssets);
    const bool bDirectoryDeleteSucceeded =
        AssetSubsystem->DeleteDirectory(OutputRoot);
    FAssetRegistryModule::GetRegistry().ScanPathsSynchronous(
        {OutputRoot}, true, true);
    FNamespaceState After;
    CollectNamespaceState(OutputRoot, After);
    const bool bPhysicalRootAbsent = After.bPhysicalRootResolved &&
        !IFileManager::Get().DirectoryExists(*After.PhysicalRoot);
    const bool bEmpty = After.ObjectPaths.IsEmpty() &&
        After.PackagePaths.IsEmpty() && After.PhysicalFiles.IsEmpty() &&
        After.PhysicalDirectories.IsEmpty();
    if (!bLoadedDeleteSucceeded || !bDirectoryDeleteSucceeded ||
        !bPhysicalRootAbsent || !bEmpty)
    {
        OutError = FString::Printf(
            TEXT("Fresh building-optics/contact composition rollback could not prove cleanup (DeleteLoadedAssets=%s DeleteDirectory=%s physicalRootAbsent=%s namespaceEmpty=%s)."),
            bLoadedDeleteSucceeded ? TEXT("true") : TEXT("false"),
            bDirectoryDeleteSucceeded ? TEXT("true") : TEXT("false"),
            bPhysicalRootAbsent ? TEXT("true") : TEXT("false"),
            bEmpty ? TEXT("true") : TEXT("false"));
        return false;
    }
    OutError.Reset();
    return true;
}

bool RollbackFreshMaterialization(
    bool bOutputRootProvenEmptyAtEntry,
    FString& InOutError)
{
    if (!bOutputRootProvenEmptyAtEntry)
    {
        InOutError += TEXT(" ROLLBACK_REFUSED: isolated output namespace was not proven empty at entry.");
        return false;
    }
    FString RollbackError;
    if (!DeleteFreshNamespaceContents(RollbackError))
    {
        InOutError += TEXT(" ROLLBACK_FAILED: ") + RollbackError;
        return false;
    }
    InOutError += TEXT(" ROLLBACK_COMPLETE recursiveRegistryLivePackageFilesystemCleanup=true physicalRootAbsent=true");
    return true;
}

bool ExactAssetMetadata(
    const UObject* Asset,
    const TCHAR* Key,
    const FString& Expected,
    FString& OutError)
{
    UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
    UMetaData* Metadata = Package && Package->HasMetaData()
        ? Package->GetMetaData()
        : nullptr;
    if (!Asset || !Metadata || Metadata->GetValue(Asset, Key) != Expected)
    {
        OutError = FString::Printf(
            TEXT("Building-surface asset '%s' lost exact metadata '%s'."),
            Asset ? *Asset->GetPathName() : TEXT("<null>"),
            Key);
        return false;
    }
    OutError.Reset();
    return true;
}

bool WriteCommonMetadata(
    UObject* Asset,
    const FAdmission& Admission,
    FString& OutError)
{
    UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
    UMetaData* Metadata = Package ? Package->GetMetaData() : nullptr;
    if (!Asset || !Metadata)
    {
        OutError = TEXT("Composition output requires writable package metadata.");
        return false;
    }
    struct FMetadataPin
    {
        const TCHAR* Key;
        const FString* Value;
    };
    const FMetadataPin Pins[] = {
        {TEXT("TRIAD_BuildingOpticsContactCompositionContractSha256"),
         &CompositionContractSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionOpticsCandidateContractSha256"),
         &OpticsCandidateContractSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionOpticsMaterialIntentSha256"),
         &OpticsMaterialIntentSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionOpticsOfflineAuditSha256"),
         &OpticsOfflineAuditSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionOpticsIntegrationContractSha256"),
         &OpticsIntegrationContractSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionFootContactCandidateContractSha256"),
         &FootContactCandidateContractSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionFootContactMaterialIntentSha256"),
         &FootContactMaterialIntentSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionFootContactGeneratedAuditSha256"),
         &FootContactGeneratedAuditSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionFootContactIntegrationContractSha256"),
         &FootContactIntegrationContractSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionR31ContractSha256"),
         &R31ContractSha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionAcceptedR33ReceiptSha256"),
         &Admission.AcceptedR33Sha256},
        {TEXT("TRIAD_BuildingOpticsContactCompositionFutureAuthorizationSha256"),
         &Admission.FutureAuthorizationSha256}};
    for (const FMetadataPin& Pin : Pins)
    {
        Metadata->SetValue(Asset, Pin.Key, **Pin.Value);
    }
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingOpticsContactCompositionAuthority"),
        TEXT("APPEARANCE_ONLY_NO_MAP_MESH_TRANSFORM_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"));
    for (const FMetadataPin& Pin : Pins)
    {
        if (!ExactAssetMetadata(Asset, Pin.Key, *Pin.Value, OutError))
        {
            return false;
        }
    }
    return ExactAssetMetadata(
        Asset,
        TEXT("TRIAD_BuildingOpticsContactCompositionAuthority"),
        TEXT("APPEARANCE_ONLY_NO_MAP_MESH_TRANSFORM_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"),
        OutError);
}

bool ValidateCommonMetadata(
    const UObject* Asset,
    const FAdmission& Admission,
    FString& OutError)
{
    return ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionContractSha256"),
               CompositionContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionOpticsCandidateContractSha256"),
               OpticsCandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionOpticsMaterialIntentSha256"),
               OpticsMaterialIntentSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionOpticsOfflineAuditSha256"),
               OpticsOfflineAuditSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionOpticsIntegrationContractSha256"),
               OpticsIntegrationContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionFootContactCandidateContractSha256"),
               FootContactCandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionFootContactMaterialIntentSha256"),
               FootContactMaterialIntentSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionFootContactGeneratedAuditSha256"),
               FootContactGeneratedAuditSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionFootContactIntegrationContractSha256"),
               FootContactIntegrationContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionR31ContractSha256"),
               R31ContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingOpticsContactCompositionAuthority"),
               TEXT("APPEARANCE_ONLY_NO_MAP_MESH_TRANSFORM_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"),
               OutError);
}

template <typename TExpression>
TExpression* FindExactExpressionByDesc(
    UMaterial* Material,
    const FString& Desc,
    FString& OutError)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    TExpression* Result = nullptr;
    int32 Count = 0;
    if (EditorOnly)
    {
        for (UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            if (Expression && Expression->Desc == Desc &&
                Expression->GetClass() == TExpression::StaticClass())
            {
                Result = Cast<TExpression>(Expression);
                ++Count;
            }
        }
    }
    if (Count != 1 || !Result)
    {
        OutError = FString::Printf(
            TEXT("Expected exactly one material expression '%s'; found %d."),
            *Desc,
            Count);
        return nullptr;
    }
    OutError.Reset();
    return Result;
}

bool HasExactR31SurfaceInputs(
    const UMaterialExpressionCustom* Surface,
    FString& OutError)
{
    if (!Surface || Surface->Inputs.Num() < ExpectedR31SurfaceInputCount)
    {
        OutError = TEXT("The cloned R31 custom surface omitted predecessor inputs.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedR31SurfaceInputCount; ++Index)
    {
        if (Surface->Inputs[Index].InputName != R31SurfaceInputNames[Index] ||
            !Surface->Inputs[Index].Input.Expression)
        {
            OutError = FString::Printf(
                TEXT("The cloned R31 custom surface input %d lost its exact name or connection."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ComposeCandidateSurfaceCode(
    const FString& R31SurfaceCode,
    const FString& OpticsMaterialIntentText,
    const FString& FootContactMaterialIntentText,
    FString& OutCode,
    FString& OutError)
{
    const FString R31Return(
        TEXT("return float4(finalColor, saturate(finalRoughness));"));
    FString OpticsBody;
    FString FootContactBody;
    if (!R31SurfaceCode.EndsWith(R31Return, ESearchCase::CaseSensitive) ||
        !ExtractOpticsFunctionBody(
            OpticsMaterialIntentText, OpticsBody, OutError) ||
        !ExtractFootContactFunctionBody(
            FootContactMaterialIntentText, FootContactBody, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The validated R31 surface no longer ends at its exact return boundary.");
        }
        return false;
    }
    const FString Aliases(
        TEXT("// BuildingOpticsContactComposition: exact optics splice begins.\n")
        TEXT("float3 R31BaseColor = finalColor;\n")
        TEXT("float3 R31TangentNormal = R31TangentNormalInput;\n")
        TEXT("float R31Roughness = saturate(finalRoughness);\n")
        TEXT("float R31Metallic = R31MetallicInput;\n")
        TEXT("float R31AmbientOcclusion = R31AmbientOcclusionInput;\n")
        TEXT("float WallRoleMask = wallMask;\n")
        TEXT("float GlassMask = visibleGlass;\n")
        TEXT("float FrameMask = visibleFrame;\n")
        TEXT("float DividerMask = visibleDivider;\n")
        TEXT("float2 R31PaneCellIndex = cellIndex;\n")
        TEXT("float2 PaneLocalUv = float2(localU, localV);\n")
        TEXT("float2 FacadeUvMetres = float2(facadeUMetres, sourceAbsoluteZMetres);\n")
        TEXT("float2 RoofUvMetres = UV0;\n")
        TEXT("float3 FacadeSignals = float3(facadeSignalA, facadeSignalB, facadeSignalC);\n")
        TEXT("float3 GeometricNormalWs = geometricNormal;\n")
        TEXT("float3 ViewDirectionWs = viewDirection;\n")
        TEXT("float CameraDistanceMetres = distanceCm * 0.01;\n")
        TEXT("float3 CandidateBaseColor = R31BaseColor;\n"));
    const FString ContactAliases(
        TEXT("\n// BuildingOpticsContactComposition: bounded foot-contact splice begins after every optics output.\n")
        TEXT("float3 ExistingBaseColor = CandidateBaseColor;\n")
        TEXT("float ExistingRoughness = CandidateRoughness;\n")
        TEXT("float ExistingAmbientOcclusion = CandidateAmbientOcclusion;\n")
        TEXT("float ExistingGlassMask = GlassMask;\n")
        TEXT("float SourceAbsoluteZMetres = sourceAbsoluteZMetres;\n")
        TEXT("float2 AbsoluteWorldPositionMetresXY = WorldPositionCm.xy * 0.01;\n")
        TEXT("float3 ContactBaseColor = ExistingBaseColor;\n")
        TEXT("float ContactRoughness = ExistingRoughness;\n")
        TEXT("float ContactAmbientOcclusion = ExistingAmbientOcclusion;\n")
        TEXT("float ContactWeight = 0.0;\n"));
    const FString ContactOutputs(
        TEXT("\nCandidateBaseColor = ContactBaseColor;\n")
        TEXT("CandidateRoughness = ContactRoughness;\n")
        TEXT("CandidateAmbientOcclusion = ContactAmbientOcclusion;\n")
        TEXT("// BuildingOpticsContactComposition: tangent normal, metallic, Clear Coat and Clear Coat roughness pass through unchanged.\n"));
    OutCode = R31SurfaceCode.LeftChop(R31Return.Len()) + Aliases +
        OpticsBody + ContactAliases + FootContactBody + ContactOutputs +
        TEXT("\nreturn float4(CandidateBaseColor, CandidateRoughness);");
    if (!OutCode.Contains(
            TEXT("float2 R31PaneCellIndex = cellIndex;"),
            ESearchCase::CaseSensitive) ||
        OutCode.Contains(TEXT("3.0, 3.2"), ESearchCase::CaseSensitive) ||
        !OutCode.Contains(
            TEXT("float3 ExistingBaseColor = CandidateBaseColor;"),
            ESearchCase::CaseSensitive) ||
        !OutCode.Contains(
            TEXT("CandidateAmbientOcclusion = ContactAmbientOcclusion;"),
            ESearchCase::CaseSensitive) ||
        OutCode.EndsWith(R31Return, ESearchCase::CaseSensitive))
    {
        OutError = TEXT("Composition did not preserve exact R31 pane identity or did not place bounded contact after optics outputs.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool HasExactCompositionOrderingAndPassThrough(const FString& Code)
{
    const int32 OpticsBegin = Code.Find(
        TEXT("BuildingOpticsContactComposition: exact optics splice begins."),
        ESearchCase::CaseSensitive);
    const int32 ContactBegin = Code.Find(
        TEXT("BuildingOpticsContactComposition: bounded foot-contact splice begins after every optics output."),
        ESearchCase::CaseSensitive);
    const int32 FinalReturn = Code.Find(
        TEXT("return float4(CandidateBaseColor, CandidateRoughness);"),
        ESearchCase::CaseSensitive,
        ESearchDir::FromEnd);
    if (OpticsBegin == INDEX_NONE || ContactBegin <= OpticsBegin ||
        FinalReturn <= ContactBegin)
    {
        return false;
    }
    const FString ContactTail = Code.Mid(ContactBegin, FinalReturn - ContactBegin);
    const TCHAR* const RequiredContactOutputs[] = {
        TEXT("CandidateBaseColor = ContactBaseColor;"),
        TEXT("CandidateRoughness = ContactRoughness;"),
        TEXT("CandidateAmbientOcclusion = ContactAmbientOcclusion;")};
    const TCHAR* const ForbiddenContactWrites[] = {
        TEXT("CandidateTangentNormal ="), TEXT("CandidateMetallic ="),
        TEXT("CandidateClearCoat ="),
        TEXT("CandidateClearCoatRoughness =")};
    for (const TCHAR* Token : RequiredContactOutputs)
    {
        if (!ContactTail.Contains(Token, ESearchCase::CaseSensitive))
        {
            return false;
        }
    }
    for (const TCHAR* Token : ForbiddenContactWrites)
    {
        if (ContactTail.Contains(Token, ESearchCase::CaseSensitive))
        {
            return false;
        }
    }
    return true;
}

bool ConnectCandidateProperty(
    UMaterialExpressionCustom* Surface,
    const FString& OutputName,
    EMaterialProperty Property,
    FString& OutError)
{
    if (!Surface ||
        !UMaterialEditingLibrary::ConnectMaterialProperty(
            Surface, OutputName, Property))
    {
        OutError = TEXT("Could not connect candidate custom output '") +
            OutputName + TEXT("'.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool InputMatches(
    const FExpressionInput& Input,
    const UMaterialExpression* Expected,
    int32 ExpectedOutputIndex)
{
    return Expected && Input.Expression == Expected &&
        Input.OutputIndex == ExpectedOutputIndex;
}

bool InputExactlySelectsExpressionOutput(
    const FExpressionInput& Input,
    UMaterialExpression* ExpectedExpression,
    int32 ExpectedOutputIndex,
    const FName ExpectedInputName)
{
    if (!ExpectedExpression ||
        Input.Expression != ExpectedExpression ||
        Input.OutputIndex != ExpectedOutputIndex ||
        Input.InputName != ExpectedInputName)
    {
        return false;
    }
    const TArray<FExpressionOutput>& Outputs =
        ExpectedExpression->GetOutputs();
    if (!Outputs.IsValidIndex(ExpectedOutputIndex))
    {
        return false;
    }
    const FExpressionOutput& Output = Outputs[ExpectedOutputIndex];
    return Input.Mask == Output.Mask && Input.MaskR == Output.MaskR &&
        Input.MaskG == Output.MaskG && Input.MaskB == Output.MaskB &&
        Input.MaskA == Output.MaskA;
}

bool HasExactCandidateSurfaceOutputs(
    const UMaterialExpressionCustom* Surface)
{
    if (!Surface || !Surface->bShowOutputNameOnPin ||
        Surface->Outputs.Num() != ExpectedCandidateSurfaceOutputCount)
    {
        return false;
    }
    for (int32 Index = 0;
         Index < ExpectedCandidateSurfaceOutputCount;
         ++Index)
    {
        const FExpressionOutput& Output = Surface->Outputs[Index];
        if (Output.OutputName != CandidateSurfaceOutputNames[Index] ||
            Output.Mask != 0 || Output.MaskR != 0 || Output.MaskG != 0 ||
            Output.MaskB != 0 || Output.MaskA != 0)
        {
            return false;
        }
    }
    return true;
}

bool IsDeniedPresentationPropertyConnected(const UMaterial* Material)
{
    const EMaterialProperty Denied[] = {
        MP_EmissiveColor, MP_Opacity, MP_OpacityMask, MP_Anisotropy,
        MP_Tangent, MP_WorldPositionOffset, MP_SubsurfaceColor,
        MP_Refraction, MP_CustomizedUVs0,
        MP_CustomizedUVs1, MP_CustomizedUVs2, MP_CustomizedUVs3,
        MP_CustomizedUVs4, MP_CustomizedUVs5, MP_CustomizedUVs6,
        MP_CustomizedUVs7, MP_PixelDepthOffset, MP_ShadingModel,
        MP_FrontMaterial, MP_SurfaceThickness, MP_Displacement,
        MP_MaterialAttributes};
    if (!Material)
    {
        return true;
    }
    for (const EMaterialProperty Property : Denied)
    {
        if (Material->IsPropertyConnected(Property))
        {
            return true;
        }
    }
    return false;
}

const FString ExactR31SurfaceDesc(
    TEXT("R31.TexturedWeatheredR25DepthSurface"));
const FString CandidateSurfaceDesc(
    TEXT("BOC.ExactR31SurfacePlusOpticsThenBoundedContact"));
const FString CandidateSurfaceDescription(
    TEXT("TRIAD_IPV5D_UNNUMBERED_POST_R33_EXACT_R31_MASKS_PANE_CELL_OPTICS_THEN_BOUNDED_SOURCE_PLANE_CONTACT"));

FString NormalizedExpressionKey(
    const UMaterialExpression* Expression,
    bool bCandidateGraph)
{
    if (!Expression)
    {
        return FString();
    }
    return bCandidateGraph && Expression->Desc == CandidateSurfaceDesc
        ? ExactR31SurfaceDesc
        : Expression->Desc;
}

bool BuildExpressionMap(
    UMaterial* Material,
    bool bCandidateGraph,
    TMap<FString, UMaterialExpression*>& OutNodes,
    FString& OutError)
{
    OutNodes.Reset();
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly)
    {
        OutError = TEXT("Exact R31 graph comparison requires both material editor graphs.");
        return false;
    }
    for (UMaterialExpression* Expression :
         EditorOnly->ExpressionCollection.Expressions)
    {
        const FString Key = NormalizedExpressionKey(
            Expression, bCandidateGraph);
        if (!Expression || Expression->GetOuter() != Material ||
            Expression->Material != Material || Expression->Function ||
            Expression->SubgraphExpression || Key.IsEmpty() ||
            OutNodes.Contains(Key))
        {
            OutError = TEXT("Exact R31 graph comparison found null, foreign, function-owned, subgraph-owned, empty-label, or duplicate-label expression state.");
            return false;
        }
        OutNodes.Add(Key, Expression);
    }
    OutError.Reset();
    return true;
}

bool ExpressionInputSelectorFieldsMatch(
    const FExpressionInput& Candidate,
    const FExpressionInput& Exact)
{
    return Candidate.InputName == Exact.InputName &&
        Candidate.Mask == Exact.Mask && Candidate.MaskR == Exact.MaskR &&
        Candidate.MaskG == Exact.MaskG && Candidate.MaskB == Exact.MaskB &&
        Candidate.MaskA == Exact.MaskA;
}

bool ExpressionInputEdgeMatches(
    const FExpressionInput& Candidate,
    const FExpressionInput& Exact,
    const TMap<FString, UMaterialExpression*>& CandidateNodes,
    const TMap<FString, UMaterialExpression*>& ExactNodes)
{
    if (!ExpressionInputSelectorFieldsMatch(Candidate, Exact) ||
        Candidate.OutputIndex != Exact.OutputIndex ||
        (Candidate.Expression == nullptr) != (Exact.Expression == nullptr))
    {
        return false;
    }
    if (!Candidate.Expression)
    {
        return true;
    }
    const FString CandidateKey = NormalizedExpressionKey(
        Candidate.Expression, true);
    const FString ExactKey = NormalizedExpressionKey(
        Exact.Expression, false);
    return !CandidateKey.IsEmpty() && CandidateKey == ExactKey &&
        Candidate.Expression->GetClass() == Exact.Expression->GetClass() &&
        CandidateNodes.FindRef(CandidateKey) == Candidate.Expression &&
        ExactNodes.FindRef(ExactKey) == Exact.Expression;
}

bool ExpressionOutputsMatch(
    UMaterialExpression* Candidate,
    UMaterialExpression* Exact)
{
    if (!Candidate || !Exact)
    {
        return false;
    }
    const TArray<FExpressionOutput>& CandidateOutputs =
        Candidate->GetOutputs();
    const TArray<FExpressionOutput>& ExactOutputs = Exact->GetOutputs();
    if (CandidateOutputs.Num() != ExactOutputs.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < CandidateOutputs.Num(); ++Index)
    {
        const FExpressionOutput& Left = CandidateOutputs[Index];
        const FExpressionOutput& Right = ExactOutputs[Index];
        if (Left.OutputName != Right.OutputName || Left.Mask != Right.Mask ||
            Left.MaskR != Right.MaskR || Left.MaskG != Right.MaskG ||
            Left.MaskB != Right.MaskB || Left.MaskA != Right.MaskA)
        {
            return false;
        }
    }
    return true;
}

FString NormalizeExpressionPropertyText(
    FString Value,
    const UMaterial* Owner)
{
    if (!Owner)
    {
        return Value;
    }
    Value.ReplaceInline(
        *Owner->GetPathName(),
        TEXT("<EXACT_GRAPH_OWNER>"),
        ESearchCase::CaseSensitive);
    if (Owner->GetOutermost())
    {
        Value.ReplaceInline(
            *Owner->GetOutermost()->GetName(),
            TEXT("<EXACT_GRAPH_PACKAGE>"),
            ESearchCase::CaseSensitive);
    }
    Value.ReplaceInline(
        *Owner->GetName(),
        TEXT("<EXACT_GRAPH_MATERIAL>"),
        ESearchCase::CaseSensitive);
    return Value;
}

bool PersistentExpressionPayloadMatchesExcept(
    const UMaterialExpression* Candidate,
    const UMaterialExpression* Exact,
    const TSet<FName>& AllowedDifferences,
    FString& OutError)
{
    if (!Candidate || !Exact || Candidate->GetClass() != Exact->GetClass())
    {
        OutError = TEXT("Exact R31 expression class comparison failed.");
        return false;
    }
    for (TFieldIterator<FProperty> PropertyIt(
             Candidate->GetClass(), EFieldIteratorFlags::IncludeSuper);
         PropertyIt;
         ++PropertyIt)
    {
        const FProperty* Property = *PropertyIt;
        if (!Property || AllowedDifferences.Contains(Property->GetFName()) ||
            Property->HasAnyPropertyFlags(
                CPF_Transient | CPF_DuplicateTransient |
                CPF_NonPIEDuplicateTransient))
        {
            continue;
        }
        for (int32 ArrayIndex = 0;
             ArrayIndex < Property->ArrayDim;
             ++ArrayIndex)
        {
            FString CandidateText;
            FString ExactText;
            const bool bCandidateExported =
                Property->ExportText_InContainer(
                    ArrayIndex,
                    CandidateText,
                    Candidate,
                    nullptr,
                    const_cast<UMaterialExpression*>(Candidate),
                    PPF_None);
            const bool bExactExported = Property->ExportText_InContainer(
                ArrayIndex,
                ExactText,
                Exact,
                nullptr,
                const_cast<UMaterialExpression*>(Exact),
                PPF_None);
            CandidateText = NormalizeExpressionPropertyText(
                MoveTemp(CandidateText), Candidate->Material);
            ExactText = NormalizeExpressionPropertyText(
                MoveTemp(ExactText), Exact->Material);
            if (bCandidateExported != bExactExported ||
                CandidateText != ExactText)
            {
                OutError = FString::Printf(
                    TEXT("Exact R31 persistent payload drifted for expression '%s', property '%s', array index %d."),
                    *Exact->Desc,
                    *Property->GetName(),
                    ArrayIndex);
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

template <typename TInput>
bool RootValueInputMatches(
    const TInput& Candidate,
    const TInput& Exact,
    const TMap<FString, UMaterialExpression*>& CandidateNodes,
    const TMap<FString, UMaterialExpression*>& ExactNodes)
{
    return ExpressionInputEdgeMatches(
               Candidate, Exact, CandidateNodes, ExactNodes) &&
        Candidate.UseConstant == Exact.UseConstant &&
        Candidate.Constant == Exact.Constant;
}

template <typename TInput>
bool RootValueInputStateMatchesExceptEdge(
    const TInput& Candidate,
    const TInput& Exact)
{
    return ExpressionInputSelectorFieldsMatch(Candidate, Exact) &&
        Candidate.UseConstant == Exact.UseConstant &&
        Candidate.Constant == Exact.Constant;
}

bool MaterialAttributesInputMatches(
    const FMaterialAttributesInput& Candidate,
    const FMaterialAttributesInput& Exact,
    const TMap<FString, UMaterialExpression*>& CandidateNodes,
    const TMap<FString, UMaterialExpression*>& ExactNodes)
{
    return ExpressionInputEdgeMatches(
               Candidate, Exact, CandidateNodes, ExactNodes) &&
        Candidate.PropertyConnectedMask == Exact.PropertyConnectedMask;
}

bool ValidateExactUnchangedR31RootSelectors(
    const UMaterialEditorOnlyData* Candidate,
    const UMaterialEditorOnlyData* Exact,
    const TMap<FString, UMaterialExpression*>& CandidateNodes,
    const TMap<FString, UMaterialExpression*>& ExactNodes,
    FString& OutError)
{
    if (!Candidate || !Exact ||
        // These six roots intentionally select the composed custom outputs;
        // R31.SurfaceRoughnessA remains the exact MP_Roughness route.
        // every other selector bit and constant remains inherited from R31.
        !RootValueInputStateMatchesExceptEdge(
            Candidate->BaseColor, Exact->BaseColor) ||
        !RootValueInputStateMatchesExceptEdge(
            Candidate->Normal, Exact->Normal) ||
        !RootValueInputStateMatchesExceptEdge(
            Candidate->Metallic, Exact->Metallic) ||
        !RootValueInputStateMatchesExceptEdge(
            Candidate->AmbientOcclusion, Exact->AmbientOcclusion) ||
        !RootValueInputStateMatchesExceptEdge(
            Candidate->ClearCoat, Exact->ClearCoat) ||
        !RootValueInputStateMatchesExceptEdge(
            Candidate->ClearCoatRoughness, Exact->ClearCoatRoughness) ||
        // MP_Specular is not a composition output: it must retain the exact
        // R31.Specular expression, output index, masks, and constant selector.
        !RootValueInputMatches(
            Candidate->Specular,
            Exact->Specular,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->Roughness,
            Exact->Roughness,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->Anisotropy,
            Exact->Anisotropy,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->Tangent,
            Exact->Tangent,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->EmissiveColor,
            Exact->EmissiveColor,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->Opacity,
            Exact->Opacity,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->OpacityMask,
            Exact->OpacityMask,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->WorldPositionOffset,
            Exact->WorldPositionOffset,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->Displacement,
            Exact->Displacement,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->SubsurfaceColor,
            Exact->SubsurfaceColor,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->Refraction,
            Exact->Refraction,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->PixelDepthOffset,
            Exact->PixelDepthOffset,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->ShadingModelFromMaterialExpression,
            Exact->ShadingModelFromMaterialExpression,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->SurfaceThickness,
            Exact->SurfaceThickness,
            CandidateNodes,
            ExactNodes) ||
        !RootValueInputMatches(
            Candidate->FrontMaterial,
            Exact->FrontMaterial,
            CandidateNodes,
            ExactNodes) ||
        !MaterialAttributesInputMatches(
            Candidate->MaterialAttributes,
            Exact->MaterialAttributes,
            CandidateNodes,
            ExactNodes) ||
        Candidate->ParameterGroupData != Exact->ParameterGroupData ||
        Candidate->SubstrateConversionVersion !=
            Exact->SubstrateConversionVersion)
    {
        OutError = TEXT("Composition master changed an R31 root selector, constant, parameter-group field, or MP_Specular edge outside the seven explicit presentation outputs.");
        return false;
    }
    for (int32 Index = 0; Index < 8; ++Index)
    {
        if (!RootValueInputMatches(
                Candidate->CustomizedUVs[Index],
                Exact->CustomizedUVs[Index],
                CandidateNodes,
                ExactNodes))
        {
            OutError = FString::Printf(
                TEXT("Composition master changed exact R31 customized-UV root selector %d."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

void GatherReachableExpressions(
    const UMaterialExpression* Expression,
    TSet<const UMaterialExpression*>& OutReachable)
{
    if (!Expression || OutReachable.Contains(Expression))
    {
        return;
    }
    OutReachable.Add(Expression);
    for (int32 Index = 0; Index < Expression->CountInputs(); ++Index)
    {
        const FExpressionInput* Input = Expression->GetInput(Index);
        if (Input)
        {
            GatherReachableExpressions(Input->Expression, OutReachable);
        }
    }
}

void GatherReachableFromMaterialRoots(
    const UMaterialEditorOnlyData* EditorOnly,
    TSet<const UMaterialExpression*>& OutReachable)
{
    OutReachable.Reset();
    if (!EditorOnly)
    {
        return;
    }
    const FExpressionInput* const Roots[] = {
        &EditorOnly->BaseColor, &EditorOnly->Metallic,
        &EditorOnly->Specular, &EditorOnly->Roughness,
        &EditorOnly->Anisotropy, &EditorOnly->Normal,
        &EditorOnly->Tangent, &EditorOnly->EmissiveColor,
        &EditorOnly->Opacity, &EditorOnly->OpacityMask,
        &EditorOnly->WorldPositionOffset, &EditorOnly->Displacement,
        &EditorOnly->SubsurfaceColor, &EditorOnly->ClearCoat,
        &EditorOnly->ClearCoatRoughness, &EditorOnly->AmbientOcclusion,
        &EditorOnly->Refraction, &EditorOnly->MaterialAttributes,
        &EditorOnly->PixelDepthOffset,
        &EditorOnly->ShadingModelFromMaterialExpression,
        &EditorOnly->SurfaceThickness, &EditorOnly->FrontMaterial};
    for (const FExpressionInput* Root : Roots)
    {
        GatherReachableExpressions(Root ? Root->Expression : nullptr, OutReachable);
    }
    for (const FVector2MaterialInput& CustomizedUv :
         EditorOnly->CustomizedUVs)
    {
        GatherReachableExpressions(
            CustomizedUv.Expression, OutReachable);
    }
}

bool ValidateExactR31GraphInheritance(
    UMaterial* CandidateMaterial,
    UMaterial* ExactR31Master,
    UMaterialExpressionCustom* CandidateSurface,
    UMaterialExpressionCustom* ExactR31Surface,
    FString& OutError)
{
    UMaterialEditorOnlyData* CandidateEditor = CandidateMaterial
        ? CandidateMaterial->GetEditorOnlyData()
        : nullptr;
    UMaterialEditorOnlyData* ExactEditor = ExactR31Master
        ? ExactR31Master->GetEditorOnlyData()
        : nullptr;
    TMap<FString, UMaterialExpression*> CandidateNodes;
    TMap<FString, UMaterialExpression*> ExactNodes;
    if (!CandidateEditor || !ExactEditor || !CandidateSurface ||
        !ExactR31Surface ||
        !BuildExpressionMap(
            CandidateMaterial, true, CandidateNodes, OutError) ||
        !BuildExpressionMap(
            ExactR31Master, false, ExactNodes, OutError) ||
        CandidateNodes.Num() != ExpectedMasterExpressionCount ||
        ExactNodes.Num() != ExpectedMasterExpressionCount ||
        CandidateNodes.FindRef(ExactR31SurfaceDesc) != CandidateSurface ||
        ExactNodes.FindRef(ExactR31SurfaceDesc) != ExactR31Surface)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Composition master does not have the exact normalized 42-node R31 expression roster.");
        }
        return false;
    }

    const TSet<FName> NoAllowedPayloadDifferences;
    const TSet<FName> SurfaceAllowedPayloadDifferences = {
        FName(TEXT("Desc")), FName(TEXT("Description")),
        FName(TEXT("Code")), FName(TEXT("OutputType")),
        FName(TEXT("Inputs")), FName(TEXT("AdditionalOutputs")),
        FName(TEXT("Outputs")), FName(TEXT("bShowOutputNameOnPin"))};
    for (const TPair<FString, UMaterialExpression*>& Pair : ExactNodes)
    {
        UMaterialExpression* Candidate = CandidateNodes.FindRef(Pair.Key);
        UMaterialExpression* Exact = Pair.Value;
        if (!Candidate || !Exact ||
            Candidate->GetClass() != Exact->GetClass())
        {
            OutError = TEXT("Composition master substituted or removed exact R31 expression '") +
                Pair.Key + TEXT("'.");
            return false;
        }
        const bool bSurface = Pair.Key == ExactR31SurfaceDesc;
        if (!PersistentExpressionPayloadMatchesExcept(
                Candidate,
                Exact,
                bSurface
                    ? SurfaceAllowedPayloadDifferences
                    : NoAllowedPayloadDifferences,
                OutError))
        {
            return false;
        }
        if (bSurface)
        {
            if (!HasExactCandidateSurfaceOutputs(CandidateSurface) ||
                Candidate->CountInputs() !=
                    ExpectedCandidateSurfaceInputCount ||
                Exact->CountInputs() != ExpectedR31SurfaceInputCount)
            {
                OutError = TEXT("Composed Surface did not retain exactly 23 R31 inputs plus three explicit R31 presentation inputs and the exact seven-name unmasked output roster.");
                return false;
            }
            for (int32 Index = 0;
                 Index < ExpectedR31SurfaceInputCount;
                 ++Index)
            {
                const FExpressionInput* CandidateInput =
                    Candidate->GetInput(Index);
                const FExpressionInput* ExactInput = Exact->GetInput(Index);
                if (Candidate->GetInputName(Index) !=
                        Exact->GetInputName(Index) ||
                    Candidate->IsInputConnectionRequired(Index) !=
                        Exact->IsInputConnectionRequired(Index) ||
                    Candidate->GetInputType(Index) !=
                        Exact->GetInputType(Index) ||
                    !CandidateInput || !ExactInput ||
                    !ExpressionInputEdgeMatches(
                        *CandidateInput,
                        *ExactInput,
                        CandidateNodes,
                        ExactNodes))
                {
                    OutError = FString::Printf(
                        TEXT("Composed Surface mutated inherited R31 input edge %d."),
                        Index);
                    return false;
                }
            }
            continue;
        }
        if (Candidate->CountInputs() != Exact->CountInputs() ||
            !ExpressionOutputsMatch(Candidate, Exact))
        {
            OutError = TEXT("Composition master changed exact R31 input/output arity for expression '") +
                Pair.Key + TEXT("'.");
            return false;
        }
        for (int32 Index = 0; Index < Exact->CountInputs(); ++Index)
        {
            const FExpressionInput* CandidateInput =
                Candidate->GetInput(Index);
            const FExpressionInput* ExactInput = Exact->GetInput(Index);
            if (Candidate->GetInputName(Index) != Exact->GetInputName(Index) ||
                Candidate->IsInputConnectionRequired(Index) !=
                    Exact->IsInputConnectionRequired(Index) ||
                Candidate->GetInputType(Index) != Exact->GetInputType(Index) ||
                !CandidateInput || !ExactInput ||
                !ExpressionInputEdgeMatches(
                    *CandidateInput,
                    *ExactInput,
                    CandidateNodes,
                    ExactNodes))
            {
                OutError = FString::Printf(
                    TEXT("Composition master changed exact R31 input edge %d on expression '%s'."),
                    Index,
                    *Pair.Key);
                return false;
            }
        }
    }
    if (!ValidateExactUnchangedR31RootSelectors(
            CandidateEditor,
            ExactEditor,
            CandidateNodes,
            ExactNodes,
            OutError))
    {
        return false;
    }
    TSet<const UMaterialExpression*> CandidateReachable;
    TSet<const UMaterialExpression*> ExactReachable;
    GatherReachableFromMaterialRoots(CandidateEditor, CandidateReachable);
    GatherReachableFromMaterialRoots(ExactEditor, ExactReachable);
    if (CandidateReachable.Num() != CandidateNodes.Num() ||
        ExactReachable.Num() != ExactNodes.Num())
    {
        OutError = TEXT("Composition or exact R31 master contains an orphan expression outside the exact root-reachable node set.");
        return false;
    }
    for (const TPair<FString, UMaterialExpression*>& Pair : CandidateNodes)
    {
        if (!CandidateReachable.Contains(Pair.Value) ||
            !ExactReachable.Contains(ExactNodes.FindRef(Pair.Key)))
        {
            OutError = TEXT("Composition master reachable expression set differs from exact R31 at '") +
                Pair.Key + TEXT("'.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ApplyCandidateGraphToFreshClone(
    UMaterial* Material,
    const FAdmission& Admission,
    FString& OutError)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        Material->GetPathName() != MasterObjectPath ||
        EditorOnly->ExpressionCollection.Expressions.Num() !=
            ExpectedMasterExpressionCount)
    {
        OutError = TEXT("Fresh building-optics/contact composition master is not an exact isolated R31 graph clone.");
        return false;
    }
    UMaterialExpressionCustom* Surface =
        FindExactExpressionByDesc<UMaterialExpressionCustom>(
            Material,
            TEXT("R31.TexturedWeatheredR25DepthSurface"),
            OutError);
    UMaterialExpression* Normal =
        FindExactExpressionByDesc<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendFlatAndTextureNormal"), OutError);
    UMaterialExpression* Metallic =
        FindExactExpressionByDesc<UMaterialExpressionScalarParameter>(
            Material, TEXT("R31.Metallic"), OutError);
    UMaterialExpression* AmbientOcclusion =
        FindExactExpressionByDesc<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendAmbientOcclusion"), OutError);
    UMaterialExpression* RoughnessOutput =
        FindExactExpressionByDesc<UMaterialExpressionComponentMask>(
            Material, TEXT("R31.SurfaceRoughnessA"), OutError);
    if (!Surface || !Normal || !Metallic || !AmbientOcclusion ||
        !RoughnessOutput ||
        !InputMatches(EditorOnly->Roughness, RoughnessOutput, 0) ||
        Surface->OutputType != CMOT_Float4 ||
        !Surface->AdditionalOutputs.IsEmpty() ||
        Surface->Inputs.Num() != ExpectedR31SurfaceInputCount ||
        !HasExactR31SurfaceInputs(Surface, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Exact R31 surface node or its source output contract drifted before candidate splice.");
        }
        return false;
    }

    FString CandidateCode;
    if (!ComposeCandidateSurfaceCode(
            Surface->Code,
            Admission.OpticsMaterialIntentText,
            Admission.FootContactMaterialIntentText,
            CandidateCode,
            OutError) ||
        !HasExactCompositionOrderingAndPassThrough(CandidateCode))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Candidate code lost optics-first ordering or changed a protected optics pass-through channel during contact composition.");
        }
        return false;
    }

    Material->Modify();
    Surface->Modify();
    Surface->Desc = CandidateSurfaceDesc;
    Surface->Description = CandidateSurfaceDescription;
    Surface->Code = CandidateCode;
    Surface->OutputType = CMOT_Float4;
    Surface->AdditionalOutputs.Reset(ExpectedCandidateAdditionalOutputCount);
    for (int32 Index = 0;
         Index < ExpectedCandidateAdditionalOutputCount;
         ++Index)
    {
        FCustomOutput& Output = Surface->AdditionalOutputs.AddDefaulted_GetRef();
        Output.OutputName = CandidateAdditionalOutputNames[Index];
        Output.OutputType = CandidateAdditionalOutputTypes[Index];
    }
    struct FExtraInput
    {
        const TCHAR* Name;
        UMaterialExpression* Expression;
    };
    const FExtraInput ExtraInputs[] = {
        {TEXT("R31TangentNormalInput"), Normal},
        {TEXT("R31MetallicInput"), Metallic},
        {TEXT("R31AmbientOcclusionInput"), AmbientOcclusion}};
    static_assert(UE_ARRAY_COUNT(ExtraInputs) == 3);
    for (const FExtraInput& Extra : ExtraInputs)
    {
        FCustomInput& Input = Surface->Inputs.AddDefaulted_GetRef();
        Input.InputName = Extra.Name;
        Input.Input.Connect(0, Extra.Expression);
        Input.Input.InputName = NAME_None;
    }
    // Reproduce UE 5.5's non-exported RebuildOutputs implementation exactly;
    // direct calls from a plugin module would fail to link because the
    // MinimalAPI method itself is not ENGINE_API.
    Surface->Outputs.Reset(ExpectedCandidateSurfaceOutputCount);
    Surface->bShowOutputNameOnPin = true;
    Surface->Outputs.Add(FExpressionOutput(TEXT("return")));
    for (const FCustomOutput& Output : Surface->AdditionalOutputs)
    {
        Surface->Outputs.Add(FExpressionOutput(Output.OutputName));
    }

    Material->SetShadingModel(MSM_ClearCoat);
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bScreenSpaceReflections = false;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->bUsedWithNanite = true;
    Material->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif

    if (!ConnectCandidateProperty(
            Surface, FString(), MP_BaseColor, OutError) ||
        !ConnectCandidateProperty(
            Surface,
            CandidateAdditionalOutputNames[0],
            MP_Normal,
            OutError) ||
        !ConnectCandidateProperty(
            Surface,
            CandidateAdditionalOutputNames[2],
            MP_Metallic,
            OutError) ||
        !ConnectCandidateProperty(
            Surface,
            CandidateAdditionalOutputNames[3],
            MP_AmbientOcclusion,
            OutError) ||
        !ConnectCandidateProperty(
            Surface,
            CandidateAdditionalOutputNames[4],
            MP_CustomData0,
            OutError) ||
        !ConnectCandidateProperty(
            Surface,
            CandidateAdditionalOutputNames[5],
            MP_CustomData1,
            OutError))
    {
        return false;
    }

    // In UE's Clear Coat shading model the two custom-data properties are the
    // Clear Coat and Clear Coat Roughness material inputs. No generic custom
    // data or shading-model expression is introduced.
    if (!WriteCommonMetadata(Material, Admission, OutError))
    {
        return false;
    }
    UMetaData* Metadata = Material->GetOutermost()->GetMetaData();
    Metadata->SetValue(
        Material,
        TEXT("TRIAD_BuildingOpticsContactCompositionExactOpticsMasterFallback"),
        *OpticsMasterObjectPath);
    Metadata->SetValue(
        Material,
        TEXT("TRIAD_BuildingOpticsContactCompositionExactR31MasterFallback"),
        *TRIADIstanaExploreV5DR31BroadShellAssetFactory::
            GetMasterMaterialObjectPath());
    Metadata->SetValue(
        Material,
        TEXT("TRIAD_BuildingOpticsContactCompositionGeometryInvariant"),
        TEXT("43448_TRIANGLES_17_SLOTS_1388_GROUPS_IDENTITY_TRANSFORM_NO_MESH_BINDING"));

    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutError.Reset();
    return true;
}

bool HasNoPhysicalOrNaniteOverrideAuthority(const UMaterial* Material)
{
    if (!Material || Material->PhysMaterial || Material->PhysMaterialMask ||
        !Material->RenderTracePhysicalMaterialOutputs.IsEmpty() ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial())
    {
        return false;
    }
    for (const TObjectPtr<UPhysicalMaterial>& PhysicalMaterial :
         Material->PhysicalMaterialMap)
    {
        if (PhysicalMaterial)
        {
            return false;
        }
    }
    return true;
}

bool SameReferencedTextures(
    const UMaterialInterface* Left,
    const UMaterialInterface* Right)
{
    TSet<const UObject*> LeftSet;
    TSet<const UObject*> RightSet;
    if (!Left || !Right)
    {
        return false;
    }
    for (const TObjectPtr<UObject>& Object : Left->GetReferencedTextures())
    {
        if (Object)
        {
            LeftSet.Add(Object.Get());
        }
    }
    for (const TObjectPtr<UObject>& Object : Right->GetReferencedTextures())
    {
        if (Object)
        {
            RightSet.Add(Object.Get());
        }
    }
    return LeftSet.Num() > 0 && LeftSet.Num() == RightSet.Num() &&
        LeftSet.Difference(RightSet).Num() == 0 &&
        RightSet.Difference(LeftSet).Num() == 0;
}

bool ValidateCandidateMaster(
    UMaterial* Material,
    UMaterial* ExactR31Master,
    const FAdmission& Admission,
    bool bRequireSaved,
    FString& OutError)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UMaterialExpressionCustom* Surface =
        FindExactExpressionByDesc<UMaterialExpressionCustom>(
            Material, CandidateSurfaceDesc, OutError);
    UMaterialExpression* Normal =
        FindExactExpressionByDesc<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendFlatAndTextureNormal"), OutError);
    UMaterialExpression* Metallic =
        FindExactExpressionByDesc<UMaterialExpressionScalarParameter>(
            Material, TEXT("R31.Metallic"), OutError);
    UMaterialExpression* AmbientOcclusion =
        FindExactExpressionByDesc<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendAmbientOcclusion"), OutError);
    UMaterialExpression* RoughnessOutput =
        FindExactExpressionByDesc<UMaterialExpressionComponentMask>(
            Material, TEXT("R31.SurfaceRoughnessA"), OutError);
    UMaterialExpressionCustom* R31Surface =
        FindExactExpressionByDesc<UMaterialExpressionCustom>(
            ExactR31Master,
            ExactR31SurfaceDesc,
            OutError);
    FString ExpectedCode;
    if (!Surface || !R31Surface || !Normal || !Metallic ||
        !AmbientOcclusion || !RoughnessOutput ||
        !ComposeCandidateSurfaceCode(
            R31Surface->Code,
            Admission.OpticsMaterialIntentText,
            Admission.FootContactMaterialIntentText,
            ExpectedCode,
            OutError))
    {
        return false;
    }
    if (!ValidateExactR31GraphInheritance(
            Material,
            ExactR31Master,
            Surface,
            R31Surface,
            OutError))
    {
        return false;
    }
    if (!EditorOnly || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != MasterObjectPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bScreenSpaceReflections || Material->bEnableTessellation ||
        Material->bEnableDisplacementFade ||
        Material->MaxWorldPositionOffsetDisplacement != 0.0f ||
        !Material->bUsedWithNanite ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_ClearCoat) ||
        !HasNoPhysicalOrNaniteOverrideAuthority(Material) ||
        EditorOnly->ExpressionCollection.Expressions.Num() !=
            ExpectedMasterExpressionCount ||
        Surface->Description != CandidateSurfaceDescription ||
        Surface->Code != ExpectedCode || Surface->OutputType != CMOT_Float4 ||
        !HasExactCompositionOrderingAndPassThrough(Surface->Code) ||
        Surface->Inputs.Num() != ExpectedCandidateSurfaceInputCount ||
        Surface->AdditionalOutputs.Num() !=
            ExpectedCandidateAdditionalOutputCount ||
        !HasExactCandidateSurfaceOutputs(Surface) ||
        !HasExactR31SurfaceInputs(Surface, OutError) ||
        IsDeniedPresentationPropertyConnected(Material) ||
        !Material->HasBaseColorConnected() ||
        !Material->HasNormalConnected() ||
        !Material->HasRoughnessConnected() ||
        !Material->IsPropertyConnected(MP_Metallic) ||
        !Material->HasAmbientOcclusionConnected() ||
        !Material->IsPropertyConnected(MP_CustomData0) ||
        !Material->IsPropertyConnected(MP_CustomData1) ||
        !Material->IsPropertyConnected(MP_Specular) ||
        !InputMatches(EditorOnly->BaseColor, Surface, 0) ||
        !InputMatches(EditorOnly->Normal, Surface, 1) ||
        !InputMatches(EditorOnly->Roughness, RoughnessOutput, 0) ||
        !InputMatches(EditorOnly->Metallic, Surface, 3) ||
        !InputMatches(EditorOnly->AmbientOcclusion, Surface, 4) ||
        !InputMatches(EditorOnly->ClearCoat, Surface, 5) ||
        !InputMatches(EditorOnly->ClearCoatRoughness, Surface, 6) ||
        !SameReferencedTextures(Material, ExactR31Master) ||
        (bRequireSaved &&
         (!Material->GetOutermost() || Material->GetOutermost()->IsDirty() ||
          !FPackageName::DoesPackageExist(
              Material->GetOutermost()->GetName()))) ||
        Material->IsCompilingOrHadCompileError(GMaxRHIFeatureLevel))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Building-surface master lost its exact cloned R31 graph, candidate custom outputs, Clear Coat shading model, presentation-only routes, texture closure, save, or compile contract.");
        }
        return false;
    }
    for (int32 Index = 0;
         Index < ExpectedCandidateAdditionalOutputCount;
         ++Index)
    {
        if (Surface->AdditionalOutputs[Index].OutputName !=
                CandidateAdditionalOutputNames[Index] ||
            Surface->AdditionalOutputs[Index].OutputType !=
                CandidateAdditionalOutputTypes[Index])
        {
            OutError = TEXT("Building-surface custom additional-output roster drifted.");
            return false;
        }
    }
    if (Surface->Inputs[23].InputName != TEXT("R31TangentNormalInput") ||
        !InputExactlySelectsExpressionOutput(
            Surface->Inputs[23].Input, Normal, 0, NAME_None) ||
        Surface->Inputs[24].InputName != TEXT("R31MetallicInput") ||
        !InputExactlySelectsExpressionOutput(
            Surface->Inputs[24].Input, Metallic, 0, NAME_None) ||
        Surface->Inputs[25].InputName != TEXT("R31AmbientOcclusionInput") ||
        !InputExactlySelectsExpressionOutput(
            Surface->Inputs[25].Input,
            AmbientOcclusion,
            0,
            NAME_None))
    {
        OutError = TEXT("Building-surface custom node lost an exact inherited R31 normal, metallic, or AO name, edge, output index, InputName, or component-mask selector.");
        return false;
    }
    if (!ExactAssetMetadata(
            Material,
            TEXT("TRIAD_BuildingOpticsContactCompositionExactOpticsMasterFallback"),
            OpticsMasterObjectPath,
            OutError) ||
        !ExactAssetMetadata(
            Material,
            TEXT("TRIAD_BuildingOpticsContactCompositionExactR31MasterFallback"),
            TRIADIstanaExploreV5DR31BroadShellAssetFactory::
                GetMasterMaterialObjectPath(),
            OutError) ||
        !ExactAssetMetadata(
            Material,
            TEXT("TRIAD_BuildingOpticsContactCompositionGeometryInvariant"),
            TEXT("43448_TRIANGLES_17_SLOTS_1388_GROUPS_IDENTITY_TRANSFORM_NO_MESH_BINDING"),
            OutError) ||
        !ValidateCommonMetadata(Material, Admission, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool WriteSlotMetadata(
    UMaterialInstanceConstant* Instance,
    int32 SlotIndex,
    const FAdmission& Admission,
    FString& OutError)
{
    if (!Instance || SlotIndex < 0 || SlotIndex >= SlotCount ||
        !WriteCommonMetadata(Instance, Admission, OutError))
    {
        return false;
    }
    UMetaData* Metadata = Instance->GetOutermost()->GetMetaData();
    Metadata->SetValue(
        Instance,
        TEXT("TRIAD_BuildingOpticsContactCompositionSlotIndex"),
        *FString::FromInt(SlotIndex));
    Metadata->SetValue(
        Instance,
        TEXT("TRIAD_BuildingOpticsContactCompositionSlotName"),
        SlotNames[SlotIndex]);
    Metadata->SetValue(
        Instance,
        TEXT("TRIAD_BuildingOpticsContactCompositionExactOpticsFallback"),
        *OpticsFallbackObjectPath(SlotIndex));
    Metadata->SetValue(
        Instance,
        TEXT("TRIAD_BuildingOpticsContactCompositionExactR31Fallback"),
        R31FallbackPaths[SlotIndex]);
    return true;
}

bool ValidateSlotMetadata(
    const UMaterialInstanceConstant* Instance,
    int32 SlotIndex,
    const FAdmission& Admission,
    FString& OutError)
{
    return ValidateCommonMetadata(Instance, Admission, OutError) &&
        ExactAssetMetadata(
            Instance,
            TEXT("TRIAD_BuildingOpticsContactCompositionSlotIndex"),
            FString::FromInt(SlotIndex),
            OutError) &&
        ExactAssetMetadata(
            Instance,
            TEXT("TRIAD_BuildingOpticsContactCompositionSlotName"),
            SlotNames[SlotIndex],
            OutError) &&
        ExactAssetMetadata(
            Instance,
            TEXT("TRIAD_BuildingOpticsContactCompositionExactOpticsFallback"),
            OpticsFallbackObjectPath(SlotIndex),
            OutError) &&
        ExactAssetMetadata(
            Instance,
            TEXT("TRIAD_BuildingOpticsContactCompositionExactR31Fallback"),
            R31FallbackPaths[SlotIndex],
            OutError);
}

bool ValidateCandidateInstance(
    UMaterialInstanceConstant* Instance,
    UMaterialInstanceConstant* ExactOpticsFallback,
    UMaterialInstanceConstant* ExactR31Fallback,
    UMaterial* CandidateMaster,
    int32 SlotIndex,
    const FAdmission& Admission,
    bool bRequireSaved,
    FString& OutError)
{
    if (!Instance || !ExactOpticsFallback || !ExactR31Fallback ||
        !CandidateMaster ||
        SlotIndex < 0 || SlotIndex >= SlotCount ||
        Instance->GetClass() != UMaterialInstanceConstant::StaticClass() ||
        Instance->GetPathName() != CandidateObjectPath(SlotIndex) ||
        ExactOpticsFallback->GetPathName() !=
            OpticsFallbackObjectPath(SlotIndex) ||
        ExactR31Fallback->GetPathName() != R31FallbackPaths[SlotIndex] ||
        Instance->Parent != CandidateMaster ||
        !HasExactInstanceOverrideState(Instance, ExactOpticsFallback) ||
        !HasExactInstanceOverrideState(
            ExactOpticsFallback, ExactR31Fallback) ||
        !Instance->NaniteOverrideMaterial.bEnableOverride ||
        Instance->NaniteOverrideMaterial.GetOverrideMaterial() ||
        !HasNoInstancePhysicalAuthority(Instance) ||
        !Instance->GetShadingModels().HasOnlyShadingModel(MSM_ClearCoat) ||
        Instance->GetBlendMode() != BLEND_Opaque || Instance->IsTwoSided() ||
        !SameReferencedTextures(Instance, ExactOpticsFallback) ||
        !SameReferencedTextures(ExactOpticsFallback, ExactR31Fallback) ||
        (bRequireSaved &&
         (!Instance->GetOutermost() || Instance->GetOutermost()->IsDirty() ||
          !FPackageName::DoesPackageExist(
              Instance->GetOutermost()->GetName()))) ||
        !ValidateSlotMetadata(
            Instance, SlotIndex, Admission, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("Composition slot %d lost its exact path, optics/R31 override and texture equality, direct composed parent, Clear Coat, Nanite, no-physical-authority, metadata, or save contract."),
                SlotIndex);
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateOutputAssets(
    const FAdmission& Admission,
    bool bRequireSaved,
    TArray<UObject*>* OutAssets,
    FString& OutError)
{
    if (OutAssets)
    {
        OutAssets->Reset();
    }
    FString R31Report;
    if (!TRIADIstanaExploreV5DR31BroadShellAssetFactory::ValidateAssets(
            R31Report))
    {
        OutError = TEXT("Candidate output validation refused a drifted R31 predecessor: ") +
            R31Report;
        return false;
    }
    if (!ValidateExactOpticsFallbackAssets(OutError))
    {
        return false;
    }
    UMaterial* R31Master = LoadExact<UMaterial>(
        TRIADIstanaExploreV5DR31BroadShellAssetFactory::
            GetMasterMaterialObjectPath());
    UMaterial* Master = LoadExact<UMaterial>(MasterObjectPath);
    if (!R31Master ||
        !ValidateCandidateMaster(
            Master, R31Master, Admission, bRequireSaved, OutError))
    {
        return false;
    }
    if (OutAssets)
    {
        OutAssets->Add(Master);
    }
    TSet<const UObject*> UniqueInstances;
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        UMaterialInstanceConstant* Instance =
            LoadExact<UMaterialInstanceConstant>(CandidateObjectPath(Index));
        UMaterialInstanceConstant* OpticsFallback =
            LoadExact<UMaterialInstanceConstant>(
                OpticsFallbackObjectPath(Index));
        UMaterialInstanceConstant* R31Fallback =
            LoadExact<UMaterialInstanceConstant>(R31FallbackPaths[Index]);
        if (!ValidateCandidateInstance(
                Instance,
                OpticsFallback,
                R31Fallback,
                Master,
                Index,
                Admission,
                bRequireSaved,
                OutError) ||
            UniqueInstances.Contains(Instance))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Composition candidate instances are not seventeen unique ordered assets.");
            }
            return false;
        }
        UniqueInstances.Add(Instance);
        if (OutAssets)
        {
            OutAssets->Add(Instance);
        }
    }
    if (UniqueInstances.Num() != SlotCount ||
        (OutAssets && OutAssets->Num() != OutputAssetCount))
    {
        OutError = TEXT("Composition output roster is not exactly one master plus seventeen unique slot instances.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateFreshOutputAssets(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    TArray<UObject*>& OutAssets,
    FString& OutError)
{
    OutAssets.Reset();
    if (!ValidateExactOpticsFallbackAssets(OutError))
    {
        return false;
    }
    UMaterial* R31Master = LoadExact<UMaterial>(
        TRIADIstanaExploreV5DR31BroadShellAssetFactory::
            GetMasterMaterialObjectPath());
    UMaterial* CandidateMaster = R31Master
        ? Cast<UMaterial>(AssetTools.DuplicateAsset(
              MasterName, MaterialRoot, R31Master))
        : nullptr;
    if (!CandidateMaster || CandidateMaster->GetPathName() != MasterObjectPath ||
        !ApplyCandidateGraphToFreshClone(
            CandidateMaster, Admission, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not create the exact isolated R31 master clone.");
        }
        return false;
    }
    OutAssets.Add(CandidateMaster);

    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        UMaterialInstanceConstant* OpticsFallback =
            LoadExact<UMaterialInstanceConstant>(
                OpticsFallbackObjectPath(Index));
        UMaterialInstanceConstant* Candidate = OpticsFallback
            ? Cast<UMaterialInstanceConstant>(AssetTools.DuplicateAsset(
                  CandidateNames[Index], MaterialRoot, OpticsFallback))
            : nullptr;
        if (!Candidate ||
            Candidate->GetPathName() != CandidateObjectPath(Index))
        {
            OutError = FString::Printf(
                TEXT("Could not duplicate exact BuildingSurfaceOptics fallback for slot %d."),
                Index);
            return false;
        }
        Candidate->Modify();
        Candidate->SetParentEditorOnly(CandidateMaster, false);
        Candidate->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
        Candidate->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
        if (!WriteSlotMetadata(Candidate, Index, Admission, OutError))
        {
            return false;
        }
        Candidate->PostEditChange();
        Candidate->MarkPackageDirty();
        OutAssets.Add(Candidate);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    OutError.Reset();
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DBuildingOpticsContactCompositionEditorLibrary::
    InspectBuildingOpticsContactCompositionReceipts(
        const FString& CompositionCandidateRoot,
        const FString& OpticsCandidateRoot,
        const FString& FootContactCandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            CompositionCandidateRoot,
            OpticsCandidateRoot,
            FootContactCandidateRoot,
            AcceptedR33ReceiptPath,
            ExpectedAcceptedR33ReceiptSha256,
            FutureTransactionAuthorizationPath,
            ExpectedFutureTransactionAuthorizationSha256,
            false,
            Admission,
            Error))
    {
        OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_RECEIPT_INSPECTION_DENIED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_RECEIPTS_INSPECTED_ONLY executionAuthorized=false callerSuppliedHashesNeverAuthorizeExecution=true privateMaterializerInvoked=false sourceOnlyRead=true acceptedR33ReceiptHashMatchedCallerPinAndHumanAcceptedFields=true separateFutureAuthorizationHashMatchedCallerPinAndExactNarrowFieldRoster=true compositionOpticsAndFootContactContractsPlusBothMaterialIntentsAndAuditsHashPinned=true compositionOrder=R31_THEN_OPTICS_THEN_BOUNDED_CONTACT exactNativeR31PredecessorValidated=true assetsWritten=false mapOrComponentBindingModified=false exactOpticsAndR31FallbacksReplaced=false meshTopologyUvSlotRosterFootprintMassingTransformGeographyTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false UnrealLaunched=false nativeCompileMaterializationColdReloadCapturePerformanceOrVisualAcceptanceClaimed=false");
    return true;
}

bool UTRIADIstanaExploreV5DBuildingOpticsContactCompositionEditorLibrary::
    MaterializeTrustedBuildingOpticsContactCompositionAssetsInternal(
        const FString& CompositionCandidateRoot,
        const FString& OpticsCandidateRoot,
        const FString& FootContactCandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            CompositionCandidateRoot,
            OpticsCandidateRoot,
            FootContactCandidateRoot,
            AcceptedR33ReceiptPath,
            ExpectedAcceptedR33ReceiptSha256,
            FutureTransactionAuthorizationPath,
            ExpectedFutureTransactionAuthorizationSha256,
            true,
            Admission,
            Error))
    {
        OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_MATERIALIZATION_DENIED: ") +
            Error;
        return false;
    }

    TArray<FString> ExpectedPaths;
    GetOutputObjectPaths(ExpectedPaths);
    if (!NamespaceMatchesExpected(
            OutputRoot,
            ExpectedPaths,
            false,
            false,
            Error))
    {
        OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_UNEXPECTED_NAMESPACE_CONTENT_DENIED: ") +
            Error;
        return false;
    }
    const int32 ExistingCount = ExistingOutputPackageCount();
    if (ExistingCount == OutputAssetCount)
    {
        OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_EXISTING_NAMESPACE_REUSE_DENIED freshOnly=true existing=18");
        return false;
    }
    if (ExistingCount != 0)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_PARTIAL_NAMESPACE_DENIED existing=%d expectedEither=0_or_18"),
            ExistingCount);
        return false;
    }
    const TArray<FString> EmptyPaths;
    if (!NamespaceMatchesExpected(
            OutputRoot,
            EmptyPaths,
            true,
            false,
            Error))
    {
        OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_OUTPUT_NAMESPACE_NOT_EMPTY_DENIED: ") +
            Error;
        return false;
    }
    const bool bOutputRootProvenEmptyAtEntry = true;

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
            .Get();
    TArray<UObject*> AssetsToSave;
    if (!CreateFreshOutputAssets(
            AssetTools, Admission, AssetsToSave, Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_CREATE_FAILED: ") +
            Error;
        return false;
    }
    TSet<FString> UniquePaths;
    for (const UObject* Asset : AssetsToSave)
    {
        UniquePaths.Add(Asset ? Asset->GetPathName() : FString());
    }
    if (AssetsToSave.Num() != OutputAssetCount ||
        UniquePaths.Num() != OutputAssetCount ||
        UniquePaths.Contains(FString()) ||
        !NamespaceMatchesExpected(
            OutputRoot,
            ExpectedPaths,
            true,
            false,
            Error) ||
        !ValidateOutputAssets(
            Admission, false, nullptr, Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_PRE_SAVE_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem ||
        !AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        Error = TEXT("EXACT_EIGHTEEN_ASSET_SAVE_FAILED");
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_SAVE_FAILED: ") +
            Error;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!NamespaceMatchesExpected(
            OutputRoot,
            ExpectedPaths,
            true,
            true,
            Error) ||
        !ValidateOutputAssets(
            Admission, true, nullptr, Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_POST_SAVE_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_BUILDING_OPTICS_CONTACT_COMPOSITION_ASSETS_MATERIALIZED created=18 master=1 orderedSlotInstances=17 clonedExactR31MasterGraph=true clonedExactR31PerSlotOverridesAndTextures=true candidateCodeHashPinnedAndSpliced=true exactR31MaskAndPaneCellProvenance=true clearCoatAndClearCoatRoughnessConnectedToCompatibleClearCoatShadingModel=true exactR31FallbacksPreserved=true recursiveNamespaceAndPhysicalFilesExact=true freshFailureRollbackScope=isolatedNamespaceProvenEmptyAtEntry mapOrComponentBindingModified=false meshPackageTopologyVertexUvSlotRosterFootprintHeightSilhouetteTransformModified=false geographyCesiumTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false nativeUBTUHTCompileColdReloadRuntimeCaptureNaniteRasterParityPerformanceAndHumanVisualAcceptanceStillRequired=true");
    return true;
}
