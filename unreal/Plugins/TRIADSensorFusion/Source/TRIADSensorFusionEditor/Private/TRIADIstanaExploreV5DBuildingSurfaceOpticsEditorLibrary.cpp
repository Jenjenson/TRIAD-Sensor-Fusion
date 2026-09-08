#include "TRIADIstanaExploreV5DBuildingSurfaceOpticsEditorLibrary.h"

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
#include "TRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary.h"
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
constexpr int32 ExpectedCandidateSurfaceInputCount = 26;
constexpr int32 ExpectedCandidateAdditionalOutputCount = 6;
constexpr int64 MaximumInputBytes = 16ll * 1024ll * 1024ll;

const FString CandidateContractSha256(
    TEXT("3DDE345EADF05AAB69AEF4244220543F59E442FFA36571B66DC18D493246B3EE"));
const FString MaterialIntentSha256(
    TEXT("1F5639C8D85EDBBE47B5C9F93F76A43D4CA0E42CBDDB971A9694A2E8CDA3702E"));
const FString OfflineAuditSha256(
    TEXT("4161B0EB861CABFD8D23AC3980023D4DEC68995DC71C789C4EC9142276081AED"));
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
    TEXT("triad.istana_public_view_explore_v5d.building_surface_optics.future_transaction_authority.v1"));
const FString FutureOperation(
    TEXT("MATERIALIZE_BUILDING_SURFACE_OPTICS_ASSETS_ONLY_AND_EXPOSE_OPTIONAL_SEVENTEEN_SLOT_PRESENTATION_ROSTER"));
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingSurfaceOpticsIntegration"));
const FString MaterialRoot(OutputRoot + TEXT("/Materials"));
const FString MasterName(TEXT("M_IPV5D_BuildingSurfaceOptics_Master"));
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
const ECustomMaterialOutputType CandidateAdditionalOutputTypes[] = {
    CMOT_Float3, CMOT_Float1, CMOT_Float1,
    CMOT_Float1, CMOT_Float1, CMOT_Float1};

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
        OutError = TEXT("Building-surface source admission requires an absolute path, bounded byte count, and explicit SHA-256 pin.");
        return false;
    }
    const int64 ActualBytes = IFileManager::Get().FileSize(*AbsolutePath);
    if (ActualBytes != ExpectedBytes ||
        !FFileHelper::LoadFileToArray(OutBytes, *AbsolutePath) ||
        OutBytes.Num() != ActualBytes)
    {
        OutError = TEXT("A building-surface source file is absent or its exact byte count drifted.");
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
        OutError = TEXT("A building-surface source file failed its immutable SHA-256 pin.");
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
        OutError = TEXT("A hash-pinned building-surface JSON document could not be parsed.");
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
        OutError = TEXT("The building-surface candidate contract omitted a required boundary object.");
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
        OutError = TEXT("The building-surface candidate contract lost its exact source-only, geometry-preserving, R31-mask-derived, Clear Coat, or authority boundary.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateOfflineAudit(
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
        OutError = TEXT("The building-surface offline audit no longer proves its bounded source-lookdev geometry, contrast, or exact R31 pane-cell invariants.");
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
        OutError = TEXT("Future building-surface authorization must contain exactly the narrow trusted field roster and no extensions.");
        return false;
    }
    for (const TCHAR* Field : ExactFields)
    {
        if (!Root->HasField(Field))
        {
            OutError = TEXT("Future building-surface authorization omitted a field from the exact narrow roster.");
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
        OutError = TEXT("Future building-surface authorization is absent, unpinned, non-explicit, or broader than isolated optional material-set creation.");
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
    const FString Signature(TEXT("void TRIADBuildingSurfaceOpticsCandidate("));
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
        TEXT("R31PaneCellIndex"),
        TEXT("const float PaneSignal = frac(sin(dot("),
        TEXT("CandidateClearCoat ="),
        TEXT("CandidateClearCoatRoughness ="),
        TEXT("CandidateTangentNormal ="),
        TEXT("CandidateMetallic ="),
        TEXT("CandidateAmbientOcclusion ="),
        TEXT("const float WallSurfaceMask"),
        TEXT("const float RoofMicro")};
    for (const TCHAR* Token : RequiredTokens)
    {
        if (!OutBody.Contains(Token, ESearchCase::CaseSensitive))
        {
            OutError = TEXT("The candidate HLSL function body lost a required optical or exact-pane signal.");
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
        OutError = TEXT("Building-surface admission requires an absolute candidate root, two absolute receipt paths, and two explicit SHA-256 receipt pins.");
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
            OutError = TEXT("Building-surface execution is intentionally unreachable: compiled trusted receipt anchors are unset and require a separately reviewed source change plus recompile.");
            return false;
        }
        if (!ExpectedAcceptedR33ReceiptSha256.Equals(
                TrustedAcceptedR33ReceiptSha256,
                ESearchCase::IgnoreCase) ||
            !ExpectedFutureTransactionAuthorizationSha256.Equals(
                TrustedFutureAuthorizationSha256,
                ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Caller-supplied building-surface receipt hashes do not match the separately compiled trusted anchors.");
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
            TEXT("BuildingSurfaceOpticsCandidate") ||
        FPaths::IsSamePath(FullR33, FullAuthorization) ||
        ExpectedAcceptedR33ReceiptSha256.Equals(
            ExpectedFutureTransactionAuthorizationSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Building-surface admission requires the exact named candidate root and independent R33/authorization receipts.");
        return false;
    }

    TArray<uint8> IntentBytes;
    TSharedPtr<FJsonObject> CandidateContract;
    TSharedPtr<FJsonObject> OfflineAudit;
    if (!ParsePinnedJson(
            SourcePath(
                FullRoot,
                TEXT("building_surface_optics_candidate.contract.json")),
            8382,
            CandidateContractSha256,
            CandidateContract,
            OutError) ||
        !ParsePinnedJson(
            SourcePath(
                FullRoot,
                TEXT("OfflineAudit/building_surface_optics_audit.json")),
            11419,
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
            TEXT("building_surface_optics.material_intent.hlsl"))
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
        OutError = TEXT("A building-surface gate receipt is absent or outside the bounded input size.");
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
            OutError = TEXT("A building-surface output package has no exact .uasset filename.");
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
        OutError = TEXT("Saved building-surface namespace lacks the exact eighteen physical .uasset files or directory closure.");
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
        OutError = TEXT("Editor asset subsystem unavailable during building-surface rollback.");
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
            TEXT("Fresh building-surface rollback could not prove cleanup (DeleteLoadedAssets=%s DeleteDirectory=%s physicalRootAbsent=%s namespaceEmpty=%s)."),
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
        OutError = TEXT("Building-surface output requires writable package metadata.");
        return false;
    }
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingSurfaceOpticsCandidateContractSha256"),
        *CandidateContractSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingSurfaceOpticsMaterialIntentSha256"),
        *MaterialIntentSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingSurfaceOpticsOfflineAuditSha256"),
        *OfflineAuditSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingSurfaceOpticsR31ContractSha256"),
        *R31ContractSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingSurfaceOpticsAcceptedR33ReceiptSha256"),
        *Admission.AcceptedR33Sha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingSurfaceOpticsFutureAuthorizationSha256"),
        *Admission.FutureAuthorizationSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_BuildingSurfaceOpticsAuthority"),
        TEXT("APPEARANCE_ONLY_NO_MAP_MESH_TRANSFORM_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"));
    return ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsCandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsMaterialIntentSha256"),
               MaterialIntentSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsOfflineAuditSha256"),
               OfflineAuditSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsR31ContractSha256"),
               R31ContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsAuthority"),
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
               TEXT("TRIAD_BuildingSurfaceOpticsCandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsMaterialIntentSha256"),
               MaterialIntentSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsOfflineAuditSha256"),
               OfflineAuditSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsR31ContractSha256"),
               R31ContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_BuildingSurfaceOpticsAuthority"),
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
    const FString Aliases(
        TEXT("// Exact post-R33 source-only BuildingSurfaceOpticsCandidate splice.\n")
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
        TEXT("float CameraDistanceMetres = distanceCm * 0.01;\n"));
    OutCode = R31SurfaceCode.LeftChop(R31Return.Len()) + Aliases +
        CandidateBody +
        TEXT("\nreturn CandidateBaseColor;");
    if (!OutCode.Contains(
            TEXT("float2 R31PaneCellIndex = cellIndex;"),
            ESearchCase::CaseSensitive) ||
        OutCode.Contains(TEXT("3.0, 3.2"), ESearchCase::CaseSensitive) ||
        OutCode.EndsWith(R31Return, ESearchCase::CaseSensitive))
    {
        OutError = TEXT("Candidate surface composition did not preserve the exact R31 aperture-cell identity boundary.");
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
    TEXT("BSO.ExactR31SurfacePlusCandidateOptics"));
const FString CandidateSurfaceDescription(
    TEXT("TRIAD_IPV5D_UNNUMBERED_POST_R33_EXACT_R31_MASKS_PANE_CELL_BUILDING_SURFACE_OPTICS"));

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
        OutError = TEXT("Fresh building-surface master is not an exact isolated R31 graph clone.");
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
    if (!Surface || !Normal || !Metallic || !AmbientOcclusion ||
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
    Surface->OutputType = CMOT_Float3;
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
    }
    // Reproduce UE 5.5's non-exported RebuildOutputs implementation exactly;
    // direct calls from a plugin module would fail to link because the
    // MinimalAPI method itself is not ENGINE_API.
    Surface->Outputs.Reset(ExpectedCandidateAdditionalOutputCount + 1);
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
            CandidateAdditionalOutputNames[1],
            MP_Roughness,
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
        TEXT("TRIAD_BuildingSurfaceOpticsExactR31MasterFallback"),
        *TRIADIstanaExploreV5DR31BroadShellAssetFactory::
            GetMasterMaterialObjectPath());
    Metadata->SetValue(
        Material,
        TEXT("TRIAD_BuildingSurfaceOpticsGeometryInvariant"),
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
    UMaterialExpressionCustom* R31Surface =
        FindExactExpressionByDesc<UMaterialExpressionCustom>(
            ExactR31Master,
            TEXT("R31.TexturedWeatheredR25DepthSurface"),
            OutError);
    FString ExpectedCode;
    if (!Surface || !R31Surface || !Normal || !Metallic ||
        !AmbientOcclusion ||
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
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_ClearCoat) ||
        !HasNoPhysicalOrNaniteOverrideAuthority(Material) ||
        EditorOnly->ExpressionCollection.Expressions.Num() !=
            ExpectedMasterExpressionCount ||
        Surface->Description != CandidateSurfaceDescription ||
        Surface->Code != ExpectedCode || Surface->OutputType != CMOT_Float3 ||
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
        !Material->IsPropertyConnected(MP_CustomData0) ||
        !Material->IsPropertyConnected(MP_CustomData1) ||
        !Material->IsPropertyConnected(MP_Specular) ||
        !InputMatches(EditorOnly->BaseColor, Surface, 0) ||
        !InputMatches(EditorOnly->Normal, Surface, 1) ||
        !InputMatches(EditorOnly->Roughness, Surface, 2) ||
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
        !InputMatches(Surface->Inputs[23].Input, Normal, 0) ||
        Surface->Inputs[24].InputName != TEXT("R31MetallicInput") ||
        !InputMatches(Surface->Inputs[24].Input, Metallic, 0) ||
        Surface->Inputs[25].InputName != TEXT("R31AmbientOcclusionInput") ||
        !InputMatches(
            Surface->Inputs[25].Input, AmbientOcclusion, 0))
    {
        OutError = TEXT("Building-surface custom node lost exact inherited R31 normal, metallic, or AO edges.");
        return false;
    }
    if (!ExactAssetMetadata(
            Material,
            TEXT("TRIAD_BuildingSurfaceOpticsExactR31MasterFallback"),
            TRIADIstanaExploreV5DR31BroadShellAssetFactory::
                GetMasterMaterialObjectPath(),
            OutError) ||
        !ExactAssetMetadata(
            Material,
            TEXT("TRIAD_BuildingSurfaceOpticsGeometryInvariant"),
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
        TEXT("TRIAD_BuildingSurfaceOpticsSlotIndex"),
        *FString::FromInt(SlotIndex));
    Metadata->SetValue(
        Instance,
        TEXT("TRIAD_BuildingSurfaceOpticsSlotName"),
        SlotNames[SlotIndex]);
    Metadata->SetValue(
        Instance,
        TEXT("TRIAD_BuildingSurfaceOpticsExactR31Fallback"),
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
            TEXT("TRIAD_BuildingSurfaceOpticsSlotIndex"),
            FString::FromInt(SlotIndex),
            OutError) &&
        ExactAssetMetadata(
            Instance,
            TEXT("TRIAD_BuildingSurfaceOpticsSlotName"),
            SlotNames[SlotIndex],
            OutError) &&
        ExactAssetMetadata(
            Instance,
            TEXT("TRIAD_BuildingSurfaceOpticsExactR31Fallback"),
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
        !Instance->GetShadingModels().HasOnlyShadingModel(MSM_ClearCoat) ||
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
                TEXT("Building-surface slot %d lost its exact path, cloned R31 overrides/textures, direct candidate parent, Clear Coat, Nanite, no-physical-authority, metadata, or save contract."),
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
                OutError = TEXT("Building-surface candidate instances are not seventeen unique ordered assets.");
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
        OutError = TEXT("Building-surface output roster is not exactly one master plus seventeen unique slot instances.");
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

bool UTRIADIstanaExploreV5DBuildingSurfaceOpticsEditorLibrary::
    InspectBuildingSurfaceOpticsReceipts(
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
        OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_RECEIPT_INSPECTION_DENIED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_RECEIPTS_INSPECTED_ONLY executionAuthorized=false callerSuppliedHashesNeverAuthorizeExecution=true privateMaterializerInvoked=false sourceOnlyRead=true acceptedR33ReceiptHashMatchedCallerPinAndHumanAcceptedFields=true separateFutureAuthorizationHashMatchedCallerPinAndExactNarrowFieldRoster=true candidateContractMaterialIntentOfflineAuditBuilderAndComparisonHashPinned=true exactNativeR31PredecessorValidated=true assetsWritten=false mapOrComponentBindingModified=false exactR31FallbacksReplaced=false meshTopologyUvSlotRosterTransformGeographyTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false UnrealLaunched=false nativeCompileMaterializationColdReloadCapturePerformanceOrVisualAcceptanceClaimed=false");
    return true;
}

bool UTRIADIstanaExploreV5DBuildingSurfaceOpticsEditorLibrary::
    MaterializeTrustedBuildingSurfaceOpticsAssetsInternal(
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
        OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_MATERIALIZATION_DENIED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_UNEXPECTED_NAMESPACE_CONTENT_DENIED: ") +
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
                Error) ||
            !ValidateOutputAssets(
                Admission, true, nullptr, Error))
        {
            OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_EXISTING_ASSETS_INVALID: ") +
                Error;
            return false;
        }
        OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_ASSETS_VALID existing=18 created=0 master=1 orderedSlotInstances=17 exactR31GraphClone=true exactR31MaskAndPaneCellProvenance=true clearCoatShadingModel=true exactR31FallbacksPreserved=true recursiveNamespaceExact=true mapOrComponentBindingModified=false meshTopologyUvSlotRosterTransformGeographyTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false nativeColdReloadRuntimeCapturePerformanceOrHumanVisualAcceptanceClaimed=false");
        return true;
    }
    if (ExistingCount != 0)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_BUILDING_SURFACE_OPTICS_PARTIAL_NAMESPACE_DENIED existing=%d expectedEither=0_or_18"),
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
        OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_OUTPUT_NAMESPACE_NOT_EMPTY_DENIED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_CREATE_FAILED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_PRE_SAVE_VALIDATION_FAILED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_SAVE_FAILED: ") +
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
        OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_POST_SAVE_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_BUILDING_SURFACE_OPTICS_ASSETS_MATERIALIZED created=18 master=1 orderedSlotInstances=17 clonedExactR31MasterGraph=true clonedExactR31PerSlotOverridesAndTextures=true candidateCodeHashPinnedAndSpliced=true exactR31MaskAndPaneCellProvenance=true clearCoatAndClearCoatRoughnessConnectedToCompatibleClearCoatShadingModel=true exactR31FallbacksPreserved=true recursiveNamespaceAndPhysicalFilesExact=true freshFailureRollbackScope=isolatedNamespaceProvenEmptyAtEntry mapOrComponentBindingModified=false meshPackageTopologyVertexUvSlotRosterFootprintHeightSilhouetteTransformModified=false geographyCesiumTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false nativeUBTUHTCompileColdReloadRuntimeCaptureNaniteRasterParityPerformanceAndHumanVisualAcceptanceStillRequired=true");
    return true;
}
