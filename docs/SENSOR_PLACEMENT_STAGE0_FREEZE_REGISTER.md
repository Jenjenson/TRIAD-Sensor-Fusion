# Sensor-placement Stage-0 freeze register

Status: **planning-only, static placement NO-GO; optimizer execution and
production implementation are not authorised**.

This register resolves precedence and records the decisions that must be frozen
before any placement score can influence an Istana study. It covers active
radar, passive RF receivers, RGB cameras, and neuromorphic/event cameras only.
Thermal and deployed depth cameras are outside scope.

## 1. Normative precedence

For the present study, the authority order is:

1. the stakeholder-approved successor to
   `core/examples/istana_stage0_sensor_placement_study_contract.v1.json`;
2. `docs/SENSOR_PLACEMENT_STAGE0_FREEZE_REGISTER.md` and
   `docs/SENSOR_PLACEMENT_OPTIMIZATION_PLAN.md`;
3. versioned, hash-bound simulator evidence admitted by that contract;
4. all legacy placement requests, surveys, reports, Unreal patches, greedy
   selectors, and RL runners as non-authoritative research fixtures only.

The existing `istana_1km_placement_request.json`, 95.2% recommendation, Unreal
sensor-node patch, `placement/optimizer.py`, and `placement/rl.py` do not satisfy
Stage-0. They must not be used to select a physical layout, claim coverage, or
authorise implementation. Their commands are retained only for regression and
method research after a separate experiment approval.

## 2. Frozen snapshot — 2026-09-03

The current Stage-0 example is a valid schema example, not an approved study:

- approval state: `DRAFT`;
- evidence artifacts: 29 unresolved and 29 `UNVERIFIED`;
- readiness gates: 10 `BLOCKED`, 0 passed;
- eligible mount regions: 0;
- required approvals: 5 pending;
- production optimizer implementation gate: `BLOCKED`;
- physical deployment gate: `BLOCKED`;
- static RL decision: `NO_GO_STATIC_PLACEMENT`;
- RL experiment and production use: both false.

TightV1 RF geometry cannot clear the environment gate: it is an
assumption-bound main-building fixture, not the one-kilometre AOI. Current V5D
terrain, trees, grass, and surrounding buildings are render-only. No placement
score is scientifically usable until the relevant visual/collision/RF domains
and all four sensor evaluators have accepted evidence.

## 3. Decision register

| Decision | Required owner | Evidence needed | Current placeholder | Blocking gate |
| --- | --- | --- | --- | --- |
| AOI, critical zones, routes, altitude/time envelope, and prohibited regions | Study owner + site authority | Approved geometry/catalog with epoch and frame hashes | Illustrative one-kilometre boundary | Environment |
| Render, collision, and RF domain split | Simulation lead + model-validation lead | Cold-loaded map receipt; separate geometry hashes and validity envelopes | V5D render-only context and TightV1 hero-only RF | Environment / RF propagation |
| Actual sensor SKUs and revisions | Study owner | Datasheets plus pinned configuration records | `TBD_BY_STAKEHOLDER` / `UNSELECTED` | All sensor-model gates |
| Radar bands, waveform, scan, antenna, RCS/clutter/Pd/Pfa model | RF safety reviewer + radar-model owner | Band-valid antenna and controlled calibration receipts | Analytic search-radar fixture | Radar model |
| Passive-RF bands and localization observables | RF safety reviewer + passive-RF owner | Pattern/polarization, clocks, AOA/TDOA errors, minimum receiver count, GDOP/Fisher threshold | Energy-detection-only fixture | Passive RF model |
| RGB lens, pose lattice, exposure/cadence, detector and privacy mask | Vision owner + privacy reviewer | Calibrated renderer/lens/detector chain and approved masks | Generic RGB/Visibility rows | RGB model / Privacy |
| Event-camera temporal/noise parameters and detector | Vision owner | Physical temporal model, timestamps, contrast/refractory/noise and detector calibration | RGB frame-difference proxy | Event-camera model |
| Target classes, trajectories, emission/RCS/aspect distributions | Study owner + validation lead | Approved, partitioned scenario corpus | Illustrative records | Evaluation corpus |
| Detection, association, localization and persistent-track definitions | Fusion owner + validation lead | Versioned policy, thresholds, covariance and false-track tests | Unapproved fusion thresholds | Fusion policy |
| Mount slots, height/orientation sets, co-location, failure domains and maintenance | Site authority | Surveyed slots, structural/permission/resource records | One ineligible illustrative region | Mount permissions/resources |
| Lifecycle costs, power, backhaul, compute, storage and operator load | Study owner + operations | Auditable units and capacity model | Relative prototype costs | Mount permissions/resources |
| Scenario weights versus unweighted stress cases | Study owner + validation lead | Signed estimand/weight decision | Unweighted illustrative stresses | Evaluation corpus |
| Stage-1 implementation authority | All five approver roles | Separate approval artifact referencing the frozen Stage-0 hash | None | Production implementation |

Any material change to one of these decisions requires a new study revision,
new hashes, and renewed approval. A planner may not fill missing evidence with
defaults merely to make a gate pass.

## 4. Band and propagation-phenomenon matrix

Every selected radar or passive-RF band needs its own row before evaluation.
`REQUIRED` means the effect must be modeled or bounded; `CONDITIONAL` means the
owner must justify inclusion or exclusion for the band/site; `NOT YET VALID`
means current code is not evidence.

| Phenomenon | Active radar | Passive RF | RGB | Event camera | Present evidence |
| --- | --- | --- | --- | --- | --- |
| Direct path / geometric occlusion | REQUIRED | REQUIRED | REQUIRED | REQUIRED | TightV1 is hero-only; visual context has no occlusion authority |
| Wall/glazing transmission | Band- and waveform-conditional | REQUIRED where links cross envelopes | N/A | N/A | TightV1 uses uncalibrated 0.4–6.0 GHz priors |
| Specular reflection / multipath | REQUIRED if material to Pd/clutter | REQUIRED if material to SNR/localization | Reflection only through calibrated renderer | Reflection only through calibrated renderer | Evaluator exists; path enumeration is absent |
| Diffraction | CONDITIONAL | CONDITIONAL, often required for urban shadowing | N/A | N/A | Not implemented or validated |
| Foliage/grass attenuation and scattering | CONDITIONAL by band and path | CONDITIONAL by band and path | REQUIRED visually | REQUIRED visually/temporally | Render vegetation has no RF/sensor authority |
| Rain/gas/cloud/fog attenuation | Band-valid procedure required | Band-valid procedure required | Optical weather calibration required | Optical weather calibration required | Current scalar/proxy effects are not calibration evidence |
| Polarization and antenna pattern | REQUIRED | REQUIRED | N/A | N/A | No accepted site/SKU pattern evidence |
| Clutter, false alarms, false events | REQUIRED | REQUIRED | REQUIRED | REQUIRED | No accepted calibrated four-modality chain |

Do not extrapolate the TightV1 material profiles to the illustrative X-band
radar or to receiver bands outside their declared 0.4–6.0 GHz applicability.

## 5. Metric and estimand freeze

The approved Stage-0 successor must define these without an opaque aggregate:

- `P_valid_detection_by_T`: fraction of complete held-out trajectories with a
  fusion-policy-valid detection by deadline `T`, with a declared confidence
  interval over trajectory and epistemic ensembles;
- `time_to_localized_track`: elapsed time to the first persistent track meeting
  declared position/velocity covariance and continuity criteria; missed tracks
  are right-censored at the scenario horizon and counted as failures, not
  silently dropped;
- tail latency: empirical 95th percentile plus CVaR at an explicitly approved
  alpha over the declared scenario distribution; unweighted stress cases are
  reported separately and are not assigned invented occurrence probabilities;
- localization: position/velocity RMSE, coverage of declared covariance bounds,
  NIS/NEES where applicable, and passive-RF GDOP/Fisher-information thresholds;
- false behavior: per-sensor false detections per sensor-hour and false fused
  tracks per scenario-hour under a frozen association policy;
- continuity/resilience: track-break rate, largest critical-route gap, and
  replay after every single-node and declared common-cause-domain outage;
- resources: lifecycle cost, power, backhaul, compute, storage/event rate,
  maintenance access, and operator cue contention in auditable units.

Hard feasibility, permissions, privacy, emission safety, domain validity, and
resource limits precede any objective. Stakeholder weights may rank only the
feasible Pareto set, and all unweighted components remain in the receipt.

## 6. Solver decision record

The problem is a static robust facility-location/set-cover design with mixed
discrete choices and a small continuous refinement layer. The approved future
hierarchy is:

1. robust MILP, CP-SAT, or bounded branch-and-bound as the primary discrete
   selector, reporting incumbent, bound, optimality gap, and timeout status;
2. deterministic greedy/local search for screening, warm starts, and an
   equal-input reproducible baseline;
3. bounded Bayesian or derivative-free optimization only for local continuous
   pose refinement or simulator calibration;
4. evolutionary multi-objective search only as an equal-budget challenger;
5. RL only after a new decision record proves a genuine sequential use case,
   such as online PTZ tasking, mobile repositioning, or staged deployment under
   new observations.

The current tabular Q-learning environment orders additions to a static subset
and evaluates cached coverage rows. That artificial sequence provides neither
an optimality certificate nor an evidentiary advantage over the primary
combinatorial solver. Its five self-attested readiness booleans do not supersede
Stage-0 evidence gates.

## 7. Planning-only implementation sequence

1. Replace every placeholder with stakeholder-owned evidence while keeping
   unresolved gates blocked.
2. Freeze a hash-bound authority bundle: map, render/collision/RF geometry,
   materials, frame transform, epoch, validity domains, and cold-load receipt.
3. Freeze a permission-valid candidate catalog with discrete hardware, mount,
   height, orientation, lens/antenna, co-location, resources and failure domains.
4. Freeze disjoint design, validation, and sealed test trajectory/scenario
   corpora with common random seeds and epistemic ensembles.
5. Define one observation-level evaluator per allowed modality; replay through
   the actual association/fusion/track policy and cache a hash-keyed metric tensor.
6. Issue a separate Stage-1 authorization artifact before implementing or
   executing any production optimizer.
7. Solve hard feasibility first, generate the Pareto frontier, and replay every
   shortlist in the exact simulator on held-out, outage, and boundary cases.

This document does not grant Stage-1 authority and must not be used as a sensor
deployment recommendation.
