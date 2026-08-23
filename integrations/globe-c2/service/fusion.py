"""The single fusion entry point shared by every API route.

Both ingest styles -- a whole producer snapshot, or per-sensor observations
assembled from the buffer -- funnel through :func:`run_fusion`.  It performs no
fusion arithmetic of its own: it calls the existing
``layered_runtime.build_layered_snapshot`` (validation, admission gates,
fusion-v3 policy, provenance) and then the existing Globe-C2 payload builders,
so the API can never drift from the contract the rest of the repository tests.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
import sys
from typing import Any, Iterable, Mapping

_SERVICE_DIR = Path(__file__).resolve().parent
_INTEGRATION_DIR = _SERVICE_DIR.parent          # integrations/globe-c2
_REPOSITORY_ROOT = _INTEGRATION_DIR.parents[1]  # repository root

# "globe-c2" is not a valid Python identifier, so its parent cannot be a package.
# Adding the integration directory keeps ``bridge`` importable from any cwd.
if str(_INTEGRATION_DIR) not in sys.path:
    sys.path.insert(0, str(_INTEGRATION_DIR))

try:  # pragma: no cover - exercised by whichever install path is present
    from singapore_sensor_fusion.layered_runtime import build_layered_snapshot
except ModuleNotFoundError:  # pragma: no cover - fresh clone without pip install -e ./core
    sys.path.insert(0, str(_REPOSITORY_ROOT / "core" / "src"))
    from singapore_sensor_fusion.layered_runtime import build_layered_snapshot

from singapore_sensor_fusion.fusion_v3 import FUSION_V3_METHOD

from bridge.triad_bridge import (
    EXPECTED_SCHEMA,
    build_detection_payloads,
    build_operating_context_payload,
    build_sensor_payloads,
    build_standard_detection_payloads,
    build_standard_sensor_payloads,
)

FUSION_API_VERSION = "1.0"
API_PROFILES = ("globe", "standard")


class FusionInputError(ValueError):
    """The submitted sample cannot be fused; the caller gets an explicit reason."""


@dataclass(frozen=True, slots=True)
class FusionOutput:
    """One fused sample plus every projection a dashboard or C2 needs."""

    as_of_utc: str
    served_at_utc: str
    api_profile: str
    layered_snapshot: dict[str, Any]
    c2_sensors: list[dict[str, Any]]
    c2_detections: list[dict[str, Any]]
    c2_context: dict[str, Any] | None

    @property
    def tracks(self) -> list[dict[str, Any]]:
        tracks = self.layered_snapshot.get("tracks")
        return tracks if isinstance(tracks, list) else []

    def envelope(self) -> dict[str, Any]:
        """Metadata common to every response, including the safety declarations."""

        snapshot = self.layered_snapshot
        validation = snapshot.get("longRangeEvidenceValidation", {})
        return {
            "fusionApiVersion": FUSION_API_VERSION,
            "asOfUtc": self.as_of_utc,
            "servedAtUtc": self.served_at_utc,
            "apiProfile": self.api_profile,
            "outputSchemaVersion": snapshot.get("schemaVersion"),
            "fusionMethod": FUSION_V3_METHOD,
            "simulationOnly": snapshot.get("simulationOnly") is True,
            "detectionOnly": snapshot.get("detectionOnly") is True,
            "actionsTaken": snapshot.get("actionsTaken"),
            "numericScoreStacking": False,
            "summary": snapshot.get("summary", {}),
            "trackCount": len(self.tracks),
            "c2SensorCount": len(self.c2_sensors),
            "c2DetectionCount": len(self.c2_detections),
            "rejectedEvidence": {
                "rf": validation.get("rejectedRFDetections", []),
                "searchRadar": validation.get("rejectedSearchRadarDetections", []),
                "ptz": validation.get("rejectedPtzConfirmations", []),
            },
        }

    def dashboard(self) -> dict[str, Any]:
        """Compact per-track view for the C2 map, alongside the C2 payloads."""

        snapshot = self.layered_snapshot
        return {
            **self.envelope(),
            "weather": snapshot.get("weather"),
            "simulationPerimeter": snapshot.get("simulationPerimeter"),
            "sensors": self.c2_sensors,
            "detections": self.c2_detections,
            "operatingContext": self.c2_context,
            "tracks": [
                {
                    "trackId": track.get("trackId"),
                    "targetType": track.get("targetType"),
                    "positionAvailable": track.get("positionAvailable"),
                    "latitudeDegrees": track.get("latitudeDegrees"),
                    "longitudeDegrees": track.get("longitudeDegrees"),
                    "heightMeters": track.get("heightMeters"),
                    "speedMetersPerSecond": track.get("speedMetersPerSecond"),
                    "headingDegrees": track.get("headingDegrees"),
                    "airspaceState": track.get("airspaceState"),
                    "confirmationTier": (track.get("fusion") or {}).get("confirmationTier"),
                    "decision": (track.get("fusion") or {}).get("decision"),
                    "fusedEvidenceScore": (track.get("fusion") or {}).get("fusedEvidenceScore"),
                    "scoreSemantics": (track.get("fusion") or {}).get("scoreSemantics"),
                    "activeModalities": (track.get("fusion") or {}).get("activeModalities"),
                    "activeModalityFamilyCount": (track.get("fusion") or {}).get(
                        "activeModalityFamilyCount"
                    ),
                    "dominantModality": (track.get("fusion") or {}).get("dominantModality"),
                    "operatorCueActive": (track.get("fusion") or {}).get("operatorCueActive"),
                    "alert": track.get("alert"),
                    "modalityEvidence": track.get("modalityEvidence"),
                    "visualFrame": track.get("visualFrame"),
                }
                for track in self.tracks
            ],
            "limitations": snapshot.get("limitations", []),
        }


def run_fusion(
    producer_snapshot: Mapping[str, Any],
    *,
    rgb_rows: Iterable[Mapping[str, Any]] = (),
    event_rows: Iterable[Mapping[str, Any]] = (),
    api_profile: str = "globe",
    visual_max_age_s: float = 2.0,
) -> FusionOutput:
    """Validate, fuse, and project one sample. The only fusion path in the API.

    Raises :class:`FusionInputError` when the sample is unusable, carrying the
    runtime's own fail-closed reason so the caller learns *why* it was refused.
    """

    if api_profile not in API_PROFILES:
        raise FusionInputError(f"unsupported api_profile: {api_profile!r}")
    if not isinstance(producer_snapshot, Mapping):
        raise FusionInputError("snapshot body must be a JSON object")

    try:
        layered = build_layered_snapshot(
            producer_snapshot,
            rgb_rows=tuple(rgb_rows),
            event_rows=tuple(event_rows),
            visual_max_age_s=visual_max_age_s,
        )
    except (ValueError, TypeError, KeyError) as exc:
        raise FusionInputError(str(exc)) from exc

    if layered.get("schemaVersion") != EXPECTED_SCHEMA:
        raise FusionInputError(
            f"fusion produced unexpected schema {layered.get('schemaVersion')!r}"
        )

    if api_profile == "standard":
        sensors = build_standard_sensor_payloads(layered)
        detections = build_standard_detection_payloads(layered)
        context = None
    else:
        sensors = build_sensor_payloads(layered)
        detections = build_detection_payloads(layered)
        context = build_operating_context_payload(layered)

    return FusionOutput(
        as_of_utc=str(layered.get("timestampUtc")),
        served_at_utc=datetime.now(timezone.utc)
        .isoformat(timespec="milliseconds")
        .replace("+00:00", "Z"),
        api_profile=api_profile,
        layered_snapshot=layered,
        c2_sensors=sensors,
        c2_detections=detections,
        c2_context=context,
    )
