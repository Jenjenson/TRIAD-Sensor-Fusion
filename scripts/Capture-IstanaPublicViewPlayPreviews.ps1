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
$outputDirectory = Join-Path $projectDirectory 'Saved\TRIAD\IstanaPreviews\PublicView'
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$libraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewEditorLibrary'

function Invoke-PublicViewCall {
    param(
        [Parameter(Mandatory = $true)]
        [string] $FunctionName,

        [hashtable] $Parameters = @{},

        [ValidateRange(1, 120)]
        [int] $TimeoutSeconds = 60
    )

    $body = @{
        objectPath = $libraryObjectPath
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

function Get-StablePublicViewReadiness {
    $lastReadiness = $null
    for ($poll = 1; $poll -le 3; ++$poll) {
        $lastReadiness = Invoke-PublicViewCall `
            -FunctionName 'ValidateIstanaPublicViewPlayWorldReadiness' `
            -TimeoutSeconds 30
        if ($lastReadiness.ReturnValue -ne $true) {
            throw "Public-view PIE readiness failed on stable poll $poll/3: $($lastReadiness.OutReport)"
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
    -FunctionName 'ValidateIstanaPublicViewRemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory } `
    -TimeoutSeconds 30
if ($identity.ReturnValue -ne $true) {
    throw "Remote Control project verification failed: $($identity.OutReport)"
}

$initialReadiness = Get-StablePublicViewReadiness
$cameraSpecs = @(
    [PSCustomObject]@{
        Preset = 'CEREMONIAL_FRONT'
        Slug = 'ceremonial_front'
        Tag = 'TRIADIstanaPublicViewCamera_Primary'
    },
    [PSCustomObject]@{
        Preset = 'FRONT_OBLIQUE'
        Slug = 'front_oblique'
        Tag = 'TRIADIstanaPublicViewCamera_FrontOblique'
    },
    [PSCustomObject]@{
        Preset = 'ARCADE'
        Slug = 'arcade'
        Tag = 'TRIADIstanaPublicViewCamera_Arcade'
    },
    [PSCustomObject]@{
        Preset = 'TOWER'
        Slug = 'tower'
        Tag = 'TRIADIstanaPublicViewCamera_Tower'
    }
)

$captures = [System.Collections.Generic.List[object]]::new()
foreach ($camera in $cameraSpecs) {
    $outputFileName = "ipv_play_$($camera.Slug)_$RunId.png"
    $outputPath = Join-Path $outputDirectory $outputFileName
    if (Test-Path -LiteralPath $outputPath) {
        throw "Refusing to overwrite existing public-view preview: $outputPath"
    }

    $response = Invoke-PublicViewCall `
        -FunctionName 'CaptureIstanaPublicViewPlayCamera' `
        -Parameters @{
            CameraPreset = $camera.Preset
            OutputFileName = $outputFileName
        } `
        -TimeoutSeconds 60
    if ($response.ReturnValue -ne $true) {
        throw "Public-view PIE capture '$($camera.Preset)' failed: $($response.OutMessage)"
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(30)
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
        throw "Public-view capture was scheduled but no stable PNG appeared within 30 seconds: $outputPath"
    }

    $dimensions = Get-PngDimensions -LiteralPath $outputPath
    if ($dimensions.Width -ne 1920 -or $dimensions.Height -ne 1080) {
        throw "Public-view capture has unexpected dimensions $($dimensions.Width)x$($dimensions.Height): $outputPath"
    }
    $captures.Add([PSCustomObject]@{
        Preset = [string] $camera.Preset
        CameraTag = [string] $camera.Tag
        OutputPath = $outputPath
        Width = $dimensions.Width
        Height = $dimensions.Height
        Bytes = (Get-Item -LiteralPath $outputPath).Length
        Sha256 = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
        Message = [string] $response.OutMessage
    })
}

$finalReadiness = Get-StablePublicViewReadiness
[PSCustomObject]@{
    Operation = 'CaptureIstanaPublicViewPlayPreviews'
    Succeeded = $true
    DestinationMap = '/Game/Maps/Istana_PublicView_Exterior_v1'
    RunId = $RunId
    OutputDirectory = $outputDirectory
    CaptureCount = $captures.Count
    Resolution = '1920x1080'
    ProjectIdentityVerified = $true
    InitialPlayWorldReadiness = [string] $initialReadiness.OutReport
    FinalPlayWorldReadiness = [string] $finalReadiness.OutReport
    PrimaryCameraRestored = $true
    Captures = @($captures)
}
