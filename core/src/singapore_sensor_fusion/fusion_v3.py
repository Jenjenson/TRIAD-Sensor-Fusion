"""Track-centric independent-family fusion for the layered simulation.

Numeric scores are never added or multiplied across modalities.  The strongest
discounted evidence remains the displayed score, while independent families
only satisfy a visible corroboration rule.  This prevents a swarm of correlated
RF receivers or repeated camera frames from manufacturing confidence.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from enum import StrEnum
import math
from typing import Iterable


FUSION_V3_METHOD = "max_discounted_evidence_independent_family_corroboration_v3"


class LayeredModality(StrEnum):
    WIDEBAND_RF = "wideband_rf"
    MMWAVE = "mmwave"
    RGB = "rgb"
    EVENT_CAMERA = "event_camera"
    SEARCH_RADAR = "SEARCH_RADAR"
    EO_PTZ = "EO_PTZ"
    THERMAL_PTZ = "THERMAL_PTZ"


class LayeredSensorFamily(StrEnum):
    PASSIVE_RF = "passive_rf"
    ACTIVE_RADAR = "active_radar"
    VISUAL = "visual"


MODALITY_FAMILY: dict[LayeredModality, LayeredSensorFamily] = {
    LayeredModality.WIDEBAND_RF: LayeredSensorFamily.PASSIVE_RF,
    LayeredModality.MMWAVE: LayeredSensorFamily.ACTIVE_RADAR,
    LayeredModality.RGB: LayeredSensorFamily.VISUAL,
    LayeredModality.EVENT_CAMERA: LayeredSensorFamily.VISUAL,
    LayeredModality.SEARCH_RADAR: LayeredSensorFamily.ACTIVE_RADAR,
    LayeredModality.EO_PTZ: LayeredSensorFamily.VISUAL,
    LayeredModality.THERMAL_PTZ: LayeredSensorFamily.VISUAL,
}


def _score(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result) or not 0.0 <= result <= 1.0:
        raise ValueError(f"{name} must be finite and in [0, 1]")
    return result


def _nonnegative(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result) or result < 0.0:
        raise ValueError(f"{name} must be finite and >= 0")
    return result


def _utc(value: datetime) -> datetime:
    if not isinstance(value, datetime) or value.tzinfo is None or value.utcoffset() is None:
        raise ValueError("timestamp must be timezone-aware")
    return value.astimezone(timezone.utc)


@dataclass(frozen=True, slots=True)
class LayeredEvidence:
    evidence_id: str
    target_track_id: str
    node_id: str
    modality: LayeredModality
    timestamp: datetime
    raw_score: float
    source_reliability: float
    weather_multiplier: float
    latency_ms: float
    range_m: float | None = None
    correlation_group: str | None = None
    score_origin: str = "unspecified"

    def __post_init__(self) -> None:
        for name in ("evidence_id", "target_track_id", "node_id", "score_origin"):
            value = getattr(self, name)
            if not isinstance(value, str) or not value.strip():
                raise ValueError(f"{name} must be non-empty")
        object.__setattr__(self, "modality", LayeredModality(self.modality))
        object.__setattr__(self, "timestamp", _utc(self.timestamp))
        object.__setattr__(self, "raw_score", _score("raw_score", self.raw_score))
        object.__setattr__(
            self, "source_reliability", _score("source_reliability", self.source_reliability)
        )
        object.__setattr__(
            self, "weather_multiplier", _score("weather_multiplier", self.weather_multiplier)
        )
        object.__setattr__(self, "latency_ms", _nonnegative("latency_ms", self.latency_ms))
        if self.range_m is not None:
            object.__setattr__(self, "range_m", _nonnegative("range_m", self.range_m))
        if self.correlation_group is not None and not self.correlation_group.strip():
            raise ValueError("correlation_group must be non-empty when provided")

    @property
    def discounted_score(self) -> float:
        return self.raw_score * self.source_reliability * self.weather_multiplier

    @property
    def effective_group(self) -> str:
        return self.correlation_group or f"{self.modality.value}:{self.node_id}"

    def to_dict(self) -> dict[str, object]:
        return {
            "evidenceId": self.evidence_id,
            "targetTrackId": self.target_track_id,
            "nodeId": self.node_id,
            "modality": self.modality.value,
            "timestampUtc": self.timestamp.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
            "rawScore": round(self.raw_score, 6),
            "sourceReliabilityAssumption": round(self.source_reliability, 6),
            "weatherMultiplierAssumption": round(self.weather_multiplier, 6),
            "discountedEvidenceScore": round(self.discounted_score, 6),
            "latencyMilliseconds": round(self.latency_ms, 3),
            "rangeMeters": round(self.range_m, 3) if self.range_m is not None else None,
            "correlationGroup": self.effective_group,
            "scoreOrigin": self.score_origin,
        }


@dataclass(frozen=True, slots=True)
class LayeredFusionPolicy:
    detection_threshold: float = 0.55
    corroboration_threshold: float = 0.35
    minimum_families_for_alert: int = 2
    minimum_families_for_confirmation: int = 3
    minimum_rf_nodes_for_preliminary_cue: int = 2
    max_age_s: float = 2.0

    def __post_init__(self) -> None:
        object.__setattr__(self, "detection_threshold", _score("detection_threshold", self.detection_threshold))
        object.__setattr__(
            self, "corroboration_threshold", _score("corroboration_threshold", self.corroboration_threshold)
        )
        maximum_families = len(LayeredSensorFamily)
        if not 1 <= self.minimum_families_for_alert <= maximum_families:
            raise ValueError(
                f"minimum_families_for_alert must be in [1, {maximum_families}]"
            )
        if not (
            self.minimum_families_for_alert
            <= self.minimum_families_for_confirmation
            <= maximum_families
        ):
            raise ValueError(
                "minimum_families_for_confirmation must be between alert minimum "
                f"and {maximum_families}"
            )
        if not 2 <= self.minimum_rf_nodes_for_preliminary_cue <= 32:
            raise ValueError("minimum_rf_nodes_for_preliminary_cue must be in [2, 32]")
        if _nonnegative("max_age_s", self.max_age_s) == 0.0:
            raise ValueError("max_age_s must be > 0")


def fuse_layered_evidence(
    evidence: Iterable[LayeredEvidence],
    *,
    as_of: datetime,
    policy: LayeredFusionPolicy | None = None,
) -> dict[str, object]:
    """Fuse observations associated with exactly one target track."""

    items = tuple(evidence)
    if not items:
        raise ValueError("evidence must not be empty")
    if not all(isinstance(item, LayeredEvidence) for item in items):
        raise TypeError("evidence must contain LayeredEvidence")
    track_ids = {item.target_track_id for item in items}
    if len(track_ids) != 1:
        raise ValueError("fusion refuses evidence from multiple target tracks")
    effective_as_of = _utc(as_of)
    selected_policy = policy or LayeredFusionPolicy()

    eligible = [
        item
        for item in items
        if -0.1 <= (effective_as_of - item.timestamp).total_seconds() <= selected_policy.max_age_s
    ]
    strongest_by_group: dict[str, LayeredEvidence] = {}
    for item in eligible:
        incumbent = strongest_by_group.get(item.effective_group)
        if incumbent is None or (item.discounted_score, item.evidence_id) > (
            incumbent.discounted_score,
            incumbent.evidence_id,
        ):
            strongest_by_group[item.effective_group] = item

    strongest_by_modality: dict[LayeredModality, LayeredEvidence] = {}
    for item in strongest_by_group.values():
        incumbent = strongest_by_modality.get(item.modality)
        if incumbent is None or (item.discounted_score, item.evidence_id) > (
            incumbent.discounted_score,
            incumbent.evidence_id,
        ):
            strongest_by_modality[item.modality] = item

    active = {
        modality: item
        for modality, item in strongest_by_modality.items()
        if item.discounted_score >= selected_policy.corroboration_threshold
    }
    dominant = (
        max(strongest_by_modality.values(), key=lambda item: (item.discounted_score, item.evidence_id))
        if strongest_by_modality
        else None
    )
    fused_score = dominant.discounted_score if dominant else 0.0
    active_families = {MODALITY_FAMILY[modality] for modality in active}
    family_count = len(active_families)
    active_groups_by_modality = {
        modality: {
            group
            for group, item in strongest_by_group.items()
            if item.modality is modality
            and item.discounted_score >= selected_policy.corroboration_threshold
        }
        for modality in LayeredModality
    }
    preliminary_rf_cue = bool(
        dominant is not None
        and dominant.modality is LayeredModality.WIDEBAND_RF
        and fused_score >= selected_policy.detection_threshold
        and family_count == 1
        and len(active_groups_by_modality[LayeredModality.WIDEBAND_RF])
        >= selected_policy.minimum_rf_nodes_for_preliminary_cue
    )
    if dominant is None:
        decision = "NO_CURRENT_EVIDENCE"
    elif fused_score < selected_policy.detection_threshold:
        decision = "BELOW_DETECTION_THRESHOLD"
    elif family_count < selected_policy.minimum_families_for_alert:
        decision = "PRELIMINARY_RF_CUE" if preliminary_rf_cue else "HOLD_FOR_CORROBORATION"
    elif family_count >= selected_policy.minimum_families_for_confirmation:
        decision = "CONFIRMED_TRACK"
    else:
        decision = "DETECTION_ALERT"

    # A second camera representation cannot satisfy a second independent-family
    # gate. For timing, credit the earliest qualifying observation in each
    # family because that is when the family first becomes available.
    latency_candidates = sorted(
        min(
            item.latency_ms
            for modality, item in active.items()
            if MODALITY_FAMILY[modality] is family
        )
        for family in active_families
    )
    decision_latency = None
    if preliminary_rf_cue:
        rf_latencies = sorted(
            item.latency_ms
            for group, item in strongest_by_group.items()
            if group in active_groups_by_modality[LayeredModality.WIDEBAND_RF]
        )
        decision_latency = rf_latencies[selected_policy.minimum_rf_nodes_for_preliminary_cue - 1]
    elif len(latency_candidates) >= selected_policy.minimum_families_for_alert:
        decision_latency = latency_candidates[selected_policy.minimum_families_for_alert - 1]
    nearest_range = min(
        (item.range_m for item in active.values() if item.range_m is not None),
        default=None,
    )
    contributions = []
    for modality in LayeredModality:
        item = strongest_by_modality.get(modality)
        contributions.append(
            {
                "modality": modality.value,
                "family": MODALITY_FAMILY[modality].value,
                "present": item is not None,
                "activeForCorroboration": modality in active,
                "selectedEvidence": item.to_dict() if item is not None else None,
                "creditedToNumericScore": item is dominant,
                "numericStacking": False,
            }
        )

    return {
        "schemaVersion": "3.0",
        "method": FUSION_V3_METHOD,
        "targetTrackId": next(iter(track_ids)),
        "asOfUtc": effective_as_of.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
        "decision": decision,
        "detectionAlert": decision in {"DETECTION_ALERT", "CONFIRMED_TRACK"},
        "preliminaryCue": preliminary_rf_cue,
        "operatorCueActive": preliminary_rf_cue or decision in {"DETECTION_ALERT", "CONFIRMED_TRACK"},
        "confirmationTier": (
            "CONFIRMED"
            if decision == "CONFIRMED_TRACK"
            else "CORROBORATED"
            if decision == "DETECTION_ALERT"
            else "PRELIMINARY"
            if preliminary_rf_cue
            else "UNCONFIRMED"
        ),
        "fusedEvidenceScore": round(fused_score, 6),
        "scoreSemantics": "strongest reliability/weather-discounted evidence index; not a calibrated posterior probability",
        "activeModalities": sorted(item.value for item in active),
        "activeModalityFamilies": sorted(family.value for family in active_families),
        "activeModalityFamilyCount": family_count,
        "activeIndependentGroupCountByModality": {
            modality.value: len(active_groups_by_modality[modality])
            for modality in LayeredModality
        },
        "dominantModality": dominant.modality.value if dominant else None,
        "decisionLatencyMilliseconds": round(decision_latency, 3) if decision_latency is not None else None,
        "nearestContributingRangeMeters": round(nearest_range, 3) if nearest_range is not None else None,
        "inputEvidenceCount": len(items),
        "eligibleEvidenceCount": len(eligible),
        "staleOrFutureEvidenceCount": len(items) - len(eligible),
        "contributions": contributions,
        "policy": {
            "detectionThreshold": selected_policy.detection_threshold,
            "corroborationThreshold": selected_policy.corroboration_threshold,
            "minimumFamiliesForAlert": selected_policy.minimum_families_for_alert,
            "minimumFamiliesForConfirmation": selected_policy.minimum_families_for_confirmation,
            "minimumRFNodesForPreliminaryCue": selected_policy.minimum_rf_nodes_for_preliminary_cue,
            "maxEvidenceAgeSeconds": selected_policy.max_age_s,
        },
        "independenceAssumedForNumericScore": False,
        "corroboratingScoresAddedOrMultiplied": False,
        "independentFamilyDefinition": (
            "passive_rf=wideband RF; active_radar=mmWave and SEARCH_RADAR; "
            "visual=RGB, event-camera, EO_PTZ, and THERMAL_PTZ. Modalities stay "
            "distinct in contributions, but co-family evidence cannot independently "
            "corroborate itself. "
            "One EO PTZ confirmation is one evidence item: its box and radar-cued range are not "
            "counted as separate evidence."
        ),
        "detectionOnly": True,
        "engagementLogicPresent": False,
        "preliminaryCueSemantics": (
            "two-or-more spatially separated RF receivers observed the same associated emitter; "
            "this is an uncorroborated same-family early warning, not a confirmed drone classification"
        ),
    }
