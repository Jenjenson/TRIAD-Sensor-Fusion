[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [ValidateNotNullOrEmpty()]
    [string] $MaterialPythonExecutable,

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

function Resolve-HeroMaterialPython {
    param(
        [string] $ExplicitOverride,
        [string[]] $PinnedCandidates
    )

    $candidates = [System.Collections.Generic.List[string]]::new()
    if (-not [string]::IsNullOrWhiteSpace($ExplicitOverride)) {
        $candidates.Add($ExplicitOverride)
    }
    foreach ($candidate in $PinnedCandidates) {
        $candidates.Add($candidate)
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $versions = @(& $candidate -B -c 'import PIL, numpy; print(PIL.__version__ + "|" + numpy.__version__)' 2>&1)
            if ($LASTEXITCODE -eq 0 -and
                (($versions -join '').Trim()) -eq '12.3.0|2.3.5') {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }
    throw 'No validated material Python was found. Use the pinned HeroMaterialsV2 .venv or pass -MaterialPythonExecutable with exact Pillow 12.3.0 and NumPy 2.3.5; system-Python fallback is forbidden.'
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
        throw 'CRITICAL: protected v1 maps/assets, imported hero assets, or renderer settings changed file count during hero-v3 map migration.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -ne $entry.Value) {
            throw "CRITICAL: protected state changed during hero-v3 map migration: $($entry.Key)"
        }
    }
}

function Invoke-FailClosedSourceValidation {
    param(
        [string] $PythonExecutable,
        [string] $GeometryRoot,
        [string] $MaterialRoot
    )

    $geometryOutput = & $PythonExecutable -B (Join-Path $GeometryRoot 'validate_hero_v3.py') `
        --root $GeometryRoot `
        --output (Join-Path $GeometryRoot 'Generated') `
        --freeze (Join-Path $GeometryRoot 'hero_v3.freeze.json') 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: installed hero-v3 geometry validation failed. $($geometryOutput -join [Environment]::NewLine)"
    }

    $materialOutput = & $PythonExecutable -B (Join-Path $MaterialRoot 'build_hero_materials_v3.py') `
        --source-dir (Join-Path $MaterialRoot 'Source\PolyHaven') `
        --source-manifest (Join-Path $MaterialRoot 'Source\source_manifest.json') `
        --output (Join-Path $MaterialRoot 'Generated') `
        --validate-only 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: installed HeroMaterialsV3 validation failed. $($materialOutput -join [Environment]::NewLine)"
    }
}

function Invoke-HeroV3RemoteCall {
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
$geometryRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV3'
$materialRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV3'
$protectedV1Map = Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v1.umap'
$sourceMap = Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v2.umap'
$destinationMap = Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v3.umap'
$invariantReport = Join-Path $resolvedProject 'Saved\TRIAD\IstanaPublicViewV3\Istana_PublicView_Exterior_v3.invariants.json'
$heroDiskRoot = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV3'
$materialDiskRoot = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\HeroMaterialsV3'
foreach ($requiredPath in @(
    $projectFile,
    $protectedV1Map,
    $sourceMap,
    (Join-Path $geometryRoot 'validate_hero_v3.py'),
    (Join-Path $geometryRoot 'Generated\SM_IstanaPublicViewV3_Building_Hero.obj'),
    (Join-Path $materialRoot 'build_hero_materials_v3.py'),
    (Join-Path $heroDiskRoot 'Building\SM_IstanaPublicViewV3_Building_Hero.uasset'),
    $materialDiskRoot
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required hero-v3 migration input is missing: $requiredPath"
    }
}
if (Test-Path -LiteralPath $destinationMap) {
    throw "Refusing to overwrite existing v3 destination map: $destinationMap"
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$projectMaterialPythonV2 = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV2\.venv\Scripts\python.exe'
$repositoryMaterialPythonV2 = Join-Path $repositoryRoot 'unreal\SourceAssets\IstanaPublicView\HeroMaterialsV2\.venv\Scripts\python.exe'
$pythonExecutable = Resolve-HeroMaterialPython `
    -ExplicitOverride $MaterialPythonExecutable `
    -PinnedCandidates @(
    $projectMaterialPythonV2,
    $repositoryMaterialPythonV2
)
Invoke-FailClosedSourceValidation `
    -PythonExecutable $pythonExecutable `
    -GeometryRoot $geometryRoot `
    -MaterialRoot $materialRoot

$protectedV1MapShaBefore = (Get-FileHash -LiteralPath $protectedV1Map -Algorithm SHA256).Hash.ToLowerInvariant()
$protectedV1MapBytesBefore = (Get-Item -LiteralPath $protectedV1Map).Length
$sourceMapShaBefore = (Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceMapBytesBefore = (Get-Item -LiteralPath $sourceMap).Length
$protectedPaths = @(
    $projectFile,
    (Join-Path $resolvedProject 'Config\DefaultEngine.ini'),
    (Join-Path $resolvedProject 'Content\SDTH.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km_Context_v2.umap'),
    $protectedV1Map,
    $sourceMap,
    (Join-Path $resolvedProject 'Content\TRIAD\Istana'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaDigitalTwin'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Building'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Ground'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Context'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Vegetation'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Materials'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Textures'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV2'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\HeroMaterialsV2'),
    $heroDiskRoot,
    $materialDiskRoot
)
$protectedBefore = Get-ProtectedSnapshot -LiteralPaths $protectedPaths

$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV3EditorLibrary'
$operationError = $null
$migrationResponse = $null
$mapValidation = $null
try {
    $identity = Invoke-HeroV3RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV3RemoteControlProject' `
        -Parameters @{ ExpectedProjectPath = $resolvedProject } `
        -TimeoutSeconds 30
    if ($identity.ReturnValue -ne $true) {
        throw "Remote Control project verification failed: $($identity.OutReport)"
    }

    $assetValidation = Invoke-HeroV3RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV3Assets' `
        -Parameters @{} `
        -TimeoutSeconds 1200
    if ($assetValidation.ReturnValue -ne $true) {
        throw "Hero-v3 assets are missing or invalid: $($assetValidation.OutReport)"
    }

    $migrationResponse = Invoke-HeroV3RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'MigrateIstanaPublicViewExteriorMapToHeroV3' `
        -Parameters @{} `
        -TimeoutSeconds 1800
    if ($migrationResponse.ReturnValue -ne $true) {
        throw "MigrateIstanaPublicViewExteriorMapToHeroV3 failed: $($migrationResponse.OutMessage)"
    }

    $mapValidation = Invoke-HeroV3RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV3Map' `
        -Parameters @{} `
        -TimeoutSeconds 1200
    if ($mapValidation.ReturnValue -ne $true) {
        throw "Post-migration hero-v3 map validation failed: $($mapValidation.OutReport)"
    }
}
catch {
    $operationError = $_
}

$protectedAfter = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
Assert-SameSnapshot -Before $protectedBefore -After $protectedAfter
if ($null -ne $operationError) {
    if (Test-Path -LiteralPath $destinationMap -PathType Leaf) {
        throw "A new v3 destination exists after a failed migration; inspect it and the invariant report before retrying. $operationError"
    }
    throw $operationError
}
if (-not (Test-Path -LiteralPath $destinationMap -PathType Leaf)) {
    throw "Migration returned success but exact destination map is absent: $destinationMap"
}
if (-not (Test-Path -LiteralPath $invariantReport -PathType Leaf)) {
    throw "Migration returned success but its exact invariant report is absent: $invariantReport"
}

$protectedV1MapShaAfter = (Get-FileHash -LiteralPath $protectedV1Map -Algorithm SHA256).Hash.ToLowerInvariant()
$protectedV1MapBytesAfter = (Get-Item -LiteralPath $protectedV1Map).Length
$sourceMapShaAfter = (Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceMapBytesAfter = (Get-Item -LiteralPath $sourceMap).Length
$report = Get-Content -Raw -LiteralPath $invariantReport | ConvertFrom-Json
if ($protectedV1MapShaAfter -ne $protectedV1MapShaBefore -or
    $protectedV1MapBytesAfter -ne $protectedV1MapBytesBefore -or
    $sourceMapShaAfter -ne $sourceMapShaBefore -or
    $sourceMapBytesAfter -ne $sourceMapBytesBefore -or
    $report.schema -ne 'triad.istana_public_view_hero_v3.component_invariants.v1' -or
    $report.protectedV1MapPackage -ne '/Game/Maps/Istana_PublicView_Exterior_v1' -or
    $report.protectedV1MapSha256Before -ne $protectedV1MapShaBefore -or
    $report.protectedV1MapSha256After -ne $protectedV1MapShaAfter -or
    $report.protectedV1MapBytes -ne $protectedV1MapBytesBefore -or
    $report.protectedV1MapByteIdentical -ne $true -or
    $report.sourceMapPackage -ne '/Game/Maps/Istana_PublicView_Exterior_v2' -or
    $report.destinationMapPackage -ne '/Game/Maps/Istana_PublicView_Exterior_v3' -or
    $report.sourceMapSha256Before -ne $sourceMapShaBefore -or
    $report.sourceMapSha256After -ne $sourceMapShaAfter -or
    $report.sourceMapBytes -ne $sourceMapBytesBefore -or
    $report.sourceMapByteIdentical -ne $true -or
    $report.onlyHeroVisualComponentChanged -ne $true -or
    $report.heroVisualNonAssetStateExact -ne $true -or
    $report.sourceHeroMesh -ne '/Game/TRIAD/IstanaPublicViewV2/Building/SM_IstanaPublicViewV2_Building_Hero.SM_IstanaPublicViewV2_Building_Hero' -or
    $report.destinationHeroMesh -ne '/Game/TRIAD/IstanaPublicViewV3/Building/SM_IstanaPublicViewV3_Building_Hero.SM_IstanaPublicViewV3_Building_Hero' -or
    $report.componentsExact -ne $true -or
    $report.camerasExact -ne $true -or
    $report.changesSurroundings -ne $false -or
    $report.requiresNanite -ne $false -or
    $report.requiresShaderModel6 -ne $false) {
    throw 'The persisted Hero-V3 invariant report does not prove exact V1/V2 map bytes, hero-only mutation, components, cameras, and no renderer changes.'
}

[PSCustomObject]@{
    Operation = 'MigrateIstanaPublicViewExteriorMapToHeroV3'
    Succeeded = $true
    Message = [string] $migrationResponse.OutMessage
    Validation = [string] $mapValidation.OutReport
    ProtectedV1Map = '/Game/Maps/Istana_PublicView_Exterior_v1'
    SourceMap = '/Game/Maps/Istana_PublicView_Exterior_v2'
    DestinationMap = '/Game/Maps/Istana_PublicView_Exterior_v3'
    SourceMapSha256Unchanged = $sourceMapShaAfter
    ProtectedV1MapSha256Unchanged = $protectedV1MapShaAfter
    InvariantReport = $invariantReport
    BuildingHeroVisualOnlySwapped = $true
    HeroVisualNonAssetStateExact = $true
    V1CollisionReused = $true
    SurroundingsMutated = $false
    RendererSettingsMutated = $false
    NaniteEnabled = $false
    ShaderModel6Required = $false
    DestinationOverwriteAllowed = $false
}
