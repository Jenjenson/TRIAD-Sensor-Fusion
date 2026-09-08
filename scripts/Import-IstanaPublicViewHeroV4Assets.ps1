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
        throw 'CRITICAL: protected v1/v2/v3 maps/assets or renderer settings changed file count during hero-v4 import.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -ne $entry.Value) {
            throw "CRITICAL: protected v1/v2/v3 map/asset or renderer setting changed during hero-v4 import: $($entry.Key)"
        }
    }
}

function Invoke-FailClosedSourceValidation {
    param(
        [string] $PythonExecutable,
        [string] $GeometryRoot,
        [string] $MaterialRoot
    )

    $geometryOutput = & $PythonExecutable -B (Join-Path $GeometryRoot 'validate_hero_v4.py') `
        --root $GeometryRoot `
        --output (Join-Path $GeometryRoot 'Generated') `
        --freeze (Join-Path $GeometryRoot 'hero_v4.freeze.json') 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: installed hero-v4 geometry validation failed. $($geometryOutput -join [Environment]::NewLine)"
    }

    $materialOutput = & $PythonExecutable -B `
        (Join-Path $MaterialRoot 'validate_hero_materials_v4.py') `
        --validate-only 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "ASSETS_MISSING_OR_INVALID: installed HeroMaterialsV4 validation failed. $($materialOutput -join [Environment]::NewLine)"
    }
}

function Invoke-HeroV4RemoteCall {
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
$geometryRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicViewV4'
$materialRoot = Join-Path $resolvedProject 'SourceAssets\IstanaPublicView\HeroMaterialsV4'
$geometryValidator = Join-Path $geometryRoot 'validate_hero_v4.py'
$materialValidator = Join-Path $materialRoot 'validate_hero_materials_v4.py'
foreach ($requiredPath in @(
    $projectFile,
    $geometryValidator,
    (Join-Path $geometryRoot 'hero_v4.freeze.json'),
    (Join-Path $geometryRoot 'Generated\SM_IstanaPublicViewV4_Building_Hero.obj'),
    $materialValidator,
    (Join-Path $materialRoot 'hero_materials_v4.freeze.json')
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required installed hero-v4 input is missing: $requiredPath"
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

$heroDiskRoot = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV4'
$materialDiskRoot = Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\HeroMaterialsV4'
$destinationMap = Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v4.umap'
$protectedPaths = @(
    $projectFile,
    (Join-Path $resolvedProject 'Config\DefaultEngine.ini'),
    (Join-Path $resolvedProject 'Content\SDTH.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_1km_Context_v2.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v1.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v2.umap'),
    (Join-Path $resolvedProject 'Content\Maps\Istana_PublicView_Exterior_v3.umap'),
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
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\HeroMaterialsV2'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicViewV3'),
    (Join-Path $resolvedProject 'Content\TRIAD\IstanaPublicView\HeroMaterialsV3')
)
if ((Test-Path -LiteralPath $heroDiskRoot) -or
    (Test-Path -LiteralPath $materialDiskRoot)) {
    # An existing complete namespace is permitted only idempotently. A partial
    # namespace is rejected by C++, and this snapshot proves it was untouched.
    $protectedPaths += @($heroDiskRoot, $materialDiskRoot)
}
$protectedBefore = Get-ProtectedSnapshot -LiteralPaths $protectedPaths

$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV4EditorLibrary'
$operationError = $null
$importResponse = $null
$assetValidation = $null
try {
    $identity = Invoke-HeroV4RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV4RemoteControlProject' `
        -Parameters @{ ExpectedProjectPath = $resolvedProject } `
        -TimeoutSeconds 30
    if ($identity.ReturnValue -ne $true) {
        throw "Remote Control project verification failed: $($identity.OutReport)"
    }

    $importResponse = Invoke-HeroV4RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ImportIstanaPublicViewHeroV4Assets' `
        -Parameters @{} `
        -TimeoutSeconds 1800
    if ($importResponse.ReturnValue -ne $true) {
        throw "ImportIstanaPublicViewHeroV4Assets failed: $($importResponse.OutMessage)"
    }

    $assetValidation = Invoke-HeroV4RemoteCall `
        -Endpoint $endpoint `
        -ObjectPath $objectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV4Assets' `
        -Parameters @{} `
        -TimeoutSeconds 1200
    if ($assetValidation.ReturnValue -ne $true) {
        throw "Post-import hero-v4 validation failed: $($assetValidation.OutReport)"
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

$expectedHeroRelative = @('Building\SM_IstanaPublicViewV4_Building_Hero.uasset')
$actualHeroRelative = @(Get-ChildItem -LiteralPath $heroDiskRoot -Recurse -File -Filter '*.uasset' | ForEach-Object {
    $_.FullName.Substring($heroDiskRoot.Length).TrimStart('\')
})
Assert-ExactPathSet -Actual $actualHeroRelative -Expected $expectedHeroRelative -Label 'Hero-v4 mesh'

$expectedMaterialAssets = @(
    'M_IPV_HeroSurface_V4',
    'M_IPV_HeroGlass_V4',
    'MI_IPV_Hero_Render_V4',
    'MI_IPV_Hero_Trim_V4',
    'MI_IPV_Hero_Slate_V4',
    'MI_IPV_Hero_Louvre_V4',
    'MI_IPV_Hero_Stone_V4',
    'MI_IPV_Hero_Timber_V4',
    'MI_IPV_Hero_PaintedMetal_V4',
    'MI_IPV_Hero_Glass_V4',
    'MI_IPV_Hero_Opaline_V4',
    'MI_IPV_Hero_Recess_V4'
)
$expectedMaterialRelative = @($expectedMaterialAssets | ForEach-Object { "$_.uasset" })
$actualMaterialRelative = @(Get-ChildItem -LiteralPath $materialDiskRoot -Recurse -File -Filter '*.uasset' | ForEach-Object {
    $_.FullName.Substring($materialDiskRoot.Length).TrimStart('\')
})
Assert-ExactPathSet -Actual $actualMaterialRelative -Expected $expectedMaterialRelative -Label 'HeroMaterialsV4'

[PSCustomObject]@{
    Operation = 'ImportIstanaPublicViewHeroV4Assets'
    Succeeded = $true
    Message = [string] $importResponse.OutMessage
    Validation = [string] $assetValidation.OutReport
    HeroAsset = '/Game/TRIAD/IstanaPublicViewV4/Building/SM_IstanaPublicViewV4_Building_Hero'
    MaterialAssetRoot = '/Game/TRIAD/IstanaPublicView/HeroMaterialsV4'
    HeroMeshAssetCount = 1
    MaterialAndInstanceAssetCount = 12
    TextureAssetCount = 0
    FrozenV3TextureReferenceCount = 57
    DestinationMapMutated = $false
    ProtectedV1ContentMutated = $false
    RendererSettingsMutated = $false
    NaniteEnabled = $false
    ShaderModel6Required = $false
    OverwriteAllowed = $false
}
