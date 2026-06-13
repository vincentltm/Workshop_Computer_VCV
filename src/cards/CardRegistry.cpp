#include "CardRegistry.hpp"

std::vector<CardMetadata> g_card_registry;

void register_all_cards() {
    g_card_registry.clear();
    g_card_registry.push_back({
        "simple_midi",
        "Simple Midi",
        "00",
        "Takes USB midi, sends it to pulse and CV outputs, also sends knob positions and CV inputs back to the computer as CC values.",
        "Tom Whitwell"
    });
    g_card_registry.push_back({
        "turing_machine",
        "Turing Machine",
        "03",
        "Turing Machine with tap tempo clock, 2 x pulse outputs, 4 x CV outputs",
        "Tom Whitwell"
    });
    g_card_registry.push_back({
        "byo_benjolin",
        "Byo Benjolin",
        "04",
        "Rungler, Chaotic VCO, Noise Source, Turing Machine, Quantizer",
        "Dune Desormeaux"
    });
    g_card_registry.push_back({
        "chord_blimey",
        "Chord Blimey",
        "05",
        "Generates CV/Pulse arpeggios",
        "Tom Waters"
    });
    g_card_registry.push_back({
        "usb_audio_bridge",
        "USB Audio Bridge",
        "06",
        "Direct hardware bridge for the Workshop System Computer USB Audio card",
        "Music Thing Modular"
    });
    g_card_registry.push_back({
        "bumpers",
        "Bumpers",
        "07",
        "Bouncing ball' style delay and trigger generators",
        "Chris Johnson"
    });
    g_card_registry.push_back({
        "bytebeat",
        "Bytebeat",
        "08",
        "Workshop Computer Card",
        "Music Thing Modular"
    });
    g_card_registry.push_back({
        "divcom",
        "Divcom",
        "09",
        "Workshop Computer Card",
        "Music Thing Modular"
    });
    g_card_registry.push_back({
        "twists",
        "Twists",
        "10",
        "A port of Mutable Instruments Braids with a web editor",
        "Random Works"
    });
    g_card_registry.push_back({
        "goldfish",
        "Goldfish",
        "11",
        "Weird delay/looper for audio and CV",
        "Dune Desormeaux"
    });
    g_card_registry.push_back({
        "am_coupler",
        "AM Coupler",
        "12",
        "AM radio transmitter / coupler",
        "Chris Johnson"
    });
    g_card_registry.push_back({
        "noisebox",
        "Noisebox",
        "13",
        "Workshop Computer Card",
        "Music Thing Modular"
    });
    g_card_registry.push_back({
        "cvmod",
        "CVMod",
        "14",
        "Quad CV delay inspired by Make Noise Multimod",
        "Chris Johnson"
    });
    g_card_registry.push_back({
        "mlrws",
        "Mlrws",
        "15",
        "A remix of monome's classic MLR sample cutting platform (grid controller encouraged but optional)",
        "Dune Desormeaux"
    });
    g_card_registry.push_back({
        "chord_organ",
        "Chord Organ",
        "18",
        "Chord Organ-ish - 16 chords, 8 voices, 1V/oct root. Inspired by Music Thing Chord Organ.",
        "jkeyworth"
    });
    g_card_registry.push_back({
        "reverb",
        "Reverb+",
        "20",
        "Reverb effect, plus pulse/CV generators and MIDI-to-CV, configurable using web interface.",
        "Chris Johnson"
    });
    g_card_registry.push_back({
        "resonator",
        "Resonator",
        "21",
        "Karplus-Strong based sympathetic resonator. Can be used for resonant droning as well as plucking sounds.",
        "Johan Eklund"
    });
    g_card_registry.push_back({
        "sheep",
        "Sheep",
        "22",
        "A time-stretching and pitch-shifting granular processor and digital degradation playground with 2 fidelity options.",
        "Dune Desormeaux"
    });
    g_card_registry.push_back({
        "slowmod",
        "Slowmod",
        "23",
        "Workshop Computer Card",
        "Music Thing Modular"
    });
    g_card_registry.push_back({
        "crafted_volts",
        "Crafted Volts",
        "24",
        "Manually set control voltages (CV) with the input knobs and switch. It also attenuverts (attenuates and inverts) incoming voltages.",
        "Brian Dorsey"
    });
    g_card_registry.push_back({
        "utility_pair",
        "Utility Pair",
        "25",
        "Workshop Computer Card",
        "Music Thing Modular"
    });
    g_card_registry.push_back({
        "siren",
        "Siren",
        "27",
        "Multi-algorithm drone oscillator. Inspired by the Forge TME Vhikk X.",
        "Moses Hoyt"
    });
    g_card_registry.push_back({
        "eighties_bass",
        "Eighties Bass",
        "28",
        "Bass-oriented complete monosynth voice consisting of five detuned saw wave oscillators with mixable white noise and adjustable resonant filter.",
        "@todbot / Tod Kurt"
    });
    g_card_registry.push_back({
        "cirpy_wavetable",
        "Cirpy Wavetable",
        "30",
        "Wavetable oscillator that using wavetables from Plaits, Braids, and Microwave,",
        "@todbot / Tod Kurt"
    });
    g_card_registry.push_back({
        "esp",
        "Esp",
        "31",
        "A MS-20-style External Signal Processor that includes a preamp, bandpass filter, envelope follower, gate, and 1v/oct pitch outs.",
        "Ben Regnier"
    });
    g_card_registry.push_back({
        "vink",
        "Vink",
        "32",
        "Dual delay loops with sigmoid saturation for Jaap Vink / Roland Kayn style feedback patching",
        "Ben Regnier"
    });
    g_card_registry.push_back({
        "drumdrum",
        "Drumdrum",
        "33",
        "DFAM-style 8-step sequencer",
        "Moses Hoyt"
    });
    g_card_registry.push_back({
        "dual_quant",
        "Dual Quant",
        "34",
        "Dual quantised granular pitch shifter with calibrated 1V/oct CV outputs",
        "Adrian Vos - with Vibe code support"
    });
    g_card_registry.push_back({
        "od",
        "Od",
        "38",
        "Loopable chaotic Lorenz attractor trajectories and zero-crossings as CV and pulses, with sensitivity to initial conditions.",
        "M. John Mills"
    });
    g_card_registry.push_back({
        "knots",
        "Knots",
        "39",
        "Workshop Computer Card",
        "Music Thing Modular"
    });
    g_card_registry.push_back({
        "blackbird",
        "Blackbird",
        "41",
        "A scriptable, live-codable, USB-serial-to-CV device implementing monome crow's protocol",
        "Dune Desormeaux"
    });
    g_card_registry.push_back({
        "backyard_rain",
        "Backyard Rain",
        "42",
        "Nature soundscape audio. A cozy rain ambience mix for background listening. You control the intensity. This card plays rain ambience which was recorded in my backyard.",
        "Brian Dorsey"
    });
    g_card_registry.push_back({
        "birds",
        "Birds",
        "44",
        "Two birds sing to each other controlled by a Turing-style shift register sequencer with clock in and CV/pulse out.",
        "Tom Whitwell"
    });
    g_card_registry.push_back({
        "bends",
        "Bends",
        "45",
        "Stereo Multi-FX, Glitch, and Codec Demolisher Card",
        "Vincent Maurer (vincentmaurer.de) with Advanced Agentic Coding"
    });
    g_card_registry.push_back({
        "rompler",
        "Rompler",
        "46",
        "General MIDI SF2 Polyphonic Multisampler",
        "Vincent Maurer & Antigravity"
    });
    g_card_registry.push_back({
        "nzt",
        "Nzt",
        "47",
        "Workshop Computer Card",
        "Music Thing Modular"
    });
    g_card_registry.push_back({
        "modes",
        "Modes",
        "49",
        "Physical Modeling Voice (Mutable Instruments Elements port)",
        "Vincent Maurer"
    });
    g_card_registry.push_back({
        "flux",
        "Flux",
        "50",
        "Multi-FX and Synth Firmware",
        "WorkshopSystem"
    });
    g_card_registry.push_back({
        "grains",
        "Grains",
        "51",
        "Granular Sampler and Effect",
        "Vincent Maurer (vincentmaurer.de)"
    });
    g_card_registry.push_back({
        "glitter",
        "Glitter",
        "53",
        "Granular Looping Sampler",
        "Steve Jones"
    });
    g_card_registry.push_back({
        "tapegrade",
        "Tapegrade",
        "54",
        "Mono-input stereo cassette warble processor with wow, flutter, hiss, crackle, and tape wear morphing.",
        "Adrian Vos"
    });
    g_card_registry.push_back({
        "fifths",
        "Fifths",
        "55",
        "A quantizer/sequencer that can create harmony and nimbly traverse the circle of fifths in attempts to make jazz",
        "Dune Desormeaux"
    });
    g_card_registry.push_back({
        "krell",
        "Krell",
        "56",
        "Krell",
        "Benjamin Reily"
    });
    g_card_registry.push_back({
        "glitch",
        "Glitch",
        "57",
        "Clock-synced beat-repeater with ratcheting, reversal and audio degradation",
        "Andy Jenkinson (uglifruit)"
    });
    g_card_registry.push_back({
        "lochovibes",
        "LoChoVibes",
        "58",
        "Stereo chorus and vibrato effect featuring triangle, sine, and slow drift LFO modes, modulation-based delay movement, and tape-style saturation.",
        "Music Thing Modular"
    });
    g_card_registry.push_back({
        "markov",
        "Markov",
        "60",
        "Dual generative Markov chain module — evolving melody (MarkoV) left side, rhythmic percussion patterns (MarkovPerc) right side, with internal synth voice",
        "Andy Jenkinson (uglifruit)"
    });
    g_card_registry.push_back({
        "stretchcore",
        "Stretchcore",
        "66",
        "A card for playing and manipulating samples with tempo control, timestretch with browser-based audio loading (infinitedigits.com/stretchcore/)",
        "Infinite Digits"
    });
    g_card_registry.push_back({
        "trace",
        "Trace",
        "69",
        "Oscillograph stereo oscillator",
        "Ruiyang Wang"
    });
    g_card_registry.push_back({
        "degenerator",
        "Degenerator",
        "71",
        "Degenerator — Disintegrating Looper. Capture audio loops and apply irreversible degradation with 6 algorithms (Saturation, Filter Drift, Tape Hiss, Oxide Shedding, Bit Crush, Bit Rot) via preview/apply workflow. Inspired by William Basinski's The Disintegration Loops.",
        "Joep Vermaat"
    });
    g_card_registry.push_back({
        "motorik",
        "Motorik",
        "72",
        "Motorik drum machine with kick/snare/hihat, bass and melody CV outputs and inputs. Classic Krautrock grooves.",
        "Joep Vermaat"
    });
    g_card_registry.push_back({
        "wild_pebble",
        "Wild Pebble",
        "74",
        "Playable generative rhythm and melody organism inspired by Pet Rock",
        "Adrian Vos with Vibecode support"
    });
    g_card_registry.push_back({
        "talker",
        "Talker",
        "78",
        "Proof of concept speech synthesizer, based on TalkiePCM, inspired by 1970s LPC speech synths.",
        "Chris Johnson"
    });
    g_card_registry.push_back({
        "computer_grids",
        "Computer Grids",
        "82",
        "Grids-inspired trigger sequencer with Web MIDI SysEx configuration.",
        "Phil Miller"
    });
    g_card_registry.push_back({
        "tesserae",
        "Tesserae",
        "86",
        "Tesserae — Variable-voice (2-8) arpeggiated chord generator with 5 patterns, 10 scales, tap tempo, CV/audio transpose inputs, and dual CV + audio pitch outputs. Inspired by Laurie Spiegel's Music Mouse and Patchwork.",
        "MTM Community"
    });
    g_card_registry.push_back({
        "duo_midi",
        "Duo Midi",
        "98",
        "A duophonic midi device/host interface",
        "Dune Desormeaux"
    });
    g_card_registry.push_back({
        "toolbox",
        "Toolbox",
        "99",
        "Workshop Computer Card",
        "Music Thing Modular"
    });
}
