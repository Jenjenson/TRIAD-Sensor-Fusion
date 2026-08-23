# TRIAD fusion API

A FastAPI service that receives sensor observations over HTTP, routes every one
of them through a single fusion function, and serves the fused result in the
shape the Globe-C2 dashboard consumes.

It adds no fusion arithmetic of its own. [`fusion.py`](fusion.py) calls the
existing `singapore_sensor_fusion.layered_runtime.build_layered_snapshot`
(schema validation, admission gates, fusion-v3 policy, provenance rules) and
then the existing `bridge.triad_bridge` C2 payload builders, so the HTTP surface
cannot drift from the contract the rest of the repository already tests.

```text
sensors --HTTP--> /v1/observations/*  -->  buffer  --+
                                                     |
                  /v1/fusion/snapshot ---------------+--> run_fusion()  -->  layered snapshot
                                                                                  |
                                                            /v1/dashboard, /v1/c2/*, /v1/tracks
```

## Install and run

```bash
python -m pip install -e ./core                       # the fusion core
python -m pip install -r integrations/globe-c2/service/requirements.txt

cd integrations/globe-c2
python -m uvicorn service.app:app --host 127.0.0.1 --port 8100
```

Interactive docs: <http://127.0.0.1:8100/docs>. Bind to loopback; the service
has no authentication.

### Configuration

| Variable | Default | Meaning |
| --- | --- | --- |
| `TRIAD_FUSION_API_PROFILE` | `globe` | `globe` for the rich TRIAD context payloads, `standard` for the basic supplied C2 contract |
| `TRIAD_FUSION_VISUAL_MAX_AGE` | `2.0` | Evidence freshness window in seconds |
| `TRIAD_FUSION_STALE_AFTER` | `5.0` | Age at which a fused sample stops being served |
| `TRIAD_FUSION_API_ORIGINS` | `http://127.0.0.1:3000,http://localhost:3000` | Comma-separated CORS allow-list |
| `TRIAD_C2_URL` | unset | Enables `POST /v1/c2/publish` outbound forwarding |

## Two ways in, one fusion path

**Whole snapshot (stateless).** Post a complete
`triad.live_rf_snapshot.v3` document — exactly what the Unreal plugin writes to
`latest_rf_snapshot.json` — and get the fused result back in the same call:

```bash
curl -s -X POST http://127.0.0.1:8100/v1/fusion/snapshot \
  -H 'Content-Type: application/json' -d @latest_rf_snapshot.json
```

**Per-sensor ingest.** Post observations as each sensor produces them, then
fuse. Sites are registered separately because a modality must be explicitly
configured *and* online before its evidence is admissible:

```bash
curl -X POST .../v1/observations/nodes         -d '{"nodes": [...]}'
curl -X POST .../v1/observations/rf            -d '{"links": [...]}'
curl -X POST .../v1/observations/search-radar  -d '{"detections": [...]}'
curl -X POST .../v1/observations/ptz           -d '{"confirmations": [...]}'
curl -X POST .../v1/observations/visual-model  -d '{"detections": [...]}'
curl -X PUT  .../v1/environment                -d '{"weather": {...}}'
curl -X POST .../v1/fusion/run                        # assemble + fuse
```

Both paths call `run_fusion()` and nothing else. `POST /v1/fusion/run` accepts
an optional timezone-aware `as_of` query parameter, which makes replay and tests
deterministic.

### Convenience derivations

The models fill in values the fusion contract derives from fields you already
sent, so clients are not forced to restate them: `snrDb` (= power − noise),
`linkId`, `sensorId` (`<nodeId>:SEARCH_RADAR`, `<nodeId>:EO_PTZ`), PTZ
`modality`, `radarCueSensorId`, `syntheticThermal`, pixel extents, and the
allow-listed `RadarPtzFrames/<eo|thermal>/<node>/latest.{png,json}` paths. Send
a value explicitly and it is passed through untouched — a wrong one is then
rejected by the runtime with a reason rather than silently corrected.

## Reading the output

`GET /v1/dashboard` returns everything the map needs in one document: the
envelope (`asOfUtc`, `summary`, counts, safety declarations), `weather`,
`simulationPerimeter`, `sensors`, `detections` (the `POST /api/cuas` payloads),
`operatingContext`, per-track `tracks`, and `limitations`.

Narrower reads: `GET /v1/tracks`, `/v1/c2/detections`, `/v1/c2/sensors`,
`/v1/c2/context`, `/v1/snapshot` (the full layered snapshot), `/healthz`.

`POST /v1/c2/publish` forwards the last fused sample to a configured Globe-C2
using the existing `TriadBridge`, which also removes cues that are no longer
active. Globe-C2's mutation endpoints have no authentication — keep it on
`127.0.0.1`.

## Behavior worth knowing before you build on it

- **Fail closed, but never silent.** A refused sample returns `422` with the
  runtime's own reason. Individual rejected records are reported per record
  under `rejectedEvidence`, so a dashboard can show *why* a sensor is not
  contributing instead of just showing nothing.
- **Stale means unavailable, not empty.** Reading a C2 projection with no fresh
  fused sample returns `503`, because an empty `200` is indistinguishable from
  clear airspace.
- **Buffering is transport, not memory.** The buffer keeps the newest record per
  sensor/target key inside the freshness window. It gives the fusion policy no
  track continuity: fusion is stateless per sample, exactly as in the runtime.
- **No association layer.** Identity still arrives with the observation
  (`targetActor` / `targetId`); the service does not gate, merge, or split
  tracks. N distinct labels produce N tracks. Sending one physical drone under
  two labels produces two tracks — see `docs/ARCHITECTURE.md`.
- **No authored-truth ingest path.** The per-sensor endpoints have no field for
  scenario truth and `scenarioTargets` is always assembled empty. Truth inside a
  posted producer snapshot stays evaluator-only under the runtime's existing
  rules.
- **Provenance cannot be relabelled.** Route-scoped `Literal` defaults mean a
  radar post cannot declare itself learned-model output, and RGB-derived proxy
  event input is forced diagnostic-only.
- **Simulation-only, detection-only.** No engagement surface. C2 detection ids
  are pseudonyms (`triad:SG-UAS-…`), not actor names.

## Swarm demo and iteration loop

[`demo_swarm.py`](demo_swarm.py) sends simulated swarm observations through the
API and grades the emitted tracks against the physical drones that were declared
to exist. Each scenario says which sensors reported which drone *and under what
label*, so it can make two sensors disagree about one drone or make two drones
share one label.

```bash
cd integrations/globe-c2
python -m service.demo_swarm                                  # in-process, no server
python -m service.demo_swarm --base-url http://127.0.0.1:8100 # against a live server
python -m service.demo_swarm --list
python -m service.demo_swarm --scenario label_disagreement
python -m service.demo_swarm --json                           # machine-readable
```

Verdict vocabulary:

| Finding | Meaning |
| --- | --- |
| `duplicate` | one physical drone produced more than one track |
| `merged` | one track covers more than one physical drone |
| `missing` | a physical drone produced no track |
| `ghost` | a track maps to no physical drone |
| `tier` | the track exists but its confirmation tier is wrong |

Scenarios flagged `known gap` fail today because there is no association layer.
They are reported separately from regressions and excluded from the exit code, so
the demo is usable as a regression gate right now:

```bash
python -m service.demo_swarm            # exit 1 only on regressions
python -m service.demo_swarm --strict   # exit 1 on known-gap failures too
```

When a known-gap scenario starts passing, the summary says so explicitly -- that
is the signal that an association change worked.

Adding a case is a few lines in `scenarios()`: declare the physical drone, list
its observations with their labels, and state the expected tier and track count.

## Test

```bash
cd integrations/globe-c2
python -m unittest service.test_fusion_api -v      # 22 tests
```

Three of those tests run the demo oracle in-process, so the harness itself is
guarded: non-gap scenarios must stay correct, and the two gap scenarios must keep
reporting `duplicate` and `merged` until the gap is actually closed.
