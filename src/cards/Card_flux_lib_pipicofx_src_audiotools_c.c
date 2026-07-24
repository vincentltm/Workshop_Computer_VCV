/* Automatically generated C wrapper (compiled as C99, not C++) */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
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
#include "pico_mocks_c.h"

#include "stdint.h"
#include "audio/audiotools.h"

static volatile uint32_t audioState=0;

volatile uint32_t * getAudioStatePtr()
{
    return &audioState;
}
