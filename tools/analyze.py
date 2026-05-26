#!/usr/bin/env python3
"""Summarize CYD GPIO35 SignalLab CSV captures.

This analyzer intentionally treats firmware/browser status labels as hints, not
proof. Raw `kind == siglab` rows are the source of truth; event/marker rows are
ignored for metrics.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from collections import Counter
from pathlib import Path
from statistics import mean, median
from typing import Any


CONTROL_LABEL_PARTS = ("none_connected", "none_unplugged", "no_contact", "unplugged")
SAMPLE_RATE_HZ = 50
SEGMENT_WINDOW_MS = 5000
SEGMENT_STRIDE_MS = 1000


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


def safe_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def load_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text())
    except json.JSONDecodeError:
        return {}


def load_rows(path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            if row.get("kind") == "siglab":
                rows.append(row)
    return rows


def infer_label(path: Path, meta: dict[str, Any]) -> str:
    label = str(meta.get("label") or "").strip()
    if label:
        return label
    return path.parent.name.split("_", 1)[-1] if "_" in path.parent.name else path.parent.name


def is_control_capture(label: str, meta: dict[str, Any]) -> bool:
    normalized = " ".join(
        str(value).lower().replace("-", "_")
        for value in (
            label,
            meta.get("placement", ""),
            meta.get("contact", ""),
            meta.get("connected", ""),
            meta.get("notes", ""),
        )
    )
    return any(part in normalized for part in CONTROL_LABEL_PARTS)


def clipping_rows(rows: list[dict[str, str]]) -> int:
    clipped = 0
    for row in rows:
        raw = safe_int(row.get("raw"))
        if (
            row.get("rail_low") == "1"
            or row.get("rail_high") == "1"
            or row.get("status") == "CLIPPING"
            or raw <= 8
            or raw >= 1015
        ):
            clipped += 1
    return clipped


def rough_peak_candidates(raw: list[int]) -> int:
    if len(raw) < 8:
        return 0
    span = max(raw) - min(raw)
    if span < 60:
        return 0
    center = median(raw)
    threshold = center + max(12, span * 0.18)
    min_spacing = max(12, int(SAMPLE_RATE_HZ * 0.28))
    count = 0
    last_peak = -999
    for index in range(2, len(raw) - 2):
        value = raw[index]
        if value < threshold:
            continue
        if value >= raw[index - 1] and value > raw[index + 1] and value >= raw[index - 2] and value > raw[index + 2]:
            if index - last_peak >= min_spacing:
                count += 1
                last_peak = index
    return count


def status_counts(rows: list[dict[str, str]]) -> dict[str, int]:
    counts = Counter(row.get("status") or "UNKNOWN" for row in rows)
    return {key: counts[key] for key in sorted(counts)}


def segment_summary(rows: list[dict[str, str]], start_ms: int, end_ms: int) -> dict[str, Any]:
    segment = [row for row in rows if start_ms <= safe_int(row.get("ms")) <= end_ms]
    raw = [safe_int(row.get("raw")) for row in segment]
    steps = [abs(raw[i] - raw[i - 1]) for i in range(1, len(raw))]
    clip_pct = (clipping_rows(segment) / len(segment) * 100.0) if segment else 0.0
    counts = status_counts(segment)
    moving_pct = (counts.get("MOVING", 0) / len(segment) * 100.0) if segment else 0.0
    spread = percentile(raw, 0.95) - percentile(raw, 0.05) if raw else 0.0
    peaks = rough_peak_candidates(raw)
    score = 0.0
    if segment:
        score += min(spread, 500) / 10
        score += peaks * 8
        score -= clip_pct * 4
        score -= moving_pct * 0.45
        score -= max(0.0, percentile(steps, 0.95) - 70) * 0.4 if steps else 0
    return {
        "start_ms": start_ms,
        "end_ms": end_ms,
        "duration_seconds": round((end_ms - start_ms) / 1000.0, 3),
        "samples": len(segment),
        "raw_min": min(raw) if raw else None,
        "raw_max": max(raw) if raw else None,
        "raw_p05": round(percentile(raw, 0.05), 3) if raw else None,
        "raw_p95": round(percentile(raw, 0.95), 3) if raw else None,
        "raw_p05_p95_spread": round(spread, 3),
        "clipping_percent": round(clip_pct, 3),
        "step_p95": round(percentile(steps, 0.95), 3) if steps else 0.0,
        "status_counts": counts,
        "peak_candidates": peaks,
        "score": round(score, 3),
    }


def candidate_segments(rows: list[dict[str, str]], control: bool) -> list[dict[str, Any]]:
    if control or not rows:
        return []
    first_ms = safe_int(rows[0].get("ms"))
    last_ms = safe_int(rows[-1].get("ms"))
    if last_ms - first_ms < 1000:
        return []
    segments: list[dict[str, Any]] = []
    start = first_ms
    while start + SEGMENT_WINDOW_MS <= last_ms + 1:
        segment = segment_summary(rows, start, start + SEGMENT_WINDOW_MS)
        if segment["samples"] >= SAMPLE_RATE_HZ * 3:
            segments.append(segment)
        start += SEGMENT_STRIDE_MS
    segments.sort(key=lambda item: item["score"], reverse=True)
    return segments[:5]


def warning_notes(label: str, control: bool, clip_pct: float, counts: dict[str, int], samples: int, step_p95: float) -> list[str]:
    warnings: list[str] = []
    good_rows = counts.get("GOOD WAVEFORM", 0)
    moving_rows = counts.get("MOVING", 0)
    if control and good_rows:
        warnings.append(f"CONTROL_FALSE_POSITIVE: {good_rows} GOOD WAVEFORM rows in control capture")
    if clip_pct > 20:
        warnings.append("SEVERE_CLIPPING")
    elif clip_pct > 5:
        warnings.append("CLIPPING_WARNING")
    if samples and moving_rows / samples > 0.5:
        warnings.append("MOVING_DOMINANT")
    if step_p95 > 80:
        warnings.append("LARGE_STEP_CHANGES")
    if "finger" in label.lower() and clip_pct > 5:
        warnings.append("FINGER_PLACEMENT_UNSTABLE")
    return warnings


def rating_suggestion(label: str, control: bool, clip_pct: float, counts: dict[str, int], samples: int, candidates: list[dict[str, Any]]) -> str:
    if control:
        return "control"
    if clip_pct > 80:
        return "ignore"
    moving_fraction = (counts.get("MOVING", 0) / samples) if samples else 0.0
    if clip_pct > 5 or moving_fraction > 0.5:
        return "maybe"
    if candidates and candidates[0]["score"] >= 25:
        return "best"
    return "maybe"


def summarize(path: Path, meta_path: Path | None = None) -> dict[str, Any]:
    meta_path = meta_path or path.with_name("meta.json")
    meta = load_json(meta_path)
    rows = load_rows(path)
    label = infer_label(path, meta)
    control = is_control_capture(label, meta)
    raw = [safe_int(row.get("raw")) for row in rows if row.get("raw") not in (None, "")]
    ranges = [safe_int(row.get("range")) for row in rows if row.get("range") not in (None, "")]
    counts = status_counts(rows)
    rails = clipping_rows(rows)
    steps = [abs(raw[i] - raw[i - 1]) for i in range(1, len(raw))]
    clip_pct = (rails / len(rows) * 100.0) if rows else 0.0
    duration_seconds = (safe_int(rows[-1].get("ms")) - safe_int(rows[0].get("ms"))) / 1000.0 if rows else 0.0
    candidates = candidate_segments(rows, control)
    step_p95 = percentile(steps, 0.95) if steps else 0.0
    warnings = warning_notes(label, control, clip_pct, counts, len(rows), step_p95)
    rating = rating_suggestion(label, control, clip_pct, counts, len(rows), candidates)

    notes: list[str] = []
    if not rows:
        notes.append("No siglab rows found.")
    elif control:
        notes.append("Negative-control capture; do not treat waveform-like structure as PPG evidence.")
    elif clip_pct > 20:
        notes.append("Frequent rail clipping; use only clearly low-clipping segments if any.")
    elif candidates:
        notes.append("Contains low-clipping candidate segments worth later model review.")
    else:
        notes.append("No strong low-clipping candidate segment found by the current scan.")

    return {
        "source": str(path),
        "label": label,
        "is_control": control,
        "rating_suggestion": rating,
        "samples": len(rows),
        "duration_seconds": round(duration_seconds, 3),
        "raw": {
            "min": min(raw) if raw else None,
            "max": max(raw) if raw else None,
            "span": (max(raw) - min(raw)) if raw else None,
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
        "clipping_percent": round(clip_pct, 3),
        "step": {
            "p50": round(percentile(steps, 0.50), 3) if steps else 0.0,
            "p95": round(step_p95, 3),
            "max": max(steps) if steps else 0,
        },
        "peak_candidates": rough_peak_candidates(raw),
        "status_counts": counts,
        "warnings": warnings,
        "candidate_segments": candidates,
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
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(output + "\n")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
