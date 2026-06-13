const DEVICE_CONTROLS = {
    "meta": {
        "description": "MIDI CC Mapping for Flux MultiFX",
        "version": "1.1"
    },
    "controls": [
        {
            "name": "Synth Mode",
            "cc": 86,
            "type": "enum",
            "group": "Global",
            "target": "synth_mode",
            "options": [
                { "val": 0, "label": "Audio Input" },
                { "val": 1, "label": "Wavetable" },
                { "val": 2, "label": "VA Bass" },
                { "val": 3, "label": "Strings (Karplus)" },
                { "val": 4, "label": "Modal Resonator" },
                { "val": 5, "label": "White Noise" },
                { "val": 6, "label": "Sampler (One Shot)" },
                { "val": 7, "label": "Sampler (Loop)" },
                { "val": 8, "label": "Sampler (Player/Drone)" },
                { "val": 9, "label": "Drum Kit" },
                { "val": 10, "label": "Granular Cloud" },
                { "val": 11, "label": "Piano" }
            ]
        },
        {
            "name": "Pitch",
            "cc": 14,
            "type": "range",
            "min": 0,
            "max": 127,
            "group": "Synth",
            "target": "synth_pitch"
        },
        {
            "name": "Timbre",
            "cc": 15,
            "type": "range",
            "min": 0,
            "max": 127,
            "group": "Synth",
            "target": "synth_timbre"
        },
        {
            "name": "Envelope",
            "cc": 16,
            "type": "range",
            "min": 0,
            "max": 127,
            "group": "Synth",
            "target": "synth_env"
        },
        {
            "name": "Filter",
            "cc": 17,
            "type": "range",
            "min": 0,
            "max": 127,
            "group": "Synth",
            "target": "synth_filter"
        },
        {
            "name": "Volume",
            "cc": 18,
            "type": "range",
            "min": 0,
            "max": 127,
            "group": "Synth",
            "target": "synth_vol"
        },
        {
            "name": "Effect Mode",
            "cc": 81,
            "type": "enum",
            "group": "Effects",
            "target": "fx_mode",
            "options": [
                {
                    "val": 0,
                    "label": "Select Source"
                },
                {
                    "val": 1,
                    "label": "Compressor"
                },
                {
                    "val": 2,
                    "label": "Equalizer"
                },
                {
                    "val": 3,
                    "label": "Guitar Amp"
                },
                {
                    "val": 4,
                    "label": "Velvet Amp"
                },
                {
                    "val": 5,
                    "label": "Bitcrusher"
                },
                {
                    "val": 6,
                    "label": "Tremolo"
                },
                {
                    "val": 7,
                    "label": "Sine Chorus"
                },
                {
                    "val": 8,
                    "label": "Vibrato Chorus"
                },
                {
                    "val": 9,
                    "label": "Pitch Shift"
                },
                {
                    "val": 10,
                    "label": "Frequency Shifter"
                },
                {
                    "val": 11,
                    "label": "Digital Delay"
                },
                {
                    "val": 12,
                    "label": "Tape Delay"
                },
                {
                    "val": 13,
                    "label": "Ping Pong"
                },
                {
                    "val": 14,
                    "label": "Tape Loop"
                },
                {
                    "val": 15,
                    "label": "Plate Reverb"
                },
                {
                    "val": 16,
                    "label": "Spring Reverb"
                },
                {
                    "val": 17,
                    "label": "Freeverb"
                },
                {
                    "val": 18,
                    "label": "Shimmer Reverb"
                },
                {
                    "val": 19,
                    "label": "Cathedral Reverb"
                },
                {
                    "val": 20,
                    "label": "Granular Clouds"
                },
                {
                    "val": 21,
                    "label": "Micro Looper"
                },
                {
                    "val": 22,
                    "label": "Tape Degradation"
                },
                {
                    "val": 23,
                    "label": "Lossy Audio"
                },
                {
                    "val": 24,
                    "label": "Space Resonator"
                },
                {
                    "val": 25,
                    "label": "Basic Reverb"
                },
                {
                    "val": 26,
                    "label": "Allpass Reverb"
                },
                {
                    "val": 27,
                    "label": "Hadamard Reverb"
                },
                {
                    "val": 28,
                    "label": "Oil-Can Echo"
                },
                {
                    "val": 29,
                    "label": "Resonator"
                },
                {
                    "val": 30,
                    "label": "Wind"
                },
                {
                    "val": 31,
                    "label": "Chatter"
                }
            ]
        },
        {
            "name": "Param 1 (Main)",
            "cc": 20,
            "type": "range",
            "min": 0,
            "max": 127,
            "group": "Effects",
            "target": "fx_p1"
        },
        {
            "name": "Param 2 (X)",
            "cc": 21,
            "type": "range",
            "min": 0,
            "max": 127,
            "group": "Effects",
            "target": "fx_p2"
        },
        {
            "name": "Param 3 (Y)",
            "cc": 22,
            "type": "range",
            "min": 0,
            "max": 127,
            "group": "Effects",
            "target": "fx_p3"
        }
    ]
};