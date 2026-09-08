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

$prepareBody = @{
    objectPath = $objectPath
    functionName = 'PrepareIstanaRuntimeMapV2Streaming'
    parameters = @{}
} | ConvertTo-Json -Depth 5
$response = Invoke-RestMethod `
    -Uri $endpoint `
    -Method Put `
    -ContentType 'application/json' `
    -Body $prepareBody `
    -TimeoutSec 120
if ($response.ReturnValue -ne $true) {
    throw "PrepareIstanaRuntimeMapV2Streaming failed safely: $($response.OutMessage)"
}

[PSCustomObject]@{
    Operation = 'PrepareIstanaRuntimeMapV2Streaming'
    Succeeded = $true
    Message = [string] $response.OutMessage
    DestinationMap = '/Game/Maps/Istana_1km_Context_v2'
    ProjectIdentityVerified = $true
    PackageSaved = $false
    ReadinessClaimed = $false
}
