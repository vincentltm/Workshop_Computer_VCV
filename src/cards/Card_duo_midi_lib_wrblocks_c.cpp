// Automatically generated separate compilation wrapper
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <setjmp.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#include <inttypes.h>
#include <cinttypes>
#include "pico_mocks.h"
#include "tusb.h"
#define while(...) while((__VA_ARGS__) && !g_cancellation_requested.load(std::memory_order_relaxed))

#include "ComputerCard.h"

namespace Card_DuoMidi {
#include "wrblocks.h"

// Essential wrDsp functions for slopes system
// Copied from submodules/wrDsp for local use

float* b_cp( float* dest, float src, int size ){
    float* d = dest;
    for( int i=0; i<size; i++ ){
        *d++ = src;
    }
    return dest;
}

float* b_cp_v( float* dest, float* src, int size ){
    float* d = dest;
    for( int i=0; i<size; i++ ){
        *d++ = *src++;
    }
    return dest;
}

float* b_add( float* io, float add, int size ){
    float* d = io;
    for( int i=0; i<size; i++ ){
        *d++ += add;
    }
    return io;
}

float* b_sub( float* io, float sub, int size ){
    float* d = io;
    for( int i=0; i<size; i++ ){
        *d = sub - *d;
        d++;
    }
    return io;
}

float* b_mul( float* io, float mul, int size ){
    float* d = io;
    for( int i=0; i<size; i++ ){
        *d++ *= mul;
    }
    return io;
}

float* b_accum_v( float* dest, float* src, int size ){
    float* d = dest;
    for( int i=0; i<size; i++ ){
        *d++ += *src++;
    }
    return dest;
}

float* b_map( b_fn_t fn, float* io, int size ){
    float* s = io;
    for( int i=0; i<size; i++ ){
        *s = (*fn)(*s);
        s++;
    }
    return io;
}

} // namespace Card_DuoMidi
