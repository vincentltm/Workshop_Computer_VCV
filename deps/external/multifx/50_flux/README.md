# 🎹 Flux Synth & Effects - Discord Quick Start Guide

A powerful dual-core RP2040-based synthesizer and multi-effects processor.

## 🚀 Basic Operation

The physical switch controls what the Knobs and Main Encoder do:

| Switch Position | Mode | Main Encoder | Knob X | Knob Y |
| :--- | :--- | :--- | :--- | :--- |
| **UP** | **Edit Synth** | Envelope (Decay) | Pitch | Timbre / Color |
| **MIDDLE** | **Select Source** | Change Synth | Master Volume | Sample / Bank |
| **DOWN (Hold)** | **Select Effect** | Change Effect | FX Volume | Filter Cutoff |

> [!TIP]
> **Double-tap DOWN** to enter Synth Select mode without holding.

---

## 🌐 Web UI (Flux Manager)
Connect via USB and visit the **Flux Manager** to:
*   **Configure CV/Pulse I/O**: Map MIDI to CV, set Clock divisions, or use the Step Sequencer.
*   **Manage Samples**: Drag and drop WAV files to create custom kits for the Samplers and Drums.
*   **Firmware Updates**: Easily flash new features to your device.

---

## 🎹 Synth Engines
All synths (except Sampler Player) use: **X = Pitch**, **Y = Timbre**, **Main = Envelope**.

1.  **Wavetable**: Morphing Triangle ➔ Saw ➔ Square ➔ Pulse.
2.  **VA Bass**: Triple-osc stack through a resonant 24dB filter.
3.  **Strings**: Karplus-Strong physical modeling. **Y** controls Stiffness/Bowing.
4.  **Modal**: Metallic bells and percussion. **Y** controls harmonic structure.
5.  **Noise**: White noise through a resonant LPG (**Y**).
6.  **Sampler (One Shot/Loop)**: Triggered samples from flash. **Y** = Start Pos.
7.  **Sampler Player**: Drone/Tape style. **Main = Speed**, **X = Start**, **Y = Length/Dir**.
8.  **Drums**: 4-voice kit. Trigger via Pulse 1/2 or Audio L/R.
9.  **Granular Cloud**: Shared grain pool. **Y** = Scan Position.
10. **Piano**: Additive synthesis spectral piano. **Y** = Brightness.
11. **FM Synth**: 6-op DX7 style engine. **Main** = Env Speed, **Y** = Mod Depth.

---

## 🧪 Effects List
Most effects map as: **Knob 1 (Main) = Mix/Blend**, **Knob 2 (X) = Param 1**, **Knob 3 (Y) = Param 2**.

| Category | Effects |
| :--- | :--- |
| **Dynamics** | Compressor, Equalizer |
| **Amps** | Guitar Amp (Crunch), Velvet Amp (Warmth) |
| **Modulation** | Tremolo, Sine Chorus, Vibrato Chorus, Bitcrusher |
| **Shifters** | Pitch Shift, Frequency Shifter (CWO) |
| **Delays** | Digital, Tape, Ping Pong, Tape Loop (Lofi) |
| **Reverbs** | Plate, Spring, Freeverb, Shimmer, Cathedral, Granular Clouds |
| **Experimental** | Micro Looper, Gen Loss, Lossy Audio, Space Resonator, Oil-Can Echo |
| **Utilities** | Wind, Chatter (RoboTalk), Resonator |

---

## 🔌 Inputs & Outputs
*   **Audio L/R In**: Mono/Stereo processing OR Pitch/Timbre CV modulation.
*   **Pulse 1 & 2 In**: Gate/Trigger inputs for all synths.
*   **CV 1 & 2 Out**: Map MIDI Pitch, Velocity, CC, or Envelopes to your modular gear.
*   **Pulse 1 & 2 Out**: MIDI Clock, Gate, or Audio PWM.
