# Card Portability Difficulty Report

This report lists all remaining (non-ported) cards in the repository and rates their porting difficulty.

## 🟢 Low Difficulty (Standard Pico SDK C++)

### 34_dual_quant - Dual Quant
- **Creator**: Adrian Vos - with Vibe code support
- **Language**: C++
- **Description**: Dual quantised granular pitch shifter with calibrated 1V/oct CV outputs
- **Reasoning**: Standard Pico SDK C++ code. Can be ported using our default post-processing rules and the mocked Pico SDK headers.
- **Files**: 3 source files scanned

### 44_Birds - Birds
- **Creator**: Tom Whitwell
- **Language**: C++ (Pico SDK / ComputerCard)
- **Description**: Two birds sing to each other controlled by a Turing-style shift register sequencer with clock in and CV/pulse out.
- **Reasoning**: Standard Pico SDK C++ code. Can be ported using our default post-processing rules and the mocked Pico SDK headers.
- **Files**: 2 source files scanned

### 54_Tapegrade - Tapegrade
- **Creator**: 
- **Language**: C++ (Pico SDK)
- **Description**: 
- **Reasoning**: Standard Pico SDK C++ code. Can be ported using our default post-processing rules and the mocked Pico SDK headers.
- **Files**: 2 source files scanned

### 57_glitch - Glitch
- **Creator**: Andy Jenkinson (uglifruit)
- **Language**: C++ (Pico SDK)
- **Description**: Clock-synced beat-repeater with ratcheting, reversal and audio degradation
- **Reasoning**: Standard Pico SDK C++ code. Can be ported using our default post-processing rules and the mocked Pico SDK headers.
- **Files**: 1 source files scanned

### 58_LoChoVibes - Lochovibes
- **Creator**: 
- **Language**: C++ (Pico SDK)
- **Description**: Stereo chorus and vibrato effect featuring triangle, sine, and slow drift LFO modes, modulation-based delay movement, and tape-style saturation.
- **Reasoning**: Standard Pico SDK C++ code. Can be ported using our default post-processing rules and the mocked Pico SDK headers.
- **Files**: 2 source files scanned

### 60_markov - Markov
- **Creator**: Andy Jenkinson (uglifruit)
- **Language**: C++ (Pico SDK)
- **Description**: Dual generative Markov chain module — evolving melody (MarkoV) left side, rhythmic percussion patterns (MarkovPerc) right side, with internal synth voice
- **Reasoning**: Standard Pico SDK C++ code. Can be ported using our default post-processing rules and the mocked Pico SDK headers.
- **Files**: 4 source files scanned

### 66_stretchcore - Stretchcore
- **Creator**: Infinite Digits
- **Language**: Pico SDK
- **Description**: A card for playing and manipulating samples with tempo control, timestretch with browser-based audio loading (infinitedigits.com/stretchcore/)
- **Reasoning**: Standard Pico SDK C++ code. Can be ported using our default post-processing rules and the mocked Pico SDK headers.
- **Files**: 6 source files scanned

### 74_Wild_Pebble - Wild Pebble
- **Creator**: Adrian Vos with Vibecode support
- **Language**: C++
- **Description**: Playable generative rhythm and melody organism inspired by Pet Rock
- **Reasoning**: Standard Pico SDK C++ code. Can be ported using our default post-processing rules and the mocked Pico SDK headers.
- **Files**: 2 source files scanned

------------------------------

## 🟡 Medium Difficulty (Arduino Firmware)

### 00_Simple_MIDI - Simple Midi
- **Creator**: Tom Whitwell
- **Language**: Arduino-Pico
- **Description**: Takes USB midi, sends it to pulse and CV outputs, also sends knob positions and CV inputs back to the computer as CC values.
- **Reasoning**: Arduino firmware (needs custom wrapper bridging Arduino setup/loop to VCV Rack `ComputerCard` callbacks).
- User Comment: needs to be customized for vcv, probably can be simplified
- **Files**: 24 source files scanned

### 28_eighties_bass - Eighties Bass
- **Creator**: @todbot / Tod Kurt
- **Language**: arduino-pico core and Mozzi 2 library
- **Description**: Bass-oriented complete monosynth voice consisting of five detuned saw wave oscillators with mixable white noise and adjustable resonant filter.
- **Reasoning**: Arduino firmware (needs custom wrapper bridging Arduino setup/loop to VCV Rack `ComputerCard` callbacks).
- **Files**: 4 source files scanned

### 06_usb_audio - Usb Audio
- **Creator**: Vincent Maurer (vincentmaurer.de)
- **Language**: C++ (RPi Pico SDK)
- **Description**: 6-Channel USB Audio & MIDI firmware with CV/Gate support
- User Comment: needs to be customized for vcv, probably can be simplified
- **Reasoning**: Standard Pico SDK C++ code. Can be ported using our default post-processing rules and the mocked Pico SDK headers.
- **Files**: 20 source files scanned

------------------------------

## 🔴 High Difficulty (Complex Hardware / Peripherals)

### 15_MLRws - Mlrws
- **Creator**: Dune Desormeaux
- **Language**: Pico SDK
- **Description**: A remix of monome's classic MLR sample cutting platform (grid controller encouraged but optional)
- **Reasoning**: Uses USB Host API (needs wrapping VCV's MIDI input pipeline to simulate USB MIDI controllers).
- **Files**: 12 source files scanned

### 33_drumdrum - Drumdrum
- **Creator**: Moses Hoyt
- **Language**: C++ (ComputerCard)
- **Description**: DFAM-style 8-step sequencer
- **Reasoning**: Uses USB Host API (needs wrapping VCV's MIDI input pipeline to simulate USB MIDI controllers).
- **Files**: 15 source files scanned

### 41_blackbird - Blackbird
- **Creator**: Dune Desormeaux
- **Language**: Pico SDK + Lua
- **Description**: A scriptable, live-codable, USB-serial-to-CV device implementing monome crow's protocol
- **Reasoning**: Uses a physical OLED screen (needs custom rendering/display widget support in the VCV Rack UI).
- **Files**: 111 source files scanned

------------------------------

## ⚪ Placeholder / Binary-Only Cards (No source code)

### 02_comingsoon - Comingsoon
- **Creator**: 
- **Language**: N/A
- **Description**: 
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 09_DivCom - Divcom
- **Creator**: divmod
- **Language**: C++ (RPi Pico SDK) compat. cmake.
- **Description**: Comparator and VC clock divider, inspired by Serge NCOM
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 23_SlowMod - Slowmod
- **Creator**: divmod
- **Language**: C++ (RPi Pico SDK) compat. w/ cmake and Arduino IDE.
- **Description**: Chaotic quad-LFO with VCAs
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 24_crafted_volts - Crafted Volts
- **Creator**: Brian Dorsey
- **Language**: Rust (Embassy framework)
- **Description**: Manually set control voltages (CV) with the input knobs and switch. It also attenuverts (attenuates and inverts) incoming voltages.
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 30_cirpy_wavetable - Cirpy Wavetable
- **Creator**: @todbot / Tod Kurt
- **Language**: Circuit Python
- **Description**: Wavetable oscillator that using wavetables from Plaits, Braids, and Microwave,
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 37_compulidean - Compulidean
- **Creator**: Tristan Rowley
- **Language**: C++/Arduino, with vscode+platformio.
- **Description**: Generative Euclidean drum + sample player.
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 38_od - Od
- **Creator**: M. John Mills
- **Language**: MicroPython
- **Description**: Loopable chaotic Lorenz attractor trajectories and zero-crossings as CV and pulses, with sensitivity to initial conditions.
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 42_backyard_rain - Backyard Rain
- **Creator**: Brian Dorsey
- **Language**: Rust (Embassy framework)
- **Description**: Nature soundscape audio. A cozy rain ambience mix for background listening. You control the intensity. This card plays rain ambience which was recorded in my backyard.
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 47_NZT - Nzt
- **Creator**: @kjnilsson
- **Language**: C++ (ComputerCard)
- **Description**: Grain Noise and Noise Tools
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 53_glitter - Glitter
- **Creator**: Steve Jones
- **Language**: Pico SDK 2.1.1
- **Description**: Granular Looping Sampler
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 56_Krell - Krell
- **Creator**: Benjamin Reily
- **Language**: Blackbird Lua
- **Description**: Krell
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 71_degenerator - Degenerator
- **Creator**: Joep Vermaat
- **Language**: C++ (Pico SDK)
- **Description**: Degenerator — Disintegrating Looper. Capture audio loops and apply irreversible degradation with 6 algorithms (Saturation, Filter Drift, Tape Hiss, Oxide Shedding, Bit Crush, Bit Rot) via preview/apply workflow. Inspired by William Basinski's The Disintegration Loops.
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 72_motorik - Motorik
- **Creator**: Joep Vermaat
- **Language**: C++ (Pico SDK)
- **Description**: Motorik drum machine — kick/snare/hihat with bass and melody CV, classic Krautrock grooves
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 77_Placeholder - Placeholder
- **Creator**: None
- **Language**: None
- **Description**: Reserved for secret project
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 86_tesserae - Tesserae
- **Creator**: Joep Vermaat
- **Language**: C++ (Pico SDK)
- **Description**: Tesserae — Variable-voice (2-8) arpeggiated chord generator with 5 patterns, 10 scales, tap tempo, CV/audio transpose inputs, and dual CV + audio pitch outputs. Inspired by Laurie Spiegel's Music Mouse and Patchwork.
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 88_Blank - Blank
- **Creator**: Tom Whitwell
- **Language**: None
- **Description**: Reserved for blank 88 cards
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 98_duo_midi - Duo Midi
- **Creator**: Dune Desormeaux
- **Language**: Lua / Blackbird
- **Description**: A duophonic midi device/host interface
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

### 99_toolbox - Toolbox
- **Creator**: divmod
- **Language**: C++ (ComputerCard)
- **Description**: Mixer, VCA, noise, S&H, clock generator, etc.
- **Reasoning**: Empty placeholder or binary-only card (no source code available in workspace).
- **Files**: 0 source files scanned

------------------------------
