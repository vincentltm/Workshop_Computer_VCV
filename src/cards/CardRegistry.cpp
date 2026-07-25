#include "CardRegistry.hpp"

std::vector<CardMetadata> g_card_registry;

void register_all_cards() {
    g_card_registry.clear();
    g_card_registry.push_back({
        "simple_midi",
        "Simple MIDI",
        "00",
        "Takes USB midi, sends it to pulse and CV outputs, also sends knob positions and CV inputs back to the computer as CC values.",
        "Tom Whitwell",
        true
    });
    g_card_registry.push_back({
        "turing_machine",
        "Turing Machine",
        "03",
        "Turing Machine with tap tempo clock, 2 x pulse outputs, 4 x CV outputs",
        "Tom Whitwell",
        true
    });
    g_card_registry.push_back({
        "byo_benjolin",
        "BYO Benjolin",
        "04",
        "Rungler, Chaotic VCO, Noise Source, Turing Machine, Quantizer",
        "Dune Desormeaux",
        true
    });
    g_card_registry.push_back({
        "usb_audio_bridge",
        "USB Audio Bridge",
        "06",
        "Direct hardware bridge for the Workshop System Computer USB Audio card",
        "Music Thing Modular",
        true
    });
    g_card_registry.push_back({
        "goldfish",
        "Goldfish",
        "11",
        "Weird delay/looper for audio and CV",
        "Dune Desormeaux",
        true
    });
    g_card_registry.push_back({
        "noisebox",
        "Noisebox",
        "13",
        "13-algorithm noise synth with CV modulation, sample-and-hold, and crusher mode",
        "Eric Gao",
        true
    });
    g_card_registry.push_back({
        "cvmod",
        "CVMod",
        "14",
        "Quad CV delay inspired by Make Noise Multimod",
        "Chris Johnson",
        true
    });
    g_card_registry.push_back({
        "mlrws",
        "MLRws",
        "15",
        "A remix of monome's classic MLR sample cutting platform (grid controller encouraged but optional). Diagrams here are for Gridless mode.",
        "Dune Desormeaux",
        true
    });
    g_card_registry.push_back({
        "ca_sequencer",
        "Cellular Automata Sequencer",
        "19",
        "16-cell gate and quantized CV melody generator inspired by NLC Cellular Automata, using CA rules 90 & 150 on a 4x4 grid",
        "Ainews",
        true
    });
    g_card_registry.push_back({
        "resonator",
        "Resonator",
        "21",
        "Karplus-Strong sympathetic resonator for droning and plucked textures, with chord & tanpura tunings, arpeggiator, pitch tracking and web-configurable CV/pulse outputs.",
        "Johan Eklund",
        true
    });
    g_card_registry.push_back({
        "sheep",
        "Sheep",
        "22",
        "A time-stretching and pitch-shifting granular processor and digital degradation playground with 2 fidelity options.",
        "Dune Desormeaux",
        true
    });
    g_card_registry.push_back({
        "crafted_volts",
        "Crafted Volts",
        "24",
        "Manually set control voltages (CV) with the input knobs and switch. It also attenuverts (attenuates and inverts) incoming voltages.",
        "Brian Dorsey",
        true
    });
    g_card_registry.push_back({
        "utility_pair",
        "Utility Pair",
        "25",
        "Workshop Computer Card",
        "Music Thing Modular",
        true
    });
    g_card_registry.push_back({
        "clockwork",
        "Clockwork",
        "26",
        "6-channel polyrhythmic clock, gate, and LFO/envelope generator inspired by Pamela's Workout.",
        "Vincent Maurer",
        true
    });
    g_card_registry.push_back({
        "siren",
        "Siren",
        "27",
        "Multi-algorithm drone oscillator. Inspired by the Forge TME Vhikk X.",
        "Moses Hoyt",
        true
    });
    g_card_registry.push_back({
        "eighties_bass",
        "Eighties Bass",
        "28",
        "Bass-oriented complete monosynth voice consisting of five detuned saw wave oscillators with mixable white noise and adjustable resonant filter.",
        "Tod Kurt (@todbot)",
        true
    });
    g_card_registry.push_back({
        "cirpy_wavetable",
        "Cirpy Wavetable",
        "30",
        "Wavetable oscillator that using wavetables from Plaits, Braids, and Microwave,",
        "Tod Kurt (@todbot)",
        true
    });
    g_card_registry.push_back({
        "drumdrum",
        "drumdrum",
        "33",
        "DFAM-style 8-step sequencer",
        "Moses Hoyt",
        true
    });
    g_card_registry.push_back({
        "dual_quant",
        "DualQuant",
        "34",
        "Dual quantised granular pitch shifter with calibrated 1V/oct CV outputs",
        "Adrian Vos",
        true
    });
    g_card_registry.push_back({
        "od",
        "Od",
        "38",
        "Loopable chaotic Lorenz attractor trajectories and zero-crossings as CV and pulses, with sensitivity to initial conditions.",
        "M. John Mills",
        true
    });
    g_card_registry.push_back({
        "blackbird",
        "Blackbird",
        "41",
        "A scriptable, live-codable, USB-serial-to-CV device implementing monome crow's protocol",
        "Dune Desormeaux",
        true
    });
    g_card_registry.push_back({
        "backyard_rain",
        "Backyard Rain",
        "42",
        "Nature soundscape audio. A cozy rain ambience mix for background listening. You control the intensity. This card plays rain ambience which was recorded in my backyard.",
        "Brian Dorsey",
        true
    });
    g_card_registry.push_back({
        "castle_process",
        "Castle Process",
        "43",
        "Fort Processor-inspired harsh noise processor with chopped external audio and a bass pulse voice",
        "Adrian Vos",
        true
    });
    g_card_registry.push_back({
        "birds",
        "Birds",
        "44",
        "Two birds sing to each other controlled by a Turing-style shift register sequencer with clock in and CV/pulse out.",
        "Tom Whitwell",
        true
    });
    g_card_registry.push_back({
        "flux",
        "Flux",
        "50",
        "Multi-FX and Synth Firmware",
        "WorkshopSystem",
        true
    });
    g_card_registry.push_back({
        "grains",
        "Grains",
        "51",
        "Granular Sampler and Effect",
        "Vincent Maurer",
        true
    });
    g_card_registry.push_back({
        "lens",
        "Lens",
        "52",
        "A programmable synth. Write patches in Loupe (a tiny Lisp); sequences of values read through lenses become pitch, rhythm, CV and audio.",
        "Graham Ritchie",
        true
    });
    g_card_registry.push_back({
        "glitter",
        "Glitter",
        "53",
        "Granular Looping Sampler",
        "Steve Jones",
        true
    });
    g_card_registry.push_back({
        "tapegrade",
        "Tapegrade",
        "54",
        "Mono-input stereo cassette warble processor with wow, flutter, hiss, crackle, and tape wear morphing.",
        "Adrian Vos",
        true
    });
    g_card_registry.push_back({
        "fifths",
        "Fifths",
        "55",
        "A quantizer/sequencer that can create harmony and nimbly traverse the circle of fifths in attempts to make jazz",
        "Dune Desormeaux",
        true
    });
    g_card_registry.push_back({
        "glitch",
        "Glitch",
        "57",
        "Clock-synced beat-repeater with ratcheting, reversal and audio degradation",
        "Andy Jenkinson (uglifruit)",
        true
    });
    g_card_registry.push_back({
        "lochovibes",
        "LoCho Vibes",
        "58",
        "Stereo chorus and vibrato effect featuring triangle, sine, and slow drift LFO modes, modulation-based delay movement, and tape-style saturation.",
        "Adrian Vos",
        true
    });
    g_card_registry.push_back({
        "bitphase",
        "BitPhase",
        "59",
        "Resonant 4-stage phaser with wide modulation sweeps, tremolo blending, and Burst-mode degradation",
        "Adrian Vos",
        true
    });
    g_card_registry.push_back({
        "stretchcore",
        "Stretchcore",
        "66",
        "A card for playing and manipulating samples with tempo control, timestretch with browser-based audio loading (infinitedigits.com/stretchcore/)",
        "Infinite Digits",
        true
    });
    g_card_registry.push_back({
        "fragments",
        "Fragments",
        "67",
        "Six-slot audio recorder and clocked fragment sequencer with browser librarian, MIDI pitch control, random CV outputs, and an alternate long-sample variation mode.",
        "Max Harnishfeger",
        true
    });
    g_card_registry.push_back({
        "degenerator",
        "Degenerator",
        "71",
        "Degenerator — Disintegrating Looper. Capture audio loops and apply irreversible degradation with 6 algorithms (Saturation, Filter Drift, Tape Hiss, Oxide Shedding, Bit Crush, Bit Rot) via preview/apply workflow. Inspired by William Basinski's The Disintegration Loops.",
        "Joep Vermaat",
        true
    });
    g_card_registry.push_back({
        "motorik",
        "Motorik",
        "72",
        "Motorik drum machine with kick/snare/hihat, bass and melody CV outputs and inputs. Classic Krautrock grooves.",
        "Joep Vermaat",
        true
    });
    g_card_registry.push_back({
        "wild_pebble",
        "Wild Pebble",
        "74",
        "MIDI-clockable generative rhythm and melody organism inspired by Pet Rock",
        "Adrian Vos with Vibecode support",
        true
    });
    g_card_registry.push_back({
        "turing_clouds",
        "Turing Clouds",
        "75",
        "Turing Machine-driven granular texture generator and rhythmic delay for the Workshop Computer",
        "Ainews",
        true
    });
    g_card_registry.push_back({
        "talker",
        "Talker",
        "78",
        "Proof of concept speech synthesizer, based on TalkiePCM, inspired by 1970s LPC speech synths.",
        "Chris Johnson",
        true
    });
    g_card_registry.push_back({
        "computer_grids",
        "Computer Grids",
        "82",
        "Grids-inspired trigger sequencer with Web MIDI SysEx configuration.",
        "Phil Miller",
        true
    });
    g_card_registry.push_back({
        "cosmik_c1zzl3",
        "Cosmik C1ZZL3",
        "84",
        "Stable phase-distortion synthesiser and Turing machine firmware with Web MIDI envelope readback, PD, detune, eight waveform families, hosted CZ patch import, USB MIDI device/host operation, and optional Turing MIDI output.",
        "Adrian Vos",
        true
    });
    g_card_registry.push_back({
        "fr330hfr33",
        "Fr330hfr33",
        "87",
        "Performance-focused acid voice with diode filtering, distortion, MIDI, and a persistent sequencer",
        "Adrian Vos",
        true
    });
    g_card_registry.push_back({
        "pantograph",
        "Pantograph",
        "90",
        "Trace and record CV — record knob movements, loop them at bipolar speed",
        "Kenny Shen",
        true
    });
    g_card_registry.push_back({
        "offair2",
        "OffAir",
        "95",
        "OffAir — AM/Shortwave/Longwave radio simulator. Tune between two Stations and interference with authentic heterodyne whistles, SSB pitch-shift detuning, AM envelope detection, swelling per-band static, and triggerable Insta-ference one-shots. Baked recordings or live audio inputs become the Stations.",
        "Andy Jenkinson (uglifruit)",
        true
    });
    g_card_registry.push_back({
        "alloy",
        "Alloy",
        "97",
        "Fixed-point 15-zone cross-modulator with a clocked Turing CV and gate companion.",
        "Eric Gao",
        true
    });
    g_card_registry.push_back({
        "duo_midi",
        "Duo MIDI",
        "98",
        "A duophonic midi device/host interface",
        "Dune Desormeaux",
        true
    });
    g_card_registry.push_back({
        "sense_of_space",
        "433 Sense of Space",
        "433",
        "Whimsical 4'33\\\\\"-inspired ambience looper with a three-loop transport, LED countdown, restlessness fidgets, Pulse In restart, and chair creak one-shot.",
        "Music Thing Modular / AI-assisted",
        true
    });
}
