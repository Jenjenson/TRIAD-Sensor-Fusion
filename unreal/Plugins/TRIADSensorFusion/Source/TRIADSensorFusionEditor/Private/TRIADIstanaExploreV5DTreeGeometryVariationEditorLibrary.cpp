#include "TRIADIstanaExploreV5DTreeGeometryVariationEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DTreeGeometryVariationActor.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <openssl/sha.h>

namespace
{
constexpr int32 FormCount = 5;
constexpr int32 VariantsPerForm = 3;
constexpr int32 CandidateCount = FormCount * VariantsPerForm;
constexpr int32 SourceAnchorCount = 736;
constexpr int64 MaximumJsonBytes = 16 * 1024 * 1024;
constexpr int64 CandidateContractBytes = 28625;
constexpr int64 SelectorManifestBytes = 227267;

const FString CandidateContractSha256(
    TEXT("3E8216CBB0D78E755C1597030C3C513ACE70E5412464620EEDAF5530AD54DF31"));
const FString SelectorManifestSha256(
    TEXT("59BA4B21B81A3546CBE76E1108D6EB39C2DEA1DD0CBB309791515F9AC5B59E51"));

// Deliberately invalid SHA-256 sentinels. Caller-provided receipt paths and
// hashes may be inspected, but cannot bootstrap execution authority. A future
// reviewed source change must replace both values with exact trusted hashes
// and recompile before the private materializer can pass BuildAdmission.
const FString TrustedAcceptedR33ReceiptSha256(
    TEXT("UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));
const FString TrustedFutureAuthorizationSha256(
    TEXT("UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"));
const FString CandidateSchema(
    TEXT("triad.istana_public_view_explore_v5d_tree_realism.geometry_variation_candidate.v4"));
const FString SelectorSchema(
    TEXT("triad.istana_public_view_explore_v5d_tree_realism.geometry_variation_candidate.instance_selector.v4"));
const FString AcceptedR33Schema(
    TEXT("triad.istana_explore_v5d.r33_player0_capture.v1"));
const FString R33TransactionSchema(
    TEXT("triad.istana_explore_v5d.r33_cesium_world_terrain_reference.native_transaction.v1"));
const FString FutureAuthorizationSchema(
    TEXT("triad.istana_public_view_explore_v5d_tree_realism.geometry_variation.future_transaction_authority.v1"));
const FString FutureOperation(
    TEXT("MATERIALIZE_TREE_GEOMETRY_VARIATION_CANDIDATE_V4_ASSETS_ONLY"));
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));

const TCHAR* const BaseMeshObjectPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Dome_NearLOD0.SM_IPV5D_Tree_Dome_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_HighForkRounded_NearLOD0.SM_IPV5D_Tree_HighForkRounded_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_ColumnarNarrow_NearLOD0.SM_IPV5D_Tree_ColumnarNarrow_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Palm_NearLOD0.SM_IPV5D_Tree_Palm_NearLOD0")};
const TCHAR* const FormNames[] = {
    TEXT("umbrella"), TEXT("dome"), TEXT("highForkRounded"),
    TEXT("columnar"), TEXT("palm")};
const int32 ExpectedLodCounts[] = {4, 4, 4, 4, 3};
static_assert(UE_ARRAY_COUNT(BaseMeshObjectPaths) == FormCount);
static_assert(UE_ARRAY_COUNT(FormNames) == FormCount);
static_assert(UE_ARRAY_COUNT(ExpectedLodCounts) == FormCount);

struct FAdmission
{
    TSharedPtr<FJsonObject> CandidateContract;
    TSharedPtr<FJsonObject> SelectorManifest;
    FString AcceptedR33Sha256;
    FString FutureAuthorizationSha256;
};

template <typename TObjectType>
TObjectType* LoadExact(const FString& ObjectPath)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
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

bool LoadHashPinnedJson(
    const FString& InputPath,
    const FString& ExpectedSha256,
    int64 ExpectedBytes,
    TSharedPtr<FJsonObject>& OutRoot,
    FString& OutError)
{
    OutRoot.Reset();
    if (InputPath.IsEmpty() || FPaths::IsRelative(InputPath) ||
        !IsSha256(ExpectedSha256))
    {
        OutError = TEXT("Admission JSON requires an absolute path and explicit SHA-256 pin.");
        return false;
    }
    FString FullPath = FPaths::ConvertRelativePathToFull(InputPath);
    FPaths::NormalizeFilename(FullPath);
    const int64 ActualBytes = IFileManager::Get().FileSize(*FullPath);
    if (ActualBytes < 2 || ActualBytes > MaximumJsonBytes ||
        (ExpectedBytes >= 0 && ActualBytes != ExpectedBytes))
    {
        OutError = TEXT("Admission JSON byte count is absent, unsafe, or different from its frozen pin.");
        return false;
    }
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *FullPath) ||
        Bytes.Num() != ActualBytes)
    {
        OutError = TEXT("Admission JSON could not be read exactly once.");
        return false;
    }
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(),
            static_cast<size_t>(Bytes.Num()),
            Digest) == nullptr ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper() !=
            ExpectedSha256.ToUpper())
    {
        OutError = TEXT("Admission JSON SHA-256 does not match the caller pin.");
        return false;
    }
    FString Text;
    FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
    if (Text.IsEmpty())
    {
        OutError = TEXT("Admission JSON text decode failed.");
        return false;
    }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, OutRoot) ||
        !OutRoot.IsValid())
    {
        OutError = TEXT("Admission JSON parse failed.");
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
        FMath::IsNearlyEqual(Actual, Expected, 0.0000001);
}

bool ExactNumberPair(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const FVector2f& Expected)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    return Object.IsValid() && Object->TryGetArrayField(Field, Values) &&
        Values && Values->Num() == 2 &&
        FMath::IsNearlyEqual((*Values)[0]->AsNumber(), Expected.X, 0.0000001) &&
        FMath::IsNearlyEqual((*Values)[1]->AsNumber(), Expected.Y, 0.0000001);
}

int32 FormIndexFromName(const FString& Name)
{
    for (int32 Index = 0; Index < FormCount; ++Index)
    {
        if (Name == FormNames[Index])
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

bool ValidateCandidateContract(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Delivery = nullptr;
    const TSharedPtr<FJsonObject>* Sequencing = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
    if (!ExactString(Root, TEXT("schema"), CandidateSchema) ||
        !ExactString(
            Root,
            TEXT("status"),
            TEXT("UNADMITTED_VISUAL_REFERENCE_ONLY")) ||
        !Root->TryGetObjectField(TEXT("deliveryState"), Delivery) ||
        !Delivery || !Delivery->IsValid() ||
        !ExactInteger(*Delivery, TEXT("candidateRecipeCount"), CandidateCount) ||
        !ExactInteger(*Delivery, TEXT("forms"), FormCount) ||
        !ExactInteger(*Delivery, TEXT("variantsPerForm"), VariantsPerForm) ||
        !ExactBool(*Delivery, TEXT("meshAssetsMaterialized"), false) ||
        !ExactBool(*Delivery, TEXT("unrealPackagesWritten"), false) ||
        !ExactBool(*Delivery, TEXT("mapModified"), false) ||
        !ExactBool(*Delivery, TEXT("nativeIntegrationAuthority"), false) ||
        !ExactBool(*Delivery, TEXT("nativeVisualAcceptanceProvided"), false) ||
        !Root->TryGetObjectField(TEXT("sequencing"), Sequencing) ||
        !Sequencing || !Sequencing->IsValid() ||
        !ExactString(*Sequencing, TEXT("predecessorAcceptanceRequired"), TEXT("R33")) ||
        !ExactBool(*Sequencing, TEXT("futureSuccessorTransactionRequired"), true) ||
        !ExactBool(*Sequencing, TEXT("futureSuccessorIdentifierAssigned"), false) ||
        !ExactBool(*Sequencing, TEXT("queuedTransactionOrCaptureFilesModified"), false) ||
        !Root->TryGetArrayField(TEXT("candidateVariants"), Candidates) ||
        !Candidates || Candidates->Num() != CandidateCount)
    {
        OutError = TEXT("Geometry candidate contract is not the exact unadmitted post-R33 source pack.");
        return false;
    }

    for (int32 Index = 0; Index < CandidateCount; ++Index)
    {
        const TSharedPtr<FJsonObject> Row = (*Candidates)[Index]->AsObject();
        const TSharedPtr<FJsonObject>* Parameters = nullptr;
        FTRIADIstanaExploreV5DTreeGeometryVariationRecipe Recipe;
        FString Form;
        FString Selector;
        if (!Row.IsValid() ||
            !ATRIADIstanaExploreV5DTreeGeometryVariationActor::GetRecipe(
                Index,
                Recipe) ||
            !Row->TryGetStringField(TEXT("form"), Form) ||
            !Row->TryGetStringField(TEXT("selector"), Selector) ||
            FormIndexFromName(Form) != static_cast<int32>(Recipe.Form) ||
            Selector.Len() != 1 || Selector[0] != Recipe.Selector ||
            !ExactString(
                Row,
                TEXT("candidatePackagePath"),
                ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                    CandidatePackagePath(Index)) ||
            !ExactString(
                Row,
                TEXT("candidateObjectPath"),
                ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                    CandidateObjectPath(Index)) ||
            !Row->TryGetObjectField(TEXT("parameters"), Parameters) ||
            !Parameters || !Parameters->IsValid() ||
            !ExactNumber(
                *Parameters,
                TEXT("rootHoldNormalizedHeight"),
                Recipe.RootHoldNormalizedHeight) ||
            !ExactNumber(
                *Parameters,
                TEXT("crownStartNormalizedHeight"),
                Recipe.CrownStartNormalizedHeight) ||
            !ExactNumber(
                *Parameters,
                TEXT("crownFullNormalizedHeight"),
                Recipe.CrownFullNormalizedHeight) ||
            !ExactNumberPair(
                *Parameters,
                TEXT("trunkScaleXY"),
                Recipe.TrunkScaleXY) ||
            !ExactNumberPair(
                *Parameters,
                TEXT("crownScaleXY"),
                Recipe.CrownScaleXY) ||
            !ExactNumber(
                *Parameters,
                TEXT("heightScale"),
                Recipe.HeightScale) ||
            !ExactNumberPair(
                *Parameters,
                TEXT("bendBySourceHeightXY"),
                Recipe.BendBySourceHeightXY) ||
            !ExactNumber(
                *Parameters,
                TEXT("twistDegrees"),
                Recipe.TwistDegrees) ||
            !ExactNumber(
                *Parameters,
                TEXT("radialRippleFraction"),
                Recipe.RadialRippleFraction) ||
            !ExactInteger(
                *Parameters,
                TEXT("radialLobes"),
                Recipe.RadialLobes) ||
            !ExactNumber(
                *Parameters,
                TEXT("radialPhaseDegrees"),
                Recipe.RadialPhaseDegrees))
        {
            OutError = FString::Printf(
                TEXT("Geometry candidate recipe %d drifted from the compiled materializer."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateSelectorManifest(
    const TSharedPtr<FJsonObject>& Root,
    FString& OutError)
{
    const TSharedPtr<FJsonObject>* Census = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!ExactString(Root, TEXT("schema"), SelectorSchema) ||
        !Root->TryGetObjectField(TEXT("sourceCensus"), Census) ||
        !Census || !Census->IsValid() ||
        !ExactInteger(*Census, TEXT("v4Main"), 720) ||
        !ExactInteger(*Census, TEXT("v4Heritage"), 9) ||
        !ExactInteger(*Census, TEXT("r29Landmark"), 7) ||
        !ExactInteger(*Census, TEXT("combined"), SourceAnchorCount) ||
        !Root->TryGetArrayField(TEXT("rows"), Rows) || !Rows ||
        Rows->Num() != SourceAnchorCount)
    {
        OutError = TEXT("Tree geometry selector manifest lost its exact schema or 720+9+7 census.");
        return false;
    }
    for (int32 Ordinal = 0; Ordinal < SourceAnchorCount; ++Ordinal)
    {
        const TSharedPtr<FJsonObject> Row = (*Rows)[Ordinal]->AsObject();
        FString Key;
        FString Selector;
        FString Domain;
        const FString ExpectedDomain = Ordinal < 720
            ? TEXT("v4Main")
            : (Ordinal < 729 ? TEXT("v4Heritage") : TEXT("r29Landmark"));
        const int32 ExpectedIndex = Ordinal < 720
            ? Ordinal
            : (Ordinal < 729 ? Ordinal - 720 : Ordinal - 729);
        if (!Row.IsValid() || !ExactInteger(Row, TEXT("ordinal"), Ordinal) ||
            !Row->TryGetStringField(TEXT("sourceInstanceKey"), Key) ||
            Key != ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                       ExpectedSourceInstanceKey(Ordinal) ||
            !Row->TryGetStringField(TEXT("sourceDomain"), Domain) ||
            Domain != ExpectedDomain ||
            !ExactInteger(Row, TEXT("sourceIndex"), ExpectedIndex) ||
            !Row->TryGetStringField(TEXT("variantSelector"), Selector) ||
            Selector.Len() != 1 ||
            Selector[0] !=
                ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                    ExpectedSelector(Ordinal))
        {
            OutError = FString::Printf(
                TEXT("Tree geometry selector row %d drifted from the compiled exact roster."),
                Ordinal);
            return false;
        }
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
            TEXT("SelectorManifestSha256"),
            SelectorManifestSha256) ||
        !ExactInteger(Root, TEXT("ExactCandidateMeshCount"), CandidateCount) ||
        !ExactInteger(Root, TEXT("ExactSourceAnchorCount"), SourceAnchorCount) ||
        !ExactBool(Root, TEXT("UnnumberedSuccessor"), true) ||
        !ExactBool(Root, TEXT("ExplicitExecutionAuthorized"), true) ||
        !ExactBool(Root, TEXT("AssetsOnlyEndpoint"), true) ||
        !ExactBool(Root, TEXT("MapMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceMeshMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("SourceTransformMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("CollisionNavigationLosRfSensorTerrainMutationAuthorized"), false) ||
        !ExactBool(Root, TEXT("NativeWriteOrUnrealLaunchPerformed"), false))
    {
        OutError = TEXT("Future transaction authorization is absent, unpinned, non-explicit, or broader than asset-only materialization.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool BuildAdmission(
    const FString& CandidateContractPath,
    const FString& SelectorManifestPath,
    const FString& AcceptedR33ReceiptPath,
    const FString& ExpectedAcceptedR33ReceiptSha256,
    const FString& FutureTransactionAuthorizationPath,
    const FString& ExpectedFutureTransactionAuthorizationSha256,
    bool bRequireCompiledTrustAnchors,
    FAdmission& OutAdmission,
    FString& OutError)
{
    OutAdmission = FAdmission{};
    if (bRequireCompiledTrustAnchors)
    {
        if (!IsSha256(TrustedAcceptedR33ReceiptSha256) ||
            !IsSha256(TrustedFutureAuthorizationSha256) ||
            TrustedAcceptedR33ReceiptSha256.Equals(
                TrustedFutureAuthorizationSha256,
                ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Tree-geometry execution is intentionally unreachable: compiled trusted receipt anchors are unset and require a separately reviewed source change plus recompile.");
            return false;
        }
        if (!ExpectedAcceptedR33ReceiptSha256.Equals(
                TrustedAcceptedR33ReceiptSha256,
                ESearchCase::IgnoreCase) ||
            !ExpectedFutureTransactionAuthorizationSha256.Equals(
                TrustedFutureAuthorizationSha256,
                ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Caller-supplied tree-geometry receipt hashes do not match the separately compiled trusted anchors.");
            return false;
        }
    }
    TSharedPtr<FJsonObject> AcceptedR33;
    TSharedPtr<FJsonObject> FutureAuthorization;
    if (!LoadHashPinnedJson(
            CandidateContractPath,
            CandidateContractSha256,
            CandidateContractBytes,
            OutAdmission.CandidateContract,
            OutError) ||
        !ValidateCandidateContract(
            OutAdmission.CandidateContract,
            OutError) ||
        !LoadHashPinnedJson(
            SelectorManifestPath,
            SelectorManifestSha256,
            SelectorManifestBytes,
            OutAdmission.SelectorManifest,
            OutError) ||
        !ValidateSelectorManifest(
            OutAdmission.SelectorManifest,
            OutError) ||
        !LoadHashPinnedJson(
            AcceptedR33ReceiptPath,
            ExpectedAcceptedR33ReceiptSha256,
            -1,
            AcceptedR33,
            OutError) ||
        !ValidateAcceptedR33Receipt(AcceptedR33, OutError) ||
        !LoadHashPinnedJson(
            FutureTransactionAuthorizationPath,
            ExpectedFutureTransactionAuthorizationSha256,
            -1,
            FutureAuthorization,
            OutError) ||
        !ValidateFutureAuthorization(
            FutureAuthorization,
            ExpectedAcceptedR33ReceiptSha256,
            OutError))
    {
        return false;
    }
    OutAdmission.AcceptedR33Sha256 =
        ExpectedAcceptedR33ReceiptSha256.ToUpper();
    OutAdmission.FutureAuthorizationSha256 =
        ExpectedFutureTransactionAuthorizationSha256.ToUpper();
    OutError.Reset();
    return true;
}

double SmoothStep(double Low, double High, double Value)
{
    const double X = FMath::Clamp((Value - Low) / (High - Low), 0.0, 1.0);
    return X * X * (3.0 - 2.0 * X);
}

FVector3f WarpPosition(
    const FVector3f& Source,
    const FBox3f& ReferenceBounds,
    const FTRIADIstanaExploreV5DTreeGeometryVariationRecipe& Recipe)
{
    const double MinimumZ = ReferenceBounds.Min.Z;
    const double Height = ReferenceBounds.Max.Z - ReferenceBounds.Min.Z;
    const double U = FMath::Clamp(
        (static_cast<double>(Source.Z) - MinimumZ) / Height,
        0.0,
        1.0);
    if (U <= Recipe.RootHoldNormalizedHeight)
    {
        return Source;
    }
    const double Trunk = SmoothStep(
        Recipe.RootHoldNormalizedHeight,
        Recipe.CrownStartNormalizedHeight,
        U);
    const double Crown = SmoothStep(
        Recipe.CrownStartNormalizedHeight,
        Recipe.CrownFullNormalizedHeight,
        U);
    const FVector3f Center = ReferenceBounds.GetCenter();
    const double X = static_cast<double>(Source.X) - Center.X;
    const double Y = static_cast<double>(Source.Y) - Center.Y;
    const double Theta = FMath::DegreesToRadians(
        static_cast<double>(Recipe.TwistDegrees)) *
        (0.35 * Trunk + 0.65 * Crown);
    const double Cosine = FMath::Cos(Theta);
    const double Sine = FMath::Sin(Theta);
    const double RotatedX = Cosine * X - Sine * Y;
    const double RotatedY = Sine * X + Cosine * Y;
    const double ScaleX = 1.0 +
        Trunk * (Recipe.TrunkScaleXY.X - 1.0) +
        Crown * (Recipe.CrownScaleXY.X - Recipe.TrunkScaleXY.X);
    const double ScaleY = 1.0 +
        Trunk * (Recipe.TrunkScaleXY.Y - 1.0) +
        Crown * (Recipe.CrownScaleXY.Y - Recipe.TrunkScaleXY.Y);
    const double Angle = FMath::Atan2(RotatedY, RotatedX);
    const double Phase = FMath::DegreesToRadians(
        static_cast<double>(Recipe.RadialPhaseDegrees));
    const double Ripple = 1.0 + Crown * Recipe.RadialRippleFraction *
        FMath::Sin(
            static_cast<double>(Recipe.RadialLobes) * Angle + Phase +
            PI * 0.75 * U);
    return FVector3f(
        static_cast<float>(Center.X + RotatedX * ScaleX * Ripple +
            Height * Recipe.BendBySourceHeightXY.X * Trunk * Trunk),
        static_cast<float>(Center.Y + RotatedY * ScaleY * Ripple +
            Height * Recipe.BendBySourceHeightXY.Y * Trunk * Trunk),
        static_cast<float>(MinimumZ +
            (static_cast<double>(Source.Z) - MinimumZ) *
                (1.0 + Trunk * (Recipe.HeightScale - 1.0))));
}

bool ProtectedAttributesEqual(
    const FMeshDescription& Source,
    const FMeshDescription& Candidate,
    FString& OutError)
{
    if (Source.Vertices().Num() != Candidate.Vertices().Num() ||
        Source.Edges().Num() != Candidate.Edges().Num() ||
        Source.VertexInstances().Num() != Candidate.VertexInstances().Num() ||
        Source.Triangles().Num() != Candidate.Triangles().Num() ||
        Source.Polygons().Num() != Candidate.Polygons().Num() ||
        Source.PolygonGroups().Num() != Candidate.PolygonGroups().Num())
    {
        OutError = TEXT("Geometry variation changed topology element counts.");
        return false;
    }
    const FStaticMeshConstAttributes SourceAttributes(Source);
    const FStaticMeshConstAttributes CandidateAttributes(Candidate);
    const auto SourceUvs = SourceAttributes.GetVertexInstanceUVs();
    const auto CandidateUvs = CandidateAttributes.GetVertexInstanceUVs();
    const auto SourceColors = SourceAttributes.GetVertexInstanceColors();
    const auto CandidateColors = CandidateAttributes.GetVertexInstanceColors();
    if (!SourceUvs.IsValid() || !CandidateUvs.IsValid() ||
        SourceUvs.GetNumChannels() != CandidateUvs.GetNumChannels() ||
        !SourceColors.IsValid() || !CandidateColors.IsValid())
    {
        OutError = TEXT("Geometry variation lost UV or vertex-colour attributes.");
        return false;
    }
    for (const FVertexInstanceID Id :
         Source.VertexInstances().GetElementIDs())
    {
        if (!Candidate.IsVertexInstanceValid(Id) ||
            Source.GetVertexInstanceVertex(Id) !=
                Candidate.GetVertexInstanceVertex(Id) ||
            SourceColors.Get(Id) != CandidateColors.Get(Id))
        {
            OutError = TEXT("Geometry variation changed vertex-instance identity or colour.");
            return false;
        }
        for (int32 Channel = 0; Channel < SourceUvs.GetNumChannels(); ++Channel)
        {
            if (SourceUvs.Get(Id, Channel) != CandidateUvs.Get(Id, Channel))
            {
                OutError = TEXT("Geometry variation changed a UV coordinate.");
                return false;
            }
        }
    }
    for (const FTriangleID Id : Source.Triangles().GetElementIDs())
    {
        if (!Candidate.IsTriangleValid(Id) ||
            Source.GetTrianglePolygonGroup(Id) !=
                Candidate.GetTrianglePolygonGroup(Id))
        {
            OutError = TEXT("Geometry variation changed a triangle polygon group.");
            return false;
        }
        const TArrayView<const FVertexInstanceID> SourceVertices =
            Source.GetTriangleVertexInstances(Id);
        const TArrayView<const FVertexInstanceID> CandidateVertices =
            Candidate.GetTriangleVertexInstances(Id);
        if (SourceVertices.Num() != CandidateVertices.Num())
        {
            OutError = TEXT("Geometry variation changed triangle arity.");
            return false;
        }
        for (int32 Corner = 0; Corner < SourceVertices.Num(); ++Corner)
        {
            if (SourceVertices[Corner] != CandidateVertices[Corner])
            {
                OutError = TEXT("Geometry variation changed triangle topology.");
                return false;
            }
        }
    }
    const auto SourceGroups =
        SourceAttributes.GetPolygonGroupMaterialSlotNames();
    const auto CandidateGroups =
        CandidateAttributes.GetPolygonGroupMaterialSlotNames();
    for (const FPolygonGroupID Id : Source.PolygonGroups().GetElementIDs())
    {
        if (!Candidate.IsPolygonGroupValid(Id) ||
            SourceGroups.Get(Id) != CandidateGroups.Get(Id))
        {
            OutError = TEXT("Geometry variation changed polygon-group material semantics.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool WarpDescription(
    const FMeshDescription& Source,
    const FBox3f& ReferenceBounds,
    const FTRIADIstanaExploreV5DTreeGeometryVariationRecipe& Recipe,
    FMeshDescription& OutCandidate,
    FString& OutError)
{
    if (!ReferenceBounds.IsValid ||
        ReferenceBounds.Max.Z - ReferenceBounds.Min.Z <= UE_SMALL_NUMBER ||
        Source.NeedsCompact())
    {
        OutError = TEXT("Geometry variation requires compact Z-up source geometry with positive height.");
        return false;
    }
    OutCandidate = Source;
    FStaticMeshAttributes CandidateAttributes(OutCandidate);
    TVertexAttributesRef<FVector3f> Positions =
        CandidateAttributes.GetVertexPositions();
    bool bAnyVertexChanged = false;
    for (const FVertexID Id : OutCandidate.Vertices().GetElementIDs())
    {
        const FVector3f Before = Positions.Get(Id);
        const FVector3f After = WarpPosition(Before, ReferenceBounds, Recipe);
        if (After.ContainsNaN())
        {
            OutError = TEXT("Geometry variation produced a non-finite vertex.");
            return false;
        }
        Positions.Set(Id, After);
        bAnyVertexChanged = bAnyVertexChanged || Before != After;
    }
    if (!bAnyVertexChanged ||
        !ProtectedAttributesEqual(Source, OutCandidate, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Geometry variation recipe did not move any non-root vertex.");
        }
        return false;
    }
    FStaticMeshOperations::ComputeTriangleTangentsAndNormals(
        OutCandidate,
        0.0f);
    FStaticMeshOperations::ComputeTangentsAndNormals(
        OutCandidate,
        EComputeNTBsFlags::Normals | EComputeNTBsFlags::Tangents |
            EComputeNTBsFlags::UseMikkTSpace |
            EComputeNTBsFlags::IgnoreDegenerateTriangles);
    if (!ProtectedAttributesEqual(Source, OutCandidate, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool LoadBaseMeshes(TArray<UStaticMesh*>& OutMeshes, FString& OutError)
{
    OutMeshes.Reset();
    for (const TCHAR* Path : BaseMeshObjectPaths)
    {
        OutMeshes.Add(LoadExact<UStaticMesh>(Path));
    }
    if (OutMeshes.Num() != FormCount || OutMeshes.Contains(nullptr))
    {
        OutError = TEXT("The exact five TreeRealism response-material derivatives are absent.");
        return false;
    }
    for (int32 Form = 0; Form < FormCount; ++Form)
    {
        UStaticMesh* Mesh = OutMeshes[Form];
        if (Mesh->GetNumSourceModels() != ExpectedLodCounts[Form] ||
            Mesh->GetNumLODs() != ExpectedLodCounts[Form])
        {
            OutError = TEXT("A TreeRealism source mesh changed its exact source/render LOD count.");
            return false;
        }
        for (int32 Lod = 0; Lod < ExpectedLodCounts[Form]; ++Lod)
        {
            if (!Mesh->IsMeshDescriptionValid(Lod))
            {
                OutError = FString::Printf(
                    TEXT("TreeRealism source form %d LOD%d lacks a cloneable MeshDescription; materialization fails closed."),
                    Form,
                    Lod);
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateCandidateMesh(
    UStaticMesh* Candidate,
    UStaticMesh* Source,
    int32 RecipeIndex,
    FString& OutError)
{
    FTRIADIstanaExploreV5DTreeGeometryVariationRecipe Recipe;
    if (!Candidate || !Source || Candidate == Source ||
        !ATRIADIstanaExploreV5DTreeGeometryVariationActor::GetRecipe(
            RecipeIndex,
            Recipe) ||
        Candidate->GetPathName() !=
            ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                CandidateObjectPath(RecipeIndex) ||
        Candidate->GetNumSourceModels() != Source->GetNumSourceModels() ||
        Candidate->GetNumLODs() != Source->GetNumLODs() ||
        Candidate->GetStaticMaterials().Num() !=
            Source->GetStaticMaterials().Num())
    {
        OutError = TEXT("Geometry candidate changed source identity, LOD count, or material-slot count.");
        return false;
    }
    for (int32 Slot = 0; Slot < Source->GetStaticMaterials().Num(); ++Slot)
    {
        const FStaticMaterial& A = Candidate->GetStaticMaterials()[Slot];
        const FStaticMaterial& B = Source->GetStaticMaterials()[Slot];
        if (A.MaterialSlotName != B.MaterialSlotName ||
            A.ImportedMaterialSlotName != B.ImportedMaterialSlotName ||
            Candidate->GetMaterial(Slot) != Source->GetMaterial(Slot))
        {
            OutError = TEXT("Geometry candidate changed exact response-material slot binding.");
            return false;
        }
    }
    FMeshDescription Reference;
    if (!Source->CloneMeshDescription(0, Reference))
    {
        OutError = TEXT("Geometry candidate source LOD0 description is absent.");
        return false;
    }
    const FBox3f ReferenceBounds(Reference.ComputeBoundingBox());
    for (int32 Lod = 0; Lod < Source->GetNumSourceModels(); ++Lod)
    {
        FMeshDescription SourceDescription;
        FMeshDescription CandidateDescription;
        FMeshDescription ExpectedDescription;
        if (!Source->CloneMeshDescription(Lod, SourceDescription) ||
            !Candidate->CloneMeshDescription(Lod, CandidateDescription) ||
            !WarpDescription(
                SourceDescription,
                ReferenceBounds,
                Recipe,
                ExpectedDescription,
                OutError) ||
            !ProtectedAttributesEqual(
                SourceDescription,
                CandidateDescription,
                OutError))
        {
            return false;
        }
        const FStaticMeshConstAttributes ActualAttributes(
            CandidateDescription);
        const FStaticMeshConstAttributes ExpectedAttributes(
            ExpectedDescription);
        const auto ActualPositions = ActualAttributes.GetVertexPositions();
        const auto ExpectedPositions = ExpectedAttributes.GetVertexPositions();
        for (const FVertexID Id :
             ExpectedDescription.Vertices().GetElementIDs())
        {
            if (!CandidateDescription.IsVertexValid(Id) ||
                ActualPositions.Get(Id) != ExpectedPositions.Get(Id))
            {
                OutError = FString::Printf(
                    TEXT("Geometry candidate %d LOD%d vertex warp is not the compiled exact recipe."),
                    RecipeIndex,
                    Lod);
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool LoadAndValidateCandidates(
    const TArray<UStaticMesh*>& Sources,
    TArray<UStaticMesh*>& OutCandidates,
    FString& OutError)
{
    OutCandidates.Reset();
    FTRIADIstanaExploreV5DTreeGeometryVariationAssets Assets;
    for (int32 Index = 0; Index < CandidateCount; ++Index)
    {
        UStaticMesh* Candidate = LoadExact<UStaticMesh>(
            ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                CandidateObjectPath(Index));
        OutCandidates.Add(Candidate);
        Assets.VariantMeshes.Add(Candidate);
    }
    if (!ATRIADIstanaExploreV5DTreeGeometryVariationActor::
            ValidateAssetRoster(Assets, OutError))
    {
        return false;
    }
    for (int32 Index = 0; Index < CandidateCount; ++Index)
    {
        const int32 Form = Index / VariantsPerForm;
        if (!ValidateCandidateMesh(
                OutCandidates[Index],
                Sources[Form],
                Index,
                OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool CreateCandidate(
    UStaticMesh* Source,
    int32 RecipeIndex,
    UStaticMesh*& OutCandidate,
    FString& OutError)
{
    OutCandidate = nullptr;
    FTRIADIstanaExploreV5DTreeGeometryVariationRecipe Recipe;
    if (!Source ||
        !ATRIADIstanaExploreV5DTreeGeometryVariationActor::GetRecipe(
            RecipeIndex,
            Recipe))
    {
        OutError = TEXT("Geometry candidate creation received an invalid source or recipe.");
        return false;
    }
    FMeshDescription Reference;
    if (!Source->CloneMeshDescription(0, Reference))
    {
        OutError = TEXT("Geometry candidate source LOD0 description is absent.");
        return false;
    }
    const FBox3f ReferenceBounds(Reference.ComputeBoundingBox());
    const FString PackagePath =
        ATRIADIstanaExploreV5DTreeGeometryVariationActor::
            CandidatePackagePath(RecipeIndex);
    UPackage* Package = CreatePackage(*PackagePath);
    UStaticMesh* Candidate = Package
        ? Cast<UStaticMesh>(StaticDuplicateObject(
              Source,
              Package,
              FName(Recipe.AssetName),
              RF_Public | RF_Standalone | RF_Transactional))
        : nullptr;
    if (!Candidate || Candidate == Source ||
        Candidate->GetPathName() !=
            ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                CandidateObjectPath(RecipeIndex))
    {
        OutError = TEXT("Could not allocate an isolated geometry candidate mesh.");
        return false;
    }
    Candidate->Modify();
    for (int32 Lod = 0; Lod < Source->GetNumSourceModels(); ++Lod)
    {
        FMeshDescription SourceDescription;
        FMeshDescription WarpedDescription;
        if (!Source->CloneMeshDescription(Lod, SourceDescription) ||
            !WarpDescription(
                SourceDescription,
                ReferenceBounds,
                Recipe,
                WarpedDescription,
                OutError))
        {
            return false;
        }
        const int32 SourceVertices = SourceDescription.Vertices().Num();
        const int32 SourceTriangles = SourceDescription.Triangles().Num();
        FMeshDescription* Installed = Candidate->CreateMeshDescription(
            Lod,
            MoveTemp(WarpedDescription));
        if (!Installed || Installed->Vertices().Num() != SourceVertices ||
            Installed->Triangles().Num() != SourceTriangles)
        {
            OutError = TEXT("Could not install topology-identical warped source LOD geometry.");
            return false;
        }
        UStaticMesh::FCommitMeshDescriptionParams CommitParams;
        CommitParams.bMarkPackageDirty = false;
        CommitParams.bUseHashAsGuid = true;
        Candidate->CommitMeshDescription(Lod, CommitParams);
    }
    Candidate->SetStaticMaterials(Source->GetStaticMaterials());
    Candidate->GetSectionInfoMap().CopyFrom(Source->GetSectionInfoMap());
    Candidate->GetOriginalSectionInfoMap().CopyFrom(
        Source->GetOriginalSectionInfoMap());
    if (UMetaData* Metadata = Package->GetMetaData())
    {
        Metadata->SetValue(
            Candidate,
            TEXT("TRIAD_TreeGeometryVariationCandidateContractSha256"),
            *CandidateContractSha256);
        Metadata->SetValue(
            Candidate,
            TEXT("TRIAD_TreeGeometryVariationSelectorManifestSha256"),
            *SelectorManifestSha256);
        Metadata->SetValue(
            Candidate,
            TEXT("TRIAD_TreeGeometryVariationRecipeIndex"),
            *LexToString(RecipeIndex));
        Metadata->SetValue(
            Candidate,
            TEXT("TRIAD_TreeGeometryVariationSourceObjectPath"),
            *Source->GetPathName());
        Metadata->SetValue(
            Candidate,
            TEXT("TRIAD_TreeGeometryVariationAuthority"),
            TEXT("APPEARANCE_ONLY_NO_SIMULATION_AUTHORITY"));
    }
    Candidate->PostEditChange();
    Candidate->MarkPackageDirty();
    OutCandidate = Candidate;
    OutError.Reset();
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DTreeGeometryVariationEditorLibrary::
    InspectTreeGeometryVariationReceipts(
        const FString& CandidateContractPath,
        const FString& SelectorManifestPath,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            CandidateContractPath,
            SelectorManifestPath,
            AcceptedR33ReceiptPath,
            ExpectedAcceptedR33ReceiptSha256,
            FutureTransactionAuthorizationPath,
            ExpectedFutureTransactionAuthorizationSha256,
            false,
            Admission,
            Error))
    {
        OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_RECEIPT_INSPECTION_DENIED: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_RECEIPTS_INSPECTED_ONLY executionAuthorized=false callerSuppliedHashesNeverAuthorizeExecution=true privateMaterializerInvoked=false sourceOnlyRead=true acceptedR33ReceiptHashMatchedCallerPinAndHumanAcceptedFields=true futureExplicitTransactionHashMatchedCallerPinAndExactNarrowFields=true candidateContractHashPinned=true selectorManifestHashPinned=true assetsWritten=false mapModified=false UnrealLaunched=false nativeIntegrationClaimed=false nativeVisualAcceptanceProvided=false");
    return true;
}

bool UTRIADIstanaExploreV5DTreeGeometryVariationEditorLibrary::
    MaterializeTrustedPostR33CandidateMeshesInternal(
        const FString& CandidateContractPath,
        const FString& SelectorManifestPath,
        const FString& AcceptedR33ReceiptPath,
        const FString& ExpectedAcceptedR33ReceiptSha256,
        const FString& FutureTransactionAuthorizationPath,
        const FString& ExpectedFutureTransactionAuthorizationSha256,
        FString& OutReport)
{
    FAdmission Admission;
    FString Error;
    if (!BuildAdmission(
            CandidateContractPath,
            SelectorManifestPath,
            AcceptedR33ReceiptPath,
            ExpectedAcceptedR33ReceiptSha256,
            FutureTransactionAuthorizationPath,
            ExpectedFutureTransactionAuthorizationSha256,
            true,
            Admission,
            Error))
    {
        OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_MATERIALIZATION_DENIED: ") +
            Error;
        return false;
    }
    TArray<UStaticMesh*> Sources;
    if (!LoadBaseMeshes(Sources, Error))
    {
        OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_MATERIALIZATION_DENIED: ") +
            Error;
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    int32 ExistingCount = 0;
    for (int32 Index = 0; Index < CandidateCount; ++Index)
    {
        const FString PackagePath =
            ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                CandidatePackagePath(Index);
        if (FPackageName::DoesPackageExist(PackagePath) ||
            FindPackage(nullptr, *PackagePath))
        {
            ++ExistingCount;
        }
    }
    TArray<UStaticMesh*> Candidates;
    if (ExistingCount == CandidateCount)
    {
        if (!LoadAndValidateCandidates(Sources, Candidates, Error))
        {
            OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_EXISTING_ASSETS_INVALID: ") +
                Error;
            return false;
        }
        OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_MESHES_VALID existing=15 created=0 mapModified=false sourceMeshesModified=false sourceTransformsModified=false collisionNavigationLosRfSensorTerrainAuthorityModified=false nativeRuntimeIntegrationProven=false nativeVisualAcceptanceProvided=false");
        return true;
    }
    if (ExistingCount != 0)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_TREE_GEOMETRY_VARIATION_PARTIAL_NAMESPACE_DENIED existing=%d expectedEither=0_or_15"),
            ExistingCount);
        return false;
    }

    TArray<UObject*> AssetsToSave;
    for (int32 Index = 0; Index < CandidateCount; ++Index)
    {
        UStaticMesh* Candidate = nullptr;
        if (!CreateCandidate(
                Sources[Index / VariantsPerForm],
                Index,
                Candidate,
                Error))
        {
            OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_CREATE_FAILED_BEFORE_SAVE: ") +
                Error;
            return false;
        }
        FAssetRegistryModule::AssetCreated(Candidate);
        AssetsToSave.Add(Candidate);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!LoadAndValidateCandidates(Sources, Candidates, Error))
    {
        OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_VALIDATION_FAILED_BEFORE_SAVE: ") +
            Error;
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem || AssetsToSave.Num() != CandidateCount ||
        !AssetSubsystem->SaveLoadedAssets(AssetsToSave, false))
    {
        OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_EXACT_SAVE_FAILED");
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!LoadAndValidateCandidates(Sources, Candidates, Error))
    {
        OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_COLD_RELOAD_REQUIRED_OR_INVALID: ") +
            Error;
        return false;
    }
    OutReport = TEXT("ISTANA_TREE_GEOMETRY_VARIATION_MESHES_MATERIALIZED created=15 sourceForms=5 variantsPerForm=3 everyExplicitSourceLodWarped=true topologyPreserved=true uvPreserved=true vertexColoursPreserved=true polygonGroupsPreserved=true materialSlotSemanticsPreserved=true normalsTangentsRecomputed=true mapModified=false sourceMeshesModified=false sourceTransformsModified=false collisionNavigationLosRfSensorTerrainAuthorityModified=false nativeRuntimeIntegrationProven=false nativeVisualAcceptanceProvided=false futureMapTransactionAndCaptureStillRequired=true");
    return true;
}
