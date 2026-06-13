#pragma once
#include <math.h>
inline float mtof(float midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69.0f) / 12.0f);
}
