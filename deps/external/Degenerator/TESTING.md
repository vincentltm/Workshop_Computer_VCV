# Degenerator — Testing Checklist

Use this checklist after every code change to verify all effects work correctly.
Run through every test on real hardware before considering a build "done."

Mode note: Z-down can enter STORE_SLOT when Big Knob is near zero in MIX/DEGRADE. For tests that say "Z down, record", keep Big Knob above zero or use Pulse In 1 to trigger RECORD.

## Test setup

Connect the following for all tests:

| Connection | What |
|-----------|------|
| Audio Out 1 | Monitor speakers or headphones |
| Audio Out 2 | (optional) dry monitor for A/B comparison |

**Sound sources** — use the source listed for each test. The card sounds radically different depending on input material, so using the wrong source can hide bugs or create false positives.

---

## 1. Oxide Shedding (Y knob, 0-33%)

### 1.1 — Dropouts at low intensity

*Source:* Record a sustained synthesizer chord or piano drone (~3 seconds, rich harmonics, held steady). No transients, no silence.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record chord | Loop captured |
| 2 | Z up, X = 0, **Y = ~25%, main knob = 100%** | — |
| 3 | Listen for 30-60 seconds | Audible dropouts/gaps appear within 2-3 loops |
| 4 | Verify dropouts are brief gaps (~2-40ms) | Not clicks, not permanent silence — the chord drops out momentarily and returns |

Pass if: You hear the signal "stutter" — brief moments of silence or near-silence that return each loop pass at roughly the same positions.

Fail if: Signal sounds completely unchanged after 2+ minutes (oxide was doing nothing — old bug).

### 1.2 — Dropout accumulation over time

*Source:* Same sustained chord as 1.1, or white/pink noise.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, X = 0, Y = ~25%, main = 100% | — |
| 2 | Leave running 5+ minutes | Dropouts become noticeably more frequent and gaps grow slightly longer |
| 3 | Turn Y to 100% | Dropout frequency increases dramatically, damage accumulates faster |

Pass if: Degradation worsens over time (more gaps, longer gaps, more positions affected). The sound gets progressively "holier."

Fail if: Degradation rate stays constant regardless of how long you wait (damageLevel was never accumulating — old bug).

### 1.3 — Patch position consistency

*Source:* A short rhythmic pattern (e.g., 4 drum hits, or 4 distinct synth notes spread across the loop).

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record rhythmic pattern | ~2-4 second loop |
| 2 | Z up, X = 0, Y = ~50%, main = 100% | — |
| 3 | Listen carefully for dropouts | When a dropout hits a specific drum hit or note, it keeps returning to that same position each loop |
| 4 | Watch CV Out 1 (loop position) | Dropout positions correlate to specific CV values each pass |

Pass if: Dropouts are position-specific — the same drum hit goes silent on multiple consecutive passes, proving oxide patches are location-based.

Fail if: Dropouts jump randomly to completely different positions every loop (patch position logic broken).

### 1.4 — State persists across mode switches

*Source:* Sustained chord (same as 1.1).

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, Y = ~50%, main = 100%, let it degrade for 30s | Dropout patches established |
| 2 | Z middle (MIX), wait 5 seconds | Loop plays clean (mix knob at zero) |
| 3 | Z up again, Y = ~50%, main = 100% | Dropouts resume at same positions, oxide state was preserved |

Pass if: Returning to DEGRADE continues where you left off — same patches, same damage level.

Fail if: Degradation resets to zero as if nothing happened (resetDegradeState called unexpectedly).

---

## 2. Saturation (X knob, 0-33%)

*Source for all saturation tests:* **Use a bright, harmonically rich source** — a sawtooth wave, a bright synth pad, or an acoustic guitar strum. Avoid sine waves (too pure to hear saturation) and already-dark sources.

### 2.1 — Warmth, not thinness

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record bright pad (~3s) | Loop captured |
| 2 | Z up, **X = ~25%, Y = 0, main = 100%** | — |
| 3 | Listen for 30 seconds | Signal gets thicker, fuller, warmer. Highs are gently rolled off, not stripped. |
| 4 | Turn X to 100% | Signal is significantly darker, like tape saturation. Not thin or tinny. |
| 5 | A/B: switch to MIX (Z middle) for dry comparison | Degraded version is warmer, not harsher |

Pass if: Sound gets warmer with more X. More saturation = more warmth, more low-end presence.

Fail if: Sound gets thinner, brighter, or harsher as X increases (old HPF was stripping lows).

### 2.2 — LPF behavior at high intensity

*Source:* Bright synth with lots of high-frequency content (sawtooth wave open filter).

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, Y = 0, X = 80-100%, main = 100% | — |
| 2 | Listen for high frequencies | Highs are progressively rolled off. At 100% X, the signal is noticeably darker/muffled versus the original |
| 3 | Compare to dry | Degraded version has less "air" and "shimmer" — like an aged tape recording |

Pass if: High frequencies decrease as X increases, without adding harshness. Signal stays full, just darker.

Fail if: High frequencies increase or the signal becomes thin/metallic (old HPF behavior).

### 2.3 — No harsh clipping on loud sources

*Source:* Record a **very loud** signal — a square wave or overdriven synth near the top of the 12-bit range (~±1900).

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record loud source | Near-clipping input |
| 2 | Z up, X = ~50%, Y = 0, main = 100% | — |
| 3 | Listen | Saturation warms and compresses the signal. No harsh digital clipping or aliasing artifacts. |
| 4 | X = 100% | Still no objectionable harshness — just warm, compressed, dark |

Pass if: Saturation adds musical harmonics and compression, not digital harshness.

Fail if: Loud signal produces unpleasant digital clipping artifacts or aliasing.

---

## 3. Bit Rot (Y knob, 67-100%)

*Source for all bit rot tests:* Record a **clean, steady tone** — a sine wave at ~440 Hz, or a simple held note from an oscillator. Transients make it hard to distinguish bit rot from the source material.

### 3.1 — Subtle crackle, not explosive noise

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record steady sine tone (~3s) | Clean loop |
| 2 | Z up, X = 0, **Y = ~80%, main = 100%** | — |
| 3 | Listen | Hear subtle digital crackle/ticks. Individual glitches are quiet — maximum ±128 LSBs (6% of full scale). |
| 4 | Y = 100% | More frequent crackle, but still no explosive full-scale noise spikes |

Pass if: Bit rot produces subtle digital grain — ticks, crackle, quiet glitches. Never loud, transient explosions.

Fail if: Random full-scale noise blasts appear (old bug — bits 8-15 were being flipped, causing ±1024+ jumps).

### 3.2 — Accumulation over time

*Source:* Same steady sine tone as 3.1.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, X = 0, Y = ~90%, main = 100% | — |
| 2 | Let it run 5+ minutes | Crackle gradually increases. Signal slowly gains a crust of digital grain. The underlying sine is still recognizable. |
| 3 | Check frequency spectrum (if available) | No dramatic broadband energy spikes. Grain is concentrated in low-level artifacts. |

Pass if: Bit rot accumulates gracefully — a gradual coating of digital grime, not violent corruption.

Fail if: Signal becomes unrecognizable too quickly, or produces harsh broadband noise bursts.

### 3.3 — Spectrum stays controlled

*Source:* Simple sine wave, or a clean bell/chime with clear harmonics.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, X = 0, Y = ~90%, main = 50% | Slow degradation |
| 2 | Monitor output spectrum or listen | High-frequency energy from bit rot artifacts stays well below the main signal level |
| 3 | Compare original vs degraded spectrum | Degraded has low-level noise floor, not HF peaks |

Pass if: Bit rot adds a noise floor beneath the signal, not spectral spikes above it.

Fail if: Spectrum analyzer shows large HF spikes (individual bits at positions 8+ causing large jumps).

---

## 4. Bit Crush (Y knob, 33-67%)

*Source for all bit crush tests:* Record a **voice or acoustic instrument** — spoken word, singing, or acoustic guitar. The stair-stepping of bit crush is most audible on natural, continuous sounds with subtle dynamic variation.

### 4.1 — Softened stair-stepping

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record spoken word (~3s) | Clean voice loop |
| 2 | Z up, X = 0, **Y = ~45%, main = 100%** | — |
| 3 | Listen | Signal is degraded (fewer bits), but edges are *smoothed* — the stair-stepping doesn't sound harsh or metallic. Voice is lo-fi but not painful. |
| 4 | Y = 100% | Very lo-fi, aliased, but still the LPF rounds off the harshest edges. Like an old sampler, not raw bit truncation. |

Pass if: Crushed signal sounds like vintage lo-fi gear — degraded but not piercing or harsh.

Fail if: Crushed signal produces sharp aliasing tones or metallic ringing (no post-filter smoothing).

### 4.2 — Sample rate reduction is controlled

*Source:* A melody with clear pitch changes — a simple synth melody or whistling. High-frequency content helps expose aliasing.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record melody | — |
| 2 | Z up, X = 0, Y = ~60%, main = 100% | Sample rate reduction active |
| 3 | Listen for aliasing | Some aliasing and gritty texture, but LPF warms it into tape-like character. Gritty, not grating. |
| 4 | Y = 100% | Heavily decimated but still warmer than raw bit/sample-rate reduction would be |

Pass if: Sample rate reduction adds grit with warmth, not harsh aliasing squeal.

Fail if: High-frequency aliasing products dominate the sound (LPF not working).

### 4.3 — Dry vs crushed comparison

*Source:* Acoustic guitar or piano with natural decay. The reverb tail and note decay are where bit crush is most audible.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record piano/guitar | Clean loop |
| 2 | Z up, Y = ~50%, main = 100% | — |
| 3 | Let it run a few passes | Degraded version noticeably different |
| 4 | A/B: switch to MIX (Z middle) for dry reference | Degraded version is lo-fi but warmer/darker than expected from bit crushing alone |

Pass if: Crushed version is noticeably degraded but warmer and darker than raw bit reduction would produce.

Fail if: Crushed version is significantly brighter or harsher than the dry signal.

---

## 5. Filter Drift (X knob, 33-67%)

*Source:* Record a **bright, full-spectrum signal** — a synth pad with an open filter, or pink noise. The drift effect darkens the signal, so test material needs brightness to lose.

### 5.1 — Darkening over time

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record bright synth pad (~3s) | Clean, bright loop |
| 2 | Z up, **X = ~50%, Y = 0, main = 100%** | — |
| 3 | Run 5+ minutes | Tone gets *progressively darker* over time. Highs fade, lows remain. Signal becomes muffled, like aging capacitors. |
| 4 | Check with X = 80% | Drifting is faster, still only darkens (never brightens) |

Pass if: Sound consistently gets darker over time. Only lows remain after many passes. Signal sounds "aged" and "muffled."

Fail if: Sound ever gets brighter, thinner, or more present over time (old HPF at >93% was counteracting the LP drift).

### 5.2 — No brightness boost at any X position

*Source:* Same bright synth pad. Test specifically at high X.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, Y = 0, **X = 100%**, main = 100% | — |
| 2 | Run 3-5 minutes | Signal gets very dark. No sudden brightness boost at any point. |
| 3 | Sweep X through full range while listening | Signal always gets darker as X increases. No reversal point where highs come back. |

Pass if: X knob always correlates to more darkening. Monotonic relationship — never a "sweet spot" where signal gets brighter.

Fail if: Signal gets brighter at any X position above ~93% (old HPF was kicking in).

---

## 6. Tape Hiss (X knob, 67-100%)

*Source:* Record a **clean, quiet signal** — a soft synth pad, a quiet acoustic recording, or even silence. Hiss is most audible against quiet backgrounds.

### 6.1 — Subtle, warm hiss

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record quiet pad or silence (~3s) | Clean/quiet loop |
| 2 | Z up, **X = ~85%, Y = 0, main = 100%** | — |
| 3 | Listen for hiss | Hiss is present but subtle and warm — like tape hiss, not white noise. Shouldn't dominate the signal. |
| 4 | X = 100% | Maximum hiss is still manageable, not a wall of noise. Should sound like old tape, not a broken amp. |

Pass if: Hiss is audible but musical — warm noise floor, not harsh white noise blast.

Fail if: Hiss is overwhelmingly loud or sounds like raw white noise (old noise mix ceiling of 32 was too high, now 24).

### 6.2 — Envelope follower ducks noise

*Source:* Record alternating loud and quiet sections — a synth with amplitude modulation, or play a phrase with loud and soft notes.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record dynamic material | Loud and soft sections |
| 2 | Z up, X = ~85%, Y = 0, main = 100% | — |
| 3 | Listen during loud sections | Hiss is quieter — ducked by the signal |
| 4 | Listen during quiet sections or silence | Hiss rises — becomes more audible when signal drops |

Pass if: Hiss level inversely tracks signal level. Louder signal = less audible hiss.

Fail if: Hiss level stays constant regardless of signal (envelope follower broken).

---

## 7. Global tape warmth

*Source:* Record a **bright, full-spectrum signal** — synth pad with open filter, orchestral sample, or pink noise. The global LPF is subtle — you need high-frequency content to hear its effect.

### 7.1 — Cumulative warmth

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record bright pad (~5s) | Clean loop |
| 2 | Z up, X = ~40% (sat+drift blend), Y = ~40% (oxide+crush blend), main = 100% | All effects active |
| 3 | Run 5+ minutes | Overall tone trends darker. Cumulative effect of all degradation paths produces warmth, not harshness. |
| 4 | A/B: switch to MIX (Z middle) for dry | Degraded version is audibly warmer, darker, more "vintage" than the original recording |

Pass if: Any combination of X and Y effects, running for multiple minutes, produces a net darkening/warming of the signal.

Fail if: Degraded signal is overall brighter or harsher than the original recording.

### 7.2 — Commit rate influences LPF depth

*Source:* Bright synth pad (same as 7.1). Test with different commit rates.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record bright pad | — |
| 2 | Z up, X = 100%, Y = 0, main = ~25% | Slow degradation rate |
| 3 | Run 2 minutes | Very gradual darkening |
| 4 | Z up, X = 100%, Y = 0, main = 100% | Fast degradation rate |
| 5 | Run 30 seconds | Darkening happens much faster |

Pass if: Higher commit rate produces faster darkening. Relationship is audible.

Fail if: Commit rate makes no difference to the warmth character.

---

## 8. Integration: effect combinations

*Source:* Record a **dense, complex signal** — a full mix, a chord progression, or multiple instruments. You need enough harmonic content for all effects to be audible.

### 8.1 — X + Y independence

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z down, record full mix (~5s) | — |
| 2 | Z up, X = 50%, Y = 50%, main = 100% | Both axes active |
| 3 | Turn Y to 0 (X still 50%) | Only harmonic effects (saturation + drift) audible. No destructive effects. |
| 4 | Turn X to 0 (Y back to 50%) | Only destructive effects (oxide + crush) audible. No harmonic effects. |
| 5 | X = 50%, Y = 50% | Both families combine, result is darker + grittier |

Pass if: X and Y effects can be independently heard and disabled. Combining them produces an additive result.

Fail if: Turning one knob to zero doesn't disable its effect family, or effects interact unexpectedly.

### 8.2 — Crossfade zones

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, Y = 0, main = 100% | — |
| 2 | Sweep X slowly from 0 to 100% | 0-33% = saturation only, 33-67% = saturation→drift blend, 67-100% = drift→hiss blend. Smooth transitions, no jumps. |
| 3 | Sweep Y slowly from 0 to 100% (X = 0) | 0-33% = oxide only, 33-67% = oxide→crush blend, 67-100% = crush→rot blend. Smooth transitions. |

Pass if: All three zones per axis are clearly audible. Crossfades between adjacent zones are smooth, not abrupt.

Fail if: Any zone is silent or indistinguishable from another.

---

## 9. Bypass behavior

### 9.1 — Main knob bypass

*Source:* Any material.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, X = 50%, Y = 50%, **main = 0** | — |
| 2 | Listen | Loop plays completely unmodified — no degradation at all |
| 3 | Slowly turn main knob up | Degradation begins around main = 50 (bypass threshold) and increases with knob |

Pass if: Main knob at zero = frozen state. No change to buffer regardless of X/Y position.

### 9.2 — X knob bypass

*Source:* Any material.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, **X = 0**, Y = 80%, main = 100% | — |
| 2 | Listen | Only Y effects (destructive) audible. No harmonic effects present. |

Pass if: X at zero disables all harmonic effects. Y effects work independently.

### 9.3 — Y knob bypass

*Source:* Any material.

| Step | Action | Expected result |
|------|--------|-----------------|
| 1 | Z up, X = 80%, **Y = 0**, main = 100% | — |
| 2 | Listen | Only X effects (harmonic) audible. No destructive effects present. |

Pass if: Y at zero disables all destructive effects. X effects work independently.

---

## Quick smoke test (3 minutes)

If you're in a hurry, this minimal test catches the most critical bugs:

1. **Oxide:** Record chord → Z up, X=0, Y=25%, main=100% → dropouts within 30 seconds
2. **Saturation:** Z up, X=50%, Y=0, main=100% → sound gets warmer, not thinner
3. **Bit rot:** Z up, X=0, Y=90%, main=100% → subtle crackle, no explosive spikes
4. **Global warmth:** Z up, X=50%, Y=50%, main=100% → overall darker after 2 minutes versus dry reference

If all four pass, the most likely regressions are covered.

---

## Common failure patterns

| Symptom | Likely cause |
|---------|-------------|
| Oxide does nothing at any Y position | `probBase` or `damageLevel` always zero (shift amounts wrong) |
| Oxide dropouts are inaudible clicks | `MAX_DROPOUT_LEN` too small |
| Saturation makes sound thinner/brighter | HPF reinstated or LPF removed from dry path |
| Bit rot causes loud noise blasts | Bit positions 8-15 being flipped (should be 0-7) |
| Bit crush creates harsh aliasing | LPF removed or coefficient too high |
| Filter drift brightens at high X | HPF stage reinstated |
| Tape hiss is overwhelmingly loud | Noise mix ceiling too high or `>>` shift too small |
| All degradation paths trend brighter | Global tape LPF removed or coefficient wrong |
