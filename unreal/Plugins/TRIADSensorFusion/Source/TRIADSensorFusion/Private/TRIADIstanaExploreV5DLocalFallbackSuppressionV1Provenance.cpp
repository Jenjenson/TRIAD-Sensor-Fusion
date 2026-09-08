#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.h"

UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance()
{
    SetCanonicalContract();
}

void UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
    SetCanonicalContract()
{
    ContractVersion = TEXT("local_fallback_suppression_v1");
    SourceEpoch = TEXT("2026-08-31");
    CanonicalOutputSetSha256 =
        TEXT("27EF5679CA0010AB646C3DA542B1F0EC6EA163504845BD446FA5D578095AC4FD");
    CanonicalRenderObjSha256 =
        TEXT("1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3");
    FilteredRenderObjSha256 =
        TEXT("C4781C95EBE88387A57260BD2D8BBC4CD132BA38D26AE007F862BA81FD9F31E9");
    SuppressionMetadataSha256 =
        TEXT("154BE543F3E7398900F39658FDE41604ABE4642A11EFE097DF1C9C3F8BF1CC61");
    SuppressionManifestSha256 =
        TEXT("D8627FECAC184B9B658E8C36048544A026811D56E130336441792FA997A9B88D");
    SuppressionOutputSetSha256 =
        TEXT("DD61A0746D68899217E87470F3C07BA29FB37334DC11120833E15A59732AE56C");
    SuppressedSourceKeys = {
        TEXT("OSM:way:46521250"),
        TEXT("OSM:way:1551538490"),
    };
    CanonicalTriangles = 43544;
    RenderTriangles = 43492;
    SuppressedTriangles = 52;
    VisibleFeatures = 1387;
    VisiblePolygonParts = 1389;
    SourceVertexLines = 24522;
    SourceTextureCoordinateLines = 130632;
    ImportedVertices = 24492;
    ImportedVertexInstances = 130476;
    bPhysicalMetreUv0Validated = true;
    bNaniteFullMesh = true;
    bRasterFallbackFullMesh = true;
    bCanonicalRenderAndRfInputsHashPinnedUnchanged = true;
    bRenderOnly = true;
    bLocalFallbackOnly = true;
    bProviderOverlapResolved = false;
    bCollisionNavigationSensorRfAuthority = false;
    bMeasuredSurveyAsBuiltHyperreal = false;
}

bool UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance::
    IsCanonicalContract() const
{
    const TArray<FString> ExpectedKeys = {
        TEXT("OSM:way:46521250"),
        TEXT("OSM:way:1551538490"),
    };
    return ContractVersion == TEXT("local_fallback_suppression_v1") &&
        SourceEpoch == TEXT("2026-08-31") &&
        CanonicalOutputSetSha256 ==
            TEXT("27EF5679CA0010AB646C3DA542B1F0EC6EA163504845BD446FA5D578095AC4FD") &&
        CanonicalRenderObjSha256 ==
            TEXT("1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3") &&
        FilteredRenderObjSha256 ==
            TEXT("C4781C95EBE88387A57260BD2D8BBC4CD132BA38D26AE007F862BA81FD9F31E9") &&
        SuppressionMetadataSha256 ==
            TEXT("154BE543F3E7398900F39658FDE41604ABE4642A11EFE097DF1C9C3F8BF1CC61") &&
        SuppressionManifestSha256 ==
            TEXT("D8627FECAC184B9B658E8C36048544A026811D56E130336441792FA997A9B88D") &&
        SuppressionOutputSetSha256 ==
            TEXT("DD61A0746D68899217E87470F3C07BA29FB37334DC11120833E15A59732AE56C") &&
        SuppressedSourceKeys == ExpectedKeys &&
        CanonicalTriangles == 43544 && RenderTriangles == 43492 &&
        SuppressedTriangles == 52 && VisibleFeatures == 1387 &&
        VisiblePolygonParts == 1389 && SourceVertexLines == 24522 &&
        SourceTextureCoordinateLines == 130632 &&
        ImportedVertices == 24492 && ImportedVertexInstances == 130476 &&
        bPhysicalMetreUv0Validated && bNaniteFullMesh &&
        bRasterFallbackFullMesh &&
        bCanonicalRenderAndRfInputsHashPinnedUnchanged && bRenderOnly &&
        bLocalFallbackOnly &&
        !bProviderOverlapResolved &&
        !bCollisionNavigationSensorRfAuthority &&
        !bMeasuredSurveyAsBuiltHyperreal;
}
