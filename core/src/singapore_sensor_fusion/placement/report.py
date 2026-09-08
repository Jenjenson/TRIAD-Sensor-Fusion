"""Human-readable placement recommendation reports."""

from __future__ import annotations

import os
from pathlib import Path
import tempfile

from .contracts import PlacementRecommendation, PlacementRequest


def _percent(value: object) -> str:
    return f"{float(value) * 100.0:.1f}%"


def render_markdown(
    request: PlacementRequest,
    recommendation: PlacementRecommendation,
) -> str:
    """Render a concise report for planners and non-developer reviewers."""

    metrics = recommendation.metrics
    scenario_by_id = {item.scenario_id: item for item in request.scenarios}
    lines = [
        f"# Sensor placement recommendation: {request.request_id}",
        "",
        f"**Result:** {recommendation.status}",
        "",
        (
            f"This simulation evaluated the {request.aoi.radius_meters:g} m "
            f"{request.aoi.aoi_id} area. It selected "
            f"{len(recommendation.selected_options)} site(s) at a declared cost of "
            f"{recommendation.total_cost_units:g} input cost units."
        ),
        "",
        "The result is planning evidence only. It does not establish physical sensor "
        "performance, land/building permission, structural suitability, power, or backhaul.",
        "",
        "## Coverage summary",
        "",
        f"- Worst-scenario compliant coverage: {_percent(metrics['worstScenarioCoverageFraction'])}",
        f"- Scenario-weighted compliant coverage: {_percent(metrics['weightedCoverageFraction'])}",
        f"- Critical sample coverage: {_percent(metrics['criticalCoverageFraction'])}",
        f"- Required independent sensor families: {request.constraints.minimum_family_count}",
        f"- Required contributing sites: {request.constraints.minimum_site_redundancy}",
        (
            "- Required independent failure domains: "
            f"{request.constraints.minimum_failure_domain_redundancy}"
        ),
        "",
        "| Scenario | Weather | Target emits RF | Coverage | Progress toward policy | Covered samples |",
        "| --- | --- | --- | ---: | ---: | ---: |",
    ]
    for scenario_id, row in sorted(metrics["coverageByScenario"].items()):
        scenario = scenario_by_id[scenario_id]
        lines.append(
            f"| {scenario_id} | {scenario.weather_profile} | "
            f"{'yes' if scenario.rf_emitting else 'no'} | "
            f"{_percent(row['coverageFraction'])} | "
            f"{_percent(row['progressFraction'])} | "
            f"{row['coveredSampleCount']}/{row['sampleCount']} |"
        )

    lines.extend(("", "## Selected sites", ""))
    if recommendation.selected_options:
        lines.extend(
            (
                "| Site | Package | Failure domain | Installed cost |",
                "| --- | --- | --- | ---: |",
            )
        )
        for item in recommendation.selected_options:
            lines.append(
                f"| {item['siteId']} | {item['packageId']} | "
                f"{item['failureDomainId']} | {float(item['installedCostUnits']):g} |"
            )
    else:
        lines.append("No sites were selected.")

    if recommendation.infeasibility_reasons:
        reason_heading = (
            "## Why no feasible layout was returned"
            if recommendation.status == "UNRESOLVED"
            else "## Why the request is infeasible"
        )
        lines.extend(("", reason_heading, ""))
        lines.extend(f"- {item}" for item in recommendation.infeasibility_reasons)

    uncovered = list(metrics["uncoveredSampleScenarios"])
    lines.extend(("", "## Remaining gaps", ""))
    if uncovered:
        lines.append(
            f"{len(uncovered)} sample/scenario pair(s) do not meet the complete family and "
            "redundancy policy. The first bounded examples are:"
        )
        lines.append("")
        lines.extend(f"- `{item}`" for item in uncovered[:25])
    else:
        lines.append("No sampled gaps remain under the declared simulation scenarios.")

    lines.extend(
        (
            "",
            "## Provenance and next step",
            "",
            f"- AOI centre provenance: {request.aoi.center_provenance}",
            f"- Surface/terrain height source: `{request.aoi.surface_height_source}`",
            f"- Algorithm: `{recommendation.algorithm}`",
            f"- Input digest: `{recommendation.input_digest}`",
            f"- Recommendation digest: `{recommendation.recommendation_digest}`",
            "- Review the separate Unreal SensorNodes patch before running an isolated "
            "Unreal scenario replay.",
            "- Do not publish planned sites as live C2 sensors until they are explicitly activated.",
            "",
        )
    )
    return "\n".join(lines)


def write_new_text_file(text: str, path: str | Path) -> Path:
    destination = Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        raise FileExistsError(f"refusing to overwrite existing artifact: {destination}")
    file_descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{destination.name}.", suffix=".tmp", dir=destination.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(file_descriptor, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
            if not text.endswith("\n"):
                handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.link(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    return destination
