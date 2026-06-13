#include "pipicofx/fxPrograms.h"
#include "pipicofx/picofxCore.h"
#include "audio/audiotools.h"
#include "audio/delay.h" // For getDelayMemoryPointer

#define CWO_DELAY_LEN 8192

// Pre-computed Sine Lookup Table (256 entries, 0 to 2π)
static const int16_t sineLUT[256] = {
    0,804,1607,2410,3211,4011,4807,5601,6392,7179,7961,8739,9511,10278,11038,11792,
    12539,13278,14009,14732,15446,16150,16845,17530,18204,18867,19519,20159,20787,21402,22004,22594,
    23169,23731,24278,24811,25329,25831,26318,26789,27244,27683,28105,28510,28897,29268,29621,29955,
    30272,30571,30851,31113,31356,31580,31785,31970,32137,32284,32412,32520,32609,32678,32727,32757,
    32767,32757,32727,32678,32609,32520,32412,32284,32137,31970,31785,31580,31356,31113,30851,30571,
    30272,29955,29621,29268,28897,28510,28105,27683,27244,26789,26318,25831,25329,24811,24278,23731,
    23169,22594,22004,21402,20787,20159,19519,18867,18204,17530,16845,16150,15446,14732,14009,13278,
    12539,11792,11038,10278,9511,8739,7961,7179,6392,5601,4807,4011,3211,2410,1607,804,
    0,-804,-1607,-2410,-3211,-4011,-4807,-5601,-6392,-7179,-7961,-8739,-9511,-10278,-11038,-11792,
    -12539,-13278,-14009,-14732,-15446,-16150,-16845,-17530,-18204,-18867,-19519,-20159,-20787,-21402,-22004,-22594,
    -23169,-23731,-24278,-24811,-25329,-25831,-26318,-26789,-27244,-27683,-28105,-28510,-28897,-29268,-29621,-29955,
    -30272,-30571,-30851,-31113,-31356,-31580,-31785,-31970,-32137,-32284,-32412,-32520,-32609,-32678,-32727,-32757,
    -32767,-32757,-32727,-32678,-32609,-32520,-32412,-32284,-32137,-31970,-31785,-31580,-31356,-31113,-30851,-30571,
    -30272,-29955,-29621,-29268,-28897,-28510,-28105,-27683,-27244,-26789,-26318,-25831,-25329,-24811,-24278,-23731,
    -23169,-22594,-22004,-21402,-20787,-20159,-19519,-18867,-18204,-17530,-16845,-16150,-15446,-14732,-14009,-13278,
    -12539,-11792,-11038,-10278,-9511,-8739,-7961,-7179,-6392,-5601,-4807,-4011,-3211,-2410,-1607,-804
};

// Allpass Filter State for Phase Splitter
typedef struct {
    int16_t oldIn[4];
    int16_t oldOut[4];
} PhaseSplitterChain;

typedef struct {
    int16_t* delayBufferL; // POINTER to Shared Memory
    int16_t* delayBufferR; // POINTER to Shared Memory
    uint16_t writePtr;
    
    // Phase Splitters
    PhaseSplitterChain splitL_A, splitL_B;
    PhaseSplitterChain splitR_A, splitR_B;
    
    // Oscillator
    uint16_t oscPhase;
    int16_t freqShift; 
    
    // Parameters
    int16_t mix;
    int16_t time;  
    int16_t feedback; 
    
} FxProgram22DataType;

static FxProgram22DataType data;

// Coefficients for 90-degree Phase Difference Network (Olli Niemitalo)
// Chain A: 0.161758, 0.733029, 0.945350, 0.990598
static const int16_t coeffsA[4] = { 5300, 24020, 30977, 32460 };

// Chain B: 0.479401, 0.876223, 0.976599, 0.997500
static const int16_t coeffsB[4] = { 15709, 28712, 32001, 32686 };


// 1st Order Allpass: y[n] = c*x[n] + x[n-1] - c*y[n-1]
static inline int16_t doAllpass(int16_t input, int16_t c, int16_t* oldIn, int16_t* oldOut) {
    int32_t term1 = ((int32_t)c * input) >> 15;
    int32_t term2 = *oldIn;
    int32_t term3 = ((int32_t)c * *oldOut) >> 15;
    
    int16_t output = (int16_t)(term1 + term2 - term3);
    
    *oldIn = input;
    *oldOut = output;
    return output;
}

static inline void processPhaseSplitter(int16_t input, PhaseSplitterChain* chainA, PhaseSplitterChain* chainB, int16_t* outI, int16_t* outQ) {
    int16_t sA = input;
    int16_t sB = input;
    
    // Cascade 4 filters for Chain A
    for(int i=0; i<4; i++) {
        sA = doAllpass(sA, coeffsA[i], &chainA->oldIn[i], &chainA->oldOut[i]);
    }
    
    // Cascade 4 filters for Chain B
    for(int i=0; i<4; i++) {
        sB = doAllpass(sB, coeffsB[i], &chainB->oldIn[i], &chainB->oldOut[i]);
    }
    
    *outI = sA;
    *outQ = sB;
}

static int16_t frequencyShift(int16_t in, PhaseSplitterChain* cA, PhaseSplitterChain* cB, int16_t sinVal, int16_t cosVal) {
    int16_t I, Q;
    processPhaseSplitter(in, cA, cB, &I, &Q);
    
    // SSB Modulation (Lower Sideband or Upper depending on sign)
    // Up: I*Cos - Q*Sin
    // Down: I*Cos + Q*Sin
    // We can swap sign of Sin based on freq shift direction? 
    // Actually, user knob controls Frequency. Positive = Up, Negative = Down.
    // Here we assume Phase increments positively.
    
    int32_t term1 = ((int32_t)I * cosVal) >> 15;
    int32_t term2 = ((int32_t)Q * sinVal) >> 15;
    
    // Output
    return (int16_t)(term1 - term2);
}


void fxProgram22Setup(void* d) {
    // Shared Memory Allocation
    int16_t* sharedMem = getDelayMemoryPointer();
    data.delayBufferL = sharedMem;
    data.delayBufferR = sharedMem + CWO_DELAY_LEN;

    // CRITICAL: Clear delay buffers to prevent system freeze from garbage data
    // Total: 2 * 8192 = 16384 samples
    for (int i = 0; i < CWO_DELAY_LEN; i++) {
        data.delayBufferL[i] = 0;
        data.delayBufferR[i] = 0;
    }
    
    // Initialize pointers and state
    data.writePtr = 0;
    
    // Clear Filter States
    for(int i=0; i<4; i++) {
        data.splitL_A.oldIn[i] = 0; data.splitL_A.oldOut[i] = 0;
        data.splitL_B.oldIn[i] = 0; data.splitL_B.oldOut[i] = 0;
        data.splitR_A.oldIn[i] = 0; data.splitR_A.oldOut[i] = 0;
        data.splitR_B.oldIn[i] = 0; data.splitR_B.oldOut[i] = 0;
    }
    data.oscPhase = 0;
    data.freqShift = 0;
    data.time = 4096;
    data.feedback = 0;
    data.mix = 0;
}

void fxProgram22Reset(void* d) {
    fxProgram22Setup(d);
}

void fxProgram22Param1Callback(uint16_t val, void* d) {
    // Mix
    data.mix = val; 
}

void fxProgram22Param2Callback(uint16_t val, void* d) {
    // Frequency Shift
    // Map 0-32767 to -500Hz to +500Hz?
    // Center at 2048 (from 0-4095 range check or 0-32767?)
    // In fxPrograms callbacks usually get raw 0-4095 unless scaled?
    // Let's assume 0-4095 U12 Range.
    
    // Center = 2048.
    // Shift Amount per sample (Phase Increment).
    // Fs = 48000. 65536 = 1 cycle.
    // Inc = F * 65536 / 48000 = F * 1.365.
    // Max F = +/- 700Hz. Max Inc = 1000.
    
    // Map (val - 2048) to +/- 1000.
    // 2048 * X = 1000 => X = 0.5.
    
    data.freqShift = ((int32_t)val - 2048) / 2; 
    
    // Deadzone in middle
    if (data.freqShift > -10 && data.freqShift < 10) data.freqShift = 0;
}

void fxProgram22Param3Callback(uint16_t val, void* d) {
    // Delay Time + Feedback Macro
    // "Repeats"
    // Low Val: Short Time, Low Feedback.
    // High Val: Long Time, High Feedback.
    
    // Map time 0 to BufferLen-1.
    // But keep minimum time (e.g. 50ms = 2400 samples).
    
    int32_t time = 200 + ((int32_t)val * (CWO_DELAY_LEN - 300) / 4095);
    data.time = (int16_t)time;
    
    // Feedback: 0 to 85% (Safe)
    int32_t fb = ((int32_t)val * 28000) / 4095;
    data.feedback = (int16_t)fb;
}


void fxProgram22ProcessSampleStereo(int16_t *inL, int16_t *inR, int16_t *outL, int16_t *outR, void* d, volatile uint32_t* audioState) {
    // Update Oscillator
    data.oscPhase += data.freqShift;
    
    // Get Sine/Cos from LUT (Linear Interpolation)
    uint16_t p = data.oscPhase;
    uint8_t idx = p >> 8;
    uint8_t nextIdx = (idx + 1) & 0xFF; // Wrap to prevent overflow
    uint16_t frac = p & 0xFF; // 0-255
    
    int16_t s1 = sineLUT[idx];
    int16_t s2 = sineLUT[nextIdx];
    int16_t sinVal = s1 + (((s2 - s1) * frac) >> 8);
    
    // Cos is Sin + 90 deg (64 steps in 256)
    uint8_t idxC = idx + 64;
    uint8_t nextIdxC = (idxC + 1) & 0xFF; // Wrap to prevent overflow
    int16_t c1 = sineLUT[idxC];
    int16_t c2 = sineLUT[nextIdxC];
    int16_t cosVal = c1 + (((c2 - c1) * frac) >> 8);
    // Stereo Spread: Offset Right Oscillator by 90 degrees (16384 in Q16)
    // This decorrelates the L/R shifting for a wider image
    uint16_t pR = data.oscPhase + 16384; 
    uint8_t idxR = pR >> 8;
    uint8_t nextIdxR = (idxR + 1) & 0xFF;
    uint16_t fracR = pR & 0xFF;
    
    int16_t s1R = sineLUT[idxR];
    int16_t s2R = sineLUT[nextIdxR];
    int16_t sinValR = s1R + (((s2R - s1R) * fracR) >> 8);
    
    uint8_t idxCR = idxR + 64;
    uint8_t nextIdxCR = (idxCR + 1) & 0xFF;
    int16_t c1R = sineLUT[idxCR];
    int16_t c2R = sineLUT[nextIdxCR];
    int16_t cosValR = c1R + (((c2R - c1R) * fracR) >> 8);

    // Read Delay Output (Based on Time)
    int16_t readIdx = data.writePtr - data.time;
    if (readIdx < 0) readIdx += CWO_DELAY_LEN;
    
    int16_t delayedL = data.delayBufferL[readIdx];
    int16_t delayedR = data.delayBufferR[readIdx];
    
    // --- Strong Gating to prevent Self-Oscillation of Noise ---
    // If signal is very quiet, silence it before it enters the details Frequency Shifter
    // Threshold: ~120 (approx -48dB)
    if (delayedL > -120 && delayedL < 120) delayedL = 0;
    if (delayedR > -120 && delayedR < 120) delayedR = 0;
    
    // Frequency Shift the DELAYED signal
    int16_t shiftedL = frequencyShift(delayedL, &data.splitL_A, &data.splitL_B, sinVal, cosVal);
    int16_t shiftedR = frequencyShift(delayedR, &data.splitR_A, &data.splitR_B, sinValR, cosValR);
    
    // Calculate Feedback
    int16_t fbL = ((int32_t)shiftedL * data.feedback) >> 15;
    int16_t fbR = ((int32_t)shiftedR * data.feedback) >> 15;
    
    // DC Block Feedback
    static int32_t dcL = 0, dcR = 0;
    fbL = fbL - (dcL >> 8); dcL += ((fbL - (dcL >> 8)) * 100) >> 8; // Leaky Integrator Highpass
    fbR = fbR - (dcR >> 8); dcR += ((fbR - (dcR >> 8)) * 100) >> 8;
    
    // Write new input
    // Attenuate input to prevent clipping when feedback is high
    int16_t writeValL = (*inL >> 1) + fbL; 
    int16_t writeValR = (*inR >> 1) + fbR;
    
    // DC Block Input to Delay (Critical for Freq Shifter: DC -> Sine Tone)
    static int32_t dcInL = 0, dcInR = 0;
    writeValL = writeValL - (dcInL >> 8); dcInL += ((writeValL - (dcInL >> 8)) * 50) >> 8;
    writeValR = writeValR - (dcInR >> 8); dcInR += ((writeValR - (dcInR >> 8)) * 50) >> 8;
    
    // Saturation
    if (writeValL > 32760) writeValL = 32760;
    if (writeValL < -32760) writeValL = -32760;
    if (writeValR > 32760) writeValR = 32760;
    if (writeValR < -32760) writeValR = -32760;
    
    data.delayBufferL[data.writePtr] = writeValL;
    data.delayBufferR[data.writePtr] = writeValR;
    
    data.writePtr++;
    if (data.writePtr >= CWO_DELAY_LEN) data.writePtr = 0;
    
    // Mix
    int32_t wet = data.mix * 8; 
    int32_t dry = 32767 - wet;
    if (wet > 32767) wet = 32767;
    if (dry < 0) dry = 0;
    
    // Reduced wet gain further (-6dB vs Dry) to compensate for resonant power
    *outL = clip(((*inL * dry) >> 15) + ((delayedL * wet) >> 16), audioState);
    *outR = clip(((*inR * dry) >> 15) + ((delayedR * wet) >> 16), audioState);
}

FxProgramType fxProgramCwo = {
    .name = "Cow Echo (Bode)",
    .nParameters = 3,
    .parameters = {
        {
            .name = "Mix",
            .control = 0, // Main
            .rawValue = 0,
            .setParameter = fxProgram22Param1Callback
        },
        {
            .name = "Freq Shift",
            .control = 1, // X
            .rawValue = 2048, // Center
            .setParameter = fxProgram22Param2Callback
        },
        {
            .name = "Space (Dly/Fb)",
            .control = 2, // Y
            .rawValue = 0,
            .setParameter = fxProgram22Param3Callback
        }
    },
    .processSample = 0, // Null - forces stereo path
    .processSampleStereo = fxProgram22ProcessSampleStereo,
    .setup = fxProgram22Setup,
    .reset = fxProgram22Reset,
    .isStereo = 1, // CRITICAL: Enable stereo processing
    .data = (void*)&data
};
