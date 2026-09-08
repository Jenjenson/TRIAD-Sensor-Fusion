[CmdletBinding()]
param(
    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010',
    [switch] $ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
$functionName = if ($ValidateOnly) { 'ValidateIstanaStudyMap' } else { 'BuildIstanaStudyMap' }
$outputName = if ($ValidateOnly) { 'OutReport' } else { 'OutMessage' }
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$body = @{
    objectPath = $objectPath
    functionName = $functionName
    parameters = @{}
} | ConvertTo-Json -Depth 5

try {
    $response = Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $body
}
catch {
    throw "Unreal Remote Control is unavailable at $RemoteControlUrl. Start TRIAD after building the plugin. $($_.Exception.Message)"
}

$message = [string] $response.$outputName
if ($response.ReturnValue -ne $true) {
    throw "$functionName failed: $message"
}

[PSCustomObject]@{
    Operation = $functionName
    Succeeded = $true
    Message = $message
    SourceMapMutated = $false
}
