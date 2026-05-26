#!/usr/bin/env python3
"""Serial capture tool for CYD GPIO35 SignalLab."""

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

try:
    from analyze import summarize
except ImportError:  # pragma: no cover
    from tools.analyze import summarize


HEADER = [
    "kind",
    "ms",
    "raw",
    "min",
    "max",
    "range",
    "rail_low",
    "rail_high",
    "status",
    "label",
    "placement",
    "connected",
    "recording",
    "event",
    "detail",
]


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


def normalize_row(parts: list[str]) -> dict[str, str] | None:
    if not parts:
        return None
    if parts[0] == "header":
        return None
    if parts[0] == "siglab" and len(parts) >= 10:
        padded = parts[: len(HEADER)] + [""] * max(0, len(HEADER) - len(parts))
        return dict(zip(HEADER, padded[: len(HEADER)]))
    if parts[0] in {"event", "marker"}:
        padded = parts[: len(HEADER)] + [""] * max(0, len(HEADER) - len(parts))
        return dict(zip(HEADER, padded[: len(HEADER)]))
    return None


def ensure_pyserial() -> None:
    if serial is not None:
        return
    raise SystemExit("pyserial is required. Install it with: python3 -m pip install pyserial")


def make_capture_folder(captures_dir: Path, slug: str) -> Path:
    started = datetime.now()
    folder = captures_dir / f"{started:%Y%m%d-%H%M%S}_{slugify(slug)}"
    folder.mkdir(parents=True, exist_ok=False)
    return folder


def write_artifacts(folder: Path, rows: list[dict[str, str]], meta: dict) -> None:
    raw_csv = folder / "raw.csv"
    meta_json = folder / "meta.json"
    summary_json = folder / "summary.json"
    preview_svg = folder / "preview.svg"

    with raw_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=HEADER)
        writer.writeheader()
        writer.writerows(rows)

    meta_json.write_text(json.dumps(meta, indent=2) + "\n")
    summary = summarize(raw_csv)
    summary_json.write_text(json.dumps(summary, indent=2) + "\n")
    make_preview_svg(rows, preview_svg, f"{meta['label']} {meta['started_local']}")


def screen_driven_capture(args: argparse.Namespace) -> Path:
    started = datetime.now()
    notes = ask("Session notes", "")
    folder = make_capture_folder(args.captures_dir, "screen-driven")
    rows: list[dict[str, str]] = []
    saw_start = False
    last_label = "screen-driven"

    print(f"Opening {args.port} at {args.baud} baud...")
    print("Use the CYD buttons: choose Finger/Ear/Connected, tap START, optionally PAUSE, then STOP.")

    try:
        with serial.Serial(args.port, args.baud, timeout=1) as ser:
            time.sleep(1.0)
            ser.reset_input_buffer()
            ser.write(b"VERSION\n")
            while True:
                line = ser.readline().decode(errors="replace").strip()
                if not line:
                    continue
                if line.startswith("version,"):
                    print(line)
                    continue
                row = normalize_row(line.split(","))
                if row is None:
                    continue
                rows.append(row)
                if row.get("label"):
                    last_label = row["label"]
                if row.get("kind") == "event":
                    event = row.get("event")
                    detail = row.get("detail")
                    print(
                        f"event={event or '-'} detail={detail or '-'} "
                        f"label={row.get('label') or '-'} ms={row.get('ms') or '-'}"
                    )
                    if event == "start":
                        saw_start = True
                    if event == "stop" and saw_start:
                        break
                elif row.get("kind") == "siglab" and len(rows) % 250 == 0:
                    print(f"rows={sum(1 for item in rows if item.get('kind') == 'siglab')} label={last_label}")
    except KeyboardInterrupt:
        print("\nInterrupted; writing rows captured so far.")

    meta = {
        "started_local": started.isoformat(timespec="seconds"),
        "ended_local": datetime.now().isoformat(timespec="seconds"),
        "port": args.port,
        "baud": args.baud,
        "mode": "screen-driven",
        "notes": notes,
        "label": last_label,
        "format": HEADER,
        "operator_source": "cyd_touchscreen",
    }
    write_artifacts(folder, rows, meta)
    return folder


def timed_capture(args: argparse.Namespace) -> Path:
    placement = ask("Placement", "left ear")
    contact = ask("Contact state", "body contact")
    duration = int(ask("Duration seconds (60-300)", str(args.duration)))
    if duration < 60 or duration > 300:
        raise SystemExit("Duration must be between 60 and 300 seconds.")
    notes = ask("Notes", "")

    started = datetime.now()
    label = slugify(f"{placement} {contact}")
    folder = make_capture_folder(args.captures_dir, label)
    rows: list[dict[str, str]] = []

    print(f"Opening {args.port} at {args.baud} baud...")
    with serial.Serial(args.port, args.baud, timeout=1) as ser:
        time.sleep(1.0)
        ser.reset_input_buffer()
        ser.write(b"RESET\n")
        if placement.lower().startswith("finger"):
            ser.write(b"FINGER\n")
        elif "ear" in placement.lower():
            ser.write(b"EAR\n")
        else:
            ser.write(b"NONE\n")
        ser.write(b"CONNECTED\n" if "no" not in contact.lower() and "unplug" not in contact.lower() else b"UNPLUGGED\n")
        ser.write(b"START\n")

        end_at = time.monotonic() + duration
        next_print = duration
        while time.monotonic() < end_at:
            line = ser.readline().decode(errors="replace").strip()
            row = normalize_row(line.split(","))
            if row:
                rows.append(row)
            remaining = int(end_at - time.monotonic())
            if remaining <= next_print - 10:
                print(f"{remaining:3d}s remaining, samples={sum(1 for item in rows if item.get('kind') == 'siglab')}")
                next_print = remaining
        ser.write(b"STOP\n")
        stop_deadline = time.monotonic() + 2
        while time.monotonic() < stop_deadline:
            row = normalize_row(ser.readline().decode(errors="replace").strip().split(","))
            if row:
                rows.append(row)
                if row.get("event") == "stop":
                    break

    meta = {
        "started_local": started.isoformat(timespec="seconds"),
        "ended_local": datetime.now().isoformat(timespec="seconds"),
        "port": args.port,
        "baud": args.baud,
        "mode": "timed-host-commanded",
        "placement": placement,
        "contact": contact,
        "duration_requested_seconds": duration,
        "notes": notes,
        "label": label,
        "format": HEADER,
    }
    write_artifacts(folder, rows, meta)
    return folder


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbserial-10")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--captures-dir", type=Path, default=Path("captures"))
    parser.add_argument("--mode", choices=["screen", "timed"], default="screen")
    parser.add_argument("--duration", type=int, default=120)
    args = parser.parse_args()

    ensure_pyserial()

    if args.mode == "screen":
        folder = screen_driven_capture(args)
    else:
        folder = timed_capture(args)

    print(f"Wrote {folder}")
    print(f"Commit with: git add {folder} && git commit -m 'Add {folder.name} capture'")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
