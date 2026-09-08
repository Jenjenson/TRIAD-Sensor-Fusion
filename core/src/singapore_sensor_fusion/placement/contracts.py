"""Validated, deterministic contracts for sensor-placement recommendation.

The placement package is deliberately simulation-only.  Coverage scores are
evidence indexes produced by declared models or an Unreal survey; they are not
calibrated probabilities or statements that a physical site is available.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import hashlib
import json
import math
from pathlib import Path
from types import MappingProxyType
from typing import Any, Mapping, Sequence

from ..fusion_v3 import MODALITY_FAMILY, LayeredModality


REQUEST_SCHEMA = "triad.placement_request.v1"
SURVEY_SCHEMA = "triad.placement_survey.v1"
RECOMMENDATION_SCHEMA = "triad.placement_recommendation.v1"
UNREAL_PATCH_SCHEMA = "triad.unreal_sensor_nodes_patch.v1"
FAMILY_NAMES = frozenset(family.value for family in MODALITY_FAMILY.values())


def _mapping(name: str, value: object) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise TypeError(f"{name} must be an object")
    return value


def _array(name: str, value: object) -> Sequence[object]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise TypeError(f"{name} must be an array")
    return value


def _text(name: str, value: object) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} must be a non-empty string")
    return value.strip()


def _finite(name: str, value: object) -> float:
    if isinstance(value, bool):
        raise TypeError(f"{name} must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{name} must be finite")
    return result


def _nonnegative(name: str, value: object) -> float:
    result = _finite(name, value)
    if result < 0.0:
        raise ValueError(f"{name} must be >= 0")
    return result


def _positive(name: str, value: object) -> float:
    result = _finite(name, value)
    if result <= 0.0:
        raise ValueError(f"{name} must be > 0")
    return result


def _unit(name: str, value: object) -> float:
    result = _finite(name, value)
    if not 0.0 <= result <= 1.0:
        raise ValueError(f"{name} must be in [0, 1]")
    return result


def _positive_integer(name: str, value: object) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise ValueError(f"{name} must be a positive integer")
    return value


def _optional_text_tuple(name: str, value: object) -> tuple[str, ...]:
    if value is None:
        return ()
    return tuple(_text(f"{name}[]", item) for item in _array(name, value))


def canonical_json(value: object) -> str:
    """Return the stable JSON representation used for provenance digests."""

    return json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    )


def stable_digest(value: object) -> str:
    return hashlib.sha256(canonical_json(value).encode("utf-8")).hexdigest()


def _deep_freeze(value: Any) -> Any:
    if isinstance(value, Mapping):
        return MappingProxyType({key: _deep_freeze(item) for key, item in value.items()})
    if isinstance(value, (list, tuple)):
        return tuple(_deep_freeze(item) for item in value)
    return value


def _json_ready(value: Any) -> Any:
    if isinstance(value, Mapping):
        return {key: _json_ready(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_ready(item) for item in value]
    return value


def family_for_modality(modality: str | LayeredModality) -> str:
    selected = LayeredModality(modality)
    return MODALITY_FAMILY[selected].value


@dataclass(frozen=True, slots=True)
class GeoPoint:
    latitude_degrees: float
    longitude_degrees: float
    height_meters: float = 0.0
    height_reference: str = "WGS84_ELLIPSOID"

    def __post_init__(self) -> None:
        latitude = _finite("latitude_degrees", self.latitude_degrees)
        longitude = _finite("longitude_degrees", self.longitude_degrees)
        if not -90.0 <= latitude <= 90.0:
            raise ValueError("latitude_degrees must be in [-90, 90]")
        if not -180.0 <= longitude <= 180.0:
            raise ValueError("longitude_degrees must be in [-180, 180]")
        object.__setattr__(self, "latitude_degrees", latitude)
        object.__setattr__(self, "longitude_degrees", longitude)
        object.__setattr__(self, "height_meters", _finite("height_meters", self.height_meters))
        object.__setattr__(self, "height_reference", _text("height_reference", self.height_reference))

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "GeoPoint":
        item = _mapping("point", value)
        return cls(
            latitude_degrees=item["latitudeDegrees"],
            longitude_degrees=item["longitudeDegrees"],
            height_meters=item.get("heightMeters", 0.0),
            height_reference=item.get("heightReference", "WGS84_ELLIPSOID"),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "latitudeDegrees": self.latitude_degrees,
            "longitudeDegrees": self.longitude_degrees,
            "heightMeters": self.height_meters,
            "heightReference": self.height_reference,
        }


@dataclass(frozen=True, slots=True)
class AreaOfInterest:
    aoi_id: str
    center: GeoPoint
    radius_meters: float
    candidate_buffer_meters: float = 0.0
    center_provenance: str = "unspecified"
    surface_height_source: str = "UNREAL_COLLISION_REQUIRED"

    def __post_init__(self) -> None:
        object.__setattr__(self, "aoi_id", _text("aoi_id", self.aoi_id))
        if not isinstance(self.center, GeoPoint):
            raise TypeError("center must be GeoPoint")
        object.__setattr__(self, "radius_meters", _positive("radius_meters", self.radius_meters))
        object.__setattr__(
            self,
            "candidate_buffer_meters",
            _nonnegative("candidate_buffer_meters", self.candidate_buffer_meters),
        )
        object.__setattr__(
            self, "center_provenance", _text("center_provenance", self.center_provenance)
        )
        object.__setattr__(
            self, "surface_height_source", _text("surface_height_source", self.surface_height_source)
        )

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "AreaOfInterest":
        item = _mapping("aoi", value)
        return cls(
            aoi_id=item.get("id", "aoi"),
            center=GeoPoint.from_mapping(_mapping("aoi.center", item["center"])),
            radius_meters=item["radiusMeters"],
            candidate_buffer_meters=item.get("candidateBufferMeters", 0.0),
            center_provenance=item.get("centerProvenance", "unspecified"),
            surface_height_source=item.get(
                "surfaceHeightSource", "UNREAL_COLLISION_REQUIRED"
            ),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "id": self.aoi_id,
            "center": self.center.to_dict(),
            "radiusMeters": self.radius_meters,
            "candidateBufferMeters": self.candidate_buffer_meters,
            "centerProvenance": self.center_provenance,
            "surfaceHeightSource": self.surface_height_source,
        }


@dataclass(frozen=True, slots=True)
class SamplingPlan:
    horizontal_spacing_meters: float
    target_altitude_bands_agl_meters: tuple[float, ...]
    adaptive_refinement_meters: float | None = None
    maximum_sample_count: int = 100_000

    def __post_init__(self) -> None:
        object.__setattr__(
            self,
            "horizontal_spacing_meters",
            _positive("horizontal_spacing_meters", self.horizontal_spacing_meters),
        )
        altitudes = tuple(
            _nonnegative("target_altitude_bands_agl_meters[]", item)
            for item in self.target_altitude_bands_agl_meters
        )
        if not altitudes:
            raise ValueError("target_altitude_bands_agl_meters must not be empty")
        if len(set(altitudes)) != len(altitudes):
            raise ValueError("target_altitude_bands_agl_meters must be unique")
        object.__setattr__(self, "target_altitude_bands_agl_meters", tuple(sorted(altitudes)))
        if self.adaptive_refinement_meters is not None:
            object.__setattr__(
                self,
                "adaptive_refinement_meters",
                _positive("adaptive_refinement_meters", self.adaptive_refinement_meters),
            )
        object.__setattr__(
            self,
            "maximum_sample_count",
            _positive_integer("maximum_sample_count", self.maximum_sample_count),
        )

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "SamplingPlan":
        item = _mapping("sampling", value)
        return cls(
            horizontal_spacing_meters=item["horizontalSpacingMeters"],
            target_altitude_bands_agl_meters=tuple(
                _array(
                    "sampling.targetAltitudeBandsAglMeters",
                    item["targetAltitudeBandsAglMeters"],
                )
            ),
            adaptive_refinement_meters=item.get("adaptiveRefinementMeters"),
            maximum_sample_count=item.get("maximumSampleCount", 100_000),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "horizontalSpacingMeters": self.horizontal_spacing_meters,
            "targetAltitudeBandsAglMeters": list(self.target_altitude_bands_agl_meters),
            "adaptiveRefinementMeters": self.adaptive_refinement_meters,
            "maximumSampleCount": self.maximum_sample_count,
        }


@dataclass(frozen=True, slots=True)
class PlacementScenario:
    scenario_id: str
    weather_profile: str = "Clear"
    rf_emitting: bool = True
    radar_cross_section_square_meters: float = 0.03
    weight: float = 1.0

    def __post_init__(self) -> None:
        object.__setattr__(self, "scenario_id", _text("scenario_id", self.scenario_id))
        object.__setattr__(self, "weather_profile", _text("weather_profile", self.weather_profile))
        if not isinstance(self.rf_emitting, bool):
            raise TypeError("rf_emitting must be boolean")
        object.__setattr__(
            self,
            "radar_cross_section_square_meters",
            _positive(
                "radar_cross_section_square_meters",
                self.radar_cross_section_square_meters,
            ),
        )
        object.__setattr__(self, "weight", _positive("weight", self.weight))

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "PlacementScenario":
        item = _mapping("scenario", value)
        return cls(
            scenario_id=item["scenarioId"],
            weather_profile=item.get("weatherProfile", "Clear"),
            rf_emitting=item.get("rfEmitting", True),
            radar_cross_section_square_meters=item.get(
                "radarCrossSectionSquareMeters", 0.03
            ),
            weight=item.get("weight", 1.0),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "scenarioId": self.scenario_id,
            "weatherProfile": self.weather_profile,
            "rfEmitting": self.rf_emitting,
            "radarCrossSectionSquareMeters": self.radar_cross_section_square_meters,
            "weight": self.weight,
        }


@dataclass(frozen=True, slots=True)
class SensorPackage:
    package_id: str
    modalities: tuple[str, ...]
    families: tuple[str, ...]
    cost_units: float
    maximum_concurrent_cues: int = 1
    unreal_node_template: Mapping[str, Any] = field(default_factory=dict)
    cost_semantics: str = "declared_input_units"

    def __post_init__(self) -> None:
        object.__setattr__(self, "package_id", _text("package_id", self.package_id))
        modalities = tuple(_text("modalities[]", item) for item in self.modalities)
        if not modalities or len(set(modalities)) != len(modalities):
            raise ValueError("modalities must be non-empty and unique")
        derived_families = tuple(sorted({family_for_modality(item) for item in modalities}))
        families = tuple(sorted(_text("families[]", item) for item in self.families))
        if not families:
            families = derived_families
        if any(item not in FAMILY_NAMES for item in families):
            raise ValueError(f"families must be selected from {sorted(FAMILY_NAMES)}")
        if families != derived_families:
            raise ValueError("families must exactly match the supplied modalities")
        object.__setattr__(self, "modalities", modalities)
        object.__setattr__(self, "families", families)
        object.__setattr__(self, "cost_units", _nonnegative("cost_units", self.cost_units))
        object.__setattr__(
            self,
            "maximum_concurrent_cues",
            _positive_integer("maximum_concurrent_cues", self.maximum_concurrent_cues),
        )
        if not isinstance(self.unreal_node_template, Mapping):
            raise TypeError("unreal_node_template must be an object")
        object.__setattr__(self, "unreal_node_template", dict(self.unreal_node_template))
        object.__setattr__(self, "cost_semantics", _text("cost_semantics", self.cost_semantics))

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "SensorPackage":
        item = _mapping("sensorPackage", value)
        return cls(
            package_id=item["packageId"],
            modalities=tuple(_array("sensorPackage.modalities", item["modalities"])),
            families=tuple(_array("sensorPackage.families", item.get("families", []))),
            cost_units=item.get("costUnits", 0.0),
            maximum_concurrent_cues=item.get("maximumConcurrentCues", 1),
            unreal_node_template=_mapping(
                "sensorPackage.unrealNodeTemplate", item.get("unrealNodeTemplate", {})
            ),
            cost_semantics=item.get("costSemantics", "declared_input_units"),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "packageId": self.package_id,
            "modalities": list(self.modalities),
            "families": list(self.families),
            "costUnits": self.cost_units,
            "costSemantics": self.cost_semantics,
            "maximumConcurrentCues": self.maximum_concurrent_cues,
            "unrealNodeTemplate": dict(self.unreal_node_template),
        }


@dataclass(frozen=True, slots=True)
class PlacementConstraints:
    maximum_sites: int
    minimum_family_count: int = 2
    minimum_site_redundancy: int = 1
    minimum_failure_domain_redundancy: int = 1
    minimum_coverage_fraction: float = 1.0
    budget_units: float | None = None
    critical_sample_ids: tuple[str, ...] = ()

    def __post_init__(self) -> None:
        object.__setattr__(self, "maximum_sites", _positive_integer("maximum_sites", self.maximum_sites))
        family_count = _positive_integer("minimum_family_count", self.minimum_family_count)
        if family_count > len(FAMILY_NAMES):
            raise ValueError(f"minimum_family_count must be <= {len(FAMILY_NAMES)}")
        object.__setattr__(self, "minimum_family_count", family_count)
        object.__setattr__(
            self,
            "minimum_site_redundancy",
            _positive_integer("minimum_site_redundancy", self.minimum_site_redundancy),
        )
        object.__setattr__(
            self,
            "minimum_failure_domain_redundancy",
            _positive_integer(
                "minimum_failure_domain_redundancy",
                self.minimum_failure_domain_redundancy,
            ),
        )
        object.__setattr__(
            self,
            "minimum_coverage_fraction",
            _unit("minimum_coverage_fraction", self.minimum_coverage_fraction),
        )
        if self.budget_units is not None:
            object.__setattr__(self, "budget_units", _nonnegative("budget_units", self.budget_units))
        critical = tuple(_text("critical_sample_ids[]", item) for item in self.critical_sample_ids)
        if len(set(critical)) != len(critical):
            raise ValueError("critical_sample_ids must be unique")
        object.__setattr__(self, "critical_sample_ids", critical)

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "PlacementConstraints":
        item = _mapping("constraints", value)
        return cls(
            maximum_sites=item["maximumSites"],
            minimum_family_count=item.get("minimumFamilyCount", 2),
            minimum_site_redundancy=item.get("minimumSiteRedundancy", 1),
            minimum_failure_domain_redundancy=item.get(
                "minimumFailureDomainRedundancy", 1
            ),
            minimum_coverage_fraction=item.get("minimumCoverageFraction", 1.0),
            budget_units=item.get("budgetUnits"),
            critical_sample_ids=_optional_text_tuple(
                "constraints.criticalSampleIds", item.get("criticalSampleIds")
            ),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "maximumSites": self.maximum_sites,
            "minimumFamilyCount": self.minimum_family_count,
            "minimumSiteRedundancy": self.minimum_site_redundancy,
            "minimumFailureDomainRedundancy": self.minimum_failure_domain_redundancy,
            "minimumCoverageFraction": self.minimum_coverage_fraction,
            "budgetUnits": self.budget_units,
            "criticalSampleIds": list(self.critical_sample_ids),
        }


@dataclass(frozen=True, slots=True)
class PlacementRequest:
    request_id: str
    aoi: AreaOfInterest
    sampling: SamplingPlan
    sensor_packages: tuple[SensorPackage, ...]
    scenarios: tuple[PlacementScenario, ...]
    constraints: PlacementConstraints
    random_seed: int = 0
    schema_version: str = REQUEST_SCHEMA

    def __post_init__(self) -> None:
        if self.schema_version != REQUEST_SCHEMA:
            raise ValueError(f"schema_version must be {REQUEST_SCHEMA}")
        object.__setattr__(self, "request_id", _text("request_id", self.request_id))
        if not isinstance(self.aoi, AreaOfInterest) or not isinstance(self.sampling, SamplingPlan):
            raise TypeError("aoi and sampling have invalid types")
        if not isinstance(self.constraints, PlacementConstraints):
            raise TypeError("constraints must be PlacementConstraints")
        packages = tuple(self.sensor_packages)
        scenarios = tuple(self.scenarios)
        if not packages or not all(isinstance(item, SensorPackage) for item in packages):
            raise ValueError("sensor_packages must contain SensorPackage values")
        if not scenarios or not all(isinstance(item, PlacementScenario) for item in scenarios):
            raise ValueError("scenarios must contain PlacementScenario values")
        if len({item.package_id for item in packages}) != len(packages):
            raise ValueError("sensor package IDs must be unique")
        if len({item.scenario_id for item in scenarios}) != len(scenarios):
            raise ValueError("scenario IDs must be unique")
        object.__setattr__(self, "sensor_packages", packages)
        object.__setattr__(self, "scenarios", scenarios)
        if isinstance(self.random_seed, bool) or not isinstance(self.random_seed, int):
            raise TypeError("random_seed must be an integer")

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "PlacementRequest":
        item = _mapping("placementRequest", value)
        return cls(
            request_id=item["requestId"],
            aoi=AreaOfInterest.from_mapping(_mapping("aoi", item["aoi"])),
            sampling=SamplingPlan.from_mapping(_mapping("sampling", item["sampling"])),
            sensor_packages=tuple(
                SensorPackage.from_mapping(_mapping("sensorPackages[]", package))
                for package in _array("sensorPackages", item["sensorPackages"])
            ),
            scenarios=tuple(
                PlacementScenario.from_mapping(_mapping("scenarios[]", scenario))
                for scenario in _array("scenarios", item["scenarios"])
            ),
            constraints=PlacementConstraints.from_mapping(
                _mapping("constraints", item["constraints"])
            ),
            random_seed=item.get("randomSeed", 0),
            schema_version=item.get("schemaVersion", ""),
        )

    @classmethod
    def load(cls, path: str | Path) -> "PlacementRequest":
        value = json.loads(Path(path).read_text(encoding="utf-8"))
        return cls.from_mapping(_mapping("placementRequest", value))

    def to_dict(self) -> dict[str, object]:
        return {
            "schemaVersion": self.schema_version,
            "requestId": self.request_id,
            "randomSeed": self.random_seed,
            "aoi": self.aoi.to_dict(),
            "sampling": self.sampling.to_dict(),
            "sensorPackages": [item.to_dict() for item in self.sensor_packages],
            "scenarios": [item.to_dict() for item in self.scenarios],
            "constraints": self.constraints.to_dict(),
        }


@dataclass(frozen=True, slots=True)
class CandidateOption:
    option_id: str
    site_id: str
    package_id: str
    orientation_id: str
    location: GeoPoint
    failure_domain_id: str
    site_cost_units: float = 0.0
    feasible: bool = True
    rejection_reasons: tuple[str, ...] = ()
    camera_pitch_degrees: float = 0.0
    camera_yaw_degrees: float = 0.0
    camera_roll_degrees: float = 0.0
    unreal_node_overrides: Mapping[str, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        for name in ("option_id", "site_id", "package_id", "orientation_id", "failure_domain_id"):
            object.__setattr__(self, name, _text(name, getattr(self, name)))
        if not isinstance(self.location, GeoPoint):
            raise TypeError("location must be GeoPoint")
        object.__setattr__(
            self, "site_cost_units", _nonnegative("site_cost_units", self.site_cost_units)
        )
        if not isinstance(self.feasible, bool):
            raise TypeError("feasible must be boolean")
        reasons = tuple(_text("rejection_reasons[]", item) for item in self.rejection_reasons)
        if self.feasible and reasons:
            raise ValueError("feasible options cannot have rejection_reasons")
        object.__setattr__(self, "rejection_reasons", reasons)
        for name in ("camera_pitch_degrees", "camera_yaw_degrees", "camera_roll_degrees"):
            object.__setattr__(self, name, _finite(name, getattr(self, name)))
        if not isinstance(self.unreal_node_overrides, Mapping):
            raise TypeError("unreal_node_overrides must be an object")
        object.__setattr__(self, "unreal_node_overrides", dict(self.unreal_node_overrides))

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "CandidateOption":
        item = _mapping("candidateOption", value)
        orientation = _mapping("candidateOption.cameraRotation", item.get("cameraRotation", {}))
        return cls(
            option_id=item["optionId"],
            site_id=item["siteId"],
            package_id=item["packageId"],
            orientation_id=item.get("orientationId", "default"),
            location=GeoPoint.from_mapping(_mapping("candidateOption.location", item["location"])),
            failure_domain_id=item["failureDomainId"],
            site_cost_units=item.get("siteCostUnits", 0.0),
            feasible=item.get("feasible", True),
            rejection_reasons=_optional_text_tuple(
                "candidateOption.rejectionReasons", item.get("rejectionReasons")
            ),
            camera_pitch_degrees=orientation.get("pitchDegrees", 0.0),
            camera_yaw_degrees=orientation.get("yawDegrees", 0.0),
            camera_roll_degrees=orientation.get("rollDegrees", 0.0),
            unreal_node_overrides=_mapping(
                "candidateOption.unrealNodeOverrides", item.get("unrealNodeOverrides", {})
            ),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "optionId": self.option_id,
            "siteId": self.site_id,
            "packageId": self.package_id,
            "orientationId": self.orientation_id,
            "location": self.location.to_dict(),
            "failureDomainId": self.failure_domain_id,
            "siteCostUnits": self.site_cost_units,
            "feasible": self.feasible,
            "rejectionReasons": list(self.rejection_reasons),
            "cameraRotation": {
                "pitchDegrees": self.camera_pitch_degrees,
                "yawDegrees": self.camera_yaw_degrees,
                "rollDegrees": self.camera_roll_degrees,
            },
            "unrealNodeOverrides": dict(self.unreal_node_overrides),
        }


@dataclass(frozen=True, slots=True)
class CoverageSample:
    sample_id: str
    location: GeoPoint
    east_meters: float
    north_meters: float
    altitude_agl_meters: float
    weight: float = 1.0
    critical: bool = False

    def __post_init__(self) -> None:
        object.__setattr__(self, "sample_id", _text("sample_id", self.sample_id))
        if not isinstance(self.location, GeoPoint):
            raise TypeError("location must be GeoPoint")
        object.__setattr__(self, "east_meters", _finite("east_meters", self.east_meters))
        object.__setattr__(self, "north_meters", _finite("north_meters", self.north_meters))
        object.__setattr__(
            self, "altitude_agl_meters", _nonnegative("altitude_agl_meters", self.altitude_agl_meters)
        )
        object.__setattr__(self, "weight", _positive("weight", self.weight))
        if not isinstance(self.critical, bool):
            raise TypeError("critical must be boolean")

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "CoverageSample":
        item = _mapping("coverageSample", value)
        return cls(
            sample_id=item["sampleId"],
            location=GeoPoint.from_mapping(_mapping("coverageSample.location", item["location"])),
            east_meters=item["eastMeters"],
            north_meters=item["northMeters"],
            altitude_agl_meters=item["altitudeAglMeters"],
            weight=item.get("weight", 1.0),
            critical=item.get("critical", False),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "sampleId": self.sample_id,
            "location": self.location.to_dict(),
            "eastMeters": self.east_meters,
            "northMeters": self.north_meters,
            "altitudeAglMeters": self.altitude_agl_meters,
            "weight": self.weight,
            "critical": self.critical,
        }


@dataclass(frozen=True, slots=True)
class CoverageEvaluation:
    option_id: str
    sample_id: str
    scenario_id: str
    modality: str
    family: str
    quality: float
    eligible: bool
    range_meters: float
    maximum_range_meters: float
    bearing_degrees: float = 0.0
    elevation_degrees: float = 0.0
    within_range: bool = True
    within_fov: bool = True
    line_of_sight: bool = True
    line_of_sight_required: bool = False
    source: str = "UNREAL_SURVEY"

    def __post_init__(self) -> None:
        for name in ("option_id", "sample_id", "scenario_id", "modality", "family", "source"):
            object.__setattr__(self, name, _text(name, getattr(self, name)))
        expected_family = family_for_modality(self.modality)
        if self.family != expected_family:
            raise ValueError(
                f"family {self.family!r} does not match modality {self.modality!r} ({expected_family!r})"
            )
        object.__setattr__(self, "quality", _unit("quality", self.quality))
        object.__setattr__(self, "range_meters", _nonnegative("range_meters", self.range_meters))
        object.__setattr__(
            self,
            "maximum_range_meters",
            _positive("maximum_range_meters", self.maximum_range_meters),
        )
        object.__setattr__(self, "bearing_degrees", _finite("bearing_degrees", self.bearing_degrees) % 360.0)
        object.__setattr__(self, "elevation_degrees", _finite("elevation_degrees", self.elevation_degrees))
        for name in (
            "eligible",
            "within_range",
            "within_fov",
            "line_of_sight",
            "line_of_sight_required",
        ):
            if not isinstance(getattr(self, name), bool):
                raise TypeError(f"{name} must be boolean")
        expected_within_range = self.range_meters <= self.maximum_range_meters + 1e-9
        if self.within_range != expected_within_range:
            raise ValueError(
                "within_range must agree with range_meters and maximum_range_meters"
            )
        if self.eligible and (
            not self.within_range
            or not self.within_fov
            or (self.line_of_sight_required and not self.line_of_sight)
        ):
            raise ValueError("eligible coverage cannot violate range, FOV, or required LOS")

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "CoverageEvaluation":
        item = _mapping("coverageEvaluation", value)
        return cls(
            option_id=item["optionId"],
            sample_id=item["sampleId"],
            scenario_id=item["scenarioId"],
            modality=item["modality"],
            family=item.get("family", family_for_modality(item["modality"])),
            quality=item["quality"],
            eligible=item["eligible"],
            range_meters=item["rangeMeters"],
            maximum_range_meters=item["maximumRangeMeters"],
            bearing_degrees=item.get("bearingDegrees", 0.0),
            elevation_degrees=item.get("elevationDegrees", 0.0),
            within_range=item.get("withinRange", True),
            within_fov=item.get("withinFov", True),
            line_of_sight=item.get("lineOfSight", True),
            line_of_sight_required=item.get("lineOfSightRequired", False),
            source=item.get("source", "UNREAL_SURVEY"),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "optionId": self.option_id,
            "sampleId": self.sample_id,
            "scenarioId": self.scenario_id,
            "modality": self.modality,
            "family": self.family,
            "quality": self.quality,
            "eligible": self.eligible,
            "rangeMeters": self.range_meters,
            "maximumRangeMeters": self.maximum_range_meters,
            "bearingDegrees": self.bearing_degrees,
            "elevationDegrees": self.elevation_degrees,
            "withinRange": self.within_range,
            "withinFov": self.within_fov,
            "lineOfSight": self.line_of_sight,
            "lineOfSightRequired": self.line_of_sight_required,
            "source": self.source,
        }


@dataclass(frozen=True, slots=True)
class PlacementSurvey:
    request_id: str
    options: tuple[CandidateOption, ...]
    samples: tuple[CoverageSample, ...]
    evaluations: tuple[CoverageEvaluation, ...]
    world: Mapping[str, Any] = field(default_factory=dict)
    schema_version: str = SURVEY_SCHEMA

    def __post_init__(self) -> None:
        if self.schema_version != SURVEY_SCHEMA:
            raise ValueError(f"schema_version must be {SURVEY_SCHEMA}")
        object.__setattr__(self, "request_id", _text("request_id", self.request_id))
        options = tuple(self.options)
        samples = tuple(self.samples)
        evaluations = tuple(self.evaluations)
        if not all(isinstance(item, CandidateOption) for item in options):
            raise TypeError("options must contain CandidateOption")
        if not samples or not all(isinstance(item, CoverageSample) for item in samples):
            raise ValueError("samples must contain CoverageSample")
        if not all(isinstance(item, CoverageEvaluation) for item in evaluations):
            raise TypeError("evaluations must contain CoverageEvaluation")
        option_ids = {item.option_id for item in options}
        sample_ids = {item.sample_id for item in samples}
        if len(option_ids) != len(options):
            raise ValueError("option IDs must be unique")
        if len(sample_ids) != len(samples):
            raise ValueError("sample IDs must be unique")
        for item in evaluations:
            if item.option_id not in option_ids:
                raise ValueError(f"evaluation references unknown option {item.option_id!r}")
            if item.sample_id not in sample_ids:
                raise ValueError(f"evaluation references unknown sample {item.sample_id!r}")
        if not isinstance(self.world, Mapping):
            raise TypeError("world must be an object")
        object.__setattr__(self, "options", options)
        object.__setattr__(self, "samples", samples)
        object.__setattr__(self, "evaluations", evaluations)
        object.__setattr__(self, "world", dict(self.world))

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "PlacementSurvey":
        item = _mapping("placementSurvey", value)
        return cls(
            request_id=item["requestId"],
            options=tuple(
                CandidateOption.from_mapping(_mapping("options[]", option))
                for option in _array("options", item.get("options", []))
            ),
            samples=tuple(
                CoverageSample.from_mapping(_mapping("samples[]", sample))
                for sample in _array("samples", item["samples"])
            ),
            evaluations=tuple(
                CoverageEvaluation.from_mapping(_mapping("evaluations[]", evaluation))
                for evaluation in _array("evaluations", item.get("evaluations", []))
            ),
            world=_mapping("world", item.get("world", {})),
            schema_version=item.get("schemaVersion", ""),
        )

    @classmethod
    def load(cls, path: str | Path) -> "PlacementSurvey":
        value = json.loads(Path(path).read_text(encoding="utf-8"))
        return cls.from_mapping(_mapping("placementSurvey", value))

    def to_dict(self) -> dict[str, object]:
        return {
            "schemaVersion": self.schema_version,
            "requestId": self.request_id,
            "world": dict(self.world),
            "options": [item.to_dict() for item in self.options],
            "samples": [item.to_dict() for item in self.samples],
            "evaluations": [item.to_dict() for item in self.evaluations],
        }


@dataclass(frozen=True, slots=True)
class PlacementRecommendation:
    request_id: str
    status: str
    algorithm: str
    selected_options: tuple[Mapping[str, Any], ...]
    total_cost_units: float
    metrics: Mapping[str, Any]
    infeasibility_reasons: tuple[str, ...]
    input_digest: str
    recommendation_digest: str
    schema_version: str = RECOMMENDATION_SCHEMA

    def __post_init__(self) -> None:
        if self.schema_version != RECOMMENDATION_SCHEMA:
            raise ValueError(f"schema_version must be {RECOMMENDATION_SCHEMA}")
        object.__setattr__(self, "request_id", _text("request_id", self.request_id))
        if self.status not in {"FEASIBLE", "INFEASIBLE", "UNRESOLVED"}:
            raise ValueError("status must be FEASIBLE, INFEASIBLE, or UNRESOLVED")
        object.__setattr__(self, "algorithm", _text("algorithm", self.algorithm))
        object.__setattr__(
            self, "total_cost_units", _nonnegative("total_cost_units", self.total_cost_units)
        )
        object.__setattr__(
            self,
            "selected_options",
            tuple(_deep_freeze(dict(item)) for item in self.selected_options),
        )
        object.__setattr__(self, "metrics", _deep_freeze(dict(self.metrics)))
        object.__setattr__(
            self,
            "infeasibility_reasons",
            tuple(_text("infeasibility_reasons[]", item) for item in self.infeasibility_reasons),
        )
        for name in ("input_digest", "recommendation_digest"):
            value = _text(name, getattr(self, name))
            if len(value) != 64 or any(ch not in "0123456789abcdef" for ch in value.lower()):
                raise ValueError(f"{name} must be a SHA-256 hex digest")
            object.__setattr__(self, name, value.lower())
        if self.recommendation_digest != self.computed_recommendation_digest:
            raise ValueError("recommendation_digest does not match recommendation content")

    @property
    def selected_option_ids(self) -> tuple[str, ...]:
        return tuple(str(item["optionId"]) for item in self.selected_options)

    def digest_payload(self) -> dict[str, object]:
        return {
            "schemaVersion": self.schema_version,
            "requestId": self.request_id,
            "status": self.status,
            "algorithm": self.algorithm,
            "selectedOptions": _json_ready(self.selected_options),
            "totalCostUnits": self.total_cost_units,
            "metrics": _json_ready(self.metrics),
            "infeasibilityReasons": list(self.infeasibility_reasons),
            "inputDigest": self.input_digest,
        }

    @property
    def computed_recommendation_digest(self) -> str:
        return stable_digest(self.digest_payload())

    def to_dict(self) -> dict[str, object]:
        return {
            "schemaVersion": self.schema_version,
            "requestId": self.request_id,
            "status": self.status,
            "algorithm": self.algorithm,
            "selectedOptions": _json_ready(self.selected_options),
            "totalCostUnits": self.total_cost_units,
            "metrics": _json_ready(self.metrics),
            "infeasibilityReasons": list(self.infeasibility_reasons),
            "inputDigest": self.input_digest,
            "recommendationDigest": self.recommendation_digest,
            "scoreSemantics": (
                "simulation placement coverage and evidence indexes; not calibrated "
                "probabilities, field performance, or site authorization"
            ),
            "reviewRequired": True,
        }
