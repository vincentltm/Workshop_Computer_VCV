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
                "Simple MIDI",
                "Takes USB midi, sends it to pulse and CV outputs, also sends knob positions and CV inputs back to the computer as CC values.",
                "Tom Whitwell",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV CC Source 1", "CV input sampled and sent to host as MIDI CC data" },
                    { "CV CC Source 2", "CV input sampled and sent to host as MIDI CC data" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "MIDI Pitch CV 1", "Channel 1 MIDI notes are converted to calibrated pitch CV" },
                    { "MIDI Pitch CV 2", "Channel 2 MIDI notes are converted to calibrated pitch CV" },
                    { "Gate 1", "Gate output for MIDI note events on channel 1" },
                    { "Gate 2", "Gate output for MIDI note events on channel 2" },
                },
                {
                    {
                        { "middle", "", "", "MIDI CC Source (Main)", "Main knob position is sent to host as MIDI CC" },
                        { "down", "hold", "", "Enter Calibration", "Hold switch at startup to enter CV output calibration mode" },
                    },
                    {
                        { "middle", "", "", "MIDI CC Source (X)", "X knob position is sent to host as MIDI CC" },
                    },
                    {
                        { "middle", "", "", "MIDI CC Source (Y)", "Y knob position is sent to host as MIDI CC" },
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
                    { "Audio Input 1", "Physical input channel routable to USB audio and/or MIDI processing" },
                    { "Audio Input 2", "Physical input channel routable to USB audio and/or MIDI processing" },
                    { "CV Input 1", "Configurable as pitch or CC source in alt mode" },
                    { "CV Input 2", "Configurable as pitch or CC source in alt mode" },
                    { "Pulse Input 1", "Configurable gate input (default associated with CV 1)" },
                    { "Pulse Input 2", "Configurable gate/clock input (default clock/run role)" },
                },
                {
                    { "Audio Output 1", "Physical output channel routable from USB stream or mode logic" },
                    { "Audio Output 2", "Physical output channel routable from USB stream or mode logic" },
                    { "CV Output 1", "Configurable pitch/CV destination in alt mode" },
                    { "CV Output 2", "Configurable CC/CV destination in alt mode" },
                    { "Pulse Output 1", "Configurable gate/trigger/clock output" },
                    { "Pulse Output 2", "Configurable gate/trigger/clock output" },
                },
                {
                    {
                        { "middle", "", "", "MIDI CC Source (Main)", "Sends configurable CC number (default CC1) in MIDI-enabled modes" },
                    },
                    {
                        { "middle", "", "", "MIDI CC Source (X)", "Sends configurable CC number (default CC2) in MIDI-enabled modes" },
                    },
                    {
                        { "middle", "", "", "MIDI CC Source (Y)", "Sends configurable CC number (default CC3) in MIDI-enabled modes" },
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
                    { "Audio/CV Input A", "Mixed with AudioIn2 when present, then attenuverted by Main knob" },
                    { "Audio/CV Input B", "Mixed with AudioIn1 when present, then attenuverted by Main knob" },
                    { "CV Input X", "Replaces X knob source and is attenuverted by X position" },
                    { "CV Input Y", "Replaces Y knob source and is attenuverted by Y position" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Main Voltage Output", "Main knob voltage (or mixed audio input voltage) after attenuverting" },
                    { "Inverted Main Voltage Output", "Inverted copy of AudioOut1" },
                    { "X Voltage Output", "X knob voltage or attenuverted CVIn1" },
                    { "Y Voltage Output", "Y knob voltage or attenuverted CVIn2" },
                    { "Z High Gate", "High when Z is momentary or up; low when Z is middle" },
                    { "Complement Gate", "Inverse gate state of PulseOut1" },
                },
                {
                    {
                        { "middle", "", "", "Main Attenuverter", "Sets output voltage or scales mixed audio inputs" },
                        { "up", "", "", "Gate High State", "Sets PulseOut1 high and PulseOut2 low" },
                        { "down", "momentary", "", "Momentary Gate High", "Temporarily sets PulseOut1 high and PulseOut2 low" },
                    },
                    {
                        { "middle", "", "", "CV1 Attenuverter", "Sets output voltage or scales CVIn1" },
                    },
                    {
                        { "middle", "", "", "CV2 Attenuverter", "Sets output voltage or scales CVIn2" },
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
                "Tod Kurt (@todbot)",
                {
                    { "Detune Modulation", "Controls oscillator detune spread amount" },
                    { "Noise Mix Modulation", "Controls amount of white noise mixed with oscillator signal" },
                    { "Pitch CV", "Added to X knob offset for oscillator pitch (intended V/oct behavior) # certainty: medium (README says \"maybe\")" },
                    { "Cutoff Modulation", "Bipolar additive modulation of filter cutoff" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio Left", "Main synthesized output (mirrored)" },
                    { "Audio Right", "Main synthesized output (mirrored)" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "up", "", "", "Filter Cutoff", "Cutoff frequency for selected filter mode" },
                        { "down", "momentary", "", "Filter Mode Cycle", "Press switch to cycle LPF, BPF, and HPF modes" },
                    },
                    {
                        { "up", "", "", "Pitch Offset", "Pitch transpose offset summed with CV In 1" },
                    },
                    {
                        { "up", "", "", "Filter Resonance", "Resonance/Q amount for selected filter mode" },
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
                "Tod Kurt (@todbot)",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "Pitch CV", "Pitch control input mapped to a MIDI-note range (approximate 1V/oct behavior)" },
                    { "Wavetable Position Mod", "Adds bipolar modulation to wavetable position" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "Wavetable Position CV", "Reflects current wavetable position value" },
                    { "LFO Modulation CV", "Reflects current wavemod LFO value" },
                    { "PWM Audio Out A", "Pulse output configured as audio output stream" },
                    { "PWM Audio Out B", "Pulse output configured as mirrored audio output stream" },
                },
                {
                    {
                        { "middle", "", "", "Wavetable Position", "Selects/morphs current wave position within the loaded table" },
                        { "down", "momentary", "", "Next Wavetable", "Loads next WAV file in the wavetable folder" },
                        { "up", "momentary", "", "Quantize Toggle", "Toggles CV In 1 pitch quantization on/off" },
                    },
                    {
                        { "middle", "", "", "LFO Amount", "Amount of triangle LFO applied to wavetable position" },
                    },
                    {
                        { "middle", "", "", "LFO Rate", "Frequency of wavetable modulation LFO" },
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
            "freq_shift",
            {
                "freq_shift",
                "Freq Shift",
                "Dual Input Frequency Shifter for Feedback Experimentation",
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
                    { "Left Utility Signal Input", "Left-side input routed according to selected left utility" },
                    { "Right Utility Signal Input", "Right-side input routed according to selected right utility" },
                    { "Left Utility CV Input", "Left-side CV input routed according to selected left utility" },
                    { "Right Utility CV Input", "Right-side CV input routed according to selected right utility" },
                    { "Left Utility Trigger Input", "Left-side pulse input routed according to selected left utility" },
                    { "Right Utility Trigger Input", "Right-side pulse input routed according to selected right utility" },
                },
                {
                    { "Left Utility Signal Output", "Left-side output generated by selected left utility" },
                    { "Right Utility Signal Output", "Right-side output generated by selected right utility" },
                    { "Left Utility CV Output", "Left-side CV output generated by selected left utility" },
                    { "Right Utility CV Output", "Right-side CV output generated by selected right utility" },
                    { "Left Utility Pulse Output", "Left-side pulse output generated by selected left utility" },
                    { "Right Utility Pulse Output", "Right-side pulse output generated by selected right utility" },
                },
                {
                    {
                        { "middle", "", "", "Left Utility Secondary Control", "Main knob is generally owned by left utility" },
                        { "any", "", "", "Utility Selector (multi-pack firmware)", "At boot-select mode, Main chooses left or right utility slot to edit" },
                        { "up", "", "", "Run Selected Pair", "In selector mode, move Z up to start selected utility pair" },
                        { "down", "hold", "", "Enter Selector Mode", "Hold Z down on boot/reset to choose utility pair in multi-pack firmware" },
                    },
                    {
                        { "middle", "", "", "Left Utility Primary Control", "X knob is generally owned by left utility" },
                        { "any", "", "", "Left Utility Select", "In selector mode, chooses the left utility" },
                    },
                    {
                        { "middle", "", "", "Right Utility Primary Control", "Y knob is generally owned by right utility" },
                        { "any", "", "", "Right Utility Select", "In selector mode, chooses the right utility" },
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
                    { "Reset", "Rising edge resets all sequence states to step 1" },
                    { "Preset Select CV", "Experimental CV control of the two-preset mode switch" },
                    { "Diviply CV", "Positive/negative control for divide-multiply rate on channel 2" },
                    { "Pitch Offset CV", "Experimental quantized offset applied to both channels" },
                    { "External Clock 1", "Replaces tap tempo and drives the main clock" },
                    { "External Clock 2", "Overrides channel 2 diviply clock for independently clocked channel 2" },
                },
                {
                    { "Channel 1 DAC CV", "Scaled DAC signal for channel 1; output range configurable in editor" },
                    { "Channel 2 DAC CV", "Scaled DAC signal for channel 2; output range configurable in editor" },
                    { "Channel 1 Quantized CV", "Quantized pitch CV output for channel 1" },
                    { "Channel 2 Quantized CV", "Quantized pitch CV output for channel 2" },
                    { "Channel 1 Pulse", "Clock or Turing-bit pulse behavior depending on pulse mode configuration" },
                    { "Channel 2 Pulse", "Clock or Turing-bit pulse behavior depending on pulse mode configuration" },
                },
                {
                    {
                        { "middle", "", "", "Randomness / Write", "Drives the main Turing machine write behavior for both channels" },
                        { "down", "momentary", "", "Tap Tempo", "Tap switch sets internal BPM when no external clock is present" },
                    },
                    {
                        { "middle", "", "", "Loop Length", "Sets channel 1 sequence length; channel 2 length can be offset" },
                    },
                    {
                        { "middle", "", "", "Divide/Multiply", "Sets channel 2 divide/multiply clock relationship" },
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
                    { "Audio Input 1", "Delay source input (or left source in split mode)" },
                    { "Audio Input 2", "Optional second source; summed in shared mode or routed to tap 2 in split mode" },
                    { "Tap 1 Time Mod", "Modulates delay time for tap 1, averaged with Main knob target" },
                    { "Tap 2 Time Mod", "Modulates delay time for tap 2, averaged with Main+X target" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Delay Tap 1 / Mono Mix", "Tap 1 output in split mode, or mono sum in shared mode" },
                    { "Delay Tap 2 / Mono Mix", "Tap 2 output in split mode, or mono sum in shared mode" },
                    { "Chaos CV A", "Slow chaotic CV from internal logistic-map LFO" },
                    { "Chaos CV B", "Slow chaotic CV from second logistic-map LFO" },
                    { "Tap 1 Period Pulse", "Pulse train based on tap 1 delay period" },
                    { "Tap 2 Period Pulse", "Pulse train based on tap 2 delay period" },
                },
                {
                    {
                        { "up", "", "", "Base Delay Time", "Sets shared center delay time for both taps" },
                    },
                    {
                        { "up", "", "", "Tap 2 Offset", "Offsets tap 2 delay relative to tap 1" },
                    },
                    {
                        { "up", "", "", "Saturation Mix", "Crossfades dry delayed signal to sigmoid-saturated signal" },
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
                    { "Processor Input", "External audio processed by Siren's shaping path and summed with the drone # certainty: high (README + code)" },
                    { "Span Modulation", "External CV/audio modulation for oscillator spread (SPAN) # certainty: high (README + code)" },
                    { "Pitch Modulation", "Added to BASIS/root pitch control" },
                    { "Warp Modulation", "Modulates WARP amount" },
                    { "Gate", "Opens/closes the drone envelope" },
                    { "Seed / Bank Trigger", "Short pulse randomizes SEED, long hold cycles oscillator bank # certainty: high (README + code)" },
                },
                {
                    { "Left Drone Output", "Left channel of stereo Siren output (plus processed input, if patched)" },
                    { "Right Drone Output", "Right channel of stereo Siren output (plus processed input, if patched)" },
                    { "Pitch CV Mirror", "Mirrors current basis pitch value" },
                    { "Envelope CV", "Envelope level derived from Siren gate state" },
                    { "Sub-osc Clock", "Square clock derived from oscillator phase" },
                    { "Divide-by-2 Clock", "Divider pulse derived from PulseOut1 phase edges" },
                },
                {
                    {
                        { "up", "", "", "Warp", "Cross-mod / distortion intensity (bank-dependent behavior)" },
                        { "middle", "", "", "Seed", "Structural randomization parameter" },
                        { "down", "momentary", "", "Bank Cycle", "Tap switch down to cycle to the next oscillator bank with crossfade" },
                    },
                    {
                        { "up", "", "", "Span", "Oscillator spread / detuning amount (bank-dependent behavior)" },
                        { "middle", "", "", "Scan", "Timbral scan parameter" },
                    },
                    {
                        { "up", "", "", "Morph", "Waveform and timbral scan parameter (bank-dependent behavior)" },
                        { "middle", "", "", "Basis", "Root pitch / frequency basis (also modulated by CV In 1)" },
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
                "Vincent Maurer",
                {
                    { "Audio Input Left / Mono", "Source audio for granular recording and processing" },
                    { "Audio Input Right", "CV/audio modulation for grain density behavior" },
                    { "Pitch CV", "1V/oct style control for grain or tape playback pitch" },
                    { "Position CV", "Grain playhead offset or tape scrub position modulation" },
                    { "Grain Trigger", "External trigger for grain spawning or tape play/pause gating" },
                    { "Freeze / Reset", "Freeze control in granular mode; playhead reset in tape mode" },
                },
                {
                    { "Stereo Out Left", "Main processed left output" },
                    { "Stereo Out Right", "Main processed right output" },
                    { "Envelope / Playhead Ramp", "Looping ramp CV tied to buffer/playhead progress" },
                    { "Random / Motion CV", "Grain-randomness or movement/speed CV output" },
                    { "Grain / End-of-Cycle Trigger", "Trigger output for grain timing or tape EOC" },
                    { "Freeze / Midpoint Trigger", "High when frozen in granular mode or midpoint trigger in tape mode" },
                },
                {
                    {
                        { "any", "", "", "Position", "Grain read position in the captured buffer/sample" },
                        { "any", "", "", "Envelope Shape", "Grain window shape selection" },
                        { "any", "", "", "Wet/Dry", "Blend between live signal and granular output" },
                        { "any", "", "", "Reverb Mix", "Reverb amount" },
                    },
                    {
                        { "any", "", "", "Density", "Grain generation density" },
                        { "any", "", "", "Jitter / Chords / Reverse Chance", "Behavior control across left/center/right regions" },
                        { "any", "", "", "Feedback / Diffusion", "Feedback in live mode, diffusion in freeze mode" },
                        { "any", "", "", "Room Size", "Reverb size" },
                    },
                    {
                        { "any", "", "", "Size", "Maximum grain size" },
                        { "any", "", "", "Pitch", "Grain pitch control" },
                        { "any", "", "", "Stereo Spread", "Width of stereo grain field" },
                        { "any", "", "", "Damping", "Reverb high-frequency damping" },
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
                    { "Reverb Input Left", "Fixed reverb input; mixed with inverted AudioIn2 to mono reverb feed" },
                    { "Reverb Input Right (Inverted)", "Fixed reverb input contribution, summed into mono reverb input" },
                    { "Configurable CV Input 1", "Assignable modulation source via web editor (default adds to decay)" },
                    { "Configurable CV Input 2", "Assignable modulation source via web editor (default adds to wet/dry)" },
                    { "Configurable Pulse Input 1", "Assignable pulse source via web editor (default freeze gate)" },
                    { "Configurable Pulse Input 2", "Assignable pulse source via web editor (default Turing clock)" },
                },
                {
                    { "Reverb Output Left", "Stereo reverb output (left)" },
                    { "Reverb Output Right", "Stereo reverb output (right)" },
                    { "Configurable CV Output 1", "Assignable utility output (noise, MIDI/CV, Turing, clocked gate, and more)" },
                    { "Configurable CV Output 2", "Assignable utility output (noise, MIDI/CV, Turing, clocked gate, and more)" },
                    { "Configurable Pulse Output 1", "Assignable trigger/gate utility output" },
                    { "Configurable Pulse Output 2", "Assignable trigger/gate utility output" },
                },
                {
                    {
                        { "any", "", "", "Configurable Parameter A", "Source selectable in web editor (default reverb wet/dry)" },
                        { "up", "", "", "Reverb Open Input", "Reverb input passes without noise gate" },
                        { "middle", "", "", "Reverb Input Gated", "Noise gate applied to reverb input" },
                        { "down", "momentary", "", "Reverb Freeze", "Freezes reverb tail while switch is held down" },
                    },
                    {
                        { "any", "", "", "Configurable Parameter B", "Source selectable in web editor (default reverb decay)" },
                    },
                    {
                        { "any", "", "", "Configurable Parameter C", "Source selectable in web editor (default reverb tone)" },
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
                "Chord Organ-ish",
                "Chord Organ-ish - 16 chords, 8 voices, 1V/oct root. Inspired by Music Thing Chord Organ.",
                "jkeyworth",
                {
                    { "VCA CV", "Controls output level from 0 to full scale; defaults to full volume when unpatched" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "Chord Selection CV", "Added to Main knob for voltage-controlled chord selection (negative voltages ignored)" },
                    { "Root Pitch CV", "1V/oct root note input summed with X transpose control" },
                    { "Progression Trigger", "Rising edge advances progression step and retriggers chord pulse" },
                    { "Waveform Trigger", "Rising edge cycles waveform mode" },
                },
                {
                    { "Chord Audio Left", "Mixed chord audio output" },
                    { "Chord Audio Right", "Duplicate mixed chord audio output" },
                    { "Highest Chord Note", "Highest active chord tone as pitch CV" },
                    { "Progression Root CV", "Sequenced root note from selected progression" },
                    { "Chord Change Trigger", "Brief pulse emitted on chord/root retrigger events" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "middle", "", "", "Chord Select", "Selects one of 16 chord types" },
                        { "up", "", "", "Chord Select", "Same as Z middle; glide enabled in this switch position" },
                        { "down", "", "", "Chord Select", "Same as Z middle" },
                        { "up", "toggle", "", "Glide Enable", "Z switch up enables portamento between chord changes" },
                        { "middle", "toggle", "", "Glide Disable", "Z switch middle disables portamento" },
                        { "down", "momentary", "", "Waveform Cycle", "Moving to Z down cycles waveform (sine, triangle, square, saw)" },
                    },
                    {
                        { "middle", "", "", "Root Transpose", "Adds semitone transpose to root pitch" },
                        { "up", "", "", "Root Transpose", "" },
                        { "down", "", "", "Root Transpose", "" },
                    },
                    {
                        { "middle", "", "", "Progression Pattern", "Selects one of 9 root progression patterns" },
                        { "up", "", "", "Progression Pattern", "" },
                        { "down", "", "", "Progression Pattern", "" },
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
                    { "Excitation Input Left", "Mixed with AudioIn2 to excite resonator strings" },
                    { "Excitation Input Right", "Mixed with AudioIn1 to excite resonator strings" },
                    { "Pitch CV", "1V/oct pitch input; X knob becomes fine tune when connected" },
                    { "Damping CV", "Modulates damping/decay with Y knob as base setting" },
                    { "Pluck Trigger", "Injects noise burst to excite all strings" },
                    { "Next Chord Trigger", "Advances to the next chord in progression" },
                },
                {
                    { "Resonator Output Mid", "Wet/dry mixed output using summed string image" },
                    { "Resonator Output Side", "Wet/dry mixed output using alternate stereo string image" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "middle", "", "", "Wet Dry Mix", "Blends direct excitation audio with resonator output" },
                        { "up", "", "", "Wet Dry Mix", "" },
                        { "down", "momentary", "", "Next Chord", "Short press advances to next progression chord" },
                        { "down", "hold", "", "Factory Reset Progression", "Hold about 3 seconds to restore all 18 default chord modes" },
                        { "up", "toggle", "", "Tuning Mode", "Soloes first string for tuning/reference" },
                    },
                    {
                        { "middle", "", "", "Base Pitch", "Fundamental pitch (or fine tune with CV1 connected)" },
                        { "up", "", "", "Base Pitch", "" },
                    },
                    {
                        { "middle", "", "", "Damping", "Higher values increase resonance length" },
                        { "up", "", "", "Damping", "" },
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
                    { "CV/Audio Source", "Main signal to be sampled/quantized (or internal random source when unpatched)" },
                    { "VCA Control", "Optional VCA/modulation control for input scaling" },
                    { "Transpose CV", "Adds up to roughly two octaves of transpose before quantization" },
                    { "Key Select CV", "Selects key index around the circle of fifths" },
                    { "External Clock", "Advances quantizer/sequencer on rising edge when connected" },
                    { "Loop Toggle", "Toggles loop/write behavior" },
                },
                {
                    { "Key Monitor Output", "Main knob-centered monitoring signal" },
                    { "VCA Output", "Scaled input/random signal used as quantizer source" },
                    { "Quantized Note", "Primary quantized pitch output (MIDI note mapped to CV)" },
                    { "Third Harmony", "Harmonic third (major/minor context-sensitive) from quantized note" },
                    { "Internal Clock Pulse", "Gate pulse at internal quarter-note timing" },
                    { "Sequence Pulse", "Pulse stream from probabilistic/looped pulse sequencer" },
                },
                {
                    {
                        { "any", "", "", "Key Center", "Selects key center index when CV In 2 is not connected" },
                    },
                    {
                        { "any", "", "", "Loop Length", "Sets quantizer/sequencer loop length from 1 to 12 steps" },
                        { "down", "hold", "", "Pulse Duration", "Sets pulse output gate length" },
                    },
                    {
                        { "any", "", "", "VCA Amount", "Controls input scaling; can be replaced by Audio In 2 control" },
                        { "down", "hold", "", "Pulse Probability Threshold", "Sets probability threshold for sequence pulse generation" },
                    },
                },
                {
                    "Z",
                    { "Write Mode", "Favors live/write behavior over loop playback" },
                    { "Loop Mode", "Starts in looping playback mode" },
                    { "Tap/Shift Input", "Tap tempo and hold gestures for alternate parameter editing" }
                },
                true
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
                    { "Map Modulation", "Adds to pattern map X, Y, or XY blend (per config). Held off after Z middle↔up flip until X or Y is moved past takeover deadband" },
                    { "Fill Modulation", "Global fill offset in Z middle; all three lane density offsets in Z up (after knob takeover)" },
                    { "External Clock", "When patched, internal clock is bypassed and swing is ignored; follows external pulse edges" },
                    { "Pattern Reset", "Rising edge resets pattern phase to the start of the bar" },
                },
                {
                    { "Audio 1 Output", "Audio output 1" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "Trigger Lane 3", "Digital pulse-style trigger for lane 3" },
                    { "Aux Output", "Configurable accent, clock, or lane 3 mirror output" },
                    { "Trigger Lane 1", "Grids-style trigger output for drum/voice lane 1" },
                    { "Trigger Lane 2", "Grids-style trigger output for drum/voice lane 2" },
                },
                {
                    {
                        { "middle", "", "", "Global Fill Macro", "Scales all lanes together using per-lane scale/offset from config" },
                        { "up", "", "", "Lane 1 Density", "Hit probability for trigger lane 1 (PulseOut1)" },
                        { "middle", "", "", "Chaos", "Randomness amount fed into the pattern engine" },
                        { "down", "momentary", "", "Tap Tempo", "Short press sets internal clock tempo from tap interval" },
                        { "down", "hold", "", "Alt Layer Toggle", "Long press toggles alt layer with knob pickup/catch behavior" },
                    },
                    {
                        { "middle", "", "", "Pattern Map X", "Horizontal position on the Grids pattern map" },
                        { "up", "", "", "Lane 2 Density", "Hit probability for trigger lane 2 (PulseOut2)" },
                        { "middle", "", "", "Internal BPM", "Tempo for the internal clock (bpm10 parameter in config)" },
                    },
                    {
                        { "middle", "", "", "Pattern Map Y", "Vertical position on the Grids pattern map" },
                        { "up", "", "", "Lane 3 Density", "Hit probability for trigger lane 3 (CVOut1). Map X/Y held from last Z middle position until knob takeover" },
                        { "middle", "", "", "Swing", "Shuffle amount from 50% straight to 75% shuffle" },
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
                "BYO Benjolin",
                "Rungler, Chaotic VCO, Noise Source, Turing Machine, Quantizer",
                "Dune Desormeaux",
                {
                    { "External Data Source", "External signal source for shift-register data in middle switch mode" },
                    { "Probability Modulation", "Modulates Turing probability alongside the Main knob" },
                    { "Offset Modulation", "Modulates X/offset path used in rungler processing" },
                    { "VCA Modulation", "Modulates Y/VCA depth for LED/output scaling" },
                    { "Forward Clock", "Rising edge advances the shift register in the forward direction" },
                    { "Reverse Clock", "Rising edge advances the shift register in the reverse direction" },
                },
                {
                    { "Rungler Audio A", "Audio-rate rungler output channel A" },
                    { "Rungler Audio B", "Audio-rate rungler output channel B" },
                    { "Quantized CV A", "Quantized MIDI-note CV derived from channel A rungler state" },
                    { "Quantized CV B", "Quantized MIDI-note CV derived from channel B rungler state" },
                    { "Bit Pulse A", "Pulse output from shift-register bit state (channel A)" },
                    { "Bit Pulse B", "Pulse output from shift-register bit state (channel B)" },
                },
                {
                    {
                        { "middle", "", "", "Turing Probability", "Sets probability/lock behavior for shift-register updates" },
                    },
                    {
                        { "middle", "", "", "Offset", "Sets offset amount (or scale for CV In 1 modulation)" },
                    },
                    {
                        { "middle", "", "", "VCA Depth", "Sets output/LED amplitude scaling (or scale for CV In 2 modulation)" },
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
                    { "Audio Input", "Primary audio input for delay/record/play processing" },
                    { "Main Parameter Modulation", "Modulates Main control behavior in multiple modes" },
                    { "X Modulation", "Modulates X-dependent CV mix and playback position" },
                    { "Y Modulation", "Modulates Y-dependent CV mix and playback position" },
                    { "Clock / Step Trigger", "Advances divider and timing events" },
                    { "Reset Trigger", "Requests playback/reset actions on rising edge" },
                },
                {
                    { "Left Audio Output", "Main processed audio channel" },
                    { "Right Audio Output", "Secondary processed audio channel" },
                    { "CV Mix Output", "Mixed CV/noise output derived from X/Y/CV inputs" },
                    { "Quantized CV Output", "MIDI-note quantized version of CV material" },
                    { "Pulse A", "Timed pulse output tied to clock/divider events" },
                    { "Pulse B", "Secondary timed pulse output tied to divider events" },
                },
                {
                    {
                        { "down", "", "", "Record Mode", "Enter record behavior" },
                        { "middle", "", "", "Play Mode", "Enter loop playback behavior" },
                        { "up", "", "", "Delay Mode", "Enter delay behavior" },
                    },
                    {
                        { "any", "", "", "X Parameter", "Controls internal clock rate and CV/playback mapping" },
                    },
                    {
                        { "any", "", "", "Y Parameter", "Controls divider amount and CV/playback mapping" },
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
                    { "Delay Audio Input", "Enables multitap delay path when connected" },
                    { "Delay Time Modulation", "Modulates delay interpolation length when connected with Audio In 1" },
                    { "Timing Modulation A", "Modulates channel A pulse spacing in combination with knob X" },
                    { "Timing Modulation B", "Modulates channel B pulse spacing in combination with knob Y" },
                    { "Trigger Clock A", "Rising edge triggers/restarts channel A bounce sequence" },
                    { "Trigger Clock B", "Rising edge triggers/restarts channel B bounce sequence" },
                },
                {
                    { "Delay Output L", "Left delay mix output when audio delay mode is active" },
                    { "Delay Output R", "Right delay mix output when audio delay mode is active" },
                    { "Random Pitch CV", "Quantized/random pitch output associated with channel A events" },
                    { "Ramp CV", "Interpolated CV ramp output associated with channel B events" },
                    { "Bounce Pulse A", "Pulse output for channel A bounce events" },
                    { "Bounce Pulse B", "Pulse output for channel B bounce events" },
                },
                {
                    {
                        { "middle", "", "", "Bounce Decay", "Scales spacing-growth/decay for both bounce channels" },
                        { "down", "momentary", "", "Manual Trigger", "Switch edge in down position triggers both channels manually" },
                    },
                    {
                        { "middle", "", "", "Channel A Rate", "Sets channel A trigger rate base (with CV In 1 modulation)" },
                    },
                    {
                        { "middle", "", "", "Channel B Rate", "Sets channel B trigger rate base (with CV In 2 modulation)" },
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
                    { "Granular Input Left", "Recorded into circular buffer when not frozen" },
                    { "Granular Input Right", "Recorded into circular buffer when not frozen" },
                    { "Grain Position CV", "Position/scrub control with X as attenuverter" },
                    { "Grain Speed CV", "Pitch/speed modulation with Main as attenuverter" },
                    { "Grain Trigger", "Rising edge triggers new grain" },
                    { "Grain Gate", "Gates grain triggering when connected" },
                },
                {
                    { "Granular Output Left", "Processed granular audio output" },
                    { "Granular Output Right", "Processed granular audio output" },
                    { "Random CV", "Random value updated when grains are triggered" },
                    { "Buffer Phase CV", "Rising saw mapped to circular buffer write-head position" },
                    { "Grain Completion Trigger", "Triggers when a grain reaches completion threshold" },
                    { "Stochastic Clock", "Probabilistic pulse stream influenced by X and grain size" },
                },
                {
                    {
                        { "middle", "", "", "Grain Playback Speed", "Bidirectional speed from reverse to forward, with center pause detent" },
                        { "up", "", "", "Grain Playback Speed", "Same control while buffer is frozen" },
                        { "down", "", "", "Loop Modulation", "Modulates looping grain playback around captured baseline" },
                    },
                    {
                        { "middle", "", "", "Delay Spread", "Left half selects delay offset, right half increases random spread" },
                        { "up", "", "", "Position or Spread", "" },
                        { "down", "", "", "Glitch Spread", "" },
                    },
                    {
                        { "middle", "", "", "Grain Size", "Sets grain length from very short to long windows" },
                        { "up", "", "", "Grain Size", "" },
                        { "down", "", "", "Loop Grain Size", "" },
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
                    { "X Modulation Input", "Bipolar modulation source for X macro; X knob becomes attenuation amount when patched" },
                    { "Y Modulation Input", "Bipolar modulation source for Y macro; Y knob becomes attenuation amount when patched" },
                    { "Pitch CV", "1V/oct pitch modulation summed with Main knob (clamped to 10 Hz to 10 kHz range)" },
                    { "VCA CV", "Global output level control; 0V mutes and +5V reaches unity gain" },
                    { "Mode Gate", "Overrides panel Z up/middle mode latch; low selects Normal and high selects Alt" },
                    { "Engine Advance Clock", "Rising edges advance to next engine slot" },
                },
                {
                    { "Audio Output 1", "Engine output channel 1; Normal or Alt signal depending on mode" },
                    { "Audio Output 2", "Engine output channel 2; Normal or Alt signal depending on mode" },
                    { "MIDI Pitch CV", "1V/oct CV derived from incoming MIDI note values" },
                    { "MIDI CC74 CV", "Bipolar control output mapped from incoming MIDI CC 74" },
                    { "MIDI Gate", "5V gate high while MIDI note is held" },
                    { "Clock Output", "Clock pulses from internal BPM or external USB MIDI clock/divisions" },
                },
                {
                    {
                        { "middle", "", "", "Main Pitch", "Tunes oscillator pitch over roughly 10 octaves (10 Hz to 10 kHz)" },
                        { "up", "", "", "Main Pitch", "Same pitch control while Alt mode outputs are active" },
                        { "down", "momentary", "", "Engine Slot Advance", "Press-and-release advances to the next engine slot" },
                    },
                    {
                        { "middle", "", "", "Engine Macro X", "Engine-specific parameter 1 in Normal mode, or attenuation for AudioIn1 when patched" },
                        { "up", "", "", "Alt Macro X", "Engine-specific Alt-mode parameter 1" },
                        { "down", "hold", "", "Pulse Out 2 Rate/Division", "While holding Z down, X sets internal clock BPM or MIDI clock division for Pulse Out 2" },
                    },
                    {
                        { "middle", "", "", "Engine Macro Y", "Engine-specific parameter 2 in Normal mode, or attenuation for AudioIn2 when patched" },
                        { "up", "", "", "Alt Macro Y", "Engine-specific Alt-mode parameter 2" },
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
                "Chord Blimey!",
                "Generates CV/Pulse arpeggios",
                "Tom Waters",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "Root Pitch CV", "Adds to root note selection (1V per octave)" },
                    { "Chord Select CV", "Adds to chord selection in approximately 0-1V range" },
                    { "Arpeggio Trigger", "Starts arpeggio playback; patch Pulse Out 2 here to self-loop" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Random Modulation A", "Random 0-1V output updated probabilistically at chord boundaries" },
                    { "Random Modulation B", "Random 0-1V output updated probabilistically at chord boundaries" },
                    { "Arpeggio Note CV", "Current note of the arpeggio (root plus chord degree)" },
                    { "Root CV", "Root note CV held during each chord" },
                    { "Note Trigger", "Pulse for each arpeggiated note" },
                    { "End-of-Cycle Trigger", "Fires at end of arpeggio cycle for chaining or self-looping" },
                },
                {
                    {
                        { "middle", "", "", "Arpeggio Speed", "Controls note length / playback speed" },
                        { "down", "momentary", "", "Fixed Length Step Count", "Short press increments fixed sequence length (1-6, wraps)" },
                        { "down", "hold", "", "Arpeggiator Direction", "Long press cycles direction modes (up/down/up-down/random variants)" },
                    },
                    {
                        { "middle", "", "", "Root Note", "Sets base/root pitch (summed with CV In 1)" },
                    },
                    {
                        { "middle", "", "", "Chord Type", "Selects chord (summed with CV In 2)" },
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
                "13-algorithm noise synth with CV modulation, sample-and-hold, and crusher mode",
                "Eric Gao",
                {
                    { "Main Offset Modulation", "Offsets algorithm selection before wrapping" },
                    { "VCA Control", "Controls output level; normalled to full level when unpatched" },
                    { "X Offset CV", "Offsets X parameter before wrap" },
                    { "Y Offset CV", "Offsets Y parameter before wrap" },
                    { "Sample-and-Hold Trigger", "Rising edge samples current signal for CV/pulse outputs" },
                    { "Crusher Gate", "High gate enables sample-rate/bit reduction" },
                },
                {
                    { "Noise Output A", "Main synthesized noise signal" },
                    { "Noise Output B", "Duplicate synthesized noise signal" },
                    { "Sample-and-Hold CV", "Captured signal at PulseIn1 rising edges" },
                    { "Slewed CV", "Slews between successive sampled CVOut1 values over pulse period" },
                    { "S&H Comparator Gate", "High when sampled CVOut1 value is above zero" },
                    { "Realtime Comparator Gate", "High when current audio sample is above zero" },
                },
                {
                    {
                        { "up", "", "", "Algorithm Select", "" },
                        { "down", "", "", "Randomize Offsets", "Tap randomizes Main/X/Y offsets; long hold resets offsets to zero" },
                    },
                    {
                        { "any", "", "", "Algorithm Parameter X", "Primary per-algorithm shaping parameter" },
                        { "up", "", "", "Parameter X", "" },
                    },
                    {
                        { "any", "", "", "Algorithm Parameter Y", "Secondary per-algorithm shaping parameter" },
                        { "up", "", "", "Parameter Y", "" },
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
                    { "Record CV Input", "Source CV recorded into the loop (internal ramp used when unpatched)" },
                    { "Speed Modulation", "Modulates read-head speed around Main knob center" },
                    { "Loop Time Modulation", "Modulates loop duration with Knob X" },
                    { "Phase Modulation", "Modulates read-head phase offset with Knob Y" },
                    { "Reset Trigger", "Rising edge resets playback heads to record phase" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Read Head 1", "Output of first playback head" },
                    { "Read Head 2", "Output of second playback head" },
                    { "Read Head 3", "Output of third playback head" },
                    { "Read Head 4", "Output of fourth playback head" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "down", "", "", "Reset Heads", "Switch-down edge resets playback heads" },
                        { "up", "", "", "Next Motion Function", "Cycles Ramp, Saw, Triangle, Sin, and Steps read-head motion" },
                    },
                    {
                        { "any", "", "", "Loop Duration", "Sets record-loop length" },
                    },
                    {
                        { "any", "", "", "Phase Offset", "Offsets read heads from record position" },
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
                "ESP",
                "A MS-20-style External Signal Processor that includes a preamp, bandpass filter, envelope follower, gate, and 1v/oct pitch outs.",
                "Ben Regnier",
                {
                    { "Audio Input", "Source signal for all ESP analysis/processing stages" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Post-Gain Monitor", "Preamped signal after soft clipping" },
                    { "Bandpass Monitor", "Band-passed signal used by envelope and pitch detector" },
                    { "Pitch CV (1V/Oct)", "Pitch estimate converted to calibrated 1V/oct-style output # certainty: medium (algorithmic approximation in code)" },
                    { "Envelope CV", "Envelope follower output derived from bandpassed signal" },
                    { "Gate Out", "Schmitt gate derived from envelope thresholding" },
                    { "Trigger Out", "Short trigger on gate rising edge" },
                },
                {
                    {
                        { "up", "", "", "Preamp Gain", "Exponential gain from low boost to high drive" },
                        { "middle", "", "", "Pitch Hold Mode", "Pitch updates only while gate is active; helps stabilize tracking" },
                    },
                    {
                        { "up", "", "", "Bandpass Low Cut", "Lower cutoff index for high-pass stage" },
                    },
                    {
                        { "up", "", "", "Bandpass High Cut", "Upper cutoff index for low-pass stage" },
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
                    { "Modulation Input 1", "Modulates oscillator parameter 1 (Growth/depth-related role depending on active oscillator)" },
                    { "Modulation Input 2", "Modulates oscillator parameter 2 (Y-parameter role depending on active oscillator)" },
                    { "Pitch Modulation", "Added to Main pitch control" },
                    { "CV 2 Input", "CV input 2" },
                    { "Next Bank", "Rising edge advances oscillator bank and resets oscillator index to first entry" },
                    { "Next Oscillator", "Rising edge advances oscillator index within current bank" },
                },
                {
                    { "X Channel Audio", "Horizontal channel for oscilloscope X input" },
                    { "Y Channel Audio", "Vertical channel for oscilloscope Y input" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Switch Advance Trigger", "Short trigger pulse emitted when switch-down gesture changes oscillator" },
                    { "Oscillator Advance Pulse", "Set high when oscillator is advanced in CycleOscillator routine" },
                },
                {
                    {
                        { "middle", "", "", "Pitch", "Exponential pitch control summed with CVIn1" },
                        { "up", "", "", "Pitch", "Exponential pitch control summed with CVIn1" },
                        { "down", "momentary", "", "Next Oscillator", "Switch-down change advances to next oscillator and can roll over into next bank" },
                    },
                    {
                        { "middle", "", "", "Parameter 1 Offset", "Base offset for oscillator modulation parameter 1" },
                        { "up", "", "", "AudioIn1 Attenuation", "Sets attenuation depth for AudioIn1 modulation amount" },
                    },
                    {
                        { "middle", "", "", "Parameter 2 Offset", "Base offset for oscillator modulation parameter 2" },
                        { "up", "", "", "AudioIn2 Attenuation", "Sets attenuation depth for AudioIn2 modulation amount" },
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
                    { "Exciter Audio Replace", "When patched, replaces the pitched component of the LPC exciter path" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "Pitch CV", "Adds pitch modulation, scaled by knob X attenuverter" },
                    { "Speed CV", "Adds modulation to babble speed" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Speech Output", "Main synthesized speech output" },
                    { "LPC Exciter Components", "Pitched and noise components of the LPC exciter" },
                    { "Exciter Amplitude", "Exciter amplitude control signal output" },
                    { "Exciter Pitch", "Exciter pitch control signal output" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "up", "", "", "Pitch", "Sets base speech pitch" },
                        { "middle", "", "", "Off", "Speech generation is muted" },
                        { "down", "", "", "Single Word Trigger", "Triggers a single spoken word" },
                    },
                    {
                        { "up", "", "", "Pitch CV Attenuverter", "Attenuverts CVIn1 pitch modulation amount" },
                    },
                    {
                        { "up", "", "", "Babble Speed", "Sets speed of generated speech" },
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
                    { "Parameter 1 Modulation", "Modulates bytebeat parameter 1" },
                    { "Parameter 2 Modulation", "Modulates bytebeat parameter 2" },
                    { "Formula Select Modulation", "Modulates formula index (built-in) or user slot selection" },
                    { "Sample Rate Modulation", "Modulates effective sample rate/speed" },
                    { "Reset / Trigger", "Rising edge resets bytebeat time counter" },
                    { "Reverse Toggle", "Rising edge toggles reverse playback direction" },
                },
                {
                    { "Bytebeat Output", "Main bytebeat audio output" },
                    { "Next Bytebeat Output", "Audio output of the next formula/slot for parallel patching" },
                    { "Slow Bytebeat CV", "Slower-timebase bytebeat-derived CV" },
                    { "Fast Bytebeat CV", "Faster-timebase bytebeat-derived CV" },
                    { "1-Bit Output", "Bitbeat-style 1-bit pulse stream from waveform LSB" },
                    { "Time Division Clock", "Pulse clock derived from divided bytebeat time variable" },
                },
                {
                    {
                        { "up", "", "", "Sample Rate", "Sets bytebeat sample rate/speed" },
                        { "middle", "", "", "Sample Rate", "" },
                        { "down", "momentary", "", "Reset", "Momentary switch down resets bytebeat time counter" },
                    },
                    {
                        { "up", "", "", "Built-in Formula Select", "Selects formula bank and slot among 36 built-in formulas" },
                        { "middle", "", "", "User Slot Select", "Selects one of 6 stored user formulas" },
                    },
                    {
                        { "up", "", "", "Parameter 1", "Sets primary formula parameter (p1)" },
                        { "middle", "", "", "Parameter 1", "" },
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
                    { "Timbre Modulation", "Added to X control before timbre parameter mapping" },
                    { "Color Modulation", "Added to Y control before color parameter mapping" },
                    { "Pitch CV", "Primary pitch input for the oscillator voice" },
                    { "CV 2 Input", "CV input 2" },
                    { "Trigger", "Trigger input for note/strike events" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Main Audio Output", "Rendered Braids oscillator audio" },
                    { "Audio 2 Output", "Audio output 2" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "down", "", "", "Model Set Toggle", "Switch-down action toggles active six-model set" },
                    },
                    {
                        { "any", "", "", "Timbre", "Braids timbre parameter" },
                    },
                    {
                        { "any", "", "", "Color", "Braids color parameter" },
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
                "DualQuant",
                "Dual quantised granular pitch shifter with calibrated 1V/oct CV outputs",
                "Adrian Vos",
                {
                    { "Audio Input", "Source audio written into the shared granular delay buffer" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "Pitch Mod A", "Additional semitone modulation for output A" },
                    { "Pitch Mod B", "Additional semitone modulation for output B" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Pitch Shift Out A", "Granular pitch-shifted output A" },
                    { "Pitch Shift Out B", "Granular pitch-shifted output B" },
                    { "Pitch CV Out A", "Pitch A converted to millivolt CV output # certainty: medium (not explicitly calibrated in code comments)" },
                    { "Pitch CV Out B", "Pitch B converted to millivolt CV output # certainty: medium (not explicitly calibrated in code comments)" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "up", "", "", "Global Pitch Offset", "Shared coarse pitch shift offset for both outputs" },
                        { "middle", "", "", "Quantize Enable", "Enables chromatic semitone quantization for both outputs" },
                    },
                    {
                        { "up", "", "", "Output A Offset", "Additional pitch offset for output A" },
                    },
                    {
                        { "up", "", "", "Output B Offset", "Additional pitch offset for output B" },
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
                    { "Pitch CV Modulation", "Adds to X knob to control bird voice oscillator pitch" },
                    { "Speed CV Modulation", "Adds to Y knob to control sequence playback timing" },
                    { "External Clock", "Rising edge clock input; when present it overrides internal timing" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Bird One Audio", "Bird one audio voice" },
                    { "Bird Two Audio", "Bird two audio voice" },
                    { "Bird One Pitch Trace", "Held CV trace derived from bird one pitch contour" },
                    { "Bird Two Pitch Trace", "Held CV trace derived from bird two pitch contour" },
                    { "Bird One Onset Pulse", "Triggers on phrase onsets and internal trill/run sub-events for bird one" },
                    { "Bird Two Onset Pulse", "Triggers on phrase onsets and internal trill/run sub-events for bird two" },
                },
                {
                    {
                        { "middle", "", "", "Lock/Change Amount", "Center is unstable; clockwise locks pattern, anticlockwise locks inverted double-pass behavior" },
                        { "up", "", "", "Lock/Change Amount (Wild)", "Same lock/change control while wild mode behavior set is active" },
                        { "down", "momentary", "", "Reseed", "Re-randomizes sequence state" },
                    },
                    {
                        { "middle", "", "", "Bird Pitch", "Controls pitch with CVIn1 summed in" },
                        { "up", "", "", "Bird Pitch (Wild)", "Pitch control in wild mode" },
                    },
                    {
                        { "middle", "", "", "Playback Time", "Left is slower phrase timing, right is faster" },
                        { "up", "", "", "Playback Time (Wild)", "Speed control in wild mode" },
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
                    { "Mono Audio Input", "Primary mono source signal for tape processing" },
                    { "Tape Condition Mod Input", "Audio/CV modulation that morphs between cleaner and more degraded tape states" },
                    { "Tape Depth Mod", "Modulates wow/pitch movement amount; also routed to CV Out 1 attenuator path" },
                    { "Instability Mod", "Modulates flutter/transport agitation; also routed to CV Out 2 attenuator path" },
                    { "Damage Burst Trigger", "Rising edge forces brief heavy tape degradation burst" },
                    { "Crackle Gate", "High gate forces stronger crackle generation" },
                },
                {
                    { "Stereo Out Left", "Processed cassette-style left output" },
                    { "Stereo Out Right", "Processed cassette-style right output" },
                    { "CV1 Attenuated Out", "CV In 1 passthrough scaled by X knob" },
                    { "CV2 Attenuated Out", "CV In 2 passthrough scaled by Y knob" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "any", "", "", "Wet/Dry Mix", "Blend between dry input and processed tape signal" },
                    },
                    {
                        { "any", "", "", "Tape Depth", "Increases wow and pitch instability; also sets CV Out 1 attenuation" },
                    },
                    {
                        { "any", "", "", "Instability", "Increases flutter/transport roughness; also sets CV Out 2 attenuation" },
                    },
                },
                {
                    "Z",
                    { "Clean", "Brighter and more stable tape response" },
                    { "Old", "Darker tone with moderate hiss and instability" },
                    { "Damaged", "Strong hiss, crackle, and unstable behavior" }
                },
                true
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
                    { "Main Audio Input", "Source audio for buffering and processing" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "Freeze CV", "Above approximately 0V stops recording and loops frozen buffer content" },
                    { "Mod CV", "Bipolar modulation added to performance knobs (mode-dependent mapping)" },
                    { "Clock Input", "Rising edge defines beat length (max 2.33 seconds)" },
                    { "External Gate", "Gate control for switch-middle trigger behavior in both modes" },
                },
                {
                    { "Processed Output", "Glitched/stuttered output or pass-through when effect inactive" },
                    { "Dry Output", "Always dry pass-through of Audio In 1" },
                    { "Activity Gate", "High while glitch/shuffle is active, low during pass-through" },
                    { "Descending Ramp", "Falls across each slice and resets at slice boundary" },
                    { "Slice Clock", "Pulse at each ratchet/slice boundary" },
                    { "Clock Mirror", "Direct mirror of Pulse In 1" },
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
                "LoCho Vibes",
                "Stereo chorus and vibrato effect featuring triangle, sine, and slow drift LFO modes, modulation-based delay movement, and tape-style saturation.",
                "Adrian Vos",
                {
                    { "Audio Input Left", "Left input channel; summed with AudioIn2 to mono before stereo modulation processing" },
                    { "Audio Input Right", "Right input channel; summed with AudioIn1 to mono before stereo modulation processing" },
                    { "Depth Modulation", "Bipolar CV modulation for X depth control" },
                    { "Character Modulation", "Bipolar CV modulation for Y Character control" },
                    { "External LFO Clock", "Rising edges sync LFO timing to external pulse source with internal fallback when pulses stop" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Audio Output Left", "Left processed output" },
                    { "Audio Output Right", "Right processed output" },
                    { "LFO CV", "Main internal LFO waveform as CV" },
                    { "Inverted LFO CV", "Inverted version of the internal LFO waveform" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "middle", "", "", "Modulation Rate", "LFO speed for chorus/vibrato movement" },
                        { "up", "", "", "Modulation Rate", "Same rate control, applied in vibrato mode" },
                        { "down", "momentary", "", "LFO Shape Select", "Momentary down action advances waveform triangle -> sine -> random drift" },
                    },
                    {
                        { "middle", "", "", "Modulation Depth", "LFO pitch/modulation depth amount" },
                        { "up", "", "", "Modulation Depth", "Same depth control, applied in vibrato mode" },
                    },
                    {
                        { "middle", "", "", "Character", "Bipolar character shaping from lo-fi degradation to compression/saturation coloration" },
                        { "up", "", "", "Character", "Same character control, applied in vibrato mode" },
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
                    { "Melody Post-Scale Transpose", "Bipolar semitone transpose applied after scale quantization (disabled while scale-select switch down is held)" },
                    { "Internal Tempo CV", "Internal tempo control when external clock is absent" },
                    { "Master Clock", "Rising edge advances both Markov chains; after timeout the module falls back to internal CV-controlled tempo" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Internal Synth Voice A", "Square-wave synth voice tied to melody chain and percussion gating" },
                    { "Internal Output B / Dual Melody Voice B", "Unused in primary mode; outputs second synth voice in dual melody mode" },
                    { "Melody Pitch CV", "Quantized melody pitch output (V/Oct calibrated by firmware mapping)" },
                    { "Percussion Accent CV", "Accent level CV derived from percussion state" },
                    { "Melody Change Gate", "Short pulse when quantized melody pitch changes" },
                    { "Percussion Trigger", "Markov percussion trigger output including ratchets/flams" },
                },
                {
                    {
                        { "middle", "", "", "Loop Lock Length / Mutation", "Center is free run; clockwise locks fixed replay lengths, counter-clockwise locks replay with probabilistic per-step mutation" },
                        { "up", "", "", "Base Transpose", "Pre-scale transpose applied to generated melody" },
                        { "down", "hold", "", "Scale Select", "Selects quantization scale index while switch is held down" },
                    },
                    {
                        { "middle", "", "", "Melody Profile", "Selects one of five melodic transition profiles" },
                        { "up", "", "", "Melody Profile", "Selects one of five melodic transition profiles" },
                        { "down", "hold", "", "Melody Profile", "Profile selection remains available outside the momentary scale-select action" },
                    },
                    {
                        { "middle", "", "", "Percussion Profile", "Selects one of four percussion transition profiles (or voice B profile in dual melody mode)" },
                        { "up", "", "", "Percussion Profile", "Selects one of four percussion transition profiles (or voice B profile in dual melody mode)" },
                        { "down", "hold", "", "Percussion Profile", "Profile selection remains available outside the momentary scale-select action" },
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
                    { "Timestretch Modulation", "Adds bipolar modulation to Y timestretch control when patched" },
                    { "Jump Position CV", "Position source for PulseIn2 jumps" },
                    { "External Clock", "Rising-edge external clock; internal tempo is used when external clock is absent" },
                    { "Jump Trigger", "Rising edge jumps to CVIn2-defined loop position, or loop start when CVIn2 is unpatched" },
                },
                {
                    { "Audio Output Left", "Left audio output from current sample loop playback" },
                    { "Audio Output Right", "Right audio output from current sample loop playback" },
                    { "Random CV 1", "Smooth slow bipolar random modulation output" },
                    { "Random CV 2", "Smooth slow bipolar random modulation output" },
                    { "Jump Gesture Trigger", "Trigger pulse emitted after debounced switch-down jump action" },
                    { "Sample-Select Gesture Trigger", "Trigger pulse emitted after debounced switch-up sample-select action" },
                },
                {
                    {
                        { "middle", "", "", "Position / Sample Target", "Target position for switch-down jump and sample index source for switch-up sample selection" },
                        { "down", "hold", "", "Jump To Main Position", "Debounced hold in down position jumps playback to Main-selected loop position and fires PulseOut1" },
                        { "up", "hold", "", "Select Sample By Main Position", "Debounced hold in up position changes active sample using Main-selected index and fires PulseOut2" },
                    },
                    {
                        { "middle", "", "", "Internal Tempo", "Internal playback tempo control when external clock is not active" },
                    },
                    {
                        { "middle", "", "", "Timestretch Amount", "Timestretch depth with cubic easing; CVIn1 can modulate when patched" },
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
                    { "Density Modulation", "Modulates X density control" },
                    { "Mutation Modulation", "Modulates Y mutation control" },
                    { "External Clock", "External clock input; when active it overrides internal tempo clock" },
                    { "Freeze Gate", "While held high, mutation updates are disabled and structure is preserved" },
                },
                {
                    { "Kick Voice", "Internal kick/percussion synthesis output driven by primary trigger stream" },
                    { "Snare Voice", "Internal snare/percussion synthesis output driven by companion trigger stream" },
                    { "Quantized Melody CV", "Quantized pitch CV from evolving scale-constrained sequence" },
                    { "Energy/Tension CV", "Smoothed evolving modulation output from internal energy and tension state" },
                    { "Primary Trigger Stream", "Main rhythm trigger stream used for melodic progression and kick events" },
                    { "Companion Trigger Stream", "Derived companion trigger stream used for snare/percussion events" },
                },
                {
                    {
                        { "up", "", "", "Internal Tempo", "Internal clock speed when no external clock is present" },
                        { "middle", "", "", "Internal Tempo", "Internal clock speed with moderate swing profile when internally clocked" },
                        { "down", "", "", "Internal Tempo", "Internal clock speed with strongest swing profile when internally clocked" },
                    },
                    {
                        { "up", "", "", "Density", "Trigger probability density control (modulated by CVIn1)" },
                        { "middle", "", "", "Density", "Trigger probability density control (modulated by CVIn1)" },
                        { "down", "", "", "Density", "Trigger probability density control with more active companion-rhythm behavior" },
                    },
                    {
                        { "up", "", "", "Mutation", "Mutation intensity control (modulated by CVIn2)" },
                        { "middle", "", "", "Mutation", "Mutation intensity control (modulated by CVIn2)" },
                        { "down", "", "", "Mutation", "Aggressive mutation behavior and faster harmonic movement" },
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
                "DivCom",
                "Comparator and VC clock divider, inspired by Serge NCOM",
                "divmod",
                {
                    { "Comparator Signal", "Signal tested against scaled/offset reference" },
                    { "Comparator Reference", "Reference signal for comparator thresholding" },
                    { "Divider Amount CV", "Scales divider value selected by Main knob" },
                    { "Count Direction Invert", "Inverts ascending/descending pitch mapping set by switch" },
                    { "Divider Clock", "External clock for divider; breaks comparator normal" },
                    { "Divider Reset", "Resets divider phase/counter on rising edge" },
                },
                {
                    { "Comparator Gate", "High when signal is above threshold" },
                    { "Divider Gate", "Gate high when counter reaches divider value" },
                    { "Counter Pitch CV", "Current divider counter value as whole-tone steps" },
                    { "Divider Value CV", "Current effective divider value as whole-tone steps" },
                    { "Divider Flip-Flop", "Toggles state every divider reset cycle" },
                    { "Comparator XOR Divider", "High when comparator and divider gate states differ" },
                },
                {
                    {
                        { "up", "", "", "Divider Value", "Divider value stays active while switch selects pitch mapping" },
                        { "middle", "", "", "Divider Value", "" },
                    },
                    {
                        { "any", "", "", "Comparator Scale", "Sets reference signal scale factor" },
                        { "up", "", "", "Comparator Scale", "" },
                        { "middle", "", "", "Comparator Scale", "" },
                    },
                    {
                        { "any", "", "", "Comparator Offset", "Sets threshold offset" },
                        { "up", "", "", "Comparator Offset", "" },
                        { "middle", "", "", "Comparator Offset", "" },
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
                    { "Modulator Audio 1", "Summed into AM modulation signal" },
                    { "Modulator Audio 2", "Summed into AM modulation signal" },
                    { "Fine Tune CV", "Modulates carrier frequency with Knob X" },
                    { "CV 2 Input", "CV input 2" },
                    { "RF Gate", "When patched, high gate enables RF output" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "WAV Playback Monitor", "Outputs uploaded WAV playback signal" },
                    { "Modulation Signal Monitor", "Outputs post-level modulation signal used for transmission" },
                    { "CV 1 Output", "CV output 1" },
                    { "CV 2 Output", "CV output 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "middle", "", "", "RF Off", "Carrier disabled" },
                        { "up", "", "", "RF On", "Carrier enabled (unless gated low by PulseIn1)" },
                        { "down", "", "", "RF On", "Carrier enabled (unless gated low by PulseIn1)" },
                    },
                    {
                        { "any", "", "", "Carrier Frequency (Fine)", "Fine tune amount summed with CVIn1" },
                    },
                    {
                        { "any", "", "", "Modulation Level", "Sets modulation depth / broadcast volume" },
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
                "SlowMod",
                "Chaotic quad-LFO with VCAs",
                "divmod",
                {
                    { "VCA Control A", "Controls amplitude of AudioOut1 modulation signal" },
                    { "VCA Control B", "Controls amplitude of AudioOut2 modulation signal" },
                    { "VCA Control C", "Controls amplitude of CVOut1 modulation signal" },
                    { "VCA Control D", "Controls amplitude of CVOut2 modulation signal" },
                    { "Pause Trigger", "Pauses all LFO channels" },
                    { "Randomize Trigger", "Randomizes phase of all LFO channels" },
                },
                {
                    { "Fast LFO", "Fastest modulation output channel" },
                    { "Mid Fast LFO", "Second modulation output channel" },
                    { "Mid Slow LFO", "Third modulation output channel" },
                    { "Slow LFO", "Slowest modulation output channel" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Pulse 2 Output", "Pulse output 2" },
                },
                {
                    {
                        { "middle", "", "", "Global LFO Rate", "Sets all channel rates from minutes-long cycles to low audio range" },
                        { "up", "", "", "Pause", "Pauses LFO phase movement" },
                        { "down", "", "", "Phase Randomize", "Randomizes phase of all LFOs" },
                    },
                    {
                        { "middle", "", "", "Cross Mod Amount", "Intensity of cross-modulation between LFO channels" },
                    },
                    {
                        { "middle", "", "", "Neighbor Invert Crossfade", "Crossfades each output toward an inverted neighboring output" },
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
                "NZT",
                "Grain Noise and Noise Tools",
                "@kjnilsson",
                {
                    { "Ring Mod Input", "Ring-modulates internal noise source" },
                    { "External Noise Source", "Replaces internal noise source when connected" },
                    { "Density Modulation CV", "Modulates noise density; scaled by Y knob" },
                    { "Seed Modulation CV", "Modulates noise seed and overrides X knob control when connected" },
                    { "Seed Reset Trigger", "Resets noise generator seed on triggers; can be used for pitched/noise oscillator behavior" },
                    { "Sample-and-Hold Clock", "Updates CV Out 2 sample-and-hold output on trigger" },
                },
                {
                    { "Grain Noise Output A", "Primary grain-noise output" },
                    { "Grain Noise Output B", "Complementary inverse-density grain-noise output" },
                    { "Static Offset CV", "Static approximately -6V output for biasing/offset patches" },
                    { "Sample-and-Hold CV", "Sample-and-hold output updated by Pulse In 2" },
                    { "Pulse 1 Output", "Pulse output 1" },
                    { "Periodic Pulse", "Short trigger pulse emitted approximately every 1.366 seconds" },
                },
                {
                    {
                        { "any", "", "", "Noise Density", "Primary grain density amount" },
                    },
                    {
                        { "any", "", "", "Seed Control", "Controls noise seed when Pulse In 1 is used and CV In 2 is not connected" },
                    },
                    {
                        { "any", "", "", "CV In 1 Gain", "Gain for CV In 1 contribution added to Main density value" },
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
                "MLRws",
                "A remix of monome's classic MLR sample cutting platform (grid controller encouraged but optional)",
                "Dune Desormeaux",
                {
                    { "Audio Input 1", "Recording source / modulation source depending on mode" },
                    { "Audio Input 2", "Secondary recording source / speed modulation depending on mode" },
                    { "X Modulation CV", "Modulates X-layer behavior in gridless control paths" },
                    { "Y Modulation CV", "Modulates Y-layer behavior in gridless control paths" },
                    { "Reset / Cut Trigger", "Rising edge triggers reset/cut actions in gridless mode" },
                    { "Clock / Advance Trigger", "Rising edge advances track/turing clock actions in gridless mode" },
                },
                {
                    { "Stereo Left Mix", "Main mixed output bus L" },
                    { "Stereo Right Mix", "Main mixed output bus R" },
                    { "Cut/Turing Pitch CV", "Emits quantized note CV tied to cut or turing events" },
                    { "Trigger Envelope CV", "Attack/decay CV envelope output" },
                    { "Cut/Wrap Trigger", "Trigger pulse on cut/wrap events" },
                    { "Envelope-End Trigger", "Trigger pulse when CV envelope reaches end" },
                },
                {
                    {
                        { "middle", "", "", "Track Select / Turing Probability", "Selects active track in gridless mode and drives turing probability state" },
                        { "up", "", "", "Selected Track Speed / Direction", "Sets selected track speed and direction around center" },
                        { "down", "", "", "Record-Hold / Reset Gesture", "Used for record/hold/reset gestures in gridless mode" },
                    },
                    {
                        { "middle", "", "", "Global Playback Gain", "Controls global playback gain layer" },
                        { "up", "", "", "Selected Track Level Slot", "Sets per-track volume slot" },
                        { "down", "", "", "Input Gain", "Sets recording input gain for active source" },
                    },
                    {
                        { "middle", "", "", "Radiate Amount", "Controls global radiate amount layer" },
                        { "up", "", "", "Selected Track Start Position", "Sets per-track cut start column" },
                        { "down", "", "", "CV2 Envelope Attack", "Sets CV2 envelope attack while in down state" },
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
                "drumdrum",
                "DFAM-style 8-step sequencer",
                "Moses Hoyt",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "Velocity Mod", "Summed into velocity CV output (decay modulation use-case)" },
                    { "Global Transpose", "Global pitch transpose applied to both pitch outputs" },
                    { "External Clock", "Rising edge advances one sequencer step, overriding internal clock" },
                    { "Reset", "Rising edge resets playback step to step 1" },
                },
                {
                    { "White Noise", "Continuous white noise output" },
                    { "VCO 2 Pitch CV", "Pitch CV mirror with Y offset (via audio DAC path, uncalibrated)" },
                    { "VCO 1 Pitch CV", "Calibrated 1V/oct pitch from current step" },
                    { "Velocity CV", "Step velocity-derived CV with CV In 1 modulation" },
                    { "Step Trigger", "Trigger pulse every step (also preview trigger while paused in edit mode)" },
                    { "End-of-Cycle Trigger", "Trigger pulse when sequence wraps from last step to first" },
                },
                {
                    {
                        { "up", "", "", "Tempo", "Internal tempo (ignored when external clock is patched)" },
                        { "middle", "", "", "Tempo", "Internal tempo while editing" },
                        { "down", "short-press", "", "Advance Edit Cursor", "Moves edit cursor to next step" },
                        { "down", "long-press", "", "Play/Pause Toggle", "Toggles sequencer running state" },
                    },
                    {
                        { "up", "", "", "Sequence Length", "Active steps (1-8) with pickup behavior" },
                        { "middle", "", "", "Step Pitch", "Pitch for selected edit step (with pickup)" },
                    },
                    {
                        { "up", "", "", "VCO 2 Offset", "VCO 2 pitch offset relative to VCO 1 (semitones)" },
                        { "middle", "", "", "Step Velocity", "Velocity for selected edit step (with pickup)" },
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
                    { "bb.audioin[1]", "Audio input query source for Lua scripts" },
                    { "bb.audioin[2]", "Audio input query source for Lua scripts" },
                    { "input[1] / bb.connected.cv1", "Script-readable CV input mapped to crow input[1]" },
                    { "input[2] / bb.connected.cv2", "Script-readable CV input mapped to crow input[2]" },
                    { "bb.pulsein[1]", "Digital input with change/clock detection for Lua callbacks" },
                    { "bb.pulsein[2]", "Digital input with change/clock detection for Lua callbacks" },
                },
                {
                    { "output[3]", "Uncalibrated CV/audio output under script control" },
                    { "output[4]", "Uncalibrated CV/audio output under script control" },
                    { "output[1]", "Calibrated CV output under script control" },
                    { "output[2]", "Calibrated CV output under script control" },
                    { "bb.pulseout[1]", "Digital pulse output for Lua-triggered actions" },
                    { "bb.pulseout[2]", "Digital pulse output for Lua-triggered actions" },
                },
                {
                    {
                        { "middle", "", "", "bb.knob.main", "Normalized analog knob value exposed to Lua; behavior depends on loaded script" },
                        { "any", "", "", "bb.switch.position", "Three-position switch state exposed to Lua; script decides response" },
                    },
                    {
                        { "middle", "", "", "bb.knob.x", "Normalized analog knob value exposed to Lua; behavior depends on loaded script" },
                    },
                    {
                        { "middle", "", "", "bb.knob.y", "Normalized analog knob value exposed to Lua; behavior depends on loaded script" },
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
                    { "Left Pitch Sample Input", "When connected, sampled-and-held then quantized onto CV Out 1 instead of internal random pitch" },
                    { "Right Pitch Sample Input", "When connected, sampled-and-held then quantized onto CV Out 2 instead of internal random pitch" },
                    { "Pulse 1 Input", "Pulse input 1" },
                    { "Pulse 2 Input", "Pulse input 2" },
                },
                {
                    { "Left AD Envelope", "Looping attack-decay envelope output for left channel" },
                    { "Right AD Envelope", "Looping attack-decay envelope output for right channel" },
                    { "Left Pitch CV", "Random or sampled-and-held quantized pitch output" },
                    { "Right Pitch CV", "Random or sampled-and-held quantized pitch output" },
                    { "Left End-of-Cycle Pulse", "Pulse at end of each left envelope cycle" },
                    { "Right End-of-Cycle Pulse", "Pulse at end of each right envelope cycle" },
                },
                {
                    {
                        { "any", "", "", "Global Time/Mood", "Global multiplier for envelope lengths and activity feel" },
                    },
                    {
                        { "any", "", "", "Left Envelope Length", "Controls left channel envelope timing range" },
                    },
                    {
                        { "any", "", "", "Right Envelope Length", "Controls right channel envelope timing range" },
                    },
                },
                {
                    "Z",
                    { "Octave Quantize", "Quantizes pitch outputs to octave-only steps" },
                    { "Chromatic Quantize", "Quantizes pitch outputs to chromatic notes" },
                    { "Range Cycle", "Cycles random note range between approximately one to three octaves" }
                },
                true
            }
        },
        {
            "duo_midi",
            {
                "duo_midi",
                "Duo MIDI",
                "A duophonic midi device/host interface",
                "Dune Desormeaux",
                {
                    { "Audio 1 Input", "Audio input 1" },
                    { "Audio 2 Input", "Audio input 2" },
                    { "CV 1 Input", "CV input 1" },
                    { "CV 2 Input", "CV input 2" },
                    { "Voice 1 Envelope Gate Input", "Toggles AudioOut1 envelope state and refreshes envelope parameters" },
                    { "Voice 2 Envelope Gate Input", "Toggles AudioOut2 envelope state and refreshes envelope parameters" },
                },
                {
                    { "Voice 1 ASR Envelope", "ASR envelope for voice 1" },
                    { "Voice 2 ASR Envelope", "ASR envelope for voice 2" },
                    { "Voice 1 Pitch", "1V/oct pitch for assigned voice 1" },
                    { "Voice 2 Pitch", "1V/oct pitch for assigned voice 2 (or mirrored in mono mode)" },
                    { "Voice 1 Trigger/Gate", "Trigger on assignment or gate while voice 1 is active" },
                    { "Voice 2 Trigger/Gate", "Trigger on assignment or gate while voice 2 is active" },
                },
                {
                    {
                        { "up", "", "", "Velocity Sensitivity", "Scales envelope sustain by MIDI velocity" },
                        { "middle", "", "", "Velocity Sensitivity", "" },
                        { "down", "", "", "Trigger/Gate Mode Toggle", "Toggles pulse outputs between 10ms trigger and sustained gate behavior" },
                    },
                    {
                        { "up", "", "", "Envelope Attack", "Sets ASR attack time" },
                        { "middle", "", "", "Envelope Attack", "" },
                    },
                    {
                        { "up", "", "", "Envelope Release", "Sets ASR release time" },
                        { "middle", "", "", "Envelope Release", "" },
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
