[CmdletBinding()]
param(
    [switch] $Stop,

    [ValidateRange(10, 900)]
    [int] $ReadinessWaitSeconds = 180,

    [ValidateRange(5, 60)]
    [int] $QuiesceDrainSeconds = 15,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expectedSettingsSha256 = '6120769876168fe972f11950b72550539ee1fa2ca3cd7e0fab2d0d1a2eabe64c'
$expectedSettingsBytes = 1022

function Get-RequiredJsonProperty {
    param(
        [Parameter(Mandatory = $true)] $Object,
        [Parameter(Mandatory = $true)] [string] $Name
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "Visual-acceptance settings are missing required property '$Name'."
    }
    return $property.Value
}

function Assert-ExactHeroV5EditorCommandLine {
    param(
        [string] $CommandLine,
        [string] $ProjectFile,
        [string] $SettingsPath
    )

    $projectPattern = '(?i)(?:^|\s)"?' + [regex]::Escape($ProjectFile) + '"?(?:\s|$)'
    $anyProjectPattern = '(?i)(?:^|\s)(?:"[^"]+\.uproject"|[^\s"]+\.uproject)(?:\s|$)'
    $settingsPattern = '(?i)(?:^|\s)-settings="?' + [regex]::Escape($SettingsPath) + '"?(?:\s|$)'
    # FParse::Value searches for the substring, so reject prefix variants such
    # as --settings= and -foo-settings= as additional settings arguments.
    $anySettingsPattern = '(?i)-settings='
    $visualFlagPattern = '(?i)(?:^|\s)-TRIADIstanaVisualAcceptance(?:\s|$)'
    if ([regex]::Matches($CommandLine, $projectPattern).Count -ne 1 -or
        [regex]::Matches($CommandLine, $anyProjectPattern).Count -ne 1 -or
        [regex]::Matches($CommandLine, $settingsPattern).Count -ne 1 -or
        [regex]::Matches($CommandLine, $anySettingsPattern).Count -ne 1 -or
        [regex]::Matches($CommandLine, $visualFlagPattern).Count -ne 1) {
        throw 'Hero-V5 PIE start/stop requires exactly one TRIAD .uproject argument, one canonical -settings=<Project>\Config\IstanaPublicViewHeroV3VisualAcceptance.settings.json argument, and one -TRIADIstanaVisualAcceptance flag.'
    }
    return $true
}

function Assert-ExactHeroV5VisualAcceptanceProfile {
    param(
        [string] $ProjectDirectory,
        [string] $ProjectFile
    )

    $settingsPath = Join-Path $ProjectDirectory 'Config\IstanaPublicViewHeroV3VisualAcceptance.settings.json'
    if (-not (Test-Path -LiteralPath $settingsPath -PathType Leaf)) {
        throw "The exact project-owned visual-acceptance settings are missing: $settingsPath"
    }
    $item = Get-Item -LiteralPath $settingsPath
    $digest = (Get-FileHash -LiteralPath $settingsPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($item.Length -ne $expectedSettingsBytes -or $digest -cne $expectedSettingsSha256) {
        throw 'Hero-V5 PIE start/stop refuses a settings file other than the exact repository-owned shared Hero-V3 no-lidar/RPC-off profile.'
    }
    $text = Get-Content -LiteralPath $settingsPath -Raw
    $settings = $text | ConvertFrom-Json
    if ([regex]::Matches($text, '(?m)^\s*"RpcEnabled"\s*:').Count -ne 1 -or
        [regex]::Matches($text, '(?m)^\s*"EnableRpc"\s*:').Count -ne 1 -or
        (Get-RequiredJsonProperty $settings 'TRIADProfile') -cne 'ISTANA_PUBLIC_VIEW_HERO_V3_VISUAL_ACCEPTANCE_ONLY' -or
        [bool](Get-RequiredJsonProperty $settings 'TRIADProductionSensorProfile') -or
        (Get-RequiredJsonProperty $settings 'SimMode') -cne 'ComputerVision' -or
        [bool](Get-RequiredJsonProperty $settings 'RpcEnabled') -or
        [bool](Get-RequiredJsonProperty $settings 'EnableRpc') -or
        (Get-RequiredJsonProperty $settings 'LocalHostIp') -cne '127.0.0.1' -or
        [int](Get-RequiredJsonProperty $settings 'ApiServerPort') -ne 41451 -or
        $null -ne $settings.PSObject.Properties['DefaultSensors'] -or
        $text -match '(?i)"SensorType"\s*:\s*6' -or
        $text -match '(?i)"[^"]*lidar[^"]*"\s*:') {
        throw 'Hero-V5 PIE start/stop requires ComputerVision, RpcEnabled=false and deployed-AirSim compatibility EnableRpc=false exactly once each, loopback API address, no DefaultSensors, and no lidar declaration.'
    }
    $vehicles = Get-RequiredJsonProperty $settings 'Vehicles'
    $vehicleProperties = @($vehicles.PSObject.Properties)
    if ($vehicleProperties.Count -ne 1) {
        throw 'Hero-V5 PIE start/stop requires exactly one visual-acceptance vehicle.'
    }
    $vehicle = $vehicleProperties[0].Value
    $sensors = Get-RequiredJsonProperty $vehicle 'Sensors'
    if ((Get-RequiredJsonProperty $vehicle 'VehicleType') -cne 'ComputerVision' -or
        -not [bool](Get-RequiredJsonProperty $vehicle 'AutoCreate') -or
        @($sensors.PSObject.Properties).Count -ne 0) {
        throw 'Hero-V5 PIE start/stop requires one auto-created ComputerVision vehicle with an empty Sensors object.'
    }

    $editors = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'")
    if ($editors.Count -ne 1) {
        throw "Hero-V5 PIE start/stop requires exactly one guarded Unreal Editor process; found $($editors.Count)."
    }
    $commandLine = [string] $editors[0].CommandLine
    if ([string]::IsNullOrWhiteSpace($commandLine)) {
        throw "Cannot prove the command line for Unreal Editor PID $($editors[0].ProcessId)."
    }
    Assert-ExactHeroV5EditorCommandLine `
        -CommandLine $commandLine `
        -ProjectFile $ProjectFile `
        -SettingsPath $settingsPath | Out-Null
    return [PSCustomObject]@{
        SettingsPath = $settingsPath
        SettingsSha256 = $digest
        EditorProcessId = $editors[0].ProcessId
        CommandLineVerified = $true
        NoLidarProfileVerified = $true
    }
}

$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectDirectory = if (Test-Path -LiteralPath $resolvedProject -PathType Leaf) {
    if ([IO.Path]::GetExtension($resolvedProject) -ine '.uproject') {
        throw "ProjectPath file is not an Unreal project: $resolvedProject"
    }
    Split-Path -Parent $resolvedProject
}
else {
    $resolvedProject
}
$projectDirectory = [IO.Path]::GetFullPath($projectDirectory).TrimEnd('\')
$projectFile = Join-Path $projectDirectory 'TRIAD.uproject'
if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "The exact TRIAD project descriptor is missing: $projectFile"
}
if ($RemoteControlUrl.Scheme -cne 'http' -or
    $RemoteControlUrl.Host -cne '127.0.0.1' -or
    $RemoteControlUrl.Port -ne 30010 -or
    $RemoteControlUrl.AbsolutePath -cne '/' -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Query) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Fragment) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.UserInfo)) {
    throw 'Hero-V5 PIE Remote Control must use exact loopback endpoint http://127.0.0.1:30010.'
}
$visualProfile = Assert-ExactHeroV5VisualAcceptanceProfile `
    -ProjectDirectory $projectDirectory `
    -ProjectFile $projectFile
$endpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$heroLibraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV5EditorLibrary'
$runtimeLibraryObjectPath = $heroLibraryObjectPath
$levelEditorObjectPath = '/Script/LevelEditor.Default__LevelEditorSubsystem'
$destinationMap = '/Game/Maps/Istana_PublicView_Exterior_v5'

function Invoke-RemoteObjectCall {
    param(
        [Parameter(Mandatory = $true)]
        [string] $ObjectPath,

        [Parameter(Mandatory = $true)]
        [string] $FunctionName,

        [hashtable] $Parameters = @{},

        [ValidateRange(1, 900)]
        [int] $TimeoutSeconds = 30
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

$identity = Invoke-RemoteObjectCall `
    -ObjectPath $heroLibraryObjectPath `
    -FunctionName 'ValidateIstanaPublicViewHeroV5RemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory }
if ($identity.ReturnValue -ne $true) {
    throw "Hero-v5 Remote Control project verification failed: $($identity.OutReport)"
}

$initialState = Invoke-RemoteObjectCall `
    -ObjectPath $levelEditorObjectPath `
    -FunctionName 'IsInPlayInEditor' `
    -TimeoutSeconds 10
$initiallyInPlay = [bool] $initialState.ReturnValue

if ($Stop) {
    if (-not $initiallyInPlay) {
        return [PSCustomObject]@{
            Operation = 'EditorRequestEndPlay'
            DestinationMap = $destinationMap
            PlayInEditor = $false
            ProjectIdentityVerified = $true
            StopWasNecessary = $false
            VisualAcceptanceProfileVerified = $true
            SettingsSha256 = $visualProfile.SettingsSha256
        }
    }

    $quiesce = Invoke-RemoteObjectCall `
        -ObjectPath $runtimeLibraryObjectPath `
        -FunctionName 'QuiesceIstanaPublicViewHeroV5PlayWorldForStop' `
        -TimeoutSeconds 30
    if ($quiesce.ReturnValue -ne $true) {
        throw "Refusing unsafe EditorRequestEndPlay because hero-v5 AirSim quiescence was not verified. PIE was intentionally left running. $($quiesce.OutMessage)"
    }

    # The verified pause prevents new AirSim sensor work while this bounded
    # drain lets an already-running update release its Unreal actor pointers.
    Start-Sleep -Seconds $QuiesceDrainSeconds

    Invoke-RemoteObjectCall `
        -ObjectPath $levelEditorObjectPath `
        -FunctionName 'EditorRequestEndPlay' `
        -TimeoutSeconds 60 | Out-Null

    $stopDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $inPlay = $true
    do {
        Start-Sleep -Milliseconds 500
        $state = Invoke-RemoteObjectCall `
            -ObjectPath $levelEditorObjectPath `
            -FunctionName 'IsInPlayInEditor' `
            -TimeoutSeconds 10
        $inPlay = [bool] $state.ReturnValue
    } while ($inPlay -and [DateTime]::UtcNow -lt $stopDeadline)
    if ($inPlay) {
        throw 'EditorRequestEndPlay did not stop hero-v5 public-view PIE within 30 seconds.'
    }

    return [PSCustomObject]@{
        Operation = 'EditorRequestEndPlay'
        DestinationMap = $destinationMap
        PlayInEditor = $false
        ProjectIdentityVerified = $true
        StopWasNecessary = $true
        AirSimQuiescenceVerified = $true
        QuiesceDrainSeconds = $QuiesceDrainSeconds
        QuiesceReport = [string] $quiesce.OutMessage
        VisualAcceptanceProfileVerified = $true
        SettingsSha256 = $visualProfile.SettingsSha256
    }
}

$mapValidation = Invoke-RemoteObjectCall `
    -ObjectPath $heroLibraryObjectPath `
    -FunctionName 'ValidateIstanaPublicViewHeroV5Map' `
    -TimeoutSeconds 900
if ($mapValidation.ReturnValue -ne $true) {
    throw "PIE start requires the exact validated Hero-V5 map with V4 surroundings preserved: $($mapValidation.OutReport)"
}

$sessionWasAlreadyRunning = $initiallyInPlay
if (-not $initiallyInPlay) {
    Invoke-RemoteObjectCall `
        -ObjectPath $levelEditorObjectPath `
        -FunctionName 'EditorRequestBeginPlay' `
        -TimeoutSeconds 60 | Out-Null

    $startDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $inPlay = $false
    do {
        Start-Sleep -Milliseconds 500
        $state = Invoke-RemoteObjectCall `
            -ObjectPath $levelEditorObjectPath `
            -FunctionName 'IsInPlayInEditor' `
            -TimeoutSeconds 10
        $inPlay = [bool] $state.ReturnValue
    } while (-not $inPlay -and [DateTime]::UtcNow -lt $startDeadline)
    if (-not $inPlay) {
        throw 'EditorRequestBeginPlay did not enter hero-v5 public-view PIE within 30 seconds.'
    }
}

$requiredStableReadinessPolls = 3
$stableReadinessPolls = 0
$readinessDeadline = [DateTime]::UtcNow.AddSeconds($ReadinessWaitSeconds)
$playReadiness = $null
$readinessPollError = $null
do {
    Start-Sleep -Seconds 1
    try {
        $playReadiness = Invoke-RemoteObjectCall `
            -ObjectPath $runtimeLibraryObjectPath `
            -FunctionName 'ValidateIstanaPublicViewHeroV5PlayWorldReadiness' `
            -TimeoutSeconds 60
        if ($playReadiness.ReturnValue -eq $true) {
            ++$stableReadinessPolls
        }
        else {
            $stableReadinessPolls = 0
        }
    }
    catch {
        $readinessPollError = $_.Exception.Message
        break
    }
} while ($stableReadinessPolls -lt $requiredStableReadinessPolls -and
    [DateTime]::UtcNow -lt $readinessDeadline)

if (-not $playReadiness -or $playReadiness.ReturnValue -ne $true -or
    $stableReadinessPolls -lt $requiredStableReadinessPolls) {
    $lastReport = if ($readinessPollError) {
        "Remote Control poll failed: $readinessPollError"
    }
    elseif ($playReadiness) {
        [string] $playReadiness.OutReport
    }
    else {
        'No readiness response was returned.'
    }
    throw "Hero-v5 PIE readiness did not pass for $requiredStableReadinessPolls consecutive polls within $ReadinessWaitSeconds seconds. PIE was intentionally left running; retry readiness or use this script with -Stop for guarded teardown. Last report: $lastReport"
}

[PSCustomObject]@{
    Operation = if ($sessionWasAlreadyRunning) {
        'ValidateExistingHeroV5PublicViewPlaySession'
    }
    else {
        'EditorRequestBeginPlay'
    }
    DestinationMap = $destinationMap
    PlayInEditor = $true
    SessionWasAlreadyRunning = $sessionWasAlreadyRunning
    ProjectIdentityVerified = $true
    EditorMapValidation = 'ValidateIstanaPublicViewHeroV5Map'
    PlayWorldReadiness = 'ValidateIstanaPublicViewHeroV5PlayWorldReadiness'
    StableReadinessPolls = $stableReadinessPolls
    RequiredStableReadinessPolls = $requiredStableReadinessPolls
    ReadinessWaitSeconds = $ReadinessWaitSeconds
    Report = [string] $playReadiness.OutReport
    VisualAcceptanceProfileVerified = $true
    SettingsSha256 = $visualProfile.SettingsSha256
    EditorProcessId = $visualProfile.EditorProcessId
}
