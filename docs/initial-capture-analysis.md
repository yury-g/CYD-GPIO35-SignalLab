# Initial Capture Analysis

Analysis date: 2026-05-26

This is raw signal-quality analysis only. It does not make medical claims and does not treat BPM, IBI, or firmware waveform labels as trusted physiological measurements.

## Method

I read every `raw.csv`, `meta.json`, `summary.json`, and `preview.svg` under `captures/`.

The math below uses only `kind == siglab` rows from `raw.csv` for capture samples. The raw files contain `event` and `siglab` rows only; there are no `preview` rows in these captures. `preview.svg` was used only to validate that the plotted trace matched the computed raw-signal assessment.

Quality ranking used:

- `best`: connected placement with low clipping and sustained repeated waveform-like structure worth deeper analysis.
- `maybe`: connected placement with useful candidate segments, but material clipping, movement status, or capture instability.
- `control`: no-contact or unplugged negative-control capture, including false-positive-looking traces.
- `ignore`: connected capture too clipped/noisy for waveform analysis, though still useful as a bad-signal example.

The independent scan looked at raw min/max, rail clipping, status counts, 95th-percentile sample-to-sample step size, repeated low-frequency structure after detrending, and 5-second rolling windows. It intentionally does not estimate or report BPM/IBI.

## Capture Table

| Capture | Rating | Label | Duration | Raw range | Clip % | Status counts | Math quality notes |
| --- | --- | --- | ---: | ---: | ---: | --- | --- |
| `20260526-003149_finger_connected` | ignore | finger_connected | 29.995s | 0-1023 (1023) | 94.444 | FLOATING: 80, MOVING: 1349, CLIPPING: 11 | Connected finger but mathematically saturated: rails dominate, huge step jumps, MOVING almost throughout. Useful as a clipping/movement negative, not waveform analysis. |
| `20260526-003311_finger_connected` | maybe | finger_connected | 29.985s | 0-799 (799) | 34.816 | FLOATING: 655, GOOD WAVEFORM: 782, CLIPPING: 2 | Connected finger with a usable early window, but whole capture has heavy clipping/float later; keep only low-clip segment candidates. |
| `20260526-003409_none_connected` | control | none_connected | 9.984s | 11-1003 (992) | 0.000 | FLOATING: 120, GOOD WAVEFORM: 156, MOVING: 203 | No-contact connected control. Large outliers and MOVING block; any structure is a false-positive risk, not PPG evidence. |
| `20260526-003429_none_connected` | control | none_connected | 9.989s | 410-596 (186) | 0.000 | FLOATING: 120, GOOD WAVEFORM: 360 | No-contact connected control. Stable low-amplitude trace that firmware marks GOOD WAVEFORM; valuable specificity control. |
| `20260526-003522_none_unplugged` | control | none_unplugged | 9.981s | 0-717 (717) | 100.000 | NO SENSOR: 479 | Unplugged control with 100% no-sensor/rail behavior and large discontinuities. |
| `20260526-003538_none_unplugged` | control | none_unplugged | 9.990s | 0-84 (84) | 100.000 | NO SENSOR: 480 | Unplugged control with 100% no-sensor/rail behavior; small low-end range but rail classification throughout. |
| `20260526-003658_none_connected` | control | none_connected | 9.983s | 420-1023 (603) | 8.351 | FLOATING: 120, GOOD WAVEFORM: 318, MOVING: 1, CLIPPING: 40 | No-contact connected control with high-end clipping and false GOOD WAVEFORM counts. |
| `20260526-003725_none_connected` | control | none_connected | 9.982s | 0-679 (679) | 2.714 | FLOATING: 120, GOOD WAVEFORM: 345, MOVING: 9, CLIPPING: 5 | No-contact connected control with occasional low rail/clipping; false GOOD WAVEFORM counts. |
| `20260526-003830_ear_connected` | best | ear_connected | 9.986s | 398-611 (213) | 0.000 | FLOATING: 120, GOOD WAVEFORM: 359 | Best short ear candidate: no clipping, moderate range, repeated structure, low jump burden. |
| `20260526-003844_ear_connected` | maybe | ear_connected | 9.984s | 169-616 (447) | 0.000 | FLOATING: 120, GOOD WAVEFORM: 52, MOVING: 307 | Ear candidate with repeated structure but status mostly MOVING; treat as maybe until protocol stabilizes placement/pressure. |
| `20260526-004010_ear_connected` | best | ear_connected | 29.996s | 300-701 (401) | 0.000 | FLOATING: 120, GOOD WAVEFORM: 821, MOVING: 499 | Best long ear candidate: no clipping across about 30 seconds with repeated structure; includes moving-status blocks but strong analyzable spans. |
| `20260526-004038_ear_connected` | best | ear_connected | 9.991s | 337-736 (399) | 0.000 | FLOATING: 120, GOOD WAVEFORM: 255, MOVING: 105 | Best short ear candidate: no clipping and high-amplitude repeated structure; some movement status. |
| `20260526-004055_ear_connected` | maybe | ear_connected | 9.989s | 14-1023 (1009) | 7.917 | FLOATING: 101, MOVING: 377, CLIPPING: 2 | Ear maybe: early/mid span has repeated structure, but late large excursion/rails make full capture unsafe for deeper analysis. |

## Ranking Summary

Best:

- `20260526-004010_ear_connected`
- `20260526-004038_ear_connected`
- `20260526-003830_ear_connected`

Maybe:

- `20260526-003311_finger_connected`
- `20260526-003844_ear_connected`
- `20260526-004055_ear_connected`

Control:

- `20260526-003409_none_connected`
- `20260526-003429_none_connected`
- `20260526-003522_none_unplugged`
- `20260526-003538_none_unplugged`
- `20260526-003658_none_connected`
- `20260526-003725_none_connected`

Ignore for waveform analysis:

- `20260526-003149_finger_connected`

## Placement Comparison

Ear-connected captures are the strongest group. Three of five rank `best`, all have 0% clipping except `20260526-004055_ear_connected`, and the best captures show repeated structure with raw spans of 213 to 401 counts. The main weakness is that the firmware status can call useful-looking spans `MOVING`, so movement status should be treated as a guardrail rather than a final reject.

Finger-connected captures are unstable in this batch. One is almost completely saturated across the 10-bit ADC range and should be ignored for waveform analysis. The other has an early low-clipping segment, but the full capture has 34.816% clipping and many FLOATING rows.

No-contact connected captures are important negative controls. Several are incorrectly labeled `GOOD WAVEFORM` by firmware status counts, including `20260526-003429_none_connected` with 360 GOOD WAVEFORM rows. These should be kept specifically to test false-positive rejection.

Unplugged captures behave as expected negative controls: 100% `NO SENSOR` and 100% clipping by the current summary definition. They should remain in the dataset as wiring/no-sensor controls.

## Candidate Segments

These are candidate raw waveform spans worth deeper algorithm work. They are not trusted physiological measurements.

| Capture | Segment candidates | Reason |
| --- | --- | --- |
| `20260526-004010_ear_connected` | 0.0-29.7s; strongest 10.9-16.1s | Longest low-clipping ear capture; repeated structure persists across rolling windows. |
| `20260526-004038_ear_connected` | 0.0-9.9s; strongest 1.0-6.2s | High-amplitude repeated structure with no rail clipping. |
| `20260526-003830_ear_connected` | 0.0-9.9s; strongest 0.5-5.7s | Clean short ear candidate with no rail clipping and moderate step changes. |
| `20260526-003844_ear_connected` | 0.0-9.9s; strongest 3.6-8.8s | Repeated structure despite mostly MOVING status; good maybe-candidate for movement-tolerant analysis. |
| `20260526-004055_ear_connected` | 0.0-8.8s; avoid late tail | Early/mid repeated structure, but the late excursion and clipping make the whole capture unsafe. |
| `20260526-003311_finger_connected` | 0.0-17.7s; strongest 0.5-5.7s | Best finger candidate segment, but do not use the full capture because later clipping/float dominates. |

Do not use these for candidate PPG waveform analysis:

- `20260526-003149_finger_connected`: severe rail clipping and movement.
- All `none_connected` and `none_unplugged` captures: keep as controls and false-positive tests.

## Protocol Improvements

- Record at least 30 seconds per placement after a short settle period, and separately mark the settle period instead of mixing it into the capture.
- For each placement, capture repeated trials in this order: unplugged, no-contact connected, light contact, normal contact, intentionally moving. This keeps controls near the real captures.
- Add explicit operator notes for pressure, ambient light, contact stability, and whether the sensor LED/board moved.
- Reject or restart captures when rail clipping exceeds a small threshold during the first few seconds.
- For finger captures, reduce pressure and stabilize the sensor mechanically before recording; both finger captures show clipping or floating problems.
- Keep no-contact captures. They are not failures; they expose false positives in waveform-status logic.

## Browser UI Improvements

- Show a live rail/clipping meter and a clear warning when values spend time near 0 or 1023.
- Show separate counts for recording rows versus preview/event rows so analysis cannot accidentally mix them.
- Add a settle countdown, then start recording automatically after the signal has avoided rails for a configurable interval.
- Save operator tags for contact pressure, placement stability, ambient light, and intentional movement.
- Add a per-capture quality panel with raw min/max, p05/p95, clipping percent, step-jump p95, and status counts.
- Add a control-capture mode that labels no-contact and unplugged captures as negative controls and prevents them from being interpreted as candidate PPG.
- Flag potential false positives when a no-contact label receives many `GOOD WAVEFORM` rows.
