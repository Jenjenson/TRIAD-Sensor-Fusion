# Istana R30-R33 guarded native execution

This runbook applies the prepared visual-fidelity sequence to the native UE 5.5
project without skipping its independent visual-review gates:

1. R30 facade-distance cues
2. R30 five-view facade and TreeRealism v3 capture and explicit review
3. R31 broad-shell building materials
4. R31 five-pose/six-image capture, including same-camera Nanite/raster comparison, and explicit review
5. R32 medium-distance turf
6. R32 ten-view capture and explicit review
7. R33 Cesium World Terrain visual-reference integration

The wrappers are deliberately fail-closed. Do not bypass their receipt, source,
process, or memory checks. The current R30 capture harness uses eight shader
workers, has no fixed process-RAM ceiling, and keeps a 2 GiB emergency system-
commit reserve at launch and continuously. These are CPU/system-memory controls,
not GPU-VRAM controls. The completed R30 transaction and the R31--R33
transaction/capture wrappers retain their 10 GiB launch, 6 GiB continuous, and
12 GiB private-memory limits. Use PowerShell 7 (`pwsh`).

## Current checkpoint — 8 September 2026

Do **not** rerun the R30 native transaction. The authoritative native state is
already `r30-native-20260908-19`, status `COMMITTED`:

```text
D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1\r30-native-20260908-19\commit.json
65,411 bytes
SHA-256 F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F
```

That receipt sets `VisualCaptureAccepted=false` and
`CaptureRevalidationRequired=true`. Run 05 is retained historical failure
evidence. Run 08 later rolled back cleanly after its owned first prewarm exited.
The latest completed attempt, `r30-capture-20260908-09`, proved the eight-worker
shader path and an exact Columnar-tree DDC hit, then failed closed on an
`AirSimTriadRuntime` optical-flow material compile error. Its immutable rollback
is 658 bytes with SHA-256
`5827225197FE3BC4F57B28D36947F6C3642713410BD768B674C2F5D122144B87`:

```text
D:\triad\TRIAD_R30Evidence\r30-capture-20260908-09\rollback.json
```

It has no rollback errors and produced neither `pending-visual-review.json` nor
an accepted capture receipt. R31, R32, and R33 therefore remain inadmissible and
non-native.

The earlier incomplete run-04 cleanup is now separately certified at:

```text
D:\triad\TRIAD_R30Evidence\r30-capture-20260908-04\recovery\r30-capture-20260908-04-recovery-02-certification\receipt.json
8,224 bytes
SHA-256 2CBAEFE6AB2B268B21788D2BDA7239B44817897C36F392BDC6E95A7B613AE014
Status CERTIFIED_RECOVERED_AFTER_FAILED_RECEIPT
```

That certification is a required prerequisite for any later fresh capture
attempt; it is not sufficient by itself, is not a visual pass, and does not
authorize R31.

## Verify memory admission before starting

Free disk space and free commit memory are different resources. Deleting assets,
Cesium cache data, or Unreal build products does not by itself increase the
Windows commit limit. Run this read-only check immediately before every guarded
native stage:

```powershell
$os = Get-CimInstance Win32_OperatingSystem
$freeCommitBytes = [int64]$os.FreeVirtualMemory * 1KB
[pscustomobject]@{
    DDriveFreeGiB = [math]::Round((Get-PSDrive -Name D).Free / 1GB, 3)
    FreeCommitGiB = [math]::Round($freeCommitBytes / 1GB, 3)
    R30CaptureRequiredGiB = 2
    R30CaptureLaunchAdmitted = $freeCommitBytes -ge 2GB
    R31ToR33RequiredGiB = 10
    R31ToR33LaunchAdmitted = $freeCommitBytes -ge 10GB
}
```

For an R30 capture, require `R30CaptureLaunchAdmitted=True`; for any R31--R33
stage, require `R31ToR33LaunchAdmitted=True`. The target drive must also have
enough space for another isolated cook plus rollback copy. A rollback is roughly
15 GiB on this workstation, and accumulated retries can exceed 120 GiB. The
wrappers enforce commit memory, but free-disk demand varies by stage and no
single disk number is an acceptance substitute. If commit is low, close only
operator-approved applications or have an administrator adjust the Windows page
file and restart; the wrappers intentionally do not automate either action.

Never delete or edit native transaction receipts, capture evidence, rollback
backups, run-04 recovery evidence, or its certification to make space. Never
clear `Binaries`, `Intermediate`, cooked output, or derived-data caches while a
wrapper is active. Archive or remove only an exact maintainer-approved target
after Unreal and every owned helper have stopped.

## Prepare one PowerShell 7 session

Use fresh safe tokens. Transaction tokens are limited to 44 characters and
capture tokens to 64 characters. The execute and acceptance calls for one
capture must reuse the same capture token.

The absolute roots below are host-specific to the audited workstation. If the
repository or native project moves, replace only those filesystem roots; do not
change the Unreal package `/Game/Maps/Istana_PublicView_Explore_v5d_hybrid` or
adopt a different map as an apparent shortcut.

```powershell
Set-Location 'C:\Users\Lyz\Documents\Codex\2026-08-03\elston-need-ur-help-on-linking\work\TRIAD-Sensor-Fusion-Repo'

function Pin($Path) {
    $item = Get-Item -LiteralPath $Path
    [pscustomobject]@{
        Bytes = [int64]$item.Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    }
}

function Sha($Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Json($Path) {
    Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -Depth 64
}

$native = 'D:\triad\TRIAD'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$r30c = "r30-cap-$stamp"
$r31t = "r31-tx-$stamp"
$r31c = "r31-cap-$stamp"
$r32t = "r32-tx-$stamp"
$r32c = "r32-cap-$stamp"
$r33t = "r33-tx-$stamp"
$r33c = "r33-cap-$stamp"
```

If the shell will not remain open across a human-review gate, record each token
and re-create the helper functions and receipt variables in the next PowerShell
7 session. Never guess a token from a directory listing.

## 1. Verify the existing R30-19 commit and recovery certification

R30-19 already promoted the receipt-pinned TreeRealism v3/runtime/editor closure
and façade-distance cues. Do not call the R30 transaction wrapper again, do not
run the tree builder as a separate mutation, and do not manually copy any staged
asset. Pin the existing receipt, its three native artifacts, and the run-04
recovery certification instead:

```powershell
$r30tp = "$native\Saved\TRIAD\NativeTransactions\V5DContextFacadeR30V1\r30-native-20260908-19\commit.json"
$r30th = 'F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F'
$r30 = Json $r30tp
if ((Get-Item -LiteralPath $r30tp).Length -ne 65411 -or
    (Sha $r30tp) -cne $r30th -or
    $r30.Schema -cne 'triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1' -or
    $r30.Status -cne 'COMMITTED' -or
    $r30.RunToken -cne 'r30-native-20260908-19' -or
    $r30.VisualCaptureAccepted -ne $false -or
    $r30.CaptureRevalidationRequired -ne $true) {
    throw 'R30-19 transaction identity or semantics changed.'
}

$expected = @(
    [pscustomobject]@{Path="$native\Content\Maps\Istana_PublicView_Explore_v5d_hybrid.umap";Bytes=37468415L;Sha256='126D26B8CAA67C1CF9219EAA693F4A5F28A0CB7E24FF82F815D1CDDF566D97C7'},
    [pscustomobject]@{Path="$native\Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll";Bytes=5076992L;Sha256='100B061CC5677508A67D644687923CA58F68F072305B6FF62B6BCD363D472028'},
    [pscustomobject]@{Path="$native\Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusionEditor.dll";Bytes=8448000L;Sha256='3589C23333641DF7552B844D7A388E663B485B62AD8BBE9A471214FB78735553'}
)
foreach ($row in $expected) {
    $actual = Pin $row.Path
    if ($actual.Bytes -ne $row.Bytes -or $actual.Sha256 -cne $row.Sha256) {
        throw "R30-19 native pin changed: $($row.Path)"
    }
}

$certp = 'D:\triad\TRIAD_R30Evidence\r30-capture-20260908-04\recovery\r30-capture-20260908-04-recovery-02-certification\receipt.json'
$certh = '2CBAEFE6AB2B268B21788D2BDA7239B44817897C36F392BDC6E95A7B613AE014'
$cert = Json $certp
$expectedCertPins = @(
    [pscustomobject]@{Bytes=73145L;Sha256='0DC67FA74F1B9F09453FC542065BFD2DB716E2AFCC2D7E80D87A733789F8A292'},
    [pscustomobject]@{Bytes=994L;Sha256='6B84089FD0B9C9B7274D38FFA8143E9DA867F1F2E2AD73E07548D0DF335F4808'},
    [pscustomobject]@{Bytes=65411L;Sha256='F4E779462F4D9140DEEA2AD42539A082CDA2DFFF890FBA01D35DD2B6641D7E6F'}
)
if ((Get-Item -LiteralPath $certp).Length -ne 8224 -or
    (Sha $certp) -cne $certh -or
    $cert.Schema -cne 'triad.istana_explore_v5d.r30_capture_rollback_recovery_certification.v1' -or
    $cert.Status -cne 'CERTIFIED_RECOVERED_AFTER_FAILED_RECEIPT' -or
    $cert.SourceRunToken -cne 'r30-capture-20260908-04' -or
    $cert.FailedRecoveryToken -cne 'r30-capture-20260908-04-recovery-01' -or
    $cert.CertificationToken -cne 'r30-capture-20260908-04-recovery-02-certification' -or
    $cert.NativeClosure.FileCount -ne 3332 -or
    $cert.NativeClosure.TotalBytes -ne 16529511957 -or
    $cert.NativeClosure.ManifestSha256 -cne 'C8872BCAE283B311EBB08F4863A4F908B59141744C592D6CD903F6B234F951AD' -or
    $cert.PchBeforeFullClosureHashing.RestartManagerHolderCount -ne 0 -or
    $cert.PchBeforeFullClosureHashing.ExclusiveOpenProbe -cne 'PASS' -or
    $cert.RepeatedRecoveryMutation -ne $false -or
    $cert.NativeProjectWritten -ne $false -or
    @($cert.ExactDelta.psobject.Properties.Value | Where-Object { $_ -ne 0 }).Count -ne 0) {
    throw 'R30 run-04 recovery certification identity or semantics changed.'
}
if ($cert.RecoveryImplementation.Bytes -ne 73479 -or
    $cert.RecoveryImplementation.Sha256 -cne '9AD8F4D5D84DC9D5EDA077545E85F17B0593ACBC3594C9714345C34DBC3BA9DA' -or
    $cert.FailedRecoveryReceiptBefore.Bytes -ne 34145 -or
    $cert.FailedRecoveryReceiptBefore.Sha256 -cne 'B1FEE7D1BC0BD3F8A4CF1FAB92CCCFAA4119399E94D5117C27617E44AEA827EA') {
    throw 'R30 run-04 recovery implementation or failed-receipt pin changed.'
}
foreach ($which in @('EvidencePinsBefore', 'EvidencePinsAfter')) {
    $actualPins = @($cert.$which)
    if ($actualPins.Count -ne $expectedCertPins.Count) {
        throw "R30 run-04 $which count changed."
    }
    for ($i = 0; $i -lt $expectedCertPins.Count; $i++) {
        if ($actualPins[$i].Present -ne $true -or
            $actualPins[$i].Bytes -ne $expectedCertPins[$i].Bytes -or
            $actualPins[$i].Sha256 -cne $expectedCertPins[$i].Sha256) {
            throw "R30 run-04 $which prerequisite pin changed at index $i."
        }
    }
}
```

## 2. Resolve the latest rollback, then capture and review R30

Run 05 remains immutable historical evidence; it is not the active checkpoint.
Do not reuse `r30-capture-20260908-09`: its latest rollback receipt is
`D:\triad\TRIAD_R30Evidence\r30-capture-20260908-09\rollback.json` (658 bytes,
SHA-256
`5827225197FE3BC4F57B28D36947F6C3642713410BD768B674C2F5D122144B87`).
First diagnose and correct the recorded AirSim optical-flow material compile
failure without changing the R30-19 receipt or bypassing a guard, and require
the prescribed contract tests for the fix. Only after the native state is idle,
the exact Section 1 pins still pass, and a genuinely new evidence token is
selected may a maintainer run:

```powershell
$run09p = 'D:\triad\TRIAD_R30Evidence\r30-capture-20260908-09\rollback.json'
$run09 = Json $run09p
if ((Get-Item -LiteralPath $run09p).Length -ne 658 -or
    (Sha $run09p) -cne '5827225197FE3BC4F57B28D36947F6C3642713410BD768B674C2F5D122144B87' -or
    $run09.Schema -cne 'triad.istana_explore_v5d.r30_player0_capture.v1' -or
    $run09.Status -cne 'ROLLED_BACK' -or
    $run09.RunToken -cne 'r30-capture-20260908-09' -or
    $run09.Failure -cne 'R30 isolated TreeRealism DDC prewarm 01 SM_IPV5D_Tree_ColumnarNarrow_NearLOD0 contains fail-closed marker: Failed to compile Material' -or
    $run09.NativeMutationQuiescenceRequired -ne $true -or
    $run09.NativeMutationQuiescenceProven -ne $true -or
    $run09.ExactOwnedProcessContainmentOnly -ne $true -or
    $run09.SourceRestorationAttempted -ne $true -or
    $run09.BuildTreeRestorationAttempted -ne $true -or
    $run09.IsolatedCookCaptureDdcCleanupAttempted -ne $true -or
    $run09.R30MapAndContentPreserved -ne $true -or
    $run09.AirSimTriadRuntimePreserved -ne $true -or
    @($run09.RollbackErrors).Count -ne 0 -or
    (Test-Path -LiteralPath 'D:\triad\TRIAD_R30Evidence\r30-capture-20260908-09\pending-visual-review.json') -or
    (Test-Path -LiteralPath 'D:\triad\TRIAD_R30Evidence\r30-capture-20260908-09\commit.json')) {
    throw 'R30 run-09 rollback identity or semantics changed.'
}

& .\scripts\Capture-IstanaExploreV5DR30Player0Evidence.ps1 `
  -Execute -RunToken $r30c `
  -R30CommitReceipt $r30tp `
  -ExpectedR30CommitReceiptSha256 $r30th

$r30pp = "D:\triad\TRIAD_R30Evidence\$r30c\pending-visual-review.json"
$r30ph = Sha $r30pp
(Json $r30pp).Captures | Select-Object -ExpandProperty Image
```

The wrapper creates fresh cooked output with a run-local isolated DDC. It
hash-checks and copies the approved seed pair, performs largest-first bounded
per-mesh prewarms, proves all five tree packages in a cache-only probe, and then
runs the full closure cook with eight shader workers. A DDC hit or successful
mechanical cook is not visual acceptance.

If the wrapper emits `failed.json` or `rollback.json` instead of the pending
receipt, stop. Preserve the complete evidence root and do not proceed to R31.

Open and inspect every listed PNG. Confirm that all five views show the intended
facade cues, no black/loading terrain, no duplicate or blank image, no broken
vegetation ownership, and no obvious new visual regression. Mechanical image
checks do not count as this review. In the `008m` and `002m` views, also confirm
that canopy/bark response is visibly improved while leaf-alpha silhouettes,
wind motion, tree positions and geography remain intact. The offline CPU audit
cannot accept this native tree review.

```powershell
& .\scripts\Capture-IstanaExploreV5DR30Player0Evidence.ps1 `
  -AcceptVisualReview -ConfirmFiveImagesReviewed -RunToken $r30c `
  -PendingCaptureReceipt $r30pp `
  -ExpectedPendingCaptureReceiptSha256 $r30ph

$r30cp = "D:\triad\TRIAD_R30Evidence\$r30c\commit.json"
$r30ch = Sha $r30cp
$r30cJ = Json $r30cp
```

The acceptance command is a human-review gate. An agent may run the mechanical
capture and decode the files, but it must not supply
`-ConfirmFiveImagesReviewed` or publish acceptance unless a human explicitly
attests to this exact pending receipt and all five listed images.

## 3. Commit, capture, and review R31

R31 is not currently native. Run this section only after Section 2 has produced
an explicitly human-accepted R30 `commit.json`; source readiness or a pending
receipt is not sufficient.

```powershell
$ch = Pin "$native\Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Public\TRIADIstanaExploreV5DContextPolicyActor.h"
$cs = Pin "$native\Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private\TRIADIstanaExploreV5DContextPolicyActor.cpp"

& .\scripts\Invoke-IstanaExploreV5DBroadShellR31NativeTransactionV1.ps1 `
  -RunToken $r31t -Execute -RequireR30Predecessor `
  -R30CommitReceipt $r30tp -ExpectedR30CommitReceiptSha256 $r30th `
  -R30CaptureCommitReceipt $r30cp -ExpectedR30CaptureCommitReceiptSha256 $r30ch `
  -ExpectedMapBytes $r30cJ.Map.Bytes -ExpectedMapSha256 $r30cJ.Map.Sha256 `
  -ExpectedRuntimeDllBytes $r30cJ.RuntimeEditorDll.Bytes -ExpectedRuntimeDllSha256 $r30cJ.RuntimeEditorDll.Sha256 `
  -ExpectedEditorDllBytes $r30cJ.EditorDll.Bytes -ExpectedEditorDllSha256 $r30cJ.EditorDll.Sha256 `
  -ExpectedContextPolicyHeaderBytes $ch.Bytes -ExpectedContextPolicyHeaderSha256 $ch.Sha256 `
  -ExpectedContextPolicySourceBytes $cs.Bytes -ExpectedContextPolicySourceSha256 $cs.Sha256

$r31tp = "$native\Saved\TRIAD\NativeTransactions\V5DBroadShellR31V1\$r31t\commit.json"
$r31th = Sha $r31tp

& .\scripts\Capture-IstanaExploreV5DR31Player0Evidence.ps1 `
  -Execute -RunToken $r31c `
  -R31CommitReceipt $r31tp -ExpectedR31CommitReceiptSha256 $r31th

$r31pp = "D:\triad\TRIAD_R31Evidence\$r31c\pending-visual-review.json"
$r31ph = Sha $r31pp
(Json $r31pp).Captures | Select-Object -ExpandProperty Image
```

Review the five baseline images for readable windows/storeys/roof separation,
plausible scale, retained surrounding buildings, correct trees, clean terrain,
and no material tiling or blank shell. Also compare the Nanite-on and
raster-fallback images at `surroundings_oblique_macdonald`; reject cadence,
facade-family, texture-phase, orientation, apparent-scale, normal/roughness,
seam, silhouette, or geometry changes caused only by that render-path switch.
There are five poses but six PNGs. Then explicitly accept:

```powershell
& .\scripts\Capture-IstanaExploreV5DR31Player0Evidence.ps1 `
  -AcceptVisualReview -ConfirmFiveImagesReviewed `
  -ConfirmNaniteRasterPairReviewedAndAccepted -RunToken $r31c `
  -PendingCaptureReceipt $r31pp `
  -ExpectedPendingCaptureReceiptSha256 $r31ph

$r31cp = "D:\triad\TRIAD_R31Evidence\$r31c\commit.json"
$r31ch = Sha $r31cp
$r31cJ = Json $r31cp
```

Both confirmation switches require a human review of this exact six-PNG set.
An agent's image decode or mechanical comparison cannot supply either
attestation.

## 4. Commit, capture, and review R32

R32 is not currently native. Run it only after the R31 capture has its accepted,
hash-pinned `commit.json`.

```powershell
& .\scripts\Invoke-IstanaExploreV5DMediumDistanceTurfR32NativeTransactionV1.ps1 `
  -RunToken $r32t -Execute -RequireR31Predecessor `
  -R30CommitReceipt $r30tp -ExpectedR30CommitReceiptSha256 $r30th `
  -R30CaptureCommitReceipt $r30cp -ExpectedR30CaptureCommitReceiptSha256 $r30ch `
  -R31CommitReceipt $r31tp -ExpectedR31CommitReceiptSha256 $r31th `
  -R31CaptureCommitReceipt $r31cp -ExpectedR31CaptureCommitReceiptSha256 $r31ch `
  -ExpectedMapBytes $r31cJ.Map.Bytes -ExpectedMapSha256 $r31cJ.Map.Sha256 `
  -ExpectedRuntimeDllBytes $r31cJ.RuntimeEditorDll.Bytes -ExpectedRuntimeDllSha256 $r31cJ.RuntimeEditorDll.Sha256 `
  -ExpectedEditorDllBytes $r31cJ.EditorDll.Bytes -ExpectedEditorDllSha256 $r31cJ.EditorDll.Sha256 `
  -ExpectedGroundHeaderBytes $r31cJ.GroundHeader.Bytes -ExpectedGroundHeaderSha256 $r31cJ.GroundHeader.Sha256 `
  -ExpectedGroundSourceBytes $r31cJ.GroundSource.Bytes -ExpectedGroundSourceSha256 $r31cJ.GroundSource.Sha256

$r32tp = "$native\Saved\TRIAD\NativeTransactions\V5DMediumDistanceTurfR32V1\$r32t\commit.json"
$r32th = Sha $r32tp

& .\scripts\Capture-IstanaExploreV5DR32Player0Evidence.ps1 `
  -Execute -RunToken $r32c `
  -R32CommitReceipt $r32tp -ExpectedR32CommitReceiptSha256 $r32th

$r32pp = "D:\triad\TRIAD_R32Evidence\$r32c\pending-visual-review.json"
$r32ph = Sha $r32pp
(Json $r32pp).Captures | Select-Object -ExpandProperty Image
```

Review all ten images. In addition to the inherited five views, inspect the
12 m, 50 m, 65 m, 90 m, and 95 m turf probes. Confirm defined but closely mown
turf at 12 m, 20 m, and 50 m, then a natural fade through the cull band: no hard
wall or popping, no moire, no pavement intrusion, and no oversized blades.

```powershell
& .\scripts\Capture-IstanaExploreV5DR32Player0Evidence.ps1 `
  -AcceptVisualReview -ConfirmTenImagesReviewed -RunToken $r32c `
  -PendingCaptureReceipt $r32pp `
  -ExpectedPendingCaptureReceiptSha256 $r32ph

$r32cp = "D:\triad\TRIAD_R32Evidence\$r32c\commit.json"
$r32ch = Sha $r32cp
$r32cJ = Json $r32cp
```

`-ConfirmTenImagesReviewed` is a human attestation. Do not pass it based on an
agent-only or automated inspection.

## 5. Commit R33 Cesium World Terrain reference

R33 is not currently native. Run it only after the exact accepted R30, R31, and
R32 transaction/capture chain exists and all six predecessor receipt hashes
have been re-pinned.

The R33 wrapper requires the canonical identity of the complete plugin source
tree recorded by R32, not a filesystem-order-dependent hash.

```powershell
$rows = @(
    $r32cJ.NativePluginSourceTree.BaselineAfterCaptureSourcePromotion |
      ForEach-Object {
          [pscustomobject][ordered]@{
              RelativePath = [string]$_.RelativePath
              Bytes = [int64]$_.Bytes
              Sha256 = [string]$_.Sha256
          }
      } | Sort-Object RelativePath
)
$treeJson = ConvertTo-Json $rows -Depth 6 -Compress
$treeSha = [Convert]::ToHexString(
    [Security.Cryptography.SHA256]::HashData(
        [Text.Encoding]::UTF8.GetBytes($treeJson)))

& .\scripts\Invoke-IstanaExploreV5DR33CesiumWorldTerrainReferenceNativeTransactionV1.ps1 `
  -RunToken $r33t -Execute -RequireR32Predecessor `
  -R30CommitReceipt $r30tp -ExpectedR30CommitReceiptSha256 $r30th `
  -R30CaptureCommitReceipt $r30cp -ExpectedR30CaptureCommitReceiptSha256 $r30ch `
  -R31CommitReceipt $r31tp -ExpectedR31CommitReceiptSha256 $r31th `
  -R31CaptureCommitReceipt $r31cp -ExpectedR31CaptureCommitReceiptSha256 $r31ch `
  -R32CommitReceipt $r32tp -ExpectedR32CommitReceiptSha256 $r32th `
  -R32CaptureCommitReceipt $r32cp -ExpectedR32CaptureCommitReceiptSha256 $r32ch `
  -ExpectedMapBytes $r32cJ.Map.Bytes -ExpectedMapSha256 $r32cJ.Map.Sha256 `
  -ExpectedRuntimeDllBytes $r32cJ.RuntimeEditorDll.Bytes -ExpectedRuntimeDllSha256 $r32cJ.RuntimeEditorDll.Sha256 `
  -ExpectedEditorDllBytes $r32cJ.EditorDll.Bytes -ExpectedEditorDllSha256 $r32cJ.EditorDll.Sha256 `
  -ExpectedGroundHeaderBytes $r32cJ.GroundHeader.Bytes -ExpectedGroundHeaderSha256 $r32cJ.GroundHeader.Sha256 `
  -ExpectedGroundSourceBytes $r32cJ.GroundSource.Bytes -ExpectedGroundSourceSha256 $r32cJ.GroundSource.Sha256 `
  -ExpectedNativePluginSourceTreeSha256 $treeSha

$r33p = "$native\Saved\TRIAD\NativeTransactions\V5DCesiumWorldTerrainReferenceR33V1\$r33t\commit.json"
Json $r33p
```

R33 initially leaves Google Photorealistic 3D Tiles primary and keeps Cesium
World Terrain hidden and update-suspended. The optional terrain presentation is
visual-reference-only until native PIE transition, entitlement, datum,
performance, and simulation-authority checks are completed. It must not be
described as survey-grade terrain.

Use Unreal's **Window > Cesium** panel and its supported account/server workflow
for any authorised access. Never paste, print, commit, screenshot, or record a
provider token or fingerprint. Non-secret verification is limited to the
expected asset IDs, non-null server references, visible presentation state, and
authorization/rate-limit errors. The wrappers deliberately prove pointer and
state contracts without disclosing a secret; they do not prove entitlement or a
successful stream.

## 6. Capture and explicitly review the R33 Player0 comparison

Do not run this stage until the R33 transaction above is committed. The
capture wrapper independently replays that receipt and its complete accepted
R30/R31/R32 chain, then emits a pending receipt only.

```powershell
$r33h = Sha $r33p

& .\scripts\Capture-IstanaExploreV5DR33Player0Evidence.ps1 `
  -Execute -RunToken $r33c `
  -R33CommitReceipt $r33p `
  -ExpectedR33CommitReceiptSha256 $r33h

$r33pp = "D:\triad\TRIAD_R33Evidence\$r33c\pending-visual-review.json"
$r33ph = Sha $r33pp
$r33pj = Json $r33pp
$r33pj.Status
$r33pj.R34AdmissionAuthorized
$r33pj.Captures | Select-Object Presentation, Pose, Image
```

The expected status is `PENDING_VISUAL_REVIEW`, and R34 admission must be
false. Inspect all eight PNGs in receipt order: the four fixed views in
`GooglePrimary`, followed by the same four views in `CwtPresented`. Compare
terrain continuity and surrounding context, and look specifically for holes,
overlapping surfaces, z-fighting, or both streamed providers appearing at
once. Mechanical decode, dimensions, hash distinctness, and state checks do
not constitute visual acceptance.

Only after that human review, pin the pending receipt and publish the separate
acceptance receipt:

```powershell
& .\scripts\Capture-IstanaExploreV5DR33Player0Evidence.ps1 `
  -AcceptVisualReview -ConfirmEightImagesReviewed -RunToken $r33c `
  -PendingCaptureReceipt $r33pp `
  -ExpectedPendingCaptureReceiptSha256 $r33ph

$r33cp = "D:\triad\TRIAD_R33Evidence\$r33c\commit.json"
$r33cj = Json $r33cp
$r33cj.Status
$r33cj.VisualReviewAccepted
$r33cj.R34AdmissionAuthorized
```

`-ConfirmEightImagesReviewed` is also a human attestation and cannot be inferred
from mechanical validation.

Acceptance does not launch Unreal or alter the native project. It keeps
`R34AdmissionAuthorized=false`, and it makes no claim of provider readiness,
provider entitlement, datum resolution, checkpoint validation, survey or
real-world terrain accuracy, performance, or hyperrealism. SafeLocal is not
captured because no deterministic real failover is available without
manufacturing a network or entitlement failure.

## Open the result

Open `D:\triad\TRIAD\TRIAD.uproject` with Unreal Engine 5.5, then open
`/Game/Maps/Istana_PublicView_Explore_v5d_hybrid`. Never omit or substitute that
map. The rejected `Istana_1km_Context_v2` prototype is not a valid fallback. Do
not open or resave the V5D map between transaction stages. If any wrapper fails,
retain its rollback receipt and do not manually copy staged files into the
native project.
