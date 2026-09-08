#requires -Version 7.0

<#
.SYNOPSIS
Runs the strict V5D Landmark Vegetation R27 visual-correction transaction.

.DESCRIPTION
The default invocation is a read-only preflight. -Execute is the only native
write authority. The live path promotes exactly eight byte/hash-pinned sources,
forces UHT while building the two TRIAD modules, creates exactly four isolated
landmark-only grass materials, reconfigures the sole existing R26 actor on an
explicitly pinned post-Temasek-Phase-2 map, cold-validates the successor, and
proves a byte-stable idempotent apply. It never edits the shared R23B grass
materials. Visual acceptance remains false until a fresh evidence capture.

Live execution requires caller-supplied map and DLL receipts from the exact
committed Temasek Phase-2 successor. Before the first native write it records
and backs up the exact map, eight source destinations, the four absent R27
material destinations, immutable R25/V2 content pins, and every file under the bounded
TRIAD plugin Binaries/Intermediate build roots plus the known project-global
UBT sidecars. Failure restores map, build surface, immutable content, and
source destinations without deleting a directory. A pre-existing exact UE5.4
CAPSTONE process set is treated as immutable and is never stopped.

.EXAMPLE
.\Invoke-IstanaExploreV5DLandmarkVegetationR27NativeTransactionV1.ps1 -RunToken reviewed_r27

.EXAMPLE
# Supply all six values from the committed Temasek Phase-2 receipt.
.\Invoke-IstanaExploreV5DLandmarkVegetationR27NativeTransactionV1.ps1 -RunToken reviewed_r27 -Execute -RequireTemasekPhase2 -ExpectedMapBytes <bytes> -ExpectedMapSha256 <sha256> -ExpectedRuntimeDllBytes <bytes> -ExpectedRuntimeDllSha256 <sha256> -ExpectedEditorDllBytes <bytes> -ExpectedEditorDllSha256 <sha256>
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,

    [switch] $StaticSelfCheck,

    # Documentation/read-only defaults only. Live execution requires all six
    # parameters to be supplied explicitly from the committed Phase-2 receipt.
    [ValidateRange(1, [long]::MaxValue)]
    [long] $ExpectedMapBytes = 36335002L,

    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedMapSha256 =
        '0CDA45D7390A92885A911C1CD3404B0B59E889CE54A5879B8F1234197F73D498',

    [ValidateRange(1, [long]::MaxValue)]
    [long] $ExpectedRuntimeDllBytes = 4672000L,

    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedRuntimeDllSha256 =
        '0B90C971AA385244FD20D2916BA87B7AE013C0C40F5B1153FB237E4CCF156191',

    [ValidateRange(1, [long]::MaxValue)]
    [long] $ExpectedEditorDllBytes = 7725056L,

    [ValidatePattern('^[A-Fa-f0-9]{64}$')]
    [string] $ExpectedEditorDllSha256 =
        '7F5D8596FD032DFAC335D7DC9AD2EC9DC7B72029144D36A40292CB8C19510F5A',

    [switch] $RequireTemasekPhase2,

    [ValidateRange(60, 1800)]
    [int] $EditorTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema =
    'triad.istana_explore_v5d.landmark_vegetation_r27.native_transaction.v1'
$repositoryUnrealRoot = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo\unreal')
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath(
    'D:\triad\TRIAD\TRIAD.uproject')
$engineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath((Join-Path $engineRoot `
    'Engine\Build\BatchFiles\Build.bat'))
$editor = [IO.Path]::GetFullPath((Join-Path $engineRoot `
    'Engine\Binaries\Win64\UnrealEditor.exe'))
$protectedEditor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedProject = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$mapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$mapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DLandmarkVegetationR27V1'))
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase $RunToken))
$pluginBinaryRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries'))
$pluginIntermediateRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Intermediate'))
$ddcRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\DerivedDataCache'))
$rcUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$assetLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary'
$temasekLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DTemasekShophouseEditorLibrary'
$hybridLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'

$expectedProjectPin = [pscustomobject] [ordered] @{
    Path = $nativeProjectFile
    Present = $true
    Bytes = 1298L
    Sha256 =
        '42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3'
}
$expectedMapPin = [pscustomobject] [ordered] @{
    Path = $mapFile
    Present = $true
    Bytes = [int64] $ExpectedMapBytes
    Sha256 = ([string] $ExpectedMapSha256).ToUpperInvariant()
}
$expectedRuntimeDllPin = [pscustomobject] [ordered] @{
    Path = $runtimeDll
    Present = $true
    Bytes = [int64] $ExpectedRuntimeDllBytes
    Sha256 = ([string] $ExpectedRuntimeDllSha256).ToUpperInvariant()
}
$expectedEditorDllPin = [pscustomobject] [ordered] @{
    Path = $editorDll
    Present = $true
    Bytes = [int64] $ExpectedEditorDllBytes
    Sha256 = ([string] $ExpectedEditorDllSha256).ToUpperInvariant()
}

# All eight files must match the exact committed R26/Phase-2 native source
# state before promotion. The GroundVegetation pair supplies the complete
# canonical R23B derivative validator used by the R27 landmark asset gate.
$sourcePins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DLandmarkVegetationActor.h'
        Bytes = 11493L
        Sha256 = '1B73DECAEB572876817F63B3B4EA8C6362F0695C9F2468137EDA92D148107D08'
        NativeBeforePresent = $true
        NativeBeforeBytes = 10946L
        NativeBeforeSha256 = '1CBB2B107D00949BE0EB33028E2BE80C3F4A889796D0BE682412374FACC764F6'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DLandmarkVegetationActor.cpp'
        Bytes = 55817L
        Sha256 = '537C35F16F10C0E3BDFD4E25F41028E5FE2875134B08F0D3C92805A25BA00BEF'
        NativeBeforePresent = $true
        NativeBeforeBytes = 50398L
        NativeBeforeSha256 = 'FE7910E7C1B4A60BB8FE70A088A0F7C002925477842398500471883F56BC735E'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h'
        Bytes = 1619L
        Sha256 = '4718F81CDE54D999E6559382278C9EBBFE426936931F7B5669A84997B23F1A01'
        NativeBeforePresent = $true
        NativeBeforeBytes = 940L
        NativeBeforeSha256 = '026A7F9BA45C53A5AC7608D7B7BE117D4E5B847C57518F768339AA8CFABD6844'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp'
        Bytes = 28044L
        Sha256 = '7926155C2AEEDABD19BC1C56534AEC3DE3C83E41D9382E5219B576E2D6EDA7F1'
        NativeBeforePresent = $true
        NativeBeforeBytes = 6167L
        NativeBeforeSha256 = '0110187660AE630858D774F3747923653DF4D9F40760DCFD6D7C86102CDCCEBD'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h'
        Bytes = 11566L
        Sha256 = '17419FF25CD3BFECCFF082635F8ED1C9668BC4EA4D4BCA7E5D5EAEDAF7EBAD06'
        NativeBeforePresent = $true
        NativeBeforeBytes = 10845L
        NativeBeforeSha256 = '9E66E7007BE43FCCD0A228342809C4281B9C5C7C9B5B145A7EDB1C0568C9309F'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp'
        Bytes = 722720L
        Sha256 = 'B4371454C084CBE47D52E31421808C867004AE0418A66D560973C5C850AC713F'
        NativeBeforePresent = $true
        NativeBeforeBytes = 719616L
        NativeBeforeSha256 = 'EA0D9E665571089B5B2C7E00E44B4E31C0B0E8C0ADDA9790D21780CEFB074EF0'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h'
        Bytes = 11654L
        Sha256 = 'EE0A2CE3FAC359B7DDDF808775E748D7E29214437EFD438456D7333819F31661'
        NativeBeforePresent = $true
        NativeBeforeBytes = 10522L
        NativeBeforeSha256 = 'E0EA591E419A1D023F8BD047429A3EF64D194FBAAEB73F1B989BADB7FA18C994'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
        Bytes = 452992L
        Sha256 = 'D82148FCB9E9B647B1D3DFD1C5E68582052EF6175160214771CF6398E6705317'
        NativeBeforePresent = $true
        NativeBeforeBytes = 432233L
        NativeBeforeSha256 = '1AB587FC5C2DC07827351E56537057BDD54C89341B44A872BFEA3D60C2A95C0C'
    }
)

# Exact Phase-2 compiled-source closure. These files are intentionally not
# promoted by R27, but the forced two-module build may compile them; byte drift
# is therefore refused before build and rechecked through commit.
$phase2CompiledSourcePins = @(
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseActor.cpp'; Bytes=22760L; Sha256='26765231E1E48196B75F786D640BC2E2DB872CDD665DC83F148F1D4623802E28' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseProvenance.h'; Bytes=5433L; Sha256='AC19B7B935D49F7C5CBD8DD7CCC891DF361B8E74B646ADB1FBD5C565ED91E1BC' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp'; Bytes=14772L; Sha256='A97778A9C4F4DC940C9F550C24376778999D28A5E7600467582EE397514CB992' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\Tests\TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp'; Bytes=10610L; Sha256='1B9EA89E7C86374C6B9D4D82DD8DCF3BD23CA8F3B35ECBA222074B5B836AFA90' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp'; Bytes=66908L; Sha256='6CE20C0BADBBC9291B739091B11E05436BCE71B928EBD3E276A52224EA06808C' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp'; Bytes=9627L; Sha256='81AC1C9EE3B708B1943B08C60A1AA922096A010A1631C9002AEE6DF2C2BC18AE' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DTemasekShophouseActor.h'; Bytes=4064L; Sha256='D72DD0458B10D3EE5038B2E7623D405BCECB55B944B0BCCA271DDB25140F89FC' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DTemasekShophouseAssetFactory.h'; Bytes=496L; Sha256='1CCF3D7F68B74325ABFBCBACCEA9ECD099EC8FA71D9A5A53C499DEE0157A4FB0' }
    [pscustomobject] @{ RelativePath='Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.h'; Bytes=1058L; Sha256='3225071442862102AB6B7005ACCA61B0E385B47D9834819584554C9C8F8CC95A' }
)

$immutableContentPins = @(
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_ContextMassing_Master.uasset'; Bytes = 29065L; Sha256 = '47B0F9F804858280B53D4D03CEEA8E078F01740920AF589EFEA676EBC4D057B4' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRender.uasset'; Bytes = 15159L; Sha256 = '113CE217898FD5A65603493FFF82B4B0FC16C6AD3C0339097F61E78B911E326F' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRoof.uasset'; Bytes = 14843L; Sha256 = '4534461C849FD80A1D1BA34FE68239414B25F1FF31F4790BD857C9F828A8C943' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRender.uasset'; Bytes = 15170L; Sha256 = '227415242B7F0D0E61F3474C57D605848D24EF7574CD67C2242DED75B22F8079' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRoof.uasset'; Bytes = 14693L; Sha256 = 'BF11B2C73A066E03923B83AADD9C1222CD80377B44A90EBFAE868454C05898EB' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset'; Bytes = 761010L; Sha256 = 'A2062A8A90CB5FFA4D0B6B1117E775230B39AF58CB57F3C26C792DD4861429EE' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\M_IPV5D_ContextFacadeR25_Master.uasset'; Bytes = 29079L; Sha256 = '37C7305FC07DD076902A243410F7CB094356F8CBD333461E29EF3AC47694AC03' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_OfficialWall.uasset'; Bytes = 14637L; Sha256 = '2C783BA3D045FBC37432D117C372C1B6654B94ECDC3E9C8C051263E2492036DA' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_OfficialRoof.uasset'; Bytes = 14709L; Sha256 = '98DE68DFB79083DF8D690C0222BAED926F414302E840A8C88950FD73DCE729D8' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_FallbackWall.uasset'; Bytes = 14599L; Sha256 = 'CD27BC83E1876E53B485F055D5D1F2E48999EE8CEAC733D7AB55F06CC6E81C9B' }
    [pscustomobject] @{ RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25\Materials\MI_IPV5D_ContextFacadeR25_FallbackRoof.uasset'; Bytes = 14792L; Sha256 = 'E8F78D776EB235F1F25304678CCAD7F64DA125376BA04126DE2CE3BABCA64A18' }
)

$r27MaterialRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials'))
$r27MaterialRelativePaths = @(
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials\M_IPV5D_LandmarkTurf_R27_Manicured.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials\M_IPV5D_LandmarkTurf_R27_Humid.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials\M_IPV5D_LandmarkTurf_R27_Shade.uasset'
    'Content\TRIAD\IstanaPublicViewExploreV5D\LandmarkVegetationR27\Materials\M_IPV5D_LandmarkTurf_R27_DryEdge.uasset'
)

$globalBuildRelativePaths = @(
    'Intermediate\Build\SourceFileCache.bin'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.deps'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtpath'
    'Intermediate\Build\Win64\x64\UnrealEditor\ActionHistory.bin'
    'Intermediate\Build\Win64\x64\UnrealEditor\Development\DependencyCache.bin'
    'Plugins\AirSimTriadRuntime\Intermediate\Build\Win64\UnrealEditor\Inc\AirSimTriadRuntime\UHT\Timestamp'
)

$r27DirectCompileObjectRelativePaths = @(
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusion\TRIADIstanaExploreV5DLandmarkVegetationActor.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp.obj'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\TRIADSensorFusionEditor\TRIADIstanaExploreV5DHybridEditorLibrary.cpp.obj'
)

function Test-ContainedPath {
    param([string] $Path, [string] $Root)
    $candidate = [IO.Path]::GetFullPath($Path)
    $boundary = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $candidate.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparseAncestor {
    param([string] $Path, [string] $StopRoot)
    $cursor = [IO.DirectoryInfo] [IO.Path]::GetFullPath($Path)
    $stop = [IO.Path]::GetFullPath($StopRoot).TrimEnd('\')
    while ($null -ne $cursor) {
        if (($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse points are forbidden in transaction paths: $($cursor.FullName)"
        }
        if ($cursor.FullName.TrimEnd('\').Equals(
                $stop, [StringComparison]::OrdinalIgnoreCase)) {
            return
        }
        $cursor = $cursor.Parent
    }
    throw "Path did not terminate at the expected root: $Path root=$StopRoot"
}

function Get-FileState {
    param([string] $Path)
    $full = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($full)) {
        return [pscustomobject] [ordered] @{
            Path = $full; Present = $false; Bytes = 0L; Sha256 = 'ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $full
    [pscustomobject] [ordered] @{
        Path = $full
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    }
}

function Assert-State {
    param($Expected, [string] $Path, [string] $Label)
    $actual = Get-FileState -Path $Path
    if ([bool] $actual.Present -ne [bool] $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label pin mismatch: expected=$($Expected | ConvertTo-Json -Compress) actual=$($actual | ConvertTo-Json -Compress)"
    }
    $actual
}

function Assert-BinaryContainsEncodedMarkers {
    param(
        [string] $Path,
        [string[]] $Markers,
        [string] $Label
    )
    $bytes = [IO.File]::ReadAllBytes([IO.Path]::GetFullPath($Path))
    if ($bytes.Length -le 0) {
        throw "$Label is absent or empty: $Path"
    }
    # Unreal TEXT() literals are UTF-16LE on Windows, while reflection/symbol
    # strings can be narrow UTF-8/ASCII. Search exact encoded byte sequences,
    # not an ASCII-decoded DLL: the latter inserts NULs between TCHARs and
    # falsely rejects valid code. Hex substring matching is alignment agnostic
    # and remains valid when the linker pools a literal as part of a longer one.
    $binaryHex = [Convert]::ToHexString($bytes)
    foreach ($marker in $Markers) {
        $utf8Hex = [Convert]::ToHexString(
            [Text.Encoding]::UTF8.GetBytes($marker))
        $utf16LeHex = [Convert]::ToHexString(
            [Text.Encoding]::Unicode.GetBytes($marker))
        if (-not $binaryHex.Contains(
                $utf8Hex, [StringComparison]::Ordinal) -and
            -not $binaryHex.Contains(
                $utf16LeHex, [StringComparison]::Ordinal)) {
            throw "$Label lacks R27 encoded marker (UTF-8 or UTF-16LE): $marker"
        }
    }
}

function Assert-SourcePins {
    param([switch] $NativeBefore, [switch] $NativeAfter)
    foreach ($pin in $sourcePins) {
        $repo = [IO.Path]::GetFullPath((Join-Path $repositoryUnrealRoot `
            $pin.RelativePath))
        $expectedRepo = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expectedRepo $repo 'repository source')
        if ($NativeBefore) {
            $expectedNative = [pscustomobject] @{
                Present = $pin.NativeBeforePresent
                Bytes = $pin.NativeBeforeBytes
                Sha256 = $pin.NativeBeforeSha256
            }
            [void] (Assert-State $expectedNative `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'native R25 source predecessor')
        }
        if ($NativeAfter) {
            [void] (Assert-State $expectedRepo `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'promoted native R27 source')
        }
    }
}

function Assert-Phase2CompiledSourcePins {
    param([switch] $Native)
    foreach ($pin in $phase2CompiledSourcePins) {
        $expected = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expected `
            (Join-Path $repositoryUnrealRoot $pin.RelativePath) `
            'repository Phase-2 compiled source')
        if ($Native) {
            [void] (Assert-State $expected `
                (Join-Path $nativeProjectRoot $pin.RelativePath) `
                'native committed Phase-2 compiled source')
        }
    }
}

function Assert-ImmutableContent {
    foreach ($pin in $immutableContentPins) {
        $expected = [pscustomobject] @{
            Present = $true; Bytes = $pin.Bytes; Sha256 = $pin.Sha256
        }
        [void] (Assert-State $expected `
            (Join-Path $nativeProjectRoot $pin.RelativePath) `
            'immutable R25/V2 content')
    }
}

function Get-R27MaterialInventory {
    if (-not [IO.Directory]::Exists($r27MaterialRoot)) { return @() }
    @(Get-ChildItem -LiteralPath $r27MaterialRoot -File -Recurse |
        ForEach-Object { [IO.Path]::GetFullPath($_.FullName) } |
        Sort-Object)
}

function Assert-R27MaterialPredecessor {
    $inventory = @(Get-R27MaterialInventory)
    if ($inventory.Count -ne 0) {
        throw "R27 material predecessor must be exactly empty; actual=$([string]::Join(',', $inventory))"
    }
}

function Get-R27MaterialSuccessorPins {
    $inventory = @(Get-R27MaterialInventory)
    $expected = @($r27MaterialRelativePaths | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_))
    } | Sort-Object)
    if (($inventory | ConvertTo-Json -Compress) -cne
        ($expected | ConvertTo-Json -Compress)) {
        throw "R27 material successor must contain exactly four package files; actual=$([string]::Join(',', $inventory))"
    }
    @($expected | ForEach-Object {
        $state = Get-FileState $_
        if (-not $state.Present -or $state.Bytes -le 0 -or
            $state.Sha256.Length -ne 64) {
            throw "R27 material successor package is absent or empty: $_"
        }
        $state
    })
}

function Assert-R27MaterialPins {
    param([object[]] $Pins)
    if ($Pins.Count -ne 4) { throw 'R27 material pin roster must contain four files.' }
    foreach ($pin in $Pins) {
        [void] (Assert-State $pin $pin.Path 'R27 material successor')
    }
    [void] (Get-R27MaterialSuccessorPins)
}

function Get-ProcessRecord {
    param([uint32] $ProcessId)
    $cim = Get-CimInstance Win32_Process -Filter "ProcessId=$ProcessId" `
        -ErrorAction Stop
    if ($null -eq $cim) { return $null }
    $exe = [string] $cim.ExecutablePath
    $commandLine = [string] $cim.CommandLine
    if ([string]::IsNullOrWhiteSpace($exe) -or
        [string]::IsNullOrWhiteSpace($commandLine) -or
        $null -eq $cim.CreationDate) {
        # Win32_Process can be observable before these identity fields are
        # populated. Callers poll instead of treating this partial row as an
        # owned process identity.
        return $null
    }
    $creation = [DateTime] $cim.CreationDate
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $ProcessId
        ExecutablePath = [IO.Path]::GetFullPath($exe)
        CommandLine = $commandLine
        StartTimeUtcTicks = $creation.ToUniversalTime().Ticks
    }
}

function Test-Win32ProcessPresent {
    param([uint32] $ProcessId)
    $null -ne (Get-CimInstance Win32_Process `
        -Filter "ProcessId=$ProcessId" -ErrorAction Stop)
}

function Test-ProcessRecordEqual {
    param($Expected, $Actual)
    $null -ne $Expected -and $null -ne $Actual -and
        [uint32] $Actual.ProcessId -eq [uint32] $Expected.ProcessId -and
        [int64] $Actual.StartTimeUtcTicks -eq
            [int64] $Expected.StartTimeUtcTicks -and
        ([string] $Actual.ExecutablePath).Equals(
            [string] $Expected.ExecutablePath,
            [StringComparison]::OrdinalIgnoreCase) -and
        ([string] $Actual.CommandLine).Equals(
            [string] $Expected.CommandLine,
            [StringComparison]::Ordinal)
}

function Get-ProtectedSnapshot {
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($cim in @(Get-CimInstance Win32_Process -Filter `
            "Name='UnrealEditor.exe'" -ErrorAction Stop)) {
        if ([string]::IsNullOrWhiteSpace([string] $cim.ExecutablePath) -or
            -not ([IO.Path]::GetFullPath([string] $cim.ExecutablePath).Equals(
                $protectedEditor, [StringComparison]::OrdinalIgnoreCase)) -or
            -not ([string] $cim.CommandLine).Contains(
                $protectedProject, [StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        $record = Get-ProcessRecord -ProcessId ([uint32] $cim.ProcessId)
        if ($null -ne $record) { $rows.Add($record) }
    }
    @($rows | Sort-Object ProcessId)
}

function Assert-ProtectedUnchanged {
    param([object[]] $Expected)
    $actual = @(Get-ProtectedSnapshot)
    if (($Expected | ConvertTo-Json -Depth 6 -Compress) -cne
        ($actual | ConvertTo-Json -Depth 6 -Compress)) {
        throw 'The protected UE5.4 CAPSTONE process identity set changed.'
    }
}

function Get-RcOwners {
    # Querying all listeners makes a successful zero-row port filter distinct
    # from a failed authority query. Cleanup and rollback must fail closed if
    # the TCP ownership provider is unavailable.
    @((Get-NetTCPConnection -State Listen -ErrorAction Stop |
        Where-Object { [uint16] $_.LocalPort -eq 30010 } |
        Select-Object -ExpandProperty OwningProcess -Unique))
}

function Assert-NativeIdle {
    param([string] $Checkpoint)
    $offenders = [Collections.Generic.List[string]]::new()
    foreach ($cim in @(Get-CimInstance Win32_Process -ErrorAction Stop |
            Where-Object { $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') })) {
        $exe = [string] $cim.ExecutablePath
        $cmd = [string] $cim.CommandLine
        if ([string]::IsNullOrWhiteSpace($exe) -or
            [string]::IsNullOrWhiteSpace($cmd)) {
            $offenders.Add(
                "pid=$($cim.ProcessId) exe=<identity-incomplete> cmd=<identity-incomplete>")
        }
        elseif (([IO.Path]::GetFullPath($exe).StartsWith(
                $engineRoot, [StringComparison]::OrdinalIgnoreCase)) -or
            $cmd.Contains($nativeProjectFile, [StringComparison]::OrdinalIgnoreCase)) {
            $offenders.Add("pid=$($cim.ProcessId) exe=$exe cmd=$cmd")
        }
    }
    if ($offenders.Count -ne 0 -or @(Get-RcOwners).Count -ne 0) {
        throw "Native Unreal/RC boundary is not idle at ${Checkpoint}: $([string]::Join(' | ', @($offenders))) rc=$([string]::Join(',', @(Get-RcOwners)))"
    }
}

function Invoke-RcCall {
    param([string] $ObjectPath, [string] $FunctionName,
        [hashtable] $Parameters = @{}, [int] $TimeoutSec = 60)
    $payload = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 12 -Compress
    Invoke-RestMethod -Method Put -Uri $rcUri -ContentType 'application/json' `
        -Body $payload -TimeoutSec $TimeoutSec
}

function Assert-HelperIdentity {
    param($Expected, [Diagnostics.Process] $Handle)
    $actual = Get-ProcessRecord -ProcessId ([uint32] $Handle.Id)
    if (-not (Test-ProcessRecordEqual $Expected $actual) -or
        [uint32] $Handle.Id -ne [uint32] $Expected.ProcessId -or
        -not $actual.ExecutablePath.Equals(
            $editor, [StringComparison]::OrdinalIgnoreCase) -or
        -not $actual.CommandLine.Contains(
            $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Owned Unreal helper identity changed.'
    }
    $actual
}

function Wait-ExpectedHelperIdentity {
    param(
        [Diagnostics.Process] $Handle,
        [string] $MapPackage,
        [string] $Log,
        [int64] $LaunchStartTimeUtcTicks,
        [int] $TimeoutSeconds = 20
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $candidate = $null
        try {
            $candidate = Get-ProcessRecord -ProcessId ([uint32] $Handle.Id)
        }
        catch {
            # Process identity fields can be transiently unavailable during
            # editor startup. Keep the exact launched PID under observation.
        }
        if ($null -ne $candidate -and
            [uint32] $candidate.ProcessId -eq [uint32] $Handle.Id -and
            ($LaunchStartTimeUtcTicks -le 0 -or
             [Math]::Abs(
                [int64] $candidate.StartTimeUtcTicks -
                $LaunchStartTimeUtcTicks) -le
                    [TimeSpan]::FromSeconds(2).Ticks) -and
            $candidate.ExecutablePath.Equals(
                $editor, [StringComparison]::OrdinalIgnoreCase) -and
            $candidate.CommandLine.Contains(
                $nativeProjectFile, [StringComparison]::OrdinalIgnoreCase) -and
            $candidate.CommandLine.Contains(
                $MapPackage, [StringComparison]::Ordinal) -and
            $candidate.CommandLine.Contains(
                $Log, [StringComparison]::OrdinalIgnoreCase)) {
            return $candidate
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    $null
}

function Wait-ExactHelperExit {
    param(
        $Identity,
        [ValidateRange(1, 300)] [int] $TimeoutSeconds
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $actual = Get-ProcessRecord -ProcessId ([uint32] $Identity.ProcessId)
        $owners = @(Get-RcOwners)
        if ($null -eq $actual) {
            if (-not (Test-Win32ProcessPresent `
                        ([uint32] $Identity.ProcessId)) -and
                $owners -notcontains [uint32] $Identity.ProcessId) {
                return $true
            }
        }
        elseif (-not (Test-ProcessRecordEqual $Identity $actual)) {
            throw 'PID reuse or exact helper identity drift occurred while waiting for exit.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    $false
}

function Wait-LaunchedHelperBoundaryReleased {
    param(
        [uint32] $LaunchProcessId,
        [ValidateRange(1, 300)] [int] $TimeoutSeconds
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $present = Test-Win32ProcessPresent $LaunchProcessId
        $owners = @(Get-RcOwners)
        if (-not $present -and $owners.Count -eq 0) {
            return $true
        }
        if ($owners.Count -ne 0 -and
            $owners -notcontains $LaunchProcessId) {
            throw 'A foreign RC owner appeared while containing the launched helper.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    $false
}

function Stop-LaunchedHelperBeforeIdentity {
    param(
        [Diagnostics.Process] $Handle,
        [uint32] $LaunchProcessId,
        [int64] $LaunchStartTimeUtcTicks
    )
    if ([uint32] $Handle.Id -ne $LaunchProcessId) {
        throw 'Returned process handle no longer matches its launch PID receipt.'
    }
    $handleStartTicks = $Handle.StartTime.ToUniversalTime().Ticks
    if ($LaunchStartTimeUtcTicks -gt 0 -and
        [int64] $handleStartTicks -ne $LaunchStartTimeUtcTicks) {
        throw 'Returned process handle no longer matches its launch-time receipt.'
    }
    # Identity never became queryable, so RC and PID-based termination are
    # forbidden. Kill only through the exact Process object returned by this
    # wrapper's Start-Process call, then use fail-closed Win32/RC absence as
    # the authoritative containment boundary.
    $Handle.Kill()
    if (-not (Wait-LaunchedHelperBoundaryReleased $LaunchProcessId 30)) {
        throw 'Unidentified launched helper did not exit after handle containment.'
    }
}

function Stop-OwnedHelper {
    param($Identity, [Diagnostics.Process] $Handle)
    if ([uint32] $Handle.Id -ne [uint32] $Identity.ProcessId) {
        throw 'Process handle does not match the exact owned Unreal helper.'
    }
    $actual = Get-ProcessRecord -ProcessId ([uint32] $Identity.ProcessId)
    if ($null -eq $actual) {
        if ((Test-Win32ProcessPresent ([uint32] $Identity.ProcessId)) -or
            @(Get-RcOwners).Count -ne 0) {
            throw 'Owned helper identity is incomplete or its RC listener remains.'
        }
        return
    }
    [void] (Assert-HelperIdentity $Identity $Handle)

    # A stage can fail before RC finishes starting. Give only the exact owned
    # PID a bounded chance to expose port 30010, then request graceful exit.
    $quitRequested = $false
    $gracefulDeadline = [DateTime]::UtcNow.AddSeconds(
        [Math]::Min(60, $ShutdownTimeoutSeconds))
    do {
        [void] (Assert-HelperIdentity $Identity $Handle)
        $owners = @(Get-RcOwners)
        if ($owners.Count -eq 1 -and
            [uint32] $owners[0] -eq [uint32] $Identity.ProcessId) {
            try {
                [void] (Invoke-RcCall $quitLibrary 'QuitEditor' @{} 30)
            }
            catch {
                # The socket can close before QuitEditor returns.
            }
            $quitRequested = $true
            break
        }
        if ($owners.Count -ne 0) {
            throw 'RC port ownership changed before exact helper shutdown.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $gracefulDeadline)

    if ($quitRequested -and
        (Wait-ExactHelperExit $Identity $ShutdownTimeoutSeconds)) {
        if (@(Get-RcOwners).Count -ne 0) {
            throw 'RC listener remained after exact helper graceful exit.'
        }
        return
    }

    # Forced containment is permitted only after re-proving PID, creation
    # ticks, executable, and full command line. Never trust HasExited or
    # WaitForExit alone; Win32 and RC are the authoritative exit boundaries.
    [void] (Assert-HelperIdentity $Identity $Handle)
    $Handle.Kill()
    if (-not (Wait-ExactHelperExit $Identity 30)) {
        throw 'Exact owned Unreal helper did not exit after containment.'
    }
    if (@(Get-RcOwners).Count -ne 0) {
        throw 'RC listener remained after exact helper containment.'
    }
}

function Invoke-ColdStage {
    param(
        [string] $Stage,
        [string] $ObjectPath,
        [string] $FunctionName,
        [string] $TextProperty,
        [string] $ExpectedPrefix,
        [hashtable] $Parameters = @{}
    )
    Assert-NativeIdle "before $Stage"
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-SourcePins -NativeAfter
    Assert-Phase2CompiledSourcePins -Native
    Assert-ImmutableContent
    $logDir = Join-Path $transactionRoot 'logs'
    [void] [IO.Directory]::CreateDirectory($logDir)
    $log = [IO.Path]::GetFullPath((Join-Path $logDir "$Stage.log"))
    if ([IO.File]::Exists($log)) { throw "Stage log already exists: $log" }
    $argumentLine =
        "`"$nativeProjectFile`" $mapPackage -DisablePlugin=AirSim " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-DDC=InstalledNoZenLocalFallback ' +
        "-LocalDataCachePath=`"$ddcRoot`" " +
        '-RemoteControlHttpServer -RCWebControlEnable ' +
        '-ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$log`""
    $handle = Start-Process -FilePath $editor -ArgumentList $argumentLine `
        -WorkingDirectory $nativeProjectRoot -PassThru -WindowStyle Hidden
    # Register the returned process handle immediately. If identity discovery
    # fails during the startup race, both this stage and the outer transaction
    # finally block still retain an exact cleanup target.
    $script:activeHelperHandle = $handle
    $script:activeHelperIdentity = $null
    $script:activeHelperMapPackage = $mapPackage
    $script:activeHelperLog = $log
    $launchProcessId = [uint32] $handle.Id
    $script:activeHelperLaunchProcessId = $launchProcessId
    $launchStartTimeUtcTicks = 0L
    try {
        $launchStartTimeUtcTicks =
            $handle.StartTime.ToUniversalTime().Ticks
    }
    catch {
        # The unique run-token log path still binds identity while Win32
        # CreationDate is acquired by the bounded identity poll below.
    }
    $script:activeHelperLaunchStartTimeUtcTicks =
        $launchStartTimeUtcTicks
    $identity = $null
    $response = $null
    $stageError = $null
    try {
        $identity = Wait-ExpectedHelperIdentity `
            -Handle $handle -MapPackage $mapPackage -Log $log `
            -LaunchStartTimeUtcTicks $launchStartTimeUtcTicks `
            -TimeoutSeconds 20
        if ($null -eq $identity) { throw "Could not prove helper identity for $Stage" }
        $script:activeHelperIdentity = $identity

        $readyDeadline = [DateTime]::UtcNow.AddSeconds($EditorTimeoutSeconds)
        $identityResponse = $null
        do {
            Assert-ProtectedUnchanged $script:protectedBefore
            [void] (Assert-HelperIdentity $identity $handle)
            $owners = @(Get-RcOwners)
            if ($owners.Count -eq 1 -and
                [uint32] $owners[0] -eq [uint32] $identity.ProcessId) {
                try {
                    $identityResponse = Invoke-RcCall $identityLibrary `
                        'ValidateIstanaExploreRemoteControlProject' `
                        @{ ExpectedProjectPath = $nativeProjectRoot } 30
                }
                catch { $identityResponse = $null }
                if ($null -ne $identityResponse -and
                    $identityResponse.ReturnValue -eq $true) { break }
            }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $readyDeadline)
        if ($null -eq $identityResponse -or
            $identityResponse.ReturnValue -ne $true) {
            throw "Could not prove exact project/RC ownership for $Stage"
        }
        [void] (Assert-HelperIdentity $identity $handle)
        $response = Invoke-RcCall $ObjectPath $FunctionName $Parameters `
            $EditorTimeoutSeconds
        [void] (Assert-HelperIdentity $identity $handle)
        $text = [string] $response.$TextProperty
        if ($response.ReturnValue -ne $true -or
            -not $text.StartsWith($ExpectedPrefix, [StringComparison]::Ordinal)) {
            throw "Stage $Stage failed exact response gate: $FunctionName response=$text"
        }
    }
    catch { $stageError = $_.Exception }
    finally {
        if ($null -eq $identity) {
            try {
                # A startup error must not make the exact process unowned.
                # Re-prove all Win32 identity fields before any shutdown call.
                $identity = Wait-ExpectedHelperIdentity `
                    -Handle $handle -MapPackage $mapPackage -Log $log `
                    -LaunchStartTimeUtcTicks $launchStartTimeUtcTicks `
                    -TimeoutSeconds 30
                if ($null -eq $identity) {
                    throw 'Could not recover exact helper identity during cleanup.'
                }
                $script:activeHelperIdentity = $identity
            }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
            }
        }
        if ($null -ne $identity) {
            try { Stop-OwnedHelper $identity $handle }
            catch {
                if ($null -eq $stageError) { $stageError = $_.Exception }
                else {
                    $stageError = [InvalidOperationException]::new(
                        "Stage error={$($stageError.Message)}; exact helper cleanup error={$($_.Exception.Message)}")
                }
            }
        }
        # Even a fully proven identity can become temporarily unavailable
        # inside Stop-OwnedHelper. If its graceful/re-proven path throws while
        # the launch PID remains, contain through the immutable returned
        # handle receipt before releasing rollback.
        try {
            if (Test-Win32ProcessPresent $launchProcessId) {
                Stop-LaunchedHelperBeforeIdentity `
                    -Handle $handle `
                    -LaunchProcessId $launchProcessId `
                    -LaunchStartTimeUtcTicks $launchStartTimeUtcTicks
            }
            elseif (@(Get-RcOwners).Count -ne 0) {
                throw 'RC listener remains after launched helper exit.'
            }
        }
        catch {
            if ($null -eq $stageError) { $stageError = $_.Exception }
            else {
                $stageError = [InvalidOperationException]::new(
                    "Stage error={$($stageError.Message)}; launch-handle fallback containment error={$($_.Exception.Message)}")
            }
        }
    }
    $postCleanupErrors = [Collections.Generic.List[string]]::new()
    try { Assert-ProtectedUnchanged $script:protectedBefore }
    catch { $postCleanupErrors.Add($_.Exception.Message) }
    try { Assert-NativeIdle "after $Stage" }
    catch { $postCleanupErrors.Add($_.Exception.Message) }
    try {
        if ((Test-Win32ProcessPresent $launchProcessId) -or
            @(Get-RcOwners).Count -ne 0) {
            throw 'Exact launched helper/RC boundary remains after stage cleanup.'
        }
    }
    catch { $postCleanupErrors.Add($_.Exception.Message) }
    if ($postCleanupErrors.Count -ne 0) {
        $original = if ($null -eq $stageError) { 'none' }
            else { $stageError.Message }
        throw "Stage $Stage cleanup boundary failed: original={$original} cleanup={$([string]::Join(' | ', @($postCleanupErrors)))}"
    }
    $script:activeHelperHandle = $null
    $script:activeHelperIdentity = $null
    $script:activeHelperMapPackage = $null
    $script:activeHelperLog = $null
    $script:activeHelperLaunchProcessId = 0
    $script:activeHelperLaunchStartTimeUtcTicks = 0L
    if ($null -ne $stageError) { throw $stageError }
    if (-not [IO.File]::Exists($log) -or (Get-Item $log).Length -le 0) {
        throw "Stage $Stage did not persist a non-empty log."
    }
    [pscustomobject] [ordered] @{
        Stage = $Stage
        Function = $FunctionName
        ExpectedPrefix = $ExpectedPrefix
        Message = [string] $response.$TextProperty
        ProcessIdentity = $identity
        Log = Get-FileState $log
    }
}

function Get-BuildSurfacePaths {
    $paths = [Collections.Generic.List[string]]::new()
    foreach ($root in @($pluginBinaryRoot, $pluginIntermediateRoot)) {
        if ([IO.Directory]::Exists($root)) {
            foreach ($file in @(Get-ChildItem -LiteralPath $root -File -Recurse)) {
                $paths.Add([IO.Path]::GetFullPath($file.FullName))
            }
        }
    }
    foreach ($relative in $globalBuildRelativePaths) {
        $paths.Add([IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $relative)))
    }
    @($paths | Sort-Object -Unique)
}

function New-FileJournal {
    param([string[]] $Paths, [string] $BackupRoot)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($path in @($Paths | Sort-Object -Unique)) {
        if (-not (Test-ContainedPath $path $nativeProjectRoot)) {
            throw "Journal target escaped native project: $path"
        }
        $state = Get-FileState $path
        $relative = [IO.Path]::GetRelativePath($nativeProjectRoot, $path)
        $backup = [IO.Path]::GetFullPath((Join-Path $BackupRoot $relative))
        if ($state.Present) {
            [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup))
            Copy-Item -LiteralPath $path -Destination $backup
            [void] (Assert-State $state $backup 'journal backup')
        }
        $rows.Add([pscustomobject] [ordered] @{
            Path = $path
            RelativePath = $relative
            Before = $state
            Backup = if ($state.Present) { $backup } else { $null }
        })
    }
    @($rows)
}

function Remove-R27DirectCompileObjectsForForcedRebuild {
    param([object[]] $BuildRows)
    if ($r27DirectCompileObjectRelativePaths.Count -ne 4) {
        throw 'R27 direct compile-object roster must contain four translation units.'
    }
    $removed = [Collections.Generic.List[object]]::new()
    foreach ($relative in $r27DirectCompileObjectRelativePaths) {
        $matches = @($BuildRows | Where-Object {
                [string] $_.RelativePath -ceq [string] $relative
            })
        if ($matches.Count -ne 1) {
            throw "R27 direct compile object lacks one exact rollback row: $relative"
        }
        $row = $matches[0]
        [void] (Assert-State $row.Before $row.Path `
            'R27 direct compile object predecessor')
        if ([IO.File]::Exists($row.Path)) {
            $item = Get-Item -LiteralPath $row.Path -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "R27 direct compile object is a reparse file: $($row.Path)"
            }
            Remove-Item -LiteralPath $row.Path -Force
        }
        $absent = [pscustomobject] @{
            Present = $false; Bytes = 0L; Sha256 = 'ABSENT'
        }
        [void] (Assert-State $absent $row.Path `
            'R27 forced compile-object invalidation')
        $removed.Add($row)
    }
    @($removed)
}

function Restore-FileJournal {
    param([object[]] $Rows)
    foreach ($row in @($Rows | Sort-Object RelativePath -Descending)) {
        if ($row.Before.Present) {
            [void] [IO.Directory]::CreateDirectory(
                [IO.Path]::GetDirectoryName($row.Path))
            Copy-Item -LiteralPath $row.Backup -Destination $row.Path -Force
        }
        elseif ([IO.File]::Exists($row.Path)) {
            Remove-Item -LiteralPath $row.Path -Force
        }
        [void] (Assert-State $row.Before $row.Path 'rollback state')
    }
}

function Restore-BuildSurface {
    param([object[]] $Rows)
    $before = @{}
    foreach ($row in $Rows) { $before[$row.Path.ToLowerInvariant()] = $true }
    foreach ($path in @(Get-BuildSurfacePaths)) {
        if (-not $before.ContainsKey($path.ToLowerInvariant()) -and
            [IO.File]::Exists($path)) {
            if (-not (Test-ContainedPath $path $pluginBinaryRoot) -and
                -not (Test-ContainedPath $path $pluginIntermediateRoot) -and
                -not ($globalBuildRelativePaths -contains
                    [IO.Path]::GetRelativePath($nativeProjectRoot, $path))) {
                throw "Rollback refused non-allowlisted new build file: $path"
            }
            Remove-Item -LiteralPath $path -Force
        }
    }
    Restore-FileJournal $Rows
}

function Restore-MapFromJournal {
    param($MapRow)
    $temporary = [IO.Path]::GetFullPath((Join-Path `
        ([IO.Path]::GetDirectoryName($mapFile)) `
        ("TRIAD_R27_Wrapper_Restore_{0}.tmp" -f [Guid]::NewGuid().ToString('N'))))
    if (-not (Test-ContainedPath $temporary `
            ([IO.Path]::GetDirectoryName($mapFile)))) {
        throw 'Map rollback temporary escaped the map directory.'
    }
    Copy-Item -LiteralPath $MapRow.Backup -Destination $temporary
    [void] (Assert-State $MapRow.Before $temporary 'map rollback temporary')
    Move-Item -LiteralPath $temporary -Destination $mapFile -Force
    [void] (Assert-State $MapRow.Before $mapFile 'restored R25 map')
}

function Assert-StaticContract {
    if ($sourcePins.Count -ne 8) { throw 'Source roster must contain eight files.' }
    if ($phase2CompiledSourcePins.Count -ne 9) {
        throw 'Phase-2 compiled-source closure must contain nine files.'
    }
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($pin in $sourcePins) {
        if (-not $seen.Add($pin.RelativePath)) {
            throw "Duplicate source path: $($pin.RelativePath)"
        }
        if ([IO.Path]::IsPathRooted($pin.RelativePath) -or
            $pin.RelativePath.Contains('..')) {
            throw "Non-canonical source path: $($pin.RelativePath)"
        }
    }
    if (($sourcePins | Where-Object NativeBeforePresent).Count -ne 8) {
        throw 'R27 source predecessor roster must be exactly eight committed R26/Phase-2 files.'
    }
    Assert-SourcePins
    Assert-Phase2CompiledSourcePins
    $header = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h') -Raw
    $assetHeader = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h') -Raw
    $source = Get-Content -LiteralPath (Join-Path $repositoryUnrealRoot `
        'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp') -Raw
    foreach ($endpoint in @(
        'ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap',
        'ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap')) {
        if (-not $header.Contains($endpoint, [StringComparison]::Ordinal) -or
            -not $source.Contains($endpoint, [StringComparison]::Ordinal)) {
            throw "Missing R27 endpoint in pinned repo source: $endpoint"
        }
    }
    foreach ($endpoint in @(
        'BuildOrValidateLandmarkGrassMaterialsR27',
        'ValidateLandmarkGrassMaterialsR27')) {
        if (-not $assetHeader.Contains($endpoint, [StringComparison]::Ordinal)) {
            throw "Missing R27 asset endpoint in pinned repo source: $endpoint"
        }
    }
    [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'STATIC_SELF_CHECK_PASS'
        SourceCount = $sourcePins.Count
        Phase2CompiledSourceCount = $phase2CompiledSourcePins.Count
        NativeTreeReadOrWritten = $false
        UnrealBuildOrEditorLaunched = $false
        MapPin = $expectedMapPin
        GrassInstances = 3072
        GrassCullCm = @(6500, 9000)
        WpoDisableCm = 2400
        VisualCaptureAccepted = $false
        CaptureRevalidationRequired = $true
    }
}

$static = Assert-StaticContract
if ($StaticSelfCheck) {
    $static | ConvertTo-Json -Depth 12
    return
}

if ($Execute) {
    foreach ($name in @(
            'ExpectedMapBytes', 'ExpectedMapSha256',
            'ExpectedRuntimeDllBytes', 'ExpectedRuntimeDllSha256',
            'ExpectedEditorDllBytes', 'ExpectedEditorDllSha256')) {
        if (-not $PSBoundParameters.ContainsKey($name)) {
            throw "Live R27 execution requires explicit caller-supplied -$name from the committed Temasek Phase-2 receipt."
        }
    }
    if (-not $RequireTemasekPhase2) {
        throw 'Live R27 execution requires -RequireTemasekPhase2.'
    }
}

foreach ($directory in @(
        $repositoryUnrealRoot,
        $nativeProjectRoot,
        $engineRoot,
        [IO.Path]::GetDirectoryName($transactionBase))) {
    if (-not [IO.Directory]::Exists($directory)) {
        throw "Required directory is absent: $directory"
    }
}
Assert-NoReparseAncestor $nativeProjectRoot ([IO.Path]::GetPathRoot($nativeProjectRoot))
Assert-NoReparseAncestor ([IO.Path]::GetDirectoryName($transactionBase)) `
    $nativeProjectRoot
$r27ExistingBoundary = if ([IO.Directory]::Exists($r27MaterialRoot)) {
    $r27MaterialRoot
}
else {
    [IO.Path]::GetDirectoryName(
        [IO.Path]::GetDirectoryName($r27MaterialRoot))
}
Assert-NoReparseAncestor $r27ExistingBoundary $nativeProjectRoot
$script:protectedBefore = @(Get-ProtectedSnapshot)
Assert-ProtectedUnchanged $script:protectedBefore
Assert-NativeIdle 'preflight'
[void] (Assert-State $expectedProjectPin $nativeProjectFile 'project')
[void] (Assert-State $expectedMapPin $mapFile 'Temasek Phase-2 predecessor map')
[void] (Assert-State $expectedRuntimeDllPin $runtimeDll 'Temasek Phase-2 runtime DLL')
[void] (Assert-State $expectedEditorDllPin $editorDll 'Temasek Phase-2 editor DLL')
Assert-SourcePins -NativeBefore
Assert-Phase2CompiledSourcePins -Native
Assert-ImmutableContent
Assert-R27MaterialPredecessor
if ([IO.Directory]::Exists($transactionRoot) -or
    [IO.File]::Exists($transactionRoot)) {
    throw "Transaction path already exists and will not be reused: $transactionRoot"
}

$preflight = [pscustomobject] [ordered] @{
    Schema = $schema
    Status = if ($Execute) { 'EXECUTION_PREFLIGHT_PASS' } else { 'READ_ONLY_PREFLIGHT_PASS' }
    ExecuteRequested = [bool] $Execute
    SourceCount = $sourcePins.Count
    Phase2CompiledSourceCount = $phase2CompiledSourcePins.Count
    Map = Get-FileState $mapFile
    RuntimeDll = Get-FileState $runtimeDll
    EditorDll = Get-FileState $editorDll
    ImmutableContentCount = $immutableContentPins.Count
    R27MaterialPredecessorCount = @(Get-R27MaterialInventory).Count
    ProtectedUE54Sessions = @($script:protectedBefore)
    NativeTreeWritten = $false
    UnrealBuildOrEditorLaunched = $false
}
if (-not $Execute) {
    $preflight | ConvertTo-Json -Depth 12
    return
}

[void] [IO.Directory]::CreateDirectory($transactionRoot)
Assert-NoReparseAncestor $transactionRoot $nativeProjectRoot
$journalRoot = Join-Path $transactionRoot 'journal'
[void] [IO.Directory]::CreateDirectory($journalRoot)
Assert-NoReparseAncestor $journalRoot $nativeProjectRoot
$sourcePaths = @($sourcePins | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath))
})
$contentPaths = @($immutableContentPins | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_.RelativePath))
}) + @($r27MaterialRelativePaths | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_))
})
$sourceJournal = @(New-FileJournal $sourcePaths `
    (Join-Path $journalRoot 'source'))
$mapJournal = @(New-FileJournal @($mapFile) `
    (Join-Path $journalRoot 'map'))
$contentJournal = @(New-FileJournal $contentPaths `
    (Join-Path $journalRoot 'content'))
$buildPathsBefore = @(
    @(Get-BuildSurfacePaths) +
    @($r27DirectCompileObjectRelativePaths | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot $_))
    }) | Sort-Object -Unique
)
$buildJournal = @(New-FileJournal $buildPathsBefore `
    (Join-Path $journalRoot 'build'))
$stageResults = [Collections.Generic.List[object]]::new()
$committed = $false
$transactionError = $null
$rollbackReport = $null
$script:activeHelperHandle = $null
$script:activeHelperIdentity = $null
$script:activeHelperMapPackage = $null
$script:activeHelperLog = $null
$script:activeHelperLaunchProcessId = 0
$script:activeHelperLaunchStartTimeUtcTicks = 0L

try {
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'final prewrite gate'
    [void] (Assert-State $expectedProjectPin $nativeProjectFile 'final project')
    [void] (Assert-State $expectedMapPin $mapFile 'final Phase-2 map')
    [void] (Assert-State $expectedRuntimeDllPin $runtimeDll 'final runtime DLL')
    [void] (Assert-State $expectedEditorDllPin $editorDll 'final editor DLL')
    Assert-SourcePins -NativeBefore
    Assert-Phase2CompiledSourcePins -Native
    Assert-ImmutableContent
    Assert-R27MaterialPredecessor

    foreach ($pin in $sourcePins) {
        $source = Join-Path $repositoryUnrealRoot $pin.RelativePath
        $target = Join-Path $nativeProjectRoot $pin.RelativePath
        [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
        Copy-Item -LiteralPath $source -Destination $target -Force
    }
    Assert-SourcePins -NativeAfter
    Assert-Phase2CompiledSourcePins -Native
    [void] (Assert-State $expectedMapPin $mapFile 'post-promotion map')
    Assert-ImmutableContent

    $forcedDirectCompileObjects = @(
        Remove-R27DirectCompileObjectsForForcedRebuild $buildJournal)
    $buildLog = Join-Path $transactionRoot 'build.log'
    $buildArguments = @(
        'UnrealEditor', 'Win64', 'Development', $nativeProjectFile,
        '-DisablePlugin=AirSim',
        '-Module=TRIADSensorFusion',
        '-Module=TRIADSensorFusionEditor',
        '-WaitMutex',
        '-NoHotReloadFromIDE',
        '-NoUBTMakefiles',
        '-ForceHeaderGeneration',
        '-MaxParallelActions=1',
        '-NoUBA',
        '-NoUBALocal'
    )
    & $buildTool @buildArguments *> $buildLog
    if ($LASTEXITCODE -ne 0) {
        throw "R27 two-module build failed with exit code $LASTEXITCODE. Log=$buildLog"
    }
    $buildLogText = Get-Content -LiteralPath $buildLog -Raw
    foreach ($row in $forcedDirectCompileObjects) {
        $leaf = [IO.Path]::GetFileNameWithoutExtension($row.Path)
        if (-not $buildLogText.Contains(
                "Compile [x64] $leaf", [StringComparison]::Ordinal)) {
            throw "R27 build log lacks forced direct compile action: $leaf"
        }
        $rebuilt = Get-FileState $row.Path
        if (-not $rebuilt.Present -or $rebuilt.Bytes -le 0) {
            throw "R27 direct compile object was not rebuilt: $($row.Path)"
        }
        if ($row.Before.Present -and
            $rebuilt.Sha256 -ceq $row.Before.Sha256) {
            throw "R27 direct compile object is byte-identical to its predecessor after forced rebuild: $($row.Path)"
        }
    }
    Assert-NativeIdle 'after R27 build'
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-SourcePins -NativeAfter
    Assert-Phase2CompiledSourcePins -Native
    [void] (Assert-State $expectedMapPin $mapFile 'post-build map')
    Assert-ImmutableContent
    $runtimeAfter = Get-FileState $runtimeDll
    $editorAfter = Get-FileState $editorDll
    if (-not $runtimeAfter.Present -or -not $editorAfter.Present -or
        $runtimeAfter.Bytes -le 0 -or $editorAfter.Bytes -le 0 -or
        $runtimeAfter.Sha256 -ceq $expectedRuntimeDllPin.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedEditorDllPin.Sha256) {
        throw 'R27 build did not produce two distinct non-empty DLL receipts.'
    }
    $actorGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp'
    $editorGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp'
    $hybridGenerated = Join-Path $nativeProjectRoot `
        'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp'
    foreach ($generated in @($actorGenerated, $editorGenerated, $hybridGenerated)) {
        if (-not [IO.File]::Exists($generated) -or
            (Get-Item $generated).LastWriteTimeUtc -lt
                (Get-Item $buildLog).CreationTimeUtc) {
            throw "Forced-UHT output is absent or stale: $generated"
        }
    }
    $hybridGeneratedText = Get-Content -LiteralPath $hybridGenerated -Raw
    $editorGeneratedText = Get-Content -LiteralPath $editorGenerated -Raw
    $editorDllText = [Text.Encoding]::ASCII.GetString(
        [IO.File]::ReadAllBytes($editorDll))
    foreach ($endpoint in @(
        'ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap',
        'ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap')) {
        if (-not $hybridGeneratedText.Contains($endpoint) -or
            -not $editorDllText.Contains($endpoint)) {
            throw "Fresh reflection/DLL endpoint gate failed: $endpoint"
        }
    }
    Assert-BinaryContainsEncodedMarkers $runtimeDll @(
        'VISUAL_ASSUMPTION_BOUND_R27_LANDMARK_VEGETATION_CORRECTION',
        'tallestCarrierTipProjectionPixelsAtEvidenceRange') `
        'Fresh runtime DLL'
    Assert-BinaryContainsEncodedMarkers $editorDll @(
        'SINGLE_GATE_VISIBILITY_65M_90M',
        'componentFadeIntegrated=true',
        'V5D_R23B_DERIVATIVE_MATERIAL_VALID',
        'APPLY_REFUSED_FINAL_MUTATION_GATE') `
        'Fresh editor DLL'
    foreach ($endpoint in @(
        'BuildOrValidateLandmarkGrassMaterialsR27',
        'ValidateLandmarkGrassMaterialsR27',
        'ValidateReusableLandmarkVegetationAssets',
        'ConfigureLandmarkVegetationActor')) {
        if (-not $editorGeneratedText.Contains($endpoint) -or
            -not $editorDllText.Contains($endpoint)) {
            throw "Fresh landmark editor reflection/DLL gate failed: $endpoint"
        }
    }

    $stageResults.Add((Invoke-ColdStage `
        '00_cold_validate_phase2_pre_r27' $temasekLibrary `
        'ValidateIstanaExploreV5DTemasekShophouseR24Assets' `
        'OutReport' 'ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSETS_VALID'))
    [void] (Assert-State $expectedMapPin $mapFile `
        'pre-R27 Phase-2 validation map stability')

    $mapBeforeMaterialBuild = Get-FileState $mapFile
    $stageResults.Add((Invoke-ColdStage `
        '01_build_r27_grass_materials' $assetLibrary `
        'BuildOrValidateLandmarkGrassMaterialsR27' 'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_GRASS_R27_BUILD_PASS'))
    Assert-NoReparseAncestor $r27MaterialRoot $nativeProjectRoot
    [void] (Assert-State $mapBeforeMaterialBuild $mapFile `
        'material-build map stability')
    $materialSuccessorPins = @(Get-R27MaterialSuccessorPins)
    foreach ($marker in @(
            'createdPackages=4', 'exactSavedPackages=4',
            'exactDerivativeGraph=true', 'compiledMaterials=4',
            'sourceMaterialsModified=false',
            'sourceStableVisibilityMeters=20,28',
            'targetStableVisibilityMeters=65,90',
            'visibilityGateCount=1', 'componentFadeIntegrated=true',
            'visualCaptureAccepted=false',
            'captureRevalidationRequired=true')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R27 material-build response lacks required marker: $marker"
        }
    }

    $stageResults.Add((Invoke-ColdStage `
        '02_validate_reusable_assets' $assetLibrary `
        'ValidateReusableLandmarkVegetationAssets' 'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_ASSETS_VALID'))
    [void] (Assert-State $mapBeforeMaterialBuild $mapFile `
        'asset-validation map stability')
    Assert-R27MaterialPins $materialSuccessorPins

    $stageResults.Add((Invoke-ColdStage `
        '03_apply_r27_visual_correction' $hybridLibrary `
        'ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap' `
        'OutMessage' `
        'EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_PASS' `
        @{
            ExpectedPredecessorBytes = [int64] $expectedMapPin.Bytes
            ExpectedPredecessorSha256 = [string] $expectedMapPin.Sha256
            VerifiedExternalBackupFilename = [string] $mapJournal[0].Backup
        }))
    $successorPin = Get-FileState $mapFile
    if (-not $successorPin.Present -or $successorPin.Bytes -le 0 -or
        $successorPin.Sha256 -ceq $expectedMapPin.Sha256) {
        throw 'R27 apply did not produce a distinct positive successor map receipt.'
    }
    foreach ($marker in @(
        'oneSave=true', 'onePersistentActorReconfigured=true',
        'contextFacadeR25=true', 'isolatedGrassMaterials=4',
        'exactDerivativeGraph=true', 'compiledMaterials=4',
        'legacyTemasekBakedFoliageRemoved=true',
        'temasekMeshTriangles=15760', 'temasekMaterialSlots=15',
        'bakedFoliageRenderComponents=0',
        'sourceStableVisibilityMeters=20,28',
        'targetStableVisibilityMeters=65,90',
        'visibilityGateCount=1', 'componentFadeIntegrated=true',
        'projectionAssumption=perpendicularPinholeMaxSourceTip',
        'grassInstances=3072', 'maximumGrassPerSite=2048',
        'visualCaptureAccepted=false',
        'captureRevalidationRequired=true',
        'externalBackupVerified=true', 'rollbackOwnedByWrapper=true',
        'collisionNavigationSensorRfAuthority=false')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R27 apply response lacks required marker: $marker"
        }
    }
    Assert-R27MaterialPins $materialSuccessorPins

    $stageResults.Add((Invoke-ColdStage `
        '04_cold_validate_r27_map' $hybridLibrary `
        'ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap' `
        'OutReport' `
        'ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_SUCCESSOR_VALID'))
    [void] (Assert-State $successorPin $mapFile 'cold-validation map stability')
    foreach ($marker in @(
        'mapIntegrated=true', 'exactlyOneActor=true',
        'contextFacadeR25=true',
        'legacyTemasekBakedFoliageRemoved=true',
        'sourceStableVisibilityMeters=20,28',
        'targetStableVisibilityMeters=65,90',
        'visibilityGateCount=1', 'componentFadeIntegrated=true',
        'grassCullCm=6500,9000', 'evidenceRangeMeters=72.8',
        'materialVisibilityAtEvidenceRange=',
        'tallestCarrierTipProjectionPixelsAtEvidenceRange=',
        'projectionAssumption=perpendicularPinholeMaxSourceTip',
        'wpoDisableCm=2400', 'grassInstances=3072',
        'visualCaptureAccepted=false',
        'captureRevalidationRequired=true',
        'renderOnly=true', 'sensorAuthority=false', 'rfAuthority=false')) {
        if (-not $stageResults[$stageResults.Count - 1].Message.Contains($marker)) {
            throw "R27 validation response lacks required marker: $marker"
        }
    }
    Assert-R27MaterialPins $materialSuccessorPins

    $stageResults.Add((Invoke-ColdStage `
        '05_idempotent_r27_apply' $hybridLibrary `
        'ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap' `
        'OutMessage' `
        'IDEMPOTENT_EXPLORE_V5D_LANDMARK_VEGETATION_R27_ALREADY_VALID' `
        @{
            ExpectedPredecessorBytes = [int64] $expectedMapPin.Bytes
            ExpectedPredecessorSha256 = [string] $expectedMapPin.Sha256
            VerifiedExternalBackupFilename = [string] $mapJournal[0].Backup
        }))
    [void] (Assert-State $successorPin $mapFile 'idempotent map stability')
    Assert-R27MaterialPins $materialSuccessorPins
    $stageResults.Add((Invoke-ColdStage `
        '06_cold_validate_phase2_post_r27' $temasekLibrary `
        'ValidateIstanaExploreV5DTemasekShophouseR24Assets' `
        'OutReport' 'ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSETS_VALID'))
    [void] (Assert-State $successorPin $mapFile `
        'post-R27 Phase-2 validation map stability')
    Assert-R27MaterialPins $materialSuccessorPins
    Assert-ImmutableContent
    Assert-SourcePins -NativeAfter
    Assert-Phase2CompiledSourcePins -Native
    Assert-ProtectedUnchanged $script:protectedBefore
    Assert-NativeIdle 'commit gate'

    $commit = [pscustomobject] [ordered] @{
        Schema = $schema
        Status = 'PASS'
        RunToken = $RunToken
        PredecessorMap = $expectedMapPin
        SuccessorMap = $successorPin
        RuntimeDll = $runtimeAfter
        EditorDll = $editorAfter
        SourceCount = $sourcePins.Count
        Phase2CompiledSourceCount = $phase2CompiledSourcePins.Count
        ImmutableContentCount = $immutableContentPins.Count
        R27MaterialPackages = @($materialSuccessorPins)
        BuildSurfaceStateCount = $buildJournal.Count
        ForcedHeaderGeneration = $true
        RenderingCapableFreshEditorProcesses = $stageResults.Count
        Stages = @($stageResults)
        ProtectedUE54SessionsUnchanged = $true
        CollisionNavigationSensorRfAuthority = $false
        VisualCaptureAccepted = $false
        CaptureRevalidationRequired = $true
    }
    $commitPath = Join-Path $transactionRoot 'commit.json'
    $commit | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $commitPath `
        -Encoding utf8NoBOM -NoNewline
    $committed = $true
    $commit | ConvertTo-Json -Depth 20
}
catch {
    $transactionError = $_.Exception
}
finally {
    if (-not $committed) {
        $rollbackErrors = [Collections.Generic.List[string]]::new()
        # Defense in depth: a stage that throws anywhere between Start-Process
        # and its post-cleanup boundary leaves its exact handle registered.
        # Re-prove the complete Win32 identity, contain only that process, and
        # require RC release before permitting any rollback write.
        if ($null -ne $script:activeHelperHandle) {
            try {
                if ($null -eq $script:activeHelperIdentity) {
                    $script:activeHelperIdentity =
                        Wait-ExpectedHelperIdentity `
                            -Handle $script:activeHelperHandle `
                            -MapPackage $script:activeHelperMapPackage `
                            -Log $script:activeHelperLog `
                            -LaunchStartTimeUtcTicks `
                                $script:activeHelperLaunchStartTimeUtcTicks `
                            -TimeoutSeconds 30
                }
                $ownedStopError = $null
                if ($null -ne $script:activeHelperIdentity) {
                    try {
                        Stop-OwnedHelper $script:activeHelperIdentity `
                            $script:activeHelperHandle
                    }
                    catch { $ownedStopError = $_.Exception }
                }
                if (Test-Win32ProcessPresent `
                        $script:activeHelperLaunchProcessId) {
                    try {
                        Stop-LaunchedHelperBeforeIdentity `
                            -Handle $script:activeHelperHandle `
                            -LaunchProcessId `
                                $script:activeHelperLaunchProcessId `
                            -LaunchStartTimeUtcTicks `
                                $script:activeHelperLaunchStartTimeUtcTicks
                    }
                    catch {
                        $prior = if ($null -eq $ownedStopError) { 'none' }
                            else { $ownedStopError.Message }
                        throw "Exact helper stop error={$prior}; launch-handle fallback error={$($_.Exception.Message)}"
                    }
                }
                if (Test-Win32ProcessPresent `
                        $script:activeHelperLaunchProcessId) {
                    throw 'Launched helper remains after outer containment.'
                }
                if (@(Get-RcOwners).Count -ne 0) {
                    throw 'RC listener remains after outer helper containment.'
                }
                $script:activeHelperHandle = $null
                $script:activeHelperIdentity = $null
                $script:activeHelperLaunchProcessId = 0
                $script:activeHelperLaunchStartTimeUtcTicks = 0L
            }
            catch {
                $rollbackErrors.Add(
                    "owned helper containment: $($_.Exception.Message)")
            }
        }
        try { Assert-NativeIdle 'rollback entry' }
        catch { $rollbackErrors.Add($_.Exception.Message) }
        if ($rollbackErrors.Count -eq 0) {
            try { Restore-MapFromJournal $mapJournal[0] }
            catch { $rollbackErrors.Add("map: $($_.Exception.Message)") }
            try { Restore-BuildSurface $buildJournal }
            catch { $rollbackErrors.Add("build: $($_.Exception.Message)") }
            try { Restore-FileJournal $contentJournal }
            catch { $rollbackErrors.Add("content: $($_.Exception.Message)") }
            try { Restore-FileJournal $sourceJournal }
            catch { $rollbackErrors.Add("source: $($_.Exception.Message)") }
        }
        try { Assert-ProtectedUnchanged $script:protectedBefore }
        catch { $rollbackErrors.Add("protected: $($_.Exception.Message)") }
        $rollbackReport = [pscustomobject] [ordered] @{
            Schema = $schema
            Status = if ($rollbackErrors.Count -eq 0) { 'ROLLED_BACK' } else { 'ROLLBACK_INCOMPLETE' }
            RunToken = $RunToken
            Failure = if ($null -ne $transactionError) { $transactionError.Message } else { 'unknown' }
            Errors = @($rollbackErrors)
            Map = Get-FileState $mapFile
            RuntimeDll = Get-FileState $runtimeDll
            EditorDll = Get-FileState $editorDll
            ProtectedUE54SessionsUnchanged = $rollbackErrors.Count -eq 0
        }
        $rollbackPath = Join-Path $transactionRoot 'rollback.json'
        $rollbackReport | ConvertTo-Json -Depth 16 | Set-Content `
            -LiteralPath $rollbackPath -Encoding utf8NoBOM -NoNewline
    }
}

if (-not $committed) {
    throw "R27 native transaction failed and reported $($rollbackReport.Status): $($transactionError.Message). Receipt=$(Join-Path $transactionRoot 'rollback.json')"
}
