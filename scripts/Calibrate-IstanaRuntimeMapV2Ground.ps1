[CmdletBinding()]
param(
    [string] $ProjectPath = 'D:\triad\TRIAD',
    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010',
    [ValidateRange(0, 600)]
    [int] $ReadinessWaitSeconds = 120
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
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$identityBody = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = 'ValidateIstanaRemoteControlProject'
    parameters = @{ ExpectedProjectPath = $projectDirectory }
} | ConvertTo-Json -Depth 5
$identity = Invoke-RestMethod -Uri $endpoint -Method Put -ContentType 'application/json' -Body $identityBody -TimeoutSec 30
if ($identity.ReturnValue -ne $true) {
    throw "Remote Control project verification failed: $($identity.OutReport)"
}

$readinessBody = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = 'ValidateIstanaRuntimeMapV2Readiness'
    parameters = @{}
} | ConvertTo-Json -Depth 5
$readinessDeadline = [DateTime]::UtcNow.AddSeconds($ReadinessWaitSeconds)
do {
    $readiness = Invoke-RestMethod -Uri $endpoint -Method Put -ContentType 'application/json' -Body $readinessBody -TimeoutSec 30
    if ($readiness.ReturnValue -eq $true) {
        break
    }
    if ([DateTime]::UtcNow -ge $readinessDeadline) {
        throw "Cesium context did not reach the guarded calibration threshold within $ReadinessWaitSeconds seconds: $($readiness.OutReport)"
    }
    Start-Sleep -Seconds 5
} while ($true)

$body = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = 'CalibrateIstanaRuntimeMapV2Ground'
    parameters = @{}
} | ConvertTo-Json -Depth 5

try {
    $response = Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $body `
        -TimeoutSec 180
}
catch {
    throw "Unreal Remote Control is unavailable at $RemoteControlUrl. $($_.Exception.Message)"
}

if ($response.ReturnValue -ne $true) {
    throw "CalibrateIstanaRuntimeMapV2Ground failed safely: $($response.OutMessage)"
}

[PSCustomObject]@{
    Operation = 'CalibrateIstanaRuntimeMapV2Ground'
    Succeeded = $true
    Message = [string] $response.OutMessage
    DestinationMap = '/Game/Maps/Istana_1km_Context_v2'
    SourceMapMutated = $false
    CenterRoofSampleUsed = $false
}
