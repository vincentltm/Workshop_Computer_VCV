# AGENTS.md — Voices of SID

A dual MOS 6581 SID emulation Workshop Computer card using the reSID engine.

## Read first

| Document | Purpose |
|----------|---------|
| `ComputerCard.h` | **Authoritative API source.** Verify every method name and signature here. Do not invent APIs. |
| `reSID/sid.h` | reSID SID class API. The only external API used: `reset()`, `set_chip_model()`, `set_sampling_parameters()`, `enable_filter()`, `mute()`, `clock()`, `output_filtered()`, `write()`. |
| `README.md` | User-facing documentation. Controls, architecture, build instructions. |

## Critical rules

1. **Never invent APIs.** LLMs hallucinate methods (e.g. `LED::L1`). Verify every call against `ComputerCard.h`.
2. **`int32_t` everywhere.** `float` is software-emulated and slow. Use `int32_t`, multiply + `>>` instead of divide. Where float is unavoidable, use `sinf()` not `sin()`, `float` not `double`. Build flag `-Wdouble-promotion` catches accidental doubles.
3. **`ProcessSample()` must complete in ≤ 20 μs.** It runs in interrupt context at 48 kHz. Overrun symptoms: knob values lock at ~1780, channels permute. Profile with a debug GPIO and oscilloscope if in doubt.
4. **`PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64` is essential.** Without it, code flashes fine but fails after reset.
5. **`pico_set_binary_type(name copy_to_ram)` is strongly recommended.** Eliminates flash cache miss jitter.
6. **Default clock: 200 MHz.** `set_sys_clock_khz(200000, true)`. Do not change without profiling.
7. **Read knobs and CV only inside `ProcessSample`.** Reading from outside causes interrupt safety issues.
8. **GPIO4 = normalisation probe = UART1 TX.** Never enable Arduino UART1 serial — it corrupts jack detection.
9. **USB/MIDI must run on core 1.** Core 0 handles the audio interrupt. Keep audio DSP on one core.
10. **Confirm understanding before non-trivial work.** Restate the goal, scope, and assumptions; wait for confirmation. Skip this only for obvious one-liners.

## Project layout

```
voices_of_sid/
├── AGENTS.md              ← you are here
├── README.md               ← user documentation
├── info.yaml               ← card catalogue metadata
├── CMakeLists.txt           ← build config (Pico SDK, 200 MHz, copy_to_ram)
├── pico_sdk_import.cmake    ← Pico SDK bootstrap
├── main.cpp                 ← all card logic (single file)
├── ComputerCard/
│   └── ComputerCard.h      ← Workshop Computer API (authoritative)
├── reSID/                   ← reSID emulation engine (Dag Lem, GPL v2)
│   ├── sid.h, sid.cc        ← main SID class
│   ├── voice.h, voice.cc    ← voice/oscillator
│   ├── filter.h, filter.cc  ← analog filter model
│   ├── envelope.h, envelope.cc ← ADSR envelope
│   ├── extfilt.h, extfilt.cc  ← external filter
│   ├── pot.h, pot.cc        ← potentiometer input
│   ├── wave.h, wave.cc      ← waveform generator
│   ├── siddefs.h             ← compile-time switches
│   └── spline.h              ← waveform lookup tables
└── build/                   ← generated (do not check in)
```

## Architecture

Two SID chips run in parallel at PAL C64 clock (985248 Hz), downsampled to 48 kHz:

- **sid1** — voices 0 (lead) + 2 (ring-mod/sync carrier) → AudioOut1
- **sid2** — voice 1 (second voice) → AudioOut2

Voice routing uses `mute()` per chip. Voice 3 on sid1 is enabled only when ring modulation or hard sync is active.

## SID register map (quick reference)

Per voice (base = N × 7):
- `+0/+1`: frequency divisor lo/hi (16-bit)
- `+2/+3`: pulse width lo/hi (12-bit)
- `+4`: control register (waveform bits | gate)
- `+5`: attack (hi nibble) | decay (lo nibble)
- `+6`: sustain (hi nibble) | release (lo nibble)

Chip-level filter registers:
- `0x15/0x16`: cutoff (11-bit, written as lo 3 bits + hi 8 bits)
- `0x17`: resonance (hi nibble) | voice routing (lo nibble, 0x03 = voices 0+1 through filter)
- `0x18`: filter mode (hi nibble) | volume (lo nibble, 0x1F = low-pass, max volume)

## Verified API reference (from ComputerCard.h)

### Audio
```cpp
int16_t AudioIn1()           // -2048..2047 (AC-coupled, not useful for DC CV)
int16_t AudioIn2()           // -2048..2047 (AC-coupled, not useful for DC CV)
void    AudioOut1(int16_t)   // -2048..2047
void    AudioOut2(int16_t)   // -2048..2047
```

### CV
```cpp
int16_t CVIn1()              // -2048..2047 (DC-coupled, 1V/oct pitch CV)
int16_t CVIn2()              // -2048..2047
void    CVOut1(int16_t)      // -2048..2047 (1:1 passthrough from CVIn1)
void    CVOut2(int16_t)      // -2048..2047 (1:1 passthrough from CVIn2)
```

### Pulse/Gate
```cpp
bool PulseIn1()              // true while signal high (held gate)
bool PulseIn1RisingEdge()    // true for exactly one sample on rising edge
bool PulseIn2()
bool PulseIn2RisingEdge()
void PulseOut1(bool val)     // true = high (1:1 passthrough from PulseIn1)
void PulseOut2(bool val)     // true = high (1:1 passthrough from PulseIn2)
```

### Knobs
```cpp
int32_t KnobVal(Knob::Main)  // 0..4095
int32_t KnobVal(Knob::X)     // 0..4095
int32_t KnobVal(Knob::Y)     // 0..4095
```

### Switch (three-position Z)
```cpp
Switch SwitchVal()           // Switch::Up, Switch::Centre, Switch::Down
```

### LEDs (indices 0–5)
```cpp
void LedOn(int index, bool on)
void LedBrightness(int index, int val)  // 0..2047
```

## Build

```sh
# First time (configure):
cmake -S . -B build

# Every time (compile):
cmake --build build -j$(sysctl -n hw.logicalcpu)

# Output:
build/voices_of_sid.uf2
```

Do **not** use `cd build && cmake ..` — use `cmake -S . -B build` from the card root.

## Common pitfalls

### LSP errors are always false positives
The LSP (clangd) cannot find `hardware/gpio.h` and similar SDK headers because they are not on the LSP include path. This produces cascading "unknown type" errors for `int32_t`, `uint8_t`, all API calls, etc. **Ignore all LSP errors.** Only compiler errors from the actual `cmake --build` matter.

### Audio inputs are AC-coupled
`AudioIn1()` / `AudioIn2()` have hardware AC-coupling capacitors that block DC voltages. They cannot be used for slow CV (cutoff, resonance, etc.). Use `CVIn1()` / `CVIn2()` instead — those are DC-coupled.

### Audio-input tracking bug
Do not use `AudioIn1()` values to drive envelope state (e.g. as a peak follower feeding the attack). Any live audio signal will continuously reset the envelope upward during the release phase, preventing decay entirely.

### SID chip model
Always use `MOS6581` (not `MOS8580`). The 6581 has the classic aggressive analog filter character that people associate with the SID sound.

### Pulse width minimum
Pulse width must be clamped to a minimum of 40 (out of 4095). PW of 0 produces silence with the pulse waveform.