[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('FRONT', 'OBLIQUE', 'SIDE')]
    [string] $Preset,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^v2_editor_[^\\/:*?"<>|]+\.png$')]
    [string] $OutputFileName,

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
    throw "Refusing to overwrite preview: $outputPath"
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

$body = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = 'CaptureIstanaPreview'
    parameters = @{
        PresetName = $Preset
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
    throw "CaptureIstanaPreview failed: $($response.OutMessage)"
}

$deadline = [DateTime]::UtcNow.AddSeconds(10)
while (-not (Test-Path -LiteralPath $outputPath -PathType Leaf) -and [DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 250
}
if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
    throw "Preview was scheduled but did not appear within 10 seconds: $outputPath"
}

[PSCustomObject]@{
    Kind = 'V2_IMPORTED_MESH_EDITOR'
    Preset = $Preset
    OutputPath = $outputPath
    Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash
    Bytes = (Get-Item -LiteralPath $outputPath).Length
    Message = [string] $response.OutMessage
}
