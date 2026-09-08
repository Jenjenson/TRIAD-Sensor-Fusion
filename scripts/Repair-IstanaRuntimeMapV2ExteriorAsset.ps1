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
$legacyAssetPath = Join-Path $projectDirectory 'Content\TRIAD\Istana\Meshes\SM_IstanaExterior.uasset'
$refinedAssetPath = Join-Path $projectDirectory 'Content\TRIAD\Istana\Meshes\SM_IstanaExterior_Refined.uasset'
foreach ($requiredPath in @($sourceMapPath, $v2MapPath, $refinedAssetPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required refined-exterior migration target is missing: $requiredPath"
    }
}

function Get-OptionalSha256([string] $LiteralPath) {
    if (Test-Path -LiteralPath $LiteralPath -PathType Leaf) {
        return (Get-FileHash -Algorithm SHA256 -LiteralPath $LiteralPath).Hash
    }
    return $null
}

$sourceHashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceMapPath).Hash
$v1HashBefore = Get-OptionalSha256 $v1MapPath
$v2HashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $v2MapPath).Hash
$legacyHashBefore = Get-OptionalSha256 $legacyAssetPath
$refinedHashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $refinedAssetPath).Hash
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
    functionName = 'RepairIstanaRuntimeMapV2ExteriorAsset'
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
    $callError = $_.Exception.Message
    $sourceHashOnFailure = Get-OptionalSha256 $sourceMapPath
    $v1HashOnFailure = Get-OptionalSha256 $v1MapPath
    $v2HashOnFailure = Get-OptionalSha256 $v2MapPath
    $legacyHashOnFailure = Get-OptionalSha256 $legacyAssetPath
    $refinedHashOnFailure = Get-OptionalSha256 $refinedAssetPath
    if ($sourceHashOnFailure -ne $sourceHashBefore -or
        $v1HashOnFailure -ne $v1HashBefore -or
        $legacyHashOnFailure -ne $legacyHashBefore -or
        $refinedHashOnFailure -ne $refinedHashBefore) {
        throw "CRITICAL: protected SDTH/v1/legacy/refined asset state changed while the v2 migration call failed."
    }
    if ($v2HashOnFailure -ne $v2HashBefore) {
        throw "CRITICAL: the Remote Control call failed or timed out and exact v2 disk state changed. Inspect the map/backup before retrying. Before=$v2HashBefore After=$v2HashOnFailure"
    }
    throw "Unreal refined-exterior migration call failed or timed out; protected hashes remain unchanged. $callError"
}

$sourceHashAfter = Get-OptionalSha256 $sourceMapPath
$v1HashAfter = Get-OptionalSha256 $v1MapPath
$v2HashAfter = Get-OptionalSha256 $v2MapPath
$legacyHashAfter = Get-OptionalSha256 $legacyAssetPath
$refinedHashAfter = Get-OptionalSha256 $refinedAssetPath
if ($sourceHashAfter -ne $sourceHashBefore) {
    throw "CRITICAL: SDTH changed during refined-exterior migration. Before=$sourceHashBefore After=$sourceHashAfter"
}
if ($v1HashAfter -ne $v1HashBefore) {
    throw "CRITICAL: v1 map changed during refined-exterior migration. Before=$v1HashBefore After=$v1HashAfter"
}
if ($legacyHashAfter -ne $legacyHashBefore) {
    throw "CRITICAL: legacy SM_IstanaExterior changed during map migration. Before=$legacyHashBefore After=$legacyHashAfter"
}
if ($refinedHashAfter -ne $refinedHashBefore) {
    throw "CRITICAL: refined exterior asset changed during map migration. Before=$refinedHashBefore After=$refinedHashAfter"
}
if ($null -eq $v2HashAfter) {
    throw "CRITICAL: exact v2 map disappeared during refined-exterior migration: $v2MapPath"
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
        throw "Refined-exterior migration returned a backup outside the allowlisted project Saved directory: $backupFull"
    }
    if (-not (Test-Path -LiteralPath $backupFull -PathType Leaf)) {
        throw "Refined-exterior migration reported a backup that does not exist: $backupFull"
    }
    $backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupFull).Hash
    if ($backupHash -ne $v2HashBefore) {
        throw "CRITICAL: pre-migration backup hash does not match original v2. Original=$v2HashBefore Backup=$backupHash"
    }
    $backupFile = $backupFull
}

$message = [string] $response.OutMessage
if ($response.ReturnValue -ne $true) {
    if ($v2HashAfter -ne $v2HashBefore) {
        throw "CRITICAL: RepairIstanaRuntimeMapV2ExteriorAsset returned failure after changing exact v2. Before=$v2HashBefore After=$v2HashAfter Message=$message Backup=$backupFile"
    }
    throw "RepairIstanaRuntimeMapV2ExteriorAsset failed safely with protected hashes unchanged: $message Backup=$backupFile"
}
if ($v2HashAfter -ne $v2HashBefore -and
    [string]::IsNullOrWhiteSpace($backupFile)) {
    throw "CRITICAL: exact v2 changed without a reported, verified pre-migration backup. Before=$v2HashBefore After=$v2HashAfter"
}
if ($v2HashAfter -eq $v2HashBefore -and
    -not [string]::IsNullOrWhiteSpace($backupFile)) {
    throw "CRITICAL: refined-exterior migration created a backup but exact v2 did not change. Refusing to classify a possibly non-persisted edit as idempotent. Hash=$v2HashAfter Backup=$backupFile"
}

[PSCustomObject]@{
    Operation = 'RepairIstanaRuntimeMapV2ExteriorAsset'
    Succeeded = $true
    Message = $message
    DestinationMap = '/Game/Maps/Istana_1km_Context_v2'
    RefinedMesh = '/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined.SM_IstanaExterior_Refined'
    IdempotentNoChange = (
        $v2HashAfter -eq $v2HashBefore -and
        [string]::IsNullOrWhiteSpace($backupFile))
    BackupFile = $backupFile
    BackupSha256 = $backupHash
    V2Sha256Before = $v2HashBefore
    V2Sha256After = $v2HashAfter
    SourceMapMutated = $false
    V1MapMutated = $false
    LegacyMeshMutated = $false
    RefinedMeshMutated = $false
    ReloadPersistenceValidated = $true
}
