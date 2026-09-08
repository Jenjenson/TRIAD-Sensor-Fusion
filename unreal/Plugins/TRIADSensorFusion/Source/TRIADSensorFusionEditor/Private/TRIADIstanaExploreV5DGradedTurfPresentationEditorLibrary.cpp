#include "TRIADIstanaExploreV5DGradedTurfPresentationEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Ssl.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DGradedTurfPresentationActor.h"
#include "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#include <initializer_list>

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
constexpr int32 OutputAssetCount = 1;
constexpr int32 CandidateResponseExpressionCount = 25;
constexpr int32 AcceptedOverlayExpressionCount = 26;
constexpr int32 SourceTextureCount = 5;
constexpr int32 ProviderClipPointCount = 64;
constexpr int32 R29OwnedPlacementCount = 6144;
constexpr int32 R32OwnedPlacementCount = 4608;
constexpr int64 MaximumInputBytes = 2ll * 1024ll * 1024ll;
constexpr int64 AcceptedOverlayPackageBytes = 37844;
constexpr int64 OrdinaryGrassMaterialPackageBytes = -1;

const FString CandidateContractSha256(
    TEXT("473BFA2D1AA3B5B2F5E8762E8FADF1D9681EE083BD7068FED1C919BEF5956357"));
const FString MaterialIntentSha256(
    TEXT("93E0DF74EB12316559C5AEEC42FC79B879ACB8C61B7D0B878FFC141A0FBF79F6"));
const FString ProvenanceSha256(
    TEXT("5F4B304157AF89EE6DB7BE9D86A7424D70B4F8D0AF55A8DA88205AAE0B1BB051"));
const FString GrassSurfaceIntegrationContractSha256(
    TEXT("FF28F16C77992B7D48A3DCD20D0E068D9BE15CDF3D53382FB73578178C25F41D"));
const FString GroundVegetationContractSha256(
    TEXT("05B9B004726A3A21D2EB8B362B36FB27DB4B4575B65262FB09E2FE01901D1308"));
const FString R29ContractSha256(
    TEXT("8311A6A1BFD12A7C77243F0B2016782603C7D839D29FA438F2BB1E5B25A807D9"));
const FString R32ContractSha256(
    TEXT("8DED60CE751611306A29071B1E90AA47C17274EB601997C5BB0E420DA9413A89"));
const FString AcceptedOverlayPackageSha256(
    TEXT("4F976F96F28C57A3C9A98668460B8BF8F51824849D00F88BEDB8E10850FF6915"));
const FString OrdinaryGrassMaterialPackageSha256(
    TEXT("UNSET_GRADED_TURF_ACCEPTED_ORDINARY_GRASS_PACKAGE_SHA256_REQUIRES_REVIEWED_SOURCE_CHANGE"));

// Distinct deliberately invalid SHA-256 sentinels. Caller pins can authorize
// inspection only. A reviewed source edit must replace both values, followed
// by UHT/UBT and an explicit call-site review, before a write can be reached.
const FString TrustedAcceptedR33ReceiptSha256(
    TEXT("UNSET_GRADED_TURF_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));
const FString TrustedFutureAuthorizationSha256(
    TEXT("UNSET_GRADED_TURF_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));

const FString AcceptedR33Schema(
    TEXT("triad.istana_explore_v5d.r33_player0_capture.v1"));
const FString R33TransactionSchema(
    TEXT("triad.istana_explore_v5d.r33_cesium_world_terrain.native_transaction.v1"));
const FString FutureAuthorizationSchema(
    TEXT("triad.istana_public_view_explore_v5d.graded_turf_presentation_v2.future_transaction_authority.v1"));
const FString FutureOperation(
    TEXT("MATERIALIZE_GRADED_TURF_PRESENTATION_V2_MATERIAL_ONLY"));
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));

const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "GradedTurfPresentationIntegrationV2"));
const FString MaterialRoot(OutputRoot + TEXT("/Materials"));
const FString CandidateMaterialName(
    TEXT("M_IPV5D_GradedTurfPresentationV2"));
const FString CandidateMaterialPackagePath(
    MaterialRoot + TEXT("/") + CandidateMaterialName);
const FString CandidateMaterialObjectPath(
    CandidateMaterialPackagePath + TEXT(".") + CandidateMaterialName);
const FString AcceptedOverlayObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/"
         "M_IPV5D_LawnMacroVariation.M_IPV5D_LawnMacroVariation"));
const FString OrdinaryGrassMaterialObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Materials/"
          "M_IPV5D_OrdinaryDistanceGrassSurface."
          "M_IPV5D_OrdinaryDistanceGrassSurface"));
const FString OrdinaryGrassMaterialPackagePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Materials/"
         "M_IPV5D_OrdinaryDistanceGrassSurface"));

const TCHAR* const OrdinaryTextureRoles[] = {
    TEXT("BaseColor"),
    TEXT("NormalDX"),
    TEXT("Roughness"),
    TEXT("AmbientOcclusion"),
    TEXT("Height")};

const TCHAR* const OrdinaryTextureObjectPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_BaseColor.T_IPV5D_Grass001_BaseColor"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_NormalDX.T_IPV5D_Grass001_NormalDX"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_Roughness.T_IPV5D_Grass001_Roughness"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_AmbientOcclusion."
         "T_IPV5D_Grass001_AmbientOcclusion"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_Height.T_IPV5D_Grass001_Height")};

const FString CoreMaskDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_IRREGULAR_ELLIPSE_64_VERTEX_50M_OPAQUE_COLLAR_OUTWARD_DITHER_MASK_V5"));

// Independent pins for the upstream material graph. These intentionally do
// not derive from the live 25-node package being admitted: a drifted package
// must not become its own reference simply because its roster and count match.
const FString GrassPrimaryUvDescription(
    TEXT("TRIAD_OD_GRASS_PROVIDER_1P4M_PRIMARY_WORLD_UV"));
const FString GrassPrimaryUvCode(
    TEXT("return AbsoluteWorldPosition.xy / 140.0;"));
const FString GrassPhaseUvDescription(
    TEXT("TRIAD_OD_GRASS_PROVIDER_1P4M_ROTATED_PHASE_WORLD_UV"));
const FString GrassPhaseUvCode(
    TEXT("float2 worldM=AbsoluteWorldPosition.xy*0.01; return float2(-worldM.y,worldM.x)/1.4+float2(0.371,0.613);"));
const FString GrassBlendMaskDescription(
    TEXT("TRIAD_OD_GRASS_7M_SMOOTH_PHASE_BLEND_MASK"));
const FString GrassBlendMaskCode(
    TEXT("float2 p=AbsoluteWorldPosition.xy*0.01/7.0; float2 i=floor(p); float2 f=frac(p); f=f*f*(3.0-2.0*f); float4 h=frac(sin(float4(dot(i,float2(127.1,311.7)),dot(i+float2(1,0),float2(127.1,311.7)),dot(i+float2(0,1),float2(127.1,311.7)),dot(i+float2(1,1),float2(127.1,311.7))))*43758.5453); float n=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y); return smoothstep(0.2,0.8,n);"));
const FString GrassNormalBlendDescription(
    TEXT("TRIAD_OD_GRASS_INVERSE_ROTATE_PHASE_NORMAL_XY_90_BEFORE_BLEND"));
const FString GrassNormalBlendCode(
    TEXT("float3 phaseRotated=float3(PhaseNormal.y,-PhaseNormal.x,PhaseNormal.z); return lerp(PrimaryNormal,phaseRotated,BlendMask);"));
const FString GrassColorRoughnessDescription(
    TEXT("TRIAD_OD_GRASS_3P5M_8P4M_18P2M_COLOR_ROUGHNESS_RESPONSE"));
const FString GrassColorRoughnessCode(
    TEXT("float distanceM=length(AbsoluteWorldPosition-CameraPosition)*0.01; float2 p=AbsoluteWorldPosition.xy*0.01; float meso=sin(dot(p/3.5,float2(1.731,2.417))); float macroA=sin(dot(p/8.4,float2(-1.193,2.071))+0.73); float macroB=sin(dot(p/18.2,float2(0.817,-1.643))-1.11); float macro=0.6*macroA+0.4*macroB; float multiplier=clamp(1.0+0.045*meso+0.15*macro,0.82,1.18); float footprintM=max(length(ddx(p)),length(ddy(p))); float bandLimit=1.0-smoothstep(0.35,1.4,footprintM); float sourceWeight=(1.0-smoothstep(50.0,70.0,distanceM))*bandLimit; float3 farBase=float3(0.058,0.105,0.022); float3 color=lerp(farBase,saturate(BaseColor*multiplier),sourceWeight); float rough=clamp(lerp(0.82,Roughness,sourceWeight),0.48,0.98); return float4(color,rough);"));
const FString GrassNormalDescription(
    TEXT("TRIAD_OD_GRASS_20M_35M_50M_NORMAL_RESPONSE"));
const FString GrassNormalCode(
    TEXT("float distanceM=length(AbsoluteWorldPosition-CameraPosition)*0.01; float strength=distanceM<=20.0?0.8:(distanceM<=35.0?lerp(0.8,0.62,(distanceM-20.0)/15.0):(distanceM<=50.0?lerp(0.62,0.48,(distanceM-35.0)/15.0):lerp(0.48,0.0,smoothstep(50.0,70.0,distanceM)))); float3 n=normalize(NormalDirectX); n.xy*=max(strength,0.0); return normalize(n);"));
const FString GrassAoDescription(
    TEXT("TRIAD_OD_GRASS_AO_STRENGTH_0P7_DISTANCE_RESPONSE"));
const FString GrassAoCode(
    TEXT("float distanceM=length(AbsoluteWorldPosition-CameraPosition)*0.01; float sourceWeight=1.0-smoothstep(50.0,70.0,distanceM); float retainedHeight=max(RetainedHeightNoDisplacement,0.0); return lerp(1.0,lerp(1.0,AmbientOcclusion,0.7),sourceWeight)+retainedHeight*0.0;"));

struct FPinnedSource
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const FString* Sha256;
};

const FPinnedSource PinnedSources[] = {
    {TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceCandidate/ordinary_distance_grass_surface.contract.json"), 5345, &CandidateContractSha256},
    {TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceCandidate/Generated/ordinary_distance_grass_material_intent.json"), 3402, &MaterialIntentSha256},
    {TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceCandidate/Provenance/grass001_local_reuse.provenance.json"), 6134, &ProvenanceSha256},
    {TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/GrassSurfaceIntegration/grass_surface_post_r33.source_contract.v1.json"), 18549, &GrassSurfaceIntegrationContractSha256},
    {TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/GroundVegetation/istana_public_view_v5d_ground_vegetation.contract.json"), 76277, &GroundVegetationContractSha256},
    {TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/R29TropicalDetail/r29_tropical_vegetation.contract.json"), 7788, &R29ContractSha256},
    {TEXT("unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/R32MediumDistanceTurf/r32_medium_distance_turf.contract.json"), 26288, &R32ContractSha256}};

static_assert(UE_ARRAY_COUNT(OrdinaryTextureObjectPaths) == SourceTextureCount);
static_assert(UE_ARRAY_COUNT(OrdinaryTextureRoles) == SourceTextureCount);
static_assert(UE_ARRAY_COUNT(PinnedSources) == 7);
static_assert(R29OwnedPlacementCount == 6144);
static_assert(R32OwnedPlacementCount == 4608);
static_assert(ProviderClipPointCount == 64);

struct FAdmission
{
    FString CandidateRoot;
    FString AcceptedR33Sha256;
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
        !IsSha256(ExpectedSha256) ||
        IFileManager::Get().FileSize(*AbsolutePath) != ExpectedBytes ||
        !FFileHelper::LoadFileToArray(OutBytes, *AbsolutePath) ||
        OutBytes.Num() != ExpectedBytes)
    {
        OutError = TEXT("A graded-turf source or receipt is absent, unbounded, relative, or byte-drifted.");
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
        OutError = TEXT("A graded-turf source or receipt failed its immutable SHA-256 pin.");
        return false;
    }
#else
    OutError = TEXT("Graded-turf source admission requires WITH_SSL SHA-256 support.");
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
        OutError = TEXT("A hash-pinned graded-turf JSON document could not be parsed.");
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
        !ExactBool(Root, TEXT("HumanVisualReviewAttested"), true) ||
        !ExactBool(Root, TEXT("AutomaticVisualAcceptanceAllowed"), false) ||
        !ExactBool(Root, TEXT("VisualReviewAccepted"), true) ||
        !ExactBool(
            Root, TEXT("R33CesiumWorldTerrainVisualQaAccepted"), true) ||
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
        OutError = TEXT("The accepted-R33 gate is not the exact human-reviewed, eight-image, no-mutation receipt.");
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
        TEXT("ProvenanceSha256"), TEXT("GrassSurfaceIntegrationContractSha256"),
        TEXT("GroundVegetationContractSha256"), TEXT("R29ContractSha256"),
        TEXT("R32ContractSha256"), TEXT("AcceptedOverlayPackageSha256"),
        TEXT("ExactOutputAssetCount"), TEXT("OutputNamespace"),
        TEXT("UnnumberedSuccessor"), TEXT("ExplicitExecutionAuthorized"),
        TEXT("AssetsOnlyEndpoint"), TEXT("ExactMaskedOverlayCloneRequired"),
        TEXT("ExactOpacityMaskSubgraphPreserved"),
        TEXT("OnlyBaseColorRoughnessNormalAoChanged"),
        TEXT("ExactFallbackRestorationRequired"),
        TEXT("R29AndR32PlacementOwnershipPreserved"),
        TEXT("MapMutationAuthorized"), TEXT("SourceMaterialMutationAuthorized"),
        TEXT("ComponentMutationAuthorized"),
        TEXT("GeographyTerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"),
        TEXT("NativeWriteOrUnrealLaunchPerformed")};
    if (!Root.IsValid() ||
        Root->Values.Num() != UE_ARRAY_COUNT(ExactFields))
    {
        OutError = TEXT("Future graded-turf authorization must contain exactly the narrow trusted field roster and no extensions.");
        return false;
    }
    for (const TCHAR* Field : ExactFields)
    {
        if (!Root->HasField(Field))
        {
            OutError = TEXT("Future graded-turf authorization omitted an exact required field.");
            return false;
        }
    }
    if (!ExactString(Root, TEXT("Schema"), FutureAuthorizationSchema) ||
        !ExactString(
            Root, TEXT("Status"), TEXT("AUTHORIZED_NOT_EXECUTED")) ||
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
        !ExactString(Root, TEXT("ProvenanceSha256"), ProvenanceSha256) ||
        !ExactString(
            Root,
            TEXT("GrassSurfaceIntegrationContractSha256"),
            GrassSurfaceIntegrationContractSha256) ||
        !ExactString(
            Root,
            TEXT("GroundVegetationContractSha256"),
            GroundVegetationContractSha256) ||
        !ExactString(Root, TEXT("R29ContractSha256"), R29ContractSha256) ||
        !ExactString(Root, TEXT("R32ContractSha256"), R32ContractSha256) ||
        !ExactString(
            Root,
            TEXT("AcceptedOverlayPackageSha256"),
            AcceptedOverlayPackageSha256) ||
        !ExactInteger(Root, TEXT("ExactOutputAssetCount"), OutputAssetCount) ||
        !ExactString(Root, TEXT("OutputNamespace"), OutputRoot) ||
        !ExactBool(Root, TEXT("UnnumberedSuccessor"), true) ||
        !ExactBool(Root, TEXT("ExplicitExecutionAuthorized"), true) ||
        !ExactBool(Root, TEXT("AssetsOnlyEndpoint"), true) ||
        !ExactBool(Root, TEXT("ExactMaskedOverlayCloneRequired"), true) ||
        !ExactBool(Root, TEXT("ExactOpacityMaskSubgraphPreserved"), true) ||
        !ExactBool(
            Root, TEXT("OnlyBaseColorRoughnessNormalAoChanged"), true) ||
        !ExactBool(Root, TEXT("ExactFallbackRestorationRequired"), true) ||
        !ExactBool(
            Root, TEXT("R29AndR32PlacementOwnershipPreserved"), true) ||
        !ExactBool(Root, TEXT("MapMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceMaterialMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("ComponentMutationAuthorized"), false) ||
        !ExactBool(
            Root,
            TEXT("GeographyTerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"),
            false) ||
        !ExactBool(
            Root, TEXT("NativeWriteOrUnrealLaunchPerformed"), false))
    {
        OutError = TEXT("Future graded-turf authorization lost its exact asset-only, mask-preserving, no-authority contract.");
        return false;
    }
    OutError.Reset();
    return true;
}

FString SourcePath(const FString& CandidateRoot, const TCHAR* RelativePath)
{
    FString Result = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(CandidateRoot, RelativePath));
    FPaths::NormalizeFilename(Result);
    return Result;
}

bool ValidatePinnedSourceClosure(
    const FString& CandidateRoot,
    FString& OutError)
{
    for (const FPinnedSource& Source : PinnedSources)
    {
        TArray<uint8> Bytes;
        if (!LoadPinnedBytes(
                SourcePath(CandidateRoot, Source.RelativePath),
                Source.Bytes,
                *Source.Sha256,
                Bytes,
                OutError))
        {
            return false;
        }
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
    FString FullRoot = FPaths::ConvertRelativePathToFull(CandidateRoot);
    FString FullR33 = FPaths::ConvertRelativePathToFull(
        AcceptedR33ReceiptPath);
    FString FullAuthorization = FPaths::ConvertRelativePathToFull(
        FutureTransactionAuthorizationPath);
    FPaths::NormalizeDirectoryName(FullRoot);
    FPaths::NormalizeFilename(FullR33);
    FPaths::NormalizeFilename(FullAuthorization);
    if (!IFileManager::Get().DirectoryExists(*FullRoot) ||
        FPaths::IsSamePath(FullR33, FullAuthorization) ||
        !IsSha256(ExpectedAcceptedR33ReceiptSha256) ||
        !IsSha256(ExpectedFutureTransactionAuthorizationSha256) ||
        ExpectedAcceptedR33ReceiptSha256.Equals(
            ExpectedFutureTransactionAuthorizationSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Graded-turf admission requires a repository root and two distinct absolute, hash-pinned receipts.");
        return false;
    }
    if (bRequireCompiledTrustAnchors &&
        (!IsSha256(TrustedAcceptedR33ReceiptSha256) ||
         !IsSha256(TrustedFutureAuthorizationSha256) ||
         !ExpectedAcceptedR33ReceiptSha256.Equals(
             TrustedAcceptedR33ReceiptSha256,
             ESearchCase::IgnoreCase) ||
         !ExpectedFutureTransactionAuthorizationSha256.Equals(
             TrustedFutureAuthorizationSha256,
             ESearchCase::IgnoreCase)))
    {
        OutError = TEXT("Graded-turf compiled trusted receipt anchors are unset; caller-supplied paths and hashes cannot authorize materialization.");
        return false;
    }
    if (!ValidatePinnedSourceClosure(FullRoot, OutError))
    {
        return false;
    }

    const int64 R33Bytes = IFileManager::Get().FileSize(*FullR33);
    const int64 AuthorizationBytes =
        IFileManager::Get().FileSize(*FullAuthorization);
    TSharedPtr<FJsonObject> AcceptedR33;
    TSharedPtr<FJsonObject> FutureAuthorization;
    if (R33Bytes < 2 || R33Bytes > MaximumInputBytes ||
        AuthorizationBytes < 2 ||
        AuthorizationBytes > MaximumInputBytes ||
        !ParsePinnedJson(
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

struct FNamespaceState
{
    TSet<FString> ObjectPaths;
    TSet<FString> PackagePaths;
    TSet<FString> PhysicalFiles;
    TSet<FString> PhysicalDirectories;
    FString PhysicalRoot;
    bool bPhysicalRootResolved = false;
};

bool IsInsideNamespace(
    const FString& PackageName,
    const FString& NamespaceRoot)
{
    return PackageName == NamespaceRoot ||
        PackageName.StartsWith(NamespaceRoot + TEXT("/"));
}

void CollectNamespaceState(
    const FString& NamespaceRoot,
    FNamespaceState& OutState)
{
    OutState = FNamespaceState{};
    OutState.bPhysicalRootResolved =
        FPackageName::TryConvertLongPackageNameToFilename(
            NamespaceRoot, OutState.PhysicalRoot);
    if (OutState.bPhysicalRootResolved)
    {
        FPaths::NormalizeDirectoryName(OutState.PhysicalRoot);
    }

    TArray<FAssetData> Assets;
    FAssetRegistryModule::GetRegistry().GetAssetsByPath(
        FName(*NamespaceRoot), Assets, true, true);
    for (const FAssetData& Asset : Assets)
    {
        OutState.ObjectPaths.Add(Asset.GetObjectPathString());
        OutState.PackagePaths.Add(Asset.PackageName.ToString());
    }
    for (TObjectIterator<UObject> It; It; ++It)
    {
        UObject* Object = *It;
        if (!Object || Object->HasAnyFlags(RF_ClassDefaultObject) ||
            !Object->IsAsset())
        {
            continue;
        }
        const FString PackageName = Object->GetOutermost()->GetName();
        if (IsInsideNamespace(PackageName, NamespaceRoot) &&
            Object->GetOutermost() != Object)
        {
            OutState.ObjectPaths.Add(Object->GetPathName());
            OutState.PackagePaths.Add(PackageName);
        }
    }
    for (TObjectIterator<UPackage> It; It; ++It)
    {
        if (IsInsideNamespace(It->GetName(), NamespaceRoot))
        {
            OutState.PackagePaths.Add(It->GetName());
        }
    }
    if (OutState.bPhysicalRootResolved &&
        IFileManager::Get().DirectoryExists(*OutState.PhysicalRoot))
    {
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
        for (FString File : Files)
        {
            FPaths::NormalizeFilename(File);
            OutState.PhysicalFiles.Add(File);
            FString PackageName;
            if (FPackageName::TryConvertFilenameToLongPackageName(
                    File, PackageName) &&
                IsInsideNamespace(PackageName, NamespaceRoot))
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

bool NamespaceIsEmpty(FString& OutError)
{
    FNamespaceState State;
    CollectNamespaceState(OutputRoot, State);
    const bool bPhysicalRootAbsent = State.bPhysicalRootResolved &&
        !IFileManager::Get().DirectoryExists(*State.PhysicalRoot);
    if (!State.bPhysicalRootResolved || !State.ObjectPaths.IsEmpty() ||
        !State.PackagePaths.IsEmpty() || !State.PhysicalFiles.IsEmpty() ||
        !State.PhysicalDirectories.IsEmpty() || !bPhysicalRootAbsent)
    {
        OutError = TEXT("The graded-turf V2 output namespace is not recursively empty across registry, live objects, packages, files, and directories.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool NamespaceHasExactOutput(FString& OutError)
{
    FNamespaceState State;
    CollectNamespaceState(OutputRoot, State);
    FString ExpectedFile;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            CandidateMaterialPackagePath,
            ExpectedFile,
            FPackageName::GetAssetPackageExtension()))
    {
        OutError = TEXT("Could not resolve the exact graded-turf output file.");
        return false;
    }
    FPaths::NormalizeFilename(ExpectedFile);
    if (State.ObjectPaths.Num() != 1 ||
        !State.ObjectPaths.Contains(CandidateMaterialObjectPath) ||
        State.PackagePaths.Num() != 1 ||
        !State.PackagePaths.Contains(CandidateMaterialPackagePath) ||
        State.PhysicalFiles.Num() != 1 ||
        !State.PhysicalFiles.Contains(ExpectedFile))
    {
        OutError = TEXT("The graded-turf namespace does not contain exactly one contracted material object, package, and .uasset file.");
        return false;
    }
    for (const FString& Directory : State.PhysicalDirectories)
    {
        if (!FPaths::IsUnderDirectory(Directory, State.PhysicalRoot))
        {
            OutError = TEXT("A graded-turf physical directory escaped its isolated root.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool DeleteFreshNamespaceContents(FString& OutError)
{
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutError = TEXT("Editor asset subsystem unavailable during graded-turf rollback.");
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
            TEXT("Fresh graded-turf rollback failed (DeleteLoadedAssets=%s DeleteDirectory=%s physicalRootAbsent=%s namespaceEmpty=%s)."),
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
        InOutError += TEXT(" ROLLBACK_REFUSED: fresh isolated namespace was not proven empty at entry.");
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

void CollectDependencies(
    UMaterialExpression* Expression,
    TSet<UMaterialExpression*>& OutDependencies)
{
    if (!Expression || OutDependencies.Contains(Expression))
    {
        return;
    }
    OutDependencies.Add(Expression);
    for (FExpressionInput* Input : Expression->GetInputsView())
    {
        if (Input)
        {
            CollectDependencies(Input->Expression, OutDependencies);
        }
    }
}

bool ExpressionPayloadEquivalent(
    const UMaterialExpression* Source,
    const UMaterialExpression* Candidate)
{
    if (!Source || !Candidate || Source->GetClass() != Candidate->GetClass() ||
        Source->Desc != Candidate->Desc)
    {
        return false;
    }
    if (const auto* SourceCustom = Cast<UMaterialExpressionCustom>(Source))
    {
        const auto* CandidateCustom =
            Cast<UMaterialExpressionCustom>(Candidate);
        if (!CandidateCustom ||
            SourceCustom->Description != CandidateCustom->Description ||
            SourceCustom->Code != CandidateCustom->Code ||
            SourceCustom->OutputType != CandidateCustom->OutputType ||
            !SourceCustom->AdditionalOutputs.IsEmpty() ||
            !CandidateCustom->AdditionalOutputs.IsEmpty() ||
            !SourceCustom->AdditionalDefines.IsEmpty() ||
            !CandidateCustom->AdditionalDefines.IsEmpty() ||
            !SourceCustom->IncludeFilePaths.IsEmpty() ||
            !CandidateCustom->IncludeFilePaths.IsEmpty() ||
            SourceCustom->Inputs.Num() != CandidateCustom->Inputs.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < SourceCustom->Inputs.Num(); ++Index)
        {
            if (SourceCustom->Inputs[Index].InputName !=
                CandidateCustom->Inputs[Index].InputName)
            {
                return false;
            }
        }
    }
    if (const auto* SourceSample =
            Cast<UMaterialExpressionTextureSample>(Source))
    {
        const auto* CandidateSample =
            Cast<UMaterialExpressionTextureSample>(Candidate);
        if (!CandidateSample || SourceSample->Texture != CandidateSample->Texture ||
            SourceSample->SamplerType != CandidateSample->SamplerType ||
            SourceSample->MipValueMode != CandidateSample->MipValueMode)
        {
            return false;
        }
    }
    if (const auto* SourceParameter =
            Cast<UMaterialExpressionTextureSampleParameter2D>(Source))
    {
        const auto* CandidateParameter =
            Cast<UMaterialExpressionTextureSampleParameter2D>(Candidate);
        if (!CandidateParameter ||
            SourceParameter->ParameterName != CandidateParameter->ParameterName)
        {
            return false;
        }
    }
    if (const auto* SourceMask =
            Cast<UMaterialExpressionComponentMask>(Source))
    {
        const auto* CandidateMask =
            Cast<UMaterialExpressionComponentMask>(Candidate);
        if (!CandidateMask || SourceMask->R != CandidateMask->R ||
            SourceMask->G != CandidateMask->G ||
            SourceMask->B != CandidateMask->B ||
            SourceMask->A != CandidateMask->A)
        {
            return false;
        }
    }
    if (const auto* SourceScalar = Cast<UMaterialExpressionConstant>(Source))
    {
        const auto* CandidateScalar =
            Cast<UMaterialExpressionConstant>(Candidate);
        if (!CandidateScalar || SourceScalar->R != CandidateScalar->R)
        {
            return false;
        }
    }
    if (const auto* SourceVector =
            Cast<UMaterialExpressionConstant3Vector>(Source))
    {
        const auto* CandidateVector =
            Cast<UMaterialExpressionConstant3Vector>(Candidate);
        if (!CandidateVector ||
            SourceVector->Constant != CandidateVector->Constant)
        {
            return false;
        }
    }
    if (const auto* SourceScalarParameter =
            Cast<UMaterialExpressionScalarParameter>(Source))
    {
        const auto* CandidateScalarParameter =
            Cast<UMaterialExpressionScalarParameter>(Candidate);
        if (!CandidateScalarParameter ||
            SourceScalarParameter->ParameterName !=
                CandidateScalarParameter->ParameterName ||
            SourceScalarParameter->DefaultValue !=
                CandidateScalarParameter->DefaultValue)
        {
            return false;
        }
    }
    if (const auto* SourceVectorParameter =
            Cast<UMaterialExpressionVectorParameter>(Source))
    {
        const auto* CandidateVectorParameter =
            Cast<UMaterialExpressionVectorParameter>(Candidate);
        if (!CandidateVectorParameter ||
            SourceVectorParameter->ParameterName !=
                CandidateVectorParameter->ParameterName ||
            SourceVectorParameter->DefaultValue !=
                CandidateVectorParameter->DefaultValue)
        {
            return false;
        }
    }
    if (const auto* SourceWorldPosition =
            Cast<UMaterialExpressionWorldPosition>(Source))
    {
        const auto* CandidateWorldPosition =
            Cast<UMaterialExpressionWorldPosition>(Candidate);
        if (!CandidateWorldPosition ||
            SourceWorldPosition->WorldPositionShaderOffset !=
                CandidateWorldPosition->WorldPositionShaderOffset)
        {
            return false;
        }
    }
    return true;
}

bool GraphEquivalentRecursive(
    UMaterialExpression* Source,
    UMaterialExpression* Candidate,
    TMap<const UMaterialExpression*, const UMaterialExpression*>& Seen)
{
    if (!Source || !Candidate)
    {
        return Source == Candidate;
    }
    if (const UMaterialExpression* const* Existing = Seen.Find(Source))
    {
        return *Existing == Candidate;
    }
    if (!ExpressionPayloadEquivalent(Source, Candidate))
    {
        return false;
    }
    Seen.Add(Source, Candidate);
    const TArrayView<FExpressionInput*> SourceInputs = Source->GetInputsView();
    const TArrayView<FExpressionInput*> CandidateInputs =
        Candidate->GetInputsView();
    if (SourceInputs.Num() != CandidateInputs.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < SourceInputs.Num(); ++Index)
    {
        const FExpressionInput* A = SourceInputs[Index];
        const FExpressionInput* B = CandidateInputs[Index];
        if (!A || !B || A->OutputIndex != B->OutputIndex || A->Mask != B->Mask ||
            A->MaskR != B->MaskR || A->MaskG != B->MaskG ||
            A->MaskB != B->MaskB || A->MaskA != B->MaskA ||
            !GraphEquivalentRecursive(A->Expression, B->Expression, Seen))
        {
            return false;
        }
    }
    return true;
}

bool GraphEquivalent(
    UMaterialExpression* Source,
    UMaterialExpression* Candidate)
{
    TMap<const UMaterialExpression*, const UMaterialExpression*> Seen;
    return GraphEquivalentRecursive(Source, Candidate, Seen);
}

bool InputAndGraphEquivalent(
    const FExpressionInput* Source,
    const FExpressionInput* Candidate)
{
    return Source && Candidate &&
        Source->OutputIndex == Candidate->OutputIndex &&
        Source->Mask == Candidate->Mask &&
        Source->MaskR == Candidate->MaskR &&
        Source->MaskG == Candidate->MaskG &&
        Source->MaskB == Candidate->MaskB &&
        Source->MaskA == Candidate->MaskA &&
        GraphEquivalent(Source->Expression, Candidate->Expression);
}

bool HasOnlyAuthorizedFinalPropertyConnections(UMaterial* Material)
{
    if (!Material)
    {
        return false;
    }
    const EMaterialProperty Authorized[] = {
        MP_BaseColor,
        MP_Roughness,
        MP_Normal,
        MP_AmbientOcclusion,
        MP_OpacityMask,
        MP_Specular};
    for (int32 Index = 0; Index < MP_MAX; ++Index)
    {
        const EMaterialProperty Property =
            static_cast<EMaterialProperty>(Index);
        bool bAuthorized = false;
        for (const EMaterialProperty Allowed : Authorized)
        {
            bAuthorized |= Property == Allowed;
        }
        FExpressionInput* Input =
            Material->GetExpressionInputForProperty(Property);
        if (!bAuthorized && Input && Input->Expression)
        {
            return false;
        }
    }
    return true;
}

bool AllFinalExpressionsBelongToAuthorizedRootClosures(
    UMaterial* Material,
    const UMaterialEditorOnlyData* Data)
{
    if (!Material || !Data)
    {
        return false;
    }
    const EMaterialProperty Authorized[] = {
        MP_BaseColor,
        MP_Roughness,
        MP_Normal,
        MP_AmbientOcclusion,
        MP_OpacityMask,
        MP_Specular};
    TSet<UMaterialExpression*> Reachable;
    for (const EMaterialProperty Property : Authorized)
    {
        FExpressionInput* Input =
            Material->GetExpressionInputForProperty(Property);
        if (!Input || !Input->Expression)
        {
            return false;
        }
        CollectDependencies(Input->Expression, Reachable);
    }
    if (Reachable.Num() != Data->ExpressionCollection.Expressions.Num())
    {
        return false;
    }
    for (UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        if (!Expression || !Reachable.Contains(Expression))
        {
            return false;
        }
    }
    return true;
}

bool ValidateExactAcceptedOverlay(
    UMaterial*& OutOverlay,
    FString& OutError)
{
    OutOverlay = LoadExact<UMaterial>(AcceptedOverlayObjectPath);
    FString OverlayFilename;
    const FString OverlayPackage =
        FPackageName::ObjectPathToPackageName(AcceptedOverlayObjectPath);
    TArray<uint8> PackageBytes;
    UMaterialEditorOnlyData* Data = OutOverlay
        ? OutOverlay->GetEditorOnlyData()
        : nullptr;
    UMaterialExpressionCustom* CoreMask = nullptr;
    int32 TextureSampleCount = 0;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            TextureSampleCount +=
                Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
            UMaterialExpressionCustom* Custom =
                Cast<UMaterialExpressionCustom>(Expression);
            if (Custom && Custom->Description == CoreMaskDescription)
            {
                if (CoreMask)
                {
                    OutError = TEXT("The accepted overlay contains more than one V5 estate/core-feather mask node.");
                    return false;
                }
                CoreMask = Custom;
            }
        }
    }
    FExpressionInput* OpacityMask = OutOverlay
        ? OutOverlay->GetExpressionInputForProperty(MP_OpacityMask)
        : nullptr;
    if (!OutOverlay || OutOverlay->GetClass() != UMaterial::StaticClass() ||
        !Data || OutOverlay->GetOutermost()->IsDirty() ||
        !FPackageName::DoesPackageExist(OverlayPackage, &OverlayFilename) ||
        !LoadPinnedBytes(
            OverlayFilename,
            AcceptedOverlayPackageBytes,
            AcceptedOverlayPackageSha256,
            PackageBytes,
            OutError) ||
        OutOverlay->MaterialDomain != MD_Surface ||
        OutOverlay->BlendMode != BLEND_Masked ||
        OutOverlay->OpacityMaskClipValue != 0.5f || OutOverlay->TwoSided ||
        !OutOverlay->bTangentSpaceNormal ||
        OutOverlay->bUseMaterialAttributes ||
        OutOverlay->bEnableTessellation ||
        OutOverlay->bEnableDisplacementFade ||
        OutOverlay->MaxWorldPositionOffsetDisplacement != 0.0f ||
        !OutOverlay->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        Data->ExpressionCollection.Expressions.Num() !=
            AcceptedOverlayExpressionCount ||
        TextureSampleCount != 4 || !CoreMask || !OpacityMask ||
        OpacityMask->Expression != CoreMask || OpacityMask->OutputIndex != 0 ||
        CoreMask->OutputType != CMOT_Float1 || CoreMask->Inputs.Num() != 1 ||
        CoreMask->Inputs[0].InputName != TEXT("WorldPosition") ||
        !CoreMask->Inputs[0].Input.Expression ||
        !CoreMask->Code.Contains(TEXT("const int edgeCount = 64;")) ||
        !CoreMask->Code.Contains(
            TEXT("const float opaqueCollarCm = 5000.000000000;")) ||
        !CoreMask->Code.Contains(
            TEXT("const float outwardFeatherCm = 800.000000000;")) ||
        !CoreMask->Code.Contains(
            TEXT("const float ditherCellCm = 25.000000000;")) ||
        OutOverlay->IsPropertyConnected(MP_WorldPositionOffset) ||
        OutOverlay->IsPropertyConnected(MP_PixelDepthOffset) ||
        OutOverlay->IsPropertyConnected(MP_Displacement))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The live lawn fallback is not the exact clean R23B Masked overlay with its V5 64-point/50 m collar/8 m feather/25 cm dither mask.");
        }
        OutOverlay = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

FString GrassTextureSampleNodeId(int32 Index, bool bPhase)
{
    return FString::Printf(
        TEXT("ODGrass.%s.%s"),
        OrdinaryTextureRoles[Index],
        bPhase ? TEXT("Phase") : TEXT("Primary"));
}

FString GrassTextureParameterName(int32 Index, bool bPhase)
{
    return FString::Printf(
        TEXT("%s_%s_Provider1p4m"),
        OrdinaryTextureRoles[Index],
        bPhase ? TEXT("Phase") : TEXT("Primary"));
}

FString GrassTextureBlendNodeId(int32 Index)
{
    return FString::Printf(
        TEXT("ODGrass.%s.DualPhaseBlend"), OrdinaryTextureRoles[Index]);
}

bool GrassInputIs(
    const FExpressionInput& Input,
    const UMaterialExpression* Expression,
    int32 OutputIndex = 0)
{
    return Input.Expression == Expression && Input.OutputIndex == OutputIndex;
}

template <typename TExpression>
const TExpression* FindExactGrassNode(
    const UMaterialEditorOnlyData* Data,
    const FString& NodeId)
{
    const TExpression* Match = nullptr;
    if (!Data)
    {
        return nullptr;
    }
    for (const UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        if (Expression && Expression->Desc == NodeId)
        {
            if (Match || !Cast<TExpression>(Expression))
            {
                return nullptr;
            }
            Match = Cast<TExpression>(Expression);
        }
    }
    return Match;
}

bool GrassCustomIsExact(
    const UMaterialExpressionCustom* Custom,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    std::initializer_list<const TCHAR*> InputNames,
    std::initializer_list<const UMaterialExpression*> InputExpressions)
{
    if (!Custom || Custom->Description != Description ||
        Custom->Code != Code || Custom->OutputType != OutputType ||
        !Custom->AdditionalOutputs.IsEmpty() ||
        !Custom->AdditionalDefines.IsEmpty() ||
        !Custom->IncludeFilePaths.IsEmpty() ||
        Custom->Inputs.Num() != static_cast<int32>(InputNames.size()) ||
        InputNames.size() != InputExpressions.size())
    {
        return false;
    }
    auto ExpressionIt = InputExpressions.begin();
    int32 Index = 0;
    for (const TCHAR* InputName : InputNames)
    {
        if (Custom->Inputs[Index].InputName != InputName ||
            !GrassInputIs(Custom->Inputs[Index].Input, *ExpressionIt))
        {
            return false;
        }
        ++Index;
        ++ExpressionIt;
    }
    return true;
}

bool GrassSampleIsExact(
    const UMaterialExpressionTextureSampleParameter2D* Sample,
    UTexture2D* Texture,
    int32 Index,
    bool bPhase,
    const UMaterialExpression* Coordinates)
{
    const EMaterialSamplerType ExpectedSampler = Index == 0
        ? SAMPLERTYPE_Color
        : (Index == 1 ? SAMPLERTYPE_Normal : SAMPLERTYPE_Masks);
    return Sample && Texture && Coordinates &&
        Sample->ParameterName ==
            FName(GrassTextureParameterName(Index, bPhase)) &&
        Sample->Texture == Texture && Sample->SamplerType == ExpectedSampler &&
        Sample->SamplerSource == SSM_FromTextureAsset &&
        Sample->MipValueMode == TMVM_None &&
        GrassInputIs(Sample->Coordinates, Coordinates);
}

bool GrassBlendIsExact(
    const UMaterialExpressionLinearInterpolate* Blend,
    int32 Index,
    const UMaterialExpression* Primary,
    const UMaterialExpression* Phase,
    const UMaterialExpression* BlendMask)
{
    const int32 SourceOutputIndex = Index >= 2 ? 1 : 0;
    return Blend &&
        GrassInputIs(Blend->A, Primary, SourceOutputIndex) &&
        GrassInputIs(Blend->B, Phase, SourceOutputIndex) &&
        GrassInputIs(Blend->Alpha, BlendMask);
}

bool GrassNormalBlendIsExact(
    const UMaterialExpressionCustom* Blend,
    const UMaterialExpression* Primary,
    const UMaterialExpression* Phase,
    const UMaterialExpression* BlendMask)
{
    return GrassCustomIsExact(
        Blend,
        GrassNormalBlendDescription,
        GrassNormalBlendCode,
        CMOT_Float3,
        {TEXT("PrimaryNormal"), TEXT("PhaseNormal"), TEXT("BlendMask")},
        {Primary, Phase, BlendMask});
}

bool ExactGrassMetadata(
    const UMaterial* Material,
    const TCHAR* Key,
    const FString& Expected)
{
    UPackage* Package = Material ? Material->GetOutermost() : nullptr;
    UMetaData* Metadata = Package && Package->HasMetaData()
        ? Package->GetMetaData()
        : nullptr;
    return Material && Metadata &&
        Metadata->GetValue(Material, Key) == Expected;
}

bool ValidatePinnedOrdinaryGrassGraph(
    UMaterial* Material,
    const TArray<TObjectPtr<UTexture2D>>& Textures,
    FString& OutError)
{
    if (Material)
    {
        // This cache is derived rather than serialized; rebuild it so the
        // independently pinned graph is judged from the persisted package.
        Material->BuildEditorParameterList();
    }
    const UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const auto* World = FindExactGrassNode<UMaterialExpressionWorldPosition>(
        Data, TEXT("ODGrass.AbsoluteWorldPositionNoOffsets"));
    const auto* Camera =
        FindExactGrassNode<UMaterialExpressionCameraPositionWS>(
            Data, TEXT("ODGrass.CameraPositionWS"));
    const auto* PrimaryUv = FindExactGrassNode<UMaterialExpressionCustom>(
        Data, TEXT("ODGrass.ProviderPrimaryUv"));
    const auto* PhaseUv = FindExactGrassNode<UMaterialExpressionCustom>(
        Data, TEXT("ODGrass.ProviderPhaseUv"));
    const auto* BlendMask = FindExactGrassNode<UMaterialExpressionCustom>(
        Data, TEXT("ODGrass.PhaseBlendMask"));
    const auto* ColorRoughness =
        FindExactGrassNode<UMaterialExpressionCustom>(
            Data, TEXT("ODGrass.ColorRoughnessResponse"));
    const auto* Normal = FindExactGrassNode<UMaterialExpressionCustom>(
        Data, TEXT("ODGrass.NormalResponse"));
    const auto* Ao = FindExactGrassNode<UMaterialExpressionCustom>(
        Data, TEXT("ODGrass.AmbientOcclusionResponse"));
    const auto* ColorOutput =
        FindExactGrassNode<UMaterialExpressionComponentMask>(
            Data, TEXT("ODGrass.BaseColorOutput"));
    const auto* RoughnessOutput =
        FindExactGrassNode<UMaterialExpressionComponentMask>(
            Data, TEXT("ODGrass.RoughnessOutput"));
    const UMaterialExpressionTextureSampleParameter2D*
        Primary[SourceTextureCount] = {};
    const UMaterialExpressionTextureSampleParameter2D*
        Phase[SourceTextureCount] = {};
    const UMaterialExpression* Blends[SourceTextureCount] = {};
    for (int32 Index = 0; Index < SourceTextureCount; ++Index)
    {
        Primary[Index] =
            FindExactGrassNode<UMaterialExpressionTextureSampleParameter2D>(
                Data, GrassTextureSampleNodeId(Index, false));
        Phase[Index] =
            FindExactGrassNode<UMaterialExpressionTextureSampleParameter2D>(
                Data, GrassTextureSampleNodeId(Index, true));
        Blends[Index] = Index == 1
            ? static_cast<const UMaterialExpression*>(
                  FindExactGrassNode<UMaterialExpressionCustom>(
                      Data, GrassTextureBlendNodeId(Index)))
            : static_cast<const UMaterialExpression*>(
                  FindExactGrassNode<UMaterialExpressionLinearInterpolate>(
                      Data, GrassTextureBlendNodeId(Index)));
    }

    bool bValid = Material && Data &&
        Textures.Num() == SourceTextureCount && !Textures.Contains(nullptr) &&
        Data->ExpressionCollection.Expressions.Num() ==
            CandidateResponseExpressionCount &&
        World &&
        World->WorldPositionShaderOffset == WPT_ExcludeAllShaderOffsets &&
        Camera &&
        GrassCustomIsExact(
            PrimaryUv,
            GrassPrimaryUvDescription,
            GrassPrimaryUvCode,
            CMOT_Float2,
            {TEXT("AbsoluteWorldPosition")},
            {World}) &&
        GrassCustomIsExact(
            PhaseUv,
            GrassPhaseUvDescription,
            GrassPhaseUvCode,
            CMOT_Float2,
            {TEXT("AbsoluteWorldPosition")},
            {World}) &&
        GrassCustomIsExact(
            BlendMask,
            GrassBlendMaskDescription,
            GrassBlendMaskCode,
            CMOT_Float1,
            {TEXT("AbsoluteWorldPosition")},
            {World});
    for (int32 Index = 0; bValid && Index < SourceTextureCount; ++Index)
    {
        bValid = GrassSampleIsExact(
                     Primary[Index],
                     Textures[Index],
                     Index,
                     false,
                     PrimaryUv) &&
            GrassSampleIsExact(
                     Phase[Index],
                     Textures[Index],
                     Index,
                     true,
                     PhaseUv) &&
            (Index == 1
                 ? GrassNormalBlendIsExact(
                       Cast<UMaterialExpressionCustom>(Blends[Index]),
                       Primary[Index],
                       Phase[Index],
                       BlendMask)
                 : GrassBlendIsExact(
                       Cast<UMaterialExpressionLinearInterpolate>(
                           Blends[Index]),
                       Index,
                       Primary[Index],
                       Phase[Index],
                       BlendMask));
    }
    bValid = bValid &&
        GrassCustomIsExact(
            ColorRoughness,
            GrassColorRoughnessDescription,
            GrassColorRoughnessCode,
            CMOT_Float4,
            {TEXT("BaseColor"),
             TEXT("Roughness"),
             TEXT("AbsoluteWorldPosition"),
             TEXT("CameraPosition")},
            {Blends[0], Blends[2], World, Camera}) &&
        GrassCustomIsExact(
            Normal,
            GrassNormalDescription,
            GrassNormalCode,
            CMOT_Float3,
            {TEXT("NormalDirectX"),
             TEXT("AbsoluteWorldPosition"),
             TEXT("CameraPosition")},
            {Blends[1], World, Camera}) &&
        GrassCustomIsExact(
            Ao,
            GrassAoDescription,
            GrassAoCode,
            CMOT_Float1,
            {TEXT("AmbientOcclusion"),
             TEXT("RetainedHeightNoDisplacement"),
             TEXT("AbsoluteWorldPosition"),
             TEXT("CameraPosition")},
            {Blends[3], Blends[4], World, Camera}) &&
        ColorOutput && ColorOutput->R && ColorOutput->G && ColorOutput->B &&
        !ColorOutput->A && GrassInputIs(ColorOutput->Input, ColorRoughness) &&
        RoughnessOutput && !RoughnessOutput->R && !RoughnessOutput->G &&
        !RoughnessOutput->B && RoughnessOutput->A &&
        GrassInputIs(RoughnessOutput->Input, ColorRoughness) &&
        GrassInputIs(Data->BaseColor, ColorOutput) &&
        GrassInputIs(Data->Roughness, RoughnessOutput) &&
        GrassInputIs(Data->Normal, Normal) &&
        GrassInputIs(Data->AmbientOcclusion, Ao);
    if (bValid)
    {
        if (Material->EditorParameters.Num() != SourceTextureCount * 2)
        {
            bValid = false;
        }
        for (const UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            bValid &= Expression && Expression->Material == Material &&
                Expression->MaterialExpressionGuid.IsValid();
        }
        for (int32 Index = 0; bValid && Index < SourceTextureCount; ++Index)
        {
            const UMaterialExpressionTextureSampleParameter2D* Samples[] = {
                Primary[Index], Phase[Index]};
            for (const UMaterialExpressionTextureSampleParameter2D* Sample :
                 Samples)
            {
                const TArray<UMaterialExpression*>* Registered = Sample
                    ? Material->EditorParameters.Find(Sample->ParameterName)
                    : nullptr;
                bValid &= Sample && Sample->ExpressionGUID.IsValid() &&
                    Registered && Registered->Num() == 1 &&
                    (*Registered)[0] == Sample;
            }
        }
    }
    if (!bValid)
    {
        OutError = TEXT("The persisted ordinary-distance Grass001 source package lost its independently pinned 25-node code, UV mode, parameter roster, or graph topology.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateExactOrdinaryGrassSurface(
    UMaterial*& OutMaterial,
    TArray<TObjectPtr<UTexture2D>>& OutTextures,
    FString& OutError)
{
    OutTextures.Reset(SourceTextureCount);
    OutMaterial = nullptr;
    if (OrdinaryGrassMaterialPackageBytes < 2 ||
        !IsSha256(OrdinaryGrassMaterialPackageSha256))
    {
        OutError = TEXT("The accepted ordinary-distance Grass001 material package byte/SHA-256 dependency anchor is deliberately unset; a reviewed source change must pin its cold-reloaded package before V2 can materialize.");
        return false;
    }

    OutMaterial = LoadExact<UMaterial>(OrdinaryGrassMaterialObjectPath);
    for (const TCHAR* TexturePath : OrdinaryTextureObjectPaths)
    {
        OutTextures.Add(LoadExact<UTexture2D>(TexturePath));
    }
    FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceAssets Assets;
    Assets.SourceTextures = OutTextures;
    Assets.SurfaceMaterial = OutMaterial;
    UMaterialEditorOnlyData* Data = OutMaterial
        ? OutMaterial->GetEditorOnlyData()
        : nullptr;
    FString PackageFilename;
    TArray<uint8> PackageBytes;
    if (!OutMaterial || !Data || OutMaterial->GetOutermost()->IsDirty() ||
        OutMaterial->GetOutermost()->GetName() !=
            OrdinaryGrassMaterialPackagePath ||
        !FPackageName::DoesPackageExist(
            OrdinaryGrassMaterialPackagePath, &PackageFilename) ||
        !LoadPinnedBytes(
            PackageFilename,
            OrdinaryGrassMaterialPackageBytes,
            OrdinaryGrassMaterialPackageSha256,
            PackageBytes,
            OutError) ||
        !UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
            ValidateAssetRoster(Assets, OutError) ||
        !ExactGrassMetadata(
            OutMaterial,
            TEXT("TRIAD_GrassSurfaceCandidateContractSha256"),
            CandidateContractSha256) ||
        !ExactGrassMetadata(
            OutMaterial,
            TEXT("TRIAD_GrassSurfaceMaterialIntentSha256"),
            MaterialIntentSha256) ||
        !ExactGrassMetadata(
            OutMaterial,
            TEXT("TRIAD_GrassSurfaceProvenanceSha256"),
            ProvenanceSha256) ||
        !ExactGrassMetadata(
            OutMaterial,
            TEXT("TRIAD_GrassSurfacePhysicalTileMetres"),
            TEXT("1.4")) ||
        !ExactGrassMetadata(
            OutMaterial,
            TEXT("TRIAD_GrassSurfaceDistancePolicyMetres"),
            TEXT("full_source_through_20_normal_0.8_0.62_0.48_at_20_35_50_fade_50_70")) ||
        !ExactGrassMetadata(
            OutMaterial,
            TEXT("TRIAD_GrassSurfaceHeightRoute"),
            TEXT("RETAINED_INPUT_NO_DISPLACEMENT_NO_GEOMETRY_AUTHORITY")) ||
        !ExactGrassMetadata(
            OutMaterial,
            TEXT("TRIAD_GrassSurfaceOptionalFallback"),
            AcceptedOverlayObjectPath) ||
        !ValidatePinnedOrdinaryGrassGraph(
            OutMaterial, OutTextures, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact clean hash-pinned ordinary-distance Grass001 package, immutable metadata, five-texture roster, and independently pinned 25-node graph are mandatory.");
        }
        OutMaterial = nullptr;
        OutTextures.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool CopyExactResponseGraph(
    UMaterial* Source,
    UMaterial* Target,
    FString& OutError)
{
    UMaterialEditorOnlyData* SourceData = Source
        ? Source->GetEditorOnlyData()
        : nullptr;
    UMaterialEditorOnlyData* TargetData = Target
        ? Target->GetEditorOnlyData()
        : nullptr;
    if (!SourceData || !TargetData ||
        SourceData->ExpressionCollection.Expressions.Num() !=
            CandidateResponseExpressionCount)
    {
        OutError = TEXT("Grass001 response graft requires exact source and target editor graphs.");
        return false;
    }

    TMap<UMaterialExpression*, UMaterialExpression*> Remap;
    for (UMaterialExpression* SourceExpression :
         SourceData->ExpressionCollection.Expressions)
    {
        UMaterialExpression* Duplicate =
            UMaterialEditingLibrary::DuplicateMaterialExpression(
                Target, nullptr, SourceExpression);
        if (!SourceExpression || !Duplicate ||
            Duplicate->GetOuter() != Target)
        {
            OutError = TEXT("Could not duplicate every exact Grass001 response expression into the isolated target material.");
            return false;
        }
        Remap.Add(SourceExpression, Duplicate);
    }
    if (Remap.Num() != CandidateResponseExpressionCount)
    {
        OutError = TEXT("Grass001 response duplication lost the exact 25-node census.");
        return false;
    }

    for (const TPair<UMaterialExpression*, UMaterialExpression*>& Pair : Remap)
    {
        const TArrayView<FExpressionInput*> SourceInputs =
            Pair.Key->GetInputsView();
        const TArrayView<FExpressionInput*> TargetInputs =
            Pair.Value->GetInputsView();
        if (SourceInputs.Num() != TargetInputs.Num())
        {
            OutError = TEXT("A duplicated Grass001 node changed input arity.");
            return false;
        }
        for (int32 Index = 0; Index < SourceInputs.Num(); ++Index)
        {
            const FExpressionInput* SourceInput = SourceInputs[Index];
            FExpressionInput* TargetInput = TargetInputs[Index];
            if (!SourceInput || !TargetInput)
            {
                OutError = TEXT("A duplicated Grass001 node exposed an invalid input.");
                return false;
            }
            *TargetInput = *SourceInput;
            TargetInput->Expression = SourceInput->Expression
                ? Remap.FindRef(SourceInput->Expression)
                : nullptr;
            if (SourceInput->Expression && !TargetInput->Expression)
            {
                OutError = TEXT("Grass001 dependency remapping escaped the exact 25-node source graph.");
                return false;
            }
        }
    }

    const EMaterialProperty ResponseProperties[] = {
        MP_BaseColor, MP_Roughness, MP_Normal, MP_AmbientOcclusion};
    for (const EMaterialProperty Property : ResponseProperties)
    {
        FExpressionInput* SourceInput =
            Source->GetExpressionInputForProperty(Property);
        FExpressionInput* TargetInput =
            Target->GetExpressionInputForProperty(Property);
        if (!SourceInput || !TargetInput || !SourceInput->Expression ||
            !Remap.Contains(SourceInput->Expression))
        {
            OutError = TEXT("Grass001 source lost one of its four exact response outputs.");
            return false;
        }
        *TargetInput = *SourceInput;
        TargetInput->Expression = Remap.FindRef(SourceInput->Expression);
    }
    for (const TPair<UMaterialExpression*, UMaterialExpression*>& Pair : Remap)
    {
        if (!ExpressionPayloadEquivalent(Pair.Key, Pair.Value))
        {
            OutError = TEXT("A Grass001 response expression payload drifted during exact graph graft.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool StripOnlyOldResponseGraph(
    UMaterial* Target,
    FString& OutError)
{
    UMaterialEditorOnlyData* Data = Target
        ? Target->GetEditorOnlyData()
        : nullptr;
    FExpressionInput* MaskInput = Target
        ? Target->GetExpressionInputForProperty(MP_OpacityMask)
        : nullptr;
    FExpressionInput* SpecularInput = Target
        ? Target->GetExpressionInputForProperty(MP_Specular)
        : nullptr;
    if (!Data || !MaskInput || !MaskInput->Expression ||
        !SpecularInput || !SpecularInput->Expression)
    {
        OutError = TEXT("The exact cloned overlay mask/specular boundary is absent before response replacement.");
        return false;
    }
    TSet<UMaterialExpression*> Protected;
    CollectDependencies(MaskInput->Expression, Protected);
    CollectDependencies(SpecularInput->Expression, Protected);
    if (Protected.Num() < 3 ||
        Protected.Num() >= AcceptedOverlayExpressionCount)
    {
        OutError = TEXT("The cloned overlay did not expose a bounded mask/specular dependency closure.");
        return false;
    }

    Data->BaseColor.Expression = nullptr;
    Data->Roughness.Expression = nullptr;
    Data->Normal.Expression = nullptr;
    Data->AmbientOcclusion.Expression = nullptr;
    const TArray<UMaterialExpression*> Before =
        Data->ExpressionCollection.Expressions;
    for (UMaterialExpression* Expression : Before)
    {
        if (!Protected.Contains(Expression))
        {
            UMaterialEditingLibrary::DeleteMaterialExpression(
                Target, Expression);
        }
    }
    if (Data->ExpressionCollection.Expressions.Num() != Protected.Num())
    {
        OutError = TEXT("Old overlay response deletion did not retain exactly the protected mask/specular closure.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool HasExactFinalTextureRoster(const UMaterial* Material)
{
    TSet<FString> Actual;
    if (!Material)
    {
        return false;
    }
    for (const TObjectPtr<UObject>& Texture : Material->GetReferencedTextures())
    {
        if (Texture)
        {
            Actual.Add(Texture->GetPathName());
        }
    }
    if (Actual.Num() != SourceTextureCount)
    {
        return false;
    }
    for (const TCHAR* Expected : OrdinaryTextureObjectPaths)
    {
        if (!Actual.Contains(Expected))
        {
            return false;
        }
    }
    return true;
}

bool ValidateFinalMaterial(
    UMaterial* AcceptedOverlay,
    UMaterial* OrdinaryGrass,
    UMaterial* Candidate,
    const FAdmission& Admission,
    FString& OutError)
{
    UMaterialEditorOnlyData* CandidateData = Candidate
        ? Candidate->GetEditorOnlyData()
        : nullptr;
    FExpressionInput* SourceMask = AcceptedOverlay
        ? AcceptedOverlay->GetExpressionInputForProperty(MP_OpacityMask)
        : nullptr;
    FExpressionInput* CandidateMask = Candidate
        ? Candidate->GetExpressionInputForProperty(MP_OpacityMask)
        : nullptr;
    FExpressionInput* SourceSpecular = AcceptedOverlay
        ? AcceptedOverlay->GetExpressionInputForProperty(MP_Specular)
        : nullptr;
    FExpressionInput* CandidateSpecular = Candidate
        ? Candidate->GetExpressionInputForProperty(MP_Specular)
        : nullptr;
    if (!Candidate || Candidate->GetClass() != UMaterial::StaticClass() ||
        Candidate->GetPathName() != CandidateMaterialObjectPath ||
        !CandidateData || Candidate->MaterialDomain != MD_Surface ||
        Candidate->BlendMode != BLEND_Masked || Candidate->TwoSided ||
        !Candidate->bTangentSpaceNormal || Candidate->bUseMaterialAttributes ||
        Candidate->OpacityMaskClipValue != 0.5f ||
        Candidate->bEnableTessellation ||
        Candidate->bEnableDisplacementFade ||
        Candidate->MaxWorldPositionOffsetDisplacement != 0.0f ||
        !Candidate->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !Candidate->HasBaseColorConnected() ||
        !Candidate->HasRoughnessConnected() ||
        !Candidate->HasNormalConnected() ||
        !Candidate->HasAmbientOcclusionConnected() ||
        !InputAndGraphEquivalent(SourceMask, CandidateMask) ||
        !InputAndGraphEquivalent(SourceSpecular, CandidateSpecular) ||
        !HasOnlyAuthorizedFinalPropertyConnections(Candidate) ||
        !AllFinalExpressionsBelongToAuthorizedRootClosures(
            Candidate, CandidateData) ||
        !HasExactFinalTextureRoster(Candidate))
    {
        OutError = TEXT("Final graded turf lost its exact cloned Masked overlay semantics, root-selector-exact mask/specular closures, exhaustive six-property allowlist, Grass001 texture roster, or no-authority graph boundary.");
        return false;
    }
    const EMaterialProperty ResponseProperties[] = {
        MP_BaseColor, MP_Roughness, MP_Normal, MP_AmbientOcclusion};
    for (const EMaterialProperty Property : ResponseProperties)
    {
        FExpressionInput* SourceResponse =
            OrdinaryGrass->GetExpressionInputForProperty(Property);
        FExpressionInput* CandidateResponse =
            Candidate->GetExpressionInputForProperty(Property);
        if (!InputAndGraphEquivalent(SourceResponse, CandidateResponse))
        {
            OutError = TEXT("Final graded turf did not retain one exact Grass001 dual-phase response dependency graph.");
            return false;
        }
    }
    UMetaData* Metadata = Candidate->GetOutermost()->HasMetaData()
        ? Candidate->GetOutermost()->GetMetaData()
        : nullptr;
    if (!Metadata ||
        Metadata->GetValue(
            Candidate,
            TEXT("TRIAD_GradedTurfAcceptedR33ReceiptSha256")) !=
            Admission.AcceptedR33Sha256 ||
        Metadata->GetValue(
            Candidate,
            TEXT("TRIAD_GradedTurfFutureAuthorizationSha256")) !=
            Admission.FutureAuthorizationSha256 ||
        Metadata->GetValue(
            Candidate,
            TEXT("TRIAD_GradedTurfExactMaskBoundary")) !=
            TEXT("R23B_MASKED_64_POINT_50M_CORE_8M_FEATHER_25CM_DITHER") ||
        Metadata->GetValue(
            Candidate,
            TEXT("TRIAD_GradedTurfPlacementOwnership")) !=
            TEXT("R29=6144;R32=4608;UNCHANGED") ||
        Metadata->GetValue(
            Candidate,
            TEXT("TRIAD_GradedTurfPresentationProofMetres")) !=
            TEXT("0-95_SOURCE_CONTRACT_NATIVE_CAPTURE_REQUIRED"))
    {
        OutError = TEXT("Final graded turf lost exact receipt, mask, ownership, or presentation-proof metadata.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool WriteExactMetadata(
    UMaterial* Candidate,
    const FAdmission& Admission,
    FString& OutError)
{
    UPackage* Package = Candidate ? Candidate->GetOutermost() : nullptr;
    UMetaData* Metadata = Package ? Package->GetMetaData() : nullptr;
    if (!Candidate || !Metadata)
    {
        OutError = TEXT("The isolated graded-turf material requires writable package metadata.");
        return false;
    }
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfCandidateContractSha256"),
        *CandidateContractSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfMaterialIntentSha256"),
        *MaterialIntentSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfProvenanceSha256"),
        *ProvenanceSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfGrassSurfaceIntegrationContractSha256"),
        *GrassSurfaceIntegrationContractSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfGroundVegetationContractSha256"),
        *GroundVegetationContractSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfR29ContractSha256"),
        *R29ContractSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfR32ContractSha256"),
        *R32ContractSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfAcceptedOverlayPackageSha256"),
        *AcceptedOverlayPackageSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfAcceptedR33ReceiptSha256"),
        *Admission.AcceptedR33Sha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfFutureAuthorizationSha256"),
        *Admission.FutureAuthorizationSha256);
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfExactMaskBoundary"),
        TEXT("R23B_MASKED_64_POINT_50M_CORE_8M_FEATHER_25CM_DITHER"));
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfResponseBoundary"),
        TEXT("GRASS001_1P4M_DUAL_PHASE_BASECOLOR_ROUGHNESS_NORMAL_AO_ONLY"));
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfPlacementOwnership"),
        TEXT("R29=6144;R32=4608;UNCHANGED"));
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfPresentationProofMetres"),
        TEXT("0-95_SOURCE_CONTRACT_NATIVE_CAPTURE_REQUIRED"));
    Metadata->SetValue(
        Candidate,
        TEXT("TRIAD_GradedTurfAuthority"),
        TEXT("APPEARANCE_ONLY_NO_MAP_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_SIMULATION_AUTHORITY"));
    OutError.Reset();
    return true;
}

bool CreateFreshMaterial(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    UMaterial*& OutCandidate,
    FString& OutError)
{
    OutCandidate = nullptr;
    UMaterial* AcceptedOverlay = nullptr;
    UMaterial* OrdinaryGrass = nullptr;
    TArray<TObjectPtr<UTexture2D>> OrdinaryTextures;
    if (!ValidateExactAcceptedOverlay(AcceptedOverlay, OutError) ||
        !ValidateExactOrdinaryGrassSurface(
            OrdinaryGrass, OrdinaryTextures, OutError))
    {
        return false;
    }

    OutCandidate = Cast<UMaterial>(AssetTools.DuplicateAsset(
        CandidateMaterialName,
        MaterialRoot,
        AcceptedOverlay));
    if (!OutCandidate ||
        OutCandidate->GetPathName() != CandidateMaterialObjectPath)
    {
        OutError = TEXT("Could not clone the exact accepted live masked lawn overlay into the fresh graded-turf V2 namespace.");
        return false;
    }
    FExpressionInput* SourceMask =
        AcceptedOverlay->GetExpressionInputForProperty(MP_OpacityMask);
    FExpressionInput* ClonedMask =
        OutCandidate->GetExpressionInputForProperty(MP_OpacityMask);
    if (!SourceMask || !ClonedMask || !SourceMask->Expression ||
        !ClonedMask->Expression ||
        !GraphEquivalent(SourceMask->Expression, ClonedMask->Expression))
    {
        OutError = TEXT("The duplicated material did not preserve the complete exact estate/core-feather OpacityMask dependency closure.");
        return false;
    }

    OutCandidate->Modify();
    OutCandidate->PreEditChange(nullptr);
    if (!StripOnlyOldResponseGraph(OutCandidate, OutError) ||
        !CopyExactResponseGraph(
            OrdinaryGrass, OutCandidate, OutError) ||
        !WriteExactMetadata(OutCandidate, Admission, OutError))
    {
        return false;
    }
    // Blend, mask, shading, specular, Nanite-use, and every non-response
    // material property remain inherited from the exact accepted overlay.
    UMaterialEditingLibrary::RecompileMaterial(OutCandidate);
    OutCandidate->PostEditChange();
    OutCandidate->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    return ValidateFinalMaterial(
        AcceptedOverlay,
        OrdinaryGrass,
        OutCandidate,
        Admission,
        OutError);
}
} // namespace

bool UTRIADIstanaExploreV5DGradedTurfPresentationEditorLibrary::
    InspectGradedTurfPresentationReceipts(
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
        OutReport = TEXT("ISTANA_GRADED_TURF_V2_RECEIPT_INSPECTION_DENIED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_GRADED_TURF_V2_RECEIPTS_INSPECTED_ONLY executionAuthorized=false callerSuppliedHashesNeverAuthorizeExecution=true privateMaterializerInvoked=false pinnedCandidateMaterialIntentProvenanceGrassSurfaceGroundVegetationR29R32Closure=true acceptedR33HumanReview=true separateFutureAuthorizationExactFieldRoster=true exactLiveR23BMaskedOverlayRequired=true outputAssetsWritten=0 mapOrComponentBindingModified=false R29PlacementOwnership=6144 R32PlacementOwnership=4608 geographyTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false UnrealLaunched=false nativeCompileMaterializationColdReloadCapturePerformanceOrHumanVisualAcceptanceClaimed=false");
    return true;
}

bool UTRIADIstanaExploreV5DGradedTurfPresentationEditorLibrary::
    MaterializeTrustedGradedTurfPresentationInternal(
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
        OutReport = TEXT("ISTANA_GRADED_TURF_V2_MATERIALIZATION_DENIED: ") +
            Error;
        return false;
    }

    if (FPackageName::DoesPackageExist(CandidateMaterialPackagePath))
    {
        UMaterial* AcceptedOverlay = nullptr;
        UMaterial* OrdinaryGrass = nullptr;
        TArray<TObjectPtr<UTexture2D>> OrdinaryTextures;
        UMaterial* Candidate = LoadExact<UMaterial>(CandidateMaterialObjectPath);
        if (!NamespaceHasExactOutput(Error) ||
            !ValidateExactAcceptedOverlay(AcceptedOverlay, Error) ||
            !ValidateExactOrdinaryGrassSurface(
                OrdinaryGrass, OrdinaryTextures, Error) ||
            !ValidateFinalMaterial(
                AcceptedOverlay,
                OrdinaryGrass,
                Candidate,
                Admission,
                Error))
        {
            OutReport = TEXT("ISTANA_GRADED_TURF_V2_EXISTING_OUTPUT_INVALID: ") +
                Error;
            return false;
        }
        OutReport = TEXT("ISTANA_GRADED_TURF_V2_MATERIAL_VALID existing=1 created=0 exactMaskedOverlayClone=true exactOpacityMaskSubgraphPreserved=true Grass001DualPhaseFourResponseGraph=true exactFallbackPreserved=true R29PlacementOwnership=6144 R32PlacementOwnership=4608 presentationProofMeters=0-95 sourceOnlyNativeCaptureRequired=true");
        return true;
    }
    if (!NamespaceIsEmpty(Error))
    {
        OutReport = TEXT("ISTANA_GRADED_TURF_V2_OUTPUT_NAMESPACE_NOT_EMPTY_DENIED: ") +
            Error;
        return false;
    }
    const bool bOutputRootProvenEmptyAtEntry = true;

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
            .Get();
    UMaterial* Candidate = nullptr;
    if (!CreateFreshMaterial(
            AssetTools, Admission, Candidate, Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_GRADED_TURF_V2_CREATE_FAILED: ") + Error;
        return false;
    }
    if (!Candidate || Candidate->GetPathName() != CandidateMaterialObjectPath)
    {
        Error = TEXT("EXACT_ONE_MATERIAL_PRE_SAVE_ROSTER_FAILED");
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_GRADED_TURF_V2_PRE_SAVE_FAILED: ") + Error;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem ||
        !AssetSubsystem->SaveLoadedAsset(Candidate, false))
    {
        Error = TEXT("EXACT_ONE_GRADED_TURF_MATERIAL_SAVE_FAILED");
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_GRADED_TURF_V2_SAVE_FAILED: ") + Error;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UMaterial* AcceptedOverlay = nullptr;
    UMaterial* OrdinaryGrass = nullptr;
    TArray<TObjectPtr<UTexture2D>> OrdinaryTextures;
    if (!NamespaceHasExactOutput(Error) ||
        !ValidateExactAcceptedOverlay(AcceptedOverlay, Error) ||
        !ValidateExactOrdinaryGrassSurface(
            OrdinaryGrass, OrdinaryTextures, Error) ||
        !ValidateFinalMaterial(
            AcceptedOverlay,
            OrdinaryGrass,
            Candidate,
            Admission,
            Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_GRADED_TURF_V2_POST_SAVE_FAILED: ") + Error;
        return false;
    }
    OutReport = TEXT("ISTANA_GRADED_TURF_V2_MATERIALIZED created=1 exactAcceptedR23BMaskedOverlayClone=true exact64PointEstateBoundary50mCore8mFeather25cmDitherOpacityMaskSubgraphPreserved=true onlyBaseColorRoughnessNormalAoChanged=true exactExistingGrass001DualPhase25NodeResponseGraph=true retainedHeightNoDisplacement=true noWPO=true noPDO=true exactFallbackRestorationRuntimeBoundary=true recursiveRegistryLivePackageFilesystemInventory=true freshFailureRollback=true R29PlacementOwnership=6144 R32PlacementOwnership=4608 presentationIntentMeters=0-95 mapOrComponentBindingModified=false geographyTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false nativeUBTUHTCompileColdReloadMapApplyCapturePerformanceAndHumanVisualAcceptanceStillRequired=true");
    return true;
}
