#!/usr/bin/env python3
"""Summarize the evidence registry for the historical impostor challenge.

Research-only. The registry deliberately stores unavailable IR as null; this
tool never substitutes PWM or elapsed time for measured wheel distance.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path


def summarize(registry: dict) -> dict:
    rows = registry["records"]
    categories = Counter(row["category"] for row in rows)
    full_model = Counter(row["full_model_result"] for row in rows)
    available_survivors = [
        row["id"]
        for row in rows
        if row["category"] == "B"
        and row["proposed_non_ir_result"] == "survives available screens"
    ]
    available_rejections = [
        row["id"]
        for row in rows
        if row["category"] == "B"
        and row["proposed_non_ir_result"] == "reject"
    ]
    genuine_conditional_accepts = [
        row["id"]
        for row in rows
        if row["category"] in {"A", "C"}
        and row["proposed_non_ir_result"] == "accept"
    ]
    missing_ir = [row["id"] for row in rows if row["ir_distance_mm"] is None]
    return {
        "registry_rows": len(rows),
        "categories": dict(sorted(categories.items())),
        "full_model_results": dict(sorted(full_model.items())),
        "non_mm_survivors_before_ir": available_survivors,
        "non_mm_rejections_before_ir": available_rejections,
        "genuine_conditional_accepts": genuine_conditional_accepts,
        "rows_without_ir_distance": len(missing_ir),
        "complete_ir_testable_rows": len(rows) - len(missing_ir),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "registry",
        nargs="?",
        default="field-records/analysis/20260928_navi_eyes_wide_open_historical_impostor_registry.json",
    )
    args = parser.parse_args()
    registry = json.loads(Path(args.registry).read_text(encoding="utf-8"))
    print(json.dumps(summarize(registry), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
