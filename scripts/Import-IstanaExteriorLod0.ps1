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
$identityBody = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = 'ValidateIstanaRemoteControlProject'
    parameters = @{ ExpectedProjectPath = $projectDirectory }
} | ConvertTo-Json -Depth 5
$identity = Invoke-RestMethod -Uri $endpoint -Method Put -ContentType 'application/json' -Body $identityBody -TimeoutSec 30
if ($identity.ReturnValue -ne $true) {
    throw "Remote Control project verification failed: $($identity.OutReport)"
}

$body = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = 'ImportIstanaExteriorLod0'
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
    throw "Unreal Remote Control is unavailable at $RemoteControlUrl. $($_.Exception.Message)"
}

if ($response.ReturnValue -ne $true) {
    throw "ImportIstanaExteriorLod0 failed: $($response.OutMessage)"
}

[PSCustomObject]@{
    Operation = 'ImportIstanaExteriorLod0'
    Succeeded = $true
    Message = [string] $response.OutMessage
    MeshObjectPath = '/Game/TRIAD/Istana/Meshes/SM_IstanaExterior.SM_IstanaExterior'
    OverwriteAllowed = $false
}
