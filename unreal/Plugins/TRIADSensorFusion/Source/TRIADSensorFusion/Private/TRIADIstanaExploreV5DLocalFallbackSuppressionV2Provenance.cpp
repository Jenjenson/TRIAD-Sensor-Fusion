#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.h"

UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance::
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance()
{
    SetCanonicalContract();
}

void UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance::
    SetCanonicalContract()
{
    ContractVersion = TEXT("local_fallback_suppression_v2");
    SourceEpoch = TEXT("2026-08-31");
    CanonicalOutputSetSha256 =
        TEXT("27EF5679CA0010AB646C3DA542B1F0EC6EA163504845BD446FA5D578095AC4FD");
    CanonicalRenderObjSha256 =
        TEXT("1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3");
    FilteredRenderObjSha256 =
        TEXT("99175681A1F307D02D8FD01E09850AD017B782BCD4A56043F64B0EA285703110");
    SuppressionMetadataSha256 =
        TEXT("31A32BCB8DAED756E0B8D90D0EE795A43B389BFB3148322A0FAC761A9BD73477");
    SuppressionManifestSha256 =
        TEXT("7D455FE8C1E057F2380BE4941026F511AE5D5A1817795F238496FCEAD77EDE04");
    SuppressionOutputSetSha256 =
        TEXT("FBE8F7D0C8BB935A3DFC2AE2953B9480902B4CCBFBE7765170FC09AF49081120");
    SuppressedSourceKeys = {
        TEXT("OSM:way:46521250"),
        TEXT("OSM:way:1551538490"),
        TEXT("OSM:way:429681826"),
    };
    CanonicalTriangles = 43544;
    RenderTriangles = 43448;
    SuppressedTriangles = 96;
    VisibleFeatures = 1386;
    VisiblePolygonParts = 1388;
    SourceVertexLines = 24522;
    SourceTextureCoordinateLines = 130632;
    ImportedVertices = 24468;
    ImportedVertexInstances = 130344;
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

bool UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance::
    IsCanonicalContract() const
{
    const TArray<FString> ExpectedKeys = {
        TEXT("OSM:way:46521250"),
        TEXT("OSM:way:1551538490"),
        TEXT("OSM:way:429681826"),
    };
    return ContractVersion == TEXT("local_fallback_suppression_v2") &&
        SourceEpoch == TEXT("2026-08-31") &&
        CanonicalOutputSetSha256 ==
            TEXT("27EF5679CA0010AB646C3DA542B1F0EC6EA163504845BD446FA5D578095AC4FD") &&
        CanonicalRenderObjSha256 ==
            TEXT("1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3") &&
        FilteredRenderObjSha256 ==
            TEXT("99175681A1F307D02D8FD01E09850AD017B782BCD4A56043F64B0EA285703110") &&
        SuppressionMetadataSha256 ==
            TEXT("31A32BCB8DAED756E0B8D90D0EE795A43B389BFB3148322A0FAC761A9BD73477") &&
        SuppressionManifestSha256 ==
            TEXT("7D455FE8C1E057F2380BE4941026F511AE5D5A1817795F238496FCEAD77EDE04") &&
        SuppressionOutputSetSha256 ==
            TEXT("FBE8F7D0C8BB935A3DFC2AE2953B9480902B4CCBFBE7765170FC09AF49081120") &&
        SuppressedSourceKeys == ExpectedKeys &&
        CanonicalTriangles == 43544 && RenderTriangles == 43448 &&
        SuppressedTriangles == 96 && VisibleFeatures == 1386 &&
        VisiblePolygonParts == 1388 && SourceVertexLines == 24522 &&
        SourceTextureCoordinateLines == 130632 &&
        ImportedVertices == 24468 && ImportedVertexInstances == 130344 &&
        bPhysicalMetreUv0Validated && bNaniteFullMesh &&
        bRasterFallbackFullMesh &&
        bCanonicalRenderAndRfInputsHashPinnedUnchanged && bRenderOnly &&
        bLocalFallbackOnly && !bProviderOverlapResolved &&
        !bCollisionNavigationSensorRfAuthority &&
        !bMeasuredSurveyAsBuiltHyperreal;
}
