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
    throw "Exact v2 repair target is missing: $v2MapPath"
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
    functionName = 'RepairIstanaRuntimeMapV2GameMode'
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
    $sourceHashOnFailure = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceMapPath).Hash
    $v1HashOnFailure = if (Test-Path -LiteralPath $v1MapPath -PathType Leaf) {
        (Get-FileHash -Algorithm SHA256 -LiteralPath $v1MapPath).Hash
    }
    else {
        $null
    }
    if ($sourceHashOnFailure -ne $sourceHashBefore -or
        $v1HashOnFailure -ne $v1HashBefore) {
        throw "CRITICAL: SDTH or v1 changed while the repair call failed. Stop integration. SDTH before=$sourceHashBefore after=$sourceHashOnFailure; v1 before=$v1HashBefore after=$v1HashOnFailure"
    }
    throw "Unreal Remote Control repair call failed or timed out; SDTH/v1 hashes remain unchanged. $($_.Exception.Message)"
}

$sourceHashAfter = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceMapPath).Hash
$v1HashAfter = if (Test-Path -LiteralPath $v1MapPath -PathType Leaf) {
    (Get-FileHash -Algorithm SHA256 -LiteralPath $v1MapPath).Hash
}
else {
    $null
}
if ($sourceHashAfter -ne $sourceHashBefore) {
    throw "CRITICAL: SDTH changed during the v2 GameMode repair. Before=$sourceHashBefore After=$sourceHashAfter"
}
if ($v1HashAfter -ne $v1HashBefore) {
    throw "CRITICAL: the v1 Istana map changed during the v2 GameMode repair. Before=$v1HashBefore After=$v1HashAfter"
}
if (-not (Test-Path -LiteralPath $v2MapPath -PathType Leaf)) {
    throw "CRITICAL: the exact v2 map disappeared during repair: $v2MapPath"
}
$v2HashAfter = (Get-FileHash -Algorithm SHA256 -LiteralPath $v2MapPath).Hash

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
        throw "Repair returned a backup outside the allowlisted project Saved directory: $backupFull"
    }
    if (-not (Test-Path -LiteralPath $backupFull -PathType Leaf)) {
        throw "Repair reported a backup that does not exist: $backupFull"
    }
    $backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupFull).Hash
    if ($backupHash -ne $v2HashBefore) {
        throw "CRITICAL: pre-repair backup hash does not match the original v2 map. Original=$v2HashBefore Backup=$backupHash"
    }
    $backupFile = $backupFull
}

$message = [string] $response.OutMessage
if ($response.ReturnValue -ne $true) {
    throw "RepairIstanaRuntimeMapV2GameMode failed safely: $message Backup=$backupFile"
}

[PSCustomObject]@{
    Operation = 'RepairIstanaRuntimeMapV2GameMode'
    Succeeded = $true
    Message = $message
    DestinationMap = '/Game/Maps/Istana_1km_Context_v2'
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
