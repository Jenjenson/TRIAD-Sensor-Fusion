<#
.SYNOPSIS
Certify the already-completed fail-closed recovery of R30 capture run 04.

.DESCRIPTION
The original recovery moved every audited addition to quarantine and restored
every audited baseline file, but its final receipt was refused when Restart
Manager transiently reported the hashing process as a PCH holder. This script
does not repeat or compensate those mutations. It validates the preserved
failure evidence, proves that the complete six-root native closure equals the
immutable rollback backup (including timestamps and directories), verifies the
protected R30 state, and publishes a separate certification receipt.

The PCH and native-mutator checks intentionally run before full-closure hashing
in this fresh process. Full hashing is read-only and can itself create a
transient mapped-file observation on this host.
#>
[CmdletBinding()]
param(
    [switch] $StaticSelfCheck,
    [switch] $Execute,
    [ValidatePattern('^r30-capture-20260908-04-recovery-[0-9]{2}-certification$')]
    [string] $CertificationToken =
        'r30-capture-20260908-04-recovery-02-certification'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($StaticSelfCheck -and $Execute) {
    throw '-StaticSelfCheck and -Execute are mutually exclusive.'
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repairScript = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'Repair-IstanaExploreV5DR30CaptureRun04Rollback.ps1'))
$repairScriptPin = [pscustomobject] [ordered] @{
    Bytes = 73479L
    Sha256 = '9AD8F4D5D84DC9D5EDA077545E85F17B0593ACBC3594C9714345C34DBC3BA9DA'
}
$failedRoot = [IO.Path]::GetFullPath(
    'D:\triad\TRIAD_R30Evidence\r30-capture-20260908-04\recovery\r30-capture-20260908-04-recovery-01')
$failedReceipt = [IO.Path]::GetFullPath((Join-Path $failedRoot 'failed.json'))
$failedReceiptPin = [pscustomobject] [ordered] @{
    Bytes = 34145L
    Sha256 = 'B1FEE7D1BC0BD3F8A4CF1FAB92CCCFAA4119399E94D5117C27617E44AEA827EA'
}
$certificationToken = $CertificationToken
$certificationRoot = [IO.Path]::GetFullPath(
    (Join-Path 'D:\triad\TRIAD_R30Evidence\r30-capture-20260908-04\recovery' $certificationToken))
$certificationReceipt = [IO.Path]::GetFullPath((Join-Path $certificationRoot 'receipt.json'))

function Get-LocalFileIdentity {
    param([Parameter(Mandatory = $true)] [string] $Path)
    if (-not [IO.File]::Exists($Path)) {
        return [pscustomobject] [ordered] @{
            Present=$false; Path=$Path; Bytes=0L; Sha256='ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $Path -Force
    [pscustomobject] [ordered] @{
        Present=$true
        Path=[IO.Path]::GetFullPath($item.FullName)
        Bytes=[int64] $item.Length
        Sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

function Assert-LocalPin {
    param([string] $Path, $Expected, [string] $Label)
    $actual = Get-LocalFileIdentity $Path
    if (-not $actual.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label identity mismatch: $($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Assert-RowArtifact {
    param($Row, [string] $ExpectedPath, [string] $Label)
    Assert-NoReparsePathAncestors $ExpectedPath
    $actual = Get-LocalFileIdentity $ExpectedPath
    if (-not $actual.Present -or
        [int64] $actual.Bytes -ne [int64] $Row.Bytes -or
        [string] $actual.Sha256 -cne [string] $Row.Sha256) {
        throw "$Label artifact mismatch: $ExpectedPath"
    }
    $actual
}

function Assert-NoLocalDescendantReparsePoints {
    param([string] $Root, [string] $Label)
    if (-not [IO.Directory]::Exists($Root)) {
        throw "$Label root is absent: $Root"
    }
    $reparse = @(Get-ChildItem -LiteralPath $Root -Force -Recurse |
        Where-Object {
            ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
        })
    if ($reparse.Count -ne 0) {
        throw "$Label contains descendant reparse points: $(@($reparse.FullName) -join ', ')"
    }
}

function Get-LocalRelativeFileSet {
    param([string] $Root, [string] $Label)
    Assert-NoLocalDescendantReparsePoints $Root $Label
    @(
        Get-ChildItem -LiteralPath $Root -File -Force -Recurse |
            ForEach-Object { [IO.Path]::GetRelativePath($Root, $_.FullName) }
    )
}

if ($StaticSelfCheck) {
    [pscustomobject] [ordered] @{
        Schema='triad.istana_explore_v5d.r30_capture_rollback_recovery_certification.v1'
        Status='STATIC_SELF_CHECK_PASS'
        SourceRunToken='r30-capture-20260908-04'
        FailedRecoveryToken='r30-capture-20260908-04-recovery-01'
        CertificationToken=$certificationToken
        RepeatsNativeMutation=$false
        CompensationAttempted=$false
        FullClosureVerificationRequired=$true
        TimestampAndDirectoryEqualityRequired=$true
        ProtectedR30StateRequired=$true
        PchCheckRunsBeforeFullClosureHashing=$true
        NativeProjectAccessed=$false
        NativeTreeWritten=$false
        UnrealLaunched=$false
    } | ConvertTo-Json -Depth 5
    return
}

if (-not $Execute) {
    [pscustomobject] [ordered] @{
        Schema='triad.istana_explore_v5d.r30_capture_rollback_recovery_certification.v1'
        Status='INERT'
        ExecuteRequired=$true
        NativeProjectAccessed=$false
        NativeTreeWritten=$false
        UnrealLaunched=$false
    } | ConvertTo-Json -Depth 4
    return
}

$startedUtc = [DateTime]::UtcNow
$repairIdentity = Assert-LocalPin $repairScript $repairScriptPin 'pinned recovery implementation'

# Import only the reviewed recovery constants and read-only verification helpers.
. $repairScript | Out-Null

if (-not (Test-ContainedPath $certificationRoot $recoveryBase) -or
    $certificationRoot.Equals($recoveryBase, [StringComparison]::OrdinalIgnoreCase) -or
    [IO.Directory]::Exists($certificationRoot) -or
    [IO.File]::Exists($certificationRoot)) {
    throw "Certification output must be a new strict evidence child: $certificationRoot"
}
foreach ($path in @($failedRoot, $certificationRoot, $backupRoot, $nativeRoot)) {
    Assert-NoReparsePathAncestors $path
}
Assert-NoBuildTreeReparsePoints
Assert-NoLocalDescendantReparsePoints $failedRoot 'failed recovery evidence'

$failedIdentity = Assert-LocalPin $failedReceipt $failedReceiptPin 'failed recovery receipt'
if ([IO.File]::Exists((Join-Path $failedRoot 'receipt.json'))) {
    throw 'The failed recovery root unexpectedly contains a success receipt.'
}
$failed = Get-Content -LiteralPath $failedReceipt -Raw | ConvertFrom-Json
if ([string] $failed.Schema -cne 'triad.istana_explore_v5d.r30_capture_rollback_recovery.v1' -or
    [string] $failed.Status -cne 'FAILED' -or
    [string] $failed.SourceRunToken -cne 'r30-capture-20260908-04' -or
    [string] $failed.RecoveryToken -cne 'r30-capture-20260908-04-recovery-01' -or
    $null -ne $failed.Compensation -or
    $null -ne $failed.PublishedReceiptError -or
    [bool] $failed.OriginalEvidenceAndRollbackBackupMustBePreserved -ne $true -or
    [string] $failed.Error -notmatch '^Recovery refused because Restart Manager reports PCH holders: [0-9]+:Git for Windows$' -or
    [string] $failed.CompensationError -notmatch '^Recovery refused because Restart Manager reports PCH holders: [0-9]+:Git for Windows$') {
    throw 'Failed recovery receipt semantics drifted.'
}

$preRows = @($failed.PreRecoveryCurrentCopies)
$quarantineRows = @($failed.QuarantinedAddedFiles)
$restoredRows = @($failed.RestoredBaselineFiles)
if ($preRows.Count -ne 14 -or $quarantineRows.Count -ne 24 -or
    $restoredRows.Count -ne 32) {
    throw "Failed recovery operation counts drifted: pre=$($preRows.Count) quarantine=$($quarantineRows.Count) restored=$($restoredRows.Count)"
}
Assert-ExactPathSet @($preRows | ForEach-Object RelativePath) @($contentDifferent) 'preserved pre-recovery file'
Assert-ExactPathSet @($quarantineRows | ForEach-Object RelativePath) @($addedFiles) 'quarantined generated file'
Assert-ExactPathSet @($restoredRows | ForEach-Object RelativePath) @(@($contentDifferent) + @($timestampOnly)) 'restored baseline file'
$preRecoveryArtifactRoot = [IO.Path]::GetFullPath(
    (Join-Path $failedRoot 'pre-recovery-current'))
$quarantineArtifactRoot = [IO.Path]::GetFullPath(
    (Join-Path $failedRoot 'quarantine'))
$actualPreFileSet = @(Get-LocalRelativeFileSet `
    $preRecoveryArtifactRoot 'preserved pre-recovery evidence')
$actualQuarantineFileSet = @(Get-LocalRelativeFileSet `
    $quarantineArtifactRoot 'quarantined generated evidence')
Assert-ExactPathSet $actualPreFileSet @($contentDifferent) 'preserved pre-recovery subtree file'
Assert-ExactPathSet $actualQuarantineFileSet @($addedFiles) 'quarantine subtree file'

$preArtifacts = [Collections.Generic.List[object]]::new()
foreach ($row in $preRows) {
    $relative = [string] $row.RelativePath
    if (-not $contentDifferentCurrentPins.ContainsKey($relative)) {
        throw "Unknown preserved pre-recovery path: $relative"
    }
    $pin = $contentDifferentCurrentPins[$relative]
    if ([int64] $row.Bytes -ne [int64] $pin.Bytes -or
        [string] $row.Sha256 -cne [string] $pin.Sha256 -or
        [int64] $row.LastWriteUtcTicks -ne [int64] $pin.LastWriteUtcTicks) {
        throw "Preserved pre-recovery row drifted: $relative"
    }
    $expectedPath = [IO.Path]::GetFullPath((Join-Path $preRecoveryArtifactRoot $relative))
    if ([string] $row.PreservedAt -cne $expectedPath) {
        throw "Preserved pre-recovery destination drifted: $relative"
    }
    $preArtifacts.Add((Assert-RowArtifact $row $expectedPath 'preserved pre-recovery'))
}

$quarantineArtifacts = [Collections.Generic.List[object]]::new()
foreach ($row in $quarantineRows) {
    $relative = [string] $row.RelativePath
    if (-not $addedFilePins.ContainsKey($relative) -or
        [string] $row.Sha256 -cne [string] $addedFilePins[$relative]) {
        throw "Quarantine row drifted: $relative"
    }
    $expectedPath = [IO.Path]::GetFullPath((Join-Path $quarantineArtifactRoot $relative))
    if ([string] $row.QuarantinedAt -cne $expectedPath) {
        throw "Quarantine destination drifted: $relative"
    }
    $quarantineArtifacts.Add((Assert-RowArtifact $row $expectedPath 'quarantined generated'))
}

$evidenceBefore = @($evidencePins | ForEach-Object {
    Assert-FilePin $_ ([string] $_.Label)
})
$admission = Get-Content -LiteralPath $admissionReceipt -Raw | ConvertFrom-Json

# These checks run in a fresh process before the read-only hash traversal.
$nativeIdle = Assert-NativeMutatorsQuiescent
$pch = Assert-PchUnheld
$protected = Assert-ProtectedNativeState $admission

$null = Assert-NoBuildTreeReparsePoints
$baseline = Get-BuildSnapshot 'Backup'
$backupClosure = Assert-PinnedBackupSnapshot $baseline
$current = Get-BuildSnapshot 'Native'
$delta = Compare-BuildSnapshots $baseline $current
Assert-ExactPathSet @($delta.Missing) @() 'certification missing file'
Assert-ExactPathSet @($delta.Added) @() 'certification added file'
Assert-ExactPathSet @($delta.ContentDifferent) @() 'certification content difference'
Assert-ExactPathSet @($delta.TimestampOnly) @() 'certification timestamp difference'
Assert-ExactPathSet @($delta.MissingDirectories) @() 'certification missing directory'
Assert-ExactPathSet @($delta.AddedDirectories) @() 'certification added directory'
if ([int] $current.FileCount -ne $expectedBackupFileCount -or
    [int64] $current.TotalBytes -ne $expectedBackupBytes -or
    [string] $current.ManifestSha256 -cne $expectedBackupManifestSha256) {
    throw 'Native build closure does not equal the immutable recovery baseline.'
}

$baselineMap = New-RowMap @($baseline.Files)
foreach ($row in $restoredRows) {
    $relative = [string] $row.RelativePath
    if (-not $baselineMap.ContainsKey($relative)) {
        throw "Restored receipt row is absent from baseline: $relative"
    }
    $expected = $baselineMap[$relative]
    if ([int64] $row.Bytes -ne [int64] $expected.Bytes -or
        [string] $row.Sha256 -cne [string] $expected.Sha256 -or
        [int64] $row.LastWriteUtcTicks -ne [int64] $expected.LastWriteUtcTicks) {
        throw "Restored receipt row does not match baseline: $relative"
    }
}

$evidenceAfter = @($evidencePins | ForEach-Object {
    Assert-FilePin $_ ([string] $_.Label)
})
$failedAfter = Assert-LocalPin $failedReceipt $failedReceiptPin 'failed recovery receipt after verification'

$receiptTemporary = "$certificationReceipt.tmp.$PID"
if ([IO.Directory]::Exists($certificationRoot) -or
    [IO.File]::Exists($certificationRoot) -or
    [IO.File]::Exists($certificationReceipt) -or
    [IO.File]::Exists($receiptTemporary)) {
    throw "Certification output was adopted before publication: $certificationRoot"
}
[void] [IO.Directory]::CreateDirectory($certificationRoot)
Assert-NoReparsePathAncestors $certificationRoot
if (@(Get-ChildItem -LiteralPath $certificationRoot -Force).Count -ne 0 -or
    [IO.File]::Exists($certificationReceipt) -or
    [IO.File]::Exists($receiptTemporary)) {
    throw "New certification output is not empty before publication: $certificationRoot"
}
$receipt = [pscustomobject] [ordered] @{
    Schema='triad.istana_explore_v5d.r30_capture_rollback_recovery_certification.v1'
    Status='CERTIFIED_RECOVERED_AFTER_FAILED_RECEIPT'
    SourceRunToken='r30-capture-20260908-04'
    FailedRecoveryToken='r30-capture-20260908-04-recovery-01'
    CertificationToken=$certificationToken
    StartedUtc=$startedUtc.ToString('o')
    CompletedUtc=[DateTime]::UtcNow.ToString('o')
    RecoveryImplementation=$repairIdentity
    FailedRecoveryReceiptBefore=$failedIdentity
    FailedRecoveryReceiptAfter=$failedAfter
    FailureSemantics=[pscustomobject] [ordered] @{
        Status='FAILED'
        FinalReceiptAbsent=$true
        CompensationNotPerformed=$true
        TransientRestartManagerHolderRecorded=$true
        OriginalFailureEvidencePreserved=$true
    }
    PreservedOperationEvidence=[pscustomobject] [ordered] @{
        PreRecoveryCurrentFileCount=$preRows.Count
        QuarantinedGeneratedFileCount=$quarantineRows.Count
        RestoredBaselineFileCount=$restoredRows.Count
        PreRecoveryArtifactsExact=$preArtifacts.Count
        QuarantineArtifactsExact=$quarantineArtifacts.Count
    }
    NativeIdleBeforeReadOnlyHashing=$nativeIdle
    PchBeforeFullClosureHashing=$pch
    EvidencePinsBefore=$evidenceBefore
    EvidencePinsAfter=$evidenceAfter
    ProtectedR30State=$protected
    BackupClosure=$backupClosure
    NativeClosure=[pscustomobject] [ordered] @{
        FileCount=[int] $current.FileCount
        TotalBytes=[int64] $current.TotalBytes
        ManifestSha256=[string] $current.ManifestSha256
        DirectoryCount=@($current.Directories).Count
        PerRoot=@($current.PerRoot)
    }
    ExactDelta=[pscustomobject] [ordered] @{
        MissingCount=0
        AddedCount=0
        ContentDifferentCount=0
        TimestampOnlyCount=0
        MissingDirectoryCount=0
        AddedDirectoryCount=0
    }
    RepeatedRecoveryMutation=$false
    CompensationAttempted=$false
    NativeProjectWritten=$false
    UnrealLaunched=$false
    OriginalFailureEvidencePreserved=$true
}
$receiptState = $null
try {
    if (@(Get-ChildItem -LiteralPath $certificationRoot -Force).Count -ne 0 -or
        [IO.File]::Exists($certificationReceipt) -or
        [IO.File]::Exists($receiptTemporary)) {
        throw "Certification output changed immediately before publication: $certificationRoot"
    }
    $receiptState = Write-JsonNewAtomic `
        $receipt $certificationReceipt $certificationRoot
}
catch {
    # The writer removes its own temporary file. Delete only an empty directory
    # created by this invocation; otherwise preserve it and use a new reviewed
    # two-digit certification token for a later attempt.
    if ([IO.Directory]::Exists($certificationRoot) -and
        -not [IO.File]::Exists($certificationReceipt) -and
        @(Get-ChildItem -LiteralPath $certificationRoot -Force).Count -eq 0) {
        [IO.Directory]::Delete($certificationRoot, $false)
    }
    throw
}
[pscustomobject] [ordered] @{
    Receipt=$receipt
    ReceiptFile=$receiptState
} | ConvertTo-Json -Depth 18
