#pragma once
#include <stdint.h>

class MultiResonantFilter {
public:
    uint8_t q = 0;
    uint8_t f = 0;
    uint16_t fb = 0;
    int32_t buf0 = 0;
    int32_t buf1 = 0;
    int32_t last_in = 0;

    void setCutoffFreqAndResonance(uint8_t cutoff, uint8_t resonance) {
        f = cutoff;
        q = resonance;
        fb = q + (((uint16_t)q * ((255 + cutoff) >> 1)) >> 7);
    }

    inline void next(int32_t in) {
        last_in = in;
        int32_t term1 = ((int32_t)fb * (buf0 - buf1)) >> 8;
        buf0 += ((in - buf0 + term1) * (int32_t)f) >> 8;
        buf1 += ((buf0 - buf1) * (int32_t)f) >> 8;
    }

    inline int32_t low() { return buf1; }
    inline int32_t high() { return last_in - buf0; }
    inline int32_t band() { return buf0 - buf1; }
    inline int32_t notch() { return last_in - buf0 + buf1; }
};
