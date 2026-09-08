#requires -Version 7.0

<#
.SYNOPSIS
Guarded UE 5.5 transaction for the R29 tropical-vegetation successor.

.DESCRIPTION
The default invocation and -StaticSelfCheck are repository-only and never read
or write a native Unreal tree. Live execution requires -Execute plus explicit
native/engine/project/admission-manifest parameters. The admission manifest is
itself SHA-256 pinned and must pin the R28 map, project, both module DLLs, the
R28 commit receipt, and every native material/mesh dependency consumed by R29.

The live transaction promotes fourteen exact repo inputs, journals all source,
asset, map, and bounded build surfaces, forces both TRIAD modules through UE
5.5, and runs six cold-editor stages. The R29 C++ endpoint validates all seven
new assets and exact R28-derived grass/tree geography before removing R28,
refuses mixed/co-resident owners, adds one R29 render owner, saves once, cold
reloads, and leaves collision/navigation/sensor/RF/geospatial authority false.

No live command is authorized by this source delivery.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,
    [switch] $StaticSelfCheck,

    [string] $NativeProjectRoot,
    [string] $NativeProjectFile,
    [string] $EngineRoot,
    [ValidatePattern('^[A-Za-z0-9_]+Editor$')]
    [string] $EditorTarget,
    [string] $AdmissionManifestPath,
    [string] $AdmissionManifestSha256,

    [ValidateRange(60, 1800)]
    [int] $EditorTimeoutSeconds = 900,
    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema =
    'triad.istana_explore_v5d.r29_tropical_vegetation.native_transaction.v1'
$admissionSchema =
    'triad.istana_explore_v5d.r29_tropical_vegetation.native_admission.v1'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repositoryUnrealRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'unreal'))
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapRelativePath = 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'
$r29Library =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR29VegetationEditorLibrary'
$hybridLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'
$rcCallUri = 'http://127.0.0.1:30010/remote/object/call'
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L

$codeSourcePins = @(
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR29VegetationActor.h'; Bytes=8047L; Sha256='A0D108EEA4185AC111B8180A3907C48869FD46E029A23CDAB9033462AC47D3B6' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR29VegetationActor.cpp'; Bytes=35301L; Sha256='8C717426DFC6C0BDD7EC03BF47949518BED0B24F490BFB91EEF72804224A49E2' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DR29VegetationActorTests.cpp'; Bytes=3782L; Sha256='068DC93A9813D874B7C53AB9190635D8A085CCDA03B77D0AA2966329E6EF7753' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29VegetationAssetFactory.h'; Bytes=496L; Sha256='0D23EAC12C4AFA3792E9A5E6AB0D4CC0194B973E632DAFD481FEA3ADDCBB791B' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29VegetationAssetFactory.cpp'; Bytes=33236L; Sha256='92CDD0660F4A852E0359BF1DB3C60239FE2F142A235B6C4EE669F0A191B65B61' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR29VegetationEditorLibrary.h'; Bytes=2411L; Sha256='BA79E86E426387CF9E731D76AAE8BD56DED1095D4067CD1F6CD850BF9E199062' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29VegetationEditorLibrary.cpp'; Bytes=38195L; Sha256='82507840EE6C697E835D606EB309B6EB181975811752D526C0028EEDBC848B0E' }
)

$sourceAssetPins = @(
    [pscustomobject] @{ RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R29TropicalDetail\build_r29_tropical_vegetation.py'; Bytes=13468L; Sha256='708B5C7C7247D6E38A66FC8CA94CFCFFDF236FACFB9811D1C9F19947BEEDCE87' }
    [pscustomobject] @{ RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R29TropicalDetail\r29_tropical_vegetation.contract.json'; Bytes=7788L; Sha256='8311A6A1BFD12A7C77243F0B2016782603C7D839D29FA438F2BB1E5B25A807D9' }
    [pscustomobject] @{ RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R29TropicalDetail\Generated\IstanaPublicViewV5DR29TropicalVegetation.manifest.json'; Bytes=3542L; Sha256='6D0ECDDB999BC7A3FDA41405F4D7270784452C9D6830A98C843BD2BB2D8961C4' }
    [pscustomobject] @{ RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R29TropicalDetail\Generated\IstanaPublicViewV5DR29TropicalVegetation.mtl'; Bytes=211L; Sha256='AD38CAA9165558EA9051DF18E762885C62E9E7D65AD77086A5B72D5E774C0C34' }
    [pscustomobject] @{ RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R29TropicalDetail\Generated\SM_IPV5D_R29_GrassFineCluster_Render.obj'; Bytes=152684L; Sha256='F2EE9137C885D9EFDB50B8E4542A7EF40979FA59A549F5F15B14F7DB47A8F527' }
    [pscustomobject] @{ RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R29TropicalDetail\Generated\SM_IPV5D_R29_GrassBroadCluster_Render.obj'; Bytes=133033L; Sha256='D90E8436E1F9607AFDCF484C70AA944E12612979730853B5F1F61736061343D3' }
    [pscustomobject] @{ RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R29TropicalDetail\Generated\SM_IPV5D_R29_GrassMixedCluster_Render.obj'; Bytes=172938L; Sha256='BBA5D2912A7AFAEE0DB710A977094FB821786507DFC42AC56BB7C984B7399FB9' }
)

$expectedNativeInputRelativePaths = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28\Materials\M_IPV5D_LandmarkTurf_R28_Manicured.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28\Materials\M_IPV5D_LandmarkTurf_R28_Humid.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28\Materials\M_IPV5D_LandmarkTurf_R28_Shade.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28\Materials\M_IPV5D_LandmarkTurf_R28_DryEdge.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism\Meshes\SM_IPV5D_Tree_Umbrella_NearLOD0.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism\Meshes\SM_IPV5D_Tree_Dome_NearLOD0.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism\Meshes\SM_IPV5D_Tree_HighForkRounded_NearLOD0.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism\Meshes\SM_IPV5D_Tree_ColumnarNarrow_NearLOD0.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism\Meshes\SM_IPV5D_Tree_Palm_NearLOD0.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV4\Vegetation\Meshes\SM_IPV4_Shrub04_A.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV4\Vegetation\Materials\M_IPV4_Shrub04_Wind.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV4\Vegetation\Meshes\SM_IPV4_Calathea_D.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV4\Vegetation\Materials\M_IPV4_Calathea_Wind.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV4\Vegetation\Meshes\SM_IPV4_Periwinkle06_F.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV4\Vegetation\Materials\M_IPV4_Periwinkle_Wind.uasset'
)

$r29AssetRelativePaths = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29\Meshes\SM_IPV5D_R29_GrassFineCluster_Render.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29\Meshes\SM_IPV5D_R29_GrassBroadCluster_Render.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29\Meshes\SM_IPV5D_R29_GrassMixedCluster_Render.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29\Materials\M_IPV5D_R29_Turf_Manicured.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29\Materials\M_IPV5D_R29_Turf_Humid.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29\Materials\M_IPV5D_R29_Turf_Shade.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29\Materials\M_IPV5D_R29_Turf_DryEdge.uasset'
)

function Test-Sha256 {
    param([string] $Value)
    $null -ne $Value -and $Value -cmatch '^[0-9A-F]{64}$'
}

function Test-ContainedPath {
    param([string] $Path, [string] $Root)
    $candidate = [IO.Path]::GetFullPath($Path)
    $boundary = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $candidate.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparseAncestor {
    param([string] $Path, [string] $StopRoot)
    $cursor = [IO.DirectoryInfo] [IO.Path]::GetFullPath($Path)
    $stop = [IO.Path]::GetFullPath($StopRoot).TrimEnd('\')
    while ($null -ne $cursor) {
        if ($cursor.Exists -and
            ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse point forbidden in R29 transaction boundary: $($cursor.FullName)"
        }
        if ($cursor.FullName.TrimEnd('\').Equals(
                $stop, [StringComparison]::OrdinalIgnoreCase)) { return }
        $cursor = $cursor.Parent
    }
    throw "Path did not terminate at expected root: $Path root=$StopRoot"
}

function Get-FileState {
    param([string] $Path)
    $full = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($full)) {
        return [pscustomobject] [ordered] @{
            Path=$full; Present=$false; Bytes=0L; Sha256='ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $full
    [pscustomobject] [ordered] @{
        Path=$full
        Present=$true
        Bytes=[int64] $item.Length
        Sha256=(Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    }
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual = Get-FileState $Path
    if (-not $actual.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        -not ([string] $actual.Sha256).Equals(
            ([string] $Expected.Sha256).ToUpperInvariant(),
            [StringComparison]::Ordinal)) {
        throw "$Label pin mismatch: $Path expected=$($Expected.Bytes)/$($Expected.Sha256) actual=$($actual.Bytes)/$($actual.Sha256)"
    }
    $actual
}

function Assert-RepoPins {
    $all = @($codeSourcePins) + @($sourceAssetPins)
    if ($codeSourcePins.Count -ne 7 -or $sourceAssetPins.Count -ne 7 -or
        $all.Count -ne 14) {
        throw 'R29 transaction input roster must be exactly 7 code + 7 source assets.'
    }
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in $all) {
        if (-not $seen.Add($pin.RelativePath) -or
            [IO.Path]::IsPathRooted($pin.RelativePath) -or
            $pin.RelativePath.Contains('..') -or
            -not (Test-Sha256 ([string] $pin.Sha256))) {
            throw "Invalid or duplicate R29 repo pin: $($pin.RelativePath)"
        }
        [void] (Assert-State $pin `
            (Join-Path $repositoryUnrealRoot $pin.RelativePath) 'repo source')
    }
    if ($expectedNativeInputRelativePaths.Count -ne 15 -or
        $r29AssetRelativePaths.Count -ne 7) {
        throw 'R29 native dependency/output rosters must be exactly 15/7.'
    }
}

Assert-RepoPins

if ($StaticSelfCheck -or -not $Execute) {
    $status = if ($StaticSelfCheck) {
        'STATIC_SELF_CHECK_PASS'
    } else {
        'READ_ONLY_PREFLIGHT_PASS'
    }
    [pscustomobject] [ordered] @{
        Schema=$schema
        Status=$status
        RunToken=$RunToken
        CodeSourceCount=$codeSourcePins.Count
        SourceAssetInputCount=$sourceAssetPins.Count
        HashPinnedRepoInputCount=$codeSourcePins.Count + $sourceAssetPins.Count
        RequiredHashPinnedNativeInputCount=$expectedNativeInputRelativePaths.Count
        ExactlyNewAssetCount=$r29AssetRelativePaths.Count
        NormalAlignmentMinimumSignedDot=0.987537
        UnrealBuildOrEditorLaunched=$false
        NativeTreeReadOrWritten=$false
        TargetMapMutated=$false
        CollisionNavigationSensorRfGeospatialAuthority=$false
        VisualCaptureAccepted=$false
        CaptureRevalidationRequired=$true
    } | ConvertTo-Json -Depth 6
    return
}

foreach ($name in @(
        'NativeProjectRoot', 'NativeProjectFile', 'EngineRoot', 'EditorTarget',
        'AdmissionManifestPath', 'AdmissionManifestSha256')) {
    if (-not $PSBoundParameters.ContainsKey($name)) {
        throw "Live R29 execution requires explicit caller-supplied -$name."
    }
}
if (-not (Test-Sha256 $AdmissionManifestSha256.ToUpperInvariant())) {
    throw 'AdmissionManifestSha256 must be exactly 64 hexadecimal characters.'
}

$nativeProjectRoot = [IO.Path]::GetFullPath($NativeProjectRoot).TrimEnd('\')
$nativeProjectFile = [IO.Path]::GetFullPath($NativeProjectFile)
$engineRoot = [IO.Path]::GetFullPath($EngineRoot).TrimEnd('\')
$admissionManifestPath = [IO.Path]::GetFullPath($AdmissionManifestPath)
if (-not (Test-ContainedPath $nativeProjectFile $nativeProjectRoot) -or
    -not $nativeProjectFile.EndsWith('.uproject', [StringComparison]::OrdinalIgnoreCase) -or
    -not (Test-ContainedPath $admissionManifestPath $nativeProjectRoot)) {
    throw 'Native project or admission manifest escaped the explicit native root.'
}
Assert-NoReparseAncestor $nativeProjectRoot $nativeProjectRoot
Assert-NoReparseAncestor $nativeProjectFile $nativeProjectRoot
Assert-NoReparseAncestor $admissionManifestPath $nativeProjectRoot

$buildVersionPath = Join-Path $engineRoot 'Engine\Build\Build.version'
$buildTool = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
foreach ($path in @($buildVersionPath, $buildTool, $editor, $nativeProjectFile)) {
    if (-not [IO.File]::Exists($path)) { throw "Required live input missing: $path" }
}
$buildVersion = Get-Content -LiteralPath $buildVersionPath -Raw |
    ConvertFrom-Json -Depth 8
if ([int] $buildVersion.MajorVersion -ne 5 -or
    [int] $buildVersion.MinorVersion -ne 5) {
    throw "R29 wrapper admits UE 5.5 only; found $($buildVersion.MajorVersion).$($buildVersion.MinorVersion)."
}

$mapFile = Join-Path $nativeProjectRoot $mapRelativePath
$runtimeDllRelativePath =
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'
$editorDllRelativePath =
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'
$runtimeDll = Join-Path $nativeProjectRoot $runtimeDllRelativePath
$editorDll = Join-Path $nativeProjectRoot $editorDllRelativePath
$transactionBase = Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DR29TropicalVegetationV1'
$transactionRoot = Join-Path $transactionBase $RunToken
$pluginBinaryRoot = Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries'
$pluginIntermediateRoot = Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Intermediate'
$ddcRoot = Join-Path $nativeProjectRoot 'Saved\DerivedDataCache'
foreach ($path in @(
        $mapFile, $runtimeDll, $editorDll, $transactionBase,
        $transactionRoot, $pluginBinaryRoot, $pluginIntermediateRoot)) {
    if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
        throw "Derived R29 transaction path escaped native root: $path"
    }
}
if ([IO.Directory]::Exists($transactionRoot) -or [IO.File]::Exists($transactionRoot)) {
    throw "Run token already has a native transaction surface: $transactionRoot"
}

function Convert-ManifestPin {
    param($Row, [string] $ExpectedRelativePath, [string] $Label)
    if ($null -eq $Row -or
        -not ([string] $Row.RelativePath).Equals(
            $ExpectedRelativePath, [StringComparison]::OrdinalIgnoreCase) -or
        [int64] $Row.Bytes -le 0 -or
        -not (Test-Sha256 ([string] $Row.Sha256).ToUpperInvariant())) {
        throw "$Label has an invalid path/byte/SHA pin."
    }
    [pscustomobject] [ordered] @{
        RelativePath=$ExpectedRelativePath
        Bytes=[int64] $Row.Bytes
        Sha256=([string] $Row.Sha256).ToUpperInvariant()
    }
}

$admissionManifestState = Get-FileState $admissionManifestPath
if (-not $admissionManifestState.Present -or
    -not $admissionManifestState.Sha256.Equals(
        $AdmissionManifestSha256.ToUpperInvariant(),
        [StringComparison]::Ordinal)) {
    throw 'Caller-supplied R29 admission-manifest SHA-256 does not match.'
}
$admission = Get-Content -LiteralPath $admissionManifestPath -Raw |
    ConvertFrom-Json -Depth 20
if ($admission.Schema -cne $admissionSchema -or $admission.Status -cne 'FROZEN') {
    throw 'R29 native admission manifest must have the exact FROZEN v1 schema.'
}
$projectRelativePath = [IO.Path]::GetRelativePath(
    $nativeProjectRoot, $nativeProjectFile)
$expectedProjectPin = Convert-ManifestPin $admission.Project `
    $projectRelativePath 'project pin'
$expectedMapPin = Convert-ManifestPin $admission.PredecessorMap `
    $mapRelativePath 'R28 predecessor map pin'
$expectedRuntimeDllPin = Convert-ManifestPin $admission.RuntimeDll `
    $runtimeDllRelativePath 'runtime DLL pin'
$expectedEditorDllPin = Convert-ManifestPin $admission.EditorDll `
    $editorDllRelativePath 'editor DLL pin'
$r28ReceiptRelativePath = [string] $admission.R28CommitReceipt.RelativePath
$expectedR28ReceiptPin = Convert-ManifestPin $admission.R28CommitReceipt `
    $r28ReceiptRelativePath 'R28 commit receipt pin'
if (-not $r28ReceiptRelativePath.StartsWith(
        'Saved\TRIAD\NativeTransactions\V5DVisualRealismR28V1\',
        [StringComparison]::OrdinalIgnoreCase)) {
    throw 'R28 commit receipt is outside its exact transaction namespace.'
}
$r28ReceiptPath = Join-Path $nativeProjectRoot $r28ReceiptRelativePath

$manifestNativeInputs = @($admission.ImmutableNativeInputs)
$expectedSorted = @($expectedNativeInputRelativePaths | Sort-Object)
$actualSorted = @($manifestNativeInputs | ForEach-Object {
    [string] $_.RelativePath
} | Sort-Object)
if ($manifestNativeInputs.Count -ne 15 -or
    @(Compare-Object $expectedSorted $actualSorted).Count -ne 0) {
    throw 'Admission manifest does not pin the exact 15 native R29 dependencies.'
}
$immutableNativeInputPins = @(
    foreach ($relative in $expectedNativeInputRelativePaths) {
        $matches = @($manifestNativeInputs | Where-Object {
            ([string] $_.RelativePath).Equals(
                $relative, [StringComparison]::OrdinalIgnoreCase)
        })
        if ($matches.Count -ne 1) { throw "Native input pin is not unique: $relative" }
        Convert-ManifestPin $matches[0] $relative 'native input pin'
    }
)

function Assert-AdmissionPins {
    [void] (Assert-State $expectedProjectPin $nativeProjectFile 'project')
    [void] (Assert-State $expectedMapPin $mapFile 'R28 predecessor map')
    [void] (Assert-State $expectedRuntimeDllPin $runtimeDll 'runtime DLL')
    [void] (Assert-State $expectedEditorDllPin $editorDll 'editor DLL')
    [void] (Assert-State $expectedR28ReceiptPin $r28ReceiptPath 'R28 receipt')
    foreach ($pin in $immutableNativeInputPins) {
        [void] (Assert-State $pin `
            (Join-Path $nativeProjectRoot $pin.RelativePath) `
            'immutable native R29 dependency')
    }
    $receipt = Get-Content -LiteralPath $r28ReceiptPath -Raw |
        ConvertFrom-Json -Depth 30
    if ($receipt.Schema -cne
            'triad.istana_explore_v5d.visual_realism_r28.native_transaction.v1' -or
        $receipt.Status -cne 'PASS' -or
        [int64] $receipt.SuccessorMap.Bytes -ne [int64] $expectedMapPin.Bytes -or
        -not ([string] $receipt.SuccessorMap.Sha256).Equals(
            $expectedMapPin.Sha256, [StringComparison]::Ordinal) -or
        [bool] $receipt.CollisionNavigationSensorRfTerrainAuthority -ne $false -or
        @($receipt.R28GrassPackages).Count -ne 4 -or
        @($receipt.R28EnvironmentPackages).Count -ne 13) {
        throw 'Pinned R28 receipt does not prove the exact authority-safe predecessor.'
    }
}

function Assert-PromotablePins {
    foreach ($pin in @($codeSourcePins) + @($sourceAssetPins)) {
        $destination = Join-Path $nativeProjectRoot $pin.RelativePath
        $state = Get-FileState $destination
        if ($state.Present -and
            ([int64] $state.Bytes -ne [int64] $pin.Bytes -or
             -not $state.Sha256.Equals(
                $pin.Sha256, [StringComparison]::Ordinal))) {
            throw "R29 promotion refuses a non-identical native destination: $destination"
        }
    }
    $existingAssets = @($r29AssetRelativePaths | Where-Object {
        [IO.File]::Exists((Join-Path $nativeProjectRoot $_))
    })
    if ($existingAssets.Count -ne 0 -and $existingAssets.Count -ne 7) {
        throw "R29 output namespace is partial: $($existingAssets.Count)/7 packages."
    }
}

function Assert-PromotedRepoPins {
    foreach ($pin in @($codeSourcePins) + @($sourceAssetPins)) {
        [void] (Assert-State $pin `
            (Join-Path $nativeProjectRoot $pin.RelativePath) 'promoted R29 input')
    }
}

function Get-ProcessRecord {
    param([uint32] $ProcessId)
    $cim = Get-CimInstance Win32_Process -Filter "ProcessId=$ProcessId" `
        -ErrorAction Stop
    if ($null -eq $cim) { return $null }
    $process = Get-Process -Id $ProcessId -ErrorAction Stop
    [pscustomobject] [ordered] @{
        ProcessId=[uint32] $cim.ProcessId
        StartTimeUtcTicks=[int64] $process.StartTime.ToUniversalTime().Ticks
        ExecutablePath=[string] $cim.ExecutablePath
        CommandLine=[string] $cim.CommandLine
    }
}

function Test-ProcessRecordEqual {
    param($Expected, $Actual)
    $null -ne $Expected -and $null -ne $Actual -and
        [uint32] $Expected.ProcessId -eq [uint32] $Actual.ProcessId -and
        [int64] $Expected.StartTimeUtcTicks -eq [int64] $Actual.StartTimeUtcTicks -and
        ([string] $Expected.ExecutablePath).Equals(
            [string] $Actual.ExecutablePath,
            [StringComparison]::OrdinalIgnoreCase) -and
        ([string] $Expected.CommandLine).Equals(
            [string] $Actual.CommandLine,
            [StringComparison]::Ordinal)
}

function Get-RcOwners {
    @(
        Get-NetTCPConnection -State Listen -ErrorAction Stop |
            Where-Object { [int] $_.LocalPort -eq 30010 } |
            Select-Object -ExpandProperty OwningProcess -Unique
    )
}

function Assert-NativeIdle {
    param([string] $Label)
    $owners = @(Get-RcOwners)
    if ($owners.Count -ne 0) {
        throw "$Label refused: Remote Control port 30010 already has an owner."
    }
    $nativeProcesses = @(Get-CimInstance Win32_Process -ErrorAction Stop |
        Where-Object {
            ([string] $_.ExecutablePath).StartsWith(
                $engineRoot, [StringComparison]::OrdinalIgnoreCase) -and
            ([string] $_.Name) -like 'UnrealEditor*'
        })
    if ($nativeProcesses.Count -ne 0) {
        throw "$Label refused: a UE 5.5 editor process already exists."
    }
}

function Assert-LaunchAdmission {
    param([string] $Label)
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    $free = [int64] $os.FreeVirtualMemory * 1024L
    if ($free -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "$Label refused by fixed 10 GiB FreeVirtualMemory launch gate: $free"
    }
    [pscustomobject] [ordered] @{
        Status='LAUNCH_ADMISSION_PASS'
        FreeVirtualMemoryBytes=$free
        MinimumBytes=$minimumSystemFreeVirtualAtLaunchBytes
    }
}

function Invoke-RcCall {
    param(
        [string] $ObjectPath,
        [string] $FunctionName,
        [hashtable] $Parameters,
        [int] $TimeoutSeconds
    )
    $body = [ordered] @{
        objectPath=$ObjectPath
        functionName=$FunctionName
        generateTransaction=$false
        parameters=$Parameters
    } | ConvertTo-Json -Depth 12 -Compress
    Invoke-RestMethod -Method Put -Uri $rcCallUri -ContentType `
        'application/json' -Body $body -TimeoutSec $TimeoutSeconds
}

function Wait-ExpectedHelperIdentity {
    param([Diagnostics.Process] $Handle, [string] $Stage)
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        try { $record = Get-ProcessRecord ([uint32] $Handle.Id) }
        catch { $record = $null }
        if ($null -ne $record -and
            $record.ExecutablePath.Equals(
                $editor, [StringComparison]::OrdinalIgnoreCase) -and
            $record.CommandLine.Contains(
                $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase) -and
            $record.CommandLine.Contains(
                $mapPackage, [StringComparison]::Ordinal)) {
            return $record
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Could not prove exact launched helper identity for $Stage."
}

function Assert-HelperIdentity {
    param($Expected)
    $actual = Get-ProcessRecord ([uint32] $Expected.ProcessId)
    if (-not (Test-ProcessRecordEqual $Expected $actual)) {
        throw 'PID reuse or exact launched-helper identity drift detected.'
    }
    $actual
}

function Wait-OwnedHelperFullyReleased {
    param($Identity, [int] $TimeoutSeconds = 30)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $cim = Get-CimInstance Win32_Process `
            -Filter "ProcessId=$([uint32] $Identity.ProcessId)" `
            -ErrorAction Stop
        $owners = @(Get-RcOwners)
        if ($null -eq $cim -and
            -not ($owners -contains [uint32] $Identity.ProcessId)) {
            # Require a second stable observation; Get-Process can disappear
            # just before the CIM row and TCP listener are fully released.
            Start-Sleep -Milliseconds 500
            $cim = Get-CimInstance Win32_Process `
                -Filter "ProcessId=$([uint32] $Identity.ProcessId)" `
                -ErrorAction Stop
            $owners = @(Get-RcOwners)
            if ($null -eq $cim -and
                -not ($owners -contains [uint32] $Identity.ProcessId)) {
                return
            }
        } elseif ($null -ne $cim) {
            $cimExecutable = [string] $cim.ExecutablePath
            $cimCommandLine = [string] $cim.CommandLine
            if ((-not [string]::IsNullOrWhiteSpace($cimExecutable) -and
                 -not $cimExecutable.Equals(
                    [string] $Identity.ExecutablePath,
                    [StringComparison]::OrdinalIgnoreCase)) -or
                (-not [string]::IsNullOrWhiteSpace($cimCommandLine) -and
                 -not $cimCommandLine.Equals(
                    [string] $Identity.CommandLine,
                    [StringComparison]::Ordinal))) {
                throw 'Owned helper PID was reused while waiting for full release.'
            }
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Exact launched UE 5.5 helper did not fully release its process/RC surfaces.'
}

function Stop-OwnedHelper {
    param($Identity, [Diagnostics.Process] $Handle)
    try { [void] (Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30) }
    catch { }
    $deadline = [DateTime]::UtcNow.AddSeconds($ShutdownTimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        try { $actual = Get-ProcessRecord ([uint32] $Identity.ProcessId) }
        catch {
            Wait-OwnedHelperFullyReleased $Identity 30
            return
        }
        if ($null -eq $actual) {
            Wait-OwnedHelperFullyReleased $Identity 30
            return
        }
        if (-not (Test-ProcessRecordEqual $Identity $actual)) {
            throw 'Owned helper identity drifted during graceful shutdown.'
        }
        Start-Sleep -Milliseconds 250
    }
    [void] (Assert-HelperIdentity $Identity)
    $Handle.Kill()
    $killDeadline = [DateTime]::UtcNow.AddSeconds(30)
    while ([DateTime]::UtcNow -lt $killDeadline) {
        try { $actual = Get-ProcessRecord ([uint32] $Identity.ProcessId) }
        catch {
            Wait-OwnedHelperFullyReleased $Identity 30
            return
        }
        if ($null -eq $actual) {
            Wait-OwnedHelperFullyReleased $Identity 30
            return
        }
        if (-not (Test-ProcessRecordEqual $Identity $actual)) {
            throw 'PID changed after exact helper containment.'
        }
        Start-Sleep -Milliseconds 250
    }
    throw 'Exact launched UE 5.5 helper remained after containment.'
}

function Stop-LaunchedHelperBeforeIdentity {
    param([Diagnostics.Process] $Handle)
    # This handle was returned directly by Start-Process, so containment is
    # limited to the exact child even when CIM identity proof never completed.
    try { $Handle.Refresh() } catch { }
    if (-not $Handle.HasExited) {
        $Handle.Kill()
        if (-not $Handle.WaitForExit(30000)) {
            throw 'Exact launched helper could not be contained before identity proof.'
        }
    }
}

function Invoke-ColdStage {
    param(
        [string] $Stage,
        [string] $ObjectPath,
        [string] $FunctionName,
        [hashtable] $Parameters,
        [string[]] $ExpectedPrefixes
    )
    Assert-NativeIdle "before $Stage"
    Assert-PromotedRepoPins
    foreach ($pin in $immutableNativeInputPins) {
        [void] (Assert-State $pin (Join-Path $nativeProjectRoot $pin.RelativePath) `
            "pre-stage native input $Stage")
    }
    $launchAdmission = Assert-LaunchAdmission "before helper $Stage"
    $logRoot = Join-Path $transactionRoot 'logs'
    [void] [IO.Directory]::CreateDirectory($logRoot)
    $log = Join-Path $logRoot "$Stage.log"
    if ([IO.File]::Exists($log)) { throw "Cold-stage log already exists: $log" }
    $argumentLine =
        "`"$nativeProjectFile`" $mapPackage -DisablePlugin=AirSim " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-DDC=InstalledNoZenLocalFallback ' +
        "-LocalDataCachePath=`"$ddcRoot`" " +
        '-RemoteControlHttpServer -RCWebControlEnable ' +
        '-ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$log`""
    $handle = Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $nativeProjectRoot -PassThru -WindowStyle Hidden
    $identity = $null
    $response = $null
    $stageError = $null
    try {
        $identity = Wait-ExpectedHelperIdentity $handle $Stage
        $readyDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
        $identityResponse = $null
        do {
            [void] (Assert-HelperIdentity $identity)
            $owners = @(Get-RcOwners)
            if ($owners.Count -eq 1 -and
                [uint32] $owners[0] -eq [uint32] $identity.ProcessId) {
                try {
                    $identityResponse = Invoke-RcCall $identityLibrary `
                        'ValidateIstanaExploreRemoteControlProject' `
                        @{ ExpectedProjectPath=$nativeProjectRoot } 30
                } catch { $identityResponse = $null }
                if ($null -ne $identityResponse -and
                    $identityResponse.ReturnValue -eq $true) { break }
            }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        if ($null -eq $identityResponse -or
            $identityResponse.ReturnValue -ne $true) {
            throw "Could not prove exact project/RC ownership for $Stage."
        }
        [void] (Assert-HelperIdentity $identity)
        $response = Invoke-RcCall $ObjectPath $FunctionName $Parameters `
            $EditorTimeoutSeconds
        [void] (Assert-HelperIdentity $identity)
        $report = [string] $response.OutReport
        $matched = @($ExpectedPrefixes | Where-Object {
            $report.StartsWith($_, [StringComparison]::Ordinal)
        })
        if ($response.ReturnValue -ne $true -or $matched.Count -ne 1) {
            throw "Cold stage $Stage failed response gate: $FunctionName report=$report"
        }
    } catch { $stageError = $_.Exception }
    finally {
        if ($null -ne $identity) {
            try { Stop-OwnedHelper $identity $handle }
            catch { if ($null -eq $stageError) { $stageError = $_.Exception } }
        } elseif ($null -ne $handle) {
            try { Stop-LaunchedHelperBeforeIdentity $handle }
            catch { if ($null -eq $stageError) { $stageError = $_.Exception } }
        }
    }
    Assert-NativeIdle "after $Stage"
    if ($null -ne $stageError) { throw $stageError }
    if (-not [IO.File]::Exists($log) -or (Get-Item $log).Length -le 0) {
        throw "Cold stage $Stage did not persist a non-empty log."
    }
    [pscustomobject] [ordered] @{
        Stage=$Stage
        Function=$FunctionName
        Report=[string] $response.OutReport
        ProcessIdentity=$identity
        LaunchAdmission=$launchAdmission
        Log=Get-FileState $log
    }
}

function Get-BuildSurfacePaths {
    $paths = [Collections.Generic.List[string]]::new()
    foreach ($root in @($pluginBinaryRoot, $pluginIntermediateRoot)) {
        if ([IO.Directory]::Exists($root)) {
            foreach ($file in @(Get-ChildItem -LiteralPath $root -File -Recurse)) {
                $paths.Add([IO.Path]::GetFullPath($file.FullName))
            }
        }
    }
    @($paths | Sort-Object -Unique)
}

function New-FileJournal {
    param([string[]] $Paths, [string] $BackupRoot)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        $full = [IO.Path]::GetFullPath($path)
        if (-not (Test-ContainedPath $full $nativeProjectRoot)) {
            throw "Journal target escaped native project: $full"
        }
        $state = Get-FileState $full
        $relative = [IO.Path]::GetRelativePath($nativeProjectRoot, $full)
        $backup = Join-Path $BackupRoot $relative
        if ($state.Present) {
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($backup))
            Copy-Item -LiteralPath $full -Destination $backup
            [void] (Assert-State $state $backup 'journal backup')
        }
        $rows.Add([pscustomobject] [ordered] @{
            Path=$full; RelativePath=$relative; Before=$state
            Backup=if ($state.Present) { $backup } else { $null }
        })
    }
    @($rows)
}

function Restore-FileJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows | Sort-Object RelativePath -Descending)) {
        if ($row.Before.Present) {
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($row.Path))
            Copy-Item -LiteralPath $row.Backup -Destination $row.Path -Force
        } elseif ([IO.File]::Exists($row.Path)) {
            Remove-Item -LiteralPath $row.Path -Force
        }
        if ($row.Before.Present) {
            [void] (Assert-State $row.Before $row.Path 'rollback')
        } elseif ([IO.File]::Exists($row.Path)) {
            throw "Rollback failed to remove exact newly-created file: $($row.Path)"
        }
    }
}

function Restore-MapFromJournal {
    param([object[]] $Rows)
    if ($Rows.Count -ne 1 -or -not $Rows[0].Before.Present -or
        -not $Rows[0].Backup) {
        throw 'Atomic map rollback requires exactly one present predecessor row.'
    }
    $row = $Rows[0]
    $temporary = "$($row.Path).triad-r29-restore-$RunToken.tmp"
    if ([IO.File]::Exists($temporary)) {
        throw "Atomic map rollback temporary path already exists: $temporary"
    }
    Copy-Item -LiteralPath $row.Backup -Destination $temporary
    [void] (Assert-State $row.Before $temporary 'atomic map rollback staging')
    [IO.File]::Move($temporary, $row.Path, $true)
    [void] (Assert-State $row.Before $row.Path 'atomic map rollback')
}

function Restore-BuildSurface {
    param([object[]] $Rows)
    $before = @{}
    foreach ($row in $Rows) { $before[$row.Path.ToLowerInvariant()] = $true }
    foreach ($path in @(Get-BuildSurfacePaths)) {
        if (-not $before.ContainsKey($path.ToLowerInvariant())) {
            if (-not (Test-ContainedPath $path $pluginBinaryRoot) -and
                -not (Test-ContainedPath $path $pluginIntermediateRoot)) {
                throw "New build file escaped allowlisted roots: $path"
            }
            Remove-Item -LiteralPath $path -Force
        }
    }
    Restore-FileJournal $Rows
}

function Assert-BinaryContainsEncodedMarkers {
    param([string] $Path, [string[]] $Markers, [string] $Label)
    $hex = [Convert]::ToHexString([IO.File]::ReadAllBytes($Path))
    foreach ($marker in $Markers) {
        $utf8 = [Convert]::ToHexString([Text.Encoding]::UTF8.GetBytes($marker))
        $utf16 = [Convert]::ToHexString([Text.Encoding]::Unicode.GetBytes($marker))
        if (-not $hex.Contains($utf8, [StringComparison]::Ordinal) -and
            -not $hex.Contains($utf16, [StringComparison]::Ordinal)) {
            throw "$Label lacks fresh UTF-8/UTF-16 marker: $marker"
        }
    }
}

Assert-NativeIdle 'R29 prewrite gate'
$prewriteAdmission = Assert-LaunchAdmission 'R29 prewrite gate'
Assert-AdmissionPins
Assert-PromotablePins

[void] [IO.Directory]::CreateDirectory($transactionRoot)
$journalRoot = Join-Path $transactionRoot 'journal'
$sourceDestinations = @(@($codeSourcePins) + @($sourceAssetPins) |
    ForEach-Object { Join-Path $nativeProjectRoot $_.RelativePath })
$assetDestinations = @($r29AssetRelativePaths | ForEach-Object {
    Join-Path $nativeProjectRoot $_
})
$sourceJournal = @(New-FileJournal $sourceDestinations `
    (Join-Path $journalRoot 'sources'))
$contentJournal = @(New-FileJournal $assetDestinations `
    (Join-Path $journalRoot 'content'))
$mapJournal = @(New-FileJournal @($mapFile) (Join-Path $journalRoot 'map'))
$buildJournal = @(New-FileJournal @(Get-BuildSurfacePaths) `
    (Join-Path $journalRoot 'build'))
if ($mapJournal.Count -ne 1 -or -not $mapJournal[0].Before.Present) {
    throw 'R29 transaction failed to journal exactly one predecessor map.'
}
$verifiedMapBackup = [string] $mapJournal[0].Backup
[void] (Assert-State $expectedMapPin $verifiedMapBackup `
    'external R28 map rollback backup')

$committed = $false
$stageResults = [Collections.Generic.List[object]]::new()
$r29AssetPins = @()
try {
    foreach ($pin in @($codeSourcePins) + @($sourceAssetPins)) {
        $source = Join-Path $repositoryUnrealRoot $pin.RelativePath
        $destination = Join-Path $nativeProjectRoot $pin.RelativePath
        [void] [IO.Directory]::CreateDirectory(
            [IO.Path]::GetDirectoryName($destination))
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }
    Assert-PromotedRepoPins
    Assert-AdmissionPins

    $buildAdmission = Assert-LaunchAdmission 'before forced R29 build'
    $forcedObjects = @(
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DR29VegetationActor.cpp.obj'
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DR29VegetationActor.gen.cpp.obj'
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DR29VegetationAssetFactory.cpp.obj'
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DR29VegetationEditorLibrary.cpp.obj'
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DR29VegetationEditorLibrary.gen.cpp.obj'
    )
    foreach ($relative in $forcedObjects) {
        $path = Join-Path $nativeProjectRoot $relative
        if ([IO.File]::Exists($path)) { Remove-Item -LiteralPath $path -Force }
    }
    $buildLog = Join-Path $transactionRoot 'forced-build.log'
    $buildArguments = @(
        $EditorTarget, 'Win64', 'Development',
        "-Project=$nativeProjectFile", '-WaitMutex', '-NoHotReloadFromIDE',
        '-Module=TRIADSensorFusion', '-Module=TRIADSensorFusionEditor',
        '-ForceHeaderGeneration', '-NoUBTMakefiles', '-MaxParallelActions=1'
    )
    & $buildTool @buildArguments 2>&1 | Tee-Object -FilePath $buildLog
    if ($LASTEXITCODE -ne 0 -or -not [IO.File]::Exists($buildLog) -or
        (Get-Item $buildLog).Length -le 0) {
        throw "Forced R29 UE 5.5 build failed with exit code $LASTEXITCODE."
    }
    $runtimeAfter = Get-FileState $runtimeDll
    $editorAfter = Get-FileState $editorDll
    if (-not $runtimeAfter.Present -or -not $editorAfter.Present -or
        ($runtimeAfter.Bytes -eq $expectedRuntimeDllPin.Bytes -and
         $runtimeAfter.Sha256 -eq $expectedRuntimeDllPin.Sha256) -or
        ($editorAfter.Bytes -eq $expectedEditorDllPin.Bytes -and
         $editorAfter.Sha256 -eq $expectedEditorDllPin.Sha256)) {
        throw 'Forced R29 build did not produce fresh runtime and editor DLL receipts.'
    }
    Assert-BinaryContainsEncodedMarkers $runtimeDll @(
        'VISUAL_ASSUMPTION_BOUND_R29_MODELED_BLADE_TROPICAL_MORPHOLOGY_VARIATION',
        'meshVariantSelectionWeightsPercent=50,30,20',
        'sensorAuthority=false', 'rfAuthority=false', 'geospatialAuthority=false'
    ) 'fresh runtime DLL'
    Assert-BinaryContainsEncodedMarkers $editorDll @(
        'EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_FINAL_MUTATION_GATE',
        'EXPLORE_V5D_R29_VEGETATION_APPLY_PASS',
        'ISTANA_EXPLORE_V5D_R29_VEGETATION_SUCCESSOR_MAP_VALID',
        'exactlyOneEnvironmentOwner=true',
        'publicRealmInvariantPreserved=true',
        'authoredNormalsFaceAligned=true'
    ) 'fresh editor DLL'

    $stageResults.Add((Invoke-ColdStage `
        '00_cold_validate_r28_predecessor' $hybridLibrary `
        'ValidateIstanaExploreV5DR28VisualSuccessorMap' @{} `
        @('ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID')))
    [void] (Assert-State $expectedMapPin $mapFile 'map after R28 validation')

    $stageResults.Add((Invoke-ColdStage `
        '01_build_r29_assets' $r29Library `
        'BuildOrValidateR29VegetationAssets' @{} @(
            'ISTANA_EXPLORE_V5D_R29_VEGETATION_ASSET_BUILD_PASS',
            'ISTANA_EXPLORE_V5D_R29_VEGETATION_ASSET_BUILD_IDEMPOTENT_PASS')))
    [void] (Assert-State $expectedMapPin $mapFile 'map after R29 asset build')
    $r29AssetPins = @($assetDestinations | ForEach-Object {
        $state = Get-FileState $_
        if (-not $state.Present -or $state.Bytes -le 0 -or
            -not (Test-Sha256 $state.Sha256)) {
            throw "R29 asset package missing after build: $_"
        }
        $state
    })
    if ($r29AssetPins.Count -ne 7) { throw 'R29 asset build did not produce 7 packages.' }

    $stageResults.Add((Invoke-ColdStage `
        '02_cold_validate_r29_assets' $r29Library `
        'ValidateR29VegetationAssets' @{} `
        @('ISTANA_EXPLORE_V5D_R29_VEGETATION_ASSETS_VALID')))
    [void] (Assert-State $expectedMapPin $mapFile 'map after R29 asset validation')

    $stageResults.Add((Invoke-ColdStage `
        '03_apply_r29_swap_once' $r29Library `
        'ApplyR29VegetationSuccessorToLoadedHybridMap' @{
            ExpectedPredecessorBytes=[int64] $expectedMapPin.Bytes
            ExpectedPredecessorSha256=[string] $expectedMapPin.Sha256
            VerifiedExternalBackupFilename=$verifiedMapBackup
        } @('EXPLORE_V5D_R29_VEGETATION_APPLY_PASS')))
    $successorMapPin = Get-FileState $mapFile
    if (-not $successorMapPin.Present -or
        ($successorMapPin.Bytes -eq $expectedMapPin.Bytes -and
         $successorMapPin.Sha256 -eq $expectedMapPin.Sha256)) {
        throw 'R29 apply did not create a distinct successor map receipt.'
    }

    $stageResults.Add((Invoke-ColdStage `
        '04_cold_validate_r29_successor' $r29Library `
        'ValidateR29VegetationSuccessorMap' @{} `
        @('ISTANA_EXPLORE_V5D_R29_VEGETATION_SUCCESSOR_MAP_VALID')))
    [void] (Assert-State $successorMapPin $mapFile 'map after cold R29 validation')

    $stageResults.Add((Invoke-ColdStage `
        '05_idempotent_r29_apply' $r29Library `
        'ApplyR29VegetationSuccessorToLoadedHybridMap' @{
            ExpectedPredecessorBytes=[int64] $expectedMapPin.Bytes
            ExpectedPredecessorSha256=[string] $expectedMapPin.Sha256
            VerifiedExternalBackupFilename=$verifiedMapBackup
        } @('IDEMPOTENT_EXPLORE_V5D_R29_VEGETATION_ALREADY_VALID')))
    [void] (Assert-State $successorMapPin $mapFile 'map after idempotent R29 apply')

    Assert-PromotedRepoPins
    foreach ($pin in $immutableNativeInputPins) {
        [void] (Assert-State $pin (Join-Path $nativeProjectRoot $pin.RelativePath) `
            'commit immutable native input')
    }
    foreach ($pin in $r29AssetPins) {
        [void] (Assert-State $pin $pin.Path 'commit R29 output asset')
    }
    [void] (Assert-State $expectedMapPin $verifiedMapBackup `
        'commit preserved R28 map backup')
    Assert-NativeIdle 'R29 commit gate'

    $commit = [pscustomobject] [ordered] @{
        Schema=$schema
        Status='PASS'
        RunToken=$RunToken
        EngineVersion='5.5'
        AdmissionManifest=$admissionManifestState
        PredecessorMap=$expectedMapPin
        SuccessorMap=$successorMapPin
        PredecessorRuntimeDll=$expectedRuntimeDllPin
        SuccessorRuntimeDll=$runtimeAfter
        PredecessorEditorDll=$expectedEditorDllPin
        SuccessorEditorDll=$editorAfter
        HashPinnedRepoInputCount=14
        HashPinnedNativeInputCount=15
        R29AssetPackages=@($r29AssetPins)
        R29AssetPackageCount=7
        R28RenderOwnersRemoved=1
        R29RenderOwnersAdded=1
        CoexistenceObserved=$false
        GrassGeographyExact=$true
        TreeGeographyExact=$true
        GrassInstances=6144
        TreeInstances=7
        AuthoredNormalMinimumSignedDot=0.987537
        OneMapSave=$true
        ColdStageCount=$stageResults.Count
        Stages=@($stageResults)
        PrewriteAdmission=$prewriteAdmission
        BuildAdmission=$buildAdmission
        CollisionNavigationSensorRfGeospatialAuthority=$false
        VisualCaptureAccepted=$false
        CaptureRevalidationRequired=$true
    }
    $commitPath = Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 30 |
        Set-Content -LiteralPath $commitPath -Encoding utf8NoBOM -NoNewline
    $committed = $true
    $commit | ConvertTo-Json -Depth 30
}
finally {
    if (-not $committed) {
        $rollbackErrors = [Collections.Generic.List[string]]::new()
        try { Assert-NativeIdle 'R29 rollback entry' }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        foreach ($action in @(
                { Restore-MapFromJournal $mapJournal },
                { Restore-BuildSurface $buildJournal },
                { Restore-FileJournal $contentJournal },
                { Restore-FileJournal $sourceJournal })) {
            try { & $action }
            catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        if ($rollbackErrors.Count -ne 0) {
            throw 'R29 ROLLBACK_INCOMPLETE: ' + ($rollbackErrors -join ' | ')
        }
    }
}
