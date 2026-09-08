[CmdletBinding()]
param(
    [string] $ProjectPath = 'D:\triad\TRIAD',
    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010',
    [switch] $ValidateOnly,
    [switch] $StructuralOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($StructuralOnly -and -not $ValidateOnly) {
    throw '-StructuralOnly is valid only together with -ValidateOnly.'
}

$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolvedProject -PathType Leaf) {
    Split-Path -Parent $resolvedProject
}
else {
    $resolvedProject
}
$sourceMapPath = Join-Path $projectDirectory 'Content\SDTH.umap'
if (-not (Test-Path -LiteralPath $sourceMapPath -PathType Leaf)) {
    throw "Source map is missing: $sourceMapPath"
}
$sourceHashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceMapPath).Hash
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')

$identityBody = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
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

$functionName = if ($ValidateOnly -and -not $StructuralOnly) {
    'ValidateIstanaRuntimeMapV2Readiness'
}
elseif ($ValidateOnly) {
    'ValidateIstanaRuntimeMapV2'
}
else {
    'BuildIstanaRuntimeMapV2'
}
$outputName = if ($ValidateOnly) { 'OutReport' } else { 'OutMessage' }
$body = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = $functionName
    parameters = @{}
} | ConvertTo-Json -Depth 5

try {
    $response = Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $body `
        -TimeoutSec 600
}
catch {
    $sourceHashAfter = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceMapPath).Hash
    if ($sourceHashAfter -ne $sourceHashBefore) {
        throw "CRITICAL: source map hash changed while $functionName failed. Stop integration and restore SDTH deliberately. Before=$sourceHashBefore After=$sourceHashAfter"
    }
    throw "Unreal Remote Control is unavailable at $RemoteControlUrl. $($_.Exception.Message)"
}

$sourceHashAfter = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceMapPath).Hash
if ($sourceHashAfter -ne $sourceHashBefore) {
    throw "CRITICAL: source map hash changed during $functionName. Stop integration and restore SDTH deliberately. Before=$sourceHashBefore After=$sourceHashAfter"
}

$message = [string] $response.$outputName
if ($response.ReturnValue -ne $true) {
    throw "$functionName failed: $message"
}

[PSCustomObject]@{
    Operation = $functionName
    Succeeded = $true
    Message = $message
    DestinationMap = '/Game/Maps/Istana_1km_Context_v2'
    SourceMapMutated = $false
    SourceMapSha256Before = $sourceHashBefore
    SourceMapSha256After = $sourceHashAfter
    ContextReadinessChecked = [bool] ($ValidateOnly -and -not $StructuralOnly)
}
