#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"

// Mock structures for usbh_pvt.h
typedef enum {
    XFER_RESULT_SUCCESS = 0,
    XFER_RESULT_FAILED,
    XFER_RESULT_STALLED,
    XFER_RESULT_TIMEOUT
} xfer_result_t;

typedef struct {
    const char* name;
    bool (*init)(void);
    bool (*deinit)(void);
    bool (*open)(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const* itf_desc, uint16_t max_len);
    bool (*set_config)(uint8_t dev_addr, uint8_t itf_num);
    bool (*xfer_cb)(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
    void (*close)(uint8_t dev_addr);
} usbh_class_driver_t;

inline bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) { (void)dev_addr; (void)ep_addr; return false; }
inline bool usbh_edpt_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t* buffer, uint16_t len) { (void)dev_addr; (void)ep_addr; (void)buffer; (void)len; return false; }
inline bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) { (void)dev_addr; (void)ep_addr; return false; }
inline bool tuh_edpt_open(uint8_t dev_addr, void const* desc_ep) { (void)dev_addr; (void)desc_ep; return false; }
inline void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) { (void)dev_addr; (void)itf_num; }
