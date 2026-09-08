#include "TRIADIstanaExploreV5DCurrentSurroundingsProvenance.h"

UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
    UTRIADIstanaExploreV5DCurrentSurroundingsProvenance()
{
    SetCanonicalContract();
}

void UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
    SetCanonicalContract()
{
    SourceEpoch = TEXT("2026-08-31");
    OutputSetSha256 =
        TEXT("27EF5679CA0010AB646C3DA542B1F0EC6EA163504845BD446FA5D578095AC4FD");
    RenderObjSha256 =
        TEXT("1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3");
    SelectedFeatures = 1389;
    PolygonParts = 1391;
    SourceVertices = 24522;
    SourceVertexInstances = 130632;
    Triangles = 43544;
    bPhysicalMetreUv0Validated = true;
    bRenderOnly = true;
    bCollisionNavigationSensorRfAuthority = false;
    bMeasuredSurveyAsBuiltHyperreal = false;
}

bool UTRIADIstanaExploreV5DCurrentSurroundingsProvenance::
    IsCanonicalContract() const
{
    return SourceEpoch == TEXT("2026-08-31") &&
        OutputSetSha256 ==
            TEXT("27EF5679CA0010AB646C3DA542B1F0EC6EA163504845BD446FA5D578095AC4FD") &&
        RenderObjSha256 ==
            TEXT("1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3") &&
        SelectedFeatures == 1389 && PolygonParts == 1391 &&
        SourceVertices == 24522 && SourceVertexInstances == 130632 &&
        Triangles == 43544 && bPhysicalMetreUv0Validated && bRenderOnly &&
        !bCollisionNavigationSensorRfAuthority &&
        !bMeasuredSurveyAsBuiltHyperreal;
}
