# Radar, RF, RGB, and neuromorphic sensor-placement plan

Status: **planning only; do not implement the recommendation algorithm until the
simulation-readiness gates below pass**.

This plan covers fixed-site placement of four sensor classes only:

- active radar, including the existing search-radar and mmWave model families;
- passive RF receivers;
- visible-light RGB cameras, fixed or steerable;
- neuromorphic/event cameras.

Thermal imaging is outside this scope. Simulator depth may be used as evaluator
truth or a diagnostic, but it is not a deployed sensor modality unless the scope
is later changed. The output is a reviewable planning recommendation, never an
automatic physical deployment or an authorization to use a site.

## 1. Repository baseline and what it proves

The repository has useful interfaces, but not yet a defensible end-to-end
placement evaluator.

| Area | Current repository evidence | Consequence for placement work |
| --- | --- | --- |
| Placement contracts | `core/src/singapore_sensor_fusion/placement/contracts.py` defines versioned requests, candidates, samples, per-modality evaluations, constraints, recommendations, digests, and review-only Unreal patches. | Reuse the provenance and fail-closed contract pattern. Extend the metrics rather than treating the current binary coverage row as final. |
| Site selection | `placement/optimizer.py` uses complete enumeration for at most eight options and a deterministic greedy/pruning heuristic otherwise. | Retain it as a reproducible baseline. A greedy result is not an optimality certificate. |
| Unreal survey | `GenerateIstanaPlacementSurvey` requires exactly one package; creates one centre and four fixed candidate rings, one inward camera orientation per site, 33 target points per altitude band, and only `SEARCH_RADAR` and `rgb` evaluation rows. It does not use the request's horizontal spacing or adaptive-refinement values. | It does not evaluate passive RF or event cameras, alternative packages/orientations/heights, continuous mount regions, or trajectory behavior. Surface resolution alone also does not prove permission, structural suitability, power, backhaul, or maintainability. |
| Survey quality | Eligible radar/RGB rows use generic `ECC_Visibility`, a linear range margin, and one weather multiplier. The request's RF-emission state and radar-cross-section value do not affect these rows. | Identical clear, monsoon, haze, and RF-silent coverage is not evidence of sensor performance. Replace this with sensor-specific simulation outputs. |
| Existing recommendation | `core/reports/istana_1km_recommendation.json` reports four full-stack sites and 95.238% worst-scenario coverage. | This is a regression fixture for the prototype workflow, not a final recommendation. It also contains a thermal modality that is outside the present scope. |
| Cost semantics | The current exporter copies package cost into each option's `siteCostUnits`; the Python selector then adds site and package costs, producing two units from the example's one-unit package plus one-unit site value. | Decide and test whether equipment and site costs are intentionally separate before using cost to rank layouts. The current relative units are not procurement data. |
| Passive RF runtime | `TRIADSensorFusionScenarioManager.cpp` retains legacy free-space/Visibility/scalar-NLOS propagation by default, but now also has an opt-in, hash-bound indexed-geometry path. The live scenario admits only direct and paired straight-transmission candidates; reflection enumeration is not wired into the scenario path. | Do not use the legacy mode for propagation-aware placement. Dedicated-mode readiness remains bounded by its exact mesh, coverage envelope, frame, material catalog, and validation evidence. |
| RF geometry/query seam | `TRIADRFIndexedGeometryQuery` and `TRIADRFInteractionModel` are implemented, native-tested, and wired into the scenario manager. TightV1 is internally watertight and fail-closed, but its materials and apertures are assumption-bound and its closed coverage envelope is only the main hero. | Preserve the dedicated seam, but do not optimize against TightV1 as if it represented the deployment area, surrounding blockers, calibrated materials, or field truth. |
| Radar | Unreal search radar has a deterministic analytic range/RCS/LOS/weather score and seeded measurement noise. Python also has a 77 GHz FMCW simulation, but live mmWave observations are not wired. | Both are useful test models, not calibrated probability-of-detection or clutter models. Placement needs aspect-dependent RCS, antenna patterns, clutter/false alarms, and measured error curves. |
| RGB | Unreal exports RGB/depth frames; the Python OAK adapter runs hash-pinned RGB YOLO and uses simulated depth only for post-detection ranging. | Measure pixels-on-target, occlusion, illumination, motion blur, detector operating curves, cadence, and latency. Do not count simulator depth as an allowed deployed modality. |
| Neuromorphic | `event_camera.py` converts pairs of RGB images into a deterministic proxy event stack; `event_yolo.py` labels proxy-derived outputs explicitly. | The proxy is useful for software plumbing only. A physical event-camera model needs contrast thresholds, refractory period, timestamp/noise behavior, lens response, and validated detector curves. |
| Fusion | `fusion_v3.py` groups passive RF, active radar, and visual evidence into three independent families. RGB and event cameras are both visual and cannot corroborate each other as independent families. | Placement objectives must evaluate the actual fusion policy and correlation groups, not add per-sensor scores. |
| Learning code | A tabular Q-learning research runner now exists in `placement/rl.py`, but it turns static subset construction into an artificial episode and gates itself with five self-attested booleans rather than the Stage-0 evidence contract. No production Bayesian, evolutionary, or exact robust optimizer is authorised. | Quarantine the runner as a legacy method fixture. Its existence is not a reason to prefer RL and does not clear any Stage-0 gate. |

The RF geometry-authority gate remains fail closed as of 2026-09-05. The V5D
public surroundings, terrain context, trees, and grass are render-only and
carry zero collision, sensor, or RF authority. TightV1 covers only the main
hero in an approximately 115 m by 79 m closed envelope; the earlier dedicated
V5D cold run loaded its exact hashes but rejected all 96 sampled links as
outside that modeled domain.

The generic indexed-polyhedron foundation is now implemented and native-tested.
It keeps TightV1 immutable, validates arbitrary closed solids and
overlap/containment failures, and carries solid, profile, uncertainty, and
coefficient provenance. The assumption-bound `OneKilometreV2` resource now
provides an exact georeferenced 1 km circular contract with 1,094 solids,
13,936 surfaces, 23,496 vertices, 42,700 triangles, 721 index cells, and zero
declared positive-volume overlaps. Its pinned geometry, material-catalog, and
scene-contract SHA-256 values are respectively
`85E654FBA602B2DBC51EB64C6B66FF234C8EA1F948152612C755EDC22C65DA51`,
`210CB26DDB9B531ADEB3C917606A736AEFC857EB6696DA485A2E63DBB8B31662`,
and `FE509917AE59BE0918BCD799F23DC981E00A394C6C328A7342F56B371C40BCC2`.

This is a propagation-ready geometry kernel, not a field-ready propagation
solution. The full OneKilometreV2 file now passes its dedicated, hash-pinned
native actual-file load/query test; the immutable transaction receipt is
`D:\triad\TRIAD\Saved\TRIAD\RFActualFileNativeTransactions\rf_actual_fix_20260905T184017Z\receipt.json`.
That result validates bounded geometry loading and querying only. Materials
remain uncalibrated, and diffraction, coherent phase, polarization,
foliage/weather volume effects, reflection enumeration in the live scenario,
and packaged end-to-end execution remain unproven. The next RF gates are
calibrated band-valid material and sensor evidence, followed by packaged
in-domain direct, transmission, opaque-block, and fail-closed outside-domain
cases. Any score based on generic binary LOS, the seven-box collision proxy,
render-only surroundings, or an uncalibrated OneKilometreV2 model remains
preliminary and unavailable for production optimization.

The source-only robust branch-and-bound fixture now emits a versioned v2 result
with the feasible incumbent cost, a conservative cost lower bound, absolute and
relative cost gaps, an explicit node-limit flag, and a termination reason. A
completed search reports a zero cost gap; a node-limited search reports its
remaining structural bound without claiming optimality or infeasibility. This
closes the solver-reporting mechanics required for deterministic comparison,
but it does **not** authorize the strict evidence intake for optimization or
replace the still-missing robust fused-outcome validator.

## 2. Decision problem

The first problem is a **static, robust, mixed discrete/continuous design**:
choose a permitted site, height, sensor class and model, orientation, and
configuration for each installed unit. A later problem may schedule steerable
cameras or relocate mobile sensors, but that is a different sequential-control
problem.

For a candidate mount region `i`, sensor configuration `k`, orientation `o`,
and height choice `h`, define a binary decision `x[i,k,o,h]`. Keep continuous
position/orientation refinement inside survey-approved mount regions as an
optional second phase. At most one incompatible configuration may occupy a
physical mount, while compatible co-location must consume the correct space,
power, network, and failure-domain capacity.

Evaluate layouts on complete target trajectories and time windows, not only
isolated grid points. Spatial cells remain useful for candidate pruning and
coverage visualization, but the final evaluator must exercise detection
latency, revisit/cue contention, track continuity, and localization geometry.

### Required scenario dimensions

Each deterministic scenario record should bind at least:

- trajectory, altitude, speed, acceleration, target size/aspect, and radar
  cross-section distribution;
- RF-emitting and RF-silent cases, supported bands, transmitter power/duty
  cycle uncertainty, and emitter orientation;
- clear, rain/monsoon, haze, wind-driven vegetation, and day/night/backlit
  visual conditions;
- building aperture state and RF-material parameter realization within the
  accepted epoch and evidence scope;
- sensor noise, calibration drift, clock error, dropped frames/events, network
  latency, compute contention, and node/failure-domain outage;
- clutter and non-target traffic sufficient to estimate false detections and
  false tracks.

Use a versioned training scenario set and disjoint validation/test sets. Bind
the map, RF mesh, material library, sensor-model, detector, fusion-policy, and
scenario-set hashes in every evaluation.

## 3. Sensor-specific evaluator contracts

The common evaluator must return observations and uncertainty in the same
coordinate/time frame, while preserving modality-specific physics.

| Sensor class | Placement variables | Minimum modeled outputs | Critical readiness gap |
| --- | --- | --- | --- |
| Radar | position, height, boresight/scan sector, elevation coverage, waveform/model, update rate | detection and false-alarm events, range/azimuth/elevation/radial-velocity measurements, covariance, latency, clutter state, resource use | Calibrate probability of detection and false alarm versus range, aspect/RCS, weather, clutter, and occlusion; validate multipath effects separately from RF-receiver propagation. |
| Passive RF | position, height, antenna type/pattern/polarization/orientation, bands, bandwidth, dwell schedule, clock class | received power/SNR, detected bands, timestamp/AOA/TDOA observables when supported, covariance, duty-cycle miss probability, latency | Dedicated RF mesh and solver must preserve apertures and evaluate validated reflection/transmission/diffraction scope. Current energy detection alone does not localize a target. |
| RGB camera | position, height, yaw/pitch/roll, lens/FOV, resolution, cadence, exposure policy, fixed versus steerable | occlusion, pixels on target, detections/false detections, boxes or bearings, calibrated confidence, latency, cue/revisit state | Calibrate the renderer/camera/detector chain across distance, light, weather, target aspect, motion blur, and clutter. |
| Event camera | position, height, orientation, lens/FOV, contrast thresholds, refractory/noise settings, event bandwidth | event stream or validated event representation, detections/false detections, angular observations, latency, event/data rate | Replace the RGB-frame-difference proxy with a validated temporal sensor model and detector preprocessing contract before making physical claims. |

RGB and event-camera evidence may improve visual-family robustness, cadence, and
weather/lighting complementarity, but they still count as one fusion family.
Likewise, multiple radars are one active-radar family and multiple RF receivers
are one passive-RF family. Spatially independent nodes remain important for
resilience and RF localization even when they do not create a new family.

## 4. Objectives and constraints

Do not begin with one opaque weighted score. Enforce safety, feasibility, and
minimum performance as hard constraints, then present a Pareto frontier for
cost, latency, localization, and resilience.

### Primary performance measures

For each layout and held-out scenario distribution, compute:

1. probability or fraction of trajectories producing a policy-valid fused
   detection within a declared deadline;
2. time to first detection and time to a localized, persistent track, including
   median, 95th percentile, worst declared scenario, and CVaR tail;
3. track continuity, break rate, position/velocity RMSE, and covariance
   calibration such as NIS/NEES where ground truth is used only by the evaluator;
4. critical-zone and altitude-band coverage, plus the largest contiguous gap;
5. false detections per sensor-hour and false fused tracks per scenario-hour;
6. degradation after any single node and any single declared failure-domain
   outage;
7. installation/lifecycle cost, power, backhaul, compute, storage/event rate,
   and operator cue load.

Until per-sensor scores are calibrated, call them **simulation evidence
indexes**, not probabilities. A useful constraint-first formulation is:

```text
minimize       lifecycle cost
subject to     worst-scenario fused detection coverage >= target
               CVaR(time to localized track) <= deadline
               critical trajectories meet coverage and latency targets
               false-track rate <= threshold
               localization error/covariance <= threshold
               single-node and failure-domain resilience >= target
               every physical/site/resource constraint is satisfied
```

After this feasible frontier exists, rank its points with stakeholder-approved
weights. Keep the unweighted component metrics in the recommendation artifact.

### Hard constraints

- approved mount surface/region, ownership and access permission;
- collision-free mounting and unobstructed maintenance envelope;
- height, mass, structural load, orientation, radar exclusion, and legal RF
  emission limits where applicable;
- power, backhaul, compute, storage, thermal, and event-bandwidth capacity;
- maximum site and per-class counts plus lifecycle budget;
- one choice per incompatible mount and explicit co-location capacity;
- required distinct sites and independent power/backhaul/failure domains;
- minimum angular baseline or geometric dilution for localization;
- camera privacy masks and prohibited views;
- accepted RF geometry/material frequency band and epoch;
- no use of restricted or unlicensed site data.

## 5. Recommended solver architecture

### Comparison

| Method | Best fit | Strengths | Main limitations here | Role |
| --- | --- | --- | --- | --- |
| Exact MILP/CP-SAT or branch-and-bound | Discrete candidate/site/package/orientation selection with precomputed scenario metrics | Deterministic, auditable, handles budgets/redundancy, can provide optimality gap or infeasibility proof | Requires a tractable linearized/constraint model; expensive interactions may need cuts or decomposition | **Primary static-placement baseline and preferred production selector** |
| Greedy/submodular and local search | Large candidate screening and warm starts | Fast, deterministic, easy to explain; approximation bounds are possible for truly submodular objectives | Fusion thresholds, failure domains, cue contention, and localization geometry can break submodularity; no general optimality proof | Retain as baseline, candidate reducer, and solver warm start |
| Bayesian optimization | A small number of expensive continuous design variables or simulator calibration parameters | Sample-efficient, models uncertainty, useful when each simulation campaign is costly | Scales poorly with many discrete sites and conditional configurations; surrogate bias can hide narrow failures | Use for continuous refinement or simulator/model calibration, not the main combinatorial selector |
| Evolutionary/multi-objective search, such as NSGA-II or CMA-ES variants | Non-smooth mixed variables and Pareto-front exploration | Flexible, parallel, naturally produces trade-off sets | Simulation hungry, stochastic, difficult to certify, sensitive to representation and penalties | Use as a challenger and optional refinement after a deterministic baseline |
| Reinforcement learning | Sequential deployment, mobile repositioning, or online sensor/camera scheduling under evolving observations | Can learn long-horizon contingent policies and amortize decisions across many related episodes | Static placement is not naturally sequential; high sample cost, reward hacking risk, weak optimality evidence, difficult generalization and audit | **Do not make RL the first static-placement solver.** Consider it only after the simulator and robust baseline are validated |

### Proposed hierarchy

1. Generate only physically feasible mount regions and sensor configurations in
   Unreal; cache deterministic per-sensor/per-trajectory results using common
   random numbers.
2. Remove dominated candidates, but retain failure-domain and rare-scenario
   diversity.
3. Solve the discrete robust problem with MILP/CP-SAT or equivalent exact
   branch-and-bound. Report the incumbent, bound, gap, and timeout status.
4. Refine continuous position/orientation within approved regions using a
   bounded derivative-free or Bayesian method, rechecking the exact simulator.
5. Run a multi-objective evolutionary challenger from the same budget and
   compare its nondominated layouts against the deterministic frontier.
6. Approve RL experimentation only if the desired output is a sequential
   policy, or if it demonstrably beats the robust baseline on disjoint maps and
   scenarios at equal site/resource budgets.

## 6. RL formulation if a later sequential use case justifies it

For a sequential construction policy, one episode builds one layout.

**State**

- immutable map/RF-mesh/sensor-model identifiers and accepted uncertainty
  bounds;
- candidate graph with mount feasibility, site/failure-domain/resource data,
  and action mask;
- currently selected sensors, poses, remaining budget and capacity;
- residual per-scenario detection, latency, localization-information, and
  robustness maps;
- summary statistics of simulation uncertainty, never privileged live target
  truth that would be unavailable at planning time.

**Action**

- select one `(site region, sensor class/model, height, orientation,
  configuration)` tuple, optionally followed by a bounded local pose delta;
- remove or replace a prior choice only if the episode definition permits it;
- `STOP` when no valid addition is useful.

Invalid actions must be masked, not merely penalized. The transition invokes a
versioned cached evaluator or a validated surrogate and updates residual
metrics. The final layout must be replayed in the exact simulator.

**Reward**

Use the change in a constraint-aware potential, not a raw sum of sensor scores:

```text
reward_t = robust_utility(layout_t) - robust_utility(layout_t-1)
           - incremental lifecycle/resource cost
```

`robust_utility` should prioritize critical-trajectory feasibility, worst-case
and CVaR fused coverage/latency, localization information, false-track control,
and outage resilience. Apply terminal failure for violated hard constraints;
do not let a large average-coverage reward compensate for an uncovered critical
scenario. Train across map/material/weather/sensor-model randomization and
evaluate on withheld maps, routes, seeds, and parameter ranges.

For online camera scheduling rather than placement, the state should contain
only current measurement-derived tracks, uncertainty, cue queues, and sensor
availability; actions choose sensor tasking; reward uses track quality, latency,
coverage fairness, and slew/resource costs. Keep that policy separate from the
static placement recommendation.

## 7. Uncertainty treatment

Separate two kinds of uncertainty:

- **Aleatoric:** target route/timing/aspect, sensor noise, RF duty cycle,
  clutter, weather, illumination, temporary occlusion, packet/frame loss, and
  node failure. Represent these with scenario distributions and repeated seeds.
- **Epistemic:** uncertain geometry, material coefficients, detector calibration,
  RCS, event-camera parameters, and source age. Represent these with bounded
  parameter sets or ensembles and reduce them only with new evidence.

Use common random numbers when comparing layouts, stratified or importance
sampling for rare critical events, and confidence intervals or bootstrap bands
for every reported metric. Optimize worst declared scenario and a tail-risk
measure such as CVaR, while also publishing expected performance. A layout whose
ranking changes materially across plausible model ensembles is not ready for a
single-site recommendation; return the stable Pareto set or request more data.

Surrogates may accelerate search, but their uncertainty must be included in
selection and every final candidate must be reevaluated in the high-fidelity
simulator. No model may infer success for a scenario outside its validated
frequency, range, weather, lighting, or geometry scope.

## 8. Validation strategy

### Simulation and sensor gates

1. **Environment gate:** cold-load the exact target map; bind map, render,
   collision, RF-mesh, material, georeference-transform, and epoch hashes.
   Verify apertures, watertight solids, surface IDs, transforms, and all
   required surroundings. V5D render-only public surroundings have zero RF
   authority and cannot satisfy this gate. TightV1 also cannot satisfy a
   corridor/AOI gate because its approximately 115 m by 79 m hero-only domain
   produced 96/96 outside-domain link failures in the dedicated cold run.
2. **RF gate:** validate direct, transmission, reflection, and required
   diffraction/multipath behavior against controlled or field measurements for
   every claimed band. Generic visibility collision cannot pass this gate.
3. **Radar gate:** compare detection, false alarms, bias, covariance, latency,
   clutter, RCS/aspect, and weather behavior with accepted data.
4. **RGB gate:** validate camera intrinsics/extrinsics, renderer response,
   pixels-on-target, occlusion, detector calibration, false positives, cadence,
   and latency across held-out conditions.
5. **Neuromorphic gate:** validate event generation, noise/timestamps,
   preprocessing, detector calibration, false positives, latency, and event
   rate against physical-sensor or accepted reference data.
6. **Fusion gate:** demonstrate that correlation groups and family rules prevent
   RGB/event, multiple radars, or multiple RF rows from manufacturing independent
   corroboration. Test RF-silent, visual-degraded, radar-degraded, and outage
   scenarios.

### Optimizer gates

- Compare every heuristic/learned solver with exact enumeration on small cases.
- For the production discrete solver, report proof of optimality or the final
  optimality gap; distinguish timeout from infeasibility.
- Use identical simulator calls, seeds, wall-clock/simulation budgets, and
  candidate sets for solver comparisons.
- Hold out complete routes, weather/light combinations, geometry/material
  perturbations, detector versions, and node-failure patterns.
- Report Pareto stability, selection frequency across bootstrap samples, and
  sensitivity to each constraint/weight.
- Test degenerate cases: no feasible sites, zero emitters, RF-silent targets,
  total visual occlusion, correlated failure domains, exhausted cue capacity,
  and conflicting privacy/site constraints.
- Replay every shortlisted layout in Unreal end to end through the actual
  observation and fusion interfaces; prohibit evaluator-truth seeding of
  operational tracks.
- Run a controlled shadow-mode or instrumented pilot before any physical claim.
  Simulation success alone is not deployment validation.

## 9. Staged milestones and stop/go criteria

### Stage-0 machine contract

The normative requirements-freeze artifact is
`triad.sensor_placement_stage0_study_contract.v1`, validated by
`core/src/singapore_sensor_fusion/placement/study_contract.py`. The DRAFT,
intentionally blocked example is
`core/examples/istana_stage0_sensor_placement_study_contract.v1.json`. Validate
an edited copy from `core` with:

```powershell
python -m singapore_sensor_fusion.placement.validate_study_contract `
  examples\istana_stage0_sensor_placement_study_contract.v1.json
```

The contract admits exactly radar, passive RF, RGB, and event cameras. It
rejects deployed thermal and depth modalities, while permitting simulator
depth only as evaluator truth or a diagnostic. Unknown or missing fields,
partition overlap, unbound critical-zone IDs, weakened RGB/event correlation,
unapproved mount eligibility, unequal solver budgets, wrong gate evidence, and
an unjustified RL scope change fail closed. `VERIFIED` artifacts must resolve
to regular local files (relative paths resolve beside the contract), and the
validator recomputes their SHA-256 digests; unresolved or non-local URIs can
only remain `UNVERIFIED`. A validation receipt proves declaration consistency
and the observed bytes of artifacts marked `VERIFIED`; it does not establish
the scientific adequacy or provenance of those bytes, pass blocked readiness
gates, authorize optimizer implementation, approve a site, or authorize a
physical deployment.

| Stage | Deliverable | Exit criterion |
| --- | --- | --- |
| 0. Freeze requirements | Approved `triad.sensor_placement_stage0_study_contract.v1` binding the four-class hardware inventory, target/trajectory envelope, fusion policy, thresholds, scenario partitions, mount constraints, artifact hashes, readiness gates, equal-budget benchmark, and RL decision | The validator passes, the contract is `FROZEN`, every required approver record is approved, thermal/deployed depth remain prohibited, and any still-blocked downstream readiness gate is explicit |
| 1. Finish the high-fidelity environment | Improve the bounded visual surroundings and promote the native-validated, assumption-bound `OneKilometreV2` geometry/material resource through its remaining calibrated-material and packaged-scenario gates | Visual and RF geometry gates pass in the packaged executable on a cold-loaded, hash-bound map; receipts prove in-domain direct, paired-transmission, opaque-block, and outside-domain cases; TightV1's 96/96 outside-domain failure is no longer representative of configured placement links |
| 2. Calibrate sensor models | Versioned radar, passive-RF, RGB, and event-camera models with provenance and uncertainty | Each sensor gate passes in its claimed operating envelope; scores may be called probabilities only if calibration supports that term |
| 3. Build the evaluation corpus | Approved mount regions, trajectory/scenario sets, failure domains, common seed schedule, and cached per-sensor outputs | Complete coverage of required scenario dimensions with disjoint validation/test partitions |
| 4. Establish deterministic baselines | Current greedy baseline plus exact MILP/CP-SAT robust selector and Pareto report | Small cases match exhaustive results; large cases include bounds/gaps; no live config is mutated |
| 5. Compare alternatives | Bayesian continuous refinement and multi-objective evolutionary challenger | Equal-budget comparison on held-out scenarios; final layouts replay successfully in the exact simulator |
| 6. RL go/no-go | Written justification tied to a genuinely sequential use case, then a benchmark protocol | RL is rejected for static placement unless it improves held-out robust performance over the deterministic baseline without violating auditability or constraints |
| 7. Validate recommendations | End-to-end Unreal/fusion replays, uncertainty/sensitivity report, ablations, outage tests, and independent review | All hard constraints and held-out thresholds pass with confidence bounds; recommendation includes hashes and known limitations |
| 8. Physical-validation handoff | Site survey, permissions, structural/power/backhaul review, RF compliance, privacy review, and controlled pilot plan | Separate authorized deployment decision; no automatic activation from the optimizer |

The immediate work order is therefore: replace every placeholder in the
Stage-0 contract and freeze its approved requirements, finish and validate the
environment and RF propagation layer, calibrate the four allowed sensor
models, then build the trajectory evaluator. Only after those gates should
implementation of a placement solver begin. For the present static problem,
robust mathematical optimization is the default; RL remains a later hypothesis
for sequential deployment or adaptive sensor tasking.
