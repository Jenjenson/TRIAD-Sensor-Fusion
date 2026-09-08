[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')]
    [string] $RunId,

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
$outputDirectory = Join-Path $projectDirectory 'Saved\TRIAD\IstanaPreviews\PublicViewHeroV2'
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$heroLibraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV2EditorLibrary'
$runtimeLibraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewEditorLibrary'
$destinationMap = '/Game/Maps/Istana_PublicView_Exterior_v2'
$v1MapPath = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v1.umap'
$v2MapPath = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v2.umap'
foreach ($mapPath in @($v1MapPath, $v2MapPath)) {
    if (-not (Test-Path -LiteralPath $mapPath -PathType Leaf)) {
        throw "Required public-view map is missing: $mapPath"
    }
}
$v1HashBefore = (Get-FileHash -LiteralPath $v1MapPath -Algorithm SHA256).Hash
$v2HashBefore = (Get-FileHash -LiteralPath $v2MapPath -Algorithm SHA256).Hash

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

function Get-StableHeroV2Readiness {
    $lastReadiness = $null
    for ($poll = 1; $poll -le 3; ++$poll) {
        $lastReadiness = Invoke-PublicViewCall `
            -ObjectPath $runtimeLibraryObjectPath `
            -FunctionName 'ValidateIstanaPublicViewHeroV2PlayWorldReadiness' `
            -TimeoutSeconds 60
        if ($lastReadiness.ReturnValue -ne $true) {
            throw "Hero-v2 PIE readiness failed on stable poll $poll/3: $($lastReadiness.OutReport)"
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
    -FunctionName 'ValidateIstanaPublicViewHeroV2RemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory } `
    -TimeoutSeconds 30
if ($identity.ReturnValue -ne $true) {
    throw "Hero-v2 Remote Control project verification failed: $($identity.OutReport)"
}

$initialReadiness = Get-StableHeroV2Readiness
$cameraSpecs = @(
    [PSCustomObject]@{
        Preset = 'HERO_FRONT_CLOSE'
        Slug = 'hero_front_close'
        View = 'Transient centered whole-hero façade view'
    },
    [PSCustomObject]@{
        Preset = 'HERO_FRONT_OBLIQUE_CLOSE'
        Slug = 'hero_front_oblique_close'
        View = 'Transient whole-hero front-oblique view'
    },
    [PSCustomObject]@{
        Preset = 'HERO_FACADE_MACRO'
        Slug = 'hero_facade_macro'
        View = 'Transient intentional detail-only central-façade crop'
    }
)

$captures = [System.Collections.Generic.List[object]]::new()
foreach ($camera in $cameraSpecs) {
    $outputFileName = "ipv_v2_play_$($camera.Slug)_$RunId.png"
    $outputPath = Join-Path $outputDirectory $outputFileName
    if (Test-Path -LiteralPath $outputPath) {
        throw "Refusing to overwrite existing hero-v2 preview: $outputPath"
    }

    $response = Invoke-PublicViewCall `
        -ObjectPath $runtimeLibraryObjectPath `
        -FunctionName 'CaptureIstanaPublicViewHeroV2PlayCamera' `
        -Parameters @{
            CameraPreset = $camera.Preset
            OutputFileName = $outputFileName
        } `
        -TimeoutSeconds 120
    if ($response.ReturnValue -ne $true) {
        throw "Hero-v2 PIE capture '$($camera.Preset)' failed: $($response.OutMessage)"
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
        throw "Hero-v2 capture was scheduled but no stable PNG appeared within 60 seconds: $outputPath"
    }

    $dimensions = Get-PngDimensions -LiteralPath $outputPath
    if ($dimensions.Width -ne 3840 -or $dimensions.Height -ne 2160) {
        throw "Hero-v2 capture has unexpected dimensions $($dimensions.Width)x$($dimensions.Height): $outputPath"
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

$finalReadiness = Get-StableHeroV2Readiness
$v1HashAfter = (Get-FileHash -LiteralPath $v1MapPath -Algorithm SHA256).Hash
$v2HashAfter = (Get-FileHash -LiteralPath $v2MapPath -Algorithm SHA256).Hash
if ($v1HashAfter -ne $v1HashBefore) {
    throw "V1 map changed during hero-v2 capture; refusing acceptance. Before=$v1HashBefore After=$v1HashAfter"
}
if ($v2HashAfter -ne $v2HashBefore) {
    throw "V2 map changed during transient capture; refusing acceptance. Before=$v2HashBefore After=$v2HashAfter"
}

[PSCustomObject]@{
    Operation = 'CaptureIstanaPublicViewHeroV2PlayPreviews'
    Succeeded = $true
    DestinationMap = $destinationMap
    RunId = $RunId
    OutputDirectory = $outputDirectory
    CaptureCount = $captures.Count
    Resolution = '3840x2160'
    ProjectIdentityVerified = $true
    V1MapSha256Before = $v1HashBefore
    V1MapSha256After = $v1HashAfter
    V2MapSha256Before = $v2HashBefore
    V2MapSha256After = $v2HashAfter
    MapsUnchanged = $true
    SurroundingsModified = $false
    InitialPlayWorldReadiness = [string] $initialReadiness.OutReport
    FinalPlayWorldReadiness = [string] $finalReadiness.OutReport
    PrimaryCameraRestored = $true
    Captures = @($captures)
}
