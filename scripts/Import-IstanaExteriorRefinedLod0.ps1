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
$sourceObj = Join-Path $projectDirectory 'SourceAssets\Istana\Generated\SM_IstanaExterior_LOD0.obj'
$sourceManifest = Join-Path $projectDirectory 'SourceAssets\Istana\Generated\IstanaExterior.manifest.json'
$legacyAsset = Join-Path $projectDirectory 'Content\TRIAD\Istana\Meshes\SM_IstanaExterior.uasset'
$refinedAsset = Join-Path $projectDirectory 'Content\TRIAD\Istana\Meshes\SM_IstanaExterior_Refined.uasset'
$sourceMap = Join-Path $projectDirectory 'Content\SDTH.umap'
$expectedDigest = '4C822BB2C85451136C41FAB0A362C7D2F0CA6663C0AE44CFECBE8F73E6A83B91'
$expectedSlots = @(
    'M_Istana_Plaster',
    'M_Istana_PlasterTrim',
    'M_Istana_Slate',
    'M_Istana_Shutter',
    'M_Istana_Glass',
    'M_Istana_Stone',
    'M_Istana_Metal',
    'M_Istana_Door'
)

foreach ($requiredPath in @($sourceObj, $sourceManifest, $sourceMap)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required refined-import input is missing: $requiredPath"
    }
}
if (Test-Path -LiteralPath $refinedAsset) {
    throw "Refusing to overwrite existing refined exterior asset: $refinedAsset"
}

$manifest = Get-Content -LiteralPath $sourceManifest -Raw | ConvertFrom-Json
$lod0 = @($manifest.files | Where-Object { $_.role -eq 'visual_lod_0' })
if ($manifest.schema -ne 'triad.istana_source_assets.v1' -or
    $manifest.coordinateSystem -ne 'right-handed Z-up; ceremonial approach +Y' -or
    $manifest.authoringUnits -ne 'metres' -or
    $manifest.encodedObjUnits -ne 'centimetres' -or
    $manifest.unrealUnits -ne 'centimetres (import scale 1.0)' -or
    $lod0.Count -ne 1 -or
    $lod0[0].path -ne 'SM_IstanaExterior_LOD0.obj' -or
    [int] $lod0[0].vertices -ne 175974 -or
    [int] $lod0[0].triangles -ne 58658 -or
    [string] $lod0[0].sha256 -ne $expectedDigest -or
    (@($manifest.materialSlots) -join '|') -ne ($expectedSlots -join '|')) {
    throw 'Refined source manifest does not match the reviewed schema/units/metrics/digest/material-slot contract.'
}
$actualDigest = (Get-FileHash -LiteralPath $sourceObj -Algorithm SHA256).Hash
if ($actualDigest -ne $expectedDigest) {
    throw "Refined LOD0 OBJ SHA-256 does not match the reviewed manifest. Expected=$expectedDigest Actual=$actualDigest"
}

$sourceMapHashBefore = (Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256).Hash
$legacyHashBefore = if (Test-Path -LiteralPath $legacyAsset -PathType Leaf) {
    (Get-FileHash -LiteralPath $legacyAsset -Algorithm SHA256).Hash
}
else {
    $null
}
$materialFiles = $expectedSlots | ForEach-Object {
    Join-Path $projectDirectory "Content\TRIAD\Istana\Materials\$_.uasset"
}
$materialHashesBefore = @{}
foreach ($materialFile in $materialFiles) {
    if (-not (Test-Path -LiteralPath $materialFile -PathType Leaf)) {
        throw "Required existing project-owned PBR material is missing: $materialFile"
    }
    $materialHashesBefore[$materialFile] =
        (Get-FileHash -LiteralPath $materialFile -Algorithm SHA256).Hash
}

$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$objectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaEditorLibrary'
$identityBody = @{
    objectPath = $objectPath
    functionName = 'ValidateIstanaRemoteControlProject'
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
    functionName = 'ImportIstanaExteriorRefinedLod0'
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
    $callError = $_.Exception.Message
    $sourceMapHashOnFailure = if (Test-Path -LiteralPath $sourceMap -PathType Leaf) {
        (Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256).Hash
    }
    else {
        $null
    }
    $legacyHashOnFailure = if (Test-Path -LiteralPath $legacyAsset -PathType Leaf) {
        (Get-FileHash -LiteralPath $legacyAsset -Algorithm SHA256).Hash
    }
    else {
        $null
    }
    if ($sourceMapHashOnFailure -ne $sourceMapHashBefore -or
        $legacyHashOnFailure -ne $legacyHashBefore) {
        throw 'CRITICAL: SDTH or legacy SM_IstanaExterior changed while the refined import Remote Control call failed or timed out.'
    }
    foreach ($materialFile in $materialFiles) {
        $materialHashOnFailure = if (Test-Path -LiteralPath $materialFile -PathType Leaf) {
            (Get-FileHash -LiteralPath $materialFile -Algorithm SHA256).Hash
        }
        else {
            $null
        }
        if ($materialHashOnFailure -ne $materialHashesBefore[$materialFile]) {
            throw "CRITICAL: existing PBR material changed while the refined import call failed or timed out: $materialFile"
        }
    }
    if (Test-Path -LiteralPath $refinedAsset -PathType Leaf) {
        throw "CRITICAL: the refined asset appeared on disk while the Remote Control call failed or timed out. Inspect '$refinedAsset' before retrying."
    }
    throw "Unreal refined-exterior import call failed or timed out with protected disk hashes unchanged. Restart the editor before retrying so any unsaved import package is discarded. $callError"
}

$sourceMapHashAfter = (Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256).Hash
$legacyHashAfter = if (Test-Path -LiteralPath $legacyAsset -PathType Leaf) {
    (Get-FileHash -LiteralPath $legacyAsset -Algorithm SHA256).Hash
}
else {
    $null
}
if ($sourceMapHashAfter -ne $sourceMapHashBefore) {
    throw "CRITICAL: SDTH changed during refined exterior import. Before=$sourceMapHashBefore After=$sourceMapHashAfter"
}
if ($legacyHashAfter -ne $legacyHashBefore) {
    throw "CRITICAL: legacy SM_IstanaExterior changed during refined import. Before=$legacyHashBefore After=$legacyHashAfter"
}
foreach ($materialFile in $materialFiles) {
    $materialHashAfter = (Get-FileHash -LiteralPath $materialFile -Algorithm SHA256).Hash
    if ($materialHashAfter -ne $materialHashesBefore[$materialFile]) {
        throw "CRITICAL: existing PBR material changed during refined mesh import: $materialFile"
    }
}
if ($response.ReturnValue -ne $true) {
    if (Test-Path -LiteralPath $refinedAsset -PathType Leaf) {
        throw "CRITICAL: refined importer returned failure after creating disk asset '$refinedAsset'. Inspect before retrying. $($response.OutMessage)"
    }
    throw "ImportIstanaExteriorRefinedLod0 failed without changing SDTH, legacy mesh, materials, or refined disk asset. Restart editor before retrying. $($response.OutMessage)"
}
if (-not (Test-Path -LiteralPath $refinedAsset -PathType Leaf)) {
    throw "Refined importer returned success but the expected asset was not saved: $refinedAsset"
}

[PSCustomObject]@{
    Operation = 'ImportIstanaExteriorRefinedLod0'
    Succeeded = $true
    Message = [string] $response.OutMessage
    MeshObjectPath = '/Game/TRIAD/Istana/Meshes/SM_IstanaExterior_Refined.SM_IstanaExterior_Refined'
    SourceObjSha256 = $actualDigest
    LegacyMeshMutated = $false
    ExistingMaterialsMutated = $false
    SourceMapMutated = $false
    OverwriteAllowed = $false
}
