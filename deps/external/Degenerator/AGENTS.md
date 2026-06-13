# AGENTS.md — Degenerator

Platform reference: `/Users/pjoe/Experimenten/mtm/Workshop_Computer/AGENTS.md`

---

## Build

```sh
cmake -S . -B build
cmake --build build -j$(sysctl -n hw.logicalcpu)
# Output: build/degenerator.uf2
```

Flash: hold BOOTSEL, plug USB, copy `.uf2` to mounted drive.

---

## Implementation notes

- **Global buffer**: `g_buffer` must be static/global, not a class member
- **Multicore**: USB serial on core 0, audio processing on core 1
- **Real-time constraint**: `ProcessSample()` must complete in ≤ 20 μs
- **Integer-only DSP**: no floating point — all arithmetic uses bit-shifts and integer math
- **Audio range**: -2048 to 2047 (12-bit signed); knob range: 0-4095
- **μ-law companding**: buffer stored as `uint8_t` with μ-law companding (2:1 compression)
  - 240,000 samples × 1 byte = 234.4 KB = **5.0 seconds** at 48 kHz
  - Encode: 4,096-entry lookup table (4 KB in flash) + add offset, indexed by `val + 2048`
  - Decode: 256-entry lookup table (512 bytes in flash), single table read
  - Quantization error: max 44 LSBs (2.1%), near-perfect at low levels, does not accumulate across degrade passes

### Architecture

- **State organized into sub-structs**: `PulseState pulse`, `FilterDriftState drift`, `OxideState oxide`, `SampleHoldState crush`, `HissState hiss`, `SaturationState sat` — `resetDegradeState()` zero-inits via `= {}`
- **`loopWrapped`** is a local bool in `ProcessSample()`, passed explicitly to `applyFilterDrift()` and `applyOxideShedding()` — no implicit member coupling
- **`applyHarmonic()`** receives `audioIn` as a separate parameter for tape hiss envelope tracking (not the processed signal)
- **`applyDestructive()`** no longer takes a `mainKnob` parameter — Y knob controls its own intensity

### Modes

- **Operating modes** (chosen at boot, persists for entire session):
  - **YOLO mode** (Z not Down at boot): no flash slot features. Buffer starts filled with silence (`0x00` μ-law), playing immediately in MIX/DEGRADE. Z-Down always starts RECORD. No STORE_SLOT available. Pulse In 2 always resets loop to start. LED 5 pulses slowly in MIX mode (~1.5 Hz triangle).
  - **SLOT mode** (Z Down at boot): flash slot features enabled. Enters SELECT_SLOT at boot. STORE_SLOT available (Z-Down with Big Knob near zero). SELECT_SLOT accessible via Pulse In 2 while Z Down. LED 5 off in MIX mode.
- **SELECT_SLOT** (SLOT mode only): Big Knob selects flash loop slot (0-3). LEDs 1-3 show binary slot number, LED 0 on. Audio muted. On release of Z (to Middle or Up): load selected slot → MIX or DEGRADE. If no data at slot → RECORD.
- **STORE_SLOT** (SLOT mode only, Z Down with Big Knob at zero from MIX/DEGRADE): Big Knob selects target slot (0-3). LEDs 1-3 show binary slot, LED 0 = slot has data, LED 4 blinks (selected slot). Z Down → write buffer to flash. Z Up → cancel to DEGRADE. Z Middle → stay in store mode (allows knob turning). Flash write runs on core 0; core 1 mutes during write.
- **RECORD** (Z Down): records to full buffer (~5.0s); flip Z to stop early and finalize length. Big Knob not used for loop length.
- **MIX** (Z Middle): additive overdub. Big Knob = mix level (quadratic `>>16`). Starting at zero prevents accidental overdub.
- **DEGRADE** (Z Up): irreversible degradation. Big Knob = write-back rate (quadratic `>>15`). Below threshold = bypass.

### Effects pipeline

Read sample → X effects (harmonic) → Y effects (destructive) → write back

X zones (knob position):
- 0-1365: Saturation
- 1365-2730: Saturation↔Filter Drift crossfade
- 2730-4095: Filter Drift↔Tape Hiss crossfade

Y zones:
- 0-1365: Oxide Shedding
- 1365-2730: Oxide Shedding↔Bit Crush crossfade
- 2730-4095: Bit Crush↔Bit Rot crossfade

### I/O

- **CV In 1**: adds to Big Knob (bipolar, offset +2048)
- **CV In 2**: adds to Y knob (bipolar, offset +2048)
- **Audio In 2**: mixes with Audio In 1 when connected (stereo-to-mono)
- **Pulse In 1**: triggers RECORD
- **Pulse In 2**: resets loop to start
- **Pulse Out 1**: fires on loop boundary wrap
- **Pulse Out 2**: fires when RECORD completes (buffer full)
- **CV Out 1**: loop position (ramp 0→4095 over one loop, mapped to -2048→+2047)
- **CV Out 2**: output envelope follower
- **Audio Out 2**: monitors Audio In 1 (dry)

### Flash storage (web manager)

- **Flash layout** (2 MB RP2040 Pico):
  - `0x10000000`: Firmware (~48 KB)
  - `0x10100000`: Loop data header (magic `0x64656772` = "degr", version 1, count, defaultIdx, 4 entries × 32 bytes, padded to 288 bytes) + loop data (up to 4 fixed slots, `FLASH_LOOP_SLOT_SIZE = 241664` bytes each)
- **Boot behavior**: YOLO mode (Z not Down) fills buffer with silence and enters MIX/DEGRADE. SLOT mode (Z Down) enters SELECT_SLOT at boot.
- **Save trigger**: Z Down with Big Knob at zero (<50) from MIX or DEGRADE → enters STORE_SLOT mode. Big Knob selects slot 0-3, Z Down writes, Z Up cancels, Z Middle stays. Flash write runs on core 0 via atomic handoff flags; uses `multicore_lockout_start_blocking()` / `multicore_lockout_end_blocking()` with `multicore_lockout_victim_init()` on core 1 to safely pause core 1 during flash erase/program. Core 0 disables interrupts via `save_and_disable_interrupts()`. No heap allocation, `__not_in_flash_func` for core 0 write path.
- **Web manager**: single-file HTML app at `web/degenerator_manager.html`, uses Picoboot/WebUSB to read/write flash in BOOTSEL mode. Audio uploaded as WAV/MP3 is resampled to 48 kHz and u-law encoded via embedded tables. Downloaded loops are u-law decoded to float then exported as WAV.
- **u-law round-trip**: web app embeds identical encode/decode tables as firmware, ensuring bit-perfect upload and minimal-error download

### Known behavior

- YOLO mode (Z not Down at boot): buffer starts with silence (μ-law code 0x00), Z-Down always starts RECORD, no STORE_SLOT, Pulse In 2 always resets loop
- SLOT mode (Z Down at boot): enters SELECT_SLOT, STORE_SLOT available, Z-Down with Big Knob near zero enters STORE_SLOT
- In YOLO mode, LED 5 pulses slowly in MIX mode (~1.5 Hz triangle between brightness 100-400)
- In SLOT mode, LED 5 is off in MIX mode
- When buffer fills during RECORD, auto-transitions to MIX with a Pulse Out 2 pulse
- CV Out 1 uses `(phaseInt * 4096) / loopLength` for linear sweep (no overflow risk: 240000×4096 fits in int32_t)
- Phase wrapping uses `if` not `while` — phase only increments by 256 per sample
- Flash writes use `multicore_lockout` to safely pause core 1; core 0 disables interrupts during erase/program
