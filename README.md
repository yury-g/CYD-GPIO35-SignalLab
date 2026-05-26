# CYD GPIO35 SignalLab

Raw signal-capture lab for the ESP32 Cheap Yellow Display (CYD) and a PulseSensor signal wire on `GPIO35`.

This repo is for repeatable raw datasets: ear, finger, no contact, nothing connected, and deliberate motion/pressure captures. It is not product firmware, beat detection firmware, or a BPM/IBI trust experiment.

## Firmware

- PlatformIO Arduino project for ESP32 CYD.
- One sketch: `src/SignalLab.ino`.
- Reads only `GPIO35`.
- Samples at 50 Hz.
- Uses 10-bit ADC values from `0..1023`.
- First visible firmware version: `SignalLab 0.1.0-gpio35scope`.
- Current visible firmware version: `SignalLab 0.2.1-touchrec`.
- Shows live waveform, raw value, rolling min/max/range, rail indicators, placement/contact controls, recording controls, elapsed recording timer, and simple status.
- Never shows trusted BPM/IBI and does not use PulseSensor beat auto-detection.

Visible statuses:

- `NO SENSOR`
- `FLOATING`
- `TOO FLAT`
- `GOOD WAVEFORM`
- `MOVING`
- `CLIPPING`

## Serial CSV

At 115200 baud the firmware prints sample rows only while recording. The CYD screen controls the operator state:

```csv
kind,ms,raw,min,max,range,rail_low,rail_high,status,label,placement,connected,recording,event,detail
```

Sample rows use `kind=siglab`. Button taps and markers use `kind=event` or `kind=marker`, so placement/contact/recording transitions are preserved in the same `raw.csv`.

Command input:

```text
MARK <text>    emit a marker row
RESET          clear rolling stats and elapsed timer
START          start recording from host
PAUSE          pause recording from host
STOP           stop recording from host
FINGER         select finger placement
EAR            select ear placement
NONE           clear placement
CONNECTED      mark sensor as plugged/connected
UNPLUGGED      mark sensor as unplugged/not connected
VERSION        print the firmware version
```

## Build And Flash

Every CYD flash must get a visible version bump first.

```bash
platformio run
platformio run -t upload --upload-port /dev/cu.usbserial-10
platformio device monitor --port /dev/cu.usbserial-10 --baud 115200
```

If `platformio` is not on PATH, create a local environment:

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -U pip platformio pyserial
pio run
pio run -t upload --upload-port /dev/cu.usbserial-10
pio device monitor --port /dev/cu.usbserial-10 --baud 115200
```

Boot confirmation should include:

```text
version,SignalLab 0.2.1-touchrec
```

## Capture Datasets

Install host dependencies:

```bash
python3 -m pip install pyserial
```

Record a screen-driven capture. Leave this running on the Mac, then use the CYD buttons from your chair:

```bash
python3 tools/capture.py --port /dev/cu.usbserial-10
```

CYD buttons:

- `FINGER`, `EAR`: select placement. Tapping the active placement clears it back to `none`.
- `CONNECTED`: toggle whether the sensor is plugged/connected.
- `START`: begin a new recording segment and reset rolling stats/timer.
- `PAUSE`: pause sample rows without ending the capture.
- `STOP`: end the capture; `tools/capture.py` writes the files and exits.

The waveform remains live even while stopped or paused, but `siglab` sample rows are written only while recording.

Host-commanded timed capture is still available:

```bash
python3 tools/capture.py --mode timed --port /dev/cu.usbserial-10
```

Each capture writes:

- `captures/YYYYMMDD-HHMMSS_slug/raw.csv`
- `captures/YYYYMMDD-HHMMSS_slug/meta.json`
- `captures/YYYYMMDD-HHMMSS_slug/summary.json`
- `captures/YYYYMMDD-HHMMSS_slug/preview.svg`

Analyze an existing capture:

```bash
python3 tools/analyze.py captures/YYYYMMDD-HHMMSS_slug/raw.csv --json-out captures/YYYYMMDD-HHMMSS_slug/summary.json
```

## Initial Test Matrix

- 1 min nothing connected
- 1 min sensor connected but no body contact
- 2 min left ear
- 2 min right ear
- 2 min finger
- 1 min deliberate movement/pressure changes

## Commit And Push Captures

Datasets are intended to live on GitHub:

```bash
git add captures/YYYYMMDD-HHMMSS_slug
git commit -m "Add <placement> capture"
git push
```

For a batch:

```bash
git add captures
git commit -m "Add initial GPIO35 signal captures"
git push
```

## Research Notes

- [PPG techniques worth testing with SignalLab](docs/ppg-research-notes.md)
