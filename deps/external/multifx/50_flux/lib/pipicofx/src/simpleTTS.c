#include "simpleTTS.h"
#include "pipicofx/picofxCore.h"
#include <math.h> 

// --- Constants ---
#define SAMPLING_RATE 48000
#define F_SCALE 14 // Q14

// --- Phoneme Table ---
// F1, F2, F3 (Hz). Amps ignored in Cascade (Unity gain). Dur (ticks)
const PhonemeDef PHONEMES[] = {
    {PH_SILENCE, 0,    0,    0,    0,   0,   20},
    
    // Vowels - Freqs optimized for male voice (Cascade friendly)
    {PH_IY,      270,  2290, 3010, 0,  0,   12}, // fEEt
    {PH_IH,      390,  1990, 2550, 0,  0,   10}, // pIn
    {PH_EH,      530,  1840, 2480, 0,  0,   12}, // bEd
    {PH_AE,      660,  1720, 2410, 0,  0,   14}, // cAt
    {PH_AA,      730,  1090, 2440, 0,  0,   14}, // cOt
    {PH_AH,      640,  1190, 2390, 0,  0,   12}, // bUt
    {PH_AO,      570,  840,  2410, 0,  0,   14}, // dOg
    {PH_UH,      440,  1020, 2240, 0,  0,   10}, // bOOk
    {PH_UW,      300,  870,  2240, 0,  0,   12}, // bOOt
    {PH_ER,      490,  1350, 1690, 0,  0,   14}, // bIrd
    {PH_AX,      500,  1500, 2500, 0,  0,   8},  // Schwa
    
    // Diphthongs
    {PH_EY,      450,  2000, 2800, 0,  0,   14}, 
    {PH_AY,      700,  1500, 2600, 0,  0,   16},
    {PH_OY,      400,  900,  2300, 0,  0,   16},
    {PH_OW,      500,  850,  2300, 0,  0,   14},
    {PH_AW,      650,  1200, 2400, 0,  0,   16},
    
    // Liquids
    {PH_L,       350,  1050, 2800, 0,  0,   12},
    {PH_R,       350,  1000, 1700, 0,  0,   12},
    {PH_W,       300,  700,  2400, 0,  0,   10},
    {PH_Y,       300,  2200, 3000, 0,  0,   10},
    
    // Nasals
    {PH_M,       250,  1000, 1500, 0,  0,   12}, 
    {PH_N,       250,  1500, 2000, 0,  0,   12},
    {PH_NX,      250,  2000, 2500, 0,  0,   12},
    
    // Fricatives (Uses F3/Highpass mainly)
    {PH_S,       0,    4000, 5000, 0,   80,  10}, 
    {PH_SH,      0,    2500, 4000, 0,   80,  10},
    {PH_F,       0,    1500, 3000, 0,   60,  10},
    {PH_TH,      0,    1800, 3500, 0,   60,  10},
    {PH_H,       0,    1000, 2000, 0,   60,  8}, 
    
    // Bursts
    {PH_Z,       250,  4000, 5000, 0,  50,  10},
    {PH_ZH,      250,  2500, 4000, 0,  50,  10},
    {PH_V,       250,  1000, 2500, 0,  40,  8},
    
    {PH_P,       0,    1000, 2000, 0,   60,  4},
    {PH_B,       200,  800,  1500, 0,  20,  4},
    {PH_T,       0,    2500, 4000, 0,   60,  4},
    {PH_D,       200,  2000, 3500, 0,  20,  4},
    {PH_K,       0,    2000, 3000, 0,   60,  4},
    {PH_G,       200,  1500, 2500, 0,  20,  4}
};

typedef struct {
    int32_t band;
    int32_t low;
} SVFState;

// Series Resonator (Unity Gain peak)
static inline int32_t runSVF(SVFState* s, int32_t in, int32_t f, int32_t q) {
    // q = 1/Q value approx.
    // To maintain unity gain in cascade, we want Bandpass peak to be 1.0.
    // Standard SVF bandpass peak gain IS Q.
    // So output = band * q?
    // Let's stick to standard and attenuate externally or use q in feedback.
    
    // Chamberlin SVF
    int32_t low = s->low;
    int32_t band = s->band;
    
    low = low + ((f * band) >> F_SCALE);
    int32_t high = in - low - ((q * band) >> F_SCALE);
    band = band + ((f * high) >> F_SCALE);
    
    s->low = low;
    s->band = band;
    
    // In cascade, we pass the bandpass output.
    // We scale it down by Q to prevent gain explosion.
    // Actually, simple way: Output = Band * q (since q=1/Q).
    // This normalizes peak to input level.
    int32_t out = (band * q) >> F_SCALE;
    return out;
}

static inline int32_t getFCoeff(uint16_t freq) {
    if (freq > 8000) freq = 8000;
    return ((int32_t)freq * 35137) >> 14;
}

void ttsInit(TTSEngineType* tts) {
    tts->phase = 0;
    tts->noisePhase = 12345;
    tts->talking = 0;
    tts->textPtr = 0;
    tts->pitch = 100;
    tts->speed = 60;
    
    SVFState* s1 = (SVFState*)&tts->r1_y1;
    SVFState* s2 = (SVFState*)&tts->r2_y1;
    SVFState* s3 = (SVFState*)&tts->r3_y1;
    SVFState* sn = (SVFState*)&tts->rn_y1;
    s1->low=0; s1->band=0;
    s2->low=0; s2->band=0;
    s3->low=0; s3->band=0;
    sn->low=0; sn->band=0;
}

void ttsSetPitch(TTSEngineType* tts, uint16_t pitch) {
    if (pitch < 40) pitch = 40; if (pitch > 600) pitch = 600;
    tts->pitch = pitch;
}
void ttsSetSpeed(TTSEngineType* tts, uint8_t speed) {
    if (speed < 10) speed = 10; tts->speed = speed;
}
void ttsSay(TTSEngineType* tts, const uint8_t* phonemes) {
    tts->textPtr = phonemes;
    tts->talking = 1;
    tts->samplesRemainingInPhoneme = 0;
}
uint8_t ttsIsTalking(TTSEngineType* tts) { return tts->talking; }

static void updateParameter(uint16_t* current, uint16_t target, uint16_t rate) {
    if (*current < target) { *current += rate; if(*current > target) *current = target; }
    else if (*current > target) { if(*current > rate) *current -= rate; else *current=0; if(*current < target) *current = target; }
}
static void updateByteParam(uint8_t* current, uint8_t target, uint8_t rate) {
    if (*current < target) { *current += rate; if(*current > target) *current = target; }
    else if (*current > target) { if(*current > rate) *current -= rate; else *current=0; if(*current < target) *current = target; }
}

int16_t ttsProcessSample(TTSEngineType* tts) {
    if (!tts->talking) return 0;
    
    // --- Sequencer ---
    if (tts->samplesRemainingInPhoneme == 0) {
        if (!tts->textPtr || *tts->textPtr == 255) { tts->talking = 0; return 0; }
        
        uint8_t code = *tts->textPtr;
        if (code >= PHONEME_COUNT) code = PH_SILENCE;
        
        const PhonemeDef* ph = &PHONEMES[code];
        tts->targetF1 = ph->f1;
        tts->targetF2 = ph->f2;
        tts->targetF3 = ph->f3;
        tts->targetAmpV = 255; // Always full on for Voiced
        tts->targetAmpN = ph->ampN; // Noise amount
        
        // Use AmpV as Voiced/Unvoiced switch effectively? No, use explicit
        // If F1=0, it's unvoiced
        if (ph->f1 == 0) tts->targetAmpV = 0;
        
        uint32_t d = (uint32_t)ph->duration * 24000;
        d /= tts->speed;
        tts->samplesRemainingInPhoneme = d;
        tts->textPtr++;
    }
    tts->samplesRemainingInPhoneme--;
    
    // Smoothing
    uint16_t slew = 2; // Smooth glides
    updateParameter(&tts->currentF1, tts->targetF1, slew);
    updateParameter(&tts->currentF2, tts->targetF2, slew);
    updateParameter(&tts->currentF3, tts->targetF3, slew);
    updateByteParam(&tts->currentAmpV, tts->targetAmpV, 5);
    updateByteParam(&tts->currentAmpN, tts->targetAmpN, 5);
    
    // --- Excitation ---
    
    // 1. Voiced Impulse
    uint32_t inc = (uint32_t)tts->pitch * 89479;
    tts->phase += inc;
    
    int32_t exc = 0;
    if (tts->phase < inc) exc = 30000; // Strong impulse, Q15
    
    // 2. Unvoiced Noise
    tts->noisePhase = tts->noisePhase * 1664525 + 1013904223;
    int32_t noise = (tts->noisePhase >> 17); // +-16k
    if (tts->currentAmpV == 0) {
        // Unvoiced case
        exc = noise; // Replace pulse with noise
    } else {
        // Voiced + Aspiration?
        // Add a little noise jitter
        exc += (noise >> 2);
    }
    
    // --- Cascade Filtering ---
    int32_t out = exc;
    
    // If Unvoiced, bypass F1/F2, go straight to Highpass or F3
    if (tts->currentAmpV > 0) {
        // F1 -> F2 -> F3
        // Bandwidth: Wide. Q=0.5 -> q=2.0 (32768). Q=2 -> q=0.5 (8192).
        // Vocal tract has Q ~ 5-10?
        // Let's use q = 2000 (Q=8)
        int32_t q = 2000; 
        
        int32_t f1 = getFCoeff(tts->currentF1);
        int32_t f2 = getFCoeff(tts->currentF2);
        int32_t f3 = getFCoeff(tts->currentF3);
        
        out = runSVF((SVFState*)&tts->r1_y1, out, f1, q);
        out = runSVF((SVFState*)&tts->r2_y1, out, f2, q);
        out = runSVF((SVFState*)&tts->r3_y1, out, f3, q);
    } else {
        // Fricative Noise Filtering (Highpass essentially)
        // Using F3 as noise formant
        int32_t fN = getFCoeff(tts->currentF2 > 0 ? tts->currentF2 : 4000);
        int32_t qN = 4000; // Wider
        out = runSVF((SVFState*)&tts->rn_y1, out, fN, qN);
    }
    
    // Clip
    if (out > 32767) out = 32767;
    if (out < -32768) out = -32768;
    
    return (int16_t)out;
}
