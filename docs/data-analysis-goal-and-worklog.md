# Data Analysis Goal And Worklog

This file is the starting brief for future analysis passes by humans or models. Read it before changing analysis code, interpreting captures, or adding new conclusions.

## Goal

Use the raw `captures/` dataset to learn how to distinguish useful PulseSensor PPG-like waveform captures from controls, clipping, motion, floating-input behavior, and other noise.

The immediate objective is signal quality, not physiology:

- Identify which captures and segments are worth deeper waveform analysis.
- Identify which captures are negative controls or false-positive traps.
- Improve capture protocol and browser UI so future data is cleaner and better labeled.
- Preserve notes about what worked, what failed, and what should be tried next.

Do not report trusted BPM, IBI, pulse, diagnosis, health status, or medical conclusions from this dataset.

## Data Rules

- Treat `raw.csv` as the source of truth.
- Analyze only `kind == siglab` rows as recorded capture samples.
- Do not treat live-preview rows as captures. If a future raw file includes `kind == preview`, exclude those rows from capture metrics.
- Use `meta.json` for labels, placement, connected/unplugged state, timestamps, and operator notes.
- Use `summary.json` as a convenience summary, but verify important claims from `raw.csv`.
- Use `preview.svg` only to validate that math-based conclusions match the plotted trace. Do not classify by image inspection alone.
- Do not modify raw data files in `captures/*/raw.csv`.

## Current Dataset Context

Firmware streams 10-bit ADC values from `0..1023` at about 50 Hz from GPIO35. Labels such as `ear_connected`, `finger_connected`, `none_connected`, and `none_unplugged` come from browser controls and should be treated as operator labels, not proof of signal quality.

Important negative controls are intentionally present:

- `none_connected`: sensor connected but not touching the body.
- `none_unplugged`: nothing connected / unplugged state.
- bad connected captures with clipping, movement, or floating behavior.

These controls are valuable because simple waveform-looking or firmware `GOOD WAVEFORM` status can produce false positives.

## Current Baseline

The first full-pass report is:

- [`docs/initial-capture-analysis.md`](initial-capture-analysis.md)

Current rough rankings from that pass:

- `best`: `20260526-004010_ear_connected`, `20260526-004038_ear_connected`, `20260526-003830_ear_connected`
- `maybe`: `20260526-003311_finger_connected`, `20260526-003844_ear_connected`, `20260526-004055_ear_connected`
- `control`: all `none_connected` and `none_unplugged` captures
- `ignore`: `20260526-003149_finger_connected`

These ratings are signal-quality labels only. They should be revised when better methods or more captures are added.

## Suggested Analysis Workflow

1. Enumerate every capture directory under `captures/`.
2. Read `raw.csv`, `meta.json`, `summary.json`, and `preview.svg` for each capture.
3. Compute metrics from `kind == siglab` rows:
   - sample count and duration
   - raw min/max, p05/p95, median, and range
   - rail/clipping percent
   - status counts
   - sample-to-sample step/jump percentiles
   - flatness, drift, and repeated low-frequency structure
   - candidate low-clipping segments
4. Compare connected placements against no-contact and unplugged controls.
5. Separate candidate waveform detection from control rejection.
6. Record results, failed methods, and next experiments in this file or in a linked dated report.

## Attempt Log

Add new entries at the top of this section.

### 2026-05-26 - Initial Math-First Capture Triage

Files:

- [`docs/initial-capture-analysis.md`](initial-capture-analysis.md)
- [`captures/README.md`](../captures/README.md)

Worked:

- Reading only `siglab` rows produced consistent durations and metrics.
- Rail/clipping percent quickly separated unplugged controls and severely bad captures.
- No-contact controls exposed false positives in firmware `GOOD WAVEFORM` status.
- Ear captures were clearly stronger than this batch's finger captures.
- Rolling 5-second windows helped identify useful segments inside otherwise mixed-quality captures.

Failed or risky:

- Firmware `GOOD WAVEFORM` status is not specific enough; no-contact controls can receive many GOOD WAVEFORM rows.
- Shape/periodicity alone can be fooled by no-contact and noise traces.
- Full-capture ratings hide useful subsegments and bad tails.
- Visual SVG inspection alone is too subjective; it should remain a validation step only.

Next:

- Build or improve an analysis script that outputs reusable per-capture and per-segment quality metrics.
- Add stricter false-positive checks using `none_connected` and `none_unplugged` captures.
- Capture more repeated 30-second trials per placement with explicit settle periods and operator notes.
- Add browser UI warnings for clipping, floating, and no-contact false positives.

## Update Template

Copy this block for future attempts:

```markdown
### YYYY-MM-DD - Short Attempt Name

Files:

- `path/to/file`

Question:

- What did this attempt try to answer?

Worked:

- What produced useful signal-quality separation?

Failed or risky:

- What produced false positives, false negatives, unstable results, or confusing output?

Evidence:

- Which captures or segments support the conclusion?

Next:

- What should the next analysis pass try?
```
