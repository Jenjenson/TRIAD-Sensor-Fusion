#include "TRIADIstanaExploreV5DBuildingFootContactEditorLibrary.h"

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
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHIFeatureLevel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Ssl.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DBuildingFootContactLibrary.h"
#include "TRIADIstanaExploreV5DR31BroadShellAssetFactory.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
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
constexpr int32 ExpectedCandidateSurfaceInputCount = 24;
constexpr int32 ExpectedCandidateAdditionalOutputCount = 1;
constexpr int64 MaximumInputBytes = 16ll * 1024ll * 1024ll;

const FString CandidateContractSha256(
    TEXT("81BB2A8F81A5C0DEF753898255335C5DAF57209704197297AECCA927DAF7B63E"));
const FString MaterialIntentSha256(
    TEXT("C396D13A2B1509868DA2EC528354043038AA2631B2299BFD896AD682008FEB77"));
const FString OfflineAuditSha256(
    TEXT("0D94CBEE0FFDF62B1C588E621D1183AAE6EB3A6E763131BC94E2287F7C4A4D1C"));
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
    TEXT("triad.istana_public_view_explore_v5d.building_foot_contact.future_transaction_authority.v1"));
const FString FutureOperation(
    TEXT("MATERIALIZE_BUILDING_FOOT_CONTACT_ASSETS_ONLY_AND_EXPOSE_OPTIONAL_SEVENTEEN_SLOT_PRESENTATION_ROSTER"));
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingFootContactIntegration"));
const FString MaterialRoot(OutputRoot + TEXT("/Materials"));
const FString MasterName(TEXT("M_IPV5D_BuildingFootContact_Master"));
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
    TEXT("MI_IPV5D_BFC_00_MAT_BOTTOM_HIDDEN"),
    TEXT("MI_IPV5D_BFC_01_MAT_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BFC_02_MAT_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BFC_03_MAT_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BFC_04_MAT_HOTEL_HINT"),
    TEXT("MI_IPV5D_BFC_05_MAT_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BFC_06_MAT_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BFC_07_MAT_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BFC_08_MAT_ROOF_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BFC_09_MAT_ROOF_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BFC_10_MAT_ROOF_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BFC_11_MAT_ROOF_HOTEL_HINT"),
    TEXT("MI_IPV5D_BFC_12_MAT_ROOF_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BFC_13_MAT_ROOF_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BFC_14_MAT_ROOF_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BFC_15_MAT_ROOF_TRANSPORT_HINT"),
    TEXT("MI_IPV5D_BFC_16_MAT_TRANSPORT_HINT")};

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
    TEXT("ContactAmbientOcclusionOutput")};
const ECustomMaterialOutputType CandidateAdditionalOutputTypes[] = {
    CMOT_Float1};

static_assert(UE_ARRAY_COUNT(SlotNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(CandidateNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(R31FallbackPaths) == SlotCount);
static_assert(
    UE_ARRAY_COUNT(R31SurfaceInputNames) == ExpectedR31SurfaceInputCount);
static_assert(
    UE_ARRAY_COUNT(CandidateAdditionalOutputNames) ==
        ExpectedCandidateAdditionalOutputCount);
static_assert(
    UE_ARRAY_COUNT(CandidateAdditionalOutputTypes) ==
        ExpectedCandidateAdditionalOutputCount);

struct FSourcePin
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourcePin SourcePins[] = {
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
    FString CandidateRoot;
    FString AcceptedR33Sha256;
    FString FutureAuthorizationSha256;
    FString MaterialIntentText;
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
        OutError = TEXT("Building-foot-contact source admission requires an absolute path, bounded byte count, and explicit SHA-256 pin.");
        return false;
    }
    const int64 ActualBytes = IFileManager::Get().FileSize(*AbsolutePath);
    if (ActualBytes != ExpectedBytes ||
        !FFileHelper::LoadFileToArray(OutBytes, *AbsolutePath) ||
        OutBytes.Num() != ActualBytes)
    {
        OutError = TEXT("A building-foot-contact source file is absent or its exact byte count drifted.");
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
        OutError = TEXT("A building-foot-contact source file failed its immutable SHA-256 pin.");
        return false;
    }
#else
    OutError = TEXT("Building-foot-contact source admission requires WITH_SSL SHA-256 support.");
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
        OutError = TEXT("A hash-pinned building-foot-contact JSON document could not be parsed.");
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

bool ValidateCandidateContract(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Facts = nullptr;
    const TSharedPtr<FJsonObject>* Response = nullptr;
    const TSharedPtr<FJsonObject>* Authority = nullptr;
    const TSharedPtr<FJsonObject>* Preservation = nullptr;
    const TSharedPtr<FJsonObject>* Integration = nullptr;
    const TCHAR* const FalseAuthorityFields[] = {
        TEXT("asBuiltOrCurrentSiteTruth"), TEXT("collision"),
        TEXT("geospatial"), TEXT("lineOfSight"), TEXT("navigation"),
        TEXT("physicalMaterial"), TEXT("rf"), TEXT("sensor"),
        TEXT("surveyOrVerticalDatum"), TEXT("terrain")};
    const TCHAR* const FalsePreservationFields[] = {
        TEXT("buildingFootprintMutation"), TEXT("buildingHeightMutation"),
        TEXT("buildingSilhouetteMutation"),
        TEXT("componentTransformMutation"), TEXT("meshIndexMutation"),
        TEXT("meshPositionMutation"), TEXT("meshTopologyMutation"),
        TEXT("opacityOrCoverageMutation"), TEXT("pixelDepthOffsetMutation"),
        TEXT("terrainHeightOrTransformMutation"), TEXT("uvMutation"),
        TEXT("worldPositionOffsetMutation")};

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
        ExactInteger(*Facts, TEXT("sourceVertexRecordCount"), 24522) &&
        ExactInteger(*Facts, TEXT("sourceUvRecordCount"), 130632) &&
        ExactInteger(*Facts, TEXT("referencedSourceUvRecordCount"), 130344) &&
        ExactInteger(*Facts, TEXT("unreferencedSourceUvRecordCount"), 288) &&
        ExactInteger(*Facts, TEXT("sourceTriangleCount"), 43448) &&
        ExactInteger(*Facts, TEXT("wallTriangleCount"), 24360) &&
        ExactInteger(*Facts, TEXT("roofOrHiddenBottomTriangleCount"), 19088) &&
        ExactInteger(*Facts, TEXT("materialSlotCount"), SlotCount) &&
        ExactInteger(*Facts, TEXT("retainedGroupCount"), 1388) &&
        ExactInteger(*Facts, TEXT("groundPlaneGroupCount"), 1370) &&
        ExactInteger(*Facts, TEXT("groundPlaneWallTriangleCount"), 23746) &&
        ExactInteger(*Facts, TEXT("elevatedGroupCount"), 18) &&
        ExactInteger(*Facts, TEXT("elevatedWallTriangleCount"), 614) &&
        ExactNumber(*Facts, TEXT("minimumElevatedGroupBaseMetres"), 3.2) &&
        ExactBool(
            *Facts,
            TEXT("wallUvVEqualsSourceAbsoluteZExactly"),
            true) &&
        Root->TryGetObjectField(TEXT("materialResponse"), Response) &&
        Response && Response->IsValid() &&
        ExactNumber(
            *Response, TEXT("fullWeightThroughSourceZMetres"), 0.1) &&
        ExactNumber(
            *Response, TEXT("zeroWeightAtAndAboveSourceZMetres"), 1.25) &&
        ExactNumber(
            *Response, TEXT("baseColorDarkeningMaximum"), 0.08) &&
        ExactNumber(
            *Response, TEXT("roughnessDeltaMaximum"), 0.05) &&
        ExactNumber(
            *Response, TEXT("ambientOcclusionDarkeningMaximum"), 0.07) &&
        ExactBool(
            *Response, TEXT("existingGlassMaskExcludesGlazing"), true) &&
        ExactBool(
            *Response, TEXT("wallVerticalWeatherMaskRequired"), true) &&
        ExactBool(
            *Response, TEXT("worldXyBreakupIsContinuous"), true) &&
        ExactBool(
            *Response, TEXT("hardWorldGridOrBuildingSelector"), false) &&
        Root->TryGetObjectField(TEXT("authorityBoundary"), Authority) &&
        Authority && Authority->IsValid() &&
        Root->TryGetObjectField(TEXT("preservationBoundary"), Preservation) &&
        Preservation && Preservation->IsValid() &&
        Root->TryGetObjectField(
            TEXT("nativeIntegrationBoundary"), Integration) &&
        Integration && Integration->IsValid() &&
        ExactBool(
            *Integration, TEXT("acceptedR33ReceiptRequired"), true) &&
        ExactBool(
            *Integration, TEXT("humanAcceptedMatchedCaptureRequired"), true) &&
        ExactBool(
            *Integration, TEXT("separateSuccessorAuthorizationRequired"), true) &&
        ExactBool(
            *Integration,
            TEXT("futureImplementationMayOnlyCloneMaterialGraph"),
            true) &&
        ExactBool(
            *Integration, TEXT("mapOrComponentBindingPresent"), false) &&
        ExactBool(
            *Integration,
            TEXT("sourcePlaneAlignmentMustBeRejectedIfItDoesNotMatchVisibleFeet"),
            true);

    if (!Authority || !Authority->IsValid() ||
        !Preservation || !Preservation->IsValid())
    {
        OutError = TEXT("The building-foot-contact candidate omitted a required authority or preservation boundary.");
        return false;
    }
    for (const TCHAR* Field : FalseAuthorityFields)
    {
        bValid = bValid && ExactBool(*Authority, Field, false);
    }
    for (const TCHAR* Field : FalsePreservationFields)
    {
        bValid = bValid && ExactBool(*Preservation, Field, false);
    }
    if (!bValid)
    {
        OutError = TEXT("The building-foot-contact candidate lost its exact source-plane, bounded material-response, geometry-preserving, or no-authority contract.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateOfflineAudit(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* SourceAudit = nullptr;
    const TSharedPtr<FJsonObject>* Response = nullptr;
    const TSharedPtr<FJsonObject>* Authority = nullptr;
    const TSharedPtr<FJsonObject>* Preservation = nullptr;
    const TCHAR* const FalseAuthorityFields[] = {
        TEXT("asBuiltOrCurrentSiteTruth"), TEXT("collision"),
        TEXT("geospatial"), TEXT("lineOfSight"), TEXT("navigation"),
        TEXT("physicalMaterial"), TEXT("rf"), TEXT("sensor"),
        TEXT("surveyOrVerticalDatum"), TEXT("terrain")};
    const TCHAR* const FalsePreservationFields[] = {
        TEXT("buildingFootprintMutation"), TEXT("buildingHeightMutation"),
        TEXT("buildingSilhouetteMutation"),
        TEXT("componentTransformMutation"), TEXT("meshIndexMutation"),
        TEXT("meshPositionMutation"), TEXT("meshTopologyMutation"),
        TEXT("opacityOrCoverageMutation"), TEXT("pixelDepthOffsetMutation"),
        TEXT("terrainHeightOrTransformMutation"), TEXT("uvMutation"),
        TEXT("worldPositionOffsetMutation")};

    bool bValid =
        ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.istana.building_foot_contact_source_audit.v1")) &&
        ExactString(
            Root,
            TEXT("status"),
            TEXT("POST_R33_UNNUMBERED_UNADMITTED_SOURCE_ONLY_CANDIDATE")) &&
        ExactBool(Root, TEXT("nativeProjectApplied"), false) &&
        ExactBool(Root, TEXT("unrealLaunched"), false) &&
        ExactBool(Root, TEXT("visualAcceptance"), false) &&
        Root->TryGetObjectField(TEXT("sourceAudit"), SourceAudit) &&
        SourceAudit && SourceAudit->IsValid() &&
        ExactInteger(*SourceAudit, TEXT("vertexRecords"), 24522) &&
        ExactInteger(*SourceAudit, TEXT("textureCoordinateRecords"), 130632) &&
        ExactInteger(
            *SourceAudit, TEXT("referencedTextureCoordinateRecords"), 130344) &&
        ExactInteger(
            *SourceAudit, TEXT("unreferencedTextureCoordinateRecords"), 288) &&
        ExactInteger(*SourceAudit, TEXT("triangles"), 43448) &&
        ExactInteger(*SourceAudit, TEXT("wallTriangles"), 24360) &&
        ExactInteger(
            *SourceAudit, TEXT("roofOrHiddenBottomTriangles"), 19088) &&
        ExactInteger(*SourceAudit, TEXT("materialSlots"), SlotCount) &&
        ExactInteger(*SourceAudit, TEXT("retainedGroups"), 1388) &&
        ExactInteger(*SourceAudit, TEXT("groundPlaneGroups"), 1370) &&
        ExactInteger(*SourceAudit, TEXT("groundPlaneWallTriangles"), 23746) &&
        ExactInteger(*SourceAudit, TEXT("elevatedGroups"), 18) &&
        ExactInteger(*SourceAudit, TEXT("elevatedWallTriangles"), 614) &&
        ExactNumber(
            *SourceAudit, TEXT("minimumElevatedGroupBaseMetres"), 3.2) &&
        ExactNumber(
            *SourceAudit, TEXT("maximumWallVAbsoluteZErrorMetres"), 0.0) &&
        ExactBool(
            *SourceAudit, TEXT("rawGroupIdentifiersPersistedInAudit"), false) &&
        Root->TryGetObjectField(TEXT("response"), Response) &&
        Response && Response->IsValid() &&
        ExactNumber(
            *Response, TEXT("fullWeightThroughSourceZMetres"), 0.1) &&
        ExactNumber(
            *Response, TEXT("zeroWeightAtAndAboveSourceZMetres"), 1.25) &&
        ExactNumber(*Response, TEXT("maximumBaseColorDarkening"), 0.08) &&
        ExactNumber(*Response, TEXT("maximumRoughnessDelta"), 0.05) &&
        ExactNumber(*Response, TEXT("maximumAoDarkening"), 0.07) &&
        Root->TryGetObjectField(TEXT("authorityBoundary"), Authority) &&
        Authority && Authority->IsValid() &&
        Root->TryGetObjectField(TEXT("preservation"), Preservation) &&
        Preservation && Preservation->IsValid();

    if (!Authority || !Authority->IsValid() ||
        !Preservation || !Preservation->IsValid())
    {
        OutError = TEXT("The building-foot-contact audit omitted a required authority or preservation boundary.");
        return false;
    }
    for (const TCHAR* Field : FalseAuthorityFields)
    {
        bValid = bValid && ExactBool(*Authority, Field, false);
    }
    for (const TCHAR* Field : FalsePreservationFields)
    {
        bValid = bValid && ExactBool(*Preservation, Field, false);
    }
    if (!bValid)
    {
        OutError = TEXT("The building-foot-contact audit no longer proves the exact shell facts, bounded response, source-only status, or zero mutation boundary.");
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
        TEXT("CandidateContractSha256"), TEXT("MaterialIntentSha256"),
        TEXT("OfflineAuditSha256"), TEXT("R31ContractSha256"),
        TEXT("ExactMaterialSlotCount"), TEXT("ExactOutputAssetCount"),
        TEXT("OutputNamespace"), TEXT("UnnumberedSuccessor"),
        TEXT("ExplicitExecutionAuthorized"), TEXT("AssetsOnlyEndpoint"),
        TEXT("OptionalPresentationMaterialSetOnly"),
        TEXT("ExactR31FallbacksPreserved"), TEXT("MapMutationAuthorized"),
        TEXT("SourceMeshMutationAuthorized"),
        TEXT("SourceMaterialMutationAuthorized"),
        TEXT("SourceTransformMutationAuthorized"),
        TEXT("GeographyTerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"),
        TEXT("NativeWriteOrUnrealLaunchPerformed")};
    if (!Root.IsValid() ||
        Root->Values.Num() != UE_ARRAY_COUNT(ExactFields))
    {
        OutError = TEXT("Future building-foot-contact authorization must contain exactly the narrow trusted field roster and no extensions.");
        return false;
    }
    for (const TCHAR* Field : ExactFields)
    {
        if (!Root->HasField(Field))
        {
            OutError = TEXT("Future building-foot-contact authorization omitted a field from the exact narrow roster.");
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
            Root, TEXT("CandidateContractSha256"), CandidateContractSha256) ||
        !ExactString(
            Root, TEXT("MaterialIntentSha256"), MaterialIntentSha256) ||
        !ExactString(Root, TEXT("OfflineAuditSha256"), OfflineAuditSha256) ||
        !ExactString(Root, TEXT("R31ContractSha256"), R31ContractSha256) ||
        !ExactInteger(Root, TEXT("ExactMaterialSlotCount"), SlotCount) ||
        !ExactInteger(Root, TEXT("ExactOutputAssetCount"), OutputAssetCount) ||
        !ExactString(Root, TEXT("OutputNamespace"), OutputRoot) ||
        !ExactBool(Root, TEXT("UnnumberedSuccessor"), true) ||
        !ExactBool(Root, TEXT("ExplicitExecutionAuthorized"), true) ||
        !ExactBool(Root, TEXT("AssetsOnlyEndpoint"), true) ||
        !ExactBool(
            Root, TEXT("OptionalPresentationMaterialSetOnly"), true) ||
        !ExactBool(Root, TEXT("ExactR31FallbacksPreserved"), true) ||
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
        OutError = TEXT("Future building-foot-contact authorization is absent, unpinned, non-explicit, or broader than isolated optional material-set creation.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ExtractCandidateFunctionBody(
    const FString& MaterialIntentText,
    FString& OutBody,
    FString& OutError)
{
    const FString Signature(TEXT("void TRIADBuildingFootContactResponse("));
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
    const TCHAR* const RequiredTokens[] = {
        TEXT("const float SourcePlaneEnvelope = 1.0 - smoothstep("),
        TEXT("const float BroadSignal = 0.5 + 0.5 * sin("),
        TEXT("const float BreakupGain = lerp(0.82, 1.00, BroadSignal);"),
        TEXT("const float OpaqueWallMask = saturate(WallVerticalWeatherMask)"),
        TEXT("(1.0 - saturate(ExistingGlassMask))"),
        TEXT("ContactBaseColor = ExistingBaseColor * lerp(1.0, 0.92, ContactWeight);"),
        TEXT("ContactRoughness = saturate(ExistingRoughness + 0.05 * ContactWeight);"),
        TEXT("ExistingAmbientOcclusion * (1.0 - 0.07 * ContactWeight)")};
    for (const TCHAR* Token : RequiredTokens)
    {
        if (!OutBody.Contains(Token, ESearchCase::CaseSensitive))
        {
            OutError = TEXT("The candidate HLSL function body lost a required source-plane contact signal.");
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
        OutError = TEXT("Building-foot-contact admission requires an absolute candidate root, two absolute receipt paths, and two explicit SHA-256 receipt pins.");
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
            OutError = TEXT("Building-foot-contact execution is intentionally unreachable: compiled trusted receipt anchors are unset and require a separately reviewed source change plus recompile.");
            return false;
        }
        if (!ExpectedAcceptedR33ReceiptSha256.Equals(
                TrustedAcceptedR33ReceiptSha256,
                ESearchCase::IgnoreCase) ||
            !ExpectedFutureTransactionAuthorizationSha256.Equals(
                TrustedFutureAuthorizationSha256,
                ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Caller-supplied building-foot-contact receipt hashes do not match the separately compiled trusted anchors.");
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
    if (FPaths::GetCleanFilename(FullRoot) !=
            TEXT("BuildingFootContactCandidate") ||
        FPaths::IsSamePath(FullR33, FullAuthorization) ||
        ExpectedAcceptedR33ReceiptSha256.Equals(
            ExpectedFutureTransactionAuthorizationSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Building-foot-contact admission requires the exact named candidate root and independent R33/authorization receipts.");
        return false;
    }

    TArray<uint8> IntentBytes;
    TSharedPtr<FJsonObject> CandidateContract;
    TSharedPtr<FJsonObject> OfflineAudit;
    if (!ParsePinnedJson(
            SourcePath(
                FullRoot,
                TEXT("building_foot_contact_candidate.contract.v1.json")),
            4823,
            CandidateContractSha256,
            CandidateContract,
            OutError) ||
        !ParsePinnedJson(
            SourcePath(
                FullRoot,
                TEXT("Generated/building_foot_contact_source_audit.v1.json")),
            6462,
            OfflineAuditSha256,
            OfflineAudit,
            OutError) ||
        !ValidateCandidateContract(CandidateContract, OutError) ||
        !ValidateOfflineAudit(OfflineAudit, OutError))
    {
        return false;
    }

    for (const FSourcePin& Pin : SourcePins)
    {
        TArray<uint8> Bytes;
        if (!LoadPinnedBytes(
                SourcePath(FullRoot, Pin.RelativePath),
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
            IntentBytes = MoveTemp(Bytes);
        }
    }
    FFileHelper::BufferToString(
        OutAdmission.MaterialIntentText,
        IntentBytes.GetData(),
        IntentBytes.Num());
    FString CandidateBody;
    if (OutAdmission.MaterialIntentText.IsEmpty() ||
        !ExtractCandidateFunctionBody(
            OutAdmission.MaterialIntentText, CandidateBody, OutError))
    {
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
        OutError = TEXT("A building-foot-contact gate receipt is absent or outside the bounded input size.");
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
        OutError = TEXT("Building-foot-contact namespace has no resolvable package-root filesystem path.");
        return false;
    }
    FPaths::NormalizeDirectoryName(OutPhysicalRoot);
    OutPhysicalDirectories.Add(OutPhysicalRoot);
    for (const FString& ObjectPath : ExpectedObjectPaths)
    {
        if (ObjectPath.IsEmpty() || OutObjects.Contains(ObjectPath))
        {
            OutError = TEXT("Building-foot-contact expected object roster contains an empty or duplicate path.");
            return false;
        }
        OutObjects.Add(ObjectPath);
        const FString PackagePath =
            FPackageName::ObjectPathToPackageName(ObjectPath);
        if (!IsPackageWithinNamespace(PackagePath, NamespaceRoot) ||
            OutPackages.Contains(PackagePath))
        {
            OutError = TEXT("Building-foot-contact expected package roster escaped the isolated namespace or duplicated a package.");
            return false;
        }
        OutPackages.Add(PackagePath);
        FString PhysicalFile;
        if (!FPackageName::TryConvertLongPackageNameToFilename(
                PackagePath,
                PhysicalFile,
                FPackageName::GetAssetPackageExtension()))
        {
            OutError = TEXT("A building-foot-contact output package has no exact .uasset filename.");
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
        OutError = TEXT("Building-foot-contact physical namespace resolution drifted.");
        return false;
    }
    for (const FString& ObjectPath : Actual.ObjectPaths)
    {
        if (!ExpectedObjects.Contains(ObjectPath))
        {
            OutError = TEXT("Building-foot-contact namespace contains an uncontracted asset: ") +
                ObjectPath;
            return false;
        }
    }
    for (const FString& PackagePath : Actual.PackagePaths)
    {
        if (!ExpectedPackages.Contains(PackagePath))
        {
            OutError = TEXT("Building-foot-contact namespace contains an uncontracted loaded or physical package: ") +
                PackagePath;
            return false;
        }
    }
    for (const FString& PhysicalFile : Actual.PhysicalFiles)
    {
        if (!ExpectedPhysicalFiles.Contains(PhysicalFile))
        {
            OutError = TEXT("Building-foot-contact namespace contains an uncontracted physical file: ") +
                PhysicalFile;
            return false;
        }
    }
    for (const FString& PhysicalDirectory : Actual.PhysicalDirectories)
    {
        if (!ExpectedPhysicalDirectories.Contains(PhysicalDirectory))
        {
            OutError = TEXT("Building-foot-contact namespace contains an uncontracted physical directory: ") +
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
        OutError = TEXT("Building-foot-contact namespace does not contain the exact eighteen-object/package roster.");
        return false;
    }
    if (bRequirePhysicalAssetFiles &&
        (Actual.PhysicalFiles.Num() != ExpectedPhysicalFiles.Num() ||
         Actual.PhysicalFiles.Difference(ExpectedPhysicalFiles).Num() != 0 ||
         ExpectedPhysicalFiles.Difference(Actual.PhysicalFiles).Num() != 0 ||
         Actual.PhysicalDirectories.Difference(ExpectedPhysicalDirectories).Num() != 0 ||
         ExpectedPhysicalDirectories.Difference(Actual.PhysicalDirectories).Num() != 0))
    {
        OutError = TEXT("Saved building-foot-contact namespace lacks the exact eighteen physical .uasset files or directory closure.");
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
        OutError = TEXT("Editor asset subsystem unavailable during building-foot-contact rollback.");
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
            TEXT("Fresh building-foot-contact rollback could not prove cleanup (DeleteLoadedAssets=%s DeleteDirectory=%s physicalRootAbsent=%s namespaceEmpty=%s)."),
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
            TEXT("Building-foot-contact asset '%s' lost exact metadata '%s'."),
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
        OutError = TEXT("Building-foot-contact output requires writable package metadata.");
        return false;
    }
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingFootContactCandidateContractSha256"),
        *CandidateContractSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingFootContactMaterialIntentSha256"),
        *MaterialIntentSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingFootContactOfflineAuditSha256"),
        *OfflineAuditSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingFootContactR31ContractSha256"),
        *R31ContractSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingFootContactAcceptedR33ReceiptSha256"),
        *Admission.AcceptedR33Sha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingFootContactFutureAuthorizationSha256"),
        *Admission.FutureAuthorizationSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingFootContactAuthority"),
        TEXT("APPEARANCE_ONLY_NO_MAP_MESH_TRANSFORM_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"));
    return ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactCandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactMaterialIntentSha256"),
               MaterialIntentSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactOfflineAuditSha256"),
               OfflineAuditSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactR31ContractSha256"),
               R31ContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactAuthority"),
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
               TEXT("TRIAD_BuildingFootContactCandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactMaterialIntentSha256"),
               MaterialIntentSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactOfflineAuditSha256"),
               OfflineAuditSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactR31ContractSha256"),
               R31ContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingFootContactAuthority"),
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
    const FString& MaterialIntentText,
    FString& OutCode,
    FString& OutError)
{
    const FString R31Return(
        TEXT("return float4(finalColor, saturate(finalRoughness));"));
    FString CandidateBody;
    if (!R31SurfaceCode.EndsWith(R31Return, ESearchCase::CaseSensitive) ||
        !ExtractCandidateFunctionBody(
            MaterialIntentText, CandidateBody, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The validated R31 surface no longer ends at its exact return boundary.");
        }
        return false;
    }
    if (CandidateBody.ReplaceInline(
            TEXT("WallVerticalWeatherMask"),
            TEXT("R31WallVerticalWeatherMask"),
            ESearchCase::CaseSensitive) != 1)
    {
        OutError = TEXT("The pinned contact body no longer has exactly one wall-mask use to bind to R31.");
        return false;
    }
    const FString Aliases(
        TEXT("// Exact post-R33 source-only BuildingFootContactCandidate splice.\n")
        TEXT("float3 ExistingBaseColor = finalColor;\n")
        TEXT("float ExistingRoughness = saturate(finalRoughness);\n")
        TEXT("float ExistingAmbientOcclusion = saturate(R31AmbientOcclusionInput);\n")
        TEXT("float ExistingGlassMask = visibleGlass;\n")
        TEXT("float R31WallVerticalWeatherMask = wallMask;\n")
        TEXT("float SourceAbsoluteZMetres = sourceAbsoluteZMetres;\n")
        TEXT("float2 AbsoluteWorldPositionMetresXY = WorldPositionCm.xy * 0.01;\n")
        TEXT("float3 ContactBaseColor = ExistingBaseColor;\n")
        TEXT("float ContactRoughness = ExistingRoughness;\n")
        TEXT("float ContactAmbientOcclusion = ExistingAmbientOcclusion;\n")
        TEXT("float ContactWeight = 0.0;\n"));
    OutCode = R31SurfaceCode.LeftChop(R31Return.Len()) + Aliases +
        CandidateBody +
        TEXT("\nContactAmbientOcclusionOutput = ContactAmbientOcclusion;\n")
        TEXT("return float4(ContactBaseColor, ContactRoughness);");
    if (!OutCode.Contains(
            TEXT("float SourceAbsoluteZMetres = sourceAbsoluteZMetres;"),
            ESearchCase::CaseSensitive) ||
        !OutCode.Contains(
            TEXT("float ExistingGlassMask = visibleGlass;"),
            ESearchCase::CaseSensitive) ||
        !OutCode.Contains(
            TEXT("ContactAmbientOcclusionOutput = ContactAmbientOcclusion;"),
            ESearchCase::CaseSensitive) ||
        OutCode.EndsWith(R31Return, ESearchCase::CaseSensitive))
    {
        OutError = TEXT("Candidate surface composition did not preserve the exact R31 source-Z, glass-mask, or AO edge.");
        return false;
    }
    OutError.Reset();
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

bool IsDeniedPresentationPropertyConnected(const UMaterial* Material)
{
    const EMaterialProperty Denied[] = {
        MP_EmissiveColor, MP_Opacity, MP_OpacityMask, MP_Anisotropy,
        MP_Tangent, MP_WorldPositionOffset, MP_SubsurfaceColor,
        MP_Refraction, MP_CustomizedUVs0,
        MP_CustomizedUVs1, MP_CustomizedUVs2, MP_CustomizedUVs3,
        MP_CustomizedUVs4, MP_CustomizedUVs5, MP_CustomizedUVs6,
        MP_CustomizedUVs7, MP_PixelDepthOffset, MP_ShadingModel,
        MP_CustomData0, MP_CustomData1,
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

const FString CandidateSurfaceDesc(
    TEXT("BFC.ExactR31SurfacePlusSourcePlaneContact"));
const FString CandidateSurfaceDescription(
    TEXT("TRIAD_IPV5D_UNNUMBERED_POST_R33_EXACT_R31_SOURCE_Z_GLASS_MASK_BUILDING_FOOT_CONTACT"));

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
        OutError = TEXT("Fresh building-foot-contact master is not an exact isolated R31 graph clone.");
        return false;
    }
    UMaterialExpressionCustom* Surface =
        FindExactExpressionByDesc<UMaterialExpressionCustom>(
            Material,
            TEXT("R31.TexturedWeatheredR25DepthSurface"),
            OutError);
    UMaterialExpressionComponentMask* RoughnessOutput =
        FindExactExpressionByDesc<UMaterialExpressionComponentMask>(
            Material, TEXT("R31.SurfaceRoughnessA"), OutError);
    UMaterialExpression* Normal =
        FindExactExpressionByDesc<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendFlatAndTextureNormal"), OutError);
    UMaterialExpression* Metallic =
        FindExactExpressionByDesc<UMaterialExpressionScalarParameter>(
            Material, TEXT("R31.Metallic"), OutError);
    UMaterialExpression* Specular =
        FindExactExpressionByDesc<UMaterialExpressionScalarParameter>(
            Material, TEXT("R31.Specular"), OutError);
    UMaterialExpression* AmbientOcclusion =
        FindExactExpressionByDesc<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendAmbientOcclusion"), OutError);
    if (!Surface || !RoughnessOutput || !Normal || !Metallic || !Specular ||
        !AmbientOcclusion ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->bTangentSpaceNormal ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        Surface->OutputType != CMOT_Float4 ||
        !Surface->AdditionalOutputs.IsEmpty() ||
        Surface->Inputs.Num() != ExpectedR31SurfaceInputCount ||
        !HasExactR31SurfaceInputs(Surface, OutError) ||
        !InputMatches(EditorOnly->BaseColor, Surface, 0) ||
        !InputMatches(EditorOnly->Normal, Normal, 0) ||
        !InputMatches(EditorOnly->Roughness, RoughnessOutput, 0) ||
        !InputMatches(RoughnessOutput->Input, Surface, 0) ||
        !InputMatches(EditorOnly->Metallic, Metallic, 0) ||
        !InputMatches(EditorOnly->Specular, Specular, 0) ||
        !InputMatches(EditorOnly->AmbientOcclusion, AmbientOcclusion, 0))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Exact R31 surface node, Default Lit state, or inherited presentation edges drifted before contact splice.");
        }
        return false;
    }

    FString CandidateCode;
    if (!ComposeCandidateSurfaceCode(
            Surface->Code,
            Admission.MaterialIntentText,
            CandidateCode,
            OutError))
    {
        return false;
    }

    Material->Modify();
    Surface->Modify();
    Surface->Desc = CandidateSurfaceDesc;
    Surface->Description = CandidateSurfaceDescription;
    Surface->Code = CandidateCode;
    Surface->OutputType = CMOT_Float4;
    Surface->AdditionalOutputs.Reset(ExpectedCandidateAdditionalOutputCount);
    FCustomOutput& Output = Surface->AdditionalOutputs.AddDefaulted_GetRef();
    Output.OutputName = CandidateAdditionalOutputNames[0];
    Output.OutputType = CandidateAdditionalOutputTypes[0];

    FCustomInput& Input = Surface->Inputs.AddDefaulted_GetRef();
    Input.InputName = TEXT("R31AmbientOcclusionInput");
    Input.Input.Connect(0, AmbientOcclusion);

    // Reproduce UE 5.5's non-exported RebuildOutputs implementation exactly;
    // direct calls from a plugin module would fail to link because the
    // MinimalAPI method itself is not ENGINE_API.
    Surface->Outputs.Reset(ExpectedCandidateAdditionalOutputCount + 1);
    Surface->bShowOutputNameOnPin = true;
    Surface->Outputs.Add(FExpressionOutput(TEXT("return")));
    Surface->Outputs.Add(FExpressionOutput(Output.OutputName));

    if (!ConnectCandidateProperty(
            Surface,
            CandidateAdditionalOutputNames[0],
            MP_AmbientOcclusion,
            OutError))
    {
        return false;
    }

    // Default Lit, BaseColor, Roughness, Normal, Metallic and Specular remain
    // the exact inherited R31 state and edges. Only AO is rerouted through the
    // bounded source-plane contact response.
    if (!WriteCommonMetadata(Material, Admission, OutError))
    {
        return false;
    }
    UMetaData* Metadata = Material->GetOutermost()->GetMetaData();
    Metadata->SetValue(
        Material,
        TEXT("TRIAD_BuildingFootContactExactR31MasterFallback"),
        *TRIADIstanaExploreV5DR31BroadShellAssetFactory::
            GetMasterMaterialObjectPath());
    Metadata->SetValue(
        Material,
        TEXT("TRIAD_BuildingFootContactGeometryInvariant"),
        TEXT("43448_TRIANGLES_17_SLOTS_1388_GROUPS_IDENTITY_TRANSFORM_NO_MESH_BINDING"));
    Metadata->SetValue(
        Material,
        TEXT("TRIAD_BuildingFootContactResponseInvariant"),
        TEXT("SOURCE_Z_0P10_FULL_1P25_ZERO_MAX_COLOR_0P08_ROUGHNESS_0P05_AO_0P07_GLASS_EXCLUDED"));

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
    UMaterialExpressionComponentMask* RoughnessOutput =
        FindExactExpressionByDesc<UMaterialExpressionComponentMask>(
            Material, TEXT("R31.SurfaceRoughnessA"), OutError);
    UMaterialExpression* Normal =
        FindExactExpressionByDesc<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendFlatAndTextureNormal"), OutError);
    UMaterialExpression* Metallic =
        FindExactExpressionByDesc<UMaterialExpressionScalarParameter>(
            Material, TEXT("R31.Metallic"), OutError);
    UMaterialExpression* Specular =
        FindExactExpressionByDesc<UMaterialExpressionScalarParameter>(
            Material, TEXT("R31.Specular"), OutError);
    UMaterialExpression* AmbientOcclusion =
        FindExactExpressionByDesc<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendAmbientOcclusion"), OutError);
    UMaterialExpressionCustom* R31Surface =
        FindExactExpressionByDesc<UMaterialExpressionCustom>(
            ExactR31Master,
            TEXT("R31.TexturedWeatheredR25DepthSurface"),
            OutError);
    FString ExpectedCode;
    if (!Surface || !R31Surface || !RoughnessOutput || !Normal ||
        !Metallic || !Specular || !AmbientOcclusion ||
        !ComposeCandidateSurfaceCode(
            R31Surface->Code,
            Admission.MaterialIntentText,
            ExpectedCode,
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
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !HasNoPhysicalOrNaniteOverrideAuthority(Material) ||
        EditorOnly->ExpressionCollection.Expressions.Num() !=
            ExpectedMasterExpressionCount ||
        Surface->Description != CandidateSurfaceDescription ||
        Surface->Code != ExpectedCode || Surface->OutputType != CMOT_Float4 ||
        Surface->Inputs.Num() != ExpectedCandidateSurfaceInputCount ||
        Surface->AdditionalOutputs.Num() !=
            ExpectedCandidateAdditionalOutputCount ||
        Surface->Outputs.Num() !=
            ExpectedCandidateAdditionalOutputCount + 1 ||
        !HasExactR31SurfaceInputs(Surface, OutError) ||
        IsDeniedPresentationPropertyConnected(Material) ||
        !Material->HasBaseColorConnected() ||
        !Material->HasNormalConnected() ||
        !Material->HasRoughnessConnected() ||
        !Material->IsPropertyConnected(MP_Metallic) ||
        !Material->HasAmbientOcclusionConnected() ||
        !Material->IsPropertyConnected(MP_Specular) ||
        !InputMatches(EditorOnly->BaseColor, Surface, 0) ||
        !InputMatches(EditorOnly->Normal, Normal, 0) ||
        !InputMatches(EditorOnly->Roughness, RoughnessOutput, 0) ||
        !InputMatches(RoughnessOutput->Input, Surface, 0) ||
        !InputMatches(EditorOnly->Metallic, Metallic, 0) ||
        !InputMatches(EditorOnly->AmbientOcclusion, Surface, 1) ||
        !InputMatches(EditorOnly->Specular, Specular, 0) ||
        !SameReferencedTextures(Material, ExactR31Master) ||
        (bRequireSaved &&
         (!Material->GetOutermost() || Material->GetOutermost()->IsDirty() ||
          !FPackageName::DoesPackageExist(
              Material->GetOutermost()->GetName()))) ||
        Material->IsCompilingOrHadCompileError(GMaxRHIFeatureLevel))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Building-foot-contact master lost its exact R31 clone, bounded Default Lit contact splice, presentation-only edges, texture closure, save, or compile contract.");
        }
        return false;
    }
    if (Surface->AdditionalOutputs[0].OutputName !=
            CandidateAdditionalOutputNames[0] ||
        Surface->AdditionalOutputs[0].OutputType !=
            CandidateAdditionalOutputTypes[0] ||
        Surface->Inputs[23].InputName !=
            TEXT("R31AmbientOcclusionInput") ||
        !InputMatches(
            Surface->Inputs[23].Input, AmbientOcclusion, 0))
    {
        OutError = TEXT("Building-foot-contact custom node lost its exact inherited R31 AO input or sole AO output.");
        return false;
    }
    if (!ExactAssetMetadata(
            Material,
            TEXT("TRIAD_BuildingFootContactExactR31MasterFallback"),
            TRIADIstanaExploreV5DR31BroadShellAssetFactory::
                GetMasterMaterialObjectPath(),
            OutError) ||
        !ExactAssetMetadata(
            Material,
            TEXT("TRIAD_BuildingFootContactGeometryInvariant"),
            TEXT("43448_TRIANGLES_17_SLOTS_1388_GROUPS_IDENTITY_TRANSFORM_NO_MESH_BINDING"),
            OutError) ||
        !ExactAssetMetadata(
            Material,
            TEXT("TRIAD_BuildingFootContactResponseInvariant"),
            TEXT("SOURCE_Z_0P10_FULL_1P25_ZERO_MAX_COLOR_0P08_ROUGHNESS_0P05_AO_0P07_GLASS_EXCLUDED"),
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
        TEXT("TRIAD_BuildingFootContactSlotIndex"),
        *FString::FromInt(SlotIndex));
    Metadata->SetValue(
        Instance,
        TEXT("TRIAD_BuildingFootContactSlotName"),
        SlotNames[SlotIndex]);
    Metadata->SetValue(
        Instance,
        TEXT("TRIAD_BuildingFootContactExactR31Fallback"),
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
            TEXT("TRIAD_BuildingFootContactSlotIndex"),
            FString::FromInt(SlotIndex),
            OutError) &&
        ExactAssetMetadata(
            Instance,
            TEXT("TRIAD_BuildingFootContactSlotName"),
            SlotNames[SlotIndex],
            OutError) &&
        ExactAssetMetadata(
            Instance,
            TEXT("TRIAD_BuildingFootContactExactR31Fallback"),
            R31FallbackPaths[SlotIndex],
            OutError);
}

bool HasNoInstancePhysicalAuthority(
    const UMaterialInstanceConstant* Instance)
{
    if (!Instance || Instance->PhysMaterial)
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

bool ValidateCandidateInstance(
    UMaterialInstanceConstant* Instance,
    UMaterialInstanceConstant* ExactR31Fallback,
    UMaterial* CandidateMaster,
    int32 SlotIndex,
    const FAdmission& Admission,
    bool bRequireSaved,
    FString& OutError)
{
    if (!Instance || !ExactR31Fallback || !CandidateMaster ||
        SlotIndex < 0 || SlotIndex >= SlotCount ||
        Instance->GetClass() != UMaterialInstanceConstant::StaticClass() ||
        Instance->GetPathName() != CandidateObjectPath(SlotIndex) ||
        ExactR31Fallback->GetPathName() != R31FallbackPaths[SlotIndex] ||
        Instance->Parent != CandidateMaster ||
        Instance->ScalarParameterValues !=
            ExactR31Fallback->ScalarParameterValues ||
        Instance->VectorParameterValues !=
            ExactR31Fallback->VectorParameterValues ||
        Instance->DoubleVectorParameterValues !=
            ExactR31Fallback->DoubleVectorParameterValues ||
        Instance->TextureParameterValues !=
            ExactR31Fallback->TextureParameterValues ||
        Instance->TextureCollectionParameterValues !=
            ExactR31Fallback->TextureCollectionParameterValues ||
        Instance->RuntimeVirtualTextureParameterValues !=
            ExactR31Fallback->RuntimeVirtualTextureParameterValues ||
        Instance->SparseVolumeTextureParameterValues !=
            ExactR31Fallback->SparseVolumeTextureParameterValues ||
        Instance->FontParameterValues !=
            ExactR31Fallback->FontParameterValues ||
        !Instance->UserSceneTextureOverrides.IsEmpty() ||
        !ExactR31Fallback->UserSceneTextureOverrides.IsEmpty() ||
        !Instance->GetStaticParameters().Equivalent(
            ExactR31Fallback->GetStaticParameters()) ||
        !(Instance->BasePropertyOverrides ==
          ExactR31Fallback->BasePropertyOverrides) ||
        !Instance->NaniteOverrideMaterial.bEnableOverride ||
        Instance->NaniteOverrideMaterial.GetOverrideMaterial() ||
        !HasNoInstancePhysicalAuthority(Instance) ||
        !Instance->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        Instance->GetBlendMode() != BLEND_Opaque || Instance->IsTwoSided() ||
        !SameReferencedTextures(Instance, ExactR31Fallback) ||
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
                TEXT("Building-foot-contact slot %d lost its exact path, cloned R31 overrides/textures, direct candidate parent, Default Lit state, Nanite, no-physical-authority, metadata, or save contract."),
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
        UMaterialInstanceConstant* Fallback =
            LoadExact<UMaterialInstanceConstant>(R31FallbackPaths[Index]);
        if (!ValidateCandidateInstance(
                Instance,
                Fallback,
                Master,
                Index,
                Admission,
                bRequireSaved,
                OutError) ||
            UniqueInstances.Contains(Instance))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("Building-foot-contact candidate instances are not seventeen unique ordered assets.");
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
        OutError = TEXT("Building-foot-contact output roster is not exactly one master plus seventeen unique slot instances.");
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
        UMaterialInstanceConstant* Fallback =
            LoadExact<UMaterialInstanceConstant>(R31FallbackPaths[Index]);
        UMaterialInstanceConstant* Candidate = Fallback
            ? Cast<UMaterialInstanceConstant>(AssetTools.DuplicateAsset(
                  CandidateNames[Index], MaterialRoot, Fallback))
            : nullptr;
        if (!Candidate ||
            Candidate->GetPathName() != CandidateObjectPath(Index))
        {
            OutError = FString::Printf(
                TEXT("Could not duplicate exact R31 fallback for slot %d."),
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

bool UTRIADIstanaExploreV5DBuildingFootContactEditorLibrary::
    InspectBuildingFootContactReceipts(
        const FString& CandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            CandidateRoot,
            AcceptedR33ReceiptPath,
            ExpectedAcceptedR33ReceiptSha256,
            FutureTransactionAuthorizationPath,
            ExpectedFutureTransactionAuthorizationSha256,
            false,
            Admission,
            Error))
    {
        OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_RECEIPT_INSPECTION_DENIED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_RECEIPTS_INSPECTED_ONLY executionAuthorized=false callerSuppliedHashesNeverAuthorizeExecution=true privateMaterializerInvoked=false sourceOnlyRead=true acceptedR33ReceiptHashMatchedCallerPinAndHumanAcceptedFields=true separateFutureAuthorizationHashMatchedCallerPinAndExactNarrowFieldRoster=true candidateContractMaterialIntentGeneratedAuditAndBuilderHashPinned=true exactNativeR31PredecessorValidated=true assetsWritten=false mapOrComponentBindingModified=false exactR31FallbacksReplaced=false meshTopologyUvSlotRosterTransformGeographyTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false UnrealLaunched=false nativeCompileMaterializationColdReloadCapturePerformanceOrVisualAcceptanceClaimed=false");
    return true;
}

bool UTRIADIstanaExploreV5DBuildingFootContactEditorLibrary::
    MaterializeTrustedBuildingFootContactAssetsInternal(
        const FString& CandidateRoot,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            CandidateRoot,
            AcceptedR33ReceiptPath,
            ExpectedAcceptedR33ReceiptSha256,
            FutureTransactionAuthorizationPath,
            ExpectedFutureTransactionAuthorizationSha256,
            true,
            Admission,
            Error))
    {
        OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_MATERIALIZATION_DENIED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_UNEXPECTED_NAMESPACE_CONTENT_DENIED: ") +
            Error;
        return false;
    }
    const int32 ExistingCount = ExistingOutputPackageCount();
    if (ExistingCount == OutputAssetCount)
    {
        if (!NamespaceMatchesExpected(
                OutputRoot,
                ExpectedPaths,
                true,
                true,
                Error))
        {
            OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_EXISTING_ASSETS_INVALID: ") +
                Error;
            return false;
        }
        // Saved assets cannot prove every inherited R31 node property and edge
        // from path/metadata alone. This dormant materializer is deliberately
        // fresh-only, so a same-path graph cannot be reused as trusted input.
        OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_EXISTING_NAMESPACE_REUSE_DENIED existing=18 freshOnly=true reviewedMigrationOrRemovalRequired=true noExistingAssetAcceptedAsExactR31Clone=true");
        return false;
    }
    if (ExistingCount != 0)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_BUILDING_FOOT_CONTACT_PARTIAL_NAMESPACE_DENIED existing=%d expectedEither=0_or_18"),
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
        OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_OUTPUT_NAMESPACE_NOT_EMPTY_DENIED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_CREATE_FAILED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_PRE_SAVE_VALIDATION_FAILED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_SAVE_FAILED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_POST_SAVE_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_BUILDING_FOOT_CONTACT_ASSETS_MATERIALIZED created=18 master=1 orderedSlotInstances=17 clonedExactR31MasterGraph=true clonedExactR31PerSlotOverridesAndTextures=true candidateCodeHashPinnedAndSpliced=true exactR31SourceZWallAndGlassMaskProvenance=true defaultLitAndExactR31NormalMetallicSpecularEdgesPreserved=true exactR31FallbacksPreserved=true recursiveNamespaceAndPhysicalFilesExact=true freshFailureRollbackScope=isolatedNamespaceProvenEmptyAtEntry mapOrComponentBindingModified=false meshPackageTopologyVertexUvSlotRosterFootprintHeightSilhouetteTransformModified=false geographyCesiumTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false nativeUBTUHTCompileColdReloadRuntimeCaptureNaniteRasterParityPerformanceAndHumanVisualAcceptanceStillRequired=true");
    return true;
}
