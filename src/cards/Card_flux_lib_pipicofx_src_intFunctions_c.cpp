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
#include <cstring>
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

namespace Card_Flux {
    extern const int16_t exptable_impl[];
    extern const int16_t logtable_impl[];
/**
 * @file intFunctions.c
 * @author Philipp Fuerholz (fuerholz@gmx.ch)
 * @brief contains function operating on integer types
 * @version 0.1
 * @date 2021-12-22
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "intFunctions.h"
#include "system.h"

/**
 * @brief compares to bytes interpreted as unsigned integers
 * 
 * @param a the first integer 
 * @param b  the second integer
 * @return a - b as signed integer
 */
int compareUint8(const void* a,const void* b)
{
	return (int)(*(uint8_t*)a - *(uint8_t*)b);
}

} // namespace Card_Flux
