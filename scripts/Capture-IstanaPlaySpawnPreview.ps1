[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v2_play_[^\\/:*?"<>|]+\.png$')]
    [string] $OutputFileName,

    [switch] $VisualAcceptanceOnly,

    [string] $ProjectPath = 'D:\triad\TRIAD',

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path
$project = if (Test-Path -LiteralPath $resolvedProject -PathType Leaf) {
    Split-Path -Parent $resolvedProject
}
else {
    $resolvedProject
}
$outputPath = Join-Path $project "Saved\TRIAD\IstanaPreviews\V2\$OutputFileName"
if (Test-Path -LiteralPath $outputPath) {
    throw "Refusing to overwrite v2 PIE preview: $outputPath"
}
if ($VisualAcceptanceOnly -and
    -not $OutputFileName.StartsWith(
        'v2_play_visual_acceptance_',
        [StringComparison]::Ordinal)) {
    throw 'Visual-acceptance captures must start with v2_play_visual_acceptance_.'
}

$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$identityBody = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = 'ValidateIstanaRemoteControlProject'
    parameters = @{ ExpectedProjectPath = $project }
} | ConvertTo-Json -Depth 5
$identity = Invoke-RestMethod -Uri $endpoint -Method Put -ContentType 'application/json' -Body $identityBody -TimeoutSec 30
if ($identity.ReturnValue -ne $true) {
    throw "Remote Control project verification failed: $($identity.OutReport)"
}

$readinessFunction = if ($VisualAcceptanceOnly) {
    'ValidateIstanaVisualAcceptancePlayWorldReadiness'
}
else {
    'ValidateIstanaPlayWorldReadiness'
}
$requiredStablePolls = if ($VisualAcceptanceOnly) { 3 } else { 1 }
$readinessBody = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = $readinessFunction
    parameters = @{}
} | ConvertTo-Json -Depth 5
$readiness = $null
for ($poll = 1; $poll -le $requiredStablePolls; ++$poll) {
    $readiness = Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $readinessBody `
        -TimeoutSec 60
    if ($readiness.ReturnValue -ne $true) {
        throw "V2 PIE capture readiness '$readinessFunction' failed on stable poll $poll/$requiredStablePolls`: $($readiness.OutReport)"
    }
    if ($poll -lt $requiredStablePolls) {
        Start-Sleep -Seconds 1
    }
}

$body = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = 'CaptureIstanaPlaySpawnPreview'
    parameters = @{
        OutputFileName = $OutputFileName
    }
} | ConvertTo-Json -Depth 6

$response = Invoke-RestMethod `
    -Uri $endpoint `
    -Method Put `
    -ContentType 'application/json' `
    -Body $body `
    -TimeoutSec 60

if ($response.ReturnValue -ne $true) {
    throw "CaptureIstanaPlaySpawnPreview failed: $($response.OutMessage)"
}

$deadline = [DateTime]::UtcNow.AddSeconds(10)
while (-not (Test-Path -LiteralPath $outputPath -PathType Leaf) -and [DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 250
}
if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
    throw "V2 PIE preview was scheduled but did not appear within 10 seconds: $outputPath"
}

[PSCustomObject]@{
    Kind = 'V2_PIE_PLAYER0_SPAWN'
    OutputPath = $outputPath
    Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash
    Bytes = (Get-Item -LiteralPath $outputPath).Length
    VisualAcceptanceOnly = [bool] $VisualAcceptanceOnly
    ReadinessFunction = $readinessFunction
    StableReadinessPolls = $requiredStablePolls
    PlayWorldReadiness = [string] $readiness.OutReport
    Message = [string] $response.OutMessage
}
