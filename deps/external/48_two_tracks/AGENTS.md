# two_tracks — AGENTS.md

## Project

Dual-core C++17 firmware for Music Thing Modular Workshop System Computer Card (RP2040). Dual-read-head phase looper recording mono audio to flash (IMA-ADPCM) and playing it through two independent heads with separately controllable positions and loop lengths.

## Build

```sh
cmake -S . -B build
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

Requires `PICO_SDK_PATH` env var (or `PICO_SDK_FETCH_FROM_GIT=ON`).

Output: `build/two_tracks.uf2` (copied to `UF2/two_tracks.1.0.uf2`).

## Flash

```sh
cp two_tracks.uf2 /Volumes/RPI-RP2/
```

Hold BOOTSEL on Pico, plug USB, mount RPI-RP2 drive first.

## Key facts

- **Sources**: `main.cpp` (DSP/control layer) + `two_tracks_stream.c/.h` (flash I/O engine, forked from Goldfish 2.0 by Dune Desormeaux). `ComputerCard.h` is a header-only library (not in this repo). `adpcm.h` has the IMA-ADPCM codec. `flash_size.h` does JEDEC flash-size detection.
- **No tests, no CI, no linter, no formatter** — embedded firmware, no test infrastructure.
- **Dual-core**: core 0 = DSP/control (audio path), core 1 = flash I/O (erase-ahead with suspend, page ring drain, head ring refill, preview/seek decode). No RTOS; core 1 spins `tt_stream_io_task()` forever.
- **Binary type**: `copy_to_ram` (mandatory — core 1 exits XIP during erase/program; core 0 must never fetch from flash). All core-0 audio functions marked `__not_in_flash_func`.
- **Clock**: `set_sys_clock_khz(192000, true)` with `vreg_set_voltage(VREG_VOLTAGE_1_20)` — 192MHz, multiple of 48kHz audio clock.
- **Audio**: 48kHz, 12-bit signed (-2048..2047), IMA-ADPCM 4-bit per sample (2 samples/byte). Mono recording (AudioIn1); both read heads play the same loop.
- **Storage**: flash (not RAM). Audio gets the full flash budget after the 384KB firmware reserve + ~36KB header. Capacity ≈ 70s on a 2MB card, ~11min on a 16MB card. No CV stream (CV outputs show live head position).
- **USB stdio disabled** (`pico_enable_stdio_usb(two_tracks 0)`).
- **`PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64`** for reliable reset.
- **State machine**: PLAY → ARMED (3s countdown with tick beeps) → RECORD → PLAY. Z Down triggers transitions. The RECORD→PLAY transition consumes the Z Down edge (sets `zDown=false`) so the same press doesn't immediately re-arm recording; a 100-sample debounce also guards PLAY→ARMED as defense-in-depth.
- **Two play modes**: Offset (Z Middle, knobs set position offsets + loop window) and Phasing (Z Up, right loop shorter, speed offset on Y 0.9×–1.1×). Selecting a mode in PLAY re-inits heads at phase 0 and requests new previews.
- **CV modulation**: CV1/2 modulate knob values in each mode (multiplicative). Pulse 1/2 reset read heads via seek.
- **ADPCM codec** from Music Thing MLRws card (releases/15_MLRws/adpcm.h). Stateful 4-bit encode/decode with step table and index table.
- **Random-access keyframes**: encoder state (predictor + step_index) snapshotted every `tt_stream_keyframe_interval()` samples (adaptive pow2: 512 on 2MB → 4096 on 16MB), keeping the in-RAM index ≤ 8192 entries (~32KB).
- **Per-head PCM ring**: 4096-sample cache per head, refilled asynchronously by core 1 (MARGIN=1536, budget=2048/call). Hold-last on underrun (momentary freeze, not a click). Core 0 never decodes ADPCM in the ISR.
- **Erase-suspend programming**: core 1 issues 0x75 (suspend) / 0x7A (resume) during sector erases to program pending pages and refill heads, so the flush frontier advances right through each erase (from Goldfish).
- **Fixed-point**: 24.8 format (8 fractional bits) for sub-sample positioning. Cubic Hermite (4-tap Catmull-Rom) interpolation.
- **Crossfade**: 64-sample `XF_LEN` blend (live fading out + loop-start preview fading in) + 32-sample `XF_BRIDGE` served from preview while head re-seeks, at loop boundaries.
- **Recording packs ADPCM nybbles** into the page ring (low nybble first, high nybble second), spread across 512 samples to avoid XIP-stall clicks during flash writes.

## Architecture

`main()` raises core voltage, sets 192MHz clock, calls `tt_stream_init()` (JEDEC probe + flash layout — single-core, before core 1 launch), initialises both heads on channel 0, launches core 1 (`core1_entry` → `tt_stream_io_task()` loop), then creates `TwoTracks` (extends `ComputerCard`), calls `EnableNormalisationProbe()`, and `Run()`.

`ProcessSample()` is the per-sample ISR callback on core 0 — all DSP lives there. State machine dispatches PLAY/ARMED/RECORD. All state transitions happen inside the switch: PLAY→ARMED (Z Down, gated on debounce==0), ARMED→RECORD (countdown expiry), RECORD→PLAY (Z Down, consumes the edge + sets debounce so PLAY→ARMED can't fire on the same sample). In PLAY, `updatePlayMode()` applies knob/CV mapping per Z position, `playHead()` reads 4 taps from the head's ring via `tt_stream_head_read()` and cubic-interpolates, with loop-boundary crossfade. Core 1 asynchronously refills the rings from flash, services previews/seeks, and drains the recording page ring.

Recording (`tt_stream_record_sample`) encodes ADPCM and packs bytes directly into the page ring (no burst copy — spreads the write to avoid XIP-stall clicks). Core 1 drains the ring to flash with erase-ahead + erase-suspend. On stop, partial pages are flushed and `s_recorded_samples` is set. Abrupt playback jumps (pulse reset, mode switch) request a seek; core 1 pre-decodes the target into a seek buffer, core 0 crossfades from live into it, then snaps the phase and re-inits the head.

## Engine (`two_tracks_stream`)

Forked from Goldfish (releases/11_goldfish/goldfish_stream.c/.h), adapted:
- `TT_AUDIO_CHANNELS = 1` (mono; both heads read channel 0)
- No CV stream (stripped — flash layout gives audio the full budget)
- No DELAY mode (stripped — two_tracks is PLAY/ARMED/RECORD only)
- Page publish `s_page_w += 1` (single channel, not `+= 2`)
- All `__dmb()`/`volatile`/`__not_in_flash_func` discipline preserved verbatim from Goldfish.