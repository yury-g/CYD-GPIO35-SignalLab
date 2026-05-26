# SignalLab Model Packet

Use this packet to orient another model before it reviews raw captures. Treat all data as raw signal-quality evidence only.

## Rating Sets

- best: `20260526-003830_ear_connected`, `20260526-004010_ear_connected`, `20260526-004038_ear_connected`
- maybe: `20260526-003311_finger_connected`, `20260526-003844_ear_connected`, `20260526-004055_ear_connected`
- control: `20260526-003409_none_connected`, `20260526-003429_none_connected`, `20260526-003522_none_unplugged`, `20260526-003538_none_unplugged`, `20260526-003658_none_connected`, `20260526-003725_none_connected`
- ignore: `20260526-003149_finger_connected`

## Key Rules

- Analyze only `kind == siglab` rows.
- Never trust firmware/browser `GOOD WAVEFORM` by itself.
- `none_connected` and `none_unplugged` captures are controls even when waveform-like.
- Prefer low-clipping ear-connected segments for deeper waveform analysis.
- Finger captures in this batch are unstable and need more controlled retesting.

## Compact Capture Metrics

- `20260526-003149_finger_connected`: rating=ignore label=finger_connected duration=30.0s raw=0-1023 clip=94.4% step_p95=242.1 segment=0.0-5.0s score=-259.589 clip=66.805% warnings=SEVERE_CLIPPING,MOVING_DOMINANT,LARGE_STEP_CHANGES,FINGER_PLACEMENT_UNSTABLE
- `20260526-003311_finger_connected`: rating=maybe label=finger_connected duration=30.0s raw=0-799 clip=34.8% step_p95=64.0 segment=10.0-15.0s score=90.805 clip=0.0% warnings=SEVERE_CLIPPING,FINGER_PLACEMENT_UNSTABLE
- `20260526-003409_none_connected`: rating=control label=none_connected duration=10.0s raw=11-1003 clip=0.0% step_p95=34.3 segment=- warnings=CONTROL_FALSE_POSITIVE: 156 GOOD WAVEFORM rows in control capture
- `20260526-003429_none_connected`: rating=control label=none_connected duration=10.0s raw=410-596 clip=0.0% step_p95=23.1 segment=- warnings=CONTROL_FALSE_POSITIVE: 360 GOOD WAVEFORM rows in control capture
- `20260526-003522_none_unplugged`: rating=control label=none_unplugged duration=10.0s raw=0-717 clip=100.0% step_p95=277.9 segment=- warnings=SEVERE_CLIPPING,LARGE_STEP_CHANGES
- `20260526-003538_none_unplugged`: rating=control label=none_unplugged duration=10.0s raw=0-84 clip=100.0% step_p95=53.0 segment=- warnings=SEVERE_CLIPPING
- `20260526-003658_none_connected`: rating=control label=none_connected duration=10.0s raw=420-1023 clip=8.4% step_p95=42.1 segment=- warnings=CONTROL_FALSE_POSITIVE: 318 GOOD WAVEFORM rows in control capture,CLIPPING_WARNING
- `20260526-003725_none_connected`: rating=control label=none_connected duration=10.0s raw=0-679 clip=2.7% step_p95=42.0 segment=- warnings=CONTROL_FALSE_POSITIVE: 345 GOOD WAVEFORM rows in control capture
- `20260526-003830_ear_connected`: rating=best label=ear_connected duration=9.9s raw=398-611 clip=0.0% step_p95=48.1 segment=3.0-8.0s score=87.2 clip=0.0% warnings=none
- `20260526-003844_ear_connected`: rating=maybe label=ear_connected duration=10.0s raw=169-616 clip=0.0% step_p95=49.0 segment=0.0-5.0s score=48.465 clip=0.0% warnings=MOVING_DOMINANT
- `20260526-004010_ear_connected`: rating=best label=ear_connected duration=30.0s raw=300-701 clip=0.0% step_p95=60.1 segment=22.0-27.0s score=95.605 clip=0.0% warnings=none
- `20260526-004038_ear_connected`: rating=best label=ear_connected duration=10.0s raw=337-736 clip=0.0% step_p95=67.1 segment=1.0-6.0s score=83.81 clip=0.0% warnings=none
- `20260526-004055_ear_connected`: rating=maybe label=ear_connected duration=10.0s raw=14-1023 clip=7.9% step_p95=89.3 segment=0.0-5.0s score=53.539 clip=0.0% warnings=CLIPPING_WARNING,MOVING_DOMINANT,LARGE_STEP_CHANGES
