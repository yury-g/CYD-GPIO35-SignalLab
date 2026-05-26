# PPG Techniques Worth Testing With SignalLab

This note summarizes peer-reviewed and experimentally studied photoplethysmography (PPG) techniques that are relevant to a raw GPIO35 capture lab. It is not medical guidance, product firmware guidance, or a claim that a single PulseSensor channel can produce trusted heart-rate, SpO2, HRV, or diagnosis outputs. The goal is narrower: collect labeled raw signals that later algorithms can analyze reproducibly.

SignalLab intentionally records raw 10-bit samples first. That design matches a common lesson from PPG research: the raw waveform, signal quality, contact condition, motion state, and clipping behavior all matter before any physiological interpretation is attempted.

## What PPG Measures

PPG is an optical measurement of changing blood volume and optical path properties in tissue. The useful pulse waveform is usually treated as a small pulsatile AC component riding on a much larger DC baseline. In pulse oximetry, the classic red/infrared method uses AC/DC ratios at two wavelengths; SignalLab has only one analog channel, so it should not attempt SpO2. The same AC/DC framing is still useful for quality analysis: a measurable pulsatile component relative to baseline generally means better perfusion and contact than a flat or clipped signal.

Practical consequence for this repo: store raw values, rolling min/max/range, rails, placement label, and notes. Do not erase the baseline or filter the only copy of the data at capture time.

## Capture Practices With Scientific Value

### 1. Negative Controls Are Real Data

The planned captures of "nothing connected" and "sensor connected but no body contact" are not throwaways. They create reference examples for floating-input behavior, electrical pickup, rail clipping, and non-physiological waveforms. Later quality classifiers need these counterexamples to avoid learning that every periodic-looking pattern is a pulse.

Recommended labels:

- `nothing-connected`
- `sensor-no-contact`
- `left-ear`
- `right-ear`
- `finger`
- `movement-pressure`

Recommended metadata:

- placement and contact state
- duration
- whether the sensor is shielded from ambient light
- whether pressure was stable, changing, or deliberately disturbed
- whether the subject was still, talking, moving, or changing posture

### 2. Keep Raw Capture Separate From Interpretation

Peer-reviewed PPG work repeatedly distinguishes signal acquisition, preprocessing, quality assessment, and feature extraction. SignalLab should stay in the acquisition and annotation layer. Offline analysis can then compare multiple preprocessing and peak-detection approaches against the same raw CSV.

Good firmware behavior:

- sample at a stable rate
- preserve raw ADC values
- mark clipping and rails
- display simple quality hints
- allow labels and markers

Bad firmware behavior for this project:

- trusted BPM/IBI display
- adaptive beat thresholds that change the dataset semantics
- hidden smoothing that becomes impossible to audit later
- discarding ugly captures

### 3. Sample Rate: 50 Hz Is Useful For This Lab

Human pulse-rate content and the main PPG morphology are low-frequency relative to many biosignals. Many peak-detection studies use band-pass filtering around the pulse band; one widely cited systolic peak detection study evaluated a 0.5-8 Hz preprocessing band before peak detection. At 50 Hz, SignalLab has enough samples for rough waveform shape, candidate peaks, movement artifacts, and offline quality analysis, while keeping CSV files small enough to commit.

Limits:

- 50 Hz is not ideal for high-fidelity derivative morphology.
- It is enough for basic raw waveform capture and later rough pulse-candidate experiments.
- It does not make single-channel PulseSensor data medically reliable.

## Techniques To Try Offline

### 1. Baseline And Band-Pass Preprocessing

Common offline steps:

- remove slow baseline wander with a moving median, high-pass filter, or detrending method
- use a band-pass filter around plausible pulse content
- normalize per-window so amplitude changes do not dominate every algorithm
- keep the original raw CSV unchanged

Why it matters: motion, pressure, skin contact, and ambient-light changes can create large low-frequency shifts. A filter may reveal a pulse-like component, but it can also manufacture confidence if used blindly. Always compare filtered output to raw, rails, range, and label.

### 2. Signal Quality Indexes

Several peer-reviewed studies evaluate PPG signal quality indexes (SQIs). SQIs are more appropriate for SignalLab than "trusted BPM," because they can say "this segment is usable for later analysis" without pretending to know the heart rate.

Useful SQI families to test:

- perfusion-style index: pulsatile amplitude relative to baseline
- skewness and kurtosis: morphology/asymmetry statistics
- entropy: disorder/noise measure
- zero-crossing or derivative counts: rough high-frequency/noise proxy
- relative spectral power: how much energy sits in a plausible pulse band
- non-stationarity: how much the segment changes over time
- template or beat-shape consistency: whether candidate pulses look similar

SignalLab already captures ingredients for early SQIs: raw values, range, clipping flags, status labels, and placement metadata.

### 3. Systolic Peak Candidate Detection

Elgendi et al. proposed and evaluated a computationally simple PPG systolic peak detector using event-related moving averages and block selection. In their PLOS ONE study, the algorithm was tested on heat-stressed emergency responders and reported high sensitivity and positive predictivity across 5,071 annotated beats. This is a good candidate for offline analysis because it is transparent and lightweight.

Important caution: peak candidates are not trusted beats. On SignalLab data, peak detection should be run only after quality checks and should output confidence, failure reasons, and plots.

Practical offline experiment:

1. Load raw CSV.
2. Remove baseline wander.
3. Filter around the pulse band.
4. Compute signal quality windows.
5. Run candidate peak detection only on acceptable windows.
6. Export candidate peaks as annotations, not as firmware truth.

### 4. Motion Artifact Detection And Removal

Motion artifacts are a central PPG problem because motion can change the optical path and can overlap the real pulse frequency. Peer-reviewed work has explored:

- adaptive noise cancellation using accelerometer reference signals
- PCA, ICA, and singular spectrum analysis
- multi-channel and multi-wavelength PPG selection
- adaptive notch filters at motion frequencies
- rejecting corrupted windows instead of trying to repair everything

SignalLab is a single-channel analog capture, so it cannot perform accelerometer-reference cancellation or multi-channel source separation by itself. It can still support motion research by collecting clearly labeled movement/pressure captures as negative or artifact-rich examples.

### 5. Multi-Wavelength And SpO2 Techniques Are Out Of Scope

Pulse oximetry techniques such as red/infrared AC/DC ratio-of-ratios, discrete saturation transforms, and heart-rate-tuned comb filters are scientifically important. They are not directly implementable with a single GPIO35 PulseSensor signal unless the hardware is changed to collect synchronized red/infrared channels with calibrated optics.

For this repo, cite these techniques as context, not as a feature target.

## Suggested Analysis Roadmap

Short term:

- improve `tools/analyze.py` with per-window SQIs
- compute clipping percentage, range percentiles, step-change scores, and rough peak candidates
- generate preview plots with rail markers and status-color bands

Medium term:

- add an offline filtered preview while preserving raw CSV
- add candidate peak annotation export
- compare simple local-max, slope/derivative, and Elgendi-style moving-average detectors
- summarize each capture into quality windows: unusable, artifact, possible PPG, good PPG

Long term:

- collect repeated captures from the same placements
- add a second synchronized reference channel if hardware changes
- add optional accelerometer data if motion-cancellation experiments become a goal
- compare model outputs across the same committed raw datasets

## Takeaways For CYD GPIO35 SignalLab

- Raw-first is the right architecture.
- Negative controls are scientifically useful.
- The screen should show quality hints, not trusted physiology.
- Offline SQI is the next best research step.
- Peak detection should be an annotation experiment, not firmware truth.
- Single-channel PulseSensor data can support waveform-quality and candidate-peak experiments, but not validated SpO2 or medical claims.

## References

- Park et al., "Photoplethysmogram Analysis and Applications: An Integrative Review," Frontiers in Physiology, 2022. https://pmc.ncbi.nlm.nih.gov/articles/PMC8920970/
- Jubran, "Pulse oximetry," Critical Care, 2015, and related fundamentals of PPG/pulse oximetry. https://pmc.ncbi.nlm.nih.gov/articles/PMC4099100/
- Elgendi et al., "Systolic Peak Detection in Acceleration Photoplethysmograms Measured from Emergency Responders in Tropical Conditions," PLOS ONE, 2013. https://doi.org/10.1371/journal.pone.0076585
- Elgendi, "Optimal Signal Quality Index for Photoplethysmogram Signals," Bioengineering, 2016. https://pmc.ncbi.nlm.nih.gov/articles/PMC5597264/
- Zhang et al., "Motion Artifact Reduction in Wearable Photoplethysmography Based on Multi-Channel Sensors with Multiple Wavelengths," Sensors, 2020. https://pmc.ncbi.nlm.nih.gov/articles/PMC7085621/
- Lee et al., "A Comparative Study of Physiological Monitoring with a Wearable Opto-Electronic Patch Sensor (OEPS) for Motion Reduction," Sensors, 2015. https://pmc.ncbi.nlm.nih.gov/articles/PMC4493550/
- "Improving Pulse Rate Measurements during Random Motion Using a Wearable Multichannel Reflectance Photoplethysmograph," Sensors, 2016. https://www.mdpi.com/1424-8220/16/3/342
- Aboy et al., "Heart-rate tuned comb filters for processing photoplethysmogram (PPG) signals in pulse oximetry," Journal of Clinical Monitoring and Computing, 2020. https://link.springer.com/article/10.1007/s10877-020-00539-2
