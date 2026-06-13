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
#include "braids/drivers/switch.h"

namespace braids {

void Switch::Init() {
  switch_state_ = 0xff;
  init_count_ = 0;
}

void Switch::Debounce(uint16_t switch_val) {
  // wait for value to settle on startup 
  if(init_count_ < 8) {
    init_count_++;
  }

  switch_state_ = (switch_state_ << 1) | !(switch_val < SWITCH_DOWN_BOUNDRY);
}

}  // namespace braids

} // namespace Card_Twists
