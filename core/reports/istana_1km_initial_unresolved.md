# Sensor placement recommendation: istana-1km-mvp

**Result:** UNRESOLVED

This simulation evaluated the 1000 m istana-1km area. It selected 6 site(s) at a declared cost of 12 input cost units.

The result is planning evidence only. It does not establish physical sensor performance, land/building permission, structural suitability, power, or backhaul.

## Coverage summary

- Worst-scenario compliant coverage: 88.1%
- Scenario-weighted compliant coverage: 88.1%
- Critical sample coverage: 100.0%
- Required independent sensor families: 2
- Required contributing sites: 2
- Required independent failure domains: 2

| Scenario | Weather | Target emits RF | Coverage | Progress toward policy | Covered samples |
| --- | --- | --- | ---: | ---: | ---: |
| clear-rf | Clear | yes | 88.1% | 94.0% | 140/165 |
| clear-rf-silent | Clear | no | 88.1% | 94.0% | 140/165 |
| haze-rf | Haze | yes | 88.1% | 94.0% | 140/165 |
| monsoon-rf | Monsoon | yes | 88.1% | 94.0% | 140/165 |

## Selected sites

| Site | Package | Failure domain | Installed cost |
| --- | --- | --- | ---: |
| site-center | full-stack | istana-main-building | 2 |
| site-r200-001 | full-stack | sector-1 | 2 |
| site-r200-004 | full-stack | sector-4 | 2 |
| site-r200-005 | full-stack | sector-5 | 2 |
| site-r750-000 | full-stack | sector-0 | 2 |
| site-r750-003 | full-stack | sector-1 | 2 |

## Why no feasible layout was returned

- The deterministic greedy search did not find a feasible layout; this is not proof that none exists.
- Worst-scenario coverage is 0.880952, below the required 0.950000.
- The declared distinct failure-domain redundancy may be unattainable with the supplied candidates.
- The budget and maximum-site constraints may exclude otherwise covering layouts.

## Remaining gaps

100 sample/scenario pair(s) do not meet the complete family and redundancy policy. The first bounded examples are:

- `clear-rf-silent/sample-agl0015-r900-000`
- `clear-rf-silent/sample-agl0015-r900-001`
- `clear-rf-silent/sample-agl0015-r900-002`
- `clear-rf-silent/sample-agl0015-r900-006`
- `clear-rf-silent/sample-agl0015-r900-007`
- `clear-rf-silent/sample-agl0030-r900-000`
- `clear-rf-silent/sample-agl0030-r900-001`
- `clear-rf-silent/sample-agl0030-r900-002`
- `clear-rf-silent/sample-agl0030-r900-006`
- `clear-rf-silent/sample-agl0030-r900-007`
- `clear-rf-silent/sample-agl0060-r900-000`
- `clear-rf-silent/sample-agl0060-r900-001`
- `clear-rf-silent/sample-agl0060-r900-002`
- `clear-rf-silent/sample-agl0060-r900-006`
- `clear-rf-silent/sample-agl0060-r900-007`
- `clear-rf-silent/sample-agl0120-r900-000`
- `clear-rf-silent/sample-agl0120-r900-001`
- `clear-rf-silent/sample-agl0120-r900-002`
- `clear-rf-silent/sample-agl0120-r900-006`
- `clear-rf-silent/sample-agl0120-r900-007`
- `clear-rf-silent/sample-agl0200-r900-000`
- `clear-rf-silent/sample-agl0200-r900-001`
- `clear-rf-silent/sample-agl0200-r900-002`
- `clear-rf-silent/sample-agl0200-r900-006`
- `clear-rf-silent/sample-agl0200-r900-007`

## Provenance and next step

- AOI centre provenance: Provisional main-building centre from OpenStreetMap way 41895536; verify against the TRIAD level before operational use.
- Surface/terrain height source: `UNREAL_COLLISION_REQUIRED`
- Algorithm: `deterministic_greedy_site_selection_v1`
- Input digest: `ba5f1924b1a8d26f88ee9f2865bed5dc33264b7de5dbd15fd326902fe99fb4ff`
- Recommendation digest: `b4240e0f1067bc10560b10867667389df2481d5dfa54b7ed236f62c3fa607eba`
- Review the separate Unreal SensorNodes patch before running an isolated Unreal scenario replay.
- Do not publish planned sites as live C2 sensors until they are explicitly activated.
