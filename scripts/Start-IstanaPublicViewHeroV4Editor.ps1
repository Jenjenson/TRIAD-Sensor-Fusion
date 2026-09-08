[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $ProjectPath,

    [ValidateNotNullOrEmpty()]
    [string] $UnrealEditorPath = 'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe',

    [uri] $RemoteControlUrl = 'http://127.0.0.1:30010',

    [ValidateRange(30, 900)]
    [int] $ReadinessTimeoutSeconds = 300,

    [switch] $ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expectedSettingsSha256 = '6120769876168fe972f11950b72550539ee1fa2ca3cd7e0fab2d0d1a2eabe64c'
$expectedSettingsBytes = 1022
$heroV4LibraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV4EditorLibrary'
$heroV3LibraryObjectPath = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaPublicViewHeroV3EditorLibrary'

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

function Assert-ExactVisualAcceptanceSettings {
    param([string] $SettingsPath)

    $item = Get-Item -LiteralPath $SettingsPath
    $digest = (Get-FileHash -LiteralPath $SettingsPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($item.Length -ne $expectedSettingsBytes -or $digest -cne $expectedSettingsSha256) {
        throw "The project Hero-V4 launch is not using the exact shared Hero-V3 no-lidar/RPC-off visual-acceptance profile: $SettingsPath"
    }
    $text = Get-Content -LiteralPath $SettingsPath -Raw
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
        throw 'Hero-V4 visual acceptance requires the exact non-production ComputerVision profile, RpcEnabled=false and deployed-AirSim compatibility EnableRpc=false exactly once each, loopback API address, and no lidar.'
    }
    $vehicles = Get-RequiredJsonProperty $settings 'Vehicles'
    $vehicleProperties = @($vehicles.PSObject.Properties)
    if ($vehicleProperties.Count -ne 1) {
        throw 'Hero-V4 visual acceptance requires exactly one ComputerVision camera vehicle.'
    }
    $vehicle = $vehicleProperties[0].Value
    $sensors = Get-RequiredJsonProperty $vehicle 'Sensors'
    if ((Get-RequiredJsonProperty $vehicle 'VehicleType') -cne 'ComputerVision' -or
        -not [bool](Get-RequiredJsonProperty $vehicle 'AutoCreate') -or
        @($sensors.PSObject.Properties).Count -ne 0) {
        throw 'The sole Hero-V4 visual-acceptance vehicle must be auto-created ComputerVision with an empty Sensors object.'
    }
    return $digest
}

function Assert-ExactHeroV4EditorCommandLine {
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
        throw 'The launched Hero-V4 editor command line does not contain exactly one TRIAD project, one canonical shared Hero-V3 -settings argument, and one visual-acceptance flag. Close it manually before retrying.'
    }
    return $true
}

function Invoke-RemoteObjectCall {
    param(
        [uri] $Endpoint,
        [string] $ObjectPath,
        [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [int] $TimeoutSeconds = 30
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

if ($RemoteControlUrl.Scheme -cne 'http' -or
    $RemoteControlUrl.Host -cne '127.0.0.1' -or
    $RemoteControlUrl.Port -ne 30010 -or
    $RemoteControlUrl.AbsolutePath -cne '/' -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Query) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.Fragment) -or
    -not [string]::IsNullOrEmpty($RemoteControlUrl.UserInfo)) {
    throw 'Hero-V4 Remote Control must use exact loopback endpoint http://127.0.0.1:30010.'
}

$resolvedInput = (Resolve-Path -LiteralPath $ProjectPath).Path
if (Test-Path -LiteralPath $resolvedInput -PathType Leaf) {
    if ([IO.Path]::GetExtension($resolvedInput) -ine '.uproject') {
        throw "ProjectPath must be a project directory or .uproject file: $resolvedInput"
    }
    $uprojectPath = $resolvedInput
    $projectDirectory = Split-Path -Parent $resolvedInput
}
else {
    $projectDirectory = $resolvedInput.TrimEnd('\')
    $uprojectPath = Join-Path $projectDirectory 'TRIAD.uproject'
}
if (-not (Test-Path -LiteralPath $uprojectPath -PathType Leaf) -or
    [IO.Path]::GetFileName($uprojectPath) -ine 'TRIAD.uproject') {
    throw "The exact TRIAD project descriptor is missing: $uprojectPath"
}
$projectDirectory = [IO.Path]::GetFullPath($projectDirectory).TrimEnd('\')
$uprojectPath = [IO.Path]::GetFullPath($uprojectPath)

$settingsPath = Join-Path $projectDirectory 'Config\IstanaPublicViewHeroV3VisualAcceptance.settings.json'
$v3MapFile = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v3.umap'
$v4MapFile = Join-Path $projectDirectory 'Content\Maps\Istana_PublicView_Exterior_v4.umap'
$heroV4Header = Join-Path $projectDirectory 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaPublicViewHeroV4EditorLibrary.h'
foreach ($requiredPath in @($settingsPath, $v3MapFile, $heroV4Header)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required Hero-V4 editor-launch input is missing: $requiredPath"
    }
}
$settingsSha256 = Assert-ExactVisualAcceptanceSettings -SettingsPath $settingsPath

$runningEditors = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'")
if ($runningEditors.Count -ne 0) {
    $pids = @($runningEditors | ForEach-Object ProcessId) -join ', '
    throw "Close every Unreal Editor before the guarded Hero-V4 launch. Running PID(s): $pids"
}

$v4MapPresent = Test-Path -LiteralPath $v4MapFile -PathType Leaf
$selectedMapFile = if ($v4MapPresent) { $v4MapFile } else { $v3MapFile }
$selectedMapPackage = if ($v4MapPresent) {
    '/Game/Maps/Istana_PublicView_Exterior_v4'
}
else {
    '/Game/Maps/Istana_PublicView_Exterior_v3'
}
$launchMode = if ($v4MapPresent) {
    'VALIDATE_EXISTING_HERO_V4'
}
else {
    'BOOTSTRAP_V3_FOR_HERO_V4_IMPORT_AND_MIGRATION'
}
$protectedHashesBefore = @{
    Project = (Get-FileHash -LiteralPath $uprojectPath -Algorithm SHA256).Hash
    Settings = (Get-FileHash -LiteralPath $settingsPath -Algorithm SHA256).Hash
    V3Map = (Get-FileHash -LiteralPath $v3MapFile -Algorithm SHA256).Hash
    SelectedMap = (Get-FileHash -LiteralPath $selectedMapFile -Algorithm SHA256).Hash
}

$preflight = [PSCustomObject]@{
    Operation = 'StartIstanaPublicViewHeroV4Editor'
    ValidationPassed = $true
    ValidateOnly = [bool] $ValidateOnly
    ProjectFile = $uprojectPath
    SelectedMap = $selectedMapPackage
    LaunchMode = $launchMode
    V4MapPresent = $v4MapPresent
    SettingsFile = $settingsPath
    SettingsSha256 = $settingsSha256
    RemoteControlUrl = 'http://127.0.0.1:30010'
    AirSimMode = 'ComputerVision'
    RpcEnabled = $false
    EnabledLidarSensors = 0
    ProductionSensorProfile = $false
}
if ($ValidateOnly) {
    return $preflight
}

$resolvedEditorPath = (Resolve-Path -LiteralPath $UnrealEditorPath).Path
if (-not (Test-Path -LiteralPath $resolvedEditorPath -PathType Leaf) -or
    [IO.Path]::GetFileName($resolvedEditorPath) -ine 'UnrealEditor.exe') {
    throw "The exact Unreal Editor executable is missing: $resolvedEditorPath"
}

$arguments = @(
    "`"$uprojectPath`"",
    $selectedMapPackage,
    "-settings=`"$settingsPath`"",
    '-TRIADIstanaVisualAcceptance',
    '-RCWebControlEnable',
    '-ExecCmds="WebControl.StartServer"'
)
$process = Start-Process `
    -FilePath $resolvedEditorPath `
    -ArgumentList $arguments `
    -WindowStyle Normal `
    -PassThru

$launchedProcessInfo = Get-CimInstance Win32_Process -Filter "ProcessId=$($process.Id)"
if ($null -eq $launchedProcessInfo -or
    [string]::IsNullOrWhiteSpace([string] $launchedProcessInfo.CommandLine)) {
    throw "Cannot prove the actual command line for launched Unreal Editor PID $($process.Id). Close it manually before retrying."
}
Assert-ExactHeroV4EditorCommandLine `
    -CommandLine ([string] $launchedProcessInfo.CommandLine) `
    -ProjectFile $uprojectPath `
    -SettingsPath $settingsPath | Out-Null

$infoEndpoint = [uri]::new($RemoteControlUrl, '/remote/info')
$callEndpoint = [uri]::new($RemoteControlUrl, '/remote/object/call')
$deadline = [DateTime]::UtcNow.AddSeconds($ReadinessTimeoutSeconds)
$remoteInfo = $null
$lastReadinessError = $null
do {
    $process.Refresh()
    if ($process.HasExited) {
        throw "Unreal Editor PID $($process.Id) exited before Remote Control became ready (exit code $($process.ExitCode))."
    }
    try {
        $remoteInfo = Invoke-RestMethod -Uri $infoEndpoint -Method Get -TimeoutSec 2
    }
    catch {
        $lastReadinessError = $_.Exception.Message
        Start-Sleep -Seconds 1
    }
} while ($null -eq $remoteInfo -and [DateTime]::UtcNow -lt $deadline)
if ($null -eq $remoteInfo) {
    throw "Unreal Editor PID $($process.Id) did not expose exact loopback Remote Control within $ReadinessTimeoutSeconds seconds. Close it manually before retrying. Last error: $lastReadinessError"
}

$identity = Invoke-RemoteObjectCall `
    -Endpoint $callEndpoint `
    -ObjectPath $heroV4LibraryObjectPath `
    -FunctionName 'ValidateIstanaPublicViewHeroV4RemoteControlProject' `
    -Parameters @{ ExpectedProjectPath = $projectDirectory } `
    -TimeoutSeconds 30
if ($identity.ReturnValue -ne $true) {
    throw "Connected Remote Control belongs to the wrong project. No Hero-V4 operation ran. $($identity.OutReport)"
}

if ($v4MapPresent) {
    $mapValidation = Invoke-RemoteObjectCall `
        -Endpoint $callEndpoint `
        -ObjectPath $heroV4LibraryObjectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV4Map' `
        -TimeoutSeconds 900
}
else {
    $mapValidation = Invoke-RemoteObjectCall `
        -Endpoint $callEndpoint `
        -ObjectPath $heroV3LibraryObjectPath `
        -FunctionName 'ValidateIstanaPublicViewHeroV3Map' `
        -TimeoutSeconds 900
}
if ($mapValidation.ReturnValue -ne $true) {
    throw "The launched editor is not on the exact required map. No Hero-V4 mutation ran. $($mapValidation.OutReport)"
}

$protectedHashesAfter = @{
    Project = (Get-FileHash -LiteralPath $uprojectPath -Algorithm SHA256).Hash
    Settings = (Get-FileHash -LiteralPath $settingsPath -Algorithm SHA256).Hash
    V3Map = (Get-FileHash -LiteralPath $v3MapFile -Algorithm SHA256).Hash
    SelectedMap = (Get-FileHash -LiteralPath $selectedMapFile -Algorithm SHA256).Hash
}
foreach ($key in $protectedHashesBefore.Keys) {
    if ($protectedHashesBefore[$key] -cne $protectedHashesAfter[$key]) {
        throw "Guarded Hero-V4 editor launch changed protected disk bytes: $key"
    }
}

$preflight | Add-Member -NotePropertyName ProcessId -NotePropertyValue $process.Id
$preflight | Add-Member -NotePropertyName Launched -NotePropertyValue $true
$preflight | Add-Member -NotePropertyName RemoteControlReady -NotePropertyValue $true
$preflight | Add-Member -NotePropertyName ProjectIdentityVerified -NotePropertyValue $true
$preflight | Add-Member -NotePropertyName ActualCommandLineVerified -NotePropertyValue $true
$currentMapValidation = if ($v4MapPresent) {
    'ValidateIstanaPublicViewHeroV4Map'
}
else {
    'ValidateIstanaPublicViewHeroV3Map'
}
$preflight | Add-Member -NotePropertyName CurrentMapValidation -NotePropertyValue $currentMapValidation
$preflight | Add-Member -NotePropertyName ProtectedDiskBytesUnchanged -NotePropertyValue $true
$preflight
