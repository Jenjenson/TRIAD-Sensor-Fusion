# Weather-robust RL sensor placement

> **Quarantined legacy research fixture.** The current Istana problem is static
> placement and its Stage-0 decision is `NO_GO_STATIC_PLACEMENT`. Do not run this
> trainer for the present study, do not treat its five readiness booleans as
> Stage-0 evidence, and do not use its output to select a physical layout. A new
> approved sequential-use-case decision record is required before execution.

This module is an **experimental legacy planning tool**, not a deployment optimizer or
an optimality certificate. It chooses among position-and-orientation candidates
that the Unreal simulation has already evaluated. It never moves actors in a
saved Unreal map and its result must be replayed in the exact simulation.

## Formulation

One episode constructs one fixed layout.

- **State:** selected candidates, remaining site/budget capacity, action mask,
  and residual coverage/progress for every declared weather scenario.
- **Action:** add one complete `(site, package, height, orientation)` candidate,
  or `STOP`. Multiple orientations at one physical site are mutually exclusive.
- **Transition:** use cached simulator survey rows to recompute fused-family,
  site-redundancy, and failure-domain coverage.
- **Reward:** the change in a constraint-aware potential. Every feasible layout
  dominates every infeasible one; within the feasible set the reward favours
  lower cost, fewer sites, and stronger worst-weather coverage.
- **Algorithm:** deterministic-seed, action-masked tabular Q-learning. The
  returned recommendation is the best terminal layout observed during training.

Candidate options already contain `cameraRotation`, so choosing an option also
chooses yaw, pitch, and roll. For meaningful angle optimization, Unreal must
export multiple physically valid orientations at each directional-sensor mount.
A practical first lattice is 15-degree yaw steps with a small declared set of
pitch values; final refinement should use the exact simulator around the best
lattice points.

## Singapore weather treatment

Humidity, rain, and visibility are separate inputs:

- relative humidity and temperature determine surface water-vapour density;
- rain rate drives frequency- and polarization-dependent RF/radar attenuation;
- visibility drives optical contrast/extinction for RGB and event cameras;
- wet surfaces, lens droplets/condensation, wind, lighting, clutter, and false
  alarms require separate calibrated simulator effects.

The included `Clear`, `HumidMorningMist`, `Monsoon`, and `Haze` profiles are
deterministic stress inputs, not forecasts or occurrence probabilities.
Meteorological Service Singapore reports a 1991–2020 mean annual relative
humidity near 82%, values above 90% before sunrise, around 60% on dry
afternoons, 2,113.3 mm mean annual rainfall, and about 171 rain days. It also
notes that rain, mist, and haze can materially reduce visibility.

RF/radar simulation should use the applicable procedures rather than a generic
"humidity multiplier":

- ITU-R P.676-13 for atmospheric gases and water vapour;
- ITU-R P.838-3 for specific rain attenuation;
- ITU-R P.840-9 for cloud and fog attenuation.

Sources:

- https://www.weather.gov.sg/climate-climate-of-singapore/
- https://www.itu.int/rec/R-REC-P.676/en
- https://www.itu.int/rec/R-REC-P.838-3-200503-I
- https://www.itu.int/rec/R-REC-P.840

## Fail-closed simulator contract

The trainer refuses to run unless `survey.world.rlPlacementReadiness` contains
all of these `true` values:

```json
{
  "rfGeometryReady": true,
  "sensorModelsCalibrated": true,
  "weatherDifferentiated": true,
  "orientationSweepEvaluated": true,
  "exactSimulatorReplayRequired": true
}
```

It also rejects:

- a survey with only one orientation per site;
- comparable Clear/Monsoon/Haze scenarios whose evaluation rows are identical;
- eligible passive-RF detections in an RF-silent scenario;
- options whose package or modality does not match the request;
- same-site duplicate choices, over-budget choices, and infeasible options.

This is intentional. The current prototype survey cannot support a defensible
weather-aware or angle-aware result if it exports one inward-facing angle and
the same scores under every weather profile.

## Historical research command — not authorised for the current study

From `core`:

```powershell
python -m singapore_sensor_fusion.placement.rl `
  path\to\placement_request.json `
  path\to\placement_survey.json `
  --episodes 20000 `
  --seed 17 `
  --output path\to\rl_recommendation.json
```

The output is created without overwriting an existing file. `FEASIBLE` means
the learned layout met the declared simulation evidence constraints;
`UNRESOLVED` means training did not find one. Neither status establishes
physical sensor performance, permission to use a site, or global optimality.

## Required evaluation

Before treating RL as useful, compare it against exact enumeration on small
instances and the existing deterministic baseline on the same candidates,
weather scenarios, seeds, and simulator-call budget. Hold out complete drone
routes, weather combinations, sensor-model perturbations, and failure patterns.
Report worst-scenario and CVaR detection latency, false tracks, localization
error, track continuity, cost, and single-node/failure-domain outages. Every
shortlisted layout must be replayed in Unreal end to end.
