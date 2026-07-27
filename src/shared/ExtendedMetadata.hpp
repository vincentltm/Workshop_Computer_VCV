// ExtendedMetadata.hpp
// This file is dynamically generated. Do not edit manually.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace ExtendedMetadata {

struct PortContext {
    std::string z;        // "up", "middle", "down", "any"
    std::string gesture;  // e.g. "hold", "double-hold", or ""
    std::string mode;     // e.g. "bank-indicator" or ""
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
    std::string editor;
    std::string manual;
    std::string license;
    std::string repository;

    std::vector<PortContext> inputs[6];
    std::vector<PortContext> outputs[6];

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
                "Workshop Computer Card",
                "Tom Whitwell",
                "",
                "USB MIDI utility firmware for routing incoming MIDI notes to CV/Gate outputs.\nKnob and CV positions are transmitted back to the host as MIDI CC values.\nHold the switch during boot to enter calibration mode for the two pitch/CV channels.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/00_Simple_MIDI",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV CC Source 1", "CV input sampled and sent to host as MIDI CC data" },
                    },
                    {
                        { "any", "", "", "CV CC Source 2", "CV input sampled and sent to host as MIDI CC data" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "MIDI Pitch CV 1", "Channel 1 MIDI notes are converted to calibrated pitch CV" },
                    },
                    {
                        { "any", "", "", "MIDI Pitch CV 2", "Channel 2 MIDI notes are converted to calibrated pitch CV" },
                    },
                    {
                        { "any", "", "", "Gate 1", "Gate output for MIDI note events on channel 1" },
                    },
                    {
                        { "any", "", "", "Gate 2", "Gate output for MIDI note events on channel 2" },
                    },
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
            "turing_machine",
            {
                "turing_machine",
                "Turing Machine",
                "Workshop Computer Card",
                "Tom Whitwell",
                "https://www.musicthing.co.uk/web_config/turing.html",
                "Dual Turing-style random sequencer with tap tempo or external clocking.\nPulse In 1 overrides tap tempo as the main clock; Pulse In 2 can clock channel 2 independently.\nCV In 1 controls divide/multiply on channel 2, CV In 2 applies quantized pitch offset.\nAudio/CV In 1 can reset sequences; Audio/CV In 2 can CV-switch between the two presets.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/Rev_1_5_Code",
                {
                    {
                        { "any", "", "", "Reset", "Rising edge resets all sequence states to step 1" },
                    },
                    {
                        { "any", "", "", "Preset Select CV", "Experimental CV control of the two-preset mode switch" },
                    },
                    {
                        { "any", "", "", "Diviply CV", "Positive/negative control for divide-multiply rate on channel 2" },
                    },
                    {
                        { "any", "", "", "Pitch Offset CV", "Experimental quantized offset applied to both channels" },
                    },
                    {
                        { "any", "", "", "External Clock 1", "Replaces tap tempo and drives the main clock" },
                    },
                    {
                        { "any", "", "", "External Clock 2", "Overrides channel 2 diviply clock for independently clocked channel 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Channel 1 DAC CV", "Scaled DAC signal for channel 1; output range configurable in editor" },
                    },
                    {
                        { "any", "", "", "Channel 2 DAC CV", "Scaled DAC signal for channel 2; output range configurable in editor" },
                    },
                    {
                        { "any", "", "", "Channel 1 Quantized CV", "Quantized pitch CV output for channel 1" },
                    },
                    {
                        { "any", "", "", "Channel 2 Quantized CV", "Quantized pitch CV output for channel 2" },
                    },
                    {
                        { "any", "", "", "Channel 1 Pulse", "Clock or Turing-bit pulse behavior depending on pulse mode configuration" },
                    },
                    {
                        { "any", "", "", "Channel 2 Pulse", "Clock or Turing-bit pulse behavior depending on pulse mode configuration" },
                    },
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
            "byo_benjolin",
            {
                "byo_benjolin",
                "BYO Benjolin",
                "Workshop Computer Card",
                "Dune Desormeaux",
                "",
                "Chaotic Benjolin-inspired program combining rungler-style modulation, quantized CV, and pulse generation.\nForward/back clocks can be driven from Pulse In 1 and Pulse In 2.\nThe card blends internal randomness with external modulation for probability, offset, and amplitude.\n\nThe Z switch sets how shift-register cells are written on each clock:\n  - Up (Double Length): toggles the write cell each clock, producing a stable loop of double length.\n  - Middle (Unlock): writes from the data input (or internal noise) when data exceeds the Chaos threshold.\n  - Down (Write): forces the write cell to a fixed value.\nThe Chaos, Offset, and Chaos VCA knobs behave the same in all switch positions.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/04_BYO_Benjolin",
                {
                    {
                        { "any", "", "", "Data Input", "External data source for shift-register writes in Unlock (middle) switch mode; normalled to internal noise" },
                    },
                    {
                        { "any", "", "", "Lock CV", "Modulates the Chaos/Lock probability alongside the Chaos knob" },
                    },
                    {
                        { "any", "", "", "Offset CV", "Modulates the Offset amount used in rungler processing" },
                    },
                    {
                        { "any", "", "", "VCA CV", "Modulates the Chaos VCA depth for output scaling" },
                    },
                    {
                        { "any", "", "", "FWD Clk In", "Rising edge advances the shift register in the forward direction" },
                    },
                    {
                        { "any", "", "", "Back Clk In", "Rising edge advances the shift register in the reverse direction" },
                    },
                },
                {
                    {
                        { "any", "", "", "Raw Out 1", "Audio-rate rungler output channel 1" },
                    },
                    {
                        { "any", "", "", "Raw Out 2", "Audio-rate rungler output channel 2" },
                    },
                    {
                        { "any", "", "", "Quant Out 1", "Quantized MIDI-note CV derived from channel 1 rungler state" },
                    },
                    {
                        { "any", "", "", "Quant Out 2", "Quantized MIDI-note CV derived from channel 2 rungler state" },
                    },
                    {
                        { "any", "", "", "1-Bit Out 1", "Single-bit pulse output from the shift-register state (channel 1)" },
                    },
                    {
                        { "any", "", "", "1-Bit Out 2", "Single-bit pulse output from the shift-register state (channel 2)" },
                    },
                },
                {
                    {
                        { "any", "", "", "Chaos", "Lock (stable loop) through Chaos (random) probability that shift-register cells change" },
                    },
                    {
                        { "any", "", "", "Offset", "Sets output offset amount (or depth for Offset CV / CV In 1 modulation)" },
                    },
                    {
                        { "any", "", "", "Chaos VCA", "Scales rungler output amplitude (or depth for VCA CV / CV In 2 modulation)" },
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
                "Workshop Computer Card",
                "Vincent Maurer (vincentmaurer.de)",
                "https://vincentmaurer.de/usb-audio/midi_config.html",
                "Class-compliant USB composite firmware providing multichannel USB audio plus USB MIDI.\nMiddle switch mode runs standard audio interface behavior; up switch mode enables configurable MIDI/CV/Gate mapping;\ndown switch mode disables MIDI for audio-only operation.\nRouting, channel count, sample rate, and CV/Pulse behaviors are configurable via the web interface and can be saved to flash.",
                "GPL-3.0",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/06_usb_audio",
                {
                    {
                        { "any", "", "", "Audio Input 1", "Physical input channel routable to USB audio and/or MIDI processing" },
                    },
                    {
                        { "any", "", "", "Audio Input 2", "Physical input channel routable to USB audio and/or MIDI processing" },
                    },
                    {
                        { "any", "", "", "CV Input 1", "Configurable as pitch or CC source in alt mode" },
                    },
                    {
                        { "any", "", "", "CV Input 2", "Configurable as pitch or CC source in alt mode" },
                    },
                    {
                        { "any", "", "", "Pulse Input 1", "Configurable gate input (default associated with CV 1)" },
                    },
                    {
                        { "any", "", "", "Pulse Input 2", "Configurable gate/clock input (default clock/run role)" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio Output 1", "Physical output channel routable from USB stream or mode logic" },
                    },
                    {
                        { "any", "", "", "Audio Output 2", "Physical output channel routable from USB stream or mode logic" },
                    },
                    {
                        { "any", "", "", "CV Output 1", "Configurable pitch/CV destination in alt mode" },
                    },
                    {
                        { "any", "", "", "CV Output 2", "Configurable CC/CV destination in alt mode" },
                    },
                    {
                        { "any", "", "", "Pulse Output 1", "Configurable gate/trigger/clock output" },
                    },
                    {
                        { "any", "", "", "Pulse Output 2", "Configurable gate/trigger/clock output" },
                    },
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
            "bumpers",
            {
                "bumpers",
                "Bumpers",
                "Workshop Computer Card",
                "Chris Johnson",
                "",
                "Bouncing-ball trigger generator and multitap delay card.\nTrigger streams can be internally generated or externally clocked, with pulse outputs and related CV/audio outputs.\nPatching audio input enables the delay processor; otherwise outputs act as trigger/CV generators.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/07_bumpers",
                {
                    {
                        { "any", "", "", "Delay Audio Input", "Enables multitap delay path when connected" },
                    },
                    {
                        { "any", "", "", "Delay Time Modulation", "Modulates delay interpolation length when connected with Audio In 1" },
                    },
                    {
                        { "any", "", "", "Timing Modulation A", "Modulates channel A pulse spacing in combination with knob X" },
                    },
                    {
                        { "any", "", "", "Timing Modulation B", "Modulates channel B pulse spacing in combination with knob Y" },
                    },
                    {
                        { "any", "", "", "Trigger Clock A", "Rising edge triggers/restarts channel A bounce sequence" },
                    },
                    {
                        { "any", "", "", "Trigger Clock B", "Rising edge triggers/restarts channel B bounce sequence" },
                    },
                },
                {
                    {
                        { "any", "", "", "Delay Output L", "Left delay mix output when audio delay mode is active" },
                    },
                    {
                        { "any", "", "", "Delay Output R", "Right delay mix output when audio delay mode is active" },
                    },
                    {
                        { "any", "", "", "Random Pitch CV", "Quantized/random pitch output associated with channel A events" },
                    },
                    {
                        { "any", "", "", "Ramp CV", "Interpolated CV ramp output associated with channel B events" },
                    },
                    {
                        { "any", "", "", "Bounce Pulse A", "Pulse output for channel A bounce events" },
                    },
                    {
                        { "any", "", "", "Bounce Pulse B", "Pulse output for channel B bounce events" },
                    },
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
            "goldfish",
            {
                "goldfish",
                "Goldfish",
                "Workshop Computer Card",
                "Dune Desormeaux",
                "",
                "Goldfish is a multi-mode stereo delay/looper with synchronized pulse/CV outputs. The switch selects\nrecord, delay, and play behaviors. Audio is streamed to flash (IMA-ADPCM), so loops/delays run from\n~55s on a 2MB card up to ~9min on 16MB. Pulse inputs provide clock/reset (and gated recording); the\nCV/audio inputs modulate delay time, playback position and speed, and the generated CV/pitch material.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/11_goldfish",
                {
                    {
                        { "any", "", "", "Left Audio Input", "Left channel audio input for record/delay/play (true stereo)" },
                    },
                    {
                        { "any", "", "", "Right Audio Input", "Right channel audio input for record/delay/play (true stereo)" },
                    },
                    {
                        { "any", "", "", "X Mod", "Modulates X-dependent CV mix and playback position" },
                    },
                    {
                        { "any", "", "", "Y Mod", "Modulates Y-dependent CV mix and playback position" },
                    },
                    {
                        { "any", "", "", "Sample Trig / clock", "Advances divider, triggers S&H" },
                    },
                    {
                        { "any", "", "", "Reset / Rec Gate", "In play, a rising edge resets/cuts to the start position; when patched in delay, gates recording (punch in on the rising edge, hand off to play on the falling edge)" },
                    },
                },
                {
                    {
                        { "any", "", "", "Left Audio Output", "Left channel processed audio output" },
                    },
                    {
                        { "any", "", "", "Right Audio Output", "Right channel processed audio output (independent stereo channel)" },
                    },
                    {
                        { "any", "", "", "CV Mix Output", "In delay/record, the live CV mix (from X/Y and the CV inputs); in play, the recorded CV track" },
                    },
                    {
                        { "any", "", "", "Quantized CV Output", "The CV output quantized to a major scale, sampled/held and latched on each clock pulse" },
                    },
                    {
                        { "any", "", "", "Internal Clock Output", "Timed pulse output tied to clock/divider events" },
                    },
                    {
                        { "any", "", "", "Clock Divider Output", "Secondary timed pulse output tied to divider events" },
                    },
                },
                {
                    {
                        { "any", "", "", "Time / Speed", "Delay time in delay; playback speed and direction (full reverse - stop at noon - full forward) in play; also crossfades the CV mix" },
                    },
                    {
                        { "any", "", "", "Int Clock Rate", "Sets the internal clock rate; also attenuverts CV mix input 1 and maps playback position" },
                    },
                    {
                        { "any", "", "", "Clock Div", "Sets the clock divider amount; also attenuverts CV mix input 2 and maps playback position" },
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
                "Eric Gao",
                "",
                "Noisebox selects among 13 noise algorithms with Main, with X/Y shaping the active algorithm.\nMain can be offset by `AudioIn1`, output level is VCA-controlled by `AudioIn2`, and switch down\nrandomizes control offsets (hold to reset). `PulseIn1` clocks sample-and-hold behavior; switch up\nor `PulseIn2` enables sample-rate/bit reduction.",
                "CC BY-NC-SA 3.0",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/13_noisebox",
                {
                    {
                        { "any", "", "", "Main Offset Modulation", "Offsets algorithm selection before wrapping" },
                    },
                    {
                        { "any", "", "", "VCA Control", "Controls output level; normalled to full level when unpatched" },
                    },
                    {
                        { "any", "", "", "X Offset CV", "Offsets X parameter before wrap" },
                    },
                    {
                        { "any", "", "", "Y Offset CV", "Offsets Y parameter before wrap" },
                    },
                    {
                        { "any", "", "", "Sample-and-Hold Trigger", "Rising edge samples current signal for CV/pulse outputs" },
                    },
                    {
                        { "any", "", "", "Crusher Gate", "High gate enables sample-rate/bit reduction" },
                    },
                },
                {
                    {
                        { "any", "", "", "Noise Output A", "Main synthesized noise signal" },
                    },
                    {
                        { "any", "", "", "Noise Output B", "Duplicate synthesized noise signal" },
                    },
                    {
                        { "any", "", "", "Sample-and-Hold CV", "Captured signal at PulseIn1 rising edges" },
                    },
                    {
                        { "any", "", "", "Slewed CV", "Slews between successive sampled CVOut1 values over pulse period" },
                    },
                    {
                        { "any", "", "", "S&H Comparator Gate", "High when sampled CVOut1 value is above zero" },
                    },
                    {
                        { "any", "", "", "Realtime Comparator Gate", "High when current audio sample is above zero" },
                    },
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
                "Workshop Computer Card",
                "Chris Johnson",
                "",
                "CVMod records one CV stream into a loop and reads it out through four moving heads. X + `CVIn1`\nsets loop duration, Main + `AudioIn2` sets head speed, and Y + `CVIn2` sets head phase offset.\nSwitch down (or `PulseIn1`) resets read-head position; switch up cycles motion function.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/14_cvmod",
                {
                    {
                        { "any", "", "", "Record CV Input", "Source CV recorded into the loop (internal ramp used when unpatched)" },
                    },
                    {
                        { "any", "", "", "Speed Modulation", "Modulates read-head speed around Main knob center" },
                    },
                    {
                        { "any", "", "", "Loop Time Modulation", "Modulates loop duration with Knob X" },
                    },
                    {
                        { "any", "", "", "Phase Modulation", "Modulates read-head phase offset with Knob Y" },
                    },
                    {
                        { "any", "", "", "Reset Trigger", "Rising edge resets playback heads to record phase" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Read Head 1", "Output of first playback head" },
                    },
                    {
                        { "any", "", "", "Read Head 2", "Output of second playback head" },
                    },
                    {
                        { "any", "", "", "Read Head 3", "Output of third playback head" },
                    },
                    {
                        { "any", "", "", "Read Head 4", "Output of fourth playback head" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
            "mlrws",
            {
                "mlrws",
                "MLRws",
                "Workshop Computer Card",
                "Dune Desormeaux",
                "https://dessertplanet.github.io/MLRws-web/",
                "MLRws is a six-track sample-cutting instrument with grid and gridless workflows. It supports on-card\nsample storage, recording/playback, and USB grid interaction. In gridless mode, Main/X/Y plus switch\nmanage track selection, level/speed/start-position/radiate behavior, while pulse/CV I/O provides\nreset/clocking and CV envelope/pitch triggers.",
                "GPL-3.0",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/15_MLRws",
                {
                    {
                        { "any", "", "", "Audio Input 1", "Recording source / modulation source depending on mode" },
                    },
                    {
                        { "any", "", "", "Audio Input 2", "Secondary recording source / speed modulation depending on mode" },
                    },
                    {
                        { "any", "", "", "X Modulation CV", "Modulates X-layer behavior in gridless control paths" },
                    },
                    {
                        { "any", "", "", "Y Modulation CV", "Modulates Y-layer behavior in gridless control paths" },
                    },
                    {
                        { "any", "", "", "Reset / Cut Trigger", "Rising edge triggers reset/cut actions in gridless mode" },
                    },
                    {
                        { "any", "", "", "Clock / Advance Trigger", "Rising edge advances track/turing clock actions in gridless mode" },
                    },
                },
                {
                    {
                        { "any", "", "", "Stereo Left Mix", "Main mixed output bus L" },
                    },
                    {
                        { "any", "", "", "Stereo Right Mix", "Main mixed output bus R" },
                    },
                    {
                        { "any", "", "", "Cut/Turing Pitch CV", "Emits quantized note CV tied to cut or turing events" },
                    },
                    {
                        { "any", "", "", "ASR Envelope CV", "Attack/decay CV envelope output" },
                    },
                    {
                        { "any", "", "", "Cut/Wrap Trigger", "Trigger pulse on cut/wrap events" },
                    },
                    {
                        { "any", "", "", "Envelope-End Trigger", "Trigger pulse when CV envelope reaches end" },
                    },
                },
                {
                    {
                        { "middle", "", "", "Track Select / Turing Probability", "Selects active track in gridless mode and drives turing probability state" },
                        { "up", "", "", "Selected Track Speed / Direction", "Sets selected track speed and direction around center" },
                        { "down", "", "", "Record-Hold / Reset Gesture", "Used for record/hold/reset gestures in gridless mode" },
                    },
                    {
                        { "middle", "", "", "Mix Gain", "Controls global playback gain layer" },
                        { "up", "", "", "Selected Track Level Slot", "Sets per-track volume slot" },
                        { "down", "", "", "Input Gain", "Sets recording input gain for active source" },
                    },
                    {
                        { "middle", "", "", "Radiate", "Controls global radiate amount layer" },
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
            "ca_sequencer",
            {
                "ca_sequencer",
                "Cellular Automata Sequencer",
                "Workshop Computer Card",
                "Ainews",
                "",
                "16-cell gate and quantized CV melody generator inspired by NLC Cellular Automata, using CA rules 90 & 150 on a 4x4 grid",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/19_CA_Sequencer",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
                "Workshop Computer Card",
                "Chris Johnson",
                "https://www.musicthing.co.uk/web_config/reverb.html",
                "Stereo reverb card with configurable utility routing for CV, pulse, clocks, Turing Machine, and Bernoulli gate.\nTop audio jacks are fixed to reverb I/O; most other controls and jacks are assignable in the web editor.\nZ up passes input normally, Z middle enables input noise gate, Z down freezes the reverb.\nHold Z down while loading to restore default configuration.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/20_reverb",
                {
                    {
                        { "any", "", "", "Reverb Input Left", "Fixed reverb input; mixed with inverted AudioIn2 to mono reverb feed" },
                    },
                    {
                        { "any", "", "", "Reverb Input Right (Inverted)", "Fixed reverb input contribution, summed into mono reverb input" },
                    },
                    {
                        { "any", "", "", "Configurable CV Input 1", "Assignable modulation source via web editor (default adds to decay)" },
                    },
                    {
                        { "any", "", "", "Configurable CV Input 2", "Assignable modulation source via web editor (default adds to wet/dry)" },
                    },
                    {
                        { "any", "", "", "Configurable Pulse Input 1", "Assignable pulse source via web editor (default freeze gate)" },
                    },
                    {
                        { "any", "", "", "Configurable Pulse Input 2", "Assignable pulse source via web editor (default Turing clock)" },
                    },
                },
                {
                    {
                        { "any", "", "", "Reverb Output Left", "Stereo reverb output (left)" },
                    },
                    {
                        { "any", "", "", "Reverb Output Right", "Stereo reverb output (right)" },
                    },
                    {
                        { "any", "", "", "Configurable CV Output 1", "Assignable utility output (noise, MIDI/CV, Turing, clocked gate, and more)" },
                    },
                    {
                        { "any", "", "", "Configurable CV Output 2", "Assignable utility output (noise, MIDI/CV, Turing, clocked gate, and more)" },
                    },
                    {
                        { "any", "", "", "Configurable Pulse Output 1", "Assignable trigger/gate utility output" },
                    },
                    {
                        { "any", "", "", "Configurable Pulse Output 2", "Assignable trigger/gate utility output" },
                    },
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
            "resonator",
            {
                "resonator",
                "Resonator",
                "Workshop Computer Card",
                "Johan Eklund",
                "https://johaneklund.io/resonator",
                "Four-string sympathetic resonator inspired by Rings/tanpura techniques.\nFeed audio to excite resonant strings, or trigger plucks with PulseIn1.\nX sets base pitch (or fine tune with CV1 connected), Y sets damping, Main sets wet/dry mix.\nPulseIn2 or short Z-down press advances chord mode; long Z-down hold resets progression defaults.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/21_resonator",
                {
                    {
                        { "any", "", "", "Excitation Input Left", "Mixed with AudioIn2 to excite resonator strings" },
                    },
                    {
                        { "any", "", "", "Excitation Input Right", "Mixed with AudioIn1 to excite resonator strings" },
                    },
                    {
                        { "any", "", "", "Pitch CV", "1V/oct pitch input; X knob becomes fine tune when connected" },
                    },
                    {
                        { "any", "", "", "Damping CV", "Modulates damping/decay with Y knob as base setting" },
                    },
                    {
                        { "any", "", "", "Pluck Trigger", "Injects noise burst to excite all strings" },
                    },
                    {
                        { "any", "", "", "Next Chord Trigger", "Advances to the next chord in progression" },
                    },
                },
                {
                    {
                        { "any", "", "", "Resonator Output Mid", "Wet/dry mixed output using summed string image" },
                    },
                    {
                        { "any", "", "", "Resonator Output Side", "Wet/dry mixed output using alternate stereo string image" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
            "sheep",
            {
                "sheep",
                "Sheep",
                "Workshop Computer Card",
                "Dune Desormeaux",
                "",
                "Granular buffer processor with triggerable grains, reverse/forward playback, loop/glitch mode, and freeze.\nMain controls grain speed or CV2 pitch attenuation, X controls delay spread (or CV1 attenuation), Y sets grain size.\nPulseIn1 triggers grains; PulseIn2 acts as a grain gate. Z up freezes buffer, Z middle runs normal mode, Z down enables loop/glitch behavior.\nTwo firmware builds are provided in source comments: lo-fi longer buffer and hi-fi shorter buffer.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/22_sheep",
                {
                    {
                        { "any", "", "", "Granular Input Left", "Recorded into circular buffer when not frozen" },
                    },
                    {
                        { "any", "", "", "Granular Input Right", "Recorded into circular buffer when not frozen" },
                    },
                    {
                        { "any", "", "", "Grain Position CV", "Position/scrub control with X as attenuverter" },
                    },
                    {
                        { "any", "", "", "Grain Speed CV", "Pitch/speed modulation with Main as attenuverter" },
                    },
                    {
                        { "any", "", "", "Grain Trigger", "Rising edge triggers new grain" },
                    },
                    {
                        { "any", "", "", "Grain Gate", "Gates grain triggering when connected" },
                    },
                },
                {
                    {
                        { "any", "", "", "Granular Output Left", "Processed granular audio output" },
                    },
                    {
                        { "any", "", "", "Granular Output Right", "Processed granular audio output" },
                    },
                    {
                        { "any", "", "", "Random CV", "Random value updated when grains are triggered" },
                    },
                    {
                        { "any", "", "", "Buffer Phase CV", "Rising saw mapped to circular buffer write-head position" },
                    },
                    {
                        { "any", "", "", "Grain Completion Trigger", "Triggers when a grain reaches completion threshold" },
                    },
                    {
                        { "any", "", "", "Stochastic Clock", "Probabilistic pulse stream influenced by X and grain size" },
                    },
                },
                {
                    {
                        { "any", "", "", "Grain Speed", "Bidirectional speed from reverse to forward, with center pause detent" },
                    },
                    {
                        { "any", "", "", "Delay Spread", "Left half selects delay offset, right half increases random spread" },
                    },
                    {
                        { "any", "", "", "Grain Size", "Sets grain length from very short to long windows" },
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
                "Workshop Computer Card",
                "Brian Dorsey",
                "",
                "CV utility for generating manual voltages and attenuverting incoming control signals.\nMain, X, and Y knobs each drive a paired output; patching into corresponding inputs replaces knob value and applies attenuverting.\nPulse outputs follow Z switch state as complementary high/low gates.\nLEDs mirror output voltage levels in the same 2x3 physical layout as output jacks.",
                "",
                "https://codeberg.org/briandorsey/mtmws_cards/src/branch/main/crafted_volts",
                {
                    {
                        { "any", "", "", "Audio/CV Input A", "Mixed with AudioIn2 when present, then attenuverted by Main knob" },
                    },
                    {
                        { "any", "", "", "Audio/CV Input B", "Mixed with AudioIn1 when present, then attenuverted by Main knob" },
                    },
                    {
                        { "any", "", "", "CV Input X", "Replaces X knob source and is attenuverted by X position" },
                    },
                    {
                        { "any", "", "", "CV Input Y", "Replaces Y knob source and is attenuverted by Y position" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Main Voltage Output", "Main knob voltage (or mixed audio input voltage) after attenuverting" },
                    },
                    {
                        { "any", "", "", "Inverted Main Voltage Output", "Inverted copy of AudioOut1" },
                    },
                    {
                        { "any", "", "", "X Voltage Output", "X knob voltage or attenuverted CVIn1" },
                    },
                    {
                        { "any", "", "", "Y Voltage Output", "Y knob voltage or attenuverted CVIn2" },
                    },
                    {
                        { "any", "", "", "Z High Gate", "High when Z is momentary or up; low when Z is middle" },
                    },
                    {
                        { "any", "", "", "Complement Gate", "Inverse gate state of PulseOut1" },
                    },
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
            "utility_pair",
            {
                "utility_pair",
                "Utility Pair",
                "Workshop Computer Card",
                "Chris Johnson",
                "",
                "Utility Pair splits the card into independent left and right utility engines.\nEach side owns one knob/control and one column of input/output jacks, letting two utilities run at once.\nIn the 2025 multi-pack firmware, hold Z down at power/reset to enter utility selection mode, then choose left and right utilities and start.\nSpecific jack behavior depends on the chosen utility pair.",
                "MIT",
                "https://github.com/chrisgjohnson/Utility-Pair",
                {
                    {
                        { "any", "", "", "Left Utility Signal Input", "Left-side input routed according to selected left utility" },
                    },
                    {
                        { "any", "", "", "Right Utility Signal Input", "Right-side input routed according to selected right utility" },
                    },
                    {
                        { "any", "", "", "Left Utility CV Input", "Left-side CV input routed according to selected left utility" },
                    },
                    {
                        { "any", "", "", "Right Utility CV Input", "Right-side CV input routed according to selected right utility" },
                    },
                    {
                        { "any", "", "", "Left Utility Trigger Input", "Left-side pulse input routed according to selected left utility" },
                    },
                    {
                        { "any", "", "", "Right Utility Trigger Input", "Right-side pulse input routed according to selected right utility" },
                    },
                },
                {
                    {
                        { "any", "", "", "Left Utility Signal Output", "Left-side output generated by selected left utility" },
                    },
                    {
                        { "any", "", "", "Right Utility Signal Output", "Right-side output generated by selected right utility" },
                    },
                    {
                        { "any", "", "", "Left Utility CV Output", "Left-side CV output generated by selected left utility" },
                    },
                    {
                        { "any", "", "", "Right Utility CV Output", "Right-side CV output generated by selected right utility" },
                    },
                    {
                        { "any", "", "", "Left Utility Pulse Output", "Left-side pulse output generated by selected left utility" },
                    },
                    {
                        { "any", "", "", "Right Utility Pulse Output", "Right-side pulse output generated by selected right utility" },
                    },
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
            "clockwork",
            {
                "clockwork",
                "Clockwork",
                "Workshop Computer Card",
                "Vincent Maurer",
                "https://vincentmaurer.de/clockwork/index.html",
                "6-channel polyrhythmic clock, gate, and LFO/envelope generator inspired by Pamela's Workout.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/26_clockwork",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Wave Param Mod", "CV control over the active channel's wave parameter, or external Delay/Math carrier input" },
                    },
                    {
                        { "any", "", "", "Probability Mod", "CV control over the active channel's trigger probability" },
                    },
                    {
                        { "any", "", "", "Clock Sync", "External clock source sync input" },
                    },
                    {
                        { "any", "", "", "Reset In", "Clock reset trigger or run gate" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "Out 3\n(CV Out 1)", "Channel 3 CV/Gate output (PWM)" },
                    },
                    {
                        { "any", "", "", "Out 4\n(CV Out 2)", "Channel 4 CV/Gate output (PWM)" },
                    },
                    {
                        { "any", "", "", "Out 5\n(Pulse Out 1)", "Channel 5 Digital Gate output (GPIO)" },
                    },
                    {
                        { "any", "", "", "Out 6\n(Pulse Out 2)", "Channel 6 Digital Gate output (GPIO)" },
                    },
                },
                {
                    {
                        { "any", "", "", "Modifier / Shape", "Master modifier or waveform shape select depending on switch tier" },
                    },
                    {
                        { "any", "", "", "Steps / Level", "Euclidean steps or output level/delay depending on switch tier" },
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "UP: Sound Shaping (MAIN: Waveform, X: Level/Delay, Y: Wave Parameter). A short flick UP toggles Play/Pause." },
                    { "Middle", "MIDDLE: Rhythm & performance (MAIN: Modifier, X: Steps, Y: Fills [steps > 0] or LFO Phase Offset [steps = 0])." },
                    { "Down", "DOWN (Momentary): Tap to cycle pages. Hold down to access Advanced settings (MAIN: Loop length, X: Probability, Y: Step Offset [steps > 0] or Quantizer Scale [steps = 0])." }
                },
                true
            }
        },
        {
            "siren",
            {
                "siren",
                "Siren",
                "Workshop Computer Card",
                "Moses Hoyt",
                "",
                "Siren is a drone source with 6 oscillator banks (SINE, CLST, DTON, ANLG, WSHP, WAVE).\nUse switch up for WARP/SPAN/MORPH and switch middle for SEED/SCAN/BASIS.\nSwitch down (momentary) cycles banks with a smoothed crossfade.\nPulse In 1 gates the drone; Pulse In 2 randomizes SEED on short trigger and cycles bank on long hold.\nAudio In 1 can be processed and mixed with the drone; Audio In 2 modulates SPAN.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/27_Siren",
                {
                    {
                        { "any", "", "", "Processor Input", "External audio processed by Siren's shaping path and summed with the drone # certainty: high (README + code)" },
                    },
                    {
                        { "any", "", "", "Span Modulation", "External CV/audio modulation for oscillator spread (SPAN) # certainty: high (README + code)" },
                    },
                    {
                        { "any", "", "", "Pitch Modulation", "Added to BASIS/root pitch control" },
                    },
                    {
                        { "any", "", "", "Warp Modulation", "Modulates WARP amount" },
                    },
                    {
                        { "any", "", "", "Gate", "Opens/closes the drone envelope" },
                    },
                    {
                        { "any", "", "", "Seed / Bank Trigger", "Short pulse randomizes SEED, long hold cycles oscillator bank # certainty: high (README + code)" },
                    },
                },
                {
                    {
                        { "any", "", "", "Left Drone Output", "Left channel of stereo Siren output (plus processed input, if patched)" },
                    },
                    {
                        { "any", "", "", "Right Drone Output", "Right channel of stereo Siren output (plus processed input, if patched)" },
                    },
                    {
                        { "any", "", "", "Pitch CV Mirror", "Mirrors current basis pitch value" },
                    },
                    {
                        { "any", "", "", "Envelope CV", "Envelope level derived from Siren gate state" },
                    },
                    {
                        { "any", "", "", "Sub-osc Clock", "Square clock derived from oscillator phase" },
                    },
                    {
                        { "any", "", "", "Divide-by-2 Clock", "Divider pulse derived from PulseOut1 phase edges" },
                    },
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
            "eighties_bass",
            {
                "eighties_bass",
                "Eighties Bass",
                "Workshop Computer Card",
                "Tod Kurt (@todbot)",
                "",
                "Eighties Bass is a complete mono bass voice using 5 saw oscillators plus optional white noise.\nMain sets filter cutoff, X sets pitch offset, and Y sets resonance.\nPress switch down to cycle filter mode (LPF/BPF/HPF).\nCV In 1 provides pitch CV, CV In 2 offsets cutoff, Audio In 1 modulates detune, and Audio In 2 modulates noise amount.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/28_eighties_bass",
                {
                    {
                        { "any", "", "", "Detune Modulation", "Controls oscillator detune spread amount" },
                    },
                    {
                        { "any", "", "", "Noise Mix Modulation", "Controls amount of white noise mixed with oscillator signal" },
                    },
                    {
                        { "any", "", "", "Pitch CV", "Added to X knob offset for oscillator pitch (intended V/oct behavior) # certainty: medium (README says \"maybe\")" },
                    },
                    {
                        { "any", "", "", "Cutoff Modulation", "Bipolar additive modulation of filter cutoff" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio Left", "Main synthesized output (mirrored)" },
                    },
                    {
                        { "any", "", "", "Audio Right", "Main synthesized output (mirrored)" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
            "xht",
            {
                "xht",
                "XHT Card",
                "Workshop Computer Card",
                "Adrian Vos",
                "none",
                "XHT Card is a synthesised deep-note-style gesture rather than a bundled sample.\nMain sets the note position and one-shot destination. The momentary down switch\nresets the note and launches a one-shot from the start toward the current\ndestination. CV2 also participates in destination control. P2 can clock stepped\nmovement through the note. X controls delay and Y controls reverb.\n\nCV1 input transposes pitch. CV Out 1 mirrors note position unless CV1 is\npatched, in which case CV Out 1 becomes pitch. CV Out 2 always mirrors note\nposition. P1 gates the note when patched and leaves the note sustained when\nunpatched.\n\nThe separate MIDI build adds MIDI note input, CC1/mod-wheel destination\ncontrol, sustain pedal on CC64, and MIDI note output of the current chord.\nMIDI clock is ignored.",
                "GPL-3.0",
                "https://github.com/adrianvos/XHT_card",
                {
                    {
                        { "any", "", "", "External Audio Left", "Mixed into the left output before effects." },
                    },
                    {
                        { "any", "", "", "External Audio Right", "Mixed into the right output before effects." },
                    },
                    {
                        { "any", "", "", "Pitch Transpose", "Transposes the current note position. When patched, CV Out 1 reports pitch instead of position." },
                    },
                    {
                        { "any", "", "", "Position / Destination", "Controls note position and the one-shot destination; scaled so 0-5V controller sources cover the full travel." },
                    },
                    {
                        { "any", "", "", "Gate", "Gates the audio when patched. In the MIDI build, also gates MIDI output when patched." },
                    },
                    {
                        { "any", "", "", "External Clock", "Clocks stepped movement through the note and overrides the one-shot while clocked." },
                    },
                },
                {
                    {
                        { "any", "", "", "Left Output", "Left channel with delay and reverb." },
                    },
                    {
                        { "any", "", "", "Right Output", "Right channel with delay and reverb." },
                    },
                    {
                        { "any", "", "", "Position / Pitch", "Mirrors note position unless CV1 is patched, then outputs pitch." },
                    },
                    {
                        { "any", "", "", "Position", "Mirrors the current note-position state." },
                    },
                    {
                        { "any", "", "", "Note Gate", "Mirrors the current note gate; with P1 patched it follows P1." },
                    },
                    {
                        { "any", "", "", "Clock Mirror", "Mirrors the P2 input pulse." },
                    },
                },
                {
                    {
                        { "any", "", "", "Position / Destination", "Manually scrubs the note and sets the one-shot destination." },
                    },
                    {
                        { "any", "", "", "Delay", "Controls delay amount/time character." },
                    },
                    {
                        { "any", "", "", "Reverb", "Controls reverb amount." },
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
                "Workshop Computer Card",
                "Tod Kurt (@todbot)",
                "",
                "cirpy_wavetable loads WAV wavetables and plays them as a continuously running oscillator.\nMain sets wavetable position, X sets LFO modulation amount, and Y sets LFO rate.\nSwitch down selects next wavetable file; switch up toggles CV pitch quantization.\nCV In 1 controls pitch and CV In 2 offsets wavetable position.\nCV Out 1 mirrors wavetable position and CV Out 2 mirrors LFO modulation signal.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/30_cirpy_wavetable",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Pitch CV", "Pitch control input mapped to a MIDI-note range (approximate 1V/oct behavior)" },
                    },
                    {
                        { "any", "", "", "Wavetable Position Mod", "Adds bipolar modulation to wavetable position" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "Wavetable Position CV", "Reflects current wavetable position value" },
                    },
                    {
                        { "any", "", "", "LFO Modulation CV", "Reflects current wavemod LFO value" },
                    },
                    {
                        { "any", "", "", "PWM Audio Out A", "Pulse output configured as audio output stream" },
                    },
                    {
                        { "any", "", "", "PWM Audio Out B", "Pulse output configured as mirrored audio output stream" },
                    },
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
            "drumdrum",
            {
                "drumdrum",
                "drumdrum",
                "Workshop Computer Card",
                "Moses Hoyt",
                "https://mohoyt.com/drumdrum.html",
                "drumdrum is an 8-step DFAM-inspired sequencer card.\nSwitch up is play (tempo, length, VCO2 offset), switch middle is edit (step pitch and velocity),\nand switch down supports short press cursor advance / long press play-pause.\nSequence data is randomized on reset and shared across panel, Monome Grid, 8mu, and browser editor workflows.\nPulse In 1 is external clock and Pulse In 2 resets to step 1.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/33_drumdrum",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Velocity Mod", "Summed into velocity CV output (decay modulation use-case)" },
                    },
                    {
                        { "any", "", "", "Global Transpose", "Global pitch transpose applied to both pitch outputs" },
                    },
                    {
                        { "any", "", "", "External Clock", "Rising edge advances one sequencer step, overriding internal clock" },
                    },
                    {
                        { "any", "", "", "Reset", "Rising edge resets playback step to step 1" },
                    },
                },
                {
                    {
                        { "any", "", "", "White Noise", "Continuous white noise output" },
                    },
                    {
                        { "any", "", "", "VCO 2 Pitch CV", "Pitch CV mirror with Y offset (via audio DAC path, uncalibrated)" },
                    },
                    {
                        { "any", "", "", "VCO 1 Pitch CV", "Calibrated 1V/oct pitch from current step" },
                    },
                    {
                        { "any", "", "", "Velocity CV", "Step velocity-derived CV with CV In 1 modulation" },
                    },
                    {
                        { "any", "", "", "Step Trigger", "Trigger pulse every step (also preview trigger while paused in edit mode)" },
                    },
                    {
                        { "any", "", "", "End-of-Cycle Trigger", "Trigger pulse when sequence wraps from last step to first" },
                    },
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
            "dual_quant",
            {
                "dual_quant",
                "Dual Quant",
                "Workshop Computer Card",
                "Music Thing Modular",
                "",
                "Dual quantised granular pitch shifter with calibrated 1V/oct CV outputs",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/34_dual_quant",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Pitch Mod A", "Additional semitone modulation for output A" },
                    },
                    {
                        { "any", "", "", "Pitch Mod B", "Additional semitone modulation for output B" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "Pitch CV Out\nA", "Pitch A converted to millivolt CV output" },
                    },
                    {
                        { "any", "", "", "Pitch CV Out\nB", "Pitch B converted to millivolt CV output" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Quantize\nEnable", "Enables chromatic semitone quantization for both outputs" },
                    },
                    {
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Z up: MAIN: Global Pitch Offset; X: Output A Offset; Y: Output B Offset." },
                    { "Middle", "Z middle: MAIN: Quantize Enable." },
                    { "Down", "" }
                },
                true
            }
        },
        {
            "od",
            {
                "od",
                "Od",
                "Workshop Computer Card",
                "M. John Mills",
                "",
                "Simulates a Lorenz attractor to generate loopable CV and pulse signals.\nThis release folder is a stub; documentation and UF2 builds are maintained in the external repository.",
                "MIT",
                "https://github.com/MJLMills/mtmws_od",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
                "Workshop Computer Card",
                "Dune Desormeaux",
                "https://dessertplanet.github.io/web-druid/",
                "Blackbird runs crow-compatible Lua scripts over USB serial and can also store scripts on card flash.\nPanel control behavior is script-defined: Lua code reads `bb.knob.*`, `bb.switch.position`, and inputs,\nthen decides how outputs respond. LEDs always reflect positive output voltages on the six outputs.",
                "GPLv3 or later",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/41_blackbird",
                {
                    {
                        { "any", "", "", "bb.audioin[1]", "Audio input query source for Lua scripts" },
                    },
                    {
                        { "any", "", "", "bb.audioin[2]", "Audio input query source for Lua scripts" },
                    },
                    {
                        { "any", "", "", "input[1]", "Script-readable CV input mapped to crow input[1]" },
                    },
                    {
                        { "any", "", "", "input[2]", "Script-readable CV input mapped to crow input[2]" },
                    },
                    {
                        { "any", "", "", "bb.pulsein[1]", "Digital input with change/clock detection for Lua callbacks" },
                    },
                    {
                        { "any", "", "", "bb.pulsein[2]", "Digital input with change/clock detection for Lua callbacks" },
                    },
                },
                {
                    {
                        { "any", "", "", "output[3]", "Uncalibrated CV/audio output under script control" },
                    },
                    {
                        { "any", "", "", "output[4]", "Uncalibrated CV/audio output under script control" },
                    },
                    {
                        { "any", "", "", "output[1]", "Calibrated CV output under script control" },
                    },
                    {
                        { "any", "", "", "output[2]", "Calibrated CV output under script control" },
                    },
                    {
                        { "any", "", "", "bb.pulseout[1]", "Digital pulse output for Lua-triggered actions" },
                    },
                    {
                        { "any", "", "", "bb.pulseout[2]", "Digital pulse output for Lua-triggered actions" },
                    },
                },
                {
                    {
                        { "any", "", "", "bb.knob.main", "Normalized analog knob value exposed to Lua; behavior depends on loaded script" },
                    },
                    {
                        { "any", "", "", "bb.knob.x", "Normalized analog knob value exposed to Lua; behavior depends on loaded script" },
                    },
                    {
                        { "any", "", "", "bb.knob.y", "Normalized analog knob value exposed to Lua; behavior depends on loaded script" },
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
                "Workshop Computer Card",
                "Brian Dorsey",
                "",
                "Ambient backyard rain playback card with user-controlled intensity.\nThis release folder is a stub; docs and release UF2 files are maintained in the external repository.",
                "",
                "https://codeberg.org/briandorsey/mtmws_cards",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
            "castle_process",
            {
                "castle_process",
                "Castle Process",
                "Workshop Computer Card",
                "Adrian Vos",
                "",
                "Castle Process is a performance card built around chopped external audio, crude\ninternal squarewave energy, aggressive switching between sources, and a separate\nbass pulse layer. It is designed as a playable sound-destruction tool rather than\na clean effect or faithful clone.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/43_Castle_Process",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV\n1", "Internal control modulation input" },
                    },
                    {
                        { "any", "", "", "CV\n2", "Internal control modulation input" },
                    },
                    {
                        { "any", "", "", "Bass\nTrigger", "External bass trigger input that takes over bass timing and suppresses the internal bass trigger behaviour" },
                    },
                    {
                        { "any", "", "", "Pulse\n2", "Reserved for further interaction" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Bass\nActivity", "Bass activity pulse output" },
                    },
                    {
                        { "any", "", "", "Chop\nPulse", "Internal chop pulse output" },
                    },
                },
                {
                    {
                        { "any", "", "", "Drive /\nVoicing", "Controls overall drive and voicing" },
                    },
                    {
                        { "any", "", "", "Chopping /\nMotion", "Controls chopping behaviour and motion" },
                    },
                    {
                        { "any", "", "", "Tuning /\nInteraction", "Controls tuning and internal interaction" },
                    },
                },
                {
                    "Z",
                    { "Up", "Alternate latched mode with a little more body and more squarewave colour." },
                    { "Middle", "Default tighter and more direct chopped mode." },
                    { "Down", "Momentary bend / chaos gesture while held for live performance accents." }
                },
                true
            }
        },
        {
            "birds",
            {
                "birds",
                "Birds",
                "Workshop Computer Card",
                "Tom Whitwell",
                "",
                "Two interacting birds are generated from a Turing-style looped random sequencer.\nMain controls lock/change behavior, X shifts bird pitch, Y controls phrase speed,\nZ selects normal/wild/reseed modes, and Pulse In 1 can replace the internal clock.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/44_Birds",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Pitch CV Modulation", "Adds to X knob to control bird voice oscillator pitch" },
                    },
                    {
                        { "any", "", "", "Speed CV Modulation", "Adds to Y knob to control sequence playback timing" },
                    },
                    {
                        { "any", "", "", "External Clock", "Rising edge clock input; when present it overrides internal timing" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Bird One Audio", "Bird one audio voice" },
                    },
                    {
                        { "any", "", "", "Bird Two Audio", "Bird two audio voice" },
                    },
                    {
                        { "any", "", "", "Bird One Pitch Trace", "Held CV trace derived from bird one pitch contour" },
                    },
                    {
                        { "any", "", "", "Bird Two Pitch Trace", "Held CV trace derived from bird two pitch contour" },
                    },
                    {
                        { "any", "", "", "Bird One Onset Pulse", "Triggers on phrase onsets and internal trill/run sub-events for bird one" },
                    },
                    {
                        { "any", "", "", "Bird Two Onset Pulse", "Triggers on phrase onsets and internal trill/run sub-events for bird two" },
                    },
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
            "two_tracks",
            {
                "two_tracks",
                "Two Tracks",
                "Workshop Computer Card",
                "Joep Vermaat",
                "",
                "A dual-read-head audio looper inspired by Steve Reich's phase music.\nRecord a loop and play it through two independent outputs with separately\ncontrollable read positions and loop lengths, creating evolving phase\npatterns and interference effects.\n\nRecording uses a hands-free three-state machine: PLAY -> ARMED -> RECORD -> PLAY.\nZ Down arms recording; a 3-second countdown with audible ticks begins; recording\nstarts automatically when the countdown expires; Z Down again stops recording.\n\nTwo play modes (Z switch selects after recording):\n  Offset mode (Z Middle): each knob sets position offset within the loop.\n  Phasing mode (Z Up): right loop is shorter than left, creating natural periodic\n  phasing. Knob Y adds a speed offset on the right head.\n\nAudio is stored in flash as IMA-ADPCM (4 bits per sample). Capacity depends on\nthe card: about 70 seconds on a 2 MB card, 11 minutes on a 16 MB card.",
                "MIT",
                "https://codeberg.org/johantv/two-tracks",
                {
                    {
                        { "any", "", "", "Audio In", "Mono audio input — recorded to the flash loop. Both read heads play back this single loop." },
                    },
                    {
                        { "any", "", "", "Audio In 2", "Unused (recording is mono)." },
                    },
                    {
                        { "any", "", "", "CV 1", "In Offset mode modulates left offset; in Phasing mode modulates right loop length." },
                    },
                    {
                        { "any", "", "", "CV 2", "In Offset mode modulates right offset; in Phasing mode modulates right speed." },
                    },
                    {
                        { "any", "", "", "Pulse 1", "Resets the left read head to the loop start." },
                    },
                    {
                        { "any", "", "", "Pulse 2", "Resets the right read head to the loop start." },
                    },
                },
                {
                    {
                        { "any", "", "", "Left Out", "Left read head output." },
                    },
                    {
                        { "any", "", "", "Right Out", "Right read head output." },
                    },
                    {
                        { "any", "", "", "CV 1", "Left head phase position (play), buffer fill (record), 0 (armed)." },
                    },
                    {
                        { "any", "", "", "CV 2", "Right head phase position (play), loop length (record), 0 (armed)." },
                    },
                    {
                        { "any", "", "", "Pulse 1", "Unused." },
                    },
                    {
                        { "any", "", "", "Pulse 2", "Unused." },
                    },
                },
                {
                    {
                        { "any", "", "", "Loop window / Right loop length", "In Offset mode controls the loop window length (tiny fragment -> full loop). In Phasing mode controls how much shorter the right loop is (short -> full)." },
                    },
                    {
                        { "any", "", "", "Left position offset / Right window start", "In Offset mode sets left head position offset. In Phasing mode sets where the shortened right window starts within the loop." },
                    },
                    {
                        { "any", "", "", "Right position offset / Right speed", "In Offset mode sets right head position offset. In Phasing mode sets right playback speed (0.9x - 1.1x, centre = 1.0x)." },
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
                "Workshop Computer Card",
                "WorkshopSystem",
                "",
                "Multi-FX and Synth Firmware",
                "GPL-3.0",
                "",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
                "Workshop Computer Card",
                "Vincent Maurer",
                "https://vincentmaurer.de/grains/grains_manager.html",
                "Grains records and replays granular audio from a live buffer or stored sample slots. The interface is page-based:\npage 1 (position/density/size), page 2 (envelope/tone/pitch), page 3 (mix/spread/feedback-diffusion), and page 4\n(reverb). Flick/hold gestures control page changes, freeze, and load/save menus. Tape Mode is available at power-up\nfor scrub-style sample playback from the same slot system.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/51_grains",
                {
                    {
                        { "any", "", "", "Audio Input Left / Mono", "Source audio for granular recording and processing" },
                    },
                    {
                        { "any", "", "", "Audio Input Right", "CV/audio modulation for grain density behavior" },
                    },
                    {
                        { "any", "", "", "Pitch CV", "1V/oct style control for grain or tape playback pitch" },
                    },
                    {
                        { "any", "", "", "Position CV", "Grain playhead offset or tape scrub position modulation" },
                    },
                    {
                        { "any", "", "", "Grain Trigger", "External trigger for grain spawning or tape play/pause gating" },
                    },
                    {
                        { "any", "", "", "Freeze / Reset", "Freeze control in granular mode; playhead reset in tape mode" },
                    },
                },
                {
                    {
                        { "any", "", "", "Stereo Out Left", "Main processed left output" },
                    },
                    {
                        { "any", "", "", "Stereo Out Right", "Main processed right output" },
                    },
                    {
                        { "any", "", "", "Envelope / Playhead Ramp", "Looping ramp CV tied to buffer/playhead progress" },
                    },
                    {
                        { "any", "", "", "Random / Motion CV", "Grain-randomness or movement/speed CV output" },
                    },
                    {
                        { "any", "", "", "Grain / End-of-Cycle Trigger", "Trigger output for grain timing or tape EOC" },
                    },
                    {
                        { "any", "", "", "Freeze / Midpoint Trigger", "High when frozen in granular mode or midpoint trigger in tape mode" },
                    },
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
            "lens",
            {
                "lens",
                "Lens",
                "Workshop Computer Card",
                "Graham Ritchie",
                "",
                "A programmable synth. Write patches in Loupe (a tiny Lisp); sequences of values read through lenses become pitch, rhythm, CV and audio.",
                "",
                "",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
                "Workshop Computer Card",
                "Steve Jones",
                "",
                "Granular Looping Sampler",
                "",
                "",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
                "Workshop Computer Card",
                "Music Thing Modular",
                "",
                "Mono-input stereo cassette warble processor with wow, flutter, hiss, crackle, and tape\nwear morphing.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/54_Tapegrade",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Tape Depth\nMod", "Modulates wow/pitch movement amount; also routed to CV Out 1 attenuator path" },
                    },
                    {
                        { "any", "", "", "Instability\nMod", "Modulates flutter/transport agitation; also routed to CV Out 2 attenuator path" },
                    },
                    {
                        { "any", "", "", "Damage Burst\nTrig", "Rising edge forces brief heavy tape degradation burst" },
                    },
                    {
                        { "any", "", "", "Crackle Gate", "High gate forces stronger crackle generation" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV1\nAttenuated", "CV In 1 passthrough scaled by X knob" },
                    },
                    {
                        { "any", "", "", "CV2\nAttenuated", "CV In 2 passthrough scaled by Y knob" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Wet / Dry\nMix", "Blend between dry input and processed tape signal" },
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
                    { "Up", "Brighter and more stable tape response." },
                    { "Middle", "Darker tone with moderate hiss and instability." },
                    { "Down", "Strong hiss, crackle, and unstable behavior." }
                },
                true
            }
        },
        {
            "fifths",
            {
                "fifths",
                "Fifths",
                "Workshop Computer Card",
                "Dune Desormeaux",
                "",
                "Fifths quantizes incoming or generated CV to keys arranged around the circle of fifths and outputs both\na quantized note and an ambiguous third harmony voice. It can run from external clock (Pulse In 1) or internal\ntap-tempo clock, and can loop or write incoming material depending on switch state/toggles. Loop length is set by X,\nkey center is selected by Main (or CV In 2), and CV In 1 transposes up to two octaves. Pulse outputs provide\nan internal clock pulse plus a probabilistic sequence pulse stream.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/55_fifths",
                {
                    {
                        { "any", "", "", "CV/Audio Source", "Main signal to be sampled/quantized (or internal random source when unpatched)" },
                    },
                    {
                        { "any", "", "", "VCA CV", "Optional VCA/modulation control for input scaling" },
                    },
                    {
                        { "any", "", "", "Transpose CV", "Adds up to roughly two octaves of transpose before quantization" },
                    },
                    {
                        { "any", "", "", "Key CV", "Selects key index around the circle of fifths" },
                    },
                    {
                        { "any", "", "", "External Clock", "Advances quantizer/sequencer on rising edge when connected" },
                    },
                    {
                        { "any", "", "", "Loop Toggle", "Toggles loop/write behavior" },
                    },
                },
                {
                    {
                        { "any", "", "", "Key Monitor Output", "Main knob-centered monitoring signal" },
                    },
                    {
                        { "any", "", "", "VCA Output", "Scaled input/random signal used as quantizer source" },
                    },
                    {
                        { "any", "", "", "Quantized Note", "Primary quantized pitch output (MIDI note mapped to CV)" },
                    },
                    {
                        { "any", "", "", "Third Harmony", "Harmonic third (major/minor context-sensitive) from quantized note" },
                    },
                    {
                        { "any", "", "", "Internal Clock Pulse", "Gate pulse at internal quarter-note timing" },
                    },
                    {
                        { "any", "", "", "Sequence Pulse", "Pulse stream from probabilistic/looped pulse sequencer" },
                    },
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
                        { "any", "", "", "VCA Att", "Controls input scaling; can be replaced by Audio In 2 control" },
                        { "down", "hold", "", "Pulse Probability Threshold", "Sets probability threshold for sequence pulse generation" },
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
                "Workshop Computer Card",
                "Andy Jenkinson (uglifruit)",
                "",
                "Glitch records into a shared 2.33-second circular buffer and runs in two power-on modes. Glitch mode loops\nratcheted sub-slices with optional reverse, decimation, and bitcrush; Stutter mode slices beats, shuffles order,\nand can repeat slices before advancing. Pulse In 1 sets beat timing, CV In 1 freezes recording while playback\ncontinues, and Audio Out 2 stays dry for parallel routing.",
                "MIT",
                "https://github.com/uglifruit/Workshop_Computer/tree/main/Demonstrations%2BHelloWorlds/PicoSDK/ComputerCard/examples/glitch",
                {
                    {
                        { "any", "", "", "Main Audio Input", "Source audio for buffering and processing" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Freeze CV", "Above approximately 0V stops recording and loops frozen buffer content" },
                    },
                    {
                        { "any", "", "", "Mod CV", "Bipolar modulation added to performance knobs (mode-dependent mapping)" },
                    },
                    {
                        { "any", "", "", "Clock Input", "Rising edge defines beat length (max 2.33 seconds)" },
                    },
                    {
                        { "any", "", "", "External Gate", "Gate control for switch-middle trigger behavior in both modes" },
                    },
                },
                {
                    {
                        { "any", "", "", "Processed Output", "Glitched/stuttered output or pass-through when effect inactive" },
                    },
                    {
                        { "any", "", "", "Dry Output", "Always dry pass-through of Audio In 1" },
                    },
                    {
                        { "any", "", "", "Activity Gate", "High while glitch/shuffle is active, low during pass-through" },
                    },
                    {
                        { "any", "", "", "Descending Ramp", "Falls across each slice and resets at slice boundary" },
                    },
                    {
                        { "any", "", "", "Slice Clock", "Pulse at each ratchet/slice boundary" },
                    },
                    {
                        { "any", "", "", "Clock Mirror", "Direct mirror of Pulse In 1" },
                    },
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
                "Lochovibes",
                "Workshop Computer Card",
                "Music Thing Modular",
                "",
                "Stereo chorus and vibrato effect featuring triangle, sine, and slow drift LFO modes,\nmodulation-based delay movement, and tape-style saturation.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/58_LoChoVibes",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Depth Mod", "Bipolar CV modulation for X depth control" },
                    },
                    {
                        { "any", "", "", "Character\nMod", "Bipolar CV modulation for Y Character control" },
                    },
                    {
                        { "any", "", "", "Ext LFO\nClock", "Rising edges sync LFO timing to external pulse source with internal fallback\nwhen pulses stop" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "LFO CV", "Main internal LFO waveform as CV" },
                    },
                    {
                        { "any", "", "", "Inverted LFO\nCV", "Inverted version of the internal LFO waveform" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Mod Rate", "LFO speed for chorus/vibrato movement" },
                    },
                    {
                        { "any", "", "", "Mod Depth", "LFO pitch/modulation depth amount" },
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Z up: MAIN: Modulation Rate; X: Modulation Depth; Y: Character." },
                    { "Middle", "Z middle: MAIN: Modulation Rate; X: Modulation Depth; Y: Character." },
                    { "Down", "Z down, momentary: MAIN: LFO Shape Select." }
                },
                true
            }
        },
        {
            "bitphase",
            {
                "bitphase",
                "Bitphase",
                "Workshop Computer Card",
                "Music Thing Modular",
                "",
                "Resonant 4-stage phaser with wide modulation sweeps, tremolo blending, and Burst-mode degradation",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/59_BitPhase",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Rate CV", "Modulates phaser and tremolo speed" },
                    },
                    {
                        { "any", "", "", "Resonance CV", "Modulates phaser feedback amount" },
                    },
                    {
                        { "any", "", "", "LFO Reset", "Resets phaser LFO so sweep can be clock-synchronised" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Processed Output A", "Dual mono output of processed signal" },
                    },
                    {
                        { "any", "", "", "Processed Output B", "Dual mono output of processed signal" },
                    },
                    {
                        { "any", "", "", "Phaser LFO CV", "Phaser LFO waveform output" },
                    },
                    {
                        { "any", "", "", "Tremolo LFO CV", "Tremolo LFO waveform output" },
                    },
                    {
                        { "any", "", "", "Burst Active", "High while Burst mode is active" },
                    },
                    {
                        { "any", "", "", "Phaser LFO Phase", "Phaser LFO phase indicator for synchronising external events" },
                    },
                },
                {
                    {
                        { "any", "", "", "Rate", "Speed of the internal phaser and tremolo LFOs" },
                    },
                    {
                        { "any", "", "", "Sweep Depth", "Phaser modulation depth from narrow movement to wide sweeping notches" },
                    },
                    {
                        { "any", "", "", "Resonance", "Phaser feedback amount from smooth phasing to edge-of-self-oscillation" },
                    },
                },
                {
                    "Z",
                    { "Up", "Pure resonant 4-stage all-pass phaser with the cleanest signal path." },
                    { "Middle", "Blends phasing with tremolo for animated pulsing movement." },
                    { "Down", "Adds sample-and-hold reduction, bit-depth reduction, and unstable lo-fi texture." }
                },
                true
            }
        },
        {
            "markov",
            {
                "markov",
                "Markov",
                "Workshop Computer Card",
                "Andy Jenkinson (uglifruit)",
                "",
                "Dual Markov-chain generator with melody on voice A and rhythm/percussion on voice B.\nKnob X selects melodic transition profile, Knob Y selects percussion profile, and\nMain is switch-dependent: transpose (Z up), loop lock/mutate length (Z middle), or\nscale select while held (Z down). Pulse In 1 clocks both chains; CV Out 1 carries\nmelody pitch, Pulse Out 1 emits pitch-change gate, Pulse Out 2 emits percussion\ntriggers/ratchets/flams, and CV Out 2 carries percussion accent.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/60_markov",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Melody Post-Scale Transpose", "Bipolar semitone transpose applied after scale quantization (disabled while scale-select switch down is held)" },
                    },
                    {
                        { "any", "", "", "Internal Tempo CV", "Internal tempo control when external clock is absent" },
                    },
                    {
                        { "any", "", "", "Master Clock", "Rising edge advances both Markov chains; after timeout the module falls back to internal CV-controlled tempo" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Internal Synth Voice A", "Square-wave synth voice tied to melody chain and percussion gating" },
                    },
                    {
                        { "any", "", "", "Internal Output B / Dual Melody Voice B", "Unused in primary mode; outputs second synth voice in dual melody mode" },
                    },
                    {
                        { "any", "", "", "Melody Pitch CV", "Quantized melody pitch output (V/Oct calibrated by firmware mapping)" },
                    },
                    {
                        { "any", "", "", "Percussion Accent CV", "Accent level CV derived from percussion state" },
                    },
                    {
                        { "any", "", "", "Melody Change Gate", "Short pulse when quantized melody pitch changes" },
                    },
                    {
                        { "any", "", "", "Percussion Trigger", "Markov percussion trigger output including ratchets/flams" },
                    },
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
            "voices_of_sid",
            {
                "voices_of_sid",
                "Voices Of Sid",
                "Workshop Computer Card",
                "Joep Vermaat",
                "",
                "Dual MOS 6581 SID emulation (reSID engine) with CV/gate control, stereo output, waveform selection, and randomize",
                "",
                "",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
                "Workshop Computer Card",
                "Infinite Digits",
                "https://infinitedigits.co/stretchcore/",
                "Mono sample-loop player with browser-loaded sample bank, tempo control, timestretch,\nand jump/selection gestures. Main knob targets playback position or sample index\ndepending on switch gesture, X sets internal tempo when not externally clocked, and\nY sets timestretch amount (with optional CV1 modulation). Switch down jumps to Main\nposition and emits Pulse Out 1; switch up selects sample from Main position and emits\nPulse Out 2. Pulse In 1 provides external clock, and Pulse In 2 triggers CV2-position\njumps (or loop start when CV2 is unpatched).",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/66_stretchcore",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Timestretch Modulation", "Adds bipolar modulation to Y timestretch control when patched" },
                    },
                    {
                        { "any", "", "", "Jump Position CV", "Position source for PulseIn2 jumps" },
                    },
                    {
                        { "any", "", "", "External Clock", "Rising-edge external clock; internal tempo is used when external clock is absent" },
                    },
                    {
                        { "any", "", "", "Jump Trigger", "Rising edge jumps to CVIn2-defined loop position, or loop start when CVIn2 is unpatched" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio Output Left", "Left audio output from current sample loop playback" },
                    },
                    {
                        { "any", "", "", "Audio Output Right", "Right audio output from current sample loop playback" },
                    },
                    {
                        { "any", "", "", "Random CV 1", "Smooth slow bipolar random modulation output" },
                    },
                    {
                        { "any", "", "", "Random CV 2", "Smooth slow bipolar random modulation output" },
                    },
                    {
                        { "any", "", "", "Jump Gesture Trigger", "Trigger pulse emitted after debounced switch-down jump action" },
                    },
                    {
                        { "any", "", "", "Sample-Select Gesture Trigger", "Trigger pulse emitted after debounced switch-up sample-select action" },
                    },
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
            "fragments",
            {
                "fragments",
                "Fragments",
                "Workshop Computer Card",
                "Max Harnishfeger",
                "web",
                "Six-slot audio recorder and clocked fragment sequencer with browser librarian, MIDI pitch control, random CV outputs, and an alternate long-sample variation mode.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/67_Fragments",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
                "Workshop Computer Card",
                "Joep Vermaat",
                "https://degenerator-web.netlify.app/",
                "Degenerator — Disintegrating Looper. Capture audio loops and apply irreversible degradation with 6 algorithms (Saturation, Filter Drift, Tape Hiss, Oxide Shedding, Bit Crush, Bit Rot) via preview/apply workflow. Inspired by William Basinski's The Disintegration Loops.",
                "",
                "https://codeberg.org/johantv/Degenerator",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
                "Workshop Computer Card",
                "Joep Vermaat",
                "",
                "Motorik drum machine with kick/snare/hihat, bass and melody CV outputs and inputs. Classic Krautrock grooves.",
                "",
                "https://codeberg.org/johantv/motorik",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
                "Workshop Computer Card",
                "Music Thing Modular",
                "",
                "MIDI-clockable generative rhythm and melody organism inspired by Pet Rock",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/74_Wild_Pebble",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Density Mod", "Modulates X density control" },
                    },
                    {
                        { "any", "", "", "Mutation Mod", "Modulates Y mutation control" },
                    },
                    {
                        { "any", "", "", "Ext Clock", "External clock input; when active it overrides internal tempo clock" },
                    },
                    {
                        { "any", "", "", "Freeze Gate", "While held high, mutation updates are disabled and structure is preserved" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "Quant Melody\nCV", "Quantized pitch CV from evolving scale-constrained sequence" },
                    },
                    {
                        { "any", "", "", "Energy /\nTension CV", "Smoothed evolving modulation output from internal energy and tension state" },
                    },
                    {
                        { "any", "", "", "Primary Trig\nStream", "Main rhythm trigger stream used for melodic progression and kick events" },
                    },
                    {
                        { "any", "", "", "Companion\nTrig Stream", "Derived companion trigger stream used for snare/percussion events" },
                    },
                },
                {
                    {
                        { "any", "", "", "Internal\nTempo", "Internal clock speed with variable swing profile according to switch position when internally clocked" },
                    },
                    {
                        { "any", "", "", "Density", "Trigger probability density control (modulated by CVIn1)" },
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Z up: Steady mode. Restrained mutation, tighter rhythms, and the calmest phrasing feel." },
                    { "Middle", "Z middle: Drift mode. Balanced mutation, moderate swing, and gradual harmonic movement." },
                    { "Down", "Z down: Surge mode. Strongest swing, wider melodic leaps, and the most animated behavior." }
                },
                true
            }
        },
        {
            "turing_clouds",
            {
                "turing_clouds",
                "Turing Clouds",
                "Workshop Computer Card",
                "Ainews",
                "",
                "Turing Machine-driven granular texture generator and rhythmic delay for the Workshop Computer",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/75_Turing_Clouds",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
            "hot_fuzz",
            {
                "hot_fuzz",
                "Hot Fuzz",
                "Workshop Computer Card",
                "Johan Vermaat",
                "",
                "Hot Fuzz is a stereo fuzz/wah effects card. It combines a clipper (soft/hard/asymmetric/foldback)\nwith a resonant state-variable filter for the wah sweep, and supports both manual\nand auto-wah modes.\n\nSwitch Up:   Fuzz + wah. Main pot selects fuzz type (4 zones: soft/hard/asym/fold,\n             CCW to CW). X pot = fuzz drive.\nSwitch Mid:  Fuzz + wah blend. Main pot = dry/wet blend (CCW=dry, CW=full fuzz).\n             X pot = drive.\nSwitch Down: Wah mode. Main pot = manual wah sweep (auto-wah off) or\n             base frequency (auto-wah on). Toggle auto-wah by double-tapping\n             Down (Mid→Down→Mid→Down within 0.5 s). LED 4 flashes to confirm.\n             X pot = dry/wet blend.\n\nY pot:       Wah Q / resonance (0 = flat, 4095 = high resonance). Same in all modes.\n\nCV1 (optional): overrides wah cutoff when no pot controls it (Up/Mid modes).\nCV2 (optional): adds to the X pot drive value.\n\nSettings are not persisted — fuzz type, auto-wah state, and all settings\nreset to defaults on power cycle.",
                "",
                "",
                {
                    {
                        { "any", "", "", "Left Audio In", "Left channel input (-2048..2047, 12-bit signed)" },
                    },
                    {
                        { "any", "", "", "Right Audio In", "Right channel input (-2048..2047, 12-bit signed)" },
                    },
                    {
                        { "any", "", "", "Wah CV (optional)", "Overrides wah cutoff in Up/Mid modes when connected" },
                    },
                    {
                        { "any", "", "", "Drive CV (optional)", "Adds to X pot drive value" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Left Audio Out", "Processed left channel (-2048..2047, 12-bit signed)" },
                    },
                    {
                        { "any", "", "", "Right Audio Out", "Processed right channel (-2048..2047, 12-bit signed)" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
                },
                {
                    {
                        { "up", "", "", "Fuzz Type", "4 zones — soft(0-1023)/hard(1024-2047)/asym(2048-3071)/fold(3072-4095)" },
                        { "middle", "", "", "Fuzz Blend", "0 = dry (bypass fuzz), 4095 = full fuzz wet" },
                        { "down", "", "", "Wah Frequency / Auto-Wah Base", "Manual sweep (auto-wah off) or base frequency (auto-wah on). Toggle auto-wah by double-tapping Down." },
                    },
                    {
                        { "up", "", "", "Fuzz Drive", "0 = clean, 4095 = maximum fuzz" },
                        { "middle", "", "", "Fuzz Drive", "0 = clean, 4095 = maximum fuzz" },
                        { "down", "", "", "Dry / Wet Blend", "0 = dry (unprocessed), 4095 = fully wet (wah only)" },
                    },
                    {
                        { "up", "", "", "Wah Q / Resonance", "0 = flat filter, 4095 = high resonance" },
                        { "middle", "", "", "Wah Q / Resonance", "0 = flat filter, 4095 = high resonance" },
                        { "down", "", "", "Wah Q / Resonance", "0 = flat filter, 4095 = high resonance" },
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
                "Workshop Computer Card",
                "Chris Johnson",
                "",
                "Early proof-of-concept LPC speech card that babbles randomized words. Z switch sets continuous,\noff, or single-word behavior. Main + CVIn1 set pitch (with X as attenuverter), while Y + CVIn2\nset speaking speed. AudioOut1 is speech, AudioOut2 exposes the LPC exciter components.",
                "GPL-3.0",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/78_Talker",
                {
                    {
                        { "any", "", "", "Exciter Audio Replace", "When patched, replaces the pitched component of the LPC exciter path" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Pitch CV", "Adds pitch modulation, scaled by knob X attenuverter" },
                    },
                    {
                        { "any", "", "", "Speed CV", "Adds modulation to babble speed" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Speech Output", "Main synthesized speech output" },
                    },
                    {
                        { "any", "", "", "LPC Exciter Components", "Pitched and noise components of the LPC exciter" },
                    },
                    {
                        { "any", "", "", "Exciter Amplitude", "Exciter amplitude control signal output" },
                    },
                    {
                        { "any", "", "", "Exciter Pitch", "Exciter pitch control signal output" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
            "computer_grids",
            {
                "computer_grids",
                "Computer Grids",
                "Workshop Computer Card",
                "Phil Miller",
                "",
                "Grids-inspired trigger sequencer for the Workshop Computer. Control pattern map and fill with knobs,\nor per-lane density when switch Z is **up**. Internal clock with swing, or follow external clock on **PulseIn1**.\nConfigure and save settings via the Web MIDI SysEx editor (Chrome recommended).",
                "GPL-3.0-or-later",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/82_Computer_Grids",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Map Modulation", "Adds to pattern map X, Y, or XY blend (per config). Held off after Z middle↔up flip until X or Y is moved past takeover deadband" },
                    },
                    {
                        { "any", "", "", "Fill Modulation", "Global fill offset in Z middle; all three lane density offsets in Z up (after knob takeover)" },
                    },
                    {
                        { "any", "", "", "External Clock", "When patched, internal clock is bypassed and swing is ignored; follows external pulse edges" },
                    },
                    {
                        { "any", "", "", "Pattern Reset", "Rising edge resets pattern phase to the start of the bar" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "Trigger Lane 3", "Digital pulse-style trigger for lane 3" },
                    },
                    {
                        { "any", "", "", "Aux Output", "Configurable accent, clock, or lane 3 mirror output" },
                    },
                    {
                        { "any", "", "", "Trigger Lane 1", "Grids-style trigger output for drum/voice lane 1" },
                    },
                    {
                        { "any", "", "", "Trigger Lane 2", "Grids-style trigger output for drum/voice lane 2" },
                    },
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
            "cosmik_c1zzl3",
            {
                "cosmik_c1zzl3",
                "Cosmik C1Zzl3",
                "Workshop Computer Card",
                "Music Thing Modular",
                "",
                "Stable phase-distortion synthesiser and Turing machine firmware with Web MIDI\nenvelope readback, PD, detune, eight waveform families, hosted CZ patch\nimport, USB MIDI device/host operation, and optional Turing MIDI output.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/84_CosmikC1zzl3",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Phase\nDistortion", "Adds phase-distortion amount in synth and Turing modes" },
                    },
                    {
                        { "any", "", "", "Wave Control\nCV", "Adds wave control in synth and Turing modes" },
                    },
                    {
                        { "any", "", "", "Ext Turing\nClock", "External Turing clock in switch-up mode" },
                    },
                    {
                        { "any", "", "", "Envelope\nTrig", "Triggers selected envelope and oscillator sync in synth mode when preset is\nactive" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "Stepped\nTuring CV", "Scaled stepped Turing CV; continues updating in synth mode" },
                    },
                    {
                        { "any", "", "", "Smoothed\nTuring CV", "Smoothed Turing CV; continues updating in synth mode" },
                    },
                    {
                        { "any", "", "", "Main Turing\nPulse", "Main Turing pulse; continues updating in synth mode" },
                    },
                    {
                        { "any", "", "", "Alternate\nTuring Pulse", "Alternate Turing bit pulse; continues updating in synth mode" },
                    },
                },
                {
                    {
                        { "any", "", "", "Pitch", "Oscillator pitch in synth mode" },
                    },
                    {
                        { "any", "", "", "Phase\nDistortion", "Phase-distortion amount" },
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Z up: MAIN: Mutation; X: Sequence Length; Y: Clock Speed." },
                    { "Middle", "Z middle: MAIN: Pitch; X: Phase Distortion; Y: Waveform Morph." },
                    { "Down", "Z down, hold: MAIN: Detune; X: Ring Modulation; Y: Noise/Grit." }
                },
                true
            }
        },
        {
            "tesserae",
            {
                "tesserae",
                "Tesserae",
                "Workshop Computer Card",
                "MTM Community",
                "",
                "Tesserae — Variable-voice (2-8) arpeggiated chord generator with 5 patterns, 10 scales, tap tempo, CV/audio transpose inputs, and dual CV + audio pitch outputs. Inspired by Laurie Spiegel's Music Mouse and Patchwork.",
                "",
                "",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
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
            "fr330hfr33",
            {
                "fr330hfr33",
                "Fr330Hfr33",
                "Workshop Computer Card",
                "Music Thing Modular",
                "",
                "Performance-focused acid voice with diode filtering, distortion, MIDI, and a persistent sequencer",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/87_fr330hfr33",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "Pitch\nCV", "1V/oct pitch input in switch-up CV/gate mode." },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Gate\nIn", "External gate input in switch-up CV/gate mode." },
                    },
                    {
                        { "any", "", "", "Clock /\nSlide", "External sequencer clock in switch-middle mode. In switch-up CV/MIDI\nmode, holding this input high manually enables glide for pitch changes." },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "Pitch\nOut", "Calibrated 1V/oct output following the currently played pitch." },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Gate\nOut", "Gate output following external, MIDI, or sequencer notes." },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Cutoff", "Sets the diode-style ladder filter resting cutoff with a curved response:\ncounter-clockwise is dark, the middle is broad and musical, and\nclockwise is bright." },
                    },
                    {
                        { "any", "", "", "Resonance", "Uses a curved response: the lower range adds body, the upper range\nbecomes nasal and acid-like, and the final part of travel reaches firm\nself-oscillation. Broad diode-pair saturation keeps the oscillation\nround, while X adds only a small amount of extra filter-envelope sweep." },
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "External CV/gate or USB MIDI mode. CV In 1 controls pitch, Pulse In 1 controls gate, and holding Pulse In 2 high enables manual glide; overlapping MIDI notes glide automatically without retriggering the envelope." },
                    { "Middle", "History-aware generative sequencer that avoids recent repeats, favors nearby scale motion, and occasionally makes wider or octave jumps. Pulse In 2 supplies an exact external clock; otherwise the configured internal tempo or optional MIDI clock can use adjustable swing." },
                    { "Down", "Momentary battery pull. The gate falls, the current VCA level is held briefly, then pitch, filter, resonance, and both audio outputs collapse with the virtual supply." }
                },
                true
            }
        },
        {
            "pantograph",
            {
                "pantograph",
                "Pantograph",
                "Workshop Computer Card",
                "Kenny Shen",
                "",
                "Pantograph lets you trace a performance on the X and Y knobs (e.g. oscillator pitch + filter\ncutoff), record it, then loop it back out the two CV outputs at variable speeds.\n\nSwitch UP auditions live; hold DOWN to record (monitored, so you hear it as you play);\nrelease to MIDDLE and it loops exactly that length. The Main\nknob sets playback speed: centre = freeze, left = reverse, right = faster.\n\nPatch a gate into PULSE IN 1 and MIDDLE becomes a triggered envelope; unpatched it free-loops.\nPULSE OUT 1 fires a trigger at the end of a recording.\nPULSE OUT 2 runs a gate from the shape of the recording.",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/90_Pantograph",
                {
                    {
                        { "any", "", "", "(unused)", "Not used" },
                    },
                    {
                        { "any", "", "", "(unused)", "Not used" },
                    },
                    {
                        { "any", "", "", "X Mod", "Live CV summed onto the CV1 output" },
                    },
                    {
                        { "any", "", "", "Y Mod", "Live CV summed onto the CV2 output" },
                    },
                    {
                        { "any", "", "", "Trigger", "When patched, Play (Middle) becomes a one-shot: each rising edge retriggers. Unpatched = loop." },
                    },
                    {
                        { "any", "", "", "(unused)", "Not used" },
                    },
                },
                {
                    {
                        { "any", "", "", "(unused)", "Not used" },
                    },
                    {
                        { "any", "", "", "(unused)", "Not used" },
                    },
                    {
                        { "any", "", "", "Trace X CV", "Trace X playback" },
                    },
                    {
                        { "any", "", "", "Trace Y CV", "Trace Y playback" },
                    },
                    {
                        { "any", "", "", "End of cycle", "Trigger on every trace wraparound" },
                    },
                    {
                        { "any", "", "", "Contour Gate", "Gates high on trace X's recorded shape" },
                    },
                },
                {
                    {
                        { "any", "", "", "Speed", "Playback speed, bipolar — centre = freeze, left = reverse, right = faster" },
                    },
                    {
                        { "any", "", "", "Trace X", "Performance value in Live/Record; playback depth (attenuation) in Play" },
                    },
                    {
                        { "any", "", "", "Trace Y", "Performance value in Live/Record; playback depth (attenuation) in Play" },
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
            "chorgan",
            {
                "chorgan",
                "Chorgan",
                "Workshop Computer Card",
                "Andy Jenkinson (uglifruit)",
                "",
                "Six-voice morphing chord synthesizer with built-in chord sequencer.\nKnob X and CV In 1 set root pitch; Knob Y selects interval above root (0–12 semitones).\nMain knob morphs all six voices through sine, triangle, saw, and narrow pulse; CV In 2 offsets timbre.\nAudio In 1 controls slew speed (0V=instant, +5V=approx 1 min glide). Audio In 2 shifts chord voicing through octave inversions (bipolar, +/-6 steps).\nTap Switch Down to cycle chord extension presets; hold one second to store a chord in the sequencer.\nPulse In 1 advances preset; Pulse In 2 recalls the next stored chord on rising edges.\nBoot with Switch Down held for slew mode (portamento chord changes instead of detune).",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/91_chorgan",
                {
                    {
                        { "any", "", "", "Slew Speed CV", "0V=instant, +5V=approx 1 min glide; negative shortens slew in slew mode" },
                    },
                    {
                        { "any", "", "", "Chord Inversion CV", "Bipolar +/-6 octave inversion steps; shifts lowest/highest voice up or down" },
                    },
                    {
                        { "any", "", "", "Root Pitch CV", "Root pitch 1V/oct (0V = C4), summed with Knob X" },
                    },
                    {
                        { "any", "", "", "Timbre Offset CV", "Bipolar offset to Main knob timbre position" },
                    },
                    {
                        { "any", "", "", "Preset Advance", "Rising edge advances chord extension preset" },
                    },
                    {
                        { "any", "", "", "Chord Recall Clock", "Rising edge recalls next stored chord in sequencer" },
                    },
                },
                {
                    {
                        { "any", "", "", "Six-Voice Mix", "Six-voice chord mix output" },
                    },
                    {
                        { "any", "", "", "Phase-Offset Mix", "Same voices with per-voice phase offsets for stereo width" },
                    },
                    {
                        { "any", "", "", "Voiced Pitch CV", "Root pitch plus voiced interval (1V/oct)" },
                    },
                    {
                        { "any", "", "", "Chord Event Ramp", "Unipolar downward ramp (+5V to 0V) on each chord event" },
                    },
                    {
                        { "any", "", "", "Sub-Octave Square", "Square wave one octave below root" },
                    },
                    {
                        { "any", "", "", "Chord Event PWM", "PWM envelope resetting on each chord event" },
                    },
                },
                {
                    {
                        { "middle", "", "", "Timbre", "Morphs waveform of all six voices (pulse → sine → saw → sine → pulse)" },
                        { "up", "", "", "Timbre", "Same timbre morph with detune zone selected by switch/knob quadrant" },
                    },
                    {
                        { "middle", "", "", "Root Pitch", "Root pitch from C3 to C6, summed with CV In 1" },
                        { "up", "", "", "Root Pitch", "Root pitch control" },
                    },
                    {
                        { "middle", "", "", "Interval", "Semitone interval between root and voice 2 (0–12)" },
                        { "up", "", "", "Interval", "Semitone interval control" },
                    },
                },
                {
                    "Z",
                    { "Detune Zone B", "Higher detune level selected by knob quadrant" },
                    { "Detune Zone A", "Lower detune level selected by knob quadrant" },
                    { "Preset Cycle / Store", "Short tap cycles preset; one-second hold stores current chord to sequencer" }
                },
                true
            }
        },
        {
            "turing_matrix",
            {
                "turing_matrix",
                "Turing Matrix",
                "Workshop Computer Card",
                "Music Thing Modular",
                "",
                "Turing Machine sequencer with a switchable mixer layer inspired by the Music Thing Modular Turing Machine and Vactrol Mix combination",
                "",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/93_Turing_Matrix",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV Input 1", "Divide/multiply modulation in middle mode; mix input 1 in up mode" },
                    },
                    {
                        { "any", "", "", "CV Input 2", "Pitch offset in middle mode; mix input 2 in up mode" },
                    },
                    {
                        { "any", "", "", "Ext Clock 1", "Replaces tap tempo and drives the main Turing clock" },
                    },
                    {
                        { "any", "", "", "Ext Clock 2", "Independently clocks channel 2 when patched" },
                    },
                },
                {
                    {
                        { "any", "", "", "Audio 1 Output", "Audio output 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Output", "Audio output 2" },
                    },
                    {
                        { "any", "", "", "Chan 1 Quant\nCV", "Quantized pitch CV in middle mode; crossfaded CV output 1 in up mode" },
                    },
                    {
                        { "any", "", "", "Chan 2 Quant\nCV", "Quantized pitch CV in middle mode; crossfaded CV output 2 in up mode" },
                    },
                    {
                        { "any", "", "", "Chan 1 Pulse", "Clock or Turing-bit pulse behavior depending on pulse mode configuration" },
                    },
                    {
                        { "any", "", "", "Chan 2 Pulse", "Clock or Turing-bit pulse behavior depending on pulse mode configuration" },
                    },
                },
                {
                    {
                        { "any", "", "", "Random / Write\nMixer Lag", "Drives the main Turing machine write behavior in middle mode and sets mixer lag in up mode" },
                    },
                    {
                        { "any", "", "", "Loop Length\nMixer Depth 1", "Sets channel 1 sequence length in middle mode and mixer depth 1 in up mode" },
                    },
                    {
                    },
                },
                {
                    "Z",
                    { "Up", "Z up: MAIN: Mixer Lag; X: Mix Depth 1; Y: Mix Depth 2. Mixer mode active." },
                    { "Middle", "Z middle: MAIN: Randomness / Write; X: Loop Length; Y: Div / Mult. Turing mode active." },
                    { "Down", "Z down, momentary: MAIN: Tap Tempo." }
                },
                true
            }
        },
        {
            "offair2",
            {
                "offair2",
                "OffAir",
                "Workshop Computer Card",
                "Andy Jenkinson (uglifruit)",
                "",
                "Tune across a virtual shortwave band with the Main knob. Two Stations (live audio\ninputs, or baked recordings in baked-in audio mode) plus three interference signals\nare scattered across the dial and re-randomised on each band change. Approaching a\nStation you hear a sliding heterodyne whistle, the audio pulls into tune and the\nstatic ducks away; off-tune it pitch-shifts (SW/LW) or distorts (AM). Knob X sets IF\nbandwidth/brightness, Knob Y the noise level. Tap Switch Down to cycle AM/SW/LW.\nHold Switch Up for dead-air (baked-in mode) or a Pulse In 1-keyed morse Station 2\n(audio-input mode). Pulse In 2 fires curated \"Insta-ference\" one-shots.",
                "CC BY-SA 4.0",
                "https://github.com/uglifruit/Workshop_Computer",
                {
                    {
                        { "any", "", "", "Station 1 In", "Station 1 source (live audio, in audio-input mode)" },
                    },
                    {
                        { "any", "", "", "Station 2 In", "Station 2 source (live audio, in audio-input mode)" },
                    },
                    {
                        { "any", "", "", "Tuner", "Tuning offset added to the Main knob (1:1, ±5V ≈ ±half the dial)" },
                    },
                    {
                        { "any", "", "", "Noise", "Adds to the Knob Y noise level (voltage-controlled static)" },
                    },
                    {
                        { "any", "", "", "Shuffle Signals", "Rising edge re-randomises the Station/interference layout. In audio-input mode with Switch Up it is the Morse In key instead." },
                    },
                    {
                        { "any", "", "", "Insta-ference", "Rising edge fires a one-shot event from the curated Insta-ference bank" },
                    },
                },
                {
                    {
                        { "any", "", "", "Output", "Full mix — tuned audio, whistles, static, Insta-ference" },
                    },
                    {
                        { "any", "", "", "Just Noise", "Noise / static only" },
                    },
                    {
                        { "any", "", "", "Signal Strength", "Envelope that rises as you tune onto a Station" },
                    },
                    {
                        { "any", "", "", "Station 1 CV Offset", "Station 1's offset from the Main knob — slew into CV In 1 to tune onto Station 1" },
                    },
                    {
                        { "any", "", "", "Station 1 Tuned Gate", "HIGH while tuned to Station 1" },
                    },
                    {
                        { "any", "", "", "Station 2 Tuned Gate", "HIGH while tuned to Station 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Tuning", "Scans across the dial (sums with CV In 1)" },
                        { "down", "momentary", "", "Cycle Band", "Tap to cycle AM → SW → LW, re-randomising the layout each time" },
                        { "up", "hold", "", "Dead-air / Morse", "Baked-in mode — mutes both Stations (static remains). Audio-input mode — Station 2 becomes a ~600Hz morse tone keyed by Pulse In 1." },
                    },
                    {
                        { "any", "", "", "Brightness", "IF bandwidth — capture width + audio brightness (CCW narrow/muffled, CW wide/bright)" },
                    },
                    {
                        { "any", "", "", "Noise Level", "Static floor (silent fully CCW); slowly swells and swishes, heaviest on LW" },
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
            "alloy",
            {
                "alloy",
                "Alloy",
                "Workshop Computer Card",
                "Eric Gao",
                "",
                "Two audio inputs fused through 15 crossfaded cross-modulation algorithms -\nring mods, wavefolder, comparators, bitcrusher, frequency shifter, delay,\nbinaural doppler, and a 20-band vocoder - with a clocked Turing machine\ndriving pitch, CV, and gates underneath.",
                "MIT",
                "https://github.com/Ericxgao/Workshop_Computer/tree/eric/warps-port/releases/97_alloy",
                {
                    {
                        { "any", "", "", "Carrier", "External carrier input" },
                    },
                    {
                        { "any", "", "", "Modulator", "Modulator input" },
                    },
                    {
                        { "any", "", "", "Algorithm CV", "Bipolar modulation added to the held Main value" },
                    },
                    {
                        { "any", "", "", "Timbre CV", "Bipolar modulation added to the held X value" },
                    },
                    {
                        { "any", "", "", "Clock", "External Turing clock; overrides the internal clock" },
                    },
                    {
                        { "any", "", "", "Reset", "Turing reset on rising edge" },
                    },
                },
                {
                    {
                        { "any", "", "", "Main", "Cross-modulated output" },
                    },
                    {
                        { "any", "", "", "Aux / Right", "Dry sum; doppler right channel, shifter opposite sideband, or bitcrushed dry mix in those zones" },
                    },
                    {
                        { "any", "", "", "Turing Pitch", "C-minor-pentatonic quantized pitch" },
                    },
                    {
                        { "any", "", "", "Turing CV", "Bipolar stepped modulation" },
                    },
                    {
                        { "any", "", "", "Turing Gate A", "First register bit gate" },
                    },
                    {
                        { "any", "", "", "Turing Gate B", "Length-aware second register bit gate" },
                    },
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
                "Blackbird",
                "Workshop Computer Card",
                "Dune Desormeaux",
                "https://dessertplanet.github.io/web-druid/",
                "Blackbird runs crow-compatible Lua scripts over USB serial and can also store scripts on card flash.\nPanel control behavior is script-defined: Lua code reads `bb.knob.*`, `bb.switch.position`, and inputs,\nthen decides how outputs respond. LEDs always reflect positive output voltages on the six outputs.",
                "GPLv3 or later",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/41_blackbird",
                {
                    {
                        { "any", "", "", "bb.audioin[1]", "Audio input query source for Lua scripts" },
                    },
                    {
                        { "any", "", "", "bb.audioin[2]", "Audio input query source for Lua scripts" },
                    },
                    {
                        { "any", "", "", "input[1]", "Script-readable CV input mapped to crow input[1]" },
                    },
                    {
                        { "any", "", "", "input[2]", "Script-readable CV input mapped to crow input[2]" },
                    },
                    {
                        { "any", "", "", "bb.pulsein[1]", "Digital input with change/clock detection for Lua callbacks" },
                    },
                    {
                        { "any", "", "", "bb.pulsein[2]", "Digital input with change/clock detection for Lua callbacks" },
                    },
                },
                {
                    {
                        { "any", "", "", "output[3]", "Uncalibrated CV/audio output under script control" },
                    },
                    {
                        { "any", "", "", "output[4]", "Uncalibrated CV/audio output under script control" },
                    },
                    {
                        { "any", "", "", "output[1]", "Calibrated CV output under script control" },
                    },
                    {
                        { "any", "", "", "output[2]", "Calibrated CV output under script control" },
                    },
                    {
                        { "any", "", "", "bb.pulseout[1]", "Digital pulse output for Lua-triggered actions" },
                    },
                    {
                        { "any", "", "", "bb.pulseout[2]", "Digital pulse output for Lua-triggered actions" },
                    },
                },
                {
                    {
                        { "any", "", "", "bb.knob.main", "Normalized analog knob value exposed to Lua; behavior depends on loaded script" },
                    },
                    {
                        { "any", "", "", "bb.knob.x", "Normalized analog knob value exposed to Lua; behavior depends on loaded script" },
                    },
                    {
                        { "any", "", "", "bb.knob.y", "Normalized analog knob value exposed to Lua; behavior depends on loaded script" },
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
            "acid",
            {
                "acid",
                "Acid",
                "Workshop Computer Card",
                "Samuel Smith",
                "",
                "A deliberately slightly fiddly 16-step sequencer capturing some of the obtuse workflow of the\noriginal 303. Walk a cursor through the pattern, setting pitch and step type (rest / normal /\naccent / slide) per step. Pitch and step-type sequences can be independently phase-shifted and\nreset during playback.",
                "MIT",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/303_acid",
                {
                    {
                        { "any", "", "", "Pitch Phase Offset", "Phase-shifts the pitch sequence" },
                    },
                    {
                        { "any", "", "", "Step-type Phase Offset", "Phase-shifts the step-type sequence" },
                    },
                    {
                        { "any", "", "", "Transpose", "Global transpose across the pattern" },
                    },
                    {
                        { "any", "", "", "Portamento Time", "Increase glide time" },
                    },
                    {
                        { "any", "", "", "External Clock", "One step per rising edge, overrides the internal tempo" },
                    },
                    {
                        { "any", "", "", "Reset", "Quantised reset based on selected reset-mode" },
                    },
                },
                {
                    {
                        { "any", "", "", "Sawtooth Voice", "Internal sawtooth oscillator tracking the sequenced pitch (no envelope)" },
                    },
                    {
                        { "any", "", "", "Gate", "~2ms trigger for normal/accent steps, held high on slide steps" },
                    },
                    {
                        { "any", "", "", "Pitch", "Calibrated 1V/oct pitch output, quantised to semitones, with portamento on slide" },
                    },
                    {
                        { "any", "", "", "Accent CV", "~5V on accented steps, 0V otherwise" },
                    },
                    {
                        { "any", "", "", "Clock", "Internal clock pulses, or the external clock when Pulse In 1 is connected" },
                    },
                    {
                        { "any", "", "", "Accent Trigger", "~2ms trigger on accented steps" },
                    },
                },
                {
                    {
                        { "up", "", "", "Length", "Sequence length, 1-16 steps" },
                        { "middle", "", "", "Note", "Semitone within the octave (0-11) for the step under the cursor" },
                        { "down", "", "", "Reset Mode", "Held ≥1s, selects the behaviour applied by a Pulse In 2 reset" },
                    },
                    {
                        { "up", "", "", "Swing", "Swing amount from 0% to ~33%" },
                        { "middle", "", "", "Octave", "Octave offset (-2..+2) for the step under the cursor" },
                        { "down", "", "", "Randomise Pitch", "Held ≥1s, a large turn randomises the pitch sequence" },
                    },
                    {
                        { "up", "", "", "Tempo", "Internal tempo" },
                        { "middle", "", "", "Step Mode", "Step type for the step under the cursor - rest / normal / accent / slide" },
                        { "down", "", "", "Randomise Step Type", "Held ≥1s, a large turn randomises the step-type sequence" },
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
            "sense_of_space",
            {
                "sense_of_space",
                "433 Sense of Space",
                "Workshop Computer Card",
                "Music Thing Modular / AI-assisted",
                "",
                "433 Sense of Space is supplied as 2 MB and 16 MB builds. The 2 MB build uses a\ncompact 10 kHz stereo 8-bit ambience asset, while the 16 MB build uses a cleaner\n24 kHz stereo 16-bit ambience asset.\nSwitch Up stops and arms the performance, Switch Middle starts three 91 second\nloops sourced from 1:30-3:01 of the BBC recording, and the left column LEDs count\ndown from three loops to zero. Switch Down or Pulse In 1 triggers a short\nchair/stool creak one-shot representing the musician shifting in their seat.\nPulse In 2 restarts the full performance from the beginning.",
                "Mixed - source code/reverb components follow their original project licences; embedded BBC Sound Effects audio is subject to BBC Sound Effects Archive licensing",
                "https://github.com/TomWhitwell/Workshop_Computer/tree/main/releases/433_sense_of_space",
                {
                    {
                        { "any", "", "", "Audio 1 Input", "Audio input 1" },
                    },
                    {
                        { "any", "", "", "Audio 2 Input", "Audio input 2" },
                    },
                    {
                        { "any", "", "", "CV 1 Input", "CV input 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Input", "CV input 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Input", "Pulse input 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Input", "Pulse input 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Left", "Left stereo output" },
                    },
                    {
                        { "any", "", "", "Right", "Right stereo output" },
                    },
                    {
                        { "any", "", "", "CV 1 Output", "CV output 1" },
                    },
                    {
                        { "any", "", "", "CV 2 Output", "CV output 2" },
                    },
                    {
                        { "any", "", "", "Pulse 1 Output", "Pulse output 1" },
                    },
                    {
                        { "any", "", "", "Pulse 2 Output", "Pulse output 2" },
                    },
                },
                {
                    {
                        { "any", "", "", "Restlessness", "Adds occasional quieter automatic chair/stool creaks" },
                        { "down", "momentary", "", "Seat Creak", "Trigger a short chair/stool creak one-shot" },
                        { "up", "", "", "Stop / Arm", "Stop playback, reset to the beginning, and light all three countdown LEDs" },
                        { "middle", "", "", "Start", "Start a three-loop 4'33\" performance from the beginning" },
                    },
                    {
                        { "any", "", "", "Space", "Reverb character, from small room to a restrained cathedral-like hall" },
                    },
                    {
                        { "any", "", "", "Amount", "Dry/wet reverb amount" },
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
