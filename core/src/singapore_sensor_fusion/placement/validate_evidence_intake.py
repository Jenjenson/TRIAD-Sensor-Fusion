"""CLI for strict Stage-0 placement-evidence intake."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Sequence

from .evidence_intake import validate_evidence_intake


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Validate a file-backed placement calibration/precompute evidence package. "
            "Success proves structural consistency only; robust outcome validation, "
            "Stage-1 review readiness, solver use, and physical deployment remain "
            "unauthorized."
        )
    )
    parser.add_argument("manifest", type=Path, help="evidence-intake manifest JSON")
    parser.add_argument(
        "--approved-root",
        required=True,
        type=Path,
        help="operator-approved directory containing every admitted evidence file",
    )
    args = parser.parse_args(argv)
    try:
        receipt = validate_evidence_intake(args.manifest, args.approved_root)
    except (OSError, TypeError, ValueError, json.JSONDecodeError, UnicodeError) as exc:
        print(f"INVALID: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(receipt, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


__all__ = ["main"]
