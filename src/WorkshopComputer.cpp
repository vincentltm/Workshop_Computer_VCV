#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <direct.h>
#else
    #include <dlfcn.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif
#include <fstream>
#include <osdialog.h>

#include "plugin_local.hpp"
#include "BridgeAudioPort.hpp"
#include "ComputerCard.h"
#include "cards/CardRegistry.hpp"
#include "tusb.h"
#include "WebServer.hpp"
#include "monome_bridge.hpp"
#include "shared/ComputerWidgets.hpp"
#include <app/MidiDisplay.hpp>
#include <app/AudioDisplay.hpp>
#include <thread>
#include <atomic>
#include <string>
#include <mutex>
#include <condition_variable>

// ── Per-thread state routing ────────────────────────────────────────────────
// Each WorkshopComputer module instance owns a CardGlobals.
// Before any card code runs (audio thread or background thread) t_instance is
// pointed at the right module's state. All globals in pico_mocks.h are macros
// that expand through this pointer.
thread_local CardGlobals* t_instance = nullptr;

// ── Web Server Globals ──────────────────────────────────────────────────────
#include <set>
#include <mutex>
#include <atomic>

#ifdef _WIN32
    typedef SOCKET socket_t;
    #ifndef INVALID_SOCKET
        #define INVALID_SOCKET -1
    #endif
#else
    typedef int socket_t;
    #define INVALID_SOCKET -1
#endif

struct WorkshopComputer;
void start_web_server();
void stop_web_server();

std::mutex g_instances_mutex;
std::set<void*> g_instances;
int g_web_server_port = 0;
std::atomic<bool> g_web_server_running{false};
socket_t g_server_fd = INVALID_SOCKET;
std::thread g_server_thread;

// Per-thread card pointer (thread-local analogue of ComputerCard::thisptr so
// each background / core1 thread sees its own card, not another instance's).
thread_local ComputerCard* g_current_card_ptr = nullptr;

// thread_local static member definition (required in one TU)
thread_local ComputerCard* ComputerCard::thisptr = nullptr;

// is_core1_thread: already thread_local, defined once here
thread_local bool is_core1_thread = false;

static std::string hex_encode(const uint8_t* data, size_t size) {
    std::string s;
    s.reserve(size * 2);
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < size; ++i) {
        s.push_back(hex_chars[data[i] >> 4]);
        s.push_back(hex_chars[data[i] & 0xf]);
    }
    return s;
}

static void hex_decode(const std::string& s, uint8_t* data, size_t max_size) {
    size_t len = s.length();
    size_t limit = std::min(len / 2, max_size);
    for (size_t i = 0; i < limit; ++i) {
        char c1 = s[i * 2];
        char c2 = s[i * 2 + 1];
        uint8_t b = 0;
        if (c1 >= '0' && c1 <= '9') b |= (c1 - '0') << 4;
        else if (c1 >= 'a' && c1 <= 'f') b |= (c1 - 'a' + 10) << 4;
        else if (c1 >= 'A' && c1 <= 'F') b |= (c1 - 'A' + 10) << 4;
        if (c2 >= '0' && c2 <= '9') b |= (c2 - '0');
        else if (c2 >= 'a' && c2 <= 'f') b |= (c2 - 'a' + 10);
        else if (c2 >= 'A' && c2 <= 'F') b |= (c2 - 'A' + 10);
        data[i] = b;
    }
}

void host_multicore_launch_core1(void (*entry)()) {
    CardGlobals* inst = t_instance;
    if (!inst) return;
    ComputerCard* card = inst->card_ptr;

    if (inst->g_core1_thread_val.joinable()) {
        inst->g_core1_cancellation_requested_val = true;
        inst->g_core1_thread_val.join();
    }
    inst->g_core1_cancellation_requested_val = false;

    inst->g_core1_thread_val = std::thread([entry, inst, card]() {
        t_instance = inst;
        is_core1_thread = true;
        g_current_card_ptr = card;
        // Also sync ComputerCard::thisptr for this thread
        ComputerCard::thisptr = card;
        if (inst->set_thread_globals_fn) {
            inst->set_thread_globals_fn(inst);
        }
        if (inst->set_core1_thread_fn) {
            inst->set_core1_thread_fn(true);
        }
        try { entry(); } catch (const ThreadExitException&) {}
    });
}

// Helper to push midi to rx queue
void push_midi_to_rx_queue(const uint8_t* msg_bytes, size_t msg_size) {
    if (msg_size == 0) return;
    
    if (msg_bytes[0] == 0xF0) {
        // SysEx
        size_t idx = 0;
        while (idx < msg_size) {
            size_t rem = msg_size - idx;
            uint8_t packet[4] = {0};
            if (rem >= 3) {
                if (msg_bytes[idx + 2] == 0xF7) {
                    packet[0] = 0x07; // SysEx ends with 3 bytes
                } else {
                    packet[0] = 0x04; // SysEx starts or continues
                }
                packet[1] = msg_bytes[idx];
                packet[2] = msg_bytes[idx+1];
                packet[3] = msg_bytes[idx+2];
                idx += 3;
            } else if (rem == 2) {
                packet[0] = 0x06; // SysEx ends with 2 bytes
                packet[1] = msg_bytes[idx];
                packet[2] = msg_bytes[idx+1];
                idx += 2;
            } else if (rem == 1) {
                packet[0] = 0x05; // SysEx ends with 1 byte
                packet[1] = msg_bytes[idx];
                idx += 1;
            }
            g_midi_rx_packet_queue.push(packet);
        }
    } else {
        // Non-SysEx
        uint8_t status = msg_bytes[0];
        uint8_t high_nibble = status & 0xF0;
        uint8_t packet[4] = {0};
        
        if (high_nibble == 0x80) { packet[0] = 0x08; packet[1] = status; packet[2] = msg_size > 1 ? msg_bytes[1] : 0; packet[3] = msg_size > 2 ? msg_bytes[2] : 0; }
        else if (high_nibble == 0x90) { packet[0] = 0x09; packet[1] = status; packet[2] = msg_size > 1 ? msg_bytes[1] : 0; packet[3] = msg_size > 2 ? msg_bytes[2] : 0; }
        else if (high_nibble == 0xA0) { packet[0] = 0x0A; packet[1] = status; packet[2] = msg_size > 1 ? msg_bytes[1] : 0; packet[3] = msg_size > 2 ? msg_bytes[2] : 0; }
        else if (high_nibble == 0xB0) { packet[0] = 0x0B; packet[1] = status; packet[2] = msg_size > 1 ? msg_bytes[1] : 0; packet[3] = msg_size > 2 ? msg_bytes[2] : 0; }
        else if (high_nibble == 0xC0) { packet[0] = 0x0C; packet[1] = status; packet[2] = msg_size > 1 ? msg_bytes[1] : 0; }
        else if (high_nibble == 0xD0) { packet[0] = 0x0D; packet[1] = status; packet[2] = msg_size > 1 ? msg_bytes[1] : 0; }
        else if (high_nibble == 0xE0) { packet[0] = 0x0E; packet[1] = status; packet[2] = msg_size > 1 ? msg_bytes[1] : 0; packet[3] = msg_size > 2 ? msg_bytes[2] : 0; }
        else {
            packet[0] = 0x0F;
            packet[1] = status;
            if (msg_size > 1) packet[2] = msg_bytes[1];
            if (msg_size > 2) packet[3] = msg_bytes[2];
        }
        g_midi_rx_packet_queue.push(packet);
    }
}// MidiTxParser is now defined in WebServer.hpp

void save_flash_to_disk() {
    // t_instance is always set before this is called (by background thread or change_card)
    // Store flash files next to the plugin binary for portability
    std::string flash_dir = rack::asset::plugin(pluginInstance, "");
    // Trim trailing slash if needed
    if (!flash_dir.empty() && flash_dir.back() == '/') flash_dir.pop_back();
    std::string filename = flash_dir + "/flash_" + g_active_card_id + ".bin";
    FILE* f = fopen(filename.c_str(), "wb");
    if (f) { fwrite(g_flash_memory, 1, PICO_FLASH_SIZE_BYTES, f); fclose(f); }
}

void load_flash_from_disk() {
    std::string flash_dir = rack::asset::plugin(pluginInstance, "");
    if (!flash_dir.empty() && flash_dir.back() == '/') flash_dir.pop_back();
    std::string filename = flash_dir + "/flash_" + g_active_card_id + ".bin";
    FILE* f = fopen(filename.c_str(), "rb");
    if (f) {
        size_t n = fread(g_flash_memory, 1, PICO_FLASH_SIZE_BYTES, f);
        fclose(f);
        if (n < PICO_FLASH_SIZE_BYTES)
            memset(g_flash_memory + n, 0xFF, PICO_FLASH_SIZE_BYTES - n);
    } else {
        memset(g_flash_memory, 0xFF, PICO_FLASH_SIZE_BYTES);
    }
}

struct MonomeSerialParser {
    std::vector<uint8_t> buf;
    int expected_len = 0;
    uint8_t grid_leds[16][16] = {};
    bool grid_dirty[4] = {};

    // When non-null, send LED updates directly to the virtual grid
    // instead of (or in addition to) the MIDI path.
    Grid* target_grid = nullptr;

    void parse_byte(uint8_t b, std::vector<rack::midi::Message>& out_messages) {
        if (expected_len == 0) {
            buf.clear();
            buf.push_back(b);
            expected_len = get_cmd_len(b);
            if (expected_len == 1) {
                process_packet(buf, out_messages);
                expected_len = 0;
            }
        } else {
            buf.push_back(b);
            if (buf.size() >= (size_t)expected_len) {
                process_packet(buf, out_messages);
                expected_len = 0;
            }
        }
    }

    int get_cmd_len(uint8_t header) {
        uint8_t addr = header >> 4;
        uint8_t cmd  = header & 0x0F;

        if (addr == 0x1) {
            switch (cmd) {
                case 0x0: return 4;  // set single LED
                case 0x1: return 2;  // set all
                case 0x2: return 1;  // clear all
                case 0x3: return 4;  // map 8x8 binary
                case 0x4: return 11; // map 8x8 binary rows
                case 0x5: return 67; // map 8x8 intensity
                case 0x6: return 11; // row intensity
                case 0x7: return 2;  // brightness
                case 0x8: return 11; // col intensity
                case 0x9: return 11; // row map intensity
                case 0xA: return 35; // 8x8 block intensity map
                case 0xB: return 11; // col map intensity
                default: return 1;
            }
        }
        
        // Legacy protocols
        if ((header & 0xF0) == 0x80) return 9; // series quadrant map
        if ((header & 0xF0) == 0x70) return 2; // 40h row map
        if ((header & 0xF0) == 0x90) return 2; // 40h col map
        if ((header & 0xF0) == 0xA0) return 1; // series brightness
        if ((header & 0xF0) == 0x20) return 1; // 40h brightness
        
        return 1;
    }

    void process_packet(const std::vector<uint8_t>& packet, std::vector<rack::midi::Message>& out_messages) {
        if (packet.empty()) return;
        uint8_t header = packet[0];
        uint8_t addr = header >> 4;
        uint8_t cmd = header & 0x0F;

        // ── Direct grid update path (mirrors LibAVR32Module::readSerialMessages) ──
        // MEXT protocol: 0x1x commands
        if (addr == 0x1) {
            if (cmd == 0x2) { // clear all
                if (target_grid) target_grid->clearAll();
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        grid_leds[y][x] = 0;
                    }
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++)
                        send_led_midi(x, y, 0, out_messages);
            }
            else if (cmd == 0x0 && packet.size() >= 4) { // single LED set
                uint8_t x = packet[1], y = packet[2], level = packet[3];
                if (target_grid) {
                    // updateRow with a single bit — just send a 1-pixel quadrant
                    // Use local state + updateQuadrant for single LEDs
                    if (x < 16 && y < 16) grid_leds[y][x] = level;
                    int xo = (x >= 8) ? 8 : 0;
                    int yo = (y >= 8) ? 8 : 0;
                    uint8_t quad[64];
                    for (int r = 0; r < 8; r++)
                        for (int c = 0; c < 8; c++)
                            quad[r * 8 + c] = grid_leds[yo + r][xo + c];
                    target_grid->updateQuadrant(xo, yo, quad);
                }
                send_led_midi(x, y, level, out_messages);
            }
            else if (cmd == 0x1 && packet.size() >= 2) { // set all
                uint8_t level = packet[1];
                if (target_grid) {
                    for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++)
                            grid_leds[y][x] = level;
                    for (int q = 0; q < 4; q++) {
                        int xo = (q & 1) ? 8 : 0;
                        int yo = (q & 2) ? 8 : 0;
                        uint8_t quad[64];
                        for (int r = 0; r < 8; r++)
                            for (int c = 0; c < 8; c++)
                                quad[r * 8 + c] = level;
                        target_grid->updateQuadrant(xo, yo, quad);
                    }
                } else {
                    for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++)
                            grid_leds[y][x] = level;
                }
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++)
                        send_led_midi(x, y, level, out_messages);
            }
            else if (cmd == 0x4 && packet.size() >= 11) { // map 8x8 binary rows
                uint8_t xo = packet[1];
                uint8_t yo = packet[2];
                if (target_grid) {
                    uint8_t quad[64] = {};
                    for (int r = 0; r < 8; r++) {
                        uint8_t mask = packet[3 + r];
                        for (int c = 0; c < 8; c++) {
                            uint8_t level = (mask & (1 << c)) ? 15 : 0;
                            quad[r * 8 + c] = level;
                            if (xo < 16 && yo + r < 16) grid_leds[yo + r][xo + c] = level;
                        }
                    }
                    target_grid->updateQuadrant(xo, yo, quad);
                }
                for (int r = 0; r < 8; r++) {
                    uint8_t mask = packet[3 + r];
                    for (int c = 0; c < 8; c++)
                        send_led_midi(xo + c, yo + r, (mask & (1 << c)) ? 15 : 0, out_messages);
                }
            }
            else if (cmd == 0xA && packet.size() >= 35) { // 8x8 block varibright (MEXT)
                uint8_t xo = packet[1];
                uint8_t yo = packet[2];
                if (target_grid) {
                    uint8_t quad[64];
                    int p = 3;
                    for (int r = 0; r < 8; r++) {
                        for (int c = 0; c < 8; c += 2) {
                            uint8_t val = packet[p++];
                            quad[r * 8 + c]     = val >> 4;
                            quad[r * 8 + c + 1] = val & 0x0F;
                            if (xo + c     < 16 && yo + r < 16) grid_leds[yo + r][xo + c]     = val >> 4;
                            if (xo + c + 1 < 16 && yo + r < 16) grid_leds[yo + r][xo + c + 1] = val & 0x0F;
                        }
                    }
                    target_grid->updateQuadrant(xo, yo, quad);
                } else {
                    int p = 3;
                    for (int r = 0; r < 8; r++)
                        for (int c = 0; c < 8; c += 2) {
                            uint8_t val = packet[p++];
                            send_led_midi(xo + c, yo + r, val >> 4, out_messages);
                            send_led_midi(xo + c + 1, yo + r, val & 0x0F, out_messages);
                        }
                }
            }
        }
        // Series protocol: 0x8x quadrant (binary)
        else if ((header & 0xF0) == 0x80 && packet.size() >= 9) {
            uint8_t q = header & 0x03;
            uint8_t xo = (q & 1) ? 8 : 0;
            uint8_t yo = (q & 2) ? 8 : 0;
            if (target_grid) {
                uint8_t quad[64];
                for (int r = 0; r < 8; r++) {
                    uint8_t mask = packet[1 + r];
                    for (int c = 0; c < 8; c++) {
                        uint8_t level = (mask & (1 << c)) ? 15 : 0;
                        quad[r * 8 + c] = level;
                        grid_leds[yo + r][xo + c] = level;
                    }
                }
                target_grid->updateQuadrant(xo, yo, quad);
            }
            for (int r = 0; r < 8; r++) {
                uint8_t mask = packet[1 + r];
                for (int c = 0; c < 8; c++)
                    send_led_midi(xo + c, yo + r, (mask & (1 << c)) ? 15 : 0, out_messages);
            }
        }
        // 40h protocol: 0x7x row update
        else if ((header & 0xF0) == 0x70 && packet.size() >= 2) {
            uint8_t y = header & 0x07;
            uint8_t mask = packet[1];
            if (target_grid) {
                target_grid->updateRow(0, y, mask);
                for (int x = 0; x < 8; x++)
                    grid_leds[y][x] = (mask & (1 << x)) ? 15 : 0;
            }
            for (int x = 0; x < 8; x++)
                send_led_midi(x, y, (mask & (1 << x)) ? 15 : 0, out_messages);
        }
    }

    void send_led_midi(int x, int y, uint8_t level, std::vector<rack::midi::Message>& out_messages) {
        if (x < 0 || x >= 16 || y < 0 || y >= 16) return;

        // Track local LED state
        if (grid_leds[y][x] != level) {
            grid_leds[y][x] = level;
            int q = ((y >= 8) << 1) | (x >= 8);
            grid_dirty[q] = true;
        }

        // Velocity scaling: 0-15 level -> 0-127 MIDI velocity
        uint8_t vel = level * 8;
        if (vel > 127) vel = 127;

        // Send to all three layouts so Launchpads and generic grids light up.
        
        // 1. Launchpad Keypad layout
        uint8_t lp_note = (8 - y) * 10 + x + 1;
        push_note_message(lp_note, vel, out_messages);

        // 2. Launchpad Chromatic Drum Rack layout
        uint8_t drum_note = 36 + y * 8 + x;
        push_note_message(drum_note, vel, out_messages);

        // 3. Generic Chromatic layout
        uint8_t gen_note = y * 8 + x;
        push_note_message(gen_note, vel, out_messages);
    }

    void push_note_message(uint8_t note, uint8_t vel, std::vector<rack::midi::Message>& out_messages) {
        rack::midi::Message msg;
        msg.bytes.resize(3);
        if (vel > 0) {
            msg.bytes[0] = 0x90; // Note On
            msg.bytes[1] = note;
            msg.bytes[2] = vel;
        } else {
            msg.bytes[0] = 0x80; // Note Off
            msg.bytes[1] = note;
            msg.bytes[2] = 0;
        }
        out_messages.push_back(msg);
    }
};

void translate_midi_to_monome_grid(const uint8_t* msg_bytes, size_t msg_size) {
    if (t_instance && t_instance->sample_mgr_active) return;
    if (msg_size < 3) return;
    uint8_t status = msg_bytes[0];
    uint8_t type = status & 0xF0;
    uint8_t note = msg_bytes[1];
    uint8_t vel = msg_bytes[2];

    bool is_note_on = (type == 0x90 && vel > 0);
    bool is_note_off = (type == 0x80 || (type == 0x90 && vel == 0));

    if (!is_note_on && !is_note_off) return;

    int x = -1, y = -1;

    // 1. Launchpad Keypad layout (row 0: 81-88, row 7: 11-18)
    if (note >= 11 && note <= 88 && (note % 10) >= 1 && (note % 10) <= 8) {
        y = 8 - (note / 10);
        x = (note % 10) - 1;
    }
    // 2. Launchpad Chromatic Drum Rack layout (36 to 99)
    else if (note >= 36 && note <= 99) {
        y = (note - 36) / 8;
        x = (note - 36) % 8;
    }
    // 3. Generic Chromatic layout (0 to 63)
    else if (note >= 0 && note <= 63) {
        y = note / 8;
        x = note % 8;
    }

    if (x >= 0 && x < 16 && y >= 0 && y < 16) {
        // MEXT grid key packet only — legacy packets corrupt the mext RX stream
        // because 0x00/0x10 headers cause the mext parser to expect more bytes,
        // swallowing subsequent real key events as data.
        uint8_t mext_pkt[3];
        mext_pkt[0] = is_note_on ? 0x21 : 0x20;
        mext_pkt[1] = (uint8_t)x;
        mext_pkt[2] = (uint8_t)y;
        g_serial_rx_byte_queue.push(mext_pkt, 3);
    }
}

struct WorkshopComputer : Module, IGridConsumer, IComputerModule {
    enum ParamIds {
        KNOB_MAIN_PARAM,
        KNOB_X_PARAM,
        KNOB_Y_PARAM,
        SWITCH_Z_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        AUDIO_1_INPUT,
        AUDIO_2_INPUT,
        CV_1_INPUT,
        CV_2_INPUT,
        PULSE_1_INPUT,
        PULSE_2_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        AUDIO_1_OUTPUT,
        AUDIO_2_OUTPUT,
        CV_1_OUTPUT,
        CV_2_OUTPUT,
        PULSE_1_OUTPUT,
        PULSE_2_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        LED_0_LIGHT,
        LED_1_LIGHT,
        LED_2_LIGHT,
        LED_3_LIGHT,
        LED_4_LIGHT,
        LED_5_LIGHT,
        NUM_LIGHTS
    };

    // ── Per-instance state ────────────────────────────────────────────────
    CardGlobals card_globals;   // owns ALL card state (FIFOs, flash, LEDs …)
    BridgeAudioPort bridge_audio_port_inst;

    void* card_lib_handle = nullptr;
    std::string loaded_temp_path = "";

    void unload_card_library() {
        if (card_lib_handle) {
            #ifdef _WIN32
                FreeLibrary((HMODULE)card_lib_handle);
            #else
                dlclose(card_lib_handle);
            #endif
            card_lib_handle = nullptr;
        }
        if (!loaded_temp_path.empty()) {
            #ifdef _WIN32
                _unlink(loaded_temp_path.c_str());
            #else
                unlink(loaded_temp_path.c_str());
            #endif
            loaded_temp_path = "";
        }
    }

    int active_card_idx = 0;
    std::thread background_thread;
    int utility_indices[2] = {0, 0};

    // Switch page navigation from WorkshopToggleSwitch widget
    int pending_page_direction = 0;

    // ── IComputerModule interface (shared with WorkshopSystem) ───────────────
    void change_card(int idx) override { change_card_impl(idx, true); }
    int  get_active_card_idx() const override { return active_card_idx; }
    std::string get_active_card_id() const override { return card_globals.active_card_id_str; }
    int  get_utility_index(int slot) const override { return utility_indices[slot & 1]; }
    void set_pending_page_direction(int dir) override { pending_page_direction = dir; }


    rack::dsp::SchmittTrigger pulse1Trigger;
    rack::dsp::SchmittTrigger pulse2Trigger;

    rack::midi::InputQueue midiInput;
    rack::midi::Output midiOutput;
    MidiTxParser txParser;
    MonomeSerialParser serialParser;

    // Outgoing Web UI WebSocket queues
    ThreadSafeMessageQueue websocket_midi_tx_queue;
    ThreadSafeMessageQueue websocket_serial_tx_queue;

    Grid* connected_grid = nullptr;
    std::string last_grid_device_id = "";
    bool consumer_registered = false;

    // Try to (re-)register this consumer with the Monome bridge.
    // Safe to call multiple times — is a no-op if already registered.
    void ensure_monome_registered() {
        if (!consumer_registered && MonomeBridge::get().is_available()) {
            MonomeBridge::get().register_consumer(this);
            consumer_registered = true;
        }
    }

    void gridConnected(Grid* grid) override {
        connected_grid = grid;
        if (connected_grid) {
            card_globals.grid_connected_flag = true;
            if (card_globals.on_grid_connected_fn) {
                card_globals.on_grid_connected_fn(true);
            }

            if (card_globals.sample_mgr_active) {
                last_grid_device_id = connected_grid->getDevice().id;
                return;
            }

            // Flush any stale bytes from the RX queue before injecting.
            // Without this, bytes from a prior sample manager session or
            // a previous grid connection could corrupt the mext handshake
            // (or vice-versa: our discovery bytes could corrupt the sample manager).
            g_serial_rx_byte_queue.clear();

            // Inject a mext discovery response so drumdrum (and any other card using
            // the mext protocol) sets its grid_x/grid_y and starts rendering.
            // Real grids send "0x03, cols, rows" in response to the size query (0x05).
            // We synthesise it here from the VCV Grid's device dimensions.
            const MonomeDevice& dev = connected_grid->getDevice();
            uint8_t grid_cols = (dev.width  > 0 && dev.width  <= 16) ? (uint8_t)dev.width  : 16;
            uint8_t grid_rows = (dev.height > 0 && dev.height <= 16) ? (uint8_t)dev.height :  8;

            // mext size response: header 0x03, cols, rows
            uint8_t size_resp[3] = { 0x03, grid_cols, grid_rows };
            g_serial_rx_byte_queue.push(size_resp, 3);


            // Push the current local LED state to the newly connected grid
            for (int q = 0; q < 4; q++) {
                int xo = (q & 1) ? 8 : 0;
                int yo = (q & 2) ? 8 : 0;
                uint8_t quad_leds[64];
                for (int r = 0; r < 8; r++) {
                    for (int c = 0; c < 8; c++) {
                        quad_leds[r * 8 + c] = serialParser.grid_leds[yo + r][xo + c];
                    }
                }
                connected_grid->updateQuadrant(xo, yo, quad_leds);
            }
            last_grid_device_id = connected_grid->getDevice().id;
        }
    }

    void gridDisconnected(bool ownerChanged) override {
        card_globals.grid_connected_flag = false;
        card_globals.sample_mgr_active = false;
        if (card_globals.on_grid_connected_fn) {
            card_globals.on_grid_connected_fn(false);
        }
        connected_grid = nullptr;
    }

    std::string gridGetCurrentDeviceId() override {
        if (connected_grid) {
            return connected_grid->getDevice().id;
        }
        return "";
    }

    std::string gridGetLastDeviceId(bool owned) override {
        return last_grid_device_id;
    }

    void setLastDeviceId(std::string id) override {
        last_grid_device_id = id;
    }

    void gridButtonEvent(int x, int y, bool state) override {
        if (card_globals.sample_mgr_active) {
            return;
        }
        if (x >= 0 && x < 16 && y >= 0 && y < 16) {
            // MEXT key packet only — do NOT send legacy packets into this queue.
            // Legacy 0x00/0x10 bytes are treated as system command headers by the
            // mext parser and cause it to consume subsequent events as data bytes.
            uint8_t mext_pkt[3];
            mext_pkt[0] = state ? 0x21 : 0x20;
            mext_pkt[1] = (uint8_t)x;
            mext_pkt[2] = (uint8_t)y;
            g_serial_rx_byte_queue.push(mext_pkt, 3);
        }
    }

    void encDeltaEvent(int n, int d) override {}

    Grid* gridGetDevice() override {
        return connected_grid;
    }

    WorkshopComputer() {
        card_globals.bridge_audio_port = &bridge_audio_port_inst;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        configParam(KNOB_MAIN_PARAM, 0.f, 1.f, 0.5f, "Main Parameter");
        configParam(KNOB_X_PARAM, 0.f, 1.f, 0.5f, "X Parameter");
        configParam(KNOB_Y_PARAM, 0.f, 1.f, 0.5f, "Y Parameter");
        configParam(SWITCH_Z_PARAM, 0.f, 2.f, 1.f, "Switch Position (Down/Middle/Up)");

        configInput(AUDIO_1_INPUT, "Audio 1 Input");
        configInput(AUDIO_2_INPUT, "Audio 2 Input");
        configInput(CV_1_INPUT, "CV 1 Input");
        configInput(CV_2_INPUT, "CV 2 Input");
        configInput(PULSE_1_INPUT, "Pulse 1 Input");
        configInput(PULSE_2_INPUT, "Pulse 2 Input");

        configOutput(AUDIO_1_OUTPUT, "Audio 1 Output");
        configOutput(AUDIO_2_OUTPUT, "Audio 2 Output");
        configOutput(CV_1_OUTPUT, "CV 1 Output");
        configOutput(CV_2_OUTPUT, "CV 2 Output");
        configOutput(PULSE_1_OUTPUT, "Pulse 1 Output");
        configOutput(PULSE_2_OUTPUT, "Pulse 2 Output");

        // Register cards
        register_all_cards();

        // Start with no card loaded
        change_card_impl(-1);

        // Register instance and start web server
        {
            std::lock_guard<std::mutex> lock(g_instances_mutex);
            g_instances.insert(this);
            start_web_server();
        }

        // Register with Monome Bridge if available (may retry later if not yet loaded)
        ensure_monome_registered();
    }

    ~WorkshopComputer() {
        if (MonomeBridge::get().is_available()) {
            MonomeBridge::get().disconnect(this);
            MonomeBridge::get().deregister_consumer(this);
        }

        stop_card();

        // Deregister instance and stop web server if last instance
        {
            std::lock_guard<std::mutex> lock(g_instances_mutex);
            g_instances.erase(this);
            if (g_instances.empty()) {
                stop_web_server();
            }
        }
    }

    bool load_file_to_flash(const std::string& filepath) {
        FILE* f = fopen(filepath.c_str(), "rb");
        if (!f) return false;

        // Check if it's a UF2 file by reading the first block
        uint32_t header[2];
        if (fread(header, sizeof(uint32_t), 2, f) != 2) {
            fclose(f);
            return false;
        }
        fseek(f, 0, SEEK_SET);

        bool is_uf2 = (header[0] == 0x0A324655 && header[1] == 0x9E5D5157);

        if (is_uf2) {
            // Clear flash first so we don't merge old and new contents
            memset(card_globals.g_flash_memory_val, 0xFF, PICO_FLASH_SIZE_BYTES);

            uint8_t block[512];
            while (fread(block, 1, 512, f) == 512) {
                uint32_t magic_start_0 = *(uint32_t*)&block[0];
                uint32_t magic_start_1 = *(uint32_t*)&block[4];
                uint32_t magic_end = *(uint32_t*)&block[508];
                if (magic_start_0 != 0x0A324655 || magic_start_1 != 0x9E5D5157 || magic_end != 0x0AB16F30) {
                    continue;
                }
                uint32_t target_addr = *(uint32_t*)&block[12];
                uint32_t payload_size = *(uint32_t*)&block[16];

                if (target_addr >= 0x10000000 && target_addr < 0x10000000 + PICO_FLASH_SIZE_BYTES) {
                    uint32_t offset = target_addr - 0x10000000;
                    if (offset + payload_size <= PICO_FLASH_SIZE_BYTES) {
                        memcpy(card_globals.g_flash_memory_val + offset, &block[32], payload_size);
                    }
                }
            }
        } else {
            // Raw binary/SoundFont file
            size_t n = fread(card_globals.g_flash_memory_val, 1, PICO_FLASH_SIZE_BYTES, f);
            if (n < PICO_FLASH_SIZE_BYTES) {
                memset(card_globals.g_flash_memory_val + n, 0xFF, PICO_FLASH_SIZE_BYTES - n);
            }
        }

        fclose(f);
        
        // Save to disk
        CardGlobals* old_instance = t_instance;
        t_instance = &card_globals;
        save_flash_to_disk();
        t_instance = old_instance;
        return true;
    }

    void stop_card() {
        card_globals.block_audio_processing.store(true, std::memory_order_release);
        while (card_globals.in_audio_callback.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        t_instance = &card_globals;

        // First: null out card_ptr so the audio thread IMMEDIATELY stops calling
        // ProcessSample. The audio thread checks card_ptr before every call.
        if (card_globals.active_card_id_str == "usb_audio_bridge" && card_globals.bridge_audio_port) {
            card_globals.bridge_audio_port->setDeviceId(-1);
        }
        card_globals.card_ptr = nullptr;
        card_globals.g_dsp_ready = false;
        ComputerCard::thisptr = nullptr;
        g_current_card_ptr = nullptr;

        card_globals.set_thread_globals_fn = nullptr;
        card_globals.set_core1_thread_fn = nullptr;
        card_globals.save_flash_to_disk_fn = nullptr;
        card_globals.multicore_launch_core1_fn = nullptr;

        // Now signal cancellation and wake blocked threads
        g_cancellation_requested = true;
        try { g_fifo_1_to_0.push(0); } catch (...) {}
        try { g_fifo_0_to_1.push(0); } catch (...) {}
        g_synth_need_render.store(true);
        g_synth_cv.notify_all();

        if (background_thread.joinable()) {
            try { background_thread.join(); } catch (...) {}
        }
        if (card_globals.g_core1_thread_val.joinable()) {
            try { card_globals.g_core1_thread_val.join(); } catch (...) {}
        }

        g_fifo_1_to_0.clear();
        g_fifo_0_to_1.clear();
        g_midi_rx_packet_queue.clear();
        g_midi_tx_byte_queue.clear();

        // Clean output and LED state
        for (int i = 0; i < 2; i++) {
            g_audio_out[i] = 0.f;
            g_cv_out[i] = 0.f;
            g_pulse_out[i] = false;
        }
        for (int i = 0; i < 6; i++) {
            g_led_brightness[i] = 0.f;
        }

        g_synth_need_render.store(false);
        g_cancellation_requested = false;

        unload_card_library();
    }

    void autoConnectBridge() {
        // 1. Auto-connect Audio Device
        if (bridge_audio_port_inst.getDriverId() == -1 || bridge_audio_port_inst.getDeviceId() == -1) {
            for (int driverId : rack::audio::getDriverIds()) {
                rack::audio::Driver* driver = rack::audio::getDriver(driverId);
                if (!driver) continue;
                
                bool found = false;
                for (int deviceId : driver->getDeviceIds()) {
                    std::string name = driver->getDeviceName(deviceId);
                    std::string nameLower = name;
                    for (auto &c : nameLower) c = ::tolower(c);
                    if (nameLower.find("workshop") != std::string::npos || nameLower.find("usb audio") != std::string::npos) {
                        bridge_audio_port_inst.setDriverId(driverId);
                        bridge_audio_port_inst.setDeviceId(deviceId);
                        // Default to 48kHz if available
                        for (float sr : bridge_audio_port_inst.getSampleRates()) {
                            if (sr == 48000.f) {
                                bridge_audio_port_inst.setSampleRate(48000.f);
                                break;
                            }
                        }
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        // 2. Auto-connect MIDI Input Device
        if (midiInput.getDriverId() == -1 || midiInput.getDeviceId() == -1) {
            for (int driverId : rack::midi::getDriverIds()) {
                rack::midi::Driver* driver = rack::midi::getDriver(driverId);
                if (!driver) continue;
                
                bool found = false;
                for (int deviceId : driver->getInputDeviceIds()) {
                    std::string name = driver->getInputDeviceName(deviceId);
                    std::string nameLower = name;
                    for (auto &c : nameLower) c = ::tolower(c);
                    if (nameLower.find("workshop") != std::string::npos || nameLower.find("usb audio") != std::string::npos) {
                        midiInput.setDriverId(driverId);
                        midiInput.setDeviceId(deviceId);
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        // 3. Auto-connect MIDI Output Device
        if (midiOutput.getDriverId() == -1 || midiOutput.getDeviceId() == -1) {
            for (int driverId : rack::midi::getDriverIds()) {
                rack::midi::Driver* driver = rack::midi::getDriver(driverId);
                if (!driver) continue;
                
                bool found = false;
                for (int deviceId : driver->getOutputDeviceIds()) {
                    std::string name = driver->getOutputDeviceName(deviceId);
                    std::string nameLower = name;
                    for (auto &c : nameLower) c = ::tolower(c);
                    if (nameLower.find("workshop") != std::string::npos || nameLower.find("usb audio") != std::string::npos) {
                        midiOutput.setDriverId(driverId);
                        midiOutput.setDeviceId(deviceId);
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }
    }

    bool activeCardNeedsMidi() {
        if (card_globals.active_card_id_str.empty()) return false;
        return (card_globals.active_card_id_str == "simple_midi" || 
                card_globals.active_card_id_str == "duo_midi" || 
                card_globals.active_card_id_str == "usb_audio_bridge" ||
                card_globals.active_card_id_str == "blackbird" ||
                card_globals.active_card_id_str == "krell" ||
                card_globals.active_card_id_str == "rompler" ||
                card_globals.active_card_id_str == "computer_grids" ||
                card_globals.active_card_id_str == "reverb" ||
                card_globals.active_card_id_str == "flux" ||
                card_globals.active_card_id_str == "twists" ||
                card_globals.active_card_id_str == "modes");
    }

    bool activeCardNeedsGrid() {
        if (card_globals.active_card_id_str.empty()) return false;
        return (card_globals.active_card_id_str == "blackbird" || 
                card_globals.active_card_id_str == "krell" || 
                card_globals.active_card_id_str == "duo_midi" || 
                card_globals.active_card_id_str == "mlrws" ||
                card_globals.active_card_id_str == "drumdrum");
    }

    bool activeCardNeedsUsbAudio() {
        if (card_globals.active_card_id_str.empty()) return false;
        return (card_globals.active_card_id_str == "usb_audio_bridge");
    }

    void change_card_impl(int new_idx, bool load_flash = true) {
        stop_card();
        t_instance = &card_globals;

        card_globals.grid_connected_flag = (connected_grid != nullptr);
        card_globals.sample_mgr_active = false;
        card_globals.on_grid_connected_fn = nullptr;

        active_card_idx = new_idx;
        if (active_card_idx < 0 || active_card_idx >= (int)g_card_registry.size()) {
            active_card_idx = -1;
            g_active_card_id = "";
            return;
        }

        g_active_card_id = g_card_registry[active_card_idx].id;

        for (int i = 0; i < 6; i++) g_led_brightness[i] = 0.f;

        if (load_flash) {
            load_flash_from_disk();
        }

        if (g_active_card_id == "utility_pair") {
            g_flash_memory[2093056] = utility_indices[0];
            g_flash_memory[2093057] = utility_indices[1];
        }

        g_midi_rx_packet_queue.clear();
        g_midi_tx_byte_queue.clear();
        g_synth_need_render.store(false);
        g_cancellation_requested = false;

        // Set callback pointers for dynamic libraries
        card_globals.save_flash_to_disk_fn = save_flash_to_disk;
        card_globals.multicore_launch_core1_fn = host_multicore_launch_core1;

        // Determine library suffix and get plugin-relative path
        #ifdef _WIN32
            std::string lib_name = "libcard_" + g_active_card_id + ".dll";
        #elif defined(__APPLE__)
            std::string lib_name = "libcard_" + g_active_card_id + ".dylib";
        #else
            std::string lib_name = "libcard_" + g_active_card_id + ".so";
        #endif
        std::string src_path = asset::plugin(pluginInstance, "res/cards/" + lib_name);

        // Copy dynamic library to a unique temp name to isolate static variables across instances
        std::string tmp_dir = asset::plugin(pluginInstance, "tmp");
        #ifdef _WIN32
            _mkdir(tmp_dir.c_str());
        #else
            mkdir(tmp_dir.c_str(), 0777);
        #endif

        char temp_name[512];
        #ifdef _WIN32
            snprintf(temp_name, sizeof(temp_name), "%s\\libcard_%s_%p.dll", 
                     tmp_dir.c_str(), g_active_card_id.c_str(), this);
        #elif defined(__APPLE__)
            snprintf(temp_name, sizeof(temp_name), "%s/libcard_%s_%p.dylib", 
                     tmp_dir.c_str(), g_active_card_id.c_str(), this);
        #else
            snprintf(temp_name, sizeof(temp_name), "%s/libcard_%s_%p.so", 
                     tmp_dir.c_str(), g_active_card_id.c_str(), this);
        #endif
        loaded_temp_path = temp_name;

        {
            std::ifstream src(src_path, std::ios::binary);
            std::ofstream dst(loaded_temp_path, std::ios::binary);
            if (src && dst) {
                dst << src.rdbuf();
            } else {
                DEBUG("Failed to copy dynamic library from %s to %s", src_path.c_str(), loaded_temp_path.c_str());
            }
        }

        #ifdef __APPLE__
            // On macOS, dyld uses the LC_ID_DYLIB install name as the unique key to cache/deduplicate dylibs.
            // Since we copied the dylib from a shared source, the copy still has the same install name.
            // We must update the install name to be unique (we use the temp file's own path) so dyld loads it as a separate image
            // and isolates static/global variables between multiple module instances.
            std::string cmd = "install_name_tool -id \"" + loaded_temp_path + "\" \"" + loaded_temp_path + "\"";
            int ret = std::system(cmd.c_str());
            if (ret != 0) {
                DEBUG("install_name_tool failed with code %d", ret);
            }
        #endif

        // Load the isolated copy
        #ifdef _WIN32
            card_lib_handle = LoadLibraryA(loaded_temp_path.c_str());
        #else
            card_lib_handle = dlopen(loaded_temp_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        #endif

        if (!card_lib_handle) {
            #ifdef _WIN32
                DEBUG("Failed to load library: %lu", GetLastError());
            #else
                DEBUG("Failed to load library: %s", dlerror());
            #endif
            return;
        }

        // Resolve export functions
        #ifdef _WIN32
            card_globals.set_thread_globals_fn = (void(*)(CardGlobals*)) GetProcAddress((HMODULE)card_lib_handle, "set_thread_globals");
            card_globals.set_core1_thread_fn = (void(*)(bool)) GetProcAddress((HMODULE)card_lib_handle, "set_core1_thread");
            auto run_card_fn = (void(*)()) GetProcAddress((HMODULE)card_lib_handle, "run_card");
        #else
            card_globals.set_thread_globals_fn = (void(*)(CardGlobals*)) dlsym(card_lib_handle, "set_thread_globals");
            card_globals.set_core1_thread_fn = (void(*)(bool)) dlsym(card_lib_handle, "set_core1_thread");
            auto run_card_fn = (void(*)()) dlsym(card_lib_handle, "run_card");
        #endif

        // Capture static card pointer immediately in the loading thread
        if (card_globals.set_thread_globals_fn) {
            card_globals.set_thread_globals_fn(&card_globals);
        }

        CardGlobals* inst = &card_globals;
        background_thread = std::thread([inst, run_card_fn]() {
            t_instance = inst;
            is_core1_thread = false;
            if (inst->set_thread_globals_fn) {
                inst->set_thread_globals_fn(inst);
            }
            if (inst->set_core1_thread_fn) {
                inst->set_core1_thread_fn(false);
            }
            try {
                if (run_card_fn) run_card_fn();
            } catch (const ThreadExitException&) {}
        });

        // Wait a bit longer for card to fully initialize (set card_ptr, launch core1, etc.)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        if (connected_grid) {
            uint8_t dummy = 0;
            g_serial_rx_byte_queue.push(&dummy, 1);
        }
        
        card_globals.block_audio_processing.store(false, std::memory_order_release);
        if (g_active_card_id == "usb_audio_bridge") {
            autoConnectBridge();
        }
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override {
        pulse1Trigger.reset();
        pulse2Trigger.reset();
    }

    void onReset() override {
        utility_indices[0] = 0;
        utility_indices[1] = 0;
        change_card_impl(0);
    }

    void process(const ProcessArgs& args) override {
        if (card_globals.block_audio_processing.load(std::memory_order_acquire)) {
            return;
        }
        card_globals.in_audio_callback.store(true, std::memory_order_release);
        if (card_globals.block_audio_processing.load(std::memory_order_acquire)) {
            card_globals.in_audio_callback.store(false, std::memory_order_release);
            return;
        }

        t_instance = &card_globals;
        if (card_globals.set_thread_globals_fn) {
            card_globals.set_thread_globals_fn(&card_globals);
        }

        // 1. Gather inputs
        g_knobs[0] = params[KNOB_MAIN_PARAM].getValue();
        g_knobs[1] = params[KNOB_X_PARAM].getValue();
        g_knobs[2] = params[KNOB_Y_PARAM].getValue();
        g_switch   = (int)params[SWITCH_Z_PARAM].getValue();

        g_audio_in[0] = inputs[AUDIO_1_INPUT].getVoltage();
        g_audio_in[1] = inputs[AUDIO_2_INPUT].getVoltage();
        g_cv_in[0]    = inputs[CV_1_INPUT].getVoltage();
        g_cv_in[1]    = inputs[CV_2_INPUT].getVoltage();

        pulse1Trigger.process(inputs[PULSE_1_INPUT].getVoltage(), 0.1f, 1.0f);
        g_pulse_in[0] = pulse1Trigger.isHigh();
        pulse2Trigger.process(inputs[PULSE_2_INPUT].getVoltage(), 0.1f, 1.0f);
        g_pulse_in[1] = pulse2Trigger.isHigh();

        g_input_connected[0] = inputs[AUDIO_1_INPUT].isConnected();
        g_input_connected[1] = inputs[AUDIO_2_INPUT].isConnected();
        g_input_connected[2] = inputs[CV_1_INPUT].isConnected();
        g_input_connected[3] = inputs[CV_2_INPUT].isConnected();
        g_input_connected[4] = inputs[PULSE_1_INPUT].isConnected();
        g_input_connected[5] = inputs[PULSE_2_INPUT].isConnected();

        // 1.5. MIDI Input
        rack::midi::Message rx_msg;
        while (midiInput.tryPop(&rx_msg, args.frame)) {
            push_midi_to_rx_queue(rx_msg.bytes.data(), rx_msg.bytes.size());
            translate_midi_to_monome_grid(rx_msg.bytes.data(), rx_msg.bytes.size());
        }

        // 2. Drive the card's DSP callback
        ComputerCard* card = card_globals.card_ptr;
        if (card && card_globals.g_dsp_ready.load(std::memory_order_relaxed)) {
            card_globals.dsp_phase += card_globals.expected_sample_rate / args.sampleRate;
            while (card_globals.dsp_phase >= 1.0) {
                card->update_inputs();
                card->ProcessSample();
                card_globals.dsp_phase -= 1.0;
            }
        }

        // 2.5. MIDI Output
        uint8_t tx_byte;
        std::vector<rack::midi::Message> tx_msgs;
        while (g_midi_tx_byte_queue.pop(tx_byte))
            txParser.parse_byte(tx_byte, tx_msgs);
        for (const auto& tx_msg : tx_msgs) {
            midiOutput.sendMessage(tx_msg);
            websocket_midi_tx_queue.push(tx_msg.bytes);
        }

        // 2.7. Serial Output
        uint8_t serial_tx_byte;
        std::vector<uint8_t> serial_bytes;
        std::vector<rack::midi::Message> grid_midi_msgs;

        // Keep target_grid synced so the parser dispatches LED updates directly
        serialParser.target_grid = connected_grid;

        while (g_serial_tx_byte_queue.pop(serial_tx_byte)) {
            serial_bytes.push_back(serial_tx_byte);
            serialParser.parse_byte(serial_tx_byte, grid_midi_msgs);

            // Auto-respond to mext size queries (0x05) when a VCV grid is connected.
            // Cards send this periodically until they get a response.
            // On real hardware the grid answers; we synthesise the response here.
            if (serial_tx_byte == 0x05 && connected_grid && !card_globals.sample_mgr_active) {
                const MonomeDevice& dev = connected_grid->getDevice();
                uint8_t cols = (dev.width  > 0 && dev.width  <= 16) ? (uint8_t)dev.width  : 16;
                uint8_t rows = (dev.height > 0 && dev.height <= 16) ? (uint8_t)dev.height :  8;
                uint8_t size_resp[3] = { 0x03, cols, rows };
                g_serial_rx_byte_queue.push(size_resp, 3);
            }
        }
        if (!serial_bytes.empty()) {
            websocket_serial_tx_queue.push(serial_bytes);
        }
        for (const auto& tx_msg : grid_midi_msgs) {
            midiOutput.sendMessage(tx_msg);
        }

        // 3. Write outputs
        outputs[AUDIO_1_OUTPUT].setVoltage(g_audio_out[0]);
        outputs[AUDIO_2_OUTPUT].setVoltage(g_audio_out[1]);
        outputs[CV_1_OUTPUT].setVoltage(g_cv_out[0]);
        outputs[CV_2_OUTPUT].setVoltage(g_cv_out[1]);
        outputs[PULSE_1_OUTPUT].setVoltage(g_pulse_out[0] ? 5.f : 0.f);
        outputs[PULSE_2_OUTPUT].setVoltage(g_pulse_out[1] ? 5.f : 0.f);

        // 4. LEDs
        for (int i = 0; i < 6; i++)
            lights[LED_0_LIGHT + i].setBrightness(g_led_brightness[i]);

        card_globals.in_audio_callback.store(false, std::memory_order_release);
    }

    json_t* dataToJson() override {
        t_instance = &card_globals;
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "active_card_idx", json_integer(active_card_idx));
        json_object_set_new(rootJ, "active_card_id", json_string(card_globals.active_card_id_str.c_str()));
        if (active_card_idx != -1) {
            std::string flash_hex = hex_encode(card_globals.g_flash_memory_val, PICO_FLASH_SIZE_BYTES);
            json_object_set_new(rootJ, "flash_memory_hex", json_string(flash_hex.c_str()));
        }
        json_object_set_new(rootJ, "left_utility", json_integer(utility_indices[0]));
        json_object_set_new(rootJ, "right_utility", json_integer(utility_indices[1]));
        json_object_set_new(rootJ, "midi_input", midiInput.toJson());
        json_object_set_new(rootJ, "midi_output", midiOutput.toJson());
        json_object_set_new(rootJ, "last_grid_device_id", json_string(last_grid_device_id.c_str()));
        if (card_globals.bridge_audio_port) {
            json_object_set_new(rootJ, "bridge_audio_port", card_globals.bridge_audio_port->toJson());
        }
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        t_instance = &card_globals;
        json_t* idJ = json_object_get(rootJ, "active_card_id");
        if (idJ) {
            std::string saved_id = json_string_value(idJ);
            active_card_idx = -1;
            for (size_t i = 0; i < g_card_registry.size(); i++) {
                if (g_card_registry[i].id == saved_id) {
                    active_card_idx = (int)i;
                    break;
                }
            }
        } else {
            json_t* acJ = json_object_get(rootJ, "active_card_idx");
            if (acJ) active_card_idx = json_integer_value(acJ);
        }
        
        json_t* luJ = json_object_get(rootJ, "left_utility");
        if (luJ) utility_indices[0] = json_integer_value(luJ);
        
        json_t* ruJ = json_object_get(rootJ, "right_utility");
        if (ruJ) utility_indices[1] = json_integer_value(ruJ);

        json_t* miJ = json_object_get(rootJ, "midi_input");
        if (miJ) midiInput.fromJson(miJ);

        json_t* moJ = json_object_get(rootJ, "midi_output");
        if (moJ) midiOutput.fromJson(moJ);

        bool has_json_flash = false;
        json_t* flashHexJ = json_object_get(rootJ, "flash_memory_hex");
        if (flashHexJ) {
            std::string flash_hex = json_string_value(flashHexJ);
            hex_decode(flash_hex, card_globals.g_flash_memory_val, PICO_FLASH_SIZE_BYTES);
            has_json_flash = true;
        }

        json_t* gridJ = json_object_get(rootJ, "last_grid_device_id");
        if (gridJ) {
            last_grid_device_id = json_string_value(gridJ);
        }

        change_card_impl(active_card_idx, !has_json_flash);

        json_t* bapJ = json_object_get(rootJ, "bridge_audio_port");
        if (bapJ && card_globals.bridge_audio_port) {
            card_globals.bridge_audio_port->fromJson(bapJ);
        }

        // Auto-connect to the saved grid if it exists and bridge is available
        if (MonomeBridge::get().is_available() && !last_grid_device_id.empty()) {
            auto grids = MonomeBridge::get().get_grids();
            for (Grid* grid : grids) {
                if (grid->getDevice().id == last_grid_device_id) {
                    MonomeBridge::get().connect(grid, this);
                    break;
                }
            }
        }
    }
};

// ── Shared Widget classes ────────────────────────────────────────────────────
// WorkshopLargeKnob, WorkshopSmallKnob, WorkshopComputerPort,
// WorkshopToggleSwitch, WorkshopResetButton, ProgramCardWidget
// are defined in shared/ComputerWidgets.hpp (included above).
// Both the standalone Workshop Computer and Workshop System use the same
// implementations via the IComputerModule interface.


struct WorkshopComputerWidget : ModuleWidget {
    WorkshopComputerWidget(WorkshopComputer* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/WorkshopComputer.svg")));

        auto getVec = [](float x, float y) -> Vec {
            return Vec(x * 1.019277f + 3.283006f, y * 1.019277f + 4.27722f);
        };

        // Black Screws positioned on left-side mounting slots (Eurorack style)
        addChild(createWidget<ScrewBlack>(Vec(0.f, 0.f)));
        addChild(createWidget<ScrewBlack>(Vec(0.f, 365.f)));

        // Large parameters knob
        addParam(createParamCentered<WorkshopLargeKnob>(getVec(55.881f, 54.847f), module, WorkshopComputer::KNOB_MAIN_PARAM));
        
        // Small knobs
        addParam(createParamCentered<WorkshopSmallKnob>(getVec(17.962f, 118.715f), module, WorkshopComputer::KNOB_X_PARAM));
        addParam(createParamCentered<WorkshopSmallKnob>(getVec(55.691f, 118.431f), module, WorkshopComputer::KNOB_Y_PARAM));
        
        // Switch Z (3-position hold-to-latch)
        if (module) {
            WorkshopToggleSwitch* sw = createParamCentered<WorkshopToggleSwitch>(Vec(97.842f, 121.513f), module, WorkshopComputer::SWITCH_Z_PARAM);
            sw->setComputerModule(module);
            addParam(sw);
        } else {
            addParam(createParamCentered<WorkshopToggleSwitch>(Vec(97.842f, 121.513f), module, WorkshopComputer::SWITCH_Z_PARAM));
        }

        // Visual cartridge program card (sits in the slot x=47.503, y=327.814, w=36.452)
        if (module) {
            ProgramCardWidget* cardWidget = new ProgramCardWidget();
            cardWidget->box.pos = Vec(43.759f, 286.027f);
            cardWidget->box.size = Vec(37.155f, 55.04f);
            cardWidget->setComputerModule(module);
            addChild(cardWidget);
        }

        // Virtual Reset Button (black circle next to the card slot)
        if (module) {
            WorkshopResetButton* resetButton = new WorkshopResetButton();
            resetButton->box.pos = getVec(99.712f - 5.669f, 332.802f - 5.669f);
            resetButton->box.size = Vec(11.338f * 1.019277f, 11.338f * 1.019277f);
            resetButton->setComputerModule(module);
            addChild(resetButton);
        }

        // 6 LEDs (2x3 Grid on bottom left)
        addChild(createLightCentered<MediumLight<RedLight>>(getVec(6.714f, 309.281f), module, WorkshopComputer::LED_0_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(getVec(19.952f, 309.309f), module, WorkshopComputer::LED_1_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(getVec(6.686f, 322.263f), module, WorkshopComputer::LED_2_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(getVec(19.952f, 322.235f), module, WorkshopComputer::LED_3_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(getVec(6.714f, 335.189f), module, WorkshopComputer::LED_4_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(getVec(19.924f, 335.189f), module, WorkshopComputer::LED_5_LIGHT));

        // Jacks Row 1 (Audio): y = 167.873
        addInput(createInputCentered<WorkshopComputerPort>(getVec(12.409f, 167.873f), module, WorkshopComputer::AUDIO_1_INPUT));
        addInput(createInputCentered<WorkshopComputerPort>(getVec(41.209f, 167.873f), module, WorkshopComputer::AUDIO_2_INPUT));
        addOutput(createOutputCentered<WorkshopComputerPort>(getVec(70.008f, 167.873f), module, WorkshopComputer::AUDIO_1_OUTPUT));
        addOutput(createOutputCentered<WorkshopComputerPort>(getVec(98.808f, 167.873f), module, WorkshopComputer::AUDIO_2_OUTPUT));

        // Jacks Row 2 (CV): y = 207.473
        addInput(createInputCentered<WorkshopComputerPort>(getVec(12.409f, 207.473f), module, WorkshopComputer::CV_1_INPUT));
        addInput(createInputCentered<WorkshopComputerPort>(getVec(41.209f, 207.473f), module, WorkshopComputer::CV_2_INPUT));
        addOutput(createOutputCentered<WorkshopComputerPort>(getVec(70.008f, 207.473f), module, WorkshopComputer::CV_1_OUTPUT));
        addOutput(createOutputCentered<WorkshopComputerPort>(getVec(98.808f, 207.473f), module, WorkshopComputer::CV_2_OUTPUT));

        // Jacks Row 3 (Pulses)
        addInput(createInputCentered<WorkshopComputerPort>(getVec(12.409f, 247.130f), module, WorkshopComputer::PULSE_1_INPUT));
        addInput(createInputCentered<WorkshopComputerPort>(getVec(41.205f, 247.035f), module, WorkshopComputer::PULSE_2_INPUT));
        addOutput(createOutputCentered<WorkshopComputerPort>(getVec(70.086f, 247.054f), module, WorkshopComputer::PULSE_1_OUTPUT));
        addOutput(createOutputCentered<WorkshopComputerPort>(getVec(98.854f, 246.997f), module, WorkshopComputer::PULSE_2_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        WorkshopComputer* module = dynamic_cast<WorkshopComputer*>(this->module);
        if (!module) return;

        if (module->activeCardNeedsMidi()) {
            menu->addChild(new MenuSeparator());

            struct MidiSubmenuItem : MenuItem {
                WorkshopComputer* module;
                Menu* createChildMenu() override {
                    Menu* menu = new Menu();

                    struct MidiInputSubmenuItem : MenuItem {
                        WorkshopComputer* module;
                        Menu* createChildMenu() override {
                            Menu* menu = new Menu();
                            appendMidiMenu(menu, &module->midiInput);
                            return menu;
                        }
                    };
                    MidiInputSubmenuItem* midiInMenu = new MidiInputSubmenuItem();
                    midiInMenu->text = "MIDI Input Settings";
                    midiInMenu->module = module;
                    midiInMenu->rightText = "➔";
                    menu->addChild(midiInMenu);

                    struct MidiOutputSubmenuItem : MenuItem {
                        WorkshopComputer* module;
                        Menu* createChildMenu() override {
                            Menu* menu = new Menu();
                            appendMidiMenu(menu, &module->midiOutput);
                            return menu;
                        }
                    };
                    MidiOutputSubmenuItem* midiOutMenu = new MidiOutputSubmenuItem();
                    midiOutMenu->text = "MIDI Output Settings";
                    midiOutMenu->module = module;
                    midiOutMenu->rightText = "➔";
                    menu->addChild(midiOutMenu);

                    return menu;
                }
            };
            MidiSubmenuItem* midiMenu = new MidiSubmenuItem();
            midiMenu->text = "MIDI Settings";
            midiMenu->module = module;
            midiMenu->rightText = "➔";
            menu->addChild(midiMenu);
        }

        if (module->activeCardNeedsUsbAudio() && module->card_globals.bridge_audio_port) {
            menu->addChild(new MenuSeparator());

            struct UsbAudioSubmenuItem : MenuItem {
                WorkshopComputer* module;
                Menu* createChildMenu() override {
                    Menu* menu = new Menu();
                    appendAudioMenu(menu, module->card_globals.bridge_audio_port);
                    return menu;
                }
            };
            UsbAudioSubmenuItem* audioMenu = new UsbAudioSubmenuItem();
            audioMenu->text = "USB Audio Settings";
            audioMenu->module = module;
            audioMenu->rightText = "➔";
            menu->addChild(audioMenu);
        }

        if (module->activeCardNeedsGrid()) {
            // Monome Grid Connection Menu – retry registration in case monome loaded after us
            module->ensure_monome_registered();
            if (MonomeBridge::get().is_available()) {
                menu->addChild(new MenuSeparator());

                struct MonomeGridSubmenuItem : MenuItem {
                    WorkshopComputer* module;
                    Menu* createChildMenu() override {
                        Menu* menu = new Menu();
                        auto grids = MonomeBridge::get().get_grids();
                        if (grids.empty()) {
                            menu->addChild(createMenuLabel("(no hardware or virtual grids found)"));
                        } else {
                            for (Grid* grid : grids) {
                                struct GridConnectItem : MenuItem {
                                    WorkshopComputer* module;
                                    Grid* grid;
                                    void onAction(const event::Action& e) override {
                                        if (module->connected_grid == grid) {
                                            MonomeBridge::get().disconnect(module);
                                        } else {
                                            MonomeBridge::get().connect(grid, module);
                                        }
                                    }
                                };

                                GridConnectItem* item = new GridConnectItem();
                                item->module = module;
                                item->grid = grid;
                                
                                std::string label = grid->getDevice().type + " (" + grid->getDevice().id + ")";
                                item->text = label;
                                if (module->connected_grid == grid) {
                                    item->rightText = "✔";
                                }
                                menu->addChild(item);
                            }
                        }
                        return menu;
                    }
                };
                MonomeGridSubmenuItem* gridMenu = new MonomeGridSubmenuItem();
                gridMenu->text = "Monome Grid Settings";
                gridMenu->module = module;
                gridMenu->rightText = "➔";
                menu->addChild(gridMenu);
            }
        }

        // Open Card Manager context menu item
        if (module && !module->card_globals.active_card_id_str.empty()) {
            std::string active_id = module->card_globals.active_card_id_str;
            if (active_id == "krell" || active_id == "duo_midi") {
                active_id = "blackbird";
            }
            std::string manager_file = "";
            std::vector<std::string> candidates = {
                "index.html",
                "editor.html",
                active_id + "_manager.html",
                active_id + ".html"
            };
            
            bool found = false;
             for (const auto& candidate : candidates) {
                std::string path = asset::plugin(pluginInstance, "res/web/" + active_id + "/" + candidate);
                std::ifstream f(path);
                if (f.good()) {
                    f.close();
                    manager_file = candidate;
                    found = true;
                    break;
                }
                // Try under dist/
                std::string dist_path = asset::plugin(pluginInstance, "res/web/" + active_id + "/dist/" + candidate);
                std::ifstream f_dist(dist_path);
                if (f_dist.good()) {
                    f_dist.close();
                    manager_file = "dist/" + candidate;
                    found = true;
                    break;
                }
            }
            
            if (found) {
                menu->addChild(new MenuSeparator());
                
                struct OpenManagerItem : MenuItem {
                    WorkshopComputer* module;
                    std::string manager_file;
                    void onAction(const event::Action& e) override {
                        char url[512];
                        snprintf(url, sizeof(url), "http://127.0.0.1:%d/%s/%s?instance=%p",
                                 g_web_server_port, module->card_globals.active_card_id_str.c_str(), manager_file.c_str(), module);
                        
                        #if defined(_WIN32)
                            std::string cmd = "start " + std::string(url);
                        #elif defined(__APPLE__)
                            std::string cmd = "open \"" + std::string(url) + "\"";
                        #else
                            std::string cmd = "xdg-open \"" + std::string(url) + "\"";
                        #endif
                        int ret = std::system(cmd.c_str());
                        (void)ret;
                    }
                };
                
                OpenManagerItem* openItem = new OpenManagerItem();
                openItem->text = "Open Card Manager...";
                openItem->module = module;
                openItem->manager_file = manager_file;
                menu->addChild(openItem);
            }
        }

        // Card selection is now only shown when clicking directly on the cartridge slot.

        // Submenus for Left & Right utility pair channels
        if (module->active_card_idx >= 0 && module->active_card_idx < (int)g_card_registry.size() && g_card_registry[module->active_card_idx].id == "utility_pair") {
            menu->addChild(new MenuSeparator());
            menu->addChild(createMenuLabel("Utility Pair Selection"));

            struct UtilitySubmenuItem : MenuItem {
                WorkshopComputer* module;
                int channel; // 0 = Left, 1 = Right

                Menu* createChildMenu() override {
                    Menu* menu = new Menu();
                    static const std::string UTILITIES[24] = {
                        "Attenuverter", "Bernoulli Gate", "Bitcrusher", "Chords",
                        "Chorus", "Clock Divider", "Cross Switch", "CV Mixer",
                        "Delay", "Euclidean Rhythms", "Glitch", "Karplus-Strong",
                        "Low Pass Gate", "Max/Rectify", "Quantiser", "Sample & Hold",
                        "Slopes Plus", "Slow LFO", "Super Saw", "Turing 185",
                        "VCA", "VCO", "Wavefolder", "Window Comparator"
                    };

                    struct UtilItem : MenuItem {
                        WorkshopComputer* module;
                        int channel;
                        int index;
                        void onAction(const event::Action& e) override {
                            module->utility_indices[channel] = index;
                            module->change_card(module->active_card_idx); // Reload card to engage the new utility
                        }
                    };

                    for (int i = 0; i < 24; i++) {
                        UtilItem* item = new UtilItem();
                        item->text = UTILITIES[i];
                        item->module = module;
                        item->channel = channel;
                        item->index = i;
                        item->rightText = (module->utility_indices[channel] == i) ? "✔" : "";
                        menu->addChild(item);
                    }
                    return menu;
                }
            };

            UtilitySubmenuItem* leftItem = new UtilitySubmenuItem();
            leftItem->text = "Left Utility Channel";
            leftItem->module = module;
            leftItem->channel = 0;
            menu->addChild(leftItem);

            UtilitySubmenuItem* rightItem = new UtilitySubmenuItem();
            rightItem->text = "Right Utility Channel";
            rightItem->module = module;
            rightItem->channel = 1;
            menu->addChild(rightItem);
        }

        // Flash Memory actions
        menu->addChild(new MenuSeparator());
        menu->addChild(createMenuLabel("Flash Memory"));

        struct FlashLoadItem : MenuItem {
            WorkshopComputer* module;
            void onAction(const event::Action& e) override {
                osdialog_filters* filters = osdialog_filters_parse("Flash/SoundFont:uf2,bin,sf2");
                char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
                if (path) {
                    module->load_file_to_flash(path);
                    free(path);
                }
                osdialog_filters_free(filters);
            }
        };

        FlashLoadItem* loadFlashItem = new FlashLoadItem();
        loadFlashItem->text = "Load Flash File (.uf2 / .bin / .sf2) ...";
        loadFlashItem->module = module;
        menu->addChild(loadFlashItem);

        struct FlashClearItem : MenuItem {
            WorkshopComputer* module;
            void onAction(const event::Action& e) override {
                if (osdialog_message(OSDIALOG_WARNING, OSDIALOG_YES_NO, "Are you sure you want to clear the simulated flash memory? This will erase all samples/settings loaded in flash.")) {
                    memset(module->card_globals.g_flash_memory_val, 0xFF, PICO_FLASH_SIZE_BYTES);
                    CardGlobals* old_instance = t_instance;
                    t_instance = &module->card_globals;
                    save_flash_to_disk();
                    t_instance = old_instance;
                }
            }
        };

        FlashClearItem* clearFlashItem = new FlashClearItem();
        clearFlashItem->text = "Clear Flash Memory (Reset to 0xFF)";
        clearFlashItem->module = module;
        menu->addChild(clearFlashItem);
    }
};

Model* modelWorkshopComputer = createModel<WorkshopComputer, WorkshopComputerWidget>("WorkshopComputer");

#include "WebServer.hpp"

// Standalone Web Server client connection handler implementation
void handle_client(socket_t client_fd) {
    // Set socket timeout to 5 seconds for initial HTTP request reading
    set_socket_timeout(client_fd, 5000);
    
    // Read HTTP request
    std::string request;
    char buf[4096];
    while (true) {
        int r = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (r <= 0) {
            close_socket(client_fd);
            return;
        }
        buf[r] = '\0';
        request += buf;
        if (request.find("\r\n\r\n") != std::string::npos) {
            break;
        }
        if (request.length() > 8192) {
            close_socket(client_fd);
            return;
        }
    }
    
    // Parse request line
    size_t req_line_end = request.find("\r\n");
    if (req_line_end == std::string::npos) {
        close_socket(client_fd);
        return;
    }
    std::string req_line = request.substr(0, req_line_end);
    
    // Parse method and path
    std::stringstream ss(req_line);
    std::string method, full_path, protocol;
    ss >> method >> full_path >> protocol;
    
    if (method != "GET") {
        std::string resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 18\r\n\r\nMethod Not Allowed";
        send(client_fd, resp.c_str(), resp.length(), 0);
        close_socket(client_fd);
        return;
    }
    
    // Separate path and query
    std::string path = full_path;
    std::string query = "";
    size_t q_pos = full_path.find('?');
    if (q_pos != std::string::npos) {
        path = full_path.substr(0, q_pos);
        query = full_path.substr(q_pos + 1);
    }
    
    // Check for WebSocket upgrade
    std::string upgrade_hdr = get_header_value(request, "Upgrade");
    std::transform(upgrade_hdr.begin(), upgrade_hdr.end(), upgrade_hdr.begin(), ::tolower);
    
    if (upgrade_hdr == "websocket") {
        std::string sec_key = get_header_value(request, "Sec-WebSocket-Key");
        if (sec_key.empty()) {
            std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request";
            send(client_fd, resp.c_str(), resp.length(), 0);
            close_socket(client_fd);
            return;
        }
        
        uint8_t sha1_res[20];
        sha1::calculate(sec_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", sha1_res);
        std::string accept_key = base64::encode(sha1_res, 20);
        
        std::string handshake = 
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept_key + "\r\n\r\n";
        send(client_fd, handshake.c_str(), handshake.length(), 0);
        
        // Extract instance pointer from query
        size_t inst_pos = query.find("instance=");
        std::string inst_str = "";
        if (inst_pos != std::string::npos) {
            size_t end_pos = query.find_first_of(" &", inst_pos + 9);
            inst_str = query.substr(inst_pos + 9, end_pos - (inst_pos + 9));
        }
        
        uintptr_t val = 0;
        try {
            if (!inst_str.empty()) {
                val = std::stoull(inst_str, nullptr, 16);
            }
        } catch(...) {}
        
        WorkshopComputer* module_ptr = reinterpret_cast<WorkshopComputer*>(val);
        
        // Verify instance is valid
        {
            std::lock_guard<std::mutex> lock(g_instances_mutex);
            if (g_instances.find(module_ptr) == g_instances.end()) {
                close_socket(client_fd);
                return;
            }
            module_ptr->card_globals.web_ui_connected = true;
        }
        
        // WebSocket loop
        bool ws_connected = true;
        while (ws_connected && g_web_server_running) {
            if (is_readable(client_fd, 5)) {
                uint8_t header[2];
                if (!recv_all(client_fd, header, 2)) {
                    ws_connected = false;
                    break;
                }
                
                uint8_t opcode = header[0] & 0x0F;
                bool masked = (header[1] & 0x80) != 0;
                uint64_t payload_len = header[1] & 0x7F;
                
                if (payload_len == 126) {
                    uint8_t len_bytes[2];
                    if (!recv_all(client_fd, len_bytes, 2)) { ws_connected = false; break; }
                    payload_len = (len_bytes[0] << 8) | len_bytes[1];
                } else if (payload_len == 127) {
                    uint8_t len_bytes[8];
                    if (!recv_all(client_fd, len_bytes, 8)) { ws_connected = false; break; }
                    payload_len = 0;
                    for (int i = 0; i < 8; i++) {
                        payload_len = (payload_len << 8) | len_bytes[i];
                    }
                }
                
                uint8_t mask_key[4] = {0};
                if (masked) {
                    if (!recv_all(client_fd, mask_key, 4)) { ws_connected = false; break; }
                }
                
                std::vector<uint8_t> payload(payload_len);
                if (payload_len > 0) {
                    if (!recv_all(client_fd, payload.data(), payload_len)) { ws_connected = false; break; }
                    if (masked) {
                        for (size_t i = 0; i < payload_len; i++) {
                            payload[i] ^= mask_key[i % 4];
                        }
                    }
                }
                
                if (opcode == 0x08) {
                    ws_connected = false;
                    break;
                } else if (opcode == 0x09) {
                    uint8_t pong[2] = {0x8A, 0x00};
                    send(client_fd, (const char*)pong, 2, 0);
                } else if (opcode == 0x01) {
                    std::string payload_str(payload.begin(), payload.end());
                    json_error_t json_err;
                    json_t* root = json_loads(payload_str.c_str(), 0, &json_err);
                    if (root) {
                        json_t* type_j = json_object_get(root, "type");
                        json_t* data_j = json_object_get(root, "data");
                        if (type_j && data_j && json_is_string(type_j) && json_is_array(data_j)) {
                            std::string type_str = json_string_value(type_j);
                            size_t data_size = json_array_size(data_j);
                            std::vector<uint8_t> msg_bytes;
                            msg_bytes.reserve(data_size);
                            for (size_t i = 0; i < data_size; i++) {
                                json_t* item = json_array_get(data_j, i);
                                if (json_is_integer(item)) {
                                    msg_bytes.push_back((uint8_t)json_integer_value(item));
                                }
                            }
                            
                            std::lock_guard<std::mutex> lock(g_instances_mutex);
                            if (g_instances.find(module_ptr) != g_instances.end()) {
                                t_instance = &module_ptr->card_globals;
                                if (type_str == "midi") {
                                    push_midi_to_rx_queue(msg_bytes.data(), msg_bytes.size());
                                } else if (type_str == "serial") {
                                    DEBUG("WebSocket serial RX: %zu bytes", msg_bytes.size());
                                    g_serial_rx_byte_queue.push(msg_bytes.data(), msg_bytes.size());
                                } else if (type_str == "flash") {
                                    json_t* addr_j = json_object_get(root, "address");
                                    if (addr_j && json_is_integer(addr_j)) {
                                        uintptr_t addr = json_integer_value(addr_j);
                                        uintptr_t offset = addr;
                                        if (offset >= 0x10000000) {
                                            offset -= 0x10000000;
                                        }
                                        if (offset + msg_bytes.size() <= PICO_FLASH_SIZE_BYTES) {
                                            memcpy(module_ptr->card_globals.g_flash_memory_val + offset, msg_bytes.data(), msg_bytes.size());
                                            DEBUG("Flashed %zu bytes to simulated memory offset 0x%lx via WebSocket", msg_bytes.size(), offset);
                                            
                                            CardGlobals* old_instance = t_instance;
                                            t_instance = &module_ptr->card_globals;
                                            save_flash_to_disk();
                                            t_instance = old_instance;
                                            
                                            module_ptr->change_card(module_ptr->active_card_idx);
                                        }
                                    }
                                }
                            }
                        }
                        json_decref(root);
                    }
                }
            }
            
            if (ws_connected) {
                bool alive = false;
                {
                    std::lock_guard<std::mutex> lock(g_instances_mutex);
                    if (g_instances.find(module_ptr) != g_instances.end()) {
                        alive = true;
                    }
                }
                
                if (!alive) {
                    ws_connected = false;
                    break;
                }
                
                std::vector<uint8_t> midi_msg;
                while (module_ptr->websocket_midi_tx_queue.pop(midi_msg)) {
                    send_websocket_frame(client_fd, 0x01, midi_msg, "midi");
                }
                
                std::vector<uint8_t> serial_msg;
                while (module_ptr->websocket_serial_tx_queue.pop(serial_msg)) {
                    DEBUG("WebSocket serial TX: %zu bytes", serial_msg.size());
                    send_websocket_frame(client_fd, 0x01, serial_msg, "serial");
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        
        {
            std::lock_guard<std::mutex> lock(g_instances_mutex);
            if (g_instances.find(module_ptr) != g_instances.end()) {
                module_ptr->card_globals.web_ui_connected = false;
            }
        }
        
        close_socket(client_fd);
        return;
    }
    
    std::string file_path = "";
    if (path == "/vcv_bridge.js") {
        file_path = asset::plugin(pluginInstance, "res/vcv_bridge.js");
    } else {
        std::string rel_path = path;
        if (!rel_path.empty() && rel_path[0] == '/') {
            rel_path = rel_path.substr(1);
        }
        
        size_t slash_pos = rel_path.find('/');
        if (slash_pos == std::string::npos) {
            std::string card_id = rel_path;
            if (card_id == "krell" || card_id == "duo_midi") {
                card_id = "blackbird";
            }
            std::string subpath = "index.html";
            std::string dist_path = asset::plugin(pluginInstance, "res/web/" + card_id + "/dist/" + subpath);
            std::ifstream test_f(dist_path, std::ios::binary);
            if (test_f.good()) {
                test_f.close();
                file_path = dist_path;
            } else {
                file_path = asset::plugin(pluginInstance, "res/web/" + card_id + "/" + subpath);
            }
        } else {
            std::string card_id = rel_path.substr(0, slash_pos);
            if (card_id == "krell" || card_id == "duo_midi") {
                card_id = "blackbird";
            }
            std::string subpath = rel_path.substr(slash_pos + 1);
            if (subpath.empty()) {
                subpath = "index.html";
            }
            
            std::string dist_path = asset::plugin(pluginInstance, "res/web/" + card_id + "/dist/" + subpath);
            std::ifstream test_f(dist_path, std::ios::binary);
            if (test_f.good()) {
                test_f.close();
                file_path = dist_path;
            } else {
                file_path = asset::plugin(pluginInstance, "res/web/" + card_id + "/" + subpath);
            }
        }
    }
    
    std::ifstream f(file_path, std::ios::binary);
    if (!f.good()) {
        std::string resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
        send(client_fd, resp.c_str(), resp.length(), 0);
        close_socket(client_fd);
        return;
    }
    
    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string content = buffer.str();
    f.close();
    
    if (path != "/vcv_bridge.js") {
        if (path.find(".html") != std::string::npos || path.find(".htm") != std::string::npos) {
            size_t pos = content.find("<head>");
            if (pos != std::string::npos) {
                content.insert(pos + 6, "\n<script src=\"/vcv_bridge.js\"></script>");
            } else {
                pos = content.find("<body>");
                if (pos != std::string::npos) {
                    content.insert(pos + 6, "\n<script src=\"/vcv_bridge.js\"></script>");
                }
            }
        }
    }
    
    std::string mime = get_mime_type(path);
    std::string headers = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: " + mime + "\r\n"
        "Content-Length: " + std::to_string(content.length()) + "\r\n"
        "Connection: close\r\n\r\n";
    send(client_fd, headers.c_str(), headers.length(), 0);
    send(client_fd, content.data(), content.length(), 0);
    close_socket(client_fd);
}
