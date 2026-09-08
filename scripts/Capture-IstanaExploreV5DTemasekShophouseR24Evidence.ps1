#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,47}$')]
    [string] $RunToken,

    # Supply these explicitly from the sealed Phase-2 commit receipt. The
    # wrapper cross-checks them against that independently pinned receipt.
    [int64] $ExpectedMapBytes = 0,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedMapSha256 = '',

    [ValidateSet('TelemetryOnly', 'ProviderFallback', 'ProviderReady')]
    [string] $ProviderEvidenceMode = 'TelemetryOnly',

    [ValidateRange(6, 120)]
    [int] $TelemetryDwellSeconds = 12,

    # Temasek integration changes both DLLs, so live capture must use values
    # copied explicitly from the successful guarded native-build receipt.
    [int64] $ExpectedRuntimeDllBytes = 0,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedRuntimeDllSha256 = '',

    [int64] $ExpectedEditorDllBytes = 0,

    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedEditorDllSha256 = '',

    [switch] $StaticSelfCheck,

    [ValidateRange(60, 900)]
    [int] $EditorTimeoutSeconds = 600,

    [ValidateRange(60, 300)]
    [int] $PieTimeoutSeconds = 120,

    [ValidateRange(60, 3600)]
    [int] $ProviderTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $CaptureTimeoutSeconds = 120
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$editor =
    'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe'
$projectRoot = 'D:\triad\TRIAD'
$projectFile = 'D:\triad\TRIAD\TRIAD.uproject'
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile =
    'D:\triad\TRIAD\Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'
$runtimeDll =
    'D:\triad\TRIAD\Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'
$editorDll =
    'D:\triad\TRIAD\Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'
$suppressedFallbackMeshAsset =
    'D:\triad\TRIAD\Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset'
$outerGroundMeshAsset =
    'D:\triad\TRIAD\Content\TRIAD\IstanaPublicViewExploreV5D\OuterGroundLoadingFallback\SM_IPV5D_OuterGroundLoadingFallback_Render.uasset'
$outerGroundMaterialAsset =
    'D:\triad\TRIAD\Content\TRIAD\IstanaPublicViewExploreV5D\OuterGroundLoadingFallback\Materials\M_IPV5D_OuterGroundLoadingFallback.uasset'
$ddcRoot = 'D:\triad\TRIAD\Saved\DerivedDataCache'
$outputRoot = 'D:\triad\TRIAD\Saved\TRIAD\IstanaPreviews\ExploreV5D'
$logRoot = 'D:\triad\TRIAD\Saved\Logs'
$logFile = Join-Path $logRoot `
    "Codex_V5D_R24_TemasekShophouse_${RunToken}.log"
$evidenceManifestPath = Join-Path $outputRoot `
    "explore_v5d_r24_temasek_shophouse_evidence_${RunToken}.json"
$rcCallUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$v5dLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$temasekLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DTemasekShophouseEditorLibrary'
$levelEditor = '/Script/LevelEditor.Default__LevelEditorSubsystem'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'
$strictFallbackCaptureAcknowledgementMarker =
    'localFallbackVisible=true localFallbackHidden=false'

# Every live user-owned UE5.4 CAPSTONE editor is protected as an exact set.
# The helper may run beside that set, but it never sends RC/quit/containment
# operations to those processes and fails closed if any identity changes.
$protectedUE54Editor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedUE54Project = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$nativeUE55EngineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')

$expectedProjectBytes = 1298L
$expectedProjectSha256 =
    '42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3'
$phase2CommitReceiptPath =
    'D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DTemasekPhase2V1\phase2_r26_native_retry2_20260906T0608SGT\commit.json'
$expectedPhase2CommitReceiptBytes = 163086L
$expectedPhase2CommitReceiptSha256 =
    'F1006E1338DC74311749AA0E4C95940795EA807DFFF8E813E3C1D2593801E58A'
$expectedPhase2CommitSchema =
    'triad.istana_explore_v5d.temasek_phase2.native_transaction.v1.commit'
$expectedPhase2CommitRunToken =
    'phase2_r26_native_retry2_20260906T0608SGT'
$expectedCommittedMapBytes = 36335002L
$expectedCommittedMapSha256 =
    '0CDA45D7390A92885A911C1CD3404B0B59E889CE54A5879B8F1234197F73D498'
$expectedCommittedRuntimeDllBytes = 4672000L
$expectedCommittedRuntimeDllSha256 =
    '0B90C971AA385244FD20D2916BA87B7AE013C0C40F5B1153FB237E4CCF156191'
$expectedCommittedEditorDllBytes = 7725056L
$expectedCommittedEditorDllSha256 =
    '7F5D8596FD032DFAC335D7DC9AD2EC9DC7B72029144D36A40292CB8C19510F5A'
$expectedPlacementBytes = 4517L
$expectedPlacementSha256 =
    '6BEA0091D68FB0202A759733AB2838AA0A4405783D38EE1420A9E4CC090014A5'
$expectedPlacementSchema =
    'triad.istana_explore_v5d.r24_temasek_shophouse.unreal_placement.v1'
$expectedGeometrySha256 =
    'CC40E8C2C975D2A19124D74EAAC80B6C14C7121AD6BDDD616A740620737B05A7'
$expectedMaterialCatalogSha256 =
    '945404D4E6EC0E873FCDCBDA13977A67747961A3ADAAD413D4C1D20690EBE3FF'
$expectedRenderObjSha256 =
    'F5ED94C312A38A542D9C078F19F16FAF04CEA6ECAE5B4C2CF0AA7F3D651B9007'
$expectedPhase2SourceContractBytes = 10720L
$expectedPhase2SourceContractSha256 =
    'F304EE5D090D41F2331F293E6CD797A9B27D5B3B74DE44DCAA6A3CC2A66FE082'
$expectedPhase2ManifestBytes = 9131L
$expectedPhase2ManifestSha256 =
    'ED46D1B6367859A42BF1404FE1C0F5E457C16276777818B510FD8A8CA3BE8D87'
$expectedOutputSetSha256 =
    '0ACA9941B8C60766EA4647A991DF80944B4A746E06CC2A5680C2D167C0FA4A4D'
$expectedFoliageLayoutSchema =
    'triad.istana_explore_v5d.r24_temasek_shophouse.foliage_layout.v1'
$expectedFoliageOwner =
    'ATRIADIstanaExploreV5DLandmarkVegetationActor'
$expectedActorClass = 'ATRIADIstanaExploreV5DTemasekShophouseActor'
$expectedTriangleCount = 15760
$expectedSourceVertexCount = 9536
$expectedComponentCount = 828
$expectedMaterialCount = 15
$expectedPackageCount = 16
$expectedBakedFoliageCount = 0
$expectedFoliageTreeAnchorCount = 3

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$nativeTemasekSourceRoot =
    'D:\triad\TRIAD\SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse'
$placementPath = Join-Path $nativeTemasekSourceRoot `
    'Generated\temasek_shophouse_r24.unreal_placement.json'
$phase2SourceContractPath = Join-Path $nativeTemasekSourceRoot `
    'temasek_shophouse_r24.contract.json'
$phase2ManifestPath = Join-Path $nativeTemasekSourceRoot `
    'Generated\IstanaPublicViewV5DR24TemasekShophouse.manifest.json'
$temasekAssetRoot =
    'D:\triad\TRIAD\Content\TRIAD\IstanaPublicViewExploreV5D\Surroundings\R24TemasekShophouse'
$temasekAssetRelativeStems = @(
    'SM_IPV5D_R24_TemasekShophouse_Render',
    'Materials\M_TSH_Brass_PBR_R24',
    'Materials\M_TSH_CharcoalTrim_PBR_R24',
    'Materials\M_TSH_DarkWindowGlass_PBR_R24',
    'Materials\M_TSH_PaverDark_PBR_R24',
    'Materials\M_TSH_PaverLight_PBR_R24',
    'Materials\M_TSH_PinkGreyMosaic_PBR_R24',
    'Materials\M_TSH_PrecastConcrete_PBR_R24',
    'Materials\M_TSH_Rainwater_PBR_R24',
    'Materials\M_TSH_ReusedTimber_PBR_R24',
    'Materials\M_TSH_ShadowRecess_PBR_R24',
    'Materials\M_TSH_SoilMulch_PBR_R24',
    'Materials\M_TSH_SolarPanel_PBR_R24',
    'Materials\M_TSH_Terracotta_PBR_R24',
    'Materials\M_TSH_Timber_PBR_R24',
    'Materials\M_TSH_WhiteShanghaiPlaster_PBR_R24'
)
$removedLegacyFoliageRelativeStems = @(
    'Materials\M_TSH_Bark_PBR_R24',
    'Materials\M_TSH_LeafDeep_PBR_R24',
    'Materials\M_TSH_LeafLight_PBR_R24',
    'Materials\M_TSH_PollinatorBloom_PBR_R24'
)
$packageArtifactSuffixes = @('.uasset', '.uexp', '.ubulk', '.uptnl')

function Get-FileIdentity {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($fullPath)) {
        throw "Required regular file is absent: $fullPath"
    }
    $item = Get-Item -LiteralPath $fullPath -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Required regular file is a reparse point: $fullPath"
    }
    [pscustomobject] [ordered] @{
        Path = $fullPath
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = [string] (Get-FileHash -LiteralPath $fullPath `
            -Algorithm SHA256).Hash
    }
}

function Get-FileStateIdentity {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if ([IO.File]::Exists($fullPath)) {
        return Get-FileIdentity -Path $fullPath
    }
    [pscustomobject] [ordered] @{
        Path = $fullPath
        Present = $false
        Bytes = 0L
        Sha256 = 'ABSENT'
    }
}

function Assert-FilePin {
    param(
        [Parameter(Mandatory = $true)] $Pin,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    $actual = Get-FileStateIdentity -Path ([string] $Pin.Path)
    if ([bool] $actual.Present -ne [bool] $Pin.Present -or
        [int64] $actual.Bytes -ne [int64] $Pin.Bytes -or
        [string] $actual.Sha256 -cne [string] $Pin.Sha256) {
        throw "File identity changed at '$Checkpoint': $($Pin.Path) expected=$($Pin.Present)/$($Pin.Bytes)/$($Pin.Sha256) actual=$($actual.Present)/$($actual.Bytes)/$($actual.Sha256)"
    }
}

function Assert-FilePins {
    param(
        [Parameter(Mandatory = $true)] [object[]] $Pins,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    foreach ($pin in $Pins) {
        Assert-FilePin -Pin $pin -Checkpoint $Checkpoint
    }
}

function Assert-ExactPin {
    param(
        [Parameter(Mandatory = $true)] $Pin,
        [Parameter(Mandatory = $true)] [int64] $Bytes,
        [Parameter(Mandatory = $true)] [string] $Sha256,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    if ([int64] $Pin.Bytes -ne $Bytes -or
        [string] $Pin.Sha256 -cne $Sha256.ToUpperInvariant()) {
        throw "$Description identity mismatch: expected=$Bytes/$($Sha256.ToUpperInvariant()) actual=$($Pin.Bytes)/$($Pin.Sha256)"
    }
}

function Assert-ExactDoubleVector {
    param(
        [Parameter(Mandatory = $true)] [object[]] $Actual,
        [Parameter(Mandatory = $true)] [double[]] $Expected,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    if ($Actual.Count -ne $Expected.Count) {
        throw "$Description vector length changed: $($Actual.Count)/$($Expected.Count)"
    }
    for ($index = 0; $index -lt $Expected.Count; ++$index) {
        if ([double] $Actual[$index] -ne [double] $Expected[$index]) {
            throw "$Description vector changed at index $index`: $($Actual[$index])/$($Expected[$index])"
        }
    }
}

function Get-ValidatedTemasekPlacementContract {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $contract = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    if ([string] $contract.schema -cne $expectedPlacementSchema -or
        [string] $contract.status -cne
            'SOURCE_ONLY_PLACEMENT_RECEIPT_NOT_APPLIED_TO_UNREAL_MAP' -or
        [string] $contract.sourceAssetReceipt.geometrySha256 -cne
            $expectedGeometrySha256 -or
        [string] $contract.sourceAssetReceipt.materialCatalogSha256 -cne
            $expectedMaterialCatalogSha256 -or
        [string] $contract.sourceAssetReceipt.renderObjSha256 -cne
            $expectedRenderObjSha256 -or
        [bool] $contract.compositionPolicy.providerImageryUsed -or
        -not [bool] $contract.runtimeAuthorityPolicy.renderOnly -or
        [bool] $contract.runtimeAuthorityPolicy.surveyAuthority -or
        [bool] $contract.runtimeAuthorityPolicy.asBuiltAuthority -or
        [bool] $contract.runtimeAuthorityPolicy.collisionAuthority -or
        [bool] $contract.runtimeAuthorityPolicy.navigationAuthority -or
        [bool] $contract.runtimeAuthorityPolicy.sensorOcclusionAuthority -or
        [bool] $contract.runtimeAuthorityPolicy.rfGeometryAuthority -or
        [bool] $contract.runtimeAuthorityPolicy.rfMaterialAuthority) {
        throw 'The exact source-side Temasek placement/negative-authority contract changed.'
    }
    Assert-ExactDoubleVector `
        -Actual @($contract.unrealTransform.translationCentimeters) `
        -Expected @([double] 40411.657951, [double] 88424.311139, 0.0) `
        -Description 'Temasek translation-centimeters'
    Assert-ExactDoubleVector `
        -Actual @($contract.unrealTransform.rotationDegrees) `
        -Expected @(0.0, [double] -162.5152283523459, 0.0) `
        -Description 'Temasek rotation-degrees'
    Assert-ExactDoubleVector -Actual @($contract.unrealTransform.scale3D) `
        -Expected @([double] 1.053891, [double] 1.221655, 1.0) `
        -Description 'Temasek scale'
    $contract
}

function Assert-Phase2FoliageLayout {
    param(
        [Parameter(Mandatory = $true)] $Layout,
        [Parameter(Mandatory = $true)] [string] $Description
    )

    if ([string] $Layout.schema -cne $expectedFoliageLayoutSchema -or
        [string] $Layout.renderOwnerClass -cne $expectedFoliageOwner -or
        [int] $Layout.bakedRenderComponentCount -ne
            $expectedBakedFoliageCount -or
        @($Layout.treeAnchors).Count -ne $expectedFoliageTreeAnchorCount) {
        throw "$Description does not carry the exact Phase-2 foliage handoff."
    }
    $expectedAnchors = @(
        [pscustomobject] @{ Id='mature_tree_01'; Role='UMBRELLA'; Position=@(-18.5, -19.0, 0.0) },
        [pscustomobject] @{ Id='mature_tree_02'; Role='DOME'; Position=@(18.5, -18.8, 0.0) },
        [pscustomobject] @{ Id='mature_tree_03'; Role='HIGH_FORK'; Position=@(-13.2, -16.2, 0.0) }
    )
    for ($index = 0; $index -lt $expectedAnchors.Count; ++$index) {
        $actual = $Layout.treeAnchors[$index]
        $expected = $expectedAnchors[$index]
        if ([string] $actual.id -cne [string] $expected.Id -or
            [string] $actual.landmarkMeshRole -cne [string] $expected.Role) {
            throw "$Description tree anchor $index changed identity or role."
        }
        Assert-ExactDoubleVector -Actual @($actual.localPositionMeters) `
            -Expected ([double[]] $expected.Position) `
            -Description "$Description tree anchor $index"
    }
    foreach ($property in $Layout.bakedRenderCategoryCounts.PSObject.Properties) {
        if ([int] $property.Value -ne 0) {
            throw "$Description still contains baked foliage category '$($property.Name)'."
        }
    }
}

function Get-ValidatedPhase2SourceContract {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $contract = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -Depth 100
    if ([string] $contract.schema -cne
            'triad.istana_explore_v5d.r24_temasek_shophouse.contract.v2' -or
        [string] $contract.status -cne
            'SOURCE_PACKAGE_GENERATED_NOT_LIVE_UE_INTEGRATED') {
        throw 'The committed native Phase-2 source contract changed.'
    }
    Assert-Phase2FoliageLayout -Layout $contract.foliageLayout `
        -Description 'Phase-2 source contract'
    $contract
}

function Get-ValidatedPhase2Manifest {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $manifest = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -Depth 100
    if ([string] $manifest.outputSetSha256 -cne $expectedOutputSetSha256 -or
        [int] $manifest.counts.vertices -ne $expectedSourceVertexCount -or
        [int] $manifest.counts.triangles -ne $expectedTriangleCount -or
        [int] $manifest.counts.components -ne $expectedComponentCount -or
        [int] $manifest.counts.materials -ne $expectedMaterialCount) {
        throw 'The committed native Phase-2 manifest census changed.'
    }
    Assert-Phase2FoliageLayout -Layout $manifest.foliageLayout `
        -Description 'Phase-2 generated manifest'
    $manifest
}

function Get-ValidatedPhase2CommitReceipt {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $receipt = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -Depth 100
    if ([string] $receipt.Schema -cne $expectedPhase2CommitSchema -or
        [string] $receipt.Status -cne 'PASS' -or
        [string] $receipt.RunToken -cne $expectedPhase2CommitRunToken -or
        [int64] $receipt.SuccessorMap.Bytes -ne $expectedCommittedMapBytes -or
        [string] $receipt.SuccessorMap.Sha256 -cne $expectedCommittedMapSha256 -or
        [int64] $receipt.RuntimeDll.Bytes -ne
            $expectedCommittedRuntimeDllBytes -or
        [string] $receipt.RuntimeDll.Sha256 -cne
            $expectedCommittedRuntimeDllSha256 -or
        [int64] $receipt.EditorDll.Bytes -ne
            $expectedCommittedEditorDllBytes -or
        [string] $receipt.EditorDll.Sha256 -cne
            $expectedCommittedEditorDllSha256 -or
        [int] $receipt.Phase2AssetCount -ne $expectedPackageCount -or
        [int] $receipt.RemovedBakedFoliageMaterialCount -ne
            $removedLegacyFoliageRelativeStems.Count -or
        [int] $receipt.Phase2Census.SourceVertices -ne
            $expectedSourceVertexCount -or
        [int] $receipt.Phase2Census.Triangles -ne $expectedTriangleCount -or
        [int] $receipt.Phase2Census.Components -ne $expectedComponentCount -or
        [int] $receipt.Phase2Census.MaterialSlots -ne $expectedMaterialCount -or
        [int] $receipt.Phase2Census.BakedFoliageRenderComponents -ne
            $expectedBakedFoliageCount -or
        [int] $receipt.Phase2Census.FoliageTreeAnchors -ne
            $expectedFoliageTreeAnchorCount -or
        -not [bool] $receipt.IdempotentSecondApplyMapBytesAndHashUnchanged -or
        [bool] $receipt.MapBytesChanged) {
        throw 'The exact committed Phase-2 native transaction receipt changed.'
    }
    $coldStages = @($receipt.Stages | Where-Object {
            [string] $_.Stage -match '^0[1-7]_'
        })
    if ($coldStages.Count -ne 7 -or @($coldStages | Where-Object {
                [int] $_.ExitCode -ne 0 -or -not [bool] $_.FreshProcess -or
                -not [bool] $_.RenderingCapable -or
                [bool] $_.ForcedContainment
            }).Count -ne 0) {
        throw 'The Phase-2 receipt does not seal seven successful fresh cold stages.'
    }
    $receipt
}

function Convert-R24LocalMetersToWorldCentimeters {
    param([Parameter(Mandatory = $true)] [double[]] $LocalMeters)

    if ($LocalMeters.Count -ne 3) {
        throw 'R24 pose derivation requires exactly three local coordinates.'
    }
    $actorYawRadians = -162.5152283523459 * [Math]::PI / 180.0
    $localX = [double] $LocalMeters[0]
    $localY = [double] $LocalMeters[1]
    $localZ = [double] $LocalMeters[2]
    $scaledX = 100.0 * 1.053891 * $localX
    $scaledY = 100.0 * 1.221655 * $localY
    $scaledZ = 100.0 * $localZ
    $worldX = 40411.657951 +
        [Math]::Cos($actorYawRadians) * $scaledX -
        [Math]::Sin($actorYawRadians) * $scaledY
    $worldY = 88424.311139 +
        [Math]::Sin($actorYawRadians) * $scaledX +
        [Math]::Cos($actorYawRadians) * $scaledY
    [double[]] @($worldX, $worldY, $scaledZ)
}

function New-TargetLockedPose {
    param(
        [Parameter(Mandatory = $true)] [string] $Label,
        [Parameter(Mandatory = $true)] [double[]] $CameraLocalMeters,
        [Parameter(Mandatory = $true)] [double[]] $TargetLocalMeters,
        [Parameter(Mandatory = $true)] [string] $Rationale,
        [Parameter(Mandatory = $true)] [string] $OutputPrefix,
        [string] $AcceptanceSubject = 'TEMASEK_FACADE_AND_CONTEXT',
        [string] $FoliageAnchorId = '',
        [string] $FoliageMeshRole = '',
        [double[]] $FoliageAnchorLocalMeters = @(),
        [double] $MinimumTargetDistanceMeters = 0.0,
        [double] $MaximumTargetDistanceMeters = 1200.0
    )

    $camera = [double[]] @(
        Convert-R24LocalMetersToWorldCentimeters `
            -LocalMeters $CameraLocalMeters)
    $target = [double[]] @(
        Convert-R24LocalMetersToWorldCentimeters `
            -LocalMeters $TargetLocalMeters)
    $cameraX = [double] $camera[0]
    $cameraY = [double] $camera[1]
    $cameraZ = [double] $camera[2]
    $targetX = [double] $target[0]
    $targetY = [double] $target[1]
    $targetZ = [double] $target[2]
    $dx = $targetX - $cameraX
    $dy = $targetY - $cameraY
    $dz = $targetZ - $cameraZ
    $horizontal = [Math]::Sqrt($dx * $dx + $dy * $dy)
    $distance = [Math]::Sqrt($horizontal * $horizontal + $dz * $dz)
    $yaw = [Math]::Atan2($dy, $dx) * 180.0 / [Math]::PI
    $pitch = [Math]::Atan2($dz, $horizontal) * 180.0 / [Math]::PI
    if ([Math]::Sqrt($cameraX * $cameraX + $cameraY * $cameraY) -gt
            120000.0 -or
        $cameraZ -lt 50.0 -or $cameraZ -gt 50000.0) {
        throw "Derived pose '$Label' is outside the native QA endpoint envelope."
    }
    $distanceMeters = $distance / 100.0
    if ($distanceMeters -lt $MinimumTargetDistanceMeters -or
        $distanceMeters -gt $MaximumTargetDistanceMeters) {
        throw "Derived pose '$Label' target distance is outside its exact acceptance band: $distanceMeters m."
    }
    if (-not [string]::IsNullOrWhiteSpace($FoliageAnchorId)) {
        if ($FoliageAnchorLocalMeters.Count -ne 3 -or
            [double] $FoliageAnchorLocalMeters[0] -ne
                [double] $TargetLocalMeters[0] -or
            [double] $FoliageAnchorLocalMeters[1] -ne
                [double] $TargetLocalMeters[1]) {
            throw "Tree acceptance pose '$Label' is not locked to its exact foliage anchor XY."
        }
    }
    [pscustomobject] [ordered] @{
        Label = $Label
        X = $cameraX
        Y = $cameraY
        Z = $cameraZ
        Pitch = [double] $pitch
        Yaw = [double] $yaw
        Roll = 0.0
        CameraLocalMeters = [double[]] $CameraLocalMeters
        TargetLocalMeters = [double[]] $TargetLocalMeters
        TargetWorldCentimeters = [double[]] $target
        TargetDistanceCentimeters = [double] $distance
        TargetDistanceMeters = [double] $distanceMeters
        Rationale = $Rationale
        VisualAcceptance = [pscustomobject] [ordered] @{
            Schema =
                'triad.istana_explore_v5d.r24_temasek_shophouse.visual_acceptance.v2'
            Subject = $AcceptanceSubject
            FoliageAnchorId = $FoliageAnchorId
            FoliageMeshRole = $FoliageMeshRole
            FoliageAnchorLocalMeters = [double[]] $FoliageAnchorLocalMeters
            RequiredTargetDistanceBandMeters = [double[]] @(
                $MinimumTargetDistanceMeters,
                $MaximumTargetDistanceMeters)
            ComputedTargetDistanceMeters = [double] $distanceMeters
            RequiredManualChecks = if ([string]::IsNullOrWhiteSpace(
                    $FoliageAnchorId)) {
                @('framing_readable', 'facade_and_context_not_occluded')
            }
            else {
                @(
                    'landmark_tree_present_at_handoff_anchor',
                    'non_spherical_crown_silhouette_readable',
                    'trunk_and_crown_separation_readable',
                    'legacy_baked_foliage_absent',
                    'building_and_tree_spatial_relationship_readable'
                )
            }
            AutomatedPixelAcceptanceClaimed = $false
        }
        OutputFileName = "${OutputPrefix}_${Label}_${RunToken}.png"
    }
}

# The procedural source facade faces local -Y. The exact receipt transform
# below is a volunteered-OSM-oriented visual-fit assumption, not a survey. The
# Three retained cameras assess the full 40 m facade and five-foot-way detail,
# a facade/return/roof/frontage oblique, and the street-scale silhouette in
# provider context. A fourth, dedicated camera assesses the Phase-2 landmark
# tree handoff at approximately 19 m. None inspect or reconstruct provider
# geometry.
$captureFilenamePrefix = if ($ProviderEvidenceMode -cne 'ProviderReady') {
    'explore_v5d_diagnostic_r24_temasek_shophouse'
}
else {
    'explore_v5d_r24_temasek_shophouse'
}
$evidenceModeClassification =
    if ($ProviderEvidenceMode -ceq 'ProviderReady') {
        'PROOF_CANDIDATE_STRICT_GLOBAL_PROVIDER_READY_FAIL_CLOSED'
    }
    elseif ($ProviderEvidenceMode -ceq 'ProviderFallback') {
        'STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF'
    }
    else { 'EXPLICIT_NON_PROOF_TELEMETRY_DIAGNOSTIC' }
$facadeClosePose = New-TargetLockedPose -Label 'facade_close' `
    -CameraLocalMeters @(0.0, -40.0, 6.2) `
    -TargetLocalMeters @(0.0, -10.8, 7.2) `
    -Rationale 'Close full-facade reading of warm plaster, charcoal geometric trim, timber openings, projecting bays, ornament, deep five-foot way, columns, mosaic threshold, and planted frontage.' `
    -OutputPrefix $captureFilenamePrefix
$frontCornerObliquePose = New-TargetLockedPose `
    -Label 'front_corner_oblique' `
    -CameraLocalMeters @(39.0, -49.0, 10.5) `
    -TargetLocalMeters @(2.0, -7.5, 8.5) `
    -Rationale 'Front-corner oblique reading of facade depth, projecting bays, verandah, return wall, roof terrace and solar silhouette, arrival shelter, mature-tree planting, and procedural PBR transitions.' `
    -OutputPrefix $captureFilenamePrefix
$streetscapeContextPose = New-TargetLockedPose `
    -Label 'streetscape_context' `
    -CameraLocalMeters @(-72.0, -96.0, 31.0) `
    -TargetLocalMeters @(0.0, -3.0, 8.5) `
    -Rationale 'Wider public streetscape composition showing the three-storey silhouette and bounded frontage against visual-only provider surroundings while disclosing unresolved coarse-shell and provider overlap.' `
    -OutputPrefix $captureFilenamePrefix
$temasekTreeAcceptancePose = New-TargetLockedPose `
    -Label 'tree_acceptance_19m' `
    -CameraLocalMeters @(18.5, -34.2, 2.2) `
    -TargetLocalMeters @(18.5, -18.8, 3.9) `
    -Rationale 'Dedicated approximately 19 m acceptance view of the Phase-2 mature_tree_02 handoff anchor, requiring a readable non-spherical landmark crown and clear tree/building relationship without any baked legacy foliage.' `
    -OutputPrefix $captureFilenamePrefix `
    -AcceptanceSubject 'TEMASEK_LANDMARK_TREE_HANDOFF' `
    -FoliageAnchorId 'mature_tree_02' -FoliageMeshRole 'DOME' `
    -FoliageAnchorLocalMeters @(18.5, -18.8, 0.0) `
    -MinimumTargetDistanceMeters 18.5 -MaximumTargetDistanceMeters 19.5
$poses = @(
    $facadeClosePose,
    $frontCornerObliquePose,
    $streetscapeContextPose,
    $temasekTreeAcceptancePose
)

$temasekMapValidationMarkers = @(
    'cesiumGeoreference=(103.84288055,1.30709615,47.000)',
    'ionAssetId=2275207',
    'maximumSse=1.0',
    'applyDpiScaling=false',
    'forbidHoles=true',
    'loadingDescendantLimit=20',
    'authoredCoreClipShape=irregularEllipse64',
    'deterministicLocalSimulationLayersPreserved=true',
    'r24MacDonaldHouseCount=1',
    'r24MacDonaldDedicatedOverlayVisible=true',
    'r24TemasekShophouseCount=1',
    'r24TemasekDedicatedOverlayVisible=true',
    'landmarkVegetationCount=1',
    'landmarkVegetationMapIntegrated=true',
    'r24VisibilityInvariantAcrossProviderTransitions=true',
    'r24ProviderStateTelemetryOnly=true',
    'r24CoarseLocalLandmarkShellsSuppressed=true',
    'r24SuppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490',
    'r24ProviderAndDedicatedOverlayOverlapUnresolved=true',
    'r24DedicatedProviderExclusion=false',
    'r24ProviderReadyLiveSuccessor=false',
    'outerGroundLoadingFallbackProviderCoupled=true',
    'currentContextSuppressionContract=local_fallback_suppression_v2',
    'currentContextTriangles=43448',
    'currentContextSuppressedTriangles=96',
    'contextFacadeR25=true',
    'outerGroundLoadingFallbackTriangles=1280',
    'outerGroundLoadingFallbackSourceCorners=3840',
    'outerGroundLoadingFallbackRenderVertices=768',
    'outerGroundLoadingFallbackRenderOnly=true',
    'outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false',
    'inheritedV5CPlanningGroundHidden=true',
    'temasekShophouse={ISTANA_EXPLORE_V5D_R24_TEMASEK_VALID',
    'sourceFeature=OSM:way:1551538490',
    'placementReceiptSha256=6BEA0091D68FB0202A759733AB2838AA0A4405783D38EE1420A9E4CC090014A5',
    'transformCm=(40411.658,88424.311,0.000)',
    'yawDeg=-162.515228',
    'scale=(1.053891,1.221655,1.000000)',
    'meshTriangles=15760',
    'materialSlots=15',
    'proceduralTextureFreePbr=true',
    'materialsCalibrated=false',
    'foliageLayoutSchema=triad.istana_explore_v5d.r24_temasek_shophouse.foliage_layout.v1',
    'foliageOwner=ATRIADIstanaExploreV5DLandmarkVegetationActor',
    'bakedFoliageRenderComponents=0',
    'foliageTreeAnchors=3',
    'coarseShellOverlapUnresolved=true',
    'providerOverlapUnresolved=true',
    'renderOnly=true',
    'collisionNavigationSensorRfAuthority=false',
    'surveyAsBuilt=false'
)
$temasekAssetValidationMarkers = @(
    'ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSETS_VALID',
    'assets=16',
    'meshTriangles=15760',
    'sourceVertices=9536',
    'proceduralComponents=828',
    'materialSlots=15',
    'renderObjSha256=F5ED94C312A38A542D9C078F19F16FAF04CEA6ECAE5B4C2CF0AA7F3D651B9007',
    'outputSetSha256=0ACA9941B8C60766EA4647A991DF80944B4A746E06CC2A5680C2D167C0FA4A4D',
    'placementReceiptSha256=6BEA0091D68FB0202A759733AB2838AA0A4405783D38EE1420A9E4CC090014A5',
    'proceduralTextureFreePbr=true',
    'whitePlasterMicrovariation=true',
    'charcoalTrim=true',
    'opaqueRoughnessControlledGlass=true',
    'timberGrain=true',
    'brass=true',
    'mosaic=true',
    'precastConcrete=true',
    'terracotta=true',
    'solarPanels=true',
    'reclaimedPavers=true',
    'soilAndRainwater=true',
    'foliageLayoutSchema=triad.istana_explore_v5d.r24_temasek_shophouse.foliage_layout.v1',
    'foliageOwner=ATRIADIstanaExploreV5DLandmarkVegetationActor',
    'bakedFoliageRenderComponents=0',
    'foliageTreeAnchors=3',
    'materialCalibration=false',
    'zeroCollision=true',
    'noNavigation=true',
    'renderOnly=true',
    'surveyAsBuiltOneToOne=false',
    'sensorRfAuthority=false'
)

$selfPin = Get-FileIdentity -Path $PSCommandPath
$phase2CommitReceiptPin = Get-FileIdentity -Path $phase2CommitReceiptPath
Assert-ExactPin -Pin $phase2CommitReceiptPin `
    -Bytes $expectedPhase2CommitReceiptBytes `
    -Sha256 $expectedPhase2CommitReceiptSha256 `
    -Description 'sealed Phase-2 native transaction commit receipt'
$phase2CommitReceipt = Get-ValidatedPhase2CommitReceipt `
    -Path $phase2CommitReceiptPath
$placementPin = Get-FileIdentity -Path $placementPath
Assert-ExactPin -Pin $placementPin -Bytes $expectedPlacementBytes `
    -Sha256 $expectedPlacementSha256 `
    -Description 'committed native Phase-2 Temasek placement receipt'
$placementContract = Get-ValidatedTemasekPlacementContract -Path $placementPath
$phase2SourceContractPin = Get-FileIdentity -Path $phase2SourceContractPath
Assert-ExactPin -Pin $phase2SourceContractPin `
    -Bytes $expectedPhase2SourceContractBytes `
    -Sha256 $expectedPhase2SourceContractSha256 `
    -Description 'committed native Phase-2 source contract'
$phase2SourceContract = Get-ValidatedPhase2SourceContract `
    -Path $phase2SourceContractPath
$phase2ManifestPin = Get-FileIdentity -Path $phase2ManifestPath
Assert-ExactPin -Pin $phase2ManifestPin -Bytes $expectedPhase2ManifestBytes `
    -Sha256 $expectedPhase2ManifestSha256 `
    -Description 'committed native Phase-2 generated manifest'
$phase2Manifest = Get-ValidatedPhase2Manifest -Path $phase2ManifestPath
$uniqueAssetStems = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::OrdinalIgnoreCase)
foreach ($stem in $temasekAssetRelativeStems) {
    if (-not $uniqueAssetStems.Add($stem)) {
        throw "Duplicate Temasek native package stem in the exact roster: $stem"
    }
}
$materialStems = @($temasekAssetRelativeStems | Where-Object {
        $_.StartsWith('Materials\M_TSH_', [StringComparison]::Ordinal)
    })
if ($temasekAssetRelativeStems.Count -ne $expectedPackageCount -or
    $materialStems.Count -ne $expectedMaterialCount -or
    $temasekAssetRelativeStems[0] -cne
        'SM_IPV5D_R24_TemasekShophouse_Render') {
    throw 'Temasek Phase-2 package roster is not one mesh plus fifteen named materials.'
}
foreach ($removedStem in $removedLegacyFoliageRelativeStems) {
    if ($uniqueAssetStems.Contains($removedStem)) {
        throw "Removed legacy foliage stem remains in the Phase-2 roster: $removedStem"
    }
}

if ($StaticSelfCheck) {
    [pscustomobject] [ordered] @{
        Status = 'STATIC_SELF_CHECK_PASS'
        Schema =
            'triad.istana_explore_v5d.r24_temasek_shophouse.visual_capture_static_check.v1'
        Script = $selfPin
        PlacementReceipt = $placementPin
        Phase2CommitReceipt = $phase2CommitReceiptPin
        Phase2SourceContract = $phase2SourceContractPin
        Phase2Manifest = $phase2ManifestPin
        ProviderEvidenceMode = $ProviderEvidenceMode
        EvidenceClassification = $evidenceModeClassification
        TelemetryDwellSeconds = $TelemetryDwellSeconds
        PoseCount = $poses.Count
        Poses = $poses
        LandmarkContract = [pscustomobject] [ordered] @{
            ActorClass = $expectedActorClass
            RequiredActorCount = 1
            MeshTriangles = $expectedTriangleCount
            MaterialCount = $expectedMaterialCount
            PackageCount = $expectedPackageCount
            SourceVertices = $expectedSourceVertexCount
            ProceduralComponents = $expectedComponentCount
            BakedFoliageRenderComponents = $expectedBakedFoliageCount
            FoliageTreeAnchors = $expectedFoliageTreeAnchorCount
            FoliageOwner = $expectedFoliageOwner
            MeshPackageStem = $temasekAssetRelativeStems[0]
            MaterialPackageStems = $materialStems
            RemovedLegacyFoliagePackageStems =
                $removedLegacyFoliageRelativeStems
            MapValidationMarkers = $temasekMapValidationMarkers
            AssetValidationMarkers = $temasekAssetValidationMarkers
            PlacementContract = $placementContract
            SourceContract = $phase2SourceContract
            GeneratedManifest = $phase2Manifest
            NativeCommitReceipt = $phase2CommitReceipt
            BoundaryAssetPaths = @(
                $suppressedFallbackMeshAsset,
                $outerGroundMeshAsset,
                $outerGroundMaterialAsset
            )
            StrictFallbackCaptureAcknowledgementMarker =
                $strictFallbackCaptureAcknowledgementMarker
        }
        LiveEditorLaunched = $false
        NativeCommittedContractsRead = $true
        NativeTreeWritten = $false
        ProtectedUE54Interaction =
            'SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY'
        NativeUE55AndTRIADHelpersMustBeIdle = $true
        RemoteControlEndpointAllowlist = @($rcCallUri)
        ProviderGeometryExportedTracedAnalysedOrBaked = $false
    } | ConvertTo-Json -Depth 12
    return
}

if ($ExpectedMapBytes -le 0 -or
    $ExpectedMapSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
    throw 'Live R24 visual evidence requires -ExpectedMapBytes and -ExpectedMapSha256 from the successful native-transaction successor receipt.'
}
if ($ExpectedRuntimeDllBytes -le 0 -or $ExpectedEditorDllBytes -le 0 -or
    $ExpectedRuntimeDllSha256 -cnotmatch '^[0-9A-Fa-f]{64}$' -or
    $ExpectedEditorDllSha256 -cnotmatch '^[0-9A-Fa-f]{64}$') {
    throw 'Live R24 visual evidence requires positive byte counts and SHA-256 identities for both DLLs from the successful guarded native-build receipt.'
}
$ExpectedMapSha256 = $ExpectedMapSha256.ToUpperInvariant()
$ExpectedRuntimeDllSha256 = $ExpectedRuntimeDllSha256.ToUpperInvariant()
$ExpectedEditorDllSha256 = $ExpectedEditorDllSha256.ToUpperInvariant()
if ($ExpectedMapBytes -ne [int64] $phase2CommitReceipt.SuccessorMap.Bytes -or
    $ExpectedMapSha256 -cne [string] $phase2CommitReceipt.SuccessorMap.Sha256 -or
    $ExpectedRuntimeDllBytes -ne [int64] $phase2CommitReceipt.RuntimeDll.Bytes -or
    $ExpectedRuntimeDllSha256 -cne
        [string] $phase2CommitReceipt.RuntimeDll.Sha256 -or
    $ExpectedEditorDllBytes -ne [int64] $phase2CommitReceipt.EditorDll.Bytes -or
    $ExpectedEditorDllSha256 -cne
        [string] $phase2CommitReceipt.EditorDll.Sha256) {
    throw 'Caller-selected map/DLL pins do not exactly match the sealed Phase-2 commit receipt.'
}

function Test-ExactCommandLineToken {
    param(
        [Parameter(Mandatory = $true)] [string] $CommandLine,
        [Parameter(Mandatory = $true)] [string] $Token
    )

    $pattern = '(?i)(?:^|\s)"?' + [regex]::Escape($Token) + '"?(?:\s|$)'
    [regex]::Matches($CommandLine, $pattern).Count -eq 1
}

function Get-ProcessIdentity {
    param([Parameter(Mandatory = $true)] [uint32] $ProcessId)

    $row = Get-CimInstance Win32_Process -Filter "ProcessId = $ProcessId" `
        -ErrorAction SilentlyContinue
    if ($null -eq $row) {
        return $null
    }
    $creation = [DateTime] $row.CreationDate
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $row.ProcessId
        Name = [string] $row.Name
        ExecutablePath = [string] $row.ExecutablePath
        CommandLine = [string] $row.CommandLine
        CreationUtcTicks = [int64] $creation.ToUniversalTime().Ticks
        CreationTime = $creation.ToString('o')
    }
}

function Test-ProcessIdentityEqual {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] $Actual
    )

    $null -ne $Actual -and
        [uint32] $Actual.ProcessId -eq [uint32] $Expected.ProcessId -and
        [int64] $Actual.CreationUtcTicks -eq [int64] $Expected.CreationUtcTicks -and
        [string] $Actual.Name -ceq [string] $Expected.Name -and
        [string] $Actual.ExecutablePath -ceq [string] $Expected.ExecutablePath -and
        [string] $Actual.CommandLine -ceq [string] $Expected.CommandLine
}

function Get-ProtectedUE54Identity {
    $matches = @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') -and
                -not [string]::IsNullOrWhiteSpace([string] $_.CommandLine) -and
                (Test-ExactCommandLineToken `
                    -CommandLine ([string] $_.CommandLine) `
                    -Token $protectedUE54Project)
        })
    $identities = @($matches | ForEach-Object {
        $process = $_
        $actualExecutable = if ([string]::IsNullOrWhiteSpace(
                [string] $process.ExecutablePath)) {
            ''
        }
        else {
            [IO.Path]::GetFullPath([string] $process.ExecutablePath)
        }
        if ($process.Name -cne 'UnrealEditor.exe' -or
            $actualExecutable -ine $protectedUE54Editor) {
            throw 'The CAPSTONE project token is owned by an unexpected process; refusing the R24 capture run.'
        }
        [pscustomobject] [ordered] @{
            ProcessId = [uint32] $process.ProcessId
            CreationUtcTicks =
                [int64] ([DateTimeOffset] $process.CreationDate).UtcTicks
            Name = [string] $process.Name
            ExecutablePath = $actualExecutable
            ProjectPath = $protectedUE54Project
            CommandLine = [string] $process.CommandLine
        }
    } | Sort-Object ProcessId)
    [pscustomobject] [ordered] @{
        State = if ($identities.Count -eq 0) { 'ABSENT' } else { 'PRESENT' }
        SessionCount = $identities.Count
        Sessions = @($identities)
    }
}

function Assert-ProtectedUE54Unchanged {
    param([Parameter(Mandatory = $true)] $Before)

    $after = Get-ProtectedUE54Identity
    $beforeCanonical = $Before | ConvertTo-Json -Compress -Depth 8
    $afterCanonical = $after | ConvertTo-Json -Compress -Depth 8
    if ($afterCanonical -cne $beforeCanonical) {
        throw 'Protected UE5.4/CAPSTONE identity set changed during the R24 capture run.'
    }
    $after
}

function Get-NativeTRIADUnrealProcesses {
    $enginePrefix = $nativeUE55EngineRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            if ($_.Name -notin @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe')) {
                return $false
            }
            $actualExecutable = if ([string]::IsNullOrWhiteSpace(
                    [string] $_.ExecutablePath)) {
                ''
            }
            else {
                [IO.Path]::GetFullPath([string] $_.ExecutablePath)
            }
            $isUE55 = $actualExecutable.StartsWith(
                $enginePrefix, [StringComparison]::OrdinalIgnoreCase)
            $isNativeTRIAD = -not [string]::IsNullOrWhiteSpace(
                    [string] $_.CommandLine) -and
                (Test-ExactCommandLineToken `
                    -CommandLine ([string] $_.CommandLine) `
                    -Token $projectFile)
            $isUE55 -or $isNativeTRIAD
        }
    )
}

function Get-RcListeners {
    $rows = & "$env:SystemRoot\System32\netstat.exe" -ano -p TCP
    if ($LASTEXITCODE -ne 0) {
        throw 'netstat failed while checking Remote Control ownership.'
    }
    @(
        foreach ($line in $rows) {
            if ($line -match
                '^\s*TCP\s+\S+:30010\s+\S+\s+LISTENING\s+(\d+)\s*$') {
                [uint32] $Matches[1]
            }
        }
    )
}

function Assert-LaunchedProcessIdentity {
    param([Parameter(Mandatory = $true)] $Expected)

    $actual = Get-ProcessIdentity -ProcessId ([uint32] $Expected.ProcessId)
    if (-not (Test-ProcessIdentityEqual -Expected $Expected -Actual $actual)) {
        throw "Owned UE5.5 process changed identity or disappeared: PID=$($Expected.ProcessId)"
    }
    $actual
}

function Test-ExpectedHelperLaunchIdentity {
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] $Identity,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle
    )

    if ($null -eq $Identity -or $Identity.Name -cne 'UnrealEditor.exe' -or
        [string]::IsNullOrWhiteSpace($Identity.ExecutablePath) -or
        [string]::IsNullOrWhiteSpace($Identity.CommandLine) -or
        [IO.Path]::GetFullPath($Identity.ExecutablePath) -ine
            [IO.Path]::GetFullPath($editor)) {
        return $false
    }
    $projectPattern =
        '(?i)(?:^|\s)(?:"[^"]+\.uproject"|[^\s"]+\.uproject)(?:\s|$)'
    [regex]::Matches($Identity.CommandLine, $projectPattern).Count -eq 1 -and
        (Test-ExactCommandLineToken $Identity.CommandLine $projectFile) -and
        (Test-ExactCommandLineToken $Identity.CommandLine $mapPackage) -and
        $Identity.CommandLine.Contains('-NoAutoSave',
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains('-RemoteControlHttpServer',
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains('-RCWebControlEnable',
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains('WebControl.StartServer',
            [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains($logFile,
            [StringComparison]::OrdinalIgnoreCase) -and
        [Math]::Abs($Identity.CreationUtcTicks -
            $Handle.StartTime.ToUniversalTime().Ticks) -le
                [TimeSpan]::FromSeconds(2).Ticks
}

function Test-RcOwnership {
    param([Parameter(Mandatory = $true)] [uint32] $ProcessId)

    $listeners = @(Get-RcListeners)
    $listeners.Count -eq 1 -and [uint32] $listeners[0] -eq $ProcessId
}

function Assert-OwnedBoundary {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
        Out-Null
    Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
    $others = @(Get-NativeTRIADUnrealProcesses)
    if ($others.Count -ne 1 -or
        [uint32] $others[0].ProcessId -ne
            [uint32] $ExpectedProcess.ProcessId) {
        throw "The owned helper is not the sole non-protected Unreal editor at '$Checkpoint'."
    }
    if (-not (Test-RcOwnership -ProcessId `
            ([uint32] $ExpectedProcess.ProcessId))) {
        throw "RC port 30010 is not solely owned by the helper at '$Checkpoint'."
    }
}

function Invoke-RcCall {
    param(
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    $payload = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 12 -Compress
    Invoke-RestMethod -Method Put -Uri $rcCallUri `
        -ContentType 'application/json' -Body $payload -TimeoutSec $TimeoutSec
}

function Invoke-OwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint "before RC $FunctionName"
    $result = Invoke-RcCall -ObjectPath $ObjectPath `
        -FunctionName $FunctionName -Parameters $Parameters `
        -TimeoutSec $TimeoutSec
    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint "after RC $FunctionName"
    $result
}

function Invoke-RequiredOwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [string] $TextProperty = '',
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    $result = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
        -ObjectPath $ObjectPath -FunctionName $FunctionName `
        -Parameters $Parameters -TimeoutSec $TimeoutSec
    if ($result.ReturnValue -ne $true) {
        $detail = if ([string]::IsNullOrWhiteSpace($TextProperty)) {
            ''
        }
        else {
            [string] $result.$TextProperty
        }
        throw "Required RC call failed: $FunctionName $detail"
    }
    if (-not [string]::IsNullOrWhiteSpace($TextProperty) -and
        [string]::IsNullOrWhiteSpace([string] $result.$TextProperty)) {
        throw "Required RC call returned blank $TextProperty`: $FunctionName"
    }
    $result
}

function Wait-OwnedPieState {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [bool] $Expected,
        [ValidateRange(5, 600)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastError = $null
    do {
        Start-Sleep -Milliseconds 500
        try {
            $state = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
                -ObjectPath $levelEditor -FunctionName 'IsInPlayInEditor' `
                -TimeoutSec 30
            if ([bool] $state.ReturnValue -eq $Expected) {
                return
            }
        }
        catch {
            $lastError = $_.Exception
            Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
                -Checkpoint 'PIE-state retry after RC error'
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    $suffix = if ($null -eq $lastError) {
        ''
    }
    else {
        " Last error: $($lastError.Message)"
    }
    throw "PIE did not reach IsInPlayInEditor=$Expected.$suffix"
}

function Convert-InvariantDouble {
    param([Parameter(Mandatory = $true)] [string] $Text)

    [double]::Parse($Text, [Globalization.NumberStyles]::Float,
        [Globalization.CultureInfo]::InvariantCulture)
}

function Get-AngularDistanceDegrees {
    param(
        [Parameter(Mandatory = $true)] [double] $A,
        [Parameter(Mandatory = $true)] [double] $B
    )

    [Math]::Abs((($A - $B + 540.0) % 360.0) - 180.0)
}

function Test-ExactPoseState {
    param(
        [Parameter(Mandatory = $true)] $Response,
        [Parameter(Mandatory = $true)] $Pose,
        [Parameter(Mandatory = $true)]
        [ValidateSet('TelemetryOnly', 'ProviderFallback', 'ProviderReady')]
        [string] $EvidenceMode
    )

    if ($Response.ReturnValue -ne $true) {
        return $false
    }
    $report = [string] $Response.OutReport
    $markers = @(
            "map=$mapPackage",
            'providerSiteClipActive=true',
            'authoredCoreVisualsVisible=true',
            'aerialProviderHandoffRequested=false',
            'aerialProviderHandoffActive=false',
            'stableVisualPolicy=true',
            'viewTargetMatchesPawn=true',
            'v5CameraProfile=true',
            'exactQaViewPose=true')
    if ($EvidenceMode -ceq 'ProviderReady') {
        $markers += @('providerReadyForProof=true', 'localFallbackHidden=true')
    }
    elseif ($EvidenceMode -ceq 'ProviderFallback') {
        $markers += @(
            'providerReadyForProof=false',
            'localFallbackHidden=false'
        )
    }
    foreach ($marker in $markers) {
        if (-not $report.Contains($marker, [StringComparison]::Ordinal)) {
            return $false
        }
    }
    $progressMatch = [regex]::Match($report,
        'cesiumLoadProgress=(?<Progress>[0-9]+(?:\.[0-9]+)?)')
    if (-not $progressMatch.Success) {
        return $false
    }
    $progress = Convert-InvariantDouble $progressMatch.Groups['Progress'].Value
    if ([double]::IsNaN($progress) -or [double]::IsInfinity($progress) -or
        $progress -lt 0.0 -or $progress -gt 100.0) {
        return $false
    }
    $number = '[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?'
    $pattern = 'viewLocationCm=X=(?<X>' + $number +
        ')\s+Y=(?<Y>' + $number + ')\s+Z=(?<Z>' + $number +
        ')\s+viewRotationDeg=P=(?<Pitch>' + $number +
        ')\s+Y=(?<Yaw>' + $number + ')\s+R=(?<Roll>' + $number +
        ')\s+exactQaViewPose=true'
    $match = [regex]::Match($report, $pattern,
        [Text.RegularExpressions.RegexOptions]::CultureInvariant)
    if (-not $match.Success) {
        return $false
    }
    $x = Convert-InvariantDouble $match.Groups['X'].Value
    $y = Convert-InvariantDouble $match.Groups['Y'].Value
    $z = Convert-InvariantDouble $match.Groups['Z'].Value
    $pitch = Convert-InvariantDouble $match.Groups['Pitch'].Value
    $yaw = Convert-InvariantDouble $match.Groups['Yaw'].Value
    $roll = Convert-InvariantDouble $match.Groups['Roll'].Value
    [Math]::Abs($x - [double] $Pose.X) -le 0.002 -and
        [Math]::Abs($y - [double] $Pose.Y) -le 0.002 -and
        [Math]::Abs($z - [double] $Pose.Z) -le 0.002 -and
        (Get-AngularDistanceDegrees $pitch ([double] $Pose.Pitch)) -le
            0.002 -and
        (Get-AngularDistanceDegrees $yaw ([double] $Pose.Yaw)) -le
            0.002 -and
        (Get-AngularDistanceDegrees $roll ([double] $Pose.Roll)) -le
            0.002
}

function Get-ProviderTelemetrySample {
    param([Parameter(Mandatory = $true)] $Response)

    $report = [string] $Response.OutReport
    $progressMatch = [regex]::Match($report,
        'cesiumLoadProgress=(?<Progress>[0-9]+(?:\.[0-9]+)?)')
    $readyMatch = [regex]::Match($report,
        'providerReadyForProof=(?<Value>true|false)')
    $fallbackMatch = [regex]::Match($report,
        'localFallbackHidden=(?<Value>true|false)')
    if (-not $progressMatch.Success -or -not $readyMatch.Success -or
        -not $fallbackMatch.Success) {
        throw "Provider telemetry fields are absent from V5D state: $report"
    }
    [pscustomobject] [ordered] @{
        ObservedUtc = [DateTime]::UtcNow.ToString('o')
        CesiumLoadProgress = Convert-InvariantDouble `
            $progressMatch.Groups['Progress'].Value
        ProviderReadyForProof =
            $readyMatch.Groups['Value'].Value -ceq 'true'
        LocalFallbackHidden =
            $fallbackMatch.Groups['Value'].Value -ceq 'true'
        Report = $report
    }
}

function Wait-StableExactPoseState {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] $Pose,
        [ValidateRange(30, 3600)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $requireReady = $ProviderEvidenceMode -ceq 'ProviderReady'
    $requireFallback = $ProviderEvidenceMode -ceq 'ProviderFallback'
    $stable = 0
    $reports = [Collections.Generic.List[string]]::new()
    $samples = [Collections.Generic.List[object]]::new()
    $firstAcceptedUtc = $null
    $lastReport = ''
    $lastProgress = ''
    $complete = $false
    do {
        Start-Sleep -Seconds 2
        $state = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
            -ObjectPath $v5dLibrary `
            -FunctionName 'GetIstanaExploreV5DHybridPlayStateReport' `
            -TimeoutSec 120
        $lastReport = [string] $state.OutReport
        $accepted = Test-ExactPoseState -Response $state -Pose $Pose `
            -EvidenceMode $ProviderEvidenceMode
        if ($accepted) {
            $sample = Get-ProviderTelemetrySample -Response $state
            $sampleUtc = [DateTime]::Parse(
                [string] $sample.ObservedUtc,
                [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]::RoundtripKind)
            if ($null -eq $firstAcceptedUtc) {
                $firstAcceptedUtc = $sampleUtc
            }
            ++$stable
            $reports.Add($lastReport)
            $samples.Add($sample)
            $progress = [string] $sample.CesiumLoadProgress
            if ($requireReady) {
                $complete = $stable -ge 3
            }
            else {
                $complete = $stable -ge 3 -and
                    ($sampleUtc - $firstAcceptedUtc).TotalSeconds -ge
                        $TelemetryDwellSeconds
            }
        }
        else {
            $stable = 0
            $reports.Clear()
            $samples.Clear()
            $firstAcceptedUtc = $null
            $progressMatch = [regex]::Match($lastReport,
                'cesiumLoadProgress=(?<Progress>[0-9]+(?:\.[0-9]+)?)')
            $progress = if ($progressMatch.Success) {
                $progressMatch.Groups['Progress'].Value
            }
            else { '?' }
        }
        if ($progress -cne $lastProgress -or $accepted) {
            Write-Host "POSE=$($Pose.Label) MODE=$ProviderEvidenceMode PROVIDER_PROGRESS=$progress EXACT_STATE=$accepted STABLE=$stable COMPLETE=$complete"
            $lastProgress = $progress
        }
    } while (-not $complete -and [DateTime]::UtcNow -lt $deadline)
    if (-not $complete) {
        $expectation = if ($requireReady) {
            'three consecutive exact provider-ready readbacks'
        }
        elseif ($requireFallback) {
            "$TelemetryDwellSeconds seconds of consecutive exact-pose providerReadyForProof=false and localFallbackHidden=false readbacks"
        }
        else {
            "$TelemetryDwellSeconds seconds of consecutive exact-pose finite provider telemetry"
        }
        throw "Pose $($Pose.Label) did not reach $expectation`: $lastReport"
    }
    [pscustomobject] [ordered] @{
        Mode = $ProviderEvidenceMode
        ProviderReadyRequired = $requireReady
        ProviderFallbackRequired = $requireFallback
        TelemetryDwellSeconds = if ($requireReady) { 0 } else {
            $TelemetryDwellSeconds
        }
        Reports = @($reports)
        ProviderTelemetrySamples = @($samples)
        FinalReport = $lastReport
    }
}

function Get-PngEvidence {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [DateTime] $NotBeforeUtc
    )

    $pin = Get-FileIdentity -Path $Path
    $item = Get-Item -LiteralPath $pin.Path
    if ($item.LastWriteTimeUtc -lt $NotBeforeUtc) {
        throw "Capture is stale: $Path"
    }
    Add-Type -AssemblyName System.Drawing
    $stream = [IO.File]::Open($pin.Path, [IO.FileMode]::Open,
        [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $image = [Drawing.Image]::FromStream($stream, $true, $true)
        try {
            if ($image.RawFormat.Guid -ne
                    [Drawing.Imaging.ImageFormat]::Png.Guid -or
                $image.Width -ne 2560 -or $image.Height -ne 1440) {
                throw "Capture is not the required decodable 2560x1440 PNG: $Path"
            }
            [pscustomobject] [ordered] @{
                Path = $pin.Path
                Present = $true
                Bytes = $pin.Bytes
                Sha256 = $pin.Sha256
                Width = $image.Width
                Height = $image.Height
                LastWriteTimeUtc = $item.LastWriteTimeUtc.ToString('o')
            }
        }
        finally {
            $image.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Wait-StablePng {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [DateTime] $NotBeforeUtc,
        [ValidateRange(30, 300)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastLength = -1L
    $lastWriteTicks = -1L
    $stable = 0
    $lastError = $null
    do {
        Start-Sleep -Milliseconds 500
        if ([IO.File]::Exists([IO.Path]::GetFullPath($Path))) {
            $item = Get-Item -LiteralPath $Path
            if ($item.Length -gt 0 -and $item.Length -eq $lastLength -and
                $item.LastWriteTimeUtc.Ticks -eq $lastWriteTicks) {
                ++$stable
            }
            else {
                $stable = 0
            }
            $lastLength = [int64] $item.Length
            $lastWriteTicks = [int64] $item.LastWriteTimeUtc.Ticks
            if ($stable -ge 3) {
                try {
                    return Get-PngEvidence -Path $Path `
                        -NotBeforeUtc $NotBeforeUtc
                }
                catch {
                    $lastError = $_.Exception
                    $stable = 0
                }
            }
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    $suffix = if ($null -eq $lastError) { '' } else {
        " Last decode error: $($lastError.Message)"
    }
    throw "Capture did not become a stable decodable PNG: $Path.$suffix"
}

function Assert-PostCaptureWorld {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] $Pose
    )

    $state = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
        -ObjectPath $v5dLibrary `
        -FunctionName 'GetIstanaExploreV5DHybridPlayStateReport' `
        -TimeoutSec 120
    if (-not (Test-ExactPoseState -Response $state -Pose $Pose `
            -EvidenceMode $ProviderEvidenceMode)) {
        throw "Post-capture state lost the exact $ProviderEvidenceMode pose contract for $($Pose.Label): $($state.OutReport)"
    }
    $providerTelemetry = Get-ProviderTelemetrySample -Response $state
    $validation = Invoke-RequiredOwnedRcCall `
        -ExpectedProcess $ExpectedProcess -ObjectPath $v5dLibrary `
        -FunctionName 'ValidateIstanaExploreV5DHybridPlayWorld' `
        -TextProperty 'OutReport' -TimeoutSec 180
    if (-not ([string] $validation.OutReport).StartsWith(
            'ISTANA_EXPLORE_V5D_HYBRID_PIE_VALID',
            [StringComparison]::Ordinal)) {
        throw "Unexpected post-capture V5D validation for $($Pose.Label): $($validation.OutReport)"
    }
    [pscustomobject] [ordered] @{
        StateReport = [string] $state.OutReport
        ProviderTelemetry = $providerTelemetry
        ValidationReport = [string] $validation.OutReport
    }
}

function Wait-ExactProcessExit {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle,
        [ValidateRange(1, 300)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $Handle.Refresh()
        if ($Handle.HasExited) {
            $Handle.WaitForExit()
            return $true
        }
        $actual = Get-ProcessIdentity -ProcessId `
            ([uint32] $ExpectedProcess.ProcessId)
        if ($null -ne $actual -and
            -not (Test-ProcessIdentityEqual -Expected $ExpectedProcess `
                -Actual $actual)) {
            throw 'PID reuse or helper identity drift occurred while the owned process handle remained live.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    $false
}

function Invoke-OwnedEndPlay {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [ValidateRange(120, 600)] [int] $TimeoutSeconds = 420
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint 'before RC EditorRequestEndPlay'
    $requestResult = 'returned'
    try {
        Invoke-RcCall -ObjectPath $levelEditor `
            -FunctionName 'EditorRequestEndPlay' `
            -TimeoutSec ([Math]::Min($TimeoutSeconds, 300)) | Out-Null
    }
    catch {
        $requestResult = $_.Exception.Message
    }
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lastError = ''
    do {
        Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
            Out-Null
        Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
        if (Test-RcOwnership -ProcessId `
                ([uint32] $ExpectedProcess.ProcessId)) {
            try {
                $state = Invoke-RcCall -ObjectPath $levelEditor `
                    -FunctionName 'IsInPlayInEditor' -TimeoutSec 30
                if ([bool] $state.ReturnValue -eq $false) {
                    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
                        -Checkpoint 'after completed PIE teardown'
                    return [pscustomobject] [ordered] @{
                        RequestResult = $requestResult
                        Completed = $true
                    }
                }
            }
            catch {
                $lastError = $_.Exception.Message
            }
        }
        Start-Sleep -Seconds 2
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "PIE teardown did not complete safely: request=$requestResult lastError=$lastError"
}

function Invoke-GracefulQuit {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint 'before RC QuitEditor'
    $connectionResult = 'returned'
    try {
        Invoke-RcCall -ObjectPath $quitLibrary -FunctionName 'QuitEditor' `
            -TimeoutSec 30 | Out-Null
    }
    catch {
        $connectionResult = $_.Exception.Message
    }
    Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
        Out-Null
    $Handle.Refresh()
    if (-not $Handle.HasExited) {
        $actual = Get-ProcessIdentity -ProcessId `
            ([uint32] $ExpectedProcess.ProcessId)
        if ($null -ne $actual -and
            -not (Test-ProcessIdentityEqual -Expected $ExpectedProcess `
                -Actual $actual)) {
            throw 'Owned process identity changed after QuitEditor.'
        }
    }
    $connectionResult
}

function Stop-ExactHelperForContainment {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [System.Diagnostics.Process] $Handle
    )

    $Handle.Refresh()
    if ($Handle.HasExited) {
        return
    }
    Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
        Out-Null
    Assert-LaunchedProcessIdentity -Expected $ExpectedProcess | Out-Null
    if ([uint32] $Handle.Id -ne [uint32] $ExpectedProcess.ProcessId) {
        throw 'Process handle does not match the exact UE5.5 helper; refusing containment.'
    }
    Stop-Process -Id ([int] $ExpectedProcess.ProcessId) -Force `
        -ErrorAction Stop
    if (-not (Wait-ExactProcessExit -ExpectedProcess $ExpectedProcess `
            -Handle $Handle -TimeoutSeconds 30)) {
        throw 'Exact UE5.5 helper did not exit after containment.'
    }
}

function Get-ExactTemasekAssetPins {
    if (-not (Test-Path -LiteralPath $temasekAssetRoot -PathType Container)) {
        throw "Temasek Shophouse native asset root is absent: $temasekAssetRoot"
    }
    $rootItem = Get-Item -LiteralPath $temasekAssetRoot -Force
    if (($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Temasek Shophouse native asset root is a reparse point: $temasekAssetRoot"
    }
    $rootFullPath = [IO.Path]::GetFullPath($temasekAssetRoot)
    $materialFullPath = [IO.Path]::GetFullPath(
        (Join-Path $temasekAssetRoot 'Materials'))
    $allowedDirectories = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    [void] $allowedDirectories.Add($rootFullPath)
    [void] $allowedDirectories.Add($materialFullPath)
    foreach ($directory in Get-ChildItem -LiteralPath $temasekAssetRoot `
            -Directory -Recurse -Force) {
        $directoryFullPath = [IO.Path]::GetFullPath($directory.FullName)
        if (($directory.Attributes -band
                [IO.FileAttributes]::ReparsePoint) -ne 0 -or
            -not $allowedDirectories.Contains($directoryFullPath)) {
            throw "Unexpected or reparse-point directory in the exact Temasek Shophouse asset root: $($directory.FullName)"
        }
    }
    $allowed = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($removedStem in $removedLegacyFoliageRelativeStems) {
        foreach ($suffix in $packageArtifactSuffixes) {
            $removedPath = [IO.Path]::GetFullPath(
                (Join-Path $temasekAssetRoot "$removedStem$suffix"))
            if ([IO.File]::Exists($removedPath)) {
                throw "Removed legacy foliage package artifact is present: $removedPath"
            }
        }
    }
    foreach ($stem in $temasekAssetRelativeStems) {
        foreach ($suffix in $packageArtifactSuffixes) {
            [void] $allowed.Add([IO.Path]::GetFullPath(
                (Join-Path $temasekAssetRoot "$stem$suffix")))
        }
        $primary = Join-Path $temasekAssetRoot "$stem.uasset"
        if (-not [IO.File]::Exists([IO.Path]::GetFullPath($primary))) {
            throw "Required Temasek Shophouse primary package is absent: $primary"
        }
    }
    foreach ($file in Get-ChildItem -LiteralPath $temasekAssetRoot -File -Recurse) {
        if (-not $allowed.Contains([IO.Path]::GetFullPath($file.FullName))) {
            throw "Unexpected file in the exact Temasek Shophouse asset root: $($file.FullName)"
        }
    }
    @(
        foreach ($path in @($allowed | Sort-Object)) {
            Get-FileStateIdentity -Path $path
        }
    )
}

if (-not (Test-Path -LiteralPath $outputRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $logRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $ddcRoot -PathType Container)) {
    throw 'The exact output, log, or DDC directory is absent.'
}
foreach ($path in @($logFile, $evidenceManifestPath) + @(
        $poses | ForEach-Object {
            Join-Path $outputRoot ([string] $_.OutputFileName)
        })) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite an existing R24 evidence artifact: $path"
    }
}

$script:protectedUE54Before = Get-ProtectedUE54Identity
if (@(Get-NativeTRIADUnrealProcesses).Count -ne 0) {
    throw 'R24 capture requires UE5.5/native TRIAD helpers to be idle; the protected UE5.4/CAPSTONE set may remain open.'
}
if (@(Get-RcListeners).Count -ne 0) {
    throw 'R24 capture requires no existing RC port 30010 listener.'
}

$editorPin = Get-FileIdentity -Path $editor
$projectPin = Get-FileIdentity -Path $projectFile
$mapPin = Get-FileIdentity -Path $mapFile
$runtimeDllPin = Get-FileIdentity -Path $runtimeDll
$editorDllPin = Get-FileIdentity -Path $editorDll
$temasekAssetPins = @(Get-ExactTemasekAssetPins)
$suppressedFallbackMeshPin =
    Get-FileIdentity -Path $suppressedFallbackMeshAsset
$outerGroundMeshPin = Get-FileIdentity -Path $outerGroundMeshAsset
$outerGroundMaterialPin = Get-FileIdentity -Path $outerGroundMaterialAsset
Assert-ExactPin -Pin $projectPin -Bytes $expectedProjectBytes `
    -Sha256 $expectedProjectSha256 -Description 'TRIAD project file'
Assert-ExactPin -Pin $mapPin -Bytes $ExpectedMapBytes `
    -Sha256 $ExpectedMapSha256 -Description 'selected committed V5D map successor'
Assert-ExactPin -Pin $runtimeDllPin -Bytes $ExpectedRuntimeDllBytes `
    -Sha256 $ExpectedRuntimeDllSha256 -Description 'selected runtime DLL'
Assert-ExactPin -Pin $editorDllPin -Bytes $ExpectedEditorDllBytes `
    -Sha256 $ExpectedEditorDllSha256 -Description 'selected editor DLL'
$script:boundaryPins = @(
    $selfPin,
    $phase2CommitReceiptPin,
    $placementPin,
    $phase2SourceContractPin,
    $phase2ManifestPin,
    $editorPin,
    $projectPin,
    $mapPin,
    $runtimeDllPin,
    $editorDllPin,
    $suppressedFallbackMeshPin,
    $outerGroundMeshPin,
    $outerGroundMaterialPin
) + $temasekAssetPins
Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'

$argumentLine = "`"$projectFile`" $mapPackage -DisablePlugin=AirSim -unattended -nop4 -NoSplash -NoAutoSave -RenderOffscreen -NoSound -asyncstaticmeshcompilation=0 -dx12 -sm6 -ResX=2560 -ResY=1440 -Windowed -ini:Engine:[HTTP]:HttpMaxConnectionsPerServer=12 -ini:Engine:[/Script/CesiumRuntime.CesiumRuntimeSettings]:MaxCacheItems=32768 -DDC=InstalledNoZenLocalFallback -LocalDataCachePath=`"$ddcRoot`" -RemoteControlHttpServer -RCWebControlEnable -ExecCmds=`"WebControl.StartServer`" -abslog=`"$logFile`""
$oldLocalDdc = ${env:UE-LocalDataCachePath}
$process = $null
$launchedIdentity = $null
$pieMayBeActive = $false
$quitRequested = $false
$forcedContainment = $false
$exitCode = $null
$quitConnectionResult = ''
$endPlayEvidence = $null
$workflowError = $null
$cleanupErrors = [Collections.Generic.List[string]]::new()
$postconditionErrors = [Collections.Generic.List[string]]::new()
$captureEvidence = [Collections.Generic.List[object]]::new()
$projectIdentityReport = ''
$assetValidationReport = ''
$mapValidationReport = ''
$initialPieValidationReport = ''

try {
    ${env:UE-LocalDataCachePath} = $ddcRoot
    $process = Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $projectRoot -PassThru -WindowStyle Hidden
    $identityDeadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        $candidate = Get-ProcessIdentity -ProcessId ([uint32] $process.Id)
        if ($null -ne $candidate -and
            -not [string]::IsNullOrWhiteSpace($candidate.ExecutablePath) -and
            -not [string]::IsNullOrWhiteSpace($candidate.CommandLine) -and
            $candidate.CreationUtcTicks -gt 0) {
            $launchedIdentity = $candidate
            break
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $identityDeadline)
    if (-not (Test-ExpectedHelperLaunchIdentity -Identity $launchedIdentity `
            -Handle $process)) {
        throw 'Launched process did not prove the exact UE5.5/project/map/RC/log identity.'
    }

    $editorDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
    $projectIdentity = $null
    do {
        Assert-ProtectedUE54Unchanged -Before $script:protectedUE54Before |
            Out-Null
        Assert-LaunchedProcessIdentity -Expected $launchedIdentity | Out-Null
        if (Test-RcOwnership -ProcessId `
                ([uint32] $launchedIdentity.ProcessId)) {
            try {
                $projectIdentity = Invoke-OwnedRcCall `
                    -ExpectedProcess $launchedIdentity `
                    -ObjectPath $identityLibrary `
                    -FunctionName 'ValidateIstanaExploreRemoteControlProject' `
                    -Parameters @{ ExpectedProjectPath = $projectRoot } `
                    -TimeoutSec 30
            }
            catch {
                $projectIdentity = $null
            }
            if ($null -ne $projectIdentity -and
                $projectIdentity.ReturnValue -eq $true) {
                break
            }
        }
        Start-Sleep -Seconds 2
    } while ([DateTime]::UtcNow -lt $editorDeadline)
    if ($null -eq $projectIdentity -or
        $projectIdentity.ReturnValue -ne $true) {
        throw 'The helper did not prove exact RC ownership and project identity.'
    }
    $projectIdentityReport = [string] $projectIdentity.OutReport

    $assetValidation = Invoke-RequiredOwnedRcCall `
        -ExpectedProcess $launchedIdentity -ObjectPath $temasekLibrary `
        -FunctionName 'ValidateIstanaExploreV5DTemasekShophouseR24Assets' `
        -TextProperty 'OutReport' -TimeoutSec 900
    $assetValidationReport = [string] $assetValidation.OutReport
    foreach ($marker in $temasekAssetValidationMarkers) {
        if (-not $assetValidationReport.Contains(
                $marker, [StringComparison]::Ordinal)) {
            throw "Temasek asset validation lacks the exact mesh/material marker: $marker"
        }
    }

    $mapValidation = Invoke-RequiredOwnedRcCall `
        -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
        -FunctionName 'ValidateIstanaExploreV5DHybridMap' `
        -TextProperty 'OutReport' -TimeoutSec 900
    $mapValidationReport = [string] $mapValidation.OutReport
    if (-not $mapValidationReport.StartsWith(
            'ISTANA_EXPLORE_V5D_HYBRID_MAP_VALID',
            [StringComparison]::Ordinal)) {
        throw "Unexpected V5D map validation: $mapValidationReport"
    }
    foreach ($marker in $temasekMapValidationMarkers) {
        if (-not $mapValidationReport.Contains(
                $marker, [StringComparison]::Ordinal)) {
            throw "V5D map validation lacks the R24 evidence marker: $marker"
        }
    }

    $initialPie = Invoke-OwnedRcCall -ExpectedProcess $launchedIdentity `
        -ObjectPath $levelEditor -FunctionName 'IsInPlayInEditor' `
        -TimeoutSec 30
    if ([bool] $initialPie.ReturnValue) {
        throw 'A fresh non-PIE editor world was not available.'
    }
    $pieMayBeActive = $true
    Invoke-OwnedRcCall -ExpectedProcess $launchedIdentity `
        -ObjectPath $levelEditor -FunctionName 'EditorRequestBeginPlay' `
        -TimeoutSec 60 | Out-Null
    Wait-OwnedPieState -ExpectedProcess $launchedIdentity -Expected $true `
        -TimeoutSeconds $PieTimeoutSeconds

    $pieDeadline = [DateTime]::UtcNow.AddSeconds($PieTimeoutSeconds)
    $initialPieValidation = $null
    do {
        Start-Sleep -Seconds 1
        $initialPieValidation = Invoke-OwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName 'ValidateIstanaExploreV5DHybridPlayWorld' `
            -TimeoutSec 180
        if ($initialPieValidation.ReturnValue -eq $true -and
            ([string] $initialPieValidation.OutReport).StartsWith(
                'ISTANA_EXPLORE_V5D_HYBRID_PIE_VALID',
                [StringComparison]::Ordinal)) {
            break
        }
    } while ([DateTime]::UtcNow -lt $pieDeadline)
    if ($null -eq $initialPieValidation -or
        $initialPieValidation.ReturnValue -ne $true -or
        -not ([string] $initialPieValidation.OutReport).StartsWith(
            'ISTANA_EXPLORE_V5D_HYBRID_PIE_VALID',
            [StringComparison]::Ordinal)) {
        throw "Initial V5D PIE validation failed: $($initialPieValidation.OutReport)"
    }
    $initialPieValidationReport = [string] $initialPieValidation.OutReport

    foreach ($pose in $poses) {
        Assert-FilePins -Pins $script:boundaryPins `
            -Checkpoint "before capture $($pose.Label)"
        $setPose = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName 'SetIstanaExploreV5DHybridPlayViewPoseForQa' `
            -Parameters @{
                WorldViewLocationCentimeters = @{
                    X = [double] $pose.X
                    Y = [double] $pose.Y
                    Z = [double] $pose.Z
                }
                WorldViewRotationDegrees = @{
                    Pitch = [double] $pose.Pitch
                    Yaw = [double] $pose.Yaw
                    Roll = [double] $pose.Roll
                }
            } -TextProperty 'OutMessage' -TimeoutSec 60
        $setPoseMessage = [string] $setPose.OutMessage
        if (-not $setPoseMessage.StartsWith(
                'EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_PASS:',
                [StringComparison]::Ordinal) -or
            -not $setPoseMessage.Contains(
                'exactQaViewPose=true', [StringComparison]::Ordinal)) {
            throw "Unexpected pose acknowledgement for $($pose.Label): $setPoseMessage"
        }

        $stateWindow = Wait-StableExactPoseState `
            -ExpectedProcess $launchedIdentity -Pose $pose `
            -TimeoutSeconds $ProviderTimeoutSeconds
        $preCaptureState = Invoke-OwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName 'GetIstanaExploreV5DHybridPlayStateReport' `
            -TimeoutSec 120
        if (-not (Test-ExactPoseState -Response $preCaptureState `
                -Pose $pose -EvidenceMode $ProviderEvidenceMode)) {
            throw "The immediate pre-capture state lost the exact $ProviderEvidenceMode pose contract for $($pose.Label): $($preCaptureState.OutReport)"
        }
        $preCaptureProviderTelemetry =
            Get-ProviderTelemetrySample -Response $preCaptureState
        $captureRequestedUtc = [DateTime]::UtcNow
        $captureFunction = if ($ProviderEvidenceMode -cne 'ProviderReady') {
            'CaptureIstanaExploreV5DHybridDiagnosticPlayView'
        }
        else {
            'CaptureIstanaExploreV5DHybridPlayView'
        }
        $capture = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $launchedIdentity -ObjectPath $v5dLibrary `
            -FunctionName $captureFunction `
            -Parameters @{ OutputFileName = [string] $pose.OutputFileName } `
            -TextProperty 'OutMessage' -TimeoutSec $CaptureTimeoutSeconds
        $captureMessage = [string] $capture.OutMessage
        $expectedCapturePrefix =
            if ($ProviderEvidenceMode -cne 'ProviderReady') {
                'NON-PROOF DIAGNOSTIC EVIDENCE:'
            }
            else {
                'Accepted exact 2560x1440 HDR-off V5D Player0 proof capture'
            }
        if (-not $captureMessage.StartsWith($expectedCapturePrefix,
                [StringComparison]::Ordinal)) {
            throw "Unexpected capture acknowledgement for $($pose.Label): $captureMessage"
        }
        if ($ProviderEvidenceMode -ceq 'ProviderFallback' -and
            -not $captureMessage.Contains(
                $strictFallbackCaptureAcknowledgementMarker,
                [StringComparison]::Ordinal)) {
            throw "ProviderFallback capture acknowledgement lost its strict visible-fallback state for $($pose.Label): $captureMessage"
        }
        $png = Wait-StablePng `
            -Path (Join-Path $outputRoot ([string] $pose.OutputFileName)) `
            -NotBeforeUtc $captureRequestedUtc `
            -TimeoutSeconds $CaptureTimeoutSeconds
        $postCapture = Assert-PostCaptureWorld `
            -ExpectedProcess $launchedIdentity -Pose $pose
        Assert-FilePins -Pins $script:boundaryPins `
            -Checkpoint "after capture $($pose.Label)"
        $captureEvidence.Add([pscustomobject] [ordered] @{
            Label = $pose.Label
            OutputFileName = $pose.OutputFileName
            Pose = $pose
            SetPoseMessage = $setPoseMessage
            ProviderEvidenceMode = $ProviderEvidenceMode
            ConsecutiveExactStateReports = @($stateWindow.Reports)
            ProviderTelemetrySamples =
                @($stateWindow.ProviderTelemetrySamples)
            ImmediatePreCaptureStateReport =
                [string] $preCaptureState.OutReport
            ImmediatePreCaptureProviderTelemetry =
                $preCaptureProviderTelemetry
            CaptureMessage = $captureMessage
            Png = $png
            PostCaptureStateReport = $postCapture.StateReport
            PostCaptureProviderTelemetry =
                $postCapture.ProviderTelemetry
            PostCaptureValidationReport = $postCapture.ValidationReport
        })
    }
}
catch {
    $workflowError = $_.Exception
}
finally {
    if ($null -ne $process -and $null -eq $launchedIdentity) {
        try {
            $process.Refresh()
            if (-not $process.HasExited) {
                $candidate = Get-ProcessIdentity -ProcessId `
                    ([uint32] $process.Id)
                if (Test-ExpectedHelperLaunchIdentity -Identity $candidate `
                        -Handle $process) {
                    $launchedIdentity = $candidate
                }
                else {
                    throw 'Live started process could not be recovered as the exact owned helper.'
                }
            }
        }
        catch {
            $cleanupErrors.Add(
                "Startup identity recovery failed: $($_.Exception.Message)")
        }
    }
    if ($null -ne $process -and $null -ne $launchedIdentity) {
        try {
            $process.Refresh()
            if (-not $process.HasExited -and $pieMayBeActive) {
                $endPlayEvidence = Invoke-OwnedEndPlay `
                    -ExpectedProcess $launchedIdentity
                $pieMayBeActive = $false
            }
        }
        catch {
            $cleanupErrors.Add("EndPlay failed: $($_.Exception.Message)")
        }
        try {
            $process.Refresh()
            if (-not $process.HasExited) {
                $quitRequested = $true
                $quitConnectionResult = Invoke-GracefulQuit `
                    -ExpectedProcess $launchedIdentity -Handle $process
            }
        }
        catch {
            $cleanupErrors.Add("QuitEditor failed: $($_.Exception.Message)")
        }
        try {
            $process.Refresh()
            if (-not $process.HasExited -and
                -not (Wait-ExactProcessExit -ExpectedProcess $launchedIdentity `
                    -Handle $process -TimeoutSeconds 180)) {
                $forcedContainment = $true
                Stop-ExactHelperForContainment `
                    -ExpectedProcess $launchedIdentity -Handle $process
            }
            $process.Refresh()
            if ($process.HasExited) {
                $process.WaitForExit()
                $exitCode = $process.ExitCode
            }
        }
        catch {
            $cleanupErrors.Add("Exact helper containment failed: $($_.Exception.Message)")
        }
    }
    ${env:UE-LocalDataCachePath} = $oldLocalDdc
}

$protectedUE54After = $null
try {
    $protectedUE54After = Assert-ProtectedUE54Unchanged `
        -Before $script:protectedUE54Before
}
catch {
    $postconditionErrors.Add($_.Exception.Message)
}
try {
    Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'final postflight'
    [void] (Get-ExactTemasekAssetPins)
}
catch {
    $postconditionErrors.Add($_.Exception.Message)
}
if (@(Get-NativeTRIADUnrealProcesses).Count -ne 0) {
    $postconditionErrors.Add(
        'A UE5.5/native TRIAD Unreal helper remains after final containment.')
}
if (@(Get-RcListeners).Count -ne 0) {
    $postconditionErrors.Add(
        'RC port 30010 still has a listener after final containment.')
}
if ($captureEvidence.Count -ne $poses.Count) {
    $postconditionErrors.Add(
        "Capture count mismatch: $($captureEvidence.Count)/$($poses.Count)")
}
$seenHashes = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::OrdinalIgnoreCase)
foreach ($capture in $captureEvidence) {
    try {
        $verified = Get-PngEvidence -Path ([string] $capture.Png.Path) `
            -NotBeforeUtc ([DateTime]::SpecifyKind(
                [DateTime]::MinValue, [DateTimeKind]::Utc))
        if ($verified.Bytes -ne [int64] $capture.Png.Bytes -or
            $verified.Sha256 -cne [string] $capture.Png.Sha256) {
            throw "Capture identity changed after acceptance: $($capture.Png.Path)"
        }
        if (-not $seenHashes.Add([string] $verified.Sha256)) {
            throw "Two distinct R24 poses produced the same PNG hash: $($verified.Sha256)"
        }
    }
    catch {
        $postconditionErrors.Add($_.Exception.Message)
    }
}
if ($forcedContainment) {
    $postconditionErrors.Add(
        'The exact helper required forced containment; evidence cannot pass.')
}
if (-not $quitRequested) {
    $postconditionErrors.Add('Graceful QuitEditor was never requested.')
}
if ($exitCode -ne 0) {
    $postconditionErrors.Add(
        "UE5.5 helper exit code was not zero: $exitCode")
}

if ($null -ne $workflowError -or $cleanupErrors.Count -ne 0 -or
    $postconditionErrors.Count -ne 0) {
    $workflowText = if ($null -eq $workflowError) {
        'none'
    }
    else { $workflowError.Message }
    throw "R24 Temasek Shophouse visual evidence failed: workflow=$workflowText cleanup=$([string]::Join(' | ', @($cleanupErrors))) postconditions=$([string]::Join(' | ', @($postconditionErrors)))"
}

$result = [pscustomobject] [ordered] @{
    Schema =
        'triad.istana_explore_v5d.r24_temasek_shophouse.visual_evidence.v1'
    Status = 'PASS'
    Classification = if ($ProviderEvidenceMode -ceq 'ProviderFallback') {
        'STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF'
    }
    else {
        'RASTER_VISUAL_QA_EVIDENCE_NOT_SURVEY_NOT_PROVIDER_GEOMETRY_PROOF'
    }
    EvidenceModeClassification = $evidenceModeClassification
    Operation =
        "V5D_R24_TEMASEK_SHOPHOUSE_PHASE2_FACADE_CONTEXT_TREE_${ProviderEvidenceMode}_CAPTURE"
    RunToken = $RunToken
    ProviderEvidenceMode = $ProviderEvidenceMode
    TelemetryDwellSeconds = if ($ProviderEvidenceMode -cne 'ProviderReady') {
        $TelemetryDwellSeconds
    }
    else { 0 }
    PoseOrder = @($poses | ForEach-Object { $_.Label })
    ScriptPin = $selfPin
    Phase2CommitReceiptPin = $phase2CommitReceiptPin
    PlacementReceiptPin = $placementPin
    Phase2SourceContractPin = $phase2SourceContractPin
    Phase2ManifestPin = $phase2ManifestPin
    Phase2VisualContract = [pscustomobject] [ordered] @{
        MeshTriangles = $expectedTriangleCount
        SourceVertices = $expectedSourceVertexCount
        ProceduralComponents = $expectedComponentCount
        MaterialSlots = $expectedMaterialCount
        NativePackageCount = $expectedPackageCount
        BakedFoliageRenderComponents = $expectedBakedFoliageCount
        FoliageTreeAnchors = $expectedFoliageTreeAnchorCount
        FoliageOwner = $expectedFoliageOwner
        RemovedLegacyFoliagePackageStems =
            $removedLegacyFoliageRelativeStems
        DedicatedTreeAcceptancePose =
            $temasekTreeAcceptancePose.VisualAcceptance
        AutomatedPixelAcceptanceClaimed = $false
    }
    ProjectIdentityReport = $projectIdentityReport
    AssetValidationReport = $assetValidationReport
    MapValidationReport = $mapValidationReport
    InitialPieValidationReport = $initialPieValidationReport
    MapPin = $mapPin
    RuntimeDllPin = $runtimeDllPin
    EditorDllPin = $editorDllPin
    TemasekShophouseAssetPins = $temasekAssetPins
    SuppressedFallbackMeshAssetPin = $suppressedFallbackMeshPin
    OuterGroundMeshAssetPin = $outerGroundMeshPin
    OuterGroundMaterialAssetPin = $outerGroundMaterialPin
    EditorIdentity = $launchedIdentity
    Captures = @($captureEvidence)
    ProviderHandling = [pscustomobject] [ordered] @{
        Usage = 'VISUAL_BACKGROUND_PIXELS_IN_RASTER_SCREENSHOTS_ONLY'
        CaptureFunction = if ($ProviderEvidenceMode -cne 'ProviderReady') {
            'CaptureIstanaExploreV5DHybridDiagnosticPlayView'
        }
        else { 'CaptureIstanaExploreV5DHybridPlayView' }
        EvidenceMode = $ProviderEvidenceMode
        StableExactPoseAndFiniteTelemetryDwellRequired =
            $ProviderEvidenceMode -cne 'ProviderReady'
        GlobalProviderReadyGateRequired =
            $ProviderEvidenceMode -ceq 'ProviderReady'
        StrictLocalFallbackGateRequired =
            $ProviderEvidenceMode -ceq 'ProviderFallback'
        RequiredProviderReadyForProof =
            if ($ProviderEvidenceMode -ceq 'ProviderFallback') { $false }
            elseif ($ProviderEvidenceMode -ceq 'ProviderReady') { $true }
            else { $null }
        RequiredLocalFallbackHidden =
            if ($ProviderEvidenceMode -ceq 'ProviderFallback') { $false }
            elseif ($ProviderEvidenceMode -ceq 'ProviderReady') { $true }
            else { $null }
        StrictDiagnosticFallbackAcknowledgementRequired =
            $ProviderEvidenceMode -ceq 'ProviderFallback'
        StrictDiagnosticFallbackAcknowledgementMarker =
            if ($ProviderEvidenceMode -ceq 'ProviderFallback') {
                $strictFallbackCaptureAcknowledgementMarker
            }
            else { $null }
        ProviderReadyProofClaimed =
            $ProviderEvidenceMode -ceq 'ProviderReady'
        GlobalLoadProgressRecordedAsTelemetryOnly = $true
        LandmarkSpecificProviderReadinessClaimed = $false
        DedicatedProviderExclusionClaimed = $false
        PossibleProviderOverlapRemainsDisclosed = $true
        ProviderGeometryExported = $false
        ProviderGeometryTraced = $false
        ProviderGeometryAnalysed = $false
        ProviderGeometryDerived = $false
        ProviderGeometryBaked = $false
    }
    Authority = [pscustomobject] [ordered] @{
        VisualQaOnly = $true
        SurveyOrAsBuilt = $false
        Collision = $false
        Navigation = $false
        SensorOcclusion = $false
        RfGeometryOrMaterial = $false
        MaterialsCalibrated = $false
    }
    EndPlayEvidence = $endPlayEvidence
    QuitConnectionResult = $quitConnectionResult
    GracefulQuitRequested = $quitRequested
    ForcedContainment = $forcedContainment
    EditorExitCode = $exitCode
    ProtectedUE54 = $script:protectedUE54Before
    ProtectedUE54Before = $script:protectedUE54Before
    ProtectedUE54After = $protectedUE54After
    FinalNonProtectedEditorCount = @(Get-NativeTRIADUnrealProcesses).Count
    FinalRcListenerCount = @(Get-RcListeners).Count
    Log = $logFile
    EvidenceManifestPath = $evidenceManifestPath
}
$json = $result | ConvertTo-Json -Depth 20
$temporaryManifest = $evidenceManifestPath + '.tmp.' +
    [Guid]::NewGuid().ToString('N')
try {
    $stream = [IO.File]::Open($temporaryManifest, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $writer = [IO.StreamWriter]::new(
            $stream, [Text.UTF8Encoding]::new($false), 4096, $true)
        try {
            $writer.Write($json)
            $writer.Write([Environment]::NewLine)
            $writer.Flush()
            $stream.Flush($true)
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
    [IO.File]::Move($temporaryManifest, $evidenceManifestPath)
}
finally {
    if ([IO.File]::Exists($temporaryManifest)) {
        [IO.File]::Delete($temporaryManifest)
    }
}
$published = Get-FileIdentity -Path $evidenceManifestPath
[pscustomobject] [ordered] @{
    Status = 'PASS'
    EvidenceManifest = $published
    Captures = @($captureEvidence | ForEach-Object { $_.Png })
    ProviderGeometryExportedTracedAnalysedDerivedOrBaked = $false
    ProtectedUE54Unchanged = $true
} | ConvertTo-Json -Depth 8
