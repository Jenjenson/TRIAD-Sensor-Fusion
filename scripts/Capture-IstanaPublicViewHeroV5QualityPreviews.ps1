[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')]
    [string] $RunId,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [ValidateRange(8, 64)]
    [int] $TemporalWarmupFrames = 32,

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
$outputDirectory = Join-Path $projectDirectory 'Saved\TRIAD\IstanaPreviews\PublicViewHeroV5'
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$heroLibraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV5EditorLibrary'
$runtimeLibraryObjectPath = $heroLibraryObjectPath
$destinationMap = '/Game/Maps/Istana_PublicView_Exterior_v5'
$visualProfilePath = Join-Path $projectDirectory 'Config\IstanaPublicViewHeroV3VisualAcceptance.settings.json'
$rendererConfigPath = Join-Path $projectDirectory 'Config\DefaultEngine.ini'
$v1MapPath = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v1.umap'
$v2MapPath = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v2.umap'
$v3MapPath = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v3.umap'
$v4MapPath = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v4.umap'
$v5MapPath = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v5.umap'
foreach ($mapPath in @($visualProfilePath, $rendererConfigPath, $v1MapPath, $v2MapPath, $v3MapPath, $v4MapPath, $v5MapPath)) {
    if (-not (Test-Path -LiteralPath $mapPath -PathType Leaf)) {
        throw "Required public-view map is missing: $mapPath"
    }
}
$visualProfileHashBefore = (Get-FileHash -LiteralPath $visualProfilePath -Algorithm SHA256).Hash
$rendererConfigHashBefore = (Get-FileHash -LiteralPath $rendererConfigPath -Algorithm SHA256).Hash
$v1HashBefore = (Get-FileHash -LiteralPath $v1MapPath -Algorithm SHA256).Hash
$v2HashBefore = (Get-FileHash -LiteralPath $v2MapPath -Algorithm SHA256).Hash
$v3HashBefore = (Get-FileHash -LiteralPath $v3MapPath -Algorithm SHA256).Hash
$v4HashBefore = (Get-FileHash -LiteralPath $v4MapPath -Algorithm SHA256).Hash
$v5HashBefore = (Get-FileHash -LiteralPath $v5MapPath -Algorithm SHA256).Hash

function Invoke-PublicViewCall {
    param(
        [Parameter(Mandatory = $true)]
        [string] $ObjectPath,

        [Parameter(Mandatory = $true)]
        [string] $FunctionName,

        [hashtable] $Parameters = @{},

        [ValidateRange(1, 900)]
        [int] $TimeoutSeconds = 60
    )

    $body = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 6
    return Invoke-RestMethod `
        -Uri $endpoint `
        -Method Put `
        -ContentType 'application/json' `
        -Body $body `
        -TimeoutSec $TimeoutSeconds
}

function Get-StableHeroV5Readiness {
    $lastReadiness = $null
    for ($poll = 1; $poll -le 3; ++$poll) {
        $lastReadiness = Invoke-PublicViewCall `
            -ObjectPath $runtimeLibraryObjectPath `
            -FunctionName 'ValidateIstanaPublicViewHeroV5PlayWorldReadiness' `
            -TimeoutSeconds 60
        if ($lastReadiness.ReturnValue -ne $true) {
            throw "Hero-v5 PIE readiness failed on stable poll $poll/3: $($lastReadiness.OutReport)"
        }
        if ($poll -lt 3) {
            Start-Sleep -Seconds 1
        }
    }
    return $lastReadiness
}

function Get-PngDimensions {
    param([Parameter(Mandatory = $true)] [string] $LiteralPath)

    $bytes = [System.IO.File]::ReadAllBytes($LiteralPath)
    $signature = @(137, 80, 78, 71, 13, 10, 26, 10)
    if ($bytes.Length -lt 24) {
        throw "Capture is too small to be a PNG: $LiteralPath"
    }
    for ($index = 0; $index -lt $signature.Count; ++$index) {
        if ($bytes[$index] -ne $signature[$index]) {
            throw "Capture does not have the PNG signature: $LiteralPath"
        }
    }
    $width = ([uint32] $bytes[16] -shl 24) -bor
        ([uint32] $bytes[17] -shl 16) -bor
        ([uint32] $bytes[18] -shl 8) -bor
        [uint32] $bytes[19]
    $height = ([uint32] $bytes[20] -shl 24) -bor
        ([uint32] $bytes[21] -shl 16) -bor
        ([uint32] $bytes[22] -shl 8) -bor
        [uint32] $bytes[23]
    return [PSCustomObject]@{
        Width = [int] $width
        Height = [int] $height
    }
}

$identity = Invoke-PublicViewCall `
    -ObjectPath $heroLibraryObjectPath `
    -FunctionName 'ValidateIstanaPublicViewHeroV5RemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory } `
    -TimeoutSeconds 30
if ($identity.ReturnValue -ne $true) {
    throw "Hero-v5 Remote Control project verification failed: $($identity.OutReport)"
}

$initialReadiness = Get-StableHeroV5Readiness
$cameraSpecs = @(
    [PSCustomObject]@{
        Preset = 'HERO_FRONT'
        Slug = 'hero_front'
        View = 'Transient centered whole-hero façade view'
    },
    [PSCustomObject]@{
        Preset = 'HERO_OBLIQUE_RIGHT'
        Slug = 'hero_oblique_right'
        View = 'Transient whole-hero right-oblique view'
    },
    [PSCustomObject]@{
        Preset = 'HERO_OBLIQUE_LEFT'
        Slug = 'hero_oblique_left'
        View = 'Transient whole-hero left-oblique view'
    },
    [PSCustomObject]@{
        Preset = 'HERO_FACADE_MACRO'
        Slug = 'hero_facade_macro'
        View = 'Transient intentional detail-only central-façade crop'
    },
    [PSCustomObject]@{
        Preset = 'HERO_GROUND_DETAIL'
        Slug = 'hero_ground_detail'
        View = 'Transient qualitative ground-level context at 1.70 m human eye height'
    },
    [PSCustomObject]@{
        Preset = 'HERO_MATERIAL_DETAIL'
        Slug = 'hero_material_detail'
        View = 'Transient qualitative central arch/door/material proof at 1.70 m eye height, outside the stair and 17.20 m from the public face'
    },
    [PSCustomObject]@{
        Preset = 'HERO_ORBIT_RIGHT'
        Slug = 'hero_orbit_right'
        View = 'Transient right-orbit evidence view'
    },
    [PSCustomObject]@{
        Preset = 'HERO_ORBIT_LEFT'
        Slug = 'hero_orbit_left'
        View = 'Transient left-orbit evidence view'
    }
)

$captures = [System.Collections.Generic.List[object]]::new()
foreach ($camera in $cameraSpecs) {
    $outputFileName = "ipv_v5_quality_$($camera.Slug)_$RunId.png"
    $outputPath = Join-Path $outputDirectory $outputFileName
    if (Test-Path -LiteralPath $outputPath) {
        throw "Refusing to overwrite existing hero-v5 preview: $outputPath"
    }

    $response = Invoke-PublicViewCall `
        -ObjectPath $runtimeLibraryObjectPath `
        -FunctionName 'CaptureIstanaPublicViewHeroV5QualityFrame' `
        -Parameters @{
            CameraPreset = $camera.Preset
            OutputFileName = $outputFileName
            TemporalWarmupFrames = $TemporalWarmupFrames
        } `
        -TimeoutSeconds 120
    if ($response.ReturnValue -ne $true) {
        throw "Hero-v5 PIE capture '$($camera.Preset)' failed: $($response.OutMessage)"
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(60)
    $previousLength = -1L
    $stableLengthPolls = 0
    do {
        Start-Sleep -Milliseconds 250
        if (Test-Path -LiteralPath $outputPath -PathType Leaf) {
            $length = (Get-Item -LiteralPath $outputPath).Length
            if ($length -gt 24 -and $length -eq $previousLength) {
                ++$stableLengthPolls
            }
            else {
                $stableLengthPolls = 0
            }
            $previousLength = $length
        }
    } while ($stableLengthPolls -lt 2 -and [DateTime]::UtcNow -lt $deadline)
    if ($stableLengthPolls -lt 2) {
        throw "Hero-v5 capture was scheduled but no stable PNG appeared within 60 seconds: $outputPath"
    }

    $dimensions = Get-PngDimensions -LiteralPath $outputPath
    if ($dimensions.Width -ne 3840 -or $dimensions.Height -ne 2160) {
        throw "Hero-v5 capture has unexpected dimensions $($dimensions.Width)x$($dimensions.Height): $outputPath"
    }
    $captures.Add([PSCustomObject]@{
        Preset = [string] $camera.Preset
        View = [string] $camera.View
        OutputPath = $outputPath
        Width = $dimensions.Width
        Height = $dimensions.Height
        Bytes = (Get-Item -LiteralPath $outputPath).Length
        Sha256 = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
        Message = [string] $response.OutMessage
    })
}

$finalReadiness = Get-StableHeroV5Readiness
$v1HashAfter = (Get-FileHash -LiteralPath $v1MapPath -Algorithm SHA256).Hash
$v2HashAfter = (Get-FileHash -LiteralPath $v2MapPath -Algorithm SHA256).Hash
$v3HashAfter = (Get-FileHash -LiteralPath $v3MapPath -Algorithm SHA256).Hash
$v4HashAfter = (Get-FileHash -LiteralPath $v4MapPath -Algorithm SHA256).Hash
$v5HashAfter = (Get-FileHash -LiteralPath $v5MapPath -Algorithm SHA256).Hash
$visualProfileHashAfter = (Get-FileHash -LiteralPath $visualProfilePath -Algorithm SHA256).Hash
$rendererConfigHashAfter = (Get-FileHash -LiteralPath $rendererConfigPath -Algorithm SHA256).Hash
if ($visualProfileHashAfter -ne $visualProfileHashBefore) {
    throw "Shared no-lidar visual profile changed during Hero-V5 capture; refusing acceptance. Before=$visualProfileHashBefore After=$visualProfileHashAfter"
}
if ($rendererConfigHashAfter -ne $rendererConfigHashBefore) {
    throw "DefaultEngine renderer/project configuration changed during Hero-V5 capture; refusing acceptance. Before=$rendererConfigHashBefore After=$rendererConfigHashAfter"
}
if ($v1HashAfter -ne $v1HashBefore) {
    throw "V1 map changed during hero-v5 capture; refusing acceptance. Before=$v1HashBefore After=$v1HashAfter"
}
if ($v2HashAfter -ne $v2HashBefore) {
    throw "V2 map changed during Hero-V5 capture; refusing acceptance. Before=$v2HashBefore After=$v2HashAfter"
}
if ($v3HashAfter -ne $v3HashBefore) {
    throw "V3 protected map changed during Hero-V5 capture; refusing acceptance. Before=$v3HashBefore After=$v3HashAfter"
}
if ($v4HashAfter -ne $v4HashBefore) {
    throw "V4 source map changed during Hero-V5 capture; refusing acceptance. Before=$v4HashBefore After=$v4HashAfter"
}
if ($v5HashAfter -ne $v5HashBefore) {
    throw "V5 map changed during transient capture; refusing acceptance. Before=$v5HashBefore After=$v5HashAfter"
}

[PSCustomObject]@{
    Operation = 'CaptureIstanaPublicViewHeroV5QualityPreviews'
    Succeeded = $true
    DestinationMap = $destinationMap
    RunId = $RunId
    OutputDirectory = $outputDirectory
    CaptureCount = $captures.Count
    Resolution = '3840x2160'
    TemporalWarmupFrames = $TemporalWarmupFrames
    CaptureProfile = 'D65_6500K_MANUAL_EXPOSURE_TSR_200_PERCENT_HISTORY_TRANSIENT'
    ProjectIdentityVerified = $true
    VisualProfileSha256Before = $visualProfileHashBefore
    VisualProfileSha256After = $visualProfileHashAfter
    RendererConfigSha256Before = $rendererConfigHashBefore
    RendererConfigSha256After = $rendererConfigHashAfter
    V1MapSha256Before = $v1HashBefore
    V1MapSha256After = $v1HashAfter
    V2MapSha256Before = $v2HashBefore
    V2MapSha256After = $v2HashAfter
    V3MapSha256Before = $v3HashBefore
    V3MapSha256After = $v3HashAfter
    V4MapSha256Before = $v4HashBefore
    V4MapSha256After = $v4HashAfter
    V5MapSha256Before = $v5HashBefore
    V5MapSha256After = $v5HashAfter
    MapsUnchanged = $true
    SurroundingsModified = $false
    InitialPlayWorldReadiness = [string] $initialReadiness.OutReport
    FinalPlayWorldReadiness = [string] $finalReadiness.OutReport
    PrimaryCameraRestored = $true
    Captures = @($captures)
}
