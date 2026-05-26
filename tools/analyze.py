#!/usr/bin/env python3
"""Summarize CYD GPIO35 SignalLab CSV captures."""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
from statistics import mean, median


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    pos = (len(ordered) - 1) * pct
    lo = math.floor(pos)
    hi = math.ceil(pos)
    if lo == hi:
        return float(ordered[lo])
    return float(ordered[lo] + (ordered[hi] - ordered[lo]) * (pos - lo))


def load_rows(path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            if row.get("kind") == "siglab":
                rows.append(row)
    return rows


def summarize(path: Path) -> dict:
    rows = load_rows(path)
    raw = [int(row["raw"]) for row in rows if row.get("raw")]
    ranges = [int(row["range"]) for row in rows if row.get("range")]
    statuses = [row.get("status", "") for row in rows]
    rails = [
        row
        for row in rows
        if row.get("rail_low") == "1" or row.get("rail_high") == "1" or row.get("status") == "CLIPPING"
    ]
    steps = [abs(raw[i] - raw[i - 1]) for i in range(1, len(raw))]

    peak_candidates = 0
    if len(raw) >= 3:
        local_range = max(raw) - min(raw)
        threshold = min(raw) + max(20, local_range * 0.55)
        last_peak = -999
        for index in range(1, len(raw) - 1):
            if raw[index] > threshold and raw[index] >= raw[index - 1] and raw[index] > raw[index + 1]:
                if index - last_peak >= 12:
                    peak_candidates += 1
                    last_peak = index

    clipping_percent = (len(rails) / len(rows) * 100.0) if rows else 0.0
    noise_score = percentile(steps, 0.95) if steps else 0.0
    step_change_score = max(steps) if steps else 0
    duration_seconds = 0.0
    if rows:
      duration_seconds = (int(rows[-1]["ms"]) - int(rows[0]["ms"])) / 1000.0

    notes: list[str] = []
    if not rows:
        notes.append("No siglab rows found.")
    elif clipping_percent > 5:
        notes.append("Frequent rail clipping; placement or wiring likely needs adjustment.")
    elif ranges and median(ranges) < 15:
        notes.append("Signal is very flat for most of the capture.")
    elif noise_score > 80:
        notes.append("Large step changes suggest movement, pressure shifts, or a floating input.")
    elif peak_candidates >= max(3, duration_seconds / 3):
        notes.append("Contains repeated peak-like candidates worth later model review.")
    else:
        notes.append("No obvious repeating waveform found by the rough independent scan.")

    return {
        "source": str(path),
        "samples": len(rows),
        "duration_seconds": round(duration_seconds, 3),
        "raw": {
            "min": min(raw) if raw else None,
            "max": max(raw) if raw else None,
            "mean": round(mean(raw), 3) if raw else None,
            "median": median(raw) if raw else None,
            "p05": round(percentile(raw, 0.05), 3) if raw else None,
            "p95": round(percentile(raw, 0.95), 3) if raw else None,
        },
        "range": {
            "median": median(ranges) if ranges else None,
            "p05": round(percentile(ranges, 0.05), 3) if ranges else None,
            "p95": round(percentile(ranges, 0.95), 3) if ranges else None,
        },
        "clipping_percent": round(clipping_percent, 3),
        "noise_step_p95": round(noise_score, 3),
        "max_step_change": step_change_score,
        "peak_candidates": peak_candidates,
        "status_counts": {status: statuses.count(status) for status in sorted(set(statuses))},
        "quality_notes": notes,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    summary = summarize(args.csv_path)
    output = json.dumps(summary, indent=2)
    if args.json_out:
        args.json_out.write_text(output + "\n")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
