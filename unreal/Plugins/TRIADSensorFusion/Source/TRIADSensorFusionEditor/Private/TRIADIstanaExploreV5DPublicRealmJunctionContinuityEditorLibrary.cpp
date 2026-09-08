#include "TRIADIstanaExploreV5DPublicRealmJunctionContinuityEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Ssl.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary.h"
#include "TRIADIstanaExploreV5DPublicRealmAssetFactory.h"
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
constexpr int32 CandidateAssetCount = 2;
constexpr int32 ClusterCount = 60;
constexpr int32 NormalOverrideCount = 355;
constexpr int32 CoreNormalOverrideCount = 174;
constexpr int32 FallbackNormalOverrideCount = 181;
constexpr int32 CoreVertexRecordCount = 1332;
constexpr int32 CoreTriangleCount = 444;
constexpr int32 CoreMaterialCount = 1;
constexpr int32 FallbackVertexRecordCount = 3303;
constexpr int32 FallbackTriangleCount = 1101;
constexpr int32 FallbackMaterialCount = 3;
constexpr double MaximumCandidateAdjustmentDegrees = 0.05;
constexpr int64 MaximumInputBytes = 4ll * 1024ll * 1024ll;

const FString CandidateContractSha256(
    TEXT("DF341566ED5DD8EA35A514587322076D85F47B8401730466218A2574D2AF5CC7"));
const FString NormalOverrideTableSha256(
    TEXT("7DF9C641DC3DD6175C692678020306EC2D273DA2FB244A6F67D58765B90C7464"));
const FString SourceAuditSha256(
    TEXT("ED171BEEDD034A44892DDF5DAB069A2E63D10303EC682E9F31645ACCAEFF63C0"));
const FString CandidateBuilderSha256(
    TEXT("CEA41110814D4E639A66B4CA4020A5A15A6E60A0B0F8BC057B3427F2027DAE84"));
const FString CoreObjSha256(
    TEXT("6418A023D64FA0A0F6C4CA14C79195BF96C02818B49C2AC03E4C81A61ECE9438"));
const FString FallbackObjSha256(
    TEXT("EFB1E7FE2371D5C522297698647DFC01C30240BE5A8A54945A7BCFDFA7C488F9"));
const FString PublicRealmContractSha256(
    TEXT("C5B4BFFF1FD90D5A0E7F056CE510CFC76BF3E7914C008F4D0CBCA4852B853C28"));
const FString PublicRealmFeaturesSha256(
    TEXT("DBBC471315517DBC0B6AB69C1F4169FCAA9CD475F708009580C1C25A4CF3155A"));

// Deliberately distinct invalid SHA-256 sentinels. Caller-provided paths and
// hashes can be inspected, but cannot bootstrap execution authority. A future
// reviewed source change must replace both values and recompile before the
// private materializer can pass BuildAdmission.
const FString TrustedAcceptedR33ReceiptSha256(
    TEXT("UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));
const FString TrustedFutureAuthorizationSha256(
    TEXT("UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));

const FString CandidateSchema(
    TEXT("triad.istana.public_realm_junction_continuity_candidate.v1"));
const FString OverrideSchema(
    TEXT("triad.istana.public_realm_junction_normal_overrides.v1"));
const FString AuditSchema(
    TEXT("triad.istana.public_realm_junction_continuity_source_audit.v1"));
const FString CandidateStatus(
    TEXT("POST_R33_UNNUMBERED_UNADMITTED_SOURCE_ONLY_CANDIDATE"));
const FString AcceptedR33Schema(
    TEXT("triad.istana_explore_v5d.r33_player0_capture.v1"));
const FString R33TransactionSchema(
    TEXT("triad.istana_explore_v5d.r33_cesium_world_terrain_reference.native_transaction.v1"));
const FString FutureAuthorizationSchema(
    TEXT("triad.istana_public_view_explore_v5d.public_realm.junction_continuity.future_transaction_authority.v1"));
const FString FutureOperation(
    TEXT("MATERIALIZE_PUBLIC_REALM_JUNCTION_CONTINUITY_CANDIDATE_ASSETS_ONLY"));
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmJunctionContinuityIntegration"));
const FString RoadMaterialSlotName(
    TEXT("MI_IPV5C_OfficialPlanningRoadZone"));

const TCHAR* const CoreMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadZone."
         "MI_IPV5C_OfficialPlanningRoadZone")};
const TCHAR* const FallbackMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadZone."
         "MI_IPV5C_OfficialPlanningRoadZone"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadGraphic."
         "MI_IPV5C_OfficialPlanningRoadGraphic"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/Materials/"
         "MI_IPV5D_PublicRealmConcrete."
         "MI_IPV5D_PublicRealmConcrete")};
const TCHAR* const CoreMaterialSlotNames[] = {
    TEXT("MI_IPV5C_OfficialPlanningRoadZone")};
const TCHAR* const FallbackMaterialSlotNames[] = {
    TEXT("MI_IPV5C_OfficialPlanningRoadZone"),
    TEXT("MI_IPV5C_OfficialPlanningRoadGraphic"),
    TEXT("MI_IPV5D_PublicRealmConcrete")};

static_assert(UE_ARRAY_COUNT(CoreMaterialPaths) == CoreMaterialCount);
static_assert(UE_ARRAY_COUNT(CoreMaterialSlotNames) == CoreMaterialCount);
static_assert(UE_ARRAY_COUNT(FallbackMaterialPaths) == FallbackMaterialCount);
static_assert(
    UE_ARRAY_COUNT(FallbackMaterialSlotNames) == FallbackMaterialCount);

struct FSourcePin
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourcePin SourcePins[] = {
    {TEXT("public_realm_junction_continuity_candidate.contract.v1.json"),
     5659,
     TEXT("DF341566ED5DD8EA35A514587322076D85F47B8401730466218A2574D2AF5CC7")},
    {TEXT("Generated/public_realm_junction_normal_overrides.v1.json"),
     211077,
     TEXT("7DF9C641DC3DD6175C692678020306EC2D273DA2FB244A6F67D58765B90C7464")},
    {TEXT("Generated/public_realm_junction_continuity_source_audit.v1.json"),
     5690,
     TEXT("ED171BEEDD034A44892DDF5DAB069A2E63D10303EC682E9F31645ACCAEFF63C0")},
    {TEXT("build_public_realm_junction_continuity_candidate.py"),
     28786,
     TEXT("CEA41110814D4E639A66B4CA4020A5A15A6E60A0B0F8BC057B3427F2027DAE84")},
    {TEXT("../Generated/SM_IPV5D_PublicRealm_Core_Render.obj"),
     168139,
     TEXT("6418A023D64FA0A0F6C4CA14C79195BF96C02818B49C2AC03E4C81A61ECE9438")},
    {TEXT("../Generated/SM_IPV5D_PublicRealm_Fallback_Render.obj"),
     420002,
     TEXT("EFB1E7FE2371D5C522297698647DFC01C30240BE5A8A54945A7BCFDFA7C488F9")},
    {TEXT("../istana_public_view_v5d_public_realm.contract.json"),
     17548,
     TEXT("C5B4BFFF1FD90D5A0E7F056CE510CFC76BF3E7914C008F4D0CBCA4852B853C28")},
    {TEXT("../Generated/IstanaPublicViewV5DPublicRealm.features.json"),
     19432,
     TEXT("DBBC471315517DBC0B6AB69C1F4169FCAA9CD475F708009580C1C25A4CF3155A")}};

enum class ESourceRole : uint8
{
    Core,
    Fallback,
};

struct FNormalOverride
{
    ESourceRole Role = ESourceRole::Core;
    int32 FaceRecordIndex = 0;
    int32 VertexRecordIndex = 0;
    int32 UvRecordIndex = 0;
    int32 NormalRecordIndex = 0;
    FVector3f SourceObjPositionCentimetres = FVector3f::ZeroVector;
    FVector3f SourceObjNormal = FVector3f::UpVector;
    FVector3f CandidateObjNormal = FVector3f::UpVector;
};

struct FAdmission
{
    FString CandidateRoot;
    FString AcceptedR33Sha256;
    FString FutureAuthorizationSha256;
    TArray<FNormalOverride> Overrides;
};

struct FMeshSpec
{
    ESourceRole Role;
    FString SourceObjectPath;
    FString CandidateObjectPath;
    int32 VertexRecords;
    int32 Triangles;
    int32 Materials;
    int32 Overrides;
    const TCHAR* const* MaterialPaths;
    const TCHAR* const* MaterialSlotNames;
};

FMeshSpec CoreSpec()
{
    return {
        ESourceRole::Core,
        UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
            ExistingCoreMeshObjectPath(),
        UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
            CandidateCoreMeshObjectPath(),
        CoreVertexRecordCount,
        CoreTriangleCount,
        CoreMaterialCount,
        CoreNormalOverrideCount,
        CoreMaterialPaths,
        CoreMaterialSlotNames};
}

FMeshSpec FallbackSpec()
{
    return {
        ESourceRole::Fallback,
        UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
            ExistingFallbackMeshObjectPath(),
        UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
            CandidateFallbackMeshObjectPath(),
        FallbackVertexRecordCount,
        FallbackTriangleCount,
        FallbackMaterialCount,
        FallbackNormalOverrideCount,
        FallbackMaterialPaths,
        FallbackMaterialSlotNames};
}

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
        OutError = TEXT("Junction-continuity source admission requires an absolute path, bounded byte count, and explicit SHA-256 pin.");
        return false;
    }
    const int64 ActualBytes = IFileManager::Get().FileSize(*AbsolutePath);
    if (ActualBytes != ExpectedBytes ||
        !FFileHelper::LoadFileToArray(OutBytes, *AbsolutePath) ||
        OutBytes.Num() != ActualBytes)
    {
        OutError = TEXT("A junction-continuity source file is absent or its exact byte count drifted.");
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
        OutError = TEXT("A junction-continuity source file failed its immutable SHA-256 pin.");
        return false;
    }
#else
    OutError = TEXT("Junction-continuity source admission requires WITH_SSL SHA-256 support.");
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
        OutError = TEXT("A hash-pinned junction-continuity JSON document could not be parsed.");
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

bool AllExactBools(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* const* Fields,
    int32 Count,
    bool Expected)
{
    if (!Object.IsValid())
    {
        return false;
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (!ExactBool(Object, Fields[Index], Expected))
        {
            return false;
        }
    }
    return true;
}

bool ValidatePinnedInput(
    const TSharedPtr<FJsonObject>& Inputs,
    const TCHAR* Field,
    int64 ExpectedBytes,
    const FString& ExpectedSha256)
{
    const TSharedPtr<FJsonObject>* Row = nullptr;
    return Inputs.IsValid() && Inputs->TryGetObjectField(Field, Row) && Row &&
        Row->IsValid() &&
        ExactInteger(*Row, TEXT("bytes"), static_cast<int32>(ExpectedBytes)) &&
        ExactString(*Row, TEXT("sha256"), ExpectedSha256);
}

bool ValidateCandidateContract(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Inputs = nullptr;
    const TSharedPtr<FJsonObject>* Scope = nullptr;
    const TSharedPtr<FJsonObject>* Evidence = nullptr;
    const TSharedPtr<FJsonObject>* Preservation = nullptr;
    const TSharedPtr<FJsonObject>* Authority = nullptr;
    const TSharedPtr<FJsonObject>* Outputs = nullptr;
    const TSharedPtr<FJsonObject>* Future = nullptr;
    const TCHAR* const PreservationFields[] = {
        TEXT("sourceObjBytesModified"), TEXT("vertexPositionsModified"),
        TEXT("uv0Modified"), TEXT("facesOrWindingModified"),
        TEXT("triangleCountModified"), TEXT("groupsModified"),
        TEXT("materialSlotsModified"), TEXT("coreFallbackPartitionModified"),
        TEXT("providerOrLocalIdentityModified"),
        TEXT("componentOrActorTransformModified"),
        TEXT("roadWidthOrElevationModified"),
        TEXT("terrainOrGeographyModified")};
    const TCHAR* const AuthorityFields[] = {
        TEXT("surveyOrAsBuiltAuthority"), TEXT("currentCompleteAuthority"),
        TEXT("collisionAuthority"), TEXT("navigationAuthority"),
        TEXT("lineOfSightAuthority"), TEXT("rfAuthority"),
        TEXT("sensorAuthority"), TEXT("simulationAuthority"),
        TEXT("routeAccessOrSecurityAuthority"),
        TEXT("physicalRoadMaterialWidthOrElevationAuthority"),
        TEXT("visualAcceptanceClaimed")};
    const TCHAR* const FutureFields[] = {
        TEXT("humanAcceptedR33ReceiptRequired"),
        TEXT("separateNarrowSuccessorAuthorizationRequired"),
        TEXT("ue55CompileImportSaveColdReloadRequired"),
        TEXT("exactBeforeAfterPositionUvFaceGroupSlotAndTransformProofRequired"),
        TEXT("sharpBendAndFourWayJunctionNativeCapturesRequired"),
        TEXT("naniteAndRasterParityRequired"),
        TEXT("collisionNavigationLosRfSensorSimulationRegressionRequired"),
        TEXT("performanceProofRequired"),
        TEXT("explicitHumanVisualAcceptanceRequired")};

    const bool bValid =
        ExactString(Root, TEXT("schema"), CandidateSchema) &&
        ExactString(Root, TEXT("status"), CandidateStatus) &&
        ExactString(
            Root,
            TEXT("purpose"),
            TEXT("Remove only the bounded road-normal discontinuity at duplicate/tolerance-equivalent corners shared by the exact frozen Core and Fallback render sources; do not patch, move, add, remove, or reinterpret any geometry.")) &&
        ExactBool(Root, TEXT("sourceOnly"), true) &&
        ExactBool(Root, TEXT("nativeProjectApplied"), false) &&
        ExactBool(Root, TEXT("unrealLaunchedOrBuilt"), false) &&
        ExactBool(Root, TEXT("newNumberedStageAuthorized"), false) &&
        Root->TryGetObjectField(TEXT("inputs"), Inputs) && Inputs &&
        Inputs->IsValid() &&
        ValidatePinnedInput(
            *Inputs,
            TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/SM_IPV5D_PublicRealm_Core_Render.obj"),
            168139,
            CoreObjSha256) &&
        ValidatePinnedInput(
            *Inputs,
            TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/SM_IPV5D_PublicRealm_Fallback_Render.obj"),
            420002,
            FallbackObjSha256) &&
        ValidatePinnedInput(
            *Inputs,
            TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/istana_public_view_v5d_public_realm.contract.json"),
            17548,
            PublicRealmContractSha256) &&
        ValidatePinnedInput(
            *Inputs,
            TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/IstanaPublicViewV5DPublicRealm.features.json"),
            19432,
            PublicRealmFeaturesSha256) &&
        Root->TryGetObjectField(TEXT("admittedScope"), Scope) && Scope &&
        Scope->IsValid() &&
        ExactString(
            *Scope,
            TEXT("group"),
            TEXT("V5D_PUBLIC_ROAD_BASE_VISUAL_ASSUMPTION")) &&
        ExactInteger(*Scope, TEXT("coreRoadTriangles"), CoreTriangleCount) &&
        ExactInteger(*Scope, TEXT("fallbackRoadTriangles"), 697) &&
        ExactInteger(*Scope, TEXT("roadTriangles"), 1141) &&
        ExactInteger(*Scope, TEXT("roadCornerRecords"), 3423) &&
        ExactInteger(
            *Scope, TEXT("sourceNormalDisagreementClusters"), ClusterCount) &&
        ExactInteger(
            *Scope,
            TEXT("sourceNormalRecordsInClusters"),
            NormalOverrideCount) &&
        ExactInteger(
            *Scope,
            TEXT("coreNormalRecordsInClusters"),
            CoreNormalOverrideCount) &&
        ExactInteger(
            *Scope,
            TEXT("fallbackNormalRecordsInClusters"),
            FallbackNormalOverrideCount) &&
        ExactNumber(
            *Scope,
            TEXT("maximumCandidateAdjustmentAllowedDegrees"),
            MaximumCandidateAdjustmentDegrees) &&
        Root->TryGetObjectField(TEXT("sourceGeometryEvidence"), Evidence) &&
        Evidence && Evidence->IsValid() &&
        ExactBool(
            *Evidence,
            TEXT("allRoadFaceTokensUseIdenticalVertexUvNormalIndices"),
            true) &&
        ExactBool(
            *Evidence,
            TEXT("allRoadUv0RowsExactlyEqualLogicalHeroLocalXYMetres"),
            true) &&
        ExactBool(*Evidence, TEXT("roadUvMutationNeeded"), false) &&
        ExactBool(*Evidence, TEXT("coreRoadUnionValid"), true) &&
        ExactBool(*Evidence, TEXT("fallbackRoadUnionValid"), true) &&
        ExactNumber(
            *Evidence,
            TEXT("coreFallbackIntersectionAreaSquareMetres"),
            0.0) &&
        ExactNumber(
            *Evidence, TEXT("coreFallbackDistanceMetres"), 0.0) &&
        ExactBool(*Evidence, TEXT("coreFallbackTouch"), true) &&
        ExactBool(
            *Evidence,
            TEXT("geometryPatchJustifiedBySourceEvidence"),
            false) &&
        Root->TryGetObjectField(
            TEXT("preservationBoundary"), Preservation) &&
        Preservation &&
        AllExactBools(
            *Preservation,
            PreservationFields,
            UE_ARRAY_COUNT(PreservationFields),
            false) &&
        Root->TryGetObjectField(TEXT("authorityBoundary"), Authority) &&
        Authority &&
        AllExactBools(
            *Authority,
            AuthorityFields,
            UE_ARRAY_COUNT(AuthorityFields),
            false) &&
        Root->TryGetObjectField(TEXT("outputs"), Outputs) && Outputs &&
        Outputs->IsValid() &&
        ExactString(
            *Outputs,
            TEXT("normalOverrideTable"),
            TEXT("Generated/public_realm_junction_normal_overrides.v1.json")) &&
        ExactBool(*Outputs, TEXT("nativeMeshOrMaterialProduced"), false) &&
        ExactBool(*Outputs, TEXT("mapOrComponentBindingProduced"), false) &&
        Root->TryGetObjectField(TEXT("futureAdmission"), Future) && Future &&
        AllExactBools(
            *Future,
            FutureFields,
            UE_ARRAY_COUNT(FutureFields),
            true);
    if (!bValid)
    {
        OutError = TEXT("The public-realm junction candidate lost its exact bounded normal-only, geometry-preserving, or no-authority contract.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSourceAudit(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* CandidateOutput = nullptr;
    const TSharedPtr<FJsonObject>* CandidateSources = nullptr;
    const TSharedPtr<FJsonObject>* CandidateContract = nullptr;
    const TSharedPtr<FJsonObject>* Builder = nullptr;
    const TSharedPtr<FJsonObject>* Candidate = nullptr;
    const bool bValid =
        ExactString(Root, TEXT("schema"), AuditSchema) &&
        ExactString(Root, TEXT("status"), CandidateStatus) &&
        ExactBool(Root, TEXT("nativeProjectApplied"), false) &&
        ExactBool(Root, TEXT("unrealLaunchedOrBuilt"), false) &&
        ExactBool(Root, TEXT("visualAcceptance"), false) &&
        Root->TryGetObjectField(TEXT("candidateOutput"), CandidateOutput) &&
        CandidateOutput && CandidateOutput->IsValid() &&
        ExactString(
            *CandidateOutput,
            TEXT("path"),
            TEXT("Generated/public_realm_junction_normal_overrides.v1.json")) &&
        ExactInteger(*CandidateOutput, TEXT("bytes"), 211077) &&
        ExactString(
            *CandidateOutput,
            TEXT("sha256"),
            NormalOverrideTableSha256) &&
        Root->TryGetObjectField(
            TEXT("candidateSources"), CandidateSources) &&
        CandidateSources && CandidateSources->IsValid() &&
        (*CandidateSources)->TryGetObjectField(
            TEXT("contract"), CandidateContract) &&
        CandidateContract && CandidateContract->IsValid() &&
        ExactInteger(*CandidateContract, TEXT("bytes"), 5659) &&
        ExactString(
            *CandidateContract,
            TEXT("sha256"),
            CandidateContractSha256) &&
        (*CandidateSources)->TryGetObjectField(TEXT("builder"), Builder) &&
        Builder && Builder->IsValid() &&
        ExactInteger(*Builder, TEXT("bytes"), 28786) &&
        ExactString(*Builder, TEXT("sha256"), CandidateBuilderSha256) &&
        Root->TryGetObjectField(TEXT("normalCandidate"), Candidate) &&
        Candidate && Candidate->IsValid() &&
        ExactInteger(*Candidate, TEXT("clusterCount"), ClusterCount) &&
        ExactInteger(
            *Candidate, TEXT("normalRecordCount"), NormalOverrideCount) &&
        ExactInteger(
            *Candidate,
            TEXT("coreNormalRecordCount"),
            CoreNormalOverrideCount) &&
        ExactInteger(
            *Candidate,
            TEXT("fallbackNormalRecordCount"),
            FallbackNormalOverrideCount) &&
        ExactInteger(*Candidate, TEXT("coreOnlyClusterCount"), 0) &&
        ExactInteger(*Candidate, TEXT("fallbackOnlyClusterCount"), 0) &&
        ExactInteger(
            *Candidate, TEXT("crossCoreFallbackClusterCount"), ClusterCount) &&
        ExactNumber(
            *Candidate,
            TEXT("maximumCandidateAdjustmentAllowedDegrees"),
            MaximumCandidateAdjustmentDegrees);
    if (!bValid)
    {
        OutError = TEXT("The public-realm junction source audit lost its exact source-only output pin or 60-cluster/355-row census.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ParseNumberVector3(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FVector3f& OutValue)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) ||
        !Values || Values->Num() != 3)
    {
        return false;
    }
    const double X = (*Values)[0]->AsNumber();
    const double Y = (*Values)[1]->AsNumber();
    const double Z = (*Values)[2]->AsNumber();
    if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z))
    {
        return false;
    }
    OutValue = FVector3f(
        static_cast<float>(X),
        static_cast<float>(Y),
        static_cast<float>(Z));
    return !OutValue.ContainsNaN();
}

bool ParseStringVector3(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FVector3f& OutValue)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) ||
        !Values || Values->Num() != 3)
    {
        return false;
    }
    FString Tokens[3];
    double Numbers[3] = {};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (!(*Values)[Index]->TryGetString(Tokens[Index]) ||
            Tokens[Index].IsEmpty())
        {
            return false;
        }
        if (!FDefaultValueHelper::ParseDouble(Tokens[Index], Numbers[Index]) ||
            !FMath::IsFinite(Numbers[Index]))
        {
            return false;
        }
    }
    OutValue = FVector3f(
        static_cast<float>(Numbers[0]),
        static_cast<float>(Numbers[1]),
        static_cast<float>(Numbers[2]));
    return !OutValue.ContainsNaN();
}

bool ValidateOverrideTable(
    const TSharedPtr<FJsonObject>& Root,
    TArray<FNormalOverride>& OutOverrides,
    FString& OutError)
{
    OutOverrides.Reset();
    const TSharedPtr<FJsonObject>* Summary = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Clusters = nullptr;
    if (!ExactString(Root, TEXT("schema"), OverrideSchema) ||
        !ExactString(Root, TEXT("status"), CandidateStatus) ||
        !ExactString(
            Root,
            TEXT("indexing"),
            TEXT("ONE_BASED_SOURCE_OBJ_RECORD_INDICES")) ||
        !ExactString(
            Root,
            TEXT("operation"),
            TEXT("REPLACE_ONLY_LISTED_RENDER_NORMAL_ROWS_WITH_CANDIDATE_NORMAL")) ||
        !ExactBool(Root, TEXT("nativeAssetProduced"), false) ||
        !ExactBool(Root, TEXT("nonNormalObjRowsModified"), false) ||
        !Root->TryGetObjectField(TEXT("summary"), Summary) || !Summary ||
        !Summary->IsValid() ||
        !ExactInteger(*Summary, TEXT("clusterCount"), ClusterCount) ||
        !ExactInteger(
            *Summary, TEXT("normalRecordCount"), NormalOverrideCount) ||
        !ExactInteger(
            *Summary,
            TEXT("coreNormalRecordCount"),
            CoreNormalOverrideCount) ||
        !ExactInteger(
            *Summary,
            TEXT("fallbackNormalRecordCount"),
            FallbackNormalOverrideCount) ||
        !ExactInteger(*Summary, TEXT("coreOnlyClusterCount"), 0) ||
        !ExactInteger(*Summary, TEXT("fallbackOnlyClusterCount"), 0) ||
        !ExactInteger(
            *Summary, TEXT("crossCoreFallbackClusterCount"), ClusterCount) ||
        !ExactNumber(
            *Summary,
            TEXT("maximumSourcePairAngleDegrees"),
            0.0700412) ||
        !ExactNumber(
            *Summary,
            TEXT("maximumCandidateAdjustmentDegrees"),
            0.044152281) ||
        !ExactNumber(
            *Summary,
            TEXT("maximumCandidateAdjustmentAllowedDegrees"),
            MaximumCandidateAdjustmentDegrees) ||
        !Root->TryGetArrayField(TEXT("clusters"), Clusters) || !Clusters ||
        Clusters->Num() != ClusterCount)
    {
        OutError = TEXT("The junction normal table lost its exact schema, operation, or 60-cluster/355-row summary.");
        return false;
    }

    TSet<FString> UniqueRecordKeys;
    int32 CoreRows = 0;
    int32 FallbackRows = 0;
    for (int32 ClusterIndex = 0;
         ClusterIndex < Clusters->Num();
         ++ClusterIndex)
    {
        const TSharedPtr<FJsonObject> Cluster =
            (*Clusters)[ClusterIndex]->AsObject();
        const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
        FVector3f CandidateNormal = FVector3f::ZeroVector;
        double ClusterAdjustment = 0.0;
        if (!Cluster.IsValid() ||
            !Cluster->TryGetArrayField(TEXT("sourceRoles"), Roles) ||
            !Roles || Roles->Num() != 2 ||
            (*Roles)[0]->AsString() != TEXT("CORE") ||
            (*Roles)[1]->AsString() != TEXT("FALLBACK") ||
            !Cluster->TryGetArrayField(TEXT("members"), Members) ||
            !Members || Members->Num() < 2 ||
            !ExactInteger(
                Cluster,
                TEXT("memberCount"),
                Members->Num()) ||
            !ParseNumberVector3(
                Cluster, TEXT("candidateNormal"), CandidateNormal) ||
            !Cluster->TryGetNumberField(
                TEXT("maximumCandidateAdjustmentDegrees"),
                ClusterAdjustment) ||
            ClusterAdjustment < 0.0 ||
            ClusterAdjustment > MaximumCandidateAdjustmentDegrees ||
            !FMath::IsNearlyEqual(
                CandidateNormal.SizeSquared(), 1.0f, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("Junction normal cluster %d lost its exact cross-Core/Fallback bounded candidate."),
                ClusterIndex);
            return false;
        }

        for (const TSharedPtr<FJsonValue>& MemberValue : *Members)
        {
            const TSharedPtr<FJsonObject> Member = MemberValue->AsObject();
            FNormalOverride Row;
            FString Role;
            double Area = 0.0;
            if (!Member.IsValid() ||
                !Member->TryGetStringField(TEXT("sourceRole"), Role) ||
                (Role != TEXT("CORE") && Role != TEXT("FALLBACK")) ||
                !Member->TryGetNumberField(
                    TEXT("sourceTriangleAreaSquareMetres"), Area) ||
                !FMath::IsFinite(Area) || Area <= 0.0 ||
                !ParseStringVector3(
                    Member,
                    TEXT("sourcePositionCentimetres"),
                    Row.SourceObjPositionCentimetres) ||
                !ParseNumberVector3(
                    Member, TEXT("sourceNormal"), Row.SourceObjNormal))
            {
                OutError = TEXT("A junction normal override member lost its exact role, position, normal, or positive source-area evidence.");
                return false;
            }
            Row.Role = Role == TEXT("CORE")
                ? ESourceRole::Core
                : ESourceRole::Fallback;
            Row.CandidateObjNormal = CandidateNormal;
            if (!Member->TryGetNumberField(
                    TEXT("faceRecordIndex"), Row.FaceRecordIndex) ||
                !Member->TryGetNumberField(
                    TEXT("vertexRecordIndex"), Row.VertexRecordIndex) ||
                !Member->TryGetNumberField(
                    TEXT("uvRecordIndex"), Row.UvRecordIndex) ||
                !Member->TryGetNumberField(
                    TEXT("normalRecordIndex"), Row.NormalRecordIndex) ||
                Row.VertexRecordIndex != Row.UvRecordIndex ||
                Row.VertexRecordIndex != Row.NormalRecordIndex ||
                Row.FaceRecordIndex < 1 ||
                Row.FaceRecordIndex >
                    (Row.Role == ESourceRole::Core
                         ? CoreTriangleCount
                         : FallbackTriangleCount) ||
                Row.NormalRecordIndex < 1 ||
                Row.NormalRecordIndex >
                    (Row.Role == ESourceRole::Core
                         ? CoreVertexRecordCount
                         : FallbackVertexRecordCount) ||
                !FMath::IsNearlyEqual(
                    Row.SourceObjNormal.SizeSquared(),
                    1.0f,
                    0.0001f) ||
                Row.SourceObjNormal.Equals(
                    Row.CandidateObjNormal,
                    0.000000001f))
            {
                OutError = TEXT("A junction normal override member lost its exact one-based record identity or bounded changed-normal contract.");
                return false;
            }
            const FString UniqueKey = FString::Printf(
                TEXT("%s:%d"), *Role, Row.NormalRecordIndex);
            if (UniqueRecordKeys.Contains(UniqueKey))
            {
                OutError = TEXT("The junction normal override table contains a duplicate source role/normal record.");
                return false;
            }
            UniqueRecordKeys.Add(UniqueKey);
            CoreRows += Row.Role == ESourceRole::Core ? 1 : 0;
            FallbackRows += Row.Role == ESourceRole::Fallback ? 1 : 0;
            OutOverrides.Add(Row);
        }
    }

    if (OutOverrides.Num() != NormalOverrideCount ||
        UniqueRecordKeys.Num() != NormalOverrideCount ||
        CoreRows != CoreNormalOverrideCount ||
        FallbackRows != FallbackNormalOverrideCount)
    {
        OutError = TEXT("The parsed junction normal override roster is not the exact unique 174-Core/181-Fallback table.");
        OutOverrides.Reset();
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
        !ExactBool(
            Root, TEXT("MechanicalCaptureValidationPassed"), true) ||
        !ExactBool(Root, TEXT("ExplicitHumanReviewAcceptance"), true) ||
        !ExactBool(Root, TEXT("ConfirmedEightImagesReviewed"), true) ||
        !ExactBool(Root, TEXT("HumanVisualReviewAttested"), true) ||
        !ExactBool(Root, TEXT("AutomaticVisualAcceptanceAllowed"), false) ||
        !ExactBool(Root, TEXT("VisualReviewRequired"), false) ||
        !ExactBool(Root, TEXT("VisualReviewAccepted"), true) ||
        !ExactBool(
            Root, TEXT("R33CesiumWorldTerrainVisualQaAccepted"), true) ||
        !ExactBool(Root, TEXT("GooglePrimaryVisualQaAccepted"), true) ||
        !ExactBool(Root, TEXT("CwtPresentedVisualQaAccepted"), true) ||
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
        OutError = TEXT("R33 receipt is not the exact human-accepted, no-mutation eight-image receipt.");
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
        TEXT("CandidateContractSha256"),
        TEXT("NormalOverrideTableSha256"), TEXT("SourceAuditSha256"),
        TEXT("CoreObjSha256"), TEXT("FallbackObjSha256"),
        TEXT("ExactCandidateAssetCount"), TEXT("ExactNormalOverrideCount"),
        TEXT("OutputNamespace"), TEXT("UnnumberedSuccessor"),
        TEXT("ExplicitExecutionAuthorized"), TEXT("AssetsOnlyEndpoint"),
        TEXT("NormalOnlyMeshDescriptionMutation"),
        TEXT("ExactCoreFallbackSourceAssetsPreserved"),
        TEXT("MapOrComponentBindingAuthorized"),
        TEXT("SourceObjMutationAuthorized"),
        TEXT("VertexUvTopologyGroupSlotTransformIdentityMutationAuthorized"),
        TEXT("TerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"),
        TEXT("NativeWriteOrUnrealLaunchPerformed")};
    if (!Root.IsValid() ||
        Root->Values.Num() != UE_ARRAY_COUNT(ExactFields))
    {
        OutError = TEXT("Future junction-continuity authorization must contain exactly the narrow trusted field roster and no extensions.");
        return false;
    }
    for (const TCHAR* Field : ExactFields)
    {
        if (!Root->HasField(Field))
        {
            OutError = TEXT("Future junction-continuity authorization omitted a field from the exact narrow roster.");
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
            Root,
            TEXT("CandidateContractSha256"),
            CandidateContractSha256) ||
        !ExactString(
            Root,
            TEXT("NormalOverrideTableSha256"),
            NormalOverrideTableSha256) ||
        !ExactString(Root, TEXT("SourceAuditSha256"), SourceAuditSha256) ||
        !ExactString(Root, TEXT("CoreObjSha256"), CoreObjSha256) ||
        !ExactString(Root, TEXT("FallbackObjSha256"), FallbackObjSha256) ||
        !ExactInteger(
            Root, TEXT("ExactCandidateAssetCount"), CandidateAssetCount) ||
        !ExactInteger(
            Root, TEXT("ExactNormalOverrideCount"), NormalOverrideCount) ||
        !ExactString(Root, TEXT("OutputNamespace"), OutputRoot) ||
        !ExactBool(Root, TEXT("UnnumberedSuccessor"), true) ||
        !ExactBool(Root, TEXT("ExplicitExecutionAuthorized"), true) ||
        !ExactBool(Root, TEXT("AssetsOnlyEndpoint"), true) ||
        !ExactBool(
            Root, TEXT("NormalOnlyMeshDescriptionMutation"), true) ||
        !ExactBool(
            Root,
            TEXT("ExactCoreFallbackSourceAssetsPreserved"),
            true) ||
        !ExactBool(
            Root, TEXT("MapOrComponentBindingAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceObjMutationAuthorized"), false) ||
        !ExactBool(
            Root,
            TEXT("VertexUvTopologyGroupSlotTransformIdentityMutationAuthorized"),
            false) ||
        !ExactBool(
            Root,
            TEXT("TerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"),
            false) ||
        !ExactBool(
            Root, TEXT("NativeWriteOrUnrealLaunchPerformed"), false))
    {
        OutError = TEXT("Future junction-continuity authorization is absent, unpinned, non-explicit, or broader than isolated two-mesh normal-only materialization.");
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
        OutError = TEXT("Junction-continuity admission requires an absolute candidate root, two absolute receipt paths, and two explicit SHA-256 pins.");
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
            OutError = TEXT("Public-realm junction execution is intentionally unreachable: distinct compiled trusted receipt anchors are unset and require a separately reviewed source change plus recompile.");
            return false;
        }
        if (!ExpectedAcceptedR33ReceiptSha256.Equals(
                TrustedAcceptedR33ReceiptSha256,
                ESearchCase::IgnoreCase) ||
            !ExpectedFutureTransactionAuthorizationSha256.Equals(
                TrustedFutureAuthorizationSha256,
                ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Caller-supplied public-realm junction receipt hashes do not match the separately compiled trusted anchors.");
            return false;
        }
    }

    FString FullRoot = FPaths::ConvertRelativePathToFull(CandidateRoot);
    FString FullR33 =
        FPaths::ConvertRelativePathToFull(AcceptedR33ReceiptPath);
    FString FullAuthorization = FPaths::ConvertRelativePathToFull(
        FutureTransactionAuthorizationPath);
    FPaths::NormalizeDirectoryName(FullRoot);
    FPaths::NormalizeFilename(FullR33);
    FPaths::NormalizeFilename(FullAuthorization);
    if (FPaths::GetCleanFilename(FullRoot) !=
            TEXT("JunctionContinuityCandidate") ||
        FPaths::IsSamePath(FullR33, FullAuthorization) ||
        ExpectedAcceptedR33ReceiptSha256.Equals(
            ExpectedFutureTransactionAuthorizationSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Junction-continuity admission requires the exact named candidate root and independent accepted-R33/future-authorization receipts.");
        return false;
    }

    TSharedPtr<FJsonObject> CandidateContract;
    TSharedPtr<FJsonObject> OverrideTable;
    TSharedPtr<FJsonObject> SourceAudit;
    if (!ParsePinnedJson(
            SourcePath(
                FullRoot,
                TEXT("public_realm_junction_continuity_candidate.contract.v1.json")),
            5659,
            CandidateContractSha256,
            CandidateContract,
            OutError) ||
        !ValidateCandidateContract(CandidateContract, OutError) ||
        !ParsePinnedJson(
            SourcePath(
                FullRoot,
                TEXT("Generated/public_realm_junction_normal_overrides.v1.json")),
            211077,
            NormalOverrideTableSha256,
            OverrideTable,
            OutError) ||
        !ValidateOverrideTable(
            OverrideTable, OutAdmission.Overrides, OutError) ||
        !ParsePinnedJson(
            SourcePath(
                FullRoot,
                TEXT("Generated/public_realm_junction_continuity_source_audit.v1.json")),
            5690,
            SourceAuditSha256,
            SourceAudit,
            OutError) ||
        !ValidateSourceAudit(SourceAudit, OutError))
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
    }

    const int64 R33Bytes = IFileManager::Get().FileSize(*FullR33);
    const int64 AuthorizationBytes =
        IFileManager::Get().FileSize(*FullAuthorization);
    if (R33Bytes < 2 || R33Bytes > MaximumInputBytes ||
        AuthorizationBytes < 2 || AuthorizationBytes > MaximumInputBytes)
    {
        OutError = TEXT("A junction-continuity gate receipt is absent or outside the bounded input size.");
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

FVector3f NativePosition(const FVector3f& ObjPosition)
{
    // The frozen legacy OBJ is preconditioned as disk v=(x,-y,z).
    return FVector3f(ObjPosition.X, -ObjPosition.Y, ObjPosition.Z);
}

FVector3f NativeNormal(const FVector3f& ObjNormal)
{
    // The frozen legacy OBJ is preconditioned as disk vn=(nx,-ny,nz).
    return FVector3f(ObjNormal.X, -ObjNormal.Y, ObjNormal.Z);
}

FVector2f NativeUv0(const FVector3f& ObjPosition)
{
    // Disk vt=(u,1-v), while the UE importer reads back (u,1-diskV).
    return FVector2f(ObjPosition.X * 0.01f, -ObjPosition.Y * 0.01f);
}

bool HasExactMaterialRoster(
    const UStaticMesh* Mesh,
    const FMeshSpec& Spec,
    FString& OutError)
{
    if (!Mesh || Mesh->GetStaticMaterials().Num() != Spec.Materials)
    {
        OutError = TEXT("A public-realm junction mesh lost its exact material-slot count.");
        return false;
    }
    for (int32 Index = 0; Index < Spec.Materials; ++Index)
    {
        const FStaticMaterial& Slot = Mesh->GetStaticMaterials()[Index];
        if (!Slot.MaterialInterface ||
            Slot.MaterialInterface->GetPathName() !=
                Spec.MaterialPaths[Index] ||
            Slot.MaterialSlotName != FName(Spec.MaterialSlotNames[Index]) ||
            Slot.ImportedMaterialSlotName !=
                FName(Spec.MaterialSlotNames[Index]))
        {
            OutError = FString::Printf(
                TEXT("Public-realm junction material slot %d lost its exact name, imported identity, or binding."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateMeshDescriptionCensus(
    const FMeshDescription& Description,
    const FMeshSpec& Spec,
    FString& OutError)
{
    if (Description.NeedsCompact() ||
        Description.Vertices().Num() != Spec.VertexRecords ||
        Description.VertexInstances().Num() != Spec.VertexRecords ||
        Description.Triangles().Num() != Spec.Triangles ||
        Description.Polygons().Num() != Spec.Triangles ||
        Description.PolygonGroups().Num() != Spec.Materials)
    {
        OutError = TEXT("A public-realm junction mesh description lost its compact one-record-per-corner topology census.");
        return false;
    }
    const FStaticMeshConstAttributes Attributes(Description);
    const auto Positions = Attributes.GetVertexPositions();
    const auto Normals = Attributes.GetVertexInstanceNormals();
    const auto Tangents = Attributes.GetVertexInstanceTangents();
    const auto BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
    const auto Uvs = Attributes.GetVertexInstanceUVs();
    const auto Colors = Attributes.GetVertexInstanceColors();
    const auto GroupNames = Attributes.GetPolygonGroupMaterialSlotNames();
    if (!Positions.IsValid() || !Normals.IsValid() || !Tangents.IsValid() ||
        !BinormalSigns.IsValid() || !Uvs.IsValid() || !Colors.IsValid() ||
        !GroupNames.IsValid() || Uvs.GetNumChannels() != 1)
    {
        OutError = TEXT("A public-realm junction mesh description lost a protected position, normal, tangent, UV0, colour, or group-name attribute.");
        return false;
    }
    for (int32 Index = 0; Index < Spec.VertexRecords; ++Index)
    {
        const FVertexID VertexId(Index);
        const FVertexInstanceID InstanceId(Index);
        if (!Description.IsVertexValid(VertexId) ||
            !Description.IsVertexInstanceValid(InstanceId) ||
            Description.GetVertexInstanceVertex(InstanceId) != VertexId ||
            Positions.Get(VertexId).ContainsNaN() ||
            Normals.Get(InstanceId).ContainsNaN() ||
            Tangents.Get(InstanceId).ContainsNaN() ||
            Uvs.Get(InstanceId, 0).ContainsNaN())
        {
            OutError = TEXT("A public-realm junction source record no longer maps one-to-one to the same native vertex instance.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateStaticMeshBoundary(
    UStaticMesh* Mesh,
    const FMeshSpec& Spec,
    const FString& ExpectedObjectPath,
    FString& OutError)
{
    if (Mesh)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    const FMeshDescription* Description =
        Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        Mesh->GetPathName() != ExpectedObjectPath ||
        Mesh->GetNumSourceModels() != 1 || Mesh->GetNumLODs() != 1 ||
        !Description || !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() != Spec.Triangles ||
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D !=
            FVector::OneVector ||
        Mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs ||
        Mesh->GetSourceModel(0).BuildSettings.bRecomputeNormals ||
        !Mesh->GetSourceModel(0).BuildSettings.bRecomputeTangents ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseMikkTSpace ||
        Mesh->GetSourceModel(0).BuildSettings.bRemoveDegenerates ||
        Mesh->GetLightMapCoordinateIndex() != 0 ||
        !Mesh->NaniteSettings.bEnabled ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(
            Mesh->NaniteSettings.TrimRelativeError) ||
        Mesh->NaniteSettings.FallbackTarget !=
            ENaniteFallbackTarget::PercentTriangles ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError) ||
        !Body || Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision() ||
        !HasExactMaterialRoster(Mesh, Spec, OutError) ||
        !ValidateMeshDescriptionCensus(*Description, Spec, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A public-realm junction mesh lost its exact identity, explicit-normal/Mikk-tangent settings, full-fidelity Nanite state, material roster, topology, or render-only no-collision/no-navigation boundary.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool StaticMeshAssetStateEqual(
    UStaticMesh* Source,
    UStaticMesh* Candidate,
    const FMeshSpec& Spec,
    FString& OutError)
{
    const FStaticMeshRenderData* SourceRender =
        Source ? Source->GetRenderData() : nullptr;
    const FStaticMeshRenderData* CandidateRender =
        Candidate ? Candidate->GetRenderData() : nullptr;
    if (!Source || !Candidate || Source == Candidate || !SourceRender ||
        !CandidateRender || SourceRender->LODResources.Num() != 1 ||
        CandidateRender->LODResources.Num() != 1 ||
        SourceRender->LODResources[0].Sections.Num() !=
            CandidateRender->LODResources[0].Sections.Num() ||
        Source->GetStaticMaterials().Num() !=
            Candidate->GetStaticMaterials().Num() ||
        !Source->GetBounds().Origin.Equals(
            Candidate->GetBounds().Origin, 0.001) ||
        !Source->GetBounds().BoxExtent.Equals(
            Candidate->GetBounds().BoxExtent, 0.001) ||
        !FMath::IsNearlyEqual(
            Source->GetBounds().SphereRadius,
            Candidate->GetBounds().SphereRadius,
            0.001f) ||
        Source->GetLightMapCoordinateIndex() !=
            Candidate->GetLightMapCoordinateIndex() ||
        Source->GetLightMapResolution() !=
            Candidate->GetLightMapResolution())
    {
        OutError = TEXT("A junction-continuity candidate changed bounds, render sections, materials, or lightmap identity outside the normal stream.");
        return false;
    }
    for (int32 SlotIndex = 0;
         SlotIndex < Source->GetStaticMaterials().Num();
         ++SlotIndex)
    {
        const FStaticMaterial& A = Source->GetStaticMaterials()[SlotIndex];
        const FStaticMaterial& B =
            Candidate->GetStaticMaterials()[SlotIndex];
        if (A.MaterialInterface != B.MaterialInterface ||
            A.MaterialSlotName != B.MaterialSlotName ||
            A.ImportedMaterialSlotName != B.ImportedMaterialSlotName)
        {
            OutError = TEXT("A junction-continuity candidate changed a material slot name, order, imported identity, or binding.");
            return false;
        }
    }
    const int32 SectionCount =
        SourceRender->LODResources[0].Sections.Num();
    for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
    {
        const FStaticMeshSection& A =
            SourceRender->LODResources[0].Sections[SectionIndex];
        const FStaticMeshSection& B =
            CandidateRender->LODResources[0].Sections[SectionIndex];
        const FMeshSectionInfo SourceInfo =
            Source->GetSectionInfoMap().Get(0, SectionIndex);
        const FMeshSectionInfo CandidateInfo =
            Candidate->GetSectionInfoMap().Get(0, SectionIndex);
        const FMeshSectionInfo SourceOriginal =
            Source->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        const FMeshSectionInfo CandidateOriginal =
            Candidate->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (A.MaterialIndex != B.MaterialIndex ||
            A.NumTriangles != B.NumTriangles ||
            SourceInfo.MaterialIndex != CandidateInfo.MaterialIndex ||
            SourceInfo.bEnableCollision != CandidateInfo.bEnableCollision ||
            SourceInfo.bCastShadow != CandidateInfo.bCastShadow ||
            SourceOriginal.MaterialIndex != CandidateOriginal.MaterialIndex ||
            SourceOriginal.bEnableCollision !=
                CandidateOriginal.bEnableCollision ||
            SourceOriginal.bCastShadow != CandidateOriginal.bCastShadow)
        {
            OutError = TEXT("A junction-continuity candidate changed section topology, slot mapping, collision, or shadow semantics.");
            return false;
        }
    }
    const FStaticMeshSourceModel& SourceModel = Source->GetSourceModel(0);
    const FStaticMeshSourceModel& CandidateModel = Candidate->GetSourceModel(0);
    if (SourceModel.BuildSettings.BuildScale3D !=
            CandidateModel.BuildSettings.BuildScale3D ||
        SourceModel.BuildSettings.bGenerateLightmapUVs !=
            CandidateModel.BuildSettings.bGenerateLightmapUVs ||
        SourceModel.BuildSettings.bUseFullPrecisionUVs !=
            CandidateModel.BuildSettings.bUseFullPrecisionUVs ||
        SourceModel.BuildSettings.bRecomputeNormals !=
            CandidateModel.BuildSettings.bRecomputeNormals ||
        SourceModel.BuildSettings.bRecomputeTangents !=
            CandidateModel.BuildSettings.bRecomputeTangents ||
        SourceModel.BuildSettings.bUseMikkTSpace !=
            CandidateModel.BuildSettings.bUseMikkTSpace ||
        SourceModel.BuildSettings.bRemoveDegenerates !=
            CandidateModel.BuildSettings.bRemoveDegenerates ||
        Source->NaniteSettings.bEnabled !=
            Candidate->NaniteSettings.bEnabled ||
        Source->NaniteSettings.KeepPercentTriangles !=
            Candidate->NaniteSettings.KeepPercentTriangles ||
        Source->NaniteSettings.TrimRelativeError !=
            Candidate->NaniteSettings.TrimRelativeError ||
        Source->NaniteSettings.FallbackTarget !=
            Candidate->NaniteSettings.FallbackTarget ||
        Source->NaniteSettings.FallbackPercentTriangles !=
            Candidate->NaniteSettings.FallbackPercentTriangles ||
        Source->NaniteSettings.FallbackRelativeError !=
            Candidate->NaniteSettings.FallbackRelativeError)
    {
        OutError = TEXT("A junction-continuity candidate changed source build, tangent, UV precision, or Nanite/fallback settings.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool TriangleContainsVertexInstance(
    const FMeshDescription& Description,
    FTriangleID TriangleId,
    FVertexInstanceID InstanceId)
{
    if (!Description.IsTriangleValid(TriangleId))
    {
        return false;
    }
    for (const FVertexInstanceID CandidateId :
         Description.GetTriangleVertexInstances(TriangleId))
    {
        if (CandidateId == InstanceId)
        {
            return true;
        }
    }
    return false;
}

bool ProtectedDescriptionEqual(
    const FMeshDescription& Source,
    const FMeshDescription& Candidate,
    const FMeshSpec& Spec,
    const TMap<int32, FVector3f>& ExpectedNativeNormals,
    FString& OutError)
{
    if (Source.Vertices().Num() != Candidate.Vertices().Num() ||
        Source.Edges().Num() != Candidate.Edges().Num() ||
        Source.VertexInstances().Num() != Candidate.VertexInstances().Num() ||
        Source.Triangles().Num() != Candidate.Triangles().Num() ||
        Source.Polygons().Num() != Candidate.Polygons().Num() ||
        Source.PolygonGroups().Num() != Candidate.PolygonGroups().Num())
    {
        OutError = TEXT("Junction-continuity candidate changed a protected topology element count.");
        return false;
    }
    const FStaticMeshConstAttributes SourceAttributes(Source);
    const FStaticMeshConstAttributes CandidateAttributes(Candidate);
    const auto SourcePositions = SourceAttributes.GetVertexPositions();
    const auto CandidatePositions = CandidateAttributes.GetVertexPositions();
    const auto SourceNormals = SourceAttributes.GetVertexInstanceNormals();
    const auto CandidateNormals = CandidateAttributes.GetVertexInstanceNormals();
    const auto SourceTangents = SourceAttributes.GetVertexInstanceTangents();
    const auto CandidateTangents = CandidateAttributes.GetVertexInstanceTangents();
    const auto SourceSigns = SourceAttributes.GetVertexInstanceBinormalSigns();
    const auto CandidateSigns = CandidateAttributes.GetVertexInstanceBinormalSigns();
    const auto SourceUvs = SourceAttributes.GetVertexInstanceUVs();
    const auto CandidateUvs = CandidateAttributes.GetVertexInstanceUVs();
    const auto SourceColors = SourceAttributes.GetVertexInstanceColors();
    const auto CandidateColors = CandidateAttributes.GetVertexInstanceColors();
    const auto SourceGroups =
        SourceAttributes.GetPolygonGroupMaterialSlotNames();
    const auto CandidateGroups =
        CandidateAttributes.GetPolygonGroupMaterialSlotNames();
    if (!SourcePositions.IsValid() || !CandidatePositions.IsValid() ||
        !SourceNormals.IsValid() || !CandidateNormals.IsValid() ||
        !SourceTangents.IsValid() || !CandidateTangents.IsValid() ||
        !SourceSigns.IsValid() || !CandidateSigns.IsValid() ||
        !SourceUvs.IsValid() || !CandidateUvs.IsValid() ||
        SourceUvs.GetNumChannels() != CandidateUvs.GetNumChannels() ||
        !SourceColors.IsValid() || !CandidateColors.IsValid() ||
        !SourceGroups.IsValid() || !CandidateGroups.IsValid())
    {
        OutError = TEXT("Junction-continuity candidate lost a protected mesh attribute.");
        return false;
    }
    for (const FVertexID Id : Source.Vertices().GetElementIDs())
    {
        if (!Candidate.IsVertexValid(Id) ||
            SourcePositions.Get(Id) != CandidatePositions.Get(Id))
        {
            OutError = TEXT("Junction-continuity candidate changed a vertex position or identity.");
            return false;
        }
    }
    int32 ChangedNormalCount = 0;
    for (const FVertexInstanceID Id :
         Source.VertexInstances().GetElementIDs())
    {
        if (!Candidate.IsVertexInstanceValid(Id) ||
            Source.GetVertexInstanceVertex(Id) !=
                Candidate.GetVertexInstanceVertex(Id) ||
            SourceTangents.Get(Id) != CandidateTangents.Get(Id) ||
            SourceSigns.Get(Id) != CandidateSigns.Get(Id) ||
            SourceColors.Get(Id) != CandidateColors.Get(Id))
        {
            OutError = TEXT("Junction-continuity candidate changed a vertex-instance identity, tangent, binormal sign, or colour.");
            return false;
        }
        for (int32 Channel = 0;
             Channel < SourceUvs.GetNumChannels();
             ++Channel)
        {
            if (SourceUvs.Get(Id, Channel) !=
                CandidateUvs.Get(Id, Channel))
            {
                OutError = TEXT("Junction-continuity candidate changed a UV coordinate.");
                return false;
            }
        }
        const FVector3f* Expected = ExpectedNativeNormals.Find(Id.GetValue());
        if (Expected)
        {
            if (CandidateNormals.Get(Id) != *Expected ||
                SourceNormals.Get(Id) == CandidateNormals.Get(Id))
            {
                OutError = TEXT("A listed junction normal row was not replaced by its exact candidate normal.");
                return false;
            }
            ++ChangedNormalCount;
        }
        else if (SourceNormals.Get(Id) != CandidateNormals.Get(Id))
        {
            OutError = TEXT("An unlisted junction normal row changed.");
            return false;
        }
    }
    for (const FTriangleID Id : Source.Triangles().GetElementIDs())
    {
        if (!Candidate.IsTriangleValid(Id) ||
            Source.GetTrianglePolygonGroup(Id) !=
                Candidate.GetTrianglePolygonGroup(Id))
        {
            OutError = TEXT("Junction-continuity candidate changed a triangle group.");
            return false;
        }
        const TArrayView<const FVertexInstanceID> A =
            Source.GetTriangleVertexInstances(Id);
        const TArrayView<const FVertexInstanceID> B =
            Candidate.GetTriangleVertexInstances(Id);
        if (A.Num() != B.Num())
        {
            OutError = TEXT("Junction-continuity candidate changed triangle arity.");
            return false;
        }
        for (int32 Corner = 0; Corner < A.Num(); ++Corner)
        {
            if (A[Corner] != B[Corner])
            {
                OutError = TEXT("Junction-continuity candidate changed face winding or topology.");
                return false;
            }
        }
    }
    for (const FPolygonGroupID Id :
         Source.PolygonGroups().GetElementIDs())
    {
        if (!Candidate.IsPolygonGroupValid(Id) ||
            SourceGroups.Get(Id) != CandidateGroups.Get(Id))
        {
            OutError = TEXT("Junction-continuity candidate changed a polygon-group material identity.");
            return false;
        }
    }
    if (ChangedNormalCount != Spec.Overrides ||
        ExpectedNativeNormals.Num() != Spec.Overrides)
    {
        OutError = TEXT("Junction-continuity candidate did not change the exact role-specific normal roster.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ApplyExactNormalOverrides(
    const FMeshDescription& Source,
    const FMeshSpec& Spec,
    const TArray<FNormalOverride>& Overrides,
    FMeshDescription& OutCandidate,
    FString& OutError)
{
    if (!ValidateMeshDescriptionCensus(Source, Spec, OutError))
    {
        return false;
    }
    OutCandidate = Source;
    const FStaticMeshConstAttributes SourceAttributes(Source);
    FStaticMeshAttributes CandidateAttributes(OutCandidate);
    const auto SourcePositions = SourceAttributes.GetVertexPositions();
    const auto SourceNormals = SourceAttributes.GetVertexInstanceNormals();
    const auto SourceUvs = SourceAttributes.GetVertexInstanceUVs();
    const auto SourceGroups =
        SourceAttributes.GetPolygonGroupMaterialSlotNames();
    auto CandidateNormals = CandidateAttributes.GetVertexInstanceNormals();
    TMap<int32, FVector3f> ExpectedNativeNormals;

    for (const FNormalOverride& Row : Overrides)
    {
        if (Row.Role != Spec.Role)
        {
            continue;
        }
        const FVertexID VertexId(Row.VertexRecordIndex - 1);
        const FVertexInstanceID InstanceId(Row.NormalRecordIndex - 1);
        const FTriangleID TriangleId(Row.FaceRecordIndex - 1);
        const FVector3f ExpectedPosition =
            NativePosition(Row.SourceObjPositionCentimetres);
        const FVector2f ExpectedUv =
            NativeUv0(Row.SourceObjPositionCentimetres);
        const FVector3f ExpectedSourceNormal =
            NativeNormal(Row.SourceObjNormal);
        const FVector3f ExpectedCandidateNormal =
            NativeNormal(Row.CandidateObjNormal);
        if (!Source.IsVertexValid(VertexId) ||
            !Source.IsVertexInstanceValid(InstanceId) ||
            Source.GetVertexInstanceVertex(InstanceId) != VertexId ||
            !TriangleContainsVertexInstance(
                Source, TriangleId, InstanceId) ||
            SourcePositions.Get(VertexId) != ExpectedPosition ||
            !SourceUvs.Get(InstanceId, 0).Equals(ExpectedUv, 0.000001f) ||
            !SourceNormals.Get(InstanceId).Equals(
                ExpectedSourceNormal, 0.000001f) ||
            !FMath::IsNearlyEqual(
                ExpectedCandidateNormal.SizeSquared(),
                1.0f,
                0.000001f) ||
            SourceGroups.Get(Source.GetTrianglePolygonGroup(TriangleId)) !=
                FName(*RoadMaterialSlotName) ||
            ExpectedNativeNormals.Contains(InstanceId.GetValue()))
        {
            OutError = FString::Printf(
                TEXT("Normal override record %d does not exactly match the hash-pinned native source corner, face, UV0, normal, or road group."),
                Row.NormalRecordIndex);
            return false;
        }
        CandidateNormals.Set(InstanceId, ExpectedCandidateNormal);
        ExpectedNativeNormals.Add(
            InstanceId.GetValue(), ExpectedCandidateNormal);
    }
    if (ExpectedNativeNormals.Num() != Spec.Overrides ||
        !ProtectedDescriptionEqual(
            Source,
            OutCandidate,
            Spec,
            ExpectedNativeNormals,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact role-specific junction override count was not applied.");
        }
        return false;
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
    UMetaData* Metadata = Package && Package->HasMetaData()
        ? Package->GetMetaData()
        : nullptr;
    if (!Asset || !Metadata || Metadata->GetValue(Asset, Key) != Expected)
    {
        OutError = FString::Printf(
            TEXT("Junction-continuity asset '%s' lost exact metadata '%s'."),
            Asset ? *Asset->GetPathName() : TEXT("<null>"),
            Key);
        return false;
    }
    OutError.Reset();
    return true;
}

bool WriteCandidateMetadata(
    UStaticMesh* Candidate,
    const FMeshSpec& Spec,
    const FAdmission& Admission,
    FString& OutError)
{
    UPackage* Package = Candidate ? Candidate->GetOutermost() : nullptr;
    UMetaData* Metadata = Package ? Package->GetMetaData() : nullptr;
    if (!Candidate || !Metadata)
    {
        OutError = TEXT("Could not write junction-continuity candidate provenance metadata.");
        return false;
    }
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionCandidateContractSha256"),
        *CandidateContractSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionNormalOverrideTableSha256"),
        *NormalOverrideTableSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionSourceAuditSha256"),
        *SourceAuditSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionAcceptedR33ReceiptSha256"),
        *Admission.AcceptedR33Sha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionFutureAuthorizationSha256"),
        *Admission.FutureAuthorizationSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionSourceObjectPath"),
        *Spec.SourceObjectPath);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionSourceObjSha256"),
        Spec.Role == ESourceRole::Core ? *CoreObjSha256 : *FallbackObjSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionSourceRole"),
        Spec.Role == ESourceRole::Core ? TEXT("CORE") : TEXT("FALLBACK"));
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionNormalOverrideCount"),
        *LexToString(Spec.Overrides));
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionMutation"),
        TEXT("EXACT_LISTED_VERTEX_INSTANCE_NORMALS_ONLY"));
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionPreservation"),
        TEXT("POSITIONS_UVS_TANGENTS_COLOURS_TOPOLOGY_WINDING_GROUPS_SLOTS_TRANSFORMS_IDENTITIES_UNCHANGED"));
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_PublicRealmJunctionAuthority"),
        TEXT("RENDER_ONLY_NO_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"));
    OutError.Reset();
    return true;
}

bool ValidateCandidateMetadata(
    UStaticMesh* Candidate,
    const FMeshSpec& Spec,
    const FAdmission& Admission,
    FString& OutError)
{
    return ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionCandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionNormalOverrideTableSha256"),
               NormalOverrideTableSha256,
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionSourceAuditSha256"),
               SourceAuditSha256,
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionSourceObjectPath"),
               Spec.SourceObjectPath,
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionSourceObjSha256"),
               Spec.Role == ESourceRole::Core
                   ? CoreObjSha256
                   : FallbackObjSha256,
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionSourceRole"),
               Spec.Role == ESourceRole::Core
                   ? FString(TEXT("CORE"))
                   : FString(TEXT("FALLBACK")),
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionNormalOverrideCount"),
               LexToString(Spec.Overrides),
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionMutation"),
               TEXT("EXACT_LISTED_VERTEX_INSTANCE_NORMALS_ONLY"),
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionPreservation"),
               TEXT("POSITIONS_UVS_TANGENTS_COLOURS_TOPOLOGY_WINDING_GROUPS_SLOTS_TRANSFORMS_IDENTITIES_UNCHANGED"),
               OutError) &&
        ExactAssetMetadata(
               Candidate,
               TEXT("TRIAD_PublicRealmJunctionAuthority"),
               TEXT("RENDER_ONLY_NO_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"),
               OutError);
}

void BuildExpectedNativeNormals(
    const FMeshSpec& Spec,
    const TArray<FNormalOverride>& Overrides,
    TMap<int32, FVector3f>& OutNormals)
{
    OutNormals.Reset();
    for (const FNormalOverride& Row : Overrides)
    {
        if (Row.Role == Spec.Role)
        {
            OutNormals.Add(
                Row.NormalRecordIndex - 1,
                NativeNormal(Row.CandidateObjNormal));
        }
    }
}

bool ValidateCandidateMesh(
    UStaticMesh* Source,
    UStaticMesh* Candidate,
    const FMeshSpec& Spec,
    const FAdmission& Admission,
    FString& OutError)
{
    if (!ValidateStaticMeshBoundary(
            Source, Spec, Spec.SourceObjectPath, OutError) ||
        !ValidateStaticMeshBoundary(
            Candidate, Spec, Spec.CandidateObjectPath, OutError) ||
        !StaticMeshAssetStateEqual(Source, Candidate, Spec, OutError) ||
        !ValidateCandidateMetadata(
            Candidate, Spec, Admission, OutError))
    {
        return false;
    }
    FMeshDescription SourceDescription;
    FMeshDescription CandidateDescription;
    if (!Source->CloneMeshDescription(0, SourceDescription) ||
        !Candidate->CloneMeshDescription(0, CandidateDescription))
    {
        OutError = TEXT("A junction-continuity source or candidate lacks its exact LOD0 MeshDescription.");
        return false;
    }
    TMap<int32, FVector3f> ExpectedNativeNormals;
    BuildExpectedNativeNormals(
        Spec, Admission.Overrides, ExpectedNativeNormals);
    return ProtectedDescriptionEqual(
        SourceDescription,
        CandidateDescription,
        Spec,
        ExpectedNativeNormals,
        OutError);
}

bool LoadAndValidateSources(
    TArray<UStaticMesh*>& OutSources,
    FString& OutError)
{
    OutSources.Reset();
    FString PublicRealmReport;
    if (!TRIADIstanaExploreV5DPublicRealmAssetFactory::ValidateAssets(
            PublicRealmReport))
    {
        OutError = TEXT("The exact frozen PublicRealm predecessor is unavailable or invalid: ") +
            PublicRealmReport;
        return false;
    }
    const FMeshSpec Specs[] = {CoreSpec(), FallbackSpec()};
    for (const FMeshSpec& Spec : Specs)
    {
        UStaticMesh* Mesh = LoadExact<UStaticMesh>(Spec.SourceObjectPath);
        if (!ValidateStaticMeshBoundary(
                Mesh, Spec, Spec.SourceObjectPath, OutError))
        {
            return false;
        }
        OutSources.Add(Mesh);
    }
    if (OutSources.Num() != CandidateAssetCount ||
        OutSources.Contains(nullptr) || OutSources[0] == OutSources[1])
    {
        OutError = TEXT("The exact distinct Core/Fallback source mesh roster is mandatory.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool LoadAndValidateCandidates(
    const TArray<UStaticMesh*>& Sources,
    const FAdmission& Admission,
    TArray<UStaticMesh*>& OutCandidates,
    FString& OutError)
{
    OutCandidates.Reset();
    if (Sources.Num() != CandidateAssetCount || Sources.Contains(nullptr))
    {
        OutError = TEXT("Candidate validation requires the exact source pair.");
        return false;
    }
    const FMeshSpec Specs[] = {CoreSpec(), FallbackSpec()};
    for (int32 Index = 0; Index < CandidateAssetCount; ++Index)
    {
        UStaticMesh* Candidate =
            LoadExact<UStaticMesh>(Specs[Index].CandidateObjectPath);
        if (!ValidateCandidateMesh(
                Sources[Index],
                Candidate,
                Specs[Index],
                Admission,
                OutError))
        {
            return false;
        }
        OutCandidates.Add(Candidate);
    }
    OutError.Reset();
    return true;
}

void GetOutputObjectPaths(TArray<FString>& OutPaths)
{
    OutPaths.Reset(CandidateAssetCount);
    OutPaths.Add(CoreSpec().CandidateObjectPath);
    OutPaths.Add(FallbackSpec().CandidateObjectPath);
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
        OutError = TEXT("Junction-continuity namespace has no resolvable filesystem path.");
        return false;
    }
    FPaths::NormalizeDirectoryName(OutPhysicalRoot);
    OutPhysicalDirectories.Add(OutPhysicalRoot);
    for (const FString& ObjectPath : ExpectedObjectPaths)
    {
        if (ObjectPath.IsEmpty() || OutObjects.Contains(ObjectPath))
        {
            OutError = TEXT("Junction-continuity expected object roster contains an empty or duplicate path.");
            return false;
        }
        OutObjects.Add(ObjectPath);
        const FString PackagePath =
            FPackageName::ObjectPathToPackageName(ObjectPath);
        if (!IsPackageWithinNamespace(PackagePath, NamespaceRoot) ||
            OutPackages.Contains(PackagePath))
        {
            OutError = TEXT("Junction-continuity expected package roster escaped its isolated namespace or duplicated a package.");
            return false;
        }
        OutPackages.Add(PackagePath);
        FString PhysicalFile;
        if (!FPackageName::TryConvertLongPackageNameToFilename(
                PackagePath,
                PhysicalFile,
                FPackageName::GetAssetPackageExtension()))
        {
            OutError = TEXT("A junction-continuity output package has no exact .uasset filename.");
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
        OutError = TEXT("Junction-continuity physical namespace resolution drifted.");
        return false;
    }
    for (const FString& ObjectPath : Actual.ObjectPaths)
    {
        if (!ExpectedObjects.Contains(ObjectPath))
        {
            OutError = TEXT("Junction-continuity namespace contains an uncontracted asset: ") +
                ObjectPath;
            return false;
        }
    }
    for (const FString& PackagePath : Actual.PackagePaths)
    {
        if (!ExpectedPackages.Contains(PackagePath))
        {
            OutError = TEXT("Junction-continuity namespace contains an uncontracted loaded or physical package: ") +
                PackagePath;
            return false;
        }
    }
    for (const FString& PhysicalFile : Actual.PhysicalFiles)
    {
        if (!ExpectedPhysicalFiles.Contains(PhysicalFile))
        {
            OutError = TEXT("Junction-continuity namespace contains an uncontracted physical file: ") +
                PhysicalFile;
            return false;
        }
    }
    for (const FString& PhysicalDirectory : Actual.PhysicalDirectories)
    {
        if (!ExpectedPhysicalDirectories.Contains(PhysicalDirectory))
        {
            OutError = TEXT("Junction-continuity namespace contains an uncontracted physical directory: ") +
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
        OutError = TEXT("Junction-continuity namespace does not contain the exact two-object/package roster.");
        return false;
    }
    if (bRequirePhysicalAssetFiles &&
        (Actual.PhysicalFiles.Num() != ExpectedPhysicalFiles.Num() ||
         Actual.PhysicalFiles.Difference(ExpectedPhysicalFiles).Num() != 0 ||
         ExpectedPhysicalFiles.Difference(Actual.PhysicalFiles).Num() != 0 ||
         Actual.PhysicalDirectories.Difference(
             ExpectedPhysicalDirectories).Num() != 0 ||
         ExpectedPhysicalDirectories.Difference(
             Actual.PhysicalDirectories).Num() != 0))
    {
        OutError = TEXT("Saved junction-continuity namespace lacks the exact two physical .uasset files or directory closure.");
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
        OutError = TEXT("Editor asset subsystem unavailable during junction-continuity rollback.");
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
    const bool bLoadedDeleteSucceeded = LoadedAssets.IsEmpty() ||
        AssetSubsystem->DeleteLoadedAssets(LoadedAssets);
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
            TEXT("Fresh junction-continuity rollback could not prove cleanup (DeleteLoadedAssets=%s DeleteDirectory=%s physicalRootAbsent=%s namespaceEmpty=%s)."),
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

bool CreateCandidateMesh(
    UStaticMesh* Source,
    const FMeshSpec& Spec,
    const FAdmission& Admission,
    UStaticMesh*& OutCandidate,
    FString& OutError)
{
    OutCandidate = nullptr;
    if (!ValidateStaticMeshBoundary(
            Source, Spec, Spec.SourceObjectPath, OutError))
    {
        return false;
    }
    FMeshDescription SourceDescription;
    FMeshDescription CandidateDescription;
    if (!Source->CloneMeshDescription(0, SourceDescription) ||
        !ApplyExactNormalOverrides(
            SourceDescription,
            Spec,
            Admission.Overrides,
            CandidateDescription,
            OutError))
    {
        return false;
    }

    const FString PackagePath =
        FPackageName::ObjectPathToPackageName(Spec.CandidateObjectPath);
    const FString AssetName =
        FPackageName::GetLongPackageAssetName(PackagePath);
    UPackage* Package = CreatePackage(*PackagePath);
    UStaticMesh* Candidate = Package
        ? Cast<UStaticMesh>(StaticDuplicateObject(
              Source,
              Package,
              FName(*AssetName),
              RF_Public | RF_Standalone | RF_Transactional))
        : nullptr;
    if (!Candidate || Candidate == Source ||
        Candidate->GetPathName() != Spec.CandidateObjectPath)
    {
        OutError = TEXT("Could not allocate an isolated public-realm junction candidate mesh.");
        return false;
    }

    Candidate->Modify();
    FMeshDescription* Installed = Candidate->CreateMeshDescription(
        0, MoveTemp(CandidateDescription));
    if (!Installed || Installed->Vertices().Num() != Spec.VertexRecords ||
        Installed->VertexInstances().Num() != Spec.VertexRecords ||
        Installed->Triangles().Num() != Spec.Triangles)
    {
        OutError = TEXT("Could not install the topology-identical normal-only junction MeshDescription.");
        return false;
    }
    UStaticMesh::FCommitMeshDescriptionParams CommitParams;
    CommitParams.bMarkPackageDirty = false;
    CommitParams.bUseHashAsGuid = true;
    Candidate->CommitMeshDescription(0, CommitParams);
    Candidate->SetStaticMaterials(Source->GetStaticMaterials());
    Candidate->GetSectionInfoMap().CopyFrom(Source->GetSectionInfoMap());
    Candidate->GetOriginalSectionInfoMap().CopyFrom(
        Source->GetOriginalSectionInfoMap());
    if (!WriteCandidateMetadata(
            Candidate, Spec, Admission, OutError))
    {
        return false;
    }
    Candidate->PostEditChange();
    Candidate->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    OutCandidate = Candidate;
    OutError.Reset();
    return true;
}

bool CreateFreshCandidates(
    const TArray<UStaticMesh*>& Sources,
    const FAdmission& Admission,
    TArray<UObject*>& OutAssets,
    FString& OutError)
{
    OutAssets.Reset();
    if (Sources.Num() != CandidateAssetCount || Sources.Contains(nullptr))
    {
        OutError = TEXT("Junction-continuity creation requires the exact Core/Fallback source pair.");
        return false;
    }
    const FMeshSpec Specs[] = {CoreSpec(), FallbackSpec()};
    for (int32 Index = 0; Index < CandidateAssetCount; ++Index)
    {
        UStaticMesh* Candidate = nullptr;
        if (!CreateCandidateMesh(
                Sources[Index],
                Specs[Index],
                Admission,
                Candidate,
                OutError))
        {
            return false;
        }
        FAssetRegistryModule::AssetCreated(Candidate);
        OutAssets.Add(Candidate);
    }
    OutError.Reset();
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DPublicRealmJunctionContinuityEditorLibrary::
    InspectPublicRealmJunctionContinuityReceipts(
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
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_RECEIPT_INSPECTION_DENIED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_RECEIPTS_INSPECTED_ONLY executionAuthorized=false callerSuppliedHashesNeverAuthorizeExecution=true privateMaterializerInvoked=false sourceOnlyRead=true acceptedR33ReceiptHashMatchedCallerPinAndHumanAcceptedFields=true separateFutureAuthorizationHashMatchedCallerPinAndExactNarrowFieldRoster=true candidateContractNormalOverrideTableSourceAuditBuilderAndCoreFallbackSourcesHashPinned=true exactOverrideRows=355 coreRows=174 fallbackRows=181 assetsWritten=false mapOrComponentBindingModified=false existingCoreFallbackMeshesReplaced=false positionsUvsTangentsColoursTopologyWindingGroupsSlotsTransformsIdentitiesModified=false terrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false UnrealLaunched=false nativeCompileMaterializationColdReloadCapturePerformanceOrVisualAcceptanceClaimed=false");
    return true;
}

bool UTRIADIstanaExploreV5DPublicRealmJunctionContinuityEditorLibrary::
    MaterializeTrustedPublicRealmJunctionContinuityAssetsInternal(
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
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_MATERIALIZATION_DENIED: ") +
            Error;
        return false;
    }

    TArray<UStaticMesh*> Sources;
    if (!LoadAndValidateSources(Sources, Error))
    {
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_MATERIALIZATION_DENIED: ") +
            Error;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();

    TArray<FString> ExpectedPaths;
    GetOutputObjectPaths(ExpectedPaths);
    if (!NamespaceMatchesExpected(
            OutputRoot,
            ExpectedPaths,
            false,
            false,
            Error))
    {
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_UNEXPECTED_NAMESPACE_CONTENT_DENIED: ") +
            Error;
        return false;
    }
    const int32 ExistingCount = ExistingOutputPackageCount();
    if (ExistingCount == CandidateAssetCount)
    {
        if (!NamespaceMatchesExpected(
                OutputRoot,
                ExpectedPaths,
                true,
                true,
                Error))
        {
            OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_EXISTING_ASSETS_INVALID: ") +
                Error;
            return false;
        }
        // Saved assets are deliberately never reused as execution inputs by
        // this dormant source. A reviewed migration must handle any existing
        // namespace explicitly.
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_EXISTING_NAMESPACE_REUSE_DENIED existing=2 freshOnly=true reviewedMigrationOrRemovalRequired=true noExistingAssetAcceptedAsExactSourceDuplicate=true");
        return false;
    }
    if (ExistingCount != 0)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_PARTIAL_NAMESPACE_DENIED existing=%d expectedEither=0_or_2"),
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
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_OUTPUT_NAMESPACE_NOT_EMPTY_DENIED: ") +
            Error;
        return false;
    }
    const bool bOutputRootProvenEmptyAtEntry = true;

    TArray<UObject*> AssetsToSave;
    if (!CreateFreshCandidates(
            Sources, Admission, AssetsToSave, Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_CREATE_FAILED: ") +
            Error;
        return false;
    }
    TSet<FString> UniquePaths;
    for (const UObject* Asset : AssetsToSave)
    {
        UniquePaths.Add(Asset ? Asset->GetPathName() : FString());
    }
    TArray<UStaticMesh*> Candidates;
    if (AssetsToSave.Num() != CandidateAssetCount ||
        UniquePaths.Num() != CandidateAssetCount ||
        UniquePaths.Contains(FString()) ||
        !NamespaceMatchesExpected(
            OutputRoot,
            ExpectedPaths,
            true,
            false,
            Error) ||
        !LoadAndValidateCandidates(
            Sources, Admission, Candidates, Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_PRE_SAVE_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem ||
        !AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        Error = TEXT("EXACT_TWO_ASSET_SAVE_FAILED");
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_SAVE_FAILED: ") +
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
        !LoadAndValidateCandidates(
            Sources, Admission, Candidates, Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_POST_SAVE_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_PUBLIC_REALM_JUNCTION_CONTINUITY_ASSETS_MATERIALIZED created=2 coreNormalRows=174 fallbackNormalRows=181 exactListedNormalRows=355 positionsUvsTangentsColoursTopologyWindingPolygonGroupsMaterialSlotsSectionsBuildSettingsBoundsSourceIdentitiesPreserved=true sourceCoreFallbackMeshesAndObjsModified=false exactSourceFallbackPairStillMandatory=true recursiveNamespaceAndPhysicalFilesExact=true freshFailureRollbackScope=isolatedNamespaceProvenEmptyAtEntry mapOrComponentBindingModified=false runtimeSelectionOrActivationPerformed=false terrainCesiumCollisionNavigationLosRfSensorSimulationAuthorityModified=false nativeUBTUHTCompileColdReloadSharpBendFourWayCaptureNaniteRasterParityPerformanceRegressionAndHumanVisualAcceptanceStillRequired=true");
    return true;
}
