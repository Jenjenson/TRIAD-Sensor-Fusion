[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-ProjectDirectory {
    param([string] $Path)

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if (Test-Path -LiteralPath $resolved -PathType Leaf) {
        if ([System.IO.Path]::GetExtension($resolved) -ne '.uproject') {
            throw "ProjectPath file is not an Unreal project: $resolved"
        }
        return (Split-Path -Parent $resolved).TrimEnd('\')
    }
    return $resolved.TrimEnd('\')
}

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
        throw 'CRITICAL: protected v1 maps/assets, imported hero assets, or renderer settings changed file count during hero-v2 map migration.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -ne $entry.Value) {
            throw "CRITICAL: protected state changed during hero-v2 map migration: $($entry.Key)"
        }
    }
}

function Invoke-FailClosedSourceValidation {
    param(
        [string] $PythonExecutable,
        [string] $GeometryRoot,
        [string] $MaterialRoot
    )

    $geometryOutput = & $PythonExecutable (Join-Path $GeometryRoot 'validate_hero_v2.py') `
        --root $GeometryRoot `
        --output (Join-Path $GeometryRoot 'Generated') `
        --freeze (Join-Path $GeometryRoot 'hero_v2.freeze.json') 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: installed hero-v2 geometry validation failed. $($geometryOutput -join [Environment]::NewLine)"
    }

    $materialOutput = & $PythonExecutable (Join-Path $MaterialRoot 'build_hero_materials_v2.py') `
        --source-dir (Join-Path $MaterialRoot 'Source') `
        --source-manifest (Join-Path $MaterialRoot 'Source\source_manifest.json') `
        --output (Join-Path $MaterialRoot 'Generated') `
        --validate-only 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: installed HeroMaterialsV2 validation failed. $($materialOutput -join [Environment]::NewLine)"
    }
}

function Invoke-HeroV2RemoteCall {
    param(
        [uri] $Endpoint,
        [string] $ObjectPath,
        [string] $FunctionName,
        [hashtable] $Parameters,
        [int] $TimeoutSeconds
    )

    $body = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 6
    return Invoke-RestMethod `
        -Uri $Endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $body `
        -TimeoutSec $TimeoutSeconds
}

$resolvedProject = Resolve-ProjectDirectory -Path $ProjectPath
$projectFile = Join-Path $resolvedProject 'TRIAD.uproject'
$geometryRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV2'
$materialRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV2'
$sourceMap = Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v1.umap'
$destinationMap = Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v2.umap'
$invariantReport = Join-Path $resolvedProject 'Saved\TRIAD\IstanaPublicViewV2\Istana_PublicView_Exterior_v2.invariants.json'
$heroDiskRoot = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV2'
$materialDiskRoot = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\HeroMaterialsV2'
foreach ($requiredPath in @(
    $projectFile,
    $sourceMap,
    (Join-Path $geometryRoot 'validate_hero_v2.py'),
    (Join-Path $geometryRoot 'Generated\SM_IstanaPublicViewV2_Building_Hero.obj'),
    (Join-Path $materialRoot 'build_hero_materials_v2.py'),
    (Join-Path $heroDiskRoot 'Building\SM_IstanaPublicViewV2_Building_Hero.uasset'),
    $materialDiskRoot
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required hero-v2 migration input is missing: $requiredPath"
    }
}
if (Test-Path -LiteralPath $destinationMap) {
    throw "Refusing to overwrite existing v2 destination map: $destinationMap"
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$projectMaterialPython = Join-Path $materialRoot '.venv\Scripts\python.exe'
$repositoryMaterialPython = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicView\HeroMaterialsV2\.venv\Scripts\python.exe'
$pythonExecutable = if (Test-Path -LiteralPath $projectMaterialPython -PathType Leaf) {
    $projectMaterialPython
}
elseif (Test-Path -LiteralPath $repositoryMaterialPython -PathType Leaf) {
    $repositoryMaterialPython
}
else {
    (Get-Command python -ErrorAction Stop).Source
}
Invoke-FailClosedSourceValidation `
    -PythonExecutable $pythonExecutable `
    -GeometryRoot $geometryRoot `
    -MaterialRoot $materialRoot

$sourceMapShaBefore = (Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceMapBytesBefore = (Get-Item -LiteralPath $sourceMap).Length
$protectedPaths = @(
    $projectFile,
    (Join-Path $resolvedProject 'Config\DefaultEngine.ini'),
    (Join-Path $resolvedProject 'Content\SDTH.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km_Context_v2.umap'),
    $sourceMap,
    (Join-Path $resolvedProject 'Content\TRIAD\Istana'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaDigitalTwin'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Building'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Ground'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Context'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Vegetation'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Materials'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Textures'),
    $heroDiskRoot,
    $materialDiskRoot
)
$protectedBefore = Get-ProtectedSnapshot -LiteralPaths $protectedPaths

$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV2EditorLibrary'
$operationError = $null
$migrationResponse = $null
$mapValidation = $null
try {
    $identity = Invoke-HeroV2RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV2RemoteControlProject' `
        -Parameters @{ ExpectedProjectPath = $resolvedProject } `
        -TimeoutSeconds 30
    if ($identity.ReturnValue -ne $true) {
        throw "Remote Control project verification failed: $($identity.OutReport)"
    }

    $assetValidation = Invoke-HeroV2RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV2Assets' `
        -Parameters @{} `
        -TimeoutSeconds 1200
    if ($assetValidation.ReturnValue -ne $true) {
        throw "Hero-v2 assets are missing or invalid: $($assetValidation.OutReport)"
    }

    $migrationResponse = Invoke-HeroV2RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'MigrateIstanaPublicViewExteriorMapToHeroV2' `
        -Parameters @{} `
        -TimeoutSeconds 1800
    if ($migrationResponse.ReturnValue -ne $true) {
        throw "MigrateIstanaPublicViewExteriorMapToHeroV2 failed: $($migrationResponse.OutMessage)"
    }

    $mapValidation = Invoke-HeroV2RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV2Map' `
        -Parameters @{} `
        -TimeoutSeconds 1200
    if ($mapValidation.ReturnValue -ne $true) {
        throw "Post-migration hero-v2 map validation failed: $($mapValidation.OutReport)"
    }
}
catch {
    $operationError = $_
}

$protectedAfter = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
Assert-SameSnapshot -Before $protectedBefore -After $protectedAfter
if ($null -ne $operationError) {
    if (Test-Path -LiteralPath $destinationMap -PathType Leaf) {
        throw "A new v2 destination exists after a failed migration; inspect it and the invariant report before retrying. $operationError"
    }
    throw $operationError
}
if (-not (Test-Path -LiteralPath $destinationMap -PathType Leaf)) {
    throw "Migration returned success but exact destination map is absent: $destinationMap"
}
if (-not (Test-Path -LiteralPath $invariantReport -PathType Leaf)) {
    throw "Migration returned success but its exact invariant report is absent: $invariantReport"
}

$sourceMapShaAfter = (Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceMapBytesAfter = (Get-Item -LiteralPath $sourceMap).Length
$report = Get-Content -Raw -LiteralPath $invariantReport | ConvertFrom-Json
if ($sourceMapShaAfter -ne $sourceMapShaBefore -or
    $sourceMapBytesAfter -ne $sourceMapBytesBefore -or
    $report.schema -ne 'triad.istana_public_view_hero_v2.component_invariants.v1' -or
    $report.sourceMapPackage -ne '/Game/Maps/Istana_PublicView_Exterior_v1' -or
    $report.destinationMapPackage -ne '/Game/Maps/Istana_PublicView_Exterior_v2' -or
    $report.sourceMapSha256Before -ne $sourceMapShaBefore -or
    $report.sourceMapSha256After -ne $sourceMapShaAfter -or
    $report.sourceMapBytes -ne $sourceMapBytesBefore -or
    $report.sourceMapByteIdentical -ne $true -or
    $report.componentsExact -ne $true -or
    $report.camerasExact -ne $true -or
    $report.changesSurroundings -ne $false -or
    $report.requiresNanite -ne $false -or
    $report.requiresShaderModel6 -ne $false) {
    throw 'The persisted hero-v2 invariant report does not prove the exact v1/map/component/camera/no-renderer-change contract.'
}

[PSCustomObject]@{
    Operation = 'MigrateIstanaPublicViewExteriorMapToHeroV2'
    Succeeded = $true
    Message = [string] $migrationResponse.OutMessage
    Validation = [string] $mapValidation.OutReport
    SourceMap = '/Game/Maps/Istana_PublicView_Exterior_v1'
    DestinationMap = '/Game/Maps/Istana_PublicView_Exterior_v2'
    SourceMapSha256Unchanged = $sourceMapShaAfter
    InvariantReport = $invariantReport
    BuildingHeroVisualOnlySwapped = $true
    V1CollisionReused = $true
    SurroundingsMutated = $false
    RendererSettingsMutated = $false
    NaniteEnabled = $false
    ShaderModel6Required = $false
    DestinationOverwriteAllowed = $false
}
