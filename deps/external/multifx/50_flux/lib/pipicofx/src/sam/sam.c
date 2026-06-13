#include "sam/sam.h"
#include <string.h>

// --- Math Helpers ---
static const int8_t SIN_Q[65] = {
    0,3,6,9,12,15,18,21,24,27,30,33,36,39,42,45,48,51,54,57,59,62,65,67,70,73,75,78,80,82,85,87,89,91,93,95,97,99,100,102,104,105,107,108,109,111,112,113,114,115,116,117,118,119,119,120,121,121,122,122,123,123,123,123,124
};

static int16_t fastSin(uint32_t phase) {
    uint16_t p = phase >> 16;
    uint8_t q = p >> 14; 
    uint8_t idx = (p >> 8) & 0x3F; 
    int8_t s = SIN_Q[idx];
    if (idx == 64) s = 124; 
    if (q==0) return (int16_t)s << 8;
    if (q==1) return (int16_t)SIN_Q[64-idx] << 8;
    if (q==2) return -((int16_t)s << 8);
    return -((int16_t)SIN_Q[64-idx] << 8);
}

// --- Phoneme Data ---
// Formant Frequencies (Hz) and Amplitudes. 
// Bandwidths ~50Hz (Decay=200).
typedef struct {
    uint16_t f[3];
    uint8_t amp[3]; 
    uint8_t len;
    uint8_t type; // 0=Voiced, 1=Unvoiced (Noise)
} Phoneme;

// Phoneme Map (0-31)
const Phoneme PHONEME_DATA[] = {
    // 0: SILENCE
    {{0,0,0}, {0,0,0}, 10, 0},
    
    // VOWELS (Voiced)
    // 1: IY (bEEt)
    {{270, 2290, 3010}, {255, 100, 50}, 15, 0},
    // 2: IH (bIt)
    {{390, 1990, 2550}, {255, 120, 60}, 15, 0},
    // 3: EH (bEd)
    {{530, 1840, 2480}, {255, 140, 70}, 15, 0},
    // 4: AE (bAt)
    {{660, 1720, 2410}, {255, 150, 80}, 20, 0},
    // 5: AA (bOt)
    {{730, 1090, 2440}, {255, 140, 60}, 20, 0},
    // 6: AH (bUt)
    {{640, 1200, 2400}, {255, 130, 60}, 15, 0},
    // 7: AO (bOught)
    {{570, 840,  2410}, {255, 120, 60}, 20, 0},
    // 8: UH (bOOk)
    {{440, 1020, 2240}, {255, 100, 50}, 15, 0},
    // 9: UW (bOOt)
    {{300, 870,  2240}, {255, 100, 50}, 15, 0},
    // 10: ER (bIrd)
    {{490, 1350, 1690}, {255, 110, 60}, 20, 0},
    // 11: AY (bIte) - Diphthong Start (AH) -> End (IY) handled by macros usually? 
    // Let's just define AY as a distinct target.
    {{660, 1700, 2600}, {255, 150, 80}, 20, 0},
    // 12: OW (bOat)
    {{500, 850, 2300}, {255, 120, 60}, 20, 0},
    
    // CONSONANTS (Voiced - Liquids/Nasals)
    // 13: L
    {{350, 1050, 2800}, {200, 100, 50}, 15, 0},
    // 14: R
    {{350, 1000, 1700}, {200, 100, 50}, 15, 0},
    // 15: M
    {{250, 900, 1600}, {180, 50, 20}, 15, 0}, // Muted F2/F3
    // 16: N
    {{250, 1500, 2000}, {180, 80, 40}, 15, 0},

    // CONSONANTS (Unvoiced - Noise)
    // Modeled as High Freq Sines with Jitter? Or separate noise source?
    // Using jittered FOF for now (Chaos).
    // 17: S
    {{4000, 5000, 6000}, {100, 100, 100}, 15, 1},
    // 18: SH
    {{2500, 3500, 4500}, {120, 120, 100}, 15, 1},
    // 19: F
    {{1500, 2500, 3500}, {100, 80, 60}, 15, 1},
    // 20: T
    {{3000, 4000, 5000}, {150, 150, 100}, 5, 1}, // Short burst
    // 21: K
    {{2000, 3000, 4000}, {150, 150, 100}, 5, 1},
    // 22: P
    {{800, 1500, 2500}, {150, 80, 50}, 5, 1},

    // 23: Z (Voiced Fricative)
    {{250, 4000, 5000}, {150, 80, 80}, 15, 0} 
};

// --- FOF State ---
typedef struct {
    uint32_t phase;     
    uint32_t phaseInc;  
    int32_t amp;        
    int32_t decay;      
} FormantGrain;

static FormantGrain formants[3];
static uint32_t pitchPhase = 0;
static uint32_t pitchInc = 0;
static uint32_t noiseRng = 12345;

static uint8_t is_talking = 0;
static const uint8_t* pText = 0; // Pointer to byte string
static uint8_t curPhoneme = 0;
static uint32_t phoneTimer = 0;
static int p_pitch = 64;
static int p_speed = 72;

void SAM_Init() {
    is_talking = 0;
    SAM_SetPitch(64);
}

void SAM_SetSpeed(uint8_t speed) { 
    if (speed < 10) speed = 10;
    p_speed = speed; 
}

void SAM_SetPitch(uint8_t pitch) { 
    p_pitch = pitch; 
    uint32_t freq = 50 + pitch;
    pitchInc = freq * 89478; // 48000Hz
}

void SAM_SetThroat(uint8_t t) {}
void SAM_SetMouth(uint8_t m) {}

void SAM_SetInput(const char* input) {
    pText = (const uint8_t*)input; // Treat char* as byte array of phoneme indices
    curPhoneme = 255; // Trigger load next
    phoneTimer = 0;
    is_talking = 1;
}

int SAM_Prepare() { return 1; }
uint8_t SAM_IsTalking() { return is_talking; }

int16_t SAM_ProcessSample() {
    if (!is_talking) return 0;
    
    // --- Phoneme Sequencing ---
    if (curPhoneme == 255 || phoneTimer == 0) {
        // Load next
        if (!pText || *pText == 255 || *pText == 0) {
            is_talking = 0;
            return 0;
        }
        
        uint8_t code = *pText;
        if (code >= 24) code = 0; // Safety clamp
        curPhoneme = code;
        pText++;
        
        // Duration: length * 50ms approx?
        // length is "ticks". Speed scales it.
        // base = 48000/60 = 800 samps per tick.
         phoneTimer = (uint32_t)PHONEME_DATA[code].len * 1500;
         phoneTimer = (phoneTimer * 60) / p_speed;
    }
    phoneTimer--;

    const Phoneme* ph = &PHONEME_DATA[curPhoneme];

    // --- Pitch Trigger ---
    // If Unvoiced, trigger randomly (Noise)
    // If Voiced, trigger on Pitch Phase
    
    uint8_t trigger = 0;
    if (ph->type == 1) {
        // Unvoiced / Noise
        noiseRng = noiseRng * 1664525 + 1013904223;
        if ((noiseRng & 0xFF) < 40) trigger = 1; // High density grains
    } else {
        // Voiced
        pitchPhase += pitchInc;
        if (pitchPhase < pitchInc) trigger = 1;
    }
    
    if (trigger) {
        for(int k=0; k<3; k++) {
            formants[k].phase = 0;
            // Jitter freq for unvoiced?
            uint32_t f = ph->f[k];
            if (ph->type == 1) f += (noiseRng & 0x1FF); // Spread
            formants[k].phaseInc = f * 89478;
            formants[k].amp = (int32_t)ph->amp[k] << 8; 
            formants[k].decay = 200 + (k*50); 
            if (ph->type == 1) formants[k].decay *= 4; // Faster decay for noise
        }
    }
    
    // --- Synthesis ---
    int32_t mix = 0;
    
    for(int k=0; k<3; k++) {
        if (formants[k].amp > 0) {
            int16_t s = fastSin(formants[k].phase);
            mix += (s * formants[k].amp) >> 16;
            formants[k].phase += formants[k].phaseInc;
            formants[k].amp -= formants[k].decay;
        }
    }
    
    // Attenuate Output
    // was just returning mix.
    // Summing 3 formants can reach ~3.0 * 32768.
    // Shift down by 2 or 3.
    // User said "too loud".
    return (int16_t)(mix >> 2);
}
