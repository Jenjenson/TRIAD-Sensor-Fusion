# Sensor placement recommendation: istana-1km-mvp

**Result:** FEASIBLE

This simulation evaluated the 1000 m istana-1km area. It selected 4 site(s) at a declared cost of 8 input cost units.

The result is planning evidence only. It does not establish physical sensor performance, land/building permission, structural suitability, power, or backhaul.

## Coverage summary

- Worst-scenario compliant coverage: 95.2%
- Scenario-weighted compliant coverage: 95.2%
- Critical sample coverage: 100.0%
- Required independent sensor families: 2
- Required contributing sites: 2
- Required independent failure domains: 2

| Scenario | Weather | Target emits RF | Coverage | Progress toward policy | Covered samples |
| --- | --- | --- | ---: | ---: | ---: |
| clear-rf | Clear | yes | 95.2% | 97.6% | 155/165 |
| clear-rf-silent | Clear | no | 95.2% | 97.6% | 155/165 |
| haze-rf | Haze | yes | 95.2% | 97.6% | 155/165 |
| monsoon-rf | Monsoon | yes | 95.2% | 97.6% | 155/165 |

## Selected sites

| Site | Package | Failure domain | Installed cost |
| --- | --- | --- | ---: |
| site-center | full-stack | istana-main-building | 2 |
| site-r950-000 | full-stack | sector-0 | 2 |
| site-r950-002 | full-stack | sector-1 | 2 |
| site-r950-014 | full-stack | sector-7 | 2 |

## Remaining gaps

40 sample/scenario pair(s) do not meet the complete family and redundancy policy. The first bounded examples are:

- `clear-rf-silent/sample-agl0015-r900-002`
- `clear-rf-silent/sample-agl0015-r900-006`
- `clear-rf-silent/sample-agl0030-r900-002`
- `clear-rf-silent/sample-agl0030-r900-006`
- `clear-rf-silent/sample-agl0060-r900-002`
- `clear-rf-silent/sample-agl0060-r900-006`
- `clear-rf-silent/sample-agl0120-r900-002`
- `clear-rf-silent/sample-agl0120-r900-006`
- `clear-rf-silent/sample-agl0200-r900-002`
- `clear-rf-silent/sample-agl0200-r900-006`
- `clear-rf/sample-agl0015-r900-002`
- `clear-rf/sample-agl0015-r900-006`
- `clear-rf/sample-agl0030-r900-002`
- `clear-rf/sample-agl0030-r900-006`
- `clear-rf/sample-agl0060-r900-002`
- `clear-rf/sample-agl0060-r900-006`
- `clear-rf/sample-agl0120-r900-002`
- `clear-rf/sample-agl0120-r900-006`
- `clear-rf/sample-agl0200-r900-002`
- `clear-rf/sample-agl0200-r900-006`
- `haze-rf/sample-agl0015-r900-002`
- `haze-rf/sample-agl0015-r900-006`
- `haze-rf/sample-agl0030-r900-002`
- `haze-rf/sample-agl0030-r900-006`
- `haze-rf/sample-agl0060-r900-002`

## Provenance and next step

- AOI centre provenance: Provisional main-building centre from OpenStreetMap way 41895536; verify against the TRIAD level before operational use.
- Surface/terrain height source: `UNREAL_COLLISION_REQUIRED`
- Algorithm: `deterministic_greedy_site_selection_v1`
- Input digest: `46b1b2b849ca444310c4966509b9599b0ca18f2751fd93a8e751a5e908c5db5e`
- Recommendation digest: `147e086b36ae29f2c2b0630161ce9d9bca82b78faba6bae2493d68dc93c4798c`
- Review the separate Unreal SensorNodes patch before running an isolated Unreal scenario replay.
- Do not publish planned sites as live C2 sensors until they are explicitly activated.
