#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance.h"

namespace
{
const FVector ExpectedBoundsMin(-125000.0, -125000.0, -51.51655292);
const FVector ExpectedBoundsMax(125000.0, 125000.0, 201.68388265);
}

UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance::
    UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance()
{
    SetCanonicalContract();
}

void UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance::
    SetCanonicalContract()
{
    ContractVersion = TEXT("outer_ground_loading_fallback_v1");
    LockedSetSha256 =
        TEXT("023068051DBFC7E146EB07548ABD322BC42F7AED439A9717FAFD7829B8805054");
    ObjSha256 =
        TEXT("13761E4CB255EB94C392F540436DA163AC3A3ECAB877C1D6F8460E5962A84A8C");
    MtlSha256 =
        TEXT("DCFA8A56AF148E1D4E4FA4DC2E772D7A3E5767B0F91E07F25B3F87C2586A1A6A");
    AuditSha256 =
        TEXT("93F414E63E9221A5160B9533155E6535B72BE8B9ADDDC994D74FE50372E5B997");
    ManifestSha256 =
        TEXT("15E151887049F59EBF501DFCA921104845BCFF563EA876E1E11878CA343F0F8E");
    AcceptanceLockSha256 =
        TEXT("2AECE6314E29054054FEA445CFB07571A8CCE5DD95EE7A347F80E819F9EFDB4D");
    ContractSha256 =
        TEXT("1CDB81D0190E9FECA20058A542609749200F66B06B9A5A01103552D446404A86");
    GeneratorSha256 =
        TEXT("B582AA05D74DFB3B8F0857E8D8B6A520C3DC9F09A2537A632AA674729B891BE6");
    MaterialSlot = TEXT("M_IPV5D_OuterGroundLoadingFallback");
    ClaimStatus =
        TEXT("SYNTHETIC_EDGE_CONTINUATION_FOR_PROVIDER_LOADING_ONLY_NOT_TERRAIN_NOT_SURVEY_NOT_AS_BUILT");
    SourceCornerVertices = 3840;
    SourceTextureCoordinates = 3840;
    SourceNormals = 3840;
    Triangles = 1280;
    AngularSectors = 128;
    RadialBands = 5;
    InnerRadiusMetres = 1000.0;
    OuterRadiusMetres = 1250.0;
    BoundsMinCentimetres = ExpectedBoundsMin;
    BoundsMaxCentimetres = ExpectedBoundsMax;
    bExactInheritedInnerSeam = true;
    bPhysicalMetreUv0 = true;
    bIdentityImportScale = true;
    bOneLod = true;
    bNaniteFullMesh = true;
    bRasterFallbackFullMesh = true;
    bRenderOnlyProviderLoadingVisual = true;
    bCollisionAuthority = false;
    bNavigationAuthority = false;
    bShadowAuthority = false;
    bDistanceFieldAuthority = false;
    bTerrainSurveyOrAsBuiltAuthority = false;
    bSensorOcclusionOrRfAuthority = false;
    bLiveMapOrProviderPolicyIntegrationIncluded = false;
}

bool UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance::
    IsCanonicalContract() const
{
    return ContractVersion == TEXT("outer_ground_loading_fallback_v1") &&
        LockedSetSha256 ==
            TEXT("023068051DBFC7E146EB07548ABD322BC42F7AED439A9717FAFD7829B8805054") &&
        ObjSha256 ==
            TEXT("13761E4CB255EB94C392F540436DA163AC3A3ECAB877C1D6F8460E5962A84A8C") &&
        MtlSha256 ==
            TEXT("DCFA8A56AF148E1D4E4FA4DC2E772D7A3E5767B0F91E07F25B3F87C2586A1A6A") &&
        AuditSha256 ==
            TEXT("93F414E63E9221A5160B9533155E6535B72BE8B9ADDDC994D74FE50372E5B997") &&
        ManifestSha256 ==
            TEXT("15E151887049F59EBF501DFCA921104845BCFF563EA876E1E11878CA343F0F8E") &&
        AcceptanceLockSha256 ==
            TEXT("2AECE6314E29054054FEA445CFB07571A8CCE5DD95EE7A347F80E819F9EFDB4D") &&
        ContractSha256 ==
            TEXT("1CDB81D0190E9FECA20058A542609749200F66B06B9A5A01103552D446404A86") &&
        GeneratorSha256 ==
            TEXT("B582AA05D74DFB3B8F0857E8D8B6A520C3DC9F09A2537A632AA674729B891BE6") &&
        MaterialSlot == TEXT("M_IPV5D_OuterGroundLoadingFallback") &&
        ClaimStatus ==
            TEXT("SYNTHETIC_EDGE_CONTINUATION_FOR_PROVIDER_LOADING_ONLY_NOT_TERRAIN_NOT_SURVEY_NOT_AS_BUILT") &&
        SourceCornerVertices == 3840 &&
        SourceTextureCoordinates == 3840 && SourceNormals == 3840 &&
        Triangles == 1280 && AngularSectors == 128 && RadialBands == 5 &&
        FMath::IsNearlyEqual(InnerRadiusMetres, 1000.0) &&
        FMath::IsNearlyEqual(OuterRadiusMetres, 1250.0) &&
        BoundsMinCentimetres.Equals(ExpectedBoundsMin, 0.000001) &&
        BoundsMaxCentimetres.Equals(ExpectedBoundsMax, 0.000001) &&
        bExactInheritedInnerSeam && bPhysicalMetreUv0 &&
        bIdentityImportScale && bOneLod && bNaniteFullMesh &&
        bRasterFallbackFullMesh && bRenderOnlyProviderLoadingVisual &&
        !bCollisionAuthority && !bNavigationAuthority &&
        !bShadowAuthority && !bDistanceFieldAuthority &&
        !bTerrainSurveyOrAsBuiltAuthority &&
        !bSensorOcclusionOrRfAuthority &&
        !bLiveMapOrProviderPolicyIntegrationIncluded;
}
