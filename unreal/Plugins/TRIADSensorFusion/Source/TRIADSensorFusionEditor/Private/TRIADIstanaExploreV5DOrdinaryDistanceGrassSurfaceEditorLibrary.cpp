#include "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "Memory/SharedBuffer.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Ssl.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
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
constexpr int32 TextureCount = 5;
constexpr int32 OutputAssetCount = 6;
constexpr int64 MaximumInputBytes = 16ll * 1024ll * 1024ll;

const FString CandidateContractSha256(
    TEXT("473BFA2D1AA3B5B2F5E8762E8FADF1D9681EE083BD7068FED1C919BEF5956357"));
const FString MaterialIntentSha256(
    TEXT("93E0DF74EB12316559C5AEEC42FC79B879ACB8C61B7D0B878FFC141A0FBF79F6"));
const FString ProvenanceSha256(
    TEXT("5F4B304157AF89EE6DB7BE9D86A7424D70B4F8D0AF55A8DA88205AAE0B1BB051"));
const FString AuditSha256(
    TEXT("A19D45F727710C6C8CAFC98C5AA54A00D6742D353228271EBC7CDAA0B9B20D7E"));

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
    TEXT("triad.istana_public_view_explore_v5d.ordinary_distance_grass_surface.future_transaction_authority.v1"));
const FString FutureOperation(
    TEXT("MATERIALIZE_ORDINARY_DISTANCE_GRASS_SURFACE_ASSETS_ONLY_AND_EXPOSE_OPTIONAL_PRESENTATION_MATERIAL"));
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration"));
const FString TextureRoot(OutputRoot + TEXT("/Textures"));
const FString MaterialRoot(OutputRoot + TEXT("/Materials"));
const FString MaterialAssetName(
    TEXT("M_IPV5D_OrdinaryDistanceGrassSurface"));
const FString MaterialPackagePath(MaterialRoot + TEXT("/") + MaterialAssetName);
const FString MaterialObjectPath(
    MaterialPackagePath + TEXT(".") + MaterialAssetName);

const TCHAR* const TextureRoles[] = {
    TEXT("BaseColor"),
    TEXT("NormalDX"),
    TEXT("Roughness"),
    TEXT("AmbientOcclusion"),
    TEXT("Height")};
const TCHAR* const TextureRelativePaths[] = {
    TEXT("SourceTextures/Grass001_2K-JPG_Color.jpg"),
    TEXT("SourceTextures/Grass001_2K-JPG_NormalDX.jpg"),
    TEXT("SourceTextures/Grass001_2K-JPG_Roughness.jpg"),
    TEXT("SourceTextures/Grass001_2K-JPG_AmbientOcclusion.jpg"),
    TEXT("SourceTextures/Grass001_2K-JPG_Displacement.jpg")};
const int64 TextureBytes[] = {
    6953345,
    9975086,
    3286058,
    3644975,
    3460522};
const TCHAR* const TextureSha256[] = {
    TEXT("8A8BFCFFD087134CAAD2A7DEF216F3DD2D307FBE02340CA47E3C5C71F49D50B4"),
    TEXT("26343D7D725A2D5E3F82D0C3FBC9319F517715DF08297A4EE42C96DDE88815C7"),
    TEXT("2A2A3F9220A351F14858075D4C6BA1FF7FDE8BEDD7009C94CB432BC2B9A362E4"),
    TEXT("7AA2C5AC4DB77005B92BED77399189C9219300ADA2F3D86C3C0FEA51BBA5A3F1"),
    TEXT("1C6B8AD765F374958CC3FB35E4965830D3F967EC5764957D913C07494D8EA12D")};
const TCHAR* const TextureMd5[] = {
    TEXT("DC6B08F588B0581702296C9C58951B31"),
    TEXT("8A46197DD2E1530CCF52E691E0BD1078"),
    TEXT("5007981963EC7E10686051486AFE6DB3"),
    TEXT("6D95D0CFB832ABD098CA1947A38421AE"),
    TEXT("2599A12F96EBD37F4188BEB37D74684E")};
const ETextureSourceFormat TextureSourceFormats[] = {
    TSF_BGRA8, TSF_BGRA8, TSF_G8, TSF_G8, TSF_G8};
const TCHAR* const TextureAssetNames[] = {
    TEXT("T_IPV5D_Grass001_BaseColor"),
    TEXT("T_IPV5D_Grass001_NormalDX"),
    TEXT("T_IPV5D_Grass001_Roughness"),
    TEXT("T_IPV5D_Grass001_AmbientOcclusion"),
    TEXT("T_IPV5D_Grass001_Height")};

struct FSourcePin
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourcePin AdditionalSourcePins[] = {
    {TEXT("build_grass_surface_candidate.py"), 55031,
     TEXT("976B49B3BA1DB37F6523417BA10DBCC7FEB9DA7E4AD8BBFF47D2017A91069FF6")},
    {TEXT("OfflineAudit/ordinary_distance_grass_fixed_views.png"), 1485207,
     TEXT("843208465C89B0F7ED3A7785B631C9775F7A83C2148DA1D76A9512BEE3FB7375")},
    {TEXT("Provenance/upstream_grass001_manifest_extract.json"), 9182,
     TEXT("817EDA4156196A1270FF852E1C2C3D54DB590931F78BB4830C2BF2CDEB50A6A7")},
    {TEXT("Provenance/Evidence/Grass001.source-page.html"), 29112,
     TEXT("DBBCD614EA0334E05C705FFC9A1B92F39B5720F57DE52BAADCB03C99DAE9F36B")},
    {TEXT("Provenance/Evidence/Grass001.full.json"), 13099,
     TEXT("5988A2CF736EB297FAAEFCE5F78266FBEC3882301F140DE8E88B2F5FB0529E74")},
    {TEXT("Provenance/Evidence/cc0-1.0-legalcode.html"), 32451,
     TEXT("001E3D1C905C18B1D034B34200CC952026ABB38457C2294C23EAEF7F6BDA64DF")},
    {TEXT("Provenance/Evidence/cc0-1.0-deed.html"), 30476,
     TEXT("4CEB8AE6835F2F5263CAA0E39C9E1ADCA9469686C267475049B33521DABBE339")},
    {TEXT("Provenance/Evidence/ambientcg-license.html"), 34106,
     TEXT("60138E09B277C89977841462634B36E99D7BAE5B77174777813502069C59461C")}};

static_assert(UE_ARRAY_COUNT(TextureRoles) == TextureCount);
static_assert(UE_ARRAY_COUNT(TextureRelativePaths) == TextureCount);
static_assert(UE_ARRAY_COUNT(TextureBytes) == TextureCount);
static_assert(UE_ARRAY_COUNT(TextureSha256) == TextureCount);
static_assert(UE_ARRAY_COUNT(TextureMd5) == TextureCount);
static_assert(UE_ARRAY_COUNT(TextureSourceFormats) == TextureCount);
static_assert(UE_ARRAY_COUNT(TextureAssetNames) == TextureCount);

const FString PrimaryUvDescription(
    TEXT("TRIAD_OD_GRASS_PROVIDER_1P4M_PRIMARY_WORLD_UV"));
const FString PrimaryUvCode(
    TEXT("return AbsoluteWorldPosition.xy / 140.0;"));
const FString PhaseUvDescription(
    TEXT("TRIAD_OD_GRASS_PROVIDER_1P4M_ROTATED_PHASE_WORLD_UV"));
const FString PhaseUvCode(
    TEXT("float2 worldM=AbsoluteWorldPosition.xy*0.01; return float2(-worldM.y,worldM.x)/1.4+float2(0.371,0.613);"));
const FString BlendMaskDescription(
    TEXT("TRIAD_OD_GRASS_7M_SMOOTH_PHASE_BLEND_MASK"));
const FString BlendMaskCode(
    TEXT("float2 p=AbsoluteWorldPosition.xy*0.01/7.0; float2 i=floor(p); float2 f=frac(p); f=f*f*(3.0-2.0*f); float4 h=frac(sin(float4(dot(i,float2(127.1,311.7)),dot(i+float2(1,0),float2(127.1,311.7)),dot(i+float2(0,1),float2(127.1,311.7)),dot(i+float2(1,1),float2(127.1,311.7))))*43758.5453); float n=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y); return smoothstep(0.2,0.8,n);"));
const FString NormalBlendDescription(
    TEXT("TRIAD_OD_GRASS_INVERSE_ROTATE_PHASE_NORMAL_XY_90_BEFORE_BLEND"));
const FString NormalBlendCode(
    TEXT("float3 phaseRotated=float3(PhaseNormal.y,-PhaseNormal.x,PhaseNormal.z); return lerp(PrimaryNormal,phaseRotated,BlendMask);"));
const FString ColorRoughnessDescription(
    TEXT("TRIAD_OD_GRASS_3P5M_8P4M_18P2M_COLOR_ROUGHNESS_RESPONSE"));
const FString ColorRoughnessCode(
    TEXT("float distanceM=length(AbsoluteWorldPosition-CameraPosition)*0.01; float2 p=AbsoluteWorldPosition.xy*0.01; float meso=sin(dot(p/3.5,float2(1.731,2.417))); float macroA=sin(dot(p/8.4,float2(-1.193,2.071))+0.73); float macroB=sin(dot(p/18.2,float2(0.817,-1.643))-1.11); float macro=0.6*macroA+0.4*macroB; float multiplier=clamp(1.0+0.045*meso+0.15*macro,0.82,1.18); float footprintM=max(length(ddx(p)),length(ddy(p))); float bandLimit=1.0-smoothstep(0.35,1.4,footprintM); float sourceWeight=(1.0-smoothstep(50.0,70.0,distanceM))*bandLimit; float3 farBase=float3(0.058,0.105,0.022); float3 color=lerp(farBase,saturate(BaseColor*multiplier),sourceWeight); float rough=clamp(lerp(0.82,Roughness,sourceWeight),0.48,0.98); return float4(color,rough);"));
const FString NormalDescription(
    TEXT("TRIAD_OD_GRASS_20M_35M_50M_NORMAL_RESPONSE"));
const FString NormalCode(
    TEXT("float distanceM=length(AbsoluteWorldPosition-CameraPosition)*0.01; float strength=distanceM<=20.0?0.8:(distanceM<=35.0?lerp(0.8,0.62,(distanceM-20.0)/15.0):(distanceM<=50.0?lerp(0.62,0.48,(distanceM-35.0)/15.0):lerp(0.48,0.0,smoothstep(50.0,70.0,distanceM)))); float3 n=normalize(NormalDirectX); n.xy*=max(strength,0.0); return normalize(n);"));
const FString AoDescription(
    TEXT("TRIAD_OD_GRASS_AO_STRENGTH_0P7_DISTANCE_RESPONSE"));
const FString AoCode(
    TEXT("float distanceM=length(AbsoluteWorldPosition-CameraPosition)*0.01; float sourceWeight=1.0-smoothstep(50.0,70.0,distanceM); float retainedHeight=max(RetainedHeightNoDisplacement,0.0); return lerp(1.0,lerp(1.0,AmbientOcclusion,0.7),sourceWeight)+retainedHeight*0.0;"));

struct FAdmission
{
    FString CandidateRoot;
    FString AcceptedR33Sha256;
    FString FutureAuthorizationSha256;
    TArray<TArray<uint8>> TextureSourceBytes;
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
        OutError = TEXT("Every ordinary-distance grass input requires an absolute path, bounded positive byte count, and explicit SHA-256 pin.");
        return false;
    }
    const int64 ActualBytes = IFileManager::Get().FileSize(*AbsolutePath);
    if (ActualBytes != ExpectedBytes ||
        !FFileHelper::LoadFileToArray(OutBytes, *AbsolutePath) ||
        OutBytes.Num() != ActualBytes)
    {
        OutError = TEXT("An ordinary-distance grass source file is absent or its exact byte count drifted.");
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
        OutError = TEXT("An ordinary-distance grass source file failed its immutable SHA-256 pin.");
        return false;
    }
#else
    OutError = TEXT("Ordinary-distance grass source admission requires WITH_SSL SHA-256 support.");
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
        OutError = TEXT("A hash-pinned ordinary-distance grass JSON document could not be parsed.");
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

bool ExactNumberArray(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    std::initializer_list<double> Expected)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) ||
        !Values || Values->Num() != static_cast<int32>(Expected.size()))
    {
        return false;
    }
    int32 Index = 0;
    for (double ExpectedValue : Expected)
    {
        if ((*Values)[Index]->AsNumber() != ExpectedValue)
        {
            return false;
        }
        ++Index;
    }
    return true;
}

bool ValidateCandidateContract(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Authority = nullptr;
    const TSharedPtr<FJsonObject>* Intent = nullptr;
    const TSharedPtr<FJsonObject>* Provenance = nullptr;
    const TSharedPtr<FJsonObject>* Inventory = nullptr;
    const TSharedPtr<FJsonObject>* Scope = nullptr;
    const TSharedPtr<FJsonObject>* R32 = nullptr;
    const TSharedPtr<FJsonObject>* Validation = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Maps = nullptr;
    const TCHAR* const FalseAuthorityFields[] = {
        TEXT("collisionAuthority"),
        TEXT("currentIstanaConditionAuthority"),
        TEXT("geographyAuthority"),
        TEXT("hyperrealClaim"),
        TEXT("nativeIntegrationAuthority"),
        TEXT("securityAuthority"),
        TEXT("sensorPlacementAuthority"),
        TEXT("simulationAuthority"),
        TEXT("speciesOrBotanicalAuthority"),
        TEXT("visualAcceptance")};
    bool bValid =
        ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.ordinary_distance_grass.source_candidate.v1")) &&
        ExactString(
            Root,
            TEXT("status"),
            TEXT("POST_R33_UNNUMBERED_UNADMITTED_SOURCE_ONLY_CANDIDATE")) &&
        ExactString(
            Root,
            TEXT("candidate"),
            TEXT("OrdinaryDistanceGrassSurfaceCandidate")) &&
        ExactString(
            Root,
            TEXT("selectedSource"),
            TEXT("ambientcg:Grass001:2K-JPG")) &&
        Root->TryGetObjectField(TEXT("authorityBoundary"), Authority) &&
        Authority && Authority->IsValid() &&
        Root->TryGetObjectField(TEXT("materialIntent"), Intent) &&
        Intent && Intent->IsValid() &&
        ExactString(
            *Intent,
            TEXT("path"),
            TEXT("Generated/ordinary_distance_grass_material_intent.json")) &&
        ExactInteger(*Intent, TEXT("bytes"), 3402) &&
        ExactString(*Intent, TEXT("sha256"), MaterialIntentSha256) &&
        Root->TryGetObjectField(TEXT("provenance"), Provenance) &&
        Provenance && Provenance->IsValid() &&
        ExactString(
            *Provenance,
            TEXT("path"),
            TEXT("Provenance/grass001_local_reuse.provenance.json")) &&
        ExactInteger(*Provenance, TEXT("bytes"), 6134) &&
        ExactString(*Provenance, TEXT("sha256"), ProvenanceSha256) &&
        Root->TryGetObjectField(TEXT("sourceInventory"), Inventory) &&
        Inventory && Inventory->IsValid() &&
        ExactString(*Inventory, TEXT("path"), TEXT("source_inventory.json")) &&
        ExactInteger(*Inventory, TEXT("bytes"), 2449) &&
        ExactString(
            *Inventory,
            TEXT("sha256"),
            TEXT("7213854045B88C91D9D17869CE6521ABEFE2A39FA930169A910BA8E8434FB67D")) &&
        Root->TryGetArrayField(TEXT("sourceMaps"), Maps) && Maps &&
        Maps->Num() == TextureCount &&
        Root->TryGetObjectField(TEXT("scopeGuard"), Scope) && Scope &&
        Scope->IsValid() &&
        ExactBool(*Scope, TEXT("mapModified"), false) &&
        ExactBool(*Scope, TEXT("nativeProjectAccessed"), false) &&
        ExactBool(*Scope, TEXT("networkAccessed"), false) &&
        ExactBool(*Scope, TEXT("unrealLaunched"), false) &&
        (*Scope)->TryGetObjectField(TEXT("r32Contract"), R32) && R32 &&
        R32->IsValid() && ExactBool(*R32, TEXT("modifiedByCandidate"), false) &&
        Root->TryGetObjectField(TEXT("validation"), Validation) &&
        Validation && Validation->IsValid() &&
        ExactBool(*Validation, TEXT("allFivePbrInputsPresent"), true) &&
        ExactBool(*Validation, TEXT("allSourceMaps2048Square"), true) &&
        ExactBool(*Validation, TEXT("providerScaleBound"), true) &&
        ExactBool(*Validation, TEXT("sourceOnly"), true);
    for (const TCHAR* Field : FalseAuthorityFields)
    {
        bValid = bValid && ExactBool(*Authority, Field, false);
    }
    for (int32 Index = 0; bValid && Index < TextureCount; ++Index)
    {
        const TSharedPtr<FJsonObject> Map = (*Maps)[Index]->AsObject();
        bValid = Map.IsValid() &&
            ExactString(Map, TEXT("path"), TextureRelativePaths[Index]) &&
            ExactInteger(
                Map,
                TEXT("bytes"),
                static_cast<int32>(TextureBytes[Index])) &&
            ExactString(Map, TEXT("sha256"), TextureSha256[Index]);
    }
    if (!bValid)
    {
        OutError = TEXT("The pinned ordinary-distance grass candidate contract lost its exact source-only, five-map, no-authority boundary.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMaterialIntent(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Engine = nullptr;
    const TSharedPtr<FJsonObject>* Scale = nullptr;
    const TSharedPtr<FJsonObject>* Anti = nullptr;
    const TSharedPtr<FJsonObject>* Primary = nullptr;
    const TSharedPtr<FJsonObject>* Phase = nullptr;
    const TSharedPtr<FJsonObject>* Bindings = nullptr;
    const TSharedPtr<FJsonObject>* Pbr = nullptr;
    const TSharedPtr<FJsonObject>* Height = nullptr;
    const bool bValid =
        ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.ordinary_distance_grass.material_intent.v1")) &&
        ExactString(
            Root,
            TEXT("status"),
            TEXT("UNADMITTED_SOURCE_LOOKDEV_INTENT")) &&
        ExactString(
            Root,
            TEXT("surfaceModel"),
            TEXT("opaque dielectric turf surface")) &&
        Root->TryGetObjectField(TEXT("engineState"), Engine) && Engine &&
        Engine->IsValid() && ExactBool(*Engine, TEXT("mapModified"), false) &&
        ExactBool(*Engine, TEXT("nativeMaterialCreated"), false) &&
        ExactBool(*Engine, TEXT("collisionAuthored"), false) &&
        ExactBool(*Engine, TEXT("runtimePerformanceValidated"), false) &&
        Root->TryGetObjectField(TEXT("physicalScale"), Scale) && Scale &&
        Scale->IsValid() &&
        ExactNumberArray(*Scale, TEXT("providerTileMeters"), {1.4, 1.4}) &&
        ExactString(
            *Scale,
            TEXT("worldAlignedUvFormula"),
            TEXT("uvA = worldXY_metres / 1.4")) &&
        Root->TryGetObjectField(TEXT("antiRepetition"), Anti) && Anti &&
        Anti->IsValid() &&
        ExactNumber(*Anti, TEXT("blendMaskScaleMeters"), 7.0) &&
        ExactNumber(*Anti, TEXT("mesoTintScaleMeters"), 3.5) &&
        ExactNumberArray(*Anti, TEXT("macroTintScalesMeters"), {8.4, 18.2}) &&
        ExactNumber(*Anti, TEXT("mesoLinearAlbedoAmplitude"), 0.045) &&
        ExactNumber(*Anti, TEXT("macroLinearAlbedoAmplitude"), 0.15) &&
        ExactBool(
            *Anti,
            TEXT("preservesProviderTileScaleInBothTextureSamples"),
            true) &&
        (*Anti)->TryGetObjectField(TEXT("primarySample"), Primary) && Primary &&
        Primary->IsValid() &&
        ExactNumber(*Primary, TEXT("tileMeters"), 1.4) &&
        ExactNumber(*Primary, TEXT("rotationDegrees"), 0.0) &&
        (*Anti)->TryGetObjectField(TEXT("phaseSample"), Phase) && Phase &&
        Phase->IsValid() && ExactNumber(*Phase, TEXT("tileMeters"), 1.4) &&
        ExactNumber(*Phase, TEXT("rotationDegrees"), 90.0) &&
        ExactNumberArray(*Phase, TEXT("offsetUv"), {0.371, 0.613}) &&
        Root->TryGetObjectField(TEXT("textureBindings"), Bindings) &&
        Bindings && Bindings->IsValid() &&
        ExactString(*Bindings, TEXT("baseColor"), TextureRelativePaths[0]) &&
        ExactString(
            *Bindings,
            TEXT("normalDirectX"),
            TextureRelativePaths[1]) &&
        ExactString(*Bindings, TEXT("roughness"), TextureRelativePaths[2]) &&
        ExactString(
            *Bindings,
            TEXT("ambientOcclusion"),
            TextureRelativePaths[3]) &&
        ExactString(*Bindings, TEXT("height"), TextureRelativePaths[4]) &&
        Root->TryGetObjectField(TEXT("pbrResponse"), Pbr) && Pbr &&
        Pbr->IsValid() && (*Pbr)->TryGetObjectField(TEXT("height"), Height) &&
        Height && Height->IsValid() &&
        ExactBool(*Height, TEXT("geometryDisplacementAt20To50Meters"), false) &&
        ExactNumber(*Height, TEXT("lookdevAmplitudeMeters"), 0.012);
    if (!bValid)
    {
        OutError = TEXT("The pinned ordinary-distance grass material intent lost its exact 1.4 m dual-phase, multi-scale, five-input, or no-displacement contract.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateProvenance(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* License = nullptr;
    const TSharedPtr<FJsonObject>* Reuse = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Maps = nullptr;
    bool bValid =
        ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.ordinary_distance_grass.local_reuse_provenance.v1")) &&
        ExactString(Root, TEXT("provider"), TEXT("ambientCG")) &&
        ExactString(Root, TEXT("providerAssetId"), TEXT("Grass001")) &&
        ExactNumberArray(Root, TEXT("physicalTileMeters"), {1.4, 1.4}) &&
        Root->TryGetObjectField(TEXT("license"), License) && License &&
        License->IsValid() &&
        ExactString(*License, TEXT("spdxLikeId"), TEXT("CC0-1.0")) &&
        Root->TryGetObjectField(TEXT("localReuse"), Reuse) && Reuse &&
        Reuse->IsValid() && ExactBool(*Reuse, TEXT("networkAccessed"), false) &&
        Root->TryGetArrayField(TEXT("maps"), Maps) && Maps &&
        Maps->Num() == TextureCount;
    const TCHAR* const ExpectedColorSpaces[] = {
        TEXT("sRGB"),
        TEXT("linear/non-color"),
        TEXT("linear/non-color"),
        TEXT("linear/non-color"),
        TEXT("linear/non-color")};
    for (int32 Index = 0; bValid && Index < TextureCount; ++Index)
    {
        const TSharedPtr<FJsonObject> Map = (*Maps)[Index]->AsObject();
        bValid = Map.IsValid() &&
            ExactString(Map, TEXT("path"), TextureRelativePaths[Index]) &&
            ExactInteger(
                Map,
                TEXT("bytes"),
                static_cast<int32>(TextureBytes[Index])) &&
            ExactString(Map, TEXT("sha256"), TextureSha256[Index]) &&
            ExactNumberArray(Map, TEXT("pixels"), {2048.0, 2048.0}) &&
            ExactString(
                Map,
                TEXT("colorSpace"),
                ExpectedColorSpaces[Index]);
    }
    if (!bValid)
    {
        OutError = TEXT("The pinned Grass001 local-reuse provenance lost its CC0, provider-scale, offline-reuse, or five-map identity.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateInventory(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    if (!ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.ordinary_distance_grass.local_source_inventory.v1")) ||
        !ExactString(
            Root,
            TEXT("selection"),
            TEXT("ambientcg:Grass001:2K-JPG")) ||
        !ExactBool(Root, TEXT("performedOffline"), true) ||
        !ExactBool(Root, TEXT("networkAccessed"), false) ||
        !ExactBool(Root, TEXT("selectionDoesNotSupersedeR32"), true))
    {
        OutError = TEXT("The pinned ordinary-distance grass inventory lost its offline selection or R32 non-supersession boundary.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAudit(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Claims = nullptr;
    const TSharedPtr<FJsonObject>* Metrics = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Panels = nullptr;
    const TCHAR* const FalseClaimFields[] = {
        TEXT("hyperrealAcceptance"),
        TEXT("nativeIntegration"),
        TEXT("physicalMaterialTruth"),
        TEXT("siteComparison"),
        TEXT("unrealRender"),
        TEXT("visualAcceptance")};
    bool bValid =
        ExactString(
            Root,
            TEXT("schema"),
            TEXT("triad.ordinary_distance_grass.cpu_source_lookdev.v1")) &&
        ExactString(
            Root,
            TEXT("status"),
            TEXT("PASS_SOURCE_LOOKDEV_ONLY")) &&
        Root->TryGetObjectField(TEXT("claimBoundary"), Claims) && Claims &&
        Claims->IsValid() && Root->TryGetObjectField(TEXT("metrics"), Metrics) &&
        Metrics && Metrics->IsValid() &&
        (*Metrics)->TryGetArrayField(TEXT("ordinaryDistancePanels"), Panels) &&
        Panels && Panels->Num() == 3;
    for (const TCHAR* Field : FalseClaimFields)
    {
        bValid = bValid && ExactBool(*Claims, Field, false);
    }
    const double Distances[] = {20.0, 35.0, 50.0};
    const double Contrasts[] = {0.080690, 0.081796, 0.082635};
    const double Deltas[] = {0.024243, 0.025028, 0.025432};
    for (int32 Index = 0; bValid && Index < 3; ++Index)
    {
        const TSharedPtr<FJsonObject> Panel = (*Panels)[Index]->AsObject();
        const TSharedPtr<FJsonObject>* Gate = nullptr;
        const TSharedPtr<FJsonObject>* Coverage = nullptr;
        bValid = Panel.IsValid() &&
            ExactNumber(
                Panel,
                TEXT("cameraHorizontalDistanceMeters"),
                Distances[Index]) &&
            ExactNumber(
                Panel,
                TEXT("targetBandLuminanceP95MinusP05"),
                Contrasts[Index]) &&
            ExactNumber(
                Panel,
                TEXT("meanNeighbourLuminanceDelta"),
                Deltas[Index]) &&
            Panel->TryGetObjectField(TEXT("definitionGate"), Gate) && Gate &&
            Gate->IsValid() && ExactBool(*Gate, TEXT("pass"), true) &&
            Panel->TryGetObjectField(TEXT("metricCoverage"), Coverage) &&
            Coverage && Coverage->IsValid() &&
            ExactBool(*Coverage, TEXT("pass"), true);
    }
    if (!bValid)
    {
        OutError = TEXT("The pinned ordinary-distance grass lookdev audit lost its source-only claim boundary or 20/35/50 m definition evidence.");
        return false;
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
    if (CandidateRoot.IsEmpty() || FPaths::IsRelative(CandidateRoot) ||
        FPaths::GetCleanFilename(CandidateRoot) !=
            TEXT("OrdinaryDistanceGrassSurfaceCandidate"))
    {
        OutError = TEXT("Ordinary-distance grass admission requires the explicit absolute candidate root.");
        return false;
    }

    TSharedPtr<FJsonObject> Candidate;
    TSharedPtr<FJsonObject> Intent;
    TSharedPtr<FJsonObject> Provenance;
    TSharedPtr<FJsonObject> Inventory;
    TSharedPtr<FJsonObject> Audit;
    if (!ParsePinnedJson(
            SourcePath(
                CandidateRoot,
                TEXT("ordinary_distance_grass_surface.contract.json")),
            5345,
            CandidateContractSha256,
            Candidate,
            OutError) ||
        !ValidateCandidateContract(Candidate, OutError) ||
        !ParsePinnedJson(
            SourcePath(
                CandidateRoot,
                TEXT("Generated/ordinary_distance_grass_material_intent.json")),
            3402,
            MaterialIntentSha256,
            Intent,
            OutError) ||
        !ValidateMaterialIntent(Intent, OutError) ||
        !ParsePinnedJson(
            SourcePath(
                CandidateRoot,
                TEXT("Provenance/grass001_local_reuse.provenance.json")),
            6134,
            ProvenanceSha256,
            Provenance,
            OutError) ||
        !ValidateProvenance(Provenance, OutError) ||
        !ParsePinnedJson(
            SourcePath(CandidateRoot, TEXT("source_inventory.json")),
            2449,
            TEXT("7213854045B88C91D9D17869CE6521ABEFE2A39FA930169A910BA8E8434FB67D"),
            Inventory,
            OutError) ||
        !ValidateInventory(Inventory, OutError) ||
        !ParsePinnedJson(
            SourcePath(
                CandidateRoot,
                TEXT("OfflineAudit/ordinary_distance_grass_source_lookdev.json")),
            7399,
            AuditSha256,
            Audit,
            OutError) ||
        !ValidateAudit(Audit, OutError))
    {
        return false;
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
    for (const FSourcePin& Pin : AdditionalSourcePins)
    {
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
        OutError = TEXT("Ordinary-distance grass admission did not retain all five exact source texture buffers.");
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
        TEXT("ProvenanceSha256"), TEXT("ExactSourceTextureCount"),
        TEXT("ExactOutputAssetCount"), TEXT("OutputNamespace"),
        TEXT("UnnumberedSuccessor"), TEXT("ExplicitExecutionAuthorized"),
        TEXT("AssetsOnlyEndpoint"),
        TEXT("OptionalPresentationMaterialOnly"),
        TEXT("ExistingLawnFallbackPreserved"),
        TEXT("MapMutationAuthorized"),
        TEXT("SourceMaterialMutationAuthorized"),
        TEXT("SourceTransformMutationAuthorized"),
        TEXT("GeographyTerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"),
        TEXT("NativeWriteOrUnrealLaunchPerformed")};
    if (!Root.IsValid() ||
        Root->Values.Num() != UE_ARRAY_COUNT(ExactFields))
    {
        OutError = TEXT("Future grass-surface authorization must contain exactly the narrow trusted field roster and no extensions.");
        return false;
    }
    for (const TCHAR* Field : ExactFields)
    {
        if (!Root->HasField(Field))
        {
            OutError = TEXT("Future grass-surface authorization omitted a field from the exact narrow roster.");
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
            TEXT("MaterialIntentSha256"),
            MaterialIntentSha256) ||
        !ExactString(
            Root,
            TEXT("ProvenanceSha256"),
            ProvenanceSha256) ||
        !ExactInteger(Root, TEXT("ExactSourceTextureCount"), TextureCount) ||
        !ExactInteger(Root, TEXT("ExactOutputAssetCount"), OutputAssetCount) ||
        !ExactString(Root, TEXT("OutputNamespace"), OutputRoot) ||
        !ExactBool(Root, TEXT("UnnumberedSuccessor"), true) ||
        !ExactBool(Root, TEXT("ExplicitExecutionAuthorized"), true) ||
        !ExactBool(Root, TEXT("AssetsOnlyEndpoint"), true) ||
        !ExactBool(Root, TEXT("OptionalPresentationMaterialOnly"), true) ||
        !ExactBool(Root, TEXT("ExistingLawnFallbackPreserved"), true) ||
        !ExactBool(Root, TEXT("MapMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceMaterialMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceTransformMutationAuthorized"), false) ||
        !ExactBool(
            Root,
            TEXT("GeographyTerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"),
            false) ||
        !ExactBool(
            Root,
            TEXT("NativeWriteOrUnrealLaunchPerformed"),
            false))
    {
        OutError = TEXT("Future grass-surface authorization is absent, unpinned, non-explicit, or broader than isolated optional presentation-asset materialization.");
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
        OutError = TEXT("Grass-surface admission requires an absolute candidate root, two absolute receipt paths, and two explicit SHA-256 receipt pins.");
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
            OutError = TEXT("Grass-surface execution is intentionally unreachable: compiled trusted receipt anchors are unset and require a separately reviewed source change plus recompile.");
            return false;
        }
        if (!ExpectedAcceptedR33ReceiptSha256.Equals(
                TrustedAcceptedR33ReceiptSha256,
                ESearchCase::IgnoreCase) ||
            !ExpectedFutureTransactionAuthorizationSha256.Equals(
                TrustedFutureAuthorizationSha256,
                ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Caller-supplied grass-surface receipt hashes do not match the separately compiled trusted anchors.");
            return false;
        }
    }
    FString FullRoot = FPaths::ConvertRelativePathToFull(CandidateRoot);
    FPaths::NormalizeDirectoryName(FullRoot);
    FString FullR33 = FPaths::ConvertRelativePathToFull(
        AcceptedR33ReceiptPath);
    FString FullAuthorization = FPaths::ConvertRelativePathToFull(
        FutureTransactionAuthorizationPath);
    FPaths::NormalizeFilename(FullR33);
    FPaths::NormalizeFilename(FullAuthorization);
    if (FPaths::GetCleanFilename(FullRoot) !=
            TEXT("OrdinaryDistanceGrassSurfaceCandidate") ||
        FPaths::IsSamePath(FullR33, FullAuthorization) ||
        ExpectedAcceptedR33ReceiptSha256.Equals(
            ExpectedFutureTransactionAuthorizationSha256,
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Grass-surface inspection requires the exact named candidate root and independent R33/authorization receipts.");
        return false;
    }
    const int64 R33Bytes = IFileManager::Get().FileSize(*FullR33);
    const int64 AuthorizationBytes = IFileManager::Get().FileSize(
        *FullAuthorization);
    TSharedPtr<FJsonObject> AcceptedR33;
    TSharedPtr<FJsonObject> FutureAuthorization;
    if (!ValidateSourceRoster(
            FullRoot,
            OutAdmission.TextureSourceBytes,
            OutError) ||
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

FString TexturePackagePath(int32 Index)
{
    return Index >= 0 && Index < TextureCount
        ? TextureRoot + TEXT("/") + TextureAssetNames[Index]
        : FString();
}

FString TextureObjectPath(int32 Index)
{
    return Index >= 0 && Index < TextureCount
        ? TexturePackagePath(Index) + TEXT(".") + TextureAssetNames[Index]
        : FString();
}

void GetOutputObjectPaths(TArray<FString>& OutPaths)
{
    OutPaths.Reset();
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        OutPaths.Add(TextureObjectPath(Index));
    }
    OutPaths.Add(MaterialObjectPath);
}

void GetOutputPackagePaths(TArray<FString>& OutPaths)
{
    OutPaths.Reset();
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        OutPaths.Add(TexturePackagePath(Index));
    }
    OutPaths.Add(MaterialPackagePath);
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
        FName(NamespaceRoot),
        RegistryAssets,
        true,
        false);
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

    // Registry scans can lag an interrupted import. Inventory every physical
    // file and child directory, not merely registered package extensions, so
    // no orphan, sidecar, temporary file, or empty unexpected subtree can be
    // mistaken for an empty/exact namespace.
    if (FPackageName::TryConvertLongPackageNameToFilename(
            NamespaceRoot,
            OutState.PhysicalRoot,
            FString()))
    {
        OutState.bPhysicalRootResolved = true;
        FPaths::NormalizeDirectoryName(OutState.PhysicalRoot);
        if (IFileManager::Get().DirectoryExists(*OutState.PhysicalRoot))
        {
            OutState.PhysicalDirectories.Add(OutState.PhysicalRoot);
        }
        TArray<FString> PhysicalFiles;
        IFileManager::Get().FindFilesRecursive(
            PhysicalFiles,
            *OutState.PhysicalRoot,
            TEXT("*"),
            true,
            false,
            true);
        TArray<FString> PhysicalDirectories;
        IFileManager::Get().FindFilesRecursive(
            PhysicalDirectories,
            *OutState.PhysicalRoot,
            TEXT("*"),
            false,
            true,
            true);
        for (FString Filename : PhysicalFiles)
        {
            FPaths::NormalizeFilename(Filename);
            OutState.PhysicalFiles.Add(Filename);
            FString PackageName;
            const FString Extension = FPaths::GetExtension(
                Filename,
                true);
            if ((Extension == FPackageName::GetAssetPackageExtension() ||
                 Extension == FPackageName::GetMapPackageExtension()) &&
                FPackageName::TryConvertFilenameToLongPackageName(
                    Filename,
                    PackageName) &&
                IsPackageWithinNamespace(PackageName, NamespaceRoot))
            {
                OutState.PackagePaths.Add(PackageName);
            }
        }
        for (FString Directory : PhysicalDirectories)
        {
            FPaths::NormalizeDirectoryName(Directory);
            OutState.PhysicalDirectories.Add(Directory);
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
    TSet<FString> ExpectedPhysicalFiles;
    TSet<FString> ExpectedPhysicalDirectories;
    FString PhysicalRoot;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            NamespaceRoot,
            PhysicalRoot,
            FString()))
    {
        OutError = FString::Printf(
            TEXT("Grass-surface namespace '%s' has no resolvable package-root filesystem path."),
            *NamespaceRoot);
        return false;
    }
    FPaths::NormalizeDirectoryName(PhysicalRoot);
    ExpectedPhysicalDirectories.Add(PhysicalRoot);
    for (const FString& ObjectPath : ExpectedObjectPaths)
    {
        ExpectedObjects.Add(ObjectPath);
        const FString PackagePath =
            FPackageName::ObjectPathToPackageName(ObjectPath);
        ExpectedPackages.Add(PackagePath);
        FString PhysicalFile;
        if (!FPackageName::TryConvertLongPackageNameToFilename(
                PackagePath,
                PhysicalFile,
                FPackageName::GetAssetPackageExtension()))
        {
            OutError = FString::Printf(
                TEXT("Grass-surface expected package '%s' has no exact asset filename."),
                *PackagePath);
            return false;
        }
        FPaths::NormalizeFilename(PhysicalFile);
        ExpectedPhysicalFiles.Add(PhysicalFile);
        FString Directory = FPaths::GetPath(PhysicalFile);
        FPaths::NormalizeDirectoryName(Directory);
        while (!Directory.IsEmpty() &&
               (FPaths::IsSamePath(Directory, PhysicalRoot) ||
                Directory.StartsWith(PhysicalRoot + TEXT("/"))))
        {
            ExpectedPhysicalDirectories.Add(Directory);
            if (FPaths::IsSamePath(Directory, PhysicalRoot))
            {
                break;
            }
            Directory = FPaths::GetPath(Directory);
            FPaths::NormalizeDirectoryName(Directory);
        }
    }
    FNamespaceState Actual;
    CollectNamespaceState(NamespaceRoot, Actual);
    if (!Actual.bPhysicalRootResolved ||
        !FPaths::IsSamePath(Actual.PhysicalRoot, PhysicalRoot))
    {
        OutError = TEXT("Grass-surface physical namespace resolution drifted.");
        return false;
    }
    for (const FString& ObjectPath : Actual.ObjectPaths)
    {
        if (!ExpectedObjects.Contains(ObjectPath))
        {
            OutError = FString::Printf(
                TEXT("Grass-surface namespace '%s' contains uncontracted asset '%s'."),
                *NamespaceRoot,
                *ObjectPath);
            return false;
        }
    }
    for (const FString& PackagePath : Actual.PackagePaths)
    {
        if (!ExpectedPackages.Contains(PackagePath))
        {
            OutError = FString::Printf(
                TEXT("Grass-surface namespace '%s' contains uncontracted loaded package '%s'."),
                *NamespaceRoot,
                *PackagePath);
            return false;
        }
    }
    for (const FString& PhysicalFile : Actual.PhysicalFiles)
    {
        if (!ExpectedPhysicalFiles.Contains(PhysicalFile))
        {
            OutError = FString::Printf(
                TEXT("Grass-surface namespace '%s' contains uncontracted physical file '%s'."),
                *NamespaceRoot,
                *PhysicalFile);
            return false;
        }
    }
    for (const FString& PhysicalDirectory : Actual.PhysicalDirectories)
    {
        if (!ExpectedPhysicalDirectories.Contains(PhysicalDirectory))
        {
            OutError = FString::Printf(
                TEXT("Grass-surface namespace '%s' contains uncontracted physical directory '%s'."),
                *NamespaceRoot,
                *PhysicalDirectory);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool NamespaceMatchesExactAssets(
    const FString& NamespaceRoot,
    const TArray<FString>& ExpectedObjectPaths,
    bool bRequirePhysicalAssetFiles,
    FString& OutError)
{
    if (!NamespaceContainsOnlyExpectedAssets(
            NamespaceRoot,
            ExpectedObjectPaths,
            OutError))
    {
        return false;
    }
    FNamespaceState Actual;
    CollectNamespaceState(NamespaceRoot, Actual);
    if (Actual.ObjectPaths.Num() != ExpectedObjectPaths.Num())
    {
        OutError = FString::Printf(
            TEXT("Grass-surface namespace '%s' has %d assets; expected exactly %d."),
            *NamespaceRoot,
            Actual.ObjectPaths.Num(),
            ExpectedObjectPaths.Num());
        return false;
    }
    for (const FString& ExpectedPath : ExpectedObjectPaths)
    {
        if (!Actual.ObjectPaths.Contains(ExpectedPath))
        {
            OutError = FString::Printf(
                TEXT("Grass-surface namespace '%s' is missing exact asset '%s'."),
                *NamespaceRoot,
                *ExpectedPath);
            return false;
        }
    }
    if (bRequirePhysicalAssetFiles)
    {
        TSet<FString> ExpectedPhysicalFiles;
        for (const FString& ExpectedPath : ExpectedObjectPaths)
        {
            FString PhysicalFile;
            if (!FPackageName::TryConvertLongPackageNameToFilename(
                    FPackageName::ObjectPathToPackageName(ExpectedPath),
                    PhysicalFile,
                    FPackageName::GetAssetPackageExtension()))
            {
                OutError = TEXT("Grass-surface exact physical output path could not be resolved.");
                return false;
            }
            FPaths::NormalizeFilename(PhysicalFile);
            ExpectedPhysicalFiles.Add(PhysicalFile);
        }
        if (Actual.PhysicalFiles.Num() != ExpectedPhysicalFiles.Num() ||
            Actual.PhysicalFiles.Difference(ExpectedPhysicalFiles).Num() != 0 ||
            ExpectedPhysicalFiles.Difference(Actual.PhysicalFiles).Num() != 0)
        {
            OutError = TEXT("Grass-surface saved namespace lacks the exact six physical .uasset files.");
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
            TEXT("Grass-surface asset '%s' lost exact metadata binding '%s'."),
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
        OutError = TEXT("Grass-surface output requires writable package metadata.");
        return false;
    }
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_GrassSurfaceCandidateContractSha256"),
        *CandidateContractSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_GrassSurfaceMaterialIntentSha256"),
        *MaterialIntentSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_GrassSurfaceProvenanceSha256"),
        *ProvenanceSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_GrassSurfaceAcceptedR33ReceiptSha256"),
        *Admission.AcceptedR33Sha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_GrassSurfaceFutureAuthorizationSha256"),
        *Admission.FutureAuthorizationSha256);
    Metadata->SetValue(
        Asset,
        TEXT("TRIAD_GrassSurfaceAuthority"),
        TEXT("APPEARANCE_ONLY_NO_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"));
    return ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceCandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceMaterialIntentSha256"),
               MaterialIntentSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceProvenanceSha256"),
               ProvenanceSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceAuthority"),
               TEXT("APPEARANCE_ONLY_NO_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"),
               OutError);
}

bool ValidateCommonMetadata(
    const UObject* Asset,
    const FAdmission& Admission,
    FString& OutError)
{
    return ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceCandidateContractSha256"),
               CandidateContractSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceMaterialIntentSha256"),
               MaterialIntentSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceProvenanceSha256"),
               ProvenanceSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceAcceptedR33ReceiptSha256"),
               Admission.AcceptedR33Sha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceFutureAuthorizationSha256"),
               Admission.FutureAuthorizationSha256,
               OutError) &&
        ExactAssetMetadata(
               Asset,
               TEXT("TRIAD_GrassSurfaceAuthority"),
               TEXT("APPEARANCE_ONLY_NO_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_OR_SIMULATION_AUTHORITY"),
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
        OutError = TEXT("Grass-surface texture lost its exact retained source bytes or single AssetImportData record.");
        return false;
    }
    const FAssetImportInfo& SourceData =
        Texture->AssetImportData->GetSourceData();
    if (SourceData.SourceFiles.Num() != 1 ||
        !SourceData.SourceFiles[0].FileHash.IsValid())
    {
        OutError = TEXT("Grass-surface texture AssetImportData lacks one valid source hash.");
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
        OutError = TEXT("Grass-surface texture import path is not the exact admitted file.");
        return false;
    }
    const TArray<uint8>& PinnedBytes = Admission.TextureSourceBytes[Index];
    FMD5 Md5Builder;
    Md5Builder.Update(
        PinnedBytes.GetData(),
        static_cast<uint64>(PinnedBytes.Num()));
    FMD5Hash ExpectedMd5;
    ExpectedMd5.Set(Md5Builder);
    if (BytesToHex(ExpectedMd5.GetBytes(), ExpectedMd5.GetSize()) !=
            FString(TextureMd5[Index]) ||
        SourceData.SourceFiles[0].FileHash != ExpectedMd5)
    {
        OutError = TEXT("Grass-surface texture AssetImportData MD5 does not match the SHA-256-admitted bytes.");
        return false;
    }
    OutError.Reset();
    return true;
#else
    OutError = TEXT("Grass-surface texture import validation requires editor-only data.");
    return false;
#endif
}

bool ValidateTextureSourcePayload(
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
        Texture->Source.GetSizeY() != 2048 ||
        Texture->Source.GetFormat() != TextureSourceFormats[Index] ||
        Texture->Source.GetSourceCompression() !=
            ETextureSourceCompressionFormat::TSCF_JPEG)
    {
        OutError = TEXT("Grass-surface source art lost its exact retained-JPEG, one-block/layer/slice/mip 2K format.");
        return false;
    }
    const TArray<uint8>& PinnedBytes = Admission.TextureSourceBytes[Index];
    const FSharedBuffer Payload = Texture->Source.GetBulkDataPayload();
    if (PinnedBytes.IsEmpty() || !Payload ||
        Payload.GetSize() != static_cast<uint64>(PinnedBytes.Num()) ||
        !Payload.GetData() ||
        FMemory::Memcmp(
            Payload.GetData(),
            PinnedBytes.GetData(),
            PinnedBytes.Num()) != 0)
    {
        OutError = TEXT("Grass-surface retained JPEG payload is not byte-exact to the admitted source.");
        return false;
    }
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (!SHA256(
            static_cast<const uint8*>(Payload.GetData()),
            static_cast<size_t>(Payload.GetSize()),
            Digest) ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper() !=
            FString(TextureSha256[Index]))
    {
        OutError = TEXT("Grass-surface retained JPEG payload failed its source SHA-256 pin.");
        return false;
    }
#else
    OutError = TEXT("Grass-surface payload validation requires WITH_SSL SHA-256 support.");
    return false;
#endif
    OutError.Reset();
    return true;
#else
    OutError = TEXT("Grass-surface payload validation requires editor-only source art.");
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
        Texture->GetSizeX() != 2048 || Texture->GetSizeY() != 2048 ||
        Texture->SRGB != bSrgb ||
        Texture->CompressionSettings != Compression ||
        Texture->LODGroup != TEXTUREGROUP_World ||
        Texture->MipGenSettings != TMGS_FromTextureGroup ||
        Texture->bFlipGreenChannel || Texture->AddressX != TA_Wrap ||
        Texture->AddressY != TA_Wrap || Texture->Filter != TF_Default ||
        Texture->VirtualTextureStreaming || Texture->NeverStream)
    {
        OutError = FString::Printf(
            TEXT("Grass-surface texture %d lost its exact 2K path, color/compression, DirectX, wrap, filter, mip, or streaming policy."),
            Index);
        return false;
    }
    return ValidateTextureImportProvenance(
               Texture,
               Index,
               Admission,
               OutError) &&
        ValidateTextureSourcePayload(Texture, Index, Admission, OutError);
}

template <typename TObjectType>
TObjectType* GetOnlyExactImportedObject(
    const UAssetImportTask* Task,
    const FString& ExpectedObjectPath,
    FString& OutError)
{
    if (!Task)
    {
        OutError = TEXT("Grass-surface import task is absent.");
        return nullptr;
    }
    const TArray<UObject*>& Objects = Task->GetObjects();
    if (Task->ImportedObjectPaths.Num() != 1 ||
        Task->ImportedObjectPaths[0] != ExpectedObjectPath ||
        Objects.Num() != 1 || !Objects[0] ||
        Objects[0]->GetPathName() != ExpectedObjectPath)
    {
        OutError = FString::Printf(
            TEXT("Grass-surface import must return one exact object at '%s'; aliases or companion outputs are denied."),
            *ExpectedObjectPath);
        return nullptr;
    }
    TObjectType* Result = Cast<TObjectType>(Objects[0]);
    if (!Result)
    {
        OutError = TEXT("Grass-surface import returned an unexpected asset class.");
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
            OutError = TEXT("Could not allocate all five exact grass-surface texture import tasks.");
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
        Task->DestinationPath = TextureRoot;
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
        if (!WriteCommonMetadata(Texture, Admission, OutError))
        {
            return false;
        }
        if (UMetaData* Metadata = Texture->GetOutermost()->GetMetaData())
        {
            Metadata->SetValue(
                Texture,
                TEXT("TRIAD_GrassSurfaceSourceSha256"),
                TextureSha256[Index]);
            Metadata->SetValue(
                Texture,
                TEXT("TRIAD_GrassSurfaceSourceRole"),
                TextureRoles[Index]);
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
            false,
            OutError))
    {
        return false;
    }
    OutError.Reset();
    return OutTextures.Num() == TextureCount;
}

bool InputIs(
    const FExpressionInput& Input,
    const UMaterialExpression* Expression,
    int32 OutputIndex = 0)
{
    return Input.Expression == Expression && Input.OutputIndex == OutputIndex;
}

template <typename TExpression>
TExpression* AddExpression(
    UMaterial* Material,
    const FString& NodeId,
    int32 X,
    int32 Y)
{
    TExpression* Expression = Material
        ? Cast<TExpression>(
              UMaterialEditingLibrary::CreateMaterialExpressionEx(
                  Material,
                  nullptr,
                  TExpression::StaticClass(),
                  nullptr,
                  X,
                  Y,
                  false))
        : nullptr;
    if (Expression)
    {
        Expression->Desc = NodeId;
    }
    return Expression;
}

UMaterialExpressionCustom* AddCustom(
    UMaterial* Material,
    const FString& NodeId,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    std::initializer_list<const TCHAR*> InputNames,
    int32 X,
    int32 Y)
{
    UMaterialExpressionCustom* Custom =
        AddExpression<UMaterialExpressionCustom>(Material, NodeId, X, Y);
    if (!Custom)
    {
        return nullptr;
    }
    Custom->Description = Description;
    Custom->Code = Code;
    Custom->OutputType = OutputType;
    Custom->Inputs.SetNum(static_cast<int32>(InputNames.size()));
    int32 Index = 0;
    for (const TCHAR* InputName : InputNames)
    {
        Custom->Inputs[Index].InputName = InputName;
        ++Index;
    }
    Custom->AdditionalOutputs.Reset();
    Custom->AdditionalDefines.Reset();
    Custom->IncludeFilePaths.Reset();
    return Custom;
}

FString TextureSampleNodeId(int32 Index, bool bPhase)
{
    return FString::Printf(
        TEXT("ODGrass.%s.%s"),
        TextureRoles[Index],
        bPhase ? TEXT("Phase") : TEXT("Primary"));
}

FString TextureParameterName(int32 Index, bool bPhase)
{
    return FString::Printf(
        TEXT("%s_%s_Provider1p4m"),
        TextureRoles[Index],
        bPhase ? TEXT("Phase") : TEXT("Primary"));
}

FString TextureBlendNodeId(int32 Index)
{
    return FString::Printf(TEXT("ODGrass.%s.DualPhaseBlend"), TextureRoles[Index]);
}

UMaterialExpressionTextureSampleParameter2D* AddTextureSample(
    UMaterial* Material,
    UTexture2D* Texture,
    int32 Index,
    bool bPhase,
    UMaterialExpression* Coordinates,
    int32 X,
    int32 Y)
{
    UMaterialExpressionTextureSampleParameter2D* Sample =
        AddExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material,
            TextureSampleNodeId(Index, bPhase),
            X,
            Y);
    if (!Sample || !Texture || !Coordinates)
    {
        return nullptr;
    }
    if (!Material->RemoveExpressionParameter(Sample))
    {
        return nullptr;
    }
    const FName ExactParameterName(TextureParameterName(Index, bPhase));
    Sample->ParameterName = ExactParameterName;
    Sample->UpdateParameterGuid(true, false);
    Sample->ValidateParameterName(false);
    if (Sample->ParameterName != ExactParameterName ||
        !Material->AddExpressionParameter(Sample, Material->EditorParameters))
    {
        return nullptr;
    }
    Sample->Texture = Texture;
    Sample->SamplerType = Index == 0
        ? SAMPLERTYPE_Color
        : (Index == 1 ? SAMPLERTYPE_Normal : SAMPLERTYPE_Masks);
    Sample->SamplerSource = SSM_FromTextureAsset;
    Sample->MipValueMode = TMVM_None;
    Sample->Coordinates.Connect(0, Coordinates);
    return Sample;
}

UMaterialExpressionLinearInterpolate* AddTextureBlend(
    UMaterial* Material,
    int32 Index,
    UMaterialExpressionTextureSampleParameter2D* Primary,
    UMaterialExpressionTextureSampleParameter2D* Phase,
    UMaterialExpressionCustom* BlendMask,
    int32 X,
    int32 Y)
{
    UMaterialExpressionLinearInterpolate* Blend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material,
            TextureBlendNodeId(Index),
            X,
            Y);
    if (!Blend || !Primary || !Phase || !BlendMask)
    {
        return nullptr;
    }
    const int32 SourceOutputIndex = Index >= 2 ? 1 : 0;
    Blend->A.Connect(SourceOutputIndex, Primary);
    Blend->B.Connect(SourceOutputIndex, Phase);
    Blend->Alpha.Connect(0, BlendMask);
    return Blend;
}

UMaterialExpressionCustom* AddNormalTextureBlend(
    UMaterial* Material,
    UMaterialExpressionTextureSampleParameter2D* Primary,
    UMaterialExpressionTextureSampleParameter2D* Phase,
    UMaterialExpressionCustom* BlendMask,
    int32 X,
    int32 Y)
{
    UMaterialExpressionCustom* Blend = AddCustom(
        Material,
        TextureBlendNodeId(1),
        NormalBlendDescription,
        NormalBlendCode,
        CMOT_Float3,
        {TEXT("PrimaryNormal"), TEXT("PhaseNormal"), TEXT("BlendMask")},
        X,
        Y);
    if (!Blend || !Primary || !Phase || !BlendMask)
    {
        return nullptr;
    }
    Blend->Inputs[0].Input.Connect(0, Primary);
    Blend->Inputs[1].Input.Connect(0, Phase);
    Blend->Inputs[2].Input.Connect(0, BlendMask);
    return Blend;
}

bool HasNoCustomizedUvConnections(const UMaterialEditorOnlyData* Data)
{
    if (!Data)
    {
        return false;
    }
    for (const FVector2MaterialInput& Input : Data->CustomizedUVs)
    {
        if (Input.Expression)
        {
            return false;
        }
    }
    return true;
}

bool ExpressionOwnershipAndParameterRegistryAreExact(
    const UMaterial* Material,
    const UMaterialEditorOnlyData* Data,
    const UMaterialExpressionTextureSampleParameter2D* const* Primary,
    const UMaterialExpressionTextureSampleParameter2D* const* Phase)
{
    if (!Material || !Data ||
        Material->EditorParameters.Num() != TextureCount * 2)
    {
        return false;
    }
    for (const UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        if (!Expression || Expression->Material != Material ||
            !Expression->MaterialExpressionGuid.IsValid())
        {
            return false;
        }
    }
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        const UMaterialExpressionTextureSampleParameter2D* Samples[] = {
            Primary[Index], Phase[Index]};
        for (const UMaterialExpressionTextureSampleParameter2D* Sample :
             Samples)
        {
            const TArray<UMaterialExpression*>* Registered = Sample
                ? Material->EditorParameters.Find(Sample->ParameterName)
                : nullptr;
            if (!Sample || !Sample->ExpressionGUID.IsValid() || !Registered ||
                Registered->Num() != 1 || (*Registered)[0] != Sample)
            {
                return false;
            }
        }
    }
    return true;
}

bool HasNoGeometryOrPhysicalAuthority(
    const UMaterial* Material,
    const UMaterialEditorOnlyData* Data)
{
    if (!Material || !Data || Material->bEnableTessellation ||
        Material->bEnableDisplacementFade ||
        !FMath::IsNearlyZero(
            Material->MaxWorldPositionOffsetDisplacement,
            0.000001f) ||
        Data->WorldPositionOffset.Expression ||
        Data->Displacement.Expression ||
        Data->PixelDepthOffset.Expression ||
        Data->MaterialAttributes.Expression || Data->FrontMaterial.Expression ||
        !HasNoCustomizedUvConnections(Data) || Material->PhysMaterial ||
        Material->PhysMaterialMask ||
        !Material->RenderTracePhysicalMaterialOutputs.IsEmpty() ||
        Material->NaniteOverrideMaterial.bEnableOverride ||
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

template <typename TExpression>
const TExpression* FindExactNode(
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

bool CustomIsExact(
    const UMaterialExpressionCustom* Custom,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    std::initializer_list<const TCHAR*> InputNames,
    std::initializer_list<const UMaterialExpression*> InputExpressions)
{
    if (!Custom || Custom->Description != Description ||
        Custom->Code != Code || Custom->OutputType != OutputType ||
        Custom->AdditionalOutputs.Num() != 0 ||
        Custom->AdditionalDefines.Num() != 0 ||
        Custom->IncludeFilePaths.Num() != 0 ||
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
            !InputIs(Custom->Inputs[Index].Input, *ExpressionIt))
        {
            return false;
        }
        ++Index;
        ++ExpressionIt;
    }
    return true;
}

bool SampleIsExact(
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
        Sample->ParameterName == FName(TextureParameterName(Index, bPhase)) &&
        Sample->Texture == Texture && Sample->SamplerType == ExpectedSampler &&
        Sample->SamplerSource == SSM_FromTextureAsset &&
        Sample->MipValueMode == TMVM_None &&
        InputIs(Sample->Coordinates, Coordinates);
}

bool BlendIsExact(
    const UMaterialExpressionLinearInterpolate* Blend,
    int32 Index,
    const UMaterialExpression* Primary,
    const UMaterialExpression* Phase,
    const UMaterialExpression* BlendMask)
{
    const int32 SourceOutputIndex = Index >= 2 ? 1 : 0;
    return Blend &&
        InputIs(Blend->A, Primary, SourceOutputIndex) &&
        InputIs(Blend->B, Phase, SourceOutputIndex) &&
        InputIs(Blend->Alpha, BlendMask);
}

bool NormalBlendIsExact(
    const UMaterialExpressionCustom* Blend,
    const UMaterialExpression* Primary,
    const UMaterialExpression* Phase,
    const UMaterialExpression* BlendMask)
{
    return CustomIsExact(
        Blend,
        NormalBlendDescription,
        NormalBlendCode,
        CMOT_Float3,
        {TEXT("PrimaryNormal"), TEXT("PhaseNormal"), TEXT("BlendMask")},
        {Primary, Phase, BlendMask});
}

bool ValidateMaterialGraph(
    UMaterial* Material,
    const TArray<UTexture2D*>& Textures,
    FString& OutError)
{
    constexpr int32 ExpectedExpressionCount = 25;
    if (Material)
    {
        // EditorParameters is a derived, non-UPROPERTY cache and is not
        // serialized. Rebuild it before exact validation so a cold-loaded
        // material is judged from its saved expression graph.
        Material->BuildEditorParameterList();
    }
    const UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const auto* World = FindExactNode<UMaterialExpressionWorldPosition>(
        Data,
        TEXT("ODGrass.AbsoluteWorldPositionNoOffsets"));
    const auto* Camera = FindExactNode<UMaterialExpressionCameraPositionWS>(
        Data,
        TEXT("ODGrass.CameraPositionWS"));
    const auto* PrimaryUv = FindExactNode<UMaterialExpressionCustom>(
        Data,
        TEXT("ODGrass.ProviderPrimaryUv"));
    const auto* PhaseUv = FindExactNode<UMaterialExpressionCustom>(
        Data,
        TEXT("ODGrass.ProviderPhaseUv"));
    const auto* BlendMask = FindExactNode<UMaterialExpressionCustom>(
        Data,
        TEXT("ODGrass.PhaseBlendMask"));
    const auto* ColorRoughness = FindExactNode<UMaterialExpressionCustom>(
        Data,
        TEXT("ODGrass.ColorRoughnessResponse"));
    const auto* Normal = FindExactNode<UMaterialExpressionCustom>(
        Data,
        TEXT("ODGrass.NormalResponse"));
    const auto* Ao = FindExactNode<UMaterialExpressionCustom>(
        Data,
        TEXT("ODGrass.AmbientOcclusionResponse"));
    const auto* ColorOutput = FindExactNode<UMaterialExpressionComponentMask>(
        Data,
        TEXT("ODGrass.BaseColorOutput"));
    const auto* RoughnessOutput =
        FindExactNode<UMaterialExpressionComponentMask>(
            Data,
            TEXT("ODGrass.RoughnessOutput"));
    const UMaterialExpressionTextureSampleParameter2D* Primary[TextureCount] = {};
    const UMaterialExpressionTextureSampleParameter2D* Phase[TextureCount] = {};
    const UMaterialExpression* Blends[TextureCount] = {};
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        Primary[Index] =
            FindExactNode<UMaterialExpressionTextureSampleParameter2D>(
                Data,
                TextureSampleNodeId(Index, false));
        Phase[Index] =
            FindExactNode<UMaterialExpressionTextureSampleParameter2D>(
                Data,
                TextureSampleNodeId(Index, true));
        Blends[Index] = Index == 1
            ? static_cast<const UMaterialExpression*>(
                  FindExactNode<UMaterialExpressionCustom>(
                      Data,
                      TextureBlendNodeId(Index)))
            : static_cast<const UMaterialExpression*>(
                  FindExactNode<UMaterialExpressionLinearInterpolate>(
                      Data,
                      TextureBlendNodeId(Index)));
    }

    bool bValid = Material && Data && Textures.Num() == TextureCount &&
        !Textures.Contains(nullptr) &&
        Material->GetClass() == UMaterial::StaticClass() &&
        Material->GetPathName() == MaterialObjectPath &&
        Material->MaterialDomain == MD_Surface &&
        Material->BlendMode == BLEND_Opaque && !Material->TwoSided &&
        Material->bTangentSpaceNormal && !Material->bUseMaterialAttributes &&
        Material->bUsedWithInstancedStaticMeshes &&
        !Material->bScreenSpaceReflections &&
        Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) &&
        HasNoGeometryOrPhysicalAuthority(Material, Data) &&
        Data->ExpressionCollection.Expressions.Num() ==
            ExpectedExpressionCount &&
        ExpressionOwnershipAndParameterRegistryAreExact(
            Material,
            Data,
            Primary,
            Phase) &&
        World &&
        World->WorldPositionShaderOffset == WPT_ExcludeAllShaderOffsets &&
        Camera &&
        CustomIsExact(
            PrimaryUv,
            PrimaryUvDescription,
            PrimaryUvCode,
            CMOT_Float2,
            {TEXT("AbsoluteWorldPosition")},
            {World}) &&
        CustomIsExact(
            PhaseUv,
            PhaseUvDescription,
            PhaseUvCode,
            CMOT_Float2,
            {TEXT("AbsoluteWorldPosition")},
            {World}) &&
        CustomIsExact(
            BlendMask,
            BlendMaskDescription,
            BlendMaskCode,
            CMOT_Float1,
            {TEXT("AbsoluteWorldPosition")},
            {World});
    for (int32 Index = 0; bValid && Index < TextureCount; ++Index)
    {
        bValid = SampleIsExact(
                     Primary[Index],
                     Textures[Index],
                     Index,
                     false,
                     PrimaryUv) &&
            SampleIsExact(
                     Phase[Index],
                     Textures[Index],
                     Index,
                     true,
                     PhaseUv) &&
            (Index == 1
                 ? NormalBlendIsExact(
                       Cast<UMaterialExpressionCustom>(Blends[Index]),
                       Primary[Index],
                       Phase[Index],
                       BlendMask)
                 : BlendIsExact(
                       Cast<UMaterialExpressionLinearInterpolate>(
                           Blends[Index]),
                       Index,
                       Primary[Index],
                       Phase[Index],
                       BlendMask));
    }
    bValid = bValid &&
        CustomIsExact(
            ColorRoughness,
            ColorRoughnessDescription,
            ColorRoughnessCode,
            CMOT_Float4,
            {TEXT("BaseColor"),
             TEXT("Roughness"),
             TEXT("AbsoluteWorldPosition"),
             TEXT("CameraPosition")},
            {Blends[0], Blends[2], World, Camera}) &&
        CustomIsExact(
            Normal,
            NormalDescription,
            NormalCode,
            CMOT_Float3,
            {TEXT("NormalDirectX"),
             TEXT("AbsoluteWorldPosition"),
             TEXT("CameraPosition")},
            {Blends[1], World, Camera}) &&
        CustomIsExact(
            Ao,
            AoDescription,
            AoCode,
            CMOT_Float1,
            {TEXT("AmbientOcclusion"),
             TEXT("RetainedHeightNoDisplacement"),
             TEXT("AbsoluteWorldPosition"),
             TEXT("CameraPosition")},
            {Blends[3], Blends[4], World, Camera}) &&
        ColorOutput && ColorOutput->R && ColorOutput->G && ColorOutput->B &&
        !ColorOutput->A && InputIs(ColorOutput->Input, ColorRoughness) &&
        RoughnessOutput && !RoughnessOutput->R && !RoughnessOutput->G &&
        !RoughnessOutput->B && RoughnessOutput->A &&
        InputIs(RoughnessOutput->Input, ColorRoughness) &&
        InputIs(Data->BaseColor, ColorOutput) &&
        InputIs(Data->Roughness, RoughnessOutput) &&
        InputIs(Data->Normal, Normal) &&
        InputIs(Data->AmbientOcclusion, Ao) &&
        !Data->Metallic.Expression && !Data->Specular.Expression &&
        !Data->Anisotropy.Expression && !Data->EmissiveColor.Expression &&
        !Data->Opacity.Expression && !Data->OpacityMask.Expression &&
        !Data->SubsurfaceColor.Expression && !Data->Refraction.Expression &&
        !Data->ClearCoat.Expression &&
        !Data->ClearCoatRoughness.Expression &&
        !Data->ShadingModelFromMaterialExpression.Expression &&
        !Data->SurfaceThickness.Expression;
    if (!bValid)
    {
        OutError = TEXT("Grass-surface material lost its exact 25-node 1.4 m dual-phase, 7/3.5/8.4/18.2 m anti-repeat, 20/35/50 m response, or no-authority graph.");
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterial* CreateMaterial(
    IAssetTools& AssetTools,
    const FAdmission& Admission,
    const TArray<UTexture2D*>& Textures,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              MaterialAssetName,
              MaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.MaterializeOrdinaryDistanceGrassSurfaceAssets"))))
        : nullptr;
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !Data || Textures.Num() != TextureCount ||
        Textures.Contains(nullptr) ||
        !Data->ExpressionCollection.Expressions.IsEmpty() ||
        Material->GetPathName() != MaterialObjectPath)
    {
        OutError = TEXT("Could not create the exact empty grass-surface material destination.");
        return nullptr;
    }

    auto* World = AddExpression<UMaterialExpressionWorldPosition>(
        Material,
        TEXT("ODGrass.AbsoluteWorldPositionNoOffsets"),
        -1500,
        -120);
    auto* Camera = AddExpression<UMaterialExpressionCameraPositionWS>(
        Material,
        TEXT("ODGrass.CameraPositionWS"),
        -1500,
        40);
    auto* PrimaryUv = AddCustom(
        Material,
        TEXT("ODGrass.ProviderPrimaryUv"),
        PrimaryUvDescription,
        PrimaryUvCode,
        CMOT_Float2,
        {TEXT("AbsoluteWorldPosition")},
        -1260,
        -300);
    auto* PhaseUv = AddCustom(
        Material,
        TEXT("ODGrass.ProviderPhaseUv"),
        PhaseUvDescription,
        PhaseUvCode,
        CMOT_Float2,
        {TEXT("AbsoluteWorldPosition")},
        -1260,
        -150);
    auto* BlendMask = AddCustom(
        Material,
        TEXT("ODGrass.PhaseBlendMask"),
        BlendMaskDescription,
        BlendMaskCode,
        CMOT_Float1,
        {TEXT("AbsoluteWorldPosition")},
        -1260,
        20);
    if (!World || !Camera || !PrimaryUv || !PhaseUv || !BlendMask)
    {
        OutError = TEXT("Could not allocate the grass-surface world/UV/mask graph.");
        return nullptr;
    }
    World->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    PrimaryUv->Inputs[0].Input.Connect(0, World);
    PhaseUv->Inputs[0].Input.Connect(0, World);
    BlendMask->Inputs[0].Input.Connect(0, World);

    UMaterialExpressionTextureSampleParameter2D* Primary[TextureCount] = {};
    UMaterialExpressionTextureSampleParameter2D* Phase[TextureCount] = {};
    UMaterialExpression* Blends[TextureCount] = {};
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        const int32 Y = -520 + Index * 220;
        Primary[Index] = AddTextureSample(
            Material,
            Textures[Index],
            Index,
            false,
            PrimaryUv,
            -960,
            Y);
        Phase[Index] = AddTextureSample(
            Material,
            Textures[Index],
            Index,
            true,
            PhaseUv,
            -960,
            Y + 90);
        Blends[Index] = Index == 1
            ? static_cast<UMaterialExpression*>(AddNormalTextureBlend(
                  Material,
                  Primary[Index],
                  Phase[Index],
                  BlendMask,
                  -650,
                  Y + 40))
            : static_cast<UMaterialExpression*>(AddTextureBlend(
                  Material,
                  Index,
                  Primary[Index],
                  Phase[Index],
                  BlendMask,
                  -650,
                  Y + 40));
        if (!Primary[Index] || !Phase[Index] || !Blends[Index])
        {
            OutError = TEXT("Could not allocate all ten grass-surface samples and five phase blends.");
            return nullptr;
        }
    }
    Material->BuildEditorParameterList();

    auto* ColorRoughness = AddCustom(
        Material,
        TEXT("ODGrass.ColorRoughnessResponse"),
        ColorRoughnessDescription,
        ColorRoughnessCode,
        CMOT_Float4,
        {TEXT("BaseColor"),
         TEXT("Roughness"),
         TEXT("AbsoluteWorldPosition"),
         TEXT("CameraPosition")},
        -330,
        -330);
    auto* Normal = AddCustom(
        Material,
        TEXT("ODGrass.NormalResponse"),
        NormalDescription,
        NormalCode,
        CMOT_Float3,
        {TEXT("NormalDirectX"),
         TEXT("AbsoluteWorldPosition"),
         TEXT("CameraPosition")},
        -330,
        10);
    auto* Ao = AddCustom(
        Material,
        TEXT("ODGrass.AmbientOcclusionResponse"),
        AoDescription,
        AoCode,
        CMOT_Float1,
        {TEXT("AmbientOcclusion"),
         TEXT("RetainedHeightNoDisplacement"),
         TEXT("AbsoluteWorldPosition"),
         TEXT("CameraPosition")},
        -330,
        250);
    auto* ColorOutput = AddExpression<UMaterialExpressionComponentMask>(
        Material,
        TEXT("ODGrass.BaseColorOutput"),
        -60,
        -330);
    auto* RoughnessOutput = AddExpression<UMaterialExpressionComponentMask>(
        Material,
        TEXT("ODGrass.RoughnessOutput"),
        -60,
        -170);
    if (!ColorRoughness || !Normal || !Ao || !ColorOutput ||
        !RoughnessOutput)
    {
        OutError = TEXT("Could not allocate the exact grass-surface response/output graph.");
        return nullptr;
    }
    ColorRoughness->Inputs[0].Input.Connect(0, Blends[0]);
    ColorRoughness->Inputs[1].Input.Connect(0, Blends[2]);
    ColorRoughness->Inputs[2].Input.Connect(0, World);
    ColorRoughness->Inputs[3].Input.Connect(0, Camera);
    Normal->Inputs[0].Input.Connect(0, Blends[1]);
    Normal->Inputs[1].Input.Connect(0, World);
    Normal->Inputs[2].Input.Connect(0, Camera);
    Ao->Inputs[0].Input.Connect(0, Blends[3]);
    Ao->Inputs[1].Input.Connect(0, Blends[4]);
    Ao->Inputs[2].Input.Connect(0, World);
    Ao->Inputs[3].Input.Connect(0, Camera);
    ColorOutput->R = true;
    ColorOutput->G = true;
    ColorOutput->B = true;
    ColorOutput->A = false;
    ColorOutput->Input.Connect(0, ColorRoughness);
    RoughnessOutput->R = false;
    RoughnessOutput->G = false;
    RoughnessOutput->B = false;
    RoughnessOutput->A = true;
    RoughnessOutput->Input.Connect(0, ColorRoughness);

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->bScreenSpaceReflections = false;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->PhysMaterial = nullptr;
    Material->PhysMaterialMask = nullptr;
    for (TObjectPtr<UPhysicalMaterial>& PhysicalMaterial :
         Material->PhysicalMaterialMap)
    {
        PhysicalMaterial = nullptr;
    }
    Material->RenderTracePhysicalMaterialOutputs.Reset();
    Material->NaniteOverrideMaterial.bEnableOverride = false;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    Data->BaseColor.Connect(0, ColorOutput);
    Data->Roughness.Connect(0, RoughnessOutput);
    Data->Normal.Connect(0, Normal);
    Data->AmbientOcclusion.Connect(0, Ao);
    if (!WriteCommonMetadata(Material, Admission, OutError))
    {
        return nullptr;
    }
    if (UMetaData* Metadata = Material->GetOutermost()->GetMetaData())
    {
        Metadata->SetValue(
            Material,
            TEXT("TRIAD_GrassSurfacePhysicalTileMetres"),
            TEXT("1.4"));
        Metadata->SetValue(
            Material,
            TEXT("TRIAD_GrassSurfaceDistancePolicyMetres"),
            TEXT("full_source_through_20_normal_0.8_0.62_0.48_at_20_35_50_fade_50_70"));
        Metadata->SetValue(
            Material,
            TEXT("TRIAD_GrassSurfaceHeightRoute"),
            TEXT("RETAINED_INPUT_NO_DISPLACEMENT_NO_GEOMETRY_AUTHORITY"));
        Metadata->SetValue(
            Material,
            TEXT("TRIAD_GrassSurfaceOptionalFallback"),
            *UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
                ExistingLawnFallbackObjectPath());
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    if (!ValidateMaterialGraph(Material, Textures, OutError))
    {
        return nullptr;
    }
    return Material;
}

bool ValidateOutputAssets(
    const FAdmission& Admission,
    TArray<UObject*>* OutAssets,
    FString& OutError)
{
    if (OutAssets)
    {
        OutAssets->Reset();
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UTexture2D*> Textures;
    for (int32 Index = 0; Index < TextureCount; ++Index)
    {
        UTexture2D* Texture = LoadExact<UTexture2D>(TextureObjectPath(Index));
        if (!ValidateTexture(Texture, Index, Admission, OutError) ||
            !ValidateCommonMetadata(Texture, Admission, OutError) ||
            !ExactAssetMetadata(
                Texture,
                TEXT("TRIAD_GrassSurfaceSourceSha256"),
                TextureSha256[Index],
                OutError) ||
            !ExactAssetMetadata(
                Texture,
                TEXT("TRIAD_GrassSurfaceSourceRole"),
                TextureRoles[Index],
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
    UMaterial* Material = LoadExact<UMaterial>(MaterialObjectPath);
    if (!ValidateMaterialGraph(Material, Textures, OutError) ||
        !ValidateCommonMetadata(Material, Admission, OutError) ||
        !ExactAssetMetadata(
            Material,
            TEXT("TRIAD_GrassSurfacePhysicalTileMetres"),
            TEXT("1.4"),
            OutError) ||
        !ExactAssetMetadata(
            Material,
            TEXT("TRIAD_GrassSurfaceDistancePolicyMetres"),
            TEXT("full_source_through_20_normal_0.8_0.62_0.48_at_20_35_50_fade_50_70"),
            OutError) ||
        !ExactAssetMetadata(
            Material,
            TEXT("TRIAD_GrassSurfaceHeightRoute"),
            TEXT("RETAINED_INPUT_NO_DISPLACEMENT_NO_GEOMETRY_AUTHORITY"),
            OutError) ||
        !ExactAssetMetadata(
            Material,
            TEXT("TRIAD_GrassSurfaceOptionalFallback"),
            UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
                ExistingLawnFallbackObjectPath(),
            OutError))
    {
        return false;
    }

    FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceAssets RuntimeAssets;
    RuntimeAssets.SurfaceMaterial = Material;
    for (UTexture2D* Texture : Textures)
    {
        RuntimeAssets.SourceTextures.Add(Texture);
    }
    if (!UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
            ValidateAssetRoster(RuntimeAssets, OutError))
    {
        return false;
    }
    if (OutAssets)
    {
        OutAssets->Add(Material);
        TSet<FString> UniquePaths;
        for (const UObject* Asset : *OutAssets)
        {
            UniquePaths.Add(Asset ? Asset->GetPathName() : FString());
        }
        if (OutAssets->Num() != OutputAssetCount ||
            UniquePaths.Num() != OutputAssetCount ||
            UniquePaths.Contains(FString()))
        {
            OutError = TEXT("Grass-surface output roster is not exactly six unique assets.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool DeleteFreshNamespaceContents(FString& OutError)
{
    FString PhysicalRoot;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            OutputRoot,
            PhysicalRoot,
            FString()))
    {
        OutError = TEXT("Fresh grass-surface rollback cannot resolve its exact physical package root.");
        return false;
    }
    FPaths::NormalizeDirectoryName(PhysicalRoot);
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutError = TEXT("Fresh grass-surface rollback requires EditorAssetSubsystem.");
        return false;
    }
    TArray<UObject*> LoadedAssets;
    for (TObjectIterator<UObject> It; It; ++It)
    {
        UObject* Object = *It;
        UPackage* Package = Object ? Object->GetOutermost() : nullptr;
        if (Object && IsValid(Object) && Object->IsAsset() && Package &&
            IsPackageWithinNamespace(Package->GetName(), OutputRoot))
        {
            LoadedAssets.AddUnique(Object);
        }
    }
    const bool bLoadedDeleteSucceeded = LoadedAssets.IsEmpty() ||
        AssetSubsystem->DeleteLoadedAssets(LoadedAssets);
    const bool bDirectoryDeleteSucceeded =
        !AssetSubsystem->DoesDirectoryExist(OutputRoot) ||
        AssetSubsystem->DeleteDirectory(OutputRoot);
    const bool bPhysicalDirectoryAbsent =
        !IFileManager::Get().DirectoryExists(*PhysicalRoot);
    const TArray<FString> Empty;
    FString ResidualError;
    const bool bEmpty = NamespaceMatchesExactAssets(
        OutputRoot,
        Empty,
        false,
        ResidualError);
    if (!bLoadedDeleteSucceeded || !bDirectoryDeleteSucceeded ||
        !bPhysicalDirectoryAbsent || !bEmpty)
    {
        OutError = FString::Printf(
            TEXT("Fresh grass-surface rollback could not prove cleanup (DeleteLoadedAssets=%s DeleteDirectory=%s PhysicalDirectoryAbsent=%s NamespaceEmpty=%s): %s"),
            bLoadedDeleteSucceeded ? TEXT("true") : TEXT("false"),
            bDirectoryDeleteSucceeded ? TEXT("true") : TEXT("false"),
            bPhysicalDirectoryAbsent ? TEXT("true") : TEXT("false"),
            bEmpty ? TEXT("true") : TEXT("false"),
            *ResidualError);
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
        if (!InOutError.IsEmpty())
        {
            InOutError += TEXT(" ");
        }
        InOutError += TEXT("ROLLBACK_REFUSED: isolated output namespace was not proven empty at entry.");
        return false;
    }
    FString RollbackError;
    if (!DeleteFreshNamespaceContents(RollbackError))
    {
        if (!InOutError.IsEmpty())
        {
            InOutError += TEXT(" ");
        }
        InOutError += TEXT("ROLLBACK_FAILED: ") + RollbackError;
        return false;
    }
    if (!InOutError.IsEmpty())
    {
        InOutError += TEXT(" ");
    }
    InOutError += TEXT("ROLLBACK_COMPLETE recursivelyDeletedFreshOutputNamespace=true registryAssets=0 liveAssets=0 loadedPackages=0 physicalFiles=0 physicalDirectories=0 physicalRootAbsent=true");
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceEditorLibrary::
    InspectGrassSurfaceReceipts(
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
        OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_RECEIPT_INSPECTION_DENIED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_RECEIPTS_INSPECTED_ONLY executionAuthorized=false callerSuppliedHashesNeverAuthorizeExecution=true privateMaterializerInvoked=false sourceOnlyRead=true acceptedR33ReceiptHashMatchedCallerPinAndHumanAcceptedFields=true separateFutureAuthorizationHashMatchedCallerPinAndExactNarrowFieldRoster=true candidateContractMaterialIntentProvenanceInventoryAuditEvidenceAndFiveTexturesHashPinned=true assetsWritten=false mapOrComponentBindingModified=false existingLawnFallbackReplaced=false geographyTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false UnrealLaunched=false nativeCompileImportRuntimeCapturePerformanceOrVisualAcceptanceClaimed=false");
    return true;
}

bool UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceEditorLibrary::
    MaterializeTrustedGrassSurfaceAssetsInternal(
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
        OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_MATERIALIZATION_DENIED: ") +
            Error;
        return false;
    }

    TArray<FString> ExpectedPaths;
    const TArray<FString> Empty;
    GetOutputObjectPaths(ExpectedPaths);
    if (!NamespaceContainsOnlyExpectedAssets(OutputRoot, ExpectedPaths, Error))
    {
        OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_UNEXPECTED_NAMESPACE_ASSET_DENIED: ") +
            Error;
        return false;
    }
    const int32 ExistingCount = ExistingOutputPackageCount();
    if (ExistingCount == OutputAssetCount)
    {
        if (!NamespaceMatchesExactAssets(
                OutputRoot,
                ExpectedPaths,
                true,
                Error) ||
            !ValidateOutputAssets(Admission, nullptr, Error))
        {
            OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_EXISTING_ASSETS_INVALID: ") +
                Error;
            return false;
        }
        OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_ASSETS_VALID existing=6 created=0 textures=5 material=1 exactPerAssetSourceIntentProvenanceAndAuthorizationMetadata=true recursiveNamespaceExact=true optionalPresentationMaterialAvailable=true existingLawnFallbackPreserved=true mapOrComponentBindingModified=false sourceMaterialsModified=false sourceTransformsModified=false geographyTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false nativeColdReloadRuntimeCapturePerformanceOrVisualAcceptanceClaimed=false");
        return true;
    }
    if (ExistingCount != 0)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_PARTIAL_NAMESPACE_DENIED existing=%d expectedEither=0_or_6"),
            ExistingCount);
        return false;
    }
    if (!NamespaceMatchesExactAssets(OutputRoot, Empty, false, Error))
    {
        OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_OUTPUT_NAMESPACE_NOT_EMPTY_DENIED: ") +
            Error;
        return false;
    }
    const bool bOutputRootProvenEmptyAtEntry = true;

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
            .Get();
    TArray<UObject*> AssetsToSave;
    TArray<UTexture2D*> Textures;
    if (!ImportTextures(
            AssetTools,
            Admission,
            Textures,
            AssetsToSave,
            Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_TEXTURE_IMPORT_FAILED: ") +
            Error;
        return false;
    }
    UMaterial* Material = CreateMaterial(
        AssetTools,
        Admission,
        Textures,
        Error);
    if (!Material)
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_MATERIAL_CREATE_FAILED: ") +
            Error;
        return false;
    }
    AssetsToSave.Add(Material);

    TArray<UObject*> ValidatedAssets;
    TSet<FString> UniquePaths;
    for (const UObject* Asset : AssetsToSave)
    {
        UniquePaths.Add(Asset ? Asset->GetPathName() : FString());
    }
    if (AssetsToSave.Num() != OutputAssetCount ||
        UniquePaths.Num() != OutputAssetCount ||
        UniquePaths.Contains(FString()) ||
        !NamespaceMatchesExactAssets(
            OutputRoot,
            ExpectedPaths,
            false,
            Error) ||
        !ValidateOutputAssets(Admission, &ValidatedAssets, Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_PRE_SAVE_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem ||
        !AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        Error = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_EXACT_SIX_ASSET_SAVE_FAILED");
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = Error;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!NamespaceMatchesExactAssets(
            OutputRoot,
            ExpectedPaths,
            true,
            Error) ||
        !ValidateOutputAssets(Admission, nullptr, Error))
    {
        RollbackFreshMaterialization(bOutputRootProvenEmptyAtEntry, Error);
        OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_POST_SAVE_VALIDATION_FAILED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_ORDINARY_DISTANCE_GRASS_SURFACE_ASSETS_MATERIALIZED created=6 textures=5 material=1 sourceTexturesByteExact=true providerTileMetres=1.4 dualEqualScaleRotatedPhases=true phaseBlendScaleMetres=7 mesoScaleMetres=3.5 macroScalesMetres=8.4,18.2 normalStrengthAtMetres20,35,50=0.8,0.62,0.48 responseFadeMetres=50,70 heightRetainedWithoutDisplacement=true exactPerAssetSourceIntentProvenanceAndAuthorizationMetadata=true recursiveNamespaceExact=true freshFailureRollbackScope=isolatedNamespaceProvenEmptyAtEntry optionalPresentationMaterialAvailable=true existingLawnFallbackPreserved=true mapOrComponentBindingModified=false sourceMaterialsModified=false sourceTransformsModified=false geographyTerrainCollisionNavigationLosRfSensorSimulationAuthorityModified=false nativeCompileColdReloadRuntimeCapturePerformanceAndHumanVisualAcceptanceStillRequired=true");
    return true;
}
