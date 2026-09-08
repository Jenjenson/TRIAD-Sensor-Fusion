#requires -Version 7.0

<#
.SYNOPSIS
Runs the strict additive V5D Landmark Vegetation R26 native transaction.

.DESCRIPTION
The default invocation is a read-only preflight. -Execute is the only native
write authority. The live path promotes exactly six byte/hash-pinned sources,
forces UHT while building the two TRIAD modules, validates the 14 reusable
assets, adds one deterministic render-only actor to the exact committed R25
map, cold-validates the successor, and proves a byte-stable idempotent apply.

Before the first native write it records and backs up the exact map, six source
destinations, immutable R25/V2 content pins, and every file under the bounded
TRIAD plugin Binaries/Intermediate build roots plus the known project-global
UBT sidecars. Failure restores map, build surface, immutable content, and
source destinations without deleting a directory. A pre-existing exact UE5.4
CAPSTONE process set is treated as immutable and is never stopped.

.EXAMPLE
.\Invoke-IstanaExploreV5DLandmarkVegetationR26NativeTransactionV1.ps1 -RunToken reviewed_r26

.EXAMPLE
.\Invoke-IstanaExploreV5DLandmarkVegetationR26NativeTransactionV1.ps1 -RunToken reviewed_r26 -Execute
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,

    [switch] $StaticSelfCheck,

    [ValidateRange(60, 1800)]
    [int] $EditorTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema =
    'triad.istana_explore_v5d.landmark_vegetation_r26.native_transaction.v1'
$repositoryUnrealRoot = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo\unreal')
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath(
    'D:\triad\TRIAD\TRIAD.uproject')
$engineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot `
    'Engine\Build\BatchFiles\Build.bat'))
$editor = [IO.Path]::GetFullPath((Join-Path $engineRoot `
    'Engine\Binaries\Win64\UnrealEditor.exe'))
$protectedEditor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedProject = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DLandmarkVegetationR26V1'))
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase $RunToken))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Intermediate'))
$ddcRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\DerivedDataCache'))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$assetLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary'
$hybridLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'

$expectedProjectPin = [pscustomobject] [ordered] @{
    Path = $nativeProjectFile
    Present = $true
    Bytes = 1298L
    Sha256 =
        '42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3'
}
$expectedMapPin = [pscustomobject] [ordered] @{
    Path = $mapFile
    Present = $true
    Bytes = 34993427L
    Sha256 =
        '38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9'
}
$expectedRuntimeDllPin = [pscustomobject] [ordered] @{
    Path = $runtimeDll
    Present = $true
    Bytes = 4585984L
    Sha256 =
        '31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4'
}
$expectedEditorDllPin = [pscustomobject] [ordered] @{
    Path = $editorDll
    Present = $true
    Bytes = 7671296L
    Sha256 =
        '471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34'
}

# Four new files are absent from the committed R25 native source tree. The two
# hybrid files must match their exact successful-R25 promoted state.
$sourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DLandmarkVegetationActor.h'
        Bytes = 10946L
        Sha256 = '1CBB2B107D00949BE0EB33028E2BE80C3F4A889796D0BE682412374FACC764F6'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DLandmarkVegetationActor.cpp'
        Bytes = 50398L
        Sha256 = 'FE7910E7C1B4A60BB8FE70A088A0F7C002925477842398500471883F56BC735E'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h'
        Bytes = 940L
        Sha256 = '026A7F9BA45C53A5AC7608D7B7BE117D4E5B847C57518F768339AA8CFABD6844'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp'
        Bytes = 6167L
        Sha256 = '0110187660AE630858D774F3747923653DF4D9F40760DCFD6D7C86102CDCCEBD'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h'
        Bytes = 10522L
        Sha256 = 'E0EA591E419A1D023F8BD047429A3EF64D194FBAAEB73F1B989BADB7FA18C994'
        NativeBeforePresent = $true
        NativeBeforeBytes = 9553L
        NativeBeforeSha256 = '762F0EB8436844912D203D6E14E962AB3BA7331F300DE19EA323096D45C11AEF'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
        Bytes = 432233L
        Sha256 = '1AB587FC5C2DC07827351E56537057BDD54C89341B44A872BFEA3D60C2A95C0C'
        NativeBeforePresent = $true
        NativeBeforeBytes = 403489L
        NativeBeforeSha256 = 'C28D6D9408C98469CF21C423A0D568ACBC2F00BDA61BF1AAB60F476285311E26'
    }
)

$immutableContentPins = @(
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_ContextMassing_Master.uasset'; Bytes = 29065L; Sha256 = '47B0F9F804858280B53D4D03CEEA8E078F01740920AF589EFEA676EBC4D057B4' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRender.uasset'; Bytes = 15159L; Sha256 = '113CE217898FD5A65603493FFF82B4B0FC16C6AD3C0339097F61E78B911E326F' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRoof.uasset'; Bytes = 14843L; Sha256 = '4534461C849FD80A1D1BA34FE68239414B25F1FF31F4790BD857C9F828A8C943' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRender.uasset'; Bytes = 15170L; Sha256 = '227415242B7F0D0E61F3474C57D605848D24EF7574CD67C2242DED75B22F8079' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRoof.uasset'; Bytes = 14693L; Sha256 = 'BF11B2C73A066E03923B83AADD9C1222CD80377B44A90EBFAE868454C05898EB' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset'; Bytes = 761010L; Sha256 = 'A2062A8A90CB5FFA4D0B6B1117E775230B39AF58CB57F3C26C792DD4861429EE' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\M_IPV5D_ContextFacadeR25_Master.uasset'; Bytes = 29079L; Sha256 = '37C7305FC07DD076902A243410F7CB094356F8CBD333461E29EF3AC47694AC03' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_OfficialWall.uasset'; Bytes = 14637L; Sha256 = '2C783BA3D045FBC37432D117C372C1B6654B94ECDC3E9C8C051263E2492036DA' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_OfficialRoof.uasset'; Bytes = 14709L; Sha256 = '98DE68DFB79083DF8D690C0222BAED926F414302E840A8C88950FD73DCE729D8' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_FallbackWall.uasset'; Bytes = 14599L; Sha256 = 'CD27BC83E1876E53B485F055D5D1F2E48999EE8CEAC733D7AB55F06CC6E81C9B' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_FallbackRoof.uasset'; Bytes = 14792L; Sha256 = 'E8F78D776EB235F1F25304678CCAD7F64DA125376BA04126DE2CE3BABCA64A18' }
)

$globalBuildRelativePaths = @(
    'Intermediate\Build\SourceFileCache.bin'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.deps'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtpath'
    'Intermediate\Build\Win64\x64\UnrealEditor\ActionHistory.bin'
    'Intermediate\Build\Win64\x64\UnrealEditor\Development\DependencyCache.bin'
    'Plugins\AirSimTriadRuntime\Intermediate\Build\Win64\UnrealEditor\Inc\AirSimTriadRuntime\UHT\Timestamp'
)

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
        if (($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse points are forbidden in transaction paths: $($cursor.FullName)"
        }
        if ($cursor.FullName.TrimEnd('\').Equals(
                $stop, [StringComparison]::OrdinalIgnoreCase)) {
            return
        }
        $cursor = $cursor.Parent
    }
    throw "Path did not terminate at the expected root: $Path root=$StopRoot"
}

function Get-FileState {
    param([string] $Path)
    $full = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($full)) {
        return [pscustomobject] [ordered] @{
            Path = $full; Present = $false; Bytes = 0L; Sha256 = 'ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $full
    [pscustomobject] [ordered] @{
        Path = $full
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    }
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual = Get-FileState -Path $Path
    if ([bool] $actual.Present -ne [bool] $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label pin mismatch: expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Assert-SourcePins {
    param([switch] $NativeBefore, [switch] $NativeAfter)
    foreach ($pin in $sourcePins) {
        $repo = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot `
            $pin.RelativePath))
        $expectedRepo = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expectedRepo $repo 'repository source')
        if ($NativeBefore) {
            $expectedNative = [pscustomobject] @{
                Present = $pin.NativeBeforePresent
                Bytes = $pin.NativeBeforeBytes
                Sha256 = $pin.NativeBeforeSha256
            }
            [void] (Assert-State $expectedNative `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'native R25 source predecessor')
        }
        if ($NativeAfter) {
            [void] (Assert-State $expectedRepo `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'promoted native R26 source')
        }
    }
}

function Assert-ImmutableContent {
    foreach ($pin in $immutableContentPins) {
        $expected = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expected `
            (Join-Path $nativeProjectRoot $pin.RelativePath) `
            'immutable R25/V2 content')
    }
}

function Get-ProcessRecord {
    param([uint32] $ProcessId)
    $cim = Get-CimInstance Win32_Process -Filter "ProcessId=$ProcessId" `
        -ErrorAction SilentlyContinue
    if ($null -eq $cim) { return $null }
    $process = Get-Process -Id ([int] $ProcessId) -ErrorAction SilentlyContinue
    if ($null -eq $process) { return $null }
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $ProcessId
        ExecutablePath = [IO.Path]::GetFullPath([string] $cim.ExecutablePath)
        CommandLine = [string] $cim.CommandLine
        StartTimeUtcTicks = $process.StartTime.ToUniversalTime().Ticks
    }
}

function Get-ProtectedSnapshot {
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($cim in @(Get-CimInstance Win32_Process -Filter `
            "Name='UnrealEditor.exe'" -ErrorAction SilentlyContinue)) {
        if ([string]::IsNullOrWhiteSpace([string] $cim.ExecutablePath) -or
            -not ([IO.Path]::GetFullPath([string] $cim.ExecutablePath).Equals(
                $protectedEditor, [StringComparison]::OrdinalIgnoreCase)) -or
            -not ([string] $cim.CommandLine).Contains(
                $protectedProject, [StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        $record = Get-ProcessRecord -ProcessId ([uint32] $cim.ProcessId)
        if ($null -ne $record) { $rows.Add($record) }
    }
    @($rows | Sort-Object ProcessId)
}

function Assert-ProtectedUnchanged {
    param([object[]] $Expected)
    $actual = @(Get-ProtectedSnapshot)
    if (($Expected | ConvertTo-Json -Depth 6 -Compress) -cne
        ($actual | ConvertTo-Json -Depth 6 -Compress)) {
        throw 'The protected UE5.4 CAPSTONE process identity set changed.'
    }
}

function Get-RcOwners {
    @((Get-NetTCPConnection -State Listen -LocalPort 30010 `
        -ErrorAction SilentlyContinue | Select-Object -ExpandProperty OwningProcess -Unique))
}

function Assert-NativeIdle {
    param([string] $Checkpoint)
    $offenders = [Collections.Generic.List[string]]::new()
    foreach ($cim in @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') })) {
        $exe = [string] $cim.ExecutablePath
        $cmd = [string] $cim.CommandLine
        if ((!([string]::IsNullOrWhiteSpace($exe)) -and
             [IO.Path]::GetFullPath($exe).StartsWith(
                $engineRoot, [StringComparison]::OrdinalIgnoreCase)) -or
            $cmd.Contains($nativeProjectFile, [StringComparison]::OrdinalIgnoreCase)) {
            $offenders.Add("pid=$($cim.ProcessId) exe=$exe cmd=$cmd")
        }
    }
    if ($offenders.Count -ne 0 -or @(Get-RcOwners).Count -ne 0) {
        throw "Native Unreal/RC boundary is not idle at ${Checkpoint}: $([string]::Join(' | ', @($offenders))) rc=$([string]::Join(',', @(Get-RcOwners)))"
    }
}

function Invoke-RcCall {
    param([string] $ObjectPath, [string] $FunctionName,
        [hashtable] $Parameters = @{}, [int] $TimeoutSec = 60)
    $payload = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 12 -Compress
    Invoke-RestMethod -Method Put -Uri $rcUri -ContentType 'application/json' `
        -Body $payload -TimeoutSec $TimeoutSec
}

function Assert-HelperIdentity {
    param($Expected, [Diagnostics.Process] $Handle)
    $Handle.Refresh()
    if ($Handle.HasExited) { throw 'Owned Unreal helper exited unexpectedly.' }
    $actual = Get-ProcessRecord -ProcessId ([uint32] $Handle.Id)
    if ($null -eq $actual -or
        [uint32] $actual.ProcessId -ne [uint32] $Expected.ProcessId -or
        [int64] $actual.StartTimeUtcTicks -ne [int64] $Expected.StartTimeUtcTicks -or
        -not $actual.ExecutablePath.Equals(
            $editor, [StringComparison]::OrdinalIgnoreCase) -or
        -not $actual.CommandLine.Contains(
            $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Owned Unreal helper identity changed.'
    }
    $actual
}

function Stop-OwnedHelper {
    param($Identity, [Diagnostics.Process] $Handle)
    $Handle.Refresh()
    if ($Handle.HasExited) { return }
    [void] (Assert-HelperIdentity $Identity $Handle)
    try {
        [void] (Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30)
    }
    catch {
        # Connection shutdown after QuitEditor is an accepted transport result.
    }
    if (-not $Handle.WaitForExit($ShutdownTimeoutSeconds * 1000)) {
        [void] (Assert-HelperIdentity $Identity $Handle)
        Stop-Process -Id ([int] $Identity.ProcessId) -Force
        if (-not $Handle.WaitForExit(30000)) {
            throw 'Exact owned Unreal helper did not exit after containment.'
        }
    }
}

function Invoke-ColdStage {
    param(
        [string] $Stage,
        [string] $ObjectPath,
        [string] $FunctionName,
        [string] $TextProperty,
        [string] $ExpectedPrefix
    )
    Assert-NativeIdle "before $Stage"
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-SourcePins -NativeAfter
    Assert-ImmutableContent
    $logDir = Join-Path $transactionRoot 'logs'
    [void] [IO.Directory]::CreateDirectory($logDir)
    $log = [IO.Path]::GetFullPath((Join-Path $logDir "$Stage.log"))
    if ([IO.File]::Exists($log)) { throw "Stage log already exists: $log" }
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
        $identityDeadline = [DateTime]::UtcNow.AddSeconds(20)
        do {
            $candidate = Get-ProcessRecord -ProcessId ([uint32] $handle.Id)
            if ($null -ne $candidate -and
                $candidate.ExecutablePath.Equals(
                    $editor, [StringComparison]::OrdinalIgnoreCase) -and
                $candidate.CommandLine.Contains(
                    $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase) -and
                $candidate.CommandLine.Contains(
                    $mapPackage, [StringComparison]::Ordinal)) {
                $identity = $candidate
                break
            }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $identityDeadline)
        if ($null -eq $identity) { throw "Could not prove helper identity for $Stage" }

        $readyDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
        $identityResponse = $null
        do {
            Assert-ProtectedUnchanged $script:protectedBefore
            [void] (Assert-HelperIdentity $identity $handle)
            $owners = @(Get-RcOwners)
            if ($owners.Count -eq 1 -and
                [uint32] $owners[0] -eq [uint32] $identity.ProcessId) {
                try {
                    $identityResponse = Invoke-RcCall $identityLibrary `
                        'ValidateIstanaExploreRemoteControlProject' `
                        @{ ExpectedProjectPath = $nativeProjectRoot } 30
                }
                catch { $identityResponse = $null }
                if ($null -ne $identityResponse -and
                    $identityResponse.ReturnValue -eq $true) { break }
            }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        if ($null -eq $identityResponse -or
            $identityResponse.ReturnValue -ne $true) {
            throw "Could not prove exact project/RC ownership for $Stage"
        }
        [void] (Assert-HelperIdentity $identity $handle)
        $response = Invoke-RcCall $ObjectPath $FunctionName @{} `
            $EditorTimeoutSeconds
        [void] (Assert-HelperIdentity $identity $handle)
        $text = [string] $response.$TextProperty
        if ($response.ReturnValue -ne $true -or
            -not $text.StartsWith($ExpectedPrefix, [StringComparison]::Ordinal)) {
            throw "Stage $Stage failed exact response gate: $FunctionName response=$text"
        }
    }
    catch { $stageError = $_.Exception }
    finally {
        if ($null -ne $identity) {
            try { Stop-OwnedHelper $identity $handle }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
            }
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle "after $Stage"
    if ($null -ne $stageError) { throw $stageError }
    if (-not [IO.File]::Exists($log) -or (Get-Item $log).Length -le 0) {
        throw "Stage $Stage did not persist a non-empty log."
    }
    [pscustomobject] [ordered] @{
        Stage = $Stage
        Function = $FunctionName
        ExpectedPrefix = $ExpectedPrefix
        Message = [string] $response.$TextProperty
        ProcessIdentity = $identity
        Log = Get-FileState $log
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
    foreach ($relative in $globalBuildRelativePaths) {
        $paths.Add([IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative)))
    }
    @($paths | Sort-Object -Unique)
}

function New-FileJournal {
    param([string[]] $Paths, [string] $BackupRoot)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "Journal target escaped native project: $path"
        }
        $state = Get-FileState $path
        $relative = [IO.Path]::GetRelativePath($nativeProjectRoot, $path)
        $backup = [IO.Path]::GetFullPath((Join-Path $BackupRoot $relative))
        if ($state.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup))
            Copy-Item -LiteralPath $path -Destination $backup
            [void] (Assert-State $state $backup 'journal backup')
        }
        $rows.Add([pscustomobject] [ordered] @{
            Path = $path
            RelativePath = $relative
            Before = $state
            Backup = if ($state.Present) { $backup } else { $null }
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
        }
        elseif ([IO.File]::Exists($row.Path)) {
            Remove-Item -LiteralPath $row.Path -Force
        }
        [void] (Assert-State $row.Before $row.Path 'rollback state')
    }
}

function Restore-BuildSurface {
    param([object[]] $Rows)
    $before = @{}
    foreach ($row in $Rows) { $before[$row.Path.ToLowerInvariant()] = $true }
    foreach ($path in @(Get-BuildSurfacePaths)) {
        if (-not $before.ContainsKey($path.ToLowerInvariant()) -and
            [IO.File]::Exists($path)) {
            if (-not (Test-ContainedPath $path $pluginBinaryRoot) -and
                -not (Test-ContainedPath $path $pluginIntermediateRoot) -and
                -not ($globalBuildRelativePaths -contains
                    [IO.Path]::GetRelativePath($nativeProjectRoot, $path))) {
                throw "Rollback refused non-allowlisted new build file: $path"
            }
            Remove-Item -LiteralPath $path -Force
        }
    }
    Restore-FileJournal $Rows
}

function Restore-MapFromJournal {
    param($MapRow)
    $temporary = [IO.Path]::GetFullPath((Join-Path `
        ([IO.Path]::GetDirectoryName($mapFile)) `
        ("TRIAD_R26_Wrapper_Restore_{0}.tmp" -f [Guid]::NewGuid().ToString('N'))))
    if (-not (Test-ContainedPath $temporary `
            ([IO.Path]::GetDirectoryName($mapFile)))) {
        throw 'Map rollback temporary escaped the map directory.'
    }
    Copy-Item -LiteralPath $MapRow.Backup -Destination $temporary
    [void] (Assert-State $MapRow.Before $temporary 'map rollback temporary')
    Move-Item -LiteralPath $temporary -Destination $mapFile -Force
    [void] (Assert-State $MapRow.Before $mapFile 'restored R25 map')
}

function Assert-StaticContract {
    if ($sourcePins.Count -ne 6) { throw 'Source roster must contain six files.' }
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in $sourcePins) {
        if (-not $seen.Add($pin.RelativePath)) {
            throw "Duplicate source path: $($pin.RelativePath)"
        }
        if ([IO.Path]::IsPathRooted($pin.RelativePath) -or
            $pin.RelativePath.Contains('..')) {
            throw "Non-canonical source path: $($pin.RelativePath)"
        }
    }
    if (($sourcePins | Where-Object NativeBeforePresent).Count -ne 2 -or
        ($sourcePins | Where-Object { -not $_.NativeBeforePresent }).Count -ne 4) {
        throw 'R26 source predecessor roster must be exactly two present/four absent.'
    }
    Assert-SourcePins
    $header = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h') -Raw
    $source = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp') -Raw
    foreach ($endpoint in @(
        'ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap',
        'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap')) {
        if (-not $header.Contains($endpoint, [StringComparison]::Ordinal) -or
            -not $source.Contains($endpoint, [StringComparison]::Ordinal)) {
            throw "Missing R26 endpoint in pinned repo source: $endpoint"
        }
    }
    [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'STATIC_SELF_CHECK_PASS'
        SourceCount = $sourcePins.Count
        NativeTreeReadOrWritten = $false
        UnrealBuildOrEditorLaunched = $false
        MapPin = $expectedMapPin
        GrassInstances = 3072
        GrassCullCm = @(6500, 9000)
        WpoDisableCm = 2400
    }
}

$static = Assert-StaticContract
if ($StaticSelfCheck) {
    $static | ConvertTo-Json -Depth 12
    return
}

foreach ($directory in @(
        $repositoryUnrealRoot,
        $nativeProjectRoot,
        $engineRoot,
        [IO.Path]::GetDirectoryName($transactionBase))) {
    if (-not [IO.Directory]::Exists($directory)) {
        throw "Required directory is absent: $directory"
    }
}
Assert-NoReparseAncestor $nativeProjectRoot ([IO.Path]::GetPathRoot($nativeProjectRoot))
$script:protectedBefore = @(Get-ProtectedSnapshot)
Assert-ProtectedUnchanged $script:protectedBefore
Assert-NativeIdle 'preflight'
[void] (Assert-State $expectedProjectPin $nativeProjectFile 'project')
[void] (Assert-State $expectedMapPin $mapFile 'R25 predecessor map')
[void] (Assert-State $expectedRuntimeDllPin $runtimeDll 'R25 runtime DLL')
[void] (Assert-State $expectedEditorDllPin $editorDll 'R25 editor DLL')
Assert-SourcePins -NativeBefore
Assert-ImmutableContent
if ([IO.Directory]::Exists($transactionRoot) -or
    [IO.File]::Exists($transactionRoot)) {
    throw "Transaction path already exists and will not be reused: $transactionRoot"
}

$preflight = [pscustomobject] [ordered] @{
    Schema = $schema
    Status = if ($Execute) { 'EXECUTION_PREFLIGHT_PASS' } else { 'READ_ONLY_PREFLIGHT_PASS' }
    ExecuteRequested = [bool] $Execute
    SourceCount = $sourcePins.Count
    Map = Get-FileState $mapFile
    RuntimeDll = Get-FileState $runtimeDll
    EditorDll = Get-FileState $editorDll
    ImmutableContentCount = $immutableContentPins.Count
    ProtectedUE54Sessions = @($script:protectedBefore)
    NativeTreeWritten = $false
    UnrealBuildOrEditorLaunched = $false
}
if (-not $Execute) {
    $preflight | ConvertTo-Json -Depth 12
    return
}

[void] [IO.Directory]::CreateDirectory($transactionRoot)
$journalRoot = Join-Path $transactionRoot 'journal'
[void] [IO.Directory]::CreateDirectory($journalRoot)
$sourcePaths = @($sourcePins | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath))
})
$contentPaths = @($immutableContentPins | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath))
})
$sourceJournal = @(New-FileJournal $sourcePaths `
    (Join-Path $journalRoot 'source'))
$mapJournal = @(New-FileJournal @($mapFile) `
    (Join-Path $journalRoot 'map'))
$contentJournal = @(New-FileJournal $contentPaths `
    (Join-Path $journalRoot 'content'))
$buildPathsBefore = @(Get-BuildSurfacePaths)
$buildJournal = @(New-FileJournal $buildPathsBefore `
    (Join-Path $journalRoot 'build'))
$stageResults = [Collections.Generic.List[object]]::new()
$committed = $false
$transactionError = $null
$rollbackReport = $null

try {
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'final prewrite gate'
    [void] (Assert-State $expectedProjectPin $nativeProjectFile 'final project')
    [void] (Assert-State $expectedMapPin $mapFile 'final R25 map')
    [void] (Assert-State $expectedRuntimeDllPin $runtimeDll 'final runtime DLL')
    [void] (Assert-State $expectedEditorDllPin $editorDll 'final editor DLL')
    Assert-SourcePins -NativeBefore
    Assert-ImmutableContent

    foreach ($pin in $sourcePins) {
        $source = Join-Path $repositoryUnrealRoot $pin.RelativePath
        $target = Join-Path $nativeProjectRoot $pin.RelativePath
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
        Copy-Item -LiteralPath $source -Destination $target -Force
    }
    Assert-SourcePins -NativeAfter
    [void] (Assert-State $expectedMapPin $mapFile 'post-promotion map')
    Assert-ImmutableContent

    $buildLog = Join-Path $transactionRoot 'build.log'
    $buildArguments = @(
        'UnrealEditor', 'Win64', 'Development', $nativeProjectFile,
        '-DisablePlugin=AirSim',
        '-Module=TRIADSensorFusion',
        '-Module=TRIADSensorFusionEditor',
        '-WaitMutex',
        '-NoHotReloadFromIDE',
        '-NoUBTMakefiles',
        '-ForceHeaderGeneration',
        '-MaxParallelActions=1',
        '-NoUBA',
        '-NoUBALocal'
    )
    & $buildTool @buildArguments *> $buildLog
    if ($LASTEXITCODE -ne 0) {
        throw "R26 two-module build failed with exit code $LASTEXITCODE. Log=$buildLog"
    }
    Assert-NativeIdle 'after R26 build'
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-SourcePins -NativeAfter
    [void] (Assert-State $expectedMapPin $mapFile 'post-build map')
    Assert-ImmutableContent
    $runtimeAfter = Get-FileState $runtimeDll
    $editorAfter = Get-FileState $editorDll
    if (-not $runtimeAfter.Present -or -not $editorAfter.Present -or
        $runtimeAfter.Bytes -le 0 -or $editorAfter.Bytes -le 0 -or
        $runtimeAfter.Sha256 -ceq $expectedRuntimeDllPin.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedEditorDllPin.Sha256) {
        throw 'R26 build did not produce two distinct non-empty DLL receipts.'
    }
    $actorGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp'
    $editorGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp'
    $hybridGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp'
    foreach ($generated in @($actorGenerated, $editorGenerated, $hybridGenerated)) {
        if (-not [IO.File]::Exists($generated) -or
            (Get-Item $generated).LastWriteTimeUtc -lt
                (Get-Item $buildLog).CreationTimeUtc) {
            throw "Forced-UHT output is absent or stale: $generated"
        }
    }
    $hybridGeneratedText = Get-Content -LiteralPath $hybridGenerated -Raw
    $editorGeneratedText = Get-Content -LiteralPath $editorGenerated -Raw
    $editorDllText = [Text.Encoding]::ASCII.GetString(
        [IO.File]::ReadAllBytes($editorDll))
    foreach ($endpoint in @(
        'ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap',
        'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap')) {
        if (-not $hybridGeneratedText.Contains($endpoint) -or
            -not $editorDllText.Contains($endpoint)) {
            throw "Fresh reflection/DLL endpoint gate failed: $endpoint"
        }
    }
    foreach ($endpoint in @(
        'ValidateReusableLandmarkVegetationAssets',
        'ConfigureLandmarkVegetationActor')) {
        if (-not $editorGeneratedText.Contains($endpoint) -or
            -not $editorDllText.Contains($endpoint)) {
            throw "Fresh landmark editor reflection/DLL gate failed: $endpoint"
        }
    }

    $mapBeforeAssetValidation = Get-FileState $mapFile
    $stageResults.Add((Invoke-ColdStage `
        '01_validate_reusable_assets' $assetLibrary `
        'ValidateReusableLandmarkVegetationAssets' 'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_ASSETS_VALID'))
    [void] (Assert-State $mapBeforeAssetValidation $mapFile `
        'asset-validation map stability')

    $stageResults.Add((Invoke-ColdStage `
        '02_apply_map' $hybridLibrary `
        'ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap' `
        'OutMessage' `
        'EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_PASS:'))
    $successorPin = Get-FileState $mapFile
    if (-not $successorPin.Present -or $successorPin.Bytes -le 0 -or
        $successorPin.Sha256 -ceq $expectedMapPin.Sha256) {
        throw 'R26 apply did not produce a distinct positive successor map receipt.'
    }
    foreach ($marker in @(
        'oneSave=true', 'landmarkVegetationR26=true',
        'contextFacadeR25=true', 'exactReusableAssets=14',
        'macDonaldGrass=1536', 'temasekGrass=1536',
        'collisionNavigationSensorRfAuthority=false')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R26 apply response lacks required marker: $marker"
        }
    }

    $stageResults.Add((Invoke-ColdStage `
        '03_cold_validate_map' $hybridLibrary `
        'ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap' `
        'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_VALID:'))
    [void] (Assert-State $successorPin $mapFile 'cold-validation map stability')
    foreach ($marker in @(
        'mapIntegrated=true', 'exactlyOneActor=true',
        'contextFacadeR25=true', 'distanceReadableGrass=true',
        'grassCullCm=6500,9000', 'evidenceRangeMeters=72.8',
        'wpoDisableCm=2400', 'grassInstances=3072',
        'renderOnly=true', 'sensorAuthority=false', 'rfAuthority=false')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R26 validation response lacks required marker: $marker"
        }
    }

    $stageResults.Add((Invoke-ColdStage `
        '04_idempotent_apply' $hybridLibrary `
        'ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap' `
        'OutMessage' `
        'IDEMPOTENT_EXPLORE_V5D_LANDMARK_VEGETATION_R26_ALREADY_VALID:'))
    [void] (Assert-State $successorPin $mapFile 'idempotent map stability')
    Assert-ImmutableContent
    Assert-SourcePins -NativeAfter
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'commit gate'

    $commit = [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'PASS'
        RunToken = $RunToken
        PredecessorMap = $expectedMapPin
        SuccessorMap = $successorPin
        RuntimeDll = $runtimeAfter
        EditorDll = $editorAfter
        SourceCount = $sourcePins.Count
        ImmutableContentCount = $immutableContentPins.Count
        BuildSurfaceStateCount = $buildJournal.Count
        ForcedHeaderGeneration = $true
        RenderingCapableFreshEditorProcesses = $stageResults.Count
        Stages = @($stageResults)
        ProtectedUE54SessionsUnchanged = $true
        CollisionNavigationSensorRfAuthority = $false
    }
    $commitPath = Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $commitPath `
        -Encoding utf8NoBOM -NoNewline
    $committed = $true
    $commit | ConvertTo-Json -Depth 20
}
catch {
    $transactionError = $_.Exception
}
finally {
    if (-not $committed) {
        $rollbackErrors = [Collections.Generic.List[string]]::new()
        try { Assert-NativeIdle 'rollback entry' }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackErrors.Count -eq 0) {
            try { Restore-MapFromJournal $mapJournal[0] }
            catch { $rollbackErrors.Add("map: $($_.Exception.Message)") }
            try { Restore-BuildSurface $buildJournal }
            catch { $rollbackErrors.Add("build: $($_.Exception.Message)") }
            try { Restore-FileJournal $contentJournal }
            catch { $rollbackErrors.Add("content: $($_.Exception.Message)") }
            try { Restore-FileJournal $sourceJournal }
            catch { $rollbackErrors.Add("source: $($_.Exception.Message)") }
        }
        try { Assert-ProtectedUnchanged $script:protectedBefore }
        catch { $rollbackErrors.Add("protected: $($_.Exception.Message)") }
        $rollbackReport = [pscustomobject] [ordered] @{
            Schema = $schema
            Status = if ($rollbackErrors.Count -eq 0) { 'ROLLED_BACK' } else { 'ROLLBACK_INCOMPLETE' }
            RunToken = $RunToken
            Failure = if ($null -ne $transactionError) { $transactionError.Message } else { 'unknown' }
            Errors = @($rollbackErrors)
            Map = Get-FileState $mapFile
            RuntimeDll = Get-FileState $runtimeDll
            EditorDll = Get-FileState $editorDll
            ProtectedUE54SessionsUnchanged = $rollbackErrors.Count -eq 0
        }
        $rollbackPath = Join-Path $transactionRoot 'rollback.json'
        $rollbackReport | ConvertTo-Json -Depth 16 | Set-Content `
            -LiteralPath $rollbackPath -Encoding utf8NoBOM -NoNewline
    }
}

if (-not $committed) {
    throw "R26 native transaction failed and reported $($rollbackReport.Status): $($transactionError.Message). Receipt=$(Join-Path $transactionRoot 'rollback.json')"
}
