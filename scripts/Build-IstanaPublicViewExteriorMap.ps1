[CmdletBinding()]
param(
    [string] $ProjectPath = 'D:\triad\TRIAD',
    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010',
    [switch] $ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ProtectedSnapshot {
    param([string[]] $LiteralPaths)

    $snapshot = [System.Collections.Generic.Dictionary[string, string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $LiteralPaths) {
        $fullPath = [System.IO.Path]::GetFullPath($path)
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            $snapshot[$fullPath] = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash
            continue
        }
        if (Test-Path -LiteralPath $fullPath -PathType Container) {
            foreach ($file in Get-ChildItem -LiteralPath $fullPath -Recurse -File | Sort-Object FullName) {
                $snapshot[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
            }
            continue
        }
        $snapshot["MISSING::$fullPath"] = 'MISSING'
    }
    return ,$snapshot
}

function Assert-SameSnapshot {
    param(
        [System.Collections.Generic.Dictionary[string, string]] $Before,
        [System.Collections.Generic.Dictionary[string, string]] $After
    )

    if ($Before.Count -ne $After.Count) {
        throw 'CRITICAL: protected existing maps/assets changed file count during isolated public-view map operation.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -ne $entry.Value) {
            throw "CRITICAL: protected existing map/asset changed during isolated public-view map operation: $($entry.Key)"
        }
    }
}

$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolvedProject -PathType Leaf) {
    Split-Path -Parent $resolvedProject
}
else {
    $resolvedProject
}
$sourceRoot = Join-Path $projectDirectory 'SourceAssets\IstanaPublicView'
$sourceValidator = Join-Path $sourceRoot 'validate_public_view_assets.py'
$osmContextValidator = Join-Path $sourceRoot 'validate_public_view_osm_context.py'
$destinationMapFile = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v1.umap'
foreach ($validator in @($sourceValidator, $osmContextValidator)) {
    if (-not (Test-Path -LiteralPath $validator -PathType Leaf)) {
        throw "ASSETS_MISSING: source validator is absent: $validator"
    }
}
$validatorOutput = & python $sourceValidator --root $sourceRoot 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "ASSETS_MISSING_OR_INVALID: source contract validation failed. $($validatorOutput -join [Environment]::NewLine)"
}
$osmValidatorOutput = & python $osmContextValidator --root $sourceRoot 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "ASSETS_MISSING_OR_INVALID: ODbL context source contract validation failed. $($osmValidatorOutput -join [Environment]::NewLine)"
}
if (-not $ValidateOnly -and (Test-Path -LiteralPath $destinationMapFile)) {
    throw "Refusing to overwrite existing destination map: $destinationMapFile"
}
if ($ValidateOnly -and -not (Test-Path -LiteralPath $destinationMapFile -PathType Leaf)) {
    throw "Validation target is absent: $destinationMapFile"
}

$protectedPaths = @(
    (Join-Path $projectDirectory 'Content\SDTH.umap'),
    (Join-Path $projectDirectory 'Content\Maps\Istana_1km.umap'),
    (Join-Path $projectDirectory 'Content\Maps\Istana_1km_Context_v2.umap'),
    (Join-Path $projectDirectory 'Content\TRIAD\Istana'),
    (Join-Path $projectDirectory 'Content\TRIAD\IstanaDigitalTwin'),
    (Join-Path $projectDirectory 'Content\TRIAD\IstanaPublicView')
)
if ($ValidateOnly) {
    $protectedPaths += $destinationMapFile
}
$protectedBefore = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewEditorLibrary'
$functionName = if ($ValidateOnly) {
    'ValidateIstanaPublicViewExteriorMap'
}
else {
    'BuildIstanaPublicViewExteriorMap'
}
$outputName = if ($ValidateOnly) { 'OutReport' } else { 'OutMessage' }
$response = $null
$operationError = $null
try {
    $identityBody = @{
        objectPath = $objectPath
        functionName = 'ValidateIstanaPublicViewRemoteControlProject'
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

    $body = @{
        objectPath = $objectPath
        functionName = $functionName
        parameters = @{}
    } | ConvertTo-Json -Depth 5
    $response = Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $body `
        -TimeoutSec 900
    if ($response.ReturnValue -ne $true) {
        throw "$functionName failed: $($response.$outputName)"
    }
}
catch {
    $operationError = $_
}

$protectedAfter = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
Assert-SameSnapshot -Before $protectedBefore -After $protectedAfter
if ($null -ne $operationError) {
    if (-not $ValidateOnly -and (Test-Path -LiteralPath $destinationMapFile -PathType Leaf)) {
        throw "CRITICAL: destination map appeared on disk while build failed. Inspect it before retrying. $operationError"
    }
    throw $operationError
}
if (-not (Test-Path -LiteralPath $destinationMapFile -PathType Leaf)) {
    throw "Operation returned success but exact destination is absent: $destinationMapFile"
}

[PSCustomObject]@{
    Operation = $functionName
    Succeeded = $true
    Message = [string] $response.$outputName
    DestinationMap = '/Game/Maps/Istana_PublicView_Exterior_v1'
    ClaimStatus = 'PUBLIC_REFERENCE_VISUAL_APPROXIMATION_NOT_SURVEY_CONTROLLED'
    SourceMapsMutated = $false
    ExistingIstanaOrDigitalTwinAssetsMutated = $false
    PublicViewAssetsMutated = $false
    DestinationOverwriteAllowed = $false
    OSMContextStatus = 'COORDINATE_CONTRACT_ALIGNED_MAPPING_GRADE_NOT_VISUALLY_OR_PHOTO_ACCEPTED'
    OSMAttribution = '© OpenStreetMap contributors; ODbL-1.0'
    OSMContextUsedForSensorTruth = $false
}
