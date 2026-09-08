"""Command-line validator for the Stage-0 placement study contract."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Sequence

from .study_contract import Stage0StudyContract


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Validate a planning-only triad.sensor_placement_stage0_study_contract.v1 file. "
            "This does not run or authorize a placement optimizer."
        )
    )
    parser.add_argument("contract", type=Path, help="Stage-0 contract JSON")
    args = parser.parse_args(argv)
    try:
        contract = Stage0StudyContract.load(args.contract)
    except (OSError, TypeError, ValueError, json.JSONDecodeError) as exc:
        print(f"INVALID: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(contract.validation_receipt(), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
