// Braids SVF lookup tables, vendored from releases/10_twists/src/braids/resources.cc
// Only the three tables needed by svf_braids.h are included here, to keep the
// Hot Fuzz card self-contained without pulling in the full Braids resources.
//
// Original: Copyright 2012 Emilie Gillet, MIT license. See svf_braids.h for full notice.

#ifndef BRAIDS_RESOURCES_H_
#define BRAIDS_RESOURCES_H_

#include <stdint.h>

namespace braids {

// f coefficient for the SVF, indexed by pitch (frequency_ << 17 for 8.24 interpolation).
// 257 entries (256 + 1 guard) for Interpolate824. Values saturate at 25078 (~0.766 in Q15),
// keeping the filter stable across the full audible range.
extern const uint16_t lut_svf_cutoff[];

// damp (1/Q) coefficient for the SVF, indexed by resonance (resonance_ << 17).
// 257 entries. Higher resonance -> lower damp.
extern const uint16_t lut_svf_damp[];

// Output scale factor per resonance level, used to compensate gain in BP mode.
// 257 entries. Not needed for LP-only use, but kept here for completeness if
// a future version wants BP or mixed outputs.
extern const uint16_t lut_svf_scale[];

}  // namespace braids

#endif  // BRAIDS_RESOURCES_H_