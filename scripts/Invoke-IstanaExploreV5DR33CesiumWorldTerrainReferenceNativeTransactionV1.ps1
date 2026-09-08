#requires -Version 7.0

<#
.SYNOPSIS
Runs the guarded V5D R33 Cesium World Terrain visual-reference transaction.

.DESCRIPTION
The default invocation and -StaticSelfCheck are repository-only. -Execute is
the sole native-write authority. Live execution requires six independently
hash-pinned predecessor receipts: R30, R31, and R32 commit plus explicitly
human-accepted capture receipts. The current mechanical-only R30 acceptance is
intentionally inadmissible.

The caller also pins the exact R32 map, runtime/editor DLLs, Ground pair, and a
canonical SHA-256 over the complete native plugin source tree recorded by the
accepted R32 capture. Exactly nine C++ files and one contract are promoted;
there are no content packages. The wrapper builds serially, cold-validates R32,
calls CommitR33 exactly once, then cold-validates R33. Source, map, binary and
intermediate journals are restored only after owned-process quiescence.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,
    [switch] $StaticSelfCheck,
    [switch] $RequireR32Predecessor,

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
    [string] $R32CommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR32CommitReceiptSha256 = '',
    [string] $R32CaptureCommitReceipt = '',
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedR32CaptureCommitReceiptSha256 = '',

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
    [ValidatePattern('^$|^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedNativePluginSourceTreeSha256 = '',

    [ValidateRange(120, 3600)]
    [int] $BuildTimeoutSeconds = 1800,
    [ValidateRange(120, 1800)]
    [int] $EditorTimeoutSeconds = 900,
    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema = 'triad.istana_explore_v5d.r33_cesium_world_terrain_reference.native_transaction.v1'
$r30TransactionSchema = 'triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1'
$r30CaptureSchema = 'triad.istana_explore_v5d.r30_player0_capture.v1'
$r31TransactionSchema = 'triad.istana_explore_v5d.broad_shell_r31.native_transaction.v1'
$r31CaptureSchema = 'triad.istana_explore_v5d.r31_player0_capture.v2'
$r32TransactionSchema = 'triad.istana_explore_v5d.r32_medium_distance_turf.native_transaction.v1'
$r32CaptureSchema = 'triad.istana_explore_v5d.r32_player0_capture.v1'
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # fixed 10 GiB
$privateMemoryCeilingBytes = 12884901888L # fixed 12 GiB
$minimumSystemFreeVirtualBytes = 6442450944L # fixed continuous 6 GiB
$memoryPollMilliseconds = 500
$memoryPersistentBreachMilliseconds = 2000
$expectedCesiumDescriptorVersion = 78
$expectedCesiumDescriptorVersionName = '2.18.0'
$expectedCesiumDescriptorEngineVersion = '5.5.0'
$expectedCesiumDescriptorBytes = 1214L
$expectedCesiumDescriptorSha256 = '77F60013ADAFAC1EADBC9364F7453E6CF0BD83AA824DFBE5291D5D0FF2F1D2A6'

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
$treeRealismContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Intermediate'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DCesiumWorldTerrainReferenceR33V1'))
$r30TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1'))
$r30EvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R30Evidence')
$r31TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DBroadShellR31V1'))
$r31EvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R31Evidence')
$r32TransactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DMediumDistanceTurfR32V1'))
$r32EvidenceBase = [IO.Path]::GetFullPath('D:\triad\TRIAD_R32Evidence')
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase $RunToken))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$r32Library = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary'
$r33Library = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'
$script:ownedProcesses = [Collections.Generic.List[object]]::new()
$script:memoryBreachStartedUtc = $null
$script:memoryObservations = [Collections.Generic.List[object]]::new()

# Exact R33 promotion: four canonical policy/ground replacement files, the
# state-aware R29 terrain consumer replacement, the R33 runtime pair, the R33
# editor pair, and one source contract. No map content package or unrelated
# plugin source is admitted.
$sourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DContextPolicyActor.h'
        Bytes=13025L; Sha256='674134B2404364384A5760012114F19C19CCCF16A6E5D9552E6A4D20C396E875'
        NativeBeforePresent=$true; NativeBeforeBytes=10649L
        NativeBeforeSha256='9114F728E337523DF3AD0FE5021685683C41BB048CB0049713C2ECC9342D962B'
    }
    [pscustomobject] [ordered] @{
        RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DContextPolicyActor.cpp'
        Bytes=156491L; Sha256='7A75941BBD020CBCA68E49C86B5234748EBF96BCACB615BB4C14B6459E904AB7'
        NativeBeforePresent=$true; NativeBeforeBytes=134398L
        NativeBeforeSha256='58074AC6449BD6FBF7D86DDB876B9CD3B7BEFF03162DAA9EFE10C0A08E0DF4C4'
    }
    [pscustomobject] [ordered] @{
        RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DGroundVegetationActor.h'
        Bytes=24974L; Sha256='95122779D264BB0539AEC25A93D88C6485E5D4DB89E8F50B855D4EF1C82302E2'
        NativeBeforePresent=$true; NativeBeforeBytes=$ExpectedGroundHeaderBytes
        NativeBeforeSha256=$ExpectedGroundHeaderSha256.ToUpperInvariant()
    }
    [pscustomobject] [ordered] @{
        RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DGroundVegetationActor.cpp'
        Bytes=249809L; Sha256='96EB52D2D6863D23A12A1F1A804403577CC1587CA1025AB0BDC6EB7A39C359B4'
        NativeBeforePresent=$true; NativeBeforeBytes=$ExpectedGroundSourceBytes
        NativeBeforeSha256=$ExpectedGroundSourceSha256.ToUpperInvariant()
    }
    [pscustomobject] [ordered] @{
        RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h'
        Bytes=6979L; Sha256='3495AD27881AD9C72788ED8ED1AA0F0AA0052A567A3CADB49A3650BC654EA750'; NativeBeforePresent=$false
    }
    [pscustomobject] [ordered] @{
        RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.cpp'
        Bytes=54715L; Sha256='0324CF4CC5674A22608A301C115010F8E1035BB94FE2D2667549FD1DE0B102CA'; NativeBeforePresent=$false
    }
    [pscustomobject] [ordered] @{
        RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary.h'
        Bytes=1656L; Sha256='AB03FACD5292C7306262FC5C0F81080727D3A69C79B31526ED141D1491692F8E'; NativeBeforePresent=$false
    }
    [pscustomobject] [ordered] @{
        RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary.cpp'
        Bytes=37009L; Sha256='C76DBBF2ADEF6022211C6176AAAFF228CB1714899C0965A5EA275372466DD151'; NativeBeforePresent=$false
    }
    [pscustomobject] [ordered] @{
        RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp'
        Bytes=19215L; Sha256='1F598910BC4D763BBBA04FD55DBE07D0F59B80078BB973924CDFBB40745E1DF7'
        NativeBeforePresent=$true; NativeBeforeBytes=19064L
        NativeBeforeSha256='F57C4C51F666290EF3C74CB03E4A536C476250CCC3AC3BD008D92456C824CA36'
    }
)

$sourceAssetPins = @(
    [pscustomobject] [ordered] @{
        RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Terrain\R33CesiumWorldTerrainReference\r33_cesium_world_terrain_reference.contract.json'
        Bytes=22421L; Sha256='1A052B21307500B793ABB65EF4DE0DE1F62A3076D6EB6385B09BC8B9BA4EFE8E'; NativeBeforePresent=$false
    }
)

$retainedR32Pins = @(
    [pscustomobject] [ordered] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h'; Bytes=6436L; Sha256='8862523D7B9CCB4D6891B6A8F23728C1B95D9E16513A1DA936565F30C3164ACE' }
    [pscustomobject] [ordered] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR32MediumDistanceTurfActor.cpp'; Bytes=35336L; Sha256='1B51CECF08785C889B63E95BD97B681AA065CBB36C734BC8AA2247EF7F6BD1D4' }
    [pscustomobject] [ordered] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.h'; Bytes=966L; Sha256='CCD2EE99EE42BD7D7B322423E5671E49D5246E445FB0DDC0103AAED10BBCD082' }
    [pscustomobject] [ordered] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.cpp'; Bytes=22691L; Sha256='90FE9E9E3152BFEFE8585A0BB643F5AF3B068852C63C496AC253F8DBC829CB16' }
    [pscustomobject] [ordered] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.h'; Bytes=2280L; Sha256='4C5B1014E2842F7DA27431260352ABFF91F7E7841D6D404D30B7C79E21FA13E5' }
    [pscustomobject] [ordered] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.cpp'; Bytes=31204L; Sha256='C85E270797F08267A67101C6B53A4E4F9383E90FBE366C90F230FF871BA1B31D' }
    [pscustomobject] [ordered] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\TRIADSensorFusion.Build.cs'; Bytes=2736L; Sha256='E066028EC940568E2927ADA63F684ABA5866786B99B6F494D02713DC0969DEB2' }
    [pscustomobject] [ordered] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\TRIADSensorFusionEditor.Build.cs'; Bytes=1229L; Sha256='B7D10EA034A37CB15C9939DADFA1910B229ABA14098F096E21994F7A6D977C1B' }
    [pscustomobject] [ordered] @{ RelativePath='Plugins\TRIADSensorFusion\TRIADSensorFusion.uplugin'; Bytes=781L; Sha256='0E9C5DDEF9AEB66EFE3077D45A39B09A0646401A829AB0E2166AB331C1E9A2B5' }
)

function Get-FileState {
    param([string] $Path)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject] [ordered] @{ Present=$false; Bytes=0L; Sha256='ABSENT' }
    }
    $item=Get-Item -LiteralPath $Path -Force
    [pscustomobject] [ordered] @{
        Present=$true; Bytes=[int64] $item.Length
        Sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual=Get-FileState $Path
    if ([bool] $actual.Present -ne [bool] $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label state mismatch: $Path expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Get-RequiredPropertyValue {
    param($Object, [string] $Name, [string] $Label)
    if ($null -eq $Object) { throw "$Label is null while requiring '$Name'." }
    $property=$Object.PSObject.Properties[$Name]
    if ($null -eq $property) { throw "$Label lacks required property '$Name'." }
    $property.Value
}

function Test-StateMatches {
    param($State, [long] $Bytes, [string] $Sha256)
    [bool] (Get-RequiredPropertyValue $State 'Present' 'receipt state') -eq $true -and
        [int64] (Get-RequiredPropertyValue $State 'Bytes' 'receipt state') -eq $Bytes -and
        [string] (Get-RequiredPropertyValue $State 'Sha256' 'receipt state') -ceq $Sha256.ToUpperInvariant()
}

function Test-ContainedPath {
    param([string] $Candidate, [string] $Root)
    $candidateFull=[IO.Path]::GetFullPath($Candidate).TrimEnd('\')
    $rootFull=[IO.Path]::GetFullPath($Root).TrimEnd('\')
    $candidateFull.StartsWith($rootFull + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Get-VerifiedCesiumPluginDescriptor {
    $searchRoots=@(
        [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins')),
        [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Plugins\Marketplace'))
    )
    $seen=[Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $candidates=[Collections.Generic.List[string]]::new()
    foreach ($root in $searchRoots) {
        if (-not [IO.Directory]::Exists($root)) { continue }
        foreach ($item in @(Get-ChildItem -LiteralPath $root -File -Recurse `
                -Filter 'CesiumForUnreal.uplugin' -Force)) {
            $path=[IO.Path]::GetFullPath($item.FullName)
            if (-not (Test-ContainedPath $path $root)) {
                throw "Cesium descriptor escaped its approved search root: $path"
            }
            if ($seen.Add($path)) { $candidates.Add($path) }
        }
    }
    if ($candidates.Count -ne 1) {
        throw "R33 requires exactly one unambiguous CesiumForUnreal.uplugin descriptor; found=$($candidates.Count)."
    }
    $path=$candidates[0]
    $state=Get-FileState $path
    if (-not (Test-StateMatches $state $expectedCesiumDescriptorBytes `
            $expectedCesiumDescriptorSha256)) {
        throw "Cesium for Unreal descriptor bytes/hash differ from the reviewed 2.18.0 build: $path"
    }
    $descriptor=Get-Content -LiteralPath $path -Raw |
        ConvertFrom-Json -Depth 32
    if ([int] (Get-RequiredPropertyValue $descriptor 'Version' `
            'Cesium descriptor') -ne $expectedCesiumDescriptorVersion -or
        [string] (Get-RequiredPropertyValue $descriptor 'VersionName' `
            'Cesium descriptor') -cne $expectedCesiumDescriptorVersionName -or
        [string] (Get-RequiredPropertyValue $descriptor 'EngineVersion' `
            'Cesium descriptor') -cne $expectedCesiumDescriptorEngineVersion -or
        [string] (Get-RequiredPropertyValue $descriptor 'FriendlyName' `
            'Cesium descriptor') -cne 'Cesium for Unreal') {
        throw "Cesium for Unreal descriptor metadata differs from the reviewed 2.18.0 / UE 5.5 build: $path"
    }
    [pscustomobject] [ordered] @{
        Path=$path; File=$state
        Version=$expectedCesiumDescriptorVersion
        VersionName=$expectedCesiumDescriptorVersionName
        EngineVersion=$expectedCesiumDescriptorEngineVersion
    }
}

function Assert-CanonicalRelativePath {
    param([string] $RelativePath, [string] $Label)
    if ([string]::IsNullOrWhiteSpace($RelativePath) -or
        [IO.Path]::IsPathRooted($RelativePath) -or
        $RelativePath -match '(^|[\\/])\.\.([\\/]|$)' -or
        $RelativePath.Contains('/')) {
        throw "$Label path is not a canonical Windows relative path: $RelativePath"
    }
}

function Assert-PinAtRoot {
    param($Pin, [string] $Root, [switch] $NativeBefore)
    $relative=[string] $Pin.RelativePath
    Assert-CanonicalRelativePath $relative 'pinned'
    $path=[IO.Path]::GetFullPath((Join-Path $Root $relative))
    $expected=if ($NativeBefore) {
        [pscustomobject] [ordered] @{
            Present=[bool] $Pin.NativeBeforePresent
            Bytes=if ([bool] $Pin.NativeBeforePresent) { [int64] $Pin.NativeBeforeBytes } else { 0L }
            Sha256=if ([bool] $Pin.NativeBeforePresent) { [string] $Pin.NativeBeforeSha256 } else { 'ABSENT' }
        }
    } else {
        [pscustomobject] [ordered] @{ Present=$true; Bytes=[int64] $Pin.Bytes; Sha256=[string] $Pin.Sha256 }
    }
    [void] (Assert-State $expected $path "pinned $relative")
}

function Get-TreeIdentityReceipt {
    param([string] $Root)
    if (-not [IO.Directory]::Exists($Root)) { return @() }
    @(
        Get-ChildItem -LiteralPath $Root -File -Recurse |
            Sort-Object { [IO.Path]::GetRelativePath($Root, $_.FullName) } |
            ForEach-Object {
                [pscustomobject] [ordered] @{
                    RelativePath=[IO.Path]::GetRelativePath($Root, $_.FullName)
                    Bytes=[int64] $_.Length
                    Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
                }
            }
    )
}

function ConvertTo-TreeIdentity {
    param([object[]] $Rows)
    @(
        $Rows | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath=[string] (Get-RequiredPropertyValue $_ 'RelativePath' 'tree receipt row')
                Bytes=[int64] (Get-RequiredPropertyValue $_ 'Bytes' 'tree receipt row')
                Sha256=[string] (Get-RequiredPropertyValue $_ 'Sha256' 'tree receipt row')
            }
        } | Sort-Object RelativePath
    )
}

function Get-TreeIdentitySha256 {
    param([object[]] $Rows)
    $canonical=ConvertTo-Json @(ConvertTo-TreeIdentity $Rows) -Depth 6 -Compress
    $bytes=[Text.Encoding]::UTF8.GetBytes($canonical)
    [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
}

function Assert-TreeIdentity {
    param([string] $Root, [object[]] $Expected, [string] $Label)
    $actual=@(Get-TreeIdentityReceipt $Root)
    $expectedIdentity=@(ConvertTo-TreeIdentity $Expected)
    if ((ConvertTo-Json $actual -Depth 6 -Compress) -cne
        (ConvertTo-Json $expectedIdentity -Depth 6 -Compress)) {
        throw "$Label full tree identity mismatch."
    }
    $actual
}

function Get-ExpectedPostR33PluginSourceTree {
    param([object[]] $Before)
    $rows=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in @(ConvertTo-TreeIdentity $Before)) {
        $rows[[string] $row.RelativePath]=$row
    }
    $prefix='Plugins\TRIADSensorFusion\Source\'
    foreach ($pin in $sourcePins) {
        if (-not ([string] $pin.RelativePath).StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "R33 plugin source pin escaped expected prefix: $($pin.RelativePath)"
        }
        $treeRelative=([string] $pin.RelativePath).Substring($prefix.Length)
        $rows[$treeRelative]=[pscustomobject] [ordered] @{
            RelativePath=$treeRelative; Bytes=[int64] $pin.Bytes; Sha256=[string] $pin.Sha256
        }
    }
    @($rows.Values | Sort-Object RelativePath)
}

function Read-HashPinnedCommitReceipt {
    param([string] $Path, [string] $ExpectedSha256, [string] $Base, [string] $Label)
    if ([string]::IsNullOrWhiteSpace($Path) -or
        $ExpectedSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
        throw "$Label requires a direct path and SHA-256."
    }
    $full=[IO.Path]::GetFullPath($Path)
    $parent=[IO.Path]::GetDirectoryName($full)
    $token=[IO.Path]::GetFileName($parent)
    if (-not [IO.Path]::GetFileName($full).Equals('commit.json', [StringComparison]::Ordinal) -or
        -not [IO.Path]::GetDirectoryName($parent).Equals($Base, [StringComparison]::OrdinalIgnoreCase) -or
        $token -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$') {
        throw "$Label must be the exact direct run-token commit.json below $Base."
    }
    $item=Get-Item -LiteralPath $full -Force
    if ($item.LinkType) { throw "$Label may not be a filesystem link." }
    $state=Get-FileState $full
    if (-not $state.Present -or $state.Sha256 -cne $ExpectedSha256.ToUpperInvariant()) {
        throw "$Label SHA-256 mismatch."
    }
    [pscustomobject] [ordered] @{
        Path=$full; Token=$token; State=$state
        Receipt=(Get-Content -LiteralPath $full -Raw | ConvertFrom-Json -Depth 64)
    }
}

function Assert-AdmissionBinding {
    param($Binding, $Admission, [string] $Label)
    $path=[IO.Path]::GetFullPath([string] (Get-RequiredPropertyValue $Binding 'Path' $Label))
    $file=Get-RequiredPropertyValue $Binding 'File' $Label
    if (-not $path.Equals($Admission.Path, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-StateMatches $file $Admission.State.Bytes $Admission.State.Sha256)) {
        throw "$Label does not bind the exact admitted receipt."
    }
}

function Assert-ExplicitHumanCaptureTruth {
    param($Receipt, [string] $Label, [int] $PoseCount, [string] $QaField, [string] $AdmissionField)
    if ([string] (Get-RequiredPropertyValue $Receipt 'Status' $Label) -cne 'COMMITTED' -or
        [int] (Get-RequiredPropertyValue $Receipt 'ExactPoseCount' $Label) -ne $PoseCount -or
        [bool] (Get-RequiredPropertyValue $Receipt 'MechanicalCaptureValidationPassed' $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $Receipt 'ExplicitHumanReviewAcceptance' $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $Receipt 'AutomaticVisualAcceptanceAllowed' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $Receipt 'VisualReviewRequired' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $Receipt 'VisualReviewAccepted' $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $Receipt $QaField $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $Receipt $AdmissionField $Label) -ne $true -or
        [bool] (Get-RequiredPropertyValue $Receipt 'MapModifiedByCapture' $Label) -ne $false -or
        [bool] (Get-RequiredPropertyValue $Receipt 'SimulationCollisionNavigationSensorRfModified' $Label) -ne $false) {
        throw "$Label is not an explicit human-accepted no-mutation capture receipt."
    }
}

function Assert-R31V2NaniteRasterCaptureTruth {
    param($Receipt, $Admission)
    if ([int] (Get-RequiredPropertyValue $Receipt 'ExactImageCount' 'R31 capture receipt') -ne 6 -or
        [bool] (Get-RequiredPropertyValue $Receipt 'ConfirmedNaniteRasterPairReviewed' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $Receipt 'HumanNaniteRasterComparisonAttested' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $Receipt 'NaniteRasterAppearanceParityAccepted' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $Receipt 'NaniteConsoleStateRestored' 'R31 capture receipt') -ne $true) {
        throw 'R31 capture receipt lacks the accepted six-image Nanite/raster review truth.'
    }
    $acceptance=Get-RequiredPropertyValue $Receipt 'AcceptanceRevalidation' 'R31 capture receipt'
    if ([bool] (Get-RequiredPropertyValue $acceptance 'Unchanged' 'R31 acceptance revalidation') -ne $true -or
        [bool] (Get-RequiredPropertyValue $acceptance 'PendingReceiptHashPinned' 'R31 acceptance revalidation') -ne $true -or
        [bool] (Get-RequiredPropertyValue $acceptance 'SixPngsRedecodedAndRehashed' 'R31 acceptance revalidation') -ne $true -or
        [bool] (Get-RequiredPropertyValue $acceptance 'NativeReceiptWriteAllowed' 'R31 acceptance revalidation') -ne $false) {
        throw 'R31 capture receipt lacks immutable six-image acceptance revalidation.'
    }

    $captures=@(Get-RequiredPropertyValue $Receipt 'Captures' 'R31 capture receipt')
    $expectedPoseIds=@('075m','020m','008m','002m','surroundings_oblique_macdonald')
    if ($captures.Count -ne 5) {
        throw 'R31 accepted capture must retain exactly five baseline pose rows.'
    }
    $fileHashes=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $pixelHashes=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    for ($index=0; $index -lt $expectedPoseIds.Count; ++$index) {
        $capture=$captures[$index]
        $pose=Get-RequiredPropertyValue $capture 'Pose' "R31 capture row $index"
        $poseId=[string] (Get-RequiredPropertyValue $pose 'Id' "R31 capture pose $index")
        $image=Get-RequiredPropertyValue $capture 'Image' "R31 capture row $index"
        $expectedImagePath=[IO.Path]::GetFullPath((Join-Path `
            (Join-Path (Join-Path $r31EvidenceBase $Admission.Token) 'captures') `
            "explore_v5d_r31_player0_${poseId}_$($Admission.Token).png"))
        $actualImagePath=[IO.Path]::GetFullPath([string] (
            Get-RequiredPropertyValue $image 'Path' "R31 capture image $index"))
        $decodedHash=[string] (Get-RequiredPropertyValue $image 'DecodedBgraSha256' "R31 capture image $index")
        if ($poseId -cne $expectedPoseIds[$index] -or
            [bool] (Get-RequiredPropertyValue $capture 'ProviderFallbackVisualQa' "R31 capture row $index") -ne $true -or
            [bool] (Get-RequiredPropertyValue $capture 'ProviderReadyProofClaimed' "R31 capture row $index") -ne $false -or
            [string] (Get-RequiredPropertyValue $capture 'RenderPathId' "R31 capture row $index") -cne 'NANITE_ON' -or
            [int] (Get-RequiredPropertyValue $capture 'NaniteConsoleValue' "R31 capture row $index") -ne 1 -or
            [int] (Get-RequiredPropertyValue $capture 'NaniteProxyRenderMode' "R31 capture row $index") -ne 1 -or
            -not $actualImagePath.Equals($expectedImagePath,[StringComparison]::OrdinalIgnoreCase) -or
            [int] (Get-RequiredPropertyValue $image 'WidthPixels' "R31 capture image $index") -ne 2560 -or
            [int] (Get-RequiredPropertyValue $image 'HeightPixels' "R31 capture image $index") -ne 1440 -or
            [bool] (Get-RequiredPropertyValue $image 'NonBlank' "R31 capture image $index") -ne $true -or
            [string]::IsNullOrWhiteSpace($decodedHash) -or
            -not $fileHashes.Add([string] (Get-RequiredPropertyValue $image 'Sha256' "R31 capture image $index")) -or
            -not $pixelHashes.Add($decodedHash)) {
            throw "R31 baseline capture row $index failed exact six-image admission."
        }
        [void] (Assert-State $image $expectedImagePath "R31 baseline capture PNG $index")
    }

    $comparison=Get-RequiredPropertyValue $Receipt `
        'NaniteRasterHighOccupancyComparison' 'R31 capture receipt'
    if ([bool] (Get-RequiredPropertyValue $comparison 'Required' 'R31 Nanite/raster comparison') -ne $true -or
        [string] (Get-RequiredPropertyValue $comparison 'PoseId' 'R31 Nanite/raster comparison') -cne 'surroundings_oblique_macdonald' -or
        [bool] (Get-RequiredPropertyValue $comparison 'SamePose' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'SameGameProcess' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'SameCookedClosure' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'MechanicalValidationPassed' 'R31 Nanite/raster comparison') -ne $true -or
        [bool] (Get-RequiredPropertyValue $comparison 'HumanNaniteRasterComparisonAttested' 'R31 Nanite/raster comparison') -ne $false -or
        [bool] (Get-RequiredPropertyValue $comparison 'NaniteRasterAppearanceParityAccepted' 'R31 Nanite/raster comparison') -ne $false -or
        [bool] (Get-RequiredPropertyValue $comparison 'AutomaticVisualAcceptanceAllowed' 'R31 Nanite/raster comparison') -ne $false) {
        throw 'R31 nested Nanite/raster comparison is not the exact fail-closed mechanical proof.'
    }
    $nanite=Get-RequiredPropertyValue $comparison 'NaniteOn' 'R31 Nanite/raster comparison'
    if ((ConvertTo-Json (Get-RequiredPropertyValue $nanite 'Capture' 'R31 Nanite side') -Depth 20 -Compress) -cne
            (ConvertTo-Json $captures[4] -Depth 20 -Compress) -or
        [string] (Get-RequiredPropertyValue $nanite 'RenderPathId' 'R31 Nanite side') -cne 'NANITE_ON' -or
        [int] (Get-RequiredPropertyValue $nanite 'NaniteConsoleValue' 'R31 Nanite side') -ne 1 -or
        [int] (Get-RequiredPropertyValue $nanite 'NaniteProxyRenderMode' 'R31 Nanite side') -ne 1) {
        throw 'R31 Nanite comparison side is not the exact fifth baseline capture.'
    }
    $raster=Get-RequiredPropertyValue $comparison 'RasterFallback' 'R31 Nanite/raster comparison'
    $rasterPose=Get-RequiredPropertyValue $raster 'Pose' 'R31 raster side'
    $baselinePose=Get-RequiredPropertyValue $captures[4] 'Pose' 'R31 fifth baseline capture'
    if ((ConvertTo-Json $rasterPose -Depth 8 -Compress) -cne
            (ConvertTo-Json $baselinePose -Depth 8 -Compress) -or
        [string] (Get-RequiredPropertyValue $raster 'RenderPathId' 'R31 raster side') -cne 'RASTER_FALLBACK' -or
        [int] (Get-RequiredPropertyValue $raster 'NaniteConsoleValue' 'R31 raster side') -ne 0 -or
        [int] (Get-RequiredPropertyValue $raster 'NaniteProxyRenderMode' 'R31 raster side') -ne 0) {
        throw 'R31 raster comparison side changed pose or lacks exact zero CVar readback.'
    }
    foreach ($stateName in @('ImmediatePreCaptureState','ImmediatePostCaptureState')) {
        $stateReport=[string] (Get-RequiredPropertyValue $raster $stateName 'R31 raster side')
        foreach ($marker in @(
            'poseId=surroundings_oblique_macdonald','exactQaViewPose=true',
            'renderPath=RASTER_FALLBACK','r.Nanite=0',
            'r.Nanite.ProxyRenderMode=0','naniteMeshEnabled=true',
            'naniteDataValid=true','naniteKeepPercentTriangles=1.0',
            'fallbackPercentTriangles=1.0')) {
            if (-not $stateReport.Contains($marker,[StringComparison]::Ordinal)) {
                throw "R31 raster comparison $stateName lacks '$marker'."
            }
        }
    }
    $rasterImage=Get-RequiredPropertyValue $raster 'Image' 'R31 raster side'
    $expectedRasterPath=[IO.Path]::GetFullPath((Join-Path `
        (Join-Path (Join-Path $r31EvidenceBase $Admission.Token) 'captures') `
        "explore_v5d_r31_player0_surroundings_oblique_macdonald_$($Admission.Token)_raster_fallback.png"))
    if (-not [IO.Path]::GetFullPath([string] (Get-RequiredPropertyValue $rasterImage 'Path' 'R31 raster image')).Equals($expectedRasterPath,[StringComparison]::OrdinalIgnoreCase) -or
        [int] (Get-RequiredPropertyValue $rasterImage 'WidthPixels' 'R31 raster image') -ne 2560 -or
        [int] (Get-RequiredPropertyValue $rasterImage 'HeightPixels' 'R31 raster image') -ne 1440 -or
        [bool] (Get-RequiredPropertyValue $rasterImage 'NonBlank' 'R31 raster image') -ne $true -or
        [string]::IsNullOrWhiteSpace([string] (Get-RequiredPropertyValue $rasterImage 'DecodedBgraSha256' 'R31 raster image'))) {
        throw 'R31 raster comparison image is not the canonical decoded sixth image.'
    }
    [void] (Assert-State $rasterImage $expectedRasterPath 'R31 raster comparison PNG')
    $restore=Get-RequiredPropertyValue $comparison 'NaniteRestore' 'R31 Nanite/raster comparison'
    $restoreState=[string] (Get-RequiredPropertyValue $restore 'State' 'R31 Nanite restore')
    if ([bool] (Get-RequiredPropertyValue $restore 'Restored' 'R31 Nanite restore') -ne $true -or
        [string] (Get-RequiredPropertyValue $restore 'RenderPathId' 'R31 Nanite restore') -cne 'NANITE_ON' -or
        [int] (Get-RequiredPropertyValue $restore 'NaniteConsoleValue' 'R31 Nanite restore') -ne 1 -or
        [int] (Get-RequiredPropertyValue $restore 'NaniteProxyRenderMode' 'R31 Nanite restore') -ne 1 -or
        -not $restoreState.Contains('renderPath=NANITE_ON',[StringComparison]::Ordinal) -or
        -not $restoreState.Contains('r.Nanite=1',[StringComparison]::Ordinal) -or
        -not $restoreState.Contains('r.Nanite.ProxyRenderMode=1',[StringComparison]::Ordinal)) {
        throw 'R31 capture does not prove Nanite state restoration after the raster comparison.'
    }
}

function Assert-R32CaptureTreeExtendsCommit {
    param([object[]] $CommitTree, [object[]] $CaptureTree)
    $before=@(ConvertTo-TreeIdentity $CommitTree)
    $after=@(ConvertTo-TreeIdentity $CaptureTree)
    $afterByPath=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($row in $after) {
        if ($afterByPath.ContainsKey([string] $row.RelativePath)) {
            throw "R32 capture source tree contains duplicate path: $($row.RelativePath)"
        }
        $afterByPath.Add([string] $row.RelativePath, $row)
    }
    foreach ($row in $before) {
        if (-not $afterByPath.ContainsKey([string] $row.RelativePath)) {
            throw "R32 capture source promotion removed predecessor file: $($row.RelativePath)"
        }
        $candidate=$afterByPath[[string] $row.RelativePath]
        if ([int64] $candidate.Bytes -ne [int64] $row.Bytes -or
            [string] $candidate.Sha256 -cne [string] $row.Sha256) {
            throw "R32 capture source promotion changed predecessor file: $($row.RelativePath)"
        }
        [void] $afterByPath.Remove([string] $row.RelativePath)
    }
    $expectedExtras=@(
        'TRIADSensorFusion\Public\TRIADIstanaExploreV5DR32Player0CaptureLibrary.h'
        'TRIADSensorFusion\Private\TRIADIstanaExploreV5DR32Player0CaptureLibrary.cpp'
    )
    if ($afterByPath.Count -ne 2) {
        throw "R32 capture source promotion must add exactly two runtime files; extras=$($afterByPath.Count)."
    }
    foreach ($relative in $expectedExtras) {
        if (-not $afterByPath.ContainsKey($relative)) {
            throw "R32 capture source promotion lacks exact file: $relative"
        }
    }
}

function Assert-SixPredecessorReceipts {
    $r30CommitAdmission=Read-HashPinnedCommitReceipt `
        $R30CommitReceipt $ExpectedR30CommitReceiptSha256 `
        $r30TransactionBase 'R30 transaction receipt'
    $r30=$r30CommitAdmission.Receipt
    if ([string] (Get-RequiredPropertyValue $r30 'Schema' 'R30 transaction receipt') -cne $r30TransactionSchema -or
        [string] (Get-RequiredPropertyValue $r30 'Status' 'R30 transaction receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $r30 'RunToken' 'R30 transaction receipt') -cne $r30CommitAdmission.Token -or
        [int] (Get-RequiredPropertyValue $r30 'R30ContentPackageCount' 'R30 transaction receipt') -ne 12 -or
        [bool] (Get-RequiredPropertyValue $r30 'TreeMaterialResponseV3Promoted' 'R30 transaction receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $r30 'TreeResponseMaterialPackageCount' 'R30 transaction receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $r30 'TreeDerivativeMeshPackageCount' 'R30 transaction receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $r30 'TreeRuntimeResponseMidCount' 'R30 transaction receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $r30 'TreePlacementGeometryOpacityWindAuthorityModified' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r30 'VisualCaptureAccepted' 'R30 transaction receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r30 'CaptureRevalidationRequired' 'R30 transaction receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30 'SimulationCollisionNavigationSensorRfAuthority' 'R30 transaction receipt') -ne $false) {
        throw 'R30 transaction receipt failed exact predecessor truth.'
    }
    $r30Map=Get-RequiredPropertyValue $r30 'SuccessorMap' 'R30 transaction receipt'
    $r30Runtime=Get-RequiredPropertyValue $r30 'RuntimeDllAfter' 'R30 transaction receipt'
    $r30Editor=Get-RequiredPropertyValue $r30 'EditorDllAfter' 'R30 transaction receipt'

    $r30CaptureAdmission=Read-HashPinnedCommitReceipt `
        $R30CaptureCommitReceipt $ExpectedR30CaptureCommitReceiptSha256 `
        $r30EvidenceBase 'R30 capture receipt'
    $r30Capture=$r30CaptureAdmission.Receipt
    if ([string] (Get-RequiredPropertyValue $r30Capture 'Schema' 'R30 capture receipt') -cne $r30CaptureSchema -or
        [string] (Get-RequiredPropertyValue $r30Capture 'RunToken' 'R30 capture receipt') -cne $r30CaptureAdmission.Token -or
        [string] (Get-RequiredPropertyValue $r30Capture 'NativeOrder' 'R30 capture receipt') -cne 'R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31' -or
        [bool] (Get-RequiredPropertyValue $r30Capture 'R31DependencyAllowed' 'R30 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r30Capture 'TreeMaterialResponseV3Reviewed' 'R30 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r30Capture 'ConfirmedFiveImagesReviewed' 'R30 capture receipt') -ne $true) {
        throw 'R30 capture receipt does not provide the required explicit five-image review gate.'
    }
    Assert-ExplicitHumanCaptureTruth `
        $r30Capture 'R30 capture receipt' 5 `
        'ProviderFallbackVisualQaAccepted' 'R31AdmissionAuthorized'
    Assert-AdmissionBinding `
        (Get-RequiredPropertyValue $r30Capture 'R30CommitAdmission' 'R30 capture receipt') `
        $r30CommitAdmission 'R30 capture transaction admission'
    if (-not (Test-StateMatches (Get-RequiredPropertyValue $r30Capture 'Map' 'R30 capture receipt') $r30Map.Bytes $r30Map.Sha256) -or
        -not (Test-StateMatches (Get-RequiredPropertyValue $r30Capture 'RuntimeEditorDll' 'R30 capture receipt') $r30Runtime.Bytes $r30Runtime.Sha256) -or
        -not (Test-StateMatches (Get-RequiredPropertyValue $r30Capture 'EditorDll' 'R30 capture receipt') $r30Editor.Bytes $r30Editor.Sha256)) {
        throw 'R30 capture is not bound to its exact committed map and DLLs.'
    }

    $r31CommitAdmission=Read-HashPinnedCommitReceipt `
        $R31CommitReceipt $ExpectedR31CommitReceiptSha256 `
        $r31TransactionBase 'R31 transaction receipt'
    $r31=$r31CommitAdmission.Receipt
    if ([string] (Get-RequiredPropertyValue $r31 'Schema' 'R31 transaction receipt') -cne $r31TransactionSchema -or
        [string] (Get-RequiredPropertyValue $r31 'Status' 'R31 transaction receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $r31 'RunToken' 'R31 transaction receipt') -cne $r31CommitAdmission.Token -or
        [int] (Get-RequiredPropertyValue $r31 'R31ContentPackageCount' 'R31 transaction receipt') -ne 5 -or
        [bool] (Get-RequiredPropertyValue $r31 'TreeMaterialResponseV3PromotedAtR30' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31 'TreeMaterialResponseV3Preserved' 'R31 transaction receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $r31 'TreeResponseMaterialPackageCount' 'R31 transaction receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $r31 'TreeDerivativeMeshPackageCount' 'R31 transaction receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $r31 'TreeRuntimeResponseMidCount' 'R31 transaction receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $r31 'TreePlacementGeometryOpacityWindAuthorityModified' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31 'R33SourceOrDeclarationAllowed' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31 'VisualCaptureAccepted' 'R31 transaction receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31 'CaptureRevalidationRequired' 'R31 transaction receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31 'SimulationCollisionNavigationSensorRfAuthority' 'R31 transaction receipt') -ne $false) {
        throw 'R31 transaction receipt failed exact predecessor truth.'
    }
    Assert-AdmissionBinding (Get-RequiredPropertyValue $r31 'R30TransactionAdmission' 'R31 transaction receipt') $r30CommitAdmission 'R31 embedded R30 transaction admission'
    Assert-AdmissionBinding (Get-RequiredPropertyValue $r31 'R30CaptureAdmission' 'R31 transaction receipt') $r30CaptureAdmission 'R31 embedded R30 capture admission'
    $r31Map=Get-RequiredPropertyValue $r31 'SuccessorMap' 'R31 transaction receipt'
    $r31Runtime=Get-RequiredPropertyValue $r31 'RuntimeDllAfter' 'R31 transaction receipt'
    $r31Editor=Get-RequiredPropertyValue $r31 'EditorDllAfter' 'R31 transaction receipt'

    $r31CaptureAdmission=Read-HashPinnedCommitReceipt `
        $R31CaptureCommitReceipt $ExpectedR31CaptureCommitReceiptSha256 `
        $r31EvidenceBase 'R31 capture receipt'
    $r31Capture=$r31CaptureAdmission.Receipt
    if ([string] (Get-RequiredPropertyValue $r31Capture 'Schema' 'R31 capture receipt') -cne $r31CaptureSchema -or
        [string] (Get-RequiredPropertyValue $r31Capture 'RunToken' 'R31 capture receipt') -cne $r31CaptureAdmission.Token -or
        [string] (Get-RequiredPropertyValue $r31Capture 'NativeOrder' 'R31 capture receipt') -cne 'R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32' -or
        [bool] (Get-RequiredPropertyValue $r31Capture 'R32DependencyAllowed' 'R31 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r31Capture 'TreeMaterialResponseV3Preserved' 'R31 capture receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $r31Capture 'ExactImageCount' 'R31 capture receipt') -ne 6 -or
        [bool] (Get-RequiredPropertyValue $r31Capture 'ConfirmedFiveImagesReviewed' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture 'ConfirmedNaniteRasterPairReviewed' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture 'HumanNaniteRasterComparisonAttested' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture 'NaniteRasterAppearanceParityAccepted' 'R31 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r31Capture 'NaniteConsoleStateRestored' 'R31 capture receipt') -ne $true) {
        throw 'R31 capture receipt failed exact order or six-image Nanite/raster review truth.'
    }
    Assert-ExplicitHumanCaptureTruth `
        $r31Capture 'R31 capture receipt' 5 `
        'R31BroadShellVisualQaAccepted' 'R32AdmissionAuthorized'
    Assert-R31V2NaniteRasterCaptureTruth $r31Capture $r31CaptureAdmission
    Assert-AdmissionBinding `
        (Get-RequiredPropertyValue $r31Capture 'R31CommitAdmission' 'R31 capture receipt') `
        $r31CommitAdmission 'R31 capture transaction admission'
    if (-not (Test-StateMatches (Get-RequiredPropertyValue $r31Capture 'Map' 'R31 capture receipt') $r31Map.Bytes $r31Map.Sha256) -or
        -not (Test-StateMatches (Get-RequiredPropertyValue $r31Capture 'RuntimeEditorDll' 'R31 capture receipt') $r31Runtime.Bytes $r31Runtime.Sha256) -or
        -not (Test-StateMatches (Get-RequiredPropertyValue $r31Capture 'EditorDll' 'R31 capture receipt') $r31Editor.Bytes $r31Editor.Sha256)) {
        throw 'R31 capture is not bound to its exact committed map and DLLs.'
    }

    $r32CommitAdmission=Read-HashPinnedCommitReceipt `
        $R32CommitReceipt $ExpectedR32CommitReceiptSha256 `
        $r32TransactionBase 'R32 transaction receipt'
    $r32=$r32CommitAdmission.Receipt
    if ([string] (Get-RequiredPropertyValue $r32 'Schema' 'R32 transaction receipt') -cne $r32TransactionSchema -or
        [string] (Get-RequiredPropertyValue $r32 'Status' 'R32 transaction receipt') -cne 'COMMITTED' -or
        [string] (Get-RequiredPropertyValue $r32 'RunToken' 'R32 transaction receipt') -cne $r32CommitAdmission.Token -or
        [int] (Get-RequiredPropertyValue $r32 'PromotedCodeFileCount' 'R32 transaction receipt') -ne 8 -or
        [int] (Get-RequiredPropertyValue $r32 'PromotedContractFileCount' 'R32 transaction receipt') -ne 1 -or
        [int] (Get-RequiredPropertyValue $r32 'R32ContentPackageCount' 'R32 transaction receipt') -ne 4 -or
        [bool] (Get-RequiredPropertyValue $r32 'TreeMaterialResponseV3PromotedAtR30' 'R32 transaction receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r32 'TreeMaterialResponseV3Preserved' 'R32 transaction receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $r32 'TreeResponseMaterialPackageCount' 'R32 transaction receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $r32 'TreeDerivativeMeshPackageCount' 'R32 transaction receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $r32 'TreeRuntimeResponseMidCount' 'R32 transaction receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $r32 'TreePlacementGeometryOpacityWindAuthorityModified' 'R32 transaction receipt') -ne $false -or
        [int] (Get-RequiredPropertyValue $r32 'CommitR32EndpointInvocationCount' 'R32 transaction receipt') -ne 1 -or
        [bool] (Get-RequiredPropertyValue $r32 'R33SourceOrDeclarationAllowed' 'R32 transaction receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r32 'VisualCaptureAccepted' 'R32 transaction receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r32 'CaptureRevalidationRequired' 'R32 transaction receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r32 'SimulationCollisionNavigationSensorRfAuthority' 'R32 transaction receipt') -ne $false) {
        throw 'R32 transaction receipt failed exact predecessor truth.'
    }
    $r32MaterialReceipts=@(Get-RequiredPropertyValue $r32 'R32MaterialPackages' 'R32 transaction receipt')
    if ($r32MaterialReceipts.Count -ne 4 -or
        [bool] (Get-RequiredPropertyValue $r32 'PhysicalPbrMaterialTruthClaimed' 'R32 transaction receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r32 'SiteMeasuredMaterialTruthClaimed' 'R32 transaction receipt') -ne $false) {
        throw 'R32 transaction receipt failed isolated material-derivative truth.'
    }
    Assert-AdmissionBinding (Get-RequiredPropertyValue $r32 'R30TransactionAdmission' 'R32 transaction receipt') $r30CommitAdmission 'R32 embedded R30 transaction admission'
    Assert-AdmissionBinding (Get-RequiredPropertyValue $r32 'R30CaptureAdmission' 'R32 transaction receipt') $r30CaptureAdmission 'R32 embedded R30 capture admission'
    Assert-AdmissionBinding (Get-RequiredPropertyValue $r32 'R31TransactionAdmission' 'R32 transaction receipt') $r31CommitAdmission 'R32 embedded R31 transaction admission'
    Assert-AdmissionBinding (Get-RequiredPropertyValue $r32 'R31CaptureAdmission' 'R32 transaction receipt') $r31CaptureAdmission 'R32 embedded R31 capture admission'

    $r32Map=Get-RequiredPropertyValue $r32 'SuccessorMap' 'R32 transaction receipt'
    $r32Runtime=Get-RequiredPropertyValue $r32 'RuntimeDllAfter' 'R32 transaction receipt'
    $r32Editor=Get-RequiredPropertyValue $r32 'EditorDllAfter' 'R32 transaction receipt'
    $r32GroundHeader=Get-RequiredPropertyValue $r32 'GroundHeaderAfter' 'R32 transaction receipt'
    $r32GroundSource=Get-RequiredPropertyValue $r32 'GroundSourceAfter' 'R32 transaction receipt'
    $r32CommitTree=@(Get-RequiredPropertyValue $r32 'NativePluginSourceTreeAfter' 'R32 transaction receipt')
    if (-not (Test-StateMatches $r32Map $ExpectedMapBytes $ExpectedMapSha256) -or
        -not (Test-StateMatches $r32Runtime $ExpectedRuntimeDllBytes $ExpectedRuntimeDllSha256) -or
        -not (Test-StateMatches $r32Editor $ExpectedEditorDllBytes $ExpectedEditorDllSha256) -or
        -not (Test-StateMatches $r32GroundHeader $ExpectedGroundHeaderBytes $ExpectedGroundHeaderSha256) -or
        -not (Test-StateMatches $r32GroundSource $ExpectedGroundSourceBytes $ExpectedGroundSourceSha256) -or
        $r32CommitTree.Count -le 0) {
        throw 'R32 transaction is not bound to the caller-pinned map/DLL/Ground boundary.'
    }

    $r32CaptureAdmission=Read-HashPinnedCommitReceipt `
        $R32CaptureCommitReceipt $ExpectedR32CaptureCommitReceiptSha256 `
        $r32EvidenceBase 'R32 capture receipt'
    $r32Capture=$r32CaptureAdmission.Receipt
    if ([string] (Get-RequiredPropertyValue $r32Capture 'Schema' 'R32 capture receipt') -cne $r32CaptureSchema -or
        [string] (Get-RequiredPropertyValue $r32Capture 'RunToken' 'R32 capture receipt') -cne $r32CaptureAdmission.Token -or
        [string] (Get-RequiredPropertyValue $r32Capture 'NativeOrder' 'R32 capture receipt') -cne 'R32_COMMIT_THEN_R32_CAPTURE_BEFORE_R33' -or
        [bool] (Get-RequiredPropertyValue $r32Capture 'R33DependencyAllowed' 'R32 capture receipt') -ne $false -or
        [bool] (Get-RequiredPropertyValue $r32Capture 'TreeMaterialResponseV3Preserved' 'R32 capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $r32Capture 'ConfirmedTenImagesReviewed' 'R32 capture receipt') -ne $true) {
        throw 'R32 capture receipt failed exact order or ten-image review truth.'
    }
    Assert-ExplicitHumanCaptureTruth `
        $r32Capture 'R32 capture receipt' 10 `
        'R32MediumDistanceTurfVisualQaAccepted' 'R33AdmissionAuthorized'
    Assert-AdmissionBinding `
        (Get-RequiredPropertyValue $r32Capture 'R32CommitAdmission' 'R32 capture receipt') `
        $r32CommitAdmission 'R32 capture transaction admission'
    $captureMap=Get-RequiredPropertyValue $r32Capture 'Map' 'R32 capture receipt'
    $captureRuntime=Get-RequiredPropertyValue $r32Capture 'RuntimeEditorDll' 'R32 capture receipt'
    $captureEditor=Get-RequiredPropertyValue $r32Capture 'EditorDll' 'R32 capture receipt'
    $captureGroundHeader=Get-RequiredPropertyValue $r32Capture 'GroundHeader' 'R32 capture receipt'
    $captureGroundSource=Get-RequiredPropertyValue $r32Capture 'GroundSource' 'R32 capture receipt'
    $captureTreeReceipt=Get-RequiredPropertyValue $r32Capture 'NativePluginSourceTree' 'R32 capture receipt'
    $captureTree=@(Get-RequiredPropertyValue $captureTreeReceipt 'BaselineAfterCaptureSourcePromotion' 'R32 capture source-tree receipt')
    $captureTreePre=@(Get-RequiredPropertyValue $captureTreeReceipt 'ImmediatePreCapture' 'R32 capture source-tree receipt')
    $captureTreePost=@(Get-RequiredPropertyValue $captureTreeReceipt 'ImmediatePostCapture' 'R32 capture source-tree receipt')
    $expectedGroundHeaderPath=[IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $sourcePins[2].RelativePath))
    $expectedGroundSourcePath=[IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $sourcePins[3].RelativePath))
    if (-not (Test-StateMatches $captureMap $ExpectedMapBytes $ExpectedMapSha256) -or
        -not (Test-StateMatches $captureRuntime $ExpectedRuntimeDllBytes $ExpectedRuntimeDllSha256) -or
        -not (Test-StateMatches $captureEditor $ExpectedEditorDllBytes $ExpectedEditorDllSha256) -or
        -not (Test-StateMatches $captureGroundHeader $ExpectedGroundHeaderBytes $ExpectedGroundHeaderSha256) -or
        -not (Test-StateMatches $captureGroundSource $ExpectedGroundSourceBytes $ExpectedGroundSourceSha256) -or
        -not [IO.Path]::GetFullPath([string] (Get-RequiredPropertyValue $captureGroundHeader 'Path' 'R32 GroundHeader capture receipt')).Equals($expectedGroundHeaderPath,[StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFullPath([string] (Get-RequiredPropertyValue $captureGroundSource 'Path' 'R32 GroundSource capture receipt')).Equals($expectedGroundSourcePath,[StringComparison]::OrdinalIgnoreCase) -or
        [bool] (Get-RequiredPropertyValue $captureGroundHeader 'Unchanged' 'R32 GroundHeader capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $captureGroundSource 'Unchanged' 'R32 GroundSource capture receipt') -ne $true -or
        [bool] (Get-RequiredPropertyValue $captureTreeReceipt 'Unchanged' 'R32 capture source-tree receipt') -ne $true -or
        [int] (Get-RequiredPropertyValue $captureTreeReceipt 'FileCount' 'R32 capture source-tree receipt') -ne $captureTree.Count -or
        -not [IO.Path]::GetFullPath([string] (Get-RequiredPropertyValue $captureTreeReceipt 'Root' 'R32 capture source-tree receipt')).Equals($nativePluginSourceRoot,[StringComparison]::OrdinalIgnoreCase) -or
        (ConvertTo-Json @(ConvertTo-TreeIdentity $captureTree) -Depth 6 -Compress) -cne (ConvertTo-Json @(ConvertTo-TreeIdentity $captureTreePre) -Depth 6 -Compress) -or
        (ConvertTo-Json @(ConvertTo-TreeIdentity $captureTree) -Depth 6 -Compress) -cne (ConvertTo-Json @(ConvertTo-TreeIdentity $captureTreePost) -Depth 6 -Compress)) {
        throw 'R32 capture is not bound to the caller-pinned unchanged map/DLL/Ground/source-tree boundary.'
    }
    $treeSha256=Get-TreeIdentitySha256 $captureTree
    if ($treeSha256 -cne $ExpectedNativePluginSourceTreeSha256.ToUpperInvariant()) {
        throw "R32 capture plugin-source-tree canonical SHA-256 mismatch: expected=$($ExpectedNativePluginSourceTreeSha256.ToUpperInvariant()) actual=$treeSha256"
    }
    Assert-R32CaptureTreeExtendsCommit $r32CommitTree $captureTree

    $treeMaterialResponseV3=Get-RequiredPropertyValue `
        $r32 'TreeMaterialResponseV3' 'R32 transaction receipt'
    $treeNativePins=@(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'NativeSourcePins' 'R32 TreeRealism v3 receipt')
    $treeClosurePins=@(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'SourceClosurePins' 'R32 TreeRealism v3 receipt')
    $treeBefore=@(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'ContentBefore' 'R32 TreeRealism v3 receipt')
    $treeAfter=@(Get-RequiredPropertyValue `
        $treeMaterialResponseV3 'ContentAfter' 'R32 TreeRealism v3 receipt')
    if ([string] (Get-RequiredPropertyValue $treeMaterialResponseV3 'Status' 'R32 TreeRealism v3 receipt') -cne
            'TREE_MATERIAL_RESPONSE_V3_CONTENT_DELTA_VALID' -or
        $treeNativePins.Count -ne 4 -or $treeClosurePins.Count -ne 6 -or
        $treeAfter.Count -ne $treeBefore.Count + 13 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'ResponseMaterialPackageCount' 'R32 TreeRealism v3 receipt') -ne 13 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'ReboundManagedMeshPackageCount' 'R32 TreeRealism v3 receipt') -ne 5 -or
        [int] (Get-RequiredPropertyValue $treeMaterialResponseV3 'RuntimeResponseMidCount' 'R32 TreeRealism v3 receipt') -ne 26 -or
        [bool] (Get-RequiredPropertyValue $treeMaterialResponseV3 'TreePlacementGeometryOpacityWindAuthorityModified' 'R32 TreeRealism v3 receipt') -ne $false) {
        throw 'R32 predecessor TreeRealism v3 receipt drifted.'
    }
    foreach ($upstreamTree in @(
        (Get-RequiredPropertyValue $r30 'TreeMaterialResponseV3' 'R30 transaction receipt'),
        (Get-RequiredPropertyValue $r31 'TreeMaterialResponseV3' 'R31 transaction receipt'),
        (Get-RequiredPropertyValue `
            (Get-RequiredPropertyValue $r32Capture 'R32CommitAdmission' 'R32 capture receipt') `
            'TreeMaterialResponseV3' 'R32 capture transaction admission'))) {
        if ((ConvertTo-Json $treeMaterialResponseV3 -Depth 16 -Compress) -cne
            (ConvertTo-Json $upstreamTree -Depth 16 -Compress)) {
            throw 'R30-R32 TreeRealism v3 identity chain drifted before R33.'
        }
    }
    $r32Acceptance=Get-RequiredPropertyValue `
        $r32Capture 'AcceptanceRevalidation' 'R32 capture receipt'
    if ([bool] (Get-RequiredPropertyValue `
            $r32Acceptance 'TreeMaterialResponseV3SourceAndContentClosureRevalidated' `
            'R32 acceptance revalidation') -ne $true) {
        throw 'R32 capture did not revalidate the TreeRealism v3 source/content closure.'
    }
    foreach ($pin in @($treeNativePins)+@($treeClosurePins)) {
        $relative=[string] (Get-RequiredPropertyValue $pin 'RelativePath' 'R33 TreeRealism v3 predecessor pin')
        Assert-CanonicalRelativePath $relative 'R33 TreeRealism v3 predecessor pin'
        [void] (Assert-State ([pscustomobject] @{
            Present=$true
            Bytes=[int64] (Get-RequiredPropertyValue $pin 'Bytes' 'R33 TreeRealism v3 predecessor pin')
            Sha256=[string] (Get-RequiredPropertyValue $pin 'Sha256' 'R33 TreeRealism v3 predecessor pin')
        }) ([IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative))) `
            'R33 retained TreeRealism v3 source pin')
    }
    [void] (Assert-TreeIdentity `
        $treeRealismContentRoot $treeAfter `
        'R33 predecessor exact TreeRealism v3 content')

    [pscustomobject] [ordered] @{
        R30Transaction=$r30CommitAdmission; R30Capture=$r30CaptureAdmission
        R31Transaction=$r31CommitAdmission; R31Capture=$r31CaptureAdmission
        R32Transaction=$r32CommitAdmission; R32Capture=$r32CaptureAdmission
        Map=$r32Map; RuntimeDll=$r32Runtime; EditorDll=$r32Editor
        GroundHeader=$captureGroundHeader; GroundSource=$captureGroundSource
        NativePluginSourceTree=@(ConvertTo-TreeIdentity $captureTree)
        NativePluginSourceTreeSha256=$treeSha256
        TreeMaterialResponseV3=$treeMaterialResponseV3
    }
}

function New-FileJournal {
    param([string[]] $Paths, [string] $BackupRoot)
    [void] [IO.Directory]::CreateDirectory($BackupRoot)
    $rows=[Collections.Generic.List[object]]::new()
    for ($index=0; $index -lt $Paths.Count; ++$index) {
        $path=[IO.Path]::GetFullPath($Paths[$index])
        $before=Get-FileState $path
        $backup=Join-Path $BackupRoot ("{0:D3}.bin" -f $index)
        if ($before.Present) { Copy-Item -LiteralPath $path -Destination $backup }
        $rows.Add([pscustomobject] [ordered] @{ Path=$path; Before=$before; Backup=$backup })
    }
    @($rows)
}

function Restore-FileJournal {
    param([object[]] $Journal)
    foreach ($row in $Journal) {
        $path=[IO.Path]::GetFullPath([string] $row.Path)
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "Refused out-of-project journal restore: $path"
        }
        if ([bool] $row.Before.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
            Copy-Item -LiteralPath $row.Backup -Destination $path -Force
        }
        elseif ([IO.File]::Exists($path)) {
            Remove-Item -LiteralPath $path -Force
        }
        [void] (Assert-State $row.Before $path 'restored journal file')
    }
}

function New-DirectoryPresenceJournal {
    param([string[]] $FilePaths)
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $rows=[Collections.Generic.List[object]]::new()
    foreach ($file in $FilePaths) {
        $directory=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($file))
        while ($directory -and (Test-ContainedPath $directory $nativeProjectRoot)) {
            if ($seen.Add($directory)) {
                $rows.Add([pscustomobject] [ordered] @{
                    Path=$directory; BeforePresent=[IO.Directory]::Exists($directory)
                })
            }
            $directory=[IO.Path]::GetDirectoryName($directory)
        }
    }
    @($rows)
}

function Restore-DirectoryPresenceJournal {
    param([object[]] $Journal)
    foreach ($row in @($Journal | Sort-Object { ([string] $_.Path).Length } -Descending)) {
        $path=[IO.Path]::GetFullPath([string] $row.Path)
        if (-not [bool] $row.BeforePresent -and [IO.Directory]::Exists($path)) {
            if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
                throw "Refused out-of-project directory cleanup: $path"
            }
            if (@(Get-ChildItem -LiteralPath $path -Force).Count -eq 0) {
                Remove-Item -LiteralPath $path -Force
            }
        }
    }
}

function New-TreeJournal {
    param([string[]] $Roots, [string] $BackupRoot)
    [void] [IO.Directory]::CreateDirectory($BackupRoot)
    $rows=[Collections.Generic.List[object]]::new()
    for ($index=0; $index -lt $Roots.Count; ++$index) {
        $root=[IO.Path]::GetFullPath($Roots[$index])
        if ($root -cne $pluginBinaryRoot -and $root -cne $pluginIntermediateRoot) {
            throw "Refused unapproved build-tree journal root: $root"
        }
        $present=[IO.Directory]::Exists($root)
        $before=if ($present) { @(Get-TreeIdentityReceipt $root) } else { @() }
        $backup=Join-Path $BackupRoot ("tree-{0:D2}" -f $index)
        if ($present) { Copy-Item -LiteralPath $root -Destination $backup -Recurse }
        $rows.Add([pscustomobject] [ordered] @{
            Root=$root; BeforePresent=$present; Before=@($before); Backup=$backup
        })
    }
    @($rows)
}

function Restore-TreeJournal {
    param([object[]] $Journal)
    foreach ($row in $Journal) {
        $root=[IO.Path]::GetFullPath([string] $row.Root)
        if ($root -cne $pluginBinaryRoot -and $root -cne $pluginIntermediateRoot) {
            throw "Refused unapproved build-tree restore root: $root"
        }
        if ([IO.Directory]::Exists($root)) {
            Remove-Item -LiteralPath $root -Recurse -Force
        }
        if ([bool] $row.BeforePresent) {
            Copy-Item -LiteralPath $row.Backup -Destination $root -Recurse
            [void] (Assert-TreeIdentity $root @($row.Before) 'restored build tree')
        }
        elseif ([IO.Directory]::Exists($root)) {
            throw "Absent build tree was not restored to absence: $root"
        }
    }
}

function Get-ProtectedProcesses {
    @(
        Get-CimInstance Win32_Process | Where-Object {
            ([string] $_.ExecutablePath).Equals($protectedEditor,[StringComparison]::OrdinalIgnoreCase) -or
            ([string] $_.CommandLine).Contains($protectedProject,[StringComparison]::OrdinalIgnoreCase)
        } | Sort-Object ProcessId | ForEach-Object {
            [pscustomobject] [ordered] @{
                ProcessId=[uint32] $_.ProcessId
                ParentProcessId=[uint32] $_.ParentProcessId
                CreationDate=[string] $_.CreationDate
                ExecutablePath=[string] $_.ExecutablePath
                CommandLine=[string] $_.CommandLine
            }
        }
    )
}

function Assert-ProtectedUnchanged {
    param([object[]] $Before)
    if ((ConvertTo-Json @($Before) -Depth 5 -Compress) -cne
        (ConvertTo-Json @(Get-ProtectedProcesses) -Depth 5 -Compress)) {
        throw 'Protected UE5.4/CAPSTONE process identity changed.'
    }
}

function Get-NativeProcesses {
    @(
        Get-CimInstance Win32_Process | Where-Object {
            ([string] $_.ExecutablePath).Equals($editor,[StringComparison]::OrdinalIgnoreCase) -or
            ([string] $_.CommandLine).Contains($nativeProjectFile,[StringComparison]::OrdinalIgnoreCase) -or
            (([string] $_.CommandLine).Contains($unrealBuildTool,[StringComparison]::OrdinalIgnoreCase) -and
             ([string] $_.CommandLine).Contains($nativeProjectFile,[StringComparison]::OrdinalIgnoreCase))
        }
    )
}

function Assert-NativeIdle {
    param([string] $Label)
    $active=@(Get-NativeProcesses)
    if ($active.Count -ne 0) {
        throw "$Label requires no UE5.5/TRIAD native process; count=$($active.Count)."
    }
}

function Assert-RcPortUnowned {
    $listeners=@(Get-NetTCPConnection -State Listen -LocalPort 30010 -ErrorAction SilentlyContinue)
    if ($listeners.Count -ne 0) { throw 'Remote Control port 30010 is already owned.' }
}

function Get-FreeVirtualBytes {
    $os=Get-CimInstance Win32_OperatingSystem
    [int64] $os.FreeVirtualMemory * 1024L
}

function Assert-LaunchAdmission {
    param([string] $Label)
    $free=Get-FreeVirtualBytes
    if ($free -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "$Label requires fixed 10 GiB free virtual memory; actual=$free."
    }
    [pscustomobject] [ordered] @{
        Label=$Label; FreeVirtualBytes=$free
        MinimumFreeVirtualBytes=$minimumSystemFreeVirtualAtLaunchBytes
        PrivateMemoryCeilingBytes=$privateMemoryCeilingBytes
        ContinuousMinimumFreeVirtualBytes=$minimumSystemFreeVirtualBytes
    }
}

function Register-OwnedProcess {
    param([Diagnostics.Process] $Handle, [string] $Label, [string] $ExpectedExecutable)
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    $row=$null
    do {
        $row=Get-CimInstance Win32_Process -Filter "ProcessId=$($Handle.Id)" -ErrorAction SilentlyContinue
        if ($null -ne $row -and -not [string]::IsNullOrWhiteSpace([string] $row.ExecutablePath)) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($null -eq $row -or
        -not ([string] $row.ExecutablePath).Equals($ExpectedExecutable,[StringComparison]::OrdinalIgnoreCase)) {
        try { if (-not $Handle.HasExited) { $Handle.Kill($true) } } catch {}
        throw "$Label executable identity could not be proved."
    }
    $owned=[pscustomobject] [ordered] @{
        ProcessId=[uint32] $row.ProcessId; CreationDate=[string] $row.CreationDate
        ExecutablePath=[string] $row.ExecutablePath; CommandLine=[string] $row.CommandLine
        Label=$Label; Handle=$Handle
    }
    $script:ownedProcesses.Add($owned)
    $script:memoryBreachStartedUtc=$null
    $script:memoryObservations.Clear()
    $owned
}

function Assert-OwnedIdentity {
    param($Owned)
    $row=Get-CimInstance Win32_Process -Filter "ProcessId=$($Owned.ProcessId)" -ErrorAction SilentlyContinue
    if ($null -eq $row -or [string] $row.CreationDate -cne [string] $Owned.CreationDate -or
        -not ([string] $row.ExecutablePath).Equals([string] $Owned.ExecutablePath,[StringComparison]::OrdinalIgnoreCase) -or
        [string] $row.CommandLine -cne [string] $Owned.CommandLine) {
        throw "Owned process identity changed: $($Owned.Label)."
    }
    $row
}

function Get-OwnedProcessTreeIds {
    param([uint32] $RootId)
    $all=@(Get-CimInstance Win32_Process)
    $ids=[Collections.Generic.HashSet[uint32]]::new()
    [void] $ids.Add($RootId)
    do {
        $changed=$false
        foreach ($row in $all) {
            if ($ids.Contains([uint32] $row.ParentProcessId) -and
                $ids.Add([uint32] $row.ProcessId)) { $changed=$true }
        }
    } while ($changed)
    @($ids)
}

function Assert-ContinuousMemoryHealthy {
    param($Owned, [string] $Label)
    $free=Get-FreeVirtualBytes
    $private=0L
    foreach ($id in @(Get-OwnedProcessTreeIds $Owned.ProcessId)) {
        $process=Get-Process -Id $id -ErrorAction SilentlyContinue
        if ($null -ne $process) { $private += [int64] $process.PrivateMemorySize64 }
    }
    $now=[DateTime]::UtcNow
    $breached=$free -lt $minimumSystemFreeVirtualBytes -or
        $private -gt $privateMemoryCeilingBytes
    $script:memoryObservations.Add([pscustomobject] [ordered] @{
        Utc=$now.ToString('o'); Label=$Label; FreeVirtualBytes=$free
        OwnedTreePrivateBytes=$private; Breached=$breached
    })
    if ($breached) {
        if ($null -eq $script:memoryBreachStartedUtc) {
            $script:memoryBreachStartedUtc=$now
        }
        elseif (($now-$script:memoryBreachStartedUtc).TotalMilliseconds -ge
                $memoryPersistentBreachMilliseconds) {
            throw "$Label exceeded the fixed continuous 6 GiB free-virtual / 12 GiB owned-private guard for 2 seconds."
        }
    } else {
        $script:memoryBreachStartedUtc=$null
    }
}

function Close-OwnedProcess {
    param($Owned, [switch] $AllowContainment)
    $handle=$Owned.Handle
    if ($handle.HasExited) { return }
    [void] (Assert-OwnedIdentity $Owned)
    if (-not $AllowContainment) { throw 'Owned process containment was not authorized.' }
    $handle.Kill($true)
    if (-not $handle.WaitForExit(30000)) {
        throw "Owned process tree did not exit: $($Owned.Label)."
    }
}

function Wait-NativeMutationQuiescence {
    param([string] $Label, [int] $TimeoutSeconds=60)
    $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    foreach ($owned in @($script:ownedProcesses)) {
        $handle=$owned.Handle
        while (-not $handle.HasExited -and [DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 250
        }
        if (-not $handle.HasExited) { throw "$Label timed out on owned process $($owned.ProcessId)." }
    }
    Assert-NativeIdle $Label
    [pscustomobject] [ordered] @{ Status='QUIESCENT'; Label=$Label; OwnedProcessCount=$script:ownedProcesses.Count }
}

function Invoke-RcCall {
    param([string] $ObjectPath, [string] $FunctionName, [hashtable] $Parameters, [int] $TimeoutSeconds)
    $body=[ordered] @{
        objectPath=$ObjectPath; functionName=$FunctionName
        parameters=$Parameters; generateTransaction=$false
    } | ConvertTo-Json -Depth 8 -Compress
    Invoke-RestMethod -Method Put -Uri $rcUri -ContentType 'application/json' `
        -Body $body -TimeoutSec $TimeoutSeconds
}

function Get-OwnedHelperIdentity {
    param($Owned, [string] $Log)
    [void] (Assert-OwnedIdentity $Owned)
    if (-not ([string] $Owned.CommandLine).Contains($mapPackage,[StringComparison]::Ordinal) -or
        -not ([string] $Owned.CommandLine).Contains($Log,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Could not prove exact R33 helper map/log identity.'
    }
    [pscustomobject] [ordered] @{
        ProcessId=[uint32] $Owned.ProcessId; CreationDate=[string] $Owned.CreationDate
        ExecutablePath=[string] $Owned.ExecutablePath; CommandLine=[string] $Owned.CommandLine
    }
}

function Stop-OwnedHelper {
    param($Owned, $Identity, [string] $Log)
    $handle=$Owned.Handle
    if ([uint32] $handle.Id -ne [uint32] $Identity.ProcessId) {
        throw 'Helper handle/identity mismatch.'
    }
    try { [void] (Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30) } catch {}
    if (-not $handle.WaitForExit($ShutdownTimeoutSeconds*1000)) {
        $current=Get-OwnedHelperIdentity $Owned $Log
        if ([string] $current.CreationDate -cne [string] $Identity.CreationDate -or
            [string] $current.CommandLine -cne [string] $Identity.CommandLine) {
            throw 'Timed-out helper no longer matches exact owned identity.'
        }
        Close-OwnedProcess $Owned -AllowContainment
    }
}

function Invoke-GuardedOwnedBuild {
    param([string[]] $Arguments, [string] $StandardOutputLog, [string] $StandardErrorLog)
    Assert-NativeIdle 'before guarded owned R33 UBT build'
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Assert-LaunchAdmission 'before guarded owned R33 UBT build')
    foreach ($logPath in @($StandardOutputLog,$StandardErrorLog)) {
        if ([IO.File]::Exists($logPath)) { throw "R33 build log already exists: $logPath" }
    }
    $escaped=@(
        ('"{0}"' -f $unrealBuildTool)
        $Arguments | ForEach-Object {
            if ($_ -match '[\s"]') { '"{0}"' -f $_.Replace('"','\"') } else { $_ }
        }
    )
    $argumentLine=[string]::Join(' ', $escaped)
    $handle=Start-Process -FilePath $dotnet -ArgumentList $argumentLine `
        -WorkingDirectory $engineSourceRoot `
        -RedirectStandardOutput $StandardOutputLog `
        -RedirectStandardError $StandardErrorLog -PassThru -WindowStyle Hidden
    $owned=Register-OwnedProcess $handle 'R33 serial UBT build' $dotnet
    $deadline=[DateTime]::UtcNow.AddSeconds($BuildTimeoutSeconds)
    $failure=$null
    try {
        while (-not $handle.WaitForExit($memoryPollMilliseconds)) {
            Assert-ContinuousMemoryHealthy $owned 'R33 UBT build'
            if ([DateTime]::UtcNow -ge $deadline) {
                throw 'R33 UBT build exceeded bounded timeout.'
            }
        }
        if ($handle.ExitCode -ne 0) {
            throw "R33 UBT build failed with exit code $($handle.ExitCode)."
        }
    }
    catch { $failure=$_.Exception }
    finally {
        if (-not $handle.HasExited) {
            try { Close-OwnedProcess $owned -AllowContainment }
            catch { if ($null -eq $failure) { $failure=$_.Exception } }
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'after guarded owned R33 UBT build'
    if ($null -ne $failure) { throw $failure }
    if (-not [IO.File]::Exists($StandardOutputLog) -or
        (Get-Item -LiteralPath $StandardOutputLog).Length -le 0) {
        throw 'R33 build did not persist a non-empty stdout log.'
    }
    [pscustomobject] [ordered] @{
        ProcessId=[int] $handle.Id; ExitCode=[int] $handle.ExitCode
        SerialBuild=$true; MaxParallelActions=1
        MemoryObservations=@($script:memoryObservations)
        StandardOutputLog=Get-FileState $StandardOutputLog
        StandardErrorLog=Get-FileState $StandardErrorLog
    }
}

function Invoke-ColdStage {
    param(
        [string] $Stage,
        [string] $ObjectPath,
        [string] $FunctionName,
        [string] $TextProperty,
        [string] $ExpectedPrefix,
        [hashtable] $Parameters=@{}
    )
    Assert-NativeIdle "before $Stage"
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Assert-LaunchAdmission "before helper $Stage")
    Assert-RcPortUnowned
    $logRoot=Join-Path $transactionRoot 'logs'
    [void] [IO.Directory]::CreateDirectory($logRoot)
    $log=[IO.Path]::GetFullPath((Join-Path $logRoot "$Stage.log"))
    if ([IO.File]::Exists($log)) { throw "Stage log already exists: $log" }
    $argumentLine="`"$nativeProjectFile`" $mapPackage -d3d12 -sm6 " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-RemoteControlHttpServer -RCWebControlEnable -ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$log`""
    $handle=Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $nativeProjectRoot -PassThru -WindowStyle Hidden
    $owned=Register-OwnedProcess $handle "R33 helper $Stage" $editor
    $identity=$null
    $response=$null
    $failure=$null
    try {
        $identity=Get-OwnedHelperIdentity $owned $log
        $ready=$null
        $deadline=[DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
        do {
            if ($handle.HasExited) { throw "R33 helper exited before RC readiness: $Stage" }
            Assert-ContinuousMemoryHealthy $owned "$Stage RC readiness"
            try {
                $ready=Invoke-RcCall $identityLibrary `
                    'ValidateIstanaExploreRemoteControlProject' `
                    @{ ExpectedProjectPath=$nativeProjectRoot } 30
            }
            catch { $ready=$null }
            if ($null -ne $ready -and $ready.ReturnValue -eq $true) { break }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $deadline)
        if ($null -eq $ready -or $ready.ReturnValue -ne $true) {
            throw "R33 RC identity did not become ready for $Stage."
        }
        Assert-ContinuousMemoryHealthy $owned "$Stage before endpoint"
        $response=Invoke-RcCall $ObjectPath $FunctionName $Parameters $EditorTimeoutSeconds
        Assert-ContinuousMemoryHealthy $owned "$Stage after endpoint"
        $text=[string] (Get-RequiredPropertyValue $response $TextProperty "$Stage response")
        if ((Get-RequiredPropertyValue $response 'ReturnValue' "$Stage response") -ne $true -or
            -not $text.StartsWith($ExpectedPrefix,[StringComparison]::Ordinal)) {
            throw "Stage $Stage failed exact response gate: $text"
        }
    }
    catch { $failure=$_.Exception }
    finally {
        if (-not $handle.HasExited -and $null -ne $identity) {
            try { Stop-OwnedHelper $owned $identity $log }
            catch { if ($null -eq $failure) { $failure=$_.Exception } }
        }
        elseif (-not $handle.HasExited) {
            try { Close-OwnedProcess $owned -AllowContainment }
            catch { if ($null -eq $failure) { $failure=$_.Exception } }
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle "after $Stage"
    if ($null -ne $failure) { throw $failure }
    if (-not [IO.File]::Exists($log) -or
        (Get-Item -LiteralPath $log).Length -le 0) {
        throw "Stage $Stage lacks a non-empty log."
    }
    [pscustomobject] [ordered] @{
        Stage=$Stage; Function=$FunctionName; ExpectedPrefix=$ExpectedPrefix
        Message=[string] $response.$TextProperty; ProcessIdentity=$identity
        MemoryObservations=@($script:memoryObservations); Log=Get-FileState $log
    }
}

function Test-BinaryContainsMarker {
    param([string] $Path, [string] $Marker)
    $bytes=[IO.File]::ReadAllBytes($Path)
    $text=[Text.Encoding]::ASCII.GetString($bytes) +
        [Text.Encoding]::Unicode.GetString($bytes)
    $text.Contains($Marker,[StringComparison]::Ordinal)
}

function Assert-BuiltR33BinaryClosure {
    foreach ($marker in @(
        'ConfigureR33CesiumWorldTerrainReference',
        'ValidateR33CesiumWorldTerrainReference',
        'RegisterR33CesiumWorldTerrainController',
        'VISUAL_REFERENCE_ONLY_R33_CESIUM_WORLD_TERRAIN')) {
        if (-not (Test-BinaryContainsMarker $runtimeDll $marker)) {
            throw "Runtime DLL lacks R33 marker: $marker"
        }
    }
    foreach ($marker in @(
        ('CommitR33CesiumWorldTerrainReference' + 'ToLoadedV5DHybridMap'),
        'ValidateR33CesiumWorldTerrainReferenceInLoadedV5DHybridMap')) {
        if (-not (Test-BinaryContainsMarker $editorDll $marker)) {
            throw "Editor DLL lacks R33 marker: $marker"
        }
    }
}

function Assert-StaticContract {
    if ($sourcePins.Count -ne 9 -or $sourceAssetPins.Count -ne 1 -or
        $retainedR32Pins.Count -ne 9) {
        throw 'R33 closure must be exactly 9 C++ + 1 contract and nine retained R32 compile surfaces.'
    }
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in @($sourcePins)+@($sourceAssetPins)) {
        Assert-CanonicalRelativePath $pin.RelativePath 'R33 closure'
        if (-not $seen.Add([string] $pin.RelativePath)) {
            throw "Duplicate R33 closure path: $($pin.RelativePath)"
        }
        Assert-PinAtRoot $pin $repositoryUnrealRoot
    }
    foreach ($pin in $retainedR32Pins) {
        Assert-CanonicalRelativePath $pin.RelativePath 'retained R32'
        Assert-PinAtRoot $pin $repositoryUnrealRoot
    }
    if (@($sourcePins | Where-Object { $_.NativeBeforePresent }).Count -ne 5 -or
        @($sourcePins | Where-Object { -not $_.NativeBeforePresent }).Count -ne 4 -or
        @($sourceAssetPins | Where-Object { -not $_.NativeBeforePresent }).Count -ne 1) {
        throw 'R33 must replace five R33-aware integration files and add four C++ files plus one contract.'
    }

    $contextHeader=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[0].RelativePath) -Raw
    $contextSource=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[1].RelativePath) -Raw
    $groundSource=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[3].RelativePath) -Raw
    $actorSource=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[5].RelativePath) -Raw
    $editorSource=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[7].RelativePath) -Raw
    $r29TerrainSource=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[8].RelativePath) -Raw
    foreach ($marker in @(
        'RegisterR33CesiumWorldTerrainController',
        'ValidateR33CesiumPresentationBinding',
        'bR33DualCesiumContextConfigured',
        '#include "CesiumIonServer.h"',
        '!IsValid(GoogleTileset->GetCesiumIonServer())',
        '!IsValid(CwtTileset->GetCesiumIonServer())')) {
        if (-not ($contextHeader.Contains($marker,[StringComparison]::Ordinal) -or
                  $contextSource.Contains($marker,[StringComparison]::Ordinal))) {
            throw "R33 ContextPolicy closure lacks marker: $marker"
        }
    }
    foreach ($marker in @(
        'ShouldHideSourceTerrainRendererForVisualContext',
        'bR33DualCesiumContextConfigured')) {
        if (-not $groundSource.Contains($marker,[StringComparison]::Ordinal)) {
            throw "R33 Ground closure lacks marker: $marker"
        }
    }
    foreach ($marker in @(
        'GooglePrimary','CwtWarming','CwtPresented','SafeLocal',
        'GooglePhotorealisticIonAssetId = 2275207',
        'CesiumWorldTerrainIonAssetId = 1',
        '#include "CesiumIonServer.h"',
        '!IsValid(Tileset->GetCesiumIonServer())',
        '!IsValid(InCwtTileset->GetCesiumIonServer())',
        'VISUAL_REFERENCE_ONLY_R33_CESIUM_WORLD_TERRAIN')) {
        if (-not $actorSource.Contains($marker,[StringComparison]::Ordinal)) {
            throw "R33 runtime closure lacks marker: $marker"
        }
    }
    foreach ($marker in @(
        ('CommitR33CesiumWorldTerrainReference' + 'ToLoadedV5DHybridMap'),
        'ValidateR33CesiumWorldTerrainReferenceInLoadedV5DHybridMap',
        '#include "CesiumIonServer.h"',
        '!IsValid(OutRoster.Google->GetCesiumIonServer())',
        '!IsValid(OutRoster.Cwt->GetCesiumIonServer())',
        'rollbackOwnedByWrapper=true',
        'sourceTerrainAuthorityUnchanged=true',
        'verticalDatumResolved=false',
        'surveyAccuracyClaimed=false')) {
        if (-not $editorSource.Contains($marker,[StringComparison]::Ordinal)) {
            throw "R33 editor closure lacks marker: $marker"
        }
    }
    foreach ($marker in @(
        'const bool bPresentFallback',
        'Policy->bR33DualCesiumContextConfigured',
        'Policy->ShouldPresentR29CopernicusFallback()')) {
        if (-not $r29TerrainSource.Contains($marker,[StringComparison]::Ordinal)) {
            throw "R33 R29 terrain-consumer closure lacks marker: $marker"
        }
    }

    $contract=Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourceAssetPins[0].RelativePath) -Raw |
        ConvertFrom-Json -Depth 64
    if ([string] $contract.schema -cne 'triad.istana_explore_v5d.r33_cesium_world_terrain_reference.v1' -or
        [string] $contract.claim -cne 'VISUAL_REFERENCE_ONLY_R33_CESIUM_WORLD_TERRAIN' -or
        [bool] $contract.nativeOrdering.nativeTransactionImplemented -ne $true -or
        [bool] $contract.nativeOrdering.nativeTransactionExecuted -ne $false -or
        [bool] $contract.nativeOrdering.prerequisitesSatisfiedForThisDelivery -ne $false -or
        [bool] $contract.nativeOrdering.r30ExplicitHumanReviewReceiptRequired -ne $true -or
        [bool] $contract.nativeOrdering.r31ExplicitHumanReviewReceiptRequired -ne $true -or
        [bool] $contract.nativeOrdering.r32ExplicitHumanReviewReceiptRequired -ne $true -or
        [int] $contract.nativeTransaction.exactPromotion.cppFileCount -ne 9 -or
        [int] $contract.nativeTransaction.exactPromotion.contractFileCount -ne 1 -or
        [int] $contract.nativeTransaction.exactPromotion.contentPackageCount -ne 0 -or
        [int] $contract.nativeTransaction.predecessorReceipts.exactCount -ne 6 -or
        [bool] $contract.nativeTransaction.predecessorReceipts.currentR30MechanicalOnlyReceiptAdmissible -ne $false -or
        [string] $contract.nativeTransaction.predecessorReceipts.r31CaptureSchema -cne $r31CaptureSchema -or
        [int] $contract.nativeTransaction.predecessorReceipts.r31ExactPoseCount -ne 5 -or
        [int] $contract.nativeTransaction.predecessorReceipts.r31ExactImageCount -ne 6 -or
        [bool] $contract.nativeTransaction.predecessorReceipts.r31NaniteRasterPairReviewRequired -ne $true -or
        [bool] $contract.nativeTransaction.predecessorReceipts.r31HumanNaniteRasterComparisonAttestationRequired -ne $true -or
        [bool] $contract.nativeTransaction.predecessorReceipts.r31NaniteRasterAppearanceParityAcceptanceRequired -ne $true -or
        [bool] $contract.nativeTransaction.predecessorReceipts.r31SixPngAcceptanceRevalidationRequired -ne $true -or
        [bool] $contract.nativeTransaction.guardrails.serialBuild -ne $true -or
        [bool] $contract.nativeTransaction.guardrails.wholePluginSourceTreeBoundBeforeAndAfter -ne $true -or
        [bool] $contract.nativeTransaction.receiptTruth.visualCaptureAccepted -ne $false -or
        [bool] $contract.nativeTransaction.receiptTruth.accurateRealWorldTerrainClaimed -ne $false -or
        [bool] $contract.nativeTransaction.receiptTruth.surveyAccuracyClaimed -ne $false -or
        [bool] $contract.nativeTransaction.receiptTruth.verticalDatumResolved -ne $false -or
        [bool] $contract.sharedIonServer.exactNonNullOpaqueServerRequired -ne $true -or
        [bool] $contract.sharedIonServer.serverObjectValidityProvesTokenPresence -ne $false -or
        [bool] $contract.sharedIonServer.serverObjectValidityProvesAssetEntitlement -ne $false -or
        [bool] $contract.sharedIonServer.ionTokenValueOrFingerprintRecordedByTriad -ne $false -or
        [bool] $contract.sharedIonServer.runtimeResolverConfigureValidateApplyAndReadinessBoundariesRequireNonNullServer -ne $true -or
        [bool] $contract.sharedIonServer.contextResolverRegisterApplyAndValidateBoundariesRequireNonNullServer -ne $true -or
        [bool] $contract.sharedIonServer.editorResolverPredecessorApplyAndSuccessorValidationRequireNonNullServer -ne $true -or
        [bool] $contract.privacyAndProviderBoundary.nonNullIonServerProvesTokenOrEntitlement -ne $false -or
        [bool] $contract.runtimeActor.configurationRejectsNullOrInvalidOpaqueIonServer -ne $true -or
        [bool] $contract.runtimeActor.validationRejectsNullOrInvalidOpaqueIonServer -ne $true -or
        [bool] $contract.runtimeActor.presentationApplyRejectsNullOrInvalidOpaqueIonServer -ne $true -or
        [bool] $contract.runtimeActor.readinessRequestAndTickRejectNullOrInvalidOpaqueIonServer -ne $true -or
        [int] $contract.cesiumPluginCompatibility.descriptorVersion -ne $expectedCesiumDescriptorVersion -or
        [string] $contract.cesiumPluginCompatibility.versionName -cne $expectedCesiumDescriptorVersionName -or
        [string] $contract.cesiumPluginCompatibility.engineVersion -cne $expectedCesiumDescriptorEngineVersion -or
        [int64] $contract.cesiumPluginCompatibility.descriptorBytes -ne $expectedCesiumDescriptorBytes -or
        [string] $contract.cesiumPluginCompatibility.descriptorSha256 -cne $expectedCesiumDescriptorSha256 -or
        [bool] $contract.cesiumPluginCompatibility.verifyBeforeNativeWrite -ne $true -or
        [bool] $contract.cesiumPluginCompatibility.receiptRecordsDescriptorIdentity -ne $true) {
        throw 'R33 contract truth drifted from the guarded source-only transaction.'
    }
    $promotionFiles=@($contract.nativeTransaction.exactPromotion.files)
    if ($promotionFiles.Count -ne 10 -or
        $promotionFiles -cnotcontains 'Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp') {
        throw 'R33 contract omits the state-aware R29 Copernicus terrain consumer.'
    }
    [pscustomobject] [ordered] @{
        Schema=$schema; Status='STATIC_SELF_CHECK_PASS'
        CodeSourceCount=9; SourceContractCount=1; NativeContentPackageCount=0
        ReplacementFileCount=5; NewCodeFileCount=4
        PredecessorReceiptParserCount=6; ExplicitHumanReviewRequired=$true
        CurrentMechanicalOnlyR30ReceiptAdmissible=$false
        R31CaptureSchema=$r31CaptureSchema
        R31ExactImageCount=6
        R31NaniteRasterPairReviewRequired=$true
        CallerPinnedFullPluginSourceTree=$true
        FixedLaunchFreeVirtualBytes=$minimumSystemFreeVirtualAtLaunchBytes
        FixedPrivateMemoryCeilingBytes=$privateMemoryCeilingBytes
        FixedContinuousFreeVirtualBytes=$minimumSystemFreeVirtualBytes
        MemoryPollMilliseconds=$memoryPollMilliseconds
        MemoryPersistentBreachMilliseconds=$memoryPersistentBreachMilliseconds
        CesiumDescriptorVersion=$expectedCesiumDescriptorVersion
        CesiumVersionName=$expectedCesiumDescriptorVersionName
        CesiumEngineVersion=$expectedCesiumDescriptorEngineVersion
        CesiumDescriptorIdentityPinned=$true
        NonNullOpaqueIonServerRequired=$true
        IonTokenValueOrFingerprintInspected=$false
        NonNullServerProvesTokenOrEntitlement=$false
        SerialBuild=$true; ExactCommitR33InvocationCount=1
        NativeProjectAccessed=$false; NativeTreeWritten=$false
        UnrealLaunched=$false; NativeExecutionEligibleNow=$false
    }
}

$staticReceipt=Assert-StaticContract
if ($StaticSelfCheck -and $Execute) {
    throw '-StaticSelfCheck and -Execute are mutually exclusive.'
}
if ($StaticSelfCheck) {
    $staticReceipt | ConvertTo-Json -Depth 16
    exit 0
}
if (-not $Execute) {
    [pscustomobject] [ordered] @{
        Schema=$schema; Status='READ_ONLY_REPOSITORY_PREFLIGHT_PASS'
        ExecuteRequiredForNativeAccess=$true
        NativeProjectAccessed=$false; NativeTreeWritten=$false
        UnrealLaunched=$false; StaticReceipt=$staticReceipt
    } | ConvertTo-Json -Depth 16
    exit 0
}

foreach ($required in @(
    'RequireR32Predecessor',
    'R30CommitReceipt','ExpectedR30CommitReceiptSha256',
    'R30CaptureCommitReceipt','ExpectedR30CaptureCommitReceiptSha256',
    'R31CommitReceipt','ExpectedR31CommitReceiptSha256',
    'R31CaptureCommitReceipt','ExpectedR31CaptureCommitReceiptSha256',
    'R32CommitReceipt','ExpectedR32CommitReceiptSha256',
    'R32CaptureCommitReceipt','ExpectedR32CaptureCommitReceiptSha256',
    'ExpectedMapBytes','ExpectedMapSha256',
    'ExpectedRuntimeDllBytes','ExpectedRuntimeDllSha256',
    'ExpectedEditorDllBytes','ExpectedEditorDllSha256',
    'ExpectedGroundHeaderBytes','ExpectedGroundHeaderSha256',
    'ExpectedGroundSourceBytes','ExpectedGroundSourceSha256',
    'ExpectedNativePluginSourceTreeSha256')) {
    if (-not $PSBoundParameters.ContainsKey($required)) {
        throw "Live R33 transaction requires explicit parameter: $required"
    }
}
if (-not $RequireR32Predecessor) {
    throw 'Live R33 transaction requires -RequireR32Predecessor.'
}
foreach ($bytes in @(
    $ExpectedMapBytes,$ExpectedRuntimeDllBytes,$ExpectedEditorDllBytes,
    $ExpectedGroundHeaderBytes,$ExpectedGroundSourceBytes)) {
    if ([int64] $bytes -le 0) {
        throw 'All live R33 preimage byte counts must be positive.'
    }
}
foreach ($hash in @(
    $ExpectedMapSha256,$ExpectedRuntimeDllSha256,$ExpectedEditorDllSha256,
    $ExpectedGroundHeaderSha256,$ExpectedGroundSourceSha256,
    $ExpectedNativePluginSourceTreeSha256)) {
    if ($hash -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'All live R33 preimage SHA-256 values must be explicit.'
    }
}
if ($ExpectedGroundHeaderBytes -ne 24974L -or
    $ExpectedGroundHeaderSha256.ToUpperInvariant() -cne
        '95122779D264BB0539AEC25A93D88C6485E5D4DB89E8F50B855D4EF1C82302E2' -or
    $ExpectedGroundSourceBytes -ne 249537L -or
    $ExpectedGroundSourceSha256.ToUpperInvariant() -cne
        '8B71713A4C134132BEB7ECE6DDCB93F86E690BB073F4508E325CD1BDAC5C520C') {
    throw 'R33 requires the exact immutable R32 pre-R33 Ground pair as its caller-pinned predecessor.'
}
if (-not (Test-ContainedPath $transactionRoot $transactionBase) -or
    [IO.Path]::GetDirectoryName($transactionRoot) -cne $transactionBase) {
    throw 'R33 transaction root is not an exact direct bounded token child.'
}

$transactionStarted=$false
$committed=$false
$failure=$null
$sourceJournal=@()
$sourceDirectoryJournal=@()
$mapJournal=@()
$buildJournal=@()
$admission=$null
$expectedPostTree=$null
try {
    if ([IO.Directory]::Exists($transactionRoot)) {
        throw "R33 transaction token already exists: $transactionRoot"
    }
    foreach ($requiredFile in @(
        $nativeProjectFile,$dotnet,$unrealBuildTool,$editor,
        $mapFile,$runtimeDll,$editorDll)) {
        if (-not [IO.File]::Exists($requiredFile)) {
            throw "Required live R33 input is absent: $requiredFile"
        }
    }

    $script:protectedBefore=@(Get-ProtectedProcesses)
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before R33 native preflight'
    Assert-RcPortUnowned
    $prewriteAdmission=Assert-LaunchAdmission 'before any R33 native write'
    $cesiumPlugin=Get-VerifiedCesiumPluginDescriptor
    $admission=Assert-SixPredecessorReceipts
    $treeMaterialResponseV3Before=@(Assert-TreeIdentity `
        $treeRealismContentRoot `
        @($admission.TreeMaterialResponseV3.ContentAfter) `
        'pre-R33 exact TreeRealism v3 content')

    $expectedMap=[pscustomobject] [ordered] @{
        Present=$true; Bytes=$ExpectedMapBytes
        Sha256=$ExpectedMapSha256.ToUpperInvariant()
    }
    $expectedRuntime=[pscustomobject] [ordered] @{
        Present=$true; Bytes=$ExpectedRuntimeDllBytes
        Sha256=$ExpectedRuntimeDllSha256.ToUpperInvariant()
    }
    $expectedEditor=[pscustomobject] [ordered] @{
        Present=$true; Bytes=$ExpectedEditorDllBytes
        Sha256=$ExpectedEditorDllSha256.ToUpperInvariant()
    }
    [void] (Assert-State $expectedMap $mapFile 'caller-pinned R32 map')
    [void] (Assert-State $expectedRuntime $runtimeDll 'caller-pinned R32 runtime DLL')
    [void] (Assert-State $expectedEditor $editorDll 'caller-pinned R32 editor DLL')
    foreach ($pin in $sourcePins) {
        Assert-PinAtRoot $pin $nativeProjectRoot -NativeBefore
    }
    foreach ($pin in $sourceAssetPins) {
        Assert-PinAtRoot $pin $nativeProjectRoot -NativeBefore
    }
    foreach ($pin in $retainedR32Pins) {
        Assert-PinAtRoot $pin $nativeProjectRoot
    }
    $preTree=@(Assert-TreeIdentity `
        $nativePluginSourceRoot $admission.NativePluginSourceTree `
        'caller-pinned accepted R32 plugin source tree')
    $preTreeSha=Get-TreeIdentitySha256 $preTree
    if ($preTreeSha -cne $ExpectedNativePluginSourceTreeSha256.ToUpperInvariant() -or
        $preTreeSha -cne $admission.NativePluginSourceTreeSha256) {
        throw 'Native plugin source tree failed the independent caller/capture canonical digest gate.'
    }
    $preContext=Get-Content -LiteralPath (Join-Path $nativeProjectRoot $sourcePins[1].RelativePath) -Raw
    $preGround=Get-Content -LiteralPath (Join-Path $nativeProjectRoot $sourcePins[3].RelativePath) -Raw
    foreach ($forbidden in @(
        'RegisterR33CesiumWorldTerrainController',
        'bR33DualCesiumContextConfigured',
        'ShouldHideSourceTerrainRendererForVisualContext',
        'TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor')) {
        if ($preContext.Contains($forbidden,[StringComparison]::Ordinal) -or
            $preGround.Contains($forbidden,[StringComparison]::Ordinal)) {
            throw "R32 preimage is contaminated by R33 symbol: $forbidden"
        }
    }
    $preR29Terrain=Get-Content -LiteralPath (Join-Path $nativeProjectRoot $sourcePins[8].RelativePath) -Raw
    foreach ($forbidden in @(
        'bR33DualCesiumContextConfigured',
        'ShouldPresentR29CopernicusFallback')) {
        if ($preR29Terrain.Contains($forbidden,[StringComparison]::Ordinal)) {
            throw "R32 R29-terrain preimage is contaminated by R33 symbol: $forbidden"
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'immediately before R33 journal creation'

    [void] [IO.Directory]::CreateDirectory($transactionRoot)
    $transactionStarted=$true
    $journalRoot=Join-Path $transactionRoot 'journal'
    [void] [IO.Directory]::CreateDirectory($journalRoot)
    $sourceDestinations=@(
        @($sourcePins)+@($sourceAssetPins) | ForEach-Object {
            [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath))
        }
    )
    $sourceJournal=@(New-FileJournal $sourceDestinations (Join-Path $journalRoot 'source'))
    $sourceDirectoryJournal=@(New-DirectoryPresenceJournal $sourceDestinations)
    $mapJournal=@(New-FileJournal @($mapFile) (Join-Path $journalRoot 'map'))
    $buildJournal=@(New-TreeJournal `
        @($pluginBinaryRoot,$pluginIntermediateRoot) `
        (Join-Path $journalRoot 'build'))
    $backupMap=[IO.Path]::GetFullPath((Join-Path $transactionRoot 'Istana_PublicView_Explore_v5d_hybrid.r32-predecessor.umap'))
    Copy-Item -LiteralPath $mapFile -Destination $backupMap
    [void] (Assert-State $expectedMap $backupMap 'external byte-identical R32 map backup')

    foreach ($pin in @($sourcePins)+@($sourceAssetPins)) {
        $source=[IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot $pin.RelativePath))
        $destination=[IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $pin.RelativePath))
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        Copy-Item -LiteralPath $source -Destination $destination
        Assert-PinAtRoot $pin $nativeProjectRoot
    }
    foreach ($pin in $retainedR32Pins) {
        Assert-PinAtRoot $pin $nativeProjectRoot
    }
    $expectedPostTree=@(Get-ExpectedPostR33PluginSourceTree $preTree)
    [void] (Assert-TreeIdentity `
        $nativePluginSourceRoot $expectedPostTree `
        'exact post-promotion R33 plugin source tree')
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before R33 build'

    $buildArguments=@(
        'UnrealEditor','Win64','Development',"-Project=$nativeProjectFile",
        '-WaitMutex','-NoHotReloadFromIDE','-Module=TRIADSensorFusion',
        '-Module=TRIADSensorFusionEditor','-ForceHeaderGeneration',
        '-NoUBTMakefiles','-MaxParallelActions=1','-NoUBA','-NoUBALocal'
    )
    $buildReceipt=Invoke-GuardedOwnedBuild $buildArguments `
        (Join-Path $transactionRoot 'build.stdout.log') `
        (Join-Path $transactionRoot 'build.stderr.log')
    $runtimeAfter=Get-FileState $runtimeDll
    $editorAfter=Get-FileState $editorDll
    if (-not $runtimeAfter.Present -or -not $editorAfter.Present -or
        $runtimeAfter.Sha256 -ceq $expectedRuntime.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedEditor.Sha256) {
        throw 'Forced R33 build did not produce fresh runtime and editor DLL receipts.'
    }
    Assert-BuiltR33BinaryClosure
    [void] (Assert-TreeIdentity `
        $nativePluginSourceRoot $expectedPostTree `
        'post-build exact R33 plugin source tree')

    $stageResults=[Collections.Generic.List[object]]::new()
    $stageResults.Add((Invoke-ColdStage `
        '01_cold_validate_exact_captured_r32_predecessor' $r32Library `
        'ValidateR32MediumDistanceTurfInLoadedV5DHybridMap' 'OutReport' `
        'ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_MAP_VALID'))
    [void] (Assert-State $expectedMap $mapFile 'R32 map after cold predecessor validation')
    $stageResults.Add((Invoke-ColdStage `
        '02_commit_r33_cesium_world_terrain_reference_exactly_once' $r33Library `
        'CommitR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap' `
        'OutReport' `
        'ISTANA_EXPLORE_V5D_R33_CESIUM_WORLD_TERRAIN_REFERENCE_COMMIT_PASS' @{
            ExpectedPredecessorBytes=[int64] $ExpectedMapBytes
            ExpectedPredecessorSha256=$ExpectedMapSha256.ToUpperInvariant()
            VerifiedExternalBackupFilename=$backupMap
        }))
    $stageResults.Add((Invoke-ColdStage `
        '03_cold_validate_r33_cesium_world_terrain_reference_successor' `
        $r33Library `
        'ValidateR33CesiumWorldTerrainReferenceInLoadedV5DHybridMap' `
        'OutReport' `
        'ISTANA_EXPLORE_V5D_R33_CESIUM_WORLD_TERRAIN_REFERENCE_MAP_VALID'))

    $successQuiescence=Wait-NativeMutationQuiescence `
        'before R33 commit receipt' 60
    Assert-ProtectedUnchanged $script:protectedBefore
    foreach ($pin in @($sourcePins)+@($sourceAssetPins)+@($retainedR32Pins)) {
        Assert-PinAtRoot $pin $nativeProjectRoot
    }
    [void] (Assert-TreeIdentity `
        $nativePluginSourceRoot $expectedPostTree `
        'postflight exact R33 native plugin source')
    [void] (Assert-TreeIdentity `
        $treeRealismContentRoot $treeMaterialResponseV3Before `
        'postflight preserved TreeRealism v3 content')
    foreach ($receiptAdmission in @(
        $admission.R30Transaction,$admission.R30Capture,
        $admission.R31Transaction,$admission.R31Capture,
        $admission.R32Transaction,$admission.R32Capture)) {
        [void] (Assert-State `
            $receiptAdmission.State $receiptAdmission.Path `
            'immutable predecessor receipt postflight')
    }
    [void] (Assert-State $expectedMap $backupMap 'preserved external R32 map backup')
    $successorMap=Get-FileState $mapFile
    if (-not $successorMap.Present -or $successorMap.Bytes -le 0 -or
        ($successorMap.Bytes -eq $expectedMap.Bytes -and
         $successorMap.Sha256 -ceq $expectedMap.Sha256)) {
        throw 'R33 successor map did not change from the exact R32 predecessor.'
    }

    $commit=[pscustomobject] [ordered] @{
        Schema=$schema; Status='COMMITTED'; RunToken=$RunToken
        NativeOrder='R30_COMMIT_HUMAN_CAPTURE_THEN_R31_COMMIT_HUMAN_CAPTURE_THEN_R32_COMMIT_HUMAN_CAPTURE_THEN_R33'
        PrewriteAdmission=$prewriteAdmission
        Build=$buildReceipt; NativeMutationQuiescence=$successQuiescence
        PredecessorMap=$expectedMap; SuccessorMap=$successorMap
        ExternalBackup=Get-FileState $backupMap
        RuntimeDllBefore=$expectedRuntime; RuntimeDllAfter=$runtimeAfter
        EditorDllBefore=$expectedEditor; EditorDllAfter=$editorAfter
        GroundHeaderBefore=$admission.GroundHeader
        GroundHeaderAfter=Get-FileState (Join-Path $nativeProjectRoot $sourcePins[2].RelativePath)
        GroundSourceBefore=$admission.GroundSource
        GroundSourceAfter=Get-FileState (Join-Path $nativeProjectRoot $sourcePins[3].RelativePath)
        NativePluginSourceTreeBefore=@($preTree)
        NativePluginSourceTreeBeforeSha256=$preTreeSha
        NativePluginSourceTreeAfter=@($expectedPostTree)
        NativePluginSourceTreeAfterSha256=Get-TreeIdentitySha256 $expectedPostTree
        StageResults=@($stageResults)
        R30TransactionAdmission=[pscustomobject] [ordered] @{ Path=$admission.R30Transaction.Path; File=$admission.R30Transaction.State }
        R30CaptureAdmission=[pscustomobject] [ordered] @{ Path=$admission.R30Capture.Path; File=$admission.R30Capture.State; ExplicitHumanReviewAcceptance=$true }
        R31TransactionAdmission=[pscustomobject] [ordered] @{ Path=$admission.R31Transaction.Path; File=$admission.R31Transaction.State }
        R31CaptureAdmission=[pscustomobject] [ordered] @{ Path=$admission.R31Capture.Path; File=$admission.R31Capture.State; ExplicitHumanReviewAcceptance=$true }
        R32TransactionAdmission=[pscustomobject] [ordered] @{ Path=$admission.R32Transaction.Path; File=$admission.R32Transaction.State }
        R32CaptureAdmission=[pscustomobject] [ordered] @{ Path=$admission.R32Capture.Path; File=$admission.R32Capture.State; ExplicitHumanReviewAcceptance=$true }
        PromotedCodeFileCount=9; PromotedContractFileCount=1
        PromotedContentPackageCount=0
        PromotedFiles=@(@($sourcePins)+@($sourceAssetPins) | ForEach-Object {
            [pscustomobject] [ordered] @{
                RepositoryRelativePath=$_.RelativePath
                NativeRelativePath=$_.RelativePath
                State=Get-FileState (Join-Path $nativeProjectRoot $_.RelativePath)
            }
        })
        ContextPolicyR33AwareReplacementCount=2
        GroundVegetationR33AwareReplacementCount=2
        R33RuntimeSourceCount=2; R33EditorSourceCount=2
        CommitR33EndpointInvocationCount=1
        StandaloneApplyR33EndpointInvocationCount=0
        GooglePhotorealisticIonAssetId=2275207L
        CesiumWorldTerrainIonAssetId=1L
        CesiumPluginDescriptor=$cesiumPlugin
        ExactCesiumTilesetCount=2; ExactCesiumGeoreferenceCount=1
        SameGeoreferenceAndIonServerRequired=$true
        NonNullOpaqueIonServerRequired=$true
        IonTokenValueOrFingerprintInspectedByTriad=$false
        NonNullServerProvesTokenOrEntitlement=$false
        PresentationStates=@('GooglePrimary','CwtWarming','CwtPresented','SafeLocal')
        SourceTerrainModified=$false; CollisionModified=$false
        TreeMaterialResponseV3=$admission.TreeMaterialResponseV3
        TreeMaterialResponseV3PromotedAtR30=$true
        TreeMaterialResponseV3Preserved=$true
        TreeResponseMaterialPackageCount=13
        TreeDerivativeMeshPackageCount=5
        TreeRuntimeResponseMidCount=26
        TreePlacementGeometryOpacityWindAuthorityModified=$false
        NavigationModified=$false; LineOfSightAuthorityModified=$false
        SensorPlacementModified=$false; DetectionSimulationModified=$false
        RfTruthModified=$false
        SimulationCollisionNavigationSensorRfAuthority=$false
        VisualReferenceOnly=$true; GeospatialAuthority=$false
        AccurateRealWorldTerrainClaimed=$false
        SurveyAccuracyClaimed=$false; VerticalDatumResolved=$false
        HeightCheckpointValidationComplete=$false
        ProviderTermsAndEntitlementVerifiedByThisTransaction=$false
        ProviderCreditsMustRemainVisible=$true
        VisualCaptureAccepted=$false; CaptureRevalidationRequired=$true
    }
    $commitPath=Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 20 |
        Set-Content -LiteralPath $commitPath -Encoding utf8NoBOM
    $committed=$true
    $commit | ConvertTo-Json -Depth 20
}
catch { $failure=$_.Exception }
finally {
    if (-not $committed -and $transactionStarted) {
        $rollbackErrors=[Collections.Generic.List[string]]::new()
        $rollbackMayMutate=$false
        foreach ($owned in @($script:ownedProcesses)) {
            try {
                if (-not $owned.Handle.HasExited) {
                    Close-OwnedProcess $owned -AllowContainment
                }
            }
            catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        try {
            [void] (Wait-NativeMutationQuiescence `
                'before R33 filesystem rollback restoration' 60)
            $rollbackMayMutate=$true
        }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackMayMutate) {
            try { Restore-FileJournal $mapJournal }
            catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-TreeJournal $buildJournal }
            catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-FileJournal $sourceJournal }
            catch { $rollbackErrors.Add($_.Exception.Message) }
            try { Restore-DirectoryPresenceJournal $sourceDirectoryJournal }
            catch { $rollbackErrors.Add($_.Exception.Message) }
            if ($null -ne $admission) {
                try { [void] (Assert-State $admission.Map $mapFile 'rollback R32 map') }
                catch { $rollbackErrors.Add($_.Exception.Message) }
                try { [void] (Assert-State $admission.RuntimeDll $runtimeDll 'rollback R32 runtime DLL') }
                catch { $rollbackErrors.Add($_.Exception.Message) }
                try { [void] (Assert-State $admission.EditorDll $editorDll 'rollback R32 editor DLL') }
                catch { $rollbackErrors.Add($_.Exception.Message) }
                try { [void] (Assert-TreeIdentity $nativePluginSourceRoot $admission.NativePluginSourceTree 'rollback accepted R32 plugin source tree') }
                catch { $rollbackErrors.Add($_.Exception.Message) }
            }
        }
        else {
            $rollbackErrors.Add('Filesystem rollback was not attempted while an owned native process could remain active; exact journals were retained.')
        }
        try { Assert-ProtectedUnchanged $script:protectedBefore }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        $rollback=[pscustomobject] [ordered] @{
            Schema=$schema
            Status=if ($rollbackErrors.Count -eq 0) { 'ROLLED_BACK' } else { 'ROLLBACK_INCOMPLETE' }
            Failure=if ($null -eq $failure) { 'unknown' } else { $failure.Message }
            RollbackErrors=@($rollbackErrors)
        }
        $rollback | ConvertTo-Json -Depth 12 |
            Set-Content -LiteralPath (Join-Path $transactionRoot 'rollback.json') -Encoding utf8NoBOM
        if ($rollbackErrors.Count -ne 0) {
            throw "R33 transaction failed and rollback was incomplete: failure={$($rollback.Failure)} rollback={$([string]::Join(' | ',@($rollbackErrors)))}"
        }
    }
}
if ($null -ne $failure) { throw $failure }
