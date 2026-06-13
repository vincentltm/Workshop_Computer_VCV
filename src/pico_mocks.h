#pragma once

#include <stdint.h>

typedef unsigned int uint;

namespace rack {
namespace audio {
struct Port;
}
}
#include <stddef.h>
#include <string.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <exception>
#include <condition_variable>
#include <mutex>
#include <string>
#include <iostream>

// Mocks for compiler decorators
#define __not_in_flash_func(x) x
#define __not_in_flash(x)
#define __in_flash(group)
#ifdef __APPLE__
#define __attribute__(x)
#endif
#define VREG_VOLTAGE_1_20 120
#define VREG_VOLTAGE_1_25 125

#ifdef __dmb
#undef __dmb
#endif
#define __dmb() asm volatile("" : : : "memory")

typedef void (*gpio_irq_callback_t)(uint gpio, uint32_t events);

// Thread cancellation exception
class ThreadExitException : public std::exception {
public:
    const char* what() const noexcept override {
        return "Thread exit requested";
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// Flash Memory Emulation (sizes defined before CardGlobals)
// ──────────────────────────────────────────────────────────────────────────────
#define FLASH_PAGE_SIZE    256
#define FLASH_SECTOR_SIZE  4096
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)

// ──────────────────────────────────────────────────────────────────────────────
// Hybrid spin-sleep thread-safe FIFO (defined before CardGlobals)
// ──────────────────────────────────────────────────────────────────────────────
// Forward-declare so SpinFIFO can reference g_cancellation_requested via
// t_instance. We break the cycle by inlining the cancellation check inline.
extern thread_local bool is_core1_thread;

class SpinFIFO {
private:
    std::atomic<uintptr_t> buffer[256];
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
public:
    void push(uintptr_t val);       // defined after CardGlobals
    uintptr_t pop();
    bool pop_nonblocking(uintptr_t& val) {
        size_t h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false;
        val = buffer[h].load(std::memory_order_relaxed);
        head.store((h + 1) % 256, std::memory_order_release);
        return true;
    }
    bool empty() const {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }
    size_t size() const {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);
        if (t >= h) return t - h;
        return 256 - h + t;
    }
    void clear() { head.store(0); tail.store(0); }
};

// ──────────────────────────────────────────────────────────────────────────────
// DMA / peripheral stubs (structs needed before CardGlobals)
// ──────────────────────────────────────────────────────────────────────────────
struct dma_channel_hw_t { uint32_t read_addr; };
struct dma_hw_t {
    uint32_t ints0;
    dma_channel_hw_t ch[16];
};

// ──────────────────────────────────────────────────────────────────────────────
// MIDI queue classes — defined here so CardGlobals can own them as real members
// ──────────────────────────────────────────────────────────────────────────────
#include <queue>

class ThreadSafeByteQueue {
    std::mutex mutex_;
    std::queue<uint8_t> q_;
public:
    void push(uint8_t v)                       { std::lock_guard<std::mutex> l(mutex_); q_.push(v); }
    void push(const uint8_t* v, size_t n)      { std::lock_guard<std::mutex> l(mutex_); for (size_t i=0;i<n;i++) q_.push(v[i]); }
    bool pop(uint8_t& v)                       { std::lock_guard<std::mutex> l(mutex_); if (q_.empty()) return false; v=q_.front(); q_.pop(); return true; }
    size_t pop(uint8_t* v, size_t max)         { std::lock_guard<std::mutex> l(mutex_); size_t n=0; while(n<max && !q_.empty()){v[n++]=q_.front();q_.pop();} return n; }
    size_t size()                              { std::lock_guard<std::mutex> l(mutex_); return q_.size(); }
    bool empty()                               { std::lock_guard<std::mutex> l(mutex_); return q_.empty(); }
    void clear()                               { std::lock_guard<std::mutex> l(mutex_); while(!q_.empty()) q_.pop(); }
};

struct MidiPacket { uint8_t bytes[4]; };

class ThreadSafePacketQueue {
    std::mutex mutex_;
    std::queue<MidiPacket> q_;
public:
    void push(const uint8_t p[4]) { std::lock_guard<std::mutex> l(mutex_); MidiPacket mp; memcpy(mp.bytes,p,4); q_.push(mp); }
    bool pop(uint8_t p[4])        { std::lock_guard<std::mutex> l(mutex_); if(q_.empty()) return false; memcpy(p,q_.front().bytes,4); q_.pop(); return true; }
    size_t size()                 { std::lock_guard<std::mutex> l(mutex_); return q_.size(); }
    bool empty()                  { std::lock_guard<std::mutex> l(mutex_); return q_.empty(); }
    void clear()                  { std::lock_guard<std::mutex> l(mutex_); while(!q_.empty()) q_.pop(); }
};

class ComputerCard;

struct CustomCancellationAtomic {
    std::atomic<bool> val{false};
    
    CustomCancellationAtomic() = default;
    CustomCancellationAtomic(bool desired) { val.store(desired); }
    
    bool load(std::memory_order order = std::memory_order_seq_cst) const;
    
    void store(bool desired, std::memory_order order = std::memory_order_seq_cst) {
        val.store(desired, order);
    }
    
    CustomCancellationAtomic& operator=(bool desired) {
        val.store(desired);
        return *this;
    }
    
    operator bool() const {
        return load();
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// Per-instance card globals  ←  THE KEY STRUCT
// Every WorkshopComputer module instance owns one of these.
// All threads (background, core1, audio) set t_instance before running so
// every mock function and macro below routes to the right instance.
// ──────────────────────────────────────────────────────────────────────────────
struct CardGlobals {
    // ── Audio / control I/O (written by process(), read by card code) ──
    float g_knobs[3]           = {0.f, 0.f, 0.f};
    int   g_switch             = 1;
    float g_audio_in[2]        = {0.f, 0.f};
    float g_cv_in[2]           = {0.f, 0.f};
    bool  g_pulse_in[2]        = {false, false};
    bool  g_input_connected[6] = {false, false, false, false, false, false};

    float g_audio_out[2]      = {0.f, 0.f};
    float g_cv_out[2]         = {0.f, 0.f};
    bool  g_pulse_out[2]      = {false, false};
    float g_led_brightness[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

    // ── Threading ──
    CustomCancellationAtomic g_cancellation_requested_val{false};
    std::atomic<bool> g_core1_cancellation_requested_val{false};
    SpinFIFO g_fifo_1_to_0;
    SpinFIFO g_fifo_0_to_1;
    std::thread g_core1_thread_val;

    // ── Synth sync (Flux) ──
    std::mutex g_synth_mutex;
    std::condition_variable g_synth_cv;
    std::atomic<bool> g_synth_need_render{false};
    std::atomic<bool> g_dsp_ready{false};
    std::atomic<bool> block_audio_processing{false};
    std::atomic<bool> in_audio_callback{false};

    // ── Sample rate conversion ──
    double expected_sample_rate = 48000.0;
    double dsp_phase = 0.0;


    // ── Flash emulation ──
    uint8_t g_flash_memory_val[PICO_FLASH_SIZE_BYTES];

    // ── MIDI queues ──
    ThreadSafePacketQueue g_midi_rx_packet_queue;
    ThreadSafeByteQueue   g_midi_tx_byte_queue;

    // ── Serial queues ──
    ThreadSafeByteQueue   g_serial_rx_byte_queue;
    ThreadSafeByteQueue   g_serial_tx_byte_queue;

    // ── DMA stub ──
    dma_hw_t dma_hw_instance{};
    dma_hw_t* dma_hw;

    // ── Card identity ──
    std::string active_card_id_str = "default";
    ComputerCard* card_ptr = nullptr;
    rack::audio::Port* bridge_audio_port = nullptr;
    std::atomic<bool> grid_connected_flag{true};
    std::atomic<bool> sample_mgr_active{false};
    std::atomic<bool> web_ui_connected{false};

    // ── DSO helper function pointers ──
    void (*set_thread_globals_fn)(CardGlobals*) = nullptr;
    void (*set_core1_thread_fn)(bool) = nullptr;
    void (*save_flash_to_disk_fn)() = nullptr;
    void (*multicore_launch_core1_fn)(void (*entry)()) = nullptr;
    void (*on_grid_connected_fn)(bool connected) = nullptr;

    // ── GPIO and ADC mock states for raw SDK usage ──
    bool g_gpio_pins[32] = {false};
    uint g_adc_selected_input = 0;
    gpio_irq_callback_t g_gpio_callbacks[32] = {nullptr};
    bool g_last_pulse_state[2] = {false, false};

    CardGlobals() : dma_hw(&dma_hw_instance) {
        memset(g_flash_memory_val, 0xFF, sizeof(g_flash_memory_val));
        memset(g_gpio_pins, 0, sizeof(g_gpio_pins));
        memset(g_gpio_callbacks, 0, sizeof(g_gpio_callbacks));
        memset(g_last_pulse_state, 0, sizeof(g_last_pulse_state));
    }

    // Non-copyable / non-movable (contains mutexes, atomics, threads)
    CardGlobals(const CardGlobals&) = delete;
    CardGlobals& operator=(const CardGlobals&) = delete;
};

// ──────────────────────────────────────────────────────────────────────────────
// The thread-local pointer that routes every mock to the right instance.
// Set this at the start of every thread that touches card code.
// ──────────────────────────────────────────────────────────────────────────────
extern thread_local CardGlobals* t_instance;

inline CardGlobals* get_safe_instance() {
    static CardGlobals dummy_instance;
    return t_instance ? t_instance : &dummy_instance;
}

inline bool CustomCancellationAtomic::load(std::memory_order order) const {
    if (is_core1_thread && t_instance && t_instance->g_core1_cancellation_requested_val.load(order)) {
        return true;
    }
    return val.load(order);
}

// ──────────────────────────────────────────────────────────────────────────────
// Macro redirects — card source code uses bare names like g_knobs[0];
// these expand to t_instance->g_knobs[0] with zero source changes required.
// ──────────────────────────────────────────────────────────────────────────────
#define g_knobs              (get_safe_instance()->g_knobs)
#define g_switch             (get_safe_instance()->g_switch)
#define g_audio_in           (get_safe_instance()->g_audio_in)
#define g_cv_in              (get_safe_instance()->g_cv_in)
#define g_pulse_in           (get_safe_instance()->g_pulse_in)
#define g_input_connected    (get_safe_instance()->g_input_connected)
#define g_audio_out          (get_safe_instance()->g_audio_out)
#define g_cv_out             (get_safe_instance()->g_cv_out)
#define g_pulse_out          (get_safe_instance()->g_pulse_out)
#define g_led_brightness     (get_safe_instance()->g_led_brightness)
#define g_cancellation_requested (get_safe_instance()->g_cancellation_requested_val)
#define g_fifo_1_to_0        (get_safe_instance()->g_fifo_1_to_0)
#define g_fifo_0_to_1        (get_safe_instance()->g_fifo_0_to_1)
#define g_core1_thread       (get_safe_instance()->g_core1_thread_val)
#define g_synth_mutex        (get_safe_instance()->g_synth_mutex)
#define g_synth_cv           (get_safe_instance()->g_synth_cv)
#define g_synth_need_render  (get_safe_instance()->g_synth_need_render)
#define g_flash_memory       (get_safe_instance()->g_flash_memory_val)
#define g_midi_rx_packet_queue (get_safe_instance()->g_midi_rx_packet_queue)
#define g_midi_tx_byte_queue   (get_safe_instance()->g_midi_tx_byte_queue)
#define g_serial_rx_byte_queue (get_safe_instance()->g_serial_rx_byte_queue)
#define g_serial_tx_byte_queue (get_safe_instance()->g_serial_tx_byte_queue)
#define dma_hw                 (get_safe_instance()->dma_hw)
#define g_active_card_id       (get_safe_instance()->active_card_id_str)

// XIP_BASE now points to this instance's flash buffer
#define XIP_BASE ((uintptr_t)(get_safe_instance()->g_flash_memory_val))

// ──────────────────────────────────────────────────────────────────────────────
// SpinFIFO method bodies (need g_cancellation_requested macro)
// ──────────────────────────────────────────────────────────────────────────────
inline void SpinFIFO::push(uintptr_t val) {
    size_t t = tail.load(std::memory_order_relaxed);
    size_t next_t = (t + 1) % 256;
    int spins = 0;
    while (next_t == head.load(std::memory_order_acquire)) {
        if (g_cancellation_requested.load(std::memory_order_relaxed)) throw ThreadExitException();
        if (++spins > 50000) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        else {
            #if defined(__arm__) || defined(__aarch64__)
            asm volatile("yield");
            #else
            __builtin_ia32_pause();
            #endif
        }
    }
    buffer[t].store(val, std::memory_order_relaxed);
    tail.store(next_t, std::memory_order_release);
}

inline uintptr_t SpinFIFO::pop() {
    size_t h = head.load(std::memory_order_relaxed);
    int spins = 0;
    while (h == tail.load(std::memory_order_acquire)) {
        if (g_cancellation_requested.load(std::memory_order_relaxed)) throw ThreadExitException();
        if (!is_core1_thread) {
            // Audio thread: spin/yield but never sleep to prevent audio dropouts.
            #if defined(__arm__) || defined(__aarch64__)
            asm volatile("yield");
            #else
            __builtin_ia32_pause();
            #endif
        } else {
            if (++spins > 50000) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            else {
                #if defined(__arm__) || defined(__aarch64__)
                asm volatile("yield");
                #else
                __builtin_ia32_pause();
                #endif
            }
        }
    }
    uintptr_t val = buffer[h].load(std::memory_order_relaxed);
    head.store((h + 1) % 256, std::memory_order_release);
    return val;
}

// Critical Section Mocks
struct critical_section_t {
    std::recursive_mutex mutex;
};
inline void critical_section_init(critical_section_t* cs) {}
inline void critical_section_enter_blocking(critical_section_t* cs) { cs->mutex.lock(); }
inline void critical_section_exit(critical_section_t* cs) { cs->mutex.unlock(); }

// ──────────────────────────────────────────────────────────────────────────────
// is_core1_thread — already per-thread, stays thread_local
// ──────────────────────────────────────────────────────────────────────────────
extern thread_local bool is_core1_thread;

// ──────────────────────────────────────────────────────────────────────────────
// Time
// ──────────────────────────────────────────────────────────────────────────────
#define PICO_SDK_VERSION_STRING "2.2.0"
typedef uint64_t absolute_time_t;
#define at_the_end_of_time 0xFFFFFFFFFFFFFFFFULL
#ifndef MLR_FIRMWARE_VERSION
#define MLR_FIRMWARE_VERSION "1.1.2"
#endif

inline absolute_time_t get_absolute_time() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}
inline uint64_t to_us_since_boot(absolute_time_t t) { return t; }
inline uint32_t to_ms_since_boot(absolute_time_t t) {
    if (t_instance) {
        for (int i = 0; i < 2; i++) {
            bool current = g_pulse_in[i];
            bool last = t_instance->g_last_pulse_state[i];
            if (current != last) {
                t_instance->g_last_pulse_state[i] = current;
                uint gpio = i + 2; // PIN_PULSE1_IN is 2
                if (current && !last && t_instance->g_gpio_callbacks[gpio]) {
                    t_instance->g_gpio_callbacks[gpio](gpio, 2); // 2 = GPIO_IRQ_EDGE_FALL
                }
            }
        }
    }
    return (uint32_t)(t / 1000);
}
inline uint64_t time_us_64() { return get_absolute_time(); }
inline uint32_t time_us_32() { return (uint32_t)get_absolute_time(); }
inline int64_t absolute_time_diff_us(absolute_time_t t1, absolute_time_t t2) { return (int64_t)t2 - (int64_t)t1; }
inline absolute_time_t make_timeout_time_ms(uint32_t ms) { return get_absolute_time() + (uint64_t)ms * 1000; }
inline absolute_time_t delayed_by_ms(absolute_time_t t, uint32_t ms) { return t + (uint64_t)ms * 1000; }
inline bool time_reached(absolute_time_t t) { return get_absolute_time() >= t; }

inline void sleep_us(uint64_t us) {
    if (g_cancellation_requested.load(std::memory_order_relaxed)) throw ThreadExitException();
    if (us == 0) return;
    if (us >= 1000) {
        uint64_t ms = us / 1000;
        for (uint64_t i = 0; i < ms; ++i) {
            if (g_cancellation_requested.load(std::memory_order_relaxed)) throw ThreadExitException();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        uint64_t rem = us % 1000;
        if (rem > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(rem));
        }
    } else {
        std::this_thread::sleep_for(std::chrono::microseconds(us));
    }
}

inline void sleep_until(absolute_time_t target) {
    int64_t diff = target - get_absolute_time();
    if (diff > 0) {
        sleep_us(diff);
    }
}

inline void sleep_ms(uint32_t ms) {
    sleep_us((uint64_t)ms * 1000);
}

#define PICO_ERROR_TIMEOUT -1
inline int getchar_timeout_us(uint32_t timeout_us) {
    uint8_t byte = 0;
    if (g_serial_rx_byte_queue.pop(byte)) {
        return byte;
    }
    if (timeout_us == 0) {
        return PICO_ERROR_TIMEOUT;
    }
    
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (g_serial_rx_byte_queue.pop(byte)) {
            return byte;
        }
        if (g_cancellation_requested.load(std::memory_order_relaxed)) {
            throw ThreadExitException();
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_us) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    return PICO_ERROR_TIMEOUT;
}

inline void busy_wait_us_32(uint32_t us) {
    if (is_core1_thread) {
        // Core 1 uses this in two contexts:
        // 1. Synth render sync (tiny us) — block until Core 0 needs more audio
        // 2. External-mode idle sleep (1000 us) — should not block forever
        std::unique_lock<std::mutex> lock(g_synth_mutex);
        uint64_t wait_us = us;
        if (wait_us < 10) wait_us = 10; // minimum timeout of 10us to prevent zero-timeout wait
        g_synth_cv.wait_for(lock, std::chrono::microseconds(wait_us * 4), [&] {
            return g_synth_need_render.load() || g_cancellation_requested.load();
        });
        g_synth_need_render.store(false);
        if (g_cancellation_requested.load(std::memory_order_relaxed)) throw ThreadExitException();
    } else {
        if (us > 1000) {
            sleep_us(us);
        } else {
            auto start = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start).count() < us) {
                if (g_cancellation_requested.load(std::memory_order_relaxed)) throw ThreadExitException();
                #if defined(__arm__) || defined(__aarch64__)
                asm volatile("yield");
                #else
                __builtin_ia32_pause();
                #endif
            }
        }
    }
}
inline void busy_wait_ms(uint32_t ms) { sleep_ms(ms); }

// ──────────────────────────────────────────────────────────────────────────────
// Multicore API
// ──────────────────────────────────────────────────────────────────────────────
inline void stdio_usb_init() {}
inline void multicore_lockout_victim_init() {}
inline void multicore_lockout_start_blocking() {}
inline void multicore_lockout_end_blocking() {}
inline void multicore_reset_core1() {
    if (t_instance) {
        t_instance->g_core1_cancellation_requested_val = true;
    }
}
inline bool multicore_lockout_victim_is_initialized(uint) { return true; }

// Forward-declare so ComputerCard.h can use it
inline void multicore_launch_core1(void (*entry)()) {
    if (t_instance && t_instance->multicore_launch_core1_fn) {
        t_instance->multicore_launch_core1_fn(entry);
    }
}

inline void multicore_fifo_push_blocking(uintptr_t data) {
    if (!is_core1_thread) g_fifo_0_to_1.push(data);
    else                  g_fifo_1_to_0.push(data);
}
inline uintptr_t multicore_fifo_pop_blocking() {
    if (!is_core1_thread) return g_fifo_1_to_0.pop();
    else                  return g_fifo_0_to_1.pop();
}
inline bool multicore_fifo_rvalid() {
    if (!is_core1_thread) return !g_fifo_1_to_0.empty();
    else                  return !g_fifo_0_to_1.empty();
}
inline bool multicore_fifo_wready() { return true; }
inline void multicore_fifo_drain() {
    if (!is_core1_thread) g_fifo_1_to_0.clear();
    else                  g_fifo_0_to_1.clear();
}

// ──────────────────────────────────────────────────────────────────────────────
// Flash
// ──────────────────────────────────────────────────────────────────────────────

inline void flash_range_erase(uint32_t flash_offs, size_t count) {
    if (flash_offs + count <= PICO_FLASH_SIZE_BYTES)
        memset(g_flash_memory + flash_offs, 0xFF, count);
}
inline void flash_range_program(uint32_t flash_offs, const uint8_t* data, size_t count) {
    if (flash_offs + count <= PICO_FLASH_SIZE_BYTES) {
        memcpy(g_flash_memory + flash_offs, data, count);
        if (t_instance && t_instance->save_flash_to_disk_fn) {
            t_instance->save_flash_to_disk_fn();
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// GPIO
// ──────────────────────────────────────────────────────────────────────────────
#define PULSE_1_RAW_OUT 8
#define PULSE_2_RAW_OUT 9
#define USB_HOST_STATUS 20

inline uint32_t save_and_disable_interrupts() { return 0; }
inline void restore_interrupts(uint32_t) {}

inline uint32_t get_rand_32() {
    static uint32_t seed = 0x12345678;
    seed = seed * 1664525 + 1013904223;
    return seed;
}

inline void set_sys_clock_khz(uint32_t, bool) {}
inline void vreg_set_voltage(int) {}
inline void stdio_init_all() {}
inline int putchar_raw(int c) {
    uint8_t byte = static_cast<uint8_t>(c);
    g_serial_tx_byte_queue.push(byte);
    return c;
}
inline void stdio_flush() {}
inline bool stdio_usb_connected() { return true; }
inline void watchdog_reboot(uint32_t, uint32_t, uint32_t) {}

struct bus_ctrl_hw_t { uint32_t priority; };
inline bus_ctrl_hw_t* get_bus_ctrl_hw() { static bus_ctrl_hw_t v; return &v; }
#define bus_ctrl_hw get_bus_ctrl_hw()

inline void gpio_init(uint) {}
inline void gpio_pull_up(uint) {}
inline void gpio_disable_pulls(uint) {}
inline void gpio_set_pulls(uint, bool, bool) {}
inline void gpio_set_dir(uint, bool) {}
inline void gpio_set_function(uint, uint) {}
#define GPIO_OUT true
#define GPIO_IN false
#define GPIO_FUNC_SIO 0
#define GPIO_FUNC_PWM 1
#define GPIO_FUNC_SPI 2
#define GPIO_FUNC_I2C 3

inline bool gpio_get(uint gpio) {
    if (gpio == 2) return g_pulse_in[0];
    if (gpio == 3) return g_pulse_in[1];
    if (gpio == 20) return true; // USB_HOST_STATUS (upstream device mode) is always true in VCV Rack
    if (gpio < 32) return get_safe_instance()->g_gpio_pins[gpio];
    return false;
}
inline void gpio_put(uint gpio, bool val) {
    if (gpio < 32) {
        get_safe_instance()->g_gpio_pins[gpio] = val;
    }
    if (gpio == 8)  g_pulse_out[0] = !val; // Inverted for physical active-low Eurorack buffering!
    else if (gpio == 9) g_pulse_out[1] = !val;
    else if (gpio >= 10 && gpio <= 15)
        g_led_brightness[gpio - 10] = val ? 1.0f : 0.0f;
}

// ──────────────────────────────────────────────────────────────────────────────
// PWM
// ──────────────────────────────────────────────────────────────────────────────
struct pwm_config { uint32_t dummy; };
inline pwm_config pwm_get_default_config() { pwm_config c; return c; }
inline void pwm_config_set_wrap(pwm_config*, uint16_t) {}
inline uint pwm_gpio_to_slice_num(uint gpio) { return gpio; }
inline void pwm_init(uint, const pwm_config*, bool) {}
inline void pwm_set_gpio_level(uint gpio, uint16_t level) {
    if      (gpio == 23) g_cv_out[0] = ((float)level - 1024.f) * (5.f / 1024.f);
    else if (gpio == 22) g_cv_out[1] = ((float)level - 1024.f) * (5.f / 1024.f);
}

// ──────────────────────────────────────────────────────────────────────────────
// ADC
// ──────────────────────────────────────────────────────────────────────────────
struct adc_hw_t { uint32_t fifo; };
inline adc_hw_t* get_adc_hw() { static adc_hw_t v; return &v; }
#define adc_hw get_adc_hw()
inline void adc_init() {}
inline void adc_gpio_init(uint) {}
inline void adc_select_input(uint input) {
    if (t_instance) {
        t_instance->g_adc_selected_input = input;
    }
}
inline uint16_t adc_read() {
    CardGlobals* inst = get_safe_instance();
    uint input = inst->g_adc_selected_input;
    if (input == 0) {
        float v = g_audio_in[0];
        if (v < -6.f) v = -6.f;
        if (v > 6.f) v = 6.f;
        return (uint16_t)((v + 6.f) * (4095.f / 12.f));
    } else if (input == 1) {
        float v = g_audio_in[1];
        if (v < -6.f) v = -6.f;
        if (v > 6.f) v = 6.f;
        return (uint16_t)((v + 6.f) * (4095.f / 12.f));
    } else if (input == 2) {
        bool logic_a = inst->g_gpio_pins[24];
        bool logic_b = inst->g_gpio_pins[25];
        uint chan = (logic_b ? 2 : 0) | (logic_a ? 1 : 0);
        if (chan == 0) {
            return (uint16_t)(g_knobs[0] * 4095.f);
        } else if (chan == 1) {
            return (uint16_t)(g_knobs[1] * 4095.f);
        } else if (chan == 2) {
            return (uint16_t)(g_knobs[2] * 4095.f);
        } else if (chan == 3) {
            int sw = g_switch;
            if (sw == 0) return 0;
            if (sw == 2) return 4095;
            return 2048;
        }
    } else if (input == 3) {
        bool logic_a = inst->g_gpio_pins[24];
        bool logic_b = inst->g_gpio_pins[25];
        uint chan = (logic_b ? 2 : 0) | (logic_a ? 1 : 0);
        if (chan == 0) {
            float v = g_cv_in[0];
            if (v < -6.f) v = -6.f;
            if (v > 6.f) v = 6.f;
            return (uint16_t)((6.f - v) * (4095.f / 12.f));
        } else if (chan == 1) {
            float v = g_cv_in[1];
            if (v < -6.f) v = -6.f;
            if (v > 6.f) v = 6.f;
            return (uint16_t)((6.f - v) * (4095.f / 12.f));
        }
    }
    return 0;
}
inline void adc_set_round_robin(uint) {}
inline void adc_fifo_setup(bool, bool, uint, bool, bool) {}
inline void adc_set_clkdiv(float div) {
    if (t_instance) {
        double rate = 6000000.0 / (div + 1.0);
        if (rate < 8000.0) {
            rate = 48000000.0 / (div + 1.0);
        }
        t_instance->expected_sample_rate = rate;
    }
}
inline void adc_run(bool) {}
inline uint16_t adc_fifo_get_blocking() { return 0; }
inline uint adc_get_selected_input() { return 0; }
inline void flash_get_unique_id(uint8_t* unique_id) { memset(unique_id, 0, 8); }
#define PICO_UNIQUE_BOARD_ID_SIZE_BYTES 8
struct pico_unique_board_id_t {
    uint8_t id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES];
};
inline void pico_get_unique_board_id(pico_unique_board_id_t* id_out) {
    memset(id_out->id, 0, PICO_UNIQUE_BOARD_ID_SIZE_BYTES);
}

// ──────────────────────────────────────────────────────────────────────────────
// DMA (dma_hw macro already defined via t_instance->dma_hw above)
// ──────────────────────────────────────────────────────────────────────────────
struct dma_channel_config { uint32_t dummy; };
inline dma_channel_config dma_channel_get_default_config(uint) { dma_channel_config c; return c; }
inline void channel_config_set_transfer_data_size(dma_channel_config*, int) {}
inline void channel_config_set_read_increment(dma_channel_config*, bool) {}
inline void channel_config_set_write_increment(dma_channel_config*, bool) {}
inline void channel_config_set_dreq(dma_channel_config*, int) {}
inline uint dma_claim_unused_channel(bool) { return 0; }
inline void dma_channel_configure(uint, const dma_channel_config*, void*, const void*, uint, bool) {}
inline void dma_channel_set_irq0_enabled(uint, bool) {}
inline void dma_channel_set_write_addr(uint, void*, bool) {}
inline void dma_channel_set_read_addr(uint, const void*, bool) {}

inline void channel_config_set_chain_to(dma_channel_config*, int) {}
inline void dma_start_channel_mask(uint32_t) {}

struct pwm_slice_hw_t { uint32_t cc; };
struct pwm_hw_t { pwm_slice_hw_t slice[32]; };
inline pwm_hw_t* get_pwm_hw() { static pwm_hw_t d = {}; return &d; }
#define pwm_hw (get_pwm_hw())

#define DMA_SIZE_16 1
#define DMA_SIZE_32 2
#define DREQ_ADC 0
#define SPI_DREQ 0
#define DREQ_SPI0_TX 0
#define DREQ_PWM_WRAP0 0
#define DMA_IRQ_0 0

// ──────────────────────────────────────────────────────────────────────────────
// SPI / I2C / Interrupt stubs
// ──────────────────────────────────────────────────────────────────────────────
struct spi_hw_t { uint32_t dr; };
typedef void* spi_inst_t;
#define spi0 ((spi_inst_t)0)
inline spi_hw_t* spi_get_hw(spi_inst_t) { static spi_hw_t d; return &d; }
inline void spi_init(spi_inst_t, uint) {}
#define SPI_CPOL_0 0
#define SPI_CPHA_0 0
#define SPI_MSB_FIRST 0
#define SPI_LSB_FIRST 0
inline void spi_set_format(spi_inst_t, uint, int, int, int) {}
inline int spi_write16_blocking(spi_inst_t, const uint16_t*, size_t len) { return (int)len; }

typedef void* i2c_inst_t;
#define i2c0 ((i2c_inst_t)0)
#define i2c_default ((i2c_inst_t)0)
inline uint  i2c_init(i2c_inst_t, uint baudrate) { return baudrate; }
inline int   i2c_write_blocking(i2c_inst_t, uint8_t, const uint8_t*, size_t len, bool) { return (int)len; }
inline int   i2c_read_blocking(i2c_inst_t, uint8_t, uint8_t*, size_t len, bool) { return (int)len; }

inline void irq_set_enabled(uint, bool) {}
inline void irq_set_exclusive_handler(uint, void(*)()) {}

#define GPIO_IRQ_EDGE_FALL 2
inline void gpio_set_irq_enabled_with_callback(uint gpio, uint32_t events, bool enabled, gpio_irq_callback_t callback) {
    if (t_instance && gpio < 32) {
        t_instance->g_gpio_callbacks[gpio] = enabled ? callback : nullptr;
    }
}

inline void gpio_put_masked(uint32_t mask, uint32_t value) {
    for (uint i = 0; i < 32; i++) {
        if ((mask >> i) & 1) {
            gpio_put(i, (value >> i) & 1);
        }
    }
}

inline uint pwm_gpio_to_channel(uint) { return 0; }
inline void pwm_set_wrap(uint, uint16_t) {}
inline void pwm_set_enabled(uint, bool) {}
inline void pwm_set_chan_level(uint slice_num, uint chan, uint16_t level) {
    if (slice_num == 23) g_cv_out[0] = ((float)level - 1024.f) / 1024.f;
    else if (slice_num == 22) g_cv_out[1] = ((float)level - 1024.f) / 1024.f;
}

// ──────────────────────────────────────────────────────────────────────────────
// Additional stubs for queue, watchdog, and core utilities (used by Rompler)
// ──────────────────────────────────────────────────────────────────────────────
#include <queue>
#include <vector>
#include <mutex>

struct queue_t {
    std::mutex mutex;
    std::vector<uint8_t> buffer;
    uint element_size = 0;
    uint element_count = 0;
    uint head = 0;
    uint tail = 0;
    uint size = 0;
};

inline void queue_init(queue_t* q, uint element_size, uint element_count) {
    std::lock_guard<std::mutex> lock(q->mutex);
    q->element_size = element_size;
    q->element_count = element_count;
    q->buffer.resize(element_size * element_count);
    q->head = 0;
    q->tail = 0;
    q->size = 0;
}

inline bool queue_try_add(queue_t* q, const void* element) {
    std::lock_guard<std::mutex> lock(q->mutex);
    if (q->size >= q->element_count) return false;
    memcpy(q->buffer.data() + q->tail * q->element_size, element, q->element_size);
    q->tail = (q->tail + 1) % q->element_count;
    q->size++;
    return true;
}

inline bool queue_try_remove(queue_t* q, void* element) {
    std::lock_guard<std::mutex> lock(q->mutex);
    if (q->size == 0) return false;
    memcpy(element, q->buffer.data() + q->head * q->element_size, q->element_size);
    q->head = (q->head + 1) % q->element_count;
    q->size--;
    return true;
}

inline uint get_core_num() {
    return is_core1_thread ? 1 : 0;
}

inline void tight_loop_contents() {}

inline bool tuh_midi_packet_read(uint8_t, uint8_t*) { return false; }

struct watchdog_hw_t {
    uint32_t scratch[8];
    uint32_t reason;
};
inline watchdog_hw_t* get_watchdog_hw() {
    static watchdog_hw_t inst = {};
    return &inst;
}
#define watchdog_hw (get_watchdog_hw())

inline void watchdog_enable(uint32_t, bool) {}
inline void watchdog_disable() {}

// Spin Lock Mocks
typedef struct spin_lock spin_lock_t;
inline spin_lock_t* spin_lock_instance(uint num) { return (spin_lock_t*)(uintptr_t)(num + 1); }
inline uint32_t spin_lock_blocking(spin_lock_t* lock) { (void)lock; return 0; }
inline void spin_unlock(spin_lock_t* lock, uint32_t irq_state) { (void)lock; (void)irq_state; }

// Repeating Timer & ISR Mocks
#define __isr
#define __time_critical_func(x) x
struct repeating_timer {
    int dummy;
};
inline bool add_repeating_timer_us(int32_t delay_us, bool (*callback)(struct repeating_timer*), void* user_data, struct repeating_timer* out) {
    (void)delay_us; (void)callback; (void)user_data; (void)out;
    return true;
}



