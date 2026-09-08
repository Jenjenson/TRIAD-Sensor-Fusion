[CmdletBinding()]
param(
    [string] $ProjectPath = 'D:\triad\TRIAD',
    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolvedProject -PathType Leaf) {
    Split-Path -Parent $resolvedProject
}
else {
    $resolvedProject
}
$sourceMapPath = Join-Path $projectDirectory 'Content\SDTH.umap'
$v1MapPath = Join-Path $projectDirectory 'Content\Maps\Istana_1km.umap'
$v2MapPath = Join-Path $projectDirectory 'Content\Maps\Istana_1km_Context_v2.umap'
if (-not (Test-Path -LiteralPath $sourceMapPath -PathType Leaf)) {
    throw "Source-map safety hash target is missing: $sourceMapPath"
}
if (-not (Test-Path -LiteralPath $v2MapPath -PathType Leaf)) {
    throw "Exact v2 camera-migration target is missing: $v2MapPath"
}

$sourceHashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceMapPath).Hash
$v1HashBefore = if (Test-Path -LiteralPath $v1MapPath -PathType Leaf) {
    (Get-FileHash -Algorithm SHA256 -LiteralPath $v1MapPath).Hash
}
else {
    $null
}
$v2HashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $v2MapPath).Hash
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'

$identityBody = @{
    objectPath = $objectPath
    functionName = 'ValidateIstanaRemoteControlProject'
    parameters = @{ ExpectedProjectPath = $projectDirectory }
} | ConvertTo-Json -Depth 5
$identity = Invoke-RestMethod `
    -Uri $endpoint `
    -Method Put `
    -ContentType 'application/json' `
    -Body $identityBody `
    -TimeoutSec 30
if ($identity.ReturnValue -ne $true) {
    throw "Remote Control project verification failed: $($identity.OutReport)"
}

$repairBody = @{
    objectPath = $objectPath
    functionName = 'RepairIstanaRuntimeMapV2Camera'
    parameters = @{}
} | ConvertTo-Json -Depth 5
try {
    $response = Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $repairBody `
        -TimeoutSec 600
}
catch {
    $sourceHashOnFailure = if (Test-Path -LiteralPath $sourceMapPath -PathType Leaf) {
        (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceMapPath).Hash
    }
    else {
        $null
    }
    $v1HashOnFailure = if (Test-Path -LiteralPath $v1MapPath -PathType Leaf) {
        (Get-FileHash -Algorithm SHA256 -LiteralPath $v1MapPath).Hash
    }
    else {
        $null
    }
    $v2HashOnFailure = if (Test-Path -LiteralPath $v2MapPath -PathType Leaf) {
        (Get-FileHash -Algorithm SHA256 -LiteralPath $v2MapPath).Hash
    }
    else {
        $null
    }
    if ($sourceHashOnFailure -ne $sourceHashBefore -or
        $v1HashOnFailure -ne $v1HashBefore) {
        throw "CRITICAL: SDTH or v1 changed while the camera-migration call failed. SDTH before=$sourceHashBefore after=$sourceHashOnFailure; v1 before=$v1HashBefore after=$v1HashOnFailure"
    }
    if ($v2HashOnFailure -ne $v2HashBefore) {
        throw "CRITICAL: the Remote Control call failed or timed out and exact v2 disk state changed. Treat the result as unknown and inspect the map/backup before retrying. V2 before=$v2HashBefore after=$v2HashOnFailure"
    }
    throw "Unreal Remote Control camera-migration call failed or timed out; SDTH/v1/v2 hashes remain unchanged. $($_.Exception.Message)"
}

$sourceHashAfter = if (Test-Path -LiteralPath $sourceMapPath -PathType Leaf) {
    (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceMapPath).Hash
}
else {
    $null
}
$v1HashAfter = if (Test-Path -LiteralPath $v1MapPath -PathType Leaf) {
    (Get-FileHash -Algorithm SHA256 -LiteralPath $v1MapPath).Hash
}
else {
    $null
}
$v2HashAfter = if (Test-Path -LiteralPath $v2MapPath -PathType Leaf) {
    (Get-FileHash -Algorithm SHA256 -LiteralPath $v2MapPath).Hash
}
else {
    $null
}
if ($sourceHashAfter -ne $sourceHashBefore) {
    throw "CRITICAL: SDTH changed during the v2 camera migration. Before=$sourceHashBefore After=$sourceHashAfter"
}
if ($v1HashAfter -ne $v1HashBefore) {
    throw "CRITICAL: the v1 Istana map changed during the v2 camera migration. Before=$v1HashBefore After=$v1HashAfter"
}
if ($null -eq $v2HashAfter) {
    throw "CRITICAL: the exact v2 map disappeared during camera migration: $v2MapPath"
}

$backupFile = [string] $response.OutBackupFile
$backupHash = $null
if (-not [string]::IsNullOrWhiteSpace($backupFile)) {
    $backupRoot = [IO.Path]::GetFullPath(
        (Join-Path $projectDirectory 'Saved\TRIAD\IstanaMapBackups'))
    $backupFull = [IO.Path]::GetFullPath($backupFile)
    $requiredPrefix = $backupRoot.TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $backupFull.StartsWith(
            $requiredPrefix,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Camera migration returned a backup outside the allowlisted project Saved directory: $backupFull"
    }
    if (-not (Test-Path -LiteralPath $backupFull -PathType Leaf)) {
        throw "Camera migration reported a backup that does not exist: $backupFull"
    }
    $backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupFull).Hash
    if ($backupHash -ne $v2HashBefore) {
        throw "CRITICAL: pre-migration backup hash does not match the original v2 map. Original=$v2HashBefore Backup=$backupHash"
    }
    $backupFile = $backupFull
}

$message = [string] $response.OutMessage
if ($response.ReturnValue -ne $true) {
    if ($v2HashAfter -ne $v2HashBefore) {
        throw "CRITICAL: RepairIstanaRuntimeMapV2Camera returned failure after changing exact v2 disk state. Inspect the reported backup before any retry. Before=$v2HashBefore After=$v2HashAfter Message=$message Backup=$backupFile"
    }
    throw "RepairIstanaRuntimeMapV2Camera failed safely with all map hashes unchanged: $message Backup=$backupFile"
}
if ($v2HashAfter -ne $v2HashBefore -and
    [string]::IsNullOrWhiteSpace($backupFile)) {
    throw "CRITICAL: exact v2 changed without a reported, verified pre-migration backup. Before=$v2HashBefore After=$v2HashAfter"
}
if ($v2HashAfter -eq $v2HashBefore -and
    -not [string]::IsNullOrWhiteSpace($backupFile)) {
    throw "CRITICAL: camera migration created a backup but exact v2 did not change. Refusing to classify a possibly non-persisted edit as idempotent. Hash=$v2HashAfter Backup=$backupFile"
}

[PSCustomObject]@{
    Operation = 'RepairIstanaRuntimeMapV2Camera'
    Succeeded = $true
    Message = $message
    DestinationMap = '/Game/Maps/Istana_1km_Context_v2'
    IdempotentNoChange = (
        $v2HashAfter -eq $v2HashBefore -and
        [string]::IsNullOrWhiteSpace($backupFile))
    BackupFile = $backupFile
    BackupSha256 = $backupHash
    V2Sha256Before = $v2HashBefore
    V2Sha256After = $v2HashAfter
    SourceMapMutated = $false
    SourceMapSha256Before = $sourceHashBefore
    SourceMapSha256After = $sourceHashAfter
    V1MapMutated = $false
    V1MapSha256Before = $v1HashBefore
    V1MapSha256After = $v1HashAfter
    ReloadPersistenceValidated = $true
}
