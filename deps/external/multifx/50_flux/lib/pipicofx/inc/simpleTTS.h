#ifndef SIMPLE_TTS_H
#define SIMPLE_TTS_H

#include <stdint.h>
#include "pipicofx/picofxCore.h" // For context if needed, mostly for int types

// TTS Constants
#define TTS_SAMPLE_RATE 48000
#define TTS_BUFFER_SIZE 64 

// Phoneme Definitions
typedef enum {
    PH_SILENCE = 0,
    
    // Vowels
    PH_IY, // fEEt
    PH_IH, // pIn
    PH_EH, // bEd
    PH_AE, // cAt
    PH_AA, // cOt
    PH_AH, // bUt
    PH_AO, // dOg
    PH_UH, // bOOk
    PH_UW, // bOOt
    PH_ER, // bIrd
    PH_AX, // About (Schwa)
    
    // Diphthongs (Simplified, implemented as glides or static approximations)
    PH_EY, // dAte
    PH_AY, // lIe
    PH_OY, // bOy
    PH_OW, // gO
    PH_AW, // cOw
    
    // Consonants - Liquids/Glides
    PH_L, 
    PH_R,
    PH_W,
    PH_Y,
    
    // Consonants - Nasals
    PH_M,
    PH_N,
    PH_NX, // siNG
    
    // Consonants - Fricatives (Unvoiced)
    PH_S,
    PH_SH,
    PH_F,
    PH_TH,
    PH_H, 
    
    // Consonants - Fricatives (Voiced)
    PH_Z,
    PH_ZH,
    PH_V,
    PH_DH, // THen
    
    // Consonants - Plosives (Stop bursts modeled as short noise/silence)
    PH_P,
    PH_B,
    PH_T,
    PH_D,
    PH_K,
    PH_G,
    
    PHONEME_COUNT
} PhonemeCode;

// Structure for a Phoneme definition (Formants, Amplitudes, Durations)
typedef struct {
    uint8_t code;
    uint16_t f1;
    uint16_t f2;
    uint16_t f3;
    uint8_t ampV; // Voiced Amplitude
    uint8_t ampN; // Noise Amplitude
    uint8_t duration; // Default duration in generic "ticks"
} PhonemeDef;

// TTS Engine State
typedef struct {
    // Current Synthesis State
    uint32_t phase;
    uint32_t noisePhase;
    
    // Filter States (Resonators)
    int16_t r1_y1, r1_y2;
    int16_t r2_y1, r2_y2;
    int16_t r3_y1, r3_y2;
    int16_t rn_y1, rn_y2; // Noise filter (typically highpass or specialized frictative formant)
    
    // Current Frame Targets
    uint16_t targetF1, targetF2, targetF3;
    uint16_t currentF1, currentF2, currentF3;
    uint8_t targetAmpV, targetAmpN;
    uint8_t currentAmpV, currentAmpN;
    
    // Sequencing
    const uint8_t* textPtr; // Current pointer in phoneme string
    uint32_t samplesRemainingInPhoneme;
    uint8_t talking;
    
    // Config
    uint16_t pitch; // Fundamental frequency (Hz * scaled)
    uint8_t speed;
    uint8_t pitchBend; // For intonation
    
} TTSEngineType;

// Public API
#ifdef __cplusplus
extern "C" {
#endif

void ttsInit(TTSEngineType* tts);
void ttsSay(TTSEngineType* tts, const uint8_t* phonemes);
int16_t ttsProcessSample(TTSEngineType* tts);
uint8_t ttsIsTalking(TTSEngineType* tts);
void ttsSetPitch(TTSEngineType* tts, uint16_t pitch);
void ttsSetSpeed(TTSEngineType* tts, uint8_t speed);

#ifdef __cplusplus
}
#endif

// Helpers to get phoneme from string if we use simple byte encodings
// For now, we'll assume the input string is a byte array of PhonemeCodes
// terminated by PH_SILENCE or 0xFF.

#endif // SIMPLE_TTS_H
