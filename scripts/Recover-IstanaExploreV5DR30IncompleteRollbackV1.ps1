#requires -Version 7.0

<#
.SYNOPSIS
Restores an R30 transaction whose exact helper outlived wrapper teardown.

.DESCRIPTION
The default invocation is read-only.  -Execute is accepted for an exact
ROLLBACK_INCOMPLETE R30 run, or with -AllowInterruptedWithoutReceipt for an
operator-interrupted run that has no rollback/commit receipt, with all retained
journals present and no UE5.5 or TRIAD-project build mutator. Recovery is limited to the map, the plugin
Binaries/Intermediate trees, the TreeRealism content tree, the eleven promoted
source destinations, seven initially absent source-evidence destinations, and
the initially absent R30 façade namespace.  The transaction directory and its
journals are retained as evidence.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,
    [switch] $Execute,
    [switch] $AllowInterruptedWithoutReceipt
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$nativeRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1'))
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase $RunToken))
$journalRoot = [IO.Path]::GetFullPath((Join-Path $transactionRoot 'rollback'))
$engineRoot = [IO.Path]::GetFullPath('C:\Program Files\Epic Games\UE_5.5')
$projectFile = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'TRIAD.uproject'))

function Test-ContainedPath {
    param([string] $Candidate, [string] $Root)
    $fullCandidate = [IO.Path]::GetFullPath($Candidate)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    return $fullCandidate.Equals($fullRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $fullCandidate.StartsWith($fullRoot + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoNativeMutator {
    $busy = @(
        Get-CimInstance Win32_Process | Where-Object {
            ($_.ExecutablePath -and
             [IO.Path]::GetFullPath($_.ExecutablePath).StartsWith(
                 $engineRoot + '\', [StringComparison]::OrdinalIgnoreCase) -and
             $_.Name -like 'UnrealEditor*') -or
            ($_.CommandLine -and
             ($_.CommandLine.Contains($projectFile, [StringComparison]::OrdinalIgnoreCase) -or
              $_.CommandLine.Contains('UnrealBuildTool', [StringComparison]::OrdinalIgnoreCase)))
        }
    )
    if ($busy.Count -ne 0) {
        $summary = ($busy | ForEach-Object { "pid=$($_.ProcessId) name=$($_.Name)" }) -join '; '
        throw "R30 recovery refused while a native mutator is present: $summary"
    }
}

function Get-FileReceipt {
    param([string] $Path)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject] [ordered] @{ Present = $false; Bytes = 0L; Sha256 = 'ABSENT' }
    }
    $item = Get-Item -LiteralPath $Path -Force
    return [pscustomobject] [ordered] @{
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

function Restore-ExactTree {
    param([string] $BackupRoot, [string] $TargetRoot)
    $backup = [IO.Path]::GetFullPath($BackupRoot)
    $target = [IO.Path]::GetFullPath($TargetRoot)
    if (-not (Test-ContainedPath $backup $journalRoot) -or
        -not (Test-ContainedPath $target $nativeRoot) -or
        -not [IO.Directory]::Exists($backup)) {
        throw "Exact tree recovery boundary is invalid: backup=$backup target=$target"
    }

    $before = [Collections.Generic.Dictionary[string,string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($file in @(Get-ChildItem -LiteralPath $backup -File -Recurse)) {
        $relative = [IO.Path]::GetRelativePath($backup, $file.FullName)
        if (-not $before.TryAdd($relative, $file.FullName)) {
            throw "Duplicate recovery-journal path: $relative"
        }
    }
    if ($before.Count -eq 0) {
        throw "Exact tree journal is unexpectedly empty: $backup"
    }

    [void] [IO.Directory]::CreateDirectory($target)
    foreach ($file in @(Get-ChildItem -LiteralPath $target -File -Recurse)) {
        $relative = [IO.Path]::GetRelativePath($target, $file.FullName)
        if (-not $before.ContainsKey($relative)) {
            [IO.File]::Delete($file.FullName)
        }
    }
    foreach ($entry in $before.GetEnumerator()) {
        $destination = [IO.Path]::GetFullPath((Join-Path $target $entry.Key))
        if (-not (Test-ContainedPath $destination $target)) {
            throw "Recovered file escaped exact target tree: $destination"
        }
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        [IO.File]::Copy($entry.Value, $destination, $true)
        $expected = Get-FileReceipt $entry.Value
        $actual = Get-FileReceipt $destination
        if ($expected.Bytes -ne $actual.Bytes -or $expected.Sha256 -cne $actual.Sha256) {
            throw "Recovered file failed exact receipt validation: $destination"
        }
    }

    foreach ($directory in @(
        Get-ChildItem -LiteralPath $target -Directory -Recurse |
            Sort-Object { $_.FullName.Length } -Descending
    )) {
        if (@(Get-ChildItem -LiteralPath $directory.FullName -Force).Count -eq 0) {
            [IO.Directory]::Delete($directory.FullName, $false)
        }
    }
    return [pscustomobject] [ordered] @{
        TargetRoot = $target
        RestoredFileCount = $before.Count
    }
}

function Remove-ExactInitiallyAbsentTree {
    param([string] $TargetRoot)
    $target = [IO.Path]::GetFullPath($TargetRoot)
    $contentRoot = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'Content'))
    if (-not (Test-ContainedPath $target $contentRoot)) {
        throw "Initially absent R30 tree escaped native Content: $target"
    }
    if (-not [IO.Directory]::Exists($target)) { return }
    foreach ($file in @(Get-ChildItem -LiteralPath $target -File -Recurse)) {
        [IO.File]::Delete($file.FullName)
    }
    foreach ($directory in @(
        @(Get-ChildItem -LiteralPath $target -Directory -Recurse) +
        @(Get-Item -LiteralPath $target) |
            Sort-Object { $_.FullName.Length } -Descending
    )) {
        if (@(Get-ChildItem -LiteralPath $directory.FullName -Force).Count -eq 0) {
            [IO.Directory]::Delete($directory.FullName, $false)
        }
    }
}

if (-not (Test-ContainedPath $transactionRoot $transactionBase) -or
    -not [IO.Directory]::Exists($transactionRoot) -or
    -not [IO.Directory]::Exists($journalRoot)) {
    throw "Exact R30 transaction/journal is absent: $transactionRoot"
}
$rollbackPath = Join-Path $transactionRoot 'rollback.json'
$commitPath = Join-Path $transactionRoot 'commit.json'
$recoveryPath = Join-Path $transactionRoot 'recovery.json'
$rollback = $null
$admissionMode = 'ROLLBACK_INCOMPLETE_RECEIPT'
if ([IO.File]::Exists($rollbackPath)) {
    $rollback = Get-Content -LiteralPath $rollbackPath -Raw | ConvertFrom-Json
    if ($rollback.Schema -cne 'triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1' -or
        $rollback.Status -cne 'ROLLBACK_INCOMPLETE') {
        throw "Recovery requires an exact ROLLBACK_INCOMPLETE R30 receipt."
    }
}
elseif (-not $AllowInterruptedWithoutReceipt) {
    throw "R30 rollback receipt is absent; explicit -AllowInterruptedWithoutReceipt is required: $rollbackPath"
}
else {
    if ([IO.File]::Exists($commitPath)) {
        throw 'Interrupted-without-receipt recovery is forbidden when a commit receipt exists.'
    }
    if (-not [IO.File]::Exists((Join-Path $transactionRoot 'build.stdout.log')) -or
        -not [IO.File]::Exists((Join-Path $transactionRoot 'build.stderr.log'))) {
        throw 'Interrupted-without-receipt recovery requires both exact build logs.'
    }
    $admissionMode = 'EXPLICIT_INTERRUPTED_WITHOUT_RECEIPT'
}
Assert-NoNativeMutator

$mapBackup = [IO.Path]::GetFullPath((Join-Path $journalRoot 'map\Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$mapTarget = [IO.Path]::GetFullPath((Join-Path $nativeRoot 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$sourceBackupRoot = [IO.Path]::GetFullPath((Join-Path $journalRoot 'source'))
$absentSourcePaths = @(
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR30FacadeLookdevActor.h',
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR30FacadeLookdevActor.cpp',
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DR30FacadeLookdevActorTests.cpp',
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.h',
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.cpp',
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.h',
    'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.cpp'
)
$absentSourceAssetPaths = @(
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R30FacadeLookdev\r30_facade_lookdev.contract.json',
    'SourceAssets\IstanaPublicViewExploreV5D\Surroundings\R30FacadeLookdev\r30_tree_material_response_v3.source_closure.json',
    'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\istana_public_view_v5d_tree_realism.material_response_amendment.v3.json',
    'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\render_tree_material_response_audit.py',
    'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\MaterialResponseAudit\README.md',
    'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\MaterialResponseAudit\tree_material_response_audit.json',
    'SourceAssets\IstanaPublicViewExploreV5D\TreeRealism\MaterialResponseAudit\tree_material_response_contact_sheet.png'
)

$readiness = [pscustomobject] [ordered] @{
    Schema = 'triad.istana_explore_v5d.r30_incomplete_rollback_recovery.v1'
    Status = if ($Execute) { 'EXECUTION_REQUESTED' } else { 'READ_ONLY_RECOVERY_READY' }
    RunToken = $RunToken
    AdmissionMode = $admissionMode
    RollbackReceipt = Get-FileReceipt $rollbackPath
    NativeMutatorCount = 0
    JournalRoot = $journalRoot
    NativeWritten = $false
}
if (-not $Execute) {
    $readiness | ConvertTo-Json -Depth 5
    exit 0
}

Assert-NoNativeMutator
[void] (Get-FileReceipt $mapBackup)
if ([IO.File]::Exists($recoveryPath)) {
    throw "Recovery receipt already exists and will not be overwritten: $recoveryPath"
}
[IO.File]::Copy($mapBackup, $mapTarget, $true)
$mapExpected = Get-FileReceipt $mapBackup
$mapActual = Get-FileReceipt $mapTarget
if ($mapExpected.Bytes -ne $mapActual.Bytes -or $mapExpected.Sha256 -cne $mapActual.Sha256) {
    throw 'Recovered R29 map failed exact receipt validation.'
}

$treeResult = Restore-ExactTree `
    (Join-Path $journalRoot 'tree-realism\Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism') `
    (Join-Path $nativeRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\TreeRealism')
$binaryResult = Restore-ExactTree `
    (Join-Path $journalRoot 'build\Plugins\TRIADSensorFusion\Binaries') `
    (Join-Path $nativeRoot 'Plugins\TRIADSensorFusion\Binaries')
$intermediateResult = Restore-ExactTree `
    (Join-Path $journalRoot 'build\Plugins\TRIADSensorFusion\Intermediate') `
    (Join-Path $nativeRoot 'Plugins\TRIADSensorFusion\Intermediate')

foreach ($backup in @(Get-ChildItem -LiteralPath $sourceBackupRoot -File -Recurse)) {
    $relative = [IO.Path]::GetRelativePath($sourceBackupRoot, $backup.FullName)
    $destination = [IO.Path]::GetFullPath((Join-Path $nativeRoot $relative))
    if (-not (Test-ContainedPath $destination $nativeRoot)) {
        throw "Recovered source escaped native project: $destination"
    }
    [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
    [IO.File]::Copy($backup.FullName, $destination, $true)
    $expected = Get-FileReceipt $backup.FullName
    $actual = Get-FileReceipt $destination
    if ($expected.Bytes -ne $actual.Bytes -or $expected.Sha256 -cne $actual.Sha256) {
        throw "Recovered source failed exact receipt validation: $destination"
    }
}
foreach ($relative in @($absentSourcePaths + $absentSourceAssetPaths)) {
    $destination = [IO.Path]::GetFullPath((Join-Path $nativeRoot $relative))
    if (-not (Test-ContainedPath $destination $nativeRoot)) {
        throw "Initially absent path escaped native project: $destination"
    }
    if ([IO.File]::Exists($destination)) { [IO.File]::Delete($destination) }
}
Remove-ExactInitiallyAbsentTree (
    Join-Path $nativeRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30')

Assert-NoNativeMutator
foreach ($relative in @($absentSourcePaths + $absentSourceAssetPaths)) {
    if ([IO.File]::Exists((Join-Path $nativeRoot $relative))) {
        throw "Initially absent file remains after recovery: $relative"
    }
}

$recovery = [pscustomobject] [ordered] @{
    Schema = 'triad.istana_explore_v5d.r30_incomplete_rollback_recovery.v1'
    Status = 'RECOVERED_TO_EXACT_PRETRANSACTION_STATE'
    RunToken = $RunToken
    AdmissionMode = $admissionMode
    Map = Get-FileReceipt $mapTarget
    RuntimeDll = Get-FileReceipt (Join-Path $nativeRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll')
    EditorDll = Get-FileReceipt (Join-Path $nativeRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll')
    TreeRealism = $treeResult
    PluginBinaries = $binaryResult
    PluginIntermediate = $intermediateResult
    RestoredPreexistingSourceCount = @(Get-ChildItem -LiteralPath $sourceBackupRoot -File -Recurse).Count
    RemovedInitiallyAbsentSourceCount = $absentSourcePaths.Count
    RemovedInitiallyAbsentEvidenceCount = $absentSourceAssetPaths.Count
    R30FacadeNamespacePresent = [IO.Directory]::Exists(
        (Join-Path $nativeRoot 'Content\TRIAD\IstanaPublicViewExploreV5D\SurroundingsLookdevR30'))
    NativeMutatorCount = 0
    JournalsRetained = $true
}
$recoveryJson = $recovery | ConvertTo-Json -Depth 6
[IO.File]::WriteAllText(
    $recoveryPath,
    $recoveryJson + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))
$recoveryJson
