#requires -Version 7.0

<#
.SYNOPSIS
Runs the strict one-shot V5D Context Facade R25 native transaction.

.DESCRIPTION
The default invocation is a read-only preflight. -Execute is the only live
write authority. The transaction promotes exactly nine byte/hash-pinned repo
files, builds the two allowlisted Unreal modules, imports and cold-validates
the exact five additive R25 materials, applies the exact 17-entry map override
roster, cold-validates the successor, and proves an idempotent second apply.

Before its first native mutation it creates non-overwriting, verified backups
for every admitted native source predecessor, the exact 110-state build/UHT
rollback surface, and the exact predecessor map. A failure restores R25
content, build products, and source destinations in reverse order. The durable
transaction directory is retained whether the result commits or rolls back.

Every editor stage runs in a separate rendering-capable process. Remote
Control is accepted only while port 30010 is solely owned by the exact process
returned by Start-Process. All exact-token UE5.4 CAPSTONE sessions present at
preflight are preserved as an immutable identity set.

.EXAMPLE
.\Invoke-IstanaExploreV5DContextFacadeR25NativeTransactionV1.ps1 -RunToken reviewed_r25

.EXAMPLE
.\Invoke-IstanaExploreV5DContextFacadeR25NativeTransactionV1.ps1 -RunToken reviewed_r25 -Execute
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,43}$')]
    [string] $RunToken,

    [switch] $Execute,

    [switch] $StaticSelfCheck,

    [ValidateRange(60, 1800)]
    [int] $EditorTimeoutSeconds = 900,

    [ValidateRange(30, 300)]
    [int] $ShutdownTimeoutSeconds = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$schema =
    'triad.istana_explore_v5d.context_facade_r25.native_transaction.v1'
$repositoryUnrealRoot = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo\unreal')
$nativeProjectRoot = [IO.Path]::GetFullPath('D:\triad\TRIAD')
$nativeProjectFile = [IO.Path]::GetFullPath(
    'D:\triad\TRIAD\TRIAD.uproject')
$nativeUE55EngineRoot = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.5')
$buildTool = [IO.Path]::GetFullPath((Join-Path $nativeUE55EngineRoot `
    'Engine\Build\BatchFiles\Build.bat'))
$editor = [IO.Path]::GetFullPath((Join-Path $nativeUE55EngineRoot `
    'Engine\Binaries\Win64\UnrealEditor.exe'))
$buildVersionFile = [IO.Path]::GetFullPath((Join-Path $nativeUE55EngineRoot `
    'Engine\Build\Build.version'))
$protectedUE54Editor = [IO.Path]::GetFullPath(
    'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe')
$protectedUE54Project = [IO.Path]::GetFullPath(
    'C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject')
$hybridMapPackage = '/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'
$assetStageMapPackage = $hybridMapPackage
$hybridMapFile = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap'))
$runtimeDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'))
$editorDll = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'))
$transactionBase = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\NativeTransactions\V5DContextFacadeR25V1'))
$transactionRoot = [IO.Path]::GetFullPath((Join-Path $transactionBase `
    $RunToken))
$ddcRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\DerivedDataCache'))
$rcCallUri = 'http://127.0.0.1:30010/remote/object/call'
$identityLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreEditorLibrary'
$assetLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary'
$hybridLibrary =
    '/Script/TRIADSensorFusionEditor.Default__TRIADIstanaExploreV5DHybridEditorLibrary'
$quitLibrary = '/Script/Engine.Default__KismetSystemLibrary'
$internalMapBackup = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Saved\TRIAD\MapBackups\V5DContextFacadeR25_20260906\Istana_PublicView_Explore_v5d_hybrid_4A5F5514C7C3.umap'))
$r25AssetRoot = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
    'Content\TRIAD\IstanaPublicViewExploreV5D\ContextFacadeR25'))

$expectedProjectPin = [pscustomobject] [ordered] @{
    Path = $nativeProjectFile
    Present = $true
    Bytes = 1298L
    Sha256 =
        '42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3'
}
$expectedMapPin = [pscustomobject] [ordered] @{
    Path = $hybridMapFile
    Present = $true
    Bytes = 34992354L
    Sha256 =
        '4A5F5514C7C3B508567465BA1F3B2FE8F31F4DAAAC5E317C8C57F1C30B50FD08'
}
$expectedRuntimeDllPin = [pscustomobject] [ordered] @{
    Path = $runtimeDll
    Present = $true
    Bytes = 4577280L
    Sha256 =
        'C7A80FD955ED831A7148068444079E54D7D9847759D7DAAC24272022CC8A695A'
}
$expectedEditorDllPin = [pscustomobject] [ordered] @{
    Path = $editorDll
    Present = $true
    Bytes = 7588352L
    Sha256 =
        'EA930B29A95E341E5EB3667C3C3F796345F6A538AB75FFC80977C36CC8610028'
}

# This exact nine-file roster is the complete repo-to-native promotion surface.
# The Python contract is deliberately copied as evidence but is never passed to
# UBT. No landmark, provider-capture, mesh-factory, or V5C factory source is in
# this allowlist.
$sourcePins = @(
    [pscustomobject] [ordered] @{
        Area = 'R25_ASSET_FACTORY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DContextFacadeR25AssetFactory.h'
        Bytes = 709L
        Sha256 = 'A3D6114BA9450E9A32B0CAE51713B9A56C31AA08EAAD665A0132F6C9CF227CF0'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        Area = 'R25_ASSET_FACTORY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DContextFacadeR25AssetFactory.cpp'
        Bytes = 41282L
        Sha256 = '82376BEFB4261283466226A019C751E35B0FC59DA42FE5D9DEA0842A54ADD298'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
    [pscustomobject] [ordered] @{
        Area = 'R25_ASSET_EDITOR_API'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h'
        Bytes = 2490L
        Sha256 = '160EDC5EDBE67AF6D48B3FA34C7C9998B5F693EE4186C9D97E06F11A7B42C9C0'
        NativeBeforePresent = $true
        NativeBeforeBytes = 1722L
        NativeBeforeSha256 = 'E752E5767ABBBA900E2957231D8E1FE3CD4E03AAE8180FF08EE85346DE06243F'
    }
    [pscustomobject] [ordered] @{
        Area = 'R25_ASSET_EDITOR_API'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp'
        Bytes = 16747L
        Sha256 = '289C996F67ADD3FBAD26073083BDC83F8D1CB2117E316A94E3D5F5E6686EC06E'
        NativeBeforePresent = $true
        NativeBeforeBytes = 12313L
        NativeBeforeSha256 = '2E52D294F856C00B17ACAE0299447CBC11BA4E423F3D37A25CCC7E2C8300923D'
    }
    [pscustomobject] [ordered] @{
        Area = 'R25_RUNTIME_POLICY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DContextPolicyActor.h'
        Bytes = 9003L
        Sha256 = '07858E83E7E73E103922A0FB3EA52FA76F30A9D96F1A402DA6631F75E9A73774'
        NativeBeforePresent = $true
        NativeBeforeBytes = 8694L
        NativeBeforeSha256 = '88A4000BA015308E50C909B333E8337649409179165138E092CA8DE63393C981'
    }
    [pscustomobject] [ordered] @{
        Area = 'R25_RUNTIME_POLICY'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DContextPolicyActor.cpp'
        Bytes = 93879L
        Sha256 = '6E2C5697AE3949CFD009D4762A334E71A09DFAEA60F53823C670D19137C082EA'
        NativeBeforePresent = $true
        NativeBeforeBytes = 86305L
        NativeBeforeSha256 = '75A753072A8914D505314A6AF395AFAD89AA919A0E1C8B6253EEF67B072338D1'
    }
    [pscustomobject] [ordered] @{
        Area = 'R25_HYBRID_TRANSACTION_API'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Public\TRIADIstanaExploreV5DHybridEditorLibrary.h'
        Bytes = 9553L
        Sha256 = '762F0EB8436844912D203D6E14E962AB3BA7331F300DE19EA323096D45C11AEF'
        NativeBeforePresent = $true
        NativeBeforeBytes = 9021L
        NativeBeforeSha256 = '950ACEFDAF9EB1B3B21C535DB7E5482C217F1D72C23FF9995CAC9DA1758DFE47'
    }
    [pscustomobject] [ordered] @{
        Area = 'R25_HYBRID_TRANSACTION_API'
        RelativePath = 'Plugins\TRIADSensorFusion\Source\TRIADSensorFusionEditor\Private\TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
        Bytes = 403489L
        Sha256 = 'C28D6D9408C98469CF21C423A0D568ACBC2F00BDA61BF1AAB60F476285311E26'
        NativeBeforePresent = $true
        NativeBeforeBytes = 377184L
        NativeBeforeSha256 = '9490D09F9FC453777F5187C2440F144F01227B5D7E9F079B22D710B403B4F75D'
    }
    [pscustomobject] [ordered] @{
        Area = 'R25_STATIC_CONTRACT'
        RelativePath = 'Plugins\TRIADSensorFusion\Tests\test_istana_explore_v5d_context_facade_r25_contract.py'
        Bytes = 13305L
        Sha256 = 'F04B2A4529F5D257CB6E4CAF01B25759F76CE4FC54B71365A6C1F8F02FBE1D2F'
        NativeBeforePresent = $false
        NativeBeforeBytes = 0L
        NativeBeforeSha256 = 'ABSENT'
    }
)

# These seven exact binary products form the build rollback surface. The two
# DLL entries are also the explicit pre-build runtime/editor admission pins.
$buildProductPins = @(
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll'
        Bytes = 4577280L
        Sha256 = 'C7A80FD955ED831A7148068444079E54D7D9847759D7DAAC24272022CC8A695A'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.exp'
        Bytes = 565592L
        Sha256 = '7B3BA3AD58D287846ED93FECAE3939B78050439254BE397385DD4C7D9F4223D1'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.pdb'
        Bytes = 99340288L
        Sha256 = '54B6ABA6F5573B0EBCAD89AFCDEC3DD176C265995BF0AF950CB007F6A2D484A1'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll'
        Bytes = 7588352L
        Sha256 = 'EA930B29A95E341E5EB3667C3C3F796345F6A538AB75FFC80977C36CC8610028'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.exp'
        Bytes = 309114L
        Sha256 = 'D6CEB13EFEF5C940DD1B3FD906B44EDBEDF092E94033C73DBC02B4C870BFAA5D'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.pdb'
        Bytes = 106328064L
        Sha256 = '70A38FD67543E577C008DC8233E331775265FA4EC980035EDB0812FCDB64BECC'
    }
    [pscustomobject] [ordered] @{
        RelativePath = 'Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor.modules'
        Bytes = 186L
        Sha256 = 'E9AB983A61BF374D9AE1EBB0E4DB44936A08E7EB103BF52A2973E158BF83C238'
    }
)

# UBT also mutates source-associated objects, generated-code objects/UHT
# outputs, module link products, and global action/dependency caches. The first
# first reviewed R25 attempt proved 33 mutations outside the original 60-state
# journal: 24 outputs for eight dependency-rebuilt translation units, three UHT
# products, and six global UBT products. The second attempt exposed one more
# cross-plugin UHT timestamp. Include all five compiler sidecars for those eight
# units (the observed three plus both response files), closing the observed
# 34-path gap with 16 conservative sidecars. Some entries are legitimately
# absent, so every pre-state is captured dynamically. Together with the seven
# pinned binaries, rollback owns exactly 110 states.
$compileArtifactSuffixes = @(
    '.dep.json', '.obj', '.obj.rsp', '.obj.rsp.old', '.sarif')
$compileArtifactSpecs = @(
    [pscustomobject] @{
        Module = 'TRIADSensorFusion'
        Base = 'TRIADIstanaExploreV5DContextPolicyActor.cpp'
        Admission = 'R25_DIRECT'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusion'
        Base = 'TRIADIstanaExploreV5DContextPolicyActor.gen.cpp'
        Admission = 'R25_DIRECT'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusionEditor'
        Base = 'TRIADIstanaExploreV5DContextFacadeR25AssetFactory.cpp'
        Admission = 'R25_DIRECT'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusionEditor'
        Base = 'TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp'
        Admission = 'R25_DIRECT'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusionEditor'
        Base = 'TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.gen.cpp'
        Admission = 'R25_DIRECT'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusionEditor'
        Base = 'TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
        Admission = 'R25_DIRECT'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusionEditor'
        Base = 'TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp'
        Admission = 'R25_DIRECT'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusion'
        Base = 'TRIADIstanaExploreV5AppearanceActor.cpp'
        Admission = 'OBSERVED_20260905_DEPENDENCY_REBUILD'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusion'
        Base = 'TRIADIstanaExploreV5DGroundVegetationActor.cpp'
        Admission = 'OBSERVED_20260905_DEPENDENCY_REBUILD'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusion'
        Base = 'TRIADIstanaExploreV5DMacDonaldHouseActor.cpp'
        Admission = 'OBSERVED_20260905_DEPENDENCY_REBUILD'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusion'
        Base = 'TRIADIstanaExploreV5DTemasekShophouseActor.cpp'
        Admission = 'OBSERVED_20260905_DEPENDENCY_REBUILD'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusionEditor'
        Base = 'TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp'
        Admission = 'OBSERVED_20260905_DEPENDENCY_REBUILD'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusionEditor'
        Base = 'TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.cpp'
        Admission = 'OBSERVED_20260905_DEPENDENCY_REBUILD'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusionEditor'
        Base = 'TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp'
        Admission = 'OBSERVED_20260905_DEPENDENCY_REBUILD'
    }
    [pscustomobject] @{
        Module = 'TRIADSensorFusionEditor'
        Base = 'TRIADSensorFusionEditor.init.gen.cpp'
        Admission = 'OBSERVED_20260905_DEPENDENCY_REBUILD'
    }
)
$r25UhtOutputRelativePaths = @(
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DContextPolicyActor.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\TRIADIstanaExploreV5DContextPolicyActor.generated.h'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.generated.h'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DHybridEditorLibrary.generated.h'
)
$observedUhtMutationRelativePaths = @(
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusion\UHT\Timestamp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\Timestamp'
    'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADSensorFusionEditor.init.gen.cpp'
)
$observedCrossPluginUhtMutationRelativePaths = @(
    'Plugins\AirSimTriadRuntime\Intermediate\Build\Win64\UnrealEditor\Inc\AirSimTriadRuntime\UHT\Timestamp'
)
$observedGlobalBuildMutationRelativePaths = @(
    'Intermediate\Build\SourceFileCache.bin'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.deps'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest'
    'Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtpath'
    'Intermediate\Build\Win64\x64\UnrealEditor\ActionHistory.bin'
    'Intermediate\Build\Win64\x64\UnrealEditor\Development\DependencyCache.bin'
)
$dependencyClosureHeaderLeafNames = @(
    'TRIADIstanaExploreV5DContextPolicyActor.h'
    'TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h'
    'TRIADIstanaExploreV5DHybridEditorLibrary.h'
    'TRIADIstanaExploreV5DContextFacadeR25AssetFactory.h'
)
$expectedDependencyClosureKeys = @(
    'TRIADSensorFusion|TRIADIstanaExploreV5AppearanceActor.cpp'
    'TRIADSensorFusion|TRIADIstanaExploreV5DContextPolicyActor.cpp'
    'TRIADSensorFusion|TRIADIstanaExploreV5DContextPolicyActor.gen.cpp'
    'TRIADSensorFusion|TRIADIstanaExploreV5DGroundVegetationActor.cpp'
    'TRIADSensorFusion|TRIADIstanaExploreV5DMacDonaldHouseActor.cpp'
    'TRIADSensorFusion|TRIADIstanaExploreV5DTemasekShophouseActor.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.gen.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DHybridEditorLibrary.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.cpp'
    'TRIADSensorFusionEditor|TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp'
)
$buildAuxiliaryRelativePaths = @(
    foreach ($spec in $compileArtifactSpecs) {
        foreach ($suffix in $compileArtifactSuffixes) {
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\' +
                $spec.Module + '\' + $spec.Base + $suffix
        }
    }
    foreach ($module in @('TRIADSensorFusion', 'TRIADSensorFusionEditor')) {
        foreach ($suffix in @(
                '.dll.rsp', '.dll.rsp.old', '.exp', '.lib',
                '.lib.rsp', '.lib.rsp.old')) {
            'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\' +
                $module + '\UnrealEditor-' + $module + $suffix
        }
    }
    $r25UhtOutputRelativePaths
    $observedUhtMutationRelativePaths
    $observedCrossPluginUhtMutationRelativePaths
    $observedGlobalBuildMutationRelativePaths
)

# Editor startup also refreshes non-authoritative cache/config diagnostics.
# They are intentionally outside the source/map/material/build rollback claim;
# no transaction decision or simulation authority may depend on their bytes.
$nonAuthoritativeEphemeralStateExclusions = @(
    'Intermediate\CachedAssetRegistry_0.bin'
    'Intermediate\Config\CoalescedSourceConfigs\*.ini'
    'Intermediate\PipInstall\**'
    'Saved\Autosaves\PackageRestoreData.json'
    'Saved\Config\CrashReportClient\*\CrashReportClient.ini'
    'Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini'
    'Saved\ShaderDebugInfo\**\DDCKey-Editor.txt'
    'Saved\DerivedDataCache\**'
)

$immutableContentPins = @(
    [pscustomobject] [ordered] @{
        Role = 'V5C_MASTER_MATERIAL'
        RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_ContextMassing_Master.uasset'
        Bytes = 29065L
        Sha256 = '47B0F9F804858280B53D4D03CEEA8E078F01740920AF589EFEA676EBC4D057B4'
    }
    [pscustomobject] [ordered] @{
        Role = 'V5C_OFFICIAL_WALL_MATERIAL'
        RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRender.uasset'
        Bytes = 15159L
        Sha256 = '113CE217898FD5A65603493FFF82B4B0FC16C6AD3C0339097F61E78B911E326F'
    }
    [pscustomobject] [ordered] @{
        Role = 'V5C_OFFICIAL_ROOF_MATERIAL'
        RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OfficialContextRoof.uasset'
        Bytes = 14843L
        Sha256 = '4534461C849FD80A1D1BA34FE68239414B25F1FF31F4790BD857C9F828A8C943'
    }
    [pscustomobject] [ordered] @{
        Role = 'V5C_FALLBACK_WALL_MATERIAL'
        RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRender.uasset'
        Bytes = 15170L
        Sha256 = '227415242B7F0D0E61F3474C57D605848D24EF7574CD67C2242DED75B22F8079'
    }
    [pscustomobject] [ordered] @{
        Role = 'V5C_FALLBACK_ROOF_MATERIAL'
        RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5C\Surroundings\Materials\M_IPV5C_OsmFallbackContextRoof.uasset'
        Bytes = 14693L
        Sha256 = 'BF11B2C73A066E03923B83AADD9C1222CD80377B44A90EBFAE868454C05898EB'
    }
    [pscustomobject] [ordered] @{
        Role = 'V2_RENDER_MESH'
        RelativePath = 'Content\TRIAD\IstanaPublicViewExploreV5D\LocalFallbackSuppressionV2\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset'
        Bytes = 761010L
        Sha256 = 'A2062A8A90CB5FFA4D0B6B1117E775230B39AF58CB57F3C26C792DD4861429EE'
    }
)

$r25PrimaryAssetRelativePaths = @(
    'Materials\M_IPV5D_ContextFacadeR25_Master.uasset',
    'Materials\MI_IPV5D_ContextFacadeR25_OfficialWall.uasset',
    'Materials\MI_IPV5D_ContextFacadeR25_OfficialRoof.uasset',
    'Materials\MI_IPV5D_ContextFacadeR25_FallbackWall.uasset',
    'Materials\MI_IPV5D_ContextFacadeR25_FallbackRoof.uasset'
)
$script:protectedUE54Before = $null
$script:selfPin = $null
$script:sourceRows = @()
$script:builtDllPins = @()
$script:builtProductPins = @()

function Test-ContainedPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $fullPath.StartsWith(
        $fullRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparseAncestor {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    if (-not (Test-ContainedPath -Path $Path -Root $Root)) {
        throw "$Label escaped its reviewed root: $Path"
    }
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $relative = [IO.Path]::GetFullPath($Path).Substring(
        $fullRoot.Length).TrimStart('\', '/')
    $segments = @($relative -split '[\\/]')
    $cursor = $fullRoot
    for ($index = 0; $index -lt ($segments.Count - 1); ++$index) {
        $cursor = Join-Path $cursor $segments[$index]
        if (-not (Test-Path -LiteralPath $cursor)) {
            break
        }
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
        if (-not $item.PSIsContainer -or
            ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Label traverses a non-directory or reparse ancestor: $cursor"
        }
    }
}

function Assert-ExistingNonReparseDirectory {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $item = Get-Item -LiteralPath ([IO.Path]::GetFullPath($Path)) `
        -Force -ErrorAction Stop
    if (-not $item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Label must be an existing non-reparse directory: $Path"
    }
}

function Get-FileState {
    param([Parameter(Mandatory = $true)] [string] $Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not [IO.File]::Exists($fullPath)) {
        if (Test-Path -LiteralPath $fullPath) {
            throw "Expected file state is occupied by a non-file: $fullPath"
        }
        return [pscustomobject] [ordered] @{
            Path = $fullPath
            Present = $false
            Bytes = 0L
            Sha256 = 'ABSENT'
        }
    }
    $item = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
    if ($item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Expected a regular non-reparse file: $fullPath"
    }
    [pscustomobject] [ordered] @{
        Path = $fullPath
        Present = $true
        Bytes = [int64] $item.Length
        Sha256 = [string] (Get-FileHash -LiteralPath $fullPath `
            -Algorithm SHA256).Hash
    }
}

function Assert-SameState {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    $actual = Get-FileState -Path $Path
    if ([bool] $actual.Present -ne [bool] $Expected.Present -or
        [int64] $actual.Bytes -ne [int64] $Expected.Bytes -or
        [string] $actual.Sha256 -cne [string] $Expected.Sha256) {
        throw "$Label changed: expected=$($Expected.Present)/$($Expected.Bytes)/$($Expected.Sha256) actual=$($actual.Present)/$($actual.Bytes)/$($actual.Sha256) path=$Path"
    }
    $actual
}

function New-ExpectedState {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [bool] $Present,
        [Parameter(Mandatory = $true)] [int64] $Bytes,
        [Parameter(Mandatory = $true)] [string] $Sha256
    )

    [pscustomobject] [ordered] @{
        Path = [IO.Path]::GetFullPath($Path)
        Present = $Present
        Bytes = $Bytes
        Sha256 = $Sha256
    }
}

function Assert-NewPath {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    Assert-NoReparseAncestor -Path $Path -Root $Root -Label $Label
    if (Test-Path -LiteralPath $Path) {
        throw "$Label must not overwrite an existing path: $Path"
    }
}

function Copy-NewPinnedFile {
    param(
        [Parameter(Mandatory = $true)] [string] $Source,
        [Parameter(Mandatory = $true)] [string] $Destination,
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [string] $DestinationRoot,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    [void] (Assert-SameState -Expected $Expected -Path $Source `
        -Label "$Label source")
    Assert-NewPath -Path $Destination -Root $DestinationRoot -Label $Label
    $parent = [IO.Path]::GetDirectoryName($Destination)
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        [void] [IO.Directory]::CreateDirectory($parent)
    }
    Assert-NoReparseAncestor -Path $Destination -Root $DestinationRoot `
        -Label $Label
    [IO.File]::Copy($Source, $Destination, $false)
    Assert-SameState -Expected $Expected -Path $Destination `
        -Label "$Label readback"
}

function Write-NewJsonReceipt {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [Parameter(Mandatory = $true)] $Value,
        [Parameter(Mandatory = $true)] [string] $Root,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    Assert-NewPath -Path $Path -Root $Root -Label $Label
    $json = $Value | ConvertTo-Json -Depth 20
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes($json + "`n")
    $stream = [IO.File]::Open(
        $Path,
        [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write,
        [IO.FileShare]::None)
    try {
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
    Get-FileState -Path $Path
}

function Get-WorkspaceSourceRows {
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($pin in $sourcePins) {
        $relative = [string] $pin.RelativePath
        if ([IO.Path]::IsPathRooted($relative) -or
            $relative -match '(^|[\\/])\.\.([\\/]|$)' -or
            -not $seen.Add($relative)) {
            throw "Source allowlist path is not exact and unique: $relative"
        }
        $source = [IO.Path]::GetFullPath((Join-Path `
            $repositoryUnrealRoot $relative))
        if (-not (Test-ContainedPath -Path $source `
                -Root $repositoryUnrealRoot)) {
            throw "Source allowlist escaped the repository Unreal root: $relative"
        }
        $target = New-ExpectedState -Path $source -Present $true `
            -Bytes ([int64] $pin.Bytes) -Sha256 ([string] $pin.Sha256)
        [void] (Assert-SameState -Expected $target -Path $source `
            -Label 'reviewed R25 workspace source')
        $destination = [IO.Path]::GetFullPath((Join-Path `
            $nativeProjectRoot $relative))
        $before = New-ExpectedState -Path $destination `
            -Present ([bool] $pin.NativeBeforePresent) `
            -Bytes ([int64] $pin.NativeBeforeBytes) `
            -Sha256 ([string] $pin.NativeBeforeSha256)
        $rows.Add([pscustomobject] [ordered] @{
            Area = [string] $pin.Area
            RelativePath = $relative
            Source = $source
            Destination = $destination
            Target = $target
            NativeBefore = $before
            Backup = $null
            Stage = $null
            Displaced = $null
        })
    }
    if ($rows.Count -ne 9 -or $seen.Count -ne 9) {
        throw "R25 source allowlist must contain exactly nine files; actual=$($rows.Count)."
    }
    @($rows)
}

function Assert-WorkspaceSourcesUnchanged {
    foreach ($row in $script:sourceRows) {
        [void] (Assert-SameState -Expected $row.Target -Path $row.Source `
            -Label 'R25 workspace source boundary')
    }
}

function Assert-NativeSourceTargets {
    foreach ($row in $script:sourceRows) {
        [void] (Assert-SameState -Expected $row.Target `
            -Path $row.Destination -Label 'R25 native source target')
    }
}

function Assert-NativeSourcePredecessors {
    foreach ($row in $script:sourceRows) {
        [void] (Assert-SameState -Expected $row.NativeBefore `
            -Path $row.Destination -Label 'R25 native source predecessor')
    }
}

function Assert-ImmutableContentPins {
    foreach ($pin in $immutableContentPins) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
            ([string] $pin.RelativePath)))
        $expected = New-ExpectedState -Path $path -Present $true `
            -Bytes ([int64] $pin.Bytes) -Sha256 ([string] $pin.Sha256)
        [void] (Assert-SameState -Expected $expected -Path $path `
            -Label "immutable $($pin.Role)")
    }
}

function Assert-BuildRollbackRosterDefinition {
    if ($compileArtifactSuffixes.Count -ne 5 -or
        $compileArtifactSpecs.Count -ne 15 -or
        @($compileArtifactSpecs | Where-Object {
                $_.Admission -ceq 'R25_DIRECT'
            }).Count -ne 7 -or
        @($compileArtifactSpecs | Where-Object {
                $_.Admission -ceq
                    'OBSERVED_20260905_DEPENDENCY_REBUILD'
            }).Count -ne 8 -or
        $r25UhtOutputRelativePaths.Count -ne 6 -or
        $observedUhtMutationRelativePaths.Count -ne 3 -or
        $observedCrossPluginUhtMutationRelativePaths.Count -ne 1 -or
        $observedGlobalBuildMutationRelativePaths.Count -ne 6 -or
        $nonAuthoritativeEphemeralStateExclusions.Count -ne 8) {
        throw 'R25 build rollback component rosters drifted from the reviewed 110-state contract.'
    }
    $relativePaths = @(
        $buildProductPins | ForEach-Object { [string] $_.RelativePath }
        $buildAuxiliaryRelativePaths
    )
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($relative in $relativePaths) {
        if ([string]::IsNullOrWhiteSpace([string] $relative) -or
            [IO.Path]::IsPathRooted([string] $relative) -or
            [string] $relative -match '(^|[\\/])\.\.([\\/]|$)' -or
            -not $seen.Add([string] $relative)) {
            throw "Invalid or duplicate R25 build rollback path: $relative"
        }
    }
    if ($buildProductPins.Count -ne 7 -or
        $buildAuxiliaryRelativePaths.Count -ne 103 -or
        $relativePaths.Count -ne 110 -or $seen.Count -ne 110) {
        throw 'R25 build rollback roster must contain exactly 7 pinned binaries plus 103 scoped build/UHT states.'
    }
    [pscustomobject] [ordered] @{
        PinnedBinaryCount = 7
        ScopedBuildAndUhtCount = 103
        ObservedOutOfRosterMutationClosureCount = 34
        ConservativeObservedCompileSidecarCount = 16
        TotalBuildProductCount = 110
        WholeNativeTreeRollbackClaimed = $false
        NonAuthoritativeEphemeralStateExclusions =
            $nonAuthoritativeEphemeralStateExclusions
    }
}

function Get-ReviewedBuildDependencyClosure {
    if ($dependencyClosureHeaderLeafNames.Count -ne 4 -or
        $expectedDependencyClosureKeys.Count -ne 13) {
        throw 'R25 dependency-closure definition drifted from its reviewed shape.'
    }
    $headerNames = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($leaf in $dependencyClosureHeaderLeafNames) {
        if (-not $headerNames.Add([string] $leaf)) {
            throw "Duplicate R25 dependency header leaf: $leaf"
        }
    }
    $expected = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($key in $expectedDependencyClosureKeys) {
        if (-not $expected.Add([string] $key)) {
            throw "Duplicate R25 dependency-closure key: $key"
        }
    }
    $actual = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $evidence = [Collections.Generic.List[object]]::new()
    foreach ($module in @('TRIADSensorFusion', 'TRIADSensorFusionEditor')) {
        $directory = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
            ('Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor\Development\' +
                $module)))
        Assert-ExistingNonReparseDirectory -Path $directory `
            -Label "R25 $module dependency directory"
        foreach ($file in @(Get-ChildItem -LiteralPath $directory `
                -Filter '*.dep.json' -File -Force | Sort-Object Name)) {
            Assert-NoReparseAncestor -Path $file.FullName `
                -Root $nativeProjectRoot `
                -Label 'R25 compiler dependency evidence'
            $before = Get-FileState -Path $file.FullName
            try {
                $document = Get-Content -LiteralPath $file.FullName -Raw |
                    ConvertFrom-Json -Depth 20
            }
            catch {
                throw "Invalid R25 compiler dependency evidence $($file.FullName): $($_.Exception.Message)"
            }
            [void] (Assert-SameState -Expected $before `
                -Path $file.FullName `
                -Label 'stable R25 compiler dependency evidence')
            $matches = [Collections.Generic.List[string]]::new()
            foreach ($include in @($document.Data.Includes)) {
                $leaf = [IO.Path]::GetFileName([string] $include)
                if ($headerNames.Contains($leaf) -and
                    -not $matches.Contains($leaf)) {
                    $matches.Add($leaf)
                }
            }
            if ($matches.Count -eq 0) {
                continue
            }
            $suffix = '.dep.json'
            $base = $file.Name.Substring(
                0, $file.Name.Length - $suffix.Length)
            $key = "$module|$base"
            if (-not $expected.Contains($key)) {
                throw "Unjournalled R25 dependency-rebuild translation unit: $key"
            }
            [void] $actual.Add($key)
            $evidence.Add([pscustomobject] [ordered] @{
                Key = $key
                DependencyFile = $before
                MatchedHeaders = @($matches | Sort-Object)
            })
        }
    }
    if ($actual.Count -ne 13) {
        $missing = @($expected | Where-Object { -not $actual.Contains($_) } |
            Sort-Object)
        throw "R25 dependency closure is incomplete: actual=$($actual.Count)/13 missing=$([string]::Join(',', $missing))"
    }
    [pscustomobject] [ordered] @{
        Status = 'EXACT_13_DEPENDENCY_CLOSURE_COVERED'
        Count = $actual.Count
        HeaderLeafNames = @($headerNames | Sort-Object)
        TranslationUnits = @($actual | Sort-Object)
        Evidence = @($evidence | Sort-Object Key)
    }
}

function Get-PinnedBuildProductRows {
    [void] (Assert-BuildRollbackRosterDefinition)
    $seen = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($pin in $buildProductPins) {
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
            ([string] $pin.RelativePath)))
        $before = New-ExpectedState -Path $path -Present $true `
            -Bytes ([int64] $pin.Bytes) -Sha256 ([string] $pin.Sha256)
        [void] (Assert-SameState -Expected $before -Path $path `
            -Label 'pinned R25 build predecessor')
        $rows.Add([pscustomobject] [ordered] @{
            RelativePath = [string] $pin.RelativePath
            Path = $path
            Before = $before
            Backup = $null
            After = $null
            PinnedBinary = $true
        })
        [void] $seen.Add([string] $pin.RelativePath)
    }
    foreach ($relative in $buildAuxiliaryRelativePaths) {
        if (-not $seen.Add([string] $relative)) {
            throw "Duplicate R25 build rollback path: $relative"
        }
        $path = [IO.Path]::GetFullPath((Join-Path $nativeProjectRoot `
            ([string] $relative)))
        if (-not (Test-ContainedPath -Path $path -Root $nativeProjectRoot)) {
            throw "R25 build rollback path escaped the native root: $relative"
        }
        Assert-NoReparseAncestor -Path $path -Root $nativeProjectRoot `
            -Label 'R25 auxiliary build product'
        $rows.Add([pscustomobject] [ordered] @{
            RelativePath = [string] $relative
            Path = $path
            Before = Get-FileState -Path $path
            Backup = $null
            After = $null
            PinnedBinary = $false
        })
    }
    if ($buildAuxiliaryRelativePaths.Count -ne 103 -or
        $rows.Count -ne 110 -or $seen.Count -ne 110) {
        throw "R25 build rollback roster must contain 7 pinned binaries plus 103 scoped build/UHT states; actual=$($rows.Count)."
    }
    @($rows)
}

function Get-R25AssetInventory {
    param([switch] $RequireAbsent, [switch] $RequireComplete)

    $rootExists = Test-Path -LiteralPath $r25AssetRoot
    if ($rootExists) {
        $item = Get-Item -LiteralPath $r25AssetRoot -Force -ErrorAction Stop
        if (-not $item.PSIsContainer -or
            ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "R25 asset root is not a regular directory: $r25AssetRoot"
        }
    }
    $expectedStems = [Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    $primary = [Collections.Generic.List[object]]::new()
    foreach ($relative in $r25PrimaryAssetRelativePaths) {
        $path = [IO.Path]::GetFullPath((Join-Path $r25AssetRoot $relative))
        [void] $expectedStems.Add([IO.Path]::ChangeExtension($path, $null))
        $primary.Add((Get-FileState -Path $path))
    }
    $artifacts = [Collections.Generic.List[object]]::new()
    $allowedExtensions = @('.uasset', '.uexp', '.ubulk', '.uptnl')
    if ($rootExists) {
        $materialsRoot = [IO.Path]::GetFullPath((Join-Path `
            $r25AssetRoot 'Materials'))
        foreach ($child in @(Get-ChildItem -LiteralPath $r25AssetRoot `
                -Force)) {
            $full = [IO.Path]::GetFullPath($child.FullName)
            if (-not $child.PSIsContainer -or $full -ine $materialsRoot -or
                ($child.Attributes -band [IO.FileAttributes]::ReparsePoint) `
                    -ne 0) {
                throw "Unexpected direct child in exact R25 asset root: $full"
            }
        }
        if (Test-Path -LiteralPath $materialsRoot) {
            $materialsItem = Get-Item -LiteralPath $materialsRoot -Force
            if (-not $materialsItem.PSIsContainer -or
                ($materialsItem.Attributes -band
                    [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "R25 Materials root is not a regular directory: $materialsRoot"
            }
        }
        foreach ($file in @(Get-ChildItem -LiteralPath $materialsRoot `
                -File -Force -ErrorAction SilentlyContinue |
                Sort-Object FullName)) {
            $full = [IO.Path]::GetFullPath($file.FullName)
            Assert-NoReparseAncestor -Path $full -Root $nativeProjectRoot `
                -Label 'R25 package artifact'
            if ([IO.Path]::GetExtension($full) -notin $allowedExtensions -or
                -not $expectedStems.Contains(
                    [IO.Path]::ChangeExtension($full, $null))) {
                throw "Unexpected file in exact R25 asset root: $full"
            }
            $artifacts.Add((Get-FileState -Path $full))
        }
        if (Test-Path -LiteralPath $materialsRoot) {
            $nested = @(Get-ChildItem -LiteralPath $materialsRoot `
                -Directory -Force)
            if ($nested.Count -ne 0) {
                throw "Unexpected nested directory in exact R25 Materials root: $($nested.FullName -join ',')"
            }
        }
    }
    $complete = $rootExists -and
        @($primary | Where-Object { -not $_.Present }).Count -eq 0 -and
        $primary.Count -eq 5
    if ($RequireAbsent -and ($rootExists -or $artifacts.Count -ne 0)) {
        throw 'The one-shot R25 transaction requires the exact asset root to be absent.'
    }
    if ($RequireComplete -and -not $complete) {
        throw 'The exact five-package R25 asset roster is incomplete.'
    }
    [pscustomobject] [ordered] @{
        Root = $r25AssetRoot
        RootExists = [bool] $rootExists
        Complete = [bool] $complete
        Primary = @($primary)
        Artifacts = @($artifacts)
    }
}

function Assert-R25AssetInventorySame {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    if (-not $Expected.Complete) {
        throw "$Label cannot compare against an incomplete R25 inventory."
    }
    $actual = Get-R25AssetInventory -RequireComplete
    $expectedPaths = @($Expected.Artifacts | ForEach-Object { $_.Path } |
        Sort-Object)
    $actualPaths = @($actual.Artifacts | ForEach-Object { $_.Path } |
        Sort-Object)
    if ($expectedPaths.Count -ne $actualPaths.Count -or
        ($expectedPaths -join "`n") -cne ($actualPaths -join "`n")) {
        throw "$Label changed the exact R25 package artifact roster."
    }
    foreach ($artifact in $Expected.Artifacts) {
        [void] (Assert-SameState -Expected $artifact -Path $artifact.Path `
            -Label $Label)
    }
    $actual
}

function Test-ExactCommandLineToken {
    param(
        [Parameter(Mandatory = $true)] [string] $CommandLine,
        [Parameter(Mandatory = $true)] [string] $Token
    )

    $pattern = '(?i)(?:^|\s)"?' + [regex]::Escape($Token) +
        '"?(?:\s|$)'
    [regex]::Matches($CommandLine, $pattern).Count -eq 1
}

function Get-ProcessIdentity {
    param([Parameter(Mandatory = $true)] [uint32] $ProcessId)

    $row = Get-CimInstance Win32_Process `
        -Filter "ProcessId = $ProcessId" -ErrorAction SilentlyContinue
    if ($null -eq $row) {
        return $null
    }
    $creation = [DateTimeOffset] $row.CreationDate
    [pscustomobject] [ordered] @{
        ProcessId = [uint32] $row.ProcessId
        CreationUtcTicks = [int64] $creation.UtcTicks
        Name = [string] $row.Name
        ExecutablePath = if ([string]::IsNullOrWhiteSpace(
                [string] $row.ExecutablePath)) {
            ''
        }
        else { [IO.Path]::GetFullPath([string] $row.ExecutablePath) }
        CommandLine = [string] $row.CommandLine
    }
}

function Test-ProcessIdentityEqual {
    param(
        [Parameter(Mandatory = $true)] $Expected,
        [Parameter(Mandatory = $true)] [AllowNull()] $Actual
    )

    $null -ne $Actual -and
        [uint32] $Actual.ProcessId -eq [uint32] $Expected.ProcessId -and
        [int64] $Actual.CreationUtcTicks -eq
            [int64] $Expected.CreationUtcTicks -and
        [string] $Actual.Name -ceq [string] $Expected.Name -and
        [string] $Actual.ExecutablePath -ceq
            [string] $Expected.ExecutablePath -and
        [string] $Actual.CommandLine -ceq [string] $Expected.CommandLine
}

function Get-ProtectedUE54Identity {
    $matches = @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe') -and
                -not [string]::IsNullOrWhiteSpace(
                    [string] $_.CommandLine) -and
                (Test-ExactCommandLineToken `
                    -CommandLine ([string] $_.CommandLine) `
                    -Token $protectedUE54Project)
        })
    $identities = @($matches | ForEach-Object {
        $process = $_
        $actualExecutable = if ([string]::IsNullOrWhiteSpace(
                [string] $process.ExecutablePath)) {
            ''
        }
        else {
            [IO.Path]::GetFullPath([string] $process.ExecutablePath)
        }
        if ($process.Name -cne 'UnrealEditor.exe' -or
            $actualExecutable -ine $protectedUE54Editor) {
            throw 'The CAPSTONE project token is owned by an unexpected process; refusing the R25 transaction.'
        }
        [pscustomobject] [ordered] @{
            ProcessId = [uint32] $process.ProcessId
            CreationUtcTicks =
                [int64] ([DateTimeOffset] $process.CreationDate).UtcTicks
            Name = [string] $process.Name
            ExecutablePath = $actualExecutable
            ProjectPath = $protectedUE54Project
            CommandLine = [string] $process.CommandLine
        }
    } | Sort-Object ProcessId)
    [pscustomobject] [ordered] @{
        State = if ($identities.Count -eq 0) { 'ABSENT' } else { 'PRESENT' }
        SessionCount = $identities.Count
        Sessions = @($identities)
    }
}

function Assert-ProtectedUE54Unchanged {
    if ($null -eq $script:protectedUE54Before) {
        throw 'Protected UE5.4/CAPSTONE identity set was not initialized.'
    }
    $after = Get-ProtectedUE54Identity
    $beforeCanonical = $script:protectedUE54Before |
        ConvertTo-Json -Compress -Depth 8
    $afterCanonical = $after | ConvertTo-Json -Compress -Depth 8
    if ($afterCanonical -cne $beforeCanonical) {
        throw 'Protected UE5.4/CAPSTONE identity set changed during the R25 transaction.'
    }
    $after
}

function Get-NativeTRIADUnrealProcesses {
    $enginePrefix = $nativeUE55EngineRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar
    @(
        Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
            if ($_.Name -notin @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe')) {
                return $false
            }
            $actualExecutable = if ([string]::IsNullOrWhiteSpace(
                    [string] $_.ExecutablePath)) {
                ''
            }
            else { [IO.Path]::GetFullPath([string] $_.ExecutablePath) }
            $isUE55 = $actualExecutable.StartsWith(
                $enginePrefix, [StringComparison]::OrdinalIgnoreCase)
            $isNativeTRIAD = -not [string]::IsNullOrWhiteSpace(
                    [string] $_.CommandLine) -and
                (Test-ExactCommandLineToken `
                    -CommandLine ([string] $_.CommandLine) `
                    -Token $nativeProjectFile)
            $isUE55 -or $isNativeTRIAD
        })
}

function Get-RemoteControlListeners {
    $rows = & "$env:SystemRoot\System32\netstat.exe" -ano -p TCP
    if ($LASTEXITCODE -ne 0) {
        throw 'netstat failed while checking Remote Control ownership.'
    }
    @(
        foreach ($line in $rows) {
            if ($line -match
                '^\s*TCP\s+\S+:30010\s+\S+\s+LISTENING\s+(\d+)\s*$') {
                [uint32] $Matches[1]
            }
        })
}

function Assert-NativeProjectIdle {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    [void] (Assert-ProtectedUE54Unchanged)
    $editors = @(Get-NativeTRIADUnrealProcesses)
    if ($editors.Count -ne 0) {
        $ids = @($editors | ForEach-Object { $_.ProcessId }) -join ','
        throw "UE5.5/native TRIAD Unreal helpers must be idle at '$Checkpoint' (PIDs=$ids)."
    }
    $listeners = @(Get-RemoteControlListeners)
    if ($listeners.Count -ne 0) {
        throw "RC port 30010 must be unowned at '$Checkpoint' (PIDs=$([string]::Join(',', $listeners)))."
    }
}

function Assert-LaunchedProcessIdentity {
    param([Parameter(Mandatory = $true)] $Expected)

    $actual = Get-ProcessIdentity -ProcessId ([uint32] $Expected.ProcessId)
    if (-not (Test-ProcessIdentityEqual -Expected $Expected `
            -Actual $actual)) {
        throw "Owned UE5.5 helper changed identity or disappeared: PID=$($Expected.ProcessId)."
    }
    $actual
}

function Test-ExpectedHelperLaunchIdentity {
    param(
        [Parameter(Mandatory = $true)] [AllowNull()] $Identity,
        [Parameter(Mandatory = $true)] [Diagnostics.Process] $Handle,
        [Parameter(Mandatory = $true)] [string] $MapPackage,
        [Parameter(Mandatory = $true)] [string] $LogFile
    )

    if ($null -eq $Identity -or $Identity.Name -cne 'UnrealEditor.exe' -or
        [string]::IsNullOrWhiteSpace($Identity.ExecutablePath) -or
        [string]::IsNullOrWhiteSpace($Identity.CommandLine) -or
        [IO.Path]::GetFullPath($Identity.ExecutablePath) -ine $editor -or
        [uint32] $Identity.ProcessId -ne [uint32] $Handle.Id) {
        return $false
    }
    $projectPattern =
        '(?i)(?:^|\s)(?:"[^"]+\.uproject"|[^\s"]+\.uproject)(?:\s|$)'
    [regex]::Matches($Identity.CommandLine, $projectPattern).Count -eq 1 -and
        (Test-ExactCommandLineToken $Identity.CommandLine `
            $nativeProjectFile) -and
        (Test-ExactCommandLineToken $Identity.CommandLine $MapPackage) -and
        $Identity.CommandLine.Contains(
            '-NoAutoSave', [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains(
            '-RemoteControlHttpServer', [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains(
            '-RCWebControlEnable', [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains(
            'WebControl.StartServer', [StringComparison]::Ordinal) -and
        $Identity.CommandLine.Contains(
            $LogFile, [StringComparison]::OrdinalIgnoreCase) -and
        [Math]::Abs($Identity.CreationUtcTicks -
            $Handle.StartTime.ToUniversalTime().Ticks) -le
                [TimeSpan]::FromSeconds(2).Ticks
}

function Assert-WorkspaceAndNativeCodeBoundary {
    param([Parameter(Mandatory = $true)] [string] $Checkpoint)

    [void] (Assert-SameState -Expected $script:selfPin `
        -Path $PSCommandPath -Label "wrapper at $Checkpoint")
    Assert-WorkspaceSourcesUnchanged
    Assert-NativeSourceTargets
    [void] (Assert-SameState -Expected $expectedProjectPin `
        -Path $nativeProjectFile -Label "project at $Checkpoint")
    Assert-ImmutableContentPins
    foreach ($pin in $script:builtProductPins) {
        [void] (Assert-SameState -Expected $pin -Path $pin.Path `
            -Label "built product at $Checkpoint")
    }
}

function Assert-OwnedBoundary {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $Checkpoint
    )

    [void] (Assert-ProtectedUE54Unchanged)
    Assert-WorkspaceAndNativeCodeBoundary -Checkpoint $Checkpoint
    [void] (Assert-LaunchedProcessIdentity -Expected $ExpectedProcess)
    $nativeEditors = @(Get-NativeTRIADUnrealProcesses)
    if ($nativeEditors.Count -ne 1 -or
        [uint32] $nativeEditors[0].ProcessId -ne
            [uint32] $ExpectedProcess.ProcessId) {
        throw "The exact helper is not the sole UE5.5/native TRIAD editor at '$Checkpoint'."
    }
    $listeners = @(Get-RemoteControlListeners)
    $foreignListeners = @($listeners | Where-Object {
        [uint32] $_ -ne [uint32] $ExpectedProcess.ProcessId
    })
    if ($listeners.Count -ne 1 -or $foreignListeners.Count -ne 0 -or
        [uint32] $listeners[0] -ne [uint32] $ExpectedProcess.ProcessId) {
        throw "RC port 30010 has a foreign owner or is not solely owned by the exact helper at '$Checkpoint'."
    }
}

function Invoke-RcCall {
    param(
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    $payload = @{
        objectPath = $ObjectPath
        functionName = $FunctionName
        parameters = $Parameters
    } | ConvertTo-Json -Depth 12 -Compress
    Invoke-RestMethod -Method Put -Uri $rcCallUri `
        -ContentType 'application/json' -Body $payload -TimeoutSec $TimeoutSec
}

function Invoke-OwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint "before RC $FunctionName"
    $result = Invoke-RcCall -ObjectPath $ObjectPath `
        -FunctionName $FunctionName -Parameters $Parameters `
        -TimeoutSec $TimeoutSec
    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint "after RC $FunctionName"
    $result
}

function Invoke-RequiredOwnedRcCall {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [Parameter(Mandatory = $true)] [string] $TextProperty,
        [Parameter(Mandatory = $true)] [string] $ExpectedPrefix,
        [hashtable] $Parameters = @{},
        [ValidateRange(1, 1800)] [int] $TimeoutSec = 60
    )

    $result = Invoke-OwnedRcCall -ExpectedProcess $ExpectedProcess `
        -ObjectPath $ObjectPath -FunctionName $FunctionName `
        -Parameters $Parameters -TimeoutSec $TimeoutSec
    $text = [string] $result.$TextProperty
    if ($result.ReturnValue -ne $true -or
        -not $text.StartsWith($ExpectedPrefix, [StringComparison]::Ordinal)) {
        throw "Required exact RC call failed: $FunctionName property=$TextProperty response=$text"
    }
    $result
}

function Wait-ExactProcessExit {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [Diagnostics.Process] $Handle,
        [ValidateRange(1, 300)] [int] $TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $Handle.Refresh()
        if ($Handle.HasExited) {
            $Handle.WaitForExit()
            return $true
        }
        $actual = Get-ProcessIdentity `
            -ProcessId ([uint32] $ExpectedProcess.ProcessId)
        if ($null -ne $actual -and
            -not (Test-ProcessIdentityEqual -Expected $ExpectedProcess `
                -Actual $actual)) {
            throw 'PID reuse or helper identity drift occurred while the exact process handle remained live.'
        }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $deadline)
    $false
}

function Invoke-GracefulQuit {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [Diagnostics.Process] $Handle
    )

    Assert-OwnedBoundary -ExpectedProcess $ExpectedProcess `
        -Checkpoint 'before RC QuitEditor'
    $connectionResult = 'returned'
    try {
        Invoke-RcCall -ObjectPath $quitLibrary -FunctionName 'QuitEditor' `
            -TimeoutSec 30 | Out-Null
    }
    catch {
        $connectionResult = $_.Exception.Message
    }
    [void] (Assert-ProtectedUE54Unchanged)
    $Handle.Refresh()
    if (-not $Handle.HasExited) {
        $actual = Get-ProcessIdentity `
            -ProcessId ([uint32] $ExpectedProcess.ProcessId)
        if ($null -ne $actual -and
            -not (Test-ProcessIdentityEqual -Expected $ExpectedProcess `
                -Actual $actual)) {
            throw 'Owned process identity changed after QuitEditor.'
        }
    }
    $connectionResult
}

function Stop-ExactHelperForContainment {
    param(
        [Parameter(Mandatory = $true)] $ExpectedProcess,
        [Parameter(Mandatory = $true)] [Diagnostics.Process] $Handle
    )

    $Handle.Refresh()
    if ($Handle.HasExited) {
        return
    }
    [void] (Assert-ProtectedUE54Unchanged)
    [void] (Assert-LaunchedProcessIdentity -Expected $ExpectedProcess)
    if ([uint32] $Handle.Id -ne [uint32] $ExpectedProcess.ProcessId) {
        throw 'Process handle does not match the exact UE5.5 helper; refusing containment.'
    }
    Stop-Process -Id ([int] $ExpectedProcess.ProcessId) -Force `
        -ErrorAction Stop
    if (-not (Wait-ExactProcessExit -ExpectedProcess $ExpectedProcess `
            -Handle $Handle -TimeoutSeconds 30)) {
        throw 'Exact UE5.5 helper did not exit after containment.'
    }
}

function Invoke-StrictColdEditorStage {
    param(
        [Parameter(Mandatory = $true)] [string] $Stage,
        [Parameter(Mandatory = $true)] [string] $MapPackage,
        [Parameter(Mandatory = $true)] [string] $ObjectPath,
        [Parameter(Mandatory = $true)] [string] $FunctionName,
        [Parameter(Mandatory = $true)] [string] $TextProperty,
        [Parameter(Mandatory = $true)] [string] $ExpectedPrefix,
        [ValidateRange(1, 1800)] [int] $CallTimeoutSeconds = 900
    )

    Assert-NativeProjectIdle -Checkpoint "before cold stage $Stage"
    Assert-WorkspaceAndNativeCodeBoundary -Checkpoint "before stage $Stage"
    $logRoot = Join-Path $transactionRoot 'logs'
    if (-not (Test-Path -LiteralPath $logRoot -PathType Container)) {
        [void] [IO.Directory]::CreateDirectory($logRoot)
    }
    $logFile = [IO.Path]::GetFullPath((Join-Path $logRoot `
        ("{0}.log" -f $Stage)))
    Assert-NewPath -Path $logFile -Root $transactionRoot `
        -Label "cold stage $Stage log"
    $argumentLine =
        "`"$nativeProjectFile`" $MapPackage -DisablePlugin=AirSim " +
        '-unattended -nop4 -NoSplash -NoSound -NoAutoSave -NoCompile ' +
        '-DDC=InstalledNoZenLocalFallback ' +
        "-LocalDataCachePath=`"$ddcRoot`" " +
        '-RemoteControlHttpServer -RCWebControlEnable ' +
        '-ExecCmds="WebControl.StartServer" ' +
        "-abslog=`"$logFile`""
    $process = $null
    $identity = $null
    $workflowError = $null
    $cleanupErrors = [Collections.Generic.List[string]]::new()
    $forcedContainment = $false
    $quitRequested = $false
    $exitCode = $null
    $response = $null
    $projectReport = ''
    try {
        $process = Start-Process -FilePath $editor `
            -ArgumentList $argumentLine -WorkingDirectory $nativeProjectRoot `
            -PassThru -WindowStyle Hidden
        $identityDeadline = [DateTime]::UtcNow.AddSeconds(20)
        do {
            $candidate = Get-ProcessIdentity `
                -ProcessId ([uint32] $process.Id)
            if (Test-ExpectedHelperLaunchIdentity -Identity $candidate `
                    -Handle $process -MapPackage $MapPackage `
                    -LogFile $logFile) {
                $identity = $candidate
                break
            }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $identityDeadline)
        if ($null -eq $identity) {
            throw "Cold stage $Stage did not prove its exact helper identity."
        }

        $editorDeadline = [DateTime]::UtcNow.AddSeconds(
            $EditorTimeoutSeconds)
        $projectIdentity = $null
        do {
            [void] (Assert-ProtectedUE54Unchanged)
            [void] (Assert-LaunchedProcessIdentity -Expected $identity)
            $listeners = @(Get-RemoteControlListeners)
            if ($listeners.Count -eq 1 -and
                [uint32] $listeners[0] -eq [uint32] $identity.ProcessId) {
                try {
                    $projectIdentity = Invoke-OwnedRcCall `
                        -ExpectedProcess $identity `
                        -ObjectPath $identityLibrary `
                        -FunctionName 'ValidateIstanaExploreRemoteControlProject' `
                        -Parameters @{ ExpectedProjectPath = $nativeProjectRoot } `
                        -TimeoutSec 30
                }
                catch {
                    $projectIdentity = $null
                }
                if ($null -ne $projectIdentity -and
                    $projectIdentity.ReturnValue -eq $true) {
                    break
                }
            }
            Start-Sleep -Seconds 2
        } while ([DateTime]::UtcNow -lt $editorDeadline)
        if ($null -eq $projectIdentity -or
            $projectIdentity.ReturnValue -ne $true) {
            throw "Cold stage $Stage did not prove exact project and RC ownership."
        }
        $projectReport = [string] $projectIdentity.OutReport
        $response = Invoke-RequiredOwnedRcCall `
            -ExpectedProcess $identity -ObjectPath $ObjectPath `
            -FunctionName $FunctionName -TextProperty $TextProperty `
            -ExpectedPrefix $ExpectedPrefix `
            -TimeoutSec $CallTimeoutSeconds
    }
    catch {
        $workflowError = $_.Exception
    }
    finally {
        if ($null -ne $process -and $null -eq $identity) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    $candidate = Get-ProcessIdentity `
                        -ProcessId ([uint32] $process.Id)
                    if (Test-ExpectedHelperLaunchIdentity `
                            -Identity $candidate -Handle $process `
                            -MapPackage $MapPackage -LogFile $logFile) {
                        $identity = $candidate
                    }
                    else {
                        throw 'Live started process could not be recovered as the exact owned helper.'
                    }
                }
            }
            catch {
                $cleanupErrors.Add(
                    "Startup identity recovery failed: $($_.Exception.Message)")
            }
        }
        if ($null -ne $process -and $null -ne $identity) {
            try {
                $process.Refresh()
                if (-not $process.HasExited) {
                    $quitRequested = $true
                    [void] (Invoke-GracefulQuit `
                        -ExpectedProcess $identity -Handle $process)
                }
            }
            catch {
                $cleanupErrors.Add("QuitEditor failed: $($_.Exception.Message)")
            }
            try {
                $process.Refresh()
                if (-not $process.HasExited -and
                    -not (Wait-ExactProcessExit -ExpectedProcess $identity `
                        -Handle $process `
                        -TimeoutSeconds $ShutdownTimeoutSeconds)) {
                    $forcedContainment = $true
                    Stop-ExactHelperForContainment `
                        -ExpectedProcess $identity -Handle $process
                }
                $process.Refresh()
                if ($process.HasExited) {
                    $process.WaitForExit()
                    $exitCode = $process.ExitCode
                }
            }
            catch {
                $cleanupErrors.Add(
                    "Exact helper containment failed: $($_.Exception.Message)")
            }
        }
    }

    $postErrors = [Collections.Generic.List[string]]::new()
    try { [void] (Assert-ProtectedUE54Unchanged) }
    catch { $postErrors.Add($_.Exception.Message) }
    if (@(Get-NativeTRIADUnrealProcesses).Count -ne 0) {
        $postErrors.Add('A UE5.5/native TRIAD helper remains after stage cleanup.')
    }
    if (@(Get-RemoteControlListeners).Count -ne 0) {
        $postErrors.Add('RC port 30010 remains owned after stage cleanup.')
    }
    if ($forcedContainment) {
        $postErrors.Add('The exact helper required forced containment.')
    }
    if (-not $quitRequested) {
        $postErrors.Add('Graceful QuitEditor was never requested.')
    }
    if ($exitCode -ne 0) {
        $postErrors.Add("Exact helper exit code was not zero: $exitCode")
    }
    if ($null -ne $workflowError -or $cleanupErrors.Count -ne 0 -or
        $postErrors.Count -ne 0) {
        $workflowText = if ($null -eq $workflowError) {
            'none'
        }
        else { $workflowError.Message }
        throw "Cold R25 stage $Stage failed: workflow={$workflowText} cleanup={$([string]::Join(' | ', @($cleanupErrors)))} post={$([string]::Join(' | ', @($postErrors)))}"
    }
    Assert-NativeProjectIdle -Checkpoint "after cold stage $Stage"
    $logPin = Get-FileState -Path $logFile
    if (-not $logPin.Present -or $logPin.Bytes -le 0) {
        throw "Cold R25 stage $Stage did not persist a non-empty exact log."
    }
    [pscustomobject] [ordered] @{
        Stage = $Stage
        MapPackage = $MapPackage
        Function = $FunctionName
        ExpectedPrefix = $ExpectedPrefix
        Message = [string] $response.$TextProperty
        ProjectIdentityReport = $projectReport
        ProcessIdentity = $identity
        ExitCode = $exitCode
        Log = $logPin
        RenderingCapable = $true
        FreshProcess = $true
        ForcedContainment = $false
    }
}

function New-VerifiedTransactionBackups {
    param(
        [Parameter(Mandatory = $true)] [object[]] $BuildRows
    )

    Assert-NativeProjectIdle -Checkpoint 'before transaction backups'
    $sourceBackupRoot = Join-Path $transactionRoot 'source_before'
    $buildBackupRoot = Join-Path $transactionRoot 'build_before'
    $contentBackupRoot = Join-Path $transactionRoot 'content_before'
    foreach ($path in @($sourceBackupRoot, $buildBackupRoot,
            $contentBackupRoot)) {
        Assert-NewPath -Path $path -Root $transactionRoot `
            -Label 'transaction backup directory'
        [void] [IO.Directory]::CreateDirectory($path)
    }

    for ($index = 0; $index -lt $script:sourceRows.Count; ++$index) {
        $row = $script:sourceRows[$index]
        if ($row.NativeBefore.Present) {
            $backup = Join-Path $sourceBackupRoot `
                ('{0:D2}.predecessor' -f $index)
            [void] (Copy-NewPinnedFile -Source $row.Destination `
                -Destination $backup -Expected $row.NativeBefore `
                -DestinationRoot $transactionRoot `
                -Label 'non-overwriting source predecessor backup')
            $row.Backup = Get-FileState -Path $backup
        }
    }
    for ($index = 0; $index -lt $BuildRows.Count; ++$index) {
        $row = $BuildRows[$index]
        if ($row.Before.Present) {
            $backup = Join-Path $buildBackupRoot `
                ('{0:D2}.predecessor' -f $index)
            [void] (Copy-NewPinnedFile -Source $row.Path `
                -Destination $backup -Expected $row.Before `
                -DestinationRoot $transactionRoot `
                -Label 'non-overwriting build predecessor backup')
            $row.Backup = Get-FileState -Path $backup
        }
    }
    $mapBackupPath = Join-Path $contentBackupRoot `
        'Istana_PublicView_Explore_v5d_hybrid.predecessor.umap'
    [void] (Copy-NewPinnedFile -Source $hybridMapFile `
        -Destination $mapBackupPath -Expected $expectedMapPin `
        -DestinationRoot $transactionRoot `
        -Label 'non-overwriting map predecessor backup')
    Assert-NativeProjectIdle -Checkpoint 'after transaction backups'
    Assert-WorkspaceSourcesUnchanged
    Assert-NativeSourcePredecessors
    Assert-InitialNativePins
    foreach ($row in $BuildRows) {
        [void] (Assert-SameState -Expected $row.Before -Path $row.Path `
            -Label 'build product after transaction backup')
    }
    [pscustomobject] [ordered] @{
        SourceRows = $script:sourceRows
        BuildRows = $BuildRows
        MapBackup = Get-FileState -Path $mapBackupPath
    }
}

function Publish-NativeSources {
    for ($index = 0; $index -lt $script:sourceRows.Count; ++$index) {
        $row = $script:sourceRows[$index]
        Assert-NativeProjectIdle `
            -Checkpoint "before native source publish $index"
        [void] (Assert-SameState -Expected $row.Target -Path $row.Source `
            -Label 'workspace source at publish')
        [void] (Assert-SameState -Expected $row.NativeBefore `
            -Path $row.Destination -Label 'native source at publish')
        $parent = [IO.Path]::GetDirectoryName($row.Destination)
        Assert-ExistingNonReparseDirectory -Path $parent `
            -Label 'native source destination parent'
        $stage = "$($row.Destination).triad_r25_${RunToken}_${index}.stage"
        Assert-NewPath -Path $stage -Root $nativeProjectRoot `
            -Label 'adjacent R25 source stage'
        [IO.File]::Copy($row.Source, $stage, $false)
        [void] (Assert-SameState -Expected $row.Target -Path $stage `
            -Label 'adjacent R25 source stage readback')
        $row.Stage = $stage
        if ($row.NativeBefore.Present) {
            $displaced = Join-Path (Join-Path $transactionRoot `
                'source_displaced') ('{0:D2}.predecessor' -f $index)
            $displacedParent = [IO.Path]::GetDirectoryName($displaced)
            if (-not (Test-Path -LiteralPath $displacedParent `
                    -PathType Container)) {
                [void] [IO.Directory]::CreateDirectory($displacedParent)
            }
            Assert-NewPath -Path $displaced -Root $transactionRoot `
                -Label 'atomic source displaced predecessor'
            [IO.File]::Replace($stage, $row.Destination, $displaced, $true)
            [void] (Assert-SameState -Expected $row.NativeBefore `
                -Path $displaced -Label 'atomic displaced source predecessor')
            $row.Displaced = Get-FileState -Path $displaced
        }
        else {
            [IO.File]::Move($stage, $row.Destination)
        }
        [void] (Assert-SameState -Expected $row.Target `
            -Path $row.Destination -Label 'published R25 native source')
    }
    Assert-NativeSourceTargets
    Assert-NativeProjectIdle -Checkpoint 'after native source promotion'
}

function Restore-OneFileFromBackup {
    param(
        [Parameter(Mandatory = $true)] $Before,
        [Parameter(Mandatory = $true)] [AllowNull()] $Backup,
        [Parameter(Mandatory = $true)] [string] $Destination,
        [Parameter(Mandatory = $true)] [string] $Label
    )

    Assert-NativeProjectIdle -Checkpoint "before $Label rollback"
    Assert-NoReparseAncestor -Path $Destination -Root $nativeProjectRoot `
        -Label $Label
    if ($Before.Present) {
        if ($null -eq $Backup) {
            throw "$Label lacks its required predecessor backup."
        }
        [void] (Assert-SameState -Expected $Before -Path $Backup.Path `
            -Label "$Label backup")
        $restoreStage = "$Destination.triad_r25_${RunToken}.restore"
        Assert-NewPath -Path $restoreStage -Root $nativeProjectRoot `
            -Label "$Label restore stage"
        [IO.File]::Copy($Backup.Path, $restoreStage, $false)
        [void] (Assert-SameState -Expected $Before -Path $restoreStage `
            -Label "$Label restore stage readback")
        if ([IO.File]::Exists($Destination)) {
            $discard = Join-Path (Join-Path $transactionRoot `
                'rollback_displaced') ([Guid]::NewGuid().ToString('N') +
                '.discard')
            $discardParent = [IO.Path]::GetDirectoryName($discard)
            if (-not (Test-Path -LiteralPath $discardParent `
                    -PathType Container)) {
                [void] [IO.Directory]::CreateDirectory($discardParent)
            }
            Assert-NewPath -Path $discard -Root $transactionRoot `
                -Label "$Label rollback displacement"
            [IO.File]::Replace(
                $restoreStage, $Destination, $discard, $true)
        }
        else {
            [IO.File]::Move($restoreStage, $Destination)
        }
        [void] (Assert-SameState -Expected $Before -Path $Destination `
            -Label "$Label restored predecessor")
    }
    else {
        if ($null -ne $Backup) {
            throw "$Label has backup material for an absent predecessor."
        }
        if ([IO.File]::Exists($Destination)) {
            $item = Get-Item -LiteralPath $Destination -Force
            if ($item.PSIsContainer -or
                ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) `
                    -ne 0) {
                throw "$Label rollback target is not a regular file."
            }
            [IO.File]::Delete($Destination)
        }
        [void] (Assert-SameState -Expected $Before -Path $Destination `
            -Label "$Label restored absence")
    }
}

function Restore-NativeSources {
    for ($index = $script:sourceRows.Count - 1; $index -ge 0; --$index) {
        $row = $script:sourceRows[$index]
        Restore-OneFileFromBackup -Before $row.NativeBefore `
            -Backup $row.Backup -Destination $row.Destination `
            -Label 'R25 native source'
    }
    Assert-NativeSourcePredecessors
}

function Restore-BuildProducts {
    param([Parameter(Mandatory = $true)] [object[]] $BuildRows)

    for ($index = $BuildRows.Count - 1; $index -ge 0; --$index) {
        $row = $BuildRows[$index]
        Restore-OneFileFromBackup -Before $row.Before `
            -Backup $row.Backup -Destination $row.Path `
            -Label 'R25 build product'
    }
}

function Remove-R25GeneratedContent {
    $inventory = Get-R25AssetInventory
    foreach ($artifact in @($inventory.Artifacts | Sort-Object Path -Descending)) {
        Assert-NoReparseAncestor -Path $artifact.Path -Root $r25AssetRoot `
            -Label 'R25 generated package rollback'
        $item = Get-Item -LiteralPath $artifact.Path -Force
        if ($item.PSIsContainer -or
            ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing non-regular R25 package rollback target: $($artifact.Path)"
        }
        [IO.File]::Delete($artifact.Path)
    }
    $remaining = Get-R25AssetInventory
    if ($remaining.Artifacts.Count -ne 0) {
        throw 'R25 package artifacts remain after exact rollback.'
    }
    if ($remaining.RootExists) {
        $materialsRoot = [IO.Path]::GetFullPath((Join-Path `
            $r25AssetRoot 'Materials'))
        if (Test-Path -LiteralPath $materialsRoot) {
            Assert-NoReparseAncestor -Path (Join-Path `
                $materialsRoot '__sentinel__') -Root $r25AssetRoot `
                -Label 'R25 empty Materials directory rollback'
            if (@(Get-ChildItem -LiteralPath $materialsRoot -Force).Count `
                    -ne 0) {
                throw "R25 rollback directory is not empty: $materialsRoot"
            }
            [IO.Directory]::Delete($materialsRoot, $false)
        }
        if (@(Get-ChildItem -LiteralPath $r25AssetRoot -Force).Count -ne 0) {
            throw 'R25 asset root is not empty after package rollback.'
        }
        [IO.Directory]::Delete($r25AssetRoot, $false)
    }
    if (Test-Path -LiteralPath $r25AssetRoot) {
        throw 'R25 asset root existence was not restored to ABSENT.'
    }
}

function Restore-R25Content {
    param(
        [Parameter(Mandatory = $true)] $MapBackup
    )

    Assert-NativeProjectIdle -Checkpoint 'before R25 content rollback'
    Remove-R25GeneratedContent
    Assert-NoReparseAncestor -Path $internalMapBackup `
        -Root $nativeProjectRoot -Label 'internal R25 map backup rollback'
    $internalState = Get-FileState -Path $internalMapBackup
    if ($internalState.Present) {
        if ($internalState.Bytes -ne $expectedMapPin.Bytes -or
            $internalState.Sha256 -cne $expectedMapPin.Sha256) {
            throw 'The native R25 map backup is not the exact predecessor; refusing deletion.'
        }
        [IO.File]::Delete($internalMapBackup)
    }
    Restore-OneFileFromBackup -Before $expectedMapPin `
        -Backup $MapBackup -Destination $hybridMapFile `
        -Label 'R25 hybrid map'
    [void] (Assert-SameState -Expected $expectedMapPin `
        -Path $hybridMapFile -Label 'restored R25 predecessor map')
    Assert-ImmutableContentPins
}

function Assert-FreshGeneratedR25Reflection {
    param(
        [Parameter(Mandatory = $true)] [object[]] $BuildRows
    )

    $targets = @(
        [pscustomobject] [ordered] @{
            RelativePath = 'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.gen.cpp'
            Functions = @(
                'ImportIstanaExploreV5DContextFacadeR25Assets',
                'ValidateIstanaExploreV5DContextFacadeR25Assets')
        }
        [pscustomobject] [ordered] @{
            RelativePath = 'Plugins\TRIADSensorFusion\Intermediate\Build\Win64\UnrealEditor\Inc\TRIADSensorFusionEditor\UHT\TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp'
            Functions = @(
                'ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap',
                'ValidateIstanaExploreV5DContextFacadeR25SuccessorMap')
        }
    )
    $functionNames = [Collections.Generic.List[string]]::new()
    $generatedReceipts = [Collections.Generic.List[object]]::new()
    foreach ($target in $targets) {
        $matches = @($BuildRows | Where-Object {
                $_.RelativePath -ceq $target.RelativePath
            })
        if ($matches.Count -ne 1) {
            throw "R25 generated-reflection target is not uniquely journalled: $($target.RelativePath)"
        }
        $row = $matches[0]
        if (-not $row.Before.Present -or $null -eq $row.After -or
            -not $row.After.Present) {
            throw "R25 generated-reflection target lacks complete before/after state: $($target.RelativePath)"
        }
        [void] (Assert-SameState -Expected $row.After -Path $row.Path `
            -Label 'fresh R25 generated reflection output')
        if ($row.After.Sha256 -ceq $row.Before.Sha256) {
            throw "Forced UHT did not replace stale generated reflection: $($target.RelativePath)"
        }
        $text = [IO.File]::ReadAllText($row.Path)
        [void] (Assert-SameState -Expected $row.After -Path $row.Path `
            -Label 'stable R25 generated reflection output')
        foreach ($functionName in $target.Functions) {
            if (-not $text.Contains(
                    $functionName, [StringComparison]::Ordinal)) {
                throw "Fresh generated reflection lacks R25 endpoint: $functionName"
            }
            $functionNames.Add($functionName)
        }
        $generatedReceipts.Add([pscustomobject] [ordered] @{
            RelativePath = $target.RelativePath
            Before = $row.Before
            After = $row.After
            Functions = $target.Functions
        })
    }
    if ($functionNames.Count -ne 4) {
        throw 'R25 generated-reflection endpoint count drifted from four.'
    }
    $editorDllBeforeRead = Get-FileState -Path $editorDll
    $editorDllText = [Text.Encoding]::ASCII.GetString(
        [IO.File]::ReadAllBytes($editorDll))
    foreach ($functionName in $functionNames) {
        if (-not $editorDllText.Contains(
                $functionName, [StringComparison]::Ordinal)) {
            throw "Built editor DLL lacks reflected R25 endpoint: $functionName"
        }
    }
    [void] (Assert-SameState -Expected $editorDllBeforeRead `
        -Path $editorDll -Label 'stable editor DLL reflection proof')
    [pscustomobject] [ordered] @{
        Status = 'FRESH_GENERATED_R25_REFLECTION_AND_DLL_TEXT_VALID'
        ForcedHeaderGeneration = $true
        EndpointCount = $functionNames.Count
        Endpoints = @($functionNames)
        GeneratedOutputs = @($generatedReceipts)
        EditorDll = $editorDllBeforeRead
    }
}

function Invoke-R25Build {
    param(
        [Parameter(Mandatory = $true)] [object[]] $BuildRows
    )

    Assert-NativeProjectIdle -Checkpoint 'before R25 module build'
    Assert-WorkspaceAndNativeCodeBoundary -Checkpoint 'before R25 build'
    [void] (Assert-SameState -Expected $expectedMapPin `
        -Path $hybridMapFile -Label 'pre-build map')
    $buildLog = Join-Path $transactionRoot 'build.log'
    Assert-NewPath -Path $buildLog -Root $transactionRoot `
        -Label 'R25 build log'
    $buildArguments = @(
        'UnrealEditor',
        'Win64',
        'Development',
        $nativeProjectFile,
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
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "R25 module build failed with exit code $exitCode. Log=$buildLog"
    }
    Assert-NativeProjectIdle -Checkpoint 'after R25 module build'
    Assert-WorkspaceSourcesUnchanged
    Assert-NativeSourceTargets
    [void] (Assert-SameState -Expected $expectedMapPin `
        -Path $hybridMapFile -Label 'post-build map')
    Assert-ImmutableContentPins
    foreach ($row in $BuildRows) {
        $row.After = Get-FileState -Path $row.Path
        if ($row.PinnedBinary -and
            (-not $row.After.Present -or $row.After.Bytes -le 0)) {
            throw "R25 build product is absent or empty: $($row.Path)"
        }
    }
    $runtimeAfter = Get-FileState -Path $runtimeDll
    $editorAfter = Get-FileState -Path $editorDll
    if ($runtimeAfter.Sha256 -ceq $expectedRuntimeDllPin.Sha256 -or
        $editorAfter.Sha256 -ceq $expectedEditorDllPin.Sha256) {
        throw 'R25 build did not produce distinct runtime and editor DLL receipts.'
    }
    $reflectionApi = Assert-FreshGeneratedR25Reflection `
        -BuildRows $BuildRows
    $script:builtDllPins = @($runtimeAfter, $editorAfter)
    $script:builtProductPins = @($BuildRows | ForEach-Object { $_.After })
    $logPin = Get-FileState -Path $buildLog
    if (-not $logPin.Present -or $logPin.Bytes -le 0) {
        throw 'R25 build did not persist a non-empty log.'
    }
    [pscustomobject] [ordered] @{
        Stage = 'BUILD'
        ExitCode = [int] $exitCode
        ModuleAllowlist = @('TRIADSensorFusion', 'TRIADSensorFusionEditor')
        MaximumParallelActions = 1
        ForceSourceDiscovery = '-NoUBTMakefiles'
        Log = $logPin
        RuntimeDll = $runtimeAfter
        EditorDll = $editorAfter
        ReflectionApi = $reflectionApi
        BuildProducts = @($BuildRows | ForEach-Object { $_.After })
        PinnedBinaryProductCount = 7
        ScopedBuildAndUhtProductCount = 103
        ObservedOutOfRosterMutationClosureCount = 34
        ConservativeObservedCompileSidecarCount = 16
        TotalBuildProductCount = 110
    }
}

function Assert-InitialNativePins {
    [void] (Assert-SameState -Expected $expectedProjectPin `
        -Path $nativeProjectFile -Label 'native project file')
    [void] (Assert-SameState -Expected $expectedMapPin `
        -Path $hybridMapFile -Label 'predecessor hybrid map')
    [void] (Assert-SameState -Expected $expectedRuntimeDllPin `
        -Path $runtimeDll -Label 'predecessor runtime DLL')
    [void] (Assert-SameState -Expected $expectedEditorDllPin `
        -Path $editorDll -Label 'predecessor editor DLL')
    Assert-ImmutableContentPins
    [void] (Get-R25AssetInventory -RequireAbsent)
    $absentInternalBackup = New-ExpectedState -Path $internalMapBackup `
        -Present $false -Bytes 0L -Sha256 'ABSENT'
    [void] (Assert-SameState -Expected $absentInternalBackup `
        -Path $internalMapBackup -Label 'pre-existing internal R25 map backup')
}

$script:selfPin = Get-FileState -Path $PSCommandPath
Assert-ExistingNonReparseDirectory -Path $repositoryUnrealRoot `
    -Label 'repository Unreal root'
$script:sourceRows = @(Get-WorkspaceSourceRows)
$script:buildRollbackContract = Assert-BuildRollbackRosterDefinition

if ($Execute -and $StaticSelfCheck) {
    throw '-Execute and -StaticSelfCheck are mutually exclusive.'
}

if ($StaticSelfCheck) {
    [pscustomobject] [ordered] @{
        Schema = "$schema.static_check"
        Status = 'STATIC_SELF_CHECK_PASS'
        Script = $script:selfPin
        ExecuteRequested = $false
        SourceAllowlistCount = $script:sourceRows.Count
        SourceAllowlist = @($script:sourceRows | ForEach-Object {
            [pscustomobject] [ordered] @{
                Area = $_.Area
                RelativePath = $_.RelativePath
                Target = $_.Target
                NativeBefore = $_.NativeBefore
            }
        })
        CurrentPins = [pscustomobject] [ordered] @{
            Project = $expectedProjectPin
            Map = $expectedMapPin
            RuntimeDll = $expectedRuntimeDllPin
            EditorDll = $expectedEditorDllPin
            ImmutableV5CMaterialsAndV2 = $immutableContentPins
        }
        BuildRollbackContract = $script:buildRollbackContract
        R25PrimaryAssetCount = $r25PrimaryAssetRelativePaths.Count
        NativeStageOrder = @(
            'PROMOTE_EXACT_NINE_SOURCES',
            'BUILD_TWO_ALLOWLISTED_MODULES',
            'IMPORT_EXACT_FIVE_R25_MATERIALS',
            'COLD_VALIDATE_R25_MATERIALS',
            'APPLY_EXACT_17_OVERRIDE_ROSTER',
            'COLD_VALIDATE_R25_SUCCESSOR_MAP',
            'IDEMPOTENT_SECOND_APPLY_NO_MAP_BYTE_CHANGE'
        )
        FailureRollbackOrder = @(
            'R25_CONTENT', 'ONE_HUNDRED_TEN_BUILD_PRODUCT_STATES',
            'NINE_SOURCE_DESTINATIONS')
        BackupPolicy = 'CREATE_NEW_NON_OVERWRITING_VERIFIED_BEFORE_MUTATION'
        ProtectedUE54Policy =
            'SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY'
        NativeUE55AndTRIADHelpersMustBeIdle = $true
        RemoteControlPolicy =
            'IDLE_EXCEPT_SOLE_EXACT_LAUNCHED_HELPER_OWNER'
        ExactHelperIdentityIncludes = @(
            'START_PROCESS_HANDLE_AND_PID', 'CREATION_TIME_WITHIN_2_SECONDS',
            'UE55_EDITOR_EXE', 'EXACT_PROJECT_TOKEN', 'EXACT_MAP_TOKEN',
            'EXACT_STAGE_LOG_TOKEN')
        NativeTreeReadOrWritten = $false
        UnrealBuildOrEditorLaunched = $false
    } | ConvertTo-Json -Depth 20
    return
}

Assert-ExistingNonReparseDirectory -Path $nativeProjectRoot `
    -Label 'native project root'
Assert-ExistingNonReparseDirectory -Path $nativeUE55EngineRoot `
    -Label 'UE5.5 engine root'
Assert-ExistingNonReparseDirectory -Path $ddcRoot `
    -Label 'native Derived Data Cache root'
[void] (Get-FileState -Path $buildTool)
[void] (Get-FileState -Path $editor)
Assert-NoReparseAncestor -Path $buildTool -Root $nativeUE55EngineRoot `
    -Label 'UE5.5 build tool'
Assert-NoReparseAncestor -Path $editor -Root $nativeUE55EngineRoot `
    -Label 'UE5.5 editor executable'
foreach ($boundaryPath in @(
        $nativeProjectFile,
        $hybridMapFile,
        $runtimeDll,
        $editorDll,
        $internalMapBackup,
        (Join-Path $r25AssetRoot '__sentinel__'))) {
    Assert-NoReparseAncestor -Path $boundaryPath -Root $nativeProjectRoot `
        -Label 'reviewed native transaction boundary'
}
$buildVersion = Get-Content -LiteralPath $buildVersionFile -Raw |
    ConvertFrom-Json
if ([int] $buildVersion.MajorVersion -ne 5 -or
    [int] $buildVersion.MinorVersion -ne 5) {
    throw 'The reviewed engine root no longer reports Unreal Engine 5.5.'
}
$script:protectedUE54Before = Get-ProtectedUE54Identity
Assert-NativeProjectIdle -Checkpoint 'initial preflight'
Assert-InitialNativePins
Assert-NativeSourcePredecessors
$buildRows = @(Get-PinnedBuildProductRows)
$dependencyClosure = Get-ReviewedBuildDependencyClosure
Assert-NewPath -Path $transactionRoot -Root $nativeProjectRoot `
    -Label 'fresh R25 transaction root'

$preflight = [pscustomobject] [ordered] @{
    Schema = $schema
    Status = 'PREFLIGHT_PASS'
    ExecuteRequested = [bool] $Execute
    RunToken = $RunToken
    SourceAllowlistCount = $script:sourceRows.Count
    SourceRows = $script:sourceRows
    BuildProductCount = $buildRows.Count
    BuildRollbackContract = $script:buildRollbackContract
    DependencyClosure = $dependencyClosure
    BuildProductPredecessors = @($buildRows | ForEach-Object { $_.Before })
    Project = $expectedProjectPin
    Map = $expectedMapPin
    RuntimeDll = $expectedRuntimeDllPin
    EditorDll = $expectedEditorDllPin
    ImmutableV5CMaterialsAndV2 = $immutableContentPins
    R25AssetsInitiallyAbsent = $true
    InternalMapBackupInitiallyAbsent = $true
    ProtectedUE54 = $script:protectedUE54Before
    TransactionRoot = $transactionRoot
    NativeMutated = $false
}

if (-not $Execute) {
    $preflight | ConvertTo-Json -Depth 20
    return
}

# Final gate immediately before the first D: write.
Assert-NativeProjectIdle -Checkpoint 'final pre-write gate'
[void] (Assert-SameState -Expected $script:selfPin -Path $PSCommandPath `
    -Label 'wrapper final pre-write pin')
Assert-WorkspaceSourcesUnchanged
Assert-NativeSourcePredecessors
Assert-InitialNativePins
foreach ($row in $buildRows) {
    [void] (Assert-SameState -Expected $row.Before -Path $row.Path `
        -Label 'build product final pre-write pin')
}
$finalDependencyClosure = Get-ReviewedBuildDependencyClosure
if (($dependencyClosure | ConvertTo-Json -Compress -Depth 20) -cne
    ($finalDependencyClosure | ConvertTo-Json -Compress -Depth 20)) {
    throw 'R25 dependency closure changed before the first native write.'
}
Assert-NewPath -Path $transactionRoot -Root $nativeProjectRoot `
    -Label 'fresh R25 transaction root'
[void] [IO.Directory]::CreateDirectory($transactionRoot)

$backups = $null
$stageResults = [Collections.Generic.List[object]]::new()
$transactionError = $null
try {
    $backups = New-VerifiedTransactionBackups -BuildRows $buildRows
    $prepared = [pscustomobject] [ordered] @{
        Schema = "$schema.prepared"
        Status = 'PREPARED'
        RunToken = $RunToken
        Preflight = $preflight
        SourceBackups = @($script:sourceRows | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath = $_.RelativePath
                Before = $_.NativeBefore
                Backup = $_.Backup
            }
        })
        BuildBackups = @($buildRows | ForEach-Object {
            [pscustomobject] [ordered] @{
                RelativePath = $_.RelativePath
                Before = $_.Before
                Backup = $_.Backup
            }
        })
        MapBefore = $expectedMapPin
        MapBackup = $backups.MapBackup
        BackupWritesWereNonOverwriting = $true
    }
    $preparedReceipt = Write-NewJsonReceipt `
        -Path (Join-Path $transactionRoot 'prepared.json') `
        -Value $prepared -Root $transactionRoot `
        -Label 'durable prepared receipt'

    Publish-NativeSources
    $stageResults.Add([pscustomobject] [ordered] @{
        Stage = 'PROMOTE_EXACT_NINE_SOURCES'
        Status = 'PASS'
        Count = 9
    })

    $buildResult = Invoke-R25Build -BuildRows $buildRows
    $stageResults.Add($buildResult)

    $importResult = Invoke-StrictColdEditorStage `
        -Stage '01_import_assets' -MapPackage $assetStageMapPackage `
        -ObjectPath $assetLibrary `
        -FunctionName 'ImportIstanaExploreV5DContextFacadeR25Assets' `
        -TextProperty 'OutMessage' `
        -ExpectedPrefix 'EXPLORE_V5D_CONTEXT_FACADE_R25_ASSET_IMPORT_PASS:'
    if (-not $importResult.Message.Contains(
            'five additive texture-free materials',
            [StringComparison]::Ordinal)) {
        throw 'R25 import acknowledgement lost its exact five-asset contract.'
    }
    $stageResults.Add($importResult)
    $assetsAfterImport = Get-R25AssetInventory -RequireComplete
    [void] (Assert-SameState -Expected $expectedMapPin `
        -Path $hybridMapFile -Label 'map after R25 asset import')
    Assert-ImmutableContentPins

    $coldAssetResult = Invoke-StrictColdEditorStage `
        -Stage '02_cold_validate_assets' -MapPackage $assetStageMapPackage `
        -ObjectPath $assetLibrary `
        -FunctionName 'ValidateIstanaExploreV5DContextFacadeR25Assets' `
        -TextProperty 'OutReport' `
        -ExpectedPrefix 'ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_ASSETS_VALID'
    $stageResults.Add($coldAssetResult)
    $assetsAfterColdValidation = Assert-R25AssetInventorySame `
        -Expected $assetsAfterImport -Label 'R25 asset after cold validation'
    [void] (Assert-SameState -Expected $expectedMapPin `
        -Path $hybridMapFile -Label 'map after cold R25 asset validation')

    $applyResult = Invoke-StrictColdEditorStage `
        -Stage '03_apply_map' -MapPackage $hybridMapPackage `
        -ObjectPath $hybridLibrary `
        -FunctionName 'ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap' `
        -TextProperty 'OutMessage' `
        -ExpectedPrefix 'EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_PASS:'
    foreach ($marker in @(
            'oneSave=true',
            'materialOverrides=17',
            'V2MeshUnchanged=true',
            'landmarksUnchanged=true',
            'providerSettingsUnchanged=true',
            'collisionNavigationSensorRfAuthority=false',
            'sharedV5CAssetsMutated=false')) {
        if (-not $applyResult.Message.Contains(
                $marker, [StringComparison]::Ordinal)) {
            throw "R25 apply acknowledgement lacks marker: $marker"
        }
    }
    $stageResults.Add($applyResult)
    $successorMapPin = Get-FileState -Path $hybridMapFile
    if (-not $successorMapPin.Present -or $successorMapPin.Bytes -le 0 -or
        $successorMapPin.Sha256 -ceq $expectedMapPin.Sha256) {
        throw 'R25 map apply did not produce a distinct non-empty successor receipt.'
    }
    [void] (Assert-SameState -Expected $expectedMapPin `
        -Path $internalMapBackup `
        -Label 'native endpoint non-overwriting predecessor backup')
    Assert-ImmutableContentPins
    [void] (Assert-R25AssetInventorySame -Expected $assetsAfterImport `
        -Label 'R25 asset after map apply')

    $coldMapResult = Invoke-StrictColdEditorStage `
        -Stage '04_cold_validate_map' -MapPackage $hybridMapPackage `
        -ObjectPath $hybridLibrary `
        -FunctionName 'ValidateIstanaExploreV5DContextFacadeR25SuccessorMap' `
        -TextProperty 'OutReport' `
        -ExpectedPrefix 'ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_MAP_VALID:'
    $stageResults.Add($coldMapResult)
    [void] (Assert-SameState -Expected $successorMapPin `
        -Path $hybridMapFile -Label 'successor map after cold validation')

    $idempotenceBefore = Get-FileState -Path $hybridMapFile
    $idempotenceResult = Invoke-StrictColdEditorStage `
        -Stage '05_idempotent_apply' -MapPackage $hybridMapPackage `
        -ObjectPath $hybridLibrary `
        -FunctionName 'ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap' `
        -TextProperty 'OutMessage' `
        -ExpectedPrefix 'IDEMPOTENT_EXPLORE_V5D_CONTEXT_FACADE_R25_ALREADY_VALID:'
    $idempotenceAfter = Assert-SameState -Expected $idempotenceBefore `
        -Path $hybridMapFile `
        -Label 'idempotent R25 second apply map receipt'
    $stageResults.Add($idempotenceResult)

    Assert-NativeProjectIdle -Checkpoint 'final committed postflight'
    Assert-WorkspaceAndNativeCodeBoundary -Checkpoint 'final commit'
    Assert-ImmutableContentPins
    [void] (Assert-R25AssetInventorySame -Expected $assetsAfterImport `
        -Label 'final R25 material asset')
    [void] (Assert-SameState -Expected $expectedMapPin `
        -Path $internalMapBackup -Label 'final internal predecessor backup')
    [void] (Assert-SameState -Expected $successorMapPin `
        -Path $hybridMapFile -Label 'final R25 successor map')

    $commit = [pscustomobject] [ordered] @{
        Schema = "$schema.commit"
        Status = 'PASS'
        RunToken = $RunToken
        TransactionRoot = $transactionRoot
        PreparedReceipt = $preparedReceipt
        SourceAllowlistCount = 9
        PinnedBinaryRollbackCount = 7
        ScopedBuildAndUhtRollbackCount = 103
        ObservedOutOfRosterMutationClosureCount = 34
        ConservativeObservedCompileSidecarCount = 16
        TotalBuildProductRollbackCount = 110
        WholeNativeTreeRollbackClaimed = $false
        R25AssetCount = 5
        MaterialOverrideCount = 17
        PredecessorMap = $expectedMapPin
        SuccessorMap = $successorMapPin
        RuntimeDll = $script:builtDllPins[0]
        EditorDll = $script:builtDllPins[1]
        InternalPredecessorBackup = Get-FileState -Path $internalMapBackup
        ImportedAssets = $assetsAfterImport
        ColdValidatedAssets = $assetsAfterColdValidation
        Stages = @($stageResults)
        OneSaveAcknowledged = $true
        IdempotentMapBytesAndHashUnchanged = $true
        V5CMaterialsAndV2MeshPinsUnchanged = $true
        ProtectedUE54 = Assert-ProtectedUE54Unchanged
        ForcedContainmentUsed = $false
        RecursiveDeleteUsed = $false
    }
    $commitReceipt = Write-NewJsonReceipt `
        -Path (Join-Path $transactionRoot 'commit.json') `
        -Value $commit -Root $transactionRoot -Label 'durable commit receipt'
    $commit | Add-Member -NotePropertyName CommitReceipt `
        -NotePropertyValue $commitReceipt
    $commit | ConvertTo-Json -Depth 20
}
catch {
    $transactionError = $_.Exception.Message
    $rollbackErrors = [Collections.Generic.List[string]]::new()
    if ($null -ne $backups) {
        try {
            Assert-NativeProjectIdle -Checkpoint 'rollback entry'
            Restore-R25Content -MapBackup $backups.MapBackup
        }
        catch { $rollbackErrors.Add("content=$($_.Exception.Message)") }
        try {
            Assert-NativeProjectIdle -Checkpoint 'before build rollback'
            Restore-BuildProducts -BuildRows $buildRows
        }
        catch { $rollbackErrors.Add("build=$($_.Exception.Message)") }
        try {
            Assert-NativeProjectIdle -Checkpoint 'before source rollback'
            Restore-NativeSources
        }
        catch { $rollbackErrors.Add("source=$($_.Exception.Message)") }
    }
    $failure = [pscustomobject] [ordered] @{
        Schema = "$schema.failure"
        Status = if ($rollbackErrors.Count -eq 0) {
            'FAILED_AND_ROLLED_BACK'
        }
        else { 'FAILED_ROLLBACK_INCOMPLETE' }
        RunToken = $RunToken
        TransactionRoot = $transactionRoot
        Error = $transactionError
        RollbackErrors = @($rollbackErrors)
        CompletedStages = @($stageResults)
        RollbackOrder = @(
            'R25_CONTENT', 'ONE_HUNDRED_TEN_BUILD_PRODUCT_STATES',
            'NINE_SOURCE_DESTINATIONS')
        BackupsRetained = $true
        RecursiveDeleteUsed = $false
        ProtectedUE54AtFailure = try { Assert-ProtectedUE54Unchanged }
            catch { [pscustomobject] @{ Error = $_.Exception.Message } }
    }
    try {
        [void] (Write-NewJsonReceipt `
            -Path (Join-Path $transactionRoot 'failure.json') `
            -Value $failure -Root $transactionRoot `
            -Label 'durable failure receipt')
    }
    catch {
        $rollbackErrors.Add("failureReceipt=$($_.Exception.Message)")
    }
    if ($rollbackErrors.Count -eq 0) {
        throw "R25 native transaction failed and exact rollback passed: $transactionError"
    }
    throw "R25 native transaction failed and rollback stopped fail-closed: original={$transactionError} rollback={$([string]::Join(' | ', @($rollbackErrors)))} backups={$transactionRoot}"
}
