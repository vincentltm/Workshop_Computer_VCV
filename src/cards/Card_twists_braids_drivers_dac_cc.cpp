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
#include "braids/drivers/dac.h"

/* stripped system include */

namespace braids {
  
void Dac::Init() {
  // Initialize SPI channel (channel, baud rate set to 20MHz)
  spi_init(DAC_SPI_PORT, 20000000);
  // Format (channel, data bits per transfer, polarity, phase, order)
  spi_set_format(DAC_SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_LSB_FIRST);

  // Map SPI signals to GPIO ports    
  gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_CS, GPIO_FUNC_SPI);
}

}  // namespace braids

} // namespace Card_Twists
