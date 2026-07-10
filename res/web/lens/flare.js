// web/flare.js — Eurorack visual patcher, ground-up rewrite
// Fully implements:
//   - HP Grid snapping (free placement anywhere on the rails)
//   - Slide-to-push rail collision physics (zero overlaps, sliding module pushing)
//   - Separate "WS IN" and "WS OUT" 6 HP hardware modules
//   - Right-click Context Menus:
//     - Right-click empty rail: categorized Add Module menu (places module exactly at clicked snapped HP position)
//     - Right-click module: Action menu (Delete, Move Row Up, Move Row Down)
//   - Exposes detents, cents fine-tune, CV inputs, and outputs
//   - Dynamic rail width calculation to trigger horizontal scroll and prevent overflow
//   - Category-based DOM tags for visual variety and panel styles
//   - Stacked IN and OUT port rows (span full width, centering 1-2 jacks, 2-column grid for 3+ jacks)
//   - 2-screw panels for slim modules (HP < 6)
//   - Always-visible knob values below parameter labels (no more hover-swapping)
//   - Centered vertically distributed jacks for empty/knobless modules (.mod-knobless)
//   - Clean faceplate titles (.module-title) centered below top screws with padding to prevent overflow/overlap
//   - Visual Patcher Macros (compiled from low-level nodes):
//     - `multi-div` (Multi Clock Divider): 4 output divisions simultaneously (/2, /4, /8, /16)
//     - `sub-osc` (Sub-Oscillator VCO): Saw wave with main, sub-1 (-12), and sub-2 (-24) octave outputs
//     - `quad-vca` (Quad VCA): 4 independent VCAs in a single 12 HP panel
//     - `lfo-delay` (Delay LFO): LFO with built-in triggerable fade-in envelope
//     - `score-player` (Score Player): Plays Loupe melody patterns from a text score input (generates notes + gate)
//     - `step-seq` (4-Step Sequencer): 4-step CV step sequencer using a hold-based counter and index lens
//   - Discrete/detented snap values for Mixer, Divider, Router, WT, Euclid, etc.
//   - Added missing modules from actual language (DX Voice, Wavetable, Shaper LUT, Random, Chance, Walk, Env Follower, Add, Mul)
//   - Custom generator for 4-channel Mixer (mix) using VCA scaling

"use strict";

const HP = 15; // 1 Eurorack HP = 15px

// Lens language expressions for hardware IO ports
const LENS_PORTS = {
  // Output ports from WS IN (source points in visual patcher)
  'knob-main': '(knob :main)',
  'knob-x': '(knob :x)',
  'knob-y': '(knob :y)',
  'switch-z': '(switch :z)',
  'audio-in-1': '(audio-in :1)',
  'audio-in-2': '(audio-in :2)',
  'cv-in-1': '(cv-in :1)',
  'cv-in-2': '(cv-in :2)',
  'pulse-in-1': '(pulse-in :1)',
  'pulse-in-2': '(pulse-in :2)',
  // Input ports to WS OUT (sink points in visual patcher)
  'audio-out-1': '(audio-out :1)',
  'audio-out-2': '(audio-out :2)',
  'cv-out-1': '(cv-out :1)',
  'cv-out-2': '(cv-out :2)',
  'pulse-out-1': '(pulse-out :1)',
  'pulse-out-2': '(pulse-out :2)',
};

// Stereo macro modules expose outL/outR; legacy layouts used fromPort 'out'.
const STEREO_MACRO_TYPES = new Set(['delay', 'reverb', 'chorus', 'flanger', 'stereo-mixer']);

function resolveMacroOutputPort(modType, port) {
  if (port === 'out' && STEREO_MACRO_TYPES.has(modType)) {
    const outs = MODULE_DEFS[modType]?.outputs;
    if (outs && outs.length) return outs[0].id;
  }
  return port;
}

function normalizeFlareLayout(layoutData) {
  layoutData.flareMidiChannel = layoutData.flareMidiChannel || 16;
  if (layoutData.rows) {
    for (let i = 0; i < layoutData.rows.length; i++) {
      for (const m of layoutData.rows[i]) {
        if (m.params && m.params.__midi_cc) {
          for (const [k, v] of Object.entries(m.params.__midi_cc)) {
            if (typeof v === 'number') {
              m.params.__midi_cc[k] = { cc: v, ch: 1 };
            }
          }
        }
      }
    }
  }
  for (let i = 0; i < layoutData.rows.length; i++) {
    for (const m of layoutData.rows[i]) {
      if (m.type === 'workshop-computer') m.type = 'computer';
      if (m.type === 'tri') m.type = 'triangle';
      if (m.type === 'sqr') m.type = 'square';
      if (m.type === 'sub') m.type = 'sub-osc';
      if (m.type === 'svf') m.type = 'vcf';
      if (m.type === 'env-follow') m.type = 'envfollow';
      if (m.type === 'folder') m.type = 'wavefold';
      if (m.type === 'lut') m.type = 'shape';
      if (m.type === 'mixer') m.type = 'mix';
      if (m.type === 'ar') m.type = 'envelope';

      if (m.id.startsWith('workshopcomputer')) {
        const oldId = m.id;
        const newId = m.id.replace('workshopcomputer', 'computer');
        m.id = newId;
        if (layoutData.cables) {
          for (const c of layoutData.cables) {
            if (c.fromId === oldId) c.fromId = newId;
            if (c.toId === oldId) c.toId = newId;
          }
        }
        for (const r of layoutData.rows) {
          for (const otherM of r) {
            if (otherM.params) {
              for (const pKey of Object.keys(otherM.params)) {
                if (typeof otherM.params[pKey] === 'string') {
                  otherM.params[pKey] = otherM.params[pKey].replaceAll(oldId, newId);
                }
              }
            }
          }
        }
      }
    }
  }
  if (layoutData.cables) {
    const allMods = layoutData.rows.flat();
    for (const c of layoutData.cables) {
      const fromMod = allMods.find(m => m.id === c.fromId);
      if (fromMod) {
        c.fromPort = resolveMacroOutputPort(fromMod.type, c.fromPort);
      }
      if (fromMod && fromMod.type === 'constant' && c.fromPort === 'out') {
        c.fromPort = 'out1';
      }
    }
  }
}

const KNOB_PX = { huge: 74, large: 44, medium: 30, small: 24 };
const KNOB_ASSET = { huge: 'largeKnob', large: 'largeKnob', medium: 'mediumKnob', small: 'smallKnob' };

// Module definitions with customized HP width, knobs, inputs, outputs, and knob layouts
const MODULE_DEFS = {
  // ── Hardware IO (Separate IN/OUT, 6 HP each) ──────────────────────────
  'ws-in': {
    title: 'Computer In', hp: 6, category: 'io',
    deletable: false, isHW: true,
    knobs: [],
    inputs: [],
    outputs: [
      { id: 'knob-main', label: 'MAIN' },
      { id: 'knob-x', label: 'KNOB X' },
      { id: 'knob-y', label: 'KNOB Y' },
      { id: 'switch-z', label: 'SW Z' },
      { id: 'spacer-1', label: '' },
      { id: 'spacer-2', label: '' },
      { id: 'audio-in-1', label: 'AUD 1' },
      { id: 'audio-in-2', label: 'AUD 2' },
      { id: 'cv-in-1', label: 'CV 1' },
      { id: 'cv-in-2', label: 'CV 2' },
      { id: 'pulse-in-1', label: 'PLS 1' },
      { id: 'pulse-in-2', label: 'PLS 2' },
    ],
  },
  'ws-out': {
    title: 'Computer Out', hp: 6, category: 'io',
    deletable: false, isHW: true,
    knobs: [],
    inputs: [
      { id: 'audio-out-1', label: 'AUD 1' },
      { id: 'audio-out-2', label: 'AUD 2' },
      { id: 'cv-out-1', label: 'CV 1' },
      { id: 'cv-out-2', label: 'CV 2' },
      { id: 'pulse-out-1', label: 'PLS 1' },
      { id: 'pulse-out-2', label: 'PLS 2' },
    ],
    outputs: [],
  },

  // ── Oscillators ────────────────────────────────────────────────────────
  sine: {
    title: 'VCO (Sine)', hp: 6, category: 'oscillators', knobLayout: 'triangle',
    knobs: [
      { param: 'pitch', label: 'PITCH', size: 'large', def: 1935 },
      { param: 'cents', label: 'FINE', size: 'small', def: 2048 },
      { param: 'depth', label: 'FM DEPTH', size: 'small', def: 0 },
      { param: 'range', label: 'RANGE', size: 'small', def: 0, discrete: ['audio', 'lfo'] }
    ],
    inputs: [
      { id: 'note', label: 'V/OCT' },
      { id: 'fm', label: 'FM' },
      { id: 'pm', label: 'PM' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  triangle: {
    title: 'VCO (Tri)', hp: 6, category: 'oscillators', knobLayout: 'triangle',
    knobs: [
      { param: 'pitch', label: 'PITCH', size: 'large', def: 1935 },
      { param: 'cents', label: 'FINE', size: 'small', def: 2048 },
      { param: 'depth', label: 'FM DEPTH', size: 'small', def: 0 },
      { param: 'range', label: 'RANGE', size: 'small', def: 0, discrete: ['audio', 'lfo'] }
    ],
    inputs: [
      { id: 'note', label: 'V/OCT' },
      { id: 'fm', label: 'FM' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  saw: {
    title: 'VCO (Saw)', hp: 6, category: 'oscillators', knobLayout: 'triangle',
    knobs: [
      { param: 'pitch', label: 'PITCH', size: 'large', def: 1935 },
      { param: 'cents', label: 'FINE', size: 'small', def: 2048 },
      { param: 'depth', label: 'FM DEPTH', size: 'small', def: 0 },
      { param: 'range', label: 'RANGE', size: 'small', def: 0, discrete: ['audio', 'lfo'] }
    ],
    inputs: [
      { id: 'note', label: 'V/OCT' },
      { id: 'fm', label: 'FM' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  square: {
    title: 'VCO (Square)', hp: 8, category: 'oscillators', knobLayout: 'triangle',
    knobs: [
      { param: 'pitch', label: 'PITCH', size: 'large', def: 1935 },
      { param: 'cents', label: 'FINE', size: 'small', def: 2048 },
      { param: 'width', label: 'PW', size: 'small', def: 2048 },
      { param: 'depth', label: 'PWM DEPTH', size: 'small', def: 0 },
      { param: 'range', label: 'RANGE', size: 'small', def: 0, discrete: ['audio', 'lfo'] }
    ],
    inputs: [
      { id: 'note', label: 'V/OCT' },
      { id: 'fm', label: 'FM' },
      { id: 'pwm', label: 'PWM' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  'sub-osc': {
    title: 'Sub-Osc VCO', hp: 6, category: 'voices', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'pitch', label: 'PITCH', size: 'large', def: 1935 },
      { param: 'cents', label: 'FINE', size: 'small', def: 2048 }
    ],
    inputs: [{ id: 'note', label: 'V/OCT' }],
    outputs: [
      { id: 'out', label: 'MAIN' },
      { id: 'sub1', label: 'SUB -1' },
      { id: 'sub2', label: 'SUB -2' }
    ]
  },
  phasor: {
    title: 'Phasor LFO', hp: 4, category: 'oscillators', knobLayout: 'vertical',
    knobs: [
      { param: 'hz', label: 'RATE', size: 'medium', def: 10 }
    ],
    inputs: [
      { id: 'hz', label: 'CV RATE' },
      { id: 'sync', label: 'SYNC' }
    ],
    outputs: [{ id: 'phase', label: 'PHASE' }],
  },
  'lfo-delay': {
    title: 'Delay LFO', hp: 4, category: 'oscillators', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'hz', label: 'RATE', size: 'medium', def: 10 },
      { param: 'fade', label: 'FADE', size: 'small', def: 2048 }
    ],
    inputs: [
      { id: 'trig', label: 'FADE GATE' },
      { id: 'hz', label: 'CV RATE' },
      { id: 'fade', label: 'CV FADE' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  lfo: {
    title: 'LFO', hp: 6, category: 'oscillators', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'rate', label: 'RATE', size: 'medium', def: 2048 },
      { param: 'rateamt', label: 'CV AMT', size: 'small', def: 4095 },
      { param: 'shape', label: 'SHAPE', size: 'small', def: 0, discrete: ['sine', 'tri', 'saw', 'sqr'] },
      { param: 'range', label: 'RANGE', size: 'small', def: 0, discrete: ['bipolar', 'unipolar'] }
    ],
    inputs: [
      { id: 'rate', label: 'CV RATE' },
      { id: 'sync', label: 'SYNC' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  wt: {
    title: 'WT Osc', hp: 6, category: 'oscillators', knobLayout: 'wt',
    knobs: [
      { param: 'pitch', label: 'PITCH', size: 'large', def: 1935 },
      { param: 'cents', label: 'FINE', size: 'small', def: 2048 },
      { param: 'table', label: 'TABLE', size: 'small', def: 256, discrete: [0, 1, 2, 3, 4, 5, 6, 7] },
      { param: 'pos', label: 'MORPH', size: 'small', def: 2048 },
      { param: 'posamt', label: 'MORPH CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'pitch', label: 'V/OCT' },
      { id: 'pos', label: 'MORPH' },
      { id: 'pm', label: 'PM' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  noise: {
    title: 'Noise', hp: 2, category: 'oscillators',
    knobs: [],
    inputs: [],
    outputs: [{ id: 'out', label: 'OUT' }],
  },

  // ── Filters ────────────────────────────────────────────────────────────
  lpf: {
    title: 'LP Filter', hp: 6, category: 'filters', knobLayout: 'vertical',
    knobs: [
      { param: 'cut', label: 'CUTOFF', size: 'large', def: 2048 },
      { param: 'cutamt', label: 'CUT CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'cut', label: 'CUT' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  hpf: {
    title: 'HP Filter', hp: 6, category: 'filters', knobLayout: 'vertical',
    knobs: [
      { param: 'cut', label: 'CUTOFF', size: 'large', def: 2048 },
      { param: 'cutamt', label: 'CUT CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'cut', label: 'CUT' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  vcf: {
    title: 'State Variable VCF', hp: 8, category: 'filters', knobLayout: 'grid',
    knobs: [
      { param: 'cut', label: 'CUTOFF', size: 'medium', def: 2048 },
      { param: 'res', label: 'RES', size: 'medium', def: 1000 },
      { param: 'cutamt', label: 'CUT CV', size: 'small', def: 4095 },
      { param: 'resamt', label: 'RES CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'cut', label: 'CUT' },
      { id: 'res', label: 'RES' }
    ],
    outputs: [
      { id: 'lp', label: 'LP' },
      { id: 'hp', label: 'HP' },
      { id: 'bp', label: 'BP' },
      { id: 'notch', label: 'NOTCH' }
    ],
  },
  lpf2: {
    title: 'LP Filter 2', hp: 6, category: 'filters', knobLayout: 'vertical',
    knobs: [
      { param: 'cut', label: 'CUTOFF', size: 'large', def: 2048 },
      { param: 'res', label: 'RES', size: 'small', def: 1000 },
      { param: 'cutamt', label: 'CUT CV', size: 'small', def: 4095 }
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'cut', label: 'CUT' },
      { id: 'res', label: 'RES' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  hpf2: {
    title: 'HP Filter 2', hp: 6, category: 'filters', knobLayout: 'vertical',
    knobs: [
      { param: 'cut', label: 'CUTOFF', size: 'large', def: 2048 },
      { param: 'res', label: 'RES', size: 'small', def: 1000 },
      { param: 'cutamt', label: 'CUT CV', size: 'small', def: 4095 }
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'cut', label: 'CUT' },
      { id: 'res', label: 'RES' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  bpf2: {
    title: 'BP Filter 2', hp: 6, category: 'filters', knobLayout: 'vertical',
    knobs: [
      { param: 'cut', label: 'CUTOFF', size: 'large', def: 2048 },
      { param: 'res', label: 'RES', size: 'small', def: 1000 },
      { param: 'cutamt', label: 'CUT CV', size: 'small', def: 4095 }
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'cut', label: 'CUT' },
      { id: 'res', label: 'RES' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  lpg: {
    title: 'LP Gate', hp: 4, category: 'filters', knobLayout: 'vertical',
    knobs: [
      { param: 'ctrl', label: 'LEVEL', size: 'medium', def: 2048 },
      { param: 'ctrlamt', label: 'LEVEL CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'ctrl', label: 'LEVEL' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  slew: {
    title: 'Slew Limiter', hp: 4, category: 'envelopes', knobLayout: 'vertical',
    knobs: [
      { param: 'rate', label: 'RATE', size: 'medium', def: 1000 },
      { param: 'rateamt', label: 'RATE CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'rate', label: 'RATE' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  average: {
    title: 'Slew', hp: 4, category: 'filters', knobLayout: 'vertical',
    knobs: [{ param: 'cut', label: 'RESPONSE', size: 'medium', def: 2048 }],
    inputs: [{ id: 'in', label: 'IN' }],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  envfollow: {
    title: 'Env Follow', hp: 4, category: 'filters', knobLayout: 'vertical',
    knobs: [{ param: 'cut', label: 'RESPONSE', size: 'medium', def: 1000 }],
    inputs: [{ id: 'in', label: 'IN' }],
    outputs: [{ id: 'out', label: 'OUT' }]
  },

  // ── Shapers & Dynamics ─────────────────────────────────────────────────
  vca: {
    title: 'VCA', hp: 4, category: 'mixing', knobLayout: 'vertical',
    knobs: [{ param: 'amp', label: 'LEVEL', size: 'medium', def: 4095 }],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'amp', label: 'CV AMP' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  'quad-vca': {
    title: 'Quad VCA', hp: 12, category: 'mixing', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'volA', label: 'VCA A', size: 'small', def: 4095 },
      { param: 'volB', label: 'VCA B', size: 'small', def: 4095 },
      { param: 'volC', label: 'VCA C', size: 'small', def: 4095 },
      { param: 'volD', label: 'VCA D', size: 'small', def: 4095 }
    ],
    inputs: [
      { id: 'inA', label: 'IN A' }, { id: 'cvA', label: 'CV A' },
      { id: 'inB', label: 'IN B' }, { id: 'cvB', label: 'CV B' },
      { id: 'inC', label: 'IN C' }, { id: 'cvC', label: 'CV C' },
      { id: 'inD', label: 'IN D' }, { id: 'cvD', label: 'CV D' }
    ],
    outputs: [
      { id: 'outA', label: 'OUT A' },
      { id: 'outB', label: 'OUT B' },
      { id: 'outC', label: 'OUT C' },
      { id: 'outD', label: 'OUT D' }
    ]
  },
  envelope: {
    title: 'Decay', hp: 6, category: 'envelopes', knobLayout: 'grid',
    knobs: [
      { param: 'decay', label: 'DECAY', size: 'medium', def: 2048 },
      { param: 'peak', label: 'PEAK', size: 'small', def: 4095 },
      { param: 'decayamt', label: 'DECAY CV', size: 'small', def: 4095 },
      { param: 'peakamt', label: 'PEAK CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'trig', label: 'TRIG' },
      { id: 'decay', label: 'DECAY' },
      { id: 'peak', label: 'PEAK' },
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  adsr: {
    title: 'ADSR', hp: 8, category: 'envelopes', knobLayout: 'grid',
    knobs: [
      { param: 'attack', label: 'ATT', size: 'small', def: 512 },
      { param: 'decay', label: 'DEC', size: 'small', def: 1024 },
      { param: 'sustain', label: 'SUS', size: 'small', def: 4095 },
      { param: 'release', label: 'REL', size: 'small', def: 1024 },
    ],
    inputs: [
      { id: 'gate', label: 'GATE' },
      { id: 'attack', label: 'ATT' },
      { id: 'decay', label: 'DEC' },
      { id: 'sustain', label: 'SUS' },
      { id: 'release', label: 'REL' },
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  mix: {
    title: 'Mixer', hp: 6, category: 'mixing', knobLayout: 'grid',
    knobs: [
      { param: 'volA', label: 'A', size: 'small', def: 2048 },
      { param: 'volB', label: 'B', size: 'small', def: 2048 },
      { param: 'volC', label: 'C', size: 'small', def: 2048 },
      { param: 'volD', label: 'D', size: 'small', def: 2048 },
    ],
    inputs: [
      { id: 'a', label: 'IN A' }, { id: 'b', label: 'IN B' },
      { id: 'c', label: 'IN C' }, { id: 'd', label: 'IN D' },
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  ring: {
    title: 'Ring Mod', hp: 2, category: 'effects',
    knobs: [],
    inputs: [{ id: 'in', label: 'CARRIER' }, { id: 'with', label: 'MODULATOR' }],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  wavefold: {
    title: 'Wave Folder', hp: 4, category: 'effects', knobLayout: 'vertical',
    knobs: [
      { param: 'drive', label: 'FOLD', size: 'medium', def: 1000 },
      { param: 'driveamt', label: 'FOLD CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'drive', label: 'FOLD' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  'tape-delay': {
    title: 'Tape Looper', hp: 8, category: 'tapes', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'speed', label: 'PLAY SPEED', size: 'large', def: 2048, noMidi: true },
      { param: 'feedback', label: 'OVERDUB', size: 'medium', def: 3000 },
      { param: 'len', label: 'MAX SECS', size: 'small', def: 0, discrete: [1.0, 2.0, 4.0, 8.0] }
    ],
    inputs: [
      { id: 'in', label: 'AUDIO IN' },
      { id: 'rec', label: 'REC GATE' },
      { id: 'speed', label: 'SPEED CV' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  delay: {
    title: 'Delay', hp: 10, category: 'effects', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'time', label: 'TIME', size: 'large', def: 2048 },
      { param: 'feedback', label: 'FEEDBACK', size: 'medium', def: 1024 },
      { param: 'mix', label: 'MIX', size: 'medium', def: 2048 },
      { param: 'timeamt', label: 'TIME CV', size: 'small', def: 4095 },
      { param: 'feedamt', label: 'FEED CV', size: 'small', def: 4095 },
      { param: 'mode', label: 'MODE', size: 'small', def: 'stereo', discrete: ['mono', 'stereo', 'ping-pong'] },
      { param: 'ratio', label: 'RATIO', size: 'small', def: 2048 }
    ],
    inputs: [
      { id: 'inL', label: 'IN L' },
      { id: 'inR', label: 'IN R' },
      { id: 'time', label: 'TIME' },
      { id: 'feedback', label: 'FEED' },
      { id: 'mix', label: 'MIX' }
    ],
    outputs: [
      { id: 'outL', label: 'OUT L' },
      { id: 'outR', label: 'OUT R' }
    ]
  },
  reverb: {
    title: 'Reverb', hp: 8, category: 'effects', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'decay', label: 'DECAY', size: 'medium', def: 2048 },
      { param: 'mix', label: 'MIX', size: 'medium', def: 1024 },
      { param: 'decayamt', label: 'DECAY CV', size: 'small', def: 4095 },
      { param: 'mixamt', label: 'MIX CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'decay', label: 'DECAY' },
      { id: 'mix', label: 'MIX' },
    ],
    outputs: [
      { id: 'outL', label: 'OUT L' },
      { id: 'outR', label: 'OUT R' }
    ]
  },
  chorus: {
    title: 'Chorus', hp: 6, category: 'effects', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'rate', label: 'RATE', size: 'medium', def: 1000 },
      { param: 'depth', label: 'DEPTH', size: 'medium', def: 2048 },
      { param: 'feedback', label: 'FEEDBACK', size: 'medium', def: 2048 },
      { param: 'rateamt', label: 'RATE CV', size: 'small', def: 4095 },
      { param: 'depthamt', label: 'DEPTH CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'rate', label: 'RATE' },
      { id: 'depth', label: 'DEPTH' },
    ],
    outputs: [
      { id: 'outL', label: 'OUT L' },
      { id: 'outR', label: 'OUT R' }
    ]
  },
  flanger: {
    title: 'Flanger', hp: 6, category: 'effects', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'rate', label: 'RATE', size: 'medium', def: 500 },
      { param: 'depth', label: 'DEPTH', size: 'medium', def: 1024 },
      { param: 'feedback', label: 'FEEDBACK', size: 'medium', def: 3000 },
      { param: 'rateamt', label: 'RATE CV', size: 'small', def: 4095 },
      { param: 'depthamt', label: 'DEPTH CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'rate', label: 'RATE' },
      { id: 'depth', label: 'DEPTH' },
    ],
    outputs: [
      { id: 'outL', label: 'OUT L' },
      { id: 'outR', label: 'OUT R' }
    ]
  },
  compressor: {
    title: 'Compressor', hp: 8, category: 'mixing', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'threshold', label: 'THRESH', size: 'medium', def: 3000 },
      { param: 'ratio', label: 'RATIO', size: 'medium', def: 2048 },
      { param: 'attack', label: 'ATTACK', size: 'small', def: 100 },
      { param: 'release', label: 'RELEASE', size: 'small', def: 1000 },
    ],
    inputs: [
      { id: 'in', label: 'IN' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  logic: {
    title: 'Logic Gate', hp: 2, category: 'logic', knobLayout: 'vertical',
    isMacro: true,
    knobs: [],
    inputs: [
      { id: 'a', label: 'IN A' },
      { id: 'b', label: 'IN B' }
    ],
    outputs: [
      { id: 'and', label: 'AND' },
      { id: 'or', label: 'OR' },
      { id: 'xor', label: 'XOR' },
      { id: 'not', label: 'NOT A' }
    ]
  },
  math: {
    title: 'Math', hp: 2, category: 'math', knobLayout: 'vertical',
    isMacro: true,
    knobs: [],
    inputs: [
      { id: 'a', label: 'IN A' },
      { id: 'b', label: 'IN B' }
    ],
    outputs: [
      { id: 'add', label: 'ADD' },
      { id: 'sub', label: 'SUB' },
      { id: 'mul', label: 'MUL' },
      { id: 'div', label: 'DIV' },
      { id: 'mod', label: 'MOD' },
      { id: 'abs', label: 'ABS A' },
      { id: 'min', label: 'MIN' },
      { id: 'max', label: 'MAX' }
    ]
  },
  'cv-pitch': {
    title: 'CV to Pitch', hp: 4, category: 'math', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'octaves', label: 'OCTAVES', size: 'large', def: 1228 },
      { param: 'pitch', label: 'BASE', size: 'large', def: 1935 }
    ],
    inputs: [
      { id: 'in', label: 'CV IN' }
    ],
    outputs: [
      { id: 'out', label: 'PITCH' }
    ]
  },
  quantizer: {
    title: 'Quantizer', hp: 4, category: 'math', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      {
        param: 'scale', label: 'SCALE', size: 'large', def: 0,
        discrete: ['minor', 'major', 'm.pent', 'M.pent', 'dorian', 'phryg', 'lydian', 'mixo', 'chrom']
      },
      {
        param: 'root', label: 'ROOT', size: 'large', def: 0,
        discrete: ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
      }
    ],
    inputs: [
      { id: 'in', label: 'IN V/OCT' },
      { id: 'scale', label: 'SCALE CV' },
      { id: 'root', label: 'ROOT CV' }
    ],
    outputs: [
      { id: 'out', label: 'OUT V/OCT' }
    ]
  },
  transpose: {
    title: 'Transpose', hp: 4, category: 'math', knobLayout: 'vertical',
    knobs: [
      { param: 'by', label: 'TRANSPOSE', size: 'large', def: 2048, discrete: Array.from({ length: 49 }, (_, i) => i - 24) }
    ],
    inputs: [
      { id: 'in', label: 'IN V/OCT' },
      { id: 'by', label: 'BY CV' }
    ],
    outputs: [
      { id: 'out', label: 'OUT V/OCT' }
    ]
  },
  gain: {
    title: 'Amplifier', hp: 4, category: 'logic', knobLayout: 'vertical',
    knobs: [
      { param: 'gain', label: 'GAIN', size: 'large', def: 512 }
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'gain', label: 'GAIN CV' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  attenuverter: {
    title: 'Attenuverter', hp: 4, category: 'mixing', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'gain', label: 'GAIN', size: 'large', def: 2048 },
      { param: 'offset', label: 'OFFSET', size: 'medium', def: 2048 }
    ],
    inputs: [
      { id: 'in', label: 'IN' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  morph: {
    title: 'Morph Scanner', hp: 6, category: 'math', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'pos', label: 'POSITION', size: 'large', def: 0 }
    ],
    inputs: [
      { id: 'in1', label: 'IN 1' },
      { id: 'in2', label: 'IN 2' },
      { id: 'in3', label: 'IN 3' },
      { id: 'in4', label: 'IN 4' },
      { id: 'pos', label: 'POS CV' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  saturate: {
    title: 'Saturator', hp: 8, category: 'effects', knobLayout: 'grid',
    knobs: [
      { param: 'drive', label: 'DRIVE', size: 'large', def: 2048 },
      { param: 'bias', label: 'BIAS', size: 'small', def: 0 },
      { param: 'mix', label: 'WET', size: 'small', def: 4095 },
      { param: 'level', label: 'LEVEL', size: 'small', def: 4095 },
      { param: 'driveamt', label: 'DRIVE CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'drive', label: 'CV DRV' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  shape: {
    title: 'Shaper LUT', hp: 6, category: 'effects', knobLayout: 'vertical',
    knobs: [
      { param: 'drive', label: 'DRIVE', size: 'large', def: 2048 },
      { param: 'curve', label: 'CURVE', size: 'medium', def: 0, discrete: [0, 1, 2, 3] },
      { param: 'oversample', label: 'OVERSMPL', size: 'small', def: 0, discrete: [0, 1] }
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'drive', label: 'CV DRV' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  crush: {
    title: 'Bit Crusher', hp: 6, category: 'effects', knobLayout: 'vertical',
    knobs: [
      { param: 'rate', label: 'RATE', size: 'large', def: 4095 },
      { param: 'rateamt', label: 'RATE CV', size: 'small', def: 4095 },
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'rate', label: 'CV RATE' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  add: {
    title: 'CV Adder', hp: 2, category: 'math',
    knobs: [],
    inputs: [{ id: 'a', label: 'IN A' }, { id: 'b', label: 'IN B' }, { id: 'c', label: 'IN C' }],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  mul: {
    title: 'Attenuator', hp: 2, category: 'math', knobLayout: 'vertical',
    knobs: [{ param: 'gain', label: 'GAIN', size: 'small', def: 4095 }],
    inputs: [{ id: 'a', label: 'IN' }],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  constant: {
    title: 'Offset / DC', hp: 4, category: 'math', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'val1', label: 'VAL 1', size: 'small', def: 2048 },
      { param: 'val2', label: 'VAL 2', size: 'small', def: 2048 },
      { param: 'val3', label: 'VAL 3', size: 'small', def: 2048 }
    ],
    inputs: [],
    outputs: [
      { id: 'out1', label: 'OUT 1' },
      { id: 'out2', label: 'OUT 2' },
      { id: 'out3', label: 'OUT 3' }
    ]
  },
  expression: {
    title: 'Equation', hp: 8, category: 'logic', knobLayout: 'vertical',
    knobs: [],
    inputs: [
      { id: 'a', label: 'IN A' },
      { id: 'b', label: 'IN B' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'stereo-mixer': {
    title: 'Stereo Mixer', hp: 10, category: 'mixing', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'volA', label: 'VOL A', size: 'small', def: 2048 },
      { param: 'panA', label: 'PAN A', size: 'small', def: 2048 },
      { param: 'muteA', label: 'MUTE A', size: 'small', def: 0, discrete: ['active', 'muted'] },
      { param: 'volB', label: 'VOL B', size: 'small', def: 2048 },
      { param: 'panB', label: 'PAN B', size: 'small', def: 2048 },
      { param: 'muteB', label: 'MUTE B', size: 'small', def: 0, discrete: ['active', 'muted'] },
      { param: 'volC', label: 'VOL C', size: 'small', def: 2048 },
      { param: 'panC', label: 'PAN C', size: 'small', def: 2048 },
      { param: 'muteC', label: 'MUTE C', size: 'small', def: 0, discrete: ['active', 'muted'] },
      { param: 'volD', label: 'VOL D', size: 'small', def: 2048 },
      { param: 'panD', label: 'PAN D', size: 'small', def: 2048 },
      { param: 'muteD', label: 'MUTE D', size: 'small', def: 0, discrete: ['active', 'muted'] },
      { param: 'master', label: 'MASTER', size: 'medium', def: 3000 }
    ],
    inputs: [
      { id: 'inA', label: 'IN A' },
      { id: 'inB', label: 'IN B' },
      { id: 'inC', label: 'IN C' },
      { id: 'inD', label: 'IN D' }
    ],
    outputs: [
      { id: 'outL', label: 'OUT L' },
      { id: 'outR', label: 'OUT R' }
    ]
  },
  mult: {
    title: 'Multiple', hp: 2, category: 'logic', knobLayout: 'vertical',
    isMacro: true,
    knobs: [],
    inputs: [{ id: 'in', label: 'IN' }],
    outputs: [
      { id: 'out1', label: 'OUT 1' },
      { id: 'out2', label: 'OUT 2' },
      { id: 'out3', label: 'OUT 3' }
    ]
  },
  'signal-switch': {
    title: '3-Way Switch', hp: 2, category: 'math', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'pos', label: 'SWITCH', size: 'small', def: 2048, discrete: ['down', 'mid', 'up'] }
    ],
    inputs: [
      { id: 'cond', label: 'CONTROL' },
      { id: 'a', label: 'IN A (DN)' },
      { id: 'b', label: 'IN B (MID)' },
      { id: 'c', label: 'IN C (UP)' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'seq-switch': {
    title: 'Seq Switch', hp: 4, category: 'logic', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'steps', label: 'STEPS', size: 'small', def: 4095, discrete: [2, 3, 4, 5, 6, 7, 8] }
    ],
    inputs: [
      { id: 'trig', label: 'CLK' },
      { id: 'reset', label: 'RESET' },
      { id: 'in1', label: '1' },
      { id: 'in2', label: '2' },
      { id: 'in3', label: '3' },
      { id: 'in4', label: '4' },
      { id: 'in5', label: '5' },
      { id: 'in6', label: '6' },
      { id: 'in7', label: '7' },
      { id: 'in8', label: '8' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },

  // ── Voices ─────────────────────────────────────────────────────────────
  kick: {
    title: 'Kick Synth', hp: 6, category: 'voices', knobLayout: 'vertical',
    knobs: [
      { param: 'note', label: 'PITCH', size: 'medium', def: 1161 },
      { param: 'decay', label: 'DECAY', size: 'medium', def: 2048 },
      { param: 'drive', label: 'DRIVE', size: 'small', def: 0 },
    ],
    inputs: [
      { id: 'trig', label: 'TRIG' },
      { id: 'note', label: 'PITCH CV' },
      { id: 'decay', label: 'CV DEC' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  snare: {
    title: 'Snare Synth', hp: 6, category: 'voices', knobLayout: 'vertical',
    knobs: [
      { param: 'note', label: 'PITCH', size: 'medium', def: 1451 },
      { param: 'decay', label: 'DECAY', size: 'medium', def: 2048 },
      { param: 'snappy', label: 'NOISE', size: 'small', def: 2048 },
    ],
    inputs: [
      { id: 'trig', label: 'TRIG' },
      { id: 'note', label: 'PITCH CV' },
      { id: 'decay', label: 'CV DEC' },
      { id: 'snappy', label: 'CV NOISE' },
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  hat: {
    title: 'Hi-Hat', hp: 6, category: 'voices', knobLayout: 'vertical',
    knobs: [
      { param: 'note', label: 'TENSION', size: 'medium', def: 2580 },
      { param: 'decay', label: 'DECAY', size: 'medium', def: 1024 },
    ],
    inputs: [
      { id: 'trig', label: 'TRIG' },
      { id: 'note', label: 'PITCH CV' },
      { id: 'decay', label: 'CV DEC' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  pluck: {
    title: 'Plucked String', hp: 6, category: 'oscillators', knobLayout: 'vertical',
    knobs: [
      { param: 'pitch', label: 'PITCH', size: 'medium', def: 1935 },
      { param: 'damp', label: 'DECAY', size: 'medium', def: 2048 },
    ],
    inputs: [
      { id: 'trig', label: 'STRIKE' },
      { id: 'pitch', label: 'V/OCT' },
      { id: 'damp', label: 'CV DAMP' },
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  dx: {
    title: 'DX FM Voice', hp: 8, category: 'voices', knobLayout: 'grid',
    knobs: [
      { param: 'pitch', label: 'PITCH', size: 'large', def: 1935 },
      { param: 'cents', label: 'FINE', size: 'small', def: 2048 },
      { param: 'bank', label: 'BANK', size: 'small', def: 0, discrete: [0, 1, 2] },
      { param: 'preset', label: 'PRESET', size: 'medium', def: 0, discrete: Array.from({ length: 32 }, (_, i) => i) },
      { param: 'decay', label: 'DECAY', size: 'medium', def: 2048 },
      { param: 'tone', label: 'TONE', size: 'small', def: 2048 }
    ],
    inputs: [
      { id: 'pitch', label: 'V/OCT' },
      { id: 'gate', label: 'GATE' },
      { id: 'decay', label: 'CV DEC' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  weave: {
    title: 'Weave macro osc', hp: 10, category: 'voices', knobLayout: 'hybrid',
    knobs: [
      { param: 'pitch', label: 'PITCH', size: 'large', def: 1935 },
      { param: 'timbre', label: 'TIMBRE', size: 'medium', def: 2048 },
      { param: 'color', label: 'COLOR', size: 'medium', def: 2048 },
      { param: 'model', label: 'MODEL', size: 'small', def: 0, discrete: [
        'CSAW', 'MORPH', 'SAW_SQR', 'SIN_TRI', 'BUZZ',
        'SQR_SUB', 'SAW_SUB', 'SQR_SYNC', 'SAW_SYNC',
        'TRP_SAW', 'TRP_SQR', 'TRP_TRI', 'TRP_SIN',
        'TRP_RING', 'SAW_SWRM', 'SAW_COMB', 'TOY',
        'LP_FILT', 'PK_FILT', 'BP_FILT', 'HP_FILT',
        'VOSIM', 'VOWEL', 'VOW_FOF', 'HARMONICS',
        'FM', 'FDBK_FM', 'CHAOS_FM',
        'PLUCKED', 'BOWED', 'BLOWN', 'FLUTED',
        'BELL', 'STR_DRUM', 'KICK', 'CYMBAL', 'SNARE',
        'WAVETABLE', 'WAVE_MAP', 'WAVE_LINE', 'POLY_WAVE',
        'FILT_NOISE', 'TWIN_PEAKS', 'CLK_NOISE', 'GRN_CLOUD', 'PARTICLES',
        'DIGIT_MOD', 'MYSTERY'
      ] }
    ],
    inputs: [
      { id: 'note', label: 'V/OCT' },
      { id: 'timbre', label: 'TIMBRE' },
      { id: 'color', label: 'COLOR' },
      { id: 'model', label: 'MODEL CV' },
      { id: 'trig', label: 'TRIG' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'tape-looper': {
    title: 'Tape Sampler', hp: 8, category: 'tapes', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'speed', label: 'PLAY SPEED', size: 'large', def: 2048 },
      { param: 'len', label: 'MAX SECS', size: 'medium', def: 0, discrete: [1.0, 2.0, 4.0, 8.0] },
      { param: 'loop', label: 'PLAY MODE', size: 'small', def: 0, discrete: ['loop', 'one-shot'] }
    ],
    inputs: [
      { id: 'in', label: 'AUDIO IN' },
      { id: 'rec', label: 'REC GATE' },
      { id: 'trig', label: 'PLAY TRIG' },
      { id: 'speed', label: 'SPEED CV' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  rungler: {
    title: 'Rungler', hp: 12, category: 'sequencing', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'freq1', label: 'VCO 1 FREQ', size: 'large', def: 1935 },
      { param: 'freq2', label: 'VCO 2 FREQ', size: 'large', def: 1935 },
      { param: 'rungler', label: 'RUNG DEPT', size: 'medium', def: 1024 },
      { param: 'lock', label: 'LOOP LOCK', size: 'small', def: 0, discrete: ['run', 'lock'] }
    ],
    inputs: [
      { id: 'pitch1', label: 'VCO1 PITCH' },
      { id: 'pitch2', label: 'VCO2 PITCH' }
    ],
    outputs: [
      { id: 'out1', label: 'VCO 1 OUT' },
      { id: 'out2', label: 'VCO 2 OUT' },
      { id: 'rungle', label: 'STEP RUNG' },
      { id: 'runglesm', label: 'SMOOTH RG' }
    ]
  },

  // ── Clocks & Sequencing ────────────────────────────────────────────────
  clock: {
    title: 'Master Clock', hp: 4, category: 'clocks', knobLayout: 'vertical',
    knobs: [
      { param: 'bpm', label: 'BPM', size: 'medium', def: 1638 },
      { param: 'fm', label: 'TEMPO CV AMT', size: 'small', def: 0 },
      { param: 'width', label: 'WIDTH', size: 'small', def: 2048 }
    ],
    inputs: [
      { id: 'sync', label: 'SYNC' },
      { id: 'fm', label: 'TEMPO CV' }
    ],
    outputs: [
      { id: 'out', label: '1/4 OUT' },
      { id: 'mult2', label: '1/8 OUT' },
      { id: 'mult4', label: '1/16 OUT' }
    ],
  },
  'multi-div': {
    title: 'Clock Divider', hp: 2, category: 'clocks',
    isMacro: true,
    knobs: [],
    inputs: [{ id: 'trig', label: 'CLOCK' }],
    outputs: [
      { id: 'div2', label: '/2' },
      { id: 'div3', label: '/3' },
      { id: 'div4', label: '/4' },
      { id: 'div8', label: '/8' }
    ]
  },
  rhythm: {
    title: 'Rhythm Player', hp: 6, category: 'clocks', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      {
        param: 'pattern', label: 'PATTERN', size: 'large', def: 0,
        discrete: ['four-on-floor', 'backbeat', 'eighths', 'offbeat', 'sixteenths', 'downbeat', 'tresillo', 'cinquillo', 'habanera', 'son-clave', 'rumba-clave', 'bossa']
      },
      {
        param: 'mode', label: 'MODE', size: 'medium', def: 0,
        discrete: ['onsets', 'gates', 'hits']
      }
    ],
    inputs: [
      { id: 'trig', label: 'CLOCK' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  turing: {
    title: 'Turing Machine', hp: 6, category: 'sequencing', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'prob', label: 'MUTATION', size: 'large', def: 4095 },
      { param: 'len', label: 'LENGTH', size: 'medium', def: 4095, discrete: [1, 2, 3, 4, 5, 6, 7, 8, 12, 16] }
    ],
    inputs: [
      { id: 'trig', label: 'CLOCK' },
      { id: 'prob', label: 'CV MUT' },
    ],
    outputs: [
      { id: 'out', label: 'V/OCT' },
      { id: 'trig', label: 'TRIG' }
    ]
  },
  'score-player': {
    title: 'Score Player', hp: 8, category: 'tapes', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'speed', label: 'PLAY SPEED', size: 'small', def: 2048 }
    ],
    inputs: [
      { id: 'trig', label: 'CLOCK' },
      { id: 'speed', label: 'CV SPEED' }
    ],
    outputs: [
      { id: 'note', label: 'V/OCT' },
      { id: 'gate', label: 'GATE' }
    ]
  },
  'computer': {
    title: 'Computer', hp: 8, category: 'clocks', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'main', label: 'MAIN', size: 'huge', def: 2048 },
      { param: 'x', label: 'X', size: 'small', def: 2048 },
      { param: 'y', label: 'Y', size: 'small', def: 2048 },
      { param: 'z', label: 'Z SWITCH', size: 'small', def: 2048, discrete: ['down', 'mid', 'up'] }
    ],
    inputs: [
      { id: 'audio-in-1', label: 'AUD 1' },
      { id: 'audio-in-2', label: 'AUD 2' },
      { id: 'cv-in-1', label: 'CV 1' },
      { id: 'cv-in-2', label: 'CV 2' },
      { id: 'pulse-in-1', label: 'PLS 1' },
      { id: 'pulse-in-2', label: 'PLS 2' }
    ],
    outputs: [
      { id: 'audio-out-1', label: 'AUD 1' },
      { id: 'audio-out-2', label: 'AUD 2' },
      { id: 'cv-out-1', label: 'CV 1' },
      { id: 'cv-out-2', label: 'CV 2' },
      { id: 'pulse-out-1', label: 'PLS 1' },
      { id: 'pulse-out-2', label: 'PLS 2' }
    ]
  },
  'step-seq': {
    title: 'Step Sequencer', hp: 12, category: 'sequencing', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'val1', label: 'STEP 1', size: 'small', def: 0 },
      { param: 'val2', label: 'STEP 2', size: 'small', def: 512 },
      { param: 'val3', label: 'STEP 3', size: 'small', def: 1024 },
      { param: 'val4', label: 'STEP 4', size: 'small', def: 1536 },
      { param: 'val5', label: 'STEP 5', size: 'small', def: 2048 },
      { param: 'val6', label: 'STEP 6', size: 'small', def: 2560 },
      { param: 'val7', label: 'STEP 7', size: 'small', def: 3072 },
      { param: 'val8', label: 'STEP 8', size: 'small', def: 3584 },
      { param: 'steps', label: 'STEPS', size: 'small', def: 4095, discrete: [1, 2, 3, 4, 5, 6, 7, 8] },
      { param: 'dir', label: 'DIR', size: 'small', def: 0, discrete: ['forward', 'backward', 'random'] }
    ],
    inputs: [
      { id: 'trig', label: 'CLOCK' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  'drum-seq': {
    title: 'Drum Sequencer', hp: 24, category: 'sequencing', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'steps', label: 'STEPS', size: 'small', def: 4095, discrete: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16] }
    ],
    inputs: [
      { id: 'trig', label: 'CLOCK' }
    ],
    outputs: [
      { id: 'kick', label: 'KICK' },
      { id: 'snare', label: 'SNARE' },
      { id: 'hat', label: 'HAT' },
      { id: 'perc', label: 'PERC' }
    ]
  },
  'midi-sync': {
    title: 'MIDI Clock', hp: 4, category: 'clocks', knobLayout: 'vertical',
    isMacro: true,
    knobs: [],
    inputs: [],
    outputs: [
      { id: 'clock', label: 'CLOCK' },
      { id: 'run', label: 'RUN' }
    ]
  },
  trig: {
    title: 'Trig Delay', hp: 2, category: 'clocks', knobLayout: 'vertical',
    knobs: [{ param: 'rate', label: 'DELAY TIME', size: 'small', def: 1000 }],
    inputs: [
      { id: 'trig', label: 'IN' },
      { id: 'rate', label: 'CV RATE' },
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  every: {
    title: 'Clock Div', hp: 2, category: 'clocks', knobLayout: 'vertical',
    knobs: [{ param: 'n', label: 'DIVISOR', size: 'small', def: 1103, discrete: [1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 24, 32, 64] }],
    inputs: [{ id: 'trig', label: 'IN' }],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  counter: {
    title: 'Counter', hp: 4, category: 'clocks', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'bars', label: 'STEPS', size: 'small', def: 4095, discrete: [2, 3, 4, 5, 6, 7, 8] }
    ],
    inputs: [
      { id: 'trig', label: 'CLOCK' },
      { id: 'reset', label: 'RESET' }
    ],
    outputs: [
      { id: 'count', label: 'COUNT' },
      { id: 'out1', label: 'STEP 1' },
      { id: 'out2', label: 'STEP 2' },
      { id: 'out3', label: 'STEP 3' },
      { id: 'out4', label: 'STEP 4' },
      { id: 'out5', label: 'STEP 5' },
      { id: 'out6', label: 'STEP 6' },
      { id: 'out7', label: 'STEP 7' },
      { id: 'out8', label: 'STEP 8' }
    ]
  },
  euclid: {
    title: 'Euclid Gen', hp: 4, category: 'clocks', knobLayout: 'vertical',
    knobs: [
      { param: 'steps', label: 'STEPS', size: 'medium', def: 1984, discrete: Array.from({ length: 32 }, (_, i) => i + 1) },
      { param: 'pulses', label: 'PULSES', size: 'medium', def: 448, discrete: Array.from({ length: 32 }, (_, i) => i + 1) },
    ],
    inputs: [
      { id: 'trig', label: 'CLOCK' },
      { id: 'steps', label: 'STEPS CV' },
      { id: 'pulses', label: 'PULSES CV' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },

  turns: {
    title: 'Trig Counter', hp: 2, category: 'clocks', knobLayout: 'vertical',
    knobs: [{ param: 'n', label: 'CHANNELS', size: 'small', def: 1138, discrete: [2, 3, 4, 5, 6, 7, 8, 12, 16] }],
    inputs: [{ id: 'trig', label: 'CLOCK' }],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'shift-register': {
    title: 'Shift Register', hp: 4, category: 'clocks', knobLayout: 'vertical',
    isMacro: true,
    knobs: [],
    inputs: [
      { id: 'in', label: 'CV IN' },
      { id: 'trig', label: 'CLOCK' },
      { id: 'reset', label: 'RESET' }
    ],
    outputs: [
      { id: 'out1', label: 'STAGE 1' },
      { id: 'out2', label: 'STAGE 2' },
      { id: 'out3', label: 'STAGE 3' },
      { id: 'out4', label: 'STAGE 4' },
      { id: 'out5', label: 'STAGE 5' },
      { id: 'out6', label: 'STAGE 6' },
      { id: 'out7', label: 'STAGE 7' },
      { id: 'out8', label: 'STAGE 8' }
    ]
  },
  gate: {
    title: 'Gate Gen', hp: 6, category: 'clocks', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'thresh', label: 'THRESHOLD', size: 'medium', def: 2048 },
      { param: 'len', label: 'LENGTH', size: 'medium', def: 100 }
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'thresh', label: 'CV THR' },
      { id: 'len', label: 'CV LEN' },
    ],
    outputs: [{ id: 'out', label: 'OUT' }],
  },
  schmitt: {
    title: 'Schmitt Trigger', hp: 2, category: 'clocks', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'lo', label: 'LOW THR', size: 'small', def: 1024 },
      { param: 'hi', label: 'HIGH THR', size: 'small', def: 3072 }
    ],
    inputs: [
      { id: 'in', label: 'IN' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  toggle: {
    title: 'Toggle Flip', hp: 2, category: 'clocks',
    knobs: [],
    inputs: [{ id: 'in', label: 'IN' }],
    outputs: [{ id: 'out', label: 'OUT' }],
  },

  walk: {
    title: 'Rand Walk', hp: 4, category: 'clocks', knobLayout: 'vertical',
    knobs: [{ param: 'step', label: 'STEP SIZE', size: 'medium', def: 1000 }],
    inputs: [
      { id: 'trig', label: 'CLOCK' },
      { id: 'step', label: 'CV STEP' },
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },

  // ── MIDI ───────────────────────────────────────────────────────────────
  'midi-note': {
    title: 'MIDI Keyboard', hp: 6, category: 'io', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'ch', label: 'MIDI CH', size: 'medium', def: 0, discrete: Array.from({ length: 17 }, (_, i) => i) }
    ],
    inputs: [],
    outputs: [
      { id: 'note', label: 'V/OCT' },
      { id: 'gate', label: 'GATE' },
      { id: 'vel', label: 'VELOCITY' },
      { id: 'press', label: 'PRESSURE' },
      { id: 'bend', label: 'PITCH BEND' }
    ]
  },
  'midi-cc': {
    title: 'MIDI CC In', hp: 4, category: 'io', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'ch', label: 'MIDI CH', size: 'small', def: 0, discrete: Array.from({ length: 17 }, (_, i) => i) },
      { param: 'cc', label: 'CC NUM', size: 'medium', def: 1, discrete: Array.from({ length: 128 }, (_, i) => i) }
    ],
    inputs: [],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'midi-trig': {
    title: 'MIDI Note Trig', hp: 4, category: 'io', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'ch', label: 'MIDI CH', size: 'small', def: 0, discrete: Array.from({ length: 17 }, (_, i) => i) },
      { param: 'note', label: 'NOTE NUM', size: 'medium', def: 1920, discrete: Array.from({ length: 128 }, (_, i) => i) }
    ],
    inputs: [],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'midi-clock': {
    title: 'MIDI Clock In', hp: 2, category: 'io',
    isMacro: true,
    knobs: [],
    inputs: [],
    outputs: [
      { id: 'clk', label: 'CLOCK' },
      { id: 'play', label: 'RUNNING' }
    ]
  },
  'midi-note-out': {
    title: 'MIDI Keyb Out', hp: 6, category: 'io', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'ch', label: 'MIDI CH', size: 'medium', def: 1, discrete: Array.from({ length: 16 }, (_, i) => i + 1) }
    ],
    inputs: [
      { id: 'pitch', label: 'V/OCT' },
      { id: 'gate', label: 'GATE' },
      { id: 'vel', label: 'VELOCITY' }
    ],
    outputs: []
  },
  'midi-cc-out': {
    title: 'MIDI CC Out', hp: 4, category: 'io', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'ch', label: 'MIDI CH', size: 'small', def: 1, discrete: Array.from({ length: 16 }, (_, i) => i + 1) },
      { param: 'cc', label: 'CC NUM', size: 'medium', def: 1, discrete: Array.from({ length: 128 }, (_, i) => i) }
    ],
    inputs: [
      { id: 'val', label: 'SIGNAL' }
    ],
    outputs: []
  },
  'midi-clock-out': {
    title: 'MIDI Clock Out', hp: 4, category: 'io', knobLayout: 'vertical',
    isMacro: true,
    knobs: [],
    inputs: [
      { id: 'clk', label: 'CLOCK' }
    ],
    outputs: []
  },
  'midi-score': {
    title: 'MIDI Recorder', hp: 6, category: 'sequencing', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'speed', label: 'PLAY SPEED', size: 'large', def: 2048 }
    ],
    inputs: [
      { id: 'rec', label: 'REC GATE' },
      { id: 'clk', label: 'CLOCK' },
      { id: 'speed', label: 'CV SPEED' }
    ],
    outputs: [
      { id: 'notes', label: 'NOTE' },
      { id: 'rhythm', label: 'GATE' },
      { id: 'vel', label: 'VEL' }
    ]
  },
  audio: {
    title: 'Audio Buffer', hp: 6, category: 'tapes', knobLayout: 'vertical',
    knobs: [
      { param: 'unit', label: 'UNIT', size: 'small', def: 0, discrete: ['secs', 'smpls'] },
      { param: 'time', label: 'BUFFER SIZE', size: 'large', def: 2048 }
    ],
    inputs: [],
    outputs: [
      { id: 'tape', label: 'TAPE OUT' },
      { id: 'len', label: 'LEN' }
    ]
  },
  'feedback-cell': {
    title: 'Sample Delay', hp: 2, category: 'tapes', knobLayout: 'vertical',
    knobs: [],
    inputs: [{ id: 'in', label: 'IN' }],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'edge': {
    title: 'Edge Detector', hp: 2, category: 'logic',
    isMacro: true,
    knobs: [],
    inputs: [{ id: 'in', label: 'IN' }],
    outputs: [{ id: 'rise', label: 'RISE' }, { id: 'fall', label: 'FALL' }]
  },
  'if-gate': {
    title: 'If Gate', hp: 4, category: 'math', knobLayout: 'vertical',
    knobs: [],
    inputs: [
      { id: 'cond', label: 'COND' },
      { id: 'then', label: 'THEN' },
      { id: 'else', label: 'ELSE' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'len': {
    title: 'Tape Length', hp: 2, category: 'tapes',
    knobs: [],
    inputs: [{ id: 'tape', label: 'TAPE' }],
    outputs: [{ id: 'out', label: 'LEN' }]
  },
  'chance': {
    title: 'Probability Gate', hp: 4, category: 'clocks', knobLayout: 'vertical',
    knobs: [
      { param: 'prob', label: 'PROB', size: 'medium', def: 2048 }
    ],
    inputs: [
      { id: 'trig', label: 'CLK' },
      { id: 'prob', label: 'PROB CV' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'random': {
    title: 'Random S&H', hp: 2, category: 'math', knobLayout: 'vertical',
    knobs: [
      { param: 'range', label: 'RANGE', size: 'small', def: 0, discrete: ['bipolar', 'unipolar'] }
    ],
    inputs: [{ id: 'trig', label: 'CLK' }],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  tape: {
    title: 'Data Tape', hp: 12, category: 'tapes', knobLayout: 'horizontal',
    knobs: [
      { param: 'mode', label: 'MODE', size: 'small', def: 0, discrete: ['pattern', 'blank'] },
      { param: 'steps', label: 'STEPS', size: 'small', def: 0, discrete: [8, 16, 32, 64, 128, 256] }
    ],
    inputs: [],
    outputs: [
      { id: 'tape', label: 'TAPE OUT' },
      { id: 'len', label: 'LEN' }
    ]
  },
  'wave-draw': {
    title: 'Wave Draw', hp: 16, category: 'tapes', knobLayout: 'horizontal',
    knobs: [],
    inputs: [],
    outputs: [
      { id: 'tape', label: 'TAPE OUT' },
      { id: 'len', label: 'LEN' }
    ]
  },
  'hold': {
    title: 'Sample & Hold', hp: 4, category: 'math', knobLayout: 'vertical',
    knobs: [],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'trig', label: 'CLK (TRIG)' },
      { id: 'gate', label: 'GATE (ON)' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  step: {
    title: 'Step Reader', hp: 2, category: 'tapes', knobLayout: 'vertical',
    knobs: [],
    inputs: [
      { id: 'tape', label: 'TAPE' },
      { id: 'trig', label: 'CLK' },
      { id: 'len', label: 'LIMIT' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  seek: {
    title: 'Seek Reader', hp: 4, category: 'tapes', knobLayout: 'vertical',
    knobs: [],
    inputs: [
      { id: 'tape', label: 'TAPE' },
      { id: 'at', label: 'INDEX' },
      { id: 'len', label: 'LIMIT' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  lookup: {
    title: 'Index Lookup', hp: 2, category: 'tapes', knobLayout: 'vertical',
    knobs: [],
    inputs: [
      { id: 'tape', label: 'TAPE' },
      { id: 'at', label: 'INDEX' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'wave-scanner': {
    title: 'Wave Scanner', hp: 4, category: 'tapes', knobLayout: 'vertical',
    knobs: [],
    inputs: [
      { id: 'tape', label: 'TAPE' },
      { id: 'note', label: 'V/OCT' },
      { id: 'pos', label: 'POSITION' },
      { id: 'len', label: 'LENGTH' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'tape-write': {
    title: 'Tape Writer', hp: 4, category: 'tapes', knobLayout: 'vertical',
    knobs: [],
    inputs: [
      { id: 'tape', label: 'TAPE' },
      { id: 'at', label: 'INDEX' },
      { id: 'val', label: 'VAL' },
      { id: 'trig', label: 'CLK' },
      { id: 'when', label: 'GATE' }
    ],
    outputs: []
  },
  'tape-record': {
    title: 'Continuous Writer', hp: 2, category: 'tapes', knobLayout: 'vertical',
    knobs: [],
    inputs: [
      { id: 'tape', label: 'TAPE' },
      { id: 'in', label: 'IN' },
      { id: 'when', label: 'GATE' }
    ],
    outputs: []
  },
  tap: {
    title: 'Tap', hp: 2, category: 'tapes', knobLayout: 'vertical',
    knobs: [
      { param: 'span', label: 'SPAN MODE', size: 'small', def: 0, discrete: ['samples', 'span'] }
    ],
    inputs: [
      { id: 'tape', label: 'TAPE' },
      { id: 'amount', label: 'DELAY' }
    ],
    outputs: [{ id: 'out', label: 'OUT' }]
  },
  'cv-looper': {
    title: 'CV Looper', hp: 6, category: 'tapes', knobLayout: 'vertical',
    isMacro: true,
    knobs: [
      { param: 'len', label: 'STEPS', size: 'medium', def: 2048, discrete: [8, 16, 32] }
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'rec', label: 'REC' },
      { id: 'trig', label: 'CLK' }
    ],
    outputs: [
      { id: 'out', label: 'OUT' }
    ]
  },
  granular: {
    title: 'Granular', hp: 10, category: 'oscillators', knobLayout: 'grid',
    isMacro: true,
    knobs: [
      { param: 'density', label: 'DENS', size: 'medium', def: 1638 },
      { param: 'scatter', label: 'SCAT', size: 'medium', def: 1024 },
      { param: 'size', label: 'SIZE', size: 'small', def: 2048 },
      { param: 'spread', label: 'SPRD', size: 'small', def: 2048 },
      { param: 'mix', label: 'MIX', size: 'small', def: 4095 },
      { param: 'feedback', label: 'FDBK', size: 'small', def: 0 }
    ],
    inputs: [
      { id: 'in', label: 'IN' },
      { id: 'freeze', label: 'FREEZE' },
      { id: 'density', label: 'DENS' },
      { id: 'scatter', label: 'SCAT' },
      { id: 'size', label: 'SIZE' },
      { id: 'spread', label: 'SPRD' }
    ],
    outputs: [
      { id: 'outL', label: 'L' },
      { id: 'outR', label: 'R' }
    ]
  },
};

const CATEGORY_ORDER = [
  'io',
  'oscillators',
  'voices',
  'filters',
  'envelopes',
  'mixing',
  'effects',
  'math',
  'logic',
  'clocks',
  'tapes',
  'sequencing'
];
const CATEGORY_LABELS = {
  io: 'Hardware I/O & MIDI',
  oscillators: 'Oscillators & LFOs',
  voices: 'Synthesizers & Drums',
  filters: 'Filters & LPGs',
  envelopes: 'Envelopes & Slew',
  mixing: 'VCAs & Mixers',
  effects: 'Effects & Shapers',
  math: 'Math & CV Utilities',
  logic: 'Logic & Utilities',
  clocks: 'Clocks & Gates',
  tapes: 'Tape Memory',
  sequencing: 'Sequencers'
};

// ═══════════════════════════════════════════════════════════════════════
// 2. STATE & POSITION VARIABLES
// ═══════════════════════════════════════════════════════════════════════

const state = {
  rows: [[], []], // rows[rowIdx] = [{ id, type, left: px, params: {} }]
  cables: [],     // { fromId, fromPort, toId, toPort, color }
  nextId: 1,
  flareMidiChannel: 16,
};

let lastActiveRow = 0; // tracks which row the user last interacted with
let lastMouseX = 0;
let lastMouseY = 0;

let nodesCount = 0;
let liveUpdateTimer = null;
let isSendingLive = false;
let liveUpdatePending = false;
let skipNextLiveUpload = false; // set when a CC-mapped knob is released — hardware already has the value
let lastUploadedSnapshot = null;
let generateCodeTimer = null;

// MIDI + UI state — declared here (before live-update functions) to avoid TDZ
let midiOut = null, midiIn = null, ackWaiter = null;
let compiledSnapshot = null;
let currentCpuString = "";
let isCpuOverBudget = false;
let currentContextMenu = null;
let hoveredJack = null;
let knobConstantMap = {};
let lastGeneratedCode = '';

let stateHistory = [];
let historyIndex = -1;

function pushStateSnapshot() {
  if (historyIndex < stateHistory.length - 1) {
    stateHistory = stateHistory.slice(0, historyIndex + 1);
  }
  const snapshot = {
    rows: JSON.parse(JSON.stringify(state.rows)),
    cables: JSON.parse(JSON.stringify(state.cables)),
    nextId: state.nextId,
    flareMidiChannel: state.flareMidiChannel
  };
  stateHistory.push(snapshot);
  if (stateHistory.length > 50) {
    stateHistory.shift();
  } else {
    historyIndex++;
  }
}

function undoState() {
  if (historyIndex > 0) {
    historyIndex--;
    restoreSnapshot(stateHistory[historyIndex]);
  }
}

function redoState() {
  if (historyIndex < stateHistory.length - 1) {
    historyIndex++;
    restoreSnapshot(stateHistory[historyIndex]);
  }
}

function restoreSnapshot(snapshot) {
  state.rows = JSON.parse(JSON.stringify(snapshot.rows));
  state.cables = JSON.parse(JSON.stringify(snapshot.cables));
  state.nextId = snapshot.nextId;
  state.flareMidiChannel = snapshot.flareMidiChannel;
  clearSelection();
  rerenderRackFromState();
  generateCode();
  pushStateSnapshot();
}

function scheduleGenerateCode(delay = 35) {
  if (generateCodeTimer) clearTimeout(generateCodeTimer);
  generateCodeTimer = setTimeout(() => {
    generateCodeTimer = null;
    generateCode();
  }, delay);
}

function getConstantsMetadata(bytes) {
  const r = {
    _b: bytes,
    _p: 0,
    u8() { return this._b[this._p++]; },
    u16() { const v = this._b[this._p] | (this._b[this._p + 1] << 8); this._p += 2; return v; },
    u32() { const v = (this._b[this._p] | (this._b[this._p + 1] << 8) | (this._b[this._p + 2] << 16) | ((this._b[this._p + 3] << 24) >>> 0)); this._p += 4; return v >>> 0; },
    i32() { return this.u32() | 0; },
    str(n) { const s = String.fromCharCode(...this._b.slice(this._p, this._p + n)); this._p += n; return s; },
    pos() { return this._p; }
  };

  // Header (19 bytes)
  const magic = [r.u8(), r.u8(), r.u8(), r.u8(), r.u8()];
  const version = r.u16();
  r.u16(); // flags
  const slot_count = r.u16();
  r.u16(); // reserved
  r.u16(); // reserved
  const buffer_count = r.u8();
  const terminal_count = r.u8();
  const kernel_id_count = r.u8();
  r.u8(); // reserved

  // Kernel Registry
  for (let i = 0; i < kernel_id_count; i++) {
    const len = r.u8();
    r.str(len);
  }

  const constants = [];
  let constIdx = 0;

  // Slot Table
  for (let i = 0; i < slot_count; i++) {
    const kid = r.u8();
    const core = r.u8();
    const in_count = r.u8();
    for (let j = 0; j < in_count; j++) {
      const tag = r.u8();
      if (tag === 0 || tag === 4) { // TAG_SLOT = 0, TAG_SLOT_OUT2 = 4
        r.u16();
      } else if (tag === 1) { // TAG_BUFFER = 1
        r.u16();
      } else if (tag === 2) { // TAG_CONST_U8 = 2
        const valOffset = r.pos();
        const val = r.u8();
        constants.push({
          const_idx: constIdx++,
          tag: tag,
          byte_offset: valOffset,
          value: val,
          size: 1
        });
      } else if (tag === 3) { // TAG_CONST_I32 = 3
        const valOffset = r.pos();
        const val = r.i32();
        constants.push({
          const_idx: constIdx++,
          tag: tag,
          byte_offset: valOffset,
          value: val,
          size: 4
        });
      }
    }
    r.u16(); // out_offset
    r.u32(); // param0
  }
  return constants;
}

function getConstantValueForKnob(m, paramName, rawVal) {
  const prevVal = m.params[paramName];
  m.params[paramName] = rawVal;

  let result;
  if (m.type === 'sine' || m.type === 'triangle' || m.type === 'saw' || m.type === 'square' || m.type === 'sub-osc' || m.type === 'dx' || m.type === 'weave') {
    if (paramName === 'pitch' || paramName === 'cents') {
      const pitchKnob = getKnobValue(m.type, 'pitch', m.params.pitch ?? 1935);
      const centsVal = (m.params.cents !== undefined ? (getKnobValue(m.type, 'cents', m.params.cents) - 2048) / 204.8 : 0);
      const noteCable = state.cables.find(c => c.toId === m.id && (c.toPort === 'note' || c.toPort === 'pitch'));
      if (noteCable) {
        result = Math.round(pitchKnob - 60 + centsVal);
      } else {
        result = Math.round(pitchKnob + centsVal);
      }
    }
  }

  if (m.type === 'delay') {
    if (paramName === 'mode' || paramName === 'ratio') {
      const modeDef = MODULE_DEFS.delay.knobs.find(k => k.param === 'mode');
      const modeVal = paramName === 'mode' ? rawVal : (m.params.mode ?? 'stereo');
      let mode = 1;
      if (typeof modeVal === 'string') {
        if (modeVal === 'mono') mode = 0;
        else if (modeVal === 'stereo') mode = 1;
        else if (modeVal === 'ping-pong') mode = 2;
      } else {
        const modeIdx = Math.max(0, Math.min(modeDef.discrete.length - 1, Math.floor((modeVal / 4096) * modeDef.discrete.length)));
        const modeStr = modeDef.discrete[modeIdx];
        if (modeStr === 'mono') mode = 0;
        else if (modeStr === 'stereo') mode = 1;
        else if (modeStr === 'ping-pong') mode = 2;
      }

      const ratioRaw = paramName === 'ratio' ? rawVal : (m.params.ratio ?? 2048);
      const ratio = ratioRaw / 2048;
      const ratioScaled = Math.round(ratio * 2048);

      result = mode | (ratioScaled << 8);
    }
  }

  if (result === undefined) {
    result = getKnobValue(m.type, paramName, rawVal);
  }

  m.params[paramName] = prevVal;
  return result;
}

function rebuildKnobConstantMap() {
  knobConstantMap = {};
  if (!compiledSnapshot) return;

  const allMods = state.rows.flat();
  const baseSnapshot = new Uint8Array(compiledSnapshot);
  let baseConsts;
  try {
    baseConsts = getConstantsMetadata(baseSnapshot);
  } catch (e) {
    console.warn('Failed to parse base constants:', e);
    return;
  }

  for (const m of allMods) {
    const def = MODULE_DEFS[m.type];
    if (!def || def.isHW) continue;
    for (const k of (def.knobs || [])) {
      const paramName = k.param;
      const originalVal = m.params[paramName] ?? k.def;

      let tempVal;
      if (k.discrete) {
        let currentIdx = -1;
        if (typeof originalVal === 'string') {
          currentIdx = k.discrete.indexOf(originalVal);
        } else {
          currentIdx = Math.max(0, Math.min(k.discrete.length - 1, Math.floor((originalVal / 4096) * k.discrete.length)));
        }
        if (currentIdx === -1) currentIdx = 0;
        const nextIdx = (currentIdx + 1) % k.discrete.length;
        if (k.discrete.length === 2) {
          tempVal = nextIdx === 0 ? 0 : 4095;
        } else {
          tempVal = Math.round((nextIdx / (k.discrete.length - 1)) * 4095);
        }
      } else {
        tempVal = originalVal >= 2048 ? originalVal - 1000 : originalVal + 1000;
      }
      m.params[paramName] = tempVal;

      try {
        const code = generateCode(true); // textOnly = true
        const ast = Lens.read(code);
        const expanded = Lens.expand(ast, { loadFile: __webLoadFile });
        const lowered = Lens.lower(expanded);
        const sched = Lens.schedule(lowered);
        const tempSnapshot = Lens.encode(sched, lowered);

        if (tempSnapshot.length === baseSnapshot.length) {
          const tempConsts = getConstantsMetadata(tempSnapshot);
          if (tempConsts.length === baseConsts.length) {
            const diffs = [];
            for (let i = 0; i < baseConsts.length; i++) {
              if (baseConsts[i].value !== tempConsts[i].value) {
                diffs.push({ baseEntry: baseConsts[i], tempEntry: tempConsts[i] });
              }
            }
            if (diffs.length === 1) {
              knobConstantMap[`${m.id}.${paramName}`] = {
                const_idx: diffs[0].baseEntry.const_idx,
                byte_offset: diffs[0].baseEntry.byte_offset,
                size: diffs[0].baseEntry.size
              };
            }
          }
        }
      } catch (err) {
        console.warn(`[Knob Map] Error mapping ${m.id}.${paramName}:`, err);
      } finally {
        m.params[paramName] = originalVal;
      }
    }
  }
  console.log('[Knob Map] Rebuilt knobConstantMap:', knobConstantMap);
}

function triggerLiveUpdate() {
  if (!midiOut || !compiledSnapshot) return;
  if (isSendingLive) {
    liveUpdatePending = true;
    return;
  }
  if (liveUpdateTimer) clearTimeout(liveUpdateTimer);
  liveUpdateTimer = setTimeout(async () => {
    isSendingLive = true;
    liveUpdatePending = false;

    // CC-mapped knob release: hardware already has the live value via CC messages.
    // Accept the new snapshot (updated :init) as the baseline without a WRITE_STATE upload.
    if (skipNextLiveUpload) {
      skipNextLiveUpload = false;
      lastUploadedSnapshot = new Uint8Array(compiledSnapshot);
      isSendingLive = false;
      if (liveUpdatePending) triggerLiveUpdate();
      return;
    }

    let didUpdateConst = false;
    if (lastUploadedSnapshot && lastUploadedSnapshot.length === compiledSnapshot.length) {
      try {
        const oldConsts = getConstantsMetadata(lastUploadedSnapshot);
        const newConsts = getConstantsMetadata(compiledSnapshot);
        if (oldConsts.length === newConsts.length) {
          const diffs = [];
          for (let i = 0; i < oldConsts.length; i++) {
            if (oldConsts[i].value !== newConsts[i].value) {
              diffs.push({ oldEntry: oldConsts[i], newEntry: newConsts[i] });
            }
          }
          let allBytesMatch = true;
          for (let i = 0; i < compiledSnapshot.length; i++) {
            if (lastUploadedSnapshot[i] !== compiledSnapshot[i]) {
              allBytesMatch = false;
              break;
            }
          }

          if (allBytesMatch) {
            didUpdateConst = true;
            const statusEl = $('status');
            statusEl.textContent = getStatusPrefix() + ' · live updated!';
            statusEl.className = isCpuOverBudget ? 'err' : 'ok';
          } else if (diffs.length === 1) {
            const targetOffset = diffs[0].newEntry.byte_offset;
            const targetSize = diffs[0].newEntry.size;
            let otherBytesMatch = true;
            for (let i = 0; i < compiledSnapshot.length - 4; i++) {
              if (i >= targetOffset && i < targetOffset + targetSize) continue;
              if (lastUploadedSnapshot[i] !== compiledSnapshot[i]) {
                otherBytesMatch = false;
                break;
              }
            }
            if (otherBytesMatch && targetSize === 4) {
              const constIdx = diffs[0].newEntry.const_idx;
              const newValue = diffs[0].newEntry.value;
              const newCrc32 = compiledSnapshot[compiledSnapshot.length - 4] |
                (compiledSnapshot[compiledSnapshot.length - 3] << 8) |
                (compiledSnapshot[compiledSnapshot.length - 2] << 16) |
                ((compiledSnapshot[compiledSnapshot.length - 1] << 24) >>> 0);

              const payload = new Uint8Array(11);
              payload[0] = constIdx;
              payload[1] = newValue & 0xFF;
              payload[2] = (newValue >> 8) & 0xFF;
              payload[3] = (newValue >> 16) & 0xFF;
              payload[4] = (newValue >> 24) & 0xFF;
              payload[5] = targetOffset & 0xFF;
              payload[6] = (targetOffset >> 8) & 0xFF;
              payload[7] = newCrc32 & 0xFF;
              payload[8] = (newCrc32 >> 8) & 0xFF;
              payload[9] = (newCrc32 >> 16) & 0xFF;
              payload[10] = (newCrc32 >> 24) & 0xFF;

              midiOut.send([...Lens.frame(Lens.CMD.UPDATE_CONST, payload)]);
              const m = await recvAck();
              if (m.cmd === Lens.CMD.ACK) {
                lastUploadedSnapshot = new Uint8Array(compiledSnapshot);
                didUpdateConst = true;
                const statusEl = $('status');
                statusEl.textContent = getStatusPrefix() + ' · live updated!';
                statusEl.className = isCpuOverBudget ? 'err' : 'ok';
              }
            }
          }
        }
      } catch (e) {
        console.warn('Real-time constant update detection/execution failed, falling back to full upload:', e);
      }
    }

    if (!didUpdateConst) {
      try {
        await writeSnapshot();
        lastUploadedSnapshot = new Uint8Array(compiledSnapshot);
        const statusEl = $('status');
        statusEl.textContent = getStatusPrefix() + ' · playing live!';
        statusEl.className = isCpuOverBudget ? 'err' : 'ok';
      } catch (e) {
        console.warn('Live update failed:', e.message);
      }
    }
    isSendingLive = false;
    if (liveUpdatePending) {
      triggerLiveUpdate();
    }
  }, 100);
}
function getStatusPrefix() {
  if (!compiledSnapshot) return "";
  return `${nodesCount} nodes · ${compiledSnapshot.length} B`;
}

// ═══════════════════════════════════════════════════════════════════════
// 3. CORE UTILS
// ═══════════════════════════════════════════════════════════════════════

const $ = id => document.getElementById(id);
const el = (tag, cls, attrs) => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (attrs) Object.assign(e, attrs);
  return e;
};

function valToAngle(v) { return -135 + (Math.max(0, Math.min(4095, v)) / 4095) * 270; }

function getModuleData(id) {
  for (const row of state.rows) {
    const m = row.find(m => m.id === id);
    if (m) return m;
  }
  return null;
}
function getModuleRowIndex(id) {
  return state.rows.findIndex(row => row.some(m => m.id === id));
}

function snapToHP(val) {
  return Math.round(val / HP) * HP;
}

function findFreePosition(rowIndex, hpWidth) {
  const widthPx = hpWidth * HP;
  const rowMods = state.rows[rowIndex] || [];
  let candidate = 0;
  while (true) {
    let clash = false;
    for (const m of rowMods) {
      const def = MODULE_DEFS[m.type];
      const mWidth = (def ? def.hp : 6) * HP;
      const left = m.left || 0;
      if (candidate < left + mWidth && candidate + widthPx > left) {
        clash = true;
        candidate = snapToHP(left + mWidth + 15);
        break;
      }
    }
    if (!clash) return candidate;
  }
}

// Convert semitones/MIDI notes to clean readable text (e.g. C4, A#3)
function formatNote(val) {
  const noteNum = Math.round(val);
  const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  let idx = noteNum % 12;
  if (idx < 0) idx += 12;
  const octave = Math.floor(noteNum / 12) - 1;
  const noteName = names[idx];
  return `${noteName}${octave} (${noteNum})`;
}

// Helper to get formatted or snap value of a parameter
function scaleNote(rawVal) {
  return Math.round((rawVal / 4095) * 127);
}

function hzToRateCode(hz) {
  const lo = 0.05, hi = 20000, n = 256;
  let best = 0, bestErr = Infinity;
  for (let b = 0; b < n; b++) {
    const freq = lo * Math.pow(hi / lo, b / (n - 1));
    const err = Math.abs(freq - hz);
    if (err < bestErr) { bestErr = err; best = b; }
  }
  return best << 4;
}

function scaleBpm(rawVal) {
  return Math.round(40 + (rawVal / 4095) * 200);
}

// Convert a BPM value to the raw 0..4095 ':tempo' parameter that produces
// exactly that frequency. The runtime uses:
//   rate_table[32 + (v * 94) >> 12]  where  rate_table[b] = 0.05*(400000)^(b/255) Hz
// Inverted: b = 255 * ln(bpm/3) / ln(400000),  v = round(4096*(b-32)/94)
function bpmToTempoRaw(bpm) {
  const LOG_RANGE = Math.log(400000); // ln(400000) ≈ 12.899
  const b = 255 * Math.log(Math.max(bpm, 1) / 3) / LOG_RANGE;
  return Math.max(0, Math.min(4095, Math.round(4096 * (b - 32) / 94)));
}

function getKnobValue(type, paramName, rawVal) {
  if (rawVal === undefined || rawVal === null || Number.isNaN(rawVal)) {
    return undefined;
  }
  if (type === 'pluck' && paramName === 'damp') {
    return 4095 - rawVal;
  }
  if (type === 'dx' && paramName === 'decay') {
    return 4096 - rawVal;
  }
  const def = MODULE_DEFS[type];
  if (!def) return rawVal;
  const kDef = def.knobs.find(k => k.param === paramName);
  if (kDef && kDef.discrete) {
    const idx = Math.max(0, Math.min(kDef.discrete.length - 1, Math.floor((rawVal / 4096) * kDef.discrete.length)));
    return kDef.discrete[idx];
  }
  if (paramName === 'pitch' || paramName === 'note') {
    return scaleNote(rawVal);
  }
  if (paramName === 'bpm') {
    return scaleBpm(rawVal);
  }
  return rawVal;
}

/**
 * Like getKnobValue, but returns a Lisp expression STRING suitable for use in
 * generated code. If the knob is mapped to a MIDI CC, the expression will be
 * `(midi-cc CC :init VAL)`.  For pitch/note parameters (0..127 range expected
 * by the VM), it wraps in `(spread (...) 128)` so the 0..4095 CC range is
 * scaled back down automatically.  Otherwise returns `String(numericValue)`.
 *
 * @param {object} m       - module data object (has m.type, m.params)
 * @param {string} paramName
 * @param {number} rawVal  - raw 0..4095 knob value
 * @returns {string}
 */
function formatMidiCcExpr(entry, initVal) {
  const ccNum = (typeof entry === 'object') ? entry.cc : entry;
  const ch = (typeof entry === 'object') ? entry.ch : 1;
  const chSuffix = ch !== 1 ? ` :ch ${ch}` : '';
  return `(midi-cc ${ccNum} :init ${initVal}${chSuffix})`;
}

function getKnobValueExpr(m, paramName, rawVal) {
  const cc = m.params.__midi_cc && m.params.__midi_cc[paramName];
  const numericVal = getKnobValue(m.type, paramName, rawVal);

  if (cc === undefined || cc === null) {
    return String(numericVal);
  }

  // Pitch/note params use a 0..127 range inside the VM.
  // Use (spread (midi-cc CC :init INIT) 128) to map 0..4095 → 0..127.
  const isPitchParam = paramName === 'pitch' || paramName === 'note';
  const initVal = rawVal ?? 2048;

  if (isPitchParam) {
    return `(spread ${formatMidiCcExpr(cc, initVal)} 128)`;
  }
  return formatMidiCcExpr(cc, initVal);
}


const DX7_PRESETS = {
  0: ["SOLID BASS", "S.BAS 27.7", "LeaderTape", "ANALOG  4", "ANALOG  6", "GASHAUS", "WINTRHODES", "*Mark III", "SYNDM 25.8", "JX-33-P", "Etherial5a", "ICE PAD  2", "M1 PADS", "Bounce 4", "CARLOS   2", "'Airy'", "SOFT TOUCH", "CIRRUS", "ENTRIX", "BORON A", "Textures 6", "Mooger Low", "*Hammond 1", "Mooger Low", "Mooger Low", "Mooger Low", "Mooger Low", "Mooger Low", "Mooger Low", "Mooger Low", "Mooger Low", "Mooger Low"],
  1: ["PICCOLO", "FLUTE   2", "OBOE", "CLARINET", "SAX BC", "BASSOON", "STRINGS 4", "STRINGS 5", "STRINGS 6", "STRINGS 7", "STRINGS 8", "BRASS   4", "BRASS   5", "BRASS 6 BC", "BRASS   7", "BRASS   8", "RECORDER", "HARMONICA1", "HRMNCA2 BC", "VOICE   2", "VOICE   3", "GLOKENSPL", "VIBE    2", "XYLOPHONE", "CHIMES", "GONG    1", "GONG    2", "BELLS", "COW BELL", "BLOCK", "FLEXATONE", "LOG DRUM"],
  2: ["BRASS   1", "BRASS   2", "BRASS   3", "STRINGS 1", "STRINGS 2", "STRINGS 3", "ORCHESTRA", "PIANO   1", "PIANO   2", "PIANO   3", "E.PIANO 1", "GUITAR  1", "GUITAR  2", "SYN-LEAD 1", "BASS    1", "BASS    2", "E.ORGAN 1", "PIPES   1", "HARPSICH 1", "CLAV    1", "VIBE    1", "MARIMBA", "KOTO", "FLUTE   1", "ORCH-CHIME", "TUB BELLS", "STEEL DRUM", "TIMPANI", "REFS WHISL", "VOICE   1", "TRAIN", "TAKE OFF"]
};

function getDisplayValueStr(type, paramName, rawVal, instanceId) {
  const def = MODULE_DEFS[type];
  if (!def) return '';
  const kDef = def.knobs.find(k => k.param === paramName);
  if (!kDef) return '';

  if (type === 'audio' && paramName === 'time') {
    let unit = 'secs';
    if (instanceId) {
      const mData = getModuleData(instanceId);
      if (mData) {
        const unitDef = def.knobs.find(k => k.param === 'unit');
        const unitRaw = mData.params.unit ?? 0;
        const unitIdx = Math.max(0, Math.min(unitDef.discrete.length - 1, Math.floor((unitRaw / 4096) * unitDef.discrete.length)));
        unit = unitDef.discrete[unitIdx];
      }
    }
    if (unit === 'smpls') {
      const samples = Math.max(1, Math.round((rawVal / 4095) * 4096));
      return `${samples} smpls`;
    } else {
      const seconds = 0.05 + (2.0 - 0.05) * (rawVal / 4095);
      return `${seconds.toFixed(2)} s`;
    }
  }

  if (type === 'dx' && paramName === 'preset') {
    let bankIdx = 0;
    if (instanceId) {
      const mData = getModuleData(instanceId);
      if (mData) {
        const bankRaw = mData.params.bank ?? 0;
        bankIdx = Math.max(0, Math.min(2, Math.floor((bankRaw / 4096) * 3)));
      }
    }
    const voiceList = DX7_PRESETS[bankIdx] || [];
    const voiceIdx = Math.max(0, Math.min(voiceList.length - 1, Math.floor((rawVal / 4096) * voiceList.length)));
    return voiceList[voiceIdx] || '';
  }

  if (kDef.discrete) {
    const idx = Math.max(0, Math.min(kDef.discrete.length - 1, Math.floor((rawVal / 4096) * kDef.discrete.length)));
    const val = kDef.discrete[idx];
    if (kDef.param.startsWith('vol')) {
      return `${val}`;
    } else if (kDef.param === 'n' && type === 'every') {
      return `/${val}`;
    }
    return `${val}`;
  } else {
    if (kDef.param === 'pitch' || kDef.param === 'note') {
      if (['sine', 'triangle', 'saw', 'square'].includes(type) && instanceId) {
        const mData = getModuleData(instanceId);
        if (mData) {
          const rangeDef = def.knobs.find(k => k.param === 'range');
          if (rangeDef) {
            const rangeRaw = mData.params.range ?? 0;
            const rangeIdx = Math.max(0, Math.min(rangeDef.discrete.length - 1, Math.floor((rangeRaw / 4096) * rangeDef.discrete.length)));
            if (rangeDef.discrete[rangeIdx] === 'lfo') {
              const idx = 32 + Math.floor((rawVal * 94) / 4096);
              const hz = 0.05 * Math.pow(400000, idx / 255);
              return hz < 1000 ? `${hz.toFixed(2)} Hz` : `${(hz / 1000).toFixed(2)} kHz`;
            }
          }
        }
      }
      return formatNote(scaleNote(rawVal));
    } else if (kDef.param === 'rate' && type === 'lfo') {
      const idx = 32 + Math.floor((rawVal * 94) / 4096);
      const hz = 0.05 * Math.pow(400000, idx / 255);
      return hz < 1000 ? `${hz.toFixed(2)} Hz` : `${(hz / 1000).toFixed(2)} kHz`;
    } else if (kDef.param === 'bpm' || (kDef.param === 'speed' && type === 'score-player')) {
      const bpm = kDef.param === 'bpm' ? scaleBpm(rawVal) : (40 + Math.round((rawVal / 4095) * 200));
      return `${bpm} BPM`;
    } else if (kDef.param === 'hz' && type === 'lfo-delay') {
      const idx = 32 + Math.floor((rawVal * 94) / 4096);
      const hz = 0.05 * Math.pow(400000, idx / 255);
      return hz < 1000 ? `${hz.toFixed(2)} Hz` : `${(hz / 1000).toFixed(2)} kHz`;
    } else if (kDef.param === 'hz') {
      return `${rawVal} Hz`;
    } else if (kDef.param === 'cents') {
      const semitones = (rawVal - 2048) / 204.8;
      return semitones === 0 ? '0.0 ST' : `${semitones > 0 ? '+' : ''}${semitones.toFixed(1)} ST`;
    } else if (kDef.param === 'ch') {
      return rawVal === 0 ? 'OMNI' : `CH ${rawVal}`;
    } else if (type === 'gain' && paramName === 'gain') {
      const mult = (rawVal / 512).toFixed(2);
      return `${mult}x`;
    } else if (type === 'constant' && paramName === 'val') {
      return `${rawVal}`;
    } else {
      return `${Math.round(rawVal / 40.95)}%`;
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════
// 4. DOM BUILDER & TOOLTIP DICTIONARIES
// ═══════════════════════════════════════════════════════════════════════

const MODULE_DESCRIPTIONS = {
  // Oscillators
  'sine': 'Sine Wave VCO. Generates a pure, smooth frequency tone.',
  'triangle': 'Triangle Wave VCO. Generates a flute-like tone with odd harmonics.',
  'saw': 'Sawtooth Wave VCO. Generates a bright, buzzy brassy tone rich in harmonics.',
  'square': 'Square Wave VCO. Generates a hollow, clarinet-like tone with variable pulse width.',
  'sub-osc': 'Sub-Oscillator VCO. Generates a main saw wave and sub-octave saw waves (-12, -24 semitones).',
  'noise': 'White/Pink Noise Generator. Useful for percussion, wind, and sound effects.',
  'phasor': 'Phasor LFO. Generates a slow rising ramp phase signal.',
  'lfo': 'Low Frequency Oscillator. Generates slow modulation waves (sine, triangle, saw, square).',
  'lfo-delay': 'Delayed LFO. Generates a vibrato-style LFO that fades in slowly over time.',
  'wt': 'Wavetable Oscillator. Reads single-cycle wavetables from tape with morphing phase/position.',
  'weave': 'Weave Macro Oscillator. A multi-model synth voice with 48 distinct algorithms (Braids port).',

  // Filters / Processing
  'lpf': 'Low-pass VCF. Filters out high frequencies above the cutoff point.',
  'hpf': 'High-pass VCF. Filters out low frequencies below the cutoff point.',
  'vcf': 'State Variable Filter. Exposes simultaneous Low-pass, High-pass, Band-pass, and Notch outputs.',
  'lpf2': '2-Pole Low-pass filter. 12dB/oct low-pass filter with resonant boost.',
  'hpf2': '2-Pole High-pass filter. 12dB/oct high-pass filter with resonant boost.',
  'bpf2': '2-Pole Band-pass filter. 12dB/oct band-pass filter with resonant boost.',
  'lpg': 'Low-pass Gate. Combines a low-pass filter and VCA to simulate natural organic acoustic resonance.',
  'slew': 'Slew Limiter. Smooths out sudden jumps in control voltage (lag/glide generator).',
  'average': 'Average. Smooths high-frequency signal chatter (one-pole filter).',
  'envfollow': 'Envelope Follower. Extracts the dynamic volume envelope from an audio source.',

  // VCAs / Mixing / Math
  'vca': 'Voltage Controlled Amplifier. Controls the amplitude/volume of audio or CV signals.',
  'quad-vca': 'Quad VCA. Four independent VCAs with level controls and cascade summing.',
  'mix': '4-Channel Audio/CV Mixer. Sums four inputs with individual volume controls.',
  'stereo-mixer': '4-Channel Stereo Mixer. Features individual Level, Pan, and Mute controls, and Master Volume output.',
  'ring': 'Ring Modulator. Multiplies two inputs to generate metallic sideband frequencies.',
  'wavefold': 'Wave Folder. Folds waveform peaks back inward to add rich odd harmonics.',
  'saturate': 'Saturator. Cubic soft-clipper with drive, bias, wet/dry mix, and makeup gain.',
  'shape': 'Shaper LUT. Non-linear waveshaping distortion with selection curves and oversampling.',
  'crush': 'Bit Crusher. Reduces sample rate and bit depth for retro digital lo-fi grit.',
  'add': 'CV Adder. Sums two CV signals with optional clipping protection.',
  'mul': 'Attenuator. Scales a signal using VCA multiplication.',
  'attenuverter': 'Attenuverter. Scales (attenuates) and/or inverts a CV signal, with a bipolar offset knob.',
  'constant': 'Constant Value. Outputs up to three static, adjustable CV offset voltages.',
  'gain': 'Gain / Scale. Amplifies or attenuates input signals using the amplify helper.',
  'morph': 'Morph Scanner. Crossfades between two signals based on a morph parameter.',
  'mult': 'Signal Splitter (Multiple). Splits a single input signal to three outputs.',
  'signal-switch': '3-Way Switch. Selects DN, MID, or UP input signals based on a switch or CV.',
  'seq-switch': 'Sequential Switch. Loops through up to 8 input lines on clock trigger.',
  'logic': 'Logic Gate. Computes boolean operations (AND, OR, XOR, NOT) on inputs.',
  'math': 'Math Unit. Performs basic math operations (addition, subtraction, multiplication, division) on inputs.',
  'cv-pitch': 'CV to Pitch. Scales a full-range 0-4095 CV signal down into a selectable range of V/Oct pitch notes.',
  'quantizer': 'Scale Quantizer. Snaps input pitch CV to a selected musical scale.',
  'transpose': 'Transpose. Transposes incoming note/pitch CV up or down by semitones.',

  // Envelopes / Control
  'envelope': 'Decay Envelope. Simple trigger-fired decay envelope generator (for hits/plucks).',
  'adsr': 'ADSR Envelope Generator. Shapes amplitude/filter sweeps over Time (Attack, Decay, Sustain, Release).',
  'gate': 'Gate Generator. Compares input signal to threshold to trigger a gate pulse.',
  'schmitt': 'Comparator. Compares input to high/low thresholds to output boolean gate.',
  'toggle': 'Toggle Flip-Flop. Alternates state on rising trigger edges.',
  'walk': 'Random Walk. Outputs stepping random CV offsets on clock trigger.',
  'turns': 'Turns Router. Routes signals based on counting clock cycles.',
  'shift-register': 'Shift Register. Delays step signals sequentially across 4 outputs on clock trigger.',
  'chance': 'Chance Gate. Randomly passes trigger/gate signals with a certain probability (PROB).',
  'random': 'Random S&H. Outputs a random CV value (0..4095) on every clock trigger.',
  'hold': 'Sample & Hold. Samples the input signal on clock (CLK) or tracks when gate is active (GATE).',
  'edge': 'Edge Detector. Outputs a brief trigger pulse on the rising/falling edge of the input.',
  'if-gate': 'If Gate. Selects between THEN and ELSE inputs based on COND.',

  // Sequencers / Clock
  'clock': 'Clock Generator. Master tempo source with subdivided outputs.',
  'multi-div': 'Clock Divider. Outputs divided clocks (/2, /4, /8, /16).',
  'rhythm': 'Rhythm Player. Generates triggers, gates, or hits from standard drum patterns.',
  'turing': 'Turing Machine. An evolving shift register sequencer with random mutation.',
  'step-seq': 'Step Sequencer. Loops through up to 8 pitch/value step sliders.',
  'drum-seq': '16-Step Drum Sequencer. Visual gate sequencer for Kick, Snare, Hat, and Percussion.',
  'every': 'Clock Divider. Fires a trigger once every N input pulses.',
  'counter': 'Counter. Counts trigger pulses modulo STEPS, outputting current count and individual step gates.',
  'euclid': 'Euclidean Rhythm Generator. Generates Euclidean trigger patterns.',
  'trig': 'Trig Delay. Delays trigger/gate pulses by a specific duration.',
  'score-player': 'Score Player. Reads note arrays to trigger melody CV and gates.',

  // Tapes / Memory
  'tape': 'Data Tape. Storage cartridge for Loupe S-expressions and pattern data sequences.',
  'expression': 'Formula Eval. Compiles and executes a single-line S-expression formula on inputs A & B.',
  'computer': 'Computer Module. Runs custom Loupe S-expression scripts for full digital control.',
  'audio': 'Audio Buffer. Declares a blank audio memory tape for delays and loopers.',
  'cv-looper': 'CV Looper. Records CV automation loops live on clock trigger.',
  'tape-delay': 'Tape Looper. Records audio loops on gate, with play speed control and sound-on-sound overdubbing.',
  'tape-looper': 'Tape Sampler. Records samples on gate, playing back in loop or one-shot mode triggered on pulse.',
  'tape-write': 'Tape Writer. Writes values into a tape at specific indexes on clock trigger.',
  'tape-record': 'Continuous Writer. Records incoming values continuously into a tape loop.',
  'tap': 'Tape Tap. Reads a delayed tap from a tape buffer (delay line).',
  'feedback-cell': 'Sample Delay. Delays the input signal by exactly 1 sample to break feedback loop dependencies.',
  'len': 'Tape Length. Outputs the size/length of a cabled tape or audio buffer.',
  'step': 'Step Reader. Sweeps through tape elements sequentially on each clock trigger.',
  'seek': 'Seek Reader. Reads a tape element at a specific index offset on trigger.',
  'lookup': 'Index Lookup. Reads a tape element at a continuous index position.',
  'wave-scanner': 'Wave Scanner. Synthesizes wavetable voices from tape arrays using V/Oct pitch.',

  // Drums / Audio Effects
  'kick': 'Kick Synth. Voltage-controlled kick drum generator with sweep and drive.',
  'snare': 'Snare Synth. Analog-style snare drum voice with snappy noise.',
  'hat': 'Hi-Hat. Metallic hi-hat voice with decay and tone.',
  'pluck': 'Plucked String. Physical modeling plucked voice (Karplus-Strong).',
  'dx': 'DX FM Voice. 6-operator FM synth voice (DX7 emulator) reading preset banks.',
  'rungler': 'Rungler. A dual-oscillator engine with Rungler output (Runge-Kutta).',
  'granular': 'Granular Scatter (Clouds). Scatters audio into dense grains with pitch shifting and reverb.',
  'delay': 'Digital Delay. Adds echoes and space with adjustable feedback and time.',
  'reverb': 'Stereo Reverb. Simulates large acoustic spaces with adjustable size and dampening.',
  'chorus': 'Chorus. Modulates delay time slowly to create a rich, detuned ensemble sound.',
  'flanger': 'Flanger. Modulates short delay time with feedback to create jet-like comb-filtering sweeps.',
  'compressor': 'Compressor. Dynamically scales signal level above a threshold to control dynamic range.',

  // Hardware I/O
  'ws-in': 'HW Inputs. Bridges host keyboard knobs, switch rail, and physical CV/Pulse inputs.',
  'ws-out': 'HW Outputs. Routes audio, CV, pulse triggers, and LEDs back to the physical hardware/host.',

  // MIDI I/O
  'midi-note': 'MIDI Keyboard. Receives pitch, gate, pressure, and velocity from host MIDI input.',
  'midi-cc': 'MIDI CC In. Receives continuous controller (CC) modulation from host MIDI.',
  'midi-trig': 'MIDI Note Trig. Outputs trigger pulses on host MIDI notes.',
  'midi-clock': 'MIDI Clock In. Synchronizes to external DAW/host clock.',
  'midi-note-out': 'MIDI Keyb Out. Sends MIDI notes to external DAW/synthesizers.',
  'midi-cc-out': 'MIDI CC Out. Sends MIDI CC modulation to external DAW/synthesizers.',
  'midi-clock-out': 'MIDI Clock Out. Routes clock signals out to external host MIDI clock.',
  'midi-score': 'MIDI Recorder. Records MIDI performances and loops/plays them back.',
  'midi-sync': 'MIDI Sync. Outputs MIDI clock triggers and running transport state.',
};

const CONTROL_DESCRIPTIONS = {
  'attack': 'Attack Time: Control duration to reach peak envelope level.',
  'decay': 'Decay Time: Control duration to fall to sustain level.',
  'sustain': 'Sustain Level: Control target level while gate is held.',
  'release': 'Release Time: Control decay duration after gate is released.',
  'pitch': 'Coarse Pitch Tuning: Adjust base frequency / octave.',
  'cents': 'Fine Pitch Tuning: Minor adjustment of pitch frequency.',
  'fm': 'FM Modulation Depth: Control frequency modulation sensitivity.',
  'range': 'Range Selection: Toggle between Audio and Low-Frequency rate.',
  'rate': 'LFO Rate: Adjust modulation speed frequency.',
  'cut': 'Cutoff Frequency: Adjust filter corner frequency.',
  'res': 'Resonance: Boost frequencies around cutoff frequency.',
  'gain': 'VCA Gain: Control output signal amplification level.',
  'drive': 'Drive/Saturation: Add harmonic distortion/saturation.',
  'fold': 'Wave Folding Amount: Adjust waveform folding depth.',
  'seconds': 'Buffer Duration: Total length of buffer tape.',
  'amount': 'Mix / Feedback Depth: Amount of delayed signal fed back.',
  'steps': 'Active Steps: Number of steps in loop sequence.',
  'len': 'Loop Steps Limit: Loop step duration.',
  'dir': 'Direction: Sequencer playback direction (forward, backward, random).',
  'lock': 'Loop Lock: Lock shift register contents to loop, or run free.',
  'bpm': 'BPM: Set master tempo in beats per minute.',
  'width': 'Width: Set pulse width or stereo spread.',
  'panA': 'Pan A: Adjust panning for input channel A.',
  'panB': 'Pan B: Adjust panning for input channel B.',
  'panC': 'Pan C: Adjust panning for input channel C.',
  'panD': 'Pan D: Adjust panning for input channel D.',
  'muteA': 'Mute A: Toggle mute state for input channel A.',
  'muteB': 'Mute B: Toggle mute state for input channel B.',
  'muteC': 'Mute C: Toggle mute state for input channel C.',
  'muteD': 'Mute D: Toggle mute state for input channel D.',
  'volA': 'Volume A: Adjust volume level for input channel A.',
  'volB': 'Volume B: Adjust volume level for input channel B.',
  'volC': 'Volume C: Adjust volume level for input channel C.',
  'volD': 'Volume D: Adjust volume level for input channel D.',
  'master': 'Master Volume: Set master stereo output level.',
  'density': 'Density: Speed of grain generation (clock rate).',
  'scatter': 'Scatter: Control random offset of grains in time.',
  'size': 'Size: Set the length of each grain.',
  'spread': 'Spread: Control spatial stereo distribution of grains.',
  'feedback': 'Feedback: Set echo regeneration feedback amount.',
  'mix': 'Dry/Wet Mix: Dry/Wet balance.',
  'timbre': 'Timbre: Control primary harmonic profile of the voice.',
  'color': 'Color: Adjust secondary timbre or filter characteristic.',
  'model': 'Model: Select synthesis algorithm model.',
  'speed': 'Play Speed: Set playback speed and tape direction.',
  'loop': 'Loop Mode: Choose between repeating loop and one-shot playback.',
  'pos': 'Position: Adjust lookup position or phase offset.',
  'cc': 'MIDI CC Number: Select continuous controller mapping number.',
  'ch': 'MIDI Channel: Set target MIDI channel (1..16).',
  'preset': 'Preset: Select bank preset index (0..31).',
  'bank': 'Bank: Select active voice preset bank (0..2).',
  'val1': 'Value 1: Set output voltage or step slider value.',
  'val2': 'Value 2: Set output voltage or step slider value.',
  'val3': 'Value 3: Set output voltage or step slider value.',
  'val4': 'Value 4: Set output voltage or step slider value.',
  'val5': 'Value 5: Set output voltage or step slider value.',
  'val6': 'Value 6: Set output voltage or step slider value.',
  'val7': 'Value 7: Set output voltage or step slider value.',
  'val8': 'Value 8: Set output voltage or step slider value.',
  'n': 'Divisor: Set division divisor or counting channels.',
  'bars': 'Steps: Set step wrap modulo for counting.',
  'pulses': 'Pulses: Set active trigger hits in Euclidean loop.',
};

const JACK_DESCRIPTIONS = {
  'in': 'Signal Input: Connect incoming Audio or CV signal.',
  'in1': 'Input 1: Connect source signal.',
  'in2': 'Input 2: Connect source signal.',
  'in3': 'Input 3: Connect source signal.',
  'in4': 'Input 4: Connect source signal.',
  'v/oct': 'V/Oct Pitch Input: Connect pitch CV for tracking.',
  'fm': 'FM Input: Connect CV to modulate frequency.',
  'trig': 'Trigger/Clock Input: Connect gate pulse to advance step.',
  'gate': 'Gate Input: Hold active to run or open envelope.',
  'clock': 'Clock Input: Sync step advances.',
  'sync': 'Sync Input: Reset oscillator phase.',
  'ctrl': 'Control Voltage Input: Modulate parameter.',
  'out': 'Signal Output: Route output Audio or CV signal.',
  'out1': 'Output 1: Route output signal.',
  'out2': 'Output 2: Route output signal.',
  'out3': 'Output 3: Route output signal.',
  'out4': 'Output 4: Route output signal.',
  'kick': 'Kick Output: Route kick trigger/drum pulse.',
  'snare': 'Snare Output: Route snare trigger/drum pulse.',
  'hat': 'Hi-Hat Output: Route hi-hat trigger/drum pulse.',
  'perc': 'Percussion Output: Route percussion trigger/drum pulse.',
  'a': 'Input A: Connect first operand/gate/switch source.',
  'b': 'Input B: Connect second operand/gate/switch source.',
  'c': 'Input C: Connect third switch source or operand.',
  'd': 'Input D: Connect fourth operand.',
  'inL': 'Left Input: Left channel audio input.',
  'inR': 'Right Input: Right channel audio input.',
  'outL': 'Left Output: Left channel audio output.',
  'outR': 'Right Output: Right channel audio output.',
  'pitch': 'Pitch: Connect V/Oct pitch signal.',
  'pitch1': 'Pitch 1: Connect pitch CV for VCO 1.',
  'pitch2': 'Pitch 2: Connect pitch CV for VCO 2.',
  'rungle': 'Rungle: Evolving pseudo-random stepped voltage output.',
  'runglesm': 'Smooth Rungle: Smoothed (slew-limited) rungle output.',
  'hz': 'Rate: Frequency control voltage input.',
  'fade': 'Fade: Fade duration control voltage input.',
  'pos': 'Position: Wavetable position or morph scan location.',
  'pm': 'Phase Modulation: Connect signal for phase modulation.',
  'pwm': 'Pulse Width Modulation: Connect signal to modulate PW.',
  'amp': 'VCA Amplitude: Control voltage to scale VCA gain.',
  'cvA': 'CV A: Gain control voltage for channel A.',
  'cvB': 'CV B: Gain control voltage for channel B.',
  'cvC': 'CV C: Gain control voltage for channel C.',
  'cvD': 'CV D: Gain control voltage for channel D.',
  'inA': 'Input A: Audio/CV source for channel A.',
  'inB': 'Input B: Audio/CV source for channel B.',
  'inC': 'Input C: Audio/CV source for channel C.',
  'inD': 'Input D: Audio/CV source for channel D.',
  'outA': 'Output A: Outputs signal from VCA/channel A.',
  'outB': 'Output B: Outputs signal from VCA/channel B.',
  'outC': 'Output C: Outputs signal from VCA/channel C.',
  'outD': 'Output D: Outputs signal from VCA/channel D.',
  'rec': 'Record Gate: Hold high to record/overdub onto tape.',
  'reset': 'Reset: Pulse trigger to reset playhead/counter to start.',
  'cond': 'Condition: Control voltage to select switch path.',
  'then': 'Then Path: Value returned when COND is high.',
  'else': 'Else Path: Value returned when COND is low.',
  'at': 'Index: Specific position/index to lookup on tape.',
  'val': 'Value: Value/signal to write or route.',
  'when': 'Write Gate: Hold high to allow tape writes.',
  'tape': 'Tape: Connect tape cartridge memory line.',
  'freeze': 'Freeze: Hold high to capture and loop current grains.',
  'density': 'Density: Speed CV input for grain generation.',
  'scatter': 'Scatter: Delay dispersion CV input for grains.',
  'size': 'Size: Grain length CV input.',
  'spread': 'Spread: Stereo spread width CV input.',
  'timbre': 'Timbre: Tone control voltage input.',
  'color': 'Color: Secondary timbre control voltage input.',
  'model': 'Model Selection: Control voltage to select model.',
  'speed': 'Play Speed: Pitch/speed control voltage input.',
  'decay': 'Decay: Envelope decay duration CV input.',
  'snappy': 'Snappiness: Noise amplitude/snare snap CV input.',
  'tone': 'Tone: FM filter tone CV input.',
  'cc': 'MIDI CC: CC value output.',
  'ch': 'MIDI Channel: Target output channel.',
  'note': 'MIDI Note: Output V/Oct pitch note.',
  'count': 'Step Count: Current index count output.',
  'rise': 'Rise Trigger: Fires trigger pulse on rising edges.',
  'fall': 'Fall Trigger: Fires trigger pulse on falling edges.',
  'and': 'AND Output: Boolean AND result of inputs A & B.',
  'or': 'OR Output: Boolean OR result of inputs A & B.',
  'xor': 'XOR Output: Boolean XOR result of inputs A & B.',
  'not': 'NOT Output: Inverted boolean state of input A.',
  'add': 'Add: Output summed signal (A + B).',
  'sub': 'Subtract: Output difference signal (A - B).',
  'mul': 'Multiply: Output multiplied signal (A * B).',
  'div': 'Divide: Output divided signal (A / B).',
  'mod': 'Modulo: Output remainder of A / B.',
  'abs': 'Absolute: Output absolute value of input A.',
  'min': 'Minimum: Output smaller value of inputs A & B.',
  'max': 'Maximum: Output larger value of inputs A & B.',
  'div2': 'Divide by 2: Outputs clock pulse every 2 ticks.',
  'div3': 'Divide by 3: Outputs clock pulse every 3 ticks.',
  'div4': 'Divide by 4: Outputs clock pulse every 4 ticks.',
  'div8': 'Divide by 8: Outputs clock pulse every 8 ticks.',
  'clk': 'Clock Source: Connect source clock pulse.',
  'play': 'Transport Play: Bipolar transport gate signal.',
  'run': 'Transport Run: Bipolar transport run signal.',
};

function buildModuleEl(type, instanceId, params, leftPx) {
  const def = MODULE_DEFS[type];
  if (!def) return null;

  const hasKnobs = (def.knobs && def.knobs.length > 0) || type === 'wave-draw';
  const isIO = def.category === 'io';
  const mod = el('div', 'module' + (isIO ? ' mod-io' : '') + (hasKnobs ? '' : ' mod-knobless') + ` mod-${def.hp}hp` + (def.hp === 2 ? ' mod-2hp' : ''));
  mod.id = `mod-${instanceId}`;
  mod.dataset.instanceId = instanceId;
  mod.dataset.type = type;
  mod.dataset.category = def.category; // Tag category for visual panel variations
  mod.style.width = (def.hp * HP) + 'px';
  mod.style.left = leftPx + 'px';

  const modDesc = MODULE_DESCRIPTIONS[type] || 'Modular synthesizer unit.';
  mod.dataset.tooltip = `<strong>${def.title}</strong><br><span style="color:#aaa">${modDesc}</span>`;

  // Screws: slim modules (HP < 6) only get 2 screws per Eurorack standard
  const isSlim = def.hp < 6;
  const screws = isSlim ? ['tl', 'br'] : ['tl', 'tr', 'bl', 'br'];
  for (const pos of screws) {
    const screwEl = el('div', `screw screw-${pos}`);
    const randRot = Math.floor(Math.random() * 360);
    screwEl.style.transform = `rotate(${randRot}deg)`;
    mod.appendChild(screwEl);
  }

  // Header Title on faceplate (positioned below top screws to avoid overlap)
  const title = el('div', 'module-title');
  title.style.cursor = 'pointer';
  if (params.customName) {
    title.textContent = params.customName;
    title.title = `${def.title} (${params.customName})`;
  } else if (type === 'dx' && params.customVoiceName) {
    title.textContent = `DX: ${params.customVoiceName}`;
    title.title = `DX FM Voice: ${params.customVoiceName}`;
  } else {
    title.textContent = def.title;
    title.title = def.title;
  }

  title.addEventListener('dblclick', e => {
    e.stopPropagation();
    const oldName = params.customName || def.title;
    const newName = prompt('Enter custom name for this module:', oldName);
    if (newName !== null) {
      const trimmed = newName.trim();
      const mData = getModuleData(instanceId);
      if (mData) {
        if (trimmed && trimmed !== def.title) {
          mData.params.customName = trimmed;
          title.textContent = trimmed;
          title.title = `${def.title} (${trimmed})`;
        } else {
          delete mData.params.customName;
          title.textContent = def.title;
          title.title = def.title;
        }
        generateCode();
      }
    }
  });

  // Selected module pointerdown registration
  mod.addEventListener('pointerdown', e => {
    if (e.shiftKey) {
      e.stopPropagation();
      if (selectedModuleIds.has(instanceId)) {
        selectedModuleIds.delete(instanceId);
        $(`mod-${instanceId}`)?.classList.remove('selected');
      } else {
        selectedModuleIds.add(instanceId);
        $(`mod-${instanceId}`)?.classList.add('selected');
      }
    } else {
      if (!selectedModuleIds.has(instanceId)) {
        selectModule(instanceId);
      }
    }
  });

  mod.appendChild(title);
  setupModuleDrag(mod, mod, instanceId);

  // Weave Custom Panel: Model selector dropdown on top
  if (type === 'weave') {
    const dropdownWrap = el('div', 'module-dropdown-wrap');
    dropdownWrap.style.padding = '0 8px 6px 8px';
    dropdownWrap.style.width = '100%';
    dropdownWrap.style.boxSizing = 'border-box';

    const select = el('select', 'module-dropdown-select');
    select.style.width = '100%';
    select.style.background = '#151515';
    select.style.color = '#eee';
    select.style.border = '1px solid #333';
    select.style.borderRadius = '3px';
    select.style.fontSize = '10px';
    select.style.fontWeight = 'bold';
    select.style.padding = '3px';
    select.style.height = '22px';
    select.style.outline = 'none';
    select.style.fontFamily = 'monospace';
    select.style.cursor = 'pointer';

    // Populate options
    const modelDef = def.knobs.find(k => k.param === 'model');
    const currentVal = params.model !== undefined ? params.model : modelDef.def;
    
    let currentIdx = 0;
    if (typeof currentVal === 'string') {
      currentIdx = modelDef.discrete.indexOf(currentVal);
    } else {
      currentIdx = Math.max(0, Math.min(modelDef.discrete.length - 1, Math.floor((currentVal / 4096) * modelDef.discrete.length)));
    }
    if (currentIdx === -1) currentIdx = 0;

    modelDef.discrete.forEach((optName, idx) => {
      const opt = el('option', '');
      opt.value = idx;
      opt.textContent = optName;
      if (idx === currentIdx) {
        opt.selected = true;
      }
      select.appendChild(opt);
    });

    select.addEventListener('change', (e) => {
      const idx = parseInt(e.target.value, 10);
      // Store center-of-bucket: (idx + 0.5) / length * 4096.
      // If we store idx/length*4096 (bucket start), recovery via Math.floor can
      // land at idx-1 due to float imprecision (e.g. KICK 34→2901→floor=33).
      // Center-of-bucket guarantees Math.floor always recovers the correct idx.
      const normVal = Math.round(((idx + 0.5) / modelDef.discrete.length) * 4096);
      
      const mData = getModuleData(instanceId);
      if (mData) {
        mData.params.model = normVal;
        generateCode();
      }
    });

    // Prevent drag and zoom events when interacting with the select element
    select.addEventListener('pointerdown', e => e.stopPropagation());
    select.addEventListener('mousedown', e => e.stopPropagation());

    dropdownWrap.appendChild(select);
    mod.appendChild(dropdownWrap);
  }

  // Workshop Computer Custom Panel Layout
  if (type === 'computer') {
    mod.classList.add('mod-computer');

    const panel = el('div', 'computer-panel-wrap');
    panel.style.position = 'relative';
    panel.style.width = '100%';
    panel.style.height = '100%';
    panel.style.flex = '1';

    // Print text labels on faceplate
    const panelLabels = [
      { text: 'MAIN', x: 56, y: 86 },
      { text: 'X', x: 18, y: 144 },
      { text: 'Y', x: 56, y: 144 },
      { text: 'Z', x: 98, y: 153 }
    ];
    for (const l of panelLabels) {
      const lbl = el('div', 'computer-panel-label', { textContent: l.text });
      lbl.style.position = 'absolute';
      lbl.style.left = `${l.x}px`;
      lbl.style.top = `${l.y}px`;
      lbl.style.transform = 'translateX(-50%)';
      lbl.style.fontSize = '7px';
      lbl.style.color = '#8e8675';
      lbl.style.fontWeight = 'bold';
      lbl.style.pointerEvents = 'none';
      lbl.style.letterSpacing = '0.05em';
      panel.appendChild(lbl);
    }

    // MAIN knob (large, top)
    const mainKnobWrap = buildControlEl(type, instanceId, def.knobs[0], params.main !== undefined ? params.main : def.knobs[0].def);
    mainKnobWrap.querySelector('.knob-lbl')?.remove();
    mainKnobWrap.style.position = 'absolute';
    mainKnobWrap.style.left = '55.88px';
    mainKnobWrap.style.top = '68.85px';
    mainKnobWrap.style.transform = 'translate(-50%, -50%)';
    panel.appendChild(mainKnobWrap);

    // X knob (small, middle left)
    const xKnobWrap = buildControlEl(type, instanceId, def.knobs[1], params.x !== undefined ? params.x : def.knobs[1].def);
    xKnobWrap.querySelector('.knob-lbl')?.remove();
    xKnobWrap.style.position = 'absolute';
    xKnobWrap.style.left = '17.96px';
    xKnobWrap.style.top = '132.72px';
    xKnobWrap.style.transform = 'translate(-50%, -50%)';
    panel.appendChild(xKnobWrap);

    // Y knob (small, middle center)
    const yKnobWrap = buildControlEl(type, instanceId, def.knobs[2], params.y !== undefined ? params.y : def.knobs[2].def);
    yKnobWrap.querySelector('.knob-lbl')?.remove();
    yKnobWrap.style.position = 'absolute';
    yKnobWrap.style.left = '55.69px';
    yKnobWrap.style.top = '132.43px';
    yKnobWrap.style.transform = 'translate(-50%, -50%)';
    panel.appendChild(yKnobWrap);

    // Z switch (toggle, middle right)
    const zSwitchWrap = buildControlEl(type, instanceId, def.knobs[3], params.z !== undefined ? params.z : def.knobs[3].def);
    zSwitchWrap.querySelector('.knob-lbl')?.remove();
    zSwitchWrap.style.position = 'absolute';
    zSwitchWrap.style.left = '97.84px';
    zSwitchWrap.style.top = '135.51px';
    zSwitchWrap.style.transform = 'translate(-50%, -50%)';
    panel.appendChild(zSwitchWrap);

    // 12 Jacks in a 4x3 grid
    const jacksData = [
      // Row 1 (y = 167.87 + 14 = 181.87px)
      { id: 'audio-in-1', x: 12.41, y: 181.87, label: 'AUD 1', dir: 'input' },
      { id: 'audio-in-2', x: 41.21, y: 181.87, label: 'AUD 2', dir: 'input' },
      { id: 'audio-out-1', x: 70.01, y: 181.87, label: 'AUD 1', dir: 'output' },
      { id: 'audio-out-2', x: 98.81, y: 181.87, label: 'AUD 2', dir: 'output' },
      // Row 2 (y = 207.47 + 14 = 221.47px)
      { id: 'cv-in-1', x: 12.41, y: 221.47, label: 'CV 1', dir: 'input' },
      { id: 'cv-in-2', x: 41.21, y: 221.47, label: 'CV 2', dir: 'input' },
      { id: 'cv-out-1', x: 70.01, y: 221.47, label: 'CV 1', dir: 'output' },
      { id: 'cv-out-2', x: 98.81, y: 221.47, label: 'CV 2', dir: 'output' },
      // Row 3 (y = 247.05 + 14 = 261.05px)
      { id: 'pulse-in-1', x: 12.41, y: 261.13, label: 'PLS 1', dir: 'input' },
      { id: 'pulse-in-2', x: 41.21, y: 261.04, label: 'PLS 2', dir: 'input' },
      { id: 'pulse-out-1', x: 70.09, y: 261.05, label: 'PLS 1', dir: 'output' },
      { id: 'pulse-out-2', x: 98.85, y: 261.00, label: 'PLS 2', dir: 'output' }
    ];

    // Vertical divider line between input and output blocks
    const divider = el('div', 'computer-divider-line');
    divider.style.position = 'absolute';
    divider.style.left = '55.6px';
    divider.style.top = '170px';
    divider.style.width = '1px';
    divider.style.height = '105px';
    divider.style.background = 'rgba(255,255,255,0.08)';
    divider.style.borderRight = '1px solid rgba(0,0,0,0.4)';
    panel.appendChild(divider);

    for (const j of jacksData) {
      const jackEl = buildJackEl(instanceId, j.id, j.label, j.dir);
      jackEl.style.position = 'absolute';
      jackEl.style.left = `${j.x}px`;
      jackEl.style.top = `${j.y}px`;
      jackEl.style.transform = 'translate(-50%, -50%)';
      panel.appendChild(jackEl);
    }

    // 6 LEDs in a 2x3 grid
    const ledsData = [
      { x: 6.71, y: 323.28 }, { x: 19.95, y: 323.28 },
      { x: 6.71, y: 336.26 }, { x: 19.95, y: 336.26 },
      { x: 6.71, y: 349.19 }, { x: 19.95, y: 349.19 }
    ];

    for (let i = 0; i < 6; i++) {
      const led = ledsData[i];
      const ledDot = el('div', `computer-led-dot led-${i}`);
      ledDot.style.position = 'absolute';
      ledDot.style.left = `${led.x}px`;
      ledDot.style.top = `${led.y}px`;
      ledDot.style.transform = 'translate(-50%, -50%)';
      ledDot.style.width = '7px';
      ledDot.style.height = '7px';
      ledDot.style.borderRadius = '50%';
      ledDot.style.background = '#3a1212';
      ledDot.style.border = '1.5px solid #220a0a';
      ledDot.style.boxShadow = 'inset 0 1px 2px rgba(0,0,0,0.5)';
      panel.appendChild(ledDot);
    }

    // Program Card Slot (bottom center)
    const cardSlot = el('div', 'computer-card-slot');
    cardSlot.style.position = 'absolute';
    cardSlot.style.left = '43.76px';
    cardSlot.style.top = '300.03px';
    cardSlot.style.width = '37.16px';
    cardSlot.style.height = '55.04px';
    cardSlot.style.cursor = 'pointer';
    cardSlot.title = 'Program Card Slot - Click to Edit Code';

    const card = el('div', 'computer-cartridge-card');
    card.style.width = '100%';
    card.style.height = '100%';
    card.style.position = 'relative';
    card.style.borderRadius = '4px';
    card.style.background = 'linear-gradient(135deg, #007c5b 0%, #004d38 100%)';
    card.style.border = '1px solid rgba(223, 184, 108, 0.4)';
    card.style.boxShadow = 'inset 0 1px 2px rgba(255,255,255,0.1), 0 4px 8px rgba(0,0,0,0.5)';
    card.style.transform = 'translateY(10px)';
    card.style.transition = 'transform 0.2s cubic-bezier(0.4, 0, 0.2, 1), border-color 0.2s, box-shadow 0.2s';
    card.style.display = 'flex';
    card.style.flexDirection = 'column';
    card.style.alignItems = 'center';
    card.style.justifyContent = 'space-between';
    card.style.padding = '4px 0';

    // Gold connector contacts at the bottom
    const contacts = el('div');
    contacts.style.width = '24px';
    contacts.style.height = '6px';
    contacts.style.display = 'flex';
    contacts.style.justifyContent = 'space-between';
    for (let i = 0; i < 5; i++) {
      const finger = el('div');
      finger.style.width = '3px';
      finger.style.height = '100%';
      finger.style.background = 'linear-gradient(to bottom, #dfb86c, #a37c3f)';
      finger.style.borderRadius = '0.5px';
      contacts.appendChild(finger);
    }

    // Golden chip/micro-processor in the center
    const chip = el('div');
    chip.style.width = '14px';
    chip.style.height = '14px';
    chip.style.background = 'linear-gradient(135deg, #3d3a35 0%, #1e1d1b 100%)';
    chip.style.border = '1px solid rgba(223, 184, 108, 0.4)';
    chip.style.borderRadius = '2px';
    chip.style.boxShadow = '0 1px 3px rgba(0,0,0,0.4)';
    chip.style.display = 'flex';
    chip.style.alignItems = 'center';
    chip.style.justifyContent = 'center';

    const chipCore = el('div');
    chipCore.style.width = '6px';
    chipCore.style.height = '6px';
    chipCore.style.background = '#dfb86c';
    chipCore.style.borderRadius = '1px';
    chipCore.style.boxShadow = '0 0 4px rgba(223, 184, 108, 0.8)';
    chip.appendChild(chipCore);

    // Label text at the top
    const cardLabel = el('div');
    cardLabel.style.fontSize = '6px';
    cardLabel.style.color = '#dfb86c';
    cardLabel.style.fontWeight = 'bold';
    cardLabel.style.letterSpacing = '0.1em';
    cardLabel.style.textTransform = 'uppercase';
    cardLabel.textContent = 'LENS';

    // Append in correct order
    card.appendChild(cardLabel);
    card.appendChild(chip);
    card.appendChild(contacts);

    cardSlot.appendChild(card);

    cardSlot.addEventListener('pointerdown', e => e.stopPropagation());
    cardSlot.addEventListener('mousedown', e => e.stopPropagation());
    cardSlot.addEventListener('click', e => {
      e.stopPropagation();
      openCodeEditorModal(instanceId);
    });
    panel.appendChild(cardSlot);

    // Reset Button
    const resetBtn = el('div', 'computer-reset-btn');
    resetBtn.style.position = 'absolute';
    resetBtn.style.left = '99.71px';
    resetBtn.style.top = '346.8px';
    resetBtn.style.width = '11px';
    resetBtn.style.height = '11px';
    resetBtn.style.background = '#e63946';
    resetBtn.style.border = '1.5px solid #222';
    resetBtn.style.borderRadius = '50%';
    resetBtn.style.boxShadow = '0 1px 2px rgba(0,0,0,0.5), inset 0 1px 1px rgba(255,255,255,0.3)';
    resetBtn.style.cursor = 'pointer';
    resetBtn.title = 'Reset Module Code';

    resetBtn.addEventListener('pointerdown', e => e.stopPropagation());
    resetBtn.addEventListener('mousedown', e => e.stopPropagation());
    resetBtn.addEventListener('click', e => {
      e.stopPropagation();
      const mData = getModuleData(instanceId);
      if (mData) {
        mData.params.code = `; Computer Patch\n; Connect inputs and outputs, then write Loupe code!\n\n(def pitch (add (cv-in :1) (knob :main)))\n(<- (cv-out :1) (sine :note pitch))\n(<- (led :0) (gt (sine :note pitch) 2048))\n`;
        generateCode();
      }
    });
    panel.appendChild(resetBtn);

    mod.appendChild(panel);
    return mod;
  }

  // Quad VCA Custom Panel Layout
  if (type === 'quad-vca') {
    mod.classList.add('mod-quad-vca');
    const panel = el('div', 'quad-vca-panel-wrap');
    panel.style.display = 'flex';
    panel.style.width = '100%';
    panel.style.height = '100%';
    panel.style.flex = '1';

    const colWidth = (def.hp * HP) / 4;

    const chKeys = ['A', 'B', 'C', 'D'];
    chKeys.forEach((ch, idx) => {
      const col = el('div', 'mixer-column');
      col.style.width = `${colWidth}px`;
      col.style.height = '100%';
      col.style.display = 'flex';
      col.style.flexDirection = 'column';
      col.style.alignItems = 'center';
      col.style.justifyContent = 'space-around';
      col.style.borderRight = idx < 3 ? '1px solid rgba(255,255,255,0.05)' : 'none';
      col.style.padding = '15px 0 10px 0';
      col.style.boxSizing = 'border-box';

      const lbl = el('div', 'mixer-ch-lbl', { textContent: `VCA ${ch}`, style: 'font-size: 8px; color: #8e8675; font-weight: bold; margin-bottom: 2px;' });
      col.appendChild(lbl);

      const volDef = def.knobs.find(k => k.param === `vol${ch}`);
      const volKnob = buildControlEl(type, instanceId, volDef, params[`vol${ch}`] ?? volDef.def);
      volKnob.querySelector('.knob-lbl')?.remove();
      col.appendChild(volKnob);

      const inJack = buildJackEl(instanceId, `in${ch}`, `IN ${ch}`, 'input');
      const cvJack = buildJackEl(instanceId, `cv${ch}`, `CV ${ch}`, 'input');
      const outJack = buildJackEl(instanceId, `out${ch}`, `OUT ${ch}`, 'output');

      col.appendChild(inJack);
      col.appendChild(cvJack);
      col.appendChild(outJack);

      panel.appendChild(col);
    });

    mod.appendChild(panel);
    return mod;
  }

  // Stereo Mixer Custom Panel Layout
  if (type === 'stereo-mixer') {
    mod.classList.add('mod-stereo-mixer');
    const panel = el('div', 'mixer-panel-wrap');
    panel.style.display = 'flex';
    panel.style.width = '100%';
    panel.style.height = '100%';
    panel.style.flex = '1';

    const colWidth = (def.hp * HP) / 5;

    const chKeys = ['A', 'B', 'C', 'D'];
    chKeys.forEach((ch, idx) => {
      const col = el('div', 'mixer-column');
      col.style.width = `${colWidth}px`;
      col.style.height = '100%';
      col.style.display = 'flex';
      col.style.flexDirection = 'column';
      col.style.alignItems = 'center';
      col.style.justifyContent = 'space-around';
      col.style.borderRight = '1px solid rgba(255,255,255,0.05)';
      col.style.padding = '20px 0 10px 0';
      col.style.boxSizing = 'border-box';

      const lbl = el('div', 'mixer-ch-lbl', { textContent: `CH ${ch}`, style: 'font-size: 8px; color: #8e8675; font-weight: bold;' });
      col.appendChild(lbl);

      const panDef = def.knobs.find(k => k.param === `pan${ch}`);
      const panKnob = buildControlEl(type, instanceId, panDef, params[`pan${ch}`] ?? panDef.def);
      panKnob.querySelector('.knob-lbl')?.remove();
      col.appendChild(panKnob);

      const volDef = def.knobs.find(k => k.param === `vol${ch}`);
      const volKnob = buildControlEl(type, instanceId, volDef, params[`vol${ch}`] ?? volDef.def);
      volKnob.querySelector('.knob-lbl')?.remove();
      col.appendChild(volKnob);

      const muteDef = def.knobs.find(k => k.param === `mute${ch}`);
      const muteSwitch = buildControlEl(type, instanceId, muteDef, params[`mute${ch}`] ?? muteDef.def);
      muteSwitch.querySelector('.knob-lbl')?.remove();
      col.appendChild(muteSwitch);

      const jackEl = buildJackEl(instanceId, `in${ch}`, `IN ${ch}`, 'input');
      col.appendChild(jackEl);

      panel.appendChild(col);
    });

    const masterCol = el('div', 'mixer-column master-column');
    masterCol.style.width = `${colWidth}px`;
    masterCol.style.height = '100%';
    masterCol.style.display = 'flex';
    masterCol.style.flexDirection = 'column';
    masterCol.style.alignItems = 'center';
    masterCol.style.justifyContent = 'space-around';
    masterCol.style.padding = '20px 0 10px 0';
    masterCol.style.boxSizing = 'border-box';
    masterCol.style.background = 'rgba(0,0,0,0.15)';

    const masterLbl = el('div', 'mixer-ch-lbl', { textContent: 'MASTER', style: 'font-size: 8px; color: #dfb86c; font-weight: bold;' });
    masterCol.appendChild(masterLbl);

    const masterDef = def.knobs.find(k => k.param === 'master');
    const masterKnob = buildControlEl(type, instanceId, masterDef, params.master ?? masterDef.def);
    masterKnob.querySelector('.knob-lbl')?.remove();
    masterCol.appendChild(masterKnob);

    const spacer = el('div', '', { style: 'height: 60px;' });
    masterCol.appendChild(spacer);

    const outL = buildJackEl(instanceId, 'outL', 'OUT L', 'output');
    const outR = buildJackEl(instanceId, 'outR', 'OUT R', 'output');
    masterCol.appendChild(outL);
    masterCol.appendChild(outR);

    panel.appendChild(masterCol);
    mod.appendChild(panel);
    return mod;
  }

  // Score Player pattern input directly on faceplate
  if (type === 'score-player') {
    const wrap = el('div', 'pattern-input-wrap');
    const label = el('div', 'pattern-input-lbl', { textContent: 'SCORE PATTERN' });
    const input = el('textarea', 'module-pattern-input');
    input.value = params.pattern || '[c4 e4 g4 c5]';
    // Prevent typing from dragging module
  input.addEventListener('pointerdown', e => e.stopPropagation());
  input.addEventListener('mousedown', e => e.stopPropagation());
    input.addEventListener('input', e => {
      const mData = getModuleData(instanceId);
      if (mData) {
        mData.params.pattern = e.target.value;
      }
      scheduleGenerateCode(120);
    });
    input.addEventListener('change', e => {
      const mData = getModuleData(instanceId);
      if (mData) {
        mData.params.pattern = e.target.value;
      }
      generateCode();
    });
    wrap.appendChild(label);
    wrap.appendChild(input);
    mod.appendChild(wrap);
  }

  // Standalone Data Tape pattern input directly on faceplate
  if (type === 'tape') {
    const wrap = el('div', 'pattern-input-wrap');
    const label = el('div', 'pattern-input-lbl', { textContent: 'TAPE VALUES' });
    const input = el('textarea', 'module-pattern-input');
    input.value = params.pattern || '(0 1 1 0 1 0 0 1)';
    // Prevent typing from dragging module
    input.addEventListener('pointerdown', e => e.stopPropagation());
    input.addEventListener('mousedown', e => e.stopPropagation());
    input.addEventListener('change', e => {
      const mData = getModuleData(instanceId);
      if (mData) {
        mData.params.pattern = e.target.value;
      }
      generateCode();
    });
    wrap.appendChild(label);
    wrap.appendChild(input);
    mod.appendChild(wrap);
  }


  if (type === 'quantizer') {
    const keyboardWrap = el('div', 'module-quantizer-wrap');
    keyboardWrap.style.padding = '0 8px 8px 8px';
    keyboardWrap.style.width = '100%';
    keyboardWrap.style.boxSizing = 'border-box';
    
    const keyboard = el('div', 'piano-keyboard');
    
    const scaleDef = def.knobs.find(k => k.param === 'scale');
    const scaleIdx = Math.max(0, Math.min(scaleDef.discrete.length - 1, Math.floor((params.scale ?? 0) / 4096 * scaleDef.discrete.length)));
    const scaleName = scaleDef.discrete[scaleIdx];

    const rootDef = def.knobs.find(k => k.param === 'root');
    const rootIdx = Math.max(0, Math.min(rootDef.discrete.length - 1, Math.floor((params.root ?? 0) / 4096 * rootDef.discrete.length)));

    const SCALE_MASKS = {
      'minor': 1453, 'major': 2741, 'm.pent': 1193, 'M.pent': 661,
      'dorian': 1709, 'phryg': 1451, 'lydian': 2773, 'mixo': 1717, 'chrom': 4095
    };
    const mask = SCALE_MASKS[scaleName] || 4095;
    
    let activeMask = 0;
    for (let i = 0; i < 12; i++) {
      if ((mask & (1 << i)) !== 0) {
        activeMask |= (1 << ((i + rootIdx) % 12));
      }
    }

    const keysDef = [
      { note: 11, isBlack: false },
      { note: 10, isBlack: true, top: 10.5 },
      { note: 9, isBlack: false },
      { note: 8, isBlack: true, top: 25.5 },
      { note: 7, isBlack: false },
      { note: 6, isBlack: true, top: 40.5 },
      { note: 5, isBlack: false },
      { note: 4, isBlack: false },
      { note: 3, isBlack: true, top: 70.5 },
      { note: 2, isBlack: false },
      { note: 1, isBlack: true, top: 85.5 },
      { note: 0, isBlack: false }
    ];

    keysDef.forEach(k => {
      const keyClass = k.isBlack ? 'piano-key-black' : 'piano-key-white';
      const key = el('div', keyClass);
      if (k.isBlack) {
        key.style.top = k.top + 'px';
      }
      if ((activeMask & (1 << k.note)) !== 0) {
        key.classList.add('active');
      }
      key.addEventListener('pointerdown', e => e.stopPropagation());
      key.addEventListener('mousedown', e => e.stopPropagation());
      keyboard.appendChild(key);
    });

    keyboardWrap.appendChild(keyboard);
    mod.appendChild(keyboardWrap);
  }

  // Wave Draw Custom Panel
  if (type === 'wave-draw') {
    const wrap = el('div', 'wave-draw-wrap');
    wrap.style.padding = '4px 8px';
    wrap.style.width = '100%';
    wrap.style.boxSizing = 'border-box';
    wrap.style.height = '140px';
    wrap.style.display = 'flex';
    wrap.style.flexDirection = 'column';

    const controls = el('div', 'wave-draw-controls');
    controls.style.display = 'flex';
    controls.style.gap = '4px';
    controls.style.marginBottom = '4px';
    controls.addEventListener('pointerdown', e => e.stopPropagation());
    controls.addEventListener('mousedown', e => e.stopPropagation());

    const resSelect = el('select', 'module-dropdown-select');
    resSelect.style.flex = '1';
    resSelect.style.background = '#151515';
    resSelect.style.color = '#eee';
    resSelect.style.border = '1px solid #333';
    resSelect.style.fontSize = '9px';
    ['16', '32', '64', '128'].forEach(r => {
      const opt = el('option', ''); opt.value = r; opt.textContent = r;
      if (parseInt(r, 10) === (params.res || 64)) opt.selected = true;
      resSelect.appendChild(opt);
    });

    const presetSelect = el('select', 'module-dropdown-select');
    presetSelect.style.flex = '1';
    presetSelect.style.background = '#151515';
    presetSelect.style.color = '#eee';
    presetSelect.style.border = '1px solid #333';
    presetSelect.style.fontSize = '9px';
    ['custom', 'sine', 'saw', 'sqr', 'tri', 'noise'].forEach(p => {
      const opt = el('option', ''); opt.value = p; opt.textContent = p.toUpperCase();
      presetSelect.appendChild(opt);
    });

    controls.appendChild(resSelect);
    controls.appendChild(presetSelect);
    wrap.appendChild(controls);

    const canvas = el('canvas', 'wave-draw-canvas');
    canvas.width = 128;
    canvas.height = 110;
    canvas.style.width = '100%';
    canvas.style.height = '100%';
    canvas.style.background = '#111';
    canvas.style.border = '1px solid #333';
    canvas.style.borderRadius = '2px';
    canvas.style.cursor = 'crosshair';
    canvas.style.touchAction = 'none';
    canvas.addEventListener('mousedown', e => e.stopPropagation());

    wrap.appendChild(canvas);
    mod.appendChild(wrap);

    const ctx = canvas.getContext('2d');
    
    // Get or init wave array
    let currentRes = params.res || 64;
    let wave = params.waveform;
    if (!wave || wave.length !== currentRes) {
      wave = new Array(currentRes).fill(2048);
      params.waveform = wave;
      params.res = currentRes;
    }

    function drawWave() {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      
      // Draw center line
      ctx.beginPath();
      ctx.strokeStyle = '#333';
      ctx.lineWidth = 1;
      ctx.moveTo(0, canvas.height / 2);
      ctx.lineTo(canvas.width, canvas.height / 2);
      ctx.stroke();

      // Draw wave
      ctx.beginPath();
      ctx.strokeStyle = '#4cd137';
      ctx.lineWidth = 2;
      for (let i = 0; i < currentRes; i++) {
        const v = wave[i];
        const x = (i / (currentRes - 1)) * canvas.width;
        const y = canvas.height - ((v / 4095) * canvas.height);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }
    
    drawWave();

    resSelect.addEventListener('change', e => {
      currentRes = parseInt(e.target.value, 10);
      const newWave = new Array(currentRes).fill(2048);
      // naive resample existing wave
      for (let i = 0; i < currentRes; i++) {
        const srcIdx = Math.min(wave.length - 1, Math.floor((i / currentRes) * wave.length));
        newWave[i] = wave[srcIdx];
      }
      wave = newWave;
      const mData = getModuleData(instanceId);
      if (mData) {
        mData.params.waveform = wave;
        mData.params.res = currentRes;
        generateCode();
      }
      drawWave();
    });

    presetSelect.addEventListener('change', e => {
      const p = e.target.value;
      if (p === 'custom') return;
      for (let i = 0; i < currentRes; i++) {
        const phase = i / currentRes; // 0 to 1
        if (p === 'sine') wave[i] = Math.round(2048 + 2047 * Math.sin(phase * Math.PI * 2));
        else if (p === 'saw') wave[i] = Math.round(4095 * (1 - phase));
        else if (p === 'sqr') wave[i] = phase < 0.5 ? 4095 : 0;
        else if (p === 'tri') wave[i] = phase < 0.5 ? Math.round(phase * 2 * 4095) : Math.round((1 - phase) * 2 * 4095);
        else if (p === 'noise') wave[i] = Math.floor(Math.random() * 4096);
      }
      presetSelect.value = 'custom';
      const mData = getModuleData(instanceId);
      if (mData) {
        mData.params.waveform = wave;
        generateCode();
      }
      drawWave();
    });

    let isDrawing = false;
    let lastIdx = -1;
    
    function paint(e) {
      if (!isDrawing) return;
      const rect = canvas.getBoundingClientRect();
      const x = Math.max(0, Math.min(e.clientX - rect.left, rect.width));
      const y = Math.max(0, Math.min(e.clientY - rect.top, rect.height));
      
      const idx = Math.min(currentRes - 1, Math.floor((x / rect.width) * currentRes));
      const v = Math.min(4095, Math.max(0, Math.round((1 - (y / rect.height)) * 4095)));
      
      if (lastIdx === -1) {
        wave[idx] = v;
      } else {
        const step = Math.sign(idx - lastIdx);
        if (step !== 0) {
          const vDiff = v - wave[lastIdx];
          const dist = Math.abs(idx - lastIdx);
          for (let i = 1; i <= dist; i++) {
            wave[lastIdx + i * step] = Math.round(wave[lastIdx] + vDiff * (i / dist));
          }
        } else {
          wave[idx] = v;
        }
      }
      lastIdx = idx;
      drawWave();
    }

    canvas.addEventListener('pointerdown', e => {
      e.stopPropagation();
      isDrawing = true;
      lastIdx = -1;
      canvas.setPointerCapture(e.pointerId);
      paint(e);
    });
    
    canvas.addEventListener('pointermove', e => {
      if (isDrawing) {
        e.stopPropagation();
        paint(e);
      }
    });

    canvas.addEventListener('pointerup', e => {
      if (isDrawing) {
        isDrawing = false;
        canvas.releasePointerCapture(e.pointerId);
        
        // Save to params
        const mData = getModuleData(instanceId);
        if (mData) {
          mData.params.waveform = wave.slice();
          generateCode();
        }
      }
    });
  }

  // Formula Eval expression input directly on faceplate
  if (type === 'expression') {
    const wrap = el('div', 'pattern-input-wrap');
    const label = el('div', 'pattern-input-lbl', { textContent: 'FORMULA (A & B)' });
    const input = el('textarea', 'module-pattern-input');
    input.value = params.expression || '(add a b)';
    input.style.fontFamily = 'monospace';
    input.style.fontSize = '10px';
    // Prevent typing from dragging module
    input.addEventListener('pointerdown', e => e.stopPropagation());
    input.addEventListener('mousedown', e => e.stopPropagation());
    input.addEventListener('change', e => {
      const mData = getModuleData(instanceId);
      if (mData) {
        mData.params.expression = e.target.value;
      }
      generateCode();
    });
    wrap.appendChild(label);
    wrap.appendChild(input);
    mod.appendChild(wrap);
  }

  // Drum Sequencer step grid directly on faceplate
  if (type === 'drum-seq') {
    const gridWrap = el('div', 'drum-seq-grid-wrap');
    const channels = [
      { key: 'kickPat', label: 'K', def: [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0] },
      { key: 'snarePat', label: 'S', def: [0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0] },
      { key: 'hatPat', label: 'H', def: [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0] },
      { key: 'percPat', label: 'P', def: [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1] }
    ];
    for (const chan of channels) {
      const row = el('div', 'drum-seq-row');
      const lbl = el('span', 'drum-seq-row-lbl'); lbl.textContent = chan.label;
      row.appendChild(lbl);

      const pat = params[chan.key] || [...chan.def];
      params[chan.key] = pat;

      const stepsVal = getKnobValue(type, 'steps', params.steps ?? 4095);
      for (let stepIdx = 0; stepIdx < 16; stepIdx++) {
        const isDimmed = stepIdx >= stepsVal;
        const btn = el('button', 'drum-seq-step-btn' + (pat[stepIdx] ? ' active' : '') + (isDimmed ? ' dimmed' : ''));
        btn.dataset.step = stepIdx;
        btn.dataset.channel = chan.key;
        btn.addEventListener('pointerdown', e => e.stopPropagation());
        btn.addEventListener('mousedown', e => e.stopPropagation());
        btn.addEventListener('click', e => {
          e.stopPropagation();
          const mData = getModuleData(instanceId);
          if (mData) {
            if (!mData.params[chan.key]) {
              mData.params[chan.key] = [...chan.def];
            }
            mData.params[chan.key][stepIdx] = mData.params[chan.key][stepIdx] ? 0 : 1;
            btn.classList.toggle('active');
            generateCode();
          }
        });
        row.appendChild(btn);
      }
      gridWrap.appendChild(row);
    }
    mod.appendChild(gridWrap);
  }

  // Knobs: Layout class chosen dynamically (vertical vs grid vs hybrid)
  if (hasKnobs) {
    const layoutType = def.knobLayout || 'vertical';
    if (layoutType === 'hybrid') {
      const knobArea = el('div', 'module-knobs knob-layout-hybrid');
      const k1 = def.knobs[0];
      const v1 = params[k1.param] !== undefined ? params[k1.param] : k1.def;
      knobArea.appendChild(buildControlEl(type, instanceId, k1, v1));

      const row = el('div', 'knob-layout-hybrid-row');
      for (let i = 1; i < 3; i++) {
        const k = def.knobs[i];
        const v = params[k.param] !== undefined ? params[k.param] : k.def;
        row.appendChild(buildControlEl(type, instanceId, k, v));
      }
      knobArea.appendChild(row);
      mod.appendChild(knobArea);
    } else {
      const isCompact = def.knobs.length > 6;
      const knobArea = el('div', `module-knobs knob-layout-${layoutType}${isCompact ? ' knobs-compact' : ''}`);
      for (const kDef of def.knobs) {
        if (type === 'weave' && kDef.param === 'model') continue;
        const val = params[kDef.param] !== undefined ? params[kDef.param] : kDef.def;
        knobArea.appendChild(buildControlEl(type, instanceId, kDef, val));
      }
      mod.appendChild(knobArea);
    }
  }

  // Ports: Unified grid of input and output jacks
  if (def.inputs.length > 0 || def.outputs.length > 0) {
    const ports = el('div', 'module-ports');
    const totalJacks = def.inputs.length + def.outputs.length;
    const useGrid4 = def.hp >= 10 && totalJacks >= 4;
    const useGrid2 = totalJacks > 2 && def.hp > 2;
    const gridClass = useGrid4 ? ' ports-grid-4' : (useGrid2 ? ' ports-grid-2' : '');
    const inner = el('div', 'ports-col-inner' + gridClass);

    for (const p of def.inputs) {
      inner.appendChild(buildJackEl(instanceId, p.id, p.label, 'input'));
    }
    for (const p of def.outputs) {
      inner.appendChild(buildJackEl(instanceId, p.id, p.label, 'output'));
    }

    ports.appendChild(inner);
    mod.appendChild(ports);
  }

  return mod;
}

function buildSwitchEl(type, instanceId, kDef, val) {
  const wrap = el('div', 'knob-wrap switch-wrap');
  const numPos = kDef.discrete.length;

  let idx = 0;
  if (typeof val === 'string') {
    idx = kDef.discrete.indexOf(val);
    if (idx === -1) idx = 0;
  } else {
    idx = Math.max(0, Math.min(numPos - 1, Math.floor((val / 4096) * numPos)));
  }

  let rawVal = val;
  if (typeof val === 'string' || typeof val === 'boolean') {
    if (numPos === 2) {
      rawVal = idx === 0 ? 0 : 4095;
    } else {
      rawVal = idx === 0 ? 0 : idx === 1 ? 2048 : 4095;
    }
  }

  const sw = el('div', 'switch-control');
  sw.id = `knob-${instanceId}-${kDef.param}`;
  sw.dataset.instanceId = instanceId;
  sw.dataset.param = kDef.param;
  sw.dataset.val = rawVal;

  const hasAssets = (typeof FLARE_ASSETS !== 'undefined' && FLARE_ASSETS.switchMid);

  if (hasAssets) {
    sw.style.width = '24px';
    sw.style.height = '28px';
    sw.style.position = 'relative';
    sw.style.cursor = 'pointer';
    sw.style.margin = '3px auto';
  } else {
    sw.style.width = '16px';
    sw.style.height = '34px';
    sw.style.background = '#12100d';
    sw.style.border = '1px solid #3a342a';
    sw.style.borderRadius = '3px';
    sw.style.position = 'relative';
    sw.style.cursor = 'pointer';
    sw.style.margin = '3px auto';
  }

  let lever;
  if (!hasAssets) {
    lever = el('div', 'switch-lever');
    lever.style.width = '12px';
    lever.style.height = '12px';
    lever.style.background = 'linear-gradient(135deg, #eee, #999)';
    lever.style.border = '1px solid #444';
    lever.style.borderRadius = '2px';
    lever.style.position = 'absolute';
    lever.style.left = '1px';
    lever.style.boxShadow = '0 2px 3px rgba(0,0,0,0.6)';
    lever.style.transition = 'top 0.1s ease';
    sw.appendChild(lever);
  }

  const getTopForIdx = (i) => {
    if (numPos === 2) {
      return i === 0 ? '2px' : '18px';
    } else {
      return i === 0 ? '2px' : i === 1 ? '10px' : '18px';
    }
  };

  const updateVisual = (newVal) => {
    const newIdx = Math.max(0, Math.min(numPos - 1, Math.floor((newVal / 4096) * numPos)));
    if (hasAssets) {
      let assetKey = 'switchMid';
      if (newIdx === 0) assetKey = 'switchDown';
      if (newIdx === numPos - 1) assetKey = 'switchUp';
      sw.innerHTML = FLARE_ASSETS[assetKey] || '';
    } else {
      lever.style.top = getTopForIdx(newIdx);
    }
  };

  updateVisual(rawVal);

  const updateValue = (newVal) => {
    sw.dataset.val = newVal;
    updateVisual(newVal);

    handleParamValueUpdate(instanceId, kDef.param, newVal, sw);

    const displayVal = getDisplayValueStr(type, kDef.param, newVal, instanceId);
    valEl.textContent = displayVal;
    wrap.title = `${kDef.label}: ${displayVal}`;
    generateCode();
  };

  sw.addEventListener('click', (e) => {
    e.stopPropagation();
    const curVal = parseInt(sw.dataset.val ?? 0);
    const curIdx = Math.max(0, Math.min(numPos - 1, Math.floor((curVal / 4096) * numPos)));
    const nextIdx = (curIdx + 1) % numPos;
    let nextVal = 0;
    if (numPos === 2) {
      nextVal = nextIdx === 0 ? 0 : 4095;
    } else {
      nextVal = nextIdx === 0 ? 0 : nextIdx === 1 ? 2048 : 4095;
    }
    updateValue(nextVal);
  });

  // Enable dragging parameter adjustments like standard knobs
  let startY = 0;
  let startVal = 0;

  const handlePointerDown = (e) => {
    e.stopPropagation();
    e.preventDefault();
    startY = e.clientY;
    startVal = parseInt(sw.dataset.val);
    document.addEventListener('pointermove', handlePointerMove);
    document.addEventListener('pointerup', handlePointerUp);
  };

  const handlePointerMove = (e) => {
    const deltaY = startY - e.clientY;
    const deltaVal = Math.round((deltaY / 150) * 4095);
    const newVal = Math.max(0, Math.min(4095, startVal + deltaVal));
    updateValue(newVal);
  };

  const handlePointerUp = () => {
    document.removeEventListener('pointermove', handlePointerMove);
    document.removeEventListener('pointerup', handlePointerUp);
  };

  sw.addEventListener('pointerdown', handlePointerDown);

  const lbl = el('div', 'knob-lbl');
  const nameEl = el('span', 'knob-lbl-name'); nameEl.textContent = kDef.label;
  const valEl = el('span', 'knob-lbl-val'); valEl.textContent = getDisplayValueStr(type, kDef.param, rawVal, instanceId);

  lbl.appendChild(nameEl);
  lbl.appendChild(valEl);

  wrap.title = `${kDef.label}: ${valEl.textContent}`;
  const desc = CONTROL_DESCRIPTIONS[kDef.param] || `${kDef.label} control.`;
  wrap.dataset.tooltip = `<strong>${kDef.label}</strong><br><span style="color:#aaa">${desc}</span>`;
  wrap.appendChild(sw);
  wrap.appendChild(lbl);
  return wrap;
}

function buildControlEl(type, instanceId, kDef, val) {
  if (kDef.discrete && kDef.discrete.length >= 2 && kDef.discrete.length <= 3) {
    return buildSwitchEl(type, instanceId, kDef, val);
  }
  return buildKnobEl(type, instanceId, kDef, val);
}

function buildKnobEl(type, instanceId, kDef, val) {
  const wrap = el('div', 'knob-wrap');
  const px = KNOB_PX[kDef.size] || 30;
  const asset = KNOB_ASSET[kDef.size] || 'mediumKnob';

  const knob = el('div', 'knob');
  knob.id = `knob-${instanceId}-${kDef.param}`;
  knob.dataset.instanceId = instanceId;
  knob.dataset.param = kDef.param;
  knob.dataset.val = val;
  knob.style.width = px + 'px';
  knob.style.height = px + 'px';
  knob.innerHTML = (typeof FLARE_ASSETS !== 'undefined' && FLARE_ASSETS[asset]) || '';

  const svg = knob.querySelector('svg');
  if (svg) {
    svg.style.width = '100%'; svg.style.height = '100%';
    svg.style.transform = `rotate(${valToAngle(val)}deg)`;
  }

  knob.addEventListener('pointerdown', handleKnobDown);
  knob.addEventListener('dblclick', handleKnobDblClick);
  knob.addEventListener('contextmenu', showContextMenu);

  const lbl = el('div', 'knob-lbl');

  const nameEl = el('span', 'knob-lbl-name'); nameEl.textContent = kDef.label;
  const valEl = el('span', 'knob-lbl-val'); valEl.textContent = getDisplayValueStr(type, kDef.param, val, instanceId);

  const mData = getModuleData(instanceId);
  const isMapped = mData && mData.params.__midi_cc && mData.params.__midi_cc[kDef.param] !== undefined;
  if (isMapped) {
    wrap.classList.add('knob-mapped-midi');
  }

  lbl.appendChild(nameEl);
  lbl.appendChild(valEl);

  wrap.title = `${kDef.label}: ${valEl.textContent}`;
  const desc = CONTROL_DESCRIPTIONS[kDef.param] || `${kDef.label} control.`;
  let tooltipText = `<strong>${kDef.label}</strong>`;
  if (isMapped) {
    const entry = mData.params.__midi_cc[kDef.param];
    const cc = (typeof entry === 'object') ? entry.cc : entry;
    const ch = (typeof entry === 'object') ? entry.ch : 1;
    tooltipText += ` <span style="color:#00ffcc; font-weight:bold">(MIDI CC ${cc} ch ${ch})</span>`;
  }
  tooltipText += `<br><span style="color:#aaa">${desc}</span>`;
  wrap.dataset.tooltip = tooltipText;

  wrap.appendChild(knob);
  wrap.appendChild(lbl);
  return wrap;
}

function buildJackEl(instanceId, portId, label, direction) {
  if (portId.startsWith('spacer')) {
    const wrap = el('div', 'jack-wrap spacer-jack');
    wrap.style.visibility = 'hidden';
    wrap.style.pointerEvents = 'none';
    return wrap;
  }
  const wrap = el('div', 'jack-wrap');
  wrap.title = label;
  const lbl = el('div', 'jack-lbl'); lbl.textContent = label;

  const jack = el('div', `jack jack-${direction}`);
  jack.id = `jack-${instanceId}-${direction}-${portId}`;
  jack.dataset.instanceId = instanceId;
  jack.dataset.portId = portId;
  jack.dataset.direction = direction;
  jack.addEventListener('pointerdown', handleJackDown);

  jack.addEventListener('pointerenter', () => {
    hoveredJack = { instanceId, portId };
    redrawCables();
  });
  jack.addEventListener('pointerleave', () => {
    hoveredJack = null;
    redrawCables();
  });

  const desc = JACK_DESCRIPTIONS[portId] || `${label} port.`;
  wrap.dataset.tooltip = `<strong>${label}</strong> (${direction === 'input' ? 'IN' : 'OUT'})<br><span style="color:#aaa">${desc}</span>`;

  wrap.appendChild(lbl);
  wrap.appendChild(jack);
  return wrap;
}

// ═══════════════════════════════════════════════════════════════════════
// 5. RACK ROWS & INITIAL PLACEMENT
// ═══════════════════════════════════════════════════════════════════════

function buildRowEl(rowIndex) {
  const row = el('div', 'rack-row'); row.dataset.rowIndex = rowIndex;
  const railTop = el('div', 'rail top');
  const bay = el('div', 'module-bay'); bay.id = `bay-${rowIndex}`; bay.dataset.row = rowIndex;
  const railBot = el('div', 'rail bottom');
  row.appendChild(railTop); row.appendChild(bay); row.appendChild(railBot);
  return row;
}

function addRow() {
  const rowIndex = state.rows.length;
  state.rows.push([]);
  const rowEl = buildRowEl(rowIndex);
  $('rackCase').appendChild(rowEl);
  refreshRowButtons();
  updateRowWidths();
  return rowIndex;
}

function getBay(rowIndex) { return $(`bay-${rowIndex}`); }

// Recalculates rail width to expand with added modules, triggering scrollbars
function updateRowWidths() {
  for (let i = 0; i < state.rows.length; i++) {
    const row = state.rows[i];
    const maxRight = row.reduce((max, m) => {
      const def = MODULE_DEFS[m.type];
      const mWidth = (def ? def.hp : 6) * HP;
      return Math.max(max, m.left + mWidth);
    }, 0);
    const bay = getBay(i);
    if (bay) {
      // 120px padding at the end of the rail for spacious visual padding
      bay.style.width = Math.max(1200, maxRight + 120) + 'px';
    }
  }
}

function addModuleToRow(type, rowIndex = 0, params = {}, opts = {}) {
  const def = MODULE_DEFS[type];
  if (!def) return null;

  const id = opts.id || (type.replace(/-/g, '') + (state.nextId++));

  const finalParams = {};
  for (const k of (def.knobs || [])) finalParams[k.param] = k.def;
  // Set default pattern for Score Player and Data Tape
  if (type === 'score-player') finalParams.pattern = '[c4 e4 g4 c5]';
  if (type === 'tape') finalParams.pattern = '(0 1 1 0 1 0 0 1)';
  if (type === 'expression') finalParams.expression = '(add a b)';
  if (type === 'computer') {
    finalParams.code = `; Computer Patch\n; Connect inputs and outputs, then write Loupe code!\n\n(def pitch (add (cv-in :1) (knob :main)))\n(<- (cv-out :1) (sine :note pitch))\n(<- (led :0) (gt (sine :note pitch) 2048))\n`;
  }
  Object.assign(finalParams, params);

  const leftPx = opts.left !== undefined ? snapToHP(opts.left) : findFreePosition(rowIndex, def.hp);

  if (!state.rows[rowIndex]) state.rows[rowIndex] = [];
  state.rows[rowIndex].push({ id, type, left: leftPx, params: finalParams });

  autoAssignMidiCcs();

  const modEl = buildModuleEl(type, id, finalParams, leftPx);
  if (modEl) getBay(rowIndex)?.appendChild(modEl);

  // Resolve overlaps immediately on addition
  resolveCollisions(rowIndex, id);

  updateRowWidths();
  generateCode();
  pushStateSnapshot();
  return id;
}

function deleteModule(instanceId) {
  if (selectedModuleIds.has(instanceId)) {
    selectedModuleIds.delete(instanceId);
    $(`mod-${instanceId}`)?.classList.remove('selected');
  }
  state.cables = state.cables.filter(c => c.fromId !== instanceId && c.toId !== instanceId);
  for (let i = 0; i < state.rows.length; i++) {
    state.rows[i] = state.rows[i].filter(m => m.id !== instanceId);
  }
  $(`mod-${instanceId}`)?.remove();
  updateRowWidths();
  redrawCables(); generateCode();
}

const selectedModuleIds = new Set();
let clipboardData = null;

function selectModule(instanceId) {
  clearSelection();
  if (instanceId) {
    selectedModuleIds.add(instanceId);
    $(`mod-${instanceId}`)?.classList.add('selected');
  }
}

function clearSelection() {
  for (const id of selectedModuleIds) {
    $(`mod-${id}`)?.classList.remove('selected');
  }
  selectedModuleIds.clear();
}

function copyModules() {
  if (selectedModuleIds.size === 0) return;

  const copiedMods = [];
  const oldIds = Array.from(selectedModuleIds);

  // Find min left coordinate to use as relative anchor
  let minLeft = Infinity;
  for (const id of oldIds) {
    const m = getModuleData(id);
    if (m && m.left < minLeft) minLeft = m.left;
  }

  for (const id of oldIds) {
    const m = getModuleData(id);
    if (m) {
      const paramsCopy = JSON.parse(JSON.stringify(m.params));
      delete paramsCopy.__midi_cc;
      copiedMods.push({
        oldId: id,
        type: m.type,
        params: paramsCopy,
        relLeft: m.left - minLeft,
        row: getModuleRowIndex(id)
      });
    }
  }

  // Find cables between copied modules
  const copiedCables = [];
  for (const c of state.cables) {
    if (selectedModuleIds.has(c.fromId) && selectedModuleIds.has(c.toId)) {
      copiedCables.push({
        fromId: c.fromId,
        fromPort: c.fromPort,
        toId: c.toId,
        toPort: c.toPort,
        color: c.color
      });
    }
  }

  clipboardData = {
    modules: copiedMods,
    cables: copiedCables
  };
}

function getPositionUnderCursor() {
  let targetRow = lastActiveRow;
  let hpX = 0;
  let found = false;

  const rowElements = document.querySelectorAll('.rack-row');
  for (const rowEl of rowElements) {
    const rect = rowEl.getBoundingClientRect();
    if (lastMouseY >= rect.top && lastMouseY <= rect.bottom) {
      const idxAttr = parseInt(rowEl.dataset.rowIndex);
      if (!isNaN(idxAttr)) {
        targetRow = idxAttr;
      }
      const bay = rowEl.querySelector('.module-bay');
      if (bay) {
        const bayRect = bay.getBoundingClientRect();
        const clickX = lastMouseX - bayRect.left;
        hpX = Math.round(clickX / 15);
      }
      found = true;
      break;
    }
  }

  if (!found) {
    const rowEl = document.querySelectorAll('.rack-row')[targetRow];
    if (rowEl) {
      const bay = rowEl.querySelector('.module-bay');
      if (bay) {
        const bayRect = bay.getBoundingClientRect();
        const clickX = lastMouseX - bayRect.left;
        hpX = Math.round(clickX / 15);
      }
    }
  }

  if (hpX < 0) hpX = 0;
  return { targetRow, hpX };
}

function pasteModules() {
  if (!clipboardData || clipboardData.modules.length === 0) return;

  // Clear current selection
  clearSelection();

  const newSelection = new Set();
  const idMap = {}; // oldId -> newId

  // Find a baseline left position for pasting relative to mouse cursor
  const { targetRow, hpX } = getPositionUnderCursor();
  lastActiveRow = targetRow;
  const startLeft = hpX;


  // Paste modules
  for (const m of clipboardData.modules) {
    const def = MODULE_DEFS[m.type];
    if (!def) continue;

    // Position offset by relative left
    const hpOffset = Math.round(m.relLeft / 15); // convert px to HP (1 HP = 15px)
    const targetLeft = startLeft + hpOffset;

    const newId = m.type.replace(/-/g, '') + (state.nextId++);
    idMap[m.oldId] = newId;

    // Add module to state
    state.rows[targetRow].push({
      id: newId,
      type: m.type,
      left: snapToHP(targetLeft * 15),
      params: JSON.parse(JSON.stringify(m.params))
    });

    // Build DOM
    const rowDOM = document.querySelectorAll('.rack-row')[targetRow];
    const bay = rowDOM?.querySelector('.module-bay');
    if (bay) {
      const modEl = buildModuleEl(m.type, newId, m.params, snapToHP(targetLeft * 15));
      bay.appendChild(modEl);
    }

    // Resolve collisions immediately
    resolveCollisions(targetRow, newId);

    newSelection.add(newId);
  }

  // Paste cables
  for (const c of clipboardData.cables) {
    const newFrom = idMap[c.fromId];
    const newTo = idMap[c.toId];
    if (newFrom && newTo) {
      state.cables.push({
        fromId: newFrom,
        fromPort: c.fromPort,
        toId: newTo,
        toPort: c.toPort,
        color: c.color
      });
    }
  }

  // Update Selection Highlight
  for (const id of newSelection) {
    selectedModuleIds.add(id);
    $(`mod-${id}`)?.classList.add('selected');
  }

  updateRowWidths();
  redrawCables();
  generateCode();
  pushStateSnapshot();
}

function moveModuleRow(instanceId, delta) {
  const ri = getModuleRowIndex(instanceId);
  if (ri === -1) return;
  const targetRow = ri + delta;
  if (targetRow < 0 || targetRow >= state.rows.length) return;

  const mIdx = state.rows[ri].findIndex(m => m.id === instanceId);
  const [mData] = state.rows[ri].splice(mIdx, 1);

  const def = MODULE_DEFS[mData.type];
  mData.left = findFreePosition(targetRow, def ? def.hp : 6);
  state.rows[targetRow].push(mData);

  const modEl = $(`mod-${instanceId}`);
  if (modEl) {
    modEl.style.left = mData.left + 'px';
    getBay(targetRow)?.appendChild(modEl);
  }

  // Resolve overlaps in target row immediately
  resolveCollisions(targetRow, instanceId);

  refreshRowButtons(); updateRowWidths(); redrawCables(); generateCode();
}

function refreshRowButtons() {
  const multiRow = state.rows.length > 1;
  document.querySelectorAll('.module-row-btns').forEach(b => {
    b.style.display = multiRow ? '' : 'none';
  });
}

// ═══════════════════════════════════════════════════════════════════════
// 6. MODULE DRAGGING, COLLISION RESOLUTION, & RAIL PHYSICS
// ═══════════════════════════════════════════════════════════════════════

let activeDrag = null;

function setupModuleDrag(handleEl, modEl, instanceId) {
  handleEl.addEventListener('pointerdown', e => {
    if (e.target.tagName === 'BUTTON' || e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

    // Ignore clicks on active controls only, allowing drag on screws, labels, and margins
    let cur = e.target;
    while (cur && cur !== handleEl) {
      if (cur.classList.contains('knob') ||
        cur.classList.contains('jack') ||
        cur.classList.contains('switch-control') ||
        cur.classList.contains('computer-reset-btn') ||
        cur.classList.contains('computer-card-slot') ||
        cur.classList.contains('module-del-btn') ||
        cur.classList.contains('pattern-input-wrap')) {
        return;
      }
      cur = cur.parentElement;
    }

    e.preventDefault();
    handleEl.setPointerCapture(e.pointerId);

    const mData = getModuleData(instanceId);
    const rowIdx = getModuleRowIndex(instanceId);
    const def = MODULE_DEFS[mData.type];

    const draggedModules = [];
    if (selectedModuleIds.has(instanceId)) {
      for (const id of selectedModuleIds) {
        const m = getModuleData(id);
        const r = getModuleRowIndex(id);
        const el = $(`mod-${id}`);
        if (m && el) {
          draggedModules.push({
            instanceId: id,
            modEl: el,
            startLeft: m.left || 0,
            rowIdx: r,
            hpWidth: MODULE_DEFS[m.type]?.hp || 6
          });
          el.classList.add('is-dragging');
        }
      }
    } else {
      draggedModules.push({
        instanceId,
        modEl,
        startLeft: mData ? (mData.left || 0) : 0,
        rowIdx,
        hpWidth: def ? def.hp : 6
      });
      modEl.classList.add('is-dragging');
    }

    activeDrag = {
      instanceId,
      modEl,
      startX: e.clientX,
      startLeft: mData ? (mData.left || 0) : 0,
      rowIdx,
      hpWidth: def ? def.hp : 6,
      draggedModules,
      isMulti: draggedModules.length > 1,
      startPositions: state.rows[rowIdx].reduce((map, m) => {
        map[m.id] = m.left || 0;
        return map;
      }, {})
    };

    document.addEventListener('pointermove', handleModuleMove);
    document.addEventListener('pointerup', handleModuleUp);
  });
}

function handleModuleMove(e) {
  if (!activeDrag) return;

  const dx = e.clientX - activeDrag.startX;

  let targetRow = activeDrag.rowIdx;
  const rowElements = document.querySelectorAll('.rack-row');
  for (const rowEl of rowElements) {
    const rect = rowEl.getBoundingClientRect();
    if (e.clientY >= rect.top && e.clientY <= rect.bottom) {
      const idxAttr = parseInt(rowEl.dataset.rowIndex);
      if (!isNaN(idxAttr)) {
        targetRow = idxAttr;
      }
      break;
    }
  }

  if (targetRow !== activeDrag.rowIdx) {
    const currentRi = activeDrag.rowIdx;
    
    if (activeDrag.isMulti) {
      let groupMinLeft = Infinity;
      for (const dm of activeDrag.draggedModules) {
        if (dm.startLeft < groupMinLeft) groupMinLeft = dm.startLeft;
      }
      const shiftLeft = findFreePosition(targetRow, activeDrag.hpWidth) - groupMinLeft;
      
      for (const dm of activeDrag.draggedModules) {
        const idx = state.rows[currentRi].findIndex(m => m.id === dm.instanceId);
        if (idx !== -1) {
          const mData = state.rows[currentRi].splice(idx, 1)[0];
          mData.left = dm.startLeft + shiftLeft;
          state.rows[targetRow].push(mData);
          dm.rowIdx = targetRow;
          dm.startLeft = mData.left;
          const bay = getBay(targetRow);
          if (bay) bay.appendChild(dm.modEl);
        }
      }
      activeDrag.startPositions = state.rows[targetRow].reduce((map, m) => {
        map[m.id] = m.left || 0;
        return map;
      }, {});
      activeDrag.rowIdx = targetRow;
      activeDrag.startX = e.clientX;
      packRowTightly(currentRi);
    } else {
      const idx = state.rows[currentRi].findIndex(m => m.id === activeDrag.instanceId);
      if (idx !== -1) {
        const mData = state.rows[currentRi].splice(idx, 1)[0];
        packRowTightly(currentRi);

        const newLeft = findFreePosition(targetRow, activeDrag.hpWidth);
        mData.left = newLeft;
        state.rows[targetRow].push(mData);
        activeDrag.startPositions = state.rows[targetRow].reduce((map, m) => {
          map[m.id] = m.left || 0;
          return map;
        }, {});
        activeDrag.rowIdx = targetRow;
        activeDrag.startX = e.clientX;
        activeDrag.startLeft = newLeft;

        const bay = getBay(targetRow);
        if (bay) bay.appendChild(activeDrag.modEl);
      }
    }
  }

  const bay = getBay(activeDrag.rowIdx);
  const bayWidth = bay ? (parseInt(bay.style.width) || bay.getBoundingClientRect().width) : 1200;

  if (activeDrag.isMulti) {
    for (const dm of activeDrag.draggedModules) {
      const dragWidthPx = dm.hpWidth * HP;
      const maxLeft = Math.max(0, bayWidth - dragWidthPx);
      let newLeft = dm.startLeft + dx;
      newLeft = Math.max(0, Math.min(maxLeft, newLeft));
      dm.modEl.style.left = newLeft + 'px';
    }
  } else {
    const dragWidthPx = activeDrag.hpWidth * HP;
    const maxLeft = Math.max(0, bayWidth - dragWidthPx);
    let newLeft = activeDrag.startLeft + dx;
    newLeft = Math.max(0, Math.min(maxLeft, newLeft));
    activeDrag.modEl.style.left = newLeft + 'px';
  }

  updateRowWidths();
  redrawCables();
}

function handleModuleUp(e) {
  if (!activeDrag) return;
  document.removeEventListener('pointermove', handleModuleMove);
  document.removeEventListener('pointerup', handleModuleUp);

  const affectedRows = new Set();

  if (activeDrag.isMulti) {
    for (const dm of activeDrag.draggedModules) {
      dm.modEl.classList.remove('is-dragging');
      const mData = getModuleData(dm.instanceId);
      if (mData) {
        mData.left = snapToHP(parseInt(dm.modEl.style.left) || 0);
        dm.modEl.style.left = mData.left + 'px';
      }
      affectedRows.add(dm.rowIdx);
    }
  } else {
    activeDrag.modEl.classList.remove('is-dragging');
    const mData = getModuleData(activeDrag.instanceId);
    if (mData) {
      mData.left = snapToHP(parseInt(activeDrag.modEl.style.left) || 0);
      activeDrag.modEl.style.left = mData.left + 'px';
    }
    affectedRows.add(activeDrag.rowIdx);
  }

  // Resolve overlaps globally for the row to clean up any snapping overlaps
  for (const r of affectedRows) {
    resolveCollisions(r, null);
  }

  activeDrag = null;
  updateRowWidths();
  redrawCables();
  generateCode(); // Only fires on drop — positions are now final
  pushStateSnapshot();
}

// Solid-box slide collision avoidance physics on Eurorack rails (allows gaps)

function packRowTightly(rowIndex) {
  resolveCollisions(rowIndex, null);
}

function resolveCollisions(rowIndex, draggedId) {
  const row = state.rows[rowIndex];
  if (!row) return;

  const draggedM = row.find(m => m.id === draggedId);
  if (!draggedM) {
    // No active drag: only push apart modules that actually overlap.
    // Modules that don't overlap keep their existing positions (gaps preserved).
    const sorted = [...row].sort((a, b) => (a.left || 0) - (b.left || 0));
    let minLeft = 0; // the minimum allowed left edge for the next module
    for (const m of sorted) {
      const mDef = MODULE_DEFS[m.type];
      const mWidth = (mDef ? mDef.hp : 6) * HP;
      // Snap to HP grid but never overlap the previous module's right edge
      m.left = Math.max(minLeft, snapToHP(m.left || 0));
      const el = $('mod-' + m.id);
      if (el) el.style.left = m.left + 'px';
      minLeft = m.left + mWidth; // next module must start at least here
    }
    state.rows[rowIndex] = sorted;
    return;
  }

  const dragDef = MODULE_DEFS[draggedM.type];
  const dragWidth = (dragDef ? dragDef.hp : 6) * HP;
  const rawLeft = (activeDrag && activeDrag.modEl && activeDrag.instanceId === draggedId)
    ? (parseInt(activeDrag.modEl.style.left) || 0)
    : (draggedM.left || 0);

  const startPos = (activeDrag && activeDrag.instanceId === draggedId && activeDrag.startPositions)
    ? activeDrag.startPositions
    : {};

  // Sort by center-X using the static startPositions for stationary modules
  const sorted = [...row].sort((a, b) => {
    const aDef = MODULE_DEFS[a.type];
    const bDef = MODULE_DEFS[b.type];
    const aWidth = (aDef ? aDef.hp : 6) * HP;
    const bWidth = (bDef ? bDef.hp : 6) * HP;
    const aLeft = (a.id === draggedId) ? rawLeft : (startPos[a.id] ?? a.left ?? 0);
    const bLeft = (b.id === draggedId) ? rawLeft : (startPos[b.id] ?? b.left ?? 0);
    return (aLeft + aWidth / 2) - (bLeft + bWidth / 2);
  });

  state.rows[rowIndex] = sorted;

  // Find target dragged index
  const k = sorted.findIndex(m => m.id === draggedId);

  // Position the dragged module
  draggedM.left = rawLeft;
  const draggedEl = $('mod-' + draggedId);
  if (draggedEl) draggedEl.style.left = rawLeft + 'px';

  // Push modules to the left of the dragged module (right-to-left processing)
  for (let i = k - 1; i >= 0; i--) {
    const m = sorted[i];
    const mDef = MODULE_DEFS[m.type];
    const mWidth = (mDef ? mDef.hp : 6) * HP;
    const rightBound = sorted[i + 1].left;
    const origLeft = startPos[m.id] ?? m.left ?? 0;
    m.left = Math.max(0, Math.min(origLeft, rightBound - mWidth));
    const el = $('mod-' + m.id);
    if (el) el.style.left = m.left + 'px';
  }

  // Push modules to the right of the dragged module (left-to-right processing)
  for (let i = k + 1; i < sorted.length; i++) {
    const m = sorted[i];
    const prevM = sorted[i - 1];
    const prevDef = MODULE_DEFS[prevM.type];
    const prevWidth = (prevDef ? prevDef.hp : 6) * HP;
    const leftBound = prevM.left + prevWidth;
    const origLeft = startPos[m.id] ?? m.left ?? 0;
    m.left = Math.max(leftBound, origLeft);
    const el = $('mod-' + m.id);
    if (el) el.style.left = m.left + 'px';
  }
}

// ═══════════════════════════════════════════════════════════════════════
// 8. CONTEXT MENUS (Right-click handling)
// ═══════════════════════════════════════════════════════════════════════

function closeContextMenu() {
  if (currentContextMenu) {
    currentContextMenu.remove();
    currentContextMenu = null;
  }
}

function openDx7ImportForModule(instanceId) {
  const mData = getModuleData(instanceId);
  if (!mData) return;

  const fileInput = document.createElement('input');
  fileInput.type = 'file';
  fileInput.accept = '.syx';
  fileInput.style.display = 'none';
  document.body.appendChild(fileInput);

  fileInput.addEventListener('change', e => {
    const file = e.target.files[0];
    if (!file) {
      fileInput.remove();
      return;
    }
    const reader = new FileReader();
    reader.onload = function (evt) {
      const buf = new Uint8Array(evt.target.result);
      try {
        const voices = parseDx7Bank(buf);
        showVoiceSelectionModal(voices, (voice, voiceIdx) => {
          // Store voice data in this module instance params!
          mData.params.customVoiceData = Array.from(voice.data);
          mData.params.customVoiceName = voice.name.trim();

          // Refresh the module faceplate to display the voice name!
          const oldDom = $(`mod-${instanceId}`);
          if (oldDom) {
            const newDom = buildModuleEl('dx', instanceId, mData.params, parseInt(oldDom.style.left));
            oldDom.replaceWith(newDom);
          }

          redrawCables();
          generateCode();
        });
      } catch (err) {
        alert('Failed to parse DX7 bank: ' + err.message);
      }
      fileInput.remove();
    };
    reader.readAsArrayBuffer(file);
  });
  fileInput.click();
}

function showContextMenu(e) {
  e.preventDefault();
  closeContextMenu();

  // Hit-test: accept clicks on the knob cap OR anywhere in the knob-wrap
  // (label text, wrap padding) so right-click always works.
  const knobEl = e.target.closest('.knob') ||
    (e.target.closest('.knob-wrap') && e.target.closest('.knob-wrap').querySelector('.knob'));
  if (knobEl) {
    const instanceId = knobEl.dataset.instanceId;
    const paramName = knobEl.dataset.param;
    const mData = getModuleData(instanceId);
    if (!mData) return;

    const def = MODULE_DEFS[mData.type];
    const kDef = def?.knobs?.find(k => k.param === paramName);
    if (!kDef) return;

    const menu = el('div', 'context-menu');
    menu.style.left = e.clientX + 'px';
    menu.style.top = e.clientY + 'px';

    const header = el('div', 'context-menu-header', { textContent: `${kDef.label} Options:` });
    menu.appendChild(header);

    // Set Precise Value
    const setValItem = el('div', 'context-menu-item', { textContent: 'Set Precise Value...' });
    setValItem.addEventListener('click', () => {
      closeContextMenu();
      promptPreciseValue(knobEl, instanceId, paramName);
    });
    menu.appendChild(setValItem);

    // Reset to Default
    const resetItem = el('div', 'context-menu-item', { textContent: 'Reset to Default' });
    resetItem.addEventListener('click', () => {
      closeContextMenu();
      const defaultVal = kDef.def;
      knobEl.dataset.val = defaultVal;
      const svg = knobEl.querySelector('svg');
      if (svg) svg.style.transform = `rotate(${valToAngle(defaultVal)}deg)`;
      const valEl = knobEl.closest('.knob-wrap')?.querySelector('.knob-lbl-val');
      if (valEl) {
        valEl.textContent = getDisplayValueStr(modData.type, paramName, defaultVal, instanceId);
      }
      handleParamValueUpdate(instanceId, paramName, defaultVal, knobEl);
      generateCode();
    });
    menu.appendChild(resetItem);

    // CC Mapping separator and items (only for continuous knobs, or all knobs if we want)
    if (!kDef.discrete && !kDef.noMidi) {
      const sep = el('div', 'context-menu-sep');
      menu.appendChild(sep);

      const isMapped = mData.params.__midi_cc && mData.params.__midi_cc[paramName] !== undefined;
      if (isMapped) {
        const entry = mData.params.__midi_cc[paramName];
        const cc = (typeof entry === 'object') ? entry.cc : entry;
        const ch = (typeof entry === 'object') ? entry.ch : 1;
        const infoItem = el('div', 'context-menu-item');
        infoItem.textContent = `Mapped to CC ${cc} ch ${ch}`;
        Object.assign(infoItem.style, { color: '#dfb86c', fontWeight: 'bold', pointerEvents: 'none' });
        menu.appendChild(infoItem);

        const changeItem = el('div', 'context-menu-item', { textContent: 'Change MIDI CC...' });
        changeItem.addEventListener('click', () => {
          closeContextMenu();
          promptCcMapping(instanceId, paramName, entry);
        });
        menu.appendChild(changeItem);

        const unmapItem = el('div', 'context-menu-item context-menu-item-danger', { textContent: 'Unmap MIDI CC' });
        unmapItem.addEventListener('click', () => {
          delete mData.params.__midi_cc[paramName];
          if (Object.keys(mData.params.__midi_cc).length === 0) {
            delete mData.params.__midi_cc;
          }
          rebuildModuleDOM(instanceId);
          redrawCables(); generateCode();
          closeContextMenu();
        });
        menu.appendChild(unmapItem);
      } else {
        const mapItem = el('div', 'context-menu-item', { textContent: 'Map to MIDI CC...' });
        mapItem.addEventListener('click', () => {
          closeContextMenu();
          promptCcMapping(instanceId, paramName);
        });
        menu.appendChild(mapItem);
      }
    }

    document.body.appendChild(menu);
    currentContextMenu = menu;
    return;
  }

  const jackEl = e.target.closest('.jack');
  if (jackEl) {
    const instanceId = jackEl.dataset.instanceId;
    const portId = jackEl.dataset.portId;
    const allMods = state.rows.flat();

    // Find all cables touching this jack (either end)
    const touchingCables = state.cables.filter(c =>
      (c.fromId === instanceId && c.fromPort === portId) ||
      (c.toId === instanceId && c.toPort === portId)
    );

    if (touchingCables.length === 0) return; // nothing to disconnect

    const menu = el('div', 'context-menu');
    menu.style.left = e.clientX + 'px';
    menu.style.top = e.clientY + 'px';

    const header = el('div', 'context-menu-header', { textContent: 'Disconnect:' });
    menu.appendChild(header);

    for (const cable of touchingCables) {
      // Describe the other end of the cable
      const otherId = cable.fromId === instanceId && cable.fromPort === portId ? cable.toId : cable.fromId;
      const otherPort = cable.fromId === instanceId && cable.fromPort === portId ? cable.toPort : cable.fromPort;
      const otherMod = allMods.find(m => m.id === otherId);
      const otherDef = otherMod ? MODULE_DEFS[otherMod.type] : null;
      const otherLabel = otherDef ? otherDef.title : otherId;
      const portLabel = otherPort.replace(/-/g, ' ').toUpperCase();

      const item = el('div', 'context-menu-item');
      item.textContent = `→ ${otherLabel} · ${portLabel}`;
      item.addEventListener('click', () => {
        state.cables = state.cables.filter(c =>
          !(c.fromId === cable.fromId && c.fromPort === cable.fromPort &&
            c.toId === cable.toId && c.toPort === cable.toPort));
        redrawCables(); generateCode();
        closeContextMenu();
      });
      menu.appendChild(item);
    }

    if (touchingCables.length > 1) {
      const sep = el('div', 'context-menu-sep');
      menu.appendChild(sep);
      const allItem = el('div', 'context-menu-item context-menu-item-danger', { textContent: 'Disconnect All' });
      allItem.addEventListener('click', () => {
        state.cables = state.cables.filter(c =>
          !((c.fromId === instanceId && c.fromPort === portId) ||
            (c.toId === instanceId && c.toPort === portId)));
        redrawCables(); generateCode();
        closeContextMenu();
      });
      menu.appendChild(allItem);
    }

    document.body.appendChild(menu);
    currentContextMenu = menu;
    return;
  }

  const modEl = e.target.closest('.module');
  const bayEl = e.target.closest('.module-bay');

  if (modEl) {
    // Module Right-click Context Menu
    const instanceId = modEl.dataset.instanceId;
    const def = MODULE_DEFS[modEl.dataset.type];

    const menu = el('div', 'context-menu');
    menu.style.left = e.clientX + 'px';
    menu.style.top = e.clientY + 'px';

    const helpItem = el('div', 'context-menu-item', { textContent: 'Help / Info...' });
    helpItem.addEventListener('click', () => {
      showModuleHelpModal(modEl.dataset.type);
      closeContextMenu();
    });
    menu.appendChild(helpItem);

    const sepHelp = el('div', 'context-menu-sep');
    menu.appendChild(sepHelp);

    if (modEl.dataset.type === 'dx') {
      const importItem = el('div', 'context-menu-item', { textContent: 'Import DX7 Voice (.syx)...' });
      importItem.addEventListener('click', () => {
        openDx7ImportForModule(instanceId);
        closeContextMenu();
      });
      menu.appendChild(importItem);

      const mData = getModuleData(instanceId);
      if (mData && mData.params.customVoiceData) {
        const clearItem = el('div', 'context-menu-item context-menu-item-danger', { textContent: 'Clear Custom Voice' });
        clearItem.addEventListener('click', () => {
          delete mData.params.customVoiceData;
          delete mData.params.customVoiceName;
          const oldDom = $(`mod-${instanceId}`);
          if (oldDom) {
            const newDom = buildModuleEl('dx', instanceId, mData.params, parseInt(oldDom.style.left));
            oldDom.replaceWith(newDom);
          }
          redrawCables(); generateCode(); closeContextMenu();
        });
        menu.appendChild(clearItem);
      }
      const sep = el('div', 'context-menu-sep');
      menu.appendChild(sep);
    }

    if (def?.deletable !== false) {
      const delItem = el('div', 'context-menu-item', { textContent: 'Delete Module' });
      delItem.addEventListener('click', () => { deleteModule(instanceId); pushStateSnapshot(); closeContextMenu(); });
      menu.appendChild(delItem);
    }

    if (state.rows.length > 1) {
      const upItem = el('div', 'context-menu-item', { textContent: 'Move Row Up' });
      upItem.addEventListener('click', () => { moveModuleRow(instanceId, -1); closeContextMenu(); });
      const dnItem = el('div', 'context-menu-item', { textContent: 'Move Row Down' });
      dnItem.addEventListener('click', () => { moveModuleRow(instanceId, 1); closeContextMenu(); });
      menu.appendChild(upItem);
      menu.appendChild(dnItem);
    }

    document.body.appendChild(menu);
    currentContextMenu = menu;
  } else if (bayEl) {
    // Empty Bay Right-click Context Menu (Place module directly at snapped clicked position)
    const rowIndex = parseInt(bayEl.dataset.row);
    lastActiveRow = rowIndex; // Track which row the user is working in
    const rect = bayEl.getBoundingClientRect();
    const clickX = e.clientX - rect.left + $('rackViewport').scrollLeft;
    const hpX = snapToHP(clickX);

    showModuleMenu(e.clientX, e.clientY, rowIndex, hpX);
  }
}

function rebuildModuleDOM(instanceId) {
  const mData = getModuleData(instanceId);
  if (!mData) return;
  const oldDom = $(`mod-${instanceId}`);
  if (oldDom) {
    const newDom = buildModuleEl(mData.type, instanceId, mData.params, parseInt(oldDom.style.left));
    oldDom.replaceWith(newDom);
  }
}

function getNextSuggestedCc() {
  const FLARE_CC_RESERVED = new Set([
    0, 1, 2, 4, 6, 7, 10, 11, 32, 33, 38, 64, 65, 66, 67, 68, 96, 97, 98, 99, 100, 101, 120, 121, 122, 123, 124, 125, 126, 127
  ]);
  const used = new Set();
  for (const row of state.rows) {
    if (!row) continue;
    for (const m of row) {
      if (m.params && m.params.__midi_cc) {
        for (const p of Object.keys(m.params.__midi_cc)) {
          const entry = m.params.__midi_cc[p];
          if (entry && entry.cc !== undefined && entry.ch !== undefined) {
            used.add((entry.ch << 7) | entry.cc);
          }
        }
      }
    }
  }
  const startCh = state.flareMidiChannel || 16;
  let ch = startCh;
  while (ch >= 1) {
    for (let cc = 14; cc <= 119; cc++) {
      if (!FLARE_CC_RESERVED.has(cc) && !used.has((ch << 7) | cc)) {
        return { cc, ch };
      }
    }
    ch--;
  }
  return { cc: 14, ch: startCh };
}

function promptCcMapping(instanceId, paramName, currentCc = null) {
  const mData = getModuleData(instanceId);
  if (!mData) return;
  const def = MODULE_DEFS[mData.type];
  const kDef = def?.knobs?.find(k => k.param === paramName);
  const label = kDef ? kDef.label : paramName;

  let defaultVal = '14, 16';
  if (currentCc !== null) {
    const cc = (typeof currentCc === 'object') ? currentCc.cc : currentCc;
    const ch = (typeof currentCc === 'object') ? currentCc.ch : 1;
    defaultVal = `${cc}, ${ch}`;
  } else {
    const suggested = getNextSuggestedCc();
    defaultVal = `${suggested.cc}, ${suggested.ch}`;
  }

  const msg = `Enter MIDI CC number (0-127) and Channel (1-16) for ${label}, separated by a comma (e.g. 14, 16):`;
  const input = prompt(msg, defaultVal);
  if (input === null) return; // cancelled

  const parts = input.split(',');
  const ccNum = parseInt(parts[0]?.trim(), 10);
  let chNum = parseInt(parts[1]?.trim(), 10);
  if (Number.isNaN(chNum)) {
    chNum = state.flareMidiChannel || 16;
  }

  if (Number.isNaN(ccNum) || ccNum < 0 || ccNum > 127) {
    alert('Invalid CC number. Must be an integer between 0 and 127.');
    return;
  }
  if (chNum < 1 || chNum > 16) {
    alert('Invalid MIDI channel. Must be an integer between 1 and 16.');
    return;
  }

  // Assign CC mapping
  mData.params.__midi_cc = mData.params.__midi_cc || {};
  mData.params.__midi_cc[paramName] = { cc: ccNum, ch: chNum };

  rebuildModuleDOM(instanceId);
  redrawCables();
  generateCode();
}
function showModuleMenu(clientX, clientY, rowIndex, hpX) {
  closeContextMenu();
  
  const menu = el('div', 'context-menu');
  // Initially position off-screen/hidden to allow accurate measurement before display
  menu.style.visibility = 'hidden';
  menu.style.left = '0px';
  menu.style.top = '0px';

  // Prevent clicks and typing from closing the menu or triggering other things
  menu.addEventListener('pointerdown', e => e.stopPropagation());
  menu.addEventListener('mousedown', e => e.stopPropagation());
  menu.addEventListener('click', e => e.stopPropagation());

  // Add search input at the top
  const searchInput = el('input', 'context-menu-search', {
    type: 'text',
    placeholder: 'Search modules...',
    style: 'background: #0d0c0b; border: none; border-bottom: 1px solid #2d2c2a; color: #dfb86c; font: 11px ui-monospace, Menlo, Monaco, Consolas, monospace; padding: 6px 14px; width: 100%; box-sizing: border-box; outline: none; display: block; margin-bottom: 4px;'
  });
  menu.appendChild(searchInput);

  const listEl = el('div', 'context-menu-list');
  menu.appendChild(listEl);

  // Helper to build default categorized menu
  function buildCategorizedMenu() {
    listEl.innerHTML = '';
    listEl.classList.remove('searching');
    for (const cat of CATEGORY_ORDER) {
      const mods = Object.entries(MODULE_DEFS).filter(([k, v]) => v.category === cat).sort((a, b) => a[1].title.localeCompare(b[1].title));
      if (mods.length === 0) continue;

      const catHeader = el('div', 'context-menu-item');
      catHeader.textContent = CATEGORY_LABELS[cat];

      const arrow = el('span', { textContent: ' ▶', style: 'opacity: 0.5; font-size: 8px;' });
      catHeader.appendChild(arrow);

      const submenu = el('div', 'context-menu-submenu');
      for (const [key, def] of mods) {
        const modItem = el('div', 'context-menu-item', { textContent: def.title });
        modItem.dataset.moduleKey = key;
        modItem.addEventListener('click', () => {
          addModuleToRow(key, rowIndex, {}, { left: hpX });
          closeContextMenu();
        });
        submenu.appendChild(modItem);
      }

      catHeader.appendChild(submenu);
      listEl.appendChild(catHeader);
    }

    // Attach mouseenter bounds-checking for submenus
    const winW = window.innerWidth;
    const rect = menu.getBoundingClientRect();
    const catItems = Array.from(listEl.querySelectorAll('.context-menu-item'));
    for (const item of catItems) {
      const submenu = item.querySelector('.context-menu-submenu');
      if (!submenu) continue;
      item.addEventListener('mouseenter', () => {
        // Show submenu temporarily offscreen to measure it
        submenu.style.display = 'block';
        submenu.style.visibility = 'hidden';
        const subRect = submenu.getBoundingClientRect();
        if (subRect.right > winW) {
          // Position on the left side of the parent menu
          submenu.style.left = `-${rect.width}px`;
        } else {
          submenu.style.left = '100%';
        }
        submenu.style.visibility = '';
      });
      item.addEventListener('mouseleave', () => {
        submenu.style.display = '';
      });
    }
  }

  // Helper to build filtered flat list
  function buildFilteredMenu(query) {
    listEl.innerHTML = '';
    listEl.classList.add('searching');
    const lowerQuery = query.toLowerCase();

    // Sort all modules alphabetically by title
    const allMods = Object.entries(MODULE_DEFS).sort((a, b) => a[1].title.localeCompare(b[1].title));

    let count = 0;
    for (const [key, def] of allMods) {
      // Allow searching by key or by title
      if (key.toLowerCase().includes(lowerQuery) || def.title.toLowerCase().includes(lowerQuery)) {
        const item = el('div', 'context-menu-item', { textContent: def.title });
        item.dataset.moduleKey = key;
        item.addEventListener('click', () => {
          addModuleToRow(key, rowIndex, {}, { left: hpX });
          closeContextMenu();
        });
        listEl.appendChild(item);
        count++;
      }
    }

    if (count === 0) {
      const noResults = el('div', 'context-menu-header', { textContent: 'No matching modules' });
      noResults.style.borderBottom = 'none';
      noResults.style.textAlign = 'center';
      listEl.appendChild(noResults);
    }
  }

  // Initial build
  buildCategorizedMenu();

  // Search input events
  searchInput.addEventListener('input', e => {
    const val = e.target.value.trim();
    if (val === '') {
      buildCategorizedMenu();
    } else {
      buildFilteredMenu(val);
    }
  });

  searchInput.addEventListener('keydown', e => {
    if (e.key === 'Escape') {
      closeContextMenu();
      e.stopPropagation();
      e.preventDefault();
    } else if (e.key === 'Enter') {
      const firstItem = listEl.querySelector('.context-menu-item:not(:has(.context-menu-submenu))') || listEl.querySelector('.context-menu-submenu .context-menu-item');
      if (firstItem && firstItem.dataset.moduleKey) {
        addModuleToRow(firstItem.dataset.moduleKey, rowIndex, {}, { left: hpX });
        closeContextMenu();
        e.stopPropagation();
        e.preventDefault();
      }
    }
  });

  document.body.appendChild(menu);
  currentContextMenu = menu;

  // Position detection and screen boundary handling
  const rect = menu.getBoundingClientRect();
  const winW = window.innerWidth;
  const winH = window.innerHeight;
  let left = clientX;
  let top = clientY;

  if (left + rect.width > winW) {
    left = Math.max(10, winW - rect.width - 10);
  }
  if (top + rect.height > winH) {
    top = Math.max(10, winH - rect.height - 10);
  }

  menu.style.left = left + 'px';
  menu.style.top = top + 'px';
  menu.style.visibility = '';

  // Auto-focus search input
  setTimeout(() => searchInput.focus(), 10);
}
// ═══════════════════════════════════════════════════════════════════════
// 9. KNOBS & INTERACTION
// ═══════════════════════════════════════════════════════════════════════

let knobState = null;

function handleKnobDown(e) {
  e.stopPropagation();
  e.preventDefault();
  const knob = e.currentTarget;
  knob.setPointerCapture(e.pointerId);

  const modData = getModuleData(knob.dataset.instanceId);
  const param = knob.dataset.param;
  const startVal = modData ? (modData.params[param] ?? 2048) : 2048;

  knobState = {
    knob, svg: knob.querySelector('svg'),
    instanceId: knob.dataset.instanceId,
    param, startVal, startY: e.clientY,
    val: startVal,
  };

  knob.addEventListener('pointermove', handleKnobMove);
  knob.addEventListener('pointerup', handleKnobUp);
}

function handleKnobMove(e) {
  if (!knobState || e.currentTarget !== knobState.knob) return;
  const dy = knobState.startY - e.clientY;
  const sensitivity = e.shiftKey ? 1 : 8;
  const val = Math.max(0, Math.min(4095, knobState.startVal + dy * sensitivity));
  knobState.val = val;
  knobState.knob.dataset.val = val;
  if (knobState.svg) knobState.svg.style.transform = `rotate(${valToAngle(val)}deg)`;

  handleParamValueUpdate(knobState.instanceId, knobState.param, val, knobState.knob);
  generateCode(true);
}

function handleKnobDblClick(e) {
  e.stopPropagation();
  const knob = e.currentTarget;
  const instanceId = knob.dataset.instanceId;
  const param = knob.dataset.param;
  const modData = getModuleData(instanceId);
  if (!modData) return;
  const def = MODULE_DEFS[modData.type];
  if (!def) return;
  const kDef = def.knobs.find(k => k.param === param);
  if (!kDef) return;
  const defaultVal = kDef.def !== undefined ? kDef.def : 2048;

  knob.dataset.val = defaultVal;
  const svg = knob.querySelector('svg');
  if (svg) svg.style.transform = `rotate(${valToAngle(defaultVal)}deg)`;

  handleParamValueUpdate(instanceId, param, defaultVal, knob);
  generateCode();
}

function promptPreciseValue(knob, instanceId, param) {
  const modData = getModuleData(instanceId);
  if (!modData) return;
  const def = MODULE_DEFS[modData.type];
  if (!def) return;
  const kDef = def.knobs.find(k => k.param === param);
  if (!kDef) return;

  const currentVal = modData.params[param] !== undefined ? modData.params[param] : kDef.def;

  let promptText = `Enter precise value for ${kDef.label} (0 to 4095):\nCurrently: ${currentVal}`;
  if (kDef.discrete) {
    promptText += `\nDiscrete options: ${kDef.discrete.join(', ')}`;
  }

  const userInput = prompt(promptText, currentVal);
  if (userInput === null) return; // Cancelled

  let val;
  if (kDef.discrete) {
    const cleanInput = userInput.trim().toLowerCase();
    const strOptions = kDef.discrete.map(x => String(x).toLowerCase());
    const idx = strOptions.indexOf(cleanInput);
    if (idx !== -1) {
      if (kDef.discrete.length === 2) {
        val = idx === 0 ? 0 : 4095;
      } else {
        val = Math.round((idx / (kDef.discrete.length - 1)) * 4095);
      }
    } else {
      const parsedIdx = parseInt(userInput, 10);
      if (!isNaN(parsedIdx) && parsedIdx >= 0 && parsedIdx < kDef.discrete.length) {
        if (kDef.discrete.length === 2) {
          val = parsedIdx === 0 ? 0 : 4095;
        } else {
          val = Math.round((parsedIdx / (kDef.discrete.length - 1)) * 4095);
        }
      } else {
        val = parseInt(userInput, 10);
      }
    }
  } else {
    val = parseInt(userInput, 10);
  }

  if (isNaN(val) || val < 0 || val > 4095) {
    alert("Invalid value. Must be a number between 0 and 4095.");
    return;
  }

  knob.dataset.val = val;
  const svg = knob.querySelector('svg');
  if (svg) svg.style.transform = `rotate(${valToAngle(val)}deg)`;

  const valEl = knob.closest('.knob-wrap')?.querySelector('.knob-lbl-val');
  if (valEl) {
    valEl.textContent = getDisplayValueStr(modData.type, param, val, instanceId);
  }

  handleParamValueUpdate(instanceId, param, val, knob);
  generateCode();
}

function handleParamValueUpdate(instanceId, param, val, knobEl) {
  const modData = getModuleData(instanceId);
  if (!modData) return;
  modData.params[param] = val;

  // Real-time step sequencer dimming updates
  if (param === 'steps' && modData.type === 'drum-seq') {
    const moduleEl = knobEl.closest('.module');
    if (moduleEl) {
      const stepsVal = getKnobValue('drum-seq', 'steps', val);
      const stepBtns = moduleEl.querySelectorAll('.drum-seq-step-btn');
      stepBtns.forEach(btn => {
        const stepIdx = parseInt(btn.dataset.step);
        if (!isNaN(stepIdx)) {
          if (stepIdx >= stepsVal) {
            btn.classList.add('dimmed');
          } else {
            btn.classList.remove('dimmed');
          }
        }
      });
    }
  }

  // Real-time parameter value display updates
  const valEl = knobEl.parentNode.querySelector('.knob-lbl-val');
  if (valEl) {
    const displayVal = getDisplayValueStr(modData.type, param, val, instanceId);
    valEl.textContent = displayVal;

    // Update wrapper tooltip
    const wrap = knobEl.parentNode;
    const nameEl = wrap.querySelector('.knob-lbl-name');
    const labelText = nameEl ? nameEl.textContent : param;
    wrap.title = `${labelText}: ${displayVal}`;
  }

  // Real-time audio module label refreshes
  if (modData.type === 'audio') {
    const moduleEl = knobEl.closest('.module');
    if (moduleEl) {
      const knobs = moduleEl.querySelectorAll('.knob-wrap');
      knobs.forEach(kw => {
        const kEl = kw.querySelector('[data-param]');
        if (kEl) {
          const pName = kEl.dataset.param;
          const pVal = modData.params[pName] ?? 0;
          const vEl = kw.querySelector('.knob-lbl-val');
          if (vEl) {
            vEl.textContent = getDisplayValueStr('audio', pName, pVal, instanceId);
          }
        }
      });
    }
  }

  // Reactive DX7 bank/preset updates
  if (param === 'bank' && modData.type === 'dx') {
    const moduleEl = knobEl.closest('.module');
    if (moduleEl) {
      const presetKnob = moduleEl.querySelector('[data-param="preset"]');
      if (presetKnob) {
        const pValEl = presetKnob.parentNode.querySelector('.knob-lbl-val');
        const rawPreset = modData.params.preset ?? 0;
        if (pValEl) {
          pValEl.textContent = getDisplayValueStr('dx', 'preset', rawPreset, instanceId);
        }
      }
    }
  }

  const mapKey = `${instanceId}.${param}`;
  const map = knobConstantMap[mapKey];
  const scaledVal = getConstantValueForKnob(modData, param, val);

  // MIDI CC mapped knob: send standard Control Change (0xB0) and skip CMD_UPDATE_CONST.
  const ccMappings = modData.params.__midi_cc;
  if (midiOut && ccMappings && ccMappings[param] !== undefined) {
    const entry = ccMappings[param];
    const ccNum = (typeof entry === 'object') ? entry.cc : entry;
    const chNum = (typeof entry === 'object') ? entry.ch : 1;
    const statusByte = 0xB0 | ((chNum - 1) & 0x0F);
    let ccVal = val;
    if (modData.type === 'clock' && param === 'bpm') {
      ccVal = bpmToTempoRaw(getKnobValue('clock', 'bpm', val));
    }
    // Always send standard 7-bit CC to avoid the 14-bit cross-core ISR race in the firmware.
    const ccVal7  = Math.round(ccVal / 4095 * 127) & 0x7F;
    midiOut.send([statusByte, ccNum & 0x7F, ccVal7]);
    return;
  }

  if (midiOut && lastUploadedSnapshot && map && map.size === 4) {
    const constIdx = map.const_idx;
    const targetOffset = map.byte_offset;

    lastUploadedSnapshot[targetOffset] = scaledVal & 0xFF;
    lastUploadedSnapshot[targetOffset + 1] = (scaledVal >> 8) & 0xFF;
    lastUploadedSnapshot[targetOffset + 2] = (scaledVal >> 16) & 0xFF;
    lastUploadedSnapshot[targetOffset + 3] = (scaledVal >> 24) & 0xFF;

    const newCrc32 = Lens.crc32(lastUploadedSnapshot.slice(0, lastUploadedSnapshot.length - 4));

    lastUploadedSnapshot[lastUploadedSnapshot.length - 4] = newCrc32 & 0xFF;
    lastUploadedSnapshot[lastUploadedSnapshot.length - 3] = (newCrc32 >> 8) & 0xFF;
    lastUploadedSnapshot[lastUploadedSnapshot.length - 2] = (newCrc32 >> 16) & 0xFF;
    lastUploadedSnapshot[lastUploadedSnapshot.length - 1] = (newCrc32 >> 24) & 0xFF;

    const payload = new Uint8Array(11);
    payload[0] = constIdx;
    payload[1] = scaledVal & 0xFF;
    payload[2] = (scaledVal >> 8) & 0xFF;
    payload[3] = (scaledVal >> 16) & 0xFF;
    payload[4] = (scaledVal >> 24) & 0xFF;
    payload[5] = targetOffset & 0xFF;
    payload[6] = (targetOffset >> 8) & 0xFF;
    payload[7] = newCrc32 & 0xFF;
    payload[8] = (newCrc32 >> 8) & 0xFF;
    payload[9] = (newCrc32 >> 16) & 0xFF;
    payload[10] = (newCrc32 >> 24) & 0xFF;

    midiOut.send([...Lens.frame(Lens.CMD.UPDATE_CONST, payload)]);
  }
}

function handleKnobUp(e) {
  if (!knobState) return;
  const knob = knobState.knob;
  const { instanceId, param, val } = knobState;
  knob.removeEventListener('pointermove', handleKnobMove);
  knob.removeEventListener('pointerup', handleKnobUp);
  knobState = null;

  // Check if this knob is CC-mapped
  const modData = getModuleData(instanceId);
  const ccMappings = modData && modData.params.__midi_cc;
  const entry = ccMappings && ccMappings[param] !== undefined ? ccMappings[param] : null;

  if (midiOut && entry !== null) {
    const ccNum = (typeof entry === 'object') ? entry.cc : entry;
    const chNum = (typeof entry === 'object') ? entry.ch : 1;
    const statusByte = 0xB0 | ((chNum - 1) & 0x0F);
    // Send a final confirming CC at release so the value is locked in.
    let ccVal = val;
    if (modData.type === 'clock' && param === 'bpm') {
      ccVal = bpmToTempoRaw(getKnobValue('clock', 'bpm', val));
    }
    const ccVal7 = Math.round(ccVal / 4095 * 127) & 0x7F;
    midiOut.send([statusByte, ccNum & 0x7F, ccVal7]);
    // Tell triggerLiveUpdate to silently accept the new compiledSnapshot
    // (updated :init values) without a WRITE_STATE upload that would cause a glitch.
    skipNextLiveUpload = true;
    triggerLiveUpdate();
  }

  generateCode();
}

// ═══════════════════════════════════════════════════════════════════════
// 10. CABLE OVERLAY (Fixed SVG Screen Space)
// ═══════════════════════════════════════════════════════════════════════

let cableDrag = null;

function cableColor(fromType, fromPort, toType, toPort) {
  const fType = (fromType || '').toLowerCase();
  const fPort = (fromPort || '').toLowerCase();
  const tType = (toType || '').toLowerCase();
  const tPort = (toPort || '').toLowerCase();
  const all = [fType, fPort, tType, tPort].join(' ');

  // Pitch / V/OCT -> Green
  if (all.includes('v-oct') || all.includes('pitch') || fPort === 'note' || tPort === 'note') {
    return '#4cd137';
  }
  // Clocks / Gates / Triggers -> Gold/Yellow
  if (all.includes('trig') || all.includes('gate') || all.includes('clock') || all.includes('sync') || all.includes('pulse') || fPort === 'euclid' || fPort === 'every') {
    return '#ffb900';
  }
  // Audio -> Coral Red
  const audioModules = ['sine', 'triangle', 'saw', 'square', 'phasor', 'wt', 'noise', 'pluck', 'kick', 'snare', 'hat', 'dx', 'delay', 'reverb', 'lpf', 'hpf', 'vcf', 'lpg', 'tape-looper', 'tape-delay'];
  if (all.includes('audio') || fPort === 'out' || fPort === 'out1' || fPort === 'out2' || fPort === 'lp' || fPort === 'hp' || fPort === 'bp' || fPort === 'notch' || audioModules.includes(fType) || audioModules.includes(tType)) {
    return '#ff4d4d';
  }
  // Modulations / CV -> Neon Cyan
  return '#00d2ff';
}

function handleJackDown(e) {
  if (e.button !== 0) return; // Only trigger for left clicks

  e.stopPropagation();
  e.preventDefault();
  const jack = e.currentTarget;
  jack.setPointerCapture(e.pointerId);

  const iId = jack.dataset.instanceId;
  const pId = jack.dataset.portId;
  const dir = jack.dataset.direction;

  // Clicking an occupied INPUT lifts the cable, holding the output end free for re-patch
  if (dir === 'input') {
    const cIdx = state.cables.findIndex(c => c.toId === iId && c.toPort === pId);
    if (cIdx !== -1) {
      const removed = state.cables.splice(cIdx, 1)[0];
      const fromJack = $(`jack-${removed.fromId}-output-${removed.fromPort}`);
      if (fromJack) {
        cableDrag = {
          fromId: removed.fromId, fromPort: removed.fromPort,
          fromEl: fromJack, curX: e.clientX, curY: e.clientY,
        };
        fromJack.classList.add('active');
        document.addEventListener('pointermove', handleJackMove);
        document.addEventListener('pointerup', handleJackUp);
        redrawCables(); generateCode();
      }
      return;
    }
  }

  // Clicking an occupied OUTPUT: lift the last cable's output end, holding it free to re-patch to another output
  if (dir === 'output') {
    const existingCables = state.cables.filter(c => c.fromId === iId && c.fromPort === pId);
    if (existingCables.length > 0) {
      const removed = existingCables[existingCables.length - 1];
      state.cables = state.cables.filter(c => c !== removed);
      const toJack = $(`jack-${removed.toId}-input-${removed.toPort}`);
      if (toJack) {
        cableDrag = {
          fromId: removed.toId, fromPort: removed.toPort,
          fromEl: toJack, curX: e.clientX, curY: e.clientY,
        };
        toJack.classList.add('active');
        document.addEventListener('pointermove', handleJackMove);
        document.addEventListener('pointerup', handleJackUp);
        redrawCables(); generateCode();
      }
      return;
    }
  }

  // Default: start a new cable from this jack
  cableDrag = { fromId: iId, fromPort: pId, fromEl: jack, curX: e.clientX, curY: e.clientY };
  jack.classList.add('active');
  document.addEventListener('pointermove', handleJackMove);
  document.addEventListener('pointerup', handleJackUp);
}

function handleJackMove(e) {
  if (!cableDrag) return;
  cableDrag.curX = e.clientX; cableDrag.curY = e.clientY;

  document.querySelectorAll('.jack.snap-target').forEach(j => j.classList.remove('snap-target'));
  document.querySelectorAll('.jack.drag-incompatible').forEach(j => j.classList.remove('drag-incompatible'));
  const snap = findSnapJack(e.clientX, e.clientY);
  if (snap) snap.classList.add('snap-target');

  // Dim jacks that cannot be connected to from the current drag source
  document.querySelectorAll('.jack').forEach(j => {
    if (j === cableDrag.fromEl) return;
    if (!canConnect(cableDrag.fromEl, j)) {
      j.classList.add('drag-incompatible');
    }
  });

  redrawCables();
}

function handleJackUp(e) {
  if (!cableDrag) return;
  document.removeEventListener('pointermove', handleJackMove);
  document.removeEventListener('pointerup', handleJackUp);

  try {
    if (cableDrag.fromEl) cableDrag.fromEl.releasePointerCapture(e.pointerId);
  } catch (err) { }

  if (cableDrag.fromEl) cableDrag.fromEl.classList.remove('active');
  document.querySelectorAll('.jack.snap-target').forEach(j => j.classList.remove('snap-target'));
  document.querySelectorAll('.jack.drag-incompatible').forEach(j => j.classList.remove('drag-incompatible'));

  const snap = findSnapJack(e.clientX, e.clientY);
  if (snap && canConnect(cableDrag.fromEl, snap)) {
    const srcJack = cableDrag.fromEl.dataset.direction === 'output' ? cableDrag.fromEl : snap;
    const destJack = cableDrag.fromEl.dataset.direction === 'input' ? cableDrag.fromEl : snap;

    state.cables = state.cables.filter(c => !(c.toId === destJack.dataset.instanceId && c.toPort === destJack.dataset.portId));

    state.cables.push({
      fromId: srcJack.dataset.instanceId, fromPort: srcJack.dataset.portId,
      toId: destJack.dataset.instanceId, toPort: destJack.dataset.portId,
      color: cableColor(srcJack.dataset.instanceId, srcJack.dataset.portId,
        destJack.dataset.instanceId, destJack.dataset.portId),
    });
  }

  cableDrag = null;
  redrawCables(); generateCode();
  pushStateSnapshot();
}

function canConnect(j1, j2) {
  if (j1.dataset.direction === j2.dataset.direction) return false;
  if (j1.dataset.instanceId === j2.dataset.instanceId) return false;
  return true;
}

// Find closest jack socket to screen coordinate
function findSnapJack(x, y) {
  let best = null, bestDist = 35; // Increased snap target range to 35px for easier magnetic patching
  document.querySelectorAll('.jack').forEach(j => {
    if (cableDrag && j === cableDrag.fromEl) return;
    const r = j.getBoundingClientRect();
    const cx = r.left + r.width / 2, cy = r.top + r.height / 2;
    const d = Math.hypot(x - cx, y - cy);
    if (d < bestDist) { bestDist = d; best = j; }
  });
  return best;
}

// Fixed canvas screen space render
function redrawCables() {
  const svg = $('cableSvg');
  while (svg.firstChild) svg.firstChild.remove();

  // Count connections per jack to distribute stacked endpoints and sags
  const jackCounts = {};
  for (const c of state.cables) {
    const fromKey = `${c.fromId}-${c.fromPort}`;
    const toKey = `${c.toId}-${c.toPort}`;
    jackCounts[fromKey] = (jackCounts[fromKey] || 0) + 1;
    jackCounts[toKey] = (jackCounts[toKey] || 0) + 1;
  }

  const jackCurrentIndex = {};

  for (const c of state.cables) {
    const fromEl = $(`jack-${c.fromId}-output-${c.fromPort}`);
    const toEl = $(`jack-${c.toId}-input-${c.toPort}`);
    if (fromEl && toEl) {
      const fromKey = `${c.fromId}-${c.fromPort}`;
      const toKey = `${c.toId}-${c.toPort}`;

      const fromCount = jackCounts[fromKey] || 1;
      const toCount = jackCounts[toKey] || 1;

      const fromIdx = jackCurrentIndex[fromKey] || 0;
      jackCurrentIndex[fromKey] = fromIdx + 1;

      const toIdx = jackCurrentIndex[toKey] || 0;
      jackCurrentIndex[toKey] = toIdx + 1;

      let dim = false;
      let glow = false;
      if (hoveredJack) {
        const matchesFrom = (c.fromId === hoveredJack.instanceId && c.fromPort === hoveredJack.portId);
        const matchesTo = (c.toId === hoveredJack.instanceId && c.toPort === hoveredJack.portId);
        if (matchesFrom || matchesTo) {
          glow = true;
        } else {
          dim = true;
        }
      }

      drawCablePath(svg, fromEl, toEl, c.color, false, fromIdx, fromCount, toIdx, toCount, dim, glow);
    }
  }

  if (cableDrag) {
    const r = cableDrag.fromEl.getBoundingClientRect();
    const svgRect = svg.getBoundingClientRect();
    const x1 = r.left + r.width / 2 - svgRect.left;
    const y1 = r.top + r.height / 2 - svgRect.top;
    const curX = cableDrag.curX - svgRect.left;
    const curY = cableDrag.curY - svgRect.top;
    drawCablePathXY(svg, x1, y1, curX, curY, '#666', true, 0);
  }
}

function drawCablePath(svg, fromEl, toEl, color, dashed, fromIdx = 0, fromCount = 1, toIdx = 0, toCount = 1, dim = false, glow = false) {
  const r1 = fromEl.getBoundingClientRect();
  const r2 = toEl.getBoundingClientRect();
  const svgRect = svg.getBoundingClientRect();

  const x1_ctr = r1.left + r1.width / 2 - svgRect.left;
  const y1_ctr = r1.top + r1.height / 2 - svgRect.top;
  const x2_ctr = r2.left + r2.width / 2 - svgRect.left;
  const y2_ctr = r2.top + r2.height / 2 - svgRect.top;

  // Offset stacked plugs circularly around the jack center
  let x1 = x1_ctr, y1 = y1_ctr;
  if (fromCount > 1) {
    const angle = (fromIdx * 2 * Math.PI) / fromCount;
    x1 += Math.cos(angle) * 3.5;
    y1 += Math.sin(angle) * 3.5;
  }

  let x2 = x2_ctr, y2 = y2_ctr;
  if (toCount > 1) {
    const angle = (toIdx * 2 * Math.PI) / toCount;
    x2 += Math.cos(angle) * 3.5;
    y2 += Math.sin(angle) * 3.5;
  }

  // Vary sag to fan out paths in the middle
  const sagOffset = (fromIdx - (fromCount - 1) / 2) * 12;

  drawCablePathXY(svg, x1, y1, x2, y2, color, dashed, sagOffset, dim, glow);
}

function drawCablePathXY(svg, x1, y1, x2, y2, color, dashed, sagOffset = 0, dim = false, glow = false) {
  const dist = Math.hypot(x2 - x1, y2 - y1);
  const sag = Math.max(30, dist * 0.28) + sagOffset;
  const d = `M${x1},${y1} C${x1},${y1 + sag} ${x2},${y2 + sag} ${x2},${y2}`;

  const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
  path.setAttribute('d', d);
  path.setAttribute('stroke', color);
  path.setAttribute('stroke-width', glow ? '5.5' : '4');
  path.setAttribute('fill', 'none');
  path.setAttribute('stroke-linecap', 'round');

  if (dashed) {
    path.setAttribute('stroke-dasharray', '6 4');
    path.style.filter = 'drop-shadow(0 3px 5px rgba(0,0,0,.6))';
    svg.appendChild(path);
  } else {
    // 1. Base glow path (adds depth)
    const glowPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    glowPath.setAttribute('d', d);
    glowPath.setAttribute('stroke', color);
    glowPath.setAttribute('stroke-width', glow ? '11' : '7');
    glowPath.setAttribute('fill', 'none');
    glowPath.setAttribute('stroke-linecap', 'round');
    glowPath.style.opacity = dim ? '0.05' : (glow ? '0.6' : '0.3');
    glowPath.style.filter = 'blur(2px)';
    svg.appendChild(glowPath);

    // 2. Core colored cable path
    path.style.opacity = dim ? '0.15' : '1.0';
    path.style.filter = glow ? 'drop-shadow(0 3px 6px rgba(0,0,0,.4))' : 'drop-shadow(0 3px 5px rgba(0,0,0,.5))';
    svg.appendChild(path);
  }
}

// ═══════════════════════════════════════════════════════════════════════
// 11. MODULE BROWSER
// ═══════════════════════════════════════════════════════════════════════

function buildBrowser() {
  const grid = $('browserGrid');
  grid.innerHTML = '';

  const query = ($('browserSearch')?.value || '').toLowerCase();

  for (const cat of CATEGORY_ORDER) {
    const mods = Object.entries(MODULE_DEFS)
      .filter(([k, v]) => v.category === cat && (
        !query || k.includes(query) || v.title.toLowerCase().includes(query)
      )).sort((a, b) => a[1].title.localeCompare(b[1].title));
    if (mods.length === 0) continue;

    const catEl = el('div', 'browser-cat');
    const title = el('div', 'browser-cat-title'); title.textContent = CATEGORY_LABELS[cat];
    catEl.appendChild(title);

    const modsEl = el('div', 'browser-cat-modules');
    for (const [key, def] of mods) {
      const btn = el('button', 'browser-mod-btn');
      btn.textContent = def.title;
      btn.title = `${def.hp} HP`;
      btn.addEventListener('click', () => {
        const { targetRow, hpX } = getPositionUnderCursor();
        addModuleToRow(key, targetRow, {}, { left: hpX * HP });
        closeBrowser();
      });
      modsEl.appendChild(btn);
    }

    catEl.appendChild(modsEl);
    grid.appendChild(catEl);
  }
}

function openBrowser() {
  $('browserPanel').classList.add('open');
  $('browserSearch').focus();
  buildBrowser();
}
function closeBrowser() {
  $('browserPanel').classList.remove('open');
}

// ═══════════════════════════════════════════════════════════════════════
// 12. CODE GENERATION
// ═══════════════════════════════════════════════════════════════════════


function getPitchCenter(type, param) {
  if (param === 'pitch') return 60; // default raw 1935 is C4 (60)
  if (param === 'note') {
    if (type === 'kick') return 36;   // default raw 1161 is C2 (36)
    if (type === 'snare') return 45;  // default raw 1451 is A2 (45)
    if (type === 'hat') return 80;    // default raw 2580 is G#5 (80)
    return 60;
  }
  return 0;
}

function isModulePortPitch(modId, portId, visited = new Set()) {
  if (!modId) return false;
  const key = `${modId}.${portId}`;
  if (visited.has(key)) return false;
  visited.add(key);

  const mod = state.rows.flat().find(m => m.id === modId);
  if (!mod) return false;

  if ((mod.type === 'score-player' && portId === 'note') ||
    (mod.type === 'midi-note' && portId === 'note') ||
    (mod.type === 'midi-score' && portId === 'notes') ||
    (mod.type === 'turing' && portId === 'out') ||
    (mod.type === 'step-seq' && portId === 'out') ||
    (mod.type === 'quantizer' && portId === 'out') ||
    (mod.type === 'cv-pitch' && portId === 'out') ||
    (mod.type === 'transpose' && portId === 'out')) {
    return true;
  }

  // 2. Transparent carriers: switches
  if (mod.type === 'mult') {
    const incoming = state.cables.find(c => c.toId === mod.id && c.toPort === 'in');
    if (incoming && isModulePortPitch(incoming.fromId, incoming.fromPort, visited)) {
      return true;
    }
  }

  if (mod.type === 'seq-switch' || mod.type === 'signal-switch') {
    const incoming = state.cables.filter(c => c.toId === mod.id);
    for (const c of incoming) {
      if (isModulePortPitch(c.fromId, c.fromPort, visited)) {
        return true;
      }
    }
  }

  return false;
}

function isModulePortPitchInput(modId, portId, visited = new Set()) {
  if (!modId) return false;
  const key = `${modId}.${portId}`;
  if (visited.has(key)) return false;
  visited.add(key);

  const mod = state.rows.flat().find(m => m.id === modId);
  if (!mod) return false;

  if (mod.type === 'ws-out') {
    if (portId === 'cv-out-1' || portId === 'cv-out-2') {
      const incoming = state.cables.find(c => c.toId === mod.id && c.toPort === portId);
      if (incoming) {
        return isModulePortPitch(incoming.fromId, incoming.fromPort);
      }
    }
    return false;
  }

  const isDirectPitchInput = portId === 'note' || portId === 'pitch' || portId === 'pitch1' || portId === 'pitch2' || (mod.type === 'quantizer' && portId === 'in') || (mod.type === 'transpose' && portId === 'in');
  if (isDirectPitchInput) return true;

  if (mod.type === 'mult') {
    const outgoing = state.cables.filter(c => c.fromId === mod.id);
    for (const c of outgoing) {
      if (isModulePortPitchInput(c.toId, c.toPort, visited)) {
        return true;
      }
    }
  }

  if (mod.type === 'seq-switch' || mod.type === 'signal-switch') {
    const outgoing = state.cables.filter(c => c.fromId === mod.id);
    for (const c of outgoing) {
      if (isModulePortPitchInput(c.toId, c.toPort, visited)) {
        return true;
      }
    }
  }

  return false;
}

function getCabledSourceExpr(cable, allMods) {
  if (!cable) return '0';
  const fromMod = allMods.find(m => m.id === cable.fromId);
  if (!fromMod) return '0';

  const isPitchInput = isModulePortPitchInput(cable.toId, cable.toPort);
  const isPitchSource = isModulePortPitch(cable.fromId, cable.fromPort);

  let expr;
  if (MODULE_DEFS[fromMod.type]?.isHW) {
    if (isPitchInput && (cable.fromPort === 'cv-in-1' || cable.fromPort === 'cv-in-2')) {
      const num = cable.fromPort === 'cv-in-1' ? '1' : '2';
      expr = `(cv-in :${num} :v-oct)`;
    } else {
      expr = LENS_PORTS[cable.fromPort] || '0';
    }
  } else {
    const fromDef = MODULE_DEFS[fromMod.type];
    if (fromMod.type === 'clock') {
      expr = (cable.fromPort === 'out' || cable.fromPort === 'clk') ? cable.fromId : `${cable.fromId}${cable.fromPort}`;
    } else if (fromDef?.isMacro) {
      const port = resolveMacroOutputPort(fromMod.type, cable.fromPort);
      expr = `${cable.fromId}${port}`;
    } else if (['audio', 'tape', 'wave-draw'].includes(fromMod.type)) {
      if (cable.fromPort === 'len') {
        expr = `${cable.fromId}len`;
      } else {
        expr = cable.fromId;
      }
    } else {
      const fromPorts = fromDef?.outputs || [];
      const isDefaultOut = fromPorts.length === 1 || cable.fromPort === 'out';
      expr = isDefaultOut ? cable.fromId : `(${cable.fromId} :${cable.fromPort})`;
    }
  }

  const isDirectPitchSource = isPitchSource || (fromMod.type === 'ws-in' && (cable.fromPort === 'cv-in-1' || cable.fromPort === 'cv-in-2')) || ['step', 'seek', 'lookup'].includes(fromMod.type);
  if (isPitchInput && !isDirectPitchSource) {
    return `(spread ${expr} 128)`;
  }
  if (!isPitchInput && isPitchSource) {
    return `(spread ${expr} 132071)`;
  }
  return expr;
}

function generateCode(textOnly = false) {
  const lines = ['; generated by flare', '(patch'];
  const allMods = state.rows.flat();

  // 1. Trace active modules backward from output sinks and MIDI outputs (Unconnected Modules Skip Pass)
  const activeIds = new Set();
  const sinks = allMods.filter(m => {
    const d = MODULE_DEFS[m.type];
    return d?.isHW || m.type === 'midi-note-out' || m.type === 'midi-cc-out' || m.type === 'midi-clock-out' || m.type === 'tape-record' || m.type === 'tape-write';
  });

  const activeQueue = [];
  for (const s of sinks) {
    activeIds.add(s.id);
    activeQueue.push(s.id);
  }

  while (activeQueue.length > 0) {
    const currentId = activeQueue.shift();
    const incoming = state.cables.filter(c => c.toId === currentId);
    for (const c of incoming) {
      if (!activeIds.has(c.fromId)) {
        activeIds.add(c.fromId);
        activeQueue.push(c.fromId);
      }
    }
  }

  // Computer modules carry patch logic in params.code, not via cables.
  for (const m of allMods) {
    if (m.type === 'computer') activeIds.add(m.id);
  }

  const hasPath = (startId, endId, visited = new Set()) => {
    if (startId === endId) return true;
    visited.add(startId);
    const outCables = state.cables.filter(c => c.fromId === startId);
    for (const c of outCables) {
      if (c.toId && !visited.has(c.toId)) {
        if (hasPath(c.toId, endId, visited)) return true;
      }
    }
    return false;
  };

  // Kahn's Topological Sort
  const adj = {}, indeg = {};
  for (const m of allMods) { adj[m.id] = []; indeg[m.id] = 0; }
  const delayTypes = new Set(['delay', 'tape-delay', 'reverb', 'flanger', 'chorus']);
  for (const c of state.cables) {
    if (c.fromId && c.toId && adj[c.fromId] && indeg[c.toId] !== undefined) {
      const toMod = allMods.find(m => m.id === c.toId);
      const isDelayInput = toMod && delayTypes.has(toMod.type) &&
        (c.toPort === 'in' || c.toPort === 'in-l' || c.toPort === 'in-r') &&
        hasPath(c.toId, c.fromId);
      if (isDelayInput) {
        continue; // Break topological cycle at delay/effect buffer bounds
      }
      adj[c.fromId].push(c.toId);
      indeg[c.toId]++;
    }
  }
  const queue = allMods.filter(m => indeg[m.id] === 0).map(m => m.id);
  const ordered = [];
  while (true) {
    while (queue.length) {
      const id = queue.shift(); ordered.push(id);
      for (const nb of (adj[id] || [])) { if (--indeg[nb] === 0) queue.push(nb); }
    }
    const remaining = allMods.filter(m => activeIds.has(m.id) && !ordered.includes(m.id));
    if (remaining.length === 0) break;
    remaining.sort((a, b) => indeg[a.id] - indeg[b.id]);
    const forceId = remaining[0].id;
    indeg[forceId] = 0;
    queue.push(forceId);
  }
  for (const m of allMods) if (!ordered.includes(m.id)) ordered.push(m.id);

  const sinkLines = [];

  for (const id of ordered) {
    const m = allMods.find(m => m.id === id);
    if (!m) continue;

    // Skip unconnected modules
    if (!activeIds.has(m.id)) continue;

    const def = MODULE_DEFS[m.type];
    if (!def || def.isHW) continue;

    let expr;
    if (m.type === 'mix') {
      // Special 4-channel Mixer S-expression generation using VCA multiplication
      const channels = ['a', 'b', 'c', 'd'];
      const mixedSigs = channels.map(ch => {
        const paramName = `vol${ch.toUpperCase()}`;
        const volExpr = getKnobValueExpr(m, paramName, m.params[paramName] ?? 2048);
        const cable = state.cables.find(c => c.toId === id && c.toPort === ch);
        if (cable) {
          const src = getCabledSourceExpr(cable, allMods);
          return `(vca ${src} ${volExpr})`;
        }
        return null;
      }).filter(Boolean);

      if (mixedSigs.length === 0) {
        expr = '0';
      } else if (mixedSigs.length === 1) {
        expr = mixedSigs[0];
      } else {
        let nested = mixedSigs[0];
        for (let i = 1; i < mixedSigs.length; i++) {
          nested = `(add ${nested} ${mixedSigs[i]})`;
        }
        expr = nested;
      }
    } else if (m.type === 'clock') {
      // Clock: use :tempo mode for smooth, continuously-variable rate.
      // :bpm compiled in HZ mode only resolves to whole-Hz steps (60 BPM
      // increments), and cabled CV adds directly in Hz, blowing the tempo.
      // :tempo maps the raw 0..4095 value log-smoothly via rate_table[32..126].
      const bpmVal = getKnobValue(m.type, 'bpm', m.params.bpm ?? 1638);
      const fmVal  = getKnobValue(m.type, 'fm',  m.params.fm  ?? 0);
      const widthVal = getKnobValue(m.type, 'width', m.params.width ?? 2048);

      const syncCable = state.cables.find(c => c.toId === id && c.toPort === 'sync');
      const fmCable   = state.cables.find(c => c.toId === id && c.toPort === 'fm');

      const bpmCcNum = m.params.__midi_cc && m.params.__midi_cc['bpm'];
      const rawBpm = m.params.bpm ?? 1638;
      const initTempo = bpmToTempoRaw(getKnobValue('clock', 'bpm', rawBpm));
      const bpmExpr = bpmCcNum !== undefined
        ? formatMidiCcExpr(bpmCcNum, initTempo)
        : `${bpmToTempoRaw(bpmVal)}`;

      const fmExpr = getKnobValueExpr(m, 'fm', m.params.fm ?? 0);
      const widthExpr = getKnobValueExpr(m, 'width', m.params.width ?? 2048);

      let rateExpr;
      if (fmCable) {
        const fmSrc = getCabledSourceExpr(fmCable, allMods);
        if (fmExpr === '4095') {
          rateExpr = `(add ${bpmExpr} ${fmSrc} :sat)`;
        } else if (fmExpr === '0') {
          rateExpr = `${bpmExpr}`;
        } else {
          rateExpr = `(add ${bpmExpr} (vca ${fmSrc} ${fmExpr}) :sat)`;
        }
      } else {
        rateExpr = `${bpmExpr}`;
      }

      let args = `:tempo ${rateExpr}`;
      const widthCcNum = m.params.__midi_cc && m.params.__midi_cc['width'];
      if (widthCcNum !== undefined || widthVal !== 2048) {
        args += ` :width ${widthExpr}`;
      }
      if (syncCable) {
        const syncSrc = getCabledSourceExpr(syncCable, allMods);
        args += ` :sync ${syncSrc}`;
      }

      if (state.cables.some(c => c.fromId === id)) {
        lines.push(`  (def ${id} (clock ${args}))`);
        if (state.cables.some(c => c.fromId === id && c.fromPort === 'mult2')) {
          lines.push(`  (def ${id}mult2 (follow ${id} :mult 2))`);
        }
        if (state.cables.some(c => c.fromId === id && c.fromPort === 'mult4')) {
          lines.push(`  (def ${id}mult4 (follow ${id} :mult 4))`);
        }
      }
      continue;
    } else if (m.type === 'multi-div') {
      // Macro Clock divider: compiles to low-level clock division blocks for connected lines
      const divisions = { div2: 2, div3: 3, div4: 4, div8: 8 };
      for (const [port, divVal] of Object.entries(divisions)) {
        const hasCable = state.cables.some(c => c.fromId === id && c.fromPort === port);
        if (hasCable) {
          const clockInCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
          const clockSrc = getCabledSourceExpr(clockInCable, allMods);
          lines.push(`  (def ${id}${port} (every :n ${divVal} :trig ${clockSrc}))`);
        }
      }
      continue;
    } else if (m.type === 'sub-osc') {
      // Macro Sub-Oscillator VCO: Saw wave with main pitch, sub-1 (-12), and sub-2 (-24) octave outputs
      const noteInCable = state.cables.find(c => c.toId === id && c.toPort === 'note');
      const pitchKnob = getKnobValue(m.type, 'pitch', m.params.pitch ?? 1935);
      const centsRaw = m.params.cents ?? 2048;

      let basePitchStr;
      if (noteInCable) {
        const noteSrc = getCabledSourceExpr(noteInCable, allMods);
        // When cabled, pitch knob = semitone transpose offset centred at default (60).
        const transpose = Math.round(pitchKnob - 60);
        basePitchStr = transpose !== 0 ? `(add ${noteSrc} ${transpose})` : noteSrc;
      } else {
        basePitchStr = `${pitchKnob}`;
      }

      const centsArg = centsRaw !== 2048 ? ` :cents ${centsRaw}` : '';

      if (state.cables.some(c => c.fromId === id && c.fromPort === 'out')) {
        lines.push(`  (def ${id}out (saw :note ${basePitchStr}${centsArg}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'sub1')) {
        lines.push(`  (def ${id}sub1 (saw :note (sub ${basePitchStr} 12)${centsArg}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'sub2')) {
        lines.push(`  (def ${id}sub2 (saw :note (sub ${basePitchStr} 24)${centsArg}))`);
      }
      continue;
    } else if (m.type === 'sine' || m.type === 'triangle' || m.type === 'saw' || m.type === 'square') {
      const noteCable = state.cables.find(c => c.toId === id && c.toPort === 'note');
      const fmCable = state.cables.find(c => c.toId === id && c.toPort === 'fm');
      const pmCable = m.type === 'sine' ? state.cables.find(c => c.toId === id && c.toPort === 'pm') : null;
      const pwmCable = m.type === 'square' ? state.cables.find(c => c.toId === id && c.toPort === 'pwm') : null;
      const syncCable = state.cables.find(c => c.toId === id && c.toPort === 'sync');

      const noteSig = noteCable ? getCabledSourceExpr(noteCable, allMods) : null;
      const fmSig = fmCable ? getCabledSourceExpr(fmCable, allMods) : null;
      const pmSig = pmCable ? getCabledSourceExpr(pmCable, allMods) : null;
      const pwmSig = pwmCable ? getCabledSourceExpr(pwmCable, allMods) : null;
      const syncSig = syncCable ? getCabledSourceExpr(syncCable, allMods) : null;

      const pitchKnob = getKnobValue(m.type, 'pitch', m.params.pitch ?? 1935);
      const centsExpr = getKnobValueExpr(m, 'cents', m.params.cents ?? 2048);
      const depthExpr = getKnobValueExpr(m, 'depth', m.params.depth ?? 0);
      const rangeVal = getKnobValue(m.type, 'range', m.params.range ?? 0);

      const isLfo = rangeVal === 'lfo';

      let expr = '';
      if (isLfo) {
        // LFO Mode: frequency in Hz (0.25 Hz to 30 Hz) mapped to TEMPO code (mode 3)
        // using the raw 0..4095 pitch parameter directly.
        const pitchCcNum = m.params.__midi_cc && m.params.__midi_cc['pitch'];
        const rawPitch = m.params.pitch ?? 1935;
        const pitchExpr = pitchCcNum !== undefined
          ? formatMidiCcExpr(pitchCcNum, rawPitch)
          : `${rawPitch}`;

        let tempoExpr = pitchExpr;

        if (noteSig) {
          // CV cabled to note input: since note is expected to be a v-oct/MIDI note value (0..127),
          // we scale it up to the 0..4095 range of the tempo parameter by multiplying by 32.
          tempoExpr = `(add ${tempoExpr} (mul ${noteSig} 32) :sat)`;
        }
        if (fmSig) {
          // CV cabled to fm input: add directly to the tempo parameter.
          const lfoFmDepthExpr = m.type === 'square' ? '4095' : depthExpr;
          tempoExpr = `(add ${tempoExpr} (vca ${fmSig} ${lfoFmDepthExpr}) :sat)`;
        }

        const oscArgs = [`:tempo ${tempoExpr}`];
        if (pmSig && m.type === 'sine') oscArgs.push(`:pm (vca ${pmSig} ${depthExpr})`);
        if (syncSig) oscArgs.push(`:sync ${syncSig}`);

        if (m.type === 'square') {
          const widthCcNum = m.params.__midi_cc && m.params.__midi_cc['width'];
          const rawWidth = m.params.width ?? 2048;
          const widthKnobExpr = widthCcNum !== undefined
            ? formatMidiCcExpr(widthCcNum, rawWidth)
            : `${rawWidth}`;
          let widthExpr = widthKnobExpr;
          if (pwmSig) {
            widthExpr = `(add ${widthExpr} (vca ${pwmSig} ${depthExpr}) :sat)`;
          }
          oscArgs.push(`:width ${widthExpr}`);
        }

        expr = `(unipolar (${m.type} ${oscArgs.join(' ')}))`;
      } else {
        // Audio/VCO Mode: standard MIDI note tracking
        const pitchCcNum = m.params.__midi_cc && m.params.__midi_cc['pitch'];
        const rawPitch = m.params.pitch ?? 1935;
        let noteExpr;
        if (noteSig) {
          if (pitchCcNum !== undefined) {
            // CC-mapped pitch + note cable: CC controls transpose around C4 (60).
            // (spread (midi-cc CC :init RAW) 128) maps 0-4095→0-127 note range.
            noteExpr = `(add ${noteSig} (sub (spread ${formatMidiCcExpr(pitchCcNum, rawPitch)} 128) 60))`;
          } else {
            const transpose = Math.round(pitchKnob - 60);
            noteExpr = transpose !== 0 ? `(add ${noteSig} ${transpose})` : noteSig;
          }
        } else {
          noteExpr = pitchCcNum !== undefined
            ? `(spread ${formatMidiCcExpr(pitchCcNum, rawPitch)} 128)`
            : `${pitchKnob}`;
        }


        const oscArgs = [`:note ${noteExpr}`, `:cents ${centsExpr}`];
        if (fmSig) {
          if (m.type === 'sine') {
            // For sine wave, FM port compiles to Phase Modulation (:pm) for smooth cross FM
            oscArgs.push(`:pm (vca ${fmSig} ${depthExpr})`);
          } else {
            const audioFmDepthExpr = m.type === 'square' ? '4095' : depthExpr;
            oscArgs.push(`:fm (div (vca ${fmSig} ${audioFmDepthExpr}) 120)`);
          }
        }
        if (pmSig && m.type === 'sine') oscArgs.push(`:pm (vca ${pmSig} ${depthExpr})`);
        if (syncSig) oscArgs.push(`:sync ${syncSig}`);

        if (m.type === 'square') {
          const widthKnobExpr = getKnobValueExpr(m, 'width', m.params.width ?? 2048);
          let widthExpr = widthKnobExpr;
          if (pwmSig) {
            widthExpr = `(add ${widthExpr} (vca ${pwmSig} ${depthExpr}) :sat)`;
          }
          oscArgs.push(`:width ${widthExpr}`);
        }

        expr = `(${m.type} ${oscArgs.join(' ')})`;
      }

      lines.push(`  (def ${id} ${expr})`);
      continue;
    } else if (m.type === 'wt') {
      const pitchCable = state.cables.find(c => c.toId === id && c.toPort === 'pitch');
      const posCable = state.cables.find(c => c.toId === id && c.toPort === 'pos');
      const pmCable = state.cables.find(c => c.toId === id && c.toPort === 'pm');

      const pitchCcNum = m.params.__midi_cc && m.params.__midi_cc['pitch'];
      const rawPitch = m.params.pitch ?? 1935;

      let pitchExpr;
      if (pitchCable) {
        const pitchSrc = getCabledSourceExpr(pitchCable, allMods);
        if (pitchCcNum !== undefined) {
           pitchExpr = `(add ${pitchSrc} (sub (spread ${formatMidiCcExpr(pitchCcNum, rawPitch)} 128) 60))`;
        } else {
           const pitchKnob = getKnobValue(m.type, 'pitch', rawPitch);
           const transpose = Math.round(pitchKnob - 60);
           pitchExpr = transpose !== 0 ? `(add ${pitchSrc} ${transpose})` : pitchSrc;
        }
      } else {
        pitchExpr = getKnobValueExpr(m, 'pitch', rawPitch);
      }

      const centsExpr = getKnobValueExpr(m, 'cents', m.params.cents ?? 2048);
      const tableExpr = getKnobValueExpr(m, 'table', m.params.table ?? 256);

      const posKnobExpr = getKnobValueExpr(m, 'pos', m.params.pos ?? 2048);
      const posAmtExpr = getKnobValueExpr(m, 'posamt', m.params.posamt ?? 4095);
      
      let posExpr = posKnobExpr;
      if (posCable) {
        const posSrc = getCabledSourceExpr(posCable, allMods);
        posExpr = `(add ${posKnobExpr} (vca ${posSrc} ${posAmtExpr}) :sat)`;
      }

      let args = `:table ${tableExpr} :note ${pitchExpr} :cents ${centsExpr} :pos ${posExpr}`;
      if (pmCable) {
        const pmSrc = getCabledSourceExpr(pmCable, allMods);
        args += ` :pm ${pmSrc}`;
      }

      lines.push(`  (def ${id} (wt ${args}))`);
      continue;

    } else if (m.type === 'vca') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const cvCable = state.cables.find(c => c.toId === id && c.toPort === 'amp');
      const inSig = getCabledSourceExpr(inCable, allMods);
      const levelExpr = getKnobValueExpr(m, 'amp', m.params.amp ?? 4095);

      if (cvCable) {
        const cvSig = getCabledSourceExpr(cvCable, allMods);
        lines.push(`  (def ${id} (vca ${inSig} (vca ${cvSig} ${levelExpr})))`);
      } else {
        lines.push(`  (def ${id} (vca ${inSig} ${levelExpr}))`);
      }
      continue;
    } else if (m.type === 'quad-vca') {
      // Macro Quad VCA: compiles to 4 low-level independent VCA blocks
      const channels = ['a', 'b', 'c', 'd'];
      for (const ch of channels) {
        const outPort = `out${ch.toUpperCase()}`;
        const hasCable = state.cables.some(c => c.fromId === id && c.fromPort === outPort);
        if (hasCable) {
          const inPort = `in${ch.toUpperCase()}`;
          const cvPort = `cv${ch.toUpperCase()}`;
          const volParam = `vol${ch.toUpperCase()}`;

          const inCable = state.cables.find(c => c.toId === id && c.toPort === inPort);
          const inSig = getCabledSourceExpr(inCable, allMods);

          const cvCable = state.cables.find(c => c.toId === id && c.toPort === cvPort);
          const levelExpr = getKnobValueExpr(m, volParam, m.params[volParam] ?? 4095);

          if (cvCable) {
            const cvSig = getCabledSourceExpr(cvCable, allMods);
            lines.push(`  (def ${id}${outPort} (vca ${inSig} (vca ${cvSig} ${levelExpr})))`);
          } else {
            lines.push(`  (def ${id}${outPort} (vca ${inSig} ${levelExpr}))`);
          }
        }
      }
      continue;
    } else if (m.type === 'lfo-delay') {
      // Macro Delay LFO: combines LFO, Envelope and VCA
      const hasCable = state.cables.some(c => c.fromId === id && c.fromPort === 'out');
      if (hasCable) {
        const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
        const hzCvCable = state.cables.find(c => c.toId === id && c.toPort === 'hz');
        const fadeCvCable = state.cables.find(c => c.toId === id && c.toPort === 'fade');
        const trigSig = getCabledSourceExpr(trigCable, allMods);

        const hzCcNum = m.params.__midi_cc && m.params.__midi_cc['hz'];
        const rawHz = m.params.hz ?? 10;
        const hzKnobExpr = hzCcNum !== undefined
          ? formatMidiCcExpr(hzCcNum, rawHz)
          : `${rawHz}`;

        const fadeExprKnob = getKnobValueExpr(m, 'fade', m.params.fade ?? 2048);

        const hzExpr = hzCvCable
          ? `(add ${hzKnobExpr} ${getCabledSourceExpr(hzCvCable, allMods)} :sat)`
          : hzKnobExpr;
        const fadeExpr = fadeCvCable
          ? `(add ${getCabledSourceExpr(fadeCvCable, allMods)} ${fadeExprKnob} :sat)`
          : fadeExprKnob;

        lines.push(`  (def ${id}lfo (phasor :tempo ${hzExpr}))`);
        lines.push(`  (def ${id}env (envelope :trig ${trigSig} :decay ${fadeExpr}))`);
        lines.push(`  (def ${id}out (vca ${id}lfo ${id}env))`);
      }
      continue;
    } else if (m.type === 'rhythm') {
      // Macro Rhythm Player: Compiles to onsets/gates/hits over a built-in rhythm pattern
      const hasCable = state.cables.some(c => c.fromId === id && c.fromPort === 'out');
      if (hasCable) {
        const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
        const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : 'master';

        const patDef = def.knobs.find(k => k.param === 'pattern');
        const patIdx = Math.max(0, Math.min(patDef.discrete.length - 1, Math.floor((m.params.pattern ?? 0) / 4096 * patDef.discrete.length)));
        const patName = patDef.discrete[patIdx];

        const modeDef = def.knobs.find(k => k.param === 'mode');
        const modeIdx = Math.max(0, Math.min(modeDef.discrete.length - 1, Math.floor((m.params.mode ?? 0) / 4096 * modeDef.discrete.length)));
        const modeName = modeDef.discrete[modeIdx];

        lines.push(`  (def ${id}out (${modeName} ${patName} ${trigSig}))`);
      }
      continue;
    } else if (m.type === 'turing') {
      // Macro Turing Machine: Evolving shift register randomizer
      const hasNoteCable = state.cables.some(c => c.fromId === id && c.fromPort === 'out');
      const hasTrigCable = state.cables.some(c => c.fromId === id && c.fromPort === 'trig');
      if (hasNoteCable || hasTrigCable) {
        const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
        const probCvCable = state.cables.find(c => c.toId === id && c.toPort === 'prob');
        const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : 'master';

        const knobProbExpr = getKnobValueExpr(m, 'prob', m.params.prob ?? 4095);
        const probExpr = probCvCable
          ? `(add ${getCabledSourceExpr(probCvCable, allMods)} ${knobProbExpr} :sat)`
          : knobProbExpr;

        const lenDef = def.knobs.find(k => k.param === 'len');
        const lenIdx = Math.max(0, Math.min(lenDef.discrete.length - 1, Math.floor((m.params.len ?? 4095) / 4096 * lenDef.discrete.length)));
        const lenVal = lenDef.discrete[lenIdx];

        lines.push(`  (def ${id}loop (tape '(C3 Eb3 G3 Bb3 C4 Bb3 G3 Eb3)))`);
        lines.push(`  (def ${id}step (step ${id}loop :len ${lenVal} :trig ${trigSig}))`);
        lines.push(`  (<- ${id}loop (if (chance ${probExpr} :trig ${trigSig}) (snap (add C3 (spread (random :trig ${trigSig}) 25)) :scale minor) ${id}step) :len ${lenVal} :trig ${trigSig})`);

        if (hasNoteCable) {
          lines.push(`  (def ${id}out ${id}step)`);
        }
        if (hasTrigCable) {
          lines.push(`  (def ${id}trig (trig ${trigSig}))`);
        }
      }
      continue;
    } else if (m.type === 'signal-switch') {
      const hasCable = state.cables.some(c => c.fromId === id && c.fromPort === 'out');
      if (hasCable) {
        const condCable = state.cables.find(c => c.toId === id && c.toPort === 'cond');
        const aCable = state.cables.find(c => c.toId === id && c.toPort === 'a');
        const bCable = state.cables.find(c => c.toId === id && c.toPort === 'b');
        const cCable = state.cables.find(c => c.toId === id && c.toPort === 'c');

        const aSig = aCable ? getCabledSourceExpr(aCable, allMods) : (m.params.a !== undefined ? m.params.a : '0');
        const bSig = bCable ? getCabledSourceExpr(bCable, allMods) : (m.params.b !== undefined ? m.params.b : '0');
        const cSig = cCable ? getCabledSourceExpr(cCable, allMods) : (m.params.c !== undefined ? m.params.c : '0');

        if (condCable) {
          const condSig = getCabledSourceExpr(condCable, allMods);
          lines.push(`  (def ${id}out (if (up ${condSig}) ${cSig} (if (mid ${condSig}) ${bSig} ${aSig})))`);
        } else {
          // If no CONTROL CV cabled, fold routing compile-time to the manual switch parameter
          const posVal = m.params.pos ?? 2048;
          if (posVal > 2730) {
            lines.push(`  (def ${id}out ${cSig})`);
          } else if (posVal > 1365) {
            lines.push(`  (def ${id}out ${bSig})`);
          } else {
            lines.push(`  (def ${id}out ${aSig})`);
          }
        }
      }
      continue;
    } else if (m.type === 'seq-switch') {
      const hasCable = state.cables.some(c => c.fromId === id && c.fromPort === 'out');
      if (hasCable) {
        const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
        const resetCable = state.cables.find(c => c.toId === id && c.toPort === 'reset');
        const in1Cable = state.cables.find(c => c.toId === id && c.toPort === 'in1');
        const in2Cable = state.cables.find(c => c.toId === id && c.toPort === 'in2');
        const in3Cable = state.cables.find(c => c.toId === id && c.toPort === 'in3');
        const in4Cable = state.cables.find(c => c.toId === id && c.toPort === 'in4');
        const in5Cable = state.cables.find(c => c.toId === id && c.toPort === 'in5');
        const in6Cable = state.cables.find(c => c.toId === id && c.toPort === 'in6');
        const in7Cable = state.cables.find(c => c.toId === id && c.toPort === 'in7');
        const in8Cable = state.cables.find(c => c.toId === id && c.toPort === 'in8');

        const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : '0';
        const resetSig = resetCable ? getCabledSourceExpr(resetCable, allMods) : '0';
        const in1Sig = in1Cable ? getCabledSourceExpr(in1Cable, allMods) : '0';
        const in2Sig = in2Cable ? getCabledSourceExpr(in2Cable, allMods) : '0';
        const in3Sig = in3Cable ? getCabledSourceExpr(in3Cable, allMods) : '0';
        const in4Sig = in4Cable ? getCabledSourceExpr(in4Cable, allMods) : '0';
        const in5Sig = in5Cable ? getCabledSourceExpr(in5Cable, allMods) : '0';
        const in6Sig = in6Cable ? getCabledSourceExpr(in6Cable, allMods) : '0';
        const in7Sig = in7Cable ? getCabledSourceExpr(in7Cable, allMods) : '0';
        const in8Sig = in8Cable ? getCabledSourceExpr(in8Cable, allMods) : '0';

        const stepsDef = def.knobs.find(k => k.param === 'steps');
        const stepsIdx = Math.max(0, Math.min(stepsDef.discrete.length - 1, Math.floor(((m.params.steps ?? 4095) / 4096) * stepsDef.discrete.length)));
        const rawSteps = m.params.steps ?? 4095;
        const stepsVal = stepsDef.discrete[stepsIdx];
        const stepsCcNum = m.params.__midi_cc && m.params.__midi_cc['steps'];
        const stepsExpr = stepsCcNum !== undefined
          ? `(over ${formatMidiCcExpr(stepsCcNum, rawSteps)} 2 8)`
          : `${stepsVal}`;

        const vals = [in1Sig, in2Sig, in3Sig, in4Sig, in5Sig, in6Sig, in7Sig, in8Sig];
        let tree = vals[stepsVal - 1];
        for (let i = stepsVal - 2; i >= 0; i--) {
          tree = `(if (eq ${id}idx ${i}) ${vals[i]} ${tree})`;
        }

        lines.push(`  (def ${id}idx (counter :bars ${stepsExpr} :trig ${trigSig} :reset ${resetSig}))`);
        lines.push(`  (def ${id}out ${tree})`);
      }
      continue;
    } else if (m.type === 'drum-seq') {
      const hasAnyCable = state.cables.some(c => c.fromId === id && ['kick', 'snare', 'hat', 'perc'].includes(c.fromPort));
      if (hasAnyCable) {
        const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
        const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : '0';

        const stepsVal = getKnobValue(m.type, 'steps', m.params.steps ?? 4095);
        const getBeatStr = (arr) => {
          const steps = arr || Array(16).fill(0);
          const sliced = steps.slice(0, stepsVal);
          return sliced.map(s => s ? 'x' : '.').join(' ');
        };

        const DS_CHANNELS = ['kick', 'snare', 'hat', 'perc'];
        const usedChannels = DS_CHANNELS.filter(ch =>
          state.cables.some(c => c.fromId === id && c.fromPort === ch)
        );
        for (const ch of usedChannels) {
          const patKey = `${ch}Pat`;
          lines.push(`  (def ${id}${patKey} (beat '(${getBeatStr(m.params[patKey])})))`);
          lines.push(`  (def ${id}${ch} (onsets ${id}${patKey} ${trigSig}))`);
        }
      }

      continue;
    } else if (m.type === 'score-player') {
      // Macro Score Player: plays a melody pattern at a given tempo.
      // Internal clock uses :tempo mode (raw 0..4095 → log-spaced BPM via
      // rate_table) so the speed knob and CV are smooth and continuous.
      // op_step / op_onsets detect trig_fall internally, so we pass the
      // phasor ramp directly — no (edge ...) wrapper needed.
      const pat = m.params.pattern || '[c4 e4 g4 c5]';
      const trigCable  = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const speedCable = state.cables.find(c => c.toId === id && c.toPort === 'speed');
      const knobSpeed  = m.params.speed ?? 2048;

      const noteCabled = state.cables.some(c => c.fromId === id && c.fromPort === 'note');
      const gateCabled = state.cables.some(c => c.fromId === id && c.fromPort === 'gate');

      if (noteCabled || gateCabled) {
        const cleanPat = pat.replace(/\[/g, '(').replace(/\]/g, ')');
        lines.push(`  (def ${id}score (score '${cleanPat}))`);
      }

      if (trigCable) {
        // External clock: pass directly — op_step / op_onsets detect the
        // falling edge (phasor wrap) themselves.
        const trigSig = getCabledSourceExpr(trigCable, allMods);
        if (noteCabled) {
          lines.push(`  (def ${id}note (step (${id}score :notes) :trig ${trigSig}))`);
        }
        if (gateCabled) {
          lines.push(`  (def ${id}gate (onsets (${id}score :rhythm) :trig ${trigSig}))`);
        }
      } else {
        // Internal clock: use :tempo so the knob/CV are smooth (no 60-BPM steps).
        // Speed CV is added to the raw tempo value (0..4095 domain, clamped).
        const speedExpr = getKnobValueExpr(m, 'speed', knobSpeed);
        const speedRawExpr = speedCable
          ? `(add ${speedExpr} ${getCabledSourceExpr(speedCable, allMods)} :sat)`
          : speedExpr;
        lines.push(`  (def ${id}clk (clock :tempo ${speedRawExpr}))`);
        if (noteCabled) {
          lines.push(`  (def ${id}note (step (${id}score :notes) :trig ${id}clk))`);
        }
        if (gateCabled) {
          lines.push(`  (def ${id}gate (onsets (${id}score :rhythm) :trig ${id}clk))`);
        }
      }
      continue;
    } else if (m.type === 'step-seq') {
      // Macro Step Sequencer: 1..8 steps indexed by a trigger counter, forward/backward/random
      const hasCable = state.cables.some(c => c.fromId === id && c.fromPort === 'out');
      if (hasCable) {
        const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
        const trigSig = getCabledSourceExpr(trigCable, allMods);

        const getValExpr = (paramName, def) => {
          const raw = m.params[paramName] ?? def;
          const ccNum = m.params.__midi_cc && m.params.__midi_cc[paramName];
          if (ccNum !== undefined) return `(spread ${formatMidiCcExpr(ccNum, raw)} 128)`;
          return String(getKnobValue(m.type, 'note', raw));
        };
        const val1 = getValExpr('val1', 0);
        const val2 = getValExpr('val2', 512);
        const val3 = getValExpr('val3', 1024);
        const val4 = getValExpr('val4', 1536);
        const val5 = getValExpr('val5', 2048);
        const val6 = getValExpr('val6', 2560);
        const val7 = getValExpr('val7', 3072);
        const val8 = getValExpr('val8', 3584);

        const stepsDef = def.knobs.find(k => k.param === 'steps');
        const rawSteps = m.params.steps ?? 4095;
        const stepsVal = Math.max(1, Math.min(8, Math.floor((rawSteps / 4096) * 8) + 1));
        const stepsCcNum = m.params.__midi_cc && m.params.__midi_cc['steps'];
        const stepsExpr = stepsCcNum !== undefined
          ? `(over ${formatMidiCcExpr(stepsCcNum, rawSteps)} 1 8)`
          : `${stepsVal}`;

        const dirDef = def.knobs.find(k => k.param === 'dir');
        const dirIdx = Math.max(0, Math.min(dirDef.discrete.length - 1, Math.floor((m.params.dir ?? 0) / 4096 * dirDef.discrete.length)));
        const dirVal = dirDef.discrete[dirIdx];

        let idxExpr;
        if (dirVal === 'random') {
          idxExpr = `(spread (random :trig ${trigSig}) ${stepsExpr})`;
        } else if (dirVal === 'backward') {
          idxExpr = `(sub (sub ${stepsExpr} 1) (counter :bars ${stepsExpr} :trig ${trigSig}))`;
        } else {
          idxExpr = `(counter :bars ${stepsExpr} :trig ${trigSig})`;
        }

        const vals = [val1, val2, val3, val4, val5, val6, val7, val8];
        let tree = vals[stepsVal - 1];
        for (let i = stepsVal - 2; i >= 0; i--) {
          tree = `(if (eq ${id}count ${i}) ${vals[i]} ${tree})`;
        }

        lines.push(`  (def ${id}count ${idxExpr})`);
        lines.push(`  (def ${id}out ${tree})`);
      }
      continue;
    } else if (m.type === 'midi-note') {
      const chVal = getKnobValue(m.type, 'ch', m.params.ch ?? 0);
      const chArg = (chVal && chVal !== 'omni') ? ` :ch ${chVal}` : '';
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'note')) {
        lines.push(`  (def ${id}note (midi-note${chArg}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'gate')) {
        lines.push(`  (def ${id}gate (midi-gate${chArg}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'vel')) {
        lines.push(`  (def ${id}vel (midi-velocity${chArg}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'press')) {
        lines.push(`  (def ${id}press (midi-pressure${chArg}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'bend')) {
        lines.push(`  (def ${id}bend (midi-bend${chArg}))`);
      }
      continue;
    } else if (m.type === 'midi-cc') {
      const hasCable = state.cables.some(c => c.fromId === id && c.fromPort === 'out');
      if (hasCable) {
        const chVal = getKnobValue(m.type, 'ch', m.params.ch ?? 0);
        const ccVal = getKnobValue(m.type, 'cc', m.params.cc ?? 1);
        const chArg = chVal > 0 ? ` :ch ${chVal}` : '';
        lines.push(`  (def ${id}out (midi-cc :${ccVal}${chArg}))`);
      }
      continue;
    } else if (m.type === 'midi-trig') {
      const hasCable = state.cables.some(c => c.fromId === id && c.fromPort === 'out');
      if (hasCable) {
        const chVal = getKnobValue(m.type, 'ch', m.params.ch ?? 0);
        const noteVal = getKnobValue(m.type, 'note', m.params.note ?? 60);
        const chArg = chVal > 0 ? ` :ch ${chVal}` : '';
        lines.push(`  (def ${id}out (midi-trig :note ${noteVal}${chArg}))`);
      }
      continue;
    } else if (m.type === 'midi-clock') {
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'clk')) {
        lines.push(`  (def ${id}clk (midi-clock))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'play')) {
        lines.push(`  (def ${id}play (midi-playing))`);
      }
      continue;
    } else if (m.type === 'midi-note-out') {
      const chVal = getKnobValue(m.type, 'ch', m.params.ch ?? 1);
      const pitchCable = state.cables.find(c => c.toId === id && c.toPort === 'pitch');
      const pitchSig = getCabledSourceExpr(pitchCable, allMods) !== '0' ? getCabledSourceExpr(pitchCable, allMods) : '60';

      const gateCable = state.cables.find(c => c.toId === id && c.toPort === 'gate');
      const gateSig = getCabledSourceExpr(gateCable, allMods);

      const velCable = state.cables.find(c => c.toId === id && c.toPort === 'vel');
      const velSig = getCabledSourceExpr(velCable, allMods) !== '0' ? getCabledSourceExpr(velCable, allMods) : '100';

      sinkLines.push(`  (<- (midi-note-out :ch ${chVal}) ${pitchSig} :gate ${gateSig} :vel ${velSig})`);
      continue;
    } else if (m.type === 'midi-cc-out') {
      const chVal = getKnobValue(m.type, 'ch', m.params.ch ?? 1);
      const ccVal = getKnobValue(m.type, 'cc', m.params.cc ?? 1);
      const valCable = state.cables.find(c => c.toId === id && c.toPort === 'val');
      const valSig = getCabledSourceExpr(valCable, allMods);

      sinkLines.push(`  (<- (midi-cc-out :ch ${chVal} :cc ${ccVal}) ${valSig})`);
      continue;
    } else if (m.type === 'midi-clock-out') {
      const clkCable = state.cables.find(c => c.toId === id && c.toPort === 'clk');
      const clkSig = getCabledSourceExpr(clkCable, allMods);

      sinkLines.push(`  (<- (midi-clock-out) ${clkSig})`);
      continue;
    } else if (m.type === 'kick') {
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const noteCable = state.cables.find(c => c.toId === id && c.toPort === 'note');
      const decayCable = state.cables.find(c => c.toId === id && c.toPort === 'decay');

      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : '0';
      const noteSig = noteCable ? getCabledSourceExpr(noteCable, allMods) : null;
      const decaySig = decayCable ? getCabledSourceExpr(decayCable, allMods) : null;

      const pitchExprKnob = getKnobValueExpr(m, 'note', m.params.note ?? 1161);
      const decayExprKnob = getKnobValueExpr(m, 'decay', m.params.decay ?? 2048);
      const driveExprKnob = getKnobValueExpr(m, 'drive', m.params.drive ?? 0);

      let noteExpr = pitchExprKnob;
      if (noteSig) {
        noteExpr = `(add ${pitchExprKnob} ${noteSig} :sat)`;
      }

      let decayExpr = decayExprKnob;
      if (decaySig) {
        decayExpr = `(add ${decayExprKnob} ${decaySig} :sat)`;
      }

      lines.push(`  (def ${id} (kick :note ${noteExpr} :decay ${decayExpr} :drive ${driveExprKnob} :sweep 2800 :trig ${trigSig}))`);
      continue;
    } else if (m.type === 'attenuverter') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'out')) {
        const inSrc = getCabledSourceExpr(inCable, allMods);
        const gainRaw = m.params.gain ?? 2048;
        const offsetRaw = m.params.offset ?? 2048;

        const gainCcNum = m.params.__midi_cc && m.params.__midi_cc['gain'];
        const offsetCcNum = m.params.__midi_cc && m.params.__midi_cc['offset'];

        let gainExpr;
        if (gainCcNum !== undefined) {
          gainExpr = `(vca ${inSrc} (bipolar ${getKnobValueExpr(m, 'gain', gainRaw)}))`;
        } else {
          if (gainRaw === 2048) {
            gainExpr = '0';
          } else if (gainRaw === 4095) {
            gainExpr = inSrc;
          } else if (gainRaw === 0) {
            gainExpr = `(sub 0 ${inSrc})`;
          } else {
            gainExpr = `(vca ${inSrc} (bipolar ${gainRaw}))`;
          }
        }

        let outExpr;
        if (offsetCcNum !== undefined) {
          const offsetExpr = `(bipolar ${getKnobValueExpr(m, 'offset', offsetRaw)})`;
          if (gainExpr === '0') {
            outExpr = offsetExpr;
          } else {
            outExpr = `(add ${gainExpr} ${offsetExpr} :sat)`;
          }
        } else {
          if (offsetRaw === 2048) {
            outExpr = gainExpr;
          } else {
            if (gainExpr === '0') {
              outExpr = `${offsetRaw}`;
            } else {
              outExpr = `(add ${gainExpr} (bipolar ${offsetRaw}) :sat)`;
            }
          }
        }
        lines.push(`  (def ${id}out ${outExpr})`);
      }
      continue;
    } else if (m.type === 'tape-delay') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const recCable = state.cables.find(c => c.toId === id && c.toPort === 'rec');
      const speedCable = state.cables.find(c => c.toId === id && c.toPort === 'speed');

      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      const recSig = recCable ? getCabledSourceExpr(recCable, allMods) : '0';

      const lenDef = def.knobs.find(k => k.param === 'len');
      const lenIdx = Math.max(0, Math.min(lenDef.discrete.length - 1, Math.floor((m.params.len ?? 2048) / 4096 * lenDef.discrete.length)));
      const seconds = lenDef.discrete[lenIdx]; // 1.0, 2.0, 4.0, 8.0

      const speedKnob = getKnobValue(m.type, 'speed', m.params.speed ?? 2048);
      const speedScale = Math.pow(2.0, (speedKnob - 2048) / 1024);

      const fPlay = speedScale / seconds;
      const bPlay = Math.max(0, Math.min(255, 255 * Math.log(fPlay / 0.05) / Math.log(400000)));
      const playRateCode = Math.round(bPlay * 16);

      const fOverdub = 1.0 / seconds;
      const bOverdub = Math.max(0, Math.min(255, 255 * Math.log(fOverdub / 0.05) / Math.log(400000)));
      const overdubRateCode = Math.round(bOverdub * 16);

      const feedbackVal = getKnobValue(m.type, 'feedback', m.params.feedback ?? 3000);

      let speedExpr = `${playRateCode}`;
      if (speedCable) {
        const speedSrc = getCabledSourceExpr(speedCable, allMods);
        speedExpr = `(add ${playRateCode} (bipolar ${speedSrc}))`;
      }

      lines.push(`  (def ${id}buf (audio :seconds ${seconds.toFixed(2)}))`);
      lines.push(`  (def ${id}loop (play ${id}buf ${overdubRateCode}))`);
      // :per-sample and :when cannot be combined; gate the signal instead
      sinkLines.push(`  (<- ${id}buf (if ${recSig} (clip (add ${inSig} (vca ${id}loop ${feedbackVal}) :sat)) 0) :per-sample)`);
      lines.push(`  (def ${id}out (play ${id}buf ${speedExpr}))`);
      continue;
    } else if (m.type === 'delay') {
      const hasCable = state.cables.some(c => c.fromId === id && (c.fromPort === 'outL' || c.fromPort === 'outR'));
      if (hasCable) {
        const inLCable = state.cables.find(c => c.toId === id && c.toPort === 'inL');
        const inRCable = state.cables.find(c => c.toId === id && c.toPort === 'inR');
        const inL = inLCable ? getCabledSourceExpr(inLCable, allMods) : (inRCable ? getCabledSourceExpr(inRCable, allMods) : '0');
        const inR = inRCable ? getCabledSourceExpr(inRCable, allMods) : inL;

        const timeCable = state.cables.find(c => c.toId === id && c.toPort === 'time');
        const knobTime = m.params.time ?? 2048;
        const knobTimeExpr = getKnobValueExpr(m, 'time', knobTime);
        const timeAmt = m.params.timeamt ?? 4095;
        const timeAmtExpr = getKnobValueExpr(m, 'timeamt', timeAmt);
        let timeExpr = knobTimeExpr;
        if (timeCable) {
          const timeSrc = getCabledSourceExpr(timeCable, allMods);
          let att = timeSrc;
          const ccNum = m.params.__midi_cc && m.params.__midi_cc['timeamt'];
          if (ccNum !== undefined) {
            att = `(vca ${timeSrc} ${timeAmtExpr})`;
          } else {
            const amtVal = getKnobValue(m.type, 'timeamt', timeAmt);
            if (amtVal < 4095) att = `(vca ${timeSrc} ${amtVal})`;
          }
          timeExpr = `(clip (add ${att} ${knobTimeExpr} :sat))`;
        }

        const feedCable = state.cables.find(c => c.toId === id && c.toPort === 'feedback');
        const knobFeed = m.params.feedback ?? 1024;
        const knobFeedExpr = getKnobValueExpr(m, 'feedback', knobFeed);
        const feedAmt = m.params.feedamt ?? 4095;
        const feedAmtExpr = getKnobValueExpr(m, 'feedamt', feedAmt);
        let feedExpr = knobFeedExpr;
        if (feedCable) {
          const feedSrc = getCabledSourceExpr(feedCable, allMods);
          let att = feedSrc;
          const ccNum = m.params.__midi_cc && m.params.__midi_cc['feedamt'];
          if (ccNum !== undefined) {
            att = `(vca ${feedSrc} ${feedAmtExpr})`;
          } else {
            const amtVal = getKnobValue(m.type, 'feedamt', feedAmt);
            if (amtVal < 4095) att = `(vca ${feedSrc} ${amtVal})`;
          }
          feedExpr = `(clip (add ${att} ${knobFeedExpr} :sat))`;
        }

        const modeDef = MODULE_DEFS.delay.knobs.find(k => k.param === 'mode');
        const modeVal = m.params.mode ?? 2048;
        let modeStr = 'stereo';
        if (typeof modeVal === 'string') {
          modeStr = modeVal;
        } else {
          const modeIdx = Math.max(0, Math.min(modeDef.discrete.length - 1, Math.floor((modeVal / 4096) * modeDef.discrete.length)));
          modeStr = modeDef.discrete[modeIdx];
        }

        const mixCvCable = state.cables.find(c => c.toId === id && c.toPort === 'mix');
        const knobMix = m.params.mix ?? 2048;
        const knobMixExpr = getKnobValueExpr(m, 'mix', knobMix);
        let mixExpr = knobMixExpr;
        if (mixCvCable) {
          const mixSrc = getCabledSourceExpr(mixCvCable, allMods);
          mixExpr = `(clip (add ${mixSrc} ${knobMixExpr} :sat))`;
        }

        const knobRatioRaw = m.params.ratio ?? 2048;
        const knobRatio = (knobRatioRaw / 2048).toFixed(3);

        let modeFlag = ':stereo';
        if (modeStr === 'mono') modeFlag = ':mono';
        else if (modeStr === 'ping-pong') modeFlag = ':ping-pong';

        lines.push(`  (def ${id} (echo :in ${inL} :in-r ${inR} :time ${timeExpr} :fb ${feedExpr} ${modeFlag} :ratio ${knobRatio}))`);
        // Mix: crossfade dry input with wet delay using morph (linear interpolation at mix position)
        lines.push(`  (def ${id}outL (morph ${inL} ${id} ${mixExpr}))`);
        lines.push(`  (def ${id}outR (morph ${inR} (sel ${id}) ${mixExpr}))`);
      }
      continue;
    } else if (m.type === 'reverb') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const inSig = getCabledSourceExpr(inCable, allMods);

      const decayCvCable = state.cables.find(c => c.toId === id && c.toPort === 'decay');
      const decayVal = m.params.decay ?? 2048;
      const decayValExpr = getKnobValueExpr(m, 'decay', decayVal);
      const decayAmt = m.params.decayamt ?? 4095;
      const decayAmtExpr = getKnobValueExpr(m, 'decayamt', decayAmt);
      let decayExpr = decayValExpr;
      if (decayCvCable) {
        const decaySrc = getCabledSourceExpr(decayCvCable, allMods);
        let att = decaySrc;
        const ccNum = m.params.__midi_cc && m.params.__midi_cc['decayamt'];
        if (ccNum !== undefined) {
          att = `(vca ${decaySrc} ${decayAmtExpr})`;
        } else {
          const amtVal = getKnobValue(m.type, 'decayamt', decayAmt);
          if (amtVal < 4095) att = `(vca ${decaySrc} ${amtVal})`;
        }
        decayExpr = `(clip (add ${att} ${decayValExpr} :sat))`;
      }

      const mixCvCable = state.cables.find(c => c.toId === id && c.toPort === 'mix');
      const mixVal = m.params.mix ?? 1024;
      const mixValExpr = getKnobValueExpr(m, 'mix', mixVal);
      const mixAmt = m.params.mixamt ?? 4095;
      const mixAmtExpr = getKnobValueExpr(m, 'mixamt', mixAmt);
      let mixExpr = mixValExpr;
      if (mixCvCable) {
        const mixSrc = getCabledSourceExpr(mixCvCable, allMods);
        let att = mixSrc;
        const ccNum = m.params.__midi_cc && m.params.__midi_cc['mixamt'];
        if (ccNum !== undefined) {
          att = `(vca ${mixSrc} ${mixAmtExpr})`;
        } else {
          const amtVal = getKnobValue(m.type, 'mixamt', mixAmt);
          if (amtVal < 4095) att = `(vca ${mixSrc} ${amtVal})`;
        }
        mixExpr = `(clip (add ${att} ${mixValExpr} :sat))`;
      }

      lines.push(`  (def ${id} (reverb :in ${inSig} :decay ${decayExpr} :mix ${mixExpr}))`);
      lines.push(`  (def ${id}outL ${id})`);
      lines.push(`  (def ${id}outR (sel ${id}))`); // sel = canonical out2/value_r accessor
      continue;
    } else if (m.type === 'chorus') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const inSig = getCabledSourceExpr(inCable, allMods);

      const rateCable = state.cables.find(c => c.toId === id && c.toPort === 'rate');
      const rateVal = m.params.rate ?? 1000;
      const rateValExpr = getKnobValueExpr(m, 'rate', rateVal);
      const rateAmt = m.params.rateamt ?? 4095;
      const rateAmtExpr = getKnobValueExpr(m, 'rateamt', rateAmt);
      let rateExpr = rateValExpr;
      if (rateCable) {
        const rateSrc = getCabledSourceExpr(rateCable, allMods);
        let att = rateSrc;
        const ccNum = m.params.__midi_cc && m.params.__midi_cc['rateamt'];
        if (ccNum !== undefined) {
          att = `(vca ${rateSrc} ${rateAmtExpr})`;
        } else {
          const amtVal = getKnobValue(m.type, 'rateamt', rateAmt);
          if (amtVal < 4095) att = `(vca ${rateSrc} ${amtVal})`;
        }
        rateExpr = `(clip (add ${att} ${rateValExpr} :sat))`;
      }

      const depthCable = state.cables.find(c => c.toId === id && c.toPort === 'depth');
      const depthVal = m.params.depth ?? 2048;
      const depthValExpr = getKnobValueExpr(m, 'depth', depthVal);
      const depthAmt = m.params.depthamt ?? 4095;
      const depthAmtExpr = getKnobValueExpr(m, 'depthamt', depthAmt);
      let depthExpr = depthValExpr;
      if (depthCable) {
        const depthSrc = getCabledSourceExpr(depthCable, allMods);
        let att = depthSrc;
        const ccNum = m.params.__midi_cc && m.params.__midi_cc['depthamt'];
        if (ccNum !== undefined) {
          att = `(vca ${depthSrc} ${depthAmtExpr})`;
        } else {
          const amtVal = getKnobValue(m.type, 'depthamt', depthAmt);
          if (amtVal < 4095) att = `(vca ${depthSrc} ${amtVal})`;
        }
        depthExpr = `(clip (add ${att} ${depthValExpr} :sat))`;
      }

      const fbVal = m.params.feedback ?? 2048;
      const fbValExpr = getKnobValueExpr(m, 'feedback', fbVal);

      lines.push(`  (def ${id} (chorus :in ${inSig} :rate ${rateExpr} :depth ${depthExpr} :fb ${fbValExpr}))`); // :fb not :feedback
      lines.push(`  (def ${id}outL ${id})`);
      lines.push(`  (def ${id}outR (sel ${id}))`); // sel = canonical out2/value_r accessor
      continue;
    } else if (m.type === 'flanger') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const inSig = getCabledSourceExpr(inCable, allMods);

      const rateCable = state.cables.find(c => c.toId === id && c.toPort === 'rate');
      const rateVal = m.params.rate ?? 500;
      const rateValExpr = getKnobValueExpr(m, 'rate', rateVal);
      const rateAmt = m.params.rateamt ?? 4095;
      const rateAmtExpr = getKnobValueExpr(m, 'rateamt', rateAmt);
      let rateExpr = rateValExpr;
      if (rateCable) {
        const rateSrc = getCabledSourceExpr(rateCable, allMods);
        let att = rateSrc;
        const ccNum = m.params.__midi_cc && m.params.__midi_cc['rateamt'];
        if (ccNum !== undefined) {
          att = `(vca ${rateSrc} ${rateAmtExpr})`;
        } else {
          const amtVal = getKnobValue(m.type, 'rateamt', rateAmt);
          if (amtVal < 4095) att = `(vca ${rateSrc} ${amtVal})`;
        }
        rateExpr = `(clip (add ${att} ${rateValExpr} :sat))`;
      }

      const depthCable = state.cables.find(c => c.toId === id && c.toPort === 'depth');
      const depthVal = m.params.depth ?? 1024;
      const depthValExpr = getKnobValueExpr(m, 'depth', depthVal);
      const depthAmt = m.params.depthamt ?? 4095;
      const depthAmtExpr = getKnobValueExpr(m, 'depthamt', depthAmt);
      let depthExpr = depthValExpr;
      if (depthCable) {
        const depthSrc = getCabledSourceExpr(depthCable, allMods);
        let att = depthSrc;
        const ccNum = m.params.__midi_cc && m.params.__midi_cc['depthamt'];
        if (ccNum !== undefined) {
          att = `(vca ${depthSrc} ${depthAmtExpr})`;
        } else {
          const amtVal = getKnobValue(m.type, 'depthamt', depthAmt);
          if (amtVal < 4095) att = `(vca ${depthSrc} ${amtVal})`;
        }
        depthExpr = `(clip (add ${att} ${depthValExpr} :sat))`;
      }

      const fbVal = m.params.feedback ?? 3000;
      const fbValExpr = getKnobValueExpr(m, 'feedback', fbVal);

      lines.push(`  (def ${id} (flanger :in ${inSig} :rate ${rateExpr} :depth ${depthExpr} :fb ${fbValExpr}))`); // :fb not :feedback
      lines.push(`  (def ${id}outL ${id})`);
      lines.push(`  (def ${id}outR (sel ${id}))`); // sel = canonical out2/value_r accessor
      continue;
    } else if (m.type === 'compressor') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const inSig = getCabledSourceExpr(inCable, allMods);

      const threshExpr = getKnobValueExpr(m, 'threshold', m.params.threshold ?? 3000);
      const ratioExpr = getKnobValueExpr(m, 'ratio', m.params.ratio ?? 2048);
      const attExpr = `(sub 4095 ${getKnobValueExpr(m, 'attack', m.params.attack ?? 100)})`;
      const relExpr = `(sub 4095 ${getKnobValueExpr(m, 'release', m.params.release ?? 1000)})`;

      lines.push(`  (def ${id}out (compressor :in ${inSig} :thresh ${threshExpr} :ratio ${ratioExpr} :attack ${attExpr} :release ${relExpr}))`); // :thresh not :threshold
      continue;
    } else if (m.type === 'logic') {
      const aCable = state.cables.find(c => c.toId === id && c.toPort === 'a');
      const bCable = state.cables.find(c => c.toId === id && c.toPort === 'b');
      const aSrc = getCabledSourceExpr(aCable, allMods);
      const bSrc = getCabledSourceExpr(bCable, allMods);

      if (state.cables.some(c => c.fromId === id && c.fromPort === 'and')) {
        lines.push(`  (def ${id}and (and ${aSrc} ${bSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'or')) {
        lines.push(`  (def ${id}or (or ${aSrc} ${bSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'xor')) {
        lines.push(`  (def ${id}xor (xor ${aSrc} ${bSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'not')) {
        lines.push(`  (def ${id}not (not ${aSrc}))`);
      }
      continue;
    } else if (m.type === 'cv-pitch') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      const octKnobExpr = getKnobValueExpr(m, 'octaves', m.params.octaves ?? 1228);
      const baseKnobExpr = getKnobValueExpr(m, 'pitch', m.params.pitch ?? 1935);
      lines.push(`  (def ${id}out (add ${baseKnobExpr} (spread ${inSig} (spread ${octKnobExpr} 120))))`);
      continue;
    } else if (m.type === 'quantizer') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const scaleCable = state.cables.find(c => c.toId === id && c.toPort === 'scale');
      const rootCable = state.cables.find(c => c.toId === id && c.toPort === 'root');

      const inSig = getCabledSourceExpr(inCable, allMods);

      const scaleDef = def.knobs.find(k => k.param === 'scale');
      const scaleIdx = Math.max(0, Math.min(scaleDef.discrete.length - 1, Math.floor((m.params.scale ?? 0) / 4096 * scaleDef.discrete.length)));
      const scaleName = scaleDef.discrete[scaleIdx];

      const rootDef = def.knobs.find(k => k.param === 'root');
      const rootIdx = Math.max(0, Math.min(rootDef.discrete.length - 1, Math.floor((m.params.root ?? 0) / 4096 * rootDef.discrete.length)));

      const scaleExpr = scaleCable ? getCabledSourceExpr(scaleCable, allMods) : scaleName;
      const rootExpr = rootCable ? `(add ${rootIdx} ${getCabledSourceExpr(rootCable, allMods)} :sat)` : (rootIdx !== 0 ? `${rootIdx}` : null);

      if (rootExpr) {
        lines.push(`  (def ${id}out (add (snap (sub ${inSig} ${rootExpr}) :scale ${scaleExpr}) ${rootExpr}))`);
      } else {
        lines.push(`  (def ${id}out (snap ${inSig} :scale ${scaleExpr}))`);
      }
      continue;
    } else if (m.type === 'transpose') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const byCable = state.cables.find(c => c.toId === id && c.toPort === 'by');
      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';

      const byKnob = getKnobValue(m.type, 'by', m.params.by ?? 2048);
      const byExpr = byCable
        ? `(add ${byKnob} (sub (spread ${getCabledSourceExpr(byCable, allMods)} 25) 12) :sat)`
        : `${byKnob}`;

      lines.push(`  (def ${id} (transpose ${inSig} ${byExpr}))`);
      continue;
    } else if (m.type === 'gain') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const cvCable = state.cables.find(c => c.toId === id && c.toPort === 'gain');
      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';

      const knobVal = getKnobValue(m.type, 'gain', m.params.gain ?? 512);
      const rawGainExpr = cvCable
        ? `(add ${knobVal} ${getCabledSourceExpr(cvCable, allMods)} :sat)`
        : `${knobVal}`;

      lines.push(`  (def ${id} (amplify ${inSig} (mul ${rawGainExpr} 8)))`);
      continue;
    } else if (m.type === 'math') {
      const aCable = state.cables.find(c => c.toId === id && c.toPort === 'a');
      const bCable = state.cables.find(c => c.toId === id && c.toPort === 'b');
      const aSrc = getCabledSourceExpr(aCable, allMods);
      const bSrc = getCabledSourceExpr(bCable, allMods);

      if (state.cables.some(c => c.fromId === id && c.fromPort === 'add')) {
        lines.push(`  (def ${id}add (add ${aSrc} ${bSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'sub')) {
        lines.push(`  (def ${id}sub (sub ${aSrc} ${bSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'mul')) {
        lines.push(`  (def ${id}mul (mul ${aSrc} ${bSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'div')) {
        lines.push(`  (def ${id}div (div ${aSrc} ${bSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'mod')) {
        lines.push(`  (def ${id}mod (mod ${aSrc} ${bSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'abs')) {
        lines.push(`  (def ${id}abs (abs ${aSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'min')) {
        lines.push(`  (def ${id}min (min ${aSrc} ${bSrc}))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'max')) {
        lines.push(`  (def ${id}max (max ${aSrc} ${bSrc}))`);
      }
      continue;
    } else if (m.type === 'add') {
      const a = state.cables.find(c => c.toId === id && c.toPort === 'a');
      const b = state.cables.find(c => c.toId === id && c.toPort === 'b');
      const c = state.cables.find(c => c.toId === id && c.toPort === 'c');
      const sa = a ? getCabledSourceExpr(a, allMods) : '0';
      const sb = b ? getCabledSourceExpr(b, allMods) : '0';
      const sc = c ? getCabledSourceExpr(c, allMods) : null;
      if (sc) {
        lines.push(`  (def ${id}out (add (add ${sa} ${sb} :sat) ${sc} :sat))`);
      } else {
        lines.push(`  (def ${id}out (add ${sa} ${sb} :sat))`);
      }
      continue;
    } else if (m.type === 'mul') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'a');
      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      const gainRaw = m.params.gain ?? 4095;
      const gainExpr = getKnobValueExpr(m, 'gain', gainRaw);
      lines.push(`  (def ${id} (vca ${inSig} ${gainExpr}))`);
      continue;
    } else if (m.type === 'constant') {
      const val1 = getKnobValueExpr(m, 'val1', m.params.val1 ?? 2048);
      const val2 = getKnobValueExpr(m, 'val2', m.params.val2 ?? 2048);
      const val3 = getKnobValueExpr(m, 'val3', m.params.val3 ?? 2048);
      lines.push(`  (def ${id}out ${val1})`);
      lines.push(`  (def ${id}out1 ${val1})`);
      lines.push(`  (def ${id}out2 ${val2})`);
      lines.push(`  (def ${id}out3 ${val3})`);
      continue;
    } else if (m.type === 'rungler') {
      const pitch1Cable = state.cables.find(c => c.toId === id && c.toPort === 'pitch1');
      const pitch2Cable = state.cables.find(c => c.toId === id && c.toPort === 'pitch2');
      const p1Sig = pitch1Cable ? getCabledSourceExpr(pitch1Cable, allMods) : '0';
      const p2Sig = pitch2Cable ? getCabledSourceExpr(pitch2Cable, allMods) : '0';

      const f1Knob = m.params.freq1 ?? 1935;
      const f2Knob = m.params.freq2 ?? 1935;
      const runglerKnob = m.params.rungler ?? 1024;

      const f1Expr = getKnobValueExpr(m, 'freq1', f1Knob);
      const f2Expr = getKnobValueExpr(m, 'freq2', f2Knob);
      const runglerExpr = getKnobValueExpr(m, 'rungler', runglerKnob);

      const lockDef = def.knobs.find(k => k.param === 'lock');
      const lockIdx = Math.max(0, Math.min(lockDef.discrete.length - 1, Math.floor((m.params.lock ?? 0) / 4096 * lockDef.discrete.length)));
      const isLocked = lockDef.discrete[lockIdx] === 'lock' ? 1 : 0;

      lines.push(`  (def ${id}reg (tape '(0 1 1 0 1 0 0 1)))`);
      lines.push(`  (def ${id}reg_rec (<- ${id}reg ${id}fb :trig ${id}out2))`);
      lines.push(`  (def ${id}w (sel ${id}reg_rec))`);

      // Read shift register bits 1, 2, 3 relative to current write position
      lines.push(`  (def ${id}s1 (lookup ${id}reg (mod (add ${id}w 7) 8)))`);
      lines.push(`  (def ${id}s2 (lookup ${id}reg (mod (add ${id}w 6) 8)))`);
      lines.push(`  (def ${id}s3 (lookup ${id}reg (mod (add ${id}w 5) 8)))`);

      lines.push(`  (def ${id}r3 (add (add (mul ${id}s1 4) (mul ${id}s2 2)) ${id}s3))`);
      lines.push(`  (def ${id}rungle (div (mul ${id}r3 VMAX) 7))`);
      lines.push(`  (def ${id}runglesm (slew ${id}rungle 2048))`);
      lines.push(`  (def ${id}fm (mul ${id}r3 (spread ${runglerExpr} 13)))`);
      lines.push(`  (def ${id}pitch1 (add (add 30 (spread ${f1Expr} 49)) (add ${p1Sig} ${id}fm)))`);
      lines.push(`  (def ${id}pitch2 (add (spread ${f2Expr} 73) (add ${p2Sig} ${id}fm)))`);
      lines.push(`  (def ${id}out1 (triangle :note ${id}pitch1))`);
      lines.push(`  (def ${id}out2 (square :note ${id}pitch2))`);

      lines.push(`  (def ${id}c1 (gt ${id}out1 0))`);
      lines.push(`  (def ${id}s8 (lookup ${id}reg (mod (add ${id}w 0) 8)))`);
      if (isLocked) {
        lines.push(`  (def ${id}fb ${id}s8)`);
      } else {
        lines.push(`  (def ${id}fb (xor ${id}c1 ${id}s8))`);
      }
      continue;
    } else if (m.type === 'morph') {
      const in1 = getCabledSourceExpr(state.cables.find(c => c.toId === id && c.toPort === 'in1'), allMods);
      const in2 = getCabledSourceExpr(state.cables.find(c => c.toId === id && c.toPort === 'in2'), allMods);
      const in3 = getCabledSourceExpr(state.cables.find(c => c.toId === id && c.toPort === 'in3'), allMods);
      const in4 = getCabledSourceExpr(state.cables.find(c => c.toId === id && c.toPort === 'in4'), allMods);

      const posCable = state.cables.find(c => c.toId === id && c.toPort === 'pos');
      const posKnob = m.params.pos ?? 0;
      const posKnobExpr = getKnobValueExpr(m, 'pos', posKnob);
      let posExpr = posKnobExpr;
      if (posCable) {
        posExpr = `(add ${posExpr} ${getCabledSourceExpr(posCable, allMods)} :sat)`;
      }
      lines.push(`  (def ${id}out (morph (lens ${in1} ${in2} ${in3} ${in4}) ${posExpr}))`);
      continue;
    } else if (m.type === 'midi-sync') {
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'clock')) {
        lines.push(`  (def ${id}clock (midi-clock))`);
      }
      if (state.cables.some(c => c.fromId === id && c.fromPort === 'run')) {
        lines.push(`  (def ${id}run (midi-playing))`);
      }
      continue;
    } else if (m.type === 'schmitt') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const inSig = getCabledSourceExpr(inCable, allMods);
      const loVal = m.params.lo ?? 1024;
      const hiVal = m.params.hi ?? 3072;
      const loExpr = getKnobValueExpr(m, 'lo', loVal);
      const hiExpr = getKnobValueExpr(m, 'hi', hiVal);
      lines.push(`  (def ${id}out (schmitt ${inSig} :lo ${loExpr} :hi ${hiExpr}))`);
      continue;
    } else if (m.type === 'shift-register') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const resetCable = state.cables.find(c => c.toId === id && c.toPort === 'reset');
      const inSig = getCabledSourceExpr(inCable, allMods);
      const trigSig = getCabledSourceExpr(trigCable, allMods);

      lines.push(`  (def ${id}reg (tape '(0 0 0 0 0 0 0 0 0)))`);
      sinkLines.push(`  (<- ${id}reg ${inSig} :trig ${trigSig})`);

      if (resetCable) {
        const resetSig = getCabledSourceExpr(resetCable, allMods);
        sinkLines.push(`  (on ${resetSig} (<- ${id}reg 0 :len 9))`);
      }

      for (let i = 1; i <= 8; i++) {
        if (state.cables.some(c => c.fromId === id && c.fromPort === `out${i}`)) {
          lines.push(`  (def ${id}out${i} (tap ${id}reg ${i}))`);
        }
      }
      continue;
    } else if (m.type === 'euclid') {
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const stepsCable = state.cables.find(c => c.toId === id && c.toPort === 'steps');
      const pulsesCable = state.cables.find(c => c.toId === id && c.toPort === 'pulses');

      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : '0';

      const stepsVal = getKnobValue(m.type, 'steps', m.params.steps ?? 1984);
      const stepsExpr = stepsCable
        ? `(add ${stepsVal} (spread ${getCabledSourceExpr(stepsCable, allMods)} 32) :sat)`
        : `${stepsVal}`;

      const pulsesVal = getKnobValue(m.type, 'pulses', m.params.pulses ?? 448);
      const pulsesExpr = pulsesCable
        ? `(add ${pulsesVal} (spread ${getCabledSourceExpr(pulsesCable, allMods)} 32) :sat)`
        : `${pulsesVal}`;

      lines.push(`  (def ${id} (euclid ${pulsesExpr} ${stepsExpr} :trig ${trigSig}))`);
      continue;
    } else if (m.type === 'turns') {
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const trigSig = getCabledSourceExpr(trigCable, allMods);
      lines.push(`  (def ${id} (turns ${trigSig}))`);
      continue;
    } else if (m.type === 'trig') {
      // visual Trig Delay module -> compiles to prelude's delay function
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const rateCable = state.cables.find(c => c.toId === id && c.toPort === 'rate');
      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : '0';
      const rateVal = getKnobValue(m.type, 'rate', m.params.rate ?? 1000);
      const rateSig = rateCable ? `(add ${getCabledSourceExpr(rateCable, allMods)} ${rateVal} :sat)` : rateVal;
      lines.push(`  (def ${id} (delay :in ${trigSig} :time ${rateSig}))`);
      continue;
    } else if (m.type === 'gate') {
      // visual Gate Gen macro -> compares input to threshold, triggers a pulse (gate)
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const threshCable = state.cables.find(c => c.toId === id && c.toPort === 'thresh');
      const lenCable = state.cables.find(c => c.toId === id && c.toPort === 'len');
      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      const threshVal = getKnobValue(m.type, 'thresh', m.params.thresh ?? 2048);
      const threshSig = threshCable ? `(add ${getCabledSourceExpr(threshCable, allMods)} ${threshVal} :sat)` : threshVal;
      const lenVal = getKnobValue(m.type, 'len', m.params.len ?? 100);
      const lenSig = lenCable ? `(add ${getCabledSourceExpr(lenCable, allMods)} ${lenVal} :sat)` : lenVal;
      lines.push(`  (def ${id}out (gate :in (edge (gt ${inSig} ${threshSig})) :len ${lenSig}))`);
      continue;
    } else if (m.type === 'hold') {
      // hold expander expects positional form: (hold VAL GATE)
      // keyword :val is NOT supported by the expander — must use positional args.
      const valCable = state.cables.find(c => c.toId === id && c.toPort === 'val');
      const onCable = state.cables.find(c => c.toId === id && c.toPort === 'on');
      const valSrc = valCable ? getCabledSourceExpr(valCable, allMods) : '0';
      const onSrc = onCable ? getCabledSourceExpr(onCable, allMods) : '0';
      lines.push(`  (def ${id} (hold ${valSrc} ${onSrc}))`);
      continue;
    } else if (m.type === 'tape-looper') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const recCable = state.cables.find(c => c.toId === id && c.toPort === 'rec');
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const speedCable = state.cables.find(c => c.toId === id && c.toPort === 'speed');

      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      const recSig = recCable ? getCabledSourceExpr(recCable, allMods) : '0';

      const lenDef = def.knobs.find(k => k.param === 'len');
      const lenIdx = Math.max(0, Math.min(lenDef.discrete.length - 1, Math.floor((m.params.len ?? 2048) / 4096 * lenDef.discrete.length)));
      const seconds = lenDef.discrete[lenIdx]; // 1.0, 2.0, 4.0, 8.0

      const speedKnob = getKnobValue(m.type, 'speed', m.params.speed ?? 2048);
      const speedScale = Math.pow(2.0, (speedKnob - 2048) / 1024);

      const fPlay = speedScale / seconds;
      const bPlay = Math.max(0, Math.min(255, 255 * Math.log(fPlay / 0.05) / Math.log(400000)));
      const playRateCode = Math.round(bPlay * 16);

      const loopDef = def.knobs.find(k => k.param === 'loop');
      const loopIdx = Math.max(0, Math.min(loopDef.discrete.length - 1, Math.floor((m.params.loop ?? 0) / 4096 * loopDef.discrete.length)));
      const loopVal = loopDef.discrete[loopIdx]; // 'loop' or 'one-shot'

      let speedExpr = `${Math.round(speedScale * 65536)}`;
      if (speedCable) {
        const speedSrc = getCabledSourceExpr(speedCable, allMods);
        speedExpr = `(mul ${speedSrc} 32)`; 
      }
      
      const maxSamples = Math.floor(seconds * 48000);
      const max_q16 = maxSamples * 65536;

      let resetExpr = `0`;
      if (trigCable) {
        resetExpr = getCabledSourceExpr(trigCable, allMods);
      }

      lines.push(`  (def ${id}buf (audio :seconds ${seconds.toFixed(2)}))`);
      sinkLines.push(`  (<- ${id}buf ${inSig} :per-sample :when ${recSig})`);

      if (loopVal === 'one-shot') {
        lines.push(`  (def ${id}phase (hold i (if ${resetExpr} 0 (min (add i ${speedExpr}) ${max_q16 - 1})) :per-sample :init 0))`);
      } else {
        lines.push(`  (def ${id}phase (hold i (if ${resetExpr} 0 (mod (add i ${speedExpr}) ${max_q16})) :per-sample :init 0))`);
      }
      
      lines.push(`  (def ${id}idx (div ${id}phase 65536))`);
      lines.push(`  (def ${id}frac (mod ${id}phase 65536))`);
      lines.push(`  (def ${id}s0 (lookup ${id}buf ${id}idx))`);
      lines.push(`  (def ${id}s1 (lookup ${id}buf (mod (add ${id}idx 1) ${maxSamples})))`);
      lines.push(`  (def ${id}out (add ${id}s0 (div (mul (sub ${id}s1 ${id}s0) ${id}frac) 65536)))`);
      continue;
    } else if (m.type === 'dx') {
      const pitchCable = state.cables.find(c => c.toId === id && c.toPort === 'pitch');
      const gateCable = state.cables.find(c => c.toId === id && c.toPort === 'gate');
      const decayCable = state.cables.find(c => c.toId === id && c.toPort === 'decay');

      const noteSig = pitchCable ? getCabledSourceExpr(pitchCable, allMods) : null;
      const gateSig = gateCable ? getCabledSourceExpr(gateCable, allMods) : '0';

      const pitchCcNum = m.params.__midi_cc && m.params.__midi_cc['pitch'];
      const rawPitch = m.params.pitch ?? 1935;
      const centsVal = (m.params.cents !== undefined ? (getKnobValue(m.type, 'cents', m.params.cents) - 2048) / 204.8 : 0);

      let pitchSig;
      if (noteSig) {
        if (pitchCcNum !== undefined) {
           pitchSig = `(add ${noteSig} (sub (spread ${formatMidiCcExpr(pitchCcNum, rawPitch)} 128) 60))`;
        } else {
           const pitchKnob = getKnobValue(m.type, 'pitch', rawPitch);
           const transpose = Math.round(pitchKnob / 32.25 - 60 + centsVal);
           pitchSig = transpose !== 0 ? `(add ${noteSig} ${transpose})` : noteSig;
        }
      } else {
        if (pitchCcNum !== undefined) {
           pitchSig = `(spread ${formatMidiCcExpr(pitchCcNum, rawPitch)} 128)`;
        } else {
           const pitchKnob = getKnobValue(m.type, 'pitch', rawPitch);
           pitchSig = `${Math.round(pitchKnob / 32.25 + centsVal)}`;
        }
      }

      const decayExpr = getKnobValueExpr(m, 'decay', m.params.decay ?? 2048);
      const decaySig = decayCable ? getCabledSourceExpr(decayCable, allMods) : decayExpr;

      const toneSig = getKnobValueExpr(m, 'tone', m.params.tone ?? 2048);

      let voiceExpr;
      if (m.params.customVoiceData) {
        const tapeSig = `(tape 128 [${m.params.customVoiceData.join(' ')}])`;
        voiceExpr = `:voice ${tapeSig}`;
      } else {
        const bankVal = getKnobValue(m.type, 'bank', m.params.bank ?? 0);
        const presetVal = getKnobValue(m.type, 'preset', m.params.preset ?? 0);
        voiceExpr = `:bank ${bankVal} :preset ${presetVal}`;
      }

      lines.push(`  (def ${id} (dx ${voiceExpr} :note ${pitchSig} :gate ${gateSig} :decay ${decaySig} :tone ${toneSig}))`); // :note not :pitch
      continue;
    } else if (m.type === 'weave') {
      const noteCable = state.cables.find(c => c.toId === id && c.toPort === 'note');
      const timbreCable = state.cables.find(c => c.toId === id && c.toPort === 'timbre');
      const colorCable = state.cables.find(c => c.toId === id && c.toPort === 'color');
      const modelCable = state.cables.find(c => c.toId === id && c.toPort === 'model');
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');

      const noteSig = noteCable ? getCabledSourceExpr(noteCable, allMods) : null;
      const timbreSig = timbreCable ? getCabledSourceExpr(timbreCable, allMods) : null;
      const colorSig = colorCable ? getCabledSourceExpr(colorCable, allMods) : null;
      const modelSig = modelCable ? getCabledSourceExpr(modelCable, allMods) : null;
      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : null;

      const pitchCcNum = m.params.__midi_cc && m.params.__midi_cc['pitch'];
      const rawPitch = m.params.pitch ?? 1935;
      const pitchKnob = getKnobValue(m.type, 'pitch', rawPitch);
      const timbreKnobExpr = getKnobValueExpr(m, 'timbre', m.params.timbre ?? 2048);
      const colorKnobExpr = getKnobValueExpr(m, 'color', m.params.color ?? 2048);
      const modelVal = getKnobValue(m.type, 'model', m.params.model ?? 0);

      let pitchSig;
      if (noteSig) {
        if (pitchCcNum !== undefined) {
          pitchSig = `(add ${noteSig} (sub (spread ${formatMidiCcExpr(pitchCcNum, rawPitch)} 128) 60))`;
        } else {
          const transpose = Math.round(pitchKnob - 60);
          pitchSig = transpose !== 0 ? `(add ${noteSig} ${transpose})` : noteSig;
        }
      } else {
        pitchSig = pitchCcNum !== undefined
          ? `(spread ${formatMidiCcExpr(pitchCcNum, rawPitch)} 128)`
          : `${Math.round(pitchKnob)}`;
      }

      const timbreExpr = timbreSig ? `(add ${timbreKnobExpr} ${timbreSig} :sat)` : `${timbreKnobExpr}`;
      const colorExpr  = colorSig  ? `(add ${colorKnobExpr} ${colorSig} :sat)`  : `${colorKnobExpr}`;
      // Model: if a cable is wired use the live signal, else use the dropdown constant
      const modelExpr  = modelSig  ? modelSig  : modelVal;
      const trigExpr   = trigSig   ? trigSig   : `0`;

      lines.push(`  (def ${id} (weave :note ${pitchSig} :timbre ${timbreExpr} :color ${colorExpr} :model ${modelExpr} :trig ${trigExpr}))`);
      continue;
    } else if (m.type === 'lfo') {
      const rateCable = state.cables.find(c => c.toId === id && c.toPort === 'rate');
      const syncCable = state.cables.find(c => c.toId === id && c.toPort === 'sync');

      const rateSig = rateCable ? getCabledSourceExpr(rateCable, allMods) : null;
      const syncSig = syncCable ? getCabledSourceExpr(syncCable, allMods) : null;

      const rateKnob = getKnobValue(m.type, 'rate', m.params.rate ?? 2048);
      const rateAmt = getKnobValue(m.type, 'rateamt', m.params.rateamt ?? 4095);

      const shapeDef = def.knobs.find(k => k.param === 'shape');
      const shapeIdx = Math.max(0, Math.min(shapeDef.discrete.length - 1, Math.floor((m.params.shape ?? 0) / 4096 * shapeDef.discrete.length)));
      const shapeName = shapeDef.discrete[shapeIdx];
      const shapeFunc = shapeName === 'tri' ? 'triangle' : (shapeName === 'sqr' ? 'square' : shapeName);

      // Dedicated LFO module: frequency mapped to TEMPO code (mode 3)
      const rateCcNum = m.params.__midi_cc && m.params.__midi_cc['rate'];
      const rawRate = m.params.rate ?? 2048;
      const rateKnobExpr = rateCcNum !== undefined
        ? formatMidiCcExpr(rateCcNum, rawRate)
        : `${rawRate}`;

      let tempoExpr = rateKnobExpr;
      if (rateSig && rateAmt > 0) {
        const attRateSig = rateAmt < 4095 ? `(vca ${rateSig} ${rateAmt})` : rateSig;
        tempoExpr = `(add ${tempoExpr} ${attRateSig} :sat)`;
      }

      const rangeDef = def.knobs.find(k => k.param === 'range');
      const rangeIdx = Math.max(0, Math.min(rangeDef.discrete.length - 1, Math.floor((m.params.range ?? 0) / 4096 * rangeDef.discrete.length)));
      const isUnipolar = rangeDef.discrete[rangeIdx] === 'unipolar';

      if (syncSig) {
        // If synced LFO, drive the phase using a synchronized phasor
        lines.push(`  (def ${id}ph (phasor :tempo ${tempoExpr} :sync ${syncSig}))`);
        if (isUnipolar) {
          lines.push(`  (def ${id}out (unipolar (${shapeFunc} :phase ${id}ph)))`);
        } else {
          lines.push(`  (def ${id}out (${shapeFunc} :phase ${id}ph))`);
        }
      } else {
        if (isUnipolar) {
          lines.push(`  (def ${id}out (unipolar (${shapeFunc} :tempo ${tempoExpr})))`);
        } else {
          lines.push(`  (def ${id}out (${shapeFunc} :tempo ${tempoExpr}))`);
        }
      }
      continue;
    } else if (m.type === 'midi-score') {
      const recCable = state.cables.find(c => c.toId === id && c.toPort === 'rec');
      const clkCable = state.cables.find(c => c.toId === id && c.toPort === 'clk');
      const speedCable = state.cables.find(c => c.toId === id && c.toPort === 'speed');

      const recSig = recCable ? getCabledSourceExpr(recCable, allMods) : '0';
      const clkSig = clkCable ? getCabledSourceExpr(clkCable, allMods) : 'master';

      const speedKnob = getKnobValue(m.type, 'speed', m.params.speed ?? 2048);
      let speedExpr = `(varispeed :knob ${speedKnob})`;
      if (speedCable) {
        const speedSrc = getCabledSourceExpr(speedCable, allMods);
        speedExpr = `(varispeed :knob (add ${speedSrc} ${speedKnob} :sat))`;
      }

      lines.push(`  (def ${id}rec (midi-score :rec ${recSig} :clk ${clkSig} :speed ${speedExpr}))`);
      lines.push(`  (def ${id}notes (${id}rec :notes))`);
      lines.push(`  (def ${id}rhythm (${id}rec :rhythm))`);
      lines.push(`  (def ${id}vel (${id}rec :vel))`);
      continue;
    } else if (m.type === 'seek') {
      const tapeCable = state.cables.find(c => c.toId === id && c.toPort === 'tape');
      const idxCable = state.cables.find(c => c.toId === id && c.toPort === 'at');
      const lenCable = state.cables.find(c => c.toId === id && c.toPort === 'len');
      const tapeSig = tapeCable ? getCabledSourceExpr(tapeCable, allMods) : '0';

      const knobVal = getKnobValue(m.type, 'idx', m.params.idx ?? 0);
      const idxExpr = idxCable
        ? `(add ${knobVal} ${getCabledSourceExpr(idxCable, allMods)} :sat)`
        : `${knobVal}`;

      let lenExpr = '';
      if (lenCable) {
        lenExpr = ` :len ${getCabledSourceExpr(lenCable, allMods)}`;
      }

      lines.push(`  (def ${id} (lookup ${tapeSig} ${idxExpr}${lenExpr}))`);
      continue;
    } else if (m.type === 'audio') {
      const unitDef = def.knobs.find(k => k.param === 'unit');
      const unitIdx = Math.max(0, Math.min(unitDef.discrete.length - 1, Math.floor((m.params.unit ?? 0) / 4096 * unitDef.discrete.length)));
      const unit = unitDef.discrete[unitIdx];

      const rawTime = m.params.time ?? 2048;
      if (unit === 'smpls') {
        const samples = Math.max(1, Math.round((rawTime / 4095) * 4096));
        lines.push(`  (def ${id} (audio :length ${samples}))`);
      } else {
        const seconds = 0.05 + (2.0 - 0.05) * (rawTime / 4095);
        lines.push(`  (def ${id} (audio :seconds ${seconds.toFixed(2)}))`);
      }
      lines.push(`  (def ${id}len (len ${id}))`);
      continue;
    } else if (m.type === 'feedback-cell') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      lines.push(`  (def ${id}buf (tape :len 1))`);
      lines.push(`  (def ${id} (lookup ${id}buf 0))`);
      sinkLines.push(`  (<- ${id}buf ${inSig})`);
      continue;
    } else if (m.type === 'counter') {
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const resetCable = state.cables.find(c => c.toId === id && c.toPort === 'reset');
      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : 'master';
      const resetSig = resetCable ? getCabledSourceExpr(resetCable, allMods) : '0';

      const barsDef = def.knobs.find(k => k.param === 'bars');
      const rawBars = m.params.bars ?? 4095;
      const barsIdx = Math.max(0, Math.min(barsDef.discrete.length - 1, Math.floor((rawBars / 4096) * barsDef.discrete.length)));
      const barsCcNum = m.params.__midi_cc && m.params.__midi_cc['bars'];
      const barsExpr = barsCcNum !== undefined
        ? `(over ${formatMidiCcExpr(barsCcNum, rawBars)} 2 8)`
        : `${barsDef.discrete[barsIdx]}`;

      lines.push(`  (def ${id}count (counter :bars ${barsExpr} :trig ${trigSig} :reset ${resetSig}))`);

      // Individual step outputs
      for (let i = 1; i <= 8; i++) {
        if (state.cables.some(c => c.fromId === id && c.fromPort === `out${i}`)) {
          lines.push(`  (def ${id}out${i} (eq ${id}count ${i - 1}))`);
        }
      }
      continue;
    } else if (m.type === 'edge') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      lines.push(`  (def ${id}rise (edge ${inSig}))`);
      lines.push(`  (def ${id}fall (trig ${inSig}))`);
      continue;
    } else if (m.type === 'if-gate') {
      const condCable = state.cables.find(c => c.toId === id && c.toPort === 'cond');
      const thenCable = state.cables.find(c => c.toId === id && c.toPort === 'then');
      const elseCable = state.cables.find(c => c.toId === id && c.toPort === 'else');

      const condSig = condCable ? getCabledSourceExpr(condCable, allMods) : '0';
      const thenSig = thenCable ? getCabledSourceExpr(thenCable, allMods) : '0';
      const elseSig = elseCable ? getCabledSourceExpr(elseCable, allMods) : '0';

      lines.push(`  (def ${id} (if ${condSig} ${thenSig} ${elseSig}))`);
      continue;
    } else if (m.type === 'len') {
      const tapeCable = state.cables.find(c => c.toId === id && c.toPort === 'tape');
      const tapeSig = tapeCable ? getCabledSourceExpr(tapeCable, allMods) : '0';
      lines.push(`  (def ${id} (len ${tapeSig}))`);
      continue;
    } else if (m.type === 'chance') {
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const probCable = state.cables.find(c => c.toId === id && c.toPort === 'prob');
      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : '0';

      const knobVal = getKnobValue(m.type, 'prob', m.params.prob ?? 2048);
      const probExpr = probCable
        ? `(add ${knobVal} ${getCabledSourceExpr(probCable, allMods)} :sat)`
        : `${knobVal}`;

      lines.push(`  (def ${id} (chance :p ${probExpr} :trig ${trigSig}))`);
      continue;
    } else if (m.type === 'random') {
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : '0';

      const rangeDef = def.knobs.find(k => k.param === 'range');
      const rangeIdx = Math.max(0, Math.min(rangeDef.discrete.length - 1, Math.floor((m.params.range ?? 0) / 4096 * rangeDef.discrete.length)));
      const isUnipolar = rangeDef.discrete[rangeIdx] === 'unipolar';

      if (isUnipolar) {
        lines.push(`  (def ${id} (random :trig ${trigSig}))`);
      } else {
        lines.push(`  (def ${id} (sub (random :trig ${trigSig}) 2048))`);
      }
      continue;
    } else if (m.type === 'tape-record') {
      const tapeCable = state.cables.find(c => c.toId === id && c.toPort === 'tape');
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const whenCable = state.cables.find(c => c.toId === id && c.toPort === 'when');

      const tapeSig = tapeCable ? getCabledSourceExpr(tapeCable, allMods) : '0';
      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      const whenSig = whenCable ? getCabledSourceExpr(whenCable, allMods) : null;
      const whenPart = whenSig ? ` :when ${whenSig}` : '';

      sinkLines.push(`  (<- ${tapeSig} ${inSig} :per-sample${whenPart})`);
      continue;
    } else if (m.type === 'tape-write') {
      const tapeCable = state.cables.find(c => c.toId === id && c.toPort === 'tape');
      const indexCable = state.cables.find(c => c.toId === id && c.toPort === 'at');
      const valCable = state.cables.find(c => c.toId === id && c.toPort === 'val');
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const whenCable = state.cables.find(c => c.toId === id && c.toPort === 'when');

      const tapeSig = tapeCable ? getCabledSourceExpr(tapeCable, allMods) : '0';
      const indexSig = indexCable ? getCabledSourceExpr(indexCable, allMods) : '0';
      const valSig = valCable ? getCabledSourceExpr(valCable, allMods) : '0';
      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : 'master';
      const whenSig = whenCable ? getCabledSourceExpr(whenCable, allMods) : null;

      const whenPart = whenSig ? ` :when ${whenSig}` : '';
      sinkLines.push(`  (on ${trigSig}${whenPart} (<- (seek ${tapeSig} ${indexSig}) ${valSig}))`);
      continue;
    } else if (m.type === 'tap') {
      const tapeCable = state.cables.find(c => c.toId === id && c.toPort === 'tape');
      const amountCable = state.cables.find(c => c.toId === id && c.toPort === 'amount');

      const tapeSig = tapeCable ? getCabledSourceExpr(tapeCable, allMods) : '0';
      const amountSig = amountCable ? getCabledSourceExpr(amountCable, allMods) : '0';

      const spanDef = def.knobs.find(k => k.param === 'span');
      const spanIdx = Math.max(0, Math.min(spanDef.discrete.length - 1, Math.floor((m.params.span ?? 0) / 4096 * spanDef.discrete.length)));
      const spanVal = spanDef.discrete[spanIdx]; // 'samples' or 'span'

      if (spanVal === 'span') {
        lines.push(`  (def ${id} (tap ${tapeSig} :amount ${amountSig} :span))`);
      } else {
        lines.push(`  (def ${id} (tap ${tapeSig} :amount ${amountSig}))`);
      }
      continue;
    } else if (m.type === 'computer') {
      const rawUserCode = m.params.code || `; Computer Patch\n; Connect inputs and outputs, then write Loupe code!\n\n(def pitch (add (cv-in :1) (knob :main)))\n(<- (cv-out :1) (sine :note pitch))\n(<- (led :0) (gt (sine :note pitch) 2048))\n`;

      const definedOutputs = new Set();
      const connectedOutputs = new Set(state.cables.filter(c => c.fromId === id).map(c => c.fromPort));

      // Strip outer (patch ...) wrapper if pasted
      let userCode = rawUserCode.trim();
      if (userCode.startsWith('(patch')) {
        userCode = userCode.replace(/^\(\s*patch/, '');
        const lastParenIdx = userCode.lastIndexOf(')');
        if (lastParenIdx !== -1) {
          userCode = userCode.slice(0, lastParenIdx) + userCode.slice(lastParenIdx + 1);
        }
      }

      // 1. Tokenise the user code
      const tokens = [];
      const TOKEN_REGEX = /(;[^\n]*)|(:[\w?*+-]+)|(\(|\))|(-?\d[\w.]*)|([^\s();]+)|(\s+)/g;
      let match;
      while ((match = TOKEN_REGEX.exec(userCode)) !== null) {
        tokens.push({
          text: match[0],
          cmt: match[1],
          kw: match[2],
          paren: match[3],
          num: match[4],
          sym: match[5],
          ws: match[6]
        });
      }

      // 2. Identify all user-defined symbols in this module instance
      const userDefs = new Set();
      for (let i = 0; i < tokens.length; i++) {
        if (tokens[i].paren === '(' && tokens[i + 1]?.sym === 'def') {
          if (tokens[i + 2]?.sym) {
            const sym = tokens[i + 2].text;
            if (!PRELUDE_BUILTINS.has(sym)) {
              userDefs.add(sym);
            }
          } else if (tokens[i + 2]?.paren === '(') {
            let j = i + 3;
            while (j < tokens.length && tokens[j].paren !== ')') {
              if (tokens[j].sym) {
                const sym = tokens[j].text;
                if (!PRELUDE_BUILTINS.has(sym)) {
                  userDefs.add(sym);
                }
              }
              j++;
            }
          }
        }
      }

      // Helper function to transform a token array
      function transformTokenList(tokenList) {
        let output = '';
        let i = 0;

        function peek(offset = 0) {
          return tokenList[i + offset];
        }

        function consume() {
          return tokenList[i++];
        }

        while (i < tokenList.length) {
          // Check for (cv-in :1), (cv-in :2), etc.
          if (peek()?.paren === '(' &&
            (peek(1)?.sym === 'cv-in' || peek(1)?.sym === 'audio-in' || peek(1)?.sym === 'pulse-in') &&
            peek(2)?.kw &&
            peek(3)?.paren === ')') {
            const family = consume().text + consume().text; // '(' and family name
            const labelKw = consume().text; // kw e.g. :1
            consume(); // ')'

            const portNum = labelKw.slice(1); // '1' or '2'
            const portId = `${family.slice(1)}-in-${portNum}`; // e.g. cv-in-1

            // Find if cabled
            const cable = state.cables.find(c => c.toId === id && c.toPort === portId);
            const expr = cable ? getCabledSourceExpr(cable, allMods) : '0';
            output += expr;
            continue;
          }

          // Check for (knob :main), (knob :x), (knob :y)
          if (peek()?.paren === '(' && peek(1)?.sym === 'knob' && peek(2)?.kw && peek(3)?.paren === ')') {
            consume(); // '('
            consume(); // 'knob'
            const labelKw = consume().text; // :main, :x, :y
            consume(); // ')'

            const knobParam = labelKw.slice(1); // main, x, y
            const val = getKnobValue(m.type, knobParam, m.params[knobParam] ?? 2048);
            output += val;
            continue;
          }

          // Check for (switch :z)
          if (peek()?.paren === '(' && peek(1)?.sym === 'switch' && peek(2)?.kw && peek(3)?.paren === ')') {
            consume(); // '('
            consume(); // 'switch'
            const labelKw = consume().text; // :z
            consume(); // ')'

            const val = getKnobValue(m.type, 'z', m.params.z ?? 2048);
            output += val;
            continue;
          }

          const tok = consume();
          if (tok.sym && userDefs.has(tok.text)) {
            output += `${id}_${tok.text}`;
          } else {
            output += tok.text;
          }
        }
        return output;
      }

      // 3. Process tokens at top-level
      let transformedCode = '';
      let tIdx = 0;

      function peekTop(offset = 0) {
        return tokens[tIdx + offset];
      }

      function consumeTop() {
        return tokens[tIdx++];
      }

      function tokenAt(idx) {
        let i = idx;
        while (i < tokens.length && tokens[i].ws) i++;
        return tokens[i];
      }

      function idxAfterWs(offset) {
        let i = tIdx + offset;
        while (i < tokens.length && tokens[i].ws) i++;
        return i;
      }

      while (tIdx < tokens.length) {
        // Check for (cv-in :1), (cv-in :2), etc.
        if (peekTop()?.paren === '(' &&
          (peekTop(1)?.sym === 'cv-in' || peekTop(1)?.sym === 'audio-in' || peekTop(1)?.sym === 'pulse-in') &&
          peekTop(2)?.kw &&
          peekTop(3)?.paren === ')') {
          const family = consumeTop().text + consumeTop().text;
          const labelKw = consumeTop().text;
          consumeTop(); // ')'

          const portNum = labelKw.slice(1);
          const portId = `${family.slice(1)}-in-${portNum}`;

          const cable = state.cables.find(c => c.toId === id && c.toPort === portId);
          const expr = cable ? getCabledSourceExpr(cable, allMods) : '0';
          transformedCode += expr;
          continue;
        }

        // Check for (knob :main), (knob :x), (knob :y)
        if (peekTop()?.paren === '(' && peekTop(1)?.sym === 'knob' && peekTop(2)?.kw && peekTop(3)?.paren === ')') {
          consumeTop();
          consumeTop();
          const labelKw = consumeTop().text;
          consumeTop();

          const knobParam = labelKw.slice(1);
          const val = getKnobValue(m.type, knobParam, m.params[knobParam] ?? 2048);
          transformedCode += val;
          continue;
        }

        // Check for (switch :z)
        if (peekTop()?.paren === '(' && peekTop(1)?.sym === 'switch' && peekTop(2)?.kw && peekTop(3)?.paren === ')') {
          consumeTop();
          consumeTop();
          const labelKw = consumeTop().text;
          consumeTop();

          const val = getKnobValue(m.type, 'z', m.params.z ?? 2048);
          transformedCode += val;
          continue;
        }

        // Check for (<- (cv-out :1) expr) or (<- (cv-out :1 :v-oct) expr), etc.
        if (peekTop()?.paren === '(' && tokenAt(tIdx + 1)?.sym === '<-') {
          const openIdx = idxAfterWs(2);
          const sinkTypeTok = tokens[openIdx]?.paren === '(' ? tokenAt(openIdx + 1) : null;
          if (sinkTypeTok?.sym && ['cv-out', 'audio-out', 'pulse-out', 'led'].includes(sinkTypeTok.sym)) {
          consumeTop(); // '('
          consumeTop(); // '<-'
          while (peekTop()?.ws) consumeTop();
          consumeTop(); // '('
          const sinkType = consumeTop().text;
          while (peekTop()?.ws) consumeTop();
          const portTok = consumeTop();
          const labelVal = portTok.kw ? portTok.kw.slice(1) : portTok.text.replace(/^:/, '');

          while (peekTop() && peekTop().paren !== ')') {
            consumeTop();
          }
          if (peekTop()?.paren === ')') consumeTop();

          let depth = 1;
          let exprTokens = [];
          while (tIdx < tokens.length && depth > 0) {
            const tok = peekTop();
            if (tok.paren === '(') depth++;
            if (tok.paren === ')') depth--;
            if (depth > 0) {
              exprTokens.push(consumeTop());
            }
          }
          if (peekTop()?.paren === ')') consumeTop();

          const outputVar = `${id}${sinkType}-${labelVal}`;
          definedOutputs.add(`${sinkType}-${labelVal}`);
          const transformedExpr = transformTokenList(exprTokens);
          transformedCode += `(def ${outputVar} ${transformedExpr})`;
          continue;
          }
        }

        // Native Loupe sinks: (<- audio-out-1 expr), (<- led-0 expr), or internal (<- loop expr).
        if (peekTop()?.paren === '(' && tokenAt(tIdx + 1)?.sym === '<-') {
          const sinkTok = tokenAt(idxAfterWs(2));
          if (sinkTok?.sym && !sinkTok.paren) {
            consumeTop(); // '('
            consumeTop(); // '<-'
            while (peekTop()?.ws) consumeTop();
            const sinkSym = consumeTop().text;

            let depth = 1;
            const restTokens = [];
            while (tIdx < tokens.length && depth > 0) {
              const tok = peekTop();
              if (tok.paren === '(') depth++;
              if (tok.paren === ')') depth--;
              if (depth > 0) restTokens.push(consumeTop());
            }
            if (peekTop()?.paren === ')') consumeTop();

            const transformedRest = transformTokenList(restTokens);
            const isHardware = LENS_PORTS[sinkSym] || /^led-\d+$/.test(sinkSym);
            if (isHardware) {
              definedOutputs.add(sinkSym);
              transformedCode += `(def ${id}${sinkSym} ${transformedRest})`;
            } else {
              const target = userDefs.has(sinkSym) ? `${id}_${sinkSym}` : sinkSym;
              transformedCode += `(<- ${target} ${transformedRest})`;
            }
            continue;
          }
        }

        const tok = consumeTop();
        if (tok.sym && userDefs.has(tok.text)) {
          transformedCode += `${id}_${tok.text}`;
        } else {
          transformedCode += tok.text;
        }
      }

      // Define default zero values for any connected outputs not declared in user code
      for (const outPort of connectedOutputs) {
        if (!definedOutputs.has(outPort)) {
          transformedCode += `\n(def ${id}${outPort} 0)`;
        }
      }

      for (const outKey of definedOutputs) {
        const hw = LENS_PORTS[outKey];
        if (hw) sinkLines.push(`  (<- ${hw} ${id}${outKey})`);
        else if (/^led-\d+$/.test(outKey)) {
          sinkLines.push(`  (<- (led :${outKey.slice(4)}) ${id}${outKey})`);
        }
      }

      // Add the transformed code block to the lines
      lines.push(`  ; --- computer: ${id} ---`);
      const transformedLines = transformedCode.split('\n').map(l => l.trim()).filter(l => l.length > 0 && !l.startsWith(';'));
      for (const tl of transformedLines) {
        lines.push(`  ${tl}`);
      }
      lines.push(`  ; --- end of ${id} ---`);
      continue;
    } else if (m.type === 'tape') {
      const modeDef = def.knobs.find(k => k.param === 'mode');
      const modeIdx = Math.max(0, Math.min(modeDef.discrete.length - 1, Math.floor((m.params.mode ?? 0) / 4096 * modeDef.discrete.length)));
      const isBlank = modeDef.discrete[modeIdx] === 'blank';

      if (isBlank) {
        const stepsDef = def.knobs.find(k => k.param === 'steps');
        const stepsIdx = Math.max(0, Math.min(stepsDef.discrete.length - 1, Math.floor((m.params.steps ?? 0) / 4096 * stepsDef.discrete.length)));
        const stepsVal = stepsDef.discrete[stepsIdx];
        lines.push(`  (def ${id} (tape :len ${stepsVal}))`);
      } else {
        const pat = m.params.pattern || '(0 1 1 0 1 0 0 1)';
        let cleanPat = pat.trim();
        if (cleanPat.startsWith("'")) {
          cleanPat = cleanPat.substring(1);
        }
        lines.push(`  (def ${id} (tape '${cleanPat}))`);
      }
      lines.push(`  (def ${id}len (len ${id}))`);
      continue;
    } else if (m.type === 'wave-draw') {
      const currentRes = m.params.res || 64;
      const wf = m.params.waveform || Array.from({length: currentRes}, () => 2048);
      lines.push(`  (def ${id} (tape '(${wf.join(' ')})))`);
      lines.push(`  (def ${id}len (len ${id}))`);
      continue;
    } else if (m.type === 'hold') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');
      const gateCable = state.cables.find(c => c.toId === id && c.toPort === 'gate');

      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : null;
      const gateSig = gateCable ? getCabledSourceExpr(gateCable, allMods) : null;

      if (trigSig) {
        lines.push(`  (def ${id} (hold ${inSig} :trig ${trigSig}))`);
      } else if (gateSig) {
        lines.push(`  (def ${id} (hold ${inSig} :on ${gateSig}))`);
      } else {
        lines.push(`  (def ${id} ${inSig})`);
      }
      continue;
    } else if (m.type === 'expression') {
      const aCable = state.cables.find(c => c.toId === id && c.toPort === 'a');
      const bCable = state.cables.find(c => c.toId === id && c.toPort === 'b');
      const aSig = aCable ? getCabledSourceExpr(aCable, allMods) : '0';
      const bSig = bCable ? getCabledSourceExpr(bCable, allMods) : '0';

      const formula = m.params.expression || '(add a b)';
      const cleanFormula = formula
        .replace(/\b(a)\b/gi, aSig)
        .replace(/\b(b)\b/gi, bSig);

      lines.push(`  (def ${id} ${cleanFormula})`);
      continue;
    } else if (m.type === 'stereo-mixer') {
      const inACable = state.cables.find(c => c.toId === id && c.toPort === 'inA');
      const inBCable = state.cables.find(c => c.toId === id && c.toPort === 'inB');
      const inCCable = state.cables.find(c => c.toId === id && c.toPort === 'inC');
      const inDCable = state.cables.find(c => c.toId === id && c.toPort === 'inD');

      const volA = getKnobValue(m.type, 'volA', m.params.volA ?? 2048);
      const volB = getKnobValue(m.type, 'volB', m.params.volB ?? 2048);
      const volC = getKnobValue(m.type, 'volC', m.params.volC ?? 2048);
      const volD = getKnobValue(m.type, 'volD', m.params.volD ?? 2048);

      const panA = getKnobValue(m.type, 'panA', m.params.panA ?? 2048);
      const panB = getKnobValue(m.type, 'panB', m.params.panB ?? 2048);
      const panC = getKnobValue(m.type, 'panC', m.params.panC ?? 2048);
      const panD = getKnobValue(m.type, 'panD', m.params.panD ?? 2048);

      const muteAVal = getKnobValue(m.type, 'muteA', m.params.muteA ?? 0);
      const muteBVal = getKnobValue(m.type, 'muteB', m.params.muteB ?? 0);
      const muteCVal = getKnobValue(m.type, 'muteC', m.params.muteC ?? 0);
      const muteDVal = getKnobValue(m.type, 'muteD', m.params.muteD ?? 0);

      const masterVol = getKnobValue(m.type, 'master', m.params.master ?? 3000);

      const channels = [
        { cable: inACable, vol: volA, pan: panA, mute: muteAVal },
        { cable: inBCable, vol: volB, pan: panB, mute: muteBVal },
        { cable: inCCable, vol: volC, pan: panC, mute: muteCVal },
        { cable: inDCable, vol: volD, pan: panD, mute: muteDVal },
      ];

      const lSigs = [];
      const rSigs = [];

      channels.forEach((ch, idx) => {
        if (ch.cable && ch.mute !== 'muted') {
          const src = getCabledSourceExpr(ch.cable, allMods);
          
          let gL, gR;
          if (ch.pan <= 2048) {
            gL = (ch.vol * masterVol) / 4095;
            gR = (ch.vol * (ch.pan / 2048) * masterVol) / 4095;
          } else {
            gL = (ch.vol * ((4095 - ch.pan) / 2047) * masterVol) / 4095;
            gR = (ch.vol * masterVol) / 4095;
          }
          
          const gainL = Math.max(0, Math.min(4095, Math.round(gL)));
          const gainR = Math.max(0, Math.min(4095, Math.round(gR)));
          
          if (gainL > 0) {
            lSigs.push(gainL === 4095 ? src : `(vca ${src} ${gainL})`);
          }
          if (gainR > 0) {
            rSigs.push(gainR === 4095 ? src : `(vca ${src} ${gainR})`);
          }
        }
      });

      let finalL = '0';
      if (lSigs.length > 0) {
        finalL = lSigs.reduce((acc, sig) => `(add ${acc} ${sig})`);
      }
      let finalR = '0';
      if (rSigs.length > 0) {
        finalR = rSigs.reduce((acc, sig) => `(add ${acc} ${sig})`);
      }

      lines.push(`  (def ${id}outL (clip ${finalL}))`);
      lines.push(`  (def ${id}outR (clip ${finalR}))`);
      continue;
    } else if (m.type === 'mult') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      lines.push(`  (def ${id}out1 ${inSig})`);
      lines.push(`  (def ${id}out2 ${inSig})`);
      lines.push(`  (def ${id}out3 ${inSig})`);
      continue;
    } else if (m.type === 'wave-scanner') {
      const tapeCable = state.cables.find(c => c.toId === id && c.toPort === 'tape');
      const noteCable = state.cables.find(c => c.toId === id && c.toPort === 'note');
      const posCable = state.cables.find(c => c.toId === id && c.toPort === 'pos');
      const lenCable = state.cables.find(c => c.toId === id && c.toPort === 'len');

      const tapeSig = tapeCable ? getCabledSourceExpr(tapeCable, allMods) : '0';
      const noteSig = noteCable ? getCabledSourceExpr(noteCable, allMods) : 'C4';
      const posSig = posCable ? getCabledSourceExpr(posCable, allMods) : '0';
      const lenSig = lenCable ? getCabledSourceExpr(lenCable, allMods) : '4095';

      lines.push(`  (def ${id} (wave ${tapeSig} :note ${noteSig} :pos ${posSig} :len ${lenSig}))`);
      continue;

    } else if (m.type === 'cv-looper') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const recCable = state.cables.find(c => c.toId === id && c.toPort === 'rec');
      const trigCable = state.cables.find(c => c.toId === id && c.toPort === 'trig');

      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      const recSig = recCable ? getCabledSourceExpr(recCable, allMods) : '0';
      const trigSig = trigCable ? getCabledSourceExpr(trigCable, allMods) : 'master';

      const lenDef = def.knobs.find(k => k.param === 'len');
      const lenIdx = Math.max(0, Math.min(lenDef.discrete.length - 1, Math.floor((m.params.len ?? 2048) / 4096 * lenDef.discrete.length)));
      const lenVal = lenDef.discrete[lenIdx];

      lines.push(`  (def ${id}buf (tape :len ${lenVal}))`);
      lines.push(`  (def ${id}idx (counter :bars ${lenVal} :trig ${trigSig}))`);
      sinkLines.push(`  (on ${trigSig} :when ${recSig} (<- (seek ${id}buf ${id}idx) ${inSig}))`);
      lines.push(`  (def ${id}out (lookup ${id}buf ${id}idx))`);
      continue;
    } else if (m.type === 'granular') {
      const inCable = state.cables.find(c => c.toId === id && c.toPort === 'in');
      const freezeCable = state.cables.find(c => c.toId === id && c.toPort === 'freeze');
      const densityCable = state.cables.find(c => c.toId === id && c.toPort === 'density');
      const scatterCable = state.cables.find(c => c.toId === id && c.toPort === 'scatter');
      const sizeCable = state.cables.find(c => c.toId === id && c.toPort === 'size');
      const spreadCable = state.cables.find(c => c.toId === id && c.toPort === 'spread');

      const inSig = inCable ? getCabledSourceExpr(inCable, allMods) : '0';
      const freezeSig = freezeCable ? getCabledSourceExpr(freezeCable, allMods) : '0';

      const densityRaw = m.params.density ?? 1638;
      const densityExpr = getKnobValueExpr(m, 'density', densityRaw);
      const densitySig = densityCable
        ? `(add ${getCabledSourceExpr(densityCable, allMods)} ${densityExpr} :sat)`
        : densityExpr;

      const scatterRaw = m.params.scatter ?? 1024;
      const scatterExpr = getKnobValueExpr(m, 'scatter', scatterRaw);
      const scatterSig = scatterCable
        ? `(add ${getCabledSourceExpr(scatterCable, allMods)} ${scatterExpr} :sat)`
        : scatterExpr;

      const sizeRaw = m.params.size ?? 2048;
      const sizeExpr = getKnobValueExpr(m, 'size', sizeRaw);
      const sizeSig = sizeCable
        ? `(add ${getCabledSourceExpr(sizeCable, allMods)} ${sizeExpr} :sat)`
        : sizeExpr;

      const spreadRaw = m.params.spread ?? 2048;
      const spreadExpr = getKnobValueExpr(m, 'spread', spreadRaw);
      const spreadSig = spreadCable
        ? `(add ${getCabledSourceExpr(spreadCable, allMods)} ${spreadExpr} :sat)`
        : spreadExpr;

      const mixRaw = m.params.mix ?? 4095;
      const mixExpr = getKnobValueExpr(m, 'mix', mixRaw);

      const feedbackRaw = m.params.feedback ?? 0;
      const feedbackExpr = getKnobValueExpr(m, 'feedback', feedbackRaw);

      lines.push(`  (def ${id}buf (audio :seconds 0.8))`);
      lines.push(`  (def ${id}rec (<- ${id}buf (clip (add ${inSig} (vca (div (add ${id}mixL ${id}mixR) 2) ${feedbackExpr}) :sat)) :per-sample :when (not ${freezeSig})))`);
      lines.push(`  (def ${id}w (sel ${id}rec))`);
      lines.push(`  (def ${id}clk (clock :hz (over ${densitySig} 1 30)))`);
      lines.push(`  (def ${id}lenhz (over (sub 4095 ${sizeSig}) 2 80))`);
      lines.push(`  (def ${id}p1 (phasor :hz ${id}lenhz :sync ${id}clk))`);
      lines.push(`  (def ${id}p2 (xor ${id}p1 2048))`);

      // Window envelopes
      lines.push(`  (def ${id}env1 (sub 2048 (abs (sub ${id}p1 2048))))`);
      lines.push(`  (def ${id}env2 (sub 2048 (abs (sub ${id}p2 2048))))`);

      // Triggers
      lines.push(`  (def ${id}trig1 (trig ${id}p1))`);
      lines.push(`  (def ${id}trig2 (trig ${id}p2))`);

      // Random generators
      lines.push(`  (def ${id}rpos1 (random :trig ${id}trig1))`);
      lines.push(`  (def ${id}rpos2 (random :trig ${id}trig2))`);

      // Scatter positions (relative to write head)
      lines.push(`  (def ${id}pos1 (vca ${id}rpos1 ${scatterSig}))`);
      lines.push(`  (def ${id}pos2 (vca ${id}rpos2 ${scatterSig}))`);

      // Tap grain playback
      lines.push(`  (def ${id}g1 (tap ${id}buf ${id}pos1 :span))`);
      lines.push(`  (def ${id}g2 (tap ${id}buf ${id}pos2 :span))`);
      lines.push(`  (def ${id}g1e (vca ${id}g1 ${id}env1))`);
      lines.push(`  (def ${id}g2e (vca ${id}g2 ${id}env2))`);

      // Panning
      lines.push(`  (def ${id}pan1 (vca (sub ${id}rpos1 2048) ${spreadSig}))`);
      lines.push(`  (def ${id}pan2 (vca (sub (xor ${id}rpos2 2047) 2048) ${spreadSig}))`);

      lines.push(`  (def ${id}g1L (vca ${id}g1e (sub 2048 ${id}pan1 :sat)))`);
      lines.push(`  (def ${id}g1R (vca ${id}g1e (add 2048 ${id}pan1 :sat)))`);
      lines.push(`  (def ${id}g2L (vca ${id}g2e (sub 2048 ${id}pan2 :sat)))`);
      lines.push(`  (def ${id}g2R (vca ${id}g2e (add 2048 ${id}pan2 :sat)))`);

      // Sum L/R
      lines.push(`  (def ${id}mixL (clip (add ${id}g1L ${id}g2L)))`);
      lines.push(`  (def ${id}mixR (clip (add ${id}g1R ${id}g2R)))`);

      // Dry/Wet Mix: dry + (wet - dry) * mix
      lines.push(`  (def ${id}outL (clip (add ${inSig} (vca (sub ${id}mixL ${inSig}) ${mixExpr}) :sat)))`);
      lines.push(`  (def ${id}outR (clip (add ${inSig} (vca (sub ${id}mixR ${inSig}) ${mixExpr}) :sat)))`);
      continue;
    } else {
      expr = `(${m.type}`;
      for (const p of (def.inputs || [])) {
        const cable = state.cables.find(c => c.toId === id && c.toPort === p.id);
        if (cable) {
          const src = getCabledSourceExpr(cable, allMods);

          // Sum the cabled CV signal with the corresponding knob parameter (if any) with saturation clamping.
          // If there is a matching `${portId}amt` knob, scale the CV through it first.
          const kDef = (def.knobs || []).find(k => k.param === p.id);
          const amtDef = (def.knobs || []).find(k => k.param === `${p.id}amt`);
          let attSrc = src;
          if (amtDef) {
            const amtRaw = m.params[amtDef.param] !== undefined ? m.params[amtDef.param] : amtDef.def;
            const amtExpr = getKnobValueExpr(m, amtDef.param, amtRaw);
            const ccNum = m.params.__midi_cc && m.params.__midi_cc[amtDef.param];
            if (ccNum !== undefined) {
              attSrc = `(vca ${src} ${amtExpr})`;
            } else {
              const amtVal = getKnobValue(m.type, amtDef.param, amtRaw);
              if (amtVal <= 0) {
                attSrc = null; // CV depth zero — ignore CV entirely
              } else if (amtVal < 4095) {
                attSrc = `(vca ${src} ${amtVal})`;
              }
            }
          }
          if (kDef) {
            const knobRaw = m.params[kDef.param] ?? kDef.def;
            const knobExpr = getKnobValueExpr(m, kDef.param, knobRaw);
            if (attSrc) {
              const isPitch = p.id === 'note' || p.id === 'pitch';
              if (isPitch) {
                const center = getPitchCenter(m.type, kDef.param);
                const centsVal = (p.id === 'pitch' && (def.knobs || []).some(k => k.param === 'cents'))
                  ? ((getKnobValue(m.type, 'cents', m.params.cents ?? 2048) - 2048) / 204.8)
                  : 0;
                const ccNum = m.params.__midi_cc && m.params.__midi_cc[kDef.param];
                if (ccNum !== undefined) {
                  expr += ` :${p.id} (add ${attSrc} (sub ${knobExpr} ${center}))`;
                } else {
                  const knobVal = getKnobValue(m.type, kDef.param, knobRaw);
                  const transpose = Math.round(knobVal - center + centsVal);
                  expr += ` :${p.id} ${transpose !== 0 ? `(add ${attSrc} ${transpose})` : attSrc}`;
                }
              } else {
                expr += ` :${p.id} (add ${knobExpr} ${attSrc} :sat)`;
              }
            } else {
              expr += ` :${p.id} ${knobExpr}`; // CV fully attenuated, knob only
            }
          } else if (attSrc) {
            expr += ` :${p.id} ${attSrc}`;
          } else {
            expr += ` :${p.id} 0`; // CV zero, no knob
          }
        } else {
          const kDef = (def.knobs || []).find(k => k.param === p.id);
          const rawVal = m.params[p.id] !== undefined ? m.params[p.id] : (kDef ? kDef.def : undefined);
          if (rawVal !== undefined) {
            const valExpr = getKnobValueExpr(m, p.id, rawVal);
            if (valExpr !== 'undefined' && valExpr !== 'NaN') {
              expr += ` :${p.id} ${valExpr}`;
            }
          } else {
            // No knob, no cable, no param — emit safe default 0 so required
            // inputs (like hold's :on gate) don't cause a compile error.
            expr += ` :${p.id} 0`;
          }
        }
      }
      // Knob variables not wired by inputs — skip amt attenuator knobs, they only
      // affect codegen when their corresponding CV input port is cabled.
      for (const k of (def.knobs || [])) {
        const isAmtKnob = k.param.endsWith('amt') &&
          (def.inputs || []).some(i => i.id === k.param.slice(0, -3));
        if (isAmtKnob) continue;
        if (!(def.inputs || []).some(i => i.id === k.param)) {
          const rawVal = m.params[k.param] ?? k.def;
          const valExpr = getKnobValueExpr(m, k.param, rawVal);
          expr += ` :${k.param} ${valExpr}`;
        }
      }
      expr += ')';
    }
    lines.push(`  (def ${id} ${expr})`);
  }

  // Cable links to Hardware Sinks
  for (const c of state.cables) {
    const toDef = allMods.find(m => m.id === c.toId);
    if (!toDef || !MODULE_DEFS[toDef.type]?.isHW) continue;
    let hw = LENS_PORTS[c.toPort];
    if (!hw) continue;

    const fromMod = allMods.find(m => m.id === c.fromId);

    if (c.toPort === 'cv-out-1' || c.toPort === 'cv-out-2') {
      const isPitchSource = isModulePortPitch(c.fromId, c.fromPort);
      if (isPitchSource) {
        const num = c.toPort === 'cv-out-1' ? '1' : '2';
        hw = `(cv-out :${num} :v-oct)`;
      }
    }

    const src = getCabledSourceExpr(c, allMods);
    sinkLines.push(`  (<- ${hw} ${src})`);
  }

  if (sinkLines.length) { lines.push(''); lines.push(...sinkLines); }
  lines.push(')');

  // Append visual state layout metadata as comment at the bottom
  const layoutMetadata = {
    rows: state.rows,
    cables: state.cables,
    nextId: state.nextId,
    flareMidiChannel: state.flareMidiChannel || 16
  };
  lines.push(`\n; flare_layout: ${JSON.stringify(layoutMetadata)}`);

  const code = lines.join('\n');
  if (textOnly) return code;

  const lastCodeFunc = lastGeneratedCode.split('; flare_layout:')[0] || '';
  const currentCodeFunc = code.split('; flare_layout:')[0] || '';
  const hasCodeChanged = (currentCodeFunc !== lastCodeFunc);

  lastGeneratedCode = code;

  $('codeArea').value = code;
  if (hasCodeChanged) {
    compileAndStatus(code);
  }

  // Autosave current patch state to safeStorage
  try {
    safeStorage.setItem('flare_autosave', JSON.stringify({ state: layoutMetadata }));
  } catch (e) {
    console.error('Autosave failed:', e);
  }
  return code;
}

// ═══════════════════════════════════════════════════════════════════════
// 13. COMPILE & MIDI
// ═══════════════════════════════════════════════════════════════════════

function compileAndStatus(code) {
  const statusEl = $('status');
  try {
    const ast = Lens.read(code);
    const expanded = Lens.expand(ast, { loadFile: __webLoadFile });
    const lowered = Lens.lower(expanded);
    const sched = Lens.schedule(lowered);
    compiledSnapshot = Lens.encode(sched, lowered);
    rebuildKnobConstantMap();

    nodesCount = lowered.slots?.length ?? 0;
    const budget = sched.budget;
    const c0Pct = (budget.core0.total / budget.core0.budget * 100).toFixed(0);
    const c1Pct = (budget.core1.total / budget.core1.budget * 100).toFixed(0);
    isCpuOverBudget = !budget.ok;

    const cpuEl = $('cpu-status');
    if (cpuEl) {
      const over = !budget.ok ? " (OVER BUDGET!)" : "";
      cpuEl.textContent = `CPU: ${c0Pct}% / ${c1Pct}%${over}`;
      cpuEl.className = !budget.ok ? 'status-pill err' : 'status-pill ok';
      cpuEl.style.display = 'block';
    }
    const memEl = $('mem-status');
    if (memEl && sched.memory) {
      const mem = sched.memory;
      const audioPct = (mem.audio.total / mem.audio.budget * 100).toFixed(0);
      const statePct = (mem.nodestate.total / mem.nodestate.budget * 100).toFixed(0);
      const over = !mem.ok ? " (OVER MEMORY!)" : "";
      memEl.textContent = `MEM: Audio ${audioPct}% · State ${statePct}%${over}`;
      memEl.className = !mem.ok ? 'status-pill err' : 'status-pill ok';
      memEl.style.display = 'block';
    }

    statusEl.textContent = `${nodesCount} nodes · ${compiledSnapshot.length} B${midiOut ? ' · ' + midiOut.name : ''}`;
    statusEl.className = 'ok';
    $('sendBtn').disabled = $('saveCardBtn').disabled = !compiledSnapshot;

    // Update card slot borders to glowing green
    document.querySelectorAll('.computer-card-slot').forEach(slot => {
      slot.style.borderColor = '#629f52';
      slot.style.boxShadow = '0 0 8px rgba(98,159,82,0.6), inset 0 2px 5px rgba(0,0,0,0.9)';
    });

    triggerLiveUpdate();
  } catch (e) {
    compiledSnapshot = null;
    statusEl.textContent = e.message;
    statusEl.className = 'err';

    // Update card slot borders to glowing red
    document.querySelectorAll('.computer-card-slot').forEach(slot => {
      slot.style.borderColor = '#e63946';
      slot.style.boxShadow = '0 0 8px rgba(230,57,70,0.6), inset 0 2px 5px rgba(0,0,0,0.9)';
    });

    const cpuEl = $('cpu-status');
    if (cpuEl) cpuEl.style.display = 'none';
    const memEl = $('mem-status');
    if (memEl) memEl.style.display = 'none';
    $('sendBtn').disabled = $('saveCardBtn').disabled = true;
  }
}


async function connectMidi() {
  const s = $('status');
  if (!navigator.requestMIDIAccess) { s.textContent = 'WebMIDI not supported'; s.className = 'err'; return; }
  try {
    const midi = await navigator.requestMIDIAccess({ sysex: true });
    midiOut = [...midi.outputs.values()].find(p => /lens|workshop|music thing/i.test(p.name)) || null;
    midiIn = [...midi.inputs.values()].find(p => /lens|workshop|music thing/i.test(p.name)) || null;
    if (!midiOut || !midiIn) { s.textContent = 'Workshop card not found'; s.className = 'err'; return; }
    midiIn.onmidimessage = ev => { const m = Lens.parse([...ev.data]); if (m && ackWaiter) { const w = ackWaiter; ackWaiter = null; w(m); } };
    $('connectMidiBtn').textContent = 'Connected';
    $('connectMidiBtn').style.display = 'none';
    $('midiConnectedGroup').style.display = 'flex';
    lastUploadedSnapshot = null;
    generateCode();
  } catch (e) {
    s.textContent = 'MIDI: ' + e.message; s.className = 'err';
    $('connectMidiBtn').textContent = 'Connect MIDI';
    $('connectMidiBtn').style.display = 'inline-block';
    $('midiConnectedGroup').style.display = 'none';
  }
}

const recvAck = (ms = 1500) => new Promise((res, rej) => {
  const t = setTimeout(() => { ackWaiter = null; rej(new Error('ACK timeout')); }, ms);
  ackWaiter = m => { clearTimeout(t); res(m); };
});

async function writeSnapshot() {
  for (let t = 0; t < 4; t++) {
    midiOut.send([...Lens.frame(Lens.CMD.WRITE_STATE, compiledSnapshot)]);
    const m = await recvAck();
    if (m.cmd === Lens.CMD.ACK) return;
    if (m.cmd === Lens.CMD.NACK && m.payload[1] === 0x06) { await new Promise(r => setTimeout(r, 150)); continue; }
    throw new Error(`NACK ${m.payload[1]}`);
  }
  throw new Error('Card busy');
}

async function sendPatch() {
  if (!compiledSnapshot) return;
  if (!midiOut) {
    alert('MIDI not connected. Please click "Connect MIDI" to connect your Eurorack card first.');
    return;
  }
  const s = $('status'); $('sendBtn').disabled = true;
  s.textContent = 'sending…';
  try {
    await writeSnapshot();
    lastUploadedSnapshot = new Uint8Array(compiledSnapshot);
    s.textContent = 'playing!';
    s.className = 'ok';
  }
  catch (e) { s.textContent = e.message; s.className = 'err'; }
  $('sendBtn').disabled = !compiledSnapshot;
}

async function saveToFlash() {
  if (!compiledSnapshot) return;
  if (!midiOut) {
    alert('MIDI not connected. Please click "Connect MIDI" to connect your Eurorack card first.');
    return;
  }
  const s = $('status'); $('saveCardBtn').disabled = true;
  s.textContent = 'writing to flash…';
  try {
    await writeSnapshot();
    lastUploadedSnapshot = new Uint8Array(compiledSnapshot);
    await new Promise(r => setTimeout(r, 700));
    midiOut.send([...Lens.frame(Lens.CMD.SAVE_STATE)]);
    const m = await recvAck(3000);
    if (m.cmd !== Lens.CMD.ACK) throw new Error('flash save failed');
    s.textContent = 'saved! rebooting…'; s.className = 'ok';
    $('connectMidiBtn').textContent = 'Connect MIDI';
    $('connectMidiBtn').style.display = 'inline-block';
    $('midiConnectedGroup').style.display = 'none';
    midiOut = midiIn = null;
    lastUploadedSnapshot = null;
  } catch (e) { s.textContent = e.message; s.className = 'err'; }
  generateCode();
}

// ═══════════════════════════════════════════════════════════════════════
// 14. INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════


let isResizingCodePane = false;
let codePaneWidth = 270;
let codePaneMaximized = false;

function toggleCode() {
  const pane = $('codePane');
  const btn = $('toggleCodeBtn');
  if (pane.style.display === 'none') {
    pane.style.display = 'flex';
    pane.style.width = codePaneMaximized ? '100%' : `${codePaneWidth}px`;
    btn.textContent = 'Hide Code';
  } else {
    pane.style.display = 'none';
    btn.textContent = 'Show Code';
  }
  // Recalculate layout and redraw cables immediately after styles apply
  setTimeout(redrawCables, 50);
}

function setupCodePaneResizeAndControls() {
  const pane = $('codePane');
  const resizer = $('codePaneResizer');
  const maxBtn = $('codeMaximizeBtn');
  const closeBtn = $('codeCloseBtn');

  if (!pane || !resizer || !maxBtn || !closeBtn) return;

  maxBtn.addEventListener('click', () => {
    if (codePaneMaximized) {
      pane.style.width = `${codePaneWidth}px`;
      maxBtn.textContent = '⛶';
      codePaneMaximized = false;
    } else {
      pane.style.width = '100%';
      maxBtn.textContent = '❐';
      codePaneMaximized = true;
    }
    redrawCables();
  });

  closeBtn.addEventListener('click', () => {
    pane.style.display = 'none';
    const btn = $('toggleCodeBtn');
    if (btn) btn.textContent = 'Show Code';
    redrawCables();
  });

  resizer.addEventListener('pointerdown', initResize);

  function initResize(e) {
    e.preventDefault();
    e.stopPropagation();
    isResizingCodePane = true;
    resizer.classList.add('active');
    document.body.style.cursor = 'ew-resize';
    document.body.style.userSelect = 'none';

    document.addEventListener('pointermove', handleResize);
    document.addEventListener('pointerup', stopResize);
  }

  function handleResize(e) {
    if (!isResizingCodePane) return;
    const clientX = e.clientX;
    const newWidth = window.innerWidth - clientX;

    if (newWidth > 180 && newWidth < window.innerWidth - 100) {
      codePaneWidth = newWidth;
      codePaneMaximized = false;
      pane.style.width = `${newWidth}px`;
      maxBtn.textContent = '⛶';
      redrawCables();
    }
  }

  function stopResize(e) {
    if (!isResizingCodePane) return;
    isResizingCodePane = false;
    resizer.classList.remove('active');
    document.body.style.cursor = '';
    document.body.style.userSelect = '';
    document.removeEventListener('pointermove', handleResize);
    document.removeEventListener('pointerup', stopResize);
  }
}

// ── DX7 SysEx Parser & Importer ──────────────────────────────────────
const DX7_ALGORITHMS = {
  1: { carriers: [1, 3], edges: [[2, 1], [4, 3], [5, 4], [6, 5]], feedback: [6, 6] },
  2: { carriers: [1, 3], edges: [[2, 1], [4, 3], [5, 4], [6, 5]], feedback: [2, 2] },
  3: { carriers: [1, 4], edges: [[2, 1], [3, 2], [5, 4], [6, 5]], feedback: [6, 6] },
  4: { carriers: [1, 4], edges: [[2, 1], [3, 2], [5, 4], [6, 5]], feedback: [4, 6] },
  5: { carriers: [1, 3, 5], edges: [[2, 1], [4, 3], [6, 5]], feedback: [6, 6] },
  6: { carriers: [1, 3, 5], edges: [[2, 1], [4, 3], [6, 5]], feedback: [5, 6] },
  7: { carriers: [1, 3], edges: [[2, 1], [4, 3], [5, 3], [6, 5]], feedback: [6, 6] },
  8: { carriers: [1, 3], edges: [[2, 1], [4, 3], [5, 3], [6, 5]], feedback: [4, 4] },
  9: { carriers: [1, 3], edges: [[2, 1], [4, 3], [5, 3], [6, 5]], feedback: [2, 2] },
  10: { carriers: [1, 4], edges: [[2, 1], [3, 2], [5, 4], [6, 4]], feedback: [3, 3] },
  11: { carriers: [1, 4], edges: [[2, 1], [3, 2], [5, 4], [6, 4]], feedback: [6, 6] },
  12: { carriers: [1, 3], edges: [[2, 1], [4, 3], [5, 3], [6, 3]], feedback: [2, 2] },
  13: { carriers: [1, 3], edges: [[2, 1], [4, 3], [5, 3], [6, 3]], feedback: [6, 6] },
  14: { carriers: [1, 3], edges: [[2, 1], [4, 3], [5, 4], [6, 4]], feedback: [6, 6] },
  15: { carriers: [1, 3], edges: [[2, 1], [4, 3], [5, 4], [6, 4]], feedback: [2, 2] },
  16: { carriers: [1], edges: [[2, 1], [3, 1], [5, 1], [4, 3], [6, 5]], feedback: [6, 6] },
  17: { carriers: [1], edges: [[2, 1], [3, 1], [5, 1], [4, 3], [6, 5]], feedback: [2, 2] },
  18: { carriers: [1], edges: [[2, 1], [3, 1], [4, 1], [5, 4], [6, 5]], feedback: [3, 3] },
  19: { carriers: [1, 4, 5], edges: [[2, 1], [3, 2], [6, 4], [6, 5]], feedback: [6, 6] },
  20: { carriers: [1, 2, 4], edges: [[3, 1], [3, 2], [5, 4], [6, 4]], feedback: [3, 3] },
  21: { carriers: [1, 2, 4, 5], edges: [[3, 1], [3, 2], [6, 4], [6, 5]], feedback: [3, 3] },
  22: { carriers: [1, 3, 4, 5], edges: [[2, 1], [6, 3], [6, 4], [6, 5]], feedback: [6, 6] },
  23: { carriers: [1, 2, 4, 5], edges: [[3, 2], [6, 4], [6, 5]], feedback: [6, 6] },
  24: { carriers: [1, 2, 3, 4, 5], edges: [[6, 3], [6, 4], [6, 5]], feedback: [6, 6] },
  25: { carriers: [1, 2, 3, 4, 5], edges: [[6, 4], [6, 5]], feedback: [6, 6] },
  26: { carriers: [1, 2, 4], edges: [[3, 2], [5, 4], [6, 4]], feedback: [6, 6] },
  27: { carriers: [1, 2, 4], edges: [[3, 2], [5, 4], [6, 4]], feedback: [3, 3] },
  28: { carriers: [1, 3, 6], edges: [[2, 1], [4, 3], [5, 4]], feedback: [5, 5] },
  29: { carriers: [1, 2, 3, 5], edges: [[4, 3], [6, 5]], feedback: [6, 6] },
  30: { carriers: [1, 2, 3, 6], edges: [[4, 3], [5, 4]], feedback: [5, 5] },
  31: { carriers: [1, 2, 3, 4, 5], edges: [[6, 5]], feedback: [6, 6] },
  32: { carriers: [1, 2, 3, 4, 5, 6], edges: [], feedback: [6, 6] },
};

function unpackDx7Op(b, o) {
  return {
    r: [b[o], b[o + 1], b[o + 2], b[o + 3]],
    l: [b[o + 4], b[o + 5], b[o + 6], b[o + 7]],
    outLevel: b[o + 14],
    mode: b[o + 15] & 1,
    coarse: (b[o + 15] >> 1) & 31,
    fine: b[o + 16],
    detune: (b[o + 12] >> 3) & 15,
  };
}

function parseDx7Voice(b128) {
  const ops = [];
  for (let i = 0; i < 6; i++) ops[5 - i] = unpackDx7Op(b128, i * 17);
  return {
    ops,
    algorithm: (b128[110] & 31) + 1,
    feedback: b128[111] & 7,
    transpose: b128[117],
    name: Array.from(b128.slice(118, 128)).map(c => String.fromCharCode(c)).join('').replace(/[^\x20-\x7e]/g, ' ').trim(),
    data: Array.from(b128)
  };
}

function parseDx7Bank(buf) {
  let body = buf;
  if (buf[0] === 0xF0) body = buf.slice(6, 6 + 4096);
  const voices = [];
  for (let v = 0; v < 32; v++) voices.push(parseDx7Voice(body.slice(v * 128, v * 128 + 128)));
  return voices;
}

const DX7_LEVELLUT = [0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46];
const scaleDx7Out = x => x >= 20 ? 28 + x : DX7_LEVELLUT[x < 0 ? 0 : (x > 19 ? 19 : x)];

function opDx7Gain(egL, outLevel) {
  const outlevel_ = Math.min(127, scaleDx7Out(outLevel)) << 5;
  let act = ((scaleDx7Out(egL) >> 1) << 6) + outlevel_ - 4256;
  if (act < 16) act = 16;
  return Math.pow(2, (act - 3584) / 256);
}

function dx7egLevel(egL, outLevel) {
  const outlevel_ = Math.min(127, scaleDx7Out(outLevel)) << 5;
  let act = ((scaleDx7Out(egL) >> 1) << 6) + outlevel_ - 4256;
  if (act < 16) act = 16;
  return Math.max(0, Math.min(255, act >> 4));
}

function dx7RatioOf(op) {
  const base = op.coarse === 0 ? 0.5 : op.coarse;
  return base * (1 + op.fine / 100);
}

function dx7PitchOffset(op) {
  const oct = Math.log2(dx7RatioOf(op));
  const semisF = 12 * oct;
  const semi = Math.round(semisF);
  let cents = Math.round((semisF - semi) * 100) + Math.round((op.detune - 7) * 2.7);
  return { semi, cents };
}

function emitDx7VoiceFn(voice, name) {
  const alg = DX7_ALGORITHMS[voice.algorithm];
  if (!alg) throw new Error('algorithm ' + voice.algorithm + ' not in table yet');
  const fbTarget = alg.feedback ? alg.feedback[1] : null;
  const L = [];
  L.push('; DX7 voice "' + voice.name + '"  algorithm ' + voice.algorithm + '  feedback ' + voice.feedback);
  L.push('(def ' + name + ' (fn (:gate :pitch => :out)');
  const done = new Set();
  const emit = n => {
    if (done.has(n)) return;
    done.add(n);
    const mods = (alg.edges || []).filter(e => e[1] === n && e[0] !== n).map(e => e[0]);
    mods.forEach(emit);
    const op = voice.ops[n - 1];
    const { semi, cents } = dx7PitchOffset(op);
    const note = semi === 0 ? 'pitch' : '(add pitch ' + semi + ')';
    const centsArg = cents !== 0 ? ' :cents ' + cents : '';
    const Le = op.l.map(l => dx7egLevel(l, op.outLevel));
    const env = '(dxeg :gate gate :r1 ' + op.r[0] + ' :r2 ' + op.r[1] + ' :r3 ' + op.r[2] + ' :r4 ' + op.r[3] +
      ' :l1 ' + Le[0] + ' :l2 ' + Le[1] + ' :l3 ' + Le[2] + ' :l4 ' + Le[3] + ')';
    const pmParts = mods.map(m => 'op' + m);
    const pmSrc = pmParts.length > 1 ? '(mix ' + pmParts.join(' ') + ')' : pmParts[0];
    const pm = pmParts.length ? ' :pm ' + pmSrc : '';
    if (n === fbTarget && voice.feedback > 0) {
      const FBSCALE = Math.round(4095 * voice.feedback / 7);
      L.push('  (def op' + n + 'env ' + env + ')');
      L.push('  (def op' + n + ' (vca (sine :note ' + note + centsArg + pm +
        ' :fb (vca op' + n + 'env ' + FBSCALE + ')) op' + n + 'env))');
    } else {
      const body = '(vca (sine :note ' + note + centsArg + pm + ') ' + env + ')';
      L.push('  (def op' + n + ' ' + body + ')');
    }
  };
  alg.carriers.forEach(emit);
  const sum = alg.carriers.length === 1 ? 'op' + alg.carriers[0]
    : '(mix ' + alg.carriers.map(c => 'op' + c).join(' ') + ')';
  L.push('  (<- out (vca ' + sum + ' 2047))))');
  return L.join('\n') + '\n';
}

function showVoiceSelectionModal(voices, onSelect) {
  const modal = el('div', 'voice-modal-overlay', {
    style: 'position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.85); display: flex; align-items: center; justify-content: center; z-index: 10000; font-family: sans-serif;'
  });
  const box = el('div', 'voice-modal-box', {
    style: 'background: #1e1d1b; border: 1px solid #3a3835; border-radius: 6px; padding: 20px; width: 360px; max-height: 80vh; display: flex; flex-direction: column; box-shadow: 0 10px 30px rgba(0,0,0,0.5);'
  });
  const title = el('h3', '', {
    textContent: 'Select DX7 Preset to Import',
    style: 'margin: 0 0 15px 0; color: #9fd08a; font-size: 16px; border-bottom: 1px solid #3a3835; padding-bottom: 8px;'
  });
  box.appendChild(title);

  const list = el('div', 'voice-modal-list', {
    style: 'overflow-y: auto; flex: 1; display: flex; flex-direction: column; gap: 4px; padding-right: 4px;'
  });

  voices.forEach((v, idx) => {
    const btn = el('button', 'voice-modal-item', {
      textContent: `${idx + 1}: ${v.name || 'UNNAMED'}`,
      style: 'background: #2b2927; border: 1px solid #3c3a38; color: #ddd; padding: 8px 12px; text-align: left; border-radius: 4px; cursor: pointer; font-family: monospace; font-size: 12px; transition: all 0.1s;'
    });
    btn.addEventListener('mouseenter', () => {
      btn.style.borderColor = '#9fd08a';
      btn.style.background = '#32302e';
    });
    btn.addEventListener('mouseleave', () => {
      btn.style.borderColor = '#3c3a38';
      btn.style.background = '#2b2927';
    });
    btn.addEventListener('click', () => {
      document.body.removeChild(modal);
      onSelect(v, idx);
    });
    list.appendChild(btn);
  });
  box.appendChild(list);

  const cancel = el('button', '', {
    textContent: 'Cancel',
    style: 'margin-top: 15px; background: #4a1d1d; border: 1px solid #6a2525; color: #fff; padding: 8px; border-radius: 4px; cursor: pointer; font-weight: bold;'
  });
  cancel.addEventListener('click', () => {
    document.body.removeChild(modal);
  });
  box.appendChild(cancel);

  modal.appendChild(box);
  document.body.appendChild(modal);
}

function showModuleHelpModal(type) {
  const def = MODULE_DEFS[type];
  if (!def) return;

  const modal = el('div', 'help-modal-overlay', {
    style: 'position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.85); display: flex; align-items: center; justify-content: center; z-index: 100000; font-family: system-ui, -apple-system, sans-serif;'
  });
  modal.addEventListener('pointerdown', e => e.stopPropagation());
  modal.addEventListener('mousedown', e => e.stopPropagation());
  modal.addEventListener('click', e => {
    if (e.target === modal) {
      document.body.removeChild(modal);
    }
  });

  const box = el('div', 'help-modal-box', {
    style: 'background: #1e1d1b; border: 1px solid #dfb86c; border-radius: 6px; padding: 20px; width: 480px; max-height: 80vh; display: flex; flex-direction: column; box-shadow: 0 10px 30px rgba(0,0,0,0.8), 0 0 15px rgba(223,184,108,0.15); color: #ddd; overflow-y: auto;'
  });

  const header = el('div', '', {
    style: 'display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; border-bottom: 1px solid #3a3835; padding-bottom: 8px;'
  });

  const title = el('h3', '', {
    textContent: `${def.title} (Help)`,
    style: 'margin: 0; color: #dfb86c; font-size: 18px; font-weight: 700;'
  });
  header.appendChild(title);
  box.appendChild(header);

  // Description
  const modDesc = MODULE_DESCRIPTIONS[type] || 'Modular synthesizer unit.';
  const descEl = el('p', '', {
    textContent: modDesc,
    style: 'margin: 0 0 16px 0; font-size: 13px; line-height: 1.5; color: #b5b0a5;'
  });
  box.appendChild(descEl);

  // Knobs
  if (def.knobs && def.knobs.length > 0) {
    const sectionTitle = el('h4', '', { textContent: 'Controls & Knobs', style: 'margin: 12px 0 6px 0; font-size: 13px; color: #dfb86c; text-transform: uppercase; letter-spacing: 0.05em;' });
    box.appendChild(sectionTitle);

    const list = el('ul', '', { style: 'margin: 0 0 16px 0; padding-left: 18px; font-size: 12px; line-height: 1.45;' });
    for (const knob of def.knobs) {
      const li = el('li', '', { style: 'margin-bottom: 6px;' });
      const kDesc = CONTROL_DESCRIPTIONS[knob.param] || 'Parameter adjustment control.';
      li.innerHTML = `<strong>${knob.label}</strong>: ${kDesc}`;
      list.appendChild(li);
    }
    box.appendChild(list);
  }

  // Jacks (Inputs/Outputs)
  const hasInputs = def.inputs && def.inputs.length > 0;
  const hasOutputs = def.outputs && def.outputs.length > 0;
  if (hasInputs || hasOutputs) {
    const sectionTitle = el('h4', '', { textContent: 'Jacks & Connections', style: 'margin: 12px 0 6px 0; font-size: 13px; color: #dfb86c; text-transform: uppercase; letter-spacing: 0.05em;' });
    box.appendChild(sectionTitle);

    const list = el('ul', '', { style: 'margin: 0 0 16px 0; padding-left: 18px; font-size: 12px; line-height: 1.45;' });

    if (hasInputs) {
      for (const input of def.inputs) {
        const li = el('li', '', { style: 'margin-bottom: 6px;' });
        const jDesc = JACK_DESCRIPTIONS[input.id] || 'Signal connection port.';
        li.innerHTML = `<span style="color:#b5dbfc"><strong>${input.label}</strong> (Input)</span>: ${jDesc}`;
        list.appendChild(li);
      }
    }
    if (hasOutputs) {
      for (const output of def.outputs) {
        const li = el('li', '', { style: 'margin-bottom: 6px;' });
        const jDesc = JACK_DESCRIPTIONS[output.id] || 'Signal connection port.';
        li.innerHTML = `<span style="color:#fcd5b5"><strong>${output.label}</strong> (Output)</span>: ${jDesc}`;
        list.appendChild(li);
      }
    }
    box.appendChild(list);
  }

  // Buttons/Footer
  const footer = el('div', '', { style: 'display: flex; justify-content: flex-end; margin-top: 12px; border-top: 1px solid #3a3835; padding-top: 12px;' });
  const closeBtn = el('button', '', {
    textContent: 'Close',
    style: 'background: #2b2927; border: 1px solid #dfb86c; color: #dfb86c; font-size: 12px; padding: 6px 16px; border-radius: 4px; cursor: pointer; transition: all 0.1s ease;'
  });
  closeBtn.addEventListener('click', () => {
    document.body.removeChild(modal);
  });
  footer.appendChild(closeBtn);
  box.appendChild(footer);

  modal.appendChild(box);
  document.body.appendChild(modal);
}

const PRELUDE_BUILTINS = new Set([
  'patch', 'def', 'fn', 'on', '<-', 'use', 'tape', 'audio', 'score', 'morph', 'normal', 'connected',
  'add', 'sub', 'mul', 'div', 'mod', 'transpose', 'invert', 'spread', 'shift', 'mask', 'bit', 'xor', 'and', 'or',
  'gt', 'gte', 'lt', 'lte', 'eq', 'ne',
  'knob', 'cv-in', 'pulse-in', 'audio-in', 'switch', 'led',
  'cv-out', 'audio-out', 'pulse-out',
  'midi-note', 'midi-gate', 'midi-velocity', 'midi-bend', 'midi-pressure', 'midi-trig', 'midi-cc', 'midi-clock', 'midi-playing',
  'phasor', 'sine', 'triangle', 'saw', 'square', 'wavetable', 'wt', 'follow', 'average', 'lpf', 'hpf', 'lpg',
  'envfollow', 'vcf', 'lpf2', 'hpf2', 'bpf2', 'envelope', 'adsr', 'dxeg', 'dx', 'slew', 'vca', 'ring', 'mix',
  'wavefold', 'crush', 'saturate', 'shape', 'noise', 'random', 'chance', 'walk', 'hold', 'pickup', 'toggle',
  'schmitt', 'gate', 'clock', 'master', 'z1', 'varispeed', 'feedback', 'diff', 'trig', 'turns', 'edge', 'fall',
  'detent', 'range', 'every', 'euclid', 'groove', 'lookup', 'step', 'seek', 'len', 'counter', 'wave', 'tap', 'record',
  'play', 'loop', 'up', 'mid', 'down', 'v-oct', 'cv', 'snap', 'quantise', 'degree', 'pitch', 'thru', 'squint',
  'if', 'not', 'max', 'min', 'window', 'abs', 'rect', 'exp2', 'log2', 'onsets', 'gates', 'hits',
  'kick', 'snare', 'hat', 'pluck', 'reverb', 'chorus', 'flanger', 'compressor', 'delay',
  'VMAX', 'VMID', 'VMIN', 'SMAX', 'OCTAVE', 'MIDI-MAX', 'half', 'vmax', 'vmid', 'vmin',
  'minor', 'major', 'minor-pent', 'major-pent', 'dorian', 'phrygian', 'lydian', 'mixolydian', 'chromatic', 'scale-masks',
  'maj3', 'min3', 'dim', 'aug', 'sus2', 'sus4', 'maj7', 'min7', 'dom7', 'dim7',
  'bipolar', 'unipolar', 'signed', 'brightness', 'dist', 'clip', 'sum', 'beat', 'notes', 'midi-score', 'cap', 'pos', 'lng',
  'four-on-floor', 'backbeat', 'eighths', 'offbeat', 'sixteenths', 'downbeat', 'tresillo', 'cinquillo', 'habanera',
  'son-clave', 'rumba-clave', 'bossa', 'sel',
  'C-1', 'C#-1', 'D-1', 'D#-1', 'E-1', 'F-1', 'F#-1', 'G-1', 'G#-1', 'A-1', 'A#-1', 'B-1',
  'C0', 'C#0', 'D0', 'D#0', 'E0', 'F0', 'F#0', 'G0', 'G#0', 'A0', 'A#0', 'B0',
  'C1', 'C#1', 'D1', 'D#1', 'E1', 'F1', 'F#1', 'G1', 'G#1', 'A1', 'A#1', 'B1',
  'C2', 'C#2', 'D2', 'D#2', 'E2', 'F2', 'F#2', 'G2', 'G#2', 'A2', 'A#2', 'B2',
  'C3', 'C#3', 'D3', 'D#3', 'E3', 'F3', 'F#3', 'G3', 'G#3', 'A3', 'A#3', 'B3',
  'C4', 'C#4', 'D4', 'D#4', 'E4', 'F4', 'F#4', 'G4', 'G#4', 'A4', 'A#4', 'B4',
  'C5', 'C#5', 'D5', 'D#5', 'E5', 'F5', 'F#5', 'G5', 'G#5', 'A5', 'A#5', 'B5',
  'C6', 'C#6', 'D6', 'D#6', 'E6', 'F6', 'F#6', 'G6', 'G#6', 'A6', 'A#6', 'B6',
  'C7', 'C#7', 'D7', 'D#7', 'E7', 'F7', 'F#7', 'G7', 'G#7', 'A7', 'A#7', 'B7',
  'C8', 'C#8', 'D8', 'D#8', 'E8', 'F8', 'F#8', 'G8', 'G#8', 'A8', 'A#8', 'B8',
  'C9', 'C#9', 'D9', 'D#9', 'E9', 'F9', 'F#9', 'G9',
  'Db-1', 'Eb-1', 'Fb-1', 'Gb-1', 'Ab-1', 'Bb-1', 'Cb0', 'Db0', 'Eb0', 'Fb0', 'Gb0', 'Ab0', 'Bb0',
  'Cb1', 'Db1', 'Eb1', 'Fb1', 'Gb1', 'Ab1', 'Bb1', 'Cb2', 'Db2', 'Eb2', 'Fb2', 'Gb2', 'Ab2', 'Bb2',
  'Cb3', 'Db3', 'Eb3', 'Fb3', 'Gb3', 'Ab3', 'Bb3', 'Cb4', 'Db4', 'Eb4', 'Fb4', 'Gb4', 'Ab4', 'Bb4',
  'Cb5', 'Db5', 'Eb5', 'Fb5', 'Gb5', 'Ab5', 'Bb5', 'Cb6', 'Db6', 'Eb6', 'Fb6', 'Gb6', 'Ab6', 'Bb6',
  'Cb7', 'Db7', 'Eb7', 'Fb7', 'Gb7', 'Ab7', 'Bb7', 'Cb8', 'Db8', 'Eb8', 'Fb8', 'Gb8', 'Ab8', 'Bb8',
  'Cb9', 'Db9', 'Eb9', 'Fb9', 'Gb9',
]);

function openCodeEditorModal(instanceId) {
  const mData = getModuleData(instanceId);
  if (!mData) return;

  const modal = el('div', 'code-modal-overlay', {
    style: 'position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.85); display: flex; align-items: center; justify-content: center; z-index: 10000; font-family: ui-monospace, Menlo, Monaco, Consolas, monospace;'
  });
  modal.addEventListener('pointerdown', e => e.stopPropagation());
  modal.addEventListener('mousedown', e => e.stopPropagation());

  const box = el('div', 'code-modal-box', {
    style: 'background: #1e1d1b; border: 1px solid #3a3835; border-radius: 6px; padding: 20px; width: 800px; height: 600px; min-width: 450px; min-height: 350px; display: flex; flex-direction: column; box-shadow: 0 10px 30px rgba(0,0,0,0.8); resize: both; overflow: hidden; box-sizing: border-box;'
  });

  const header = el('div', 'code-modal-header', {
    style: 'display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; border-bottom: 1px solid #3a3835; padding-bottom: 8px;'
  });

  const title = el('h3', '', {
    textContent: `Computer Code (id: ${instanceId})`,
    style: 'margin: 0; color: #9fd08a; font-size: 16px; font-weight: normal;'
  });

  header.appendChild(title);

  const presetsWrap = el('div', '', { style: 'display: flex; gap: 8px; align-items: center;' });
  const presetsLbl = el('span', '', { textContent: 'Templates:', style: 'color: #7d7668; font-size: 11px;' });
  const presetsSelect = el('select', '', {
    style: 'background: #2b2927; border: 1px solid #3c3a38; color: #ddd; font-size: 11px; padding: 2px 6px; border-radius: 3px; cursor: pointer;'
  });

  const templates = {
    'custom': 'Custom Code...',
    'sine-lfo': 'Sine LFO (Main Knob -> Rate, Out 1 -> CV)',
    'cv-offset': 'CV Offset & Attenuverter (CV 1 + Main -> Out 1)',
    'clock-divider': 'Clock Divider (Pulse 1 -> Div 2, 4, 8 to Pulse 1, 2, LED 0)',
    'turing-machine': 'Mini Turing Machine (Pulse 1 -> Clock, Out 1 -> CV, LED 0..5)',
    'chord-organ': 'Chord Organ (Main = Chord, CV 2 = Pitch, Out 1 = Audio)',
    'bytebeat': 'Bytebeat Synth (Main = Sample Rate, Out 1 = Audio)',
    'computer-grids': 'Computer Grids (X/Y = Kick/Snare Density, Main = Hat Density)',
  };

  for (const [k, name] of Object.entries(templates)) {
    const opt = el('option', '', { value: k, textContent: name });
    presetsSelect.appendChild(opt);
  }
  presetsWrap.appendChild(presetsLbl);
  presetsWrap.appendChild(presetsSelect);
  header.appendChild(presetsWrap);
  box.appendChild(header);

  const textarea = el('textarea', 'code-modal-input', {
    style: 'background: #0d0c0b; border: 1px solid #2d2b29; color: #9fd08a; font: 13px/1.45 ui-monospace, Menlo, Monaco, Consolas, monospace; padding: 12px; border-radius: 4px; outline: none; width: 100%; box-sizing: border-box; text-shadow: 0 0 2px rgba(159,208,138,0.3); flex: 1 1 auto; min-height: 0; resize: none;'
  });
  textarea.value = mData.params.code || `; Computer Patch\n; Connect inputs and outputs, then write Loupe code!\n\n(def pitch (add (cv-in :1) (knob :main)))\n(<- (cv-out :1) (sine :note pitch))\n(<- (led :0) (gt (sine :note pitch) 2048))\n`;
  box.appendChild(textarea);

  presetsSelect.addEventListener('change', e => {
    const val = e.target.value;
    if (val === 'sine-lfo') {
      textarea.value = `; Sine LFO template\n; Main knob controls LFO rate. CV 1 output is the LFO signal.\n\n(def rate (spread (knob :main) 100)) ; Map 0..4095 knob to rate 0..100\n(def lfo-val (sine :hz rate))\n(<- (cv-out :1) lfo-val)\n(<- (led :0) (gt lfo-val 2048)) ; Blink LED 0\n`;
    } else if (val === 'cv-offset') {
      textarea.value = `; CV Offset & Attenuverter\n; Adds CV 1 input to Main Knob offset, attenuated by X Knob.\n\n(def offset (knob :main))\n(def gain (knob :x))\n(def input (cv-in :1))\n(def result (add offset (vca input gain) :sat))\n(<- (cv-out :1) result)\n`;
    } else if (val === 'clock-divider') {
      textarea.value = `; Clock Divider template\n; Divides Clock on Pulse 1 by 2, 4, 8. Outputs on Pulse 1, 2, and LED 0.\n\n(def clk (edge (pulse-in :1)))\n(def count (hold c (mod (add c 1) 8) :trig clk))\n(def div2 (bit count 0))\n(def div4 (bit count 1))\n(def div8 (bit count 2))\n(<- (pulse-out :1) (mul div2 VMAX))\n(<- (pulse-out :2) (mul div4 VMAX))\n(<- (led :0) (mul div8 VMAX))\n`;
    } else if (val === 'turing-machine') {
      textarea.value = `; Real Turing Machine template\n; Main knob = Randomness: CCW = Invert Loop, Noon = Random, CW = Lock.\n; Knob X = loop length (steps 2 to 8). Clock on Pulse 1.\n; Out 1 = CV, Pulse Out 1 & 2 = bits 0 & 1 gates.\n\n(def clk (edge (pulse-in :1)))\n(def len (add (div (knob :x) 800) 2)) ; Loop length 2..7 steps\n(def w (hold p (mod (add p 1) len) :trig clk)) ; step counter\n(def reg (tape '(1 0 1 1 0 0 1 0))) ; 8-bit shift register\n(def bit-end (lookup reg w)) ; bit at loop end\n\n(def mode (knob :main))\n(def rand-bit (chance 2048 :trig clk)) ; 50% chance bit\n(def next-bit\n  (if (lt mode 1200)\n    (not bit-end) ; invert loop\n    (if (gt mode 2800)\n      bit-end ; lock loop\n      rand-bit ; random mutation\n    )\n  )\n)\n\n(on clk (<- (seek reg w) next-bit)) ; write back bit\n\n; DAC: Convert 8 register bits to a weighted CV value\n(def val0 (mul (lookup reg 0) 2048))\n(def val1 (mul (lookup reg 1) 1024))\n(def val2 (mul (lookup reg 2) 512))\n(def val3 (mul (lookup reg 3) 256))\n(def val-dac (add val0 val1 val2 val3))\n\n(<- (cv-out :1) val-dac)\n(<- (pulse-out :1) (mul (lookup reg 0) VMAX))\n(<- (pulse-out :2) (mul (lookup reg 1) VMAX))\n(<- (led :0) (mul (lookup reg 0) VMAX))\n(<- (led :1) (mul (lookup reg 1) VMAX))\n`;
    } else if (val === 'chord-organ') {
      textarea.value = `; Chord Organ template\n; Main knob selects chord. CV 2 tracks 1V/oct pitch. Knob X transposes.\n\n(def root (add (cv-in :2) (spread (knob :x) 48))) ; Root + transpose\n(def chord-idx (div (knob :main) 256)) ; 16 chord slots\n\n; Mix 3 voices to form a triad chord (Root, Third, Fifth)\n; Third is major (add root 4) or minor (add root 3) depending on chord-idx\n(def is-minor (gt (mod chord-idx 2) 0))\n(def third-pitch (if is-minor (add root 3) (add root 4)))\n(def fifth-pitch (add root 7))\n\n(def v1 (sine :note root))\n(def v2 (sine :note third-pitch))\n(def v3 (sine :note fifth-pitch))\n\n(def mix-sig (mix v1 v2 v3))\n(<- (audio-out :1) mix-sig)\n`;
    } else if (val === 'bytebeat') {
      textarea.value = `; Bytebeat Synth template\n; Main knob controls speed/sample rate. Out 1 is the audio bytebeat.\n\n(def rate (spread (knob :main) 8000)) ; 0..8000 Hz clock rate\n(def clk (phasor :hz rate))\n(def t (counter :trig (edge clk))) ; count up on phasor ticks\n\n; Classic bytebeat formula: (t * 5 & t >> 7) | t * 3 & t >> 10\n(def val1 (and (mul t 5) (div t 128)))\n(def val2 (and (mul t 3) (div t 1024)))\n(def sig (or val1 val2))\n\n(<- (audio-out :1) (mul sig 32)) ; scale to audio amplitude\n`;
    } else if (val === 'computer-grids') {
      textarea.value = `; Computer Grids pattern sequencer template\n; Knob X = Map X, Knob Y = Map Y. Knob Main = Global Fill (all lanes).\n; Clock on Pulse 1. Outputs: PLS 1 (Kick), PLS 2 (Snare), CV 1 (Hat).\n\n(def clk (edge (pulse-in :1)))\n(def step (hold s (mod (add s 1) 16) :trig clk))\n\n; 4 Kick patterns (16 steps each)\n(def k-patterns (tape '(\n  1 0 0 0 1 0 0 0 1 0 0 0 1 0 0 0 ; 4-on-the-floor\n  1 0 0 0 0 0 1 0 0 1 0 0 0 0 1 0 ; syncopated\n  1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 0 ; breakbeat\n  1 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 ; sparse\n)))\n\n; 4 Snare patterns\n(def s-patterns (tape '(\n  0 0 0 0 1 0 0 0 0 0 0 0 1 0 0 0 ; backbeat\n  0 0 0 1 0 1 0 0 0 0 1 0 0 1 0 0 ; active\n  0 0 0 0 1 0 0 1 0 0 0 0 1 0 1 0 ; groove\n  0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 ; simple\n)))\n\n; 4 Hat patterns\n(def h-patterns (tape '(\n  0 0 1 0 0 0 1 0 0 0 1 0 0 0 1 0 ; offbeats\n  1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ; 16ths\n  1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 ; 8ths\n  0 0 0 0 1 1 0 0 1 1 0 0 1 1 0 0 ; pairs\n)))\n\n; Interpolate / Select patterns using X/Y coordinates\n(def idx-x (div (knob :x) 1024)) ; 0..3 map X\n(def idx-y (div (knob :y) 1024)) ; 0..3 map Y\n(def pat-idx (mod (add idx-x (mul idx-y 2)) 4)) ; pattern index 0..3\n\n; Look up base hit for current step\n(def k-base (lookup k-patterns (add (mul pat-idx 16) step)))\n(def s-base (lookup s-patterns (add (mul pat-idx 16) step)))\n(def h-base (lookup h-patterns (add (mul pat-idx 16) step)))\n\n; Apply Main Knob as a Global Fill threshold\n(def fill (knob :main))\n(def rnd (chance 4095 :trig clk))\n(def fill-active (lt rnd fill))\n\n; Trigger if pattern hits OR if fill is active\n(def kick-trg (and clk (or k-base fill-active)))\n(def snare-trg (and clk (or s-base (and fill-active (gt rnd 2048)))))\n(def hat-trg (and clk (or h-base (and fill-active (gt rnd 1024)))))\n\n(<- (pulse-out :1) (mul kick-trg VMAX))\n(<- (pulse-out :2) (mul snare-trg VMAX))\n(<- (cv-out :1) (mul hat-trg VMAX))\n`;
    }
  });

  const buttons = el('div', '', {
    style: 'display: flex; justify-content: flex-end; gap: 10px; margin-top: 15px;'
  });

  const cancel = el('button', '', {
    textContent: 'Cancel',
    style: 'background: #2b2927; border: 1px solid #3c3a38; color: #aaa; padding: 8px 16px; border-radius: 4px; cursor: pointer; font-size: 13px;'
  });
  cancel.addEventListener('click', () => {
    document.body.removeChild(modal);
  });

  const save = el('button', '', {
    textContent: 'Save & Compile',
    style: 'background: #4e8042; border: 1px solid #629f52; color: #fff; padding: 8px 16px; border-radius: 4px; cursor: pointer; font-weight: bold; font-size: 13px;'
  });
  save.addEventListener('click', () => {
    mData.params.code = textarea.value;
    document.body.removeChild(modal);
    generateCode();
  });

  buttons.appendChild(cancel);
  buttons.appendChild(save);
  box.appendChild(buttons);

  modal.appendChild(box);
  document.body.appendChild(modal);
}

function init() {
  const rackCase = $('rackCase');
  for (let i = 0; i < 2; i++) {
    state.rows[i] = [];
    rackCase.appendChild(buildRowEl(i));
  }

  // Load autosave if available, else initialize default empty patch
  let hasAutosave = false;
  try {
    const autosave = safeStorage.getItem('flare_autosave');
    if (autosave) {
      const parsed = JSON.parse(autosave);
      if (parsed && parsed.state && parsed.state.rows) {
        hasAutosave = true;
      }
    }
  } catch (e) {
    console.error('Failed checking autosave:', e);
  }

  if (hasAutosave) {
    loadPatch('flare_autosave');
  } else {
    // Initialize WS IN on row 0 and WS OUT on row 1 (both 6 HP, snap alignment)
    addModuleToRow('ws-in', 0, {}, { id: 'wsIn', left: 0 });
    addModuleToRow('ws-out', 1, {}, { id: 'wsOut', left: 0 });
  }

  $('toggleCodeBtn').addEventListener('click', toggleCode);
  setupCodePaneResizeAndControls();
  $('browserBtn').addEventListener('click', openBrowser);
  $('closeBrowserBtn').addEventListener('click', closeBrowser);
  $('addRowBtn').addEventListener('click', () => { addRow(); });
  $('connectMidiBtn').addEventListener('click', connectMidi);
  const flareMidiChSelect = $('flareMidiChSelect');
  if (flareMidiChSelect) {
    flareMidiChSelect.value = String(state.flareMidiChannel || 16);
    flareMidiChSelect.addEventListener('change', e => {
      state.flareMidiChannel = parseInt(e.target.value, 10) || 16;
      rerenderRackFromState();
      generateCode();
    });
  }
  $('disconnectMidiBtn').addEventListener('click', () => {
    midiOut = midiIn = null;
    lastUploadedSnapshot = null;
    $('connectMidiBtn').textContent = 'Connect MIDI';
    $('connectMidiBtn').style.display = 'inline-block';
    $('midiConnectedGroup').style.display = 'none';
    $('status').textContent = 'disconnected';
    $('status').className = 'status-pill';
    generateCode();
  });
  $('sendBtn').addEventListener('click', sendPatch);
  $('saveCardBtn').addEventListener('click', saveToFlash);

  // Local storage save/load presets
  const savePatchBtn = $('savePatchBtn');
  if (savePatchBtn) savePatchBtn.addEventListener('click', savePatch);
  const deletePatchBtn = $('deletePatchBtn');
  if (deletePatchBtn) deletePatchBtn.addEventListener('click', deletePatch);
  const patchSelect = $('patchSelect');
  if (patchSelect) patchSelect.addEventListener('change', e => loadPatch(e.target.value));

  $('clearBtn').addEventListener('click', () => {
    for (let i = 0; i < state.rows.length; i++) {
      state.rows[i] = state.rows[i].filter(m => MODULE_DEFS[m.type]?.deletable === false);
    }
    state.cables = [];
    document.querySelectorAll('.module:not(.mod-io)').forEach(m => m.remove());
    redrawCables(); updateRowWidths(); generateCode();
  });

  $('exportBtn').addEventListener('click', () => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([$('codeArea').value], { type: 'text/plain' }));
    a.download = 'patch.loupe'; a.click();
  });

  $('browserSearch').addEventListener('input', buildBrowser);

  $('rackViewport').addEventListener('scroll', redrawCables);
  window.addEventListener('resize', redrawCables);

  // Right-click Context Menus
  $('rackViewport').addEventListener('contextmenu', showContextMenu);
  window.addEventListener('click', closeContextMenu);

  // Code import interactions
  let cachedGeneratedCode = '';
  const startImport = () => {
    cachedGeneratedCode = $('codeArea').value;
    $('codeArea').readOnly = false;
    $('codeArea').value = '';
    $('codeArea').placeholder = 'Paste your Loupe S-expression script here, then click Apply...';
    $('codeArea').focus();
    $('codePaneTitle').textContent = 'Paste Loupe Script';
    $('importCodeBtn').style.display = 'none';
    $('applyImportBtn').style.display = 'inline-block';
    $('cancelImportBtn').style.display = 'inline-block';
    // Make sure code pane is visible
    const pane = $('codePane');
    const btn = $('toggleCodeBtn');
    if (pane.style.display === 'none') {
      pane.style.display = 'flex';
      btn.textContent = 'Hide Code';
      setTimeout(redrawCables, 50);
    }
  };

  $('importBtn').addEventListener('click', () => {
    $('loupeFileInput').click();
  });

  $('loupeFileInput').addEventListener('change', e => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (evt) => {
      try {
        loadLoupePatch(evt.target.result);
        const pane = $('codePane');
        const btn = $('toggleCodeBtn');
        if (pane.style.display === 'none') {
          pane.style.display = 'flex';
          btn.textContent = 'Hide Code';
          setTimeout(redrawCables, 50);
        }
      } catch (err) {
        alert('Failed to import Loupe file: ' + err.message);
      }
    };
    reader.readAsText(file);
    e.target.value = '';
  });
  $('importCodeBtn').addEventListener('click', startImport);

  $('importDx7Btn').addEventListener('click', () => {
    $('dx7FileInput').click();
  });

  $('dx7FileInput').addEventListener('change', e => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = function (evt) {
      const buf = new Uint8Array(evt.target.result);
      try {
        const voices = parseDx7Bank(buf);
        showVoiceSelectionModal(voices, (voice, voiceIdx) => {
          const name = 'dxvoice_' + voice.name.toLowerCase().replace(/[^a-z0-9]/g, '_');
          const composedCode = emitDx7VoiceFn(voice, name);
          const demoPatch = [
            composedCode,
            '(patch',
            '  (def g (clock :bpm 60 :width 2048))',
            `  (<- (audio-out :1) (${name} :gate g :pitch C3))`,
            `  (<- (audio-out :2) (${name} :gate g :pitch C3)))`
          ].join('\n');

          DX7_PRESETS[3] = voices.map(v => v.name);
          const bankKnobDef = MODULE_DEFS.dx.knobs.find(k => k.param === 'bank');
          if (bankKnobDef && !bankKnobDef.discrete.includes(3)) {
            bankKnobDef.discrete.push(3);
          }

          loadLoupePatch(demoPatch);
          const pane = $('codePane');
          const btn = $('toggleCodeBtn');
          if (pane.style.display === 'none') {
            pane.style.display = 'flex';
            btn.textContent = 'Hide Code';
            setTimeout(redrawCables, 50);
          }
        });
      } catch (err) {
        alert('Failed to parse DX7 bank: ' + err.message);
      }
    };
    reader.readAsArrayBuffer(file);
    e.target.value = '';
  });

  $('applyImportBtn').addEventListener('click', () => {
    const code = $('codeArea').value.trim();
    if (code) {
      loadLoupePatch(code);
    } else {
      $('codeArea').value = cachedGeneratedCode;
    }
    // Restore default state
    $('codeArea').readOnly = true;
    $('codePaneTitle').textContent = 'Generated Loupe Code';
    $('importCodeBtn').style.display = 'inline-block';
    $('applyImportBtn').style.display = 'none';
    $('cancelImportBtn').style.display = 'none';
  });

  $('cancelImportBtn').addEventListener('click', () => {
    $('codeArea').value = cachedGeneratedCode;
    $('codeArea').readOnly = true;
    $('codePaneTitle').textContent = 'Generated Loupe Code';
    $('importCodeBtn').style.display = 'inline-block';
    $('applyImportBtn').style.display = 'none';
    $('cancelImportBtn').style.display = 'none';
  });

  // Mouse coordinate tracking for Tab hotkey positioning
  lastMouseX = window.innerWidth / 2;
  lastMouseY = window.innerHeight / 2;
  document.addEventListener('mousemove', e => {
    lastMouseX = e.clientX;
    lastMouseY = e.clientY;
  });

  // Selected module listeners
  window.addEventListener('keydown', e => {
    if (document.activeElement &&
      (document.activeElement.tagName === 'INPUT' ||
        document.activeElement.tagName === 'TEXTAREA' ||
        document.activeElement.isContentEditable)) {
      return;
    }

    if (e.key === 'Tab') {
      e.preventDefault();

      // Determine row and snap position under mouse cursor
      const bayEl = document.elementFromPoint(lastMouseX, lastMouseY)?.closest('.module-bay');
      let rowIndex = lastActiveRow || 0;
      let hpX = 0;
      if (bayEl) {
        rowIndex = parseInt(bayEl.dataset.row);
        const rect = bayEl.getBoundingClientRect();
        const clickX = lastMouseX - rect.left + $('rackViewport').scrollLeft;
        hpX = snapToHP(clickX);
      } else {
        const bays = Array.from(document.querySelectorAll('.module-bay'));
        if (bays.length > 0) {
          let closestBay = bays[0];
          let minDist = Infinity;
          for (const b of bays) {
            const r = b.getBoundingClientRect();
            const centerY = r.top + r.height / 2;
            const dist = Math.abs(lastMouseY - centerY);
            if (dist < minDist) {
              minDist = dist;
              closestBay = b;
            }
          }
          rowIndex = parseInt(closestBay.dataset.row);
          const rect = closestBay.getBoundingClientRect();
          const clickX = Math.max(0, lastMouseX - rect.left) + $('rackViewport').scrollLeft;
          hpX = snapToHP(clickX);
        }
      }

      showModuleMenu(lastMouseX, lastMouseY, rowIndex, hpX);
      return;
    }

    // Delete selected modules: Delete / Backspace keys
    if ((e.key === 'Delete' || e.key === 'Backspace') && selectedModuleIds.size > 0) {
      for (const id of selectedModuleIds) {
        const def = MODULE_DEFS[getModuleData(id)?.type];
        if (def && def.deletable !== false) {
          deleteModule(id);
        }
      }
      clearSelection();
      pushStateSnapshot();
    }

    // Undo: Ctrl+Z / Cmd+Z
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
      e.preventDefault();
      if (e.shiftKey) {
        redoState();
      } else {
        undoState();
      }
    }

    // Redo: Ctrl+Y / Cmd+Y
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'y') {
      e.preventDefault();
      redoState();
    }

    // Copy: Ctrl+C / Cmd+C
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'c') {
      e.preventDefault();
      copyModules();
    }

    // Paste: Ctrl+V / Cmd+V
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'v') {
      e.preventDefault();
      pasteModules();
    }
  });

  document.addEventListener('pointerdown', e => {
    // Check if target is empty rack space or case
    if (e.target.closest('.code-pane') || e.target.closest('.code-modal-overlay') || e.target.closest('.voice-modal-overlay') || e.target.closest('.module') || e.target.closest('.context-menu') || e.target.closest('.code-modal-box') || e.target.closest('input') || e.target.closest('textarea') || e.target.closest('button') || e.target.closest('select')) {
      return;
    }

    // Clear current selection unless Shift key is held
    if (!e.shiftKey) {
      clearSelection();
    }

    // Start marquee box selection
    const startX = e.clientX;
    const startY = e.clientY;

    try { e.target.setPointerCapture(e.pointerId); } catch(err){}

    const boxSel = el('div', 'selection-marquee');
    boxSel.style.position = 'fixed';
    boxSel.style.border = '1px dashed #dfb86c';
    boxSel.style.background = 'rgba(223, 184, 108, 0.15)';
    boxSel.style.pointerEvents = 'none';
    boxSel.style.zIndex = '99999';
    boxSel.style.left = startX + 'px';
    boxSel.style.top = startY + 'px';
    boxSel.style.width = '0px';
    boxSel.style.height = '0px';
    document.body.appendChild(boxSel);

    function onMove(moveEvent) {
      const curX = moveEvent.clientX;
      const curY = moveEvent.clientY;

      const x = Math.min(startX, curX);
      const y = Math.min(startY, curY);
      const w = Math.abs(startX - curX);
      const h = Math.abs(startY - curY);

      boxSel.style.left = x + 'px';
      boxSel.style.top = y + 'px';
      boxSel.style.width = w + 'px';
      boxSel.style.height = h + 'px';

      const selectionRect = { left: x, top: y, right: x + w, bottom: y + h };

      document.querySelectorAll('.module').forEach(modEl => {
        const id = modEl.dataset.instanceId;
        const def = MODULE_DEFS[modEl.dataset.type];
        if (def && def.deletable === false) return; // skip HW IO

        const r = modEl.getBoundingClientRect();
        const overlap = !(r.right < selectionRect.left ||
          r.left > selectionRect.right ||
          r.bottom < selectionRect.top ||
          r.top > selectionRect.bottom);

        if (overlap) {
          selectedModuleIds.add(id);
          modEl.classList.add('selected');
        } else {
          if (!e.shiftKey) {
            selectedModuleIds.delete(id);
            modEl.classList.remove('selected');
          }
        }
      });
    }

    function onUp(upEvent) {
      document.removeEventListener('pointermove', onMove);
      document.removeEventListener('pointerup', onUp);
      try { e.target.releasePointerCapture(upEvent.pointerId); } catch(err){}
      boxSel.remove();
    }

    document.addEventListener('pointermove', onMove);
    document.addEventListener('pointerup', onUp);
  });



  updateRowWidths();
  updatePatchDropdown();
  generateCode();
}

window.addEventListener('DOMContentLoaded', init);

// ═══════════════════════════════════════════════════════════════════════
// 15. LOCAL STORAGE SAVE / LOAD

const storageFallback = {};
let hasWarnedLocalStorage = false;

const safeStorage = {
  getItem(key) {
    try {
      if (typeof localStorage !== 'undefined') {
        return localStorage.getItem(key);
      }
    } catch (e) {}
    return storageFallback[key] || null;
  },
  setItem(key, val) {
    let success = false;
    try {
      if (typeof localStorage !== 'undefined') {
        localStorage.setItem(key, val);
        success = true;
      }
    } catch (e) {}
    if (!success) {
      storageFallback[key] = val;
      if (!hasWarnedLocalStorage && key !== 'flare_autosave') {
        hasWarnedLocalStorage = true;
        alert("Warning: Local Storage is blocked or disabled by your browser (common when opening HTML files directly via file:// protocol).\n\nYour patch was saved in-memory for this session only and will be lost if you close or refresh this tab. To enable persistent saving, run a local web server (e.g., 'npm run dev' or 'python3 -m http.server').");
      }
    }
  },
  removeItem(key) {
    try {
      if (typeof localStorage !== 'undefined') {
        localStorage.removeItem(key);
        return;
      }
    } catch (e) {}
    delete storageFallback[key];
  },
  keys() {
    try {
      if (typeof localStorage !== 'undefined') {
        const list = [];
        for (let i = 0; i < localStorage.length; i++) {
          list.push(localStorage.key(i));
        }
        return list;
      }
    } catch (e) {}
    return Object.keys(storageFallback);
  }
};
// ═══════════════════════════════════════════════════════════════════════

function updatePatchDropdown() {
  const select = $('patchSelect');
  if (!select) return;
  select.innerHTML = '<option value="">-- Load Patch --</option>';

  // Add Flare-native demos: these are guaranteed to open as rack nodes.
  if (typeof FLARE_PRESETS !== 'undefined') {
    const flareGroup = el('optgroup', '', { label: 'Flare Demo Patches' });
    for (const key of Object.keys(FLARE_PRESETS)) {
      const opt = el('option');
      opt.value = 'flare_' + key;
      const title = key.split('-').map(w => w.charAt(0).toUpperCase() + w.slice(1)).join(' ');
      opt.textContent = title;
      flareGroup.appendChild(opt);
    }
    select.appendChild(flareGroup);
  }

  // Add User Presets
  const userGroup = el('optgroup', '', { label: 'My Saved Patches' });
  let hasUserPatches = false;
  const allKeys = safeStorage.keys();
  for (const key of allKeys) {
    if (key.startsWith('flare_patch_')) {
      const name = key.substring('flare_patch_'.length);
      const opt = el('option');
      opt.value = key;
      opt.textContent = name;
      userGroup.appendChild(opt);
      hasUserPatches = true;
    }
  }
  if (hasUserPatches) {
    select.appendChild(userGroup);
  }
}

function savePatch() {
  const name = prompt('Enter a name for this patch:', '');
  if (!name) return;
  const key = 'flare_patch_' + name.trim();
  const patchData = {
    state: {
      rows: state.rows,
      cables: state.cables,
      nextId: state.nextId
    }
  };
  safeStorage.setItem(key, JSON.stringify(patchData));
  updatePatchDropdown();
  const select = $('patchSelect');
  if (select) select.value = key;
}

function deletePatch() {
  const select = $('patchSelect');
  const key = select.value;
  if (!key) {
    alert('Please select a saved patch to delete.');
    return;
  }
  if (key.startsWith('factory_') || key.startsWith('flare_')) {
    alert('Factory and Flare demo patches cannot be deleted!');
    return;
  }
  if (confirm(`Are you sure you want to delete "${key.substring('flare_patch_'.length)}"?`)) {
    safeStorage.removeItem(key);
    updatePatchDropdown();
    generateCode();
  }
}

const NOTE_OFFSETS = {
  'C': 0, 'C#': 1, 'DB': 1, 'D': 2, 'D#': 3, 'EB': 3, 'E': 4, 'FB': 4, 'F': 5, 'F#': 6, 'GB': 6,
  'G': 7, 'G#': 8, 'AB': 8, 'A': 9, 'A#': 10, 'BB': 10, 'B': 11, 'CB': 11
};

function parseParamValue(valStr) {
  if (typeof valStr !== 'string') {
    if (typeof valStr === 'number') return valStr;
    return 0;
  }
  if (valStr.toUpperCase() === 'VMAX') return 4095;
  if (valStr.toUpperCase() === 'VMID') return 2048;
  if (valStr.toUpperCase() === 'VMIN') return 0;
  if (/^\d+$/.test(valStr)) {
    return parseInt(valStr);
  }
  const match = valStr.match(/^([A-G]#?|D[b-g]?|E[b-g]?|F[b-g]?|G[b-g]?|A[b-g]?|B[b-g]?|C[b-g]?)(-?\d+)$/i);
  if (match) {
    const name = match[1].toUpperCase();
    const octave = parseInt(match[2]);
    const semitones = NOTE_OFFSETS[name];
    if (semitones !== undefined) {
      const noteNum = (octave + 1) * 12 + semitones;
      return Math.round((noteNum / 127) * 4095);
    }
  }
  return 0;
}

function autoAssignMidiCcs() {
  const FLARE_CC_RESERVED = new Set([
    0, 1, 2, 4, 6, 7, 10, 11, 32, 33, 38, 64, 65, 66, 67, 68, 96, 97, 98, 99, 100, 101, 120, 121, 122, 123, 124, 125, 126, 127
  ]);

  const used = new Set(); // Stores (ch << 7) | cc
  for (const row of state.rows) {
    if (!row) continue;
    for (const m of row) {
      if (m.params && m.params.__midi_cc) {
        for (const param of Object.keys(m.params.__midi_cc)) {
          const entry = m.params.__midi_cc[param];
          if (entry && entry.cc !== undefined && entry.ch !== undefined) {
            used.add((entry.ch << 7) | entry.cc);
          }
        }
      }
    }
  }

  const startCh = state.flareMidiChannel || 16;
  let currentCh = startCh;
  let currentCc = 14;

  function getNextFreeCc() {
    while (currentCh >= 1) {
      while (currentCc <= 119) {
        const cc = currentCc;
        currentCc++;
        if (cc >= 14 && !FLARE_CC_RESERVED.has(cc) && !used.has((currentCh << 7) | cc)) {
          return { cc, ch: currentCh };
        }
      }
      currentCh--;
      currentCc = 14;
    }
    return null;
  }

  for (const row of state.rows) {
    if (!row) continue;
    for (const m of row) {
      const def = MODULE_DEFS[m.type];
      if (!def || !def.knobs) continue;

      m.params = m.params || {};
      m.params.__midi_cc = m.params.__midi_cc || {};
      for (const k of def.knobs) {
        if (k.discrete || k.noMidi) continue;
        if (m.params.__midi_cc[k.param] === undefined) {
          const free = getNextFreeCc();
          if (free) {
            m.params.__midi_cc[k.param] = free;
            used.add((free.ch << 7) | free.cc);
          }
        }
      }
    }
  }
}

function rerenderRackFromState() {
  autoAssignMidiCcs();
  const rackCase = $('rackCase');
  const existingRows = rackCase.querySelectorAll('.rack-row');
  existingRows.forEach(r => r.remove());
  for (let i = 0; i < state.rows.length; i++) {
    const rowEl = buildRowEl(i);
    rackCase.appendChild(rowEl);
    const bay = rowEl.querySelector('.module-bay');
    for (const m of state.rows[i]) {
      const modEl = buildModuleEl(m.type, m.id, m.params, m.left);
      if (modEl) bay.appendChild(modEl);
    }
  }
  setTimeout(redrawCables, 50);
  updateRowWidths();
}

// Patches without flare_layout metadata: compile the Loupe source directly.
function loadLoupeAsCode(loupeCode) {
  const code = loupeCode.replace(/;\s*flare_layout:\s*[\s\S]*$/, '').trim();
  state.rows = [
    [{ id: 'wsIn', type: 'ws-in', left: 0, params: {} }],
    [{ id: 'wsOut', type: 'ws-out', left: 0, params: {} }],
  ];
  state.cables = [];
  state.nextId = 1;
  rerenderRackFromState();
  lastGeneratedCode = code;
  if ($('codeArea')) $('codeArea').value = code;
  compileAndStatus(code);
  pushStateSnapshot();
}

function loadLoupePatch(loupeCode) {
  try {
    const layoutMatch = loupeCode.match(/;\s*flare_layout:\s*(\{.*\})/);
    if (layoutMatch) {
      try {
        const layoutData = JSON.parse(layoutMatch[1]);
        if (layoutData && layoutData.rows && layoutData.cables) {
          normalizeFlareLayout(layoutData);
          state.rows = layoutData.rows;
          state.cables = layoutData.cables;
          state.nextId = layoutData.nextId || 100;
          state.flareMidiChannel = layoutData.flareMidiChannel || 16;
          const selectEl = $('flareMidiChSelect');
          if (selectEl) selectEl.value = String(state.flareMidiChannel);
          rerenderRackFromState();
          generateCode();
          pushStateSnapshot();
          return;
        }
      } catch (err) {
        console.warn('Failed to parse flare_layout, loading Loupe source directly:', err);
      }
    }
    loadLoupeAsCode(loupeCode);
  } catch (err) {
    alert('Error loading Loupe patch: ' + err.message);
  }
}

function loadPatch(key) {
  if (!key) return;
  // 'flare_<name>' keys refer to built-in presets — but NOT 'flare_autosave',
  // which is a localStorage autosave written by generateCode().
  if (key.startsWith('flare_') && key !== 'flare_autosave') {
    const patchName = key.substring('flare_'.length);
    const loupeCode = FLARE_PRESETS[patchName];
    if (loupeCode) {
      loadLoupePatch(loupeCode);
    }
    return;
  }
  const dataStr = safeStorage.getItem(key);
  if (!dataStr) return;
  let data;
  try {
    data = JSON.parse(dataStr);
  } catch (e) {
    alert('Error loading patch: ' + e.message);
    return;
  }

  try {
    if (!data || !data.state) throw new Error('Invalid patch format');

    if (data.state?.rows && data.state?.cables) {
      normalizeFlareLayout(data.state);
    }

    state.rows = data.state.rows;
    state.cables = data.state.cables;
    state.nextId = data.state.nextId;
    state.flareMidiChannel = data.state.flareMidiChannel || 16;
    const selectEl = $('flareMidiChSelect');
    if (selectEl) selectEl.value = String(state.flareMidiChannel);

    rerenderRackFromState();
    generateCode();
  } catch (e) {
    alert('Error loading patch: ' + e.message);
  }
}
