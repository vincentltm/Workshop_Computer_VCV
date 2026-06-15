// ExtendedMetadata.hpp
// This file is dynamically generated. Do not edit manually.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace ExtendedMetadata {

struct PortMeta {
    std::string name;
    std::string description;
};

struct KnobContext {
    std::string z;        // "up", "middle", "down", "any"
    std::string gesture;  // e.g. "hold", "double-hold", or ""
    std::string mode;     // e.g. "bank-indicator" or ""
    std::string name;
    std::string description;
};

struct SwitchContext {
    std::string name;
    std::string description;
};

struct SwitchMeta {
    std::string id;
    SwitchContext up;
    SwitchContext middle;
    SwitchContext down;
};

struct CardMeta {
    std::string id;
    std::string name;
    std::string description;
    std::string creator;

    PortMeta inputs[6];
    PortMeta outputs[6];

    std::vector<KnobContext> knobs[3]; // 0=Main, 1=X, 2=Y
    SwitchMeta z_switch;
    bool has_switch_metadata = false;
};

inline const CardMeta* get_card_metadata(const std::string& card_id) {
    static const std::unordered_map<std::string, CardMeta> metadata_map = {
        {
            "simple_midi",
            {
                "simple_midi",
                "Simple Midi",
                "Takes USB midi, sends it to pulse and CV outputs, also sends knob positions and CV inputs back to the computer as CC values.",
                "Tom Whitwell",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "bitphase",
            {
                "bitphase",
                "BitPhase",
                "experimental phaser/tremolo with bit destruction",
                "Adrian Vos",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "voices_of_sid",
            {
                "voices_of_sid",
                "Voices Of Sid",
                "Dual MOS 6581 SID emulation (reSID engine) with CV/gate control, stereo output, waveform selection, and randomize",
                "Joep Vermaat",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "compulidean",
            {
                "compulidean",
                "Compulidean",
                "Generative Euclidean drum + sample player.",
                "Tristan Rowley",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "usb_audio_bridge",
            {
                "usb_audio_bridge",
                "USB Audio & MIDI",
                "6-Channel USB Audio & MIDI firmware with CV/Gate support",
                "Vincent Maurer (vincentmaurer.de)",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "crafted_volts",
            {
                "crafted_volts",
                "Crafted Volts",
                "Manually set control voltages (CV) with the input knobs and switch. It also attenuverts (attenuates and inverts) incoming voltages.",
                "Brian Dorsey",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "eighties_bass",
            {
                "eighties_bass",
                "Eighties Bass",
                "Bass-oriented complete monosynth voice consisting of five detuned saw wave oscillators with mixable white noise and adjustable resonant filter.",
                "@todbot / Tod Kurt",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "cirpy_wavetable",
            {
                "cirpy_wavetable",
                "Cirpy Wavetable",
                "Wavetable oscillator that using wavetables from Plaits, Braids, and Microwave,",
                "@todbot / Tod Kurt",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "od",
            {
                "od",
                "Od",
                "Loopable chaotic Lorenz attractor trajectories and zero-crossings as CV and pulses, with sensitivity to initial conditions.",
                "M. John Mills",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "bends",
            {
                "bends",
                "Bends",
                "Stereo Multi-FX, Glitch, and Codec Demolisher Card",
                "Vincent Maurer (vincentmaurer.de) with Advanced Agentic Coding",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "backyard_rain",
            {
                "backyard_rain",
                "Backyard Rain",
                "Nature soundscape audio. A cozy rain ambience mix for background listening. You control the intensity. This card plays rain ambience which was recorded in my backyard.",
                "Brian Dorsey",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "utility_pair",
            {
                "utility_pair",
                "Utility Pair",
                "25 small utilities, which can be combined in pairs",
                "Chris Johnson",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "turing_machine",
            {
                "turing_machine",
                "Turing Machine",
                "Turing Machine with tap tempo clock, 2 x pulse outputs, 4 x CV outputs",
                "Tom Whitwell",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "vink",
            {
                "vink",
                "Vink",
                "Dual delay loops with sigmoid saturation for Jaap Vink / Roland Kayn style feedback patching",
                "Ben Regnier",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "siren",
            {
                "siren",
                "Siren",
                "Multi-algorithm drone oscillator. Inspired by the Forge TME Vhikk X.",
                "Moses Hoyt",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "grains",
            {
                "grains",
                "Grains",
                "Granular Sampler and Effect",
                "Vincent Maurer (vincentmaurer.de)",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "reverb",
            {
                "reverb",
                "Reverb+",
                "Reverb effect, plus pulse/CV generators and MIDI-to-CV, configurable using web interface.",
                "Chris Johnson",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "flux",
            {
                "flux",
                "Flux",
                "Multi-FX and Synth Firmware",
                "WorkshopSystem",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "chord_organ",
            {
                "chord_organ",
                "Chord Organ",
                "Chord Organ-ish - 16 chords, 8 voices, 1V/oct root. Inspired by Music Thing Chord Organ.",
                "jkeyworth",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "resonator",
            {
                "resonator",
                "Resonator",
                "Karplus-Strong based sympathetic resonator. Can be used for resonant droning as well as plucking sounds.",
                "Johan Eklund",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "fifths",
            {
                "fifths",
                "Fifths",
                "A quantizer/sequencer that can create harmony and nimbly traverse the circle of fifths in attempts to make jazz",
                "Dune Desormeaux",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "computer_grids",
            {
                "computer_grids",
                "Computer Grids",
                "Grids-inspired trigger sequencer with Web MIDI SysEx configuration.",
                "Phil Miller",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "byo_benjolin",
            {
                "byo_benjolin",
                "Byo Benjolin",
                "Rungler, Chaotic VCO, Noise Source, Turing Machine, Quantizer",
                "Dune Desormeaux",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "goldfish",
            {
                "goldfish",
                "Goldfish",
                "Weird delay/looper for audio and CV",
                "Dune Desormeaux",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "bumpers",
            {
                "bumpers",
                "Bumpers",
                "Bouncing ball' style delay and trigger generators",
                "Chris Johnson",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "sheep",
            {
                "sheep",
                "Sheep",
                "A time-stretching and pitch-shifting granular processor and digital degradation playground with 2 fidelity options.",
                "Dune Desormeaux",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "knots",
            {
                "knots",
                "Knots",
                "Six-engine oscillator firmware for the Music Thing Workshop System",
                "Jeff Fletcher",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "chord_blimey",
            {
                "chord_blimey",
                "Chord Blimey",
                "Generates CV/Pulse arpeggios",
                "Tom Waters",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "noisebox",
            {
                "noisebox",
                "Noisebox",
                "Workshop Computer Card",
                "Music Thing Modular",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "cvmod",
            {
                "cvmod",
                "CVMod",
                "Quad CV delay inspired by Make Noise Multimod",
                "Chris Johnson",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "esp",
            {
                "esp",
                "Esp",
                "A MS-20-style External Signal Processor that includes a preamp, bandpass filter, envelope follower, gate, and 1v/oct pitch outs.",
                "Ben Regnier",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "modes",
            {
                "modes",
                "Modes",
                "Physical Modeling Voice (Mutable Instruments Elements port)",
                "Vincent Maurer",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "rompler",
            {
                "rompler",
                "Rompler",
                "General MIDI SF2 Polyphonic Multisampler",
                "Vincent Maurer & Antigravity",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "trace",
            {
                "trace",
                "Trace",
                "Oscillograph stereo oscillator",
                "Ruiyang Wang",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "talker",
            {
                "talker",
                "Talker",
                "Proof of concept speech synthesizer, based on TalkiePCM, inspired by 1970s LPC speech synths.",
                "Chris Johnson",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "bytebeat",
            {
                "bytebeat",
                "Bytebeat",
                "Generates and mangles bytebeats",
                "Matt Kuebrich",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "twists",
            {
                "twists",
                "Twists",
                "A port of Mutable Instruments Braids with a web editor",
                "Random Works",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "dual_quant",
            {
                "dual_quant",
                "Dual Quant",
                "Dual quantised granular pitch shifter with calibrated 1V/oct CV outputs",
                "Adrian Vos - with Vibe code support",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "birds",
            {
                "birds",
                "Birds",
                "Two birds sing to each other controlled by a Turing-style shift register sequencer with clock in and CV/pulse out.",
                "Tom Whitwell",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "tapegrade",
            {
                "tapegrade",
                "Tapegrade",
                "Mono-input stereo cassette warble processor with wow, flutter, hiss, crackle, and tape wear morphing.",
                "Adrian Vos",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "glitch",
            {
                "glitch",
                "Glitch",
                "Clock-synced beat-repeater with ratcheting, reversal and audio degradation",
                "Andy Jenkinson (uglifruit)",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "lochovibes",
            {
                "lochovibes",
                "LoChoVibes",
                "Stereo chorus and vibrato effect featuring triangle, sine, and slow drift LFO modes, modulation-based delay movement, and tape-style saturation.",
                "Music Thing Modular",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "markov",
            {
                "markov",
                "Markov",
                "Dual generative Markov chain module — evolving melody (MarkoV) left side, rhythmic percussion patterns (MarkovPerc) right side, with internal synth voice",
                "Andy Jenkinson (uglifruit)",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "stretchcore",
            {
                "stretchcore",
                "Stretchcore",
                "A card for playing and manipulating samples with tempo control, timestretch with browser-based audio loading (infinitedigits.com/stretchcore/)",
                "Infinite Digits",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "wild_pebble",
            {
                "wild_pebble",
                "Wild Pebble",
                "Playable generative rhythm and melody organism inspired by Pet Rock",
                "Adrian Vos with Vibecode support",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "divcom",
            {
                "divcom",
                "Divcom",
                "Comparator and VC clock divider, inspired by Serge NCOM",
                "divmod",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "am_coupler",
            {
                "am_coupler",
                "AM Coupler",
                "AM radio transmitter / coupler",
                "Chris Johnson",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "slowmod",
            {
                "slowmod",
                "Slowmod",
                "Chaotic quad-LFO with VCAs",
                "divmod",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "nzt",
            {
                "nzt",
                "Nzt",
                "Grain Noise and Noise Tools",
                "@kjnilsson",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "glitter",
            {
                "glitter",
                "Glitter",
                "Granular Looping Sampler",
                "Steve Jones",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "degenerator",
            {
                "degenerator",
                "Degenerator",
                "Degenerator — Disintegrating Looper. Capture audio loops and apply irreversible degradation with 6 algorithms (Saturation, Filter Drift, Tape Hiss, Oxide Shedding, Bit Crush, Bit Rot) via preview/apply workflow. Inspired by William Basinski's The Disintegration Loops.",
                "Joep Vermaat",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "motorik",
            {
                "motorik",
                "Motorik",
                "Motorik drum machine with kick/snare/hihat, bass and melody CV outputs and inputs. Classic Krautrock grooves.",
                "Joep Vermaat",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "tesserae",
            {
                "tesserae",
                "Tesserae",
                "Tesserae — Variable-voice (2-8) arpeggiated chord generator with 5 patterns, 10 scales, tap tempo, CV/audio transpose inputs, and dual CV + audio pitch outputs. Inspired by Laurie Spiegel's Music Mouse and Patchwork.",
                "MTM Community",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "toolbox",
            {
                "toolbox",
                "Toolbox",
                "Mixer, VCA, noise, S&H, clock generator, etc.",
                "divmod",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "mlrws",
            {
                "mlrws",
                "Mlrws",
                "A remix of monome's classic MLR sample cutting platform (grid controller encouraged but optional)",
                "Dune Desormeaux",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "drumdrum",
            {
                "drumdrum",
                "Drumdrum",
                "DFAM-style 8-step sequencer",
                "Moses Hoyt",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "blackbird",
            {
                "blackbird",
                "Blackbird",
                "A scriptable, live-codable, USB-serial-to-CV device implementing monome crow's protocol",
                "Dune Desormeaux",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "krell",
            {
                "krell",
                "Krell",
                "Krell",
                "Benjamin Reily",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
        {
            "duo_midi",
            {
                "duo_midi",
                "Duo Midi",
                "A duophonic midi device/host interface",
                "Dune Desormeaux",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Switch position Up" },
                    { "Middle", "Switch position Middle" },
                    { "Down", "Switch position Down" }
                },
                false
            }
        },
    };

    auto it = metadata_map.find(card_id);
    if (it != metadata_map.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace ExtendedMetadata
