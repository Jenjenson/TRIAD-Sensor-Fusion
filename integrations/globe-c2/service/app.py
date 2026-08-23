"""FastAPI service: sensor observations in, fused C2 dashboard output out.

Two ingest styles share one fusion path:

``POST /v1/fusion/snapshot``
    A complete ``triad.live_rf_snapshot.v3`` document, exactly as the Unreal
    producer writes it. Stateless: fused and answered in the same call.

``POST /v1/observations/*`` then ``POST /v1/fusion/run``
    Per-sensor posts land in a bounded, self-expiring buffer; ``run`` assembles
    one sample from whatever is still fresh and fuses it.

Both call :func:`service.fusion.run_fusion` and nothing else, so the fusion
policy, admission gates, and C2 contract cannot diverge between them.

The service is simulation-only and detection-only. It exposes no engagement
surface, and it has no authored-truth ingest path at all: scenario truth cannot
enter through the per-sensor endpoints, and truth carried inside a producer
snapshot stays evaluator-only exactly as the runtime enforces.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
import logging
import os
from typing import Any

from fastapi import Body, FastAPI, HTTPException, Query, Request, status
from fastapi.middleware.cors import CORSMiddleware

from .fusion import API_PROFILES, FUSION_API_VERSION, FusionInputError, FusionOutput, run_fusion
from .models import (
    EnvironmentUpdate,
    IngestAck,
    PtzBatch,
    RFLinkBatch,
    SearchRadarBatch,
    SensorNodeBatch,
    VisualBatch,
)
from .store import ObservationStore, StoreLimits, iso

LOGGER = logging.getLogger("triad-fusion-api")

DEFAULT_ALLOWED_ORIGINS = ("http://127.0.0.1:3000", "http://localhost:3000")

STALE_DETAIL = (
    "no fresh fused sample; failing closed rather than returning an empty picture that "
    "would be indistinguishable from clear airspace"
)


@dataclass(slots=True)
class Settings:
    """Service configuration. Environment variables override the defaults."""

    api_profile: str = "globe"
    visual_max_age_s: float = 2.0
    stale_after_s: float = 5.0
    allowed_origins: tuple[str, ...] = DEFAULT_ALLOWED_ORIGINS
    c2_url: str | None = None
    c2_timeout_s: float = 5.0
    limits: StoreLimits = field(default_factory=StoreLimits)

    @classmethod
    def from_environment(cls) -> "Settings":
        profile = os.environ.get("TRIAD_FUSION_API_PROFILE", "globe")
        if profile not in API_PROFILES:
            raise ValueError(f"TRIAD_FUSION_API_PROFILE must be one of {API_PROFILES}")
        origins = os.environ.get("TRIAD_FUSION_API_ORIGINS")
        return cls(
            api_profile=profile,
            visual_max_age_s=float(os.environ.get("TRIAD_FUSION_VISUAL_MAX_AGE", "2.0")),
            stale_after_s=float(os.environ.get("TRIAD_FUSION_STALE_AFTER", "5.0")),
            allowed_origins=tuple(item.strip() for item in origins.split(",") if item.strip())
            if origins
            else DEFAULT_ALLOWED_ORIGINS,
            c2_url=os.environ.get("TRIAD_C2_URL"),
        )


def _now() -> datetime:
    return datetime.now(timezone.utc)


def create_app(settings: Settings | None = None) -> FastAPI:
    active = settings or Settings()
    app = FastAPI(
        title="TRIAD sensor fusion API",
        version=FUSION_API_VERSION,
        description=__doc__,
        openapi_tags=[
            {"name": "fusion", "description": "Submit a sample and fuse it."},
            {"name": "observations", "description": "Per-sensor ingest into the sample buffer."},
            {"name": "c2", "description": "Projections for the Globe-C2 dashboard."},
            {"name": "ops", "description": "Health and configuration."},
        ],
    )
    app.state.settings = active
    app.state.store = ObservationStore(active.limits)
    app.state.last_output = None
    app.state.bridge = None

    if active.allowed_origins:
        app.add_middleware(
            CORSMiddleware,
            allow_origins=list(active.allowed_origins),
            allow_methods=["GET", "POST", "PUT", "DELETE"],
            allow_headers=["Content-Type"],
        )

    # -- helpers ------------------------------------------------------------

    def _settings(request: Request) -> Settings:
        return request.app.state.settings

    def _store(request: Request) -> ObservationStore:
        return request.app.state.store

    def _fuse(
        request: Request,
        snapshot: dict[str, Any],
        *,
        rgb_rows: tuple[dict[str, Any], ...] = (),
        event_rows: tuple[dict[str, Any], ...] = (),
        api_profile: str | None = None,
    ) -> FusionOutput:
        config = _settings(request)
        try:
            output = run_fusion(
                snapshot,
                rgb_rows=rgb_rows,
                event_rows=event_rows,
                api_profile=api_profile or config.api_profile,
                visual_max_age_s=config.visual_max_age_s,
            )
        except FusionInputError as exc:
            # Fail closed with the runtime's own reason rather than a generic 400.
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail={
                    "error": "sample refused",
                    "reason": str(exc),
                    "failedClosed": True,
                },
            ) from exc
        request.app.state.last_output = output
        return output

    def _fresh_output(request: Request) -> FusionOutput:
        output = request.app.state.last_output
        if output is None:
            raise HTTPException(
                status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                detail={"error": "no fused sample yet", "reason": STALE_DETAIL},
            )
        age = _output_age_seconds(output)
        limit = _settings(request).stale_after_s
        if age is None or age > limit:
            raise HTTPException(
                status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                detail={
                    "error": "fused sample is stale",
                    "reason": STALE_DETAIL,
                    "ageSeconds": age,
                    "staleAfterSeconds": limit,
                },
            )
        return output

    def _output_age_seconds(output: FusionOutput) -> float | None:
        text = output.as_of_utc
        try:
            parsed = datetime.fromisoformat(text.replace("Z", "+00:00"))
        except (AttributeError, ValueError):
            return None
        if parsed.tzinfo is None:
            parsed = parsed.replace(tzinfo=timezone.utc)
        return max(0.0, (_now() - parsed).total_seconds())

    # -- fusion -------------------------------------------------------------

    @app.post("/v1/fusion/snapshot", tags=["fusion"], summary="Fuse one complete producer snapshot")
    def fuse_snapshot(
        request: Request,
        snapshot: dict[str, Any] = Body(
            ...,
            description="A complete triad.live_rf_snapshot.v3 document.",
        ),
        api_profile: str | None = Query(
            default=None,
            description="Override the configured C2 payload profile ('globe' or 'standard').",
        ),
    ) -> dict[str, Any]:
        """Validate, fuse, and return the dashboard projection in one call."""

        output = _fuse(request, snapshot, api_profile=api_profile)
        return output.dashboard()

    @app.post("/v1/fusion/run", tags=["fusion"], summary="Fuse the buffered observations")
    def fuse_buffer(
        request: Request,
        as_of: datetime | None = Query(
            default=None,
            description="Sample time (ISO-8601, timezone-aware). Defaults to now.",
        ),
        api_profile: str | None = Query(default=None),
    ) -> dict[str, Any]:
        """Assemble one sample from the buffer and fuse it.

        Evidence outside the freshness window is not silently discarded: it is
        reported per record under ``rejectedEvidence``.
        """

        if as_of is not None and (as_of.tzinfo is None or as_of.utcoffset() is None):
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail={"error": "as_of must be timezone-aware"},
            )
        sample = _store(request).assemble(as_of or _now())
        output = _fuse(
            request,
            sample.snapshot,
            rgb_rows=sample.rgb_rows,
            event_rows=sample.event_rows,
            api_profile=api_profile,
        )
        return output.dashboard()

    # -- per-sensor ingest ---------------------------------------------------

    @app.post("/v1/observations/nodes", tags=["observations"], response_model=IngestAck)
    def ingest_nodes(request: Request, batch: SensorNodeBatch) -> IngestAck:
        """Register or refresh sensor sites. A site must be re-posted before its TTL."""

        records = [node.payload() for node in batch.nodes]
        stored = _store(request).upsert_nodes(records)
        return IngestAck(accepted=len(records), storedRecords=stored, modality="SENSOR_NODE")

    @app.post("/v1/observations/rf", tags=["observations"], response_model=IngestAck)
    def ingest_rf(request: Request, batch: RFLinkBatch) -> IngestAck:
        """Passive wideband-RF detections (family: passive_rf)."""

        records = [link.payload() for link in batch.links]
        stored = _store(request).upsert_rf(records)
        return IngestAck(accepted=len(records), storedRecords=stored, modality="WIDEBAND_RF")

    @app.post("/v1/observations/search-radar", tags=["observations"], response_model=IngestAck)
    def ingest_search_radar(request: Request, batch: SearchRadarBatch) -> IngestAck:
        """Simulated search-radar returns (family: active_radar).

        These carry the only measurement geometry the runtime will localize from.
        """

        records = [detection.payload() for detection in batch.detections]
        stored = _store(request).upsert_radar(records)
        return IngestAck(accepted=len(records), storedRecords=stored, modality="SEARCH_RADAR")

    @app.post("/v1/observations/ptz", tags=["observations"], response_model=IngestAck)
    def ingest_ptz(request: Request, batch: PtzBatch) -> IngestAck:
        """Radar-cued EO/thermal confirmations (family: visual).

        A confirmation contributes only when a matching fresh radar cue for the
        same target and track is also present in the same sample.
        """

        records = [confirmation.payload() for confirmation in batch.confirmations]
        stored = _store(request).upsert_ptz(records)
        return IngestAck(accepted=len(records), storedRecords=stored, modality="PTZ_CONFIRMATION")

    @app.post("/v1/observations/visual-model", tags=["observations"], response_model=IngestAck)
    def ingest_visual_model(request: Request, batch: VisualBatch) -> IngestAck:
        """Learned RGB/event-camera detections already associated to a track.

        RGB-derived proxy input and frames without debug-visual exclusion are
        forced diagnostic-only and cannot raise a fusion tier.
        """

        records = [detection.payload() for detection in batch.detections]
        stored = _store(request).upsert_visual(records)
        return IngestAck(accepted=len(records), storedRecords=stored, modality="VISUAL_MODEL")

    @app.put("/v1/environment", tags=["observations"], summary="Set weather and perimeter")
    def set_environment(request: Request, update: EnvironmentUpdate) -> dict[str, Any]:
        """Declared scenario context applied to every assembled sample."""

        _store(request).set_environment(
            weather=update.weather, perimeter=update.simulationPerimeter
        )
        return {
            "weather": update.weather.model_dump(mode="json") if update.weather else None,
            "simulationPerimeter": (
                update.simulationPerimeter.model_dump(mode="json")
                if update.simulationPerimeter
                else None
            ),
            "note": "declared simulation assumptions, not measured atmospheric data",
        }

    @app.delete("/v1/observations", tags=["observations"], summary="Clear the buffer")
    def clear_observations(request: Request) -> dict[str, Any]:
        _store(request).clear()
        return {"cleared": True, "servedAtUtc": iso(_now())}

    # -- dashboard / C2 projections -----------------------------------------

    @app.get("/v1/dashboard", tags=["c2"], summary="Everything the C2 map needs")
    def dashboard(request: Request) -> dict[str, Any]:
        return _fresh_output(request).dashboard()

    @app.get("/v1/tracks", tags=["c2"], summary="Fused tracks only")
    def tracks(request: Request) -> dict[str, Any]:
        output = _fresh_output(request)
        return {**output.envelope(), "tracks": output.dashboard()["tracks"]}

    @app.get("/v1/c2/detections", tags=["c2"], summary="POST /api/cuas payloads")
    def c2_detections(request: Request) -> dict[str, Any]:
        output = _fresh_output(request)
        return {**output.envelope(), "detections": output.c2_detections}

    @app.get("/v1/c2/sensors", tags=["c2"], summary="POST /api/cuas/sensor payloads")
    def c2_sensors(request: Request) -> dict[str, Any]:
        output = _fresh_output(request)
        return {**output.envelope(), "sensors": output.c2_sensors}

    @app.get("/v1/c2/context", tags=["c2"], summary="POST /api/cuas/context payload")
    def c2_context(request: Request) -> dict[str, Any]:
        output = _fresh_output(request)
        return {**output.envelope(), "operatingContext": output.c2_context}

    @app.get("/v1/snapshot", tags=["c2"], summary="Full layered snapshot")
    def layered_snapshot(request: Request) -> dict[str, Any]:
        return _fresh_output(request).layered_snapshot

    @app.post("/v1/c2/publish", tags=["c2"], summary="Push the last sample to Globe-C2")
    def c2_publish(request: Request) -> dict[str, Any]:
        """Forward sensors and detections to a configured Globe-C2 instance.

        Disabled unless ``TRIAD_C2_URL`` is set. Globe-C2's mutation endpoints
        have no authentication: keep it bound to 127.0.0.1.
        """

        config = _settings(request)
        if not config.c2_url:
            raise HTTPException(
                status_code=status.HTTP_409_CONFLICT,
                detail={
                    "error": "C2 forwarding is not configured",
                    "reason": "set TRIAD_C2_URL to enable outbound publishing",
                },
            )
        output = _fresh_output(request)
        from bridge.triad_bridge import BridgeError, C2Client, TriadBridge

        if request.app.state.bridge is None:
            request.app.state.bridge = TriadBridge(
                C2Client(config.c2_url, config.c2_timeout_s), api_profile=config.api_profile
            )
        bridge = request.app.state.bridge
        try:
            bridge.discover_existing()
            sensor_count = bridge.send_sensor_heartbeats(output.layered_snapshot)
            detection_count = bridge.send_detections(output.layered_snapshot)
        except BridgeError as exc:
            raise HTTPException(
                status_code=status.HTTP_502_BAD_GATEWAY,
                detail={"error": "C2 forwarding failed", "reason": str(exc)},
            ) from exc
        return {
            "c2Url": config.c2_url,
            "sensorsRegistered": sensor_count,
            "detectionsForwarded": detection_count,
            "asOfUtc": output.as_of_utc,
        }

    # -- ops ----------------------------------------------------------------

    @app.get("/healthz", tags=["ops"], summary="Liveness, buffer state, and freshness")
    def healthz(request: Request) -> dict[str, Any]:
        config = _settings(request)
        output = request.app.state.last_output
        age = _output_age_seconds(output) if output is not None else None
        return {
            "status": "ok",
            "fusionApiVersion": FUSION_API_VERSION,
            "servedAtUtc": iso(_now()),
            "apiProfile": config.api_profile,
            "simulationOnly": True,
            "detectionOnly": True,
            "engagementLogicPresent": False,
            "lastFusion": {
                "available": output is not None,
                "asOfUtc": output.as_of_utc if output is not None else None,
                "ageSeconds": round(age, 3) if age is not None else None,
                "stale": age is None or age > config.stale_after_s if output is not None else None,
                "staleAfterSeconds": config.stale_after_s,
                "trackCount": len(output.tracks) if output is not None else 0,
            },
            "buffer": _store(request).stats(_now()),
            "c2ForwardingConfigured": bool(config.c2_url),
        }

    return app


app = create_app(Settings.from_environment())
