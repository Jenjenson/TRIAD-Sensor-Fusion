[CmdletBinding()]
param(
    [string] $ProjectPath = 'D:\triad\TRIAD',
    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
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
        throw 'CRITICAL: protected existing maps/assets changed file count during isolated public-view import.'
    }
    foreach ($entry in $Before.GetEnumerator()) {
        if (-not $After.ContainsKey($entry.Key) -or $After[$entry.Key] -ne $entry.Value) {
            throw "CRITICAL: protected existing map/asset changed during isolated public-view import: $($entry.Key)"
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

$protectedPaths = @(
    (Join-Path $projectDirectory 'Content\SDTH.umap'),
    (Join-Path $projectDirectory 'Content\Maps\Istana_1km.umap'),
    (Join-Path $projectDirectory 'Content\Maps\Istana_1km_Context_v2.umap'),
    (Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v1.umap'),
    (Join-Path $projectDirectory 'Content\TRIAD\Istana'),
    (Join-Path $projectDirectory 'Content\TRIAD\IstanaDigitalTwin')
)
$protectedBefore = Get-ProtectedSnapshot -LiteralPaths $protectedPaths
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewEditorLibrary'
$operationError = $null
$response = $null
$validation = $null
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

    $importBody = @{
        objectPath = $objectPath
        functionName = 'ImportIstanaPublicViewAssets'
        parameters = @{}
    } | ConvertTo-Json -Depth 5
    $response = Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $importBody `
        -TimeoutSec 900
    if ($response.ReturnValue -ne $true) {
        throw "ImportIstanaPublicViewAssets failed: $($response.OutMessage)"
    }

    $validateBody = @{
        objectPath = $objectPath
        functionName = 'ValidateIstanaPublicViewAssets'
        parameters = @{}
    } | ConvertTo-Json -Depth 5
    $validation = Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $validateBody `
        -TimeoutSec 600
    if ($validation.ReturnValue -ne $true) {
        throw "Post-import validation failed: $($validation.OutReport)"
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

$expectedMeshes = @(
    'Building\SM_IstanaPublicView_Building_Hero.uasset',
    'Building\SM_IstanaPublicView_Building_Collision.uasset',
    'Ground\SM_IstanaPublicView_Terrain.uasset',
    'Ground\SM_IstanaPublicView_TerrainSkirt.uasset',
    'Ground\SM_IstanaPublicView_Hardscape.uasset',
    'Context\SM_IstanaPublicView_ContextBuildings.uasset',
    'Context\SM_IstanaPublicView_OSMContextBuildings.uasset',
    'Vegetation\SM_IstanaPublicView_Tree_Rain.uasset',
    'Vegetation\SM_IstanaPublicView_Tree_Palm.uasset',
    'Vegetation\SM_IstanaPublicView_Tree_Framing.uasset'
)
$expectedMaterials = @(
    'M_IPV_Render', 'M_IPV_Trim', 'M_IPV_Slate', 'M_IPV_Shutter',
    'M_IPV_Glass', 'M_IPV_Stone', 'M_IPV_Metal', 'M_IPV_Door',
    'M_IPV_DarkTimber', 'M_IPV_Opaline', 'M_IPV_Collision', 'M_IPV_Lawn',
    'M_IPV_Water', 'M_IPV_Planting', 'M_IPV_ContextRender',
    'M_IPV_ContextGlass', 'M_IPV_ContextRoof', 'M_IPV_Bark',
    'M_IPV_LeafDark', 'M_IPV_LeafMid', 'M_IPV_LeafLight',
    'M_IPV_TerrainSkirt'
)
$expectedTextures = @(
    'T_IPV_Bark_BaseColor', 'T_IPV_Bark_Normal', 'T_IPV_Bark_ORM',
    'T_IPV_DarkTimber_BaseColor', 'T_IPV_DarkTimber_Normal', 'T_IPV_DarkTimber_ORM',
    'T_IPV_Foliage_BaseColor', 'T_IPV_Foliage_Normal', 'T_IPV_Foliage_ORM',
    'T_IPV_Lawn_BaseColor', 'T_IPV_Lawn_Normal', 'T_IPV_Lawn_ORM',
    'T_IPV_Plaster_BaseColor', 'T_IPV_Plaster_Normal', 'T_IPV_Plaster_ORM',
    'T_IPV_Shutter_BaseColor', 'T_IPV_Shutter_Normal', 'T_IPV_Shutter_ORM',
    'T_IPV_Slate_BaseColor', 'T_IPV_Slate_Normal', 'T_IPV_Slate_ORM',
    'T_IPV_Stone_BaseColor', 'T_IPV_Stone_Normal', 'T_IPV_Stone_ORM'
)
$assetDiskRoot = Join-Path $projectDirectory 'Content\TRIAD\IstanaPublicView'
foreach ($relative in $expectedMeshes) {
    $assetPath = Join-Path $assetDiskRoot $relative
    if (-not (Test-Path -LiteralPath $assetPath -PathType Leaf)) {
        throw "Importer returned success but exact mesh package is absent: $assetPath"
    }
}
foreach ($materialName in $expectedMaterials) {
    $assetPath = Join-Path $assetDiskRoot "Materials\$materialName.uasset"
    if (-not (Test-Path -LiteralPath $assetPath -PathType Leaf)) {
        throw "Importer returned success but exact material package is absent: $assetPath"
    }
}
foreach ($textureName in $expectedTextures) {
    $assetPath = Join-Path $assetDiskRoot "Textures\$textureName.uasset"
    if (-not (Test-Path -LiteralPath $assetPath -PathType Leaf)) {
        throw "Importer returned success but exact PBR texture package is absent: $assetPath"
    }
}

[PSCustomObject]@{
    Operation = 'ImportIstanaPublicViewAssets'
    Succeeded = $true
    Message = [string] $response.OutMessage
    Validation = [string] $validation.OutReport
    AssetRoot = '/Game/TRIAD/IstanaPublicView'
    ClaimStatus = 'PUBLIC_REFERENCE_VISUAL_APPROXIMATION_NOT_SURVEY_CONTROLLED'
    MeshAssetCount = 10
    OSMContextStatus = 'COORDINATE_CONTRACT_ALIGNED_MAPPING_GRADE_NOT_VISUALLY_OR_PHOTO_ACCEPTED'
    OSMAttribution = '© OpenStreetMap contributors; ODbL-1.0'
    OSMContextUsedForSensorTruth = $false
    MaterialAssetCount = 22
    TextureAssetCount = 24
    PhotorealMaterialAcceptance = $false
    OverwriteAllowed = $false
    ExistingIstanaOrDigitalTwinMutated = $false
}
