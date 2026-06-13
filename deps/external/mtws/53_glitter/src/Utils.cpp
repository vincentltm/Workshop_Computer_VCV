#include "Utils.h"

int32_t cabs(int32_t a)
{
    return (a > 0) ? a : -a;
}

// taken from goldfish
// If a knob goes near either limit, or the middle,
// clamp it to that limit (or to the middle)
int16_t virtualDetentedKnob(int16_t val)
{
    if (val > 4071)
    {
        val = 4095;
    }
    else if (val < 24)
    {
        val = 0;
    }

    if (cabs(val - 2048) < 24)
    {
        val = 2048;
    }

    return val;
}
