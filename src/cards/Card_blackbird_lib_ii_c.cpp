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
#include "lib/ii.h"
/* stripped system include */
/* stripped system include */

// Inter-IC (I2C) communication stubs for RP2040

static uint8_t ii_address = 0x01;
static ii_mode_t ii_mode = ii_mode_none;

void ii_init(void) {
    // Stub implementation - could initialize RP2040 I2C if needed
    printf("II: Init\n");
    ii_mode = ii_mode_none;
}

void ii_deinit(void) {
    printf("II: Deinit\n");
    ii_mode = ii_mode_none;
}

void ii_set_address(uint8_t address) {
    ii_address = address;
    printf("II: Set address 0x%02x\n", address);
}

uint8_t ii_get_address(void) {
    return ii_address;
}

void ii_leader_tx(uint8_t address, uint8_t* data, uint8_t count) {
    if (data && count > 0) {
        printf("II: Leader TX to 0x%02x, %d bytes\n", address, count);
        ii_mode = ii_mode_leader_tx;
    }
}

void ii_leader_rx(uint8_t address, uint8_t count) {
    printf("II: Leader RX from 0x%02x, %d bytes\n", address, count);
    ii_mode = ii_mode_leader_rx;
}

void ii_follower_start(uint8_t address) {
    ii_address = address;
    ii_mode = ii_mode_follower;
    printf("II: Follower start at 0x%02x\n", address);
}

void ii_follower_stop(void) {
    ii_mode = ii_mode_none;
    printf("II: Follower stop\n");
}

void ii_process_leader(void) {
    // Stub implementation - process leader transactions
}

void ii_process_follower(void) {
    // Stub implementation - process follower transactions
}

uint8_t ii_tx_now(uint8_t address, uint8_t* data, uint8_t count) {
    if (data && count > 0) {
        printf("II: TX now to 0x%02x, %d bytes\n", address, count);
        return 1; // success
    }
    return 0; // failure
}

uint8_t ii_rx_now(uint8_t address, uint8_t* data, uint8_t count) {
    if (data && count > 0) {
        printf("II: RX now from 0x%02x, %d bytes\n", address, count);
        // Fill with dummy data
        memset(data, 0x00, count);
        return count; // bytes received
    }
    return 0; // no data
}

// Additional crow compatibility stubs
const char* ii_list_modules(void) {
    return "II modules: Workshop Computer stub (no actual modules)";
}

const char* ii_list_cmds(uint8_t address) {
    static char msg[64];
    snprintf(msg, sizeof(msg), "II commands for 0x%02x: Workshop Computer stub (no commands)", address);
    return msg;
}

void ii_set_pullups(uint8_t state) {
    printf("II: Set pullups %s\n", state ? "enabled" : "disabled");
    // Stub - would configure I2C pullups on actual hardware
}

uint8_t ii_leader_enqueue(uint8_t address, uint8_t cmd, float* data) {
    printf("II: Leader enqueue to 0x%02x, cmd %d\n", address, cmd);
    // Stub - would queue I2C transaction on actual hardware
    return 0; // success
}

uint8_t ii_leader_enqueue_bytes(uint8_t address, uint8_t* data, uint8_t len, uint8_t rx_len) {
    printf("II: Leader enqueue bytes to 0x%02x, %d tx bytes, %d rx bytes\n", address, len, rx_len);
    // Stub - would queue raw byte I2C transaction on actual hardware
    return 0; // success
}

} // namespace Card_Blackbird
