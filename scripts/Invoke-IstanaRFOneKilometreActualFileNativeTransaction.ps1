#requires -Version 7.0
<#
.SYNOPSIS
Installs and proves the single OneKilometreV2 native actual-file RF test.

.DESCRIPTION
This host-specific, fail-closed transaction promotes the reviewed indexed-RF
validator correction, its native regression, and the absent-or-exact actual-
file test as one atomic source set. It backs up all present native source
predecessors and the fixed allowlist of module outputs plus all five compiler
intermediates for each promoted source, builds only the runtime module with one
parallel action, and runs exactly one automation-test filter. Any failure
restores all three original source states and every allowlisted build product.

.EXAMPLE
pwsh -NoLogo -NoProfile -File .\scripts\Invoke-IstanaRFOneKilometreActualFileNativeTransaction.ps1 -RunToken rf_actual_20260906T010000Z -Apply
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,47}$')]
    [string] $RunToken,

    [ValidateRange(60, 1800)]
    [int] $AutomationTimeoutSeconds = 900,

    [switch] $Apply,

    [switch] $StaticSelfCheck
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($StaticSelfCheck) {
    $repositoryUnrealRoot = [IO.Path]::GetFullPath(
        (Join-Path (Split-Path -Parent $PSScriptRoot) 'unreal'))
}
else {
    $repositoryUnrealRoot = [IO.Path]::GetFullPath(
        'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo\unreal')
}
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath(
    (Join-Path $nativeProjectRoot 'TRIAD.uproject'))
$engineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath(
    (Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'))
$editorCommand = [IO.Path]::GetFullPath(
    (Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'))
$protectedUE54Editor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedUE54Project = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')

$sourcePins = @(
    [pscustomobject] [ordered] @{
        Role = 'INDEXED_GEOMETRY_VALIDATOR'
        RelativePath =
            'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADRFIndexedGeometryQuery.cpp'
        TargetBytes = 167670L
        TargetSha256 =
            '606FD58A90D396D34F5648B78D26A6BDD7510662565DD095CABC84DC6800FCAB'
        NativePolicy = 'EXACT_PREDECESSOR_OR_TARGET'
        PredecessorPresent = $true
        PredecessorBytes = 166878L
        PredecessorSha256 =
            'A56719963B602795F5B8A4C2715C51BA39650D9F124686C71FDE418BCF4D3C1D'
        CompileSource = 'TRIADRFIndexedGeometryQuery.cpp'
        CompileLogPattern =
            '(?m)^\s*\[\d+/\d+\]\s+Compile\s+\[x64\]\s+TRIADRFIndexedGeometryQuery\.cpp\s*$'
        CompiledObjectRelativePath =
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQuery.cpp.obj'
    },
    [pscustomobject] [ordered] @{
        Role = 'INDEXED_GEOMETRY_NATIVE_REGRESSION'
        RelativePath =
            'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADRFIndexedGeometryQueryTests.cpp'
        TargetBytes = 73946L
        TargetSha256 =
            '459831E13FC9793FDC966A3DF78876443BC078059B9D4A2D12E3AD7A99D5E823'
        NativePolicy = 'EXACT_PREDECESSOR_OR_TARGET'
        PredecessorPresent = $true
        PredecessorBytes = 69037L
        PredecessorSha256 =
            'A17DF12D9B4B365E39290D80B4461F79B03315181E978E10184258D487805003'
        CompileSource = 'TRIADRFIndexedGeometryQueryTests.cpp'
        CompileLogPattern =
            '(?m)^\s*\[\d+/\d+\]\s+Compile\s+\[x64\]\s+TRIADRFIndexedGeometryQueryTests\.cpp\s*$'
        CompiledObjectRelativePath =
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQueryTests.cpp.obj'
    },
    [pscustomobject] [ordered] @{
        Role = 'ONE_KILOMETRE_ACTUAL_FILE_TEST'
        RelativePath =
            'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADRFOneKilometreActualFileTests.cpp'
        TargetBytes = 4839L
        TargetSha256 =
            'DA1CA1CA4BEF5E21AB47B1ECBF501CB7DBEFCC8219F5E6D5EC6CCB34CDAA0D71'
        NativePolicy = 'MISSING_OR_TARGET'
        PredecessorPresent = $false
        PredecessorBytes = 0L
        PredecessorSha256 = 'ABSENT'
        CompileSource = 'TRIADRFOneKilometreActualFileTests.cpp'
        CompileLogPattern =
            '(?m)^\s*\[\d+/\d+\]\s+Compile\s+\[x64\]\s+TRIADRFOneKilometreActualFileTests\.cpp\s*$'
        CompiledObjectRelativePath =
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFOneKilometreActualFileTests.cpp.obj'
    }
)

$resourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath =
            'Plugins\TRIADSensorFusion\Resources\RF\IstanaPublicViewRFOneKilometreV2.geometry.json'
        Bytes = 12506346L
        Sha256 =
            '85E654FBA602B2DBC51EB64C6B66FF234C8EA1F948152612C755EDC22C65DA51'
    },
    [pscustomobject] [ordered] @{
        RelativePath =
            'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_materials_one_kilometre_v2.catalog.json'
        Bytes = 28674L
        Sha256 =
            '210CB26DDB9B531ADEB3C917606A736AEFC857EB6696DA485A2E63DBB8B31662'
    },
    [pscustomobject] [ordered] @{
        RelativePath =
            'Plugins\TRIADSensorFusion\Resources\RF\istana_rf_scene_one_kilometre_v2.contract.json'
        Bytes = 14783L
        Sha256 =
            'FE509917AE59BE0918BCD799F23DC981E00A394C6C328A7342F56B371C40BCC2'
    }
)
$expectedProjectBytes = 1298L
$expectedProjectSha256 =
    '42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3'
$automationFilter =
    'TRIAD.RF.IndexedGeometryQuery.OneKilometreV2ActualFile'
$compiledTestObjectRelativePath = [string] $sourcePins[2].CompiledObjectRelativePath

# These are the exact durable module outputs and all five source-associated
# compiler intermediates (.obj, .dep.json, .obj.rsp, .obj.rsp.old, and .sarif)
# this focused build may replace or create for each promoted source. Every
# pre-existing item is copied and hash-verified before any source is admitted.
$buildProductRelativePaths = @(
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll',
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.pdb',
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.exp',
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor.modules',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\UnrealEditor-TRIADSensorFusion.lib',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\UnrealEditor-TRIADSensorFusion.exp',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\UnrealEditor-TRIADSensorFusion.dll.rsp',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\UnrealEditor-TRIADSensorFusion.dll.rsp.old',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\UnrealEditor-TRIADSensorFusion.lib.rsp',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\UnrealEditor-TRIADSensorFusion.lib.rsp.old',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQuery.cpp.obj',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQuery.cpp.dep.json',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQuery.cpp.obj.rsp',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQuery.cpp.obj.rsp.old',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQuery.cpp.sarif',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQueryTests.cpp.obj',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQueryTests.cpp.dep.json',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQueryTests.cpp.obj.rsp',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQueryTests.cpp.obj.rsp.old',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQueryTests.cpp.sarif',
    $compiledTestObjectRelativePath,
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFOneKilometreActualFileTests.cpp.dep.json',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFOneKilometreActualFileTests.cpp.obj.rsp',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFOneKilometreActualFileTests.cpp.obj.rsp.old',
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFOneKilometreActualFileTests.cpp.sarif'
)
$expectedBuildProductCount = 25
$expectedSourceAssociatedIntermediateCount = 15

# The previous failed transaction compiled these two objects but, under the
# older 15-product contract, did not journal them. Their exact live bytes are
# therefore the admitted current pre-state for the next transaction. They are
# not claimed to be the older pre-failure baseline. The next transaction backs
# them up and restores these exact identities on failure.
$admittedBuildProductPrestatePins = @(
    [pscustomobject] [ordered] @{
        RelativePath =
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQuery.cpp.obj'
        Present = $true
        Bytes = 3380560L
        Sha256 =
            'A18231509929785E4BEC4ED26B15EADDFF9B2EDB798FD4A0425105C9C0906472'
        Admission = 'EXACT_CURRENT_POST_FAILURE_PRESTATE'
    },
    [pscustomobject] [ordered] @{
        RelativePath =
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADRFIndexedGeometryQueryTests.cpp.obj'
        Present = $true
        Bytes = 1583646L
        Sha256 =
            'C3FA184BBFC1364A6E58113A4B5FB9CA3D58C049F9ABEF3CCD6C3B4FA9B22762'
        Admission = 'EXACT_CURRENT_POST_FAILURE_PRESTATE'
    }
)

$transactionBase = [IO.Path]::GetFullPath(
    (Join-Path $nativeProjectRoot `
        'Saved\TRIAD\RFActualFileNativeTransactions'))
$transactionRoot = [IO.Path]::GetFullPath(
    (Join-Path $transactionBase $RunToken))
$backupRoot = Join-Path $transactionRoot 'native_before'
$sourceBackupRoot = Join-Path $backupRoot 'sources'
$buildProductBackupRoot = Join-Path $backupRoot 'build_products'
$buildLog = Join-Path $transactionRoot 'build.log'
$automationRoot = Join-Path $transactionRoot 'automation'
$automationReportRoot = Join-Path $automationRoot 'report'
$automationLog = Join-Path $automationRoot 'automation.log'
$preparedPath = Join-Path $transactionRoot 'prepared.json'
$receiptPath = Join-Path $transactionRoot 'receipt.json'
$failurePath = Join-Path $transactionRoot 'failure.json'
$script:protectedUE54Before = $null

function Test-ContainedPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $fullPath -ceq $fullRoot -or $fullPath.StartsWith(
        $fullRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)
}

function Assert-ContainedPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    if (-not (Test-ContainedPath -Path $Path -Root $Root)) {
        throw "$Label escaped its reviewed root: $Path"
    }
}

function Assert-NoReparseAncestor {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    Assert-ContainedPath -Path $Path -Root $Root -Label $Label
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $cursor = [IO.Path]::GetFullPath($Path)
    if (-not [IO.Directory]::Exists($cursor)) {
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    while (-not [string]::IsNullOrWhiteSpace($cursor) -and
        (Test-ContainedPath -Path $cursor -Root $rootFull)) {
        if ([IO.Directory]::Exists($cursor) -or [IO.File]::Exists($cursor)) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band
                    [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "$Label contains a reparse point: $cursor"
            }
        }
        if ($cursor -ceq $rootFull) {
            break
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
}

function Get-PathState {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($fullPath)) {
        if ([IO.Directory]::Exists($fullPath)) {
            throw "Expected a file or absence, but found a directory: $fullPath"
        }
        return [pscustomobject] [ordered] @{
            Path = $fullPath
            Present = $false
            Bytes = 0L
            Sha256 = 'ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $fullPath -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing non-regular reparse-point file: $fullPath"
    }
    [pscustomobject] [ordered] @{
        Path = $fullPath
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = [string] (Get-FileHash -LiteralPath $fullPath `
            -Algorithm SHA256).Hash
    }
}

function Assert-SameState {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $actual = Get-PathState -Path $Path
    if ([bool] $actual.Present -ne [bool] $Expected.Present -or
        $actual.Bytes -ne [int64] $Expected.Bytes -or
        $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label changed: expected=$($Expected.Present)/$($Expected.Bytes)/$($Expected.Sha256) actual=$($actual.Present)/$($actual.Bytes)/$($actual.Sha256) path=$Path"
    }
    $actual
}

function Test-StateIdentity {
    param(
        [Parameter(Mandatory = $true)] $State,
        [Parameter(Mandatory = $true)] [bool] $Present,
        [Parameter(Mandatory = $true)] [int64] $Bytes,
        [Parameter(Mandatory = $true)] [string] $Sha256
    )

    [bool] $State.Present -eq $Present -and
        $State.Bytes -eq $Bytes -and
        $State.Sha256 -ceq $Sha256
}

function Assert-ExactFile {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [int64] $Bytes,
        [Parameter(Mandatory = $true)] [string] $Sha256,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $actual = Get-PathState -Path $Path
    if (-not $actual.Present -or $actual.Bytes -ne $Bytes -or
        $actual.Sha256 -cne $Sha256) {
        throw "$Label identity mismatch: expected=$Bytes/$Sha256 actual=$($actual.Present)/$($actual.Bytes)/$($actual.Sha256) path=$Path"
    }
    $actual
}

function Write-JsonAtomic {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] $Value
    )

    if ([IO.File]::Exists($Path) -or [IO.Directory]::Exists($Path)) {
        throw "Refusing to overwrite transaction evidence: $Path"
    }
    $temporary = $Path + '.tmp.' + [Guid]::NewGuid().ToString('N')
    $json = $Value | ConvertTo-Json -Depth 20
    $stream = [IO.File]::Open(
        $temporary, [IO.FileMode]::CreateNew,
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
    [IO.File]::Move($temporary, $Path)
}

function Get-RemoteControlListeners {
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
                throw 'The Capstone project token is owned by an unexpected process; refusing native TRIAD work.'
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
        throw 'Protected UE5.4/Capstone identity set changed during the RF transaction.'
    }
    $after
}

function Get-NativeTRIADUnrealProcesses {
    $enginePrefix = $engineRoot.TrimEnd('\', '/') +
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
                    -Token $nativeProjectFile)
            $isUE55 -or $isNativeTRIAD
        })
}

function Assert-NativeProjectIdle {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    if ($null -eq $script:protectedUE54Before) {
        throw 'Protected UE5.4/Capstone preflight identity was not established.'
    }
    [void] (Assert-ProtectedUE54Unchanged `
        -Before $script:protectedUE54Before)
    $editors = @(Get-NativeTRIADUnrealProcesses)
    if ($editors.Count -ne 0) {
        $ids = @($editors | ForEach-Object { $_.ProcessId }) -join ','
        throw "UE5.5/native TRIAD Unreal helpers must be idle at '$Checkpoint' (PIDs=$ids)."
    }
    $listeners = @(Get-RemoteControlListeners)
    if ($listeners.Count -ne 0) {
        throw "RC port 30010 must be idle at '$Checkpoint' (PIDs=$([string]::Join(',', $listeners)))."
    }
}

function Assert-WorkspaceAndNativeInputs {
    $pins = [Collections.Generic.List[object]]::new()
    foreach ($source in $sourcePins) {
        $workspacePath = [IO.Path]::GetFullPath(
            (Join-Path $repositoryUnrealRoot `
                ([string] $source.RelativePath)))
        $pins.Add((Assert-ExactFile -Path $workspacePath `
            -Bytes ([int64] $source.TargetBytes) `
            -Sha256 ([string] $source.TargetSha256) `
            -Label "reviewed RF source $($source.Role)"))
    }
    foreach ($resource in $resourcePins) {
        $workspacePath = Join-Path $repositoryUnrealRoot `
            ([string] $resource.RelativePath)
        $nativePath = Join-Path $nativeProjectRoot `
            ([string] $resource.RelativePath)
        $pins.Add((Assert-ExactFile -Path $workspacePath `
            -Bytes ([int64] $resource.Bytes) `
            -Sha256 ([string] $resource.Sha256) `
            -Label 'reviewed workspace OneKilometreV2 resource'))
        $pins.Add((Assert-ExactFile -Path $nativePath `
            -Bytes ([int64] $resource.Bytes) `
            -Sha256 ([string] $resource.Sha256) `
            -Label 'native OneKilometreV2 resource'))
    }
    $pins.Add((Assert-ExactFile -Path $nativeProjectFile `
        -Bytes $expectedProjectBytes -Sha256 $expectedProjectSha256 `
        -Label 'native TRIAD project'))
    $pins.Add((Assert-SameState -Expected $script:selfPin `
        -Path $PSCommandPath -Label 'transaction wrapper'))
    @($pins)
}

function Assert-NativeSourceAdmissible {
    param([Parameter(Mandatory = $true)] $Pin)

    $nativePath = [IO.Path]::GetFullPath(
        (Join-Path $nativeProjectRoot ([string] $Pin.RelativePath)))
    Assert-NoReparseAncestor -Path $nativePath `
        -Root $nativeProjectRoot -Label "native RF source $($Pin.Role)"
    $state = Get-PathState -Path $nativePath
    $isTarget = Test-StateIdentity -State $state -Present $true `
        -Bytes ([int64] $Pin.TargetBytes) `
        -Sha256 ([string] $Pin.TargetSha256)
    $isPredecessor = Test-StateIdentity -State $state `
        -Present ([bool] $Pin.PredecessorPresent) `
        -Bytes ([int64] $Pin.PredecessorBytes) `
        -Sha256 ([string] $Pin.PredecessorSha256)
    if (-not $isTarget -and -not $isPredecessor) {
        throw "Native RF source is neither its exact predecessor nor target under $($Pin.NativePolicy): $nativePath"
    }
    [pscustomobject] [ordered] @{
        Pin = $Pin
        NativePath = $nativePath
        Before = $state
        BeforeIdentity = if ($isTarget) { 'TARGET' }
            elseif ($state.Present) { 'PREDECESSOR' }
            else { 'ABSENT' }
        RequiresPromotion = -not $isTarget
    }
}

function New-NativeSourceJournal {
    Assert-NoReparseAncestor -Path $sourceBackupRoot `
        -Root $nativeProjectRoot -Label 'native-source backup root'
    [void] (New-Item -ItemType Directory -Path $sourceBackupRoot)
    $rows = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $sourcePins.Count; ++$index) {
        $admission = Assert-NativeSourceAdmissible -Pin $sourcePins[$index]
        $workspacePath = [IO.Path]::GetFullPath(
            (Join-Path $repositoryUnrealRoot `
                ([string] $sourcePins[$index].RelativePath)))
        $workspace = Assert-ExactFile -Path $workspacePath `
            -Bytes ([int64] $sourcePins[$index].TargetBytes) `
            -Sha256 ([string] $sourcePins[$index].TargetSha256) `
            -Label "reviewed RF source $($sourcePins[$index].Role)"
        $backupPath = $null
        $backup = $null
        if ($admission.Before.Present) {
            $backupPath = Join-Path $sourceBackupRoot `
                ('{0:D2}_{1}' -f $index,
                    [IO.Path]::GetFileName($admission.NativePath))
            [IO.File]::Copy($admission.NativePath, $backupPath, $false)
            $backup = Assert-SameState -Expected $admission.Before `
                -Path $backupPath -Label 'native-source backup'
        }
        $promotionStagePath = Join-Path $sourceBackupRoot `
            ('{0:D2}_promotion_target.stage' -f $index)
        $promotionDisplacedPath = Join-Path $sourceBackupRoot `
            ('{0:D2}_promotion_predecessor.displaced' -f $index)
        foreach ($temporaryPath in @(
                $promotionStagePath, $promotionDisplacedPath)) {
            Assert-NoReparseAncestor -Path $temporaryPath `
                -Root $nativeProjectRoot `
                -Label 'native-source promotion temporary path'
            if ([IO.File]::Exists($temporaryPath) -or
                [IO.Directory]::Exists($temporaryPath)) {
                throw "Native-source promotion temporary path is not fresh: $temporaryPath"
            }
        }
        $rows.Add([pscustomobject] [ordered] @{
            Role = [string] $sourcePins[$index].Role
            RelativePath = [string] $sourcePins[$index].RelativePath
            WorkspacePath = $workspacePath
            Workspace = $workspace
            NativePath = $admission.NativePath
            NativePolicy = [string] $sourcePins[$index].NativePolicy
            BeforeIdentity = $admission.BeforeIdentity
            Before = $admission.Before
            Target = [pscustomobject] [ordered] @{
                Path = $admission.NativePath
                Present = $true
                Bytes = [int64] $sourcePins[$index].TargetBytes
                Sha256 = [string] $sourcePins[$index].TargetSha256
            }
            RequiresPromotion = [bool] $admission.RequiresPromotion
            CompileSource = [string] $sourcePins[$index].CompileSource
            CompileLogPattern = [string] $sourcePins[$index].CompileLogPattern
            CompiledObjectRelativePath =
                [string] $sourcePins[$index].CompiledObjectRelativePath
            BackupPath = $backupPath
            Backup = $backup
            PromotionStagePath = $promotionStagePath
            PromotionDisplacedPath = $promotionDisplacedPath
            PromotionDisplaced = $null
        })
    }
    @($rows)
}

function Install-NativeSource {
    param([Parameter(Mandatory = $true)] $JournalRow)

    [void] (Assert-SameState -Expected $JournalRow.Before `
        -Path $JournalRow.NativePath `
        -Label "native RF source before promotion $($JournalRow.Role)")
    if (-not $JournalRow.RequiresPromotion) {
        return Assert-SameState -Expected $JournalRow.Target `
            -Path $JournalRow.NativePath `
            -Label "already-target native RF source $($JournalRow.Role)"
    }
    [void] (Assert-SameState -Expected $JournalRow.Workspace `
        -Path $JournalRow.WorkspacePath `
        -Label "workspace RF source before promotion $($JournalRow.Role)")
    $stage = [string] $JournalRow.PromotionStagePath
    $displaced = [string] $JournalRow.PromotionDisplacedPath
    if ([string]::IsNullOrWhiteSpace($stage) -or
        [string]::IsNullOrWhiteSpace($displaced)) {
        throw 'Native RF source promotion paths must be non-empty.'
    }
    Assert-NoReparseAncestor -Path $stage -Root $nativeProjectRoot `
        -Label "RF source promotion stage $($JournalRow.Role)"
    Assert-NoReparseAncestor -Path $displaced -Root $nativeProjectRoot `
        -Label "RF source promotion displacement $($JournalRow.Role)"
    if ([IO.File]::Exists($stage) -or [IO.Directory]::Exists($stage) -or
        [IO.File]::Exists($displaced) -or
        [IO.Directory]::Exists($displaced)) {
        throw "Native RF source promotion paths are not fresh for $($JournalRow.Role)."
    }
    [IO.File]::Copy($JournalRow.WorkspacePath, $stage, $false)
    [void] (Assert-SameState -Expected $JournalRow.Target `
        -Path $stage -Label "staged RF source $($JournalRow.Role)")
    if ($JournalRow.Before.Present) {
        [IO.File]::Replace(
            $stage, $JournalRow.NativePath, $displaced, $true)
        $JournalRow.PromotionDisplaced = Assert-SameState `
            -Expected $JournalRow.Before -Path $displaced `
            -Label "displaced native RF source $($JournalRow.Role)"
    }
    else {
        [IO.File]::Move($stage, $JournalRow.NativePath)
    }
    Assert-SameState -Expected $JournalRow.Target `
        -Path $JournalRow.NativePath `
        -Label "promoted native RF source $($JournalRow.Role)"
}

function Assert-BuildProductAllowlistContract {
    if ($buildProductRelativePaths.Count -ne $expectedBuildProductCount) {
        throw "RF build-product allowlist count drifted: expected=$expectedBuildProductCount actual=$($buildProductRelativePaths.Count)"
    }
    $pathSet = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($relativePath in $buildProductRelativePaths) {
        if ([string]::IsNullOrWhiteSpace([string] $relativePath) -or
            -not $pathSet.Add([string] $relativePath)) {
            throw "RF build-product allowlist contains an empty or duplicate path: $relativePath"
        }
    }

    $sourceAssociatedPaths = [Collections.Generic.List[string]]::new()
    foreach ($source in $sourcePins) {
        $objectPath = [string] $source.CompiledObjectRelativePath
        if (-not $objectPath.EndsWith(
                '.obj', [StringComparison]::Ordinal)) {
            throw "RF compiled-object path has an unexpected suffix: $objectPath"
        }
        $sourceBasePath = $objectPath.Substring(0, $objectPath.Length - 4)
        foreach ($path in @(
                $objectPath,
                ($sourceBasePath + '.dep.json'),
                ($sourceBasePath + '.obj.rsp'),
                ($sourceBasePath + '.obj.rsp.old'),
                ($sourceBasePath + '.sarif'))) {
            if (-not $pathSet.Contains($path)) {
                throw "RF source-associated intermediate is absent from the fixed allowlist: $path"
            }
            $sourceAssociatedPaths.Add($path)
        }
    }
    if ($sourceAssociatedPaths.Count -ne
        $expectedSourceAssociatedIntermediateCount) {
        throw "RF source-associated intermediate count drifted: expected=$expectedSourceAssociatedIntermediateCount actual=$($sourceAssociatedPaths.Count)"
    }

    $pinnedPathSet = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in $admittedBuildProductPrestatePins) {
        $relativePath = [string] $pin.RelativePath
        if (-not $pathSet.Contains($relativePath) -or
            -not $pinnedPathSet.Add($relativePath)) {
            throw "RF build-product pre-state pin is outside the allowlist or duplicated: $relativePath"
        }
        if ([string] $pin.Admission -cne
            'EXACT_CURRENT_POST_FAILURE_PRESTATE' -or
            -not [bool] $pin.Present -or [int64] $pin.Bytes -le 0 -or
            [string] $pin.Sha256 -cnotmatch '^[0-9A-F]{64}$') {
            throw "RF build-product pre-state pin is malformed: $relativePath"
        }
    }
    if ($pinnedPathSet.Count -ne 2) {
        throw "RF build-product pre-state pin count drifted: expected=2 actual=$($pinnedPathSet.Count)"
    }

    [pscustomobject] [ordered] @{
        BuildProductCount = $pathSet.Count
        SourceAssociatedIntermediateCount = $sourceAssociatedPaths.Count
        PinnedPrestateCount = $pinnedPathSet.Count
        SourceAssociatedIntermediatePaths = @($sourceAssociatedPaths)
    }
}

function New-BuildProductJournal {
    Assert-NoReparseAncestor -Path $buildProductBackupRoot `
        -Root $nativeProjectRoot -Label 'build-product backup root'
    [void] (New-Item -ItemType Directory -Path $buildProductBackupRoot)
    $rows = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $buildProductRelativePaths.Count; ++$index) {
        $relativePath = [string] $buildProductRelativePaths[$index]
        $nativePath = [IO.Path]::GetFullPath(
            (Join-Path $nativeProjectRoot $relativePath))
        Assert-NoReparseAncestor -Path $nativePath `
            -Root $nativeProjectRoot -Label 'allowlisted build product'
        $before = Get-PathState -Path $nativePath
        $prestatePins = @($admittedBuildProductPrestatePins | Where-Object {
                [string] $_.RelativePath -ceq $relativePath
            })
        if ($prestatePins.Count -gt 1) {
            throw "RF build-product pre-state pin is duplicated: $relativePath"
        }
        $prestatePin = if ($prestatePins.Count -eq 1) {
            $prestatePins[0]
        }
        else {
            $null
        }
        $beforeAdmission = 'TRANSACTION_START_EXACT_SNAPSHOT'
        if ($null -ne $prestatePin) {
            if (-not (Test-StateIdentity -State $before `
                        -Present ([bool] $prestatePin.Present) `
                        -Bytes ([int64] $prestatePin.Bytes) `
                        -Sha256 ([string] $prestatePin.Sha256))) {
                throw "Native RF build product does not match its admitted current post-failure pre-state: $nativePath"
            }
            $beforeAdmission = [string] $prestatePin.Admission
        }
        $backupPath = $null
        $backup = $null
        if ($before.Present) {
            $backupPath = Join-Path $buildProductBackupRoot `
                ('{0:D2}_{1}' -f $index, [IO.Path]::GetFileName($nativePath))
            [IO.File]::Copy($nativePath, $backupPath, $false)
            $backup = Assert-SameState -Expected $before `
                -Path $backupPath -Label 'build-product backup'
        }
        $rows.Add([pscustomobject] [ordered] @{
            RelativePath = $relativePath
            NativePath = $nativePath
            Before = $before
            BeforeAdmission = $beforeAdmission
            PrestatePin = $prestatePin
            BackupPath = $backupPath
            Backup = $backup
        })
    }
    @($rows)
}

function Restore-OneFileState {
    param(
        [Parameter(Mandatory = $true)] $Before,
        [Parameter(Mandatory = $true)] [AllowNull()] $Backup,
        [Parameter(Mandatory = $true)] [AllowNull()] [AllowEmptyString()]
        [string] $BackupPath,
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    Assert-NoReparseAncestor -Path $Path `
        -Root $nativeProjectRoot -Label $Label
    if ($Before.Present) {
        if ($null -eq $Backup -or [string]::IsNullOrWhiteSpace($BackupPath)) {
            throw "$Label cannot restore a present predecessor without its backup."
        }
        [void] (Assert-SameState -Expected $Backup `
            -Path $BackupPath -Label "$Label authenticated backup")
        $stage = $Path + '.rf_actual_file_restore_' +
            [Guid]::NewGuid().ToString('N') + '.tmp'
        [IO.File]::Copy($BackupPath, $stage, $false)
        [void] (Assert-SameState -Expected $Before `
            -Path $stage -Label "$Label restore stage")
        if ([IO.File]::Exists($Path)) {
            [void] (Get-PathState -Path $Path)
            $displaced = $stage + '.displaced'
            [IO.File]::Replace($stage, $Path, $displaced, $true)
            [void] (Assert-SameState -Expected $Before `
                -Path $Path -Label "$Label restored destination")
            if ([IO.File]::Exists($displaced)) {
                [IO.File]::Delete($displaced)
            }
        }
        else {
            [IO.File]::Move($stage, $Path)
        }
    }
    else {
        if ($null -ne $Backup -or
            -not [string]::IsNullOrWhiteSpace($BackupPath)) {
            throw "$Label has backup material for an absent predecessor."
        }
        if ([IO.File]::Exists($Path)) {
            [void] (Get-PathState -Path $Path)
            [IO.File]::Delete($Path)
        }
    }
    [void] (Assert-SameState -Expected $Before `
        -Path $Path -Label "$Label final rollback state")
}

function Restore-NativeTransaction {
    param(
        [Parameter(Mandatory = $true)] [object[]] $SourceJournal,
        [Parameter(Mandatory = $true)] [object[]] $BuildJournal
    )

    if ($SourceJournal.Count -ne $sourcePins.Count -or
        $BuildJournal.Count -ne $expectedBuildProductCount) {
        throw "RF rollback requires the complete fixed journal: sources=$($SourceJournal.Count)/$($sourcePins.Count) buildProducts=$($BuildJournal.Count)/$expectedBuildProductCount"
    }
    Assert-NativeProjectIdle -Checkpoint 'before RF transaction rollback'
    foreach ($row in $SourceJournal) {
        $current = Get-PathState -Path $row.NativePath
        $matchesBefore = Test-StateIdentity -State $current `
            -Present ([bool] $row.Before.Present) `
            -Bytes ([int64] $row.Before.Bytes) `
            -Sha256 ([string] $row.Before.Sha256)
        $matchesTarget = Test-StateIdentity -State $current `
            -Present $true -Bytes ([int64] $row.Target.Bytes) `
            -Sha256 ([string] $row.Target.Sha256)
        if (-not $matchesBefore -and -not $matchesTarget) {
            throw "Native RF source changed to unrecognized bytes before rollback: $($row.NativePath)"
        }
        Restore-OneFileState -Before $row.Before -Backup $row.Backup `
            -BackupPath $row.BackupPath -Path $row.NativePath `
            -Label "native RF source $($row.Role)"
    }
    foreach ($row in $BuildJournal) {
        Restore-OneFileState -Before $row.Before -Backup $row.Backup `
            -BackupPath $row.BackupPath -Path $row.NativePath `
            -Label "build product $($row.RelativePath)"
    }
    Assert-NativeProjectIdle -Checkpoint 'after RF transaction rollback'
    [pscustomobject] [ordered] @{
        Status = 'ROLLED_BACK'
        NativeSourceCount = $SourceJournal.Count
        BuildProductCount = $BuildJournal.Count
        NativeSources = @($SourceJournal | ForEach-Object {
                Get-PathState -Path $_.NativePath
            })
        BuildProducts = @($BuildJournal | ForEach-Object {
                Get-PathState -Path $_.NativePath
            })
    }
}

function Test-ExactCommandLineToken {
    param(
        [Parameter(Mandatory = $true)] [string] $CommandLine,
        [Parameter(Mandatory = $true)] [string] $Token
    )

    $pattern = '(?i)(?:^|\s)"?' + [regex]::Escape($Token) +
        '"?(?:\s|$)'
    [regex]::Matches($CommandLine, $pattern).Count -eq 1
}

function Get-AutomationProcessIdentity {
    param([Parameter(Mandatory = $true)] [Diagnostics.Process] $Process)

    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        $row = Get-CimInstance Win32_Process `
            -Filter "ProcessId = $($Process.Id)" -ErrorAction SilentlyContinue
        if ($null -ne $row -and
            -not [string]::IsNullOrWhiteSpace([string] $row.ExecutablePath) -and
            -not [string]::IsNullOrWhiteSpace([string] $row.CommandLine)) {
            $identity = [pscustomobject] [ordered] @{
                ProcessId = [uint32] $row.ProcessId
                Name = [string] $row.Name
                ExecutablePath = [IO.Path]::GetFullPath(
                    [string] $row.ExecutablePath)
                CommandLine = [string] $row.CommandLine
                CreationUtcTicks =
                    ([DateTimeOffset] $row.CreationDate).UtcTicks
            }
            if ($identity.Name -cne 'UnrealEditor-Cmd.exe' -or
                $identity.ExecutablePath -cne $editorCommand -or
                -not (Test-ExactCommandLineToken `
                    -CommandLine $identity.CommandLine `
                    -Token $nativeProjectFile) -or
                -not $identity.CommandLine.Contains(
                    "Automation RunTests $automationFilter",
                    [StringComparison]::Ordinal) -or
                -not $identity.CommandLine.Contains(
                    $automationLog,
                    [StringComparison]::OrdinalIgnoreCase)) {
                throw 'Launched process did not prove the exact RF automation identity.'
            }
            return $identity
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'RF automation process identity could not be established.'
}

function Assert-OwnedAutomationBoundary {
    param(
        [Parameter(Mandatory = $true)] $Identity,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    $protected = Assert-ProtectedUE54Unchanged `
        -Before $script:protectedUE54Before
    $row = Get-CimInstance Win32_Process `
        -Filter "ProcessId = $($Identity.ProcessId)" `
        -ErrorAction SilentlyContinue
    if ($null -eq $row -or
        [int64] ([DateTimeOffset] $row.CreationDate).UtcTicks -ne
            [int64] $Identity.CreationUtcTicks -or
        [string] $row.Name -cne [string] $Identity.Name -or
        [IO.Path]::GetFullPath([string] $row.ExecutablePath) -cne
            [string] $Identity.ExecutablePath -or
        [string] $row.CommandLine -cne [string] $Identity.CommandLine) {
        throw "Owned RF automation identity changed at '$Checkpoint'."
    }
    $nativeEditors = @(Get-NativeTRIADUnrealProcesses)
    if ($nativeEditors.Count -ne 1 -or
        [uint32] $nativeEditors[0].ProcessId -ne
            [uint32] $Identity.ProcessId) {
        $ids = @($nativeEditors | ForEach-Object { $_.ProcessId }) -join ','
        throw "Owned RF automation is not the sole UE5.5/native TRIAD helper at '$Checkpoint' (PIDs=$ids)."
    }
    $listeners = @(Get-RemoteControlListeners)
    $foreignListeners = @($listeners | Where-Object {
            [uint32] $_ -ne [uint32] $Identity.ProcessId
        })
    if ($foreignListeners.Count -ne 0) {
        throw "RC port 30010 has a foreign owner at '$Checkpoint' (PIDs=$([string]::Join(',', $foreignListeners)))."
    }
    [pscustomobject] [ordered] @{
        Checkpoint = $Checkpoint
        ProtectedUE54 = $protected
        OwnedProcess = $Identity
        RemoteControlListenerPids = $listeners
    }
}

function Stop-OwnedAutomation {
    param([Parameter(Mandatory = $true)] $Identity)

    $row = Get-CimInstance Win32_Process `
        -Filter "ProcessId = $($Identity.ProcessId)" `
        -ErrorAction SilentlyContinue
    if ($null -eq $row) {
        return
    }
    $creationTicks = ([DateTimeOffset] $row.CreationDate).UtcTicks
    $executable = if ([string]::IsNullOrWhiteSpace(
            [string] $row.ExecutablePath)) {
        ''
    }
    else { [IO.Path]::GetFullPath([string] $row.ExecutablePath) }
    if ($row.Name -cne 'UnrealEditor-Cmd.exe' -or
        $executable -cne $editorCommand -or
        [int64] $creationTicks -ne [int64] $Identity.CreationUtcTicks -or
        [string] $row.CommandLine -cne [string] $Identity.CommandLine) {
        throw 'Refusing to stop a process whose exact RF automation identity changed.'
    }
    Stop-Process -Id ([uint32] $Identity.ProcessId) -Force `
        -ErrorAction Stop
}

function Invoke-ExactAutomationTest {
    Assert-NativeProjectIdle -Checkpoint 'before RF actual-file automation'
    [void] (New-Item -ItemType Directory `
        -Path $automationReportRoot -Force)
    $localDdc = Join-Path $nativeProjectRoot 'Saved\DerivedDataCache'
    if (-not (Test-Path -LiteralPath $localDdc -PathType Container)) {
        throw "Required native DDC directory is absent: $localDdc"
    }
    $argumentLine = '"' + $nativeProjectFile + '" /Game/Maps/Entry ' +
        '-DisablePlugin=AirSim -unattended -nop4 -NoSplash -NullRHI ' +
        '-NoSound -DDC=InstalledNoZenLocalFallback ' +
        '-LocalDataCachePath="' + $localDdc + '" ' +
        '-ExecCmds="Automation RunTests ' + $automationFilter + '" ' +
        '-TestExit="Automation Test Queue Empty" ' +
        '-ReportOutputPath="' + $automationReportRoot + '" ' +
        '-abslog="' + $automationLog + '"'
    $process = $null
    $identity = $null
    $ownedBoundary = $null
    try {
        $process = Start-Process -FilePath $editorCommand `
            -ArgumentList $argumentLine -WorkingDirectory $nativeProjectRoot `
            -PassThru -WindowStyle Hidden
        $identity = Get-AutomationProcessIdentity -Process $process
        $ownedBoundary = Assert-OwnedAutomationBoundary `
            -Identity $identity -Checkpoint 'RF actual-file automation live boundary'
        if (-not $process.WaitForExit($AutomationTimeoutSeconds * 1000)) {
            Stop-OwnedAutomation -Identity $identity
            [void] $process.WaitForExit(30000)
            throw "RF actual-file automation timed out after $AutomationTimeoutSeconds seconds."
        }
        if ($process.ExitCode -ne 0) {
            throw "RF actual-file automation exited with code $($process.ExitCode)."
        }
    }
    catch {
        $primaryError = $_.Exception.Message
        if ($null -ne $process) {
            $process.Refresh()
            if (-not $process.HasExited) {
                if ($null -eq $identity) {
                    $identity = Get-AutomationProcessIdentity -Process $process
                }
                Stop-OwnedAutomation -Identity $identity
                [void] $process.WaitForExit(30000)
            }
        }
        throw $primaryError
    }
    Assert-NativeProjectIdle -Checkpoint 'after RF actual-file automation'
    if (-not [IO.File]::Exists($automationLog)) {
        throw 'RF actual-file automation did not persist its log.'
    }
    $logText = Get-Content -LiteralPath $automationLog -Raw
    $allSuccesses = [regex]::Matches(
        $logText, 'Test Completed\. Result=\{Success\}').Count
    $exactSuccesses = [regex]::Matches(
        $logText,
        'Test Completed\. Result=\{Success\}[^\r\n]*Path=\{' +
            [regex]::Escape($automationFilter) + '\}').Count
    $failures = [regex]::Matches(
        $logText,
        'Test Completed\. Result=\{Fail|Automation Test Failed').Count
    if ($logText -notmatch 'Automation Test Queue Empty' -or
        $allSuccesses -ne 1 -or $exactSuccesses -ne 1 -or
        $failures -ne 0) {
        throw "RF actual-file automation requires exactly one clean Success for $automationFilter; allSuccesses=$allSuccesses exactSuccesses=$exactSuccesses failures=$failures"
    }
    [pscustomobject] [ordered] @{
        Status = 'PASS'
        Filter = $automationFilter
        ExpectedSuccessfulCompletions = 1
        SuccessfulCompletions = $allSuccesses
        ExactFilterSuccessfulCompletions = $exactSuccesses
        FailureCompletions = $failures
        ProcessIdentity = $identity
        LiveOwnershipBoundary = $ownedBoundary
        ExitCode = [int] $process.ExitCode
        Log = Get-PathState -Path $automationLog
        ReportRoot = $automationReportRoot
    }
}

$buildProductAllowlistContract = Assert-BuildProductAllowlistContract
$script:selfPin = Get-PathState -Path $PSCommandPath
$workspaceSourcePins = @($sourcePins | ForEach-Object {
        Assert-ExactFile `
            -Path (Join-Path $repositoryUnrealRoot $_.RelativePath) `
            -Bytes ([int64] $_.TargetBytes) `
            -Sha256 ([string] $_.TargetSha256) `
            -Label "reviewed RF source $($_.Role)"
    })
$workspaceResourcePins = @($resourcePins | ForEach-Object {
        Assert-ExactFile `
            -Path (Join-Path $repositoryUnrealRoot $_.RelativePath) `
            -Bytes ([int64] $_.Bytes) -Sha256 ([string] $_.Sha256) `
            -Label 'reviewed workspace OneKilometreV2 resource'
    })

if ($Apply -and $StaticSelfCheck) {
    throw '-Apply and -StaticSelfCheck are mutually exclusive.'
}
if ($StaticSelfCheck) {
    [pscustomobject] [ordered] @{
        Schema =
            'triad.rf.one_kilometre_v2.actual_file_native_transaction.static_check.v3'
        Status = 'STATIC_SELF_CHECK_PASS'
        Script = $script:selfPin
        SourceTargets = $workspaceSourcePins
        NativeSourcePolicies = @($sourcePins | ForEach-Object {
                [pscustomobject] [ordered] @{
                    Role = $_.Role
                    RelativePath = $_.RelativePath
                    NativePolicy = $_.NativePolicy
                    PredecessorPresent = $_.PredecessorPresent
                    PredecessorBytes = $_.PredecessorBytes
                    PredecessorSha256 = $_.PredecessorSha256
                    TargetBytes = $_.TargetBytes
                    TargetSha256 = $_.TargetSha256
                    CompileSource = $_.CompileSource
                    CompileLogPattern = $_.CompileLogPattern
                    CompiledObjectRelativePath =
                        $_.CompiledObjectRelativePath
                }
            })
        WorkspaceResources = $workspaceResourcePins
        NativeDestinationPolicies = @(
            'EXACT_PREDECESSOR_OR_TARGET',
            'EXACT_PREDECESSOR_OR_TARGET',
            'MISSING_OR_TARGET')
        ExistingSourcePromotionPolicy =
            'FILE_REPLACE_WITH_NONEMPTY_TRANSACTION_DISPLACEMENT'
        PromotionTemporaryFilesRoot = 'TRANSACTION_SOURCE_BACKUP_ROOT'
        PromotedRelativePaths = @($sourcePins.RelativePath)
        BuildTarget = 'UnrealEditor Win64 Development'
        BuildModuleAllowlist = @('TRIADSensorFusion')
        MaximumParallelActions = 1
        ForceSourceDiscoveryFlag = '-NoUBTMakefiles'
        ChangedOrAdmittedSourceCompileMarkersRequired = $true
        ExpectedCompileSources = @($sourcePins.CompileSource)
        CompileObjectLogPatterns = @($sourcePins.CompileLogPattern)
        CompiledTestObjectRelativePath = $compiledTestObjectRelativePath
        BuildProductRelativePaths = $buildProductRelativePaths
        BuildProductCount = $buildProductAllowlistContract.BuildProductCount
        SourceAssociatedIntermediateCount =
            $buildProductAllowlistContract.SourceAssociatedIntermediateCount
        SourceAssociatedIntermediatePaths =
            $buildProductAllowlistContract.SourceAssociatedIntermediatePaths
        AdmittedBuildProductPrestatePins =
            $admittedBuildProductPrestatePins
        BuildProductPrestateAdmissionPolicy =
            'PIN_2_CURRENT_POST_FAILURE_OBJECTS_AND_EXACTLY_SNAPSHOT_ALL_25'
        AutomationFilter = $automationFilter
        RequiredSuccessfulCompletions = 1
        ZeroSuccessfulCompletionsAccepted = $false
        FailureRollback =
            'RESTORE_ALL_3_NATIVE_SOURCE_PREDECESSORS_AND_ALL_25_BUILD_PRODUCT_STATES'
        SuccessBuildProductPolicy =
            'LEAVE_COMPILED_TARGET_PRODUCTS_IN_PLACE'
        ProtectedUE54Policy =
            'SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY'
        NativeUE55AndTRIADHelpersMustBeIdle = $true
        RemoteControlPolicy =
            'IDLE_EXCEPT_OPTIONAL_SOLE_OWNED_AUTOMATION_LISTENER'
        NativeFilesystemReadOrWritten = $false
    } | ConvertTo-Json -Depth 12
    return
}
if (-not $Apply) {
    throw 'Specify -Apply for the native transaction or -StaticSelfCheck for a source-only contract check.'
}

Assert-ContainedPath -Path $transactionRoot `
    -Root $transactionBase -Label 'transaction root'
Assert-NoReparseAncestor -Path $transactionRoot `
    -Root $nativeProjectRoot -Label 'transaction root'
if ([IO.File]::Exists($transactionRoot) -or
    [IO.Directory]::Exists($transactionRoot)) {
    throw "Fresh transaction root already exists: $transactionRoot"
}
$script:protectedUE54Before = Get-ProtectedUE54Identity
Assert-NativeProjectIdle -Checkpoint 'RF transaction preflight'
$inputPins = @(Assert-WorkspaceAndNativeInputs)
$buildToolPin = Get-PathState -Path $buildTool
$editorCommandPin = Get-PathState -Path $editorCommand
if (-not $buildToolPin.Present -or -not $editorCommandPin.Present) {
    throw 'Required UE 5.5 build/editor command is absent.'
}

if (-not [IO.Directory]::Exists($transactionBase)) {
    [void] (New-Item -ItemType Directory -Path $transactionBase)
}
Assert-NoReparseAncestor -Path $transactionBase `
    -Root $nativeProjectRoot -Label 'transaction base'
[void] (New-Item -ItemType Directory -Path $transactionRoot)
$sourceJournal = @()
$buildJournal = @()
$buildResult = $null
$automationResult = $null
try {
    [void] (New-Item -ItemType Directory -Path $backupRoot)
    $sourceJournal = @(New-NativeSourceJournal)
    $buildJournal = @(New-BuildProductJournal)
    if ($sourceJournal.Count -ne $sourcePins.Count -or
        $buildJournal.Count -ne $expectedBuildProductCount) {
        throw "RF transaction journal is incomplete: sources=$($sourceJournal.Count)/$($sourcePins.Count) buildProducts=$($buildJournal.Count)/$expectedBuildProductCount"
    }
    $prepared = [pscustomobject] [ordered] @{
        Schema =
            'triad.rf.one_kilometre_v2.actual_file_native_transaction.prepared.v3'
        Status = 'PREPARED'
        RunToken = $RunToken
        Script = $script:selfPin
        Inputs = $inputPins
        ProtectedUE54Before = $script:protectedUE54Before
        NativeSources = $sourceJournal
        BuildProducts = $buildJournal
        BuildProductCount = $buildJournal.Count
        AutomationFilter = $automationFilter
        RequiredSuccessfulCompletions = 1
    }
    Write-JsonAtomic -Path $preparedPath -Value $prepared
    [void] (Assert-WorkspaceAndNativeInputs)
    Assert-NativeProjectIdle -Checkpoint 'before native RF source promotion'
    $nativeSourcesAfterPromotion = @($sourceJournal | ForEach-Object {
            Install-NativeSource -JournalRow $_
        })
    if ($nativeSourcesAfterPromotion.Count -ne $sourcePins.Count) {
        throw 'Native RF source promotion count is incomplete.'
    }
    [void] (Assert-WorkspaceAndNativeInputs)

    $buildArguments = @(
        'UnrealEditor',
        'Win64',
        'Development',
        $nativeProjectFile,
        '-DisablePlugin=AirSim',
        '-Module=TRIADSensorFusion',
        '-WaitMutex',
        '-NoHotReloadFromIDE',
        '-NoUBTMakefiles',
        '-MaxParallelActions=1',
        '-NoUBA',
        '-NoUBALocal'
    )
    [void] (Assert-SameState -Expected $buildToolPin `
        -Path $buildTool -Label 'UE 5.5 build tool before invocation')
    Assert-NativeProjectIdle -Checkpoint 'before module-scoped RF build'
    & $buildTool @buildArguments *> $buildLog
    $buildExitCode = $LASTEXITCODE
    if ($buildExitCode -ne 0) {
        throw "Module-scoped RF build failed with exit code $buildExitCode."
    }
    $buildLogText = Get-Content -LiteralPath $buildLog -Raw
    $targetUpToDateObserved = [regex]::IsMatch(
        $buildLogText,
        '(?m)^\s*Target is up to date\s*$',
        [Text.RegularExpressions.RegexOptions]::CultureInvariant)
    $changedSourceCount = @($sourceJournal | Where-Object {
            $_.RequiresPromotion
        }).Count
    if ($changedSourceCount -ne 0 -and $targetUpToDateObserved) {
        throw 'Changed or admitted RF sources were not compiled: UBT reported Target is up to date.'
    }
    $compileProofs = @($sourceJournal | ForEach-Object {
            $markerCount = [regex]::Matches(
                $buildLogText,
                [string] $_.CompileLogPattern,
                [Text.RegularExpressions.RegexOptions]::CultureInvariant).Count
            $compiledObject = Get-PathState -Path (
                Join-Path $nativeProjectRoot `
                    ([string] $_.CompiledObjectRelativePath))
            if ($_.RequiresPromotion -and $markerCount -ne 1) {
                throw "Changed or admitted RF source lacks exactly one Compile [x64] marker: $($_.CompileSource) count=$markerCount"
            }
            if ($_.RequiresPromotion -and -not $compiledObject.Present) {
                throw "Changed or admitted RF source did not produce its compiled object: $($_.CompileSource)"
            }
            [pscustomobject] [ordered] @{
                Role = $_.Role
                CompileSource = $_.CompileSource
                Required = [bool] $_.RequiresPromotion
                MarkerCount = $markerCount
                ExactlyOneMarkerObserved = $markerCount -eq 1
                CompiledObject = $compiledObject
            }
        })
    Assert-NativeProjectIdle -Checkpoint 'after module-scoped RF build'
    [void] (Assert-WorkspaceAndNativeInputs)
    $nativeSourcesAfterBuild = @($sourceJournal | ForEach-Object {
            Assert-SameState -Expected $_.Target -Path $_.NativePath `
                -Label "native RF source after build $($_.Role)"
        })
    $buildProductsAfter = @($buildJournal | ForEach-Object {
            Get-PathState -Path $_.NativePath
        })
    $runtimeDllAfter = $buildProductsAfter[0]
    if (-not $runtimeDllAfter.Present) {
        throw 'Module-scoped RF build did not produce the runtime DLL.'
    }
    $buildResult = [pscustomobject] [ordered] @{
        Status = 'PASS'
        ExitCode = [int] $buildExitCode
        Tool = $buildToolPin
        Arguments = $buildArguments
        MaximumParallelActions = 1
        NoUbtMakefiles = $true
        ChangedOrAdmittedSourceCount = $changedSourceCount
        CompileProofs = $compileProofs
        TargetUpToDateObserved = $targetUpToDateObserved
        NativeSourcesAfterBuild = $nativeSourcesAfterBuild
        Log = Get-PathState -Path $buildLog
        ProductsAfter = $buildProductsAfter
    }

    [void] (Assert-SameState -Expected $editorCommandPin `
        -Path $editorCommand -Label 'UE 5.5 editor command before automation')
    $automationResult = Invoke-ExactAutomationTest
    [void] (Assert-WorkspaceAndNativeInputs)
    $nativeSourcesFinal = @($sourceJournal | ForEach-Object {
            Assert-SameState -Expected $_.Target -Path $_.NativePath `
                -Label "final native RF source $($_.Role)"
        })
    Assert-NativeProjectIdle -Checkpoint 'RF transaction final postflight'
    $protectedUE54After = Assert-ProtectedUE54Unchanged `
        -Before $script:protectedUE54Before
    $finalBuildProducts = @($buildJournal | ForEach-Object {
            Get-PathState -Path $_.NativePath
        })
    $receipt = [pscustomobject] [ordered] @{
        Schema =
            'triad.rf.one_kilometre_v2.actual_file_native_transaction.receipt.v3'
        Status = 'PASS'
        RunToken = $RunToken
        Script = $script:selfPin
        Inputs = $inputPins
        SourceTargets = $workspaceSourcePins
        WorkspaceResources = $workspaceResourcePins
        ProtectedUE54Before = $script:protectedUE54Before
        ProtectedUE54After = $protectedUE54After
        NativeSourceBackups = $sourceJournal
        NativeSourcesAfterPromotion = $nativeSourcesAfterPromotion
        NativeSourcesFinal = $nativeSourcesFinal
        PromotedRelativePaths = @($sourcePins.RelativePath)
        BuildProductBackups = $buildJournal
        Build = $buildResult
        Automation = $automationResult
        FinalBuildProducts = $finalBuildProducts
        BuildProductCount = $buildJournal.Count
        SuccessBuildProductPolicy =
            'LEAVE_COMPILED_TARGET_PRODUCTS_IN_PLACE'
        ExactAutomationSuccessCount = 1
        ZeroSuccessRejected = $true
        NativeProjectIdleAfter = $true
        RemoteControlListenerCountAfter = 0
        FailureRollbackArmed = $true
    }
    Write-JsonAtomic -Path $receiptPath -Value $receipt
}
catch {
    $primaryError = $_.Exception.Message
    $rollback = $null
    $rollbackError = $null
    try {
        if ($sourceJournal.Count -ne 0 -and $buildJournal.Count -ne 0) {
            $rollback = Restore-NativeTransaction `
                -SourceJournal $sourceJournal `
                -BuildJournal $buildJournal
        }
    }
    catch {
        $rollbackError = $_.Exception.Message
    }
    try {
        $failure = [pscustomobject] [ordered] @{
            Schema =
                'triad.rf.one_kilometre_v2.actual_file_native_transaction.failure.v3'
            Status = 'FAIL_CLOSED'
            RunToken = $RunToken
            Error = $primaryError
            Rollback = $rollback
            RollbackError = $rollbackError
            ProtectedUE54Before = $script:protectedUE54Before
            ExpectedBuildProductCount = $expectedBuildProductCount
            JournalledBuildProductCount = $buildJournal.Count
            NativeSourcesCurrent = @($sourceJournal | ForEach-Object {
                    Get-PathState -Path $_.NativePath
                })
            BuildProductsCurrent = @($buildJournal | ForEach-Object {
                    Get-PathState -Path $_.NativePath
                })
        }
        Write-JsonAtomic -Path $failurePath -Value $failure
    }
    catch {
        $failureWriteError = $_.Exception.Message
        throw "RF actual-file transaction failed; original={$primaryError} rollbackError={$rollbackError} failureReceiptError={$failureWriteError} transaction=$transactionRoot"
    }
    if ($null -ne $rollbackError) {
        throw "RF actual-file transaction failed and rollback failed; original={$primaryError} rollback={$rollbackError} transaction=$transactionRoot"
    }
    throw "RF actual-file transaction failed and rolled back; original={$primaryError} transaction=$transactionRoot"
}

$receiptPin = Get-PathState -Path $receiptPath
[pscustomobject] [ordered] @{
    Status = 'PASS'
    Receipt = $receiptPin
    NativeSources = @($sourceJournal | ForEach-Object {
            Get-PathState -Path $_.NativePath
        })
    RuntimeDll = Get-PathState -Path `
        (Join-Path $nativeProjectRoot $buildProductRelativePaths[0])
    AutomationFilter = $automationFilter
    SuccessfulCompletions = 1
    BuildProductCount = $buildJournal.Count
    TransactionRoot = $transactionRoot
} | ConvertTo-Json -Depth 8
