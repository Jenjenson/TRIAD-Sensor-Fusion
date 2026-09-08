# Sensor-placement evidence intake

Status: **validation boundary implemented; no measured calibration, Stage-1
approval, solver run, or deployment is claimed**.

The strict intake command validates the structure and provenance of a
file-backed Stage-0 evidence package. It does not yet validate the full frozen
outcome semantics and therefore does not make a package ready for Stage-1
outcome review. It does not calibrate a sensor, generate missing evidence, run
an optimizer, or authorize installation. The checked-in Stage-0 example remains
`DRAFT`, so it is expected to fail this intake until the named owners freeze its
inputs and supply the required evidence.

## Run the validator

From `core/`, after installing the package in the project environment:

```powershell
singapore-placement-evidence-intake `
  evidence/intake_manifest.json `
  --approved-root C:\approved\placement-evidence
```

The equivalent module entry point is:

```powershell
python -m singapore_sensor_fusion.placement.validate_evidence_intake `
  evidence/intake_manifest.json `
  --approved-root C:\approved\placement-evidence
```

A valid package writes a JSON receipt to stdout. Invalid input writes
`INVALID: ...` to stderr and exits with status 2. Keep the approved root narrow:
every referenced artifact, including the Stage-0 contract and native RF
resources, must be an existing regular file below it.

## Manifest contract

The manifest schema is `triad.placement_evidence_intake_manifest.v1`. Its exact
top-level fields are:

```json
{
  "schemaVersion": "triad.placement_evidence_intake_manifest.v1",
  "intakeId": "operator-assigned-id",
  "executionClass": "STAGE0_EVIDENCE_INTAKE_ONLY",
  "stage0Contract": {"relativePath": "stage0.json", "bytes": 0, "sha256": "..."},
  "nativeRfReceipt": {"relativePath": "rf/receipt.json", "bytes": 0, "sha256": "..."},
  "calibrationBundle": {"relativePath": "calibration/bundle.json", "bytes": 0, "sha256": "..."},
  "calibrationDatasets": [],
  "candidateCatalog": {"relativePath": "placement/candidates.json", "bytes": 0, "sha256": "..."},
  "trajectoryCorpus": {"relativePath": "placement/trajectories.json", "bytes": 0, "sha256": "..."},
  "scenarioPartitions": [],
  "evaluationEvidence": [],
  "precompute": {"relativePath": "placement/precompute.json", "bytes": 0, "sha256": "..."},
  "stage1AuthorizationPresented": false
}
```

Replace every `bytes` value and lowercase SHA-256 placeholder with the exact
on-disk identity. Each calibration-dataset entry carries `sensorClass`,
`hardwareId`, `hardwareRevision`, `stage0ArtifactId`, and `file`; each evaluator
entry carries the first three fields plus `file`. Those arrays must cover every
candidate-referenced hardware ID exactly once, including multiple revisions of
the same sensor class. Scenario-partition entries must exactly match the train,
validation, and test partitions in the Stage-0 contract. Paths use normalized
relative POSIX syntax and may not traverse symlinks, reparse points, parent
components, hardlink role aliases, or the approved-root boundary.
Physical-file identities are checked across the manifest and Stage-0 sets as a
single trust boundary. The only permitted cross-set aliases are the deliberate
exact bindings for the native receipt, candidates, trajectories, scenario
partitions, and per-hardware calibration datasets.

## What is cross-checked

The validator recomputes each file's byte count and SHA-256 from a stable read.
Large calibration datasets and other non-JSON resources are hashed as streams
and are not retained in memory. Bounded JSON is parsed from the exact bytes that
were hashed. The validator then enforces all of these relationships:

- the Stage-0 contract is `FROZEN`, all artifacts are `VERIFIED`, all gates are
  `PASSED`, the benchmark is frozen, at least one mount is eligible, and all
  required approvals are complete;
- every Stage-0 artifact URI passes the approved-root and reparse-point audit
  before the Stage-0 parser can read it, and each preverified digest is supplied
  to that parser without a second hash/parse read;
- the native RF v3 receipt records the exact one-kilometre automation filter,
  one success, no failures, idle editor state, rollback protection armed, and current
  hashes for its three workspace resources; native uppercase hexadecimal hashes
  are compared case-insensitively and normalized, while canonical package hashes
  stay lowercase;
- the exact native RF receipt bytes are a verified
  `RF_NATIVE_TEST_RECEIPT` artifact in the frozen Stage-0 authority, so a
  self-asserted replacement `PASS` receipt is rejected;
- every candidate-used hardware ID and revision binds its own real calibration
  dataset, calibration ID, and evaluator receipt without assuming one hardware
  record per sensor class;
- candidate IDs, hardware identities, revisions, poses, mounts, and failure
  domains bind to eligible Stage-0 mounts;
- trajectory IDs, scenario IDs, partitions, seeds, classes, critical flags,
  time samples, horizon, and coordinate frame bind to the frozen corpora;
- per-hardware evaluator receipts bind the same contract, native RF receipt,
  dataset, calibration, candidates, trajectories, scenario partitions, exact
  result-row count, and canonical result-row digest; and
- precompute rows form the exact sorted candidate-by-trajectory-by-sensor grid,
  retain their provenance and truth fields, use sample-aligned first-detection
  times, and respect passive-RF silence semantics.

Self-rehashing a modified JSON file cannot satisfy these cross-file bindings:
changing detection fraction, quality, or first-detection time conflicts with the
evaluator-bound result digest. The lower-level truth gate rejects empty bindings
and stripped candidate/trajectory authority fields. Existing synthetic workflows
remain available only under the explicit `DEMO_STUDY_ONLY` /
`DEMO_ONLY_SYNTHETIC_PRIORS` labels.

## Meaning of a successful receipt

Success means only `STRUCTURAL_EVIDENCE_PACKAGE_INTAKE_ONLY`. The receipt keeps
`stage0OutcomeEvidenceComplete`, `stage1OutcomeReviewReady`,
`staticSolverInputAuthorized`, `solverExecutionAuthorized`,
`productionOptimizationAuthorized`, and `physicalDeploymentAuthorized` false.
It explicitly lists the unvalidated robust outcomes: fused valid tracks, p95 and
CVaR latency, continuity and breaks, localization error/covariance, false
detections/tracks, outage resilience, resources/cue load, critical-zone metrics,
and the frozen requirement thresholds. The static-problem loader refuses this
strict structural execution class until a separate robust-outcome validator
covers those semantics. Native RF evidence remains load/query mechanics rather
than field calibration.
