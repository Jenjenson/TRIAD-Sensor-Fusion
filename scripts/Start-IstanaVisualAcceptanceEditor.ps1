[CmdletBinding()]
param(
    [string] $ProjectPath = 'D:\triad\TRIAD',
    [string] $UnrealEditorPath = 'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe',
    [string] $SettingsPath = '',
    [switch] $ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

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

$resolvedProject = (Resolve-Path -LiteralPath $ProjectPath).Path
if (Test-Path -LiteralPath $resolvedProject -PathType Leaf) {
    if ([IO.Path]::GetExtension($resolvedProject) -ine '.uproject') {
        throw "ProjectPath must be a project directory or .uproject file: $resolvedProject"
    }
    $uprojectPath = $resolvedProject
    $projectDirectory = Split-Path -Parent $resolvedProject
}
else {
    $projectDirectory = $resolvedProject
    $uprojectPath = Join-Path $projectDirectory 'TRIAD.uproject'
}
if (-not (Test-Path -LiteralPath $uprojectPath -PathType Leaf)) {
    throw "The exact TRIAD project descriptor is missing: $uprojectPath"
}

$v2MapFile = Join-Path $projectDirectory 'Content\Maps\Istana_1km_Context_v2.umap'
if (-not (Test-Path -LiteralPath $v2MapFile -PathType Leaf)) {
    throw "The exact visual-acceptance map is missing: $v2MapFile"
}

$candidateSettingsPath = if ([string]::IsNullOrWhiteSpace($SettingsPath)) {
    Join-Path $projectDirectory 'Config\IstanaVisualAcceptance.settings.json'
}
else {
    $SettingsPath
}
$resolvedSettingsPath = (Resolve-Path -LiteralPath $candidateSettingsPath).Path
if (-not (Test-Path -LiteralPath $resolvedSettingsPath -PathType Leaf)) {
    throw "Visual-acceptance settings are missing: $resolvedSettingsPath"
}

$settingsText = Get-Content -LiteralPath $resolvedSettingsPath -Raw
$settings = $settingsText | ConvertFrom-Json
if ((Get-RequiredJsonProperty $settings 'TRIADProfile') -cne 'VISUAL_ACCEPTANCE_ONLY') {
    throw 'Settings are not marked as the exact VISUAL_ACCEPTANCE_ONLY profile.'
}
if ([bool](Get-RequiredJsonProperty $settings 'TRIADProductionSensorProfile')) {
    throw 'Visual acceptance must never use a profile marked as a production sensor profile.'
}
if ((Get-RequiredJsonProperty $settings 'SimMode') -cne 'ComputerVision') {
    throw 'Visual acceptance requires AirSim SimMode=ComputerVision.'
}
if ([bool](Get-RequiredJsonProperty $settings 'EnableRpc')) {
    throw 'Visual acceptance requires EnableRpc=false.'
}
if ($null -ne $settings.PSObject.Properties['DefaultSensors']) {
    throw 'Visual acceptance refuses a DefaultSensors collection.'
}
if ($settingsText -match '(?i)"SensorType"\s*:\s*6' -or
    $settingsText -match '(?i)"[^"]*lidar[^"]*"\s*:') {
    throw 'Visual acceptance refuses every lidar sensor declaration.'
}

$vehicles = Get-RequiredJsonProperty $settings 'Vehicles'
$vehicleProperties = @($vehicles.PSObject.Properties)
if ($vehicleProperties.Count -ne 1) {
    throw "Visual acceptance requires exactly one ComputerVision camera vehicle; found $($vehicleProperties.Count)."
}
$vehicle = $vehicleProperties[0].Value
if ((Get-RequiredJsonProperty $vehicle 'VehicleType') -cne 'ComputerVision' -or
    -not [bool](Get-RequiredJsonProperty $vehicle 'AutoCreate')) {
    throw 'The sole visual-acceptance vehicle must be an auto-created ComputerVision vehicle.'
}
$sensors = Get-RequiredJsonProperty $vehicle 'Sensors'
if (@($sensors.PSObject.Properties).Count -ne 0) {
    throw 'The visual-acceptance ComputerVision vehicle must have an empty Sensors object.'
}

$profileHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedSettingsPath).Hash
$mapHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $v2MapFile).Hash
$preflight = [PSCustomObject]@{
    Operation = 'StartIstanaVisualAcceptanceEditor'
    ValidationPassed = $true
    ValidateOnly = [bool] $ValidateOnly
    ProjectFile = $uprojectPath
    DestinationMap = '/Game/Maps/Istana_1km_Context_v2'
    DestinationMapSha256 = $mapHash
    SettingsFile = $resolvedSettingsPath
    SettingsSha256 = $profileHash
    AirSimMode = 'ComputerVision'
    RpcEnabled = $false
    EnabledLidarSensors = 0
    ProductionSensorProfile = $false
    RuntimeOnlyCesiumFrustumPolicy = $true
}
if ($ValidateOnly) {
    return $preflight
}

$resolvedEditorPath = (Resolve-Path -LiteralPath $UnrealEditorPath).Path
if (-not (Test-Path -LiteralPath $resolvedEditorPath -PathType Leaf)) {
    throw "Unreal Editor executable is missing: $resolvedEditorPath"
}

# Never create a second editor for the same project. This script deliberately
# does not stop, replace, or mutate any running process.
$normalizedProject = [IO.Path]::GetFullPath($uprojectPath)
$runningEditors = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'")
foreach ($editor in $runningEditors) {
    if ([string]::IsNullOrWhiteSpace([string] $editor.CommandLine)) {
        throw "Cannot prove project identity for running UnrealEditor PID $($editor.ProcessId); close it manually before launching visual acceptance."
    }
    if ($editor.CommandLine.IndexOf(
            $normalizedProject,
            [StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw "UnrealEditor PID $($editor.ProcessId) already has this project open. Close it manually, then launch once with the isolated visual profile."
    }
}

$arguments = @(
    "`"$uprojectPath`"",
    '/Game/Maps/Istana_1km_Context_v2',
    "-settings=`"$resolvedSettingsPath`"",
    '-TRIADIstanaVisualAcceptance'
)
$process = Start-Process `
    -FilePath $resolvedEditorPath `
    -ArgumentList $arguments `
    -WindowStyle Normal `
    -PassThru

$preflight | Add-Member -NotePropertyName ProcessId -NotePropertyValue $process.Id
$preflight | Add-Member -NotePropertyName Launched -NotePropertyValue $true
$preflight
