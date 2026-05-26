#!/usr/bin/env python3
"""Interactive serial capture tool for CYD GPIO35 SignalLab."""

from __future__ import annotations

import argparse
import csv
import json
import re
import time
from datetime import datetime
from pathlib import Path

try:
    import serial
except ImportError:  # pragma: no cover
    serial = None

from analyze import summarize


HEADER = ["kind", "ms", "raw", "min", "max", "range", "rail_low", "rail_high", "status", "label"]


def slugify(text: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return slug or "capture"


def ask(prompt: str, default: str | None = None) -> str:
    suffix = f" [{default}]" if default else ""
    value = input(f"{prompt}{suffix}: ").strip()
    return value or (default or "")


def make_preview_svg(rows: list[dict[str, str]], path: Path, title: str) -> None:
    width = 1000
    height = 260
    pad = 34
    points: list[str] = []
    raw = [int(row["raw"]) for row in rows if row.get("kind") == "siglab" and row.get("raw")]
    if raw:
        for index, value in enumerate(raw):
            x = pad + (width - 2 * pad) * index / max(1, len(raw) - 1)
            y = pad + (height - 2 * pad) * (1023 - value) / 1023
            points.append(f"{x:.2f},{y:.2f}")
    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <rect width="100%" height="100%" fill="#0b0f14"/>
  <text x="{pad}" y="22" fill="#f4f7fb" font-family="monospace" font-size="16">{title}</text>
  <g stroke="#263545" stroke-width="1">
    <line x1="{pad}" y1="{pad}" x2="{pad}" y2="{height-pad}"/>
    <line x1="{pad}" y1="{height-pad}" x2="{width-pad}" y2="{height-pad}"/>
    <line x1="{pad}" y1="{pad + (height - 2 * pad) / 2}" x2="{width-pad}" y2="{pad + (height - 2 * pad) / 2}"/>
  </g>
  <polyline fill="none" stroke="#31d4ff" stroke-width="1.5" points="{' '.join(points)}"/>
</svg>
"""
    path.write_text(svg)


def write_csv_row(writer: csv.writer, parts: list[str]) -> bool:
    if not parts:
        return False
    if parts[0] == "siglab" and len(parts) >= 10:
        writer.writerow(parts[:10])
        return True
    if parts[0] == "marker":
        padded = parts[:10] + [""] * max(0, 10 - len(parts))
        writer.writerow(padded[:10])
        return True
    return False


def ensure_pyserial() -> None:
    if serial is not None:
        return
    raise SystemExit("pyserial is required. Install it with: python3 -m pip install pyserial")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbserial-10")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--captures-dir", type=Path, default=Path("captures"))
    args = parser.parse_args()

    ensure_pyserial()

    placement = ask("Placement", "left ear")
    contact = ask("Contact state", "body contact")
    duration = int(ask("Duration seconds (60-300)", "120"))
    if duration < 60 or duration > 300:
        raise SystemExit("Duration must be between 60 and 300 seconds.")
    notes = ask("Notes", "")

    started = datetime.now()
    label = slugify(f"{placement} {contact}")
    folder = args.captures_dir / f"{started:%Y%m%d-%H%M%S}_{label}"
    folder.mkdir(parents=True, exist_ok=False)
    raw_csv = folder / "raw.csv"
    meta_json = folder / "meta.json"
    summary_json = folder / "summary.json"
    preview_svg = folder / "preview.svg"

    meta = {
        "started_local": started.isoformat(timespec="seconds"),
        "port": args.port,
        "baud": args.baud,
        "placement": placement,
        "contact": contact,
        "duration_requested_seconds": duration,
        "notes": notes,
        "label": label,
        "format": HEADER,
    }
    meta_json.write_text(json.dumps(meta, indent=2) + "\n")

    rows: list[dict[str, str]] = []
    print(f"Opening {args.port} at {args.baud} baud...")
    with serial.Serial(args.port, args.baud, timeout=1) as ser, raw_csv.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(HEADER)
        time.sleep(2.0)
        ser.reset_input_buffer()
        ser.write(f"LABEL {label}\n".encode())
        ser.write(b"RESET\n")

        end_at = time.monotonic() + duration
        next_print = 0
        while time.monotonic() < end_at:
            line = ser.readline().decode(errors="replace").strip()
            if not line:
                continue
            parts = line.split(",")
            if write_csv_row(writer, parts):
                row = dict(zip(HEADER, parts[:10] + [""] * max(0, 10 - len(parts))))
                rows.append(row)
            remaining = int(end_at - time.monotonic())
            if remaining <= next_print:
                print(f"{remaining:3d}s remaining, rows={len(rows)}")
                next_print = remaining - 10

    summary = summarize(raw_csv)
    summary_json.write_text(json.dumps(summary, indent=2) + "\n")
    make_preview_svg(rows, preview_svg, f"{label} {started:%Y-%m-%d %H:%M:%S}")

    print(f"Wrote {folder}")
    print(f"Commit with: git add {folder} && git commit -m 'Add {label} capture'")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
