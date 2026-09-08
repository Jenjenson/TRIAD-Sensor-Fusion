#requires -Version 7.0

<#
.SYNOPSIS
Guarded UE 5.5 transaction for the R29 Copernicus terrain visual fallback.

.DESCRIPTION
Default and -StaticSelfCheck modes are repository-only: they hash every source
input and never inspect or mutate a native Unreal tree. Live execution requires
-Execute and explicit map/module receipts. It imports the metres OBJ at exact
x100, validates +/-100000 cm X/Y bounds, creates the exact complementary-core
masked material, then adds one render-only provider-fallback actor after a
byte-identical external map backup is verified.

Cesium remains preferred. The existing terrain keeps QueryAndPhysics collision;
the local DEM has no collision/navigation/sensor/RF/geospatial authority. No
live execution or visual acceptance is authorized or claimed by this file.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,
    [switch] $Execute,
    [switch] $StaticSelfCheck,
    [string] $NativeProjectRoot,
    [string] $NativeProjectFile,
    [string] $EngineRoot,
    [ValidatePattern('^[A-Za-z0-9_]+Editor$')]
    [string] $EditorTarget,
    [long] $ExpectedMapBytes,
    [string] $ExpectedMapSha256,
    [long] $ExpectedRuntimeDllBytes,
    [string] $ExpectedRuntimeDllSha256,
    [long] $ExpectedEditorDllBytes,
    [string] $ExpectedEditorDllSha256
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema = 'triad.istana_explore_v5d.r29_copernicus_terrain_fallback.native_transaction.v1'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repoUnrealRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'unreal'))
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapRelative = 'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'
$nativeTransactionRelative = 'Saved\TRIAD\NativeTransactions\V5DR29CopernicusTerrainFallbackV1'

$codePins = @(
    [pscustomobject]@{RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h';Bytes=6995L;Sha256='A0774C4D538018AEE617EBE34EFFB0C512CA11623334099C905D6D62B890A7DC'},
    [pscustomobject]@{RepositoryRelativePath='SourceAssets\IstanaPublicViewExploreV5D\Terrain\CopernicusDEM2021\NativeSourceClosure\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp';NativeRelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp';Bytes=19064L;Sha256='F57C4C51F666290EF3C74CB03E4A536C476250CCC3AC3BD008D92456C824CA36'},
    [pscustomobject]@{RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActorTests.cpp';Bytes=4715L;Sha256='CF37FE3E82BEB09B8E4B8214B24D4F8B18FDB20A426A4140B7F9FEE08085B963'},
    [pscustomobject]@{RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory.h';Bytes=484L;Sha256='BC466ECF6898892C4EB027C63AE4BD3D2750CF33FC0F86FC77C4B279A3A89B58'},
    [pscustomobject]@{RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory.cpp';Bytes=38219L;Sha256='B41BB7CE27CF2F69EE896DE63016700DA4D0ABA48F76694EEDF94F5AFCFB35C8'},
    [pscustomobject]@{RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary.h';Bytes=1903L;Sha256='20CBFCD7B2CF917FB125A9F63392620910EF383B763EFDD5802AF1DE64397FEA'},
    [pscustomobject]@{RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary.cpp';Bytes=18185L;Sha256='8F989A94DCB61AC9268FF0F7F548ADC493441D7F391B7FF2519E30B5EB33D534'}
)

$assetPins = @(
    [pscustomobject]@{RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Terrain\CopernicusDEM2021\Generated\SM_IPV5D_CopernicusDEM2021_TerrainProxy_Visual.obj';NativeRelativePath='Plugins\TRIADSensorFusion\Tools\IstanaExploreV5D\CopernicusDEM2021\Generated\SM_IPV5D_CopernicusDEM2021_TerrainProxy_Visual.obj';Bytes=3349537L;Sha256='6057AE3C23287E9AFFED72B1C0842177A980BFE7AAB4089BF51E6FD7B2A621DF'},
    [pscustomobject]@{RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Terrain\CopernicusDEM2021\Generated\IstanaCopernicusDEM2021TerrainProxy.mtl';NativeRelativePath='Plugins\TRIADSensorFusion\Tools\IstanaExploreV5D\CopernicusDEM2021\Generated\IstanaCopernicusDEM2021TerrainProxy.mtl';Bytes=233L;Sha256='AA98AABD87F409748B24AD344EFC43F68C3A0840CD670EC503A67597ACBEB720'},
    [pscustomobject]@{RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Terrain\CopernicusDEM2021\Generated\IstanaCopernicusDEM2021Terrain.manifest.json';NativeRelativePath='Plugins\TRIADSensorFusion\Tools\IstanaExploreV5D\CopernicusDEM2021\Generated\IstanaCopernicusDEM2021Terrain.manifest.json';Bytes=11112L;Sha256='06FD4D5A2F3988DAC311601F8255255C632A4FE60FC7D923F1F2C62CF08AFC5D'},
    [pscustomobject]@{RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Terrain\CopernicusDEM2021\copernicus_dem_2021.contract.json';NativeRelativePath='Plugins\TRIADSensorFusion\Tools\IstanaExploreV5D\CopernicusDEM2021\copernicus_dem_2021.contract.json';Bytes=5208L;Sha256='ED96EEE1B070E09F33F469747DDC27F482018959CF763A7D2B20755D6794A415'},
    [pscustomobject]@{RelativePath='SourceAssets\IstanaPublicViewExploreV5D\Terrain\CopernicusDEM2021\copernicus_dem_2021.native_fallback.contract.json';NativeRelativePath='Plugins\TRIADSensorFusion\Tools\IstanaExploreV5D\CopernicusDEM2021\copernicus_dem_2021.native_fallback.contract.json';Bytes=3739L;Sha256='36F00FB7AC870EE9411A1830817FD41EA6AFCE05AD6214E3E15F68E960849D3E'}
)

$outputAssets = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback\SM_IPV5D_R29_CopernicusTerrainFallback_Render.uasset',
    'Content\TRIAD\IstanaPublicViewExploreV5D\R29CopernicusTerrainFallback\Materials\M_IPV5D_R29_CopernicusTerrainFallback_ComplementaryCoreMask.uasset'
)

function Test-Sha256([string] $Value) {
    $null -ne $Value -and $Value -cmatch '^[0-9A-F]{64}$'
}

function Test-Contained([string] $Path, [string] $Root) {
    $candidate = [IO.Path]::GetFullPath($Path)
    $boundary = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $candidate.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)
}

function Get-PinRelativePath(
    [object] $Pin,
    [ValidateSet('Repository','Native')]
    [string] $Surface) {
    $explicitName = if ($Surface -ceq 'Repository') {
        'RepositoryRelativePath'
    } else {
        'NativeRelativePath'
    }
    $explicitProperty = $Pin.PSObject.Properties[$explicitName]
    if ($null -ne $explicitProperty) {
        $value = [string]$explicitProperty.Value
        if ([string]::IsNullOrWhiteSpace($value)) {
            throw "R29 Copernicus pin has an empty $explicitName."
        }
        return $value
    }
    $samePathProperty = $Pin.PSObject.Properties['RelativePath']
    if ($null -eq $samePathProperty -or
        [string]::IsNullOrWhiteSpace([string]$samePathProperty.Value)) {
        throw "R29 Copernicus pin lacks both $explicitName and same-path RelativePath semantics."
    }
    [string]$samePathProperty.Value
}

function Get-State([string] $Path) {
    $full = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($full)) {
        return [pscustomobject]@{Path=$full;Present=$false;Bytes=0L;Sha256='ABSENT'}
    }
    $item = Get-Item -LiteralPath $full
    [pscustomobject]@{
        Path=$full
        Present=$true
        Bytes=[long]$item.Length
        Sha256=(Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

function Assert-Pin($Pin, [string] $Path, [string] $Label) {
    $state = Get-State $Path
    if (-not $state.Present -or $state.Bytes -ne [long]$Pin.Bytes -or
        $state.Sha256 -cne [string]$Pin.Sha256) {
        throw "$Label pin mismatch: $Path expected=$($Pin.Bytes)/$($Pin.Sha256) actual=$($state.Bytes)/$($state.Sha256)"
    }
    $state
}

function Assert-RepoPins {
    if ($codePins.Count -ne 7 -or $assetPins.Count -ne 5 -or
        $outputAssets.Count -ne 2) {
        throw 'R29 Copernicus rosters must remain exactly 7 code, 5 source and 2 output assets.'
    }
    $seenRepository = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $seenNative = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in @($codePins) + @($assetPins)) {
        $repositoryRelativePath = Get-PinRelativePath $pin 'Repository'
        $nativeRelativePath = Get-PinRelativePath $pin 'Native'
        if ([IO.Path]::IsPathRooted($repositoryRelativePath) -or
            $repositoryRelativePath.Contains('..') -or
            [IO.Path]::IsPathRooted($nativeRelativePath) -or
            $nativeRelativePath.Contains('..') -or
            -not $seenRepository.Add($repositoryRelativePath) -or
            -not $seenNative.Add($nativeRelativePath) -or
            -not (Test-Sha256 ([string]$pin.Sha256))) {
            throw "Invalid R29 Copernicus pin mapping: repository=$repositoryRelativePath native=$nativeRelativePath"
        }
        [void](Assert-Pin $pin (Join-Path $repoUnrealRoot $repositoryRelativePath) 'repo')
    }
    $historicalActorRepository =
        'SourceAssets\IstanaPublicViewExploreV5D\Terrain\CopernicusDEM2021\NativeSourceClosure\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp'
    $canonicalActorNative =
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp'
    if ($null -eq $codePins[1].PSObject.Properties['RepositoryRelativePath'] -or
        $null -eq $codePins[1].PSObject.Properties['NativeRelativePath'] -or
        $null -ne $codePins[1].PSObject.Properties['RelativePath'] -or
        (Get-PinRelativePath $codePins[1] 'Repository') -cne $historicalActorRepository -or
        (Get-PinRelativePath $codePins[1] 'Native') -cne $canonicalActorNative) {
        throw 'R29 Copernicus historical actor mapping drifted from explicit repository/native semantics.'
    }
}

Assert-RepoPins

if ($StaticSelfCheck -or -not $Execute) {
    [pscustomobject][ordered]@{
        Schema=$schema
        Status=if ($StaticSelfCheck) {'STATIC_SELF_CHECK_PASS'} else {'READ_ONLY_PREFLIGHT_PASS'}
        RunToken=$RunToken
        HashPinnedCodeInputs=7
        HashPinnedSourceInputs=5
        HistoricalActorRepositorySource=(Get-PinRelativePath $codePins[1] 'Repository')
        HistoricalActorNativeDestination=(Get-PinRelativePath $codePins[1] 'Native')
        ExactOutputAssetCount=2
        ExactImportUniformScale=100.0
        RequiredPostImportBoundsXCentimeters=@(-100000.0,100000.0)
        RequiredPostImportBoundsYCentimeters=@(-100000.0,100000.0)
        CoreMask='EXACT_COMPLEMENT_64_EDGE_50M_COLLAR_8M_FEATHER_0P25M_DITHER'
        CesiumWorldTerrainPreferred=$true
        SourceTerrainQueryAndPhysicsPreserved=$true
        CollisionNavigationSensorRfGeospatialAuthority=$false
        UnrealBuildOrEditorLaunched=$false
        NativeTreeReadOrWritten=$false
        TargetMapMutated=$false
        NativeVisualAcceptance=$false
    } | ConvertTo-Json -Depth 6
    return
}

foreach ($name in @(
    'NativeProjectRoot','NativeProjectFile','EngineRoot','EditorTarget',
    'ExpectedMapBytes','ExpectedMapSha256','ExpectedRuntimeDllBytes',
    'ExpectedRuntimeDllSha256','ExpectedEditorDllBytes','ExpectedEditorDllSha256')) {
    if (-not $PSBoundParameters.ContainsKey($name)) {
        throw "Live execution requires explicit caller-supplied -$name."
    }
}
foreach ($sha in @(
    $ExpectedMapSha256,$ExpectedRuntimeDllSha256,$ExpectedEditorDllSha256)) {
    if (-not (Test-Sha256 $sha.ToUpperInvariant())) {
        throw 'Every caller-supplied SHA-256 must be exactly 64 hexadecimal characters.'
    }
}

$nativeRoot = [IO.Path]::GetFullPath($NativeProjectRoot).TrimEnd('\')
$projectFile = [IO.Path]::GetFullPath($NativeProjectFile)
$engine = [IO.Path]::GetFullPath($EngineRoot).TrimEnd('\')
if (-not (Test-Contained $projectFile $nativeRoot) -or
    -not $projectFile.EndsWith('.uproject',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Native project escaped the explicit native root or is not a .uproject.'
}
$buildVersionFile = Join-Path $engine 'Engine\Build\Build.version'
$buildTool = Join-Path $engine 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor.exe'
$editorCmd = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
foreach ($path in @($projectFile,$buildVersionFile,$buildTool,$editor,$editorCmd)) {
    if (-not [IO.File]::Exists($path)) { throw "Required live input missing: $path" }
}
$buildVersion = Get-Content -LiteralPath $buildVersionFile -Raw | ConvertFrom-Json
if ([int]$buildVersion.MajorVersion -ne 5 -or
    [int]$buildVersion.MinorVersion -ne 5) {
    throw "Only UE 5.5 is admitted; found $($buildVersion.MajorVersion).$($buildVersion.MinorVersion)."
}

$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$terrainLibrary = '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'
$remoteTimeoutSeconds = 900
$shutdownTimeoutSeconds = 180

function Invoke-RcCall(
    [string] $ObjectPath,
    [string] $FunctionName,
    [hashtable] $Parameters,
    [int] $TimeoutSeconds) {
    $body = [ordered]@{
        objectPath=$ObjectPath
        functionName=$FunctionName
        parameters=$Parameters
        generateTransaction=$false
    } | ConvertTo-Json -Depth 8 -Compress
    Invoke-RestMethod -Method Put -Uri $rcUri -ContentType 'application/json' `
        -Body $body -TimeoutSec $TimeoutSeconds
}

function Assert-NoUe55Editor([string] $Label) {
    $busy = @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $_.ExecutablePath -and
            [IO.Path]::GetFullPath($_.ExecutablePath).StartsWith(
                $engine + '\',[StringComparison]::OrdinalIgnoreCase) -and
            $_.Name -like 'UnrealEditor*'
        }
    )
    if ($busy.Count -ne 0) {
        throw "$Label refused: a UE 5.5 editor/helper process is already present."
    }
}

function Get-OwnedHelperIdentity(
    [Diagnostics.Process] $Handle,
    [string] $Log) {
    $record = Get-CimInstance Win32_Process `
        -Filter "ProcessId=$($Handle.Id)" -ErrorAction Stop
    if ($null -eq $record -or -not $record.ExecutablePath -or
        -not [IO.Path]::GetFullPath($record.ExecutablePath).Equals(
            $editor,[StringComparison]::OrdinalIgnoreCase) -or
        -not $record.CommandLine.Contains(
            $projectFile,[StringComparison]::OrdinalIgnoreCase) -or
        -not $record.CommandLine.Contains($mapPackage,[StringComparison]::Ordinal) -or
        -not $record.CommandLine.Contains($Log,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Could not prove the exact launched UE 5.5 terrain helper identity.'
    }
    [pscustomobject][ordered]@{
        ProcessId=[uint32]$record.ProcessId
        CreationDate=[string]$record.CreationDate
        ExecutablePath=[IO.Path]::GetFullPath($record.ExecutablePath)
        CommandLine=[string]$record.CommandLine
    }
}

function Stop-OwnedHelper(
    [Diagnostics.Process] $Handle,
    $Identity,
    [string] $Log) {
    if ([uint32]$Handle.Id -ne [uint32]$Identity.ProcessId) {
        throw 'Terrain helper handle does not match its proven process identity.'
    }
    try { [void](Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30) } catch {}
    if (-not $Handle.WaitForExit($shutdownTimeoutSeconds * 1000)) {
        [void](Get-OwnedHelperIdentity $Handle $Log)
        $Handle.Kill()
        if (-not $Handle.WaitForExit(30000)) {
            throw 'Exact owned terrain helper did not exit after bounded containment.'
        }
    }
}

function Invoke-TerrainRemoteStage(
    [string] $Stage,
    [string] $FunctionName,
    [string] $ExpectedPrefix,
    [hashtable] $Parameters) {
    Assert-NoUe55Editor "before $Stage"
    $log = [IO.Path]::GetFullPath((Join-Path $transactionRoot "$Stage.log"))
    if ([IO.File]::Exists($log)) {
        throw "Terrain stage log already exists: $log"
    }
    $argumentLine = "`"$projectFile`" $mapPackage -DisablePlugin=AirSim " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-RemoteControlHttpServer -RCWebControlEnable ' +
        '-ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$log`""
    $handle = Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $nativeRoot -PassThru -WindowStyle Hidden
    $identity = $null
    $response = $null
    $stageError = $null
    try {
        $identityDeadline = [DateTime]::UtcNow.AddSeconds(30)
        do {
            try { $identity = Get-OwnedHelperIdentity $handle $log }
            catch { $identity = $null }
            if ($null -ne $identity) { break }
            Start-Sleep -Milliseconds 500
        } while ([DateTime]::UtcNow -lt $identityDeadline)
        if ($null -eq $identity) {
            throw "Could not prove terrain helper identity for $Stage."
        }
        $readyDeadline = [DateTime]::UtcNow.AddSeconds($remoteTimeoutSeconds)
        do {
            try {
                $ready = Invoke-RcCall $identityLibrary `
                    'ValidateIstanaExploreRemoteControlProject' `
                    @{ExpectedProjectPath=$nativeRoot} 30
            } catch { $ready = $null }
            if ($null -ne $ready -and $ready.ReturnValue -eq $true) { break }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        if ($null -eq $ready -or $ready.ReturnValue -ne $true) {
            throw "Terrain remote-control identity did not become ready for $Stage."
        }
        [void](Get-OwnedHelperIdentity $handle $log)
        $response = Invoke-RcCall $terrainLibrary $FunctionName `
            $Parameters $remoteTimeoutSeconds
        $report = [string]$response.OutReport
        if ($response.ReturnValue -ne $true -or
            -not $report.StartsWith($ExpectedPrefix,[StringComparison]::Ordinal)) {
            throw "Terrain stage $Stage failed exact response gate: $report"
        }
    } catch {
        $stageError = $_.Exception
    } finally {
        if ($null -eq $identity -and -not $handle.HasExited) {
            try { $identity = Get-OwnedHelperIdentity $handle $log }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
            }
        }
        if ($null -ne $identity -and -not $handle.HasExited) {
            try { Stop-OwnedHelper $handle $identity $log }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
            }
        }
    }
    Assert-NoUe55Editor "after $Stage"
    if ($null -ne $stageError) { throw $stageError }
    if (-not [IO.File]::Exists($log) -or
        (Get-Item -LiteralPath $log).Length -le 0) {
        throw "Terrain stage $Stage did not persist a non-empty log."
    }
    [pscustomobject][ordered]@{
        Stage=$Stage
        Function=$FunctionName
        ExpectedPrefix=$ExpectedPrefix
        Report=[string]$response.OutReport
        ProcessIdentity=$identity
        Log=Get-State $log
    }
}

$mapFile = Join-Path $nativeRoot $mapRelative
$runtimeDll = Join-Path $nativeRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'
$editorDll = Join-Path $nativeRoot 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'
$predecessorMap = Assert-Pin ([pscustomobject]@{Bytes=$ExpectedMapBytes;Sha256=$ExpectedMapSha256.ToUpperInvariant()}) $mapFile 'map'
$predecessorRuntimeDll = Assert-Pin ([pscustomobject]@{Bytes=$ExpectedRuntimeDllBytes;Sha256=$ExpectedRuntimeDllSha256.ToUpperInvariant()}) $runtimeDll 'runtime DLL'
$predecessorEditorDll = Assert-Pin ([pscustomobject]@{Bytes=$ExpectedEditorDllBytes;Sha256=$ExpectedEditorDllSha256.ToUpperInvariant()}) $editorDll 'editor DLL'

$transactionBase = Join-Path $nativeRoot $nativeTransactionRelative
$transactionRoot = Join-Path $transactionBase $RunToken
if (-not (Test-Contained $transactionRoot $nativeRoot) -or
    [IO.Directory]::Exists($transactionRoot) -or
    [IO.File]::Exists($transactionRoot)) {
    throw 'Transaction root escaped the native root or already exists.'
}
[IO.Directory]::CreateDirectory($transactionRoot) | Out-Null
$backupMap = Join-Path $transactionRoot 'predecessor.umap'
$backupRuntimeDll = Join-Path $transactionRoot 'UnrealEditor-TRIADSensorFusion.dll'
$backupEditorDll = Join-Path $transactionRoot 'UnrealEditor-TRIADSensorFusionEditor.dll'
[IO.File]::Copy($mapFile,$backupMap,$false)
[IO.File]::Copy($runtimeDll,$backupRuntimeDll,$false)
[IO.File]::Copy($editorDll,$backupEditorDll,$false)
[void](Assert-Pin ([pscustomobject]@{Bytes=$ExpectedMapBytes;Sha256=$ExpectedMapSha256.ToUpperInvariant()}) $backupMap 'backup map')
[void](Assert-Pin ([pscustomobject]@{Bytes=$ExpectedRuntimeDllBytes;Sha256=$ExpectedRuntimeDllSha256.ToUpperInvariant()}) $backupRuntimeDll 'backup runtime DLL')
[void](Assert-Pin ([pscustomobject]@{Bytes=$ExpectedEditorDllBytes;Sha256=$ExpectedEditorDllSha256.ToUpperInvariant()}) $backupEditorDll 'backup editor DLL')

$copied = [Collections.Generic.List[string]]::new()
$outputInitiallyPresent = @{}
foreach ($relative in $outputAssets) {
    $outputInitiallyPresent[$relative] = [IO.File]::Exists((Join-Path $nativeRoot $relative))
}
try {
    foreach ($pin in $codePins) {
        $source = Join-Path $repoUnrealRoot (Get-PinRelativePath $pin 'Repository')
        $destination = Join-Path $nativeRoot (Get-PinRelativePath $pin 'Native')
        if (-not (Test-Contained $destination $nativeRoot)) { throw 'Code destination escaped native root.' }
        if ([IO.File]::Exists($destination)) {
            [void](Assert-Pin $pin $destination 'existing native code')
        } else {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
            [IO.File]::Copy($source,$destination,$false)
            $copied.Add($destination)
        }
    }
    foreach ($pin in $assetPins) {
        $source = Join-Path $repoUnrealRoot $pin.RelativePath
        $destination = Join-Path $nativeRoot $pin.NativeRelativePath
        if (-not (Test-Contained $destination $nativeRoot)) { throw 'Tool destination escaped native root.' }
        if ([IO.File]::Exists($destination)) {
            [void](Assert-Pin $pin $destination 'existing native tool source')
        } else {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
            [IO.File]::Copy($source,$destination,$false)
            $copied.Add($destination)
        }
    }

    $forcedObjects = @(
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp.obj',
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.gen.cpp.obj',
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActorTests.cpp.obj',
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADSensorFusion.init.gen.cpp.obj',
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory.cpp.obj',
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary.cpp.obj',
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary.gen.cpp.obj',
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADSensorFusionEditor.init.gen.cpp.obj'
    )
    foreach ($relative in $forcedObjects) {
        $objectPath = Join-Path $nativeRoot $relative
        if (-not (Test-Contained $objectPath $nativeRoot)) {
            throw 'Forced-object invalidation escaped the native root.'
        }
        if ([IO.File]::Exists($objectPath)) {
            Remove-Item -LiteralPath $objectPath -Force
        }
    }

    $buildLog = Join-Path $transactionRoot 'build.log'
    $buildArguments = @(
        'UnrealEditor', 'Win64', 'Development',
        "-Project=$projectFile", '-WaitMutex', '-NoHotReloadFromIDE',
        '-Module=TRIADSensorFusion', '-Module=TRIADSensorFusionEditor',
        '-ForceHeaderGeneration', '-NoUBTMakefiles', '-MaxParallelActions=1'
    )
    & $buildTool @buildArguments 2>&1 | Tee-Object -FilePath $buildLog
    if ($LASTEXITCODE -ne 0 -or -not [IO.File]::Exists($buildLog) -or
        (Get-Item -LiteralPath $buildLog).Length -le 0) {
        throw "UE 5.5 module build failed with exit code $LASTEXITCODE."
    }
    $runtimeAfterBuild = Get-State $runtimeDll
    $editorAfterBuild = Get-State $editorDll
    if (-not $runtimeAfterBuild.Present -or
        -not $editorAfterBuild.Present -or
        $runtimeAfterBuild.Bytes -le 0 -or
        $editorAfterBuild.Bytes -le 0 -or
        -not (Test-Sha256 $runtimeAfterBuild.Sha256) -or
        -not (Test-Sha256 $editorAfterBuild.Sha256) -or
        ($runtimeAfterBuild.Bytes -eq $ExpectedRuntimeDllBytes -and
         $runtimeAfterBuild.Sha256 -ceq $ExpectedRuntimeDllSha256.ToUpperInvariant()) -or
        ($editorAfterBuild.Bytes -eq $ExpectedEditorDllBytes -and
         $editorAfterBuild.Sha256 -ceq $ExpectedEditorDllSha256.ToUpperInvariant())) {
        throw 'Module build did not produce fresh runtime and editor DLL receipts.'
    }

    $statusFile = Join-Path $transactionRoot 'editor-status.json'
    $pythonFile = Join-Path $transactionRoot 'build-assets.py'
    $escapedStatus = $statusFile.Replace('\','/')
    $escapedMap = $mapFile.Replace('\','/')
    $python = @"
import json
import unreal
status_path = r'$escapedStatus'
result = {'ok': False, 'stage': 'asset_build', 'report': '', 'arity': -1}
try:
    unreal.EditorLoadingAndSavingUtils.load_map(r'$escapedMap')
    lib = unreal.TRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary
    raw = lib.build_or_validate_copernicus_terrain_fallback_assets()
    values = raw if isinstance(raw, tuple) else (raw,)
    report = str(values[-1]) if values and values[-1] is not None else ''
    ok = report.startswith('R29_COPERNICUS_TERRAIN_ASSET_BUILD_PASS') or report.startswith('R29_COPERNICUS_TERRAIN_ASSET_BUILD_IDEMPOTENT_PASS')
    result.update({'ok': ok, 'stage': 'asset_build', 'report': report, 'arity': len(values)})
except Exception as exc:
    result.update({'ok': False, 'report': str(exc)})
with open(status_path, 'w', encoding='utf-8') as handle:
    json.dump(result, handle, sort_keys=True)
if not result['ok']:
    raise RuntimeError(result['report'])
"@
    [IO.File]::WriteAllText($pythonFile,$python,[Text.UTF8Encoding]::new($false))
    & $editorCmd $projectFile $mapPackage -unattended -nosplash -NoSound `
        "-ExecutePythonScript=$pythonFile" "-log=$(Join-Path $transactionRoot 'editor.log')"
    if ($LASTEXITCODE -ne 0 -or -not [IO.File]::Exists($statusFile)) {
        throw "Unreal editor transaction failed with exit code $LASTEXITCODE."
    }
    $editorStatus = Get-Content -LiteralPath $statusFile -Raw | ConvertFrom-Json
    if (-not [bool]$editorStatus.ok) { throw "Editor validation failed: $($editorStatus.report)" }
    $applyStage = Invoke-TerrainRemoteStage `
        'apply-map' `
        'ApplyCopernicusTerrainFallbackToLoadedHybridMap' `
        'R29_COPERNICUS_TERRAIN_APPLY_PASS' `
        @{
            ExpectedPredecessorBytes=[int64]$ExpectedMapBytes
            ExpectedPredecessorSha256=$ExpectedMapSha256.ToUpperInvariant()
            VerifiedExternalBackupFilename=$backupMap
        }
    $postApplySuccessor = Get-State $mapFile
    $postApplyBackup = Get-State $backupMap
    if (-not $postApplySuccessor.Present -or
        ($postApplySuccessor.Bytes -eq $ExpectedMapBytes -and
         $postApplySuccessor.Sha256 -ceq $ExpectedMapSha256.ToUpperInvariant()) -or
        -not $postApplyBackup.Present -or
        $postApplyBackup.Bytes -ne $ExpectedMapBytes -or
        $postApplyBackup.Sha256 -cne $ExpectedMapSha256.ToUpperInvariant()) {
        throw 'Apply process did not produce a changed successor with an immutable backup.'
    }
    $coldStage = Invoke-TerrainRemoteStage `
        'cold-validate-map' `
        'ValidateCopernicusTerrainFallbackSuccessorMap' `
        'R29_COPERNICUS_TERRAIN_SUCCESSOR_VALID' `
        @{}
    $successor = Get-State $mapFile
    $backup = Get-State $backupMap
    $successorRuntimeDll = Get-State $runtimeDll
    $successorEditorDll = Get-State $editorDll
    if (-not $successor.Present -or
        ($successor.Bytes -eq $ExpectedMapBytes -and
         $successor.Sha256 -ceq $ExpectedMapSha256.ToUpperInvariant()) -or
        -not $backup.Present -or $backup.Bytes -ne $ExpectedMapBytes -or
        $backup.Sha256 -cne $ExpectedMapSha256.ToUpperInvariant() -or
        $successor.Bytes -ne $postApplySuccessor.Bytes -or
        $successor.Sha256 -cne $postApplySuccessor.Sha256 -or
        -not $successorRuntimeDll.Present -or
        -not $successorEditorDll.Present -or
        $successorRuntimeDll.Bytes -ne $runtimeAfterBuild.Bytes -or
        $successorRuntimeDll.Sha256 -cne $runtimeAfterBuild.Sha256 -or
        $successorEditorDll.Bytes -ne $editorAfterBuild.Bytes -or
        $successorEditorDll.Sha256 -cne $editorAfterBuild.Sha256) {
        throw 'Final successor-change or immutable-backup receipt failed.'
    }
    $outputAssetReceipts = [Collections.Generic.List[object]]::new()
    foreach ($relative in $outputAssets) {
        $path = Join-Path $nativeRoot $relative
        if (-not [IO.File]::Exists($path)) { throw "Expected output asset missing: $path" }
        $outputAssetReceipts.Add((Get-State $path))
    }
    $commit = [pscustomobject][ordered]@{
        Schema=$schema
        Status='NATIVE_TRANSACTION_PASS'
        RunToken=$RunToken
        EngineVersion='5.5'
        PredecessorMap=$predecessorMap
        SuccessorMap=$successor
        PredecessorRuntimeDll=$predecessorRuntimeDll
        SuccessorRuntimeDll=$successorRuntimeDll
        PredecessorEditorDll=$predecessorEditorDll
        SuccessorEditorDll=$successorEditorDll
        SuccessorMapBytes=$successor.Bytes
        SuccessorMapSha256=$successor.Sha256
        BackupMapBytes=$backup.Bytes
        BackupMapSha256=$backup.Sha256
        OutputAssets=@($outputAssetReceipts)
        ExactOutputAssetCount=2
        ForcedObjectInvalidationCount=$forcedObjects.Count
        ExactImportUniformScale=100.0
        CesiumWorldTerrainPreferred=$true
        SourceTerrainQueryAndPhysicsPreserved=$true
        CollisionNavigationSensorRfGeospatialAuthority=$false
        NativeVisualAcceptance=$false
        AssetBuildReport=[string]$editorStatus.report
        ApplyStage=$applyStage
        ColdValidationStage=$coldStage
        EditorReport=[string]$coldStage.Report
    }
    $commitPath = Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 12 |
        Set-Content -LiteralPath $commitPath -Encoding utf8NoBOM -NoNewline
    $commit | ConvertTo-Json -Depth 12
} catch {
    if ([IO.File]::Exists($backupMap)) {
        $backupState = Get-State $backupMap
        if ($backupState.Bytes -eq $ExpectedMapBytes -and
            $backupState.Sha256 -ceq $ExpectedMapSha256.ToUpperInvariant()) {
            [IO.File]::Copy($backupMap,$mapFile,$true)
        }
    }
    if ([IO.File]::Exists($backupRuntimeDll)) {
        [IO.File]::Copy($backupRuntimeDll,$runtimeDll,$true)
    }
    if ([IO.File]::Exists($backupEditorDll)) {
        [IO.File]::Copy($backupEditorDll,$editorDll,$true)
    }
    foreach ($relative in $outputAssets) {
        $output = Join-Path $nativeRoot $relative
        if (-not [bool]$outputInitiallyPresent[$relative] -and
            [IO.File]::Exists($output)) {
            Remove-Item -LiteralPath $output -Force
        }
    }
    foreach ($path in $copied) {
        if ([IO.File]::Exists($path)) {
            Remove-Item -LiteralPath $path -Force
        }
    }
    [void](Assert-Pin ([pscustomobject]@{Bytes=$ExpectedMapBytes;Sha256=$ExpectedMapSha256.ToUpperInvariant()}) $mapFile 'restored map')
    [void](Assert-Pin ([pscustomobject]@{Bytes=$ExpectedRuntimeDllBytes;Sha256=$ExpectedRuntimeDllSha256.ToUpperInvariant()}) $runtimeDll 'restored runtime DLL')
    [void](Assert-Pin ([pscustomobject]@{Bytes=$ExpectedEditorDllBytes;Sha256=$ExpectedEditorDllSha256.ToUpperInvariant()}) $editorDll 'restored editor DLL')
    throw
}
