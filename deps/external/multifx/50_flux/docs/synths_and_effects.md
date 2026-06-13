# MultiFX — Synths & Effects Reference

## Switch Layout

| Switch | Main Knob | X Knob | Y Knob |
|--------|-----------|--------|--------|
| **UP** | Decay / Env | Pitch | Timbre *(Noise: Filter Cutoff)* |
| **MID** | Blend | Time | Feedback |
| **DOWN** | Effect select *(Bypass: Volume)* | Synth / Source select | Z Param |

**Z Param (DOWN · Y):** Filter Cutoff for tonal synths · Sample Select for samplers/granular/drums

---

## Synths (Switch UP / Source = Synth)

| # | Name | Main (Decay/Env) | X (Pitch) | Y (Timbre) | Z (DOWN·Y) |
|---|------|-----------------|-----------|-----------|-----------|
| 0 | **Wavetable** | Amplitude envelope | Pitch (±range) | Wavetable position / morph | Filter cutoff |
| 1 | **VA Bass** | Env / filter decay | Pitch | Pulse width / waveform | Filter cutoff |
| 2 | **Strings** | Bow pressure / bow speed | Pitch | Vibrato depth | Filter cutoff |
| 3 | **Piano** | Hammer velocity | Pitch | Tone brightness / damping | Filter cutoff |
| 4 | **Modal** | Strike velocity | Pitch | Inharmonicity / mode density | Filter cutoff |
| 5 | **FM Synth** | Operator ratio / depth | Pitch | FM index (modulation depth) | Preset select |
| 6 | **Noise** | Amplitude envelope | Pitch (center freq bias) | **Filter Cutoff** | Filter cutoff |
| 7 | **Sampler One-Shot** | Gate length | Pitch (playback speed) | Timbre / EQ | Sample select |
| 8 | **Sampler Loop** | Crossfade time | Pitch (playback speed) | Loop point | Sample select |
| 9 | **Sampler Player** | Direction / length (bipolar) | Pitch | Start point | Sample select |
| 10 | **Drums** | Kick velocity | Pitch offset | Tone character | Sample bank |
| 11 | **Granular** | Grain size / density | Pitch | Grain position (scrub) | Sample select |

> **Note:** Noise synth Y knob → Filter Cutoff on switch UP. Filter is a 1-pole lowpass applied by the SynthCore VCA stage.

> **Sampler modes:** Z knob scrolls sample. Notes-off is called before swap to prevent unwanted preview playback.

---

## Effects (Switch MID)

| # | Name | Main (Blend) | X (Time) | Y (Feedback) |
|---|------|-------------|---------|-------------|
| 0 | **Bypass** | *(no params — use DOWN·Main for Volume)* | — | — |
| 1 | **Reverb** | Room size / pre-delay | Decay time | Damping |
| 2 | **Delay** | Wet/dry mix | Delay time | Feedback amount |
| 3 | **Chorus** | Depth / wet mix | LFO rate | Stereo spread |
| 4 | **Flanger** | Wet/dry mix | LFO rate | Feedback |
| 5 | **Phaser** | Wet/dry mix | LFO rate | Feedback |
| 6 | **Bit Crusher** | Wet/dry mix | Sample rate reduction | Bit depth |
| 7 | **Wavefolder** | Fold amount | Threshold | Symmetry |
| 8 | **Ring Mod** | Wet/dry mix | Carrier frequency | Sideband balance |
| 9 | **Filter (LP)** | Resonance | Cutoff frequency | Drive |
| 10 | **Filter (HP)** | Resonance | Cutoff frequency | Drive |
| 11 | **Comb Filter** | Wet/dry mix | Frequency / tune | Feedback |
| 12 | **Pitch Shift** | Wet/dry mix | Pitch interval | Grain overlap |
| 13 | **Freeze** | Mix | Window size | Feedback |
| 14+ | *(additional effects)* | Param 0 | Param 1 | Param 2 |

> Switch UP + external input: activates **Freeze** parameter on the current effect (if it has one).

---

## Outputs

| Output | Configured via |
|--------|---------------|
| CV 1 / CV 2 | Web UI → Outputs tab |
| Pulse 1 / Pulse 2 | Web UI → Outputs tab |

### CV Modes
| # | Mode | Channel field | Arg field |
|---|------|--------------|----------|
| 0 | MIDI Pitch (1V/Oct) | MIDI channel | — |
| 1 | MIDI Velocity | MIDI channel | — |
| 2 | MIDI CC | MIDI channel | CC number |
| 3 | Synth Envelope | — | — |
| 4 | Random S&H | — | Range |
| 5 | Step Sequencer A/B | — | — |
| 6 | Voice Audio (PWM) | — | — |
| 7 | Internal EG | — | — |
| 8 | LFO Utility | Shape (0=Sin,1=Tri,2=SawUp,3=SawDn,4=Sqr,5=SmRnd) | Speed |
| 9 | Generative Seq (Turing) | — | Spice (0-127) |

### Pulse Modes
| # | Mode | Arg field |
|---|------|----------|
| 0 | MIDI Gate | MIDI channel |
| 1 | MIDI Trigger | MIDI channel |
| 2 | Clock Out | — |
| 3 | Probabilistic | Chance (0-127) |
| 4 | Pass-through / Inverse P1 | — |
| 5 | Voice Audio (1-bit PWM) | — |
| 6 | Seq A Gate | — |
| 7 | Seq B Gate | — |

---

## MIDI CC Assignments (defaults, configurable in Web UI)

| CC | Function |
|----|----------|
| 7 | Volume |
| 20 | Effect select |
| 21 | Synth / Source select |
| 22 | Synth filter cutoff |
| 23 | Sample select |
| 71 | Synth timbre |
| 73 | Synth envelope |
| 74 | Synth pitch |
| 85 | FX Param 0 (Main) |
| 86 | FX Param 1 (X) |
| 87 | FX Param 2 (Y) |

All CCs are always active regardless of switch position and will engage the corresponding knob lock.
