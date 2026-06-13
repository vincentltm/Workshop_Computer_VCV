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

namespace Card_Blackbird {
#include "random.h"

/* stripped system include */
/* stripped system include */
/* stripped system include */

// Random number generator implementation for RP2040 Workshop Computer
// Uses standard C library functions for simplicity

static int random_initialized = 0;

void Random_Init(unsigned int seed) {
    srand(seed);
    random_initialized = 1;
    printf("Random: Initialized with seed %u\n", seed);
}

float Random_Float(void) {
    if (!random_initialized) {
        // Initialize with current time if not already initialized
        Random_Init((unsigned int)time(NULL));
    }
    
    return (float)rand() / (float)RAND_MAX;
}

int Random_Int(int min, int max) {
    if (!random_initialized) {
        // Initialize with current time if not already initialized
        Random_Init((unsigned int)time(NULL));
    }
    
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    
    return min + (rand() % (max - min + 1));
}

} // namespace Card_Blackbird
