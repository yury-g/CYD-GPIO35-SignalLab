# SignalLab Batch Capture Report

Generated from `tools/analyze_batch.py`. This is raw GPIO35 signal-quality analysis only; it makes no BPM, IBI, pulse, medical, or physiological claims.

## Overview

- Captures analyzed: 13
- Ratings: best=3, maybe=3, control=6, ignore=1
- Warnings: CLIPPING_WARNING=2, CONTROL_FALSE_POSITIVE: 156 GOOD WAVEFORM rows in control capture=1, CONTROL_FALSE_POSITIVE: 318 GOOD WAVEFORM rows in control capture=1, CONTROL_FALSE_POSITIVE: 345 GOOD WAVEFORM rows in control capture=1, CONTROL_FALSE_POSITIVE: 360 GOOD WAVEFORM rows in control capture=1, FINGER_PLACEMENT_UNSTABLE=2, LARGE_STEP_CHANGES=3, MOVING_DOMINANT=3, SEVERE_CLIPPING=4

## Capture Table

| Capture | Rating | Label | Duration | Raw range | Clip % | Step p95 | Best segment | Warnings |
| --- | --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| `20260526-003830_ear_connected` | best | ear_connected | 9.950s | 398-611 | 0.000 | 48.150 | 3.0-8.0s score=87.2 clip=0.0% | - |
| `20260526-004010_ear_connected` | best | ear_connected | 29.976s | 300-701 | 0.000 | 60.100 | 22.0-27.0s score=95.605 clip=0.0% | - |
| `20260526-004038_ear_connected` | best | ear_connected | 9.971s | 337-736 | 0.000 | 67.100 | 1.0-6.0s score=83.81 clip=0.0% | - |
| `20260526-003311_finger_connected` | maybe | finger_connected | 29.965s | 0-799 | 34.816 | 64.000 | 10.0-15.0s score=90.805 clip=0.0% | SEVERE_CLIPPING, FINGER_PLACEMENT_UNSTABLE |
| `20260526-003844_ear_connected` | maybe | ear_connected | 9.964s | 169-616 | 0.000 | 49.000 | 0.0-5.0s score=48.465 clip=0.0% | MOVING_DOMINANT |
| `20260526-004055_ear_connected` | maybe | ear_connected | 9.969s | 14-1023 | 7.917 | 89.300 | 0.0-5.0s score=53.539 clip=0.0% | CLIPPING_WARNING, MOVING_DOMINANT, LARGE_STEP_CHANGES |
| `20260526-003409_none_connected` | control | none_connected | 9.964s | 11-1003 | 0.000 | 34.300 | - | CONTROL_FALSE_POSITIVE: 156 GOOD WAVEFORM rows in control capture |
| `20260526-003429_none_connected` | control | none_connected | 9.969s | 410-596 | 0.000 | 23.100 | - | CONTROL_FALSE_POSITIVE: 360 GOOD WAVEFORM rows in control capture |
| `20260526-003522_none_unplugged` | control | none_unplugged | 9.961s | 0-717 | 100.000 | 277.900 | - | SEVERE_CLIPPING, LARGE_STEP_CHANGES |
| `20260526-003538_none_unplugged` | control | none_unplugged | 9.970s | 0-84 | 100.000 | 53.000 | - | SEVERE_CLIPPING |
| `20260526-003658_none_connected` | control | none_connected | 9.963s | 420-1023 | 8.351 | 42.150 | - | CONTROL_FALSE_POSITIVE: 318 GOOD WAVEFORM rows in control capture, CLIPPING_WARNING |
| `20260526-003725_none_connected` | control | none_connected | 9.962s | 0-679 | 2.714 | 42.000 | - | CONTROL_FALSE_POSITIVE: 345 GOOD WAVEFORM rows in control capture |
| `20260526-003149_finger_connected` | ignore | finger_connected | 29.975s | 0-1023 | 94.444 | 242.100 | 0.0-5.0s score=-259.589 clip=66.805% | SEVERE_CLIPPING, MOVING_DOMINANT, LARGE_STEP_CHANGES, FINGER_PLACEMENT_UNSTABLE |

## Best Candidate Segments

- `20260526-003311_finger_connected` (maybe): 10.0-15.0s score=90.805 clip=0.0%
- `20260526-003830_ear_connected` (best): 3.0-8.0s score=87.2 clip=0.0%
- `20260526-003844_ear_connected` (maybe): 0.0-5.0s score=48.465 clip=0.0%
- `20260526-004010_ear_connected` (best): 22.0-27.0s score=95.605 clip=0.0%
- `20260526-004038_ear_connected` (best): 1.0-6.0s score=83.81 clip=0.0%
- `20260526-004055_ear_connected` (maybe): 0.0-5.0s score=53.539 clip=0.0%

## Control False-Positive Warnings

- `20260526-003409_none_connected`: CONTROL_FALSE_POSITIVE: 156 GOOD WAVEFORM rows in control capture
- `20260526-003429_none_connected`: CONTROL_FALSE_POSITIVE: 360 GOOD WAVEFORM rows in control capture
- `20260526-003658_none_connected`: CONTROL_FALSE_POSITIVE: 318 GOOD WAVEFORM rows in control capture
- `20260526-003725_none_connected`: CONTROL_FALSE_POSITIVE: 345 GOOD WAVEFORM rows in control capture
