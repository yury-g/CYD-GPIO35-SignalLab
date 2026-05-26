#!/usr/bin/env python3
"""Build a visual Markdown index for SignalLab capture folders."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

RATING_BADGES = {
    "best": "![best](https://img.shields.io/badge/rating-best-brightgreen)",
    "maybe": "![maybe](https://img.shields.io/badge/rating-maybe-yellow)",
    "control": "![control](https://img.shields.io/badge/rating-control-blue)",
    "ignore": "![ignore](https://img.shields.io/badge/rating-ignore-lightgrey)",
}


def best_segment(summary: dict[str, Any]) -> str:
    segments = summary.get("candidate_segments") or []
    if not segments:
        return "-"
    segment = segments[0]
    return (
        f"{segment.get('start_ms', 0) / 1000:.1f}-{segment.get('end_ms', 0) / 1000:.1f}s "
        f"score={segment.get('score', '-')} clip={segment.get('clipping_percent', '-')}%"
    )


def load_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text())
    except json.JSONDecodeError:
        return {}


def markdown_table_row(label: str, value: Any) -> str:
    if value is None:
        value = "-"
    if isinstance(value, float):
        value = f"{value:.3f}".rstrip("0").rstrip(".")
    value = str(value).replace("|", "\\|").replace("\n", " ")
    return f"| {label} | {value} |"


def summary_value(summary: dict[str, Any], *keys: str) -> Any:
    for key in keys:
        value: Any = summary
        for part in key.split("."):
            if not isinstance(value, dict) or part not in value:
                value = None
                break
            value = value[part]
        if value is not None:
            return value
    return None


def analysis_summaries(captures_dir: Path) -> dict[str, dict[str, Any]]:
    index = load_json(captures_dir.parent / "analysis" / "capture-index.json")
    summaries: dict[str, dict[str, Any]] = {}
    for summary in index.get("captures", []):
        if isinstance(summary, dict) and summary.get("source"):
            summaries[Path(summary["source"]).parent.name] = summary
    return summaries


def capture_sections(captures_dir: Path) -> list[str]:
    sections: list[str] = []
    ratings = load_json(captures_dir / "ratings.json")
    computed_summaries = analysis_summaries(captures_dir)
    capture_folders = sorted(
        [path for path in captures_dir.iterdir() if path.is_dir() and not path.name.startswith(".")],
        reverse=True,
    )

    for folder in capture_folders:
        raw_csv = folder / "raw.csv"
        preview_svg = folder / "preview.svg"
        meta = load_json(folder / "meta.json")
        exported_summary = load_json(folder / "summary.json")
        summary = computed_summaries.get(folder.name) or exported_summary
        manual_rating = ratings.get(folder.name, {}) if isinstance(ratings.get(folder.name), dict) else {}
        if not raw_csv.exists() and not preview_svg.exists():
            continue

        label = meta.get("label") or folder.name
        notes = meta.get("notes") or ""
        rating_name = summary.get("rating_suggestion") or manual_rating.get("rating") or meta.get("rating")
        rating_badge = RATING_BADGES.get(rating_name, "")
        heading = f"## {folder.name}" + (f" {rating_badge}" if rating_badge else "")
        quality_notes = summary.get("quality_notes", [])
        status_counts = summary.get("status_counts", {})
        raw_min = summary_value(summary, "raw.min", "raw_min")
        raw_max = summary_value(summary, "raw.max", "raw_max")
        raw_p05 = summary_value(summary, "raw.p05", "raw_p05")
        raw_p95 = summary_value(summary, "raw.p95", "raw_p95")
        range_median = summary_value(summary, "range.median", "range_median")
        step_p95 = summary_value(summary, "step.p95", "noise_step_p95")
        warnings = summary.get("warnings") or []

        section = [
            heading,
            "",
            f"![{label}]({folder.name}/preview.svg)" if preview_svg.exists() else "_No preview.svg yet._",
            "",
            "| Field | Value |",
            "| --- | --- |",
            markdown_table_row("Computed rating", rating_name),
            markdown_table_row("Manual rating", manual_rating.get("rating")),
            markdown_table_row("Analysis note", manual_rating.get("note")),
            markdown_table_row("Best segment", best_segment(summary)),
            markdown_table_row("Manual segment", manual_rating.get("candidate_segment")),
            markdown_table_row("Label", label),
            markdown_table_row("Started", meta.get("started_local")),
            markdown_table_row("Mode", meta.get("mode")),
            markdown_table_row("Samples", summary.get("samples")),
            markdown_table_row("Duration seconds", summary.get("duration_seconds")),
            markdown_table_row("Raw min/max", f"{raw_min if raw_min is not None else '-'}/{raw_max if raw_max is not None else '-'}"),
            markdown_table_row("Raw p05/p95", f"{raw_p05 if raw_p05 is not None else '-'}/{raw_p95 if raw_p95 is not None else '-'}"),
            markdown_table_row("Range median", range_median),
            markdown_table_row("Clipping percent", summary.get("clipping_percent")),
            markdown_table_row("Step p95", step_p95),
            markdown_table_row("Peak candidates", summary.get("peak_candidates")),
            markdown_table_row("Status counts", ", ".join(f"{key}: {value}" for key, value in status_counts.items()) or "-"),
            markdown_table_row("Warnings", ", ".join(warnings) or "-"),
            markdown_table_row("Notes", notes or "-"),
            markdown_table_row("Quality notes", " ".join(quality_notes) if quality_notes else "-"),
            "",
            f"[raw.csv]({folder.name}/raw.csv) | [meta.json]({folder.name}/meta.json) | [summary.json]({folder.name}/summary.json)",
            "",
        ]
        sections.append("\n".join(section))

    return sections


def build_readme(captures_dir: Path) -> str:
    sections = capture_sections(captures_dir)
    body = [
        "# SignalLab Capture Gallery",
        "",
        "This gallery is generated by `tools/capture_index.py` so captures can be inspected visually on GitHub.",
        "Each graph is the raw 10-bit GPIO35 waveform from `preview.svg`; programmatic summaries are shown only as helpers.",
        "",
        "Computed ratings come from `tools/analyze_batch.py`; `GOOD WAVEFORM` is treated as a hint, never proof.",
        "Manual notes from [`captures/ratings.json`](ratings.json) and [`docs/initial-capture-analysis.md`](../docs/initial-capture-analysis.md) are preserved when present.",
        "",
        f"{RATING_BADGES['best']} low-clipping connected capture with candidate structure.",
        f"{RATING_BADGES['maybe']} connected capture with caveats or only candidate segments.",
        f"{RATING_BADGES['control']} no-contact/unplugged negative control or false-positive test.",
        f"{RATING_BADGES['ignore']} too clipped/noisy for waveform analysis.",
        "",
        "Regenerate after adding or editing captures:",
        "",
        "```bash",
        "python3 tools/capture_index.py",
        "```",
        "",
    ]

    if sections:
        body.extend(sections)
    else:
        body.extend(
            [
                "## Recorded So Far",
                "",
                "**0 captures.**",
                "",
                "There is no waveform to inspect yet. After a capture, each folder will appear below with its raw waveform graph.",
                "",
                "## Recording Flow",
                "",
                "```mermaid",
                "stateDiagram-v2",
                "    [*] --> Previewing: script open / waveform live",
                "    Previewing --> Recording: tap START",
                "    Recording --> Paused: tap PAUSE",
                "    Paused --> Recording: tap START",
                "    Recording --> Saved: tap STOP",
                "    Paused --> Saved: tap STOP",
                "```",
                "",
                "- **Previewing**: CYD waveform is live, but no sample rows are being saved.",
                "- **Recording**: sample rows are being saved into `raw.csv`.",
                "- **Paused**: waveform is live, but sample rows are not being saved.",
                "- **STOP**: ends the capture, writes files, and refreshes this gallery.",
                "",
                "Run a screen-driven capture from the repo root:",
                "",
                "```bash",
                "python3 tools/capture.py --port /dev/cu.usbserial-10",
                "```",
                "",
                "When the CYD `STOP` button is tapped, this gallery will be refreshed automatically.",
                "",
            ]
        )

    return "\n".join(body) + "\n"


def write_readme(captures_dir: Path) -> Path:
    captures_dir.mkdir(parents=True, exist_ok=True)
    readme_path = captures_dir / "README.md"
    readme_path.write_text(build_readme(captures_dir))
    return readme_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures-dir", type=Path, default=Path("captures"))
    args = parser.parse_args()
    readme_path = write_readme(args.captures_dir)
    print(f"Wrote {readme_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
