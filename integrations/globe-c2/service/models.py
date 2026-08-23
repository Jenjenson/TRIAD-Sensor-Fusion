"""Transport models for the TRIAD fusion API.

Validation here is deliberately shallow.  It checks transport shape, numeric
ranges, and timezone awareness so a malformed request fails fast, and it stops
there: every evidence-admission decision stays in
``singapore_sensor_fusion.layered_runtime``, which reports each rejected record
with an explicit reason instead of dropping it silently.

Provenance fields are ``Literal`` defaults.  A client may omit them, but cannot
relabel a modality's origin -- a radar post can never declare itself
learned-model output.  Identifiers and allow-listed frame paths that the fusion
contract derives from ``nodeId``/``sensorType`` are filled in when omitted;
a mismatched value is left untouched so the runtime rejects it with a reason.
"""

from __future__ import annotations

from typing import Any, Literal

from pydantic import AwareDatetime, BaseModel, ConfigDict, Field, model_validator

MAX_NODES_PER_REQUEST = 128
MAX_RECORDS_PER_REQUEST = 512

RuntimeStatus = Literal["ONLINE", "DEGRADED", "OFFLINE"]


class _Record(BaseModel):
    """Base record. Unknown keys pass through so additive v3 fields keep working."""

    model_config = ConfigDict(extra="allow")

    def payload(self) -> dict[str, Any]:
        """Return the JSON-safe mapping handed to the fusion runtime."""

        return self.model_dump(mode="json", exclude_none=True)


class SensorNode(_Record):
    """One sensor site. Each modality is off until explicitly configured online."""

    nodeId: str = Field(min_length=1, max_length=128)
    latitudeDegrees: float = Field(ge=-90.0, le=90.0)
    longitudeDegrees: float = Field(ge=-180.0, le=180.0)
    heightMeters: float = 0.0
    enabled: bool = True
    spawned: bool = True
    runtimeStatus: RuntimeStatus = "ONLINE"
    detectionRangeMeters: float = Field(default=20_000.0, gt=0.0)
    supportedFrequenciesGHz: list[float] | None = None
    receiverSensitivityDbm: float | None = None
    cameraCaptureConfigured: bool = False
    searchRadarConfigured: bool = False
    searchRadarRangeMeters: float | None = Field(default=None, gt=0.0)
    searchRadarRuntimeStatus: RuntimeStatus = "OFFLINE"
    eoPtzConfigured: bool = False
    eoPtzConfirmationRangeMeters: float | None = Field(default=None, gt=0.0)
    eoPtzRuntimeStatus: RuntimeStatus = "OFFLINE"
    thermalPtzConfigured: bool = False
    thermalPtzConfirmationRangeMeters: float | None = Field(default=None, gt=0.0)
    thermalPtzRuntimeStatus: RuntimeStatus = "OFFLINE"


class RFLink(_Record):
    """A passive wideband-RF detection from one receiver on one emitter."""

    nodeId: str = Field(min_length=1, max_length=128)
    targetActor: str = Field(min_length=1, max_length=128)
    timestampUtc: AwareDatetime
    frequencyGHz: float = Field(gt=0.0)
    slantRangeMeters: float = Field(gt=0.0)
    receivedPowerDbm: float
    noiseFloorDbm: float
    lineOfSight: bool
    snrDb: float | None = None
    detected: bool = True
    linkId: str | None = None
    emitterId: str | None = None
    frequencyBandLabel: str | None = None
    azimuthDegrees: float | None = None
    elevationDegrees: float | None = None
    weatherRFLossDb: float | None = None
    blockingActor: str | None = None

    @model_validator(mode="after")
    def _derive(self) -> "RFLink":
        if self.snrDb is None:
            # The runtime requires snr == power - noise within 0.25 dB; deriving
            # the exact difference is a convenience, never a relaxation.
            self.snrDb = self.receivedPowerDbm - self.noiseFloorDbm
        if self.linkId is None:
            self.linkId = f"{self.nodeId}|{self.targetActor}|{self.frequencyGHz:.6f}"
        return self


class SearchRadarDetection(_Record):
    """A simulated active search-radar return with polar measurement geometry."""

    nodeId: str = Field(min_length=1, max_length=128)
    targetActor: str = Field(min_length=1, max_length=128)
    trackId: str = Field(min_length=1, max_length=128)
    timestampUtc: AwareDatetime
    rangeMeters: float = Field(gt=0.0)
    bearingDegrees: float = Field(ge=0.0, lt=360.0)
    elevationDegrees: float = Field(ge=-90.0, le=90.0)
    radialVelocityMetersPerSecond: float
    confidence: float = Field(ge=0.0, le=1.0)
    lineOfSight: bool
    radarCrossSectionSquareMeters: float = Field(default=0.03, ge=0.0)
    rangeEnvelopeMeters: float | None = Field(default=None, gt=0.0)
    azimuthFieldOfRegardDegrees: float = Field(default=360.0, gt=0.0, le=360.0)
    elevationFieldOfRegardDegrees: float = Field(default=60.0, gt=0.0, le=180.0)
    sensorId: str | None = None
    simulationSeconds: float | None = None
    blockingActor: str | None = None
    weatherProfile: str | None = None
    weatherVisibilityMeters: float | None = None
    weatherRainRateMillimetersPerHour: float | None = None
    measurementNoiseModel: str | None = None
    confidenceSemantics: str | None = None
    radarCrossSectionSemantics: str | None = None
    sensorType: Literal["SEARCH_RADAR"] = "SEARCH_RADAR"
    kind: Literal["SIMULATED_SENSOR_DETECTION"] = "SIMULATED_SENSOR_DETECTION"
    source: Literal["ANALYTIC_SEARCH_RADAR"] = "ANALYTIC_SEARCH_RADAR"
    simulated: Literal[True] = True
    calibratedDetector: Literal[False] = False
    detectionOnly: Literal[True] = True

    @model_validator(mode="after")
    def _derive(self) -> "SearchRadarDetection":
        if self.sensorId is None:
            self.sensorId = f"{self.nodeId}:SEARCH_RADAR"
        return self


class PtzConfirmation(_Record):
    """A radar-cued EO or synthetic-thermal PTZ confirmation."""

    nodeId: str = Field(min_length=1, max_length=128)
    targetActor: str = Field(min_length=1, max_length=128)
    trackId: str = Field(min_length=1, max_length=128)
    sensorType: Literal["EO_PTZ", "THERMAL_PTZ"]
    timestampUtc: AwareDatetime
    rangeMeters: float = Field(gt=0.0)
    confidence: float = Field(ge=0.0, le=1.0)
    cueAgeSeconds: float = Field(ge=0.0)
    confirmed: bool
    lineOfSight: bool
    boundingBoxPixels: list[float] = Field(min_length=4, max_length=4)
    hasFrame: bool = True
    slewState: Literal["SLEWING", "SETTLING", "SETTLED"] = "SETTLED"
    imageWidthPixels: int = Field(default=1280, gt=0)
    imageHeightPixels: int = Field(default=720, gt=0)
    fovDegrees: float = Field(default=8.0, gt=0.0, le=180.0)
    pixelExtentWidth: float | None = None
    pixelExtentHeight: float | None = None
    sensorId: str | None = None
    modality: Literal["EO_VISIBLE", "THERMAL_SYNTHETIC"] | None = None
    radarCueSensorId: str | None = None
    syntheticThermal: bool | None = None
    frameRelativePath: str | None = None
    metadataRelativePath: str | None = None
    weatherConfidenceFactor: float | None = Field(default=None, ge=0.0, le=1.0)
    weatherProfile: str | None = None
    weatherVisibilityMeters: float | None = None
    weatherRainRateMillimetersPerHour: float | None = None
    simulationSeconds: float | None = None
    blockingActor: str | None = None
    kind: Literal["SIMULATED_SENSOR_CONFIRMATION"] = "SIMULATED_SENSOR_CONFIRMATION"
    source: Literal["SIMULATION_PROJECTION"] = "SIMULATION_PROJECTION"
    confirmationMethod: Literal["SIMULATION_PROJECTION_TRUTH"] = "SIMULATION_PROJECTION_TRUTH"
    boxSource: Literal["DEBUG_PROJECTION"] = "DEBUG_PROJECTION"
    simulated: Literal[True] = True
    calibratedDetector: Literal[False] = False
    actionsTaken: Literal["none"] = "none"

    @model_validator(mode="after")
    def _derive(self) -> "PtzConfirmation":
        directory = "eo" if self.sensorType == "EO_PTZ" else "thermal"
        if self.sensorId is None:
            self.sensorId = f"{self.nodeId}:{self.sensorType}"
        if self.modality is None:
            self.modality = "EO_VISIBLE" if self.sensorType == "EO_PTZ" else "THERMAL_SYNTHETIC"
        if self.radarCueSensorId is None:
            self.radarCueSensorId = f"{self.nodeId}:SEARCH_RADAR"
        if self.syntheticThermal is None:
            self.syntheticThermal = self.sensorType == "THERMAL_PTZ"
        if self.frameRelativePath is None:
            self.frameRelativePath = f"RadarPtzFrames/{directory}/{self.nodeId}/latest.png"
        if self.metadataRelativePath is None:
            self.metadataRelativePath = f"RadarPtzFrames/{directory}/{self.nodeId}/latest.json"
        left, top, right, bottom = (float(value) for value in self.boundingBoxPixels)
        if self.pixelExtentWidth is None:
            self.pixelExtentWidth = right - left
        if self.pixelExtentHeight is None:
            self.pixelExtentHeight = bottom - top
        return self


class VisualModelDetection(_Record):
    """A learned-model RGB or event-camera detection already associated to a track.

    ``fusionEligible`` is forced false for RGB-derived proxy input and for any
    frame that does not declare debug visuals excluded from sensor capture.
    Both rules mirror ``layered_runtime.load_event_evidence`` exactly.
    """

    modality: Literal["RGB", "EVENT_CAMERA"]
    evidenceId: str = Field(min_length=1, max_length=256)
    targetId: str = Field(min_length=1, max_length=128)
    nodeId: str = Field(min_length=1, max_length=128)
    timestampUtc: AwareDatetime
    score: float = Field(ge=0.0, le=1.0)
    modelId: str = Field(min_length=1, max_length=128)
    rangeMeters: float | None = Field(default=None, ge=0.0)
    className: str = "drone candidate"
    proxyInput: bool = False
    debugVisualsExcludedFromSensorCapture: bool = False
    fusionEligible: bool = False
    fusionExclusionReason: str | None = None
    associationProvenance: str | None = None
    bboxXyxyPixels: list[float] | None = Field(default=None, min_length=4, max_length=4)

    @model_validator(mode="after")
    def _enforce_visual_invariants(self) -> "VisualModelDetection":
        if self.proxyInput:
            self.fusionEligible = False
            self.fusionExclusionReason = (
                "RGB-derived proxy candidate is correlated with RGB and cannot count as an "
                "independent event-camera confirmation"
            )
        elif not self.debugVisualsExcludedFromSensorCapture:
            self.fusionEligible = False
            self.fusionExclusionReason = (
                "frame did not declare debugVisualsExcludedFromSensorCapture=true"
            )
        return self


class Weather(BaseModel):
    """Declared scenario weather. Simulation assumption, not measured atmosphere."""

    model_config = ConfigDict(extra="forbid")

    profile: str = Field(default="Clear", min_length=1, max_length=64)
    rainRateMillimetersPerHour: float = Field(default=0.0, ge=0.0)
    visibilityMeters: float = Field(default=30_000.0, gt=0.0)


class SimulationPerimeter(BaseModel):
    """Axis-aligned evaluation rectangle. Not a legal or national boundary."""

    model_config = ConfigDict(extra="forbid")

    enabled: bool = True
    minimumLongitudeDegrees: float = Field(ge=-180.0, le=180.0)
    maximumLongitudeDegrees: float = Field(ge=-180.0, le=180.0)
    minimumLatitudeDegrees: float = Field(ge=-90.0, le=90.0)
    maximumLatitudeDegrees: float = Field(ge=-90.0, le=90.0)
    phaseRateDeadbandMetersPerSecond: float = Field(default=0.25, ge=0.0)

    @model_validator(mode="after")
    def _ordered(self) -> "SimulationPerimeter":
        if self.minimumLongitudeDegrees >= self.maximumLongitudeDegrees:
            raise ValueError("minimumLongitudeDegrees must be < maximumLongitudeDegrees")
        if self.minimumLatitudeDegrees >= self.maximumLatitudeDegrees:
            raise ValueError("minimumLatitudeDegrees must be < maximumLatitudeDegrees")
        return self


class EnvironmentUpdate(BaseModel):
    """Scenario context shared by every assembled sample."""

    model_config = ConfigDict(extra="forbid")

    weather: Weather | None = None
    simulationPerimeter: SimulationPerimeter | None = None


class SensorNodeBatch(BaseModel):
    model_config = ConfigDict(extra="forbid")
    nodes: list[SensorNode] = Field(min_length=1, max_length=MAX_NODES_PER_REQUEST)


class RFLinkBatch(BaseModel):
    model_config = ConfigDict(extra="forbid")
    links: list[RFLink] = Field(min_length=1, max_length=MAX_RECORDS_PER_REQUEST)


class SearchRadarBatch(BaseModel):
    model_config = ConfigDict(extra="forbid")
    detections: list[SearchRadarDetection] = Field(min_length=1, max_length=MAX_RECORDS_PER_REQUEST)


class PtzBatch(BaseModel):
    model_config = ConfigDict(extra="forbid")
    confirmations: list[PtzConfirmation] = Field(min_length=1, max_length=MAX_RECORDS_PER_REQUEST)


class VisualBatch(BaseModel):
    model_config = ConfigDict(extra="forbid")
    detections: list[VisualModelDetection] = Field(min_length=1, max_length=MAX_RECORDS_PER_REQUEST)


class IngestAck(BaseModel):
    """Acknowledgement for one ingest call."""

    accepted: int
    storedRecords: int
    modality: str
    note: str = (
        "buffered for the next fusion sample; buffering is transport only and is not "
        "track memory -- the fusion policy is stateless per sample"
    )
