# Development Workflows, Automated Testing & Build Pipeline

This document details the software architecture, code generation pipeline, automated testing harness, web UI integration, and build procedures for the **Music Thing Modular Workshop System / Computer** project across **VCV Rack** and **Patchnotes (Emscripten WebAssembly)**.

---

## 1. System Architecture & C++ Pico SDK Porting Layer

### A. Core Abstractions
- **`ComputerCard` Base Class** ([src/ComputerCard.h](file:///Users/vmaurer/Music/Workshop_VCV_Dev/Workshop_Computer_VCV/src/ComputerCard.h)): Standard interface implemented by all program cards (`ProcessSample()`, `KnobVal()`, `SwitchVal()`, `AudioIn1()`, `AudioOut1()`, etc.).
- **Pico SDK Mock Layer** ([src/pico_mocks.h](file:///Users/vmaurer/Music/Workshop_VCV_Dev/Workshop_Computer_VCV/src/pico_mocks.h)): Hardware mock layer replacing Raspberry Pi Pico RP2040 SDK functions (GPIO, ADC, Multicore FIFO, Flash memory access, USB MIDI) with cross-platform equivalents for macOS/Linux/Windows and WebAssembly.
- **DSO Variable & Thread Isolation**: Each dynamic card library (`libcard_<id>.dylib`) isolates static and global variables across multiple module instances via thread-local handles (`t_instance`, `ComputerCard::thisptr`).

### B. Dual-Core & Multi-Thread IPC Dispatch
- Hardware cards using RP2040 Core 1 background loops register entry points via `multicore_launch_core1(entry)`.
- Under single-threaded WebAssembly or VCV Rack tick contexts, `multicore_launch_core1` registers `entry` into `t_instance->g_wasm_core1_tick`.
- In VCV Rack host environments, `host_multicore_launch_core1` spawns background worker threads (`g_core1_thread_val`) with synchronized thread-local storage pointers (`set_thread_globals_fn`).

---

## 2. Automated Testing Harness & Verification Suite

The project includes an extensive multi-tier test harness for automated regression detection, functional validation, stress testing, and cross-platform parity verification.

### A. Test Execution Entry Point ([tools/test_harness/run_all_tests.py](file:///Users/vmaurer/Music/Workshop_VCV_Dev/Workshop_Computer_VCV/tools/test_harness/run_all_tests.py))
Executes tests across all 58 cards with customizable targets and stress levels:
```bash
# Run default test suite across VCV Rack & WASM (~10s)
python3 tools/test_harness/run_all_tests.py

# Test VCV Rack cards only
python3 tools/test_harness/run_all_tests.py --vcv-only

# Test single card with extended stress checks
python3 tools/test_harness/run_all_tests.py --card acid --stress

# Run cross-platform VCV ↔ WASM parity comparison
python3 tools/test_harness/run_all_tests.py --parity-only
```

### B. VCV Rack Functional Suite ([tools/test_harness/test_vcv_cards.py](file:///Users/vmaurer/Music/Workshop_VCV_Dev/Workshop_Computer_VCV/tools/test_harness/test_vcv_cards.py))
Uses [test_card_behavior.cpp](file:///Users/vmaurer/Music/Workshop_VCV_Dev/Workshop_Computer_VCV/src/test_card_behavior.cpp) IPC runner to load dynamic card libraries and execute:
1. **Boot Verification**: Ensures card initializes, sets `card_ptr`, and runs without early crashes.
2. **Idle State Check**: Verifies outputs remain within valid audio/CV ranges (-10V to +10V).
3. **Autonomous Audio & Synthesis Generation**: Drives Z-switch modes, gate triggers, and verifies active audio/CV energy output.
4. **Trigger & Gate Response**: Exercises rising edge pulse inputs (`PulseIn1`, `PulseIn2`).
5. **Audio Passthrough**: Drives inputs on effect cards (`reverb`, `delay`, `degenerator`, `flux`) and verifies wet signal output.

### C. Stress Testing Layer (`--stress`)
1. **Extended Stability**: Runs 500,000 samples (~11.3s at 44.1kHz) across 10 checkpoints checking for state drift, NaN/Inf, and memory leaks.
2. **Extreme Input Overload**: Drives clipping ±6V audio, ±10V CV, and high-frequency pulse trains simultaneously.
3. **Full Knob × Switch Matrix**: Sweeps all 81 combinations (3 switch positions × 27 knob settings).
4. **Disconnected Inputs**: Simulates unplugging hardware cables via `DISCONNECT_INPUT`.
5. **Rapid Parameter Jitter**: Alternates knob values every 5 samples across 200 cycles to catch click/zipper crashes.

### D. Cross-Platform Parity Comparison (`--parity`)
Compares DSP output trajectories between VCV Rack C++ dylibs and Patchnotes WASM builds:
- Evaluates **RMS Difference** and **Max Absolute Difference** across deterministic test scenarios.
- Verifies exact cross-platform match (**RMS = 0.0000**, **Max Abs = 0.0000**).

---

## 3. Web Editor UI Integration & Audit Pipeline

### A. Web Editor Architecture ([patchnotes/patch_notes/js/cards/wasm/web/](file:///Users/vmaurer/Music/Workshop_VCV_Dev/patchnotes/patch_notes/js/cards/wasm/web/))
- Interactive Web Editors (Librarians, Visualizers, Lisp Editors, Waveform Managers) run in isolated `<iframe>` elements within Patchnotes.
- Bidirectional communication uses `editor_bridge.js` and `vcv_web_bridge.js` to exchange web serial/MIDI payloads with the WASM DSP engine.

### B. Headless Web Editor Audit ([patchnotes/patch_notes/wasm_dev/tools/audit_editors.py](file:///Users/vmaurer/Music/Workshop_VCV_Dev/patchnotes/patch_notes/wasm_dev/tools/audit_editors.py))
Automated script verifying all 23 card web editors:
```bash
# Run web editor file & reference audit
python3 wasm_dev/tools/audit_editors.py
```
Checks:
- `CARDS_WITH_WEB_EDITORS` registry entries match actual files on disk.
- `editor_bridge.js` script injection.
- Relative asset paths (`CSS`, `JS`, `SVG`, `WAV`, `TSX`) exist.

---

## 4. How to Port & Add a Program Card

### Step 1: Place Card Source Code
- Official cards: `deps/Workshop_Computer/releases/NN_card_name/`
- Custom/unofficial cards: `deps/external/NN_card_name/`

### Step 2: Write `info.yaml`
Add metadata to `info.yaml`:
```yaml
Name: My Card
Creator: Author Name
License: MIT
Repository: https://github.com/...
manual: |
  Card description and instructions...
panel:
  inputs: [Audio 1, Audio 2, CV 1, CV 2, Pulse 1, Pulse 2]
  outputs: [Audio 1, Audio 2, CV 1, CV 2, Pulse 1, Pulse 2]
```

### Step 3: Register in Whitelist & Run Code Generation
Add to `CARD_WHITELIST` in `tools/port_all_cards.py` and run code generation:
```bash
# Generate C++ wrappers and metadata
python3 tools/port_all_cards.py
python3 tools/generate_extended_metadata.py

# Sync Patchnotes web metadata
python3 patch_notes/wasm_dev/tools/update_patchnotes_metadata.py
```

### Step 4: Multicore Compilation Rule
Always use parallel multicore compilation flags:
```bash
# VCV Rack plugin build
make -j$(sysctl -n hw.ncpu)

# WebAssembly build (inside patchnotes/patch_notes/)
make -f Makefile.wasm -j$(sysctl -n hw.ncpu)
```

---

## 5. Summary of Recent Fixes

- **IPC Worker Thread Fix**: Resolved dual-core worker thread initialization in `fr330hfr33` (#87) and `cosmik_c1zzl3` (#84) via `multicore_launch_core1` assignment.
- **Null Pointer Dereference Fix**: Added immediate `card_ptr` registration inside `ComputerCard` constructor to prevent thread-local handle dereference crashes in `grains` (#51).
- **Looper Boot Timing Fix**: Added automatic switch warmup and boot counter fast-forward for `degenerator` (#71).
- **Web Editor Paths**: Corrected `CARDS_WITH_WEB_EDITORS` mappings for `lens`, `fr330hfr33`, `fragments`, `usb_audio_bridge`, and fixed relative module pathing in `stretchcore/index.html`.
- **Suite Pass Rate**: **58 / 58 PASS (100.0%)** on VCV Rack, **58 / 58 PASS (100.0%)** on WASM, and **80 / 80 PASS (100.0%)** on Web Editors.
