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

namespace Card_Twists {
#include "braids/drivers/display.h"

#include "braids/resources.h"

namespace braids {

void Display::Init() {
  for(int i=PIN_LED1_OUT; i<PIN_LED1_OUT+6; i++) {
    gpio_init(i);
    gpio_set_dir(i, GPIO_OUT);
  }
  SetBits(0);
}

void Display::SetBits(uint8_t values) {
  gpio_put_masked(PIN_MASK_LEDS, (values & 0x3F) << PIN_LED1_OUT);
}

}  // namespace braids

} // namespace Card_Twists
