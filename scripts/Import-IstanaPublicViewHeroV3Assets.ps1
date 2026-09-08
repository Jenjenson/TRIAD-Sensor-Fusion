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
        throw 'CRITICAL: protected v1 maps/assets or renderer settings changed file count during hero-v3 import.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -ne $entry.Value) {
            throw "CRITICAL: protected v1 map/asset or renderer setting changed during hero-v3 import: $($entry.Key)"
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

function Assert-ExactPathSet {
    param(
        [string[]] $Actual,
        [string[]] $Expected,
        [string] $Label
    )

    $actualSorted = @($Actual | Sort-Object -Unique)
    $expectedSorted = @($Expected | Sort-Object -Unique)
    $differences = @(Compare-Object -ReferenceObject $expectedSorted -DifferenceObject $actualSorted)
    if ($actualSorted.Count -ne $expectedSorted.Count -or $differences.Count -ne 0) {
        throw "$Label disk roster differs from the frozen exact set."
    }
}

$resolvedProject = Resolve-ProjectDirectory -Path $ProjectPath
$projectFile = Join-Path $resolvedProject 'TRIAD.uproject'
$geometryRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV3'
$materialRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV3'
$geometryValidator = Join-Path $geometryRoot 'validate_hero_v3.py'
$materialBuilder = Join-Path $materialRoot 'build_hero_materials_v3.py'
$materialManifestPath = Join-Path $materialRoot 'Generated\manifest.json'
foreach ($requiredPath in @(
    $projectFile,
    $geometryValidator,
    (Join-Path $geometryRoot 'Generated\SM_IstanaPublicViewV3_Building_Hero.obj'),
    $materialBuilder,
    $materialManifestPath
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required installed hero-v3 input is missing: $requiredPath"
    }
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

$heroDiskRoot = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV3'
$materialDiskRoot = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\HeroMaterialsV3'
$destinationMap = Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v3.umap'
$protectedPaths = @(
    $projectFile,
    (Join-Path $resolvedProject 'Config\DefaultEngine.ini'),
    (Join-Path $resolvedProject 'Content\SDTH.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km_Context_v2.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v1.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v2.umap'),
    $destinationMap,
    (Join-Path $resolvedProject 'Content\TRIAD\Istana'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaDigitalTwin'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Building'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Ground'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Context'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Vegetation'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Materials'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\Textures'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV2'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\HeroMaterialsV2')
)
if ((Test-Path -LiteralPath $heroDiskRoot) -or
    (Test-Path -LiteralPath $materialDiskRoot)) {
    # An existing complete namespace is permitted only idempotently. A partial
    # namespace is rejected by C++, and this snapshot proves it was untouched.
    $protectedPaths += @($heroDiskRoot, $materialDiskRoot)
}
$protectedBefore = Get-ProtectedSnapshot -LiteralPaths $protectedPaths

$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV3EditorLibrary'
$operationError = $null
$importResponse = $null
$assetValidation = $null
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

    $importResponse = Invoke-HeroV3RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ImportIstanaPublicViewHeroV3Assets' `
        -Parameters @{} `
        -TimeoutSeconds 1800
    if ($importResponse.ReturnValue -ne $true) {
        throw "ImportIstanaPublicViewHeroV3Assets failed: $($importResponse.OutMessage)"
    }

    $assetValidation = Invoke-HeroV3RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV3Assets' `
        -Parameters @{} `
        -TimeoutSeconds 1200
    if ($assetValidation.ReturnValue -ne $true) {
        throw "Post-import hero-v3 validation failed: $($assetValidation.OutReport)"
    }
}
catch {
    $operationError = $_
}

$protectedAfter = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
Assert-SameSnapshot -Before $protectedBefore -After $protectedAfter
if ($null -ne $operationError) {
    throw $operationError
}

$expectedHeroRelative = @('Building\SM_IstanaPublicViewV3_Building_Hero.uasset')
$actualHeroRelative = @(Get-ChildItem -LiteralPath $heroDiskRoot -Recurse -File -Filter '*.uasset' | ForEach-Object {
    $_.FullName.Substring($heroDiskRoot.Length).TrimStart('\')
})
Assert-ExactPathSet -Actual $actualHeroRelative -Expected $expectedHeroRelative -Label 'Hero-v3 mesh'

$expectedMaterialAssets = @(
    'M_IPV_HeroSurface_V3',
    'M_IPV_HeroGlass_V3',
    'MI_IPV_Hero_Render_V3',
    'MI_IPV_Hero_Trim_V3',
    'MI_IPV_Hero_Slate_V3',
    'MI_IPV_Hero_Louvre_V3',
    'MI_IPV_Hero_Stone_V3',
    'MI_IPV_Hero_Timber_V3',
    'MI_IPV_Hero_PaintedMetal_V3',
    'MI_IPV_Hero_Glass_V3',
    'MI_IPV_Hero_Opaline_V3',
    'MI_IPV_Hero_Recess_V3'
)
$expectedMaterialRelative = @($expectedMaterialAssets | ForEach-Object { "$_.uasset" })
$materialManifest = Get-Content -Raw -LiteralPath $materialManifestPath | ConvertFrom-Json
$textureNames = [System.Collections.Generic.List[string]]::new()
foreach ($material in $materialManifest.materials) {
    foreach ($property in $material.outputs.PSObject.Properties) {
        $textureNames.Add([System.IO.Path]::GetFileNameWithoutExtension([string] $property.Value.file))
    }
}
foreach ($property in $materialManifest.sharedOutputs.PSObject.Properties) {
    $textureNames.Add([System.IO.Path]::GetFileNameWithoutExtension([string] $property.Value.file))
}
if ($textureNames.Count -ne 57 -or ($textureNames | Sort-Object -Unique).Count -ne 57) {
    throw 'Frozen HeroMaterialsV3 manifest no longer declares exactly 57 unique texture assets.'
}
$expectedMaterialRelative += @($textureNames | ForEach-Object { "Textures\$_.uasset" })
$actualMaterialRelative = @(Get-ChildItem -LiteralPath $materialDiskRoot -Recurse -File -Filter '*.uasset' | ForEach-Object {
    $_.FullName.Substring($materialDiskRoot.Length).TrimStart('\')
})
Assert-ExactPathSet -Actual $actualMaterialRelative -Expected $expectedMaterialRelative -Label 'HeroMaterialsV3'

[PSCustomObject]@{
    Operation = 'ImportIstanaPublicViewHeroV3Assets'
    Succeeded = $true
    Message = [string] $importResponse.OutMessage
    Validation = [string] $assetValidation.OutReport
    HeroAsset = '/Game/TRIAD/IstanaPublicViewV3/Building/SM_IstanaPublicViewV3_Building_Hero'
    MaterialAssetRoot = '/Game/TRIAD/IstanaPublicView/HeroMaterialsV3'
    HeroMeshAssetCount = 1
    MaterialAndInstanceAssetCount = 12
    TextureAssetCount = 57
    DestinationMapMutated = $false
    ProtectedV1ContentMutated = $false
    RendererSettingsMutated = $false
    NaniteEnabled = $false
    ShaderModel6Required = $false
    OverwriteAllowed = $false
}
