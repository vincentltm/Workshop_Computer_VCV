#pragma once

#define COMPUTERCARD_NOIMPL
#include "ComputerCard.h"

// generate a 16 bit random number
extern int32_t rnd();

// return absolute value on an int
extern int32_t cabs(int32_t a);

// If a knob goes near either limit, or the middle,
// clamp it to that limit (or to the middle)
extern int16_t virtualDetentedKnob(int16_t val);
