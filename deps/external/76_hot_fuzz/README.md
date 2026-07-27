# Hot Fuzz

A stereo fuzz/distortion + resonant wah effects processor card for the
[Music Thing Workshop Computer](https://github.com/TomWhitwell/Workshop_Computer).

Built on the `ComputerCard` library and the Mutable Instruments Braids SVF.
Integer-only fixed-point DSP, no web editor in v1 — fully standalone operation
via 3 pots, 1 switch, and 6 LEDs.

## Playing Hot Fuzz

This section is for musicians. If you already know your way around fuzz and
wah, skim it. If you're new to these effects — or to the Workshop Computer —
read on. The technical sections for builders start further down at
[Features](#features).

### What Hot Fuzz does

Hot Fuzz is two effects chained together, in series: a **fuzz** (distortion)
followed by a **wah** (a sweeping resonant filter). Both are stereo — your
left and right inputs are processed independently, though they share the same
wah cutoff so the filter stays coherent across the stereo image.

A three-position switch on the card changes what the three pots do:

- **Switch Up**: Fuzz + wah. Main pot picks the fuzz type, X pot controls
  the fuzz drive, Y pot controls the wah's resonance.
- **Switch Mid**: Fuzz + wah blend. Main pot blends between dry and fuzzed
  signal, X pot controls drive, Y pot controls resonance.
- **Switch Down**: Wah mode. Fuzz is off. Main pot controls the wah sweep
  (or switches to auto-wah), X pot blends dry/wet, Y pot controls resonance.

In all three modes the signal flows fuzz first, then wah. In Down mode the
fuzz is bypassed, so you hear the wah on its own.

### What is a fuzz?

A fuzz (or distortion) takes your input signal and clips it — chopping off
the peaks of the waveform, which adds harmonics and sustain. The harder it
clips, the more aggressive and sustained the sound. Think of the difference
between a clean guitar tone and the saturated, singing lead sound of rock
and metal — that's clipping at work.

There are two broad flavours: **soft clipping** rounds off the peaks gently,
giving a warm, amp-like overdrive. **Hard clipping** chops them flat, which
adds harsh, buzzy harmonics and a square-wave character. Beyond those, Hot
Fuzz also offers **asymmetric clipping** (uneven top/bottom, which adds
even harmonics — the classic fuzz character) and **wavefolding**
(folding the waveform back on itself, which can produce octave-up and
metallic, bell-like tones).

If you want to go deeper, search for: *"guitar fuzz pedal"*, *"soft vs hard
clipping"*, *"wavefolding"*, *"asymmetric clipping fuzz"*.

### What is a wah?

A wah is a filter that emphasises a narrow band of frequencies and moves
that band up and down — the classic "wah-wah" vocal cry of a Cry Baby pedal
on a guitar. Low positions sound throaty and growly; high positions sound
quacky and biting.

The **resonance** (sometimes called Q) controls how sharp that emphasis is.
Low resonance is smooth and subtle; high resonance is peaky and vocal, with
a pronounced "bark" as the filter sweeps.

Hot Fuzz's wah sweeps from about 300 Hz (throaty low) to 2 kHz (quacky
high). The sweep can be controlled manually by a pot, by a CV input, or
automatically by an envelope follower that responds to how hard you play
(auto-wah).

If you want to go deeper, search for: *"wah pedal"*, *"state-variable
filter"*, *"filter resonance Q"*, *"envelope follower filter"*.

### The four fuzz sounds

You pick the fuzz type in Switch Up mode. The Main pot is divided into four
equal zones, left to right (CCW to CW). LED 5 (bottom-right) always shows
which type is active — see [Reading the LEDs](#reading-the-leds-while-you-play)
below.

| Zone | Main pot range | Fuzz | Sounds like | LED 5 |
|---|---|---|---|---|
| 1 | 0–1023 | Soft | Warm, singing overdrive. Smooth sustain that cleans up when you back off the drive. | Steady dim |
| 2 | 1024–2047 | Hard | Sharp, aggressive square-wave fuzz. In-your-face, cuts through a mix. | Steady bright |
| 3 | 2048–3071 | Asym | Classic fuzz character. Choked decay, splatty attack, rich in even harmonics. | Slow blink (~1 Hz) |
| 4 | 3072–4095 | Fold | Octave-up at low drive, metallic and bell-like. High drive folds into complex harmonics — ring-modal, alien. | Fast blink (~4 Hz) |

The fuzz type you pick in Up mode stays selected when you switch to Mid or
Down. It just stops being selectable until you come back to Up.

LED 5 visual key:

```
  ◐  Soft    — steady, dim
  ●  Hard    — steady, bright
  ●  Asym    — blinks full on/off, ~1 Hz
  ●  Fold    — blinks full on/off, ~4 Hz
```

### Quick start

1. Plug audio into **Left Audio In** and **Right Audio In**. If you only
   have one source, one channel is fine — the other just stays quiet.
2. Connect **Left/Right Audio Out** to your mixer, amp, or headphones.
3. Set the switch to **Mid** (middle position).
4. Set **Main** (the big pot) to noon — halfway.
5. Set **X** and **Y** to noon.
6. Play. You should hear a blended signal: part dry, part fuzzed, with a
   moderate wah resonance colouring the tone.

From here, turn **Main** clockwise for more fuzz, counter-clockwise for
cleaner. Turn **X** up for more drive. Turn **Y** up for a sharper, more
vocal wah.

### The three modes

#### Switch Up: Fuzz + Wah

- **Set**: Switch up. Main pot picks the fuzz type (four zones, see the
  table above). X pot = fuzz drive. Y pot = wah resonance.
- **You'll hear**: Full fuzz into the wah. The signal is always fully wet —
  no dry blend. The wah sits at whatever position you last set it (or
  wherever CV1 points, if patched). Move the Main pot to change fuzz type;
  the wah position doesn't change just because you're in this mode.
- **LEDs show**: LED 0 = drive level (brighter = more drive). LED 1 is off
  (no blend in this mode). LED 2 = wah sweep position. LED 3 = resonance.
  LED 5 = fuzz type pattern.

This is the mode for committing to a fuzz sound and shaping it with the wah.

#### Switch Mid: Fuzz + Wah Blend

- **Set**: Switch in the middle. Main pot = dry/wet blend (CCW = dry, CW =
  full fuzz). X pot = fuzz drive. Y pot = wah resonance.
- **You'll hear**: A mix between your clean signal and the fuzzed-and-wahed
  signal. At CCW you hear only the dry input. At CW you hear the full fuzz
  + wah, same as Up mode. In between, the two are blended.
- **LEDs show**: LED 0 = drive. LED 1 = blend amount (brighter = more wet).
  LED 2 = sweep. LED 3 = resonance. LED 5 = fuzz type pattern.

This is the mode for dialling in how much fuzz you want — from a hint of
grit to full saturation.

#### Switch Down: Wah Mode

Fuzz is off. You hear the wah on its own, blended with dry signal via the X
pot. The Main pot controls the wah frequency — but how it does that depends
on whether auto-wah is on or off. You toggle auto-wah with a **double-tap
Down** gesture (see below).

**Manual wah** (auto-wah off) — the Main pot directly controls the wah
frequency across the full 300 Hz–2 kHz sweep. CCW = throaty low, CW =
quacky high. Turn it and the filter follows.

**Auto-wah** (auto-wah on) — the Main pot sets a **base frequency** (the
resting point when you're not playing), and the envelope follower sweeps
the filter *upward* from that base as you play harder. Play soft = filter
stays near the base. Play hard = filter opens up. The attack and release
are fixed (10 ms attack, 200 ms release) so the response feels consistent.

**Toggling auto-wah: the double-tap Down gesture**

The Down position is momentary — it springs back to Mid when you release
it. To toggle auto-wah on or off:

1. From Mid, press Down and release back to Mid (first tap).
2. Press Down again within 0.5 seconds (second tap).
3. **LED 4 flashes briefly** to confirm the toggle.
4. Now hold Down to play. LED 4 stays dark (auto-wah off) or follows your
   playing dynamics (auto-wah on).

If you wait longer than 0.5 seconds before the second tap, nothing happens
— the first tap is ignored. This prevents accidental toggles while you're
just entering Down mode to play.

Step-by-step to try auto-wah:

1. Switch to Mid (resting position). X at noon. Y at noon.
2. Double-tap Down: press Down, release to Mid, press Down again quickly.
3. LED 4 flashes — auto-wah is now on.
4. Hold Down and play softly. The filter sits near the base frequency
   you set with Main.
5. Play harder. The filter sweeps up in response to your dynamics, then
   settles back as the note decays.
6. Turn Y up for a sharper, more vocal response; down for subtler.
7. To turn auto-wah off, double-tap Down again. LED 4 flashes and goes
   dark. Now the Main pot gives you a manual sweep across the full range.

- **Set**: Switch down. Main = manual sweep (auto-wah off) or base
  frequency (auto-wah on). X = dry/wet blend. Y = resonance.
- **You'll hear**: Wah only, no fuzz. In manual mode the pot sweeps the
  full 300 Hz–2 kHz range. In auto-wah your playing dynamics drive the
  sweep upward from the base frequency.
- **LEDs show**: LED 0 is off (no drive in this mode). LED 1 = wah blend.
  LED 2 = sweep. LED 3 = resonance. LED 4 = envelope level (dark when
  auto-wah is off, follows your playing when on; flashes briefly on each
  toggle). LED 5 = fuzz type pattern (still shown, even though fuzz is off
  — so you remember what you picked).

### Reading the LEDs while you play

The six LEDs are laid out in three rows of two. Here's what they show in
each mode:

```
  Switch Up: Fuzz + Wah

    ●   ○        Drive      —
    ●   ●        Sweep      Resonance
    ○   ●        —          Fuzz type

  Switch Mid: Fuzz + Wah Blend

    ●   ●        Drive      Blend
    ●   ●        Sweep      Resonance
    ○   ●        —          Fuzz type

  Switch Down (manual wah)

    ○   ●        —          Blend
    ●   ●        Sweep      Resonance
    ○   ●        —          Fuzz type

  Switch Down (auto-wah)

    ○   ●        —          Blend
    ●   ●        Sweep      Resonance
    ●   ●        Envelope   Fuzz type

  ● active (brightness follows the pot or your playing)
  ○ off
```

A quick visual guide:

- **LED 0 (top-left)**: brightness follows the drive pot (Up/Mid). Off in
  Down mode. Brighter = more fuzz drive.
- **LED 1 (top-right)**: brightness follows the blend amount. Off in Up
  mode (no blend there). In Mid it shows fuzz blend; in Down it shows wah
  blend.
- **LED 2 (middle-left)**: brightness follows the wah sweep position. Dark
  = low/throaty, bright = high/quacky. Watch this while you sweep the Main
  pot in Down mode or while auto-wah moves the filter.
- **LED 3 (middle-right)**: brightness follows the Y pot (resonance). Dark
  = smooth, bright = peaky/vocal.
- **LED 4 (bottom-left)**: the envelope follower. **Dark = manual mode,
  lit = auto-wah.** Its brightness follows how hard you're playing. If it's
  off, you're in manual wah; if it's reacting to your picking, you're in
  auto-wah.
- **LED 5 (bottom-right)**: always shows the fuzz type, even in Down mode
  where fuzz is off. Steady dim = soft, steady bright = hard, slow blink
  = asym, fast blink = fold. Glance at it any time to know which fuzz
  you've selected.

The full reference table is below in [LED Feedback](#led-feedback).

### CV inputs (optional)

The two CV inputs let you control the card with external signals from the
Workshop Computer or other modular gear.

- **CV1** (Wah CV): overrides the wah cutoff in Up and Mid modes when
  patched. Patch a slow LFO here for an auto-sweeping wah without giving up
  fuzz. In Down mode CV1 is ignored (the Main pot and envelope handle the
  sweep). When you first patch CV1, there's a brief debounce (~11 ms) before
  it takes over — this filters out the normalisation probe transients, so
  don't be surprised if the pot momentarily seems to override the CV.
- **CV2** (Drive CV): adds to the X pot's drive value. Patch an envelope
  follower here for touch-sensitive fuzz that gets dirtier the harder you
  play. In Down mode the X pot controls blend, so CV2 adds to the blend
  value instead — keep that in mind if you're patching CV2 across mode
  changes.

### Things to know

- **Settings reset on power cycle.** There's no save, no recall — fuzz
  type, switch position, and auto-wah state all go back to defaults when
  you power off. If you love a setting, note it down.
- **In Up/Mid without CV1, the wah holds its position.** It doesn't
  auto-sweep on its own. To move it, either turn the Main pot (Down mode),
  patch a CV or LFO into CV1 (Up/Mid), or use auto-wah (Down mode).
- **Auto-wah is toggled by double-tapping Down.** From Mid, press
  Down→release→press Down again within 0.5 seconds. LED 4 flashes to
  confirm. When off, the Main pot gives you a manual sweep across the full
  300 Hz–2 kHz range. When on, the Main pot sets the base frequency and
  your playing dynamics sweep above it.
- **Stereo with shared wah.** Fuzz and DC-blocking are independent per
  channel, but the wah cutoff is shared — so the filter stays coherent
  across the stereo image even though the channels are processed
  separately.

## Features

- **Four fuzz types**: soft (cubic), hard (symmetric clamp), asymmetric (2:1 clamp),
  foldback (wavefold + octave — full-wave rectification with drive-controlled folding)
- **Resonant wah**: Braids state-variable filter, LP output, 300 Hz–2 kHz sweep
- **Manual and auto-wah**: envelope follower with separate attack/release,
  engaged by turning the Main pot past threshold in Down mode
- **Stereo processing**: shared cutoff, independent filter state per channel
- **Dry/wet blend**: Main pot in Mid mode (fuzz blend), X pot in Down mode (wah-only blend)
- **No persistence**: settings reset to defaults on power cycle
- **CV inputs**: optional wah-frequency and drive modulation (debounced)

## Controls

| Control | Up | Mid | Down |
|---|---|---|---|
| Main pot | Fuzz type (4 zones) | Fuzz blend (dry→wet) | Wah freq (manual) or base freq (auto-wah) |
| X pot | Fuzz drive | Fuzz drive | Dry/wet blend |
| Y pot | Resonance | Resonance | Resonance |

Fuzz type zones (Up mode): soft (0–1023), hard (1024–2047), asymmetric (2048–3071), foldback (3072–4095). Type persists when switching modes.

Auto-wah: toggled on/off by double-tapping Down (Mid→Down→Mid→Down within 0.5 s). LED 4 flashes to confirm. When on, Main pot sets the base frequency and the envelope sweeps upward from there. When off, Main pot gives a manual sweep across the full 300 Hz–2 kHz range.

CV1 overrides wah cutoff in Up/Mid modes. CV2 adds to drive.

## LED Feedback

```
| 0  1 |   top row:     drive + blend
| 2  3 |   middle row:  sweep + resonance
| 4  5 |   bottom row:  envelope + fuzz type
```

| Switch | LED 0 | LED 1 | LED 2 | LED 3 | LED 4 | LED 5 |
|---|---|---|---|---|---|---|
| Up | Drive | Off | Sweep | Resonance | Off | Type* |
| Mid | Drive | Blend | Sweep | Resonance | Off | Type* |
| Down | Off | Blend | Sweep | Resonance | Envelope† | Type* |

\* Fuzz type patterns: soft = steady dim, hard = steady bright, asym = slow blink (~1 Hz), fold = fast blink (~4 Hz). Always shows current type.

† Envelope LED: off in manual mode, follows playing dynamics in auto-wah (Down mode only).

## Build

Requires the Raspberry Pi Pico SDK (2.x). Set `PICO_SDK_PATH` or let CMake
fetch it from git.

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake ..
make -j4
```

Flash `hot_fuzz.uf2` to the Pico by holding BOOTSEL and copying it to the
`RPI-RP2` drive.

## Regenerate LUTs

The wah-frequency lookup tables in `resources.h` are generated by a Python
script (checked in for reproducibility):

```bash
python3 tools/gen_luts.py > resources.h
```

## Testing

A host-side DSP test harness lives in `test/`. It compiles the DSP functions
against a desktop toolchain (no Pico SDK required) and verifies:

- SVF frequency response against a scipy reference
- DC blocker settling
- Fuzz overflow safety (all 4 types, all drive values)
- Foldback octave symmetry and edge cases
- Envelope follower timing
- Crossfade bounds and bypass bit-transparency
- Reciprocal LUT accuracy (replaces M0+ software division)

```bash
cd test && cmake -B build && cmake --build build && ctest --test-dir build
```

## Architecture

### Audio path (per sample, core0)

```
AudioIn → preamp (3x) → envelope tap (pre-fuzz) → DC block
       → fuzz (soft/hard/asym/fold) → DC block → SVF (wah, LP)
       → crossfade(dry, wet) → clamp → AudioOut
```

### Execution model

- **Core0**: audio ISR (`ProcessSample`) at 48 kHz; envelope follower and
  `f`-coefficient glide per sample; cutoff target every 64 samples.
- **Core1**: LED updates (~100 Hz). Reads core0's published state via
  `c0_to_c1` struct.
- Binary is `copy_to_ram` to avoid flash-cache jitter in the audio ISR.
- Fuzz rescaling uses a precomputed reciprocal LUT (8 KB) to avoid
  software division on the M0+ (which lacks a hardware divider).
- CV1 connection state is debounced (~11 ms) to filter normalisation
  probe transients that can briefly override the Main pot.

## References

- [Workshop Computer](https://github.com/TomWhitwell/Workshop_Computer) —
  hardware platform and `ComputerCard` library
- [Mutable Instruments Braids](https://github.com/pichenettes/eurorack) —
  SVF implementation and coefficient LUTs
- [DaisySP](https://github.com/electro-smith/DaisySP) — soft-clip reference

## License

MIT (inherited from ComputerCard and Braids).