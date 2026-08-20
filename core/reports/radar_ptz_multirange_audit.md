# TRIAD radar-cued PTZ deterministic audit

**Result:** PASS

> Simulation-only software evidence. This is not physical sensor validation and not a learned-model evaluation.

## Clear-weather range matrix

| Slant range | Sites | Passes | Pass rate | Decisions |
|---:|---:|---:|---:|---|
| 25 m | 8 | 8 | 100% | CONFIRMED_TRACK: 8 |
| 100 m | 8 | 8 | 100% | CONFIRMED_TRACK: 8 |
| 250 m | 8 | 8 | 100% | CONFIRMED_TRACK: 8 |
| 500 m | 8 | 8 | 100% | CONFIRMED_TRACK: 8 |
| 1000 m | 8 | 8 | 100% | CONFIRMED_TRACK: 8 |

## Weather at 500 m

| Weather | Rain | Visibility | Confirmed | Operator cues | Decisions |
|---|---:|---:|---:|---:|---|
| Clear | 0.0 mm/h | 30000 m | 8/8 | 8/8 | CONFIRMED_TRACK: 8 |
| Monsoon | 50.0 mm/h | 1500 m | 8/8 | 8/8 | CONFIRMED_TRACK: 8 |
| DenseFog | 5.0 mm/h | 500 m | 8/8 | 8/8 | CONFIRMED_TRACK: 8 |

## Negative gates

- lowScoreFalseAlarm: PASS
- staleRadarRejectsDependentPtz: PASS
- occludedPtzCannotConfirm: PASS

## Interpretation

- Confidence values are declared deterministic audit inputs, not calibrated probabilities.
- The audit verifies software contracts, fusion gates, geometry conversion, provenance, and fail-closed behavior.
- It does not establish physical probability of detection, false-alarm rate, recognition range, atmospheric transfer accuracy, or hardware performance.
- Real acceptance requires measured datasets, calibrated sensors, surveyed geometry, representative backgrounds, and statistically powered trials.
