"""Write the deterministic fusion-v2 Singapore weather scenario fixture."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import tempfile
from typing import Any, Mapping, Sequence

from .fusion_v2 import run_reference_weather_sweep


def write_json_atomic(payload: Mapping[str, Any], output: str | Path) -> Path:
    destination = Path(output).expanduser().resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(payload, indent=2, sort_keys=True, allow_nan=False) + "\n"
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        newline="\n",
        dir=destination.parent,
        prefix=f".{destination.name}.",
        suffix=".tmp",
        delete=False,
    ) as temporary:
        temporary.write(encoded)
        temporary_path = Path(temporary.name)
    temporary_path.replace(destination)
    return destination


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Write a deterministic, assumption-labelled Singapore fusion-v2 weather sweep."
        )
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("fusion_v2_weather_scenarios.json"),
        help="JSON destination (default: ./fusion_v2_weather_scenarios.json)",
    )
    args = parser.parse_args(argv)
    destination = write_json_atomic(run_reference_weather_sweep(), args.output)
    print(f"Wrote deterministic fusion-v2 weather sweep: {destination}")
    return 0


if __name__ == "__main__":  # pragma: no cover - exercised through module invocation
    raise SystemExit(main())
