#requires -Version 7.0

<#
.SYNOPSIS
Runs the guarded V5D R32 medium-distance turf native transaction.

.DESCRIPTION
The default invocation and -StaticSelfCheck are repository-only. -Execute is
the sole native-write authority. It requires four independently hash-pinned
predecessor receipts (R30 commit/capture, then R31 commit/capture), caller pins
for the current map, both editor DLLs, and both Ground source files, and the
exact whole plugin-source-tree receipt recorded by the R31 capture.

The live path promotes exactly eight C++ files and one contract. The Ground pair
comes from the immutable pre-R33 NativeSourceClosure and is written to its
normal canonical native paths; six R32 files, the R32 contract, and the isolated
four-material content root must be absent. No R33 source is admitted. The wrapper journals source, map, plugin
binary and plugin intermediate trees, builds serially under continuous memory
and protected-process gates, cold-validates R31, creates and cold-validates the
four isolated materials, invokes CommitR32 exactly once, then cold-validates
R32. Rollback waits for owned-process quiescence before any filesystem restoration.

.EXAMPLE
.\Invoke-IstanaExploreV5DMediumDistanceTurfR32NativeTransactionV1.ps1 `
  -RunToken review -StaticSelfCheck
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,
    [switch] $StaticSelfCheck,
    [switch] $RequireR31Predecessor,

    [string] $R30CommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR30CommitReceiptSha256 = '',
    [string] $R30CaptureCommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR30CaptureCommitReceiptSha256 = '',
    [string] $R31CommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR31CommitReceiptSha256 = '',
    [string] $R31CaptureCommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR31CaptureCommitReceiptSha256 = '',

    [long] $ExpectedMapBytes = 0L,
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedMapSha256 = '',
    [long] $ExpectedRuntimeDllBytes = 0L,
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedRuntimeDllSha256 = '',
    [long] $ExpectedEditorDllBytes = 0L,
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedEditorDllSha256 = '',
    [long] $ExpectedGroundHeaderBytes = 0L,
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedGroundHeaderSha256 = '',
    [long] $ExpectedGroundSourceBytes = 0L,
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedGroundSourceSha256 = '',

    [ValidateRange(120, 3600)]
    [int] $BuildTimeoutSeconds = 1800,
    [ValidateRange(120, 1800)]
    [int] $EditorTimeoutSeconds = 900,
    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema = 'triad.istana_explore_v5d.r32_medium_distance_turf.native_transaction.v1'
$r30TransactionSchema = 'triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1'
$r30CaptureSchema = 'triad.istana_explore_v5d.r30_player0_capture.v1'
$r31TransactionSchema = 'triad.istana_explore_v5d.broad_shell_r31.native_transaction.v1'
$r31CaptureSchema = 'triad.istana_explore_v5d.r31_player0_capture.v2'
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # fixed 10 GiB
$minimumSystemFreeVirtualBytes = 6442450944L # fixed continuous 6 GiB
$privateMemoryCeilingBytes = 12884901888L # fixed 12 GiB
$memoryWatchdogPollMilliseconds = 500
$memoryWatchdogPersistentBreachMilliseconds = 2000
$script:memoryWatchdog = $null
$script:ownedNativeProcessHandles = [Collections.Generic.List[object]]::new()

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repositoryUnrealRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'unreal'))
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'TRIAD.uproject'))
$engineRoot = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.5')
$dotnet = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'))
$unrealBuildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'))
$engineSourceRoot = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Source'))
$editor = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'))
$protectedEditor = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedProject = [IO.Path]::GetFullPath('C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$nativePluginSourceRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Source'))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Intermediate'))
$r31ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsShellLookdevR31'))
$r29VegetationContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29'))
$treeRealismContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'))
$r32ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\MediumDistanceTurfR32'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DMediumDistanceTurfR32V1'))
$r30TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1'))
$r30EvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R30Evidence')
$r31TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DBroadShellR31V1'))
$r31EvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R31Evidence')
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase $RunToken))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$r31Library = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR31BroadShellEditorLibrary'
$r32Library = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'

# Exact R32 mutation closure: two immutable pre-R33 Ground overlays followed by
# the six normal R32 actor/factory/editor files. The current Ground native-before state
# is deliberately caller-pinned; no repository state is substituted for it.
$sourcePins = @(
    [pscustomobject] [ordered] @{
        RepositoryRelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R32MediumDistanceTurf\NativeSourceClosure\TRIADIstanaExploreV5DGroundVegetationActor.h'
        NativeRelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DGroundVegetationActor.h'
        Bytes = 24974L; Sha256 = '95122779D264BB0539AEC25A93D88C6485E5D4DB89E8F50B855D4EF1C82302E2'
        NativeBeforePresent = $true; NativeBeforeBytes = $ExpectedGroundHeaderBytes
        NativeBeforeSha256 = $ExpectedGroundHeaderSha256.ToUpperInvariant()
    }
    [pscustomobject] [ordered] @{
        RepositoryRelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R32MediumDistanceTurf\NativeSourceClosure\TRIADIstanaExploreV5DGroundVegetationActor.cpp'
        NativeRelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DGroundVegetationActor.cpp'
        Bytes = 249537L; Sha256 = '8B71713A4C134132BEB7ECE6DDCB93F86E690BB073F4508E325CD1BDAC5C520C'
        NativeBeforePresent = $true; NativeBeforeBytes = $ExpectedGroundSourceBytes
        NativeBeforeSha256 = $ExpectedGroundSourceSha256.ToUpperInvariant()
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h'
        Bytes = 6436L; Sha256 = '8862523D7B9CCB4D6891B6A8F23728C1B95D9E16513A1DA936565F30C3164ACE'; NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR32MediumDistanceTurfActor.cpp'
        Bytes = 35336L; Sha256 = '1B51CECF08785C889B63E95BD97B681AA065CBB36C734BC8AA2247EF7F6BD1D4'; NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.h'
        Bytes = 966L; Sha256 = 'CCD2EE99EE42BD7D7B322423E5671E49D5246E445FB0DDC0103AAED10BBCD082'; NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.cpp'
        Bytes = 22691L; Sha256 = '90FE9E9E3152BFEFE8585A0BB643F5AF3B068852C63C496AC253F8DBC829CB16'; NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.h'
        Bytes = 2280L; Sha256 = '4C5B1014E2842F7DA27431260352ABFF91F7E7841D6D404D30B7C79E21FA13E5'; NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.cpp'
        Bytes = 31204L; Sha256 = 'C85E270797F08267A67101C6B53A4E4F9383E90FBE366C90F230FF871BA1B31D'; NativeBeforePresent = $false
    }
)

$r32ContentRelativePaths = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\MediumDistanceTurfR32\Materials\M_IPV5D_R32_Turf_Manicured.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\MediumDistanceTurfR32\Materials\M_IPV5D_R32_Turf_Humid.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\MediumDistanceTurfR32\Materials\M_IPV5D_R32_Turf_Shade.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\MediumDistanceTurfR32\Materials\M_IPV5D_R32_Turf_DryEdge.uasset'
)

$sourceAssetPins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R32MediumDistanceTurf\r32_medium_distance_turf.contract.json'
        Bytes = 26288L; Sha256 = '8DED60CE751611306A29071B1E90AA47C17274EB601997C5BB0E420DA9413A89'; NativeBeforePresent = $false
    }
)

# R31/R29 compile surfaces that must already exist and remain byte-identical.
$retainedSourcePins = @(
    [pscustomobject] [ordered] @{
        RepositoryRelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R31BroadShellLookdev\NativeSourceClosure\TRIADIstanaExploreV5DContextPolicyActor.h'
        NativeRelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DContextPolicyActor.h'
        Bytes = 10649L; Sha256 = '9114F728E337523DF3AD0FE5021685683C41BB048CB0049713C2ECC9342D962B'; NativeBeforePresent = $true
    }
    [pscustomobject] [ordered] @{
        RepositoryRelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R31BroadShellLookdev\NativeSourceClosure\TRIADIstanaExploreV5DContextPolicyActor.cpp'
        NativeRelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DContextPolicyActor.cpp'
        Bytes = 134398L; Sha256 = '58074AC6449BD6FBF7D86DDB876B9CD3B7BEFF03162DAA9EFE10C0A08E0DF4C4'; NativeBeforePresent = $true
    }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR31BroadShellAssetFactory.h'; Bytes = 735L; Sha256 = 'E4DC8E285A05A96DD927D032E42FC0139FEF099A51B5F806945732B7D59FFF45'; NativeBeforePresent = $true }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR31BroadShellAssetFactory.cpp'; Bytes = 147770L; Sha256 = 'F1F89219896F8710AA4A42FA3C8E19B82ADDEA107062AA600DE0F00BAF04523F'; NativeBeforePresent = $true }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR31BroadShellEditorLibrary.h'; Bytes = 1544L; Sha256 = '9DD63C0D38508F750CBEBE5388586ACFD5BC94F605EF05B512018F202B877A49'; NativeBeforePresent = $true }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR31BroadShellEditorLibrary.cpp'; Bytes = 34184L; Sha256 = '8F4006EB722451E1ABB988FA7ACCB6A17CE1BC5165506A6560A51FBEA7120272'; NativeBeforePresent = $true }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29VegetationAssetFactory.h'; Bytes = 496L; Sha256 = '0D23EAC12C4AFA3792E9A5E6AB0D4CC0194B973E632DAFD481FEA3ADDCBB791B'; NativeBeforePresent = $true }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29VegetationAssetFactory.cpp'; Bytes = 33236L; Sha256 = '92CDD0660F4A852E0359BF1DB3C60239FE2F142A235B6C4EE669F0A191B65B61'; NativeBeforePresent = $true }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\TRIADSensorFusion.Build.cs'; Bytes = 2736L; Sha256 = 'E066028EC940568E2927ADA63F684ABA5866786B99B6F494D02713DC0969DEB2'; NativeBeforePresent = $true }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\TRIADSensorFusionEditor.Build.cs'; Bytes = 1229L; Sha256 = 'B7D10EA034A37CB15C9939DADFA1910B229ABA14098F096E21994F7A6D977C1B'; NativeBeforePresent = $true }
    [pscustomobject] [ordered] @{ RelativePath = 'Plugins\TRIADSensorFusion\TRIADSensorFusion.uplugin'; Bytes = 781L; Sha256 = '0E9C5DDEF9AEB66EFE3077D45A39B09A0646401A829AB0E2166AB331C1E9A2B5'; NativeBeforePresent = $true }
)

$r33ForbiddenNativePins = @(
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h'
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.cpp'
)
$r33ForbiddenSymbols = @(
    'bR33DualCesiumContextConfigured'
    'ShouldHideSourceTerrainRendererForVisualContext'
    'TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor'
)

function Get-FileState {
    param([string] $Path)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject] [ordered] @{ Present = $false; Bytes = 0L; Sha256 = 'ABSENT' }
    }
    $item = Get-Item -LiteralPath $Path -Force
    [pscustomobject] [ordered] @{
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual = Get-FileState $Path
    if ([bool] $actual.Present -ne [bool] $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label state mismatch: $Path expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Get-RequiredReceiptProperty {
    param($Object, [string] $Name, [string] $Label)
    if ($null -eq $Object) { throw "$Label is null while requiring '$Name'." }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { throw "$Label lacks required property '$Name'." }
    $property.Value
}

function Test-ContainedPath {
    param([string] $Path, [string] $Root)
    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $fullPath.Equals($fullRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith($fullRoot + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Get-PinRelativePath {
    param([object] $Pin, [ValidateSet('Repository', 'Native')] [string] $Surface)
    $explicitName = if ($Surface -ceq 'Repository') { 'RepositoryRelativePath' } else { 'NativeRelativePath' }
    $explicit = $Pin.PSObject.Properties[$explicitName]
    if ($null -ne $explicit) { return [string] $explicit.Value }
    $same = $Pin.PSObject.Properties['RelativePath']
    if ($null -eq $same -or [string]::IsNullOrWhiteSpace([string] $same.Value)) {
        throw "R32 pin lacks both $explicitName and same-path RelativePath semantics."
    }
    [string] $same.Value
}

function Assert-CanonicalRelativePath {
    param([string] $RelativePath, [string] $Label)
    if ([IO.Path]::IsPathRooted($RelativePath) -or $RelativePath.Contains('..')) {
        throw "Non-canonical $Label path: $RelativePath"
    }
}

function Assert-Pins {
    param([object[]] $Pins, [string] $Root, [switch] $NativeAfter)
    foreach ($pin in $Pins) {
        $surface = if ($NativeAfter) { 'Native' } else { 'Repository' }
        $relative = Get-PinRelativePath $pin $surface
        Assert-CanonicalRelativePath $relative "$surface pin"
        $path = [IO.Path]::GetFullPath((Join-Path $Root $relative))
        if (-not (Test-ContainedPath $path $Root)) { throw "$surface pin escaped root: $path" }
        [void] (Assert-State ([pscustomobject] @{ Present=$true; Bytes=[int64] $pin.Bytes; Sha256=[string] $pin.Sha256 }) $path "$surface pinned source")
    }
}

function Read-HashPinnedDirectCommitReceipt {
    param([string] $Path, [string] $ExpectedSha256, [string] $DirectParent, [string] $Label)
    if ([string]::IsNullOrWhiteSpace($Path) -or [string]::IsNullOrWhiteSpace($ExpectedSha256)) {
        throw "$Label requires an explicit path and SHA-256."
    }
    $fullPath = [IO.Path]::GetFullPath($Path)
    $directory = [IO.Path]::GetDirectoryName($fullPath)
    $parent = [IO.Path]::GetDirectoryName($directory)
    $token = [IO.Path]::GetFileName($directory)
    if (-not [IO.Path]::GetFileName($fullPath).Equals('commit.json', [StringComparison]::Ordinal) -or
        -not $parent.Equals([IO.Path]::GetFullPath($DirectParent), [StringComparison]::OrdinalIgnoreCase) -or
        $token -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
        throw "$Label must be direct safe-token child commit.json below $DirectParent : $fullPath"
    }
    $state = Get-FileState $fullPath
    if (-not $state.Present -or $state.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw "$Label SHA-256 mismatch: expected=$($ExpectedSha256.ToUpperInvariant()) actual=$($state.Sha256)"
    }
    [pscustomobject] [ordered] @{
        Path=$fullPath; Token=$token; State=$state
        Receipt=Get-Content -LiteralPath $fullPath -Raw | ConvertFrom-Json -Depth 32
    }
}

function Test-ReceiptStateMatches {
    param($State, [long] $Bytes, [string] $Sha256)
    $null -ne $State -and
        [bool] (Get-RequiredReceiptProperty $State 'Present' 'receipt state') -eq $true -and
        [int64] (Get-RequiredReceiptProperty $State 'Bytes' 'receipt state') -eq $Bytes -and
        [string] (Get-RequiredReceiptProperty $State 'Sha256' 'receipt state') -ceq $Sha256.ToUpperInvariant()
}

function Test-ReceiptFileBinding {
    param($Admission, $ExpectedAdmission, [string] $Label)
    $path = [IO.Path]::GetFullPath([string] (Get-RequiredReceiptProperty $Admission 'Path' $Label))
    $file = Get-RequiredReceiptProperty $Admission 'File' $Label
    if (-not $path.Equals($ExpectedAdmission.Path, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-ReceiptStateMatches $file $ExpectedAdmission.State.Bytes $ExpectedAdmission.State.Sha256)) {
        throw "$Label is not bound to the exact admitted predecessor receipt."
    }
}

function Assert-R31NaniteRasterComparison {
    param($Receipt, [object[]] $Captures, [string] $CaptureToken)
    $comparison = Get-RequiredReceiptProperty $Receipt `
        'NaniteRasterHighOccupancyComparison' 'R31 capture receipt'
    if ([bool] (Get-RequiredReceiptProperty $comparison 'Required' 'R31 Nanite/raster comparison') -ne $true -or
        [string] (Get-RequiredReceiptProperty $comparison 'PoseId' 'R31 Nanite/raster comparison') -cne 'surroundings_oblique_macdonald' -or
        [bool] (Get-RequiredReceiptProperty $comparison 'SamePose' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $comparison 'SameGameProcess' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $comparison 'SameCookedClosure' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $comparison 'MechanicalValidationPassed' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $comparison 'HumanNaniteRasterComparisonAttested' 'R31 Nanite/raster comparison') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $comparison 'NaniteRasterAppearanceParityAccepted' 'R31 Nanite/raster comparison') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $comparison 'AutomaticVisualAcceptanceAllowed' 'R31 Nanite/raster comparison') -ne $false) {
        throw 'R31 Nanite/raster comparison is absent or not the exact fail-closed mechanical A/B proof.'
    }
    $nanite = Get-RequiredReceiptProperty $comparison 'NaniteOn' 'R31 Nanite/raster comparison'
    $naniteCapture = Get-RequiredReceiptProperty $nanite 'Capture' 'R31 Nanite comparison side'
    if ((ConvertTo-Json $naniteCapture -Depth 20 -Compress) -cne
            (ConvertTo-Json $Captures[4] -Depth 20 -Compress) -or
        [string] (Get-RequiredReceiptProperty $nanite 'RenderPathId' 'R31 Nanite comparison side') -cne 'NANITE_ON' -or
        [int] (Get-RequiredReceiptProperty $nanite 'NaniteConsoleValue' 'R31 Nanite comparison side') -ne 1 -or
        [int] (Get-RequiredReceiptProperty $nanite 'NaniteProxyRenderMode' 'R31 Nanite comparison side') -ne 1) {
        throw 'R31 Nanite comparison side is not the exact fifth baseline capture with both CVars read back as one.'
    }
    $raster = Get-RequiredReceiptProperty $comparison 'RasterFallback' 'R31 Nanite/raster comparison'
    $rasterPose = Get-RequiredReceiptProperty $raster 'Pose' 'R31 raster comparison side'
    $baselinePose = Get-RequiredReceiptProperty $Captures[4] 'Pose' 'R31 fifth baseline capture'
    if ((ConvertTo-Json $rasterPose -Depth 8 -Compress) -cne
            (ConvertTo-Json $baselinePose -Depth 8 -Compress) -or
        [string] (Get-RequiredReceiptProperty $raster 'RenderPathId' 'R31 raster comparison side') -cne 'RASTER_FALLBACK' -or
        [int] (Get-RequiredReceiptProperty $raster 'NaniteConsoleValue' 'R31 raster comparison side') -ne 0 -or
        [int] (Get-RequiredReceiptProperty $raster 'NaniteProxyRenderMode' 'R31 raster comparison side') -ne 0) {
        throw 'R31 raster comparison side changed pose or lacks exact zero CVar readback.'
    }
    foreach ($stateName in @('ImmediatePreCaptureState','ImmediatePostCaptureState')) {
        $stateReport = [string] (Get-RequiredReceiptProperty $raster $stateName 'R31 raster comparison side')
        foreach ($marker in @(
            'poseId=surroundings_oblique_macdonald', 'exactQaViewPose=true',
            'renderPath=RASTER_FALLBACK', 'r.Nanite=0',
            'r.Nanite.ProxyRenderMode=0', 'naniteMeshEnabled=true',
            'naniteDataValid=true', 'naniteKeepPercentTriangles=1.0',
            'fallbackPercentTriangles=1.0')) {
            if (-not $stateReport.Contains($marker,[StringComparison]::Ordinal)) {
                throw "R31 raster comparison $stateName lacks '$marker'."
            }
        }
    }
    $rasterImage = Get-RequiredReceiptProperty $raster 'Image' 'R31 raster comparison side'
    $expectedRasterPath = [IO.Path]::GetFullPath((Join-Path `
        (Join-Path (Join-Path $r31EvidenceBase $CaptureToken) 'captures') `
        "explore_v5d_r31_player0_surroundings_oblique_macdonald_${CaptureToken}_raster_fallback.png"))
    $actualRasterPath = [IO.Path]::GetFullPath([string] (
        Get-RequiredReceiptProperty $rasterImage 'Path' 'R31 raster comparison image'))
    if (-not $actualRasterPath.Equals($expectedRasterPath,[StringComparison]::OrdinalIgnoreCase) -or
        [int] (Get-RequiredReceiptProperty $rasterImage 'WidthPixels' 'R31 raster comparison image') -ne 2560 -or
        [int] (Get-RequiredReceiptProperty $rasterImage 'HeightPixels' 'R31 raster comparison image') -ne 1440 -or
        [bool] (Get-RequiredReceiptProperty $rasterImage 'NonBlank' 'R31 raster comparison image') -ne $true -or
        [string]::IsNullOrWhiteSpace([string] (Get-RequiredReceiptProperty $rasterImage 'DecodedBgraSha256' 'R31 raster comparison image'))) {
        throw 'R31 raster comparison image is not the exact canonical decoded sixth image.'
    }
    [void] (Assert-State $rasterImage $expectedRasterPath 'R31 raster comparison PNG')
    $restore = Get-RequiredReceiptProperty $comparison 'NaniteRestore' 'R31 Nanite/raster comparison'
    $restoreState = [string] (Get-RequiredReceiptProperty $restore 'State' 'R31 Nanite restore')
    if ([bool] (Get-RequiredReceiptProperty $restore 'Restored' 'R31 Nanite restore') -ne $true -or
        [string] (Get-RequiredReceiptProperty $restore 'RenderPathId' 'R31 Nanite restore') -cne 'NANITE_ON' -or
        [int] (Get-RequiredReceiptProperty $restore 'NaniteConsoleValue' 'R31 Nanite restore') -ne 1 -or
        [int] (Get-RequiredReceiptProperty $restore 'NaniteProxyRenderMode' 'R31 Nanite restore') -ne 1 -or
        -not $restoreState.Contains('renderPath=NANITE_ON',[StringComparison]::Ordinal) -or
        -not $restoreState.Contains('r.Nanite=1',[StringComparison]::Ordinal) -or
        -not $restoreState.Contains('r.Nanite.ProxyRenderMode=1',[StringComparison]::Ordinal)) {
        throw 'R31 capture does not prove exact Nanite CVar restoration after the raster image.'
    }
    $comparison
}

function Assert-FourPredecessorReceipts {
    $r30CommitAdmission = Read-HashPinnedDirectCommitReceipt `
        $R30CommitReceipt $ExpectedR30CommitReceiptSha256 `
        $r30TransactionBase 'R30 transaction receipt'
    $r30Commit = $r30CommitAdmission.Receipt
    if ([string] (Get-RequiredReceiptProperty $r30Commit 'Schema' 'R30 transaction receipt') -cne $r30TransactionSchema -or
        [string] (Get-RequiredReceiptProperty $r30Commit 'Status' 'R30 transaction receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredReceiptProperty $r30Commit 'RunToken' 'R30 transaction receipt') -cne $r30CommitAdmission.Token -or
        [int] (Get-RequiredReceiptProperty $r30Commit 'R30ContentPackageCount' 'R30 transaction receipt') -ne 12 -or
        [string] (Get-RequiredReceiptProperty $r30Commit 'PredecessorVegetationOwner' 'R30 transaction receipt') -cne 'R29' -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'VegetationMutationAllowed' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'R29FacadeMeshRetained' 'R30 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'R28PublicRealmRetained' 'R30 transaction receipt') -ne $true -or
        [int] (Get-RequiredReceiptProperty $r30Commit 'R29EnvironmentActiveRenderers' 'R30 transaction receipt') -ne 0 -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'R29EnvironmentConcurrentRenderingAllowed' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'TreeRealismValidated' 'R30 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'TreeMaterialResponseV3Promoted' 'R30 transaction receipt') -ne $true -or
        [int] (Get-RequiredReceiptProperty $r30Commit 'TreeResponseMaterialPackageCount' 'R30 transaction receipt') -ne 13 -or
        [int] (Get-RequiredReceiptProperty $r30Commit 'TreeDerivativeMeshPackageCount' 'R30 transaction receipt') -ne 5 -or
        [int] (Get-RequiredReceiptProperty $r30Commit 'TreeRuntimeResponseMidCount' 'R30 transaction receipt') -ne 26 -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'TreePlacementGeometryOpacityWindAuthorityModified' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'TerrainR29Validated' 'R30 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'ContextPolicyShellValidatedBeforeAndAfter' 'R30 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'SimulationCollisionNavigationSensorRfAuthority' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'VisualCaptureAccepted' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Commit 'CaptureRevalidationRequired' 'R30 transaction receipt') -ne $true) {
        throw 'R30 transaction receipt failed the exact committed predecessor contract.'
    }
    $r30Map = Get-RequiredReceiptProperty $r30Commit 'SuccessorMap' 'R30 transaction receipt'
    $r30Runtime = Get-RequiredReceiptProperty $r30Commit 'RuntimeDllAfter' 'R30 transaction receipt'
    $r30Editor = Get-RequiredReceiptProperty $r30Commit 'EditorDllAfter' 'R30 transaction receipt'

    $r30CaptureAdmission = Read-HashPinnedDirectCommitReceipt `
        $R30CaptureCommitReceipt $ExpectedR30CaptureCommitReceiptSha256 `
        $r30EvidenceBase 'R30 Player0 capture receipt'
    $r30Capture = $r30CaptureAdmission.Receipt
    if ([string] (Get-RequiredReceiptProperty $r30Capture 'Schema' 'R30 capture receipt') -cne $r30CaptureSchema -or
        [string] (Get-RequiredReceiptProperty $r30Capture 'Status' 'R30 capture receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredReceiptProperty $r30Capture 'RunToken' 'R30 capture receipt') -cne $r30CaptureAdmission.Token -or
        [string] (Get-RequiredReceiptProperty $r30Capture 'NativeOrder' 'R30 capture receipt') -cne 'R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31' -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'R31DependencyAllowed' 'R30 capture receipt') -ne $false -or
        [int] (Get-RequiredReceiptProperty $r30Capture 'ExactPoseCount' 'R30 capture receipt') -ne 5 -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'MechanicalCaptureValidationPassed' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'ExplicitHumanReviewAcceptance' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'ConfirmedFiveImagesReviewed' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'HumanVisualReviewAttested' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'AutomaticVisualAcceptanceAllowed' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'VisualReviewRequired' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'VisualReviewAccepted' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'ProviderFallbackVisualQaAccepted' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'TreeMaterialResponseV3Reviewed' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'R31AdmissionAuthorized' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'ProviderReadyProofClaimed' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'HyperrealismClaimed' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'MapModifiedByCapture' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r30Capture 'SimulationCollisionNavigationSensorRfModified' 'R30 capture receipt') -ne $false) {
        throw 'R30 capture receipt failed the exact accepted five-pose contract.'
    }
    $r30CaptureCommit = Get-RequiredReceiptProperty $r30Capture 'R30CommitAdmission' 'R30 capture receipt'
    Test-ReceiptFileBinding $r30CaptureCommit $r30CommitAdmission 'R30 capture commit admission'
    if (-not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $r30CaptureCommit 'SuccessorMap' 'R30 capture commit admission') $r30Map.Bytes $r30Map.Sha256) -or
        -not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $r30CaptureCommit 'RuntimeDllAfter' 'R30 capture commit admission') $r30Runtime.Bytes $r30Runtime.Sha256) -or
        -not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $r30CaptureCommit 'EditorDllAfter' 'R30 capture commit admission') $r30Editor.Bytes $r30Editor.Sha256)) {
        throw 'R30 capture receipt does not cross-bind the exact R30 map and DLL states.'
    }
    $treeMaterialResponseV3 = Get-RequiredReceiptProperty `
        $r30Commit 'TreeMaterialResponseV3' 'R30 transaction receipt'
    $treeNativePins = @(Get-RequiredReceiptProperty `
        $treeMaterialResponseV3 'NativeSourcePins' 'R30 TreeRealism v3 receipt')
    $treeClosurePins = @(Get-RequiredReceiptProperty `
        $treeMaterialResponseV3 'SourceClosurePins' 'R30 TreeRealism v3 receipt')
    $treeContentBefore = @(Get-RequiredReceiptProperty `
        $treeMaterialResponseV3 'ContentBefore' 'R30 TreeRealism v3 receipt')
    $treeContentAfter = @(Get-RequiredReceiptProperty `
        $treeMaterialResponseV3 'ContentAfter' 'R30 TreeRealism v3 receipt')
    if ([string] (Get-RequiredReceiptProperty $treeMaterialResponseV3 'Status' 'R30 TreeRealism v3 receipt') -cne
            'TREE_MATERIAL_RESPONSE_V3_CONTENT_DELTA_VALID' -or
        $treeNativePins.Count -ne 4 -or $treeClosurePins.Count -ne 6 -or
        $treeContentAfter.Count -ne $treeContentBefore.Count + 13 -or
        [int] (Get-RequiredReceiptProperty $treeMaterialResponseV3 'ResponseMaterialPackageCount' 'R30 TreeRealism v3 receipt') -ne 13 -or
        [int] (Get-RequiredReceiptProperty $treeMaterialResponseV3 'ReboundManagedMeshPackageCount' 'R30 TreeRealism v3 receipt') -ne 5 -or
        [int] (Get-RequiredReceiptProperty $treeMaterialResponseV3 'RuntimeResponseMidCount' 'R30 TreeRealism v3 receipt') -ne 26 -or
        [bool] (Get-RequiredReceiptProperty $treeMaterialResponseV3 'TreePlacementGeometryOpacityWindAuthorityModified' 'R30 TreeRealism v3 receipt') -ne $false) {
        throw 'R30 TreeRealism v3 predecessor receipt drifted.'
    }
    $embeddedR30Tree = Get-RequiredReceiptProperty `
        $r30CaptureCommit 'TreeMaterialResponseV3' 'R30 capture commit admission'
    if ((ConvertTo-Json $treeMaterialResponseV3 -Depth 16 -Compress) -cne
        (ConvertTo-Json $embeddedR30Tree -Depth 16 -Compress)) {
        throw 'R30 capture does not bind the exact TreeRealism v3 transaction identity.'
    }

    $r31CommitAdmission = Read-HashPinnedDirectCommitReceipt `
        $R31CommitReceipt $ExpectedR31CommitReceiptSha256 `
        $r31TransactionBase 'R31 transaction receipt'
    $r31Commit = $r31CommitAdmission.Receipt
    if ([string] (Get-RequiredReceiptProperty $r31Commit 'Schema' 'R31 transaction receipt') -cne $r31TransactionSchema -or
        [string] (Get-RequiredReceiptProperty $r31Commit 'Status' 'R31 transaction receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredReceiptProperty $r31Commit 'RunToken' 'R31 transaction receipt') -cne $r31CommitAdmission.Token -or
        [int] (Get-RequiredReceiptProperty $r31Commit 'R31ContentPackageCount' 'R31 transaction receipt') -ne 5 -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'R30FacadeLookdevAssetsModified' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'R30FacadeCueCoexists' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'R29FacadeEnvironmentAssetsModified' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'V2BroadShellMeshRetained' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'R28PublicRealmRetained' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'R29EnvironmentAssetsModified' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'VegetationMutationAllowed' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'TreeRealismValidated' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'TreeMaterialResponseV3PromotedAtR30' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'TreeMaterialResponseV3Preserved' 'R31 transaction receipt') -ne $true -or
        [int] (Get-RequiredReceiptProperty $r31Commit 'TreeResponseMaterialPackageCount' 'R31 transaction receipt') -ne 13 -or
        [int] (Get-RequiredReceiptProperty $r31Commit 'TreeDerivativeMeshPackageCount' 'R31 transaction receipt') -ne 5 -or
        [int] (Get-RequiredReceiptProperty $r31Commit 'TreeRuntimeResponseMidCount' 'R31 transaction receipt') -ne 26 -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'TreePlacementGeometryOpacityWindAuthorityModified' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'TerrainR29Validated' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'ContextPolicyShellValidatedBeforeAndAfter' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'ContextTextureAssetsModified' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'HeroMaterialsDependencyAllowed' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'SimulationCollisionNavigationSensorRfAuthority' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'VisualCaptureAccepted' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'CaptureRevalidationRequired' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Commit 'R33SourceOrDeclarationAllowed' 'R31 transaction receipt') -ne $false) {
        throw 'R31 transaction receipt failed the exact committed pre-R33 broad-shell contract.'
    }
    Test-ReceiptFileBinding (Get-RequiredReceiptProperty $r31Commit 'R30TransactionAdmission' 'R31 transaction receipt') $r30CommitAdmission 'R31 embedded R30 transaction admission'
    Test-ReceiptFileBinding (Get-RequiredReceiptProperty $r31Commit 'R30CaptureAdmission' 'R31 transaction receipt') $r30CaptureAdmission 'R31 embedded R30 capture admission'
    if ((ConvertTo-Json (Get-RequiredReceiptProperty $r31Commit 'TreeMaterialResponseV3' 'R31 transaction receipt') -Depth 16 -Compress) -cne
        (ConvertTo-Json $treeMaterialResponseV3 -Depth 16 -Compress)) {
        throw 'R31 transaction did not preserve the exact R30 TreeRealism v3 identity.'
    }
    if (-not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $r31Commit 'PredecessorMap' 'R31 transaction receipt') $r30Map.Bytes $r30Map.Sha256) -or
        -not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $r31Commit 'RuntimeDllBefore' 'R31 transaction receipt') $r30Runtime.Bytes $r30Runtime.Sha256) -or
        -not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $r31Commit 'EditorDllBefore' 'R31 transaction receipt') $r30Editor.Bytes $r30Editor.Sha256)) {
        throw 'R31 transaction does not bind the exact R30 map/DLL predecessor states.'
    }
    $r31ContextClosure=@(Get-RequiredReceiptProperty $r31Commit 'NativeSourceClosureFiles' 'R31 transaction receipt')
    if ($r31ContextClosure.Count -ne 2) { throw 'R31 transaction must receipt exactly two pre-R33 context-policy closure files.' }
    for ($index=0; $index -lt 2; ++$index) {
        $closure=$r31ContextClosure[$index]; $pin=$retainedSourcePins[$index]
        if ([string] (Get-RequiredReceiptProperty $closure 'RepositoryRelativePath' 'R31 source closure receipt') -cne (Get-PinRelativePath $pin 'Repository') -or
            [string] (Get-RequiredReceiptProperty $closure 'NativeRelativePath' 'R31 source closure receipt') -cne (Get-PinRelativePath $pin 'Native') -or
            -not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $closure 'State' 'R31 source closure receipt') $pin.Bytes $pin.Sha256)) {
            throw "R31 transaction context-policy source closure drifted at index $index."
        }
    }
    $r31Map = Get-RequiredReceiptProperty $r31Commit 'SuccessorMap' 'R31 transaction receipt'
    $r31Runtime = Get-RequiredReceiptProperty $r31Commit 'RuntimeDllAfter' 'R31 transaction receipt'
    $r31Editor = Get-RequiredReceiptProperty $r31Commit 'EditorDllAfter' 'R31 transaction receipt'

    $r31CaptureAdmission = Read-HashPinnedDirectCommitReceipt `
        $R31CaptureCommitReceipt $ExpectedR31CaptureCommitReceiptSha256 `
        $r31EvidenceBase 'R31 Player0 capture receipt'
    $r31Capture = $r31CaptureAdmission.Receipt
    if ([string] (Get-RequiredReceiptProperty $r31Capture 'Schema' 'R31 capture receipt') -cne $r31CaptureSchema -or
        [string] (Get-RequiredReceiptProperty $r31Capture 'Status' 'R31 capture receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredReceiptProperty $r31Capture 'RunToken' 'R31 capture receipt') -cne $r31CaptureAdmission.Token -or
        [string] (Get-RequiredReceiptProperty $r31Capture 'NativeOrder' 'R31 capture receipt') -cne 'R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32' -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'R32DependencyAllowed' 'R31 capture receipt') -ne $false -or
        [int] (Get-RequiredReceiptProperty $r31Capture 'ExactPoseCount' 'R31 capture receipt') -ne 5 -or
        [int] (Get-RequiredReceiptProperty $r31Capture 'ExactImageCount' 'R31 capture receipt') -ne 6 -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'MechanicalCaptureValidationPassed' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'ExplicitHumanReviewAcceptance' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'ConfirmedFiveImagesReviewed' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'ConfirmedNaniteRasterPairReviewed' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'HumanVisualReviewAttested' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'HumanNaniteRasterComparisonAttested' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'NaniteRasterAppearanceParityAccepted' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'NaniteConsoleStateRestored' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'AutomaticVisualAcceptanceAllowed' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'VisualReviewRequired' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'VisualReviewAccepted' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'R31BroadShellVisualQaAccepted' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'ProviderFallbackVisualQaAccepted' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'TreeMaterialResponseV3Preserved' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'R32AdmissionAuthorized' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'ProviderReadyProofClaimed' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'ProviderReadyCaptureAccepted' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'HyperrealismClaimed' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'AirSimTriadRuntimeRetained' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'AirSimDisableArgumentsUsed' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'NativeStateMutatedByAcceptance' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'UnrealLaunchedByAcceptance' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'MapModifiedByCapture' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredReceiptProperty $r31Capture 'SimulationCollisionNavigationSensorRfModified' 'R31 capture receipt') -ne $false) {
        throw 'R31 capture receipt failed the exact accepted pre-R32 visual-QA contract.'
    }
    $r31CaptureCommit = Get-RequiredReceiptProperty $r31Capture 'R31CommitAdmission' 'R31 capture receipt'
    Test-ReceiptFileBinding $r31CaptureCommit $r31CommitAdmission 'R31 capture commit admission'
    $pendingAdmission=Get-RequiredReceiptProperty $r31Capture 'PendingCaptureAdmission' 'R31 capture receipt'
    $expectedPendingPath=[IO.Path]::GetFullPath((Join-Path (Join-Path $r31EvidenceBase $r31CaptureAdmission.Token) 'pending-visual-review.json'))
    $pendingPath=[IO.Path]::GetFullPath([string] (Get-RequiredReceiptProperty $pendingAdmission 'Path' 'R31 pending capture admission'))
    $pendingFile=Get-RequiredReceiptProperty $pendingAdmission 'File' 'R31 pending capture admission'
    if (-not $pendingPath.Equals($expectedPendingPath,[StringComparison]::OrdinalIgnoreCase) -or
        [string] (Get-RequiredReceiptProperty $pendingAdmission 'Status' 'R31 pending capture admission') -cne 'PENDING_VISUAL_REVIEW' -or
        [string] (Get-RequiredReceiptProperty $pendingAdmission 'CallerSha256' 'R31 pending capture admission') -cne [string] $pendingFile.Sha256 -or
        -not (Test-ReceiptStateMatches $pendingFile $pendingFile.Bytes $pendingFile.Sha256)) {
        throw 'R31 accepted capture is not bound to its exact hash-pinned pending visual-review receipt.'
    }
    [void] (Assert-State $pendingFile $pendingPath 'R31 pending visual-review receipt')
    $acceptanceRevalidation=Get-RequiredReceiptProperty $r31Capture 'AcceptanceRevalidation' 'R31 capture receipt'
    if ([bool] (Get-RequiredReceiptProperty $acceptanceRevalidation 'Unchanged' 'R31 acceptance revalidation') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $acceptanceRevalidation 'PendingReceiptHashPinned' 'R31 acceptance revalidation') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $acceptanceRevalidation 'SixPngsRedecodedAndRehashed' 'R31 acceptance revalidation') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $acceptanceRevalidation 'TreeMaterialResponseV3SourceAndContentClosureRevalidated' 'R31 acceptance revalidation') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $acceptanceRevalidation 'NativeReceiptWriteAllowed' 'R31 acceptance revalidation') -ne $false) {
        throw 'R31 accepted capture lacks exact non-native acceptance revalidation proof.'
    }
    foreach ($binding in @(
        [pscustomobject] @{ Name='SuccessorMap'; State=$r31Map }
        [pscustomobject] @{ Name='RuntimeDllAfter'; State=$r31Runtime }
        [pscustomobject] @{ Name='EditorDllAfter'; State=$r31Editor })) {
        $embeddedState = Get-RequiredReceiptProperty $r31CaptureCommit $binding.Name 'R31 capture commit admission'
        if (-not (Test-ReceiptStateMatches $embeddedState $binding.State.Bytes $binding.State.Sha256)) {
            throw "R31 capture commit admission does not bind $($binding.Name)."
        }
    }
    $captureMap = Get-RequiredReceiptProperty $r31Capture 'Map' 'R31 capture receipt'
    $captureRuntime = Get-RequiredReceiptProperty $r31Capture 'RuntimeEditorDll' 'R31 capture receipt'
    $captureEditor = Get-RequiredReceiptProperty $r31Capture 'EditorDll' 'R31 capture receipt'
    $groundHeader = Get-RequiredReceiptProperty $r31Capture 'GroundHeader' 'R31 capture receipt'
    $groundSource = Get-RequiredReceiptProperty $r31Capture 'GroundSource' 'R31 capture receipt'
    $nativePluginSourceTreeReceipt = Get-RequiredReceiptProperty $r31Capture 'NativePluginSourceTree' 'R31 capture receipt'
    $nativePluginSourceTree = @(Get-RequiredReceiptProperty $nativePluginSourceTreeReceipt 'BaselineAfterCaptureSourcePromotion' 'R31 native plugin source-tree receipt')
    $nativePluginSourceTreePre = @(Get-RequiredReceiptProperty $nativePluginSourceTreeReceipt 'ImmediatePreCapture' 'R31 native plugin source-tree receipt')
    $nativePluginSourceTreePost = @(Get-RequiredReceiptProperty $nativePluginSourceTreeReceipt 'ImmediatePostCapture' 'R31 native plugin source-tree receipt')
    $expectedGroundHeaderPath = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot (Get-PinRelativePath $sourcePins[0] 'Native')))
    $expectedGroundSourcePath = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot (Get-PinRelativePath $sourcePins[1] 'Native')))
    if (-not (Test-ReceiptStateMatches $r31Map $ExpectedMapBytes $ExpectedMapSha256) -or
        -not (Test-ReceiptStateMatches $r31Runtime $ExpectedRuntimeDllBytes $ExpectedRuntimeDllSha256) -or
        -not (Test-ReceiptStateMatches $r31Editor $ExpectedEditorDllBytes $ExpectedEditorDllSha256) -or
        -not (Test-ReceiptStateMatches $captureMap $ExpectedMapBytes $ExpectedMapSha256) -or
        -not (Test-ReceiptStateMatches $captureRuntime $ExpectedRuntimeDllBytes $ExpectedRuntimeDllSha256) -or
        -not (Test-ReceiptStateMatches $captureEditor $ExpectedEditorDllBytes $ExpectedEditorDllSha256) -or
        -not (Test-ReceiptStateMatches $groundHeader $ExpectedGroundHeaderBytes $ExpectedGroundHeaderSha256) -or
        -not (Test-ReceiptStateMatches $groundSource $ExpectedGroundSourceBytes $ExpectedGroundSourceSha256) -or
        -not [IO.Path]::GetFullPath([string] (Get-RequiredReceiptProperty $groundHeader 'Path' 'R31 GroundHeader receipt')).Equals($expectedGroundHeaderPath, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFullPath([string] (Get-RequiredReceiptProperty $groundSource 'Path' 'R31 GroundSource receipt')).Equals($expectedGroundSourcePath, [StringComparison]::OrdinalIgnoreCase) -or
        [bool] (Get-RequiredReceiptProperty $groundHeader 'Unchanged' 'R31 GroundHeader receipt') -ne $true -or
        [bool] (Get-RequiredReceiptProperty $groundSource 'Unchanged' 'R31 GroundSource receipt') -ne $true -or
        -not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $groundHeader 'ImmediatePreCapture' 'R31 GroundHeader receipt') $ExpectedGroundHeaderBytes $ExpectedGroundHeaderSha256) -or
        -not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $groundHeader 'ImmediatePostCapture' 'R31 GroundHeader receipt') $ExpectedGroundHeaderBytes $ExpectedGroundHeaderSha256) -or
        -not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $groundSource 'ImmediatePreCapture' 'R31 GroundSource receipt') $ExpectedGroundSourceBytes $ExpectedGroundSourceSha256) -or
        -not (Test-ReceiptStateMatches (Get-RequiredReceiptProperty $groundSource 'ImmediatePostCapture' 'R31 GroundSource receipt') $ExpectedGroundSourceBytes $ExpectedGroundSourceSha256) -or
        -not [IO.Path]::GetFullPath([string] (Get-RequiredReceiptProperty $nativePluginSourceTreeReceipt 'Root' 'R31 native plugin source-tree receipt')).Equals($nativePluginSourceRoot, [StringComparison]::OrdinalIgnoreCase) -or
        [bool] (Get-RequiredReceiptProperty $nativePluginSourceTreeReceipt 'Unchanged' 'R31 native plugin source-tree receipt') -ne $true -or
        [int] (Get-RequiredReceiptProperty $nativePluginSourceTreeReceipt 'FileCount' 'R31 native plugin source-tree receipt') -ne $nativePluginSourceTree.Count -or
        (ConvertTo-Json $nativePluginSourceTree -Depth 6 -Compress) -cne (ConvertTo-Json $nativePluginSourceTreePre -Depth 6 -Compress) -or
        (ConvertTo-Json $nativePluginSourceTree -Depth 6 -Compress) -cne (ConvertTo-Json $nativePluginSourceTreePost -Depth 6 -Compress) -or
        $nativePluginSourceTree.Count -le 0) {
        throw 'Caller map/DLL/Ground pins or whole plugin-source-tree receipt do not match the accepted R31 capture.'
    }
    $captures=@(Get-RequiredReceiptProperty $r31Capture 'Captures' 'R31 capture receipt')
    $expectedPoseIds=@('075m','020m','008m','002m','surroundings_oblique_macdonald')
    if ($captures.Count -ne 5) { throw 'R31 accepted capture must contain exactly five reviewed poses.' }
    $imageHashes=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $pixelHashes=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    for ($index=0; $index -lt $expectedPoseIds.Count; ++$index) {
        $capture=$captures[$index]
        $pose=Get-RequiredReceiptProperty $capture 'Pose' "R31 capture row $index"
        $poseId=[string] (Get-RequiredReceiptProperty $pose 'Id' "R31 capture pose $index")
        $image=Get-RequiredReceiptProperty $capture 'Image' "R31 capture row $index"
        $expectedImagePath=[IO.Path]::GetFullPath((Join-Path (Join-Path (Join-Path $r31EvidenceBase $r31CaptureAdmission.Token) 'captures') "explore_v5d_r31_player0_${poseId}_$($r31CaptureAdmission.Token).png"))
        if ($poseId -cne $expectedPoseIds[$index] -or
            [bool] (Get-RequiredReceiptProperty $capture 'ProviderFallbackVisualQa' "R31 capture row $index") -ne $true -or
            [bool] (Get-RequiredReceiptProperty $capture 'ProviderReadyProofClaimed' "R31 capture row $index") -ne $false -or
            [string] (Get-RequiredReceiptProperty $capture 'RenderPathId' "R31 capture row $index") -cne 'NANITE_ON' -or
            [int] (Get-RequiredReceiptProperty $capture 'NaniteConsoleValue' "R31 capture row $index") -ne 1 -or
            [int] (Get-RequiredReceiptProperty $capture 'NaniteProxyRenderMode' "R31 capture row $index") -ne 1 -or
            -not [IO.Path]::GetFullPath([string] (Get-RequiredReceiptProperty $image 'Path' "R31 capture image $index")).Equals($expectedImagePath,[StringComparison]::OrdinalIgnoreCase) -or
            [int] (Get-RequiredReceiptProperty $image 'WidthPixels' "R31 capture image $index") -ne 2560 -or
            [int] (Get-RequiredReceiptProperty $image 'HeightPixels' "R31 capture image $index") -ne 1440 -or
            [bool] (Get-RequiredReceiptProperty $image 'NonBlank' "R31 capture image $index") -ne $true -or
            -not (Test-ReceiptStateMatches $image $image.Bytes $image.Sha256) -or
            -not $imageHashes.Add([string] $image.Sha256) -or
            -not $pixelHashes.Add([string] (Get-RequiredReceiptProperty $image 'DecodedBgraSha256' "R31 capture image $index"))) {
            throw "R31 accepted capture row $index failed exact reviewed-image admission."
        }
        [void] (Assert-State $image $expectedImagePath "R31 accepted capture PNG $index")
    }
    [void] (Assert-R31NaniteRasterComparison `
        $r31Capture $captures $r31CaptureAdmission.Token)
    [pscustomobject] [ordered] @{
        R30Transaction=$r30CommitAdmission
        R30Capture=$r30CaptureAdmission
        R31Transaction=$r31CommitAdmission
        R31Capture=$r31CaptureAdmission
        Map=$r31Map
        RuntimeDll=$r31Runtime
        EditorDll=$r31Editor
        GroundHeader=$groundHeader
        GroundSource=$groundSource
        NativePluginSourceTree=$nativePluginSourceTree
        TreeMaterialResponseV3=$treeMaterialResponseV3
    }
}

function Get-TreeReceipt {
    param([string] $Root)
    if (-not [IO.Directory]::Exists($Root)) { return @() }
    @(
        Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath=[IO.Path]::GetRelativePath($Root, $_.FullName)
                Bytes=[int64] $_.Length
                Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
                LastWriteUtc=$_.LastWriteTimeUtc.ToString('o')
            }
        }
    )
}

function Get-TreeIdentityReceipt {
    param([string] $Root)
    @(
        Get-TreeReceipt $Root | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath=[string] $_.RelativePath
                Bytes=[int64] $_.Bytes
                Sha256=[string] $_.Sha256
            }
        }
    )
}

function Get-ExpectedR32PluginSourceTreeReceipt {
    param([object[]] $AcceptedR31Baseline)
    $sourcePrefix='Plugins\TRIADSensorFusion\Source\'
    $rows=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in @($AcceptedR31Baseline)) {
        $relative=[string] (Get-RequiredReceiptProperty $row 'RelativePath' 'accepted R31 plugin-source-tree row')
        if (-not $rows.TryAdd($relative,$row)) { throw "Duplicate accepted R31 plugin-source-tree row: $relative" }
    }
    for ($index=0; $index -lt $sourcePins.Count; ++$index) {
        $pin=$sourcePins[$index]
        $nativeRelative=Get-PinRelativePath $pin 'Native'
        if (-not $nativeRelative.StartsWith($sourcePrefix,[StringComparison]::OrdinalIgnoreCase)) {
            throw "R32 C++ promotion is outside the plugin Source tree: $nativeRelative"
        }
        $treeRelative=$nativeRelative.Substring($sourcePrefix.Length)
        $wasPresent=$rows.ContainsKey($treeRelative)
        if (($index -lt 2 -and -not $wasPresent) -or ($index -ge 2 -and $wasPresent)) {
            throw "Accepted R31 plugin-source-tree native-before semantics drifted: $treeRelative"
        }
        $path=[IO.Path]::GetFullPath((Join-Path $nativePluginSourceRoot $treeRelative))
        [void] (Assert-State ([pscustomobject] @{ Present=$true; Bytes=$pin.Bytes; Sha256=$pin.Sha256 }) $path 'promoted R32 plugin source')
        $item=Get-Item -LiteralPath $path -Force
        $rows[$treeRelative]=[pscustomobject] [ordered] @{
            RelativePath=$treeRelative; Bytes=[int64] $item.Length
            Sha256=[string] $pin.Sha256
            LastWriteUtc=$item.LastWriteTimeUtc.ToString('o')
        }
    }
    $expected=@($rows.Values | Sort-Object RelativePath)
    if ($expected.Count -ne $AcceptedR31Baseline.Count + 6) {
        throw 'Expected post-R32 plugin Source tree must replace two rows and add exactly six rows.'
    }
    @($expected)
}

function Assert-TreeReceipt {
    param([string] $Root, [object[]] $Expected, [string] $Label)
    $actual = @(Get-TreeReceipt $Root)
    if ((ConvertTo-Json @($Expected) -Depth 5 -Compress) -cne
        (ConvertTo-Json @($actual) -Depth 5 -Compress)) {
        throw "$Label tree receipt changed: $Root"
    }
}

function Assert-NativePreState {
    param([object[]] $Pins)
    foreach ($pin in $Pins) {
        $relative = Get-PinRelativePath $pin 'Native'
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) { throw "Native prestate escaped project: $path" }
        if ([bool] $pin.NativeBeforePresent) {
            $bytes = if ($null -ne $pin.PSObject.Properties['NativeBeforeBytes']) { [int64] $pin.NativeBeforeBytes } else { [int64] $pin.Bytes }
            $hash = if ($null -ne $pin.PSObject.Properties['NativeBeforeSha256']) { [string] $pin.NativeBeforeSha256 } else { [string] $pin.Sha256 }
            if ($bytes -le 0 -or $hash -notmatch '^[A-F0-9]{64}$') { throw "Existing R32 target lacks explicit native-before receipt: $path" }
            [void] (Assert-State ([pscustomobject] @{ Present=$true; Bytes=$bytes; Sha256=$hash }) $path 'native source preimage')
        }
        elseif ([IO.File]::Exists($path)) {
            throw "Initially absent R32 promotion target already exists: $path"
        }
    }
}

function Assert-NoR33NativeState {
    foreach ($relative in $r33ForbiddenNativePins) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
        if ([IO.File]::Exists($path)) { throw "R33 source is forbidden during R32: $path" }
    }
    foreach ($pin in $sourcePins[0..1]) {
        $path = Join-Path $nativeProjectRoot (Get-PinRelativePath $pin 'Native')
        $text = Get-Content -LiteralPath $path -Raw
        foreach ($symbol in $r33ForbiddenSymbols) {
            if ($text.Contains($symbol, [StringComparison]::Ordinal)) {
                throw "R33 symbol contaminated R32 Ground compile closure: $symbol"
            }
        }
    }
}

function New-FileJournal {
    param([string[]] $Paths, [string] $BackupRoot)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) { throw "Journal target escaped native project: $path" }
        $state = Get-FileState $path
        $relative = [IO.Path]::GetRelativePath($nativeProjectRoot, $path)
        $backup = [IO.Path]::GetFullPath((Join-Path $BackupRoot $relative))
        if ($state.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup))
            Copy-Item -LiteralPath $path -Destination $backup
            [void] (Assert-State $state $backup 'file journal backup')
        }
        $rows.Add([pscustomobject] [ordered] @{
            Path=$path; RelativePath=$relative; Before=$state
            Backup=if ($state.Present) { $backup } else { $null }
        })
    }
    @($rows)
}

function Restore-FileJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows)) {
        if ([bool] $row.Before.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($row.Path))
            Copy-Item -LiteralPath $row.Backup -Destination $row.Path -Force
            [void] (Assert-State $row.Before $row.Path 'restored journal file')
        }
        elseif ([IO.File]::Exists($row.Path)) { Remove-Item -LiteralPath $row.Path -Force }
    }
}

function New-DirectoryPresenceJournal {
    param([string[]] $FilePaths)
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($filePath in @($FilePaths | Sort-Object -Unique)) {
        $directory = [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($filePath))
        while ((Test-ContainedPath $directory $nativeProjectRoot) -and
            -not $directory.Equals($nativeProjectRoot, [StringComparison]::OrdinalIgnoreCase)) {
            if ($seen.Add($directory)) {
                $rows.Add([pscustomobject] [ordered] @{ Path=$directory; BeforePresent=[IO.Directory]::Exists($directory) })
            }
            $directory = [IO.Path]::GetDirectoryName($directory)
        }
    }
    @($rows)
}

function Restore-DirectoryPresenceJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows | Sort-Object { ([string] $_.Path).Length } -Descending)) {
        $path = [IO.Path]::GetFullPath([string] $row.Path)
        if (-not (Test-ContainedPath $path $nativeProjectRoot) -or
            $path.Equals($nativeProjectRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Directory rollback target escaped/equalled native root: $path"
        }
        if ([bool] $row.BeforePresent) {
            if (-not [IO.Directory]::Exists($path)) { throw "Pre-existing directory disappeared: $path" }
        }
        elseif ([IO.Directory]::Exists($path)) {
            if (@(Get-ChildItem -LiteralPath $path -Force).Count -ne 0) { throw "Initially absent directory is non-empty: $path" }
            [IO.Directory]::Delete($path, $false)
        }
    }
}

function New-TreeJournal {
    param([string[]] $Roots, [string] $BackupRoot)
    $journals = [Collections.Generic.List[object]]::new()
    foreach ($root in $Roots) {
        if (-not (Test-ContainedPath $root $nativeProjectRoot)) { throw "Tree journal escaped native project: $root" }
        $directories = if ([IO.Directory]::Exists($root)) {
            @(Get-ChildItem -LiteralPath $root -Directory -Recurse | ForEach-Object FullName) + @($root)
        } else { @() }
        $files = if ([IO.Directory]::Exists($root)) {
            @(Get-ChildItem -LiteralPath $root -File -Recurse | ForEach-Object FullName)
        } else { @() }
        $journals.Add([pscustomobject] [ordered] @{
            Root=$root; Directories=@($directories | Sort-Object -Unique)
            Files=@(New-FileJournal $files $BackupRoot)
        })
    }
    @($journals)
}

function Restore-TreeJournal {
    param([object[]] $Journals)
    foreach ($journal in @($Journals)) {
        $beforeFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($row in @($journal.Files)) { [void] $beforeFiles.Add([string] $row.Path) }
        if ([IO.Directory]::Exists($journal.Root)) {
            foreach ($file in @(Get-ChildItem -LiteralPath $journal.Root -File -Recurse)) {
                if (-not $beforeFiles.Contains($file.FullName)) { Remove-Item -LiteralPath $file.FullName -Force }
            }
        }
        Restore-FileJournal @($journal.Files)
        $beforeDirectories = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($directory in @($journal.Directories)) { [void] $beforeDirectories.Add([string] $directory) }
        if ([IO.Directory]::Exists($journal.Root)) {
            foreach ($directory in @(Get-ChildItem -LiteralPath $journal.Root -Directory -Recurse | ForEach-Object FullName | Sort-Object Length -Descending)) {
                if (-not $beforeDirectories.Contains($directory) -and @(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
                    [IO.Directory]::Delete($directory, $false)
                }
            }
        }
        if ($journal.Directories.Count -eq 0 -and [IO.Directory]::Exists($journal.Root) -and
            @(Get-ChildItem -LiteralPath $journal.Root -Force).Count -eq 0) {
            [IO.Directory]::Delete($journal.Root, $false)
        }
        foreach ($directory in @($journal.Directories)) {
            if (-not [IO.Directory]::Exists($directory)) { [void] [IO.Directory]::CreateDirectory($directory) }
        }
    }
}

function Remove-IsolatedR32Content {
    if (-not [IO.Directory]::Exists($r32ContentRoot)) { return }
    $nativeContentRoot=[IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content'))
    if (-not (Test-ContainedPath $r32ContentRoot $nativeContentRoot) -or
        $r32ContentRoot.Equals($nativeContentRoot,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'R32 material-content rollback root escaped or equalled native Content.'
    }
    $expected=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in $r32ContentRelativePaths) {
        [void] $expected.Add([IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative)))
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $r32ContentRoot -File -Recurse)) {
        if (-not $expected.Contains($file.FullName)) {
            throw "R32 rollback refused unexpected content file: $($file.FullName)"
        }
        Remove-Item -LiteralPath $file.FullName -Force
    }
    $directories=@(Get-ChildItem -LiteralPath $r32ContentRoot -Directory -Recurse |
        ForEach-Object FullName) + @($r32ContentRoot)
    foreach ($directory in @($directories | Sort-Object Length -Descending)) {
        if (@(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
            [IO.Directory]::Delete($directory,$false)
        }
    }
}

function Get-ProtectedProcesses {
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $_.ExecutablePath -and
            [IO.Path]::GetFullPath($_.ExecutablePath).Equals($protectedEditor, [StringComparison]::OrdinalIgnoreCase) -and
            $_.CommandLine -and $_.CommandLine.Contains($protectedProject, [StringComparison]::OrdinalIgnoreCase)
        } | Sort-Object ProcessId | ForEach-Object {
            [pscustomobject] [ordered] @{
                ProcessId=[uint32] $_.ProcessId; CreationDate=[string] $_.CreationDate
                ExecutablePath=[IO.Path]::GetFullPath($_.ExecutablePath); CommandLine=[string] $_.CommandLine
            }
        }
    )
}

function Assert-ProtectedUnchanged {
    param([object[]] $Before)
    if ((ConvertTo-Json @($Before) -Compress) -cne (ConvertTo-Json @(Get-ProtectedProcesses) -Compress)) {
        throw 'Protected UE5.4 CAPSTONE process set changed.'
    }
}

function Get-NativeMutatorProcesses {
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $isEditor = $_.ExecutablePath -and
                [IO.Path]::GetFullPath($_.ExecutablePath).StartsWith($engineRoot + '\', [StringComparison]::OrdinalIgnoreCase) -and
                $_.Name -like 'UnrealEditor*'
            $isExactProject = $_.CommandLine -and
                ($_.CommandLine.Contains($nativeProjectFile, [StringComparison]::OrdinalIgnoreCase) -or
                 $_.CommandLine.Contains($nativeProjectRoot, [StringComparison]::OrdinalIgnoreCase))
            $isBuildTool = $_.Name -in @('dotnet.exe','UnrealBuildTool.exe','UnrealHeaderTool.exe','MSBuild.exe','cl.exe','link.exe','rc.exe','ShaderCompileWorker.exe')
            $isEditor -or ($isExactProject -and $isBuildTool)
        } | Sort-Object ProcessId | ForEach-Object {
            [pscustomobject] [ordered] @{
                ProcessId=[uint32] $_.ProcessId; ParentProcessId=[uint32] $_.ParentProcessId
                CreationDate=[string] $_.CreationDate; Name=[string] $_.Name
                ExecutablePath=if ($_.ExecutablePath) { [IO.Path]::GetFullPath($_.ExecutablePath) } else { '' }
                CommandLine=[string] $_.CommandLine
            }
        }
    )
}

function Register-OwnedNativeProcessHandle {
    param([Diagnostics.Process] $Handle, [string] $Label, [string] $ExpectedExecutablePath)
    $Handle.Refresh()
    if ($Handle.HasExited) { throw "Owned native process exited before registration: $Label" }
    $record = Get-CimInstance Win32_Process -Filter "ProcessId=$($Handle.Id)" -ErrorAction Stop
    if ($null -eq $record -or -not $record.ExecutablePath -or
        -not [IO.Path]::GetFullPath($record.ExecutablePath).Equals([IO.Path]::GetFullPath($ExpectedExecutablePath), [StringComparison]::OrdinalIgnoreCase)) {
        throw "Owned native process executable identity mismatch: $Label"
    }
    $script:ownedNativeProcessHandles.Add([pscustomobject] [ordered] @{
        Handle=$Handle; ProcessId=[int] $Handle.Id
        StartUtcTicks=[int64] $Handle.StartTime.ToUniversalTime().Ticks
        CreationDate=[string] $record.CreationDate
        ExpectedExecutablePath=[IO.Path]::GetFullPath($ExpectedExecutablePath)
        CommandLine=[string] $record.CommandLine; Label=$Label
    })
}

function Assert-OwnedProcessIdentity {
    param($Owned)
    $record = Get-CimInstance Win32_Process -Filter "ProcessId=$($Owned.ProcessId)" -ErrorAction Stop
    if ($null -eq $record -or
        [string] $record.CreationDate -cne [string] $Owned.CreationDate -or
        -not $record.ExecutablePath -or
        -not [IO.Path]::GetFullPath($record.ExecutablePath).Equals($Owned.ExpectedExecutablePath, [StringComparison]::OrdinalIgnoreCase) -or
        [string] $record.CommandLine -cne [string] $Owned.CommandLine) {
        throw "Owned process identity changed: $($Owned.Label)"
    }
    $record
}

function Get-ActiveOwnedNativeProcessHandles {
    @(
        foreach ($owned in $script:ownedNativeProcessHandles) {
            $owned.Handle.Refresh()
            if (-not $owned.Handle.HasExited) { $owned }
        }
    )
}

function Assert-NativeIdle {
    param([string] $Label)
    $busy = @(Get-NativeMutatorProcesses)
    if ($busy.Count -ne 0) {
        throw "$Label refused: native mutator present: $([string]::Join('; ', @($busy | ForEach-Object { "pid=$($_.ProcessId) name=$($_.Name)" })))"
    }
}

function Wait-NativeMutationQuiescence {
    param([string] $Label, [ValidateRange(5,120)] [int] $TimeoutSeconds=60)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $busy = @(Get-NativeMutatorProcesses)
        $owned = @(Get-ActiveOwnedNativeProcessHandles)
        if ($busy.Count -eq 0 -and $owned.Count -eq 0) {
            return [pscustomobject] [ordered] @{
                Label=$Label; Status='QUIESCENT'; RegisteredOwnedProcessCount=$script:ownedNativeProcessHandles.Count
                ActiveOwnedProcessCount=0; NativeMutatorProcessCount=0
            }
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "$Label refused filesystem mutation because owned build/editor process trees are not quiescent."
}

function Assert-LaunchAdmission {
    param([string] $Label)
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    $freeBytes = [int64] $os.FreeVirtualMemory * 1024L
    if ($freeBytes -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "$Label refused by fixed 10 GiB FreeVirtualMemory gate: available=$freeBytes"
    }
    [pscustomobject] [ordered] @{ Label=$Label; MinimumBytes=$minimumSystemFreeVirtualAtLaunchBytes; ObservedBytes=$freeBytes; Status='PASS' }
}

function Assert-RcPortUnowned {
    $client = [Net.Sockets.TcpClient]::new()
    try {
        $connect = $client.ConnectAsync('127.0.0.1', 30010)
        try { $completed = $connect.Wait(250) } catch { $completed = $false }
        if ($completed -and $connect.Status -eq [Threading.Tasks.TaskStatus]::RanToCompletion -and $client.Connected) {
            throw 'Remote Control port 30010 is already owned before R32 helper launch.'
        }
    }
    finally { $client.Dispose() }
}

function Initialize-ContinuousMemoryWatchdogType {
    if ($null -ne ('Triad.R32Safety.ContinuousMemoryWatchdog' -as [type])) { return }
    $source = @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

namespace Triad.R32Safety
{
    public sealed class ContinuousMemoryWatchdogSnapshot
    {
        public bool Started { get; internal set; }
        public bool Stopped { get; internal set; }
        public long SampleCount { get; internal set; }
        public long PeakOwnedProcessTreePrivateBytes { get; internal set; }
        public ulong MinimumAvailableCommitBytes { get; internal set; }
        public string LastSampleUtc { get; internal set; }
        public string AlertKind { get; internal set; }
        public string MonitorError { get; internal set; }
        public bool ExactIdentityVerifiedAtContainment { get; internal set; }
        public bool ForceKillUsed { get; internal set; }
    }

    public sealed class ContinuousMemoryWatchdog : IDisposable
    {
        [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Auto)]
        private sealed class MemoryStatusEx
        {
            public uint Length=(uint)Marshal.SizeOf(typeof(MemoryStatusEx));
            public uint MemoryLoad; public ulong TotalPhysical; public ulong AvailablePhysical;
            public ulong TotalPageFile; public ulong AvailablePageFile; public ulong TotalVirtual;
            public ulong AvailableVirtual; public ulong AvailableExtendedVirtual;
        }
        [DllImport("kernel32.dll", CharSet=CharSet.Auto, SetLastError=true)]
        private static extern bool GlobalMemoryStatusEx([In,Out] MemoryStatusEx buffer);
        private const uint SnapshotProcesses=0x00000002;
        private static readonly IntPtr InvalidHandle=new IntPtr(-1);
        [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
        private struct ProcessEntry
        {
            public uint Size, Usage, ProcessId; public IntPtr DefaultHeapId;
            public uint ModuleId, ThreadCount, ParentProcessId; public int BasePriority;
            public uint Flags;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst=260)] public string ExecutableName;
        }
        [DllImport("kernel32.dll", SetLastError=true)]
        private static extern IntPtr CreateToolhelp32Snapshot(uint flags,uint processId);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        private static extern bool Process32FirstW(IntPtr snapshot,ref ProcessEntry entry);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        private static extern bool Process32NextW(IntPtr snapshot,ref ProcessEntry entry);
        [DllImport("kernel32.dll", SetLastError=true)]
        private static extern bool CloseHandle(IntPtr handle);

        private readonly object gate=new object();
        private readonly int processId, pollMilliseconds, persistentMilliseconds;
        private readonly long creationTicks, privateCeiling, minimumCommit;
        private readonly string executablePath;
        private readonly ManualResetEvent stop=new ManualResetEvent(false);
        private Thread thread; private bool started, stopped, identityAtContainment, forceKill;
        private long samples, peak; private ulong minCommit=ulong.MaxValue;
        private string last=String.Empty, alert=String.Empty, error=String.Empty;
        private DateTime? breachStart;

        public ContinuousMemoryWatchdog(int processId,long creationTicks,string executablePath,
            long privateCeiling,long minimumCommit,int pollMilliseconds,int persistentMilliseconds)
        {
            this.processId=processId; this.creationTicks=creationTicks;
            this.executablePath=Path.GetFullPath(executablePath);
            this.privateCeiling=privateCeiling; this.minimumCommit=minimumCommit;
            this.pollMilliseconds=pollMilliseconds; this.persistentMilliseconds=persistentMilliseconds;
        }
        public void Start()
        {
            lock(gate) { if(started) throw new InvalidOperationException("already started"); started=true; }
            thread=new Thread(Run); thread.IsBackground=true;
            thread.Name="TRIAD R32 owned-process continuous memory watchdog"; thread.Start();
        }
        private bool ExactProcess(out Process process)
        {
            process=null;
            try
            {
                Process p=Process.GetProcessById(processId); p.Refresh();
                string path=Path.GetFullPath(p.MainModule.FileName);
                if(p.HasExited || p.StartTime.ToUniversalTime().Ticks!=creationTicks ||
                    !String.Equals(path,executablePath,StringComparison.OrdinalIgnoreCase))
                { p.Dispose(); return false; }
                process=p; return true;
            }
            catch(ArgumentException) { return false; }
            catch(Exception ex) { lock(gate) if(error.Length==0) error="identity: "+ex.GetType().Name; return false; }
        }
        private static bool Descends(int id,int root,Dictionary<int,int> parents)
        {
            HashSet<int> seen=new HashSet<int>(); int current=id;
            while(current>0 && seen.Add(current))
            { if(current==root) return true; int parent; if(!parents.TryGetValue(current,out parent)) return false; current=parent; }
            return false;
        }
        private long TreePrivateBytes()
        {
            Dictionary<int,int> parents=new Dictionary<int,int>();
            IntPtr snapshot=CreateToolhelp32Snapshot(SnapshotProcesses,0);
            if(snapshot==InvalidHandle) throw new InvalidOperationException("process snapshot failed");
            try
            {
                ProcessEntry entry=new ProcessEntry(); entry.Size=(uint)Marshal.SizeOf(typeof(ProcessEntry));
                if(!Process32FirstW(snapshot,ref entry)) throw new InvalidOperationException("process enumerate failed");
                do { parents[(int)entry.ProcessId]=(int)entry.ParentProcessId; entry.Size=(uint)Marshal.SizeOf(typeof(ProcessEntry)); }
                while(Process32NextW(snapshot,ref entry));
            }
            finally { CloseHandle(snapshot); }
            long total=0;
            foreach(int id in parents.Keys)
            {
                if(!Descends(id,processId,parents)) continue;
                try { using(Process p=Process.GetProcessById(id)) { p.Refresh(); total=checked(total+p.PrivateMemorySize64); } }
                catch(ArgumentException) { }
            }
            return total;
        }
        private void Sample()
        {
            Process root; if(!ExactProcess(out root)) return;
            using(root)
            {
                MemoryStatusEx memory=new MemoryStatusEx();
                if(!GlobalMemoryStatusEx(memory)) throw new InvalidOperationException("GlobalMemoryStatusEx failed");
                long privateBytes=TreePrivateBytes(); DateTime now=DateTime.UtcNow;
                string kind=privateBytes>=privateCeiling ? "MEMORY_GUARD_OWNED_PROCESS_TREE_PRIVATE_BYTES" :
                    memory.AvailablePageFile<(ulong)minimumCommit ? "MEMORY_GUARD_SYSTEM_FREE_VIRTUAL" : String.Empty;
                lock(gate)
                {
                    samples++; if(privateBytes>peak) peak=privateBytes;
                    if(memory.AvailablePageFile<minCommit) minCommit=memory.AvailablePageFile;
                    last=now.ToString("o");
                    if(kind.Length>0) { if(alert.Length==0) alert=kind; if(!breachStart.HasValue) breachStart=now; }
                    else breachStart=null;
                }
                DateTime? began; lock(gate) began=breachStart;
                if(began.HasValue && (now-began.Value).TotalMilliseconds>=persistentMilliseconds)
                {
                    Process exact; if(ExactProcess(out exact)) using(exact)
                    { lock(gate) identityAtContainment=true; exact.Kill(true); lock(gate) forceKill=true; }
                }
            }
        }
        private void Run()
        {
            try { while(!stop.WaitOne(0)) { Sample(); if(stop.WaitOne(pollMilliseconds)) break; } }
            catch(Exception ex) { lock(gate) if(error.Length==0) error=ex.GetType().Name; }
            finally { lock(gate) stopped=true; }
        }
        public ContinuousMemoryWatchdogSnapshot GetSnapshot()
        {
            lock(gate) return new ContinuousMemoryWatchdogSnapshot {
                Started=started,Stopped=stopped,SampleCount=samples,
                PeakOwnedProcessTreePrivateBytes=peak,
                MinimumAvailableCommitBytes=minCommit==ulong.MaxValue ? 0UL : minCommit,
                LastSampleUtc=last,AlertKind=alert,MonitorError=error,
                ExactIdentityVerifiedAtContainment=identityAtContainment,ForceKillUsed=forceKill };
        }
        public void Stop() { stop.Set(); if(thread!=null && thread!=Thread.CurrentThread) thread.Join(5000); }
        public void Dispose() { Stop(); stop.Dispose(); }
    }
}
'@
    Add-Type -TypeDefinition $source -Language CSharp -ErrorAction Stop
}

function Start-ContinuousMemoryWatchdog {
    param([Diagnostics.Process] $Handle, [string] $ExpectedExecutablePath, [string] $OwnedProcessLabel)
    Initialize-ContinuousMemoryWatchdogType
    $Handle.Refresh()
    if ($Handle.HasExited) { throw "Owned $OwnedProcessLabel exited before watchdog startup." }
    $watchdog = [Triad.R32Safety.ContinuousMemoryWatchdog]::new(
        [int] $Handle.Id, [int64] $Handle.StartTime.ToUniversalTime().Ticks,
        [IO.Path]::GetFullPath($ExpectedExecutablePath), $privateMemoryCeilingBytes,
        $minimumSystemFreeVirtualBytes, $memoryWatchdogPollMilliseconds,
        $memoryWatchdogPersistentBreachMilliseconds)
    $watchdog.Start()
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        $snapshot = $watchdog.GetSnapshot()
        if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError)) { $watchdog.Dispose(); throw "MEMORY_WATCHDOG_FAILURE: $($snapshot.MonitorError)" }
        if ([int64] $snapshot.SampleCount -gt 0) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    if ([int64] $snapshot.SampleCount -le 0) { $watchdog.Dispose(); throw "MEMORY_WATCHDOG_NO_SAMPLES: $OwnedProcessLabel" }
    $watchdog
}

function Assert-ContinuousMemoryWatchdogHealthy {
    param([string] $Checkpoint)
    if ($null -eq $script:memoryWatchdog) { throw "MEMORY_WATCHDOG_MISSING: checkpoint=$Checkpoint" }
    $snapshot = $script:memoryWatchdog.GetSnapshot()
    if ([int64] $snapshot.SampleCount -le 0 -or [string]::IsNullOrWhiteSpace($snapshot.LastSampleUtc)) {
        throw "MEMORY_WATCHDOG_NO_SAMPLES: checkpoint=$Checkpoint"
    }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.MonitorError)) { throw "MEMORY_WATCHDOG_FAILURE: checkpoint=$Checkpoint detail=$($snapshot.MonitorError)" }
    if (-not [string]::IsNullOrWhiteSpace($snapshot.AlertKind)) { throw "$($snapshot.AlertKind): checkpoint=$Checkpoint" }
}

function Stop-ContinuousMemoryWatchdog {
    if ($null -eq $script:memoryWatchdog) { return $null }
    $script:memoryWatchdog.Stop()
    $snapshot = $script:memoryWatchdog.GetSnapshot()
    $script:memoryWatchdog.Dispose(); $script:memoryWatchdog=$null
    if ([int64] $snapshot.SampleCount -le 0 -or -not [string]::IsNullOrWhiteSpace($snapshot.MonitorError) -or
        -not [string]::IsNullOrWhiteSpace($snapshot.AlertKind)) {
        throw "Continuous memory watchdog did not close healthy: $($snapshot | ConvertTo-Json -Compress)"
    }
    $snapshot
}

function Close-OwnedProcess {
    param($Owned, [int] $TimeoutSeconds, [switch] $AllowContainment)
    $handle = $Owned.Handle
    if ($handle.WaitForExit($TimeoutSeconds * 1000)) { return }
    if (-not $AllowContainment) { throw "Owned process timed out: $($Owned.Label)" }
    [void] (Assert-OwnedProcessIdentity $Owned)
    # Forced containment is limited to the exact registered handle and its
    # descendants; this never targets unrelated Unreal or CAPSTONE processes.
    $Handle = $handle
    $Handle.Kill($true)
    if (-not $Handle.WaitForExit(30000)) { throw "Owned process tree did not exit: $($Owned.Label)" }
}

function Invoke-RcCall {
    param([string] $ObjectPath, [string] $FunctionName, [hashtable] $Parameters, [int] $TimeoutSeconds)
    $body = [ordered] @{
        objectPath=$ObjectPath; functionName=$FunctionName
        parameters=$Parameters; generateTransaction=$false
    } | ConvertTo-Json -Depth 8 -Compress
    Invoke-RestMethod -Method Put -Uri $rcUri -ContentType 'application/json' -Body $body -TimeoutSec $TimeoutSeconds
}

function Get-OwnedHelperIdentity {
    param([Diagnostics.Process] $Handle, [string] $Log)
    $owned = @($script:ownedNativeProcessHandles | Where-Object { $_.ProcessId -eq $Handle.Id })[-1]
    [void] (Assert-OwnedProcessIdentity $owned)
    if (-not $owned.CommandLine.Contains($mapPackage, [StringComparison]::Ordinal) -or
        -not $owned.CommandLine.Contains($Log, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Could not prove exact R32 helper map/log identity.'
    }
    [pscustomobject] [ordered] @{
        ProcessId=[int] $owned.ProcessId; CreationDate=[string] $owned.CreationDate
        ExecutablePath=$owned.ExpectedExecutablePath; CommandLine=$owned.CommandLine
    }
}

function Stop-OwnedHelper {
    param([Diagnostics.Process] $Handle, $Identity, [string] $Log)
    if ([uint32] $Handle.Id -ne [uint32] $Identity.ProcessId) { throw 'Helper handle/identity mismatch.' }
    try { [void] (Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30) } catch {}
    if (-not $Handle.WaitForExit($ShutdownTimeoutSeconds * 1000)) {
        $current = Get-OwnedHelperIdentity $Handle $Log
        if ([string] $current.CreationDate -cne [string] $Identity.CreationDate -or
            [string] $current.ExecutablePath -cne [string] $Identity.ExecutablePath -or
            [string] $current.CommandLine -cne [string] $Identity.CommandLine) {
            throw 'Timed-out helper no longer matches exact owned identity.'
        }
        $Handle.Kill($true)
        if (-not $Handle.WaitForExit(30000)) { throw 'Exact owned helper tree did not exit.' }
    }
}

function Invoke-GuardedOwnedBuild {
    param([string[]] $Arguments, [string] $StandardOutputLog, [string] $StandardErrorLog)
    Assert-NativeIdle 'before guarded owned R32 UBT build'
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Assert-LaunchAdmission 'before guarded owned R32 UBT build')
    foreach ($logPath in @($StandardOutputLog,$StandardErrorLog)) {
        if ([IO.File]::Exists($logPath)) { throw "R32 build log already exists: $logPath" }
    }
    $escapedArguments=@(
        ('"{0}"' -f $unrealBuildTool)
        $Arguments | ForEach-Object {
            if ($_ -match '[\s"]') { '"{0}"' -f $_.Replace('"','\"') } else { $_ }
        })
    $argumentLine=[string]::Join(' ', $escapedArguments)
    $handle = Start-Process -FilePath $dotnet -ArgumentList $argumentLine `
        -WorkingDirectory $engineSourceRoot -RedirectStandardOutput $StandardOutputLog `
        -RedirectStandardError $StandardErrorLog -PassThru -WindowStyle Hidden
    Register-OwnedNativeProcessHandle $handle 'R32 serial UBT build' $dotnet
    $script:memoryWatchdog = Start-ContinuousMemoryWatchdog $handle $dotnet 'R32 serial UBT build'
    $deadline = [DateTime]::UtcNow.AddSeconds($BuildTimeoutSeconds)
    $failure = $null
    try {
        while (-not $handle.WaitForExit(500)) {
            Assert-ContinuousMemoryWatchdogHealthy 'R32 UBT build'
            if ([DateTime]::UtcNow -ge $deadline) { throw 'R32 UBT build exceeded bounded timeout.' }
        }
        Assert-ContinuousMemoryWatchdogHealthy 'R32 UBT exit'
        if ($handle.ExitCode -ne 0) { throw "R32 UBT build failed with exit code $($handle.ExitCode)." }
    }
    catch { $failure=$_.Exception }
    finally {
        if (-not $handle.HasExited) {
            $owned = @($script:ownedNativeProcessHandles | Where-Object { $_.ProcessId -eq $handle.Id })[-1]
            try { Close-OwnedProcess $owned 1 -AllowContainment } catch { if ($null -eq $failure) { $failure=$_.Exception } }
        }
        try { $memory = Stop-ContinuousMemoryWatchdog } catch { if ($null -eq $failure) { $failure=$_.Exception } }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'after guarded owned R32 UBT build'
    if ($null -ne $failure) { throw $failure }
    if (-not [IO.File]::Exists($StandardOutputLog) -or (Get-Item -LiteralPath $StandardOutputLog).Length -le 0) {
        throw 'R32 build did not persist a non-empty stdout log.'
    }
    [pscustomobject] [ordered] @{
        ProcessId=[int] $handle.Id; ExitCode=[int] $handle.ExitCode
        ContinuousMemoryGuard=$memory
        StandardOutputLog=Get-FileState $StandardOutputLog
        StandardErrorLog=Get-FileState $StandardErrorLog
    }
}

function Invoke-ColdStage {
    param([string] $Stage, [string] $ObjectPath, [string] $FunctionName,
        [string] $TextProperty, [string] $ExpectedPrefix, [hashtable] $Parameters=@{})
    Assert-NativeIdle "before $Stage"
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Assert-LaunchAdmission "before helper $Stage")
    Assert-RcPortUnowned
    $logRoot = Join-Path $transactionRoot 'logs'
    [void] [IO.Directory]::CreateDirectory($logRoot)
    $log = [IO.Path]::GetFullPath((Join-Path $logRoot "$Stage.log"))
    if ([IO.File]::Exists($log)) { throw "Stage log already exists: $log" }
    $argumentLine = "`"$nativeProjectFile`" $mapPackage -d3d12 -sm6 " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-RemoteControlHttpServer -RCWebControlEnable -ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$log`""
    $handle = Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $nativeProjectRoot -PassThru -WindowStyle Hidden
    Register-OwnedNativeProcessHandle $handle "R32 helper $Stage" $editor
    $script:memoryWatchdog = Start-ContinuousMemoryWatchdog $handle $editor "R32 helper $Stage"
    $identity=$null; $response=$null; $failure=$null
    try {
        $identityDeadline=[DateTime]::UtcNow.AddSeconds(30)
        do {
            Assert-ContinuousMemoryWatchdogHealthy "$Stage identity wait"
            try { $identity=Get-OwnedHelperIdentity $handle $log } catch { $identity=$null }
            if ($null -ne $identity) { break }
            Start-Sleep -Milliseconds 500
        } while ([DateTime]::UtcNow -lt $identityDeadline)
        if ($null -eq $identity) { throw "Could not prove helper identity for $Stage" }
        $ready=$null; $readyDeadline=[DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
        do {
            Assert-ContinuousMemoryWatchdogHealthy "$Stage RC readiness"
            try { $ready=Invoke-RcCall $identityLibrary 'ValidateIstanaExploreRemoteControlProject' @{ ExpectedProjectPath=$nativeProjectRoot } 30 } catch { $ready=$null }
            if ($null -ne $ready -and $ready.ReturnValue -eq $true) { break }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        if ($null -eq $ready -or $ready.ReturnValue -ne $true) { throw "RC identity not ready for $Stage" }
        Assert-ContinuousMemoryWatchdogHealthy "$Stage before endpoint"
        $response=Invoke-RcCall $ObjectPath $FunctionName $Parameters $EditorTimeoutSeconds
        Assert-ContinuousMemoryWatchdogHealthy "$Stage after endpoint"
        $text=[string] $response.$TextProperty
        if ($response.ReturnValue -ne $true -or -not $text.StartsWith($ExpectedPrefix, [StringComparison]::Ordinal)) {
            throw "Stage $Stage failed exact response gate: $text"
        }
    }
    catch { $failure=$_.Exception }
    finally {
        if ($null -ne $identity -and -not $handle.HasExited) {
            try { Stop-OwnedHelper $handle $identity $log } catch { if ($null -eq $failure) { $failure=$_.Exception } }
        }
        elseif (-not $handle.HasExited) {
            $owned=@($script:ownedNativeProcessHandles | Where-Object { $_.ProcessId -eq $handle.Id })[-1]
            try { Close-OwnedProcess $owned 1 -AllowContainment } catch { if ($null -eq $failure) { $failure=$_.Exception } }
        }
        try { $memory=Stop-ContinuousMemoryWatchdog } catch { if ($null -eq $failure) { $failure=$_.Exception } }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle "after $Stage"
    if ($null -ne $failure) { throw $failure }
    if (-not [IO.File]::Exists($log) -or (Get-Item -LiteralPath $log).Length -le 0) { throw "Stage $Stage lacks log." }
    [pscustomobject] [ordered] @{
        Stage=$Stage; Function=$FunctionName; ExpectedPrefix=$ExpectedPrefix
        Message=[string] $response.$TextProperty; ProcessIdentity=$identity
        ContinuousMemoryGuard=$memory; Log=Get-FileState $log
    }
}

function Test-BinaryContainsMarker {
    param([string] $Path, [string] $Marker)
    $bytes=[IO.File]::ReadAllBytes($Path)
    $text=[Text.Encoding]::ASCII.GetString($bytes)+[Text.Encoding]::Unicode.GetString($bytes)
    $text.Contains($Marker, [StringComparison]::Ordinal)
}

function Assert-BuiltR32BinaryClosure {
    foreach ($marker in @('ConfigureR32MediumDistanceTurf','ValidateR32MediumDistanceTurf','GetMediumDistanceTurfSourceProfilesR32')) {
        if (-not (Test-BinaryContainsMarker $runtimeDll $marker)) { throw "Runtime DLL lacks R32 marker: $marker" }
    }
    foreach ($marker in @('EnsureR32MediumDistanceTurfMaterials','ValidateR32MediumDistanceTurfMaterials',('CommitR32MediumDistanceTurf' + 'ToLoadedV5DHybridMap'),'ValidateR32MediumDistanceTurfInLoadedV5DHybridMap')) {
        if (-not (Test-BinaryContainsMarker $editorDll $marker)) { throw "Editor DLL lacks R32 marker: $marker" }
    }
    foreach ($marker in @('R33CesiumWorldTerrainReferenceActor','RegisterR33CesiumWorldTerrainController')) {
        if ((Test-BinaryContainsMarker $runtimeDll $marker) -or (Test-BinaryContainsMarker $editorDll $marker)) {
            throw "R33 marker contaminated R32 build: $marker"
        }
    }
}

function Assert-StaticContract {
    if ($sourcePins.Count -ne 8 -or $sourceAssetPins.Count -ne 1 -or
        $retainedSourcePins.Count -ne 11 -or $r32ContentRelativePaths.Count -ne 4) {
        throw 'R32 native closure must be exactly 8 C++ + 1 contract promotion, 11 retained compile surfaces, and 4 isolated material packages.'
    }
    $seenRepository=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $seenNative=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in @($sourcePins)+@($sourceAssetPins)+@($retainedSourcePins)) {
        $repositoryRelative=Get-PinRelativePath $pin 'Repository'
        $nativeRelative=Get-PinRelativePath $pin 'Native'
        Assert-CanonicalRelativePath $repositoryRelative 'repository'
        Assert-CanonicalRelativePath $nativeRelative 'native'
        if (-not $seenRepository.Add($repositoryRelative)) { throw "Duplicate repository closure path: $repositoryRelative" }
        if (-not $seenNative.Add($nativeRelative)) { throw "Duplicate native closure path: $nativeRelative" }
    }
    foreach ($mapping in @(
        [pscustomobject] @{ Pin=$sourcePins[0]; Repository='SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R32MediumDistanceTurf\NativeSourceClosure\TRIADIstanaExploreV5DGroundVegetationActor.h'; Native='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DGroundVegetationActor.h' }
        [pscustomobject] @{ Pin=$sourcePins[1]; Repository='SourceAssets\IstanaPublicViewExploreV5D\Vegetation\R32MediumDistanceTurf\NativeSourceClosure\TRIADIstanaExploreV5DGroundVegetationActor.cpp'; Native='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DGroundVegetationActor.cpp' })) {
        if ((Get-PinRelativePath $mapping.Pin 'Repository') -cne $mapping.Repository -or
            (Get-PinRelativePath $mapping.Pin 'Native') -cne $mapping.Native) {
            throw 'R32 Ground overlay mapping drifted from normal native destinations.'
        }
    }
    if (@($sourcePins | Where-Object { $_.NativeBeforePresent }).Count -ne 2 -or
        @($sourcePins | Where-Object { -not $_.NativeBeforePresent }).Count -ne 6 -or
        @($sourceAssetPins | Where-Object { -not $_.NativeBeforePresent }).Count -ne 1) {
        throw 'R32 native-before semantics must replace two caller-pinned Ground files and create six C++ plus one contract.'
    }
    Assert-Pins $sourcePins $repositoryUnrealRoot
    Assert-Pins $sourceAssetPins $repositoryUnrealRoot
    Assert-Pins $retainedSourcePins $repositoryUnrealRoot
    $seenContent=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in $r32ContentRelativePaths) {
        Assert-CanonicalRelativePath $relative 'R32 material package'
        $path=[IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
        if (-not (Test-ContainedPath $path $r32ContentRoot) -or
            -not $path.EndsWith('.uasset',[StringComparison]::OrdinalIgnoreCase) -or
            -not $seenContent.Add($path)) {
            throw "Invalid or duplicate R32 material package path: $relative"
        }
    }

    $groundHeader=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[0] 'Repository')) -Raw
    $groundSource=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[1] 'Repository')) -Raw
    foreach ($text in @($groundHeader,$groundSource)) {
        if (-not $text.Contains('GetMediumDistanceTurfSourceProfilesR32', [StringComparison]::Ordinal)) {
            throw 'Immutable R32 Ground overlay lacks the copy-only R32 handoff.'
        }
        foreach ($symbol in $r33ForbiddenSymbols) {
            if ($text.Contains($symbol, [StringComparison]::Ordinal)) { throw "R33 contaminated immutable R32 Ground overlay: $symbol" }
        }
    }
    $actorSource=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[3] 'Repository')) -Raw
    $factorySource=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[5] 'Repository')) -Raw
    $editorSource=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourcePins[7] 'Repository')) -Raw
    foreach ($marker in @('selectedTransforms=4608','ownedHisms=12','collision=false','navigation=false','visualCaptureAccepted=false')) {
        if (-not ($actorSource.Contains($marker, [StringComparison]::Ordinal) -or $editorSource.Contains($marker, [StringComparison]::Ordinal))) {
            throw "R32 source closure lacks truthful marker: $marker"
        }
    }
    foreach ($marker in @(('CommitR32MediumDistanceTurf' + 'ToLoadedV5DHybridMap'),'ValidateR32MediumDistanceTurfInLoadedV5DHybridMap','rollbackOwnedByWrapper=true')) {
        if (-not $editorSource.Contains($marker, [StringComparison]::Ordinal)) { throw "R32 editor closure lacks marker: $marker" }
    }
    foreach ($marker in @('R32_20M_65M_CONTINUOUS_TURF_ROUGHNESS','continuousBroadScaleMeters=11.0','continuousMesoScaleMeters=3.4','opacityModified=false','wpoModified=false','pbrTruthClaimed=false')) {
        if (-not $factorySource.Contains($marker,[StringComparison]::Ordinal)) { throw "R32 material factory lacks bounded refinement marker: $marker" }
    }
    foreach ($endpoint in @('EnsureR32MediumDistanceTurfMaterials','ValidateR32MediumDistanceTurfMaterials')) {
        if (-not $editorSource.Contains($endpoint,[StringComparison]::Ordinal)) { throw "R32 editor closure lacks material endpoint: $endpoint" }
    }
    $contract=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot (Get-PinRelativePath $sourceAssetPins[0] 'Repository')) -Raw | ConvertFrom-Json -Depth 32
    if ([string] $contract.schema -cne 'triad.istana_explore_v5d.r32_medium_distance_turf.v1' -or
        [bool] $contract.deliveryState.nativeExecutionWrapperImplemented -ne $true -or
        [bool] $contract.deliveryState.nativeExecutionEligibleNow -ne $false -or
        [bool] $contract.nativeTransaction.implemented -ne $true -or
        [bool] $contract.nativeTransaction.executed -ne $false -or
        [int] $contract.nativeTransaction.promotionCodeFileCount -ne 8 -or
        [int] $contract.nativeTransaction.newR32MaterialPackageCount -ne 4 -or
        [bool] $contract.nativeTransaction.r32MaterialContentTreeReceiptRequired -ne $true -or
        [int] $contract.nativeTransaction.promotionContractFileCount -ne 1 -or
        [int] $contract.nativeTransaction.commitEndpointInvocationCount -ne 1 -or
        [int] $contract.nativeTransaction.standaloneApplyEndpointInvocationCount -ne 0 -or
        [bool] $contract.nativeTransaction.wholePluginSourceTreeBoundBeforeAndAfter -ne $true -or
        [bool] $contract.preR33GroundVegetationCompileOverlay.exactNativeR29PreR33BaseReceiptRequired -ne $true -or
        [bool] $contract.preR33GroundVegetationCompileOverlay.repositoryCanDeriveExactNativeBaseWithoutReceipt -ne $false -or
        [bool] $contract.nativeOrdering.nativeTransactionImplemented -ne $true -or
        [bool] $contract.nativeOrdering.r31PreR33ContextPolicyWrapperSourceReclosed -ne $true -or
        [bool] $contract.nativeOrdering.r31CommittedAndCapturedPredecessorAvailable -ne $false -or
        [bool] $contract.nativeOrdering.nativeTransactionExecuted -ne $false) {
        throw 'R32 contract truth fields drifted from the source-only guarded wrapper.'
    }
    [pscustomobject] [ordered] @{
        Schema=$schema; Status='STATIC_SELF_CHECK_PASS'
        CodeSourceCount=8; SourceContractCount=1; RetainedCompileSurfaceCount=11
        GroundOverlayFileCount=2; NewR32CodeFileCount=6
        NewR32MaterialPackageCount=4
        PredecessorReceiptParserCount=4; R33SourceOrDeclarationAllowed=$false
        FixedLaunchFreeVirtualBytes=10737418240L
        FixedContinuousFreeVirtualBytes=6442450944L
        FixedPrivateMemoryCeilingBytes=12884901888L
        MemoryWatchdogPollMilliseconds=500
        MemoryWatchdogPersistentBreachMilliseconds=2000
        SerialBuild=$true; ExactCommitR32InvocationCount=1
        StandaloneApplyR32InvocationCount=0
        NativeProjectAccessed=$false; NativeTreeWritten=$false; UnrealLaunched=$false
        NativeExecutionEligibleNow=$false
    }
}

$staticReceipt=Assert-StaticContract
if ($StaticSelfCheck -and $Execute) { throw '-StaticSelfCheck and -Execute are mutually exclusive.' }
if ($StaticSelfCheck) { $staticReceipt | ConvertTo-Json -Depth 12; exit 0 }
if (-not $Execute) {
    [pscustomobject] [ordered] @{
        Schema=$schema; Status='READ_ONLY_REPOSITORY_PREFLIGHT_PASS'
        ExecuteRequiredForNativeAccess=$true
        NativeProjectAccessed=$false; NativeTreeWritten=$false; UnrealLaunched=$false
        StaticReceipt=$staticReceipt
    } | ConvertTo-Json -Depth 12
    exit 0
}

foreach ($required in @(
    'RequireR31Predecessor','R30CommitReceipt','ExpectedR30CommitReceiptSha256',
    'R30CaptureCommitReceipt','ExpectedR30CaptureCommitReceiptSha256',
    'R31CommitReceipt','ExpectedR31CommitReceiptSha256',
    'R31CaptureCommitReceipt','ExpectedR31CaptureCommitReceiptSha256',
    'ExpectedMapBytes','ExpectedMapSha256','ExpectedRuntimeDllBytes','ExpectedRuntimeDllSha256',
    'ExpectedEditorDllBytes','ExpectedEditorDllSha256','ExpectedGroundHeaderBytes','ExpectedGroundHeaderSha256',
    'ExpectedGroundSourceBytes','ExpectedGroundSourceSha256')) {
    if (-not $PSBoundParameters.ContainsKey($required)) { throw "Live R32 transaction requires explicit parameter: $required" }
}
if (-not $RequireR31Predecessor) { throw 'Live R32 transaction requires -RequireR31Predecessor.' }
foreach ($bytes in @($ExpectedMapBytes,$ExpectedRuntimeDllBytes,$ExpectedEditorDllBytes,$ExpectedGroundHeaderBytes,$ExpectedGroundSourceBytes)) {
    if ([int64] $bytes -le 0) { throw 'All live preimage byte counts must be positive.' }
}
foreach ($hash in @($ExpectedMapSha256,$ExpectedRuntimeDllSha256,$ExpectedEditorDllSha256,$ExpectedGroundHeaderSha256,$ExpectedGroundSourceSha256)) {
    if ($hash -notmatch '^[A-Fa-f0-9]{64}$') { throw 'All live preimage SHA-256 values must be explicit.' }
}
if (-not (Test-ContainedPath $transactionRoot $transactionBase) -or
    $transactionRoot.Equals($transactionBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'R32 transaction root is not a direct bounded token child.'
}

$transactionStarted=$false
$committed=$false
$failure=$null
$sourceJournal=@(); $sourceDirectoryJournal=@(); $mapJournal=@(); $buildJournal=@()
$expectedR32PluginSourceTree=$null
$immutableBefore=$null; $admission=$null; $r32ContentReceipt=@()
try {
    if ([IO.Directory]::Exists($transactionRoot)) { throw "R32 transaction token already exists: $transactionRoot" }
    if ([IO.Directory]::Exists($r32ContentRoot)) { throw "Initially absent R32 material-content root already exists: $r32ContentRoot" }
    foreach ($requiredFile in @($nativeProjectFile,$dotnet,$unrealBuildTool,$editor,$mapFile,$runtimeDll,$editorDll)) {
        if (-not [IO.File]::Exists($requiredFile)) { throw "Required live R32 input is absent: $requiredFile" }
    }
    $script:protectedBefore=@(Get-ProtectedProcesses)
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before R32 native preflight'
    Assert-RcPortUnowned
    $prewriteAdmission=Assert-LaunchAdmission 'before any R32 native write'
    $admission=Assert-FourPredecessorReceipts
    foreach ($pin in @($admission.TreeMaterialResponseV3.NativeSourcePins) +
                     @($admission.TreeMaterialResponseV3.SourceClosurePins)) {
        $relative=[string] (Get-RequiredReceiptProperty $pin 'RelativePath' 'R32 TreeRealism v3 predecessor pin')
        if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains('..')) {
            throw "R32 TreeRealism v3 predecessor has a noncanonical pin: $relative"
        }
        $path=[IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "R32 TreeRealism v3 predecessor pin escaped native root: $path"
        }
        [void] (Assert-State ([pscustomobject] @{
            Present=$true
            Bytes=[int64] (Get-RequiredReceiptProperty $pin 'Bytes' 'R32 TreeRealism v3 predecessor pin')
            Sha256=[string] (Get-RequiredReceiptProperty $pin 'Sha256' 'R32 TreeRealism v3 predecessor pin')
        }) $path 'R32 retained TreeRealism v3 source pin')
    }
    if ((ConvertTo-Json @($admission.TreeMaterialResponseV3.ContentAfter) -Depth 8 -Compress) -cne
        (ConvertTo-Json @(Get-TreeIdentityReceipt $treeRealismContentRoot) -Depth 8 -Compress)) {
        throw 'R32 native TreeRealism content does not match the exact promoted R30 v3 receipt.'
    }

    $expectedMap=[pscustomobject] [ordered] @{ Present=$true; Bytes=$ExpectedMapBytes; Sha256=$ExpectedMapSha256.ToUpperInvariant() }
    $expectedRuntime=[pscustomobject] [ordered] @{ Present=$true; Bytes=$ExpectedRuntimeDllBytes; Sha256=$ExpectedRuntimeDllSha256.ToUpperInvariant() }
    $expectedEditor=[pscustomobject] [ordered] @{ Present=$true; Bytes=$ExpectedEditorDllBytes; Sha256=$ExpectedEditorDllSha256.ToUpperInvariant() }
    [void] (Assert-State $expectedMap $mapFile 'caller-pinned R31 map preimage')
    [void] (Assert-State $expectedRuntime $runtimeDll 'caller-pinned R31 runtime DLL preimage')
    [void] (Assert-State $expectedEditor $editorDll 'caller-pinned R31 editor DLL preimage')
    Assert-NativePreState $sourcePins
    Assert-NativePreState $sourceAssetPins
    Assert-NativePreState $retainedSourcePins
    Assert-TreeReceipt $nativePluginSourceRoot $admission.NativePluginSourceTree 'R31-captured native plugin source'
    foreach ($pin in $sourcePins[0..1]) {
        $nativeGround=Get-Content -LiteralPath (Join-Path $nativeProjectRoot (Get-PinRelativePath $pin 'Native')) -Raw
        if ($nativeGround.Contains('GetMediumDistanceTurfSourceProfilesR32', [StringComparison]::Ordinal)) {
            throw 'Caller-pinned R31 Ground preimage already contains the R32 source handoff.'
        }
        foreach ($symbol in $r33ForbiddenSymbols) {
            if ($nativeGround.Contains($symbol, [StringComparison]::Ordinal)) { throw "R31 Ground preimage contains forbidden R33 symbol: $symbol" }
        }
    }
    foreach ($relative in $r33ForbiddenNativePins) {
        if ([IO.File]::Exists((Join-Path $nativeProjectRoot $relative))) { throw "R33 actor source exists before R32: $relative" }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'immediately before R32 journal creation'

    [void] [IO.Directory]::CreateDirectory($transactionRoot)
    $transactionStarted=$true
    $journalRoot=Join-Path $transactionRoot 'journal'
    [void] [IO.Directory]::CreateDirectory($journalRoot)
    $sourceDestinations=@(
        @($sourcePins)+@($sourceAssetPins) | ForEach-Object {
            [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot (Get-PinRelativePath $_ 'Native')))
        })
    $sourceJournal=@(New-FileJournal $sourceDestinations (Join-Path $journalRoot 'source'))
    $sourceDirectoryJournal=@(New-DirectoryPresenceJournal $sourceDestinations)
    $mapJournal=@(New-FileJournal @($mapFile) (Join-Path $journalRoot 'map'))
    $buildJournal=@(New-TreeJournal @($pluginBinaryRoot,$pluginIntermediateRoot) (Join-Path $journalRoot 'build'))
    $immutableBefore=[ordered] @{
        R31Content=@(Get-TreeReceipt $r31ContentRoot)
        R29Vegetation=@(Get-TreeReceipt $r29VegetationContentRoot)
        TreeRealism=@(Get-TreeReceipt $treeRealismContentRoot)
    }
    if ($immutableBefore.R31Content.Count -le 0 -or
        $immutableBefore.R29Vegetation.Count -ne 7 -or
        $immutableBefore.TreeRealism.Count -le 0) {
        throw 'Exact retained R31, seven-package R29 vegetation, and promoted TreeRealism roots are required.'
    }
    $backupMap=[IO.Path]::GetFullPath((Join-Path $transactionRoot 'Istana_PublicView_Explore_v5d_hybrid.r31-predecessor.umap'))
    Copy-Item -LiteralPath $mapFile -Destination $backupMap
    [void] (Assert-State $expectedMap $backupMap 'external byte-identical R31 map backup')

    foreach ($pin in @($sourcePins)+@($sourceAssetPins)) {
        $source=[IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot (Get-PinRelativePath $pin 'Repository')))
        $destination=[IO.Path]::GetFullPath((Join-Path $nativeProjectRoot (Get-PinRelativePath $pin 'Native')))
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        Copy-Item -LiteralPath $source -Destination $destination
    }
    Assert-Pins $sourcePins $nativeProjectRoot -NativeAfter
    Assert-Pins $sourceAssetPins $nativeProjectRoot -NativeAfter
    Assert-Pins $retainedSourcePins $nativeProjectRoot -NativeAfter
    Assert-NoR33NativeState
    $expectedR32PluginSourceTree=@(Get-ExpectedR32PluginSourceTreeReceipt $admission.NativePluginSourceTree)
    Assert-TreeReceipt $nativePluginSourceRoot $expectedR32PluginSourceTree 'derived exact post-R32 native plugin source'
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before R32 build'

    $buildAdmission=Assert-LaunchAdmission 'before forced serial R32 build'
    $buildArguments=@(
        'UnrealEditor','Win64','Development',"-Project=$nativeProjectFile",'-WaitMutex','-NoHotReloadFromIDE',
        '-Module=TRIADSensorFusion','-Module=TRIADSensorFusionEditor','-ForceHeaderGeneration','-NoUBTMakefiles',
        '-MaxParallelActions=1','-NoUBA','-NoUBALocal')
    $buildGuard=Invoke-GuardedOwnedBuild $buildArguments `
        (Join-Path $transactionRoot 'build.stdout.log') `
        (Join-Path $transactionRoot 'build.stderr.log')
    $runtimeAfter=Get-FileState $runtimeDll
    $editorAfter=Get-FileState $editorDll
    if (-not $runtimeAfter.Present -or -not $editorAfter.Present -or
        $runtimeAfter.Sha256 -ceq $expectedRuntime.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedEditor.Sha256) {
        throw 'Forced R32 build did not produce fresh runtime and editor DLL receipts.'
    }
    Assert-BuiltR32BinaryClosure

    $stageResults=[Collections.Generic.List[object]]::new()
    $stageResults.Add((Invoke-ColdStage `
        '01_cold_validate_exact_captured_r31_predecessor' $r31Library `
        'ValidateR31BroadShellInLoadedV5DHybridMap' 'OutReport' `
        'ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_MAP_VALID'))
    [void] (Assert-State $expectedMap $mapFile 'R31 map after cold predecessor validation')
    $stageResults.Add((Invoke-ColdStage `
        '02_ensure_r32_medium_distance_turf_materials' $r32Library `
        'EnsureR32MediumDistanceTurfMaterials' 'OutMessage' `
        'EXPLORE_V5D_R32_TURF_MATERIAL_BUILD_PASS'))
    foreach ($relative in $r32ContentRelativePaths) {
        $state=Get-FileState ([IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative)))
        if (-not $state.Present -or $state.Bytes -le 0 -or
            $state.Sha256 -notmatch '^[A-F0-9]{64}$') {
            throw "R32 material package missing after ensure stage: $relative"
        }
    }
    if (@(Get-ChildItem -LiteralPath $r32ContentRoot -File -Recurse).Count -ne 4) {
        throw 'R32 material-content root is not the exact four-file namespace.'
    }
    $r32ContentReceipt=@(Get-TreeReceipt $r32ContentRoot)
    if ($r32ContentReceipt.Count -ne 4) {
        throw 'R32 material-content receipt is not exactly four packages.'
    }
    $stageResults.Add((Invoke-ColdStage `
        '03_cold_validate_r32_medium_distance_turf_materials' $r32Library `
        'ValidateR32MediumDistanceTurfMaterials' 'OutReport' `
        'ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_MATERIALS_VALID'))
    Assert-TreeReceipt $r32ContentRoot $r32ContentReceipt 'R32 material packages after cold validation'
    $stageResults.Add((Invoke-ColdStage `
        '04_commit_r32_medium_distance_turf_exactly_once' $r32Library `
        'CommitR32MediumDistanceTurfToLoadedV5DHybridMap' 'OutReport' `
        'ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_COMMIT_PASS' @{
            ExpectedPredecessorBytes=[int64] $ExpectedMapBytes
            ExpectedPredecessorSha256=$ExpectedMapSha256.ToUpperInvariant()
            VerifiedExternalBackupFilename=$backupMap
        }))
    $stageResults.Add((Invoke-ColdStage `
        '05_cold_validate_r32_medium_distance_turf_successor' $r32Library `
        'ValidateR32MediumDistanceTurfInLoadedV5DHybridMap' 'OutReport' `
        'ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_MAP_VALID'))

    $successQuiescence=Wait-NativeMutationQuiescence 'before R32 commit receipt' 60
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-TreeReceipt $r31ContentRoot $immutableBefore.R31Content 'R31 broad-shell content'
    Assert-TreeReceipt $r29VegetationContentRoot $immutableBefore.R29Vegetation 'R29 vegetation packages'
    Assert-TreeReceipt $treeRealismContentRoot $immutableBefore.TreeRealism 'R30 TreeRealism v3 packages'
    if ((ConvertTo-Json @($admission.TreeMaterialResponseV3.ContentAfter) -Depth 8 -Compress) -cne
        (ConvertTo-Json @(Get-TreeIdentityReceipt $treeRealismContentRoot) -Depth 8 -Compress)) {
        throw 'R32 postflight no longer matches the exact promoted R30 TreeRealism v3 receipt.'
    }
    Assert-TreeReceipt $r32ContentRoot $r32ContentReceipt 'R32 isolated material packages'
    Assert-Pins $sourcePins $nativeProjectRoot -NativeAfter
    Assert-Pins $sourceAssetPins $nativeProjectRoot -NativeAfter
    Assert-Pins $retainedSourcePins $nativeProjectRoot -NativeAfter
    Assert-NoR33NativeState
    Assert-TreeReceipt $nativePluginSourceRoot $expectedR32PluginSourceTree 'postflight exact R32 native plugin source'
    [void] (Assert-State $admission.R30Transaction.State $admission.R30Transaction.Path 'R30 transaction receipt postflight')
    [void] (Assert-State $admission.R30Capture.State $admission.R30Capture.Path 'R30 capture receipt postflight')
    [void] (Assert-State $admission.R31Transaction.State $admission.R31Transaction.Path 'R31 transaction receipt postflight')
    [void] (Assert-State $admission.R31Capture.State $admission.R31Capture.Path 'R31 capture receipt postflight')
    [void] (Assert-State $expectedMap $backupMap 'preserved external R31 map backup')
    $successorMap=Get-FileState $mapFile
    if (-not $successorMap.Present -or $successorMap.Bytes -le 0 -or
        ($successorMap.Bytes -eq $expectedMap.Bytes -and $successorMap.Sha256 -ceq $expectedMap.Sha256)) {
        throw 'R32 successor map did not change from the exact R31 predecessor.'
    }
    $commit=[pscustomobject] [ordered] @{
        Schema=$schema; Status='COMMITTED'; RunToken=$RunToken
        PrewriteAdmission=$prewriteAdmission; BuildAdmission=$buildAdmission
        BuildContinuousMemoryGuard=$buildGuard; NativeMutationQuiescence=$successQuiescence
        PredecessorMap=$expectedMap; SuccessorMap=$successorMap; ExternalBackup=Get-FileState $backupMap
        RuntimeDllBefore=$expectedRuntime; RuntimeDllAfter=$runtimeAfter
        EditorDllBefore=$expectedEditor; EditorDllAfter=$editorAfter
        GroundHeaderBefore=$admission.GroundHeader
        GroundHeaderAfter=Get-FileState (Join-Path $nativeProjectRoot (Get-PinRelativePath $sourcePins[0] 'Native'))
        GroundSourceBefore=$admission.GroundSource
        GroundSourceAfter=Get-FileState (Join-Path $nativeProjectRoot (Get-PinRelativePath $sourcePins[1] 'Native'))
        NativePluginSourceTreeBefore=@($admission.NativePluginSourceTree)
        NativePluginSourceTreeAfter=@($expectedR32PluginSourceTree)
        StageResults=@($stageResults)
        R30TransactionAdmission=[pscustomobject] [ordered] @{ Path=$admission.R30Transaction.Path; File=$admission.R30Transaction.State }
        R30CaptureAdmission=[pscustomobject] [ordered] @{ Path=$admission.R30Capture.Path; File=$admission.R30Capture.State }
        R31TransactionAdmission=[pscustomobject] [ordered] @{ Path=$admission.R31Transaction.Path; File=$admission.R31Transaction.State }
        R31CaptureAdmission=[pscustomobject] [ordered] @{ Path=$admission.R31Capture.Path; File=$admission.R31Capture.State }
        PromotedCodeFileCount=8; PromotedContractFileCount=1
        PromotedFiles=@(@($sourcePins)+@($sourceAssetPins) | ForEach-Object {
            [pscustomobject] [ordered] @{
                RepositoryRelativePath=Get-PinRelativePath $_ 'Repository'
                NativeRelativePath=Get-PinRelativePath $_ 'Native'
                State=Get-FileState (Join-Path $nativeProjectRoot (Get-PinRelativePath $_ 'Native'))
            }
        })
        R33SourceOrDeclarationAllowed=$false
        CommitR32EndpointInvocationCount=1
        StandaloneApplyR32EndpointInvocationCount=0
        R32ContentPackageCount=4; R32MaterialPackages=@($r32ContentReceipt)
        ReusedCleanR29PackageCount=7
        SelectedTransformCount=4608; OwnedHismCount=12
        SourceGroundVegetationActorModified=$false; SourceGroundVegetationComponentsModified=$false
        SourceR29AssetPackagesModified=$false; TerrainModified=$false
        BuildingOrSurroundingsModified=$false; TreePresentationModified=$false
        TreeMaterialResponseV3=$admission.TreeMaterialResponseV3
        TreeMaterialResponseV3PromotedAtR30=$true
        TreeMaterialResponseV3Preserved=$true
        TreeResponseMaterialPackageCount=13
        TreeDerivativeMeshPackageCount=5
        TreeRuntimeResponseMidCount=26
        TreePlacementGeometryOpacityWindAuthorityModified=$false
        CollisionModified=$false; NavigationModified=$false
        SimulationCollisionNavigationSensorRfAuthority=$false
        SensorPlacementModified=$false; DetectionSimulationModified=$false; RfTruthModified=$false
        GeospatialAuthority=$false; HyperrealismClaimed=$false
        PhysicalPbrMaterialTruthClaimed=$false; SiteMeasuredMaterialTruthClaimed=$false
        VisualCaptureAccepted=$false; CaptureRevalidationRequired=$true
    }
    $commitPath=Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $commitPath -Encoding utf8NoBOM
    $committed=$true
    $commit | ConvertTo-Json -Depth 16
}
catch { $failure=$_.Exception }
finally {
    if (-not $committed -and $transactionStarted) {
        $rollbackErrors=[Collections.Generic.List[string]]::new()
        $rollbackMayMutateFilesystem=$false
        if ($null -ne $script:memoryWatchdog) {
            try { [void] (Stop-ContinuousMemoryWatchdog) } catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        foreach ($owned in @(Get-ActiveOwnedNativeProcessHandles)) {
            try { Close-OwnedProcess $owned 1 -AllowContainment } catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        try {
            [void] (Wait-NativeMutationQuiescence 'before R32 filesystem rollback restoration' 60)
            $rollbackMayMutateFilesystem=$true
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackMayMutateFilesystem) {
            try { Restore-FileJournal $mapJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Remove-IsolatedR32Content } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-TreeJournal $buildJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-FileJournal $sourceJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-DirectoryPresenceJournal $sourceDirectoryJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
            if ($null -ne $admission) {
                try { [void] (Assert-State $admission.Map $mapFile 'rollback R31 map') } catch { $rollbackErrors.Add($_.Exception.Message) }
                try { [void] (Assert-State $admission.RuntimeDll $runtimeDll 'rollback R31 runtime DLL') } catch { $rollbackErrors.Add($_.Exception.Message) }
                try { [void] (Assert-State $admission.EditorDll $editorDll 'rollback R31 editor DLL') } catch { $rollbackErrors.Add($_.Exception.Message) }
                try { Assert-NativePreState $sourcePins } catch { $rollbackErrors.Add($_.Exception.Message) }
                try { Assert-NativePreState $sourceAssetPins } catch { $rollbackErrors.Add($_.Exception.Message) }
                try { Assert-TreeReceipt $nativePluginSourceRoot $admission.NativePluginSourceTree 'rollback R31-captured plugin source' } catch { $rollbackErrors.Add($_.Exception.Message) }
            }
            if ($null -ne $immutableBefore) {
                try { Assert-TreeReceipt $r31ContentRoot $immutableBefore.R31Content 'rollback R31 content' } catch { $rollbackErrors.Add($_.Exception.Message) }
                try { Assert-TreeReceipt $r29VegetationContentRoot $immutableBefore.R29Vegetation 'rollback R29 vegetation' } catch { $rollbackErrors.Add($_.Exception.Message) }
            }
            if ([IO.Directory]::Exists($r32ContentRoot)) {
                $rollbackErrors.Add("R32 material-content root remained after rollback: $r32ContentRoot")
            }
        }
        else {
            $rollbackErrors.Add('Filesystem rollback was intentionally not attempted while an owned build/editor process tree remained active; exact journals were retained for guarded recovery.')
        }
        try { Assert-ProtectedUnchanged $script:protectedBefore } catch { $rollbackErrors.Add($_.Exception.Message) }
        $rollback=[pscustomobject] [ordered] @{
            Schema=$schema
            Status=if ($rollbackErrors.Count -eq 0) { 'ROLLED_BACK' } else { 'ROLLBACK_INCOMPLETE' }
            Failure=if ($null -eq $failure) { 'unknown' } else { $failure.Message }
            RollbackErrors=@($rollbackErrors)
        }
        $rollback | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $transactionRoot 'rollback.json') -Encoding utf8NoBOM
        if ($rollbackErrors.Count -ne 0) {
            throw "R32 transaction failed and rollback was incomplete: failure={$($rollback.Failure)} rollback={$([string]::Join(' | ', @($rollbackErrors)))}"
        }
    }
}
if ($null -ne $failure) { throw $failure }
