"""Command-line sensor-placement recommendation workflow."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence

from .contracts import PlacementRequest, PlacementSurvey
from .optimizer import solve_placement
from .report import render_markdown, write_new_text_file
from .unreal_config import build_unreal_sensor_nodes_patch, write_new_json_file


def _default_report_path(output: Path) -> Path:
    return output.with_suffix(".md")


def _default_patch_path(output: Path) -> Path:
    return output.with_name(f"{output.stem}_unreal_sensor_nodes_patch.json")


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Select deterministic sensor sites from an Unreal placement survey. "
            "All outputs are new review artifacts; live Unreal configuration is never modified."
        )
    )
    parser.add_argument("request", type=Path, help="triad.placement_request.v1 JSON")
    parser.add_argument("survey", type=Path, help="triad.placement_survey.v1 JSON")
    parser.add_argument("--output", type=Path, required=True, help="new recommendation JSON path")
    parser.add_argument(
        "--report",
        type=Path,
        help="new human-readable Markdown path; defaults beside --output",
    )
    parser.add_argument(
        "--unreal-patch",
        type=Path,
        help="new review-only SensorNodes patch path; defaults beside --output when feasible",
    )
    parser.add_argument(
        "--no-unreal-patch",
        action="store_true",
        help="do not create the otherwise-default review-only Unreal patch",
    )
    parser.add_argument(
        "--exact-option-limit",
        type=int,
        default=8,
        help="maximum option count for exact enumeration (default: 8)",
    )
    return parser


def _preflight_output_paths(paths: Sequence[Path]) -> None:
    normalized = [str(path.resolve(strict=False)).casefold() for path in paths]
    if len(set(normalized)) != len(normalized):
        raise ValueError("recommendation, report, and patch outputs must use distinct paths")
    existing = [path for path in paths if path.exists()]
    if existing:
        raise FileExistsError(
            "refusing to overwrite existing artifact(s): "
            + ", ".join(str(path) for path in existing)
        )


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    request = PlacementRequest.load(args.request)
    survey = PlacementSurvey.load(args.survey)
    recommendation = solve_placement(
        request,
        survey,
        exact_option_limit=args.exact_option_limit,
    )
    output_path = args.output
    report_path = args.report or _default_report_path(args.output)
    patch_path: Path | None = None
    patch: dict[str, object] | None = None
    if recommendation.status == "FEASIBLE" and not args.no_unreal_patch:
        patch = build_unreal_sensor_nodes_patch(request, survey, recommendation)
        patch_path = args.unreal_patch or _default_patch_path(args.output)
    intended_paths = [output_path, report_path]
    if patch_path is not None:
        intended_paths.append(patch_path)
    _preflight_output_paths(intended_paths)

    created: list[Path] = []
    try:
        write_new_json_file(recommendation.to_dict(), output_path)
        created.append(output_path)
        write_new_text_file(render_markdown(request, recommendation), report_path)
        created.append(report_path)
        if patch_path is not None and patch is not None:
            write_new_json_file(patch, patch_path)
            created.append(patch_path)
    except Exception:
        # Only artifacts successfully created by this invocation are removed;
        # pre-existing paths are never touched.
        for created_path in reversed(created):
            created_path.unlink(missing_ok=True)
        raise
    print(
        json.dumps(
            {
                "status": recommendation.status,
                "selectedSiteCount": len(recommendation.selected_options),
                "worstScenarioCoverageFraction": recommendation.metrics[
                    "worstScenarioCoverageFraction"
                ],
                "recommendation": str(output_path),
                "report": str(report_path),
                "unrealPatch": str(patch_path) if patch_path is not None else None,
                "liveConfigMutated": False,
            },
            separators=(",", ":"),
        )
    )
    return 0 if recommendation.status == "FEASIBLE" else 2


if __name__ == "__main__":
    raise SystemExit(main())
