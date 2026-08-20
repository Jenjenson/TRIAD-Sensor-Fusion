"""Conservative probability fusion that makes no independence assumption."""

from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Iterable


def _probability(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result) or not 0.0 <= result <= 1.0:
        raise ValueError(f"{name} must be finite and in [0, 1]")
    return result


@dataclass(frozen=True, slots=True)
class ProbabilityEvidence:
    """A probability plus confidence that its calibration is applicable."""

    source_id: str
    probability: float
    confidence_level: float
    correlation_group: str | None = None

    def __post_init__(self) -> None:
        if not isinstance(self.source_id, str) or not self.source_id.strip():
            raise ValueError("source_id must be a non-empty string")
        object.__setattr__(self, "source_id", self.source_id.strip())
        object.__setattr__(self, "probability", _probability("probability", self.probability))
        object.__setattr__(
            self, "confidence_level", _probability("confidence_level", self.confidence_level)
        )
        if self.correlation_group is not None:
            if not isinstance(self.correlation_group, str) or not self.correlation_group.strip():
                raise ValueError("correlation_group must be None or a non-empty string")
            object.__setattr__(self, "correlation_group", self.correlation_group.strip())

    @property
    def discounted_lower_bound(self) -> float:
        """Worst-case mixture bound if invalid calibration contributes zero."""

        return self.probability * self.confidence_level


@dataclass(frozen=True, slots=True)
class ConservativeFusionResult:
    fused_probability_lower_bound: float
    supporting_confidence_level: float
    dominant_source_id: str
    evidence_count: int
    method: str = "max_confidence_discounted_evidence_no_independence_v1"


def conservative_confidence_fusion(
    evidence: Iterable[ProbabilityEvidence],
) -> ConservativeFusionResult:
    """Fuse by retaining the strongest confidence-discounted lower bound.

    Without an empirically validated dependence model, combining scores as an
    independent noisy-OR can overstate certainty. The maximum individual lower
    bound is valid for the union even under arbitrary dependence. Consequently,
    additional sensors corroborate but do not inflate this probability.
    """

    items = tuple(evidence)
    if not items:
        raise ValueError("evidence must contain at least one item")
    if not all(isinstance(item, ProbabilityEvidence) for item in items):
        raise TypeError("evidence must contain ProbabilityEvidence values")
    dominant = max(items, key=lambda item: (item.discounted_lower_bound, item.source_id))
    return ConservativeFusionResult(
        fused_probability_lower_bound=dominant.discounted_lower_bound,
        supporting_confidence_level=dominant.confidence_level,
        dominant_source_id=dominant.source_id,
        evidence_count=len(items),
    )
