#!/usr/bin/env python3
"""Analyze every SignalLab capture and write reusable reports."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

try:
    from analyze import summarize
except ImportError:  # pragma: no cover
    from tools.analyze import summarize


RATING_ORDER = {"best": 0, "maybe": 1, "control": 2, "ignore": 3}


def capture_name(summary: dict[str, Any]) -> str:
    return Path(summary["source"]).parent.name


def raw_range(summary: dict[str, Any]) -> str:
    raw = summary.get("raw", {})
    return f"{raw.get('min')}-{raw.get('max')}"


def segment_text(segment: dict[str, Any] | None) -> str:
    if not segment:
        return "-"
    return (
        f"{segment['start_ms'] / 1000:.1f}-{segment['end_ms'] / 1000:.1f}s "
        f"score={segment['score']} clip={segment['clipping_percent']}%"
    )


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n")


def markdown_report(summaries: list[dict[str, Any]]) -> str:
    ratings = Counter(summary["rating_suggestion"] for summary in summaries)
    warnings = Counter(warning for summary in summaries for warning in summary.get("warnings", []))

    lines = [
        "# SignalLab Batch Capture Report",
        "",
        "Generated from `tools/analyze_batch.py`. This is raw GPIO35 signal-quality analysis only; it makes no BPM, IBI, pulse, medical, or physiological claims.",
        "",
        "## Overview",
        "",
        f"- Captures analyzed: {len(summaries)}",
        f"- Ratings: {', '.join(f'{key}={ratings[key]}' for key in sorted(ratings, key=lambda item: RATING_ORDER.get(item, 99)))}",
        f"- Warnings: {', '.join(f'{key}={value}' for key, value in sorted(warnings.items())) or 'none'}",
        "",
        "## Capture Table",
        "",
        "| Capture | Rating | Label | Duration | Raw range | Clip % | Step p95 | Best segment | Warnings |",
        "| --- | --- | --- | ---: | ---: | ---: | ---: | --- | --- |",
    ]
    for summary in sorted(summaries, key=lambda item: (RATING_ORDER.get(item["rating_suggestion"], 99), capture_name(item))):
        best_segment = summary.get("candidate_segments", [None])[0] if summary.get("candidate_segments") else None
        lines.append(
            "| "
            + " | ".join(
                [
                    f"`{capture_name(summary)}`",
                    summary["rating_suggestion"],
                    summary["label"],
                    f"{summary['duration_seconds']:.3f}s",
                    raw_range(summary),
                    f"{summary['clipping_percent']:.3f}",
                    f"{summary['step']['p95']:.3f}",
                    segment_text(best_segment),
                    ", ".join(summary.get("warnings", [])) or "-",
                ]
            )
            + " |"
        )

    lines.extend(
        [
            "",
            "## Best Candidate Segments",
            "",
        ]
    )
    for summary in [item for item in summaries if item["rating_suggestion"] in {"best", "maybe"}]:
        best_segment = summary.get("candidate_segments", [None])[0] if summary.get("candidate_segments") else None
        lines.append(f"- `{capture_name(summary)}` ({summary['rating_suggestion']}): {segment_text(best_segment)}")

    lines.extend(
        [
            "",
            "## Control False-Positive Warnings",
            "",
        ]
    )
    controls = [item for item in summaries if item.get("is_control")]
    for summary in controls:
        false_warning = [warning for warning in summary.get("warnings", []) if warning.startswith("CONTROL_FALSE_POSITIVE")]
        if false_warning:
            lines.append(f"- `{capture_name(summary)}`: {'; '.join(false_warning)}")
    if not any([warning for summary in controls for warning in summary.get("warnings", []) if warning.startswith("CONTROL_FALSE_POSITIVE")]):
        lines.append("- None detected.")

    return "\n".join(lines) + "\n"


def model_packet(summaries: list[dict[str, Any]]) -> str:
    lines = [
        "# SignalLab Model Packet",
        "",
        "Use this packet to orient another model before it reviews raw captures. Treat all data as raw signal-quality evidence only.",
        "",
        "## Rating Sets",
        "",
    ]
    for rating in ["best", "maybe", "control", "ignore"]:
        names = [capture_name(summary) for summary in summaries if summary["rating_suggestion"] == rating]
        lines.append(f"- {rating}: {', '.join(f'`{name}`' for name in names) or 'none'}")
    lines.extend(
        [
            "",
            "## Key Rules",
            "",
            "- Analyze only `kind == siglab` rows.",
            "- Never trust firmware/browser `GOOD WAVEFORM` by itself.",
            "- `none_connected` and `none_unplugged` captures are controls even when waveform-like.",
            "- Prefer low-clipping ear-connected segments for deeper waveform analysis.",
            "- Finger captures in this batch are unstable and need more controlled retesting.",
            "",
            "## Compact Capture Metrics",
            "",
        ]
    )
    for summary in sorted(summaries, key=lambda item: capture_name(item)):
        best_segment = summary.get("candidate_segments", [None])[0] if summary.get("candidate_segments") else None
        lines.append(
            f"- `{capture_name(summary)}`: rating={summary['rating_suggestion']} "
            f"label={summary['label']} duration={summary['duration_seconds']:.1f}s "
            f"raw={raw_range(summary)} clip={summary['clipping_percent']:.1f}% "
            f"step_p95={summary['step']['p95']:.1f} segment={segment_text(best_segment)} "
            f"warnings={','.join(summary.get('warnings', [])) or 'none'}"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures-dir", type=Path, default=Path("captures"))
    parser.add_argument("--analysis-dir", type=Path, default=Path("analysis"))
    parser.add_argument(
        "--write-capture-summaries",
        action="store_true",
        help="Also replace each capture folder's summary.json with the analyzer output.",
    )
    args = parser.parse_args()

    raw_paths = sorted(args.captures_dir.glob("*/raw.csv"))
    summaries: list[dict[str, Any]] = []
    for raw_path in raw_paths:
        summary = summarize(raw_path)
        summaries.append(summary)
        if args.write_capture_summaries:
            write_json(raw_path.with_name("summary.json"), summary)

    index = {
        "captures_dir": str(args.captures_dir),
        "capture_count": len(summaries),
        "ratings": dict(Counter(summary["rating_suggestion"] for summary in summaries)),
        "captures": summaries,
    }
    write_json(args.analysis_dir / "capture-index.json", index)
    (args.analysis_dir / "capture-report.md").write_text(markdown_report(summaries))
    (args.analysis_dir / "model-packet.md").write_text(model_packet(summaries))
    print(f"Analyzed {len(summaries)} captures into {args.analysis_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
