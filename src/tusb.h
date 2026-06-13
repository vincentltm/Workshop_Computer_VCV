#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define TUSB_INDEX_INVALID_8 0xFF
#define TUSB_CLASS_AUDIO 0x01
#define TUSB_DESC_INTERFACE 0x04
#define TUSB_DESC_ENDPOINT 0x05
#define TUSB_XFER_BULK 2
#define TUSB_DIR_IN 0x80
#define TUSB_OPT_RHPORT 0

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} tusb_desc_interface_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    struct {
        uint8_t xfer : 2;
        uint8_t sync : 2;
        uint8_t usage : 2;
        uint8_t reserved : 2;
    } bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} tusb_desc_endpoint_t;

inline uint8_t tu_desc_type(void const* desc) {
    return ((uint8_t const*)desc)[1];
}

inline uint8_t const* tu_desc_next(void const* desc) {
    return (uint8_t const*)desc + ((uint8_t const*)desc)[0];
}

inline uint8_t tu_edpt_dir(uint8_t addr) {
    return addr & 0x80;
}

inline uint16_t tu_edpt_packet_size(tusb_desc_endpoint_t const* ep) {
    return ep->wMaxPacketSize;
}

// ThreadSafeByteQueue, ThreadSafePacketQueue, MidiPacket, and
// g_midi_rx_packet_queue / g_midi_tx_byte_queue are all provided by
// pico_mocks.h (which is always included first via ComputerCard.h).
// Do NOT re-declare them here.

// ──────────────────────────────────────────────────────────────────────────────
// USB-MIDI inline stubs
// ──────────────────────────────────────────────────────────────────────────────

inline bool tud_midi_available() {
    return !g_midi_rx_packet_queue.empty();
}

// Extract packet size from USB-MIDI CIN nibble
static inline size_t get_packet_size_from_cin_local(uint8_t cin) {
    switch (cin) {
        case 0x05: case 0x0F: return 1;
        case 0x02: case 0x0C: case 0x0D: case 0x06: return 2;
        case 0x03: case 0x04: case 0x07: case 0x08:
        case 0x09: case 0x0A: case 0x0B: case 0x0E: return 3;
        default: return 0;
    }
}

inline bool tud_midi_packet_read(uint8_t packet[4]) {
    return g_midi_rx_packet_queue.pop(packet);
}

inline uint32_t tud_midi_packet_write(uint8_t const packet[4]) {
    uint8_t cin = packet[0] & 0x0F;
    size_t len = get_packet_size_from_cin_local(cin);
    if (len > 0) {
        g_midi_tx_byte_queue.push(packet + 1, len);
    }
    return 4;
}

inline size_t tud_midi_stream_read(uint8_t* buffer, size_t bufsize) {
    static thread_local uint8_t local_byte_buf[256];
    static thread_local size_t  local_byte_count = 0;
    static thread_local size_t  local_byte_idx   = 0;

    size_t bytes_written = 0;
    while (bytes_written < bufsize && local_byte_idx < local_byte_count) {
        buffer[bytes_written++] = local_byte_buf[local_byte_idx++];
    }
    while (bytes_written < bufsize) {
        uint8_t packet[4];
        if (!g_midi_rx_packet_queue.pop(packet)) break;
        uint8_t cin = packet[0] & 0x0F;
        size_t pkt_len = get_packet_size_from_cin_local(cin);
        local_byte_count = 0;
        local_byte_idx   = 0;
        for (size_t i = 0; i < pkt_len; ++i)
            local_byte_buf[local_byte_count++] = packet[1 + i];
        while (bytes_written < bufsize && local_byte_idx < local_byte_count)
            buffer[bytes_written++] = local_byte_buf[local_byte_idx++];
    }
    return bytes_written;
}

inline uint32_t tud_midi_stream_write(uint8_t /*itf*/, uint8_t const* buffer, uint32_t count) {
    g_midi_tx_byte_queue.push(buffer, count);
    return count;
}

inline bool tud_midi_n_mounted(uint8_t) { return true; }
inline bool tud_midi_mounted()          { return true; }
inline void tud_task()   {}
inline bool tusb_init()  { return true; }
inline bool tud_init(uint8_t) { return true; }
inline void board_init() {}

#define TUD_OPT_RHPORT 0
#define TUH_OPT_RHPORT 0
inline void    tuh_init(uint8_t) {}
inline void    tuh_task() {}
inline int32_t tuh_midi_stream_read(uint8_t, uint8_t*, uint8_t*, uint16_t) { return 0; }

// ──────────────────────────────────────────────────────────────────────────────
// USB-CDC inline stubs for serial communications (WebSerial support)
// ──────────────────────────────────────────────────────────────────────────────
inline uint32_t tud_cdc_available() {
    return g_serial_rx_byte_queue.size();
}

inline uint32_t tud_cdc_read(void* buffer, uint32_t bufsize) {
    uint8_t* ptr = (uint8_t*)buffer;
    uint32_t read_bytes = 0;
    while (read_bytes < bufsize) {
        uint8_t val;
        if (!g_serial_rx_byte_queue.pop(val)) {
            break;
        }
        ptr[read_bytes++] = val;
    }
    return read_bytes;
}

inline uint32_t tud_cdc_write(void const* buffer, uint32_t bufsize) {
    g_serial_tx_byte_queue.push((const uint8_t*)buffer, bufsize);
    return bufsize;
}

inline uint32_t tud_cdc_write_str(char const* str) {
    uint32_t len = strlen(str);
    g_serial_tx_byte_queue.push((const uint8_t*)str, len);
    return len;
}

inline uint32_t tud_cdc_write_char(char c) {
    g_serial_tx_byte_queue.push(static_cast<uint8_t>(c));
    return 1;
}

inline uint32_t tud_cdc_write_available() {
    return 256;
}

inline uint32_t tud_cdc_write_flush() {
    return 0;
}

inline bool tud_cdc_connected() {
    if (!t_instance) return true;
    return t_instance->grid_connected_flag || t_instance->web_ui_connected;
}

inline uint32_t tud_cdc_n_available(uint8_t itf) { (void)itf; return tud_cdc_available(); }
inline uint32_t tud_cdc_n_read(uint8_t itf, void* buffer, uint32_t bufsize) { (void)itf; return tud_cdc_read(buffer, bufsize); }
inline uint32_t tud_cdc_n_write(uint8_t itf, void const* buffer, uint32_t bufsize) { (void)itf; return tud_cdc_write(buffer, bufsize); }
inline uint32_t tud_cdc_n_write_flush(uint8_t itf) { (void)itf; return tud_cdc_write_flush(); }
inline bool tud_cdc_n_connected(uint8_t itf) { (void)itf; return tud_cdc_connected(); }
inline uint32_t tud_cdc_n_write_available(uint8_t itf) { (void)itf; return 256; }
inline uint32_t tuh_cdc_write_available(uint8_t dev_addr) { (void)dev_addr; return 256; }
inline uint32_t tuh_cdc_write(uint8_t dev_addr, void const* buffer, uint32_t bufsize) {
    (void)dev_addr;
    g_serial_tx_byte_queue.push((const uint8_t*)buffer, bufsize);
    return bufsize;
}
inline uint32_t tuh_cdc_write_flush(uint8_t dev_addr) { (void)dev_addr; return 0; }
inline uint32_t tuh_cdc_read(uint8_t dev_addr, void* buffer, uint32_t bufsize) {
    (void)dev_addr;
    return tud_cdc_read(buffer, bufsize);
}
inline bool tud_mounted() { return true; }


