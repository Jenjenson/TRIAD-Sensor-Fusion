[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[^\\/:*?"<>|]+\.json$')]
    [string] $OutputFileName,

    [string] $RequestJsonPath = 'IstanaSensorPlacementRequest.example.json',

    [string] $ProjectPath = 'D:\triad\TRIAD',

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$project = (Resolve-Path -LiteralPath $ProjectPath).Path
$outputDirectory = Join-Path $project 'Saved\TRIAD\PlacementSurveys'
$outputPath = Join-Path $outputDirectory $OutputFileName
if (Test-Path -LiteralPath $outputPath) {
    throw "Refusing to overwrite placement survey: $outputPath"
}

$body = @{
    objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
    functionName = 'GenerateIstanaPlacementSurvey'
    parameters = @{
        RequestJsonPath = $RequestJsonPath
        OutputFileName = $OutputFileName
    }
} | ConvertTo-Json -Depth 6

$response = Invoke-RestMethod `
    -Uri ([uri]::new($RemoteControlUrl, '/remote/object/call')) `
    -Method Put `
    -ContentType 'application/json' `
    -Body $body `
    -TimeoutSec 300

if ($response.ReturnValue -ne $true) {
    throw "GenerateIstanaPlacementSurvey failed: $($response.OutMessage)"
}
if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
    throw "Unreal reported success but the survey is missing: $outputPath"
}

$survey = Get-Content -Raw -LiteralPath $outputPath | ConvertFrom-Json
$authoritative = $survey.world.geometryAuthoritative -eq $true
$result = [PSCustomObject]@{
    OutputPath = $outputPath
    Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash
    GeometryAuthoritative = $authoritative
    GeometryAuthorityScope = [string] $survey.world.geometryAuthorityScope
    SurveyGrade = [bool] $survey.world.surveyGrade
    ResolvedOptions = [int] $survey.world.resolvedOptionCount
    OptionCount = [int] $survey.world.optionCount
    ResolvedSamples = [int] $survey.world.resolvedSampleCount
    SampleCount = [int] $survey.world.sampleCount
    EvaluationCount = @($survey.evaluations).Count
    Message = [string] $response.OutMessage
}

if (-not $authoritative) {
    $result | Format-List | Out-String | Write-Host
    throw 'Survey failed closed because not every candidate/sample surface resolved. Keep it only as a diagnostic and export a new filename after collision is ready.'
}

$result
