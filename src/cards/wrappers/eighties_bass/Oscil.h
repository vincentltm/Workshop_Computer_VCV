#pragma once
#include <stdint.h>

template <uint32_t NUM_CELLS, uint32_t UPDATE_RATE>
class Oscil {
public:
    const int8_t* table = nullptr;
    uint32_t phase_fractional = 0;
    uint32_t phase_increment = 0;

    Oscil() {}
    Oscil(const int8_t* t) : table(t) {}

    void setTable(const int8_t* t) {
        table = t;
    }

    void setFreq(float frequency) {
        phase_increment = (uint32_t)((frequency * 4294967296.0) / UPDATE_RATE);
    }

    void setFreq(int frequency) {
        setFreq((float)frequency);
    }

    inline int8_t next() {
        if (!table) return 0;
        phase_fractional += phase_increment;
        uint32_t index = (phase_fractional >> 23) & 511; // Hardcoded for 512 cells
        return table[index];
    }
};
