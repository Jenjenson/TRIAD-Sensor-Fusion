#requires -Version 7.0

<#
.SYNOPSIS
Runs the guarded V5D Context Facade R29 native successor transaction.

.DESCRIPTION
The default invocation and -StaticSelfCheck are repository-only and never
write the native project. -Execute is the sole write authority and additionally
requires caller-supplied receipts for the exact facade-predecessor map and both
native editor DLLs. Those receipts may be the original R28-environment state or
the outputs of the R29 vegetation transaction, provided the facade predecessor
is still exactly R28. The live transaction promotes exactly seven isolated code
files and three hash-bound R29 source files, builds with forced header
generation, creates exactly twelve isolated R29 facade packages, and performs
one guarded map save through the R29 editor library.

The map endpoint prepares R29 fully hidden beside the validated R28 environment,
removes R28, and activates R29 only afterward. The unchanged R28 connective
public realm and outer ground are retained; R28 architecture cannot co-render.
R29 vegetation, provider, geospatial, simulation, collision, sensor and RF
surfaces are outside the mutation closure. Failure restores the map, promoted
files, build surfaces and initially absent R29 content without recursive delete.

.EXAMPLE
.\Invoke-IstanaExploreV5DContextFacadeR29NativeTransactionV1.ps1 -RunToken review -StaticSelfCheck

.EXAMPLE
.\Invoke-IstanaExploreV5DContextFacadeR29NativeTransactionV1.ps1 -RunToken reviewed -Execute -RequireR28FacadePredecessor -ExpectedVegetationOwner R29 -ExpectedMapBytes <post-vegetation-bytes> -ExpectedMapSha256 <post-vegetation-sha256> -ExpectedRuntimeDllBytes <post-vegetation-bytes> -ExpectedRuntimeDllSha256 <post-vegetation-sha256> -ExpectedEditorDllBytes <post-vegetation-bytes> -ExpectedEditorDllSha256 <post-vegetation-sha256>
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,
    [switch] $StaticSelfCheck,
    [Alias('RequireR28Predecessor')]
    [switch] $RequireR28FacadePredecessor,

    [ValidateSet('R28', 'R29')]
    [string] $ExpectedVegetationOwner = 'R28',

    [long] $ExpectedMapBytes = 0L,
    [string] $ExpectedMapSha256 = '',
    [long] $ExpectedRuntimeDllBytes = 0L,
    [string] $ExpectedRuntimeDllSha256 = '',
    [long] $ExpectedEditorDllBytes = 0L,
    [string] $ExpectedEditorDllSha256 = '',

    [ValidateRange(120, 1800)]
    [int] $EditorTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema = 'triad.istana_explore_v5d.context_facade_r29.native_transaction.v1'
$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L # fixed 10 GiB
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repositoryUnrealRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'unreal'))
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'TRIAD.uproject'))
$engineRoot = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'))
$editor = [IO.Path]::GetFullPath((Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'))
$protectedEditor = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedProject = [IO.Path]::GetFullPath('C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Saved\TRIAD\NativeTransactions\V5DContextFacadeR29V1'))
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase $RunToken))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Plugins\TRIADSensorFusion\Intermediate'))
$r29ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29'))
$r28ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR28'))
$vegetationR29ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\VegetationR29'))
$landmarkVegetationR28ContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR28'))
$treeRealismContentRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism'))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$facadeLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'

$sourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h'
        Bytes = 6485L
        Sha256 = '848E321EA15BA73394799F1456C81CBE3E4D710A68382CFEF6663264E416F79E'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR29FacadeEnvironmentActor.cpp'
        Bytes = 25340L
        Sha256 = 'BBB5F40E79BD99CA57E54849EBBAFC64B97C8CAAEE3F7484C9C9E43C0860CDBA'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DR29FacadeEnvironmentActorTests.cpp'
        Bytes = 4409L
        Sha256 = '60A79F1427677EBA645CE48B83B277C04E9787D4112D21AAB41213F9DE0979FD'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory.h'
        Bytes = 855L
        Sha256 = '27497431F2ABCC300A77EB052E43240EC175BFB21F7718754DF6F842FB807BCA'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory.cpp'
        Bytes = 36828L
        Sha256 = '9115D5A416F32BFDBC8560A80EE7F0E52955566B49650A336C24F152EFD4F09C'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.h'
        Bytes = 2792L
        Sha256 = '137B141B334FE2788176A307D9AE17A788DCBC0148FDA624F995093BDDE9CAB6'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.cpp'
        Bytes = 33165L
        Sha256 = 'E6F07D50B7C599F5098629CCC8A6812552E5F515EF75D5FE0ADABE1BB863B17D'
        NativeBeforePresent = $false
    }
)

$sourceAssetPins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R29ContextFacadeCoverage\Generated\SM_IPV5D_R29_ContextFacadeCoverage_Render.obj'
        Bytes = 102051194L
        Sha256 = '8CE9659A991FC18369EC980FBCA8F9EFAF00B81784D9B2F423BEC935D13181C7'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R29ContextFacadeCoverage\Generated\IstanaPublicViewV5DR29ContextFacadeCoverage.mtl'
        Bytes = 1580L
        Sha256 = '4989AFC4478B48C55E038F1E85233C8FA1405360EA19C7E623FDB98FD3B59ADD'
        NativeBeforePresent = $false
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R29ContextFacadeCoverage\Generated\IstanaPublicViewV5DR29ContextFacadeCoverage.manifest.json'
        Bytes = 11557L
        Sha256 = 'DE2543394A10901FFA6025E325E8E92449CF866F4FC9934B99D9C9FCAEF0ECA2'
        NativeBeforePresent = $false
    }
)

$r29ContentRelativePaths = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Meshes\SM_IPV5D_R29_ContextFacadeCoverage_Render.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_GlassCool.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_GlassWarm.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_GlassNeutral.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_FrameLight.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_FrameDark.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_FrameBronze.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_SillLight.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_SillDark.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_RoofTrim.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_Canopy.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsRealismR29\Materials\MI_IPV5D_R29_BalconyRail.uasset'
)

function Get-FileState {
    param([string] $Path)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject] [ordered] @{
            Present = $false; Bytes = 0L; Sha256 = 'ABSENT'
        }
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
    if ($actual.Present -ne $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label state mismatch: $Path expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Test-ContainedPath {
    param([string] $Path, [string] $Root)
    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $fullPath.Equals($fullRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith($fullRoot + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Assert-Pins {
    param([object[]] $Pins, [string] $Root, [switch] $NativeAfter)
    foreach ($pin in $Pins) {
        if ([IO.Path]::IsPathRooted($pin.RelativePath) -or
            $pin.RelativePath.Contains('..')) {
            throw "Non-canonical promotion path: $($pin.RelativePath)"
        }
        $path = [IO.Path]::GetFullPath((Join-Path $Root $pin.RelativePath))
        if (-not (Test-ContainedPath $path $Root)) {
            throw "Promotion path escaped root: $path"
        }
        $expected = if ($NativeAfter) {
            [pscustomobject] @{ Present = $true; Bytes = [int64] $pin.Bytes; Sha256 = [string] $pin.Sha256 }
        }
        else {
            [pscustomobject] @{ Present = $true; Bytes = [int64] $pin.Bytes; Sha256 = [string] $pin.Sha256 }
        }
        [void] (Assert-State $expected $path 'pinned source')
    }
}

function Assert-NativePredecessorAbsent {
    param([object[]] $Pins)
    foreach ($pin in $Pins) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $pin.RelativePath))
        if ($pin.NativeBeforePresent -or [IO.File]::Exists($path)) {
            throw "R29 isolated native predecessor must be absent: $path"
        }
    }
}

function Get-TreeReceipt {
    param([string] $Root)
    if (-not [IO.Directory]::Exists($Root)) { return @() }
    @(
        Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath = [IO.Path]::GetRelativePath($Root, $_.FullName)
                Bytes = [int64] $_.Length
                Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
            }
        }
    )
}

function Assert-TreeReceipt {
    param([string] $Root, [object[]] $Expected, [string] $Label)
    $actual = @(Get-TreeReceipt $Root)
    $beforeJson = ConvertTo-Json @($Expected) -Depth 4 -Compress
    $afterJson = ConvertTo-Json @($actual) -Depth 4 -Compress
    if ($beforeJson -cne $afterJson) {
        throw "$Label immutable tree changed: $Root"
    }
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
            Path = $path; RelativePath = $relative; Before = $state
            Backup = if ($state.Present) { $backup } else { $null }
        })
    }
    @($rows)
}

function Restore-FileJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows)) {
        if ($row.Before.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($row.Path))
            Copy-Item -LiteralPath $row.Backup -Destination $row.Path -Force
            [void] (Assert-State $row.Before $row.Path 'restored file')
        }
        elseif ([IO.File]::Exists($row.Path)) {
            Remove-Item -LiteralPath $row.Path -Force
        }
    }
}

function New-TreeJournal {
    param([string[]] $Roots, [string] $BackupRoot)
    $journals = [Collections.Generic.List[object]]::new()
    foreach ($root in $Roots) {
        $directories = if ([IO.Directory]::Exists($root)) {
            @(Get-ChildItem -LiteralPath $root -Directory -Recurse | ForEach-Object FullName) + @($root)
        } else { @() }
        $files = if ([IO.Directory]::Exists($root)) {
            @(Get-ChildItem -LiteralPath $root -File -Recurse | ForEach-Object FullName)
        } else { @() }
        $journals.Add([pscustomobject] [ordered] @{
            Root = $root
            Directories = @($directories | Sort-Object -Unique)
            Files = @(New-FileJournal $files $BackupRoot)
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
                if (-not $beforeFiles.Contains($file.FullName)) {
                    Remove-Item -LiteralPath $file.FullName -Force
                }
            }
        }
        Restore-FileJournal @($journal.Files)
        $beforeDirectories = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($directory in @($journal.Directories)) { [void] $beforeDirectories.Add([string] $directory) }
        if ([IO.Directory]::Exists($journal.Root)) {
            $currentDirectories = @(
                Get-ChildItem -LiteralPath $journal.Root -Directory -Recurse | ForEach-Object FullName
            ) + @($journal.Root)
            foreach ($directory in @($currentDirectories | Sort-Object Length -Descending)) {
                if (-not $beforeDirectories.Contains($directory) -and
                    @(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
                    [IO.Directory]::Delete($directory, $false)
                }
            }
        }
    }
}

function Remove-IsolatedR29Content {
    if (-not [IO.Directory]::Exists($r29ContentRoot)) { return }
    if (-not (Test-ContainedPath $r29ContentRoot (Join-Path $nativeProjectRoot 'Content'))) {
        throw 'R29 content rollback root escaped native Content.'
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $r29ContentRoot -File -Recurse)) {
        Remove-Item -LiteralPath $file.FullName -Force
    }
    $directories = @(Get-ChildItem -LiteralPath $r29ContentRoot -Directory -Recurse | ForEach-Object FullName) + @($r29ContentRoot)
    foreach ($directory in @($directories | Sort-Object Length -Descending)) {
        if (@(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
            [IO.Directory]::Delete($directory, $false)
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
                ProcessId = [uint32] $_.ProcessId
                CreationDate = [string] $_.CreationDate
                ExecutablePath = [IO.Path]::GetFullPath($_.ExecutablePath)
                CommandLine = [string] $_.CommandLine
            }
        }
    )
}

function Assert-ProtectedUnchanged {
    param([object[]] $Before)
    if ((ConvertTo-Json @($Before) -Compress) -cne
        (ConvertTo-Json @(Get-ProtectedProcesses) -Compress)) {
        throw 'Protected UE5.4 CAPSTONE process set changed.'
    }
}

function Assert-NativeIdle {
    param([string] $Label)
    $busy = @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $_.ExecutablePath -and
            [IO.Path]::GetFullPath($_.ExecutablePath).StartsWith($engineRoot + '\', [StringComparison]::OrdinalIgnoreCase) -and
            $_.Name -like 'UnrealEditor*'
        }
    )
    if ($busy.Count -ne 0) {
        throw "$Label refused: UE5.5 editor/helper process is already present."
    }
}

function Assert-LaunchAdmission {
    param([string] $Label)
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    $freeBytes = [int64] $os.FreeVirtualMemory * 1024L
    if ($freeBytes -lt $minimumSystemFreeVirtualAtLaunchBytes) {
        throw "$Label refused by fixed 10 GiB FreeVirtualMemory gate: available=$freeBytes"
    }
    [pscustomobject] [ordered] @{
        Label = $Label
        MinimumBytes = $minimumSystemFreeVirtualAtLaunchBytes
        ObservedBytes = $freeBytes
        Status = 'PASS'
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
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
        generateTransaction = $false
    } | ConvertTo-Json -Depth 8 -Compress
    Invoke-RestMethod -Method Put -Uri $rcUri -ContentType 'application/json' -Body $body -TimeoutSec $TimeoutSeconds
}

function Get-OwnedHelperIdentity {
    param([Diagnostics.Process] $Handle, [string] $Log)
    $record = Get-CimInstance Win32_Process -Filter "ProcessId=$($Handle.Id)" -ErrorAction Stop
    if ($null -eq $record -or -not $record.ExecutablePath -or
        -not [IO.Path]::GetFullPath($record.ExecutablePath).Equals($editor, [StringComparison]::OrdinalIgnoreCase) -or
        -not $record.CommandLine.Contains($mapPackage, [StringComparison]::Ordinal) -or
        -not $record.CommandLine.Contains($Log, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Could not prove exact launched UE5.5 helper identity.'
    }
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $record.ProcessId
        CreationDate = [string] $record.CreationDate
        ExecutablePath = [IO.Path]::GetFullPath($record.ExecutablePath)
        CommandLine = [string] $record.CommandLine
    }
}

function Stop-OwnedHelper {
    param(
        [Diagnostics.Process] $Handle,
        $Identity,
        [string] $Log
    )
    if ([uint32] $Handle.Id -ne [uint32] $Identity.ProcessId) {
        throw 'Process handle does not match exact owned helper.'
    }
    try { [void] (Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30) } catch {}
    if (-not $Handle.WaitForExit($ShutdownTimeoutSeconds * 1000)) {
        [void] (Get-OwnedHelperIdentity $Handle $Log)
        # Forced containment is limited to the still-proven handle returned by
        # this wrapper. No process-name or broad Stop-Process action is used.
        $Handle.Kill()
        if (-not $Handle.WaitForExit(30000)) {
            throw 'Exact owned helper did not exit after bounded containment.'
        }
    }
}

function Invoke-ColdStage {
    param(
        [string] $Stage,
        [string] $FunctionName,
        [string] $TextProperty,
        [string] $ExpectedPrefix,
        [hashtable] $Parameters = @{}
    )
    Assert-NativeIdle "before $Stage"
    Assert-ProtectedUnchanged $script:protectedBefore
    [void] (Assert-LaunchAdmission "before helper $Stage")
    $logRoot = Join-Path $transactionRoot 'logs'
    [void] [IO.Directory]::CreateDirectory($logRoot)
    $log = [IO.Path]::GetFullPath((Join-Path $logRoot "$Stage.log"))
    if ([IO.File]::Exists($log)) { throw "Stage log already exists: $log" }
    $argumentLine = "`"$nativeProjectFile`" $mapPackage -DisablePlugin=AirSim " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-RemoteControlHttpServer -RCWebControlEnable ' +
        '-ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$log`""
    $handle = Start-Process -FilePath $editor -ArgumentList $argumentLine -WorkingDirectory $nativeProjectRoot -PassThru -WindowStyle Hidden
    $identity = $null
    $response = $null
    $stageError = $null
    try {
        $identityDeadline = [DateTime]::UtcNow.AddSeconds(30)
        do {
            try { $identity = Get-OwnedHelperIdentity $handle $log } catch { $identity = $null }
            if ($null -ne $identity) { break }
            Start-Sleep -Milliseconds 500
        } while ([DateTime]::UtcNow -lt $identityDeadline)
        if ($null -eq $identity) { throw "Could not prove helper identity for $Stage" }
        $readyDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
        do {
            try {
                $ready = Invoke-RcCall $identityLibrary 'ValidateIstanaExploreRemoteControlProject' @{ ExpectedProjectPath = $nativeProjectRoot } 30
            } catch { $ready = $null }
            if ($null -ne $ready -and $ready.ReturnValue -eq $true) { break }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        if ($null -eq $ready -or $ready.ReturnValue -ne $true) {
            throw "Remote-control identity did not become ready for $Stage"
        }
        [void] (Get-OwnedHelperIdentity $handle $log)
        $response = Invoke-RcCall $facadeLibrary $FunctionName $Parameters $EditorTimeoutSeconds
        $text = [string] $response.$TextProperty
        if ($response.ReturnValue -ne $true -or
            -not $text.StartsWith($ExpectedPrefix, [StringComparison]::Ordinal)) {
            throw "Stage $Stage failed exact response gate: response=$text"
        }
    } catch { $stageError = $_.Exception }
    finally {
        if ($null -eq $identity -and -not $handle.HasExited) {
            try { $identity = Get-OwnedHelperIdentity $handle $log }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
                else { $stageError = [InvalidOperationException]::new("stage={$($stageError.Message)} identityCleanup={$($_.Exception.Message)}") }
            }
        }
        if ($null -ne $identity -and -not $handle.HasExited) {
            try { Stop-OwnedHelper $handle $identity $log }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
                else { $stageError = [InvalidOperationException]::new("stage={$($stageError.Message)} cleanup={$($_.Exception.Message)}") }
            }
        }
    }
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle "after $Stage"
    if ($null -ne $stageError) { throw $stageError }
    if (-not [IO.File]::Exists($log) -or (Get-Item -LiteralPath $log).Length -le 0) {
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

function Assert-StaticContract {
    if ($sourcePins.Count -ne 7 -or $sourceAssetPins.Count -ne 3 -or
        $r29ContentRelativePaths.Count -ne 12) {
        throw 'R29 facade closure must be exactly 7 code + 3 source assets + 12 content packages.'
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in @($sourcePins) + @($sourceAssetPins)) {
        if (-not $seen.Add([string] $pin.RelativePath)) {
            throw "Duplicate R29 facade promotion path: $($pin.RelativePath)"
        }
        if ($pin.NativeBeforePresent) {
            throw "R29 facade promotion destination is not declared initially absent: $($pin.RelativePath)"
        }
        if ($pin.RelativePath.Contains('R29Vegetation', [StringComparison]::Ordinal) -or
            $pin.RelativePath.Contains('LandmarkVegetation', [StringComparison]::Ordinal)) {
            throw "Vegetation source escaped into R29 facade mutation closure: $($pin.RelativePath)"
        }
    }
    Assert-Pins $sourcePins $repositoryUnrealRoot
    Assert-Pins $sourceAssetPins $repositoryUnrealRoot
    $actor = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[1].RelativePath) -Raw
    $factory = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[4].RelativePath) -Raw
    $editorHeader = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[5].RelativePath) -Raw
    $editorSource = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot $sourcePins[6].RelativePath) -Raw
    foreach ($marker in @(
        'ConfigurePreparedR29FacadeHandoff',
        'ActivateAfterR28EnvironmentRemoval',
        'r28ArchitectureActiveRenderers=%d',
        'bR28ArchitectureRetainedOrCoRendered',
        'collision=false', 'simulationAuthority=false',
        'sensorAuthority=false', 'rfAuthority=false')) {
        if (-not $actor.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R29 facade actor marker missing: $marker"
        }
    }
    foreach ($receipt in @(
        '8CE9659A991FC18369EC980FBCA8F9EFAF00B81784D9B2F423BEC935D13181C7',
        '4989AFC4478B48C55E038F1E85233C8FA1405360EA19C7E623FDB98FD3B59ADD',
        'DE2543394A10901FFA6025E325E8E92449CF866F4FC9934B99D9C9FCAEF0ECA2',
        'FacadeTriangleCount = 256850',
        'FacadeSourceCornerCount = 770550',
        'bottomFrameRails=20011',
        'everyGroupedApertureFourSided=true')) {
        if (-not $factory.Contains($receipt, [StringComparison]::Ordinal)) {
            throw "R29 facade source/topology receipt missing: $receipt"
        }
    }
    foreach ($endpoint in @(
        'EnsureR29FacadeEnvironmentAssets',
        'ValidateR29FacadeEnvironmentAssets',
        'ApplyR29FacadeReplacementToLoadedV5DHybridMap',
        'CommitR29FacadeReplacementToLoadedV5DHybridMap',
        'ValidateR29FacadeReplacementInLoadedV5DHybridMap')) {
        if (-not $editorHeader.Contains($endpoint, [StringComparison]::Ordinal) -or
            -not $editorSource.Contains($endpoint, [StringComparison]::Ordinal)) {
            throw "R29 facade editor endpoint missing: $endpoint"
        }
    }
    foreach ($marker in @(
        'ValidateComposableVegetationOwner',
        'ValidateLandmarkVegetationR28',
        'ValidateR29Vegetation',
        'Composable vegetation owner requires exact R28 xor R29',
        'vegetationOwnerMode=EXACT_R28_XOR_R29',
        'vegetationActorsOrAssetsModified=false')) {
        if (-not $editorSource.Contains($marker, [StringComparison]::Ordinal)) {
            throw "R29 facade composable vegetation-owner marker missing: $marker"
        }
    }
    [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'STATIC_SELF_CHECK_PASS'
        CodeSourceCount = $sourcePins.Count
        SourceAssetCount = $sourceAssetPins.Count
        NewContentPackageCount = $r29ContentRelativePaths.Count
        NativeTreeWritten = $false
        UnrealBuildOrEditorLaunched = $false
        R28PublicRealmRetained = $true
        R28ArchitectureConcurrentRenderingAllowed = $false
        VegetationMutationAllowed = $false
        SimulationSensorRfAuthority = $false
    }
}

$staticReceipt = Assert-StaticContract
if ($StaticSelfCheck) {
    $staticReceipt | ConvertTo-Json -Depth 8
    exit 0
}
if (-not $Execute) {
    [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'READ_ONLY_REPOSITORY_PREFLIGHT_PASS'
        ExecuteRequiredForNativeWrites = $true
        ExplicitPredecessorAndDllReceiptsRequired = $true
        NativeTreeWritten = $false
        UnrealBuildOrEditorLaunched = $false
        StaticReceipt = $staticReceipt
    } | ConvertTo-Json -Depth 8
    exit 0
}

foreach ($name in @(
    'ExpectedVegetationOwner',
    'ExpectedMapBytes', 'ExpectedMapSha256',
    'ExpectedRuntimeDllBytes', 'ExpectedRuntimeDllSha256',
    'ExpectedEditorDllBytes', 'ExpectedEditorDllSha256')) {
    if (-not $PSBoundParameters.ContainsKey($name)) {
        throw "Live R29 facade execution requires explicit caller-supplied parameter: $name"
    }
}
if (-not $RequireR28FacadePredecessor) {
    throw 'Live R29 facade execution requires -RequireR28FacadePredecessor (legacy alias: -RequireR28Predecessor).'
}
foreach ($pair in @(
    @($ExpectedMapBytes, $ExpectedMapSha256),
    @($ExpectedRuntimeDllBytes, $ExpectedRuntimeDllSha256),
    @($ExpectedEditorDllBytes, $ExpectedEditorDllSha256))) {
    if ([int64] $pair[0] -le 0 -or [string] $pair[1] -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'Live R29 facade receipts require positive bytes and exact SHA-256.'
    }
}
foreach ($required in @($nativeProjectFile, $buildTool, $editor, $mapFile, $runtimeDll, $editorDll)) {
    if (-not [IO.File]::Exists($required)) { throw "Required native file missing: $required" }
}
if ([IO.Directory]::Exists($transactionRoot)) {
    throw "Transaction run token already exists: $transactionRoot"
}
if ([IO.Directory]::Exists($r29ContentRoot)) {
    throw "Initially absent R29 facade content root already exists: $r29ContentRoot"
}
Assert-NativePredecessorAbsent $sourcePins
Assert-NativePredecessorAbsent $sourceAssetPins
$expectedMap = [pscustomobject] @{ Present = $true; Bytes = $ExpectedMapBytes; Sha256 = $ExpectedMapSha256.ToUpperInvariant() }
$expectedRuntime = [pscustomobject] @{ Present = $true; Bytes = $ExpectedRuntimeDllBytes; Sha256 = $ExpectedRuntimeDllSha256.ToUpperInvariant() }
$expectedEditor = [pscustomobject] @{ Present = $true; Bytes = $ExpectedEditorDllBytes; Sha256 = $ExpectedEditorDllSha256.ToUpperInvariant() }
[void] (Assert-State $expectedMap $mapFile 'explicit facade-predecessor map')
[void] (Assert-State $expectedRuntime $runtimeDll 'runtime DLL predecessor')
[void] (Assert-State $expectedEditor $editorDll 'editor DLL predecessor')
$script:protectedBefore = @(Get-ProtectedProcesses)
Assert-NativeIdle 'prewrite boundary'
Assert-ProtectedUnchanged $script:protectedBefore
$prewriteAdmission = Assert-LaunchAdmission 'before first native write'

[void] [IO.Directory]::CreateDirectory($transactionRoot)
$journalRoot = Join-Path $transactionRoot 'rollback'
$sourceDestinations = @($sourcePins | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath)) })
$sourceAssetDestinations = @($sourceAssetPins | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath)) })
$sourceJournal = @(New-FileJournal $sourceDestinations (Join-Path $journalRoot 'source'))
$sourceAssetJournal = @(New-FileJournal $sourceAssetDestinations (Join-Path $journalRoot 'source-assets'))
$mapJournal = @(New-FileJournal @($mapFile) (Join-Path $journalRoot 'map'))
$buildJournal = @(New-TreeJournal @($pluginBinaryRoot, $pluginIntermediateRoot) (Join-Path $journalRoot 'build'))
$immutableBefore = [ordered] @{
    R28Environment = @(Get-TreeReceipt $r28ContentRoot)
    VegetationR29 = @(Get-TreeReceipt $vegetationR29ContentRoot)
    LandmarkVegetationR28 = @(Get-TreeReceipt $landmarkVegetationR28ContentRoot)
    TreeRealism = @(Get-TreeReceipt $treeRealismContentRoot)
}
$backupMap = [IO.Path]::GetFullPath((Join-Path $transactionRoot 'Istana_PublicView_Explore_v5d_hybrid.facade-predecessor.umap'))
Copy-Item -LiteralPath $mapFile -Destination $backupMap
[void] (Assert-State $expectedMap $backupMap 'external map backup')

$stageResults = [Collections.Generic.List[object]]::new()
$committed = $false
$failure = $null
try {
    foreach ($pin in @($sourcePins) + @($sourceAssetPins)) {
        $source = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot $pin.RelativePath))
        $destination = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $pin.RelativePath))
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        Copy-Item -LiteralPath $source -Destination $destination
    }
    Assert-Pins $sourcePins $nativeProjectRoot -NativeAfter
    Assert-Pins $sourceAssetPins $nativeProjectRoot -NativeAfter
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'before build'
    $buildAdmission = Assert-LaunchAdmission 'before forced R29 facade build'
    $buildLog = Join-Path $transactionRoot 'build.log'
    $buildArguments = @(
        'UnrealEditor', 'Win64', 'Development',
        "-Project=$nativeProjectFile", '-WaitMutex', '-NoHotReloadFromIDE',
        '-Module=TRIADSensorFusion', '-Module=TRIADSensorFusionEditor',
        '-ForceHeaderGeneration', '-NoUBTMakefiles',
        '-MaxParallelActions=1'
    )
    & $buildTool @buildArguments *> $buildLog
    if ($LASTEXITCODE -ne 0) { throw "R29 facade build failed: exit=$LASTEXITCODE" }
    $runtimeAfter = Get-FileState $runtimeDll
    $editorAfter = Get-FileState $editorDll
    if (-not $runtimeAfter.Present -or -not $editorAfter.Present -or
        $runtimeAfter.Sha256 -ceq $expectedRuntime.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedEditor.Sha256) {
        throw 'Forced build did not produce fresh runtime and editor DLL receipts.'
    }
    $runtimeBytes = [IO.File]::ReadAllBytes($runtimeDll)
    $editorBytes = [IO.File]::ReadAllBytes($editorDll)
    $runtimeText = [Text.Encoding]::ASCII.GetString($runtimeBytes) + [Text.Encoding]::Unicode.GetString($runtimeBytes)
    $editorText = [Text.Encoding]::ASCII.GetString($editorBytes) + [Text.Encoding]::Unicode.GetString($editorBytes)
    foreach ($marker in @('ATRIADIstanaExploreV5DR29FacadeEnvironmentActor', 'ActivateAfterR28EnvironmentRemoval')) {
        if (-not $runtimeText.Contains($marker, [StringComparison]::Ordinal)) { throw "Runtime DLL lacks R29 facade marker: $marker" }
    }
    foreach ($marker in @('CommitR29FacadeReplacementToLoadedV5DHybridMap', 'ValidateR29FacadeReplacementInLoadedV5DHybridMap')) {
        if (-not $editorText.Contains($marker, [StringComparison]::Ordinal)) { throw "Editor DLL lacks R29 facade marker: $marker" }
    }

    $stageResults.Add((Invoke-ColdStage '01_ensure_r29_facade_assets' 'EnsureR29FacadeEnvironmentAssets' 'OutMessage' 'EXPLORE_V5D_R29_FACADE_ASSET_BUILD_PASS'))
    foreach ($relative in $r29ContentRelativePaths) {
        $state = Get-FileState ([IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative)))
        if (-not $state.Present -or $state.Bytes -le 0 -or $state.Sha256 -notmatch '^[A-F0-9]{64}$') {
            throw "R29 facade content package missing after asset stage: $relative"
        }
    }
    if (@(Get-ChildItem -LiteralPath $r29ContentRoot -File -Recurse).Count -ne 12) {
        throw 'R29 facade content root is not the exact twelve-file namespace.'
    }
    $stageResults.Add((Invoke-ColdStage '02_validate_r29_facade_assets' 'ValidateR29FacadeEnvironmentAssets' 'OutReport' 'ISTANA_EXPLORE_V5D_R29_FACADE_ASSETS_VALID'))
    $stageResults.Add((Invoke-ColdStage '03_commit_r29_facade_replacement' 'CommitR29FacadeReplacementToLoadedV5DHybridMap' 'OutReport' 'ISTANA_EXPLORE_V5D_R29_FACADE_COMMIT_PASS' @{
        ExpectedPredecessorBytes = [int64] $ExpectedMapBytes
        ExpectedPredecessorSha256 = $ExpectedMapSha256.ToUpperInvariant()
        ExpectedVegetationOwner = $ExpectedVegetationOwner.ToUpperInvariant()
        VerifiedExternalBackupFilename = $backupMap
    }))
    $stageResults.Add((Invoke-ColdStage '04_cold_validate_r29_facade_replacement' 'ValidateR29FacadeReplacementInLoadedV5DHybridMap' 'OutReport' 'ISTANA_EXPLORE_V5D_R29_FACADE_REPLACEMENT_MAP_VALID'))

    Assert-TreeReceipt $r28ContentRoot $immutableBefore.R28Environment 'R28 environment/public realm'
    Assert-TreeReceipt $vegetationR29ContentRoot $immutableBefore.VegetationR29 'R29 vegetation'
    Assert-TreeReceipt $landmarkVegetationR28ContentRoot $immutableBefore.LandmarkVegetationR28 'R28 landmark vegetation'
    Assert-TreeReceipt $treeRealismContentRoot $immutableBefore.TreeRealism 'tree realism'
    [void] (Assert-State $expectedMap $backupMap 'preserved external map backup')
    $successorMap = Get-FileState $mapFile
    if (-not $successorMap.Present -or $successorMap.Bytes -le 0 -or
        $successorMap.Sha256 -notmatch '^[A-F0-9]{64}$' -or
        ($successorMap.Bytes -eq $expectedMap.Bytes -and $successorMap.Sha256 -ceq $expectedMap.Sha256)) {
        throw 'R29 successor map receipt did not change from exact R28 predecessor.'
    }
    $commit = [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'COMMITTED'
        RunToken = $RunToken
        PrewriteAdmission = $prewriteAdmission
        BuildAdmission = $buildAdmission
        PredecessorMap = $expectedMap
        SuccessorMap = $successorMap
        ExternalBackup = Get-FileState $backupMap
        RuntimeDllBefore = $expectedRuntime
        RuntimeDllAfter = $runtimeAfter
        EditorDllBefore = $expectedEditor
        EditorDllAfter = $editorAfter
        StageResults = @($stageResults)
        R29ContentPackageCount = 12
        R28PublicRealmRetained = $true
        R28ArchitectureActiveRenderers = 0
        R28ArchitectureConcurrentRenderingAllowed = $false
        PredecessorVegetationOwner = $ExpectedVegetationOwner.ToUpperInvariant()
        VegetationMutationAllowed = $false
        SimulationCollisionNavigationSensorRfAuthority = $false
        VisualCaptureAccepted = $false
        CaptureRevalidationRequired = $true
    }
    $commitPath = Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $commitPath -Encoding utf8NoBOM
    $committed = $true
    $commit | ConvertTo-Json -Depth 12
}
catch {
    $failure = $_.Exception
}
finally {
    if (-not $committed) {
        $rollbackErrors = [Collections.Generic.List[string]]::new()
        try { Restore-FileJournal $mapJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
        try { Remove-IsolatedR29Content } catch { $rollbackErrors.Add($_.Exception.Message) }
        try { Restore-TreeJournal $buildJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
        try { Restore-FileJournal $sourceAssetJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
        try { Restore-FileJournal $sourceJournal } catch { $rollbackErrors.Add($_.Exception.Message) }
        try { Assert-ProtectedUnchanged $script:protectedBefore } catch { $rollbackErrors.Add($_.Exception.Message) }
        $rollback = [pscustomobject] [ordered] @{
            Schema = $schema
            Status = if ($rollbackErrors.Count -eq 0) { 'ROLLED_BACK' } else { 'ROLLBACK_INCOMPLETE' }
            Failure = if ($null -eq $failure) { 'unknown' } else { $failure.Message }
            RollbackErrors = @($rollbackErrors)
        }
        $rollback | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $transactionRoot 'rollback.json') -Encoding utf8NoBOM
        if ($rollbackErrors.Count -ne 0) {
            throw "R29 facade transaction failed and rollback was incomplete: failure={$($rollback.Failure)} rollback={$([string]::Join(' | ', @($rollbackErrors)))}"
        }
    }
}
if ($null -ne $failure) { throw $failure }
