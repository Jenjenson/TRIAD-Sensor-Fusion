"""Auditable, dependence-conservative track-level sensor fusion.

The values produced here are evidence scores, not calibrated posterior
probabilities.  Raw detector output, source-reliability assumptions, and
weather-reliability assumptions remain separate in every result so a UI can
show exactly why a detection alert was or was not eligible.

This module is detection-only.  It contains no engagement or effector logic.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
from enum import StrEnum
import math
from types import MappingProxyType
from typing import Any, Iterable, Mapping

from .observations import SensorModality, SensorObservation


FUSION_V2_SCHEMA_VERSION = "2.0"
FUSION_V2_METHOD = "max_weather_discounted_evidence_arbitrary_dependence_v2"
SCENARIO_ASSUMPTION_NOTICE = (
    "Reliability multipliers are deterministic simulation scenario assumptions, "
    "not measured accuracy, calibrated likelihoods, or operational performance claims."
)
SCORE_SEMANTICS = (
    "Confidence-discounted evidence index in [0,1]; not a calibrated probability. "
    "Corroborating sensors can satisfy the alert policy but do not numerically inflate "
    "the score without a validated dependence model."
)


def _unit_interval(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result) or not 0.0 <= result <= 1.0:
        raise ValueError(f"{name} must be finite and in [0, 1]")
    return result


def _finite_nonnegative(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result) or result < 0.0:
        raise ValueError(f"{name} must be finite and >= 0")
    return result


def _finite_positive(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result) or result <= 0.0:
        raise ValueError(f"{name} must be finite and > 0")
    return result


def _identifier(name: str, value: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} must be a non-empty string")
    return value.strip()


def _utc_timestamp(name: str, value: datetime) -> datetime:
    if not isinstance(value, datetime) or value.tzinfo is None or value.utcoffset() is None:
        raise ValueError(f"{name} must be a timezone-aware datetime")
    return value.astimezone(timezone.utc)


def _timestamp_text(value: datetime) -> str:
    return value.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def _json_score(value: float) -> float:
    """Keep dashboard JSON stable and free of binary floating-point tails."""

    return round(float(value), 6)


class FusionModality(StrEnum):
    """Dashboard-facing modality keys, including explicit RF bands."""

    RF_2_4_GHZ = "rf_2_4_ghz"
    RF_5_8_GHZ = "rf_5_8_ghz"
    RGBD = "rgbd"
    NEUROMORPHIC = "neuromorphic"

    @property
    def family(self) -> str:
        """Independent-family label used only for corroboration counting."""

        if self in (FusionModality.RF_2_4_GHZ, FusionModality.RF_5_8_GHZ):
            return "rf"
        if self is FusionModality.RGBD:
            return "rgbd"
        return "neuromorphic"


@dataclass(frozen=True, slots=True)
class WeatherProfile:
    """One deterministic Singapore weather stress-test profile.

    All reliability multipliers are declared assumptions.  Meteorological
    fields describe the simulated test condition; they are not live weather.
    """

    profile_id: str
    display_name: str
    rain_rate_mm_h: float
    visibility_km: float
    relative_humidity_percent: float
    reliability_multipliers: Mapping[FusionModality, float]
    assumption_notice: str = SCENARIO_ASSUMPTION_NOTICE

    def __post_init__(self) -> None:
        object.__setattr__(self, "profile_id", _identifier("profile_id", self.profile_id))
        object.__setattr__(self, "display_name", _identifier("display_name", self.display_name))
        object.__setattr__(
            self, "rain_rate_mm_h", _finite_nonnegative("rain_rate_mm_h", self.rain_rate_mm_h)
        )
        object.__setattr__(self, "visibility_km", _finite_positive("visibility_km", self.visibility_km))
        humidity = float(self.relative_humidity_percent)
        if not math.isfinite(humidity) or not 0.0 <= humidity <= 100.0:
            raise ValueError("relative_humidity_percent must be finite and in [0, 100]")
        object.__setattr__(self, "relative_humidity_percent", humidity)
        if not isinstance(self.reliability_multipliers, Mapping):
            raise TypeError("reliability_multipliers must be a mapping")
        normalized: dict[FusionModality, float] = {}
        for raw_modality, raw_multiplier in self.reliability_multipliers.items():
            modality = FusionModality(raw_modality)
            normalized[modality] = _unit_interval(
                f"reliability_multipliers[{modality.value}]", raw_multiplier
            )
        missing = set(FusionModality) - set(normalized)
        extra = set(normalized) - set(FusionModality)
        if missing or extra:
            missing_names = ", ".join(sorted(item.value for item in missing))
            raise ValueError(f"reliability_multipliers must cover every modality; missing: {missing_names}")
        object.__setattr__(self, "reliability_multipliers", MappingProxyType(normalized))
        object.__setattr__(
            self, "assumption_notice", _identifier("assumption_notice", self.assumption_notice)
        )

    def multiplier_for(self, modality: FusionModality) -> float:
        return self.reliability_multipliers[FusionModality(modality)]

    def to_dict(self) -> dict[str, Any]:
        return {
            "profile_id": self.profile_id,
            "display_name": self.display_name,
            "simulated_conditions": {
                "rain_rate_mm_h": self.rain_rate_mm_h,
                "visibility_km": self.visibility_km,
                "relative_humidity_percent": self.relative_humidity_percent,
            },
            "reliability_multipliers": {
                modality.value: self.multiplier_for(modality) for modality in FusionModality
            },
            "reliability_values_are_scenario_assumptions": True,
            "assumption_notice": self.assumption_notice,
        }


def _profile(
    profile_id: str,
    display_name: str,
    *,
    rain_rate_mm_h: float,
    visibility_km: float,
    relative_humidity_percent: float,
    rf_2_4: float,
    rf_5_8: float,
    rgbd: float,
    neuromorphic: float,
) -> WeatherProfile:
    return WeatherProfile(
        profile_id=profile_id,
        display_name=display_name,
        rain_rate_mm_h=rain_rate_mm_h,
        visibility_km=visibility_km,
        relative_humidity_percent=relative_humidity_percent,
        reliability_multipliers={
            FusionModality.RF_2_4_GHZ: rf_2_4,
            FusionModality.RF_5_8_GHZ: rf_5_8,
            FusionModality.RGBD: rgbd,
            FusionModality.NEUROMORPHIC: neuromorphic,
        },
    )


# These profiles are deliberately conservative test knobs, not accuracy claims.
WEATHER_PROFILES: Mapping[str, WeatherProfile] = MappingProxyType(
    {
        item.profile_id: item
        for item in (
            _profile(
                "clear",
                "Clear tropical daylight",
                rain_rate_mm_h=0.0,
                visibility_km=30.0,
                relative_humidity_percent=70.0,
                rf_2_4=1.0,
                rf_5_8=1.0,
                rgbd=1.0,
                neuromorphic=1.0,
            ),
            _profile(
                "light_rain",
                "Light tropical rain",
                rain_rate_mm_h=5.0,
                visibility_km=12.0,
                relative_humidity_percent=85.0,
                rf_2_4=0.995,
                rf_5_8=0.990,
                rgbd=0.88,
                neuromorphic=0.92,
            ),
            _profile(
                "monsoon_heavy_rain",
                "Monsoon heavy rain",
                rain_rate_mm_h=50.0,
                visibility_km=4.0,
                relative_humidity_percent=96.0,
                rf_2_4=0.985,
                rf_5_8=0.970,
                rgbd=0.62,
                neuromorphic=0.75,
            ),
            _profile(
                "haze_fog",
                "Dense haze / fog stress case",
                rain_rate_mm_h=0.0,
                visibility_km=1.5,
                relative_humidity_percent=92.0,
                rf_2_4=0.998,
                rf_5_8=0.995,
                rgbd=0.50,
                neuromorphic=0.62,
            ),
        )
    }
)


@dataclass(frozen=True, slots=True)
class FusionEvidenceV2:
    """One target-associated detector score and its explicit assumptions."""

    evidence_id: str
    timestamp: datetime
    target_track_id: str
    node_id: str
    modality: FusionModality
    raw_model_confidence: float
    source_reliability_assumption: float
    score_origin: str
    model_id: str | None = None
    frequency_hz: float | None = None
    correlation_group: str | None = None
    metadata: Mapping[str, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        object.__setattr__(self, "evidence_id", _identifier("evidence_id", self.evidence_id))
        object.__setattr__(self, "timestamp", _utc_timestamp("timestamp", self.timestamp))
        object.__setattr__(
            self, "target_track_id", _identifier("target_track_id", self.target_track_id)
        )
        object.__setattr__(self, "node_id", _identifier("node_id", self.node_id))
        object.__setattr__(self, "modality", FusionModality(self.modality))
        object.__setattr__(
            self,
            "raw_model_confidence",
            _unit_interval("raw_model_confidence", self.raw_model_confidence),
        )
        object.__setattr__(
            self,
            "source_reliability_assumption",
            _unit_interval(
                "source_reliability_assumption", self.source_reliability_assumption
            ),
        )
        object.__setattr__(self, "score_origin", _identifier("score_origin", self.score_origin))
        if self.model_id is not None:
            object.__setattr__(self, "model_id", _identifier("model_id", self.model_id))
        if self.modality in (FusionModality.RF_2_4_GHZ, FusionModality.RF_5_8_GHZ):
            if self.frequency_hz is None:
                raise ValueError("frequency_hz is required for RF evidence")
            frequency = _finite_positive("frequency_hz", self.frequency_hz)
            low_hz, high_hz = (
                (2.3e9, 2.5e9)
                if self.modality is FusionModality.RF_2_4_GHZ
                else (5.7e9, 5.9e9)
            )
            if not low_hz <= frequency <= high_hz:
                raise ValueError(
                    f"frequency_hz does not match declared modality {self.modality.value}"
                )
            object.__setattr__(self, "frequency_hz", frequency)
        elif self.frequency_hz is not None:
            raise ValueError("frequency_hz is only valid for RF evidence")
        if self.correlation_group is not None:
            object.__setattr__(
                self,
                "correlation_group",
                _identifier("correlation_group", self.correlation_group),
            )
        if not isinstance(self.metadata, Mapping):
            raise TypeError("metadata must be a mapping")
        object.__setattr__(self, "metadata", MappingProxyType(dict(self.metadata)))

    @property
    def effective_correlation_group(self) -> str:
        if self.correlation_group is not None:
            return self.correlation_group
        return f"{self.modality.value}:{self.node_id}:{self.model_id or self.score_origin}"

    @property
    def pre_weather_evidence_score(self) -> float:
        return self.raw_model_confidence * self.source_reliability_assumption

    def weather_adjusted_score(self, weather: WeatherProfile) -> float:
        return self.pre_weather_evidence_score * weather.multiplier_for(self.modality)

    def to_dict(self) -> dict[str, Any]:
        return {
            "evidence_id": self.evidence_id,
            "timestamp": _timestamp_text(self.timestamp),
            "target_track_id": self.target_track_id,
            "node_id": self.node_id,
            "modality": self.modality.value,
            "modality_family": self.modality.family,
            "raw_model_confidence": self.raw_model_confidence,
            "source_reliability_assumption": self.source_reliability_assumption,
            "pre_weather_evidence_score": _json_score(self.pre_weather_evidence_score),
            "score_origin": self.score_origin,
            "model_id": self.model_id,
            "frequency_hz": self.frequency_hz,
            "frequency_ghz": (
                _json_score(self.frequency_hz / 1e9) if self.frequency_hz is not None else None
            ),
            "correlation_group": self.effective_correlation_group,
            "metadata": dict(self.metadata),
        }

    @classmethod
    def from_sensor_observation(
        cls,
        observation: SensorObservation,
        *,
        source_reliability_assumption: float,
        correlation_group: str | None = None,
    ) -> "FusionEvidenceV2":
        """Convert an existing observation without reusing its legacy confidence.

        The explicit ``source_reliability_assumption`` argument is intentional:
        existing adapters often set ``detection_probability`` and
        ``confidence_level`` to the same detector score.  Reusing both would
        silently square that score again.
        """

        if not isinstance(observation, SensorObservation):
            raise TypeError("observation must be a SensorObservation")
        if observation.target_track_id is None:
            raise ValueError("target_track_id is required for fusion-v2")
        frequency_hz: float | None = None
        if observation.modality is SensorModality.RF:
            frequency_hz = observation.frequency_hz
            if frequency_hz is None:
                raise ValueError("RF observations require frequency_hz for fusion-v2")
            if 2.3e9 <= frequency_hz <= 2.5e9:
                modality = FusionModality.RF_2_4_GHZ
            elif 5.7e9 <= frequency_hz <= 5.9e9:
                modality = FusionModality.RF_5_8_GHZ
            else:
                raise ValueError("fusion-v2 supports RF observations only in 2.4 or 5.8 GHz bands")
        elif observation.modality is SensorModality.RGBD:
            modality = FusionModality.RGBD
        elif observation.modality is SensorModality.EVENT_CAMERA:
            modality = FusionModality.NEUROMORPHIC
        else:  # pragma: no cover - SensorModality currently closes this branch
            raise ValueError(f"unsupported observation modality: {observation.modality}")
        return cls(
            evidence_id=observation.observation_id,
            timestamp=observation.timestamp,
            target_track_id=observation.target_track_id,
            node_id=observation.node_id,
            modality=modality,
            raw_model_confidence=observation.detection_probability,
            source_reliability_assumption=source_reliability_assumption,
            score_origin=observation.model_id or "unspecified_sensor_score",
            model_id=observation.model_id,
            frequency_hz=frequency_hz,
            correlation_group=correlation_group,
            metadata={
                **dict(observation.measurements),
                "legacy_observation_confidence_level_not_reused": observation.confidence_level,
            },
        )


@dataclass(frozen=True, slots=True)
class FusionPolicyV2:
    """Transparent detection-alert policy; all thresholds are test settings."""

    detection_threshold: float = 0.55
    corroboration_threshold: float = 0.35
    minimum_modality_families: int = 2
    max_observation_age_s: float = 1.0
    max_future_skew_s: float = 0.1

    def __post_init__(self) -> None:
        object.__setattr__(
            self, "detection_threshold", _unit_interval("detection_threshold", self.detection_threshold)
        )
        object.__setattr__(
            self,
            "corroboration_threshold",
            _unit_interval("corroboration_threshold", self.corroboration_threshold),
        )
        if isinstance(self.minimum_modality_families, bool) or not isinstance(
            self.minimum_modality_families, int
        ):
            raise TypeError("minimum_modality_families must be an integer")
        if not 1 <= self.minimum_modality_families <= 3:
            raise ValueError("minimum_modality_families must be in [1, 3]")
        object.__setattr__(
            self,
            "max_observation_age_s",
            _finite_positive("max_observation_age_s", self.max_observation_age_s),
        )
        object.__setattr__(
            self,
            "max_future_skew_s",
            _finite_nonnegative("max_future_skew_s", self.max_future_skew_s),
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "detection_threshold": self.detection_threshold,
            "corroboration_threshold": self.corroboration_threshold,
            "minimum_modality_families": self.minimum_modality_families,
            "max_observation_age_s": self.max_observation_age_s,
            "max_future_skew_s": self.max_future_skew_s,
            "thresholds_are_scenario_assumptions": True,
        }


@dataclass(frozen=True, slots=True)
class ModalityContributionV2:
    modality: FusionModality
    modality_family: str
    input_evidence_count: int
    correlation_group_count: int
    correlated_samples_not_stacked: int
    non_selected_correlation_groups_not_stacked: int
    selected_evidence_id: str
    selected_node_id: str
    selected_frequency_hz: float | None
    raw_model_confidence: float
    source_reliability_assumption: float
    weather_reliability_multiplier: float
    pre_weather_evidence_score: float
    weather_adjusted_evidence_score: float
    credited_to_fused_score: float
    contribution_status: str
    explanation: str

    def to_dict(self) -> dict[str, Any]:
        return {
            "modality": self.modality.value,
            "modality_family": self.modality_family,
            "input_evidence_count": self.input_evidence_count,
            "correlation_group_count": self.correlation_group_count,
            "correlated_samples_not_stacked": self.correlated_samples_not_stacked,
            "non_selected_correlation_groups_not_stacked": (
                self.non_selected_correlation_groups_not_stacked
            ),
            "selected_evidence_id": self.selected_evidence_id,
            "selected_node_id": self.selected_node_id,
            "selected_frequency_hz": self.selected_frequency_hz,
            "selected_frequency_ghz": (
                _json_score(self.selected_frequency_hz / 1e9)
                if self.selected_frequency_hz is not None
                else None
            ),
            "raw_model_confidence": self.raw_model_confidence,
            "source_reliability_assumption": self.source_reliability_assumption,
            "weather_reliability_multiplier": self.weather_reliability_multiplier,
            "pre_weather_evidence_score": _json_score(self.pre_weather_evidence_score),
            "weather_adjusted_evidence_score": _json_score(
                self.weather_adjusted_evidence_score
            ),
            "credited_to_fused_score": _json_score(self.credited_to_fused_score),
            "contribution_status": self.contribution_status,
            "explanation": self.explanation,
        }


@dataclass(frozen=True, slots=True)
class FusionResultV2:
    target_track_id: str
    as_of: datetime
    weather_profile: WeatherProfile
    policy: FusionPolicyV2
    fused_evidence_score: float
    decision: str
    active_modalities: tuple[str, ...]
    active_modality_families: tuple[str, ...]
    dominant_modality: FusionModality | None
    dominant_evidence_id: str | None
    dominant_node_id: str | None
    input_evidence_count: int
    eligible_evidence_count: int
    stale_evidence_count: int
    future_evidence_count: int
    contributions: tuple[ModalityContributionV2, ...]
    method: str = FUSION_V2_METHOD
    schema_version: str = FUSION_V2_SCHEMA_VERSION

    @property
    def detection_alert(self) -> bool:
        return self.decision == "detection_alert"

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema_version": self.schema_version,
            "method": self.method,
            "target_track_id": self.target_track_id,
            "as_of": _timestamp_text(self.as_of),
            "weather": self.weather_profile.to_dict(),
            "policy": self.policy.to_dict(),
            "fused_evidence_score": _json_score(self.fused_evidence_score),
            "score_semantics": SCORE_SEMANTICS,
            "decision": self.decision,
            "detection_alert": self.detection_alert,
            "active_modalities": list(self.active_modalities),
            "active_modality_families": list(self.active_modality_families),
            "dominant_modality": (
                self.dominant_modality.value if self.dominant_modality is not None else None
            ),
            "dominant_evidence_id": self.dominant_evidence_id,
            "dominant_node_id": self.dominant_node_id,
            "input_evidence_count": self.input_evidence_count,
            "eligible_evidence_count": self.eligible_evidence_count,
            "stale_evidence_count": self.stale_evidence_count,
            "future_evidence_count": self.future_evidence_count,
            "contributions": [item.to_dict() for item in self.contributions],
            "independence_assumed": False,
            "corroboration_numerically_stacked": False,
            "target_association_required": True,
            "reliability_values_are_scenario_assumptions": True,
            "engagement_logic_present": False,
            "assumption_notice": SCENARIO_ASSUMPTION_NOTICE,
        }


def fuse_track_evidence_v2(
    evidence: Iterable[FusionEvidenceV2],
    *,
    weather: WeatherProfile,
    as_of: datetime | None = None,
    policy: FusionPolicyV2 | None = None,
) -> FusionResultV2:
    """Fuse observations already associated with exactly one target track.

    Repeated/correlated observations are reduced to their strongest member.
    The final score is the strongest weather-discounted modality score, even
    across modalities.  This avoids assuming independence.  Other modality
    families can satisfy a separately visible corroboration gate, but never
    increase the numeric evidence score.
    """

    items = tuple(evidence)
    if not items:
        raise ValueError("evidence must contain at least one item")
    if not all(isinstance(item, FusionEvidenceV2) for item in items):
        raise TypeError("evidence must contain FusionEvidenceV2 values")
    if not isinstance(weather, WeatherProfile):
        raise TypeError("weather must be a WeatherProfile")
    policy = policy or FusionPolicyV2()
    if not isinstance(policy, FusionPolicyV2):
        raise TypeError("policy must be a FusionPolicyV2")
    track_ids = {item.target_track_id for item in items}
    if len(track_ids) != 1:
        raise ValueError("fusion-v2 refuses to combine observations from different target tracks")
    track_id = next(iter(track_ids))
    effective_as_of = _utc_timestamp("as_of", as_of or max(item.timestamp for item in items))

    eligible: list[FusionEvidenceV2] = []
    stale_count = 0
    future_count = 0
    for item in items:
        age_seconds = (effective_as_of - item.timestamp).total_seconds()
        if age_seconds > policy.max_observation_age_s:
            stale_count += 1
        elif age_seconds < -policy.max_future_skew_s:
            future_count += 1
        else:
            eligible.append(item)

    by_modality: dict[FusionModality, list[FusionEvidenceV2]] = {}
    for item in eligible:
        by_modality.setdefault(item.modality, []).append(item)

    modality_rows: list[tuple[FusionModality, FusionEvidenceV2, int, int, float]] = []
    for modality, modality_items in by_modality.items():
        strongest_by_group: dict[str, FusionEvidenceV2] = {}
        for item in modality_items:
            group = item.effective_correlation_group
            incumbent = strongest_by_group.get(group)
            if incumbent is None or (
                item.weather_adjusted_score(weather), item.evidence_id
            ) > (incumbent.weather_adjusted_score(weather), incumbent.evidence_id):
                strongest_by_group[group] = item
        selected = max(
            strongest_by_group.values(),
            key=lambda item: (item.weather_adjusted_score(weather), item.evidence_id),
        )
        modality_rows.append(
            (
                modality,
                selected,
                len(modality_items),
                len(strongest_by_group),
                selected.weather_adjusted_score(weather),
            )
        )

    dominant_row = (
        max(modality_rows, key=lambda row: (row[4], row[0].value, row[1].evidence_id))
        if modality_rows
        else None
    )
    fused_score = dominant_row[4] if dominant_row is not None else 0.0
    active_modalities = tuple(
        sorted(row[0].value for row in modality_rows if row[4] >= policy.corroboration_threshold)
    )
    active_families = tuple(
        sorted(
            {
                row[0].family
                for row in modality_rows
                if row[4] >= policy.corroboration_threshold
            }
        )
    )
    if not modality_rows:
        decision = "no_current_evidence"
    elif fused_score < policy.detection_threshold:
        decision = "below_detection_threshold"
    elif len(active_families) < policy.minimum_modality_families:
        decision = "hold_for_corroboration"
    else:
        decision = "detection_alert"

    contributions: list[ModalityContributionV2] = []
    dominant_modality = dominant_row[0] if dominant_row is not None else None
    for modality, selected, input_count, group_count, adjusted_score in sorted(
        modality_rows, key=lambda row: row[0].value
    ):
        is_dominant = dominant_row is not None and modality is dominant_modality
        if is_dominant:
            status = "dominant_score_credited"
            explanation = (
                "Strongest weather-adjusted evidence; credited as the fused score."
            )
        elif adjusted_score >= policy.corroboration_threshold:
            status = "corroborating_not_added"
            explanation = (
                "Supports the corroboration gate but is not added to the score because "
                "cross-sensor independence has not been validated."
            )
        else:
            status = "visible_below_corroboration_threshold"
            explanation = (
                "Shown for auditability but below the configured corroboration threshold."
            )
        contributions.append(
            ModalityContributionV2(
                modality=modality,
                modality_family=modality.family,
                input_evidence_count=input_count,
                correlation_group_count=group_count,
                correlated_samples_not_stacked=max(0, input_count - group_count),
                non_selected_correlation_groups_not_stacked=max(0, group_count - 1),
                selected_evidence_id=selected.evidence_id,
                selected_node_id=selected.node_id,
                selected_frequency_hz=selected.frequency_hz,
                raw_model_confidence=selected.raw_model_confidence,
                source_reliability_assumption=selected.source_reliability_assumption,
                weather_reliability_multiplier=weather.multiplier_for(modality),
                pre_weather_evidence_score=selected.pre_weather_evidence_score,
                weather_adjusted_evidence_score=adjusted_score,
                credited_to_fused_score=adjusted_score if is_dominant else 0.0,
                contribution_status=status,
                explanation=explanation,
            )
        )

    return FusionResultV2(
        target_track_id=track_id,
        as_of=effective_as_of,
        weather_profile=weather,
        policy=policy,
        fused_evidence_score=fused_score,
        decision=decision,
        active_modalities=active_modalities,
        active_modality_families=active_families,
        dominant_modality=dominant_modality,
        dominant_evidence_id=dominant_row[1].evidence_id if dominant_row else None,
        dominant_node_id=dominant_row[1].node_id if dominant_row else None,
        input_evidence_count=len(items),
        eligible_evidence_count=len(eligible),
        stale_evidence_count=stale_count,
        future_evidence_count=future_count,
        contributions=tuple(contributions),
    )


REFERENCE_AS_OF = datetime(2026, 8, 1, 0, 0, 0, tzinfo=timezone.utc)


def reference_scenario_evidence() -> tuple[FusionEvidenceV2, ...]:
    """Fixed synthetic evidence used to regression-test weather behavior."""

    timestamp = REFERENCE_AS_OF - timedelta(milliseconds=200)
    common = {"timestamp": timestamp, "target_track_id": "SIM-TRACK-017"}
    return (
        FusionEvidenceV2(
            evidence_id="rf24-city-001",
            node_id="City_Sector",
            modality=FusionModality.RF_2_4_GHZ,
            raw_model_confidence=0.86,
            source_reliability_assumption=0.82,
            score_origin="analytic_rf_fallback",
            model_id="analytic_energy_detector",
            frequency_hz=2.4e9,
            correlation_group="rf:city:receiver-a",
            metadata={"scenario_fixture": True},
            **common,
        ),
        FusionEvidenceV2(
            evidence_id="rf58-city-001",
            node_id="City_Sector",
            modality=FusionModality.RF_5_8_GHZ,
            raw_model_confidence=0.79,
            source_reliability_assumption=0.80,
            score_origin="analytic_rf_fallback",
            model_id="analytic_energy_detector",
            frequency_hz=5.8e9,
            correlation_group="rf:city:receiver-a",
            metadata={"scenario_fixture": True},
            **common,
        ),
        FusionEvidenceV2(
            evidence_id="rgbd-city-001",
            node_id="City_Sector",
            modality=FusionModality.RGBD,
            raw_model_confidence=0.88,
            source_reliability_assumption=0.90,
            score_origin="oak_rgb_yolo",
            model_id="oak_rgb_yolo:14dd5b34c4a7",
            correlation_group="rgbd:city:frame-140",
            metadata={"range_m": 612.4, "scenario_fixture": True},
            **common,
        ),
        FusionEvidenceV2(
            evidence_id="event-city-001",
            node_id="City_Sector",
            modality=FusionModality.NEUROMORPHIC,
            raw_model_confidence=0.76,
            source_reliability_assumption=0.75,
            score_origin="event_yolo_proxy",
            model_id="fred_event_yolo26s_p2",
            correlation_group="event:city:window-140",
            metadata={"proxy_input": True, "scenario_fixture": True},
            **common,
        ),
    )


def run_reference_weather_sweep() -> dict[str, Any]:
    """Return a fully deterministic, JSON-safe four-weather scenario sweep."""

    evidence = reference_scenario_evidence()
    policy = FusionPolicyV2()
    ordered_profile_ids = ("clear", "light_rain", "monsoon_heavy_rain", "haze_fog")
    results = [
        fuse_track_evidence_v2(
            evidence,
            weather=WEATHER_PROFILES[profile_id],
            as_of=REFERENCE_AS_OF,
            policy=policy,
        ).to_dict()
        for profile_id in ordered_profile_ids
    ]
    return {
        "schema_version": FUSION_V2_SCHEMA_VERSION,
        "scenario_id": "singapore_weather_reference_v1",
        "deterministic_fixture": True,
        "not_empirical_accuracy": True,
        "assumption_notice": SCENARIO_ASSUMPTION_NOTICE,
        "as_of": _timestamp_text(REFERENCE_AS_OF),
        "policy": policy.to_dict(),
        "input_evidence": [item.to_dict() for item in evidence],
        "weather_profiles": [WEATHER_PROFILES[item].to_dict() for item in ordered_profile_ids],
        "results": results,
    }
