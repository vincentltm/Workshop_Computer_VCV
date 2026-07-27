#define COMPUTERCARD_SAMPLE_RATE_DIV 1
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <fstream>
#include <iostream>
#include <sstream>
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
#include <array>
#include <cmath>
#include <complex>
#include <random>
#include <functional>
#include <queue>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <numeric>
#include <initializer_list>
#include "pico_mocks.h"
#include "tusb.h"
#define while(...) while((__VA_ARGS__) && !g_cancellation_requested.load(std::memory_order_relaxed))

#include "ComputerCard.h"

#define _Static_assert static_assert
namespace std { using ::sinf; using ::cosf; using ::tanf; using ::asinf; using ::acosf; using ::atanf; using ::atan2f; using ::sqrtf; using ::expf; using ::logf; using ::log10f; using ::powf; }

// DSO-local thread-local variables definition
thread_local CardGlobals* t_instance = nullptr;
thread_local bool is_core1_thread = false;
thread_local ComputerCard* ComputerCard::thisptr = nullptr;

namespace Card_Lens {

    int main();

// ──────────────────────────────────────────────────────────────────────────────
// Source: runtime/main.cpp
// ──────────────────────────────────────────────────────────────────────────────

/*
 * Lens hardware host: ComputerCard subclass + boot + dual-core init.
 *
 * ComputerCard by Chris Johnson (see ComputerCard.h).
 *
 * Core 0: audio interrupt via ComputerCard::ProcessSample() -> runtime_walk_core0().
 * Core 1: TinyUSB device stack + sysex receive parser + runtime_walk_core1().
 *
 * Dual-core model (non-blocking):
 *   Core 0 (audio ISR) rings Core 1 via the SIO FIFO doorbell each sample, walks
 *   its own slots, sweeps its own recordheads, publishes its own shadows, drives
 *   the output jacks, and increments sample_counter. It never waits on Core 1.
 *   Core 1's doorbell IRQ walks its slots, sweeps and publishes its own, and sets
 *   core1_done. A late Core 1 just holds last sample's shadow (one extra sample of
 *   cross-core lag, never a stall). Cross-core reads see the previous sample's
 *   shadow, so a forward reference is free one-sample feedback.
 */

/* stripped ComputerCard include */
/* stripped pico include */
/* stripped pico include */
/* stripped hardware include */
/* stripped hardware include */
/* stripped hardware include */
/* stripped hardware include */
/* stripped hardware include */
/* stripped hardware include */
/* stripped hardware include */
/* stripped bsp include */  /* board_init */
/* stripped tusb include */
#include "sysex.h"
#include "midi.h"
/* stripped usb_midi_host include */

/* stripped hardware include */

extern "C" {
#include "runtime.h"
#include "factory_snapshot.h"
}

/* ---- Flash save-slot layout ----
 *
 * Pico has 2 MB flash (PICO_FLASH_SIZE_BYTES = 2097152).
 * Reserve the last two 4 KB sectors (8 KB) for the saved snapshot + live state.
 * Offset is XIP_BASE-relative for reading; flash_range_* use byte offsets
 * from flash origin (no XIP_BASE added).
 *
 * Layout inside the slot:
 *   [0..3]    magic "LENS"  (4 bytes)
 *   [4..7]    build hash u32 LE (rejects a snapshot saved by a different build)
 *   [8..9]    snapshot length u16 LE
 *   [10..11]  node-state pool length u16 LE  (live state, 0 = none)
 *   [12..13]  control pool length u16 LE      (live tape/register state, 0 = none)
 *   [14..]    snapshot bytes, then node-state pool bytes, then control pool bytes
 *   [N..N+3]  CRC32 over bytes [0..N)
 *
 * The pools are saved as raw bytes. Restore copies them back over the live pools
 * after snapshot_apply: the same snapshot re-bumps to the same per-slot offsets, so
 * the bytes line up with no per-kernel interpretation. The build-hash guard ensures
 * struct layouts match. Audio-pool buffers (delay lines, recorded audio) are NOT
 * saved: only node state and the small control pool (registers, short tapes). If the
 * snapshot plus pools would overflow the slot, the save falls back to patch-only
 * (nd_len = ct_len = 0). The 8 KB staging buffer (g_pending) caps a write at 8 KB.
 */
#define LENS_SAVE_MAGIC    "LENS"
#define LENS_SAVE_MAGIC_LEN 4
#define LENS_SAVE_HDR_LEN  (LENS_SAVE_MAGIC_LEN + 4u + 2u + 2u + 2u) /* magic+hash+snap_len+nd_len+ct_len */
#define LENS_SAVE_SLOT_SIZE (8u * 1024u)               /* two sectors */
#define LENS_SAVE_SLOT_OFF  (PICO_FLASH_SIZE_BYTES - LENS_SAVE_SLOT_SIZE)
#define LENS_SAVE_SLOT_XIP  (XIP_BASE + LENS_SAVE_SLOT_OFF)

/* Simple CRC32 (ISO 3309 / Ethernet polynomial). */
static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
    }
    return ~crc;
}

static uint32_t save_crc32(const uint8_t* data, size_t len) {
    return crc32_update(0, data, len);
}

/* ---- DWT cycle counter ---- */
/* RP2040 Cortex-M0+ has no DWT.CYCCNT; SysTick is used instead:
 * a free-running 24-bit DOWN counter at sysclk. Read with `cvr`. Per-sample
 * deltas are ~5k cycles at 250 MHz, well inside the 16M wrap window.
 * SysTick counts DOWN: elapsed = (start - end) & 0xFFFFFF. */
static uint32_t mock_systick_csr = 0;
#define M0P_SYSTICK_CSR mock_systick_csr
static uint32_t mock_systick_rvr = 0;
#define M0P_SYSTICK_RVR mock_systick_rvr
static uint32_t mock_systick_cvr = 0;
#define M0P_SYSTICK_CVR mock_systick_cvr

static inline void dwt_enable(void) {
    M0P_SYSTICK_RVR = 0x00FFFFFFu;      /* 24-bit max */
    M0P_SYSTICK_CVR = 0;                /* writing clears */
    M0P_SYSTICK_CSR = (1u << 2) | 1u;   /* CLKSOURCE=core, ENABLE */
}

static inline uint32_t dwt_read(void) { return M0P_SYSTICK_CVR; }

/* ---- Perf ring (last 1024 samples) ---- */
/* LENS_PERF_PROBE default is in runtime.h; CMake forwards the build flag. */
#if LENS_PERF_PROBE
/* Three cycle sections per sample: walk (Core 0 slot walk), io (hw read + drive),
   total (full ProcessSample). All in raw DWT cycles. */
struct PerfRingEntry {
    uint16_t total;  /* full ProcessSample cycles (saturates at 65535) */
    uint16_t walk;   /* Core 0 walk cycles */
    uint16_t io;     /* hw_in gather + driveJacks cycles */
};
static struct PerfRingEntry  perf_ring[1024];
static uint32_t              perf_head = 0;
/* SPEC: exposed for sysex PERF_DUMP handler (Core 1 reads, Core 0 writes). */
volatile struct PerfRingEntry* const lens_perf_ring    = perf_ring;
volatile uint32_t*             const lens_perf_head_p  = &perf_head;
const    uint32_t                    lens_perf_ring_len = 1024;
#endif

/* ---- UX state ---- */

/* Boot dance: L-shaped LED sweep for the first ~1.5 s. */
static const uint32_t kBootDanceSamples  = 72000u;
/* Save-arm: hold Z-switch down for ~5 s to arm a save. */
static const uint32_t kSaveArmHoldSamples = 240000u;

/* Counts samples elapsed since ProcessSample first ran. */
static uint32_t g_boot_sample    = 0u;
/* Counts consecutive samples the Z-switch has been held DOWN (pos 0). */
static uint32_t g_save_arm_count = 0u;
/* Set true when save-arm completes or CMD_SAVE_STATE received. */
static volatile bool g_save_requested     = false;
/* Set true when CMD_FACTORY_RESET received. */
static volatile bool g_factory_reset_requested = false;

/*
 * Cache of the last applied snapshot bytes.
 * Updated on every successful snapshot_apply (factory boot + sysex WRITE).
 * Written by Core 0 only; read by Core 0 only during flash-save sequence.
 *
 * 4 KB cap: observed patches top out at ~2 KB; SOURCE_CAP (8 KB) is a wire
 * decode budget, not a patch-size budget. Saves > 4 KB get silently dropped.
 */
#define LENS_SNAPSHOT_CACHE 4096u
static uint8_t  g_last_snapshot[LENS_SNAPSHOT_CACHE];
static uint16_t g_last_snapshot_len = 0;

/*
 * Active runtime pointer. Written by Core 0 only, at apply time at the top of
 * ProcessSample (before the ring). Core 1's audio IRQ reads it after the ring;
 * Core 1's USB foreground never touches it. Volatile prevents caching.
 */
static struct LensRuntime* volatile g_rt = nullptr;


/* ---- Diagnostics ----
 * Updated on every snapshot_apply attempt (factory boot + each CMD_WRITE_STATE).
 * Read by CMD_DIAG to give the host an authoritative picture without guessing
 * from audio output.
 */
#ifndef LENS_BUILD_HASH
#define LENS_BUILD_HASH 0u
#endif
static const  uint32_t g_build_hash         = (uint32_t)(LENS_BUILD_HASH);
static volatile int32_t  g_last_apply_rc    = 0;   /* return code of last snapshot_apply */
static volatile uint32_t g_apply_count      = 0;   /* successful applies since boot */
static volatile uint32_t g_apply_attempts   = 0;   /* attempts (success + fail) */
static volatile uint32_t g_snapshot_crc     = 0;   /* CRC32 trailer of current live snapshot */
static volatile uint32_t g_last_apply_sample = 0;  /* sample_counter when last apply landed */

static inline uint32_t snapshot_trailer_crc(const uint8_t* bytes, size_t len) {
    if (len < 4) return 0;
    size_t o = len - 4;
    return (uint32_t)bytes[o] | ((uint32_t)bytes[o+1] << 8)
         | ((uint32_t)bytes[o+2] << 16) | ((uint32_t)bytes[o+3] << 24);
}

/*
 * Tempo blink on led-5 is provided by the prelude as a default cable
 * (`(<- led-5 (envelope :trig (tick) :decay 2))`) so it tracks the user's
 * master clock. No hardcoded runtime override needed.
 */

/* ---- Flash request triggers (callable from Core 1 via sysex handler) ---- */
extern "C" void lens_request_save(void) {
    g_save_requested = true;
}
extern "C" void lens_request_factory_reset(void) {
    g_factory_reset_requested = true;
}

/* USB role: 1 = host, 0 = device. Set on Core 0 before Core 1 launches; Core 1
 * reads it once at startup to select the USB init path. */
volatile uint8_t g_usb_host_mode = 0;

/* Live-swap timing, set over sysex (CMD_SWAP_MODE). Written by Core 1, read by
 * Core 0. ZERO = next near-zero audio sample (default, click-free). BEAT / BAR =
 * align the swap to the running patch's master clock (a bar is 4 beats). */
enum { SWAP_AT_ZERO = 0, SWAP_AT_BEAT = 1, SWAP_AT_BAR = 2 };
volatile uint8_t g_swap_mode = SWAP_AT_ZERO;

/* Output-change journal, read back over sysex (CMD_BEAT_LOG): every transition
 * of the driven (cv1, cv2, led0) triple with its sample timestamp, plus the
 * count of upward zero crossings of audio_out_2 over the previous dwell, so the
 * RENDERED sine frequency on hardware is measurable per beat. */
struct BeatLogEntry { uint32_t sample; uint16_t cv1; uint16_t cv2; uint16_t zc2; uint8_t led0; uint8_t beat; };
static volatile BeatLogEntry g_beatlog[64];
static volatile uint32_t     g_beatlog_head = 0;

/*
 * Perform flash save: erase slot, write magic+len+snapshot+crc32, reboot.
 * Must be called with Core 1 reset and interrupts disabled.
 */
static void do_flash_save(void) {
    if (g_last_snapshot_len == 0) return; /* nothing to save */

    const size_t HDR_LEN  = LENS_SAVE_HDR_LEN;
    const size_t snap_len = g_last_snapshot_len;
    /* Live state to overlay on restore: the node-state pool (op state, incl. pickup
       takeover values) and the small control pool (registers / short tapes). */
    size_t nd_len = lens_nodestate_used();
    size_t ct_len = lens_control_used();
    if (nd_len > LENS_NODESTATE_BYTES)      nd_len = LENS_NODESTATE_BYTES;
    if (ct_len > LENS_CONTROL_BUFFER_BYTES) ct_len = LENS_CONTROL_BUFFER_BYTES;

    /* If snapshot + live state overflow the slot or the 8 KB staging buffer, fall back
       to a patch-only save (still useful: the graph survives, settings reset). */
    if (HDR_LEN + snap_len + nd_len + ct_len + 4u > LENS_SAVE_SLOT_SIZE) {
        nd_len = 0;
        ct_len = 0;
    }
    const size_t payload_len = HDR_LEN + snap_len + nd_len + ct_len;
    const size_t total_len   = payload_len + 4; /* +4 for CRC32 */
    if (total_len > LENS_SAVE_SLOT_SIZE) return;

    /* Align total write size to FLASH_PAGE_SIZE (256 bytes). */
    const size_t write_len = (total_len + FLASH_PAGE_SIZE - 1u)
                             & ~(size_t)(FLASH_PAGE_SIZE - 1u);

    /*
     * Reuse the pending-patch buffer as our page-aligned write buffer.
     * Core 1 is already reset at this point and g_pending is unused.
     * s_pending_buf is SOURCE_CAP (8192) bytes and already in BSS.
     * flash_range_program requires the source buffer to be in RAM.
     */
    uint8_t* buf = g_pending.bytes; /* points into s_pending_buf via g_pending */

    memset(buf, 0xFF, write_len);
    /* Write magic. */
    buf[0] = 'L'; buf[1] = 'E'; buf[2] = 'N'; buf[3] = 'S';
    /* Write build hash u32 LE; load rejects a mismatch (stale cross-build save). */
    buf[4] = (uint8_t)(g_build_hash & 0xFFu);
    buf[5] = (uint8_t)((g_build_hash >> 8) & 0xFFu);
    buf[6] = (uint8_t)((g_build_hash >> 16) & 0xFFu);
    buf[7] = (uint8_t)((g_build_hash >> 24) & 0xFFu);
    /* Write snapshot / pool lengths, u16 LE. */
    buf[8]  = (uint8_t)(snap_len & 0xFFu);
    buf[9]  = (uint8_t)((snap_len >> 8) & 0xFFu);
    buf[10] = (uint8_t)(nd_len & 0xFFu);
    buf[11] = (uint8_t)((nd_len >> 8) & 0xFFu);
    buf[12] = (uint8_t)(ct_len & 0xFFu);
    buf[13] = (uint8_t)((ct_len >> 8) & 0xFFu);
    /* Write snapshot bytes, then the live pools. */
    uint8_t* w = buf + HDR_LEN;
    memcpy(w, g_last_snapshot,    snap_len); w += snap_len;
    memcpy(w, lens_nodestate_pool, nd_len);  w += nd_len;
    memcpy(w, lens_control_pool,   ct_len);  w += ct_len;

    uint32_t final_crc = save_crc32(buf, payload_len);
    buf[payload_len + 0] = (uint8_t)(final_crc & 0xFFu);
    buf[payload_len + 1] = (uint8_t)((final_crc >> 8)  & 0xFFu);
    buf[payload_len + 2] = (uint8_t)((final_crc >> 16) & 0xFFu);
    buf[payload_len + 3] = (uint8_t)((final_crc >> 24) & 0xFFu);

    uint32_t save = save_and_disable_interrupts();
    flash_range_erase(LENS_SAVE_SLOT_OFF, LENS_SAVE_SLOT_SIZE);
    flash_range_program(LENS_SAVE_SLOT_OFF, buf, write_len);
    restore_interrupts(save);
}

static void do_flash_erase_slot(void) {
    uint32_t save = save_and_disable_interrupts();
    flash_range_erase(LENS_SAVE_SLOT_OFF, LENS_SAVE_SLOT_SIZE);
    restore_interrupts(save);
}

/* ---- LensCard ---- */

class LensCard : public ComputerCard {
public:
    LensCard() {}

    /* Exposes the protected ComputerCard CC-pin probe for role detection. */
    USBPowerState_t ReadUSBPowerState() { return USBPowerState(); }

    bool init() {
        struct LensRuntime* rt = nullptr;

        /* 1. Try user-saved snapshot from flash slot. */
        const uint8_t* slot = (const uint8_t*)LENS_SAVE_SLOT_XIP;
        bool slot_valid = false;
        if (slot[0]=='L' && slot[1]=='E' && slot[2]=='N' && slot[3]=='S') {
            uint32_t stored_hash = (uint32_t)slot[4] | ((uint32_t)slot[5] << 8)
                                 | ((uint32_t)slot[6] << 16) | ((uint32_t)slot[7] << 24);
            uint16_t slen   = (uint16_t)(slot[8]  | ((uint16_t)slot[9]  << 8));
            uint16_t nd_len = (uint16_t)(slot[10] | ((uint16_t)slot[11] << 8));
            uint16_t ct_len = (uint16_t)(slot[12] | ((uint16_t)slot[13] << 8));
            const size_t HDR_LEN = LENS_SAVE_HDR_LEN;
            /* Reject a snapshot saved by a different build (struct layout may have
             * changed): fall through to the baked factory instead of applying stale. */
            if (stored_hash == g_build_hash && slen > 0 && slen <= LENS_SNAPSHOT_CACHE
                && nd_len <= LENS_NODESTATE_BYTES && ct_len <= LENS_CONTROL_BUFFER_BYTES) {
                const size_t payload_len = HDR_LEN + slen + nd_len + ct_len;
                uint32_t stored_crc =
                    (uint32_t)slot[payload_len + 0]        |
                    ((uint32_t)slot[payload_len + 1] << 8)  |
                    ((uint32_t)slot[payload_len + 2] << 16) |
                    ((uint32_t)slot[payload_len + 3] << 24);
                uint32_t computed_crc = save_crc32(slot, payload_len);
                if (stored_crc == computed_crc) {
                    int rc = snapshot_apply(&rt, slot + HDR_LEN, slen);
                    g_apply_attempts++;
                    g_last_apply_rc = rc;
                    if (rc == 0) {
                        /* Overlay saved live state onto the freshly-applied (zeroed)
                           pools: same snapshot -> same offsets, so raw bytes line up.
                           Runs before the first ProcessSample, so ops see the restored
                           state (e.g. pickup keeps its taken-over value, not init). */
                        if (nd_len) memcpy(lens_nodestate_pool, slot + HDR_LEN + slen, nd_len);
                        if (ct_len) memcpy(lens_control_pool,   slot + HDR_LEN + slen + nd_len, ct_len);
                        memcpy(g_last_snapshot, slot + HDR_LEN, slen);
                        g_last_snapshot_len = slen;
                        g_snapshot_crc = snapshot_trailer_crc(slot + HDR_LEN, slen);
                        g_apply_count++;
                        g_rt = rt;
                        slot_valid = true;
                    }
                }
            }
        }

        /* 2. Fall back to baked factory snapshot. */
        if (!slot_valid) {
            rt = nullptr;
            int rc = snapshot_apply(&rt, lens_factory, lens_factory_len);
            g_apply_attempts++;
            g_last_apply_rc = rc;
            if (rc == 0) {
                size_t flen = lens_factory_len;
                if (flen > LENS_SNAPSHOT_CACHE) flen = LENS_SNAPSHOT_CACHE;
                memcpy(g_last_snapshot, lens_factory, flen);
                g_last_snapshot_len = (uint16_t)flen;
                g_snapshot_crc = snapshot_trailer_crc(lens_factory, lens_factory_len);
                g_apply_count++;
                g_rt = rt;
            } else {
                return false;
            }
        }
        return true;
    }

protected:
    /* ProcessSample is the audio ISR body; pinned to RAM. */
    void __not_in_flash_func(ProcessSample)() override {
#if LENS_PERF_PROBE
        uint32_t t0 = dwt_read();
#endif

        /* Click-free swap: defer the pending apply until the audio output is near a
         * zero crossing, with a short timeout so a sustained drone still swaps. The
         * jacks are driven by Core 0 (this ISR), so last sample's output is known
         * here; no cross-core read is needed. Combined with node-state preservation,
         * a same-layout edit swaps continuously and any residual parameter jump lands
         * quietly. (No crossfade: that would run two graphs at once, which the RAM
         * and CPU budget cannot spare.) */
        static int32_t  g_last_out_1 = 0;
        static int32_t  g_last_out_2 = 0;
        static uint32_t g_swap_wait  = 0;
        /* Master-clock beat tracking (updated after the walk, below) for beat/bar
           swap timing: g_beat_now marks a downbeat sample, g_beat_count counts
           beats since the current patch landed (so beat 4/8/12 = a bar boundary). */
        static int32_t  g_master_prev = 0;
        static uint32_t g_beat_count  = 0;
        static bool     g_beat_now    = false;
        constexpr int32_t  kSwapZeroThresh = 32;       /* on the +-2047 audio scale */
        constexpr uint32_t kSwapMaxWait    = 480;      /* ~10 ms at 48 kHz */
        constexpr uint32_t kSwapMaxWaitClk = 192000u;  /* ~4 s: fallback if the clock is stopped */
        {
        bool near_zero = (g_last_out_1 > -kSwapZeroThresh && g_last_out_1 < kSwapZeroThresh &&
                          g_last_out_2 > -kSwapZeroThresh && g_last_out_2 < kSwapZeroThresh);
        uint8_t mode = g_swap_mode;
        bool have_master = g_rt && g_rt->master_slot_idx != 0xFFFFu;
        bool boundary;
        uint32_t maxwait;
        if (mode == SWAP_AT_BEAT && have_master) {
            boundary = g_beat_now;                                 maxwait = kSwapMaxWaitClk;
        } else if (mode == SWAP_AT_BAR && have_master) {
            boundary = g_beat_now && (g_beat_count % 4u == 0u);    maxwait = kSwapMaxWaitClk;
        } else {
            boundary = near_zero;                                  maxwait = kSwapMaxWait;
        }
        if (g_pending.ready && (boundary || g_swap_wait >= maxwait)) {
            g_swap_wait = 0;
            __dmb();   /* pair with the Core 1 writer: read len after seeing ready */
            struct LensRuntime* new_rt = nullptr;
            size_t plen = g_pending.len;
            int rc = snapshot_apply(&new_rt, g_pending.bytes, plen);
            g_apply_attempts++;
            g_last_apply_rc = rc;
            if (rc == 0) {
                if (plen <= LENS_SNAPSHOT_CACHE) {
                    memcpy(g_last_snapshot, g_pending.bytes, plen);
                    g_last_snapshot_len = (uint16_t)plen;
                }
                g_snapshot_crc = snapshot_trailer_crc(g_pending.bytes, plen);
                g_apply_count++;
                g_last_apply_sample = g_rt ? g_rt->sample_counter : 0u;
                struct LensRuntime* old_rt = g_rt;
                g_rt = new_rt;
                if (old_rt) runtime_destroy(old_rt);
                /* New patch starts at its own downbeat: restart beat tracking. */
                g_master_prev = 0; g_beat_count = 0; g_beat_now = false;
            }
            g_pending.ready = false;
        } else if (g_pending.ready) {
            g_swap_wait++;   /* a swap is queued but no boundary reached yet */
        }
        }

        /* Handle CMD_SAVE_STATE: erase + write flash slot then reboot. */
        if (g_save_requested) {
            g_save_requested = false;
            multicore_reset_core1();
            do_flash_save();
            watchdog_reboot(0, 0, 0);
            for (;;) {} /* wait for watchdog */
        }

        /* Handle CMD_FACTORY_RESET: erase flash slot then reboot. */
        if (g_factory_reset_requested) {
            g_factory_reset_requested = false;
            multicore_reset_core1();
            do_flash_erase_slot();
            watchdog_reboot(0, 0, 0);
            for (;;) {}
        }

        struct LensRuntime* rt = g_rt;
        if (!rt) return;

        /* 1. Gather hardware inputs. */
#if LENS_PERF_PROBE
        uint32_t t_io_start = dwt_read();
#endif
        struct HardwareInputs hw_in;
        hw_in.knob_main  = KnobVal(Main);
        hw_in.knob_x     = KnobVal(X);
        hw_in.knob_y     = KnobVal(Y);
        hw_in.cv_in_1    = CVIn1() + VMID;
        hw_in.cv_in_2    = CVIn2() + VMID;
        hw_in.audio_in_1 = AudioIn1();
        hw_in.audio_in_2 = AudioIn2();
        hw_in.pulse_in_1 = PulseIn1() ? VMAX : 0;
        hw_in.pulse_in_2 = PulseIn2() ? VMAX : 0;
        hw_in.switch_pos = static_cast<int32_t>(SwitchVal());
        /* Jack-connection mask (normalisation probe), indexed to match hw_scratch. */
        hw_in.connected =
              (Connected(Input::Audio1) ? (1u << 0) : 0)
            | (Connected(Input::Audio2) ? (1u << 1) : 0)
            | (Connected(Input::Pulse1) ? (1u << 2) : 0)
            | (Connected(Input::Pulse2) ? (1u << 3) : 0)
            | (Connected(Input::CV1)    ? (1u << 4) : 0)
            | (Connected(Input::CV2)    ? (1u << 5) : 0);
#if LENS_PERF_PROBE
        uint32_t t_io_in_done = dwt_read();
#endif

        struct HardwareOutputs hw_out = {};

        /* 2. Run the audio slice. Kernels write their output during the walk;
         * recordhead_sweep applies any deferred tape writes at end-of-tick. */
#if LENS_PERF_PROBE
        uint32_t t_walk_start = dwt_read();
#endif
        /* Non-blocking dual-core: Core 0 NEVER waits for Core 1. It rings Core 1
         * each sample (Core 1 free-runs and processes the latest seq when it can),
         * then walks its own slots, commits its own recordheads, publishes its own
         * shadows, and drives outputs. If Core 1 is late (e.g. a USB burst), its
         * shadows simply hold last frame's value -- one more sample of lag on the
         * cross-core CV, never a stall. Each core only ever touches its own state,
         * and terminal feeders are pinned to Core 0, so the outputs are clean. A
         * patch with no Core 1 slots runs the same path; Core 1 just walks nothing. */
        {
            uint32_t seq = rt->sample_counter;
            runtime_update_hw_scratch(&hw_in);  /* both cores read scratch */
            __dmb();                            /* scratch + Core 0 shadows visible before ring */
            sio_hw->fifo_wr = seq;              /* triggers SIO_IRQ_PROC1 on Core 1 */
            runtime_walk_core0(rt, seq);
            recordhead_sweep_core0(rt);
            runtime_publish_shadows_core0(rt);
            runtime_drive_terminals(rt, &hw_out);
            rt->sample_counter = seq + 1;
        }

        /* Beat detect on the master clock for beat/bar-quantised live swaps: a rising
           edge through a low threshold marks the downbeat (one detector serves a ramp
           clock, which crosses just after its wrap, and a pulse, which crosses on
           arrival). Mirrors the runtime's own clock edge detector. */
        if (rt->master_slot_idx != 0xFFFFu && rt->master_slot_idx < rt->slot_count) {
            int32_t mv = *(volatile int32_t*)rt->slots[rt->master_slot_idx].out;
            constexpr int32_t kBeatLevel = VMAX / 16;
            g_beat_now = (mv > kBeatLevel) && (g_master_prev <= kBeatLevel);
            g_master_prev = mv;
            if (g_beat_now) g_beat_count++;
        } else {
            g_beat_now = false;
        }

        /* Output-change journal: append an entry whenever the driven triple moves,
         * carrying the audio_out_2 upward-zero-crossing count over the dwell just
         * ended (rendered frequency = zc2 * 48000 / dwell samples). */
        {
            static uint16_t last_cv1 = 0xFFFFu, last_cv2 = 0xFFFFu, last_led0 = 0xFFFFu;
            static int32_t  zc_prev  = 0;
            static uint32_t zc_count = 0;
            int32_t a2 = hw_out.audio_out_2;
            if (zc_prev < 0 && a2 >= 0) zc_count++;
            zc_prev = a2;
            uint16_t c1 = (uint16_t)hw_out.cv_out_1;
            uint16_t c2 = (uint16_t)hw_out.cv_out_2;
            uint16_t l0 = (uint16_t)hw_out.led_0;
            if (c1 != last_cv1 || c2 != last_cv2 || l0 != last_led0) {
                last_cv1 = c1; last_cv2 = c2; last_led0 = l0;
                volatile BeatLogEntry* e = &g_beatlog[g_beatlog_head & 63u];
                e->sample = rt->sample_counter;
                e->cv1    = c1;
                e->cv2    = c2;
                e->zc2    = (uint16_t)(zc_count > 0xFFFFu ? 0xFFFFu : zc_count);
                e->led0   = (uint8_t)l0;
                e->beat   = (uint8_t)g_beat_count;
                zc_count  = 0;
                __dmb();
                g_beatlog_head++;
            }
        }
#if LENS_PERF_PROBE
        uint32_t t_walk_done = dwt_read();
#endif

        /* Tempo blink on led-5 lives in the prelude as a default cable;
         * no main.cpp override needed. */

        /* 3b. Boot dance: L flipping with upside-down L (Γ) every 0.25 s.
         * ComputerCard LED indices:  0 1
         *                            2 3
         *                            4 5
         *   0x35 = {0,2,4,5}  upright L (left column + bottom-right foot)
         *   0x2B = {0,1,3,5}  inverted Γ (top row + right column + bottom-right)
         * 6 frames over kBootDanceSamples (~1.5 s).
         */
        if (g_boot_sample < kBootDanceSamples) {
            uint32_t frame = (g_boot_sample * 6u) / kBootDanceSamples; /* 0..5 */
            uint8_t  shape = (frame & 1u) ? 0x2Bu : 0x35u;
            hw_out.led_0 = (shape & 0x01u) ? VMAX : 0;
            hw_out.led_1 = (shape & 0x02u) ? VMAX : 0;
            hw_out.led_2 = (shape & 0x04u) ? VMAX : 0;
            hw_out.led_3 = (shape & 0x08u) ? VMAX : 0;
            hw_out.led_4 = (shape & 0x10u) ? VMAX : 0;
            hw_out.led_5 = (shape & 0x20u) ? VMAX : 0;
            g_boot_sample++;
        }

        /* 3c. Save-arm: Z-switch held DOWN ramps all LEDs (highest priority). */
        if (hw_in.switch_pos == 0) {
            if (g_save_arm_count < kSaveArmHoldSamples) {
                g_save_arm_count++;
            }
        } else {
            g_save_arm_count = 0u;
        }

        if (g_save_arm_count >= kSaveArmHoldSamples) {
            lens_request_save();
            g_save_arm_count = 0u;
        }

        if (g_save_arm_count > 0u) {
            /* 32-bit only: scale count into 0..VMAX without 64-bit multiply.
             * count/kSaveArmHoldSamples * VMAX = (count >> 6) * VMAX / (kSaveArmHoldSamples >> 6)
             * kSaveArmHoldSamples >> 6 = 3750; count >> 6 fits in 16 bits. */
            int32_t ramp = (int32_t)((g_save_arm_count >> 6) * (uint32_t)VMAX
                                     / (kSaveArmHoldSamples >> 6));
            hw_out.led_0 = ramp;
            hw_out.led_1 = ramp;
            hw_out.led_2 = ramp;
            hw_out.led_3 = ramp;
            hw_out.led_4 = ramp;
            hw_out.led_5 = ramp;
        }

        /* 4. Drive outputs. */

        /* Minimum pulse width: hold each pulse output high for at least 5 ms
           (~240 samples at 48 kHz) to guarantee Eurorack gear sees the trigger. */
        static uint32_t g_pulse_hold[2] = { 0, 0 };
        static bool     g_pulse_prev[2] = { false, false };
        constexpr uint32_t kPulseMinSamples = 240;

#if LENS_PERF_PROBE
        uint32_t t_io_out_start = dwt_read();
#endif
        AudioOut1(static_cast<int16_t>(hw_out.audio_out_1));
        AudioOut2(static_cast<int16_t>(hw_out.audio_out_2));
        /* Remember this sample's audio out so the next pending swap can wait for a
           zero crossing (see the click-free swap gate at the top of ProcessSample). */
        g_last_out_1 = hw_out.audio_out_1;
        g_last_out_2 = hw_out.audio_out_2;
        /* A pitch cv-out (source is v-oct) carries a MIDI note: use the card's
           per-unit calibrated 1V/oct path. Otherwise drive the raw value. */
        if (hw_out.cv_out_1_is_pitch) CVOut1MIDINote(static_cast<uint8_t>(hw_out.cv_out_1));
        else                          CVOut1(static_cast<int16_t>(hw_out.cv_out_1));
        if (hw_out.cv_out_2_is_pitch) CVOut2MIDINote(static_cast<uint8_t>(hw_out.cv_out_2));
        else                          CVOut2(static_cast<int16_t>(hw_out.cv_out_2));

        {
            bool high_now = hw_out.pulse_out_1 > VMID;
            if (high_now && !g_pulse_prev[0]) g_pulse_hold[0] = kPulseMinSamples;
            g_pulse_prev[0] = high_now;
            bool out = high_now || g_pulse_hold[0] > 0;
            if (g_pulse_hold[0] > 0) g_pulse_hold[0]--;
            PulseOut1(out);
        }
        {
            bool high_now = hw_out.pulse_out_2 > VMID;
            if (high_now && !g_pulse_prev[1]) g_pulse_hold[1] = kPulseMinSamples;
            g_pulse_prev[1] = high_now;
            bool out = high_now || g_pulse_hold[1] > 0;
            if (g_pulse_hold[1] > 0) g_pulse_hold[1]--;
            PulseOut2(out);
        }

        LedBrightness(0, static_cast<uint16_t>(hw_out.led_0));
        LedBrightness(1, static_cast<uint16_t>(hw_out.led_1));
        LedBrightness(2, static_cast<uint16_t>(hw_out.led_2));
        LedBrightness(3, static_cast<uint16_t>(hw_out.led_3));
        LedBrightness(4, static_cast<uint16_t>(hw_out.led_4));
        LedBrightness(5, static_cast<uint16_t>(hw_out.led_5));

#if LENS_PERF_PROBE
        /* SysTick is a 24-bit DOWN counter: elapsed = (start - end) & 0xFFFFFF. */
        uint32_t t_end = dwt_read();
        uint32_t c_walk  = (t_walk_start - t_walk_done) & 0x00FFFFFFu;
        uint32_t c_io    = ((t_io_start    - t_io_in_done)   & 0x00FFFFFFu)
                         + ((t_io_out_start - t_end)          & 0x00FFFFFFu);
        uint32_t c_total = (t0 - t_end) & 0x00FFFFFFu;
        struct PerfRingEntry* e = &perf_ring[perf_head & 1023u];
        e->walk  = c_walk  > 0xFFFFu ? 0xFFFFu : (uint16_t)c_walk;
        e->io    = c_io    > 0xFFFFu ? 0xFFFFu : (uint16_t)c_io;
        e->total = c_total > 0xFFFFu ? 0xFFFFu : (uint16_t)c_total;
        perf_head++;
#endif
    }
};

/* ---- Core 1: TinyUSB + audio slice via FIFO doorbell ---- */

/*
 * SIO FIFO doorbell IRQ. Core 0 writes the sample sequence number to the FIFO
 * each sample; this IRQ drains to the latest seq and runs Core 1's walk. The
 * IRQ priority is set BELOW TinyUSB (0xC0 vs USB's 0x80) because a dropped
 * USB MIDI byte cannot be recovered (sysex parser desyncs and the whole
 * patch transfer corrupts), while a late audio slice just trips Core 0's
 * spin budget and the sample drops cleanly via the one-sample-lag semantics.
 */
/* Diagnostic: how many times Core 1 has actually run its walk. Reported in PERF so
   we can tell a real dual-core run (>0) from a silent Core 1 (==0). */
static volatile uint32_t g_core1_loops = 0;

static void __not_in_flash_func(core1_doorbell_irq)(void) {
    uint32_t seq = 0;
    bool got = false;
    while (multicore_fifo_rvalid()) { seq = sio_hw->fifo_rd; got = true; }
    multicore_fifo_clear_irq();
    if (!got) return;
    struct LensRuntime* rt = g_rt;
    if (rt) {
        /* Self-contained frame: walk Core 1's slots, commit ITS recordheads, and
         * publish ITS shadows. Core 0 only ever reads these shadows, never Core 1's
         * live state, so there is nothing for Core 0 to wait on. */
        runtime_walk_core1(rt, seq);
        recordhead_sweep_core1(rt);
        runtime_publish_shadows_core1(rt);
        g_core1_loops++;
        rt->core1_done = seq;
    }
}

/* The host MIDI driver calls this (weak) when a mounted device has RX packets.
 * Drain the decoded byte stream straight into the transport-independent parser. */
extern "C" void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets) {
    (void)num_packets;
    uint8_t cable;
    uint8_t buf[64];
    uint32_t n;
    while ((n = tuh_midi_stream_read(dev_addr, &cable, buf, sizeof(buf))) > 0)
        for (uint32_t i = 0; i < n; i++) midi_feed_byte(buf[i]);
}

/* ---- USB foreground loops (called from core1_entry) ---- */

/* Host loop: tuh_task drives enumeration; tuh_midi_rx_cb feeds the parser.
 * Run-loop split credited to Music Thing Workshop System 33_drumdrum. */
static void run_host_loop(void) {
    while (!g_cancellation_requested.load(std::memory_order_relaxed)) {
        tuh_task();
    }
}

/* Device loop: SysEx CLI + DAW MIDI over the USB device stack. */
static void run_device_loop(void) {
    /* ParserState carries a ~9 KB wire_buf; Core 1's stack is 2 KB.
     * Static keeps it in BSS. */
    static lenssysex::ParserState parser;
    lenssysex::init(&parser);

    while (!g_cancellation_requested.load(std::memory_order_relaxed)) {
        tud_task();

        /* Drain incoming MIDI as a decoded byte stream. tud_midi_stream_read
         * handles USB MIDI CIN tags and yields pure data bytes, including
         * CIN=0xF which macOS CoreMIDI uses to fragment large sysex transfers. */
        uint8_t in_buf[64];
        while (tud_midi_available()) {
            uint32_t n = tud_midi_stream_read(in_buf, sizeof(in_buf));
            for (uint32_t bi = 0; bi < n; bi++) {
                midi_feed_byte(in_buf[bi]);
                if (lenssysex::feed_byte(&parser, in_buf[bi])) {
                    /* Complete frame received. */
                    uint8_t cmd = lenssysex::get_command(&parser);
                    switch (cmd) {

                    case lenssysex::CMD_WRITE_STATE:
                        if (g_pending.ready) {
                            /* Previous patch still queued; reject to avoid overwrite. */
                            lenssysex::sysex_send_nack(lenssysex::CMD_WRITE_STATE,
                                                        lenssysex::NACK_BUSY);
                        } else {
                            size_t plen = lenssysex::get_payload(
                                    &parser, g_pending.bytes, lenssysex::SOURCE_CAP);
                            if (plen > 0) {
                                g_pending.len   = plen;
                                __dmb();   /* len visible before Core 0 observes ready */
                                g_pending.ready = true;
                                /* ACK after queuing; Core 0 applies on next sample. */
                                lenssysex::sysex_send_frame(lenssysex::CMD_ACK, nullptr, 0);
                            } else {
                                lenssysex::sysex_send_nack(lenssysex::CMD_WRITE_STATE,
                                                            lenssysex::NACK_BAD_LENGTH);
                            }
                        }
                        break;

                    case lenssysex::CMD_PING:
                        lenssysex::sysex_send_frame(lenssysex::CMD_ACK, nullptr, 0);
                        break;

                    case lenssysex::CMD_SWAP_MODE: {
                        /* 1 payload byte selects when a live WRITE_STATE swaps in:
                           0 = next near-zero sample, 1 = next beat, 2 = next bar. */
                        uint8_t mb[1];
                        size_t n = lenssysex::get_payload(&parser, mb, sizeof(mb));
                        if (n >= 1 && mb[0] <= SWAP_AT_BAR) g_swap_mode = mb[0];
                        lenssysex::sysex_send_frame(lenssysex::CMD_ACK, nullptr, 0);
                        break;
                    }

                    case lenssysex::CMD_BEAT_LOG: {
                        /* head u32 LE, then 64 entries {sample u32, cv1 u16, cv2 u16,
                         * zc2 u16, led0 u8, beat u8}, ring order. Racy against Core 0's
                         * writer by design: at most the newest entry is in flight. */
                        uint8_t d[4 + 64 * 12]; size_t i = 0;
                        auto u32 = [&](uint32_t v) {
                            d[i++] = (uint8_t)v; d[i++] = (uint8_t)(v >> 8);
                            d[i++] = (uint8_t)(v >> 16); d[i++] = (uint8_t)(v >> 24);
                        };
                        auto u16 = [&](uint16_t v) {
                            d[i++] = (uint8_t)v; d[i++] = (uint8_t)(v >> 8);
                        };
                        u32(g_beatlog_head);
                        for (uint32_t k = 0; k < 64u; k++) {
                            const volatile BeatLogEntry* e = &g_beatlog[k];
                            u32(e->sample); u16(e->cv1); u16(e->cv2); u16(e->zc2);
                            d[i++] = e->led0; d[i++] = e->beat;
                        }
                        lenssysex::sysex_send_frame(lenssysex::CMD_BEAT_LOG_DUMP, d, i);
                        break;
                    }

                    case lenssysex::CMD_DIAG: {
                        /* DIAG_DUMP payload (v2):
                           ver u8, _pad u8, _pad u16,
                           build_hash u32, snapshot_crc u32,
                           last_apply_rc i32, apply_count u32, apply_attempts u32,
                           pending_ready u8, _pad u8, pending_len u16,
                           sample_counter u32, last_apply_sample u32,
                           snapshot_len u16, _pad u16,
                           core1_done u32,                                  = 44 bytes (identity block)
                           cycles_avg_total u32, cycles_max_total u32,
                           cycles_avg_walk u32, cycles_avg_io u32,
                           sysclk_hz u32, samples_window u32               = 24 bytes (perf block)
                                                                            = 68 bytes total
                           perf fields are zero when LENS_PERF_PROBE=0 */
                        uint8_t d[68]; size_t i = 0;
                        auto u32 = [&](uint32_t v) {
                            d[i++] = (uint8_t)v; d[i++] = (uint8_t)(v >> 8);
                            d[i++] = (uint8_t)(v >> 16); d[i++] = (uint8_t)(v >> 24);
                        };
                        auto u16 = [&](uint16_t v) {
                            d[i++] = (uint8_t)v; d[i++] = (uint8_t)(v >> 8);
                        };
                        struct LensRuntime* rt = g_rt;
                        uint32_t scnt = rt ? rt->sample_counter : 0u;
                        uint32_t c1d  = rt ? rt->core1_done      : 0u;
                        d[i++] = 2; d[i++] = 0; d[i++] = 0; d[i++] = 0;
                        u32(g_build_hash);
                        u32(g_snapshot_crc);
                        u32((uint32_t)g_last_apply_rc);
                        u32(g_apply_count);
                        u32(g_apply_attempts);
                        d[i++] = g_pending.ready ? 1u : 0u;
                        d[i++] = 0;
                        u16((uint16_t)g_pending.len);
                        u32(scnt);
                        u32(g_last_apply_sample);
                        u16(g_last_snapshot_len);
                        u16(0);
                        u32(c1d);
#if LENS_PERF_PROBE
                        {
                            uint32_t head    = *lens_perf_head_p;
                            uint32_t count_n = head < lens_perf_ring_len
                                             ? head : lens_perf_ring_len;
                            uint32_t sum_total = 0, mx_total = 0;
                            uint32_t sum_walk  = 0, sum_io   = 0;
                            uint32_t pstart = (head >= lens_perf_ring_len)
                                            ? (head & 1023u) : 0u;
                            for (uint32_t k = 0; k < count_n; k++) {
                                const volatile struct PerfRingEntry* e =
                                    &lens_perf_ring[(pstart + k) & 1023u];
                                uint32_t t  = e->total;
                                uint32_t w  = e->walk;
                                uint32_t io = e->io;
                                sum_total += t;
                                sum_walk  += w;
                                sum_io    += io;
                                if (t > mx_total) mx_total = t;
                            }
                            u32(count_n ? (sum_total / count_n) : 0u);
                            u32(mx_total);
                            u32(count_n ? (sum_walk  / count_n) : 0u);
                            u32(count_n ? (sum_io    / count_n) : 0u);
                            u32(clock_get_hz(clk_sys));
                            u32(count_n);
                        }
#else
                        u32(0); u32(0); u32(0); u32(0); u32(0); u32(0);
#endif
                        lenssysex::sysex_send_frame(lenssysex::CMD_DIAG_DUMP, d, sizeof(d));
                        break;
                    }

                    case lenssysex::CMD_SAVE_STATE:
                        /* ACK first, then signal Core 0 to perform the flash write. */
                        lenssysex::sysex_send_frame(lenssysex::CMD_ACK, nullptr, 0);
                        lens_request_save();
                        break;

                    case lenssysex::CMD_FACTORY_RESET:
                        lenssysex::sysex_send_frame(lenssysex::CMD_ACK, nullptr, 0);
                        lens_request_factory_reset();
                        break;

                    case lenssysex::CMD_READ_PERF: {
#if LENS_PERF_PROBE
                        /* Snapshot ring under Core 1 (ring written by Core 0; each
                           PerfRingEntry is 6 bytes; the three uint16_t writes are not
                           guaranteed atomic, but perf data is diagnostic only). */
                        uint32_t head = *lens_perf_head_p;
                        uint32_t count_n = head < lens_perf_ring_len
                                         ? head : lens_perf_ring_len;
                        uint32_t sum_total = 0, mx_total = 0;
                        uint32_t sum_walk  = 0, sum_io   = 0;
                        uint32_t start = (head >= lens_perf_ring_len)
                                       ? (head & 1023u) : 0u;
                        for (uint32_t k = 0; k < count_n; k++) {
                            const volatile struct PerfRingEntry* e =
                                &lens_perf_ring[(start + k) & 1023u];
                            uint32_t t = e->total;
                            uint32_t w = e->walk;
                            uint32_t io = e->io;
                            sum_total += t;
                            sum_walk  += w;
                            sum_io    += io;
                            if (t > mx_total) mx_total = t;
                        }
                        uint32_t avg = count_n ? (sum_total / count_n) : 0u;
                        uint32_t mx  = mx_total;
                        uint32_t avg_walk = count_n ? (sum_walk / count_n) : 0u;
                        uint32_t avg_io   = count_n ? (sum_io   / count_n) : 0u;

                        /* Response payload (PERF_DUMP):
                           ver u8, sec_count u8, count u16, reserved u16, reserved u16,
                           sysclk u32, samples u32, reserved u32, core1_loops u32,
                           total_avg u32, total_max u32, reserved u32, reserved u32,
                           walk_avg u32, io_avg u32  (sec_count=2 sections). */
                        uint8_t perf[48];
                        size_t  pi = 0;
                        perf[pi++] = 1;          /* ver */
                        perf[pi++] = 2;          /* sec_count: walk + io */
                        perf[pi++] = (uint8_t)(count_n & 0xFFu);
                        perf[pi++] = (uint8_t)((count_n >> 8) & 0xFFu); /* count u16 */
                        perf[pi++] = 0; perf[pi++] = 0;                 /* reserved u16 */
                        perf[pi++] = 0; perf[pi++] = 0;                 /* reserved u16 */
                        /* sysclk u32 = 250_000_000 = 0x0EE6B280 LE */
                        perf[pi++] = 0x80; perf[pi++] = 0xB2; perf[pi++] = 0xE6; perf[pi++] = 0x0E;
                        /* samples = head */
                        perf[pi++] = (uint8_t)(head & 0xFFu);
                        perf[pi++] = (uint8_t)((head >> 8)  & 0xFFu);
                        perf[pi++] = (uint8_t)((head >> 16) & 0xFFu);
                        perf[pi++] = (uint8_t)((head >> 24) & 0xFFu);
                        /* reserved u32 */
                        perf[pi++] = 0; perf[pi++] = 0; perf[pi++] = 0; perf[pi++] = 0;
                        /* core1_loops: real count of Core 1 walks since boot */
                        { uint32_t cl = g_core1_loops;
                          perf[pi++] = (uint8_t)(cl & 0xFFu);       perf[pi++] = (uint8_t)((cl >> 8) & 0xFFu);
                          perf[pi++] = (uint8_t)((cl >> 16) & 0xFFu); perf[pi++] = (uint8_t)((cl >> 24) & 0xFFu); }
                        /* total_avg */
                        perf[pi++] = (uint8_t)(avg & 0xFFu);
                        perf[pi++] = (uint8_t)((avg >> 8)  & 0xFFu);
                        perf[pi++] = (uint8_t)((avg >> 16) & 0xFFu);
                        perf[pi++] = (uint8_t)((avg >> 24) & 0xFFu);
                        /* total_max */
                        perf[pi++] = (uint8_t)(mx & 0xFFu);
                        perf[pi++] = (uint8_t)((mx >> 8)  & 0xFFu);
                        perf[pi++] = (uint8_t)((mx >> 16) & 0xFFu);
                        perf[pi++] = (uint8_t)((mx >> 24) & 0xFFu);
                        /* reserved u32 x2 */
                        perf[pi++] = 0; perf[pi++] = 0; perf[pi++] = 0; perf[pi++] = 0;
                        perf[pi++] = 0; perf[pi++] = 0; perf[pi++] = 0; perf[pi++] = 0;
                        /* section averages: walk_avg, io_avg */
                        perf[pi++] = (uint8_t)(avg_walk & 0xFFu);
                        perf[pi++] = (uint8_t)((avg_walk >> 8)  & 0xFFu);
                        perf[pi++] = (uint8_t)((avg_walk >> 16) & 0xFFu);
                        perf[pi++] = (uint8_t)((avg_walk >> 24) & 0xFFu);
                        perf[pi++] = (uint8_t)(avg_io & 0xFFu);
                        perf[pi++] = (uint8_t)((avg_io >> 8)  & 0xFFu);
                        perf[pi++] = (uint8_t)((avg_io >> 16) & 0xFFu);
                        perf[pi++] = (uint8_t)((avg_io >> 24) & 0xFFu);
                        lenssysex::sysex_send_frame(lenssysex::CMD_PERF_DUMP, perf, pi);
#else
                        /* SPEC: perf ring not compiled in; reply ACK stub. */
                        lenssysex::sysex_send_frame(lenssysex::CMD_ACK, nullptr, 0);
#endif
                        break;
                    }

                    case lenssysex::CMD_SLOT_PERF: {
#if LENS_PERF_PROBE
                        struct LensRuntime* rt = g_rt;
                        uint16_t sc = rt ? rt->slot_count : 0u;
                        /* Payload: u16 slot_count, then per slot { u32 total, u32 max, u32 calls }
                           = 2 + sc*12 bytes. Cap at LENS_MAX_SLOTS. */
                        if (sc > LENS_MAX_SLOTS) sc = LENS_MAX_SLOTS;
                        const size_t payload_len = 2u + (size_t)sc * 12u;
                        /* Use a static buffer; 2 + 256*12 = 3074 bytes. */
                        static uint8_t slot_perf_buf[2 + LENS_MAX_SLOTS * 12];
                        slot_perf_buf[0] = (uint8_t)(sc & 0xFFu);
                        slot_perf_buf[1] = (uint8_t)((sc >> 8) & 0xFFu);
                        for (uint16_t si = 0; si < sc; si++) {
                            size_t o = 2u + (size_t)si * 12u;
                            uint32_t tot = rt->slot_cycle_total[si];
                            uint32_t mx  = rt->slot_cycle_max[si];
                            uint32_t cnt = rt->slot_call_count[si];
                            slot_perf_buf[o+ 0] = (uint8_t)(tot);
                            slot_perf_buf[o+ 1] = (uint8_t)(tot >> 8);
                            slot_perf_buf[o+ 2] = (uint8_t)(tot >> 16);
                            slot_perf_buf[o+ 3] = (uint8_t)(tot >> 24);
                            slot_perf_buf[o+ 4] = (uint8_t)(mx);
                            slot_perf_buf[o+ 5] = (uint8_t)(mx  >> 8);
                            slot_perf_buf[o+ 6] = (uint8_t)(mx  >> 16);
                            slot_perf_buf[o+ 7] = (uint8_t)(mx  >> 24);
                            slot_perf_buf[o+ 8] = (uint8_t)(cnt);
                            slot_perf_buf[o+ 9] = (uint8_t)(cnt >> 8);
                            slot_perf_buf[o+10] = (uint8_t)(cnt >> 16);
                            slot_perf_buf[o+11] = (uint8_t)(cnt >> 24);
                        }
                        lenssysex::sysex_send_frame(lenssysex::CMD_SLOT_PERF_DUMP,
                                                     slot_perf_buf, payload_len);
#else
                        lenssysex::sysex_send_frame(lenssysex::CMD_ACK, nullptr, 0);
#endif
                        break;
                    }

                    default:
                        /* Unknown command: NACK with reason. */
                        lenssysex::sysex_send_nack(cmd, lenssysex::NACK_UNKNOWN_CMD);
                        break;
                    }
                }
            }
        }

        /* Drain MIDI output ring -> USB MIDI TX (device-only). */
        if (tud_midi_mounted()) {
            uint8_t midi_tx_buf[4];
            uint8_t midi_tx_len;
            while ((midi_tx_len = midi_out_pop(midi_tx_buf)) > 0)
                tud_midi_stream_write(0, midi_tx_buf, midi_tx_len);
        }

        /* Audio work is in the FIFO doorbell IRQ (see core1_doorbell_irq).
         * The foreground loop owns USB only. */
    }
}

/* The app-level MIDI host driver registers itself: usb_midi_host_app_driver.c
 * provides usbh_app_driver_get_cb, called once during tuh_rhport_init. */

/* ---- Core 1 entry ---- */

static void __not_in_flash_func(core1_entry)(void) {
    /* USB stack init runs on Core 1 so the USB IRQ lands on Core 1's NVIC.
     * board_init() runs here in host mode, on Core 0 in device mode (role split
     * below and in main()). 33_drumdrum inits the device stack on Core 0 instead.
     * RHPORT0 is HOST|DEVICE, so tusb_init() (not tuh_init) configures the
     * controller for the chosen role (per 33_drumdrum). */
    bool host = (bool)g_usb_host_mode;
    if (host) { board_init(); tusb_init(); } else tud_init(0);

    multicore_fifo_clear_irq();
    irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_doorbell_irq);
    irq_set_priority(SIO_IRQ_PROC1, 0xC0);
    irq_set_enabled(SIO_IRQ_PROC1, true);

    midi_reset();

    if (host) run_host_loop(); else run_device_loop();
}

/* ---- main ---- */

int main(void) {
    /* SPEC: 250 MHz overclock at 1.15 V per spec line 75.
     * Regulator must settle before the PLL jump (prototype pattern). */
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(1);
    set_sys_clock_khz(250000, true);

    /*
     * Construct LensCard + apply factory snapshot BEFORE Core 1 launches.
     * Otherwise Core 1 calls tud_init while Core 0 is still in
     * ComputerCard's constructor, racing the USB peripheral setup.
     */
    static LensCard card;
    card.init();
    card.EnableNormalisationProbe();
    dwt_enable();

    /* Read USB-C CC pins to select USB role for this power cycle.
     * DFP = a downstream device (keyboard) is plugged in = host mode.
     * UFP (computer) or Unsupported (non-Rev1_1 board) = device mode.
     * Detection runs on Core 0 before Core 1 launches; USB init is deferred to Core 1.
     * Role-detection approach credited to Music Thing Workshop System 33_drumdrum. */
    g_usb_host_mode = (card.ReadUSBPowerState() == ComputerCard::DFP) ? 1u : 0u;

    /* board_init() (rp2040 USB hardware bring-up) runs on Core 0 in device mode;
     * in host mode Core 1 runs it, so Core 0 skips it here. */
    if (!g_usb_host_mode) board_init();
    multicore_launch_core1(core1_entry);

    card.Run(); /* never returns */
    return 0;
}


// ──────────────────────────────────────────────────────────────────────────────
// Source: runtime/sysex.cpp
// ──────────────────────────────────────────────────────────────────────────────

/*
 * Lens sysex receive parser + 8-into-7 decoder + frame sender.
 * No malloc; all buffers are static.
 */

#include "sysex.h"
/* stripped tusb include */
/* stripped system include */

namespace lenssysex {

void init(ParserState* p) {
    p->state    = St::IDLE;
    p->cmd      = 0;
    p->wire_len = 0;
}

bool feed_byte(ParserState* p, uint8_t b) {
    switch (p->state) {
    case St::IDLE:
        if (b == 0xF0) { p->state = St::PRE1; p->wire_len = 0; }
        return false;
    case St::PRE1:
        p->state = (b == 0x7D) ? St::PRE2 : St::IDLE;
        return false;
    case St::PRE2:
        p->state = (b == 0x4C) ? St::PRE3 : St::IDLE;
        return false;
    case St::PRE3:
        p->state = (b == 0x45) ? St::CMD : St::IDLE;
        return false;
    case St::CMD:
        p->cmd   = b;
        p->state = St::PAYLOAD;
        return false;
    case St::PAYLOAD:
        if (b == 0xF7) {
            p->state = St::IDLE;
            return true;
        }
        /* Discard if overrun; keep state machine alive. */
        if (p->wire_len < WIRE_CAP)
            p->wire_buf[p->wire_len++] = b;
        return false;
    }
    return false;
}

/*
 * 8-into-7 decode.
 *
 * Each group of 8 wire bytes encodes 7 source bytes:
 *   wire[0] holds the high bits: bit k -> bit 7 of out[k]  for k in 0..6.
 *   wire[1..7] hold the low 7 bits of out[0..6].
 *
 * A trailing partial group is valid: k wire bytes (1 <= k <= 8) encode k-1 source bytes.
 * (A lone high-bits byte with no data bytes encodes nothing; k==1 yields 0 source bytes.)
 */
size_t get_payload(ParserState* p, uint8_t* out, size_t out_cap) {
    const uint8_t* src = p->wire_buf;
    size_t         rem = p->wire_len;
    size_t         n   = 0;

    while (rem >= 1) {
        size_t group = rem < 8 ? rem : 8;
        uint8_t hi   = src[0];
        size_t  data = group - 1; /* number of source bytes from this group */

        if (n + data > out_cap) return 0; /* output overflow */

        for (size_t i = 0; i < data; i++) {
            uint8_t lo  = src[1 + i];
            uint8_t msb = (hi >> i) & 1u;
            out[n++] = lo | (uint8_t)(msb << 7);
        }

        src += group;
        rem -= group;
    }

    return n;
}

uint8_t get_command(ParserState* p) {
    return p->cmd;
}

/*
 * 8-into-7 encode (inverse of get_payload).
 *
 * Each group of 7 source bytes encodes 8 wire bytes:
 *   wire[0] = high bits: bit k <- bit 7 of src[k]  for k in 0..6.
 *   wire[1..7] = low 7 bits of src[0..6].
 * A trailing partial group of k source bytes (1<=k<=7) encodes k+1 wire bytes.
 */
static size_t pack78(const uint8_t* src, size_t src_len, uint8_t* dst) {
    size_t out = 0;
    while (src_len > 0) {
        size_t group = src_len < 7 ? src_len : 7;
        uint8_t hi = 0;
        for (size_t i = 0; i < group; i++)
            hi |= (uint8_t)(((src[i] >> 7) & 1u) << i);
        dst[out++] = hi;
        for (size_t i = 0; i < group; i++)
            dst[out++] = src[i] & 0x7Fu;
        src     += group;
        src_len -= group;
    }
    return out;
}

/* Send a NACK frame: F0 7D 4C 45 CMD_NACK <orig_cmd> <reason> F7 */
void sysex_send_nack(uint8_t orig_cmd, uint8_t reason) {
    uint8_t body[2] = { orig_cmd, reason };
    sysex_send_frame(CMD_NACK, body, 2);
}

void sysex_send_frame(uint8_t cmd, const uint8_t* payload, size_t len) {
    /*
     * Build wire bytes directly into one static buffer:
     *   F0 7D 4C 45 cmd <pack78(payload)> F7
     * Sized for the largest payload we send (snapshot + slot-perf).
     */
    const size_t HDR = 5;
    static uint8_t raw[HDR + WIRE_CAP + 1];
    raw[0] = 0xF0;
    raw[1] = 0x7D;
    raw[2] = 0x4C;
    raw[3] = 0x45;
    raw[4] = cmd;

    size_t wire_len = 0;
    if (len > 0 && len <= SOURCE_CAP) {
        wire_len = pack78(payload, len, raw + HDR);
    }
    raw[HDR + wire_len] = 0xF7;
    const size_t raw_len = HDR + wire_len + 1;

    /* Packetise into 3-data-byte USB MIDI packets. */
    size_t pos = 0;
    while (pos < raw_len) {
        size_t left = raw_len - pos;
        uint8_t pkt[4] = { 0, 0, 0, 0 };
        if (left >= 3) {
            bool is_end = (pos + 3 == raw_len);
            if (is_end) {
                /* Last 3 bytes: check if F7 is at position pos+2 */
                pkt[0] = 0x07; /* sysex end, 3 bytes */
            } else {
                pkt[0] = 0x04; /* sysex continue */
            }
            pkt[1] = raw[pos];
            pkt[2] = raw[pos + 1];
            pkt[3] = raw[pos + 2];
            pos += 3;
        } else if (left == 2) {
            pkt[0] = 0x06; /* sysex end, 2 bytes */
            pkt[1] = raw[pos];
            pkt[2] = raw[pos + 1];
            pos += 2;
        } else { /* left == 1 */
            pkt[0] = 0x05; /* sysex end, 1 byte */
            pkt[1] = raw[pos];
            pos += 1;
        }
        tud_midi_packet_write(pkt);
    }
}

} /* namespace lenssysex */

/* Definition of the global pending-apply slot and its static backing buffer. */
static uint8_t s_pending_buf[lenssysex::SOURCE_CAP];
PendingPatch g_pending = { false, s_pending_buf, 0 };


// ──────────────────────────────────────────────────────────────────────────────
// Source: runtime/runtime.c
// ──────────────────────────────────────────────────────────────────────────────

/*
 * runtime.c: merged audio runtime.
 *
 * Each op_* is a real function (not static inline) decorated with OP_FN
 * so the linker places it in RAM, not flash.  Taking the address of each
 * op_* for KFN[] prevents the compiler from inlining them.  LTO is off.
 *
 * Dispatch at run time: s->fn is set to KFN[kernel_id] at apply time;
 * each walk step calls (*s->fn)(s), one indirect BLX, no switch.
 *
 * KTABLE (name -> KID) lives here; snapshot_apply.c calls
 * runtime_find_kernel() / runtime_is_hw_leaf().
 */

#include "runtime.h"
#include "kernel_ids.h"
#include "midi.h"
#include "pitch_table.h"
#include "rate_table.h"
#include "sine_table.h"
#include "exp2_log2_lut.h"
#include "fm_algorithms.h"
/* dx7banks.h holds copyrighted DX7 voice data: it is gitignored and generated by
   tools/gen-dx7banks.js from .syx files the user supplies. Banks are baked in ONLY
   when built with -DLENS_WITH_DX7_BANKS and that header is present, for local
   testing. The default build -- a fresh clone, CI, and every committed / released
   binary -- is bank-free: op_dx falls back to zero banks and stays silent until the
   user generates their own and rebuilds with the flag. This keeps copyrighted voice
   data out of every distributed binary. */
#if defined(LENS_WITH_DX7_BANKS) && defined(__has_include)
#  if __has_include("dx7banks.h")
#    include "dx7banks.h"
#  endif
#endif
#ifndef NUM_DX7_BANKS
struct Dx7Bank { uint16_t nvoices; const uint8_t* data; const char* name; };
static const struct Dx7Bank DX7_BANKS[1] = { { 0, 0, "" } };
#define NUM_DX7_BANKS 1
#endif
#include "wavetables.h"
#include "msfa_op.h"
/* stripped system include */
/* stripped system include */

/* ===== stateful state structs ===== */

/*
 * NodeState layout (stateful kernels only):
 *   offset 0: int32_t value  -- output, written and read each sample
 *   offset 4+: kernel-specific fields
 *
 * The slot's `out` pointer aims at &value. Kernels cast it to their state
 * struct and write to state->value each sample.
 */

struct NodeStateBase {
    int32_t value;
};

struct LeafState {
    int32_t value;
};

struct PhasorState {
    int32_t value;        /* +0  phase ramp output (0..VMAX) */
    int32_t tick;         /* +4  unused second-output slot (consumers edge-detect +0) */
    uint32_t phase;
    int32_t last_sync;
    uint32_t sync_count;
    uint32_t locked_inc;
};

struct SineState {
    int32_t value;
    uint32_t phase;
    int32_t y0;           /* :fb history: previous raw sine output */
    int32_t y1;           /* :fb history: last raw sine output */
};

struct TriangleState {
    int32_t value;
    uint32_t phase;
};

/* DPW band-limited saw (Valimaki 2005, after Chris Johnson's Utility-Pair card). */
struct SawState {
    int32_t value;
    uint32_t phase;
    int32_t last_parab;
};

/* DPW square = saw(phase) - saw(phase + half-period). */
struct SquareState {
    int32_t value;
    uint32_t phase;
    int32_t last_parab_a;
    int32_t last_parab_b;
};

struct EdgeState {
    int32_t value;
    int32_t last;
    int32_t pulse;
};

struct FallState {
    int32_t value;
    int32_t last;
    int32_t pulse;
};

struct DiffState {
    int32_t value;
    int32_t last;
};

struct ToggleState {
    int32_t value;
    int32_t last;
    int32_t state;
};

struct HoldState {
    int32_t value;
    int32_t last;
};

struct PickupState {
    int32_t value;     /* the held value (output); survives across samples, taken over by the knob */
    int32_t last_on;   /* engage level last sample, for the arm-on-select edge */
    int32_t armed;     /* 1 = holding value, waiting for the knob to cross it */
    int32_t side;      /* sign of (live - value) captured when armed: +1 or -1 */
    int32_t primed;    /* 0 until the init value has been applied once */
};

struct GateState {
    int32_t value;
    int32_t last;
    int32_t hold_count;
};

struct SchmittState {
    int32_t value;
    int32_t last;
};


struct Z1State {
    int32_t value;
    int32_t last;
};

struct EveryState {
    int32_t value;
    int32_t last_clk;
    int32_t counter;
    int32_t pulse;
};

struct EuclidState {
    int32_t value;
    int32_t last_clk;
    int32_t counter;
    int32_t pulse;
};

/* turns: rising-edge counter, wraps at VMAX+1. */
struct TurnsState {
    int32_t value;
    int32_t last_clk;
    int32_t count;
};

/* counter: bar counter wrapping at :bars. */
struct CounterState {
    int32_t value;
    int32_t last_clk;
    int32_t count;
    int32_t last_reset;
};

/* ===== filter state structs ===== */

/*
 * One-pole IIR state stored in Q16 (int32, range +-268M fits in int32).
 * Integer part: y_q16 >> 16.  Output: round16(y_q16).
 * Coefficient: k = cut * 16  (= cut/4096 * 65536; VMAX~4096 avoids division).
 * Step: y_q16 += (x - (y_q16 >> 16)) * k
 * Output: round16(y_q16) = (y_q16 + 32768) >> 16  (with sign-correct rounding)
 */

struct OnePoleState {
    int32_t value;
    int32_t y_q16;
};

/* VCF: 2-pole state-variable (Chamberlin SVF); lp and bp in Q16. */
struct VcfState {
    int32_t value;
    int32_t lp_q16;
    int32_t bp_q16;
};

/* SVF: same Chamberlin topology as VCF, output narrowed to the audio rail
 * (+-2047) instead of +-4095.  Drives lpf2/hpf2/bpf2.  Two integrators in Q16. */
struct SvfState {
    int32_t value;
    int32_t lp_q16;
    int32_t bp_q16;
};

/* Noise: white noise via LCG.  Initial rng = 12345 (matches interp.js). */
struct NoiseState {
    int32_t value;
    uint32_t rng;
};

/* Random: clocked random.  Initial rng = 99991, cached = VMID. */
struct RandomState {
    int32_t value;
    uint32_t rng;
    int32_t  last_clk;
    int32_t  cached_value;
};

/* Chance: probability gate.  Initial rng = 77771, cached = 0. */
struct ChanceState {
    int32_t value;
    uint32_t rng;
    int32_t  last_clk;
    int32_t  cached_value;
};

/* Walk: drunken walk.  Initial rng = 55551, cached = VMID.
   param0 = step size (default 128). */
struct WalkState {
    int32_t value;
    uint32_t rng;
    int32_t  last_clk;
    int32_t  cached_value;
};

/* LPG: one-pole LP + VCA combined.
   in0 = signal, in1 = ctrl 0..VMAX.
   State y_q16 is the LP accumulator (Q16). */
struct LpgState {
    int32_t value;
    int32_t y_q16;
};

/* latest: last-moved-wins control merge. */
struct LatestState {
    int32_t value;
    int32_t anchor;   /* a's position when b last took over */
    int32_t last_b;
    int32_t own;      /* 1 = a owns, 0 = b owns */
};

/* Freeverb: 6 comb + 2 allpass per channel (lighter configuration).
 * Comb delays (samples): L=1116,1188,1277,1356,1422,1491
 *                         R=L+23 each.
 * Allpass delays:         L=556,441  R=L+23 each.
 *
 * Buffer layout (cells):
 *   L combs:    0       .. 7849      (sum = 7850)
 *   R combs:    7850    .. 15837     (sum = 7988)
 *   L allpass:  15838   .. 16834     (sum = 997)
 *   R allpass:  16835   .. 17877     (sum = 1043)
 *   Total:      17878 cells
 *
 * Cumulative base offsets within each section are pre-computed so the hot
 * audio loop does a single add (base + pos) rather than a running sum.
 */
#define FV_NCOMB   6
#define FV_NAP     2
/* Comb delays (samples): L = 1116,1188,1277,1356,1422,1491; R = L+23 each.
 * Allpass delays: L = 556,441; R = L+23 each. The per-stage lengths and
 * cumulative base offsets are the literal arguments at the COMB_STEP_* and
 * AP_STEP_* call sites in op_reverb: those are the single source of truth. */
#define FV_R_COMB_OFFSET  7850u
#define FV_L_AP_OFFSET    15838u
#define FV_R_AP_OFFSET    16835u
#define FV_BUF_CELLS      17878u

/* Freeverb state: per-comb write heads and damping filters, allpass heads,
 * and a DC blocker per channel. */
struct ReverbState {
    int32_t  value;           /* +0 left output */
    int32_t  value_r;         /* +4 right output */
    uint32_t comb_pos_L[FV_NCOMB];
    uint32_t comb_pos_R[FV_NCOMB];
    int32_t  comb_flt_L[FV_NCOMB];
    int32_t  comb_flt_R[FV_NCOMB];
    uint32_t ap_pos_L[FV_NAP];
    uint32_t ap_pos_R[FV_NAP];
    int32_t  dc_x_L;
    int32_t  dc_y_L;
    int32_t  dc_x_R;
    int32_t  dc_y_R;
};

/* Stereo modulated-delay effects (chorus/flanger). value/value_r are the L/R
 * outputs; :r port reads value_r via the +4 second-output ref. */
struct ChorusState {
    int32_t value;        /* +0 left (signed audio) */
    int32_t value_r;      /* +4 right (signed audio) */
    uint32_t lfo_phase;
    uint32_t head;
};

struct CompressorState {
    int32_t value;        /* signed audio out */
    int32_t envelope;     /* peak follower */
};

/* echo: stereo delay unit. */
struct EchoState {
    int32_t  value;       /* +0 left (signed audio) */
    int32_t  value_r;     /* +4 right (signed audio) */
    uint32_t head;
};

/* EnvFollow: full-wave rectify then one-pole LP.
   in0 = signal, in1 = cut coefficient. */
struct EnvFollowState {
    int32_t value;
    int32_t y_q16;
};

/* Wavefold: ADAA wavefolder.
   After Chris Johnson's Utility-Pair card.
   State: lastx (int32), lastval (int32). */
struct WavefoldState {
    int32_t value;
    int32_t lastx;
    int32_t lastval;
};

/* Crush: sample-rate decimator.
   in0 = signal, in1 = rate (higher = less crushing).
   N = max(1, round((VMAX - rate) / 100) + 1).
   State: held value, countdown. */
struct CrushState {
    int32_t value;
    int32_t held;
    int32_t count;
};

/* Shape: LUT waveshaper.
   lastx = previous input sample, used to linearly interpolate the 4x
   oversampled subsamples when the oversample bit is set. */
struct ShapeState {
    int32_t value;
    int32_t lastx;
};

/* ===== drum state structs ===== */

/* Drum voices after Mutable Instruments Plaits (Emilie Gillet, MIT).
 * Reimplemented for the 12-bit Loupe runtime. Numerics per heritage-kernels.md. */

struct KickState {
    int32_t value;
    int32_t level;
    int32_t last_trig;
    int32_t pitchEnv;
    uint32_t phase;
    int32_t lp;
    int32_t hp;
    int32_t cached_decay;
    int32_t cached_k;
    uint32_t dither;
};

struct SnareState {
    int32_t value;
    int32_t level;
    int32_t last_trig;
    int32_t pitchEnv;
    uint32_t phase;
    uint32_t pos;
    uint32_t rng;
    int32_t lp1;
    int32_t lp2;
    int32_t cached_decay;
    int32_t cached_k;
    uint32_t dither;
};

struct HatState {
    int32_t value;
    int32_t level;
    int32_t last_trig;
    uint32_t p1;
    uint32_t p2;
    uint32_t p3;
    int32_t hp;
    int32_t cached_decay;
    int32_t cached_k;
    uint32_t dither;
};

/* Karplus-Strong plucked string. The delay line is a private audio buffer
 * (in3); state holds the loop length (period in samples), the read/write index,
 * the one-zero damping register, the trig edge, and the LCG seed for the
 * excitation burst. n == 0 means "not yet plucked" -> silent. */
struct PluckState {
    int32_t  value;       /* +0 current delay-line sample (signed audio) */
    int32_t  last;        /* one-zero damping register (previous played sample) */
    int32_t  last_trig;
    uint32_t idx;         /* play/write position in [0, n) */
    uint32_t n;           /* loop length = samplerate / freq(pitch) */
    uint32_t rng;         /* LCG state for the noise burst */
};

/* ===== voice state structs ===== */

struct EnvelopeState {
    int32_t value;
    int32_t level;
    int32_t last_trig;
    int32_t cached_decay;
    int32_t cached_k;
};

struct AdsrState {
    int32_t value;            /* +0 output */
    int32_t level;            /* current level, <<8 for sub-LSB precision */
    int32_t phase;            /* 0 idle, 1 attack, 2 decay/sustain, 3 release */
    int32_t last_gate;
    int32_t ca, ka, cd, kd, cr, kr;  /* cached rate args + their one-pole k */
};

struct FollowState {
    int32_t  value;       /* +0  phase ramp (0..VMAX) */
    int32_t  tick;        /* +4  unused second-output slot (consumers edge-detect +0) */
    uint32_t last_base;   /* base clock's 12-bit ramp last sample (turn detect) */
    uint32_t counter;     /* base turns elapsed, mod div */
    uint32_t acc;         /* accumulated :drift phase creep (32-bit, >>20 to 12) */
    uint32_t q;           /* cached (counter*mult)/div */
    uint32_t r;           /* cached (counter*mult)%div */
};

/* ===== tape state structs ===== */

/*
 * Buffer: shared ring-buffer, cells 12-bit packed (2 per 3 bytes).
 * Each recordhead tracks its own position.
 * Byte storage: (length * 3 + 1) >> 1 bytes.
 */

struct StepState {
    int32_t value;
    int32_t last_clk;
    int32_t counter;
    int32_t cached;
    int32_t inited;
};

struct LookupState {
    int32_t value;
};

struct WaveState {
    int32_t value;
};

struct TapState {
    int32_t value;
};

/* Record-head positions are 32-bit so a per-sample (audio) head can address the
 * whole audio pool (~87381 cells = 1.82 s). The ring wraps by compare-and-
 * subtract, so the buffer length need not be a power of two. head_pos_out at
 * +4 is the published head op_tap reads (TAG_SLOT_OUT2). */
struct RecordheadPerSampleState {
    int32_t  value;
    int32_t  head_pos_out;
    uint32_t head_pos;
    uint32_t pending_pos;
    int32_t  pending_val;
    uint32_t pending_head_pos_next;
    uint8_t  pending_valid;
    uint8_t  _pad[3];
};

struct RecordheadPerCellState {
    int32_t  value;
    int32_t  head_pos_out;
    uint32_t head_pos;
    uint32_t pending_pos;
    int32_t  pending_val;
    uint32_t pending_head_pos_next;
    uint8_t  pending_valid;
    uint8_t  _pad[3];
    int32_t  last_clk;
    int32_t  inited;
    int32_t  last_reset;
};

struct RecordheadGatedState {
    int32_t  value;
    int32_t  head_pos_out;
    uint32_t head_pos;
    uint32_t pending_pos;
    int32_t  pending_val;
    uint32_t pending_head_pos_next;
    uint8_t  pending_valid;
    uint8_t  _pad[3];
    int32_t  last_clk;
    int32_t  inited;
    int32_t  last_reset;
};

struct RecordheadLenCappedState {
    int32_t  value;
    int32_t  head_pos_out;
    uint32_t head_pos;
    uint32_t pending_pos;
    int32_t  pending_val;
    uint32_t pending_head_pos_next;
    uint8_t  pending_valid;
    uint8_t  _pad[3];
    int32_t  last_clk;
    int32_t  inited;
    int32_t  last_reset;
};

struct RecordheadLenCappedGatedState {
    int32_t  value;
    int32_t  head_pos_out;
    uint32_t head_pos;
    uint32_t pending_pos;
    int32_t  pending_val;
    uint32_t pending_head_pos_next;
    uint8_t  pending_valid;
    uint8_t  _pad[3];
    int32_t  last_clk;
    int32_t  inited;
    int32_t  last_reset;
};

/* Random-access write head: same committable prefix as the other record heads,
   but the write position comes from an input index instead of auto-advancing. */
struct RecordheadSeekState {
    int32_t  value;
    int32_t  head_pos_out;
    uint32_t head_pos;
    uint32_t pending_pos;
    int32_t  pending_val;
    uint32_t pending_head_pos_next;
    uint8_t  pending_valid;
    uint8_t  _pad[3];
    int32_t  last_clk;
};

struct SeekState {
    int32_t value;
};

struct OnsetsState {
    int32_t value;
    int32_t last_clk;
    int32_t counter;
    int32_t pulseLeft;
    int32_t inited;
};

struct GatesState {
    int32_t value;
    int32_t last_clk;
    int32_t counter;
    int32_t gate;
    int32_t inited;
};

struct HitsState {
    int32_t value;
    int32_t last_clk;
    int32_t step_count;
    int32_t cached;
    int32_t inited;
};

struct DegreeState {
    int32_t value;
};

struct PitchState {
    int32_t value;
};

struct ThruState {
    int32_t value;
};

struct WaveDrumrackState {
    int32_t  value;
    uint32_t phase;
};

/* MIDI output op states. */
typedef struct { uint8_t prev_gate; uint8_t cur_note; uint8_t active; } MidiNoteOutState;
typedef struct { int16_t prev7; } MidiCcOutState;
typedef struct { uint8_t prev; } MidiClockOutState;

/*
 * Common prefix shared by all recordhead state structs (offsets 0..19).
 * Used by the runtime end-of-tick sweep to commit pending writes without
 * knowing which specific variant is in each slot.
 */
struct RecordheadCommon {
    int32_t  value;
    int32_t  head_pos_out;
    uint32_t head_pos;
    uint32_t pending_pos;
    int32_t  pending_val;
    uint32_t pending_head_pos_next;
    uint8_t  pending_valid;
    uint8_t  _pad[3];
};

/* ===== pack12: 12-bit cells packed 2-per-3-bytes ===== */

/*
 * Byte layout for pair at index pair = idx >> 1:
 *   base = pair * 3
 *   byte[base+0] = cell0[7:0]
 *   byte[base+1] = cell1[3:0]<<4 | cell0[11:8]
 *   byte[base+2] = cell1[11:4]
 *
 * M0+: byte loads only (LDRB); no halfword loads at odd addresses.
 * Byte storage for n cells: (n * 3 + 1) >> 1  bytes.
 */

__attribute__((always_inline))
static inline int32_t pack12_read(const uint8_t* buf, uint32_t idx) {
    uint32_t pair = idx >> 1;
    uint32_t base = (pair << 1) + pair;
    if ((idx & 1u) == 0u) {
        return (int32_t)(((uint32_t)buf[base] | (((uint32_t)buf[base + 1u] & 0x0Fu) << 8)));
    } else {
        return (int32_t)((((uint32_t)buf[base + 1u]) >> 4) | (((uint32_t)buf[base + 2u]) << 4));
    }
}

/* Audio cells hold a bipolar sample two's-complement in 12 bits; the read must
   sign-extend so the negative half is not wrapped to a large positive (tape
   cells stay unsigned 0..4095, so only the audio readers op_tap/op_wave use
   this). 0..2047 stay positive; 2048..4095 become -2048..-1. */
static inline int32_t pack12_read_signed(const uint8_t* buf, uint32_t idx) {
    int32_t c = pack12_read(buf, idx);
    return (c >= 2048) ? (c - 4096) : c;
}

__attribute__((always_inline))
static inline void pack12_write(uint8_t* buf, uint32_t idx, int32_t val) {
    uint32_t v    = (uint32_t)val & 0xFFFu;
    uint32_t pair = idx >> 1;
    uint32_t base = (pair << 1) + pair;
    if ((idx & 1u) == 0u) {
        buf[base]      = (uint8_t)(v & 0xFFu);
        buf[base + 1u] = (uint8_t)((buf[base + 1u] & 0xF0u) | (v >> 8));
    } else {
        buf[base + 1u] = (uint8_t)((buf[base + 1u] & 0x0Fu) | ((v & 0xFu) << 4));
        buf[base + 2u] = (uint8_t)(v >> 4);
    }
}

/* ===== audio helpers ===== */

#define SMAX 2047

/* Jack-connection mask (bit = hw_scratch jack index); set each sample from the
   hardware normalisation probe and read by op_connected. */
static uint16_t hw_connected;

static inline int32_t sclamp_(int32_t x) { return x < -SMAX ? -SMAX : (x > SMAX ? SMAX : x); }
static inline int32_t vclamp_(int32_t x) { return x < 0 ? 0 : (x > VMAX ? VMAX : x); }
/* Narrow a 32-bit runtime value to the 12-bit magnitude domain (+-VMAX). In-range
   values (uni- or bipolar) pass through; only out-of-range intermediates (e.g. a
   raw op_mul product) saturate, so a later multiply cannot overflow int32. */
static inline int32_t mclamp_(int32_t x) { return x < -VMAX ? -VMAX : (x > VMAX ? VMAX : x); }

static inline uint32_t umulhi32(uint32_t a, uint32_t b) {
    uint32_t ah = a >> 16, al = a & 0xFFFF, bh = b >> 16, bl = b & 0xFFFF;
    uint32_t lo = al * bl;
    uint32_t m1 = ah * bl + (lo >> 16);
    uint32_t m2 = al * bh + (m1 & 0xFFFF);
    return ah * bh + (m1 >> 16) + (m2 >> 16);
}

static inline int32_t onePoleStep(int32_t diff, uint32_t coefQ32) {
    uint32_t mag = diff < 0 ? (uint32_t)(-diff) : (uint32_t)diff;
    int32_t step = (int32_t)umulhi32(mag, coefQ32);
    return diff < 0 ? -step : step;
}

/* Truncating interpolation used by the drum voices (kick/snare), kept bit-for-bit
 * as ported from Plaits. The oscillators use sine_interp (round-to-nearest). */
static inline int32_t sineInterp_(uint32_t ph) {
    int32_t i = (int32_t)(ph >> 24), frac = (int32_t)((ph >> 16) & 0xFF);
    int32_t a = sine_table[i], b = sine_table[(i + 1) & 0xFF];
    return a + (((b - a) * frac) >> 8);
}

static inline uint32_t xorshift32_(uint32_t s) {
    uint32_t x = s ? s : 1u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}

static inline uint32_t rngSeed_(int i) {
    return (0x9E3779B9u ^ ((uint32_t)i * 0x6D2B79F5u)) | 1u;
}

static inline int32_t drumDecayK_(int32_t decay, int shift, int floor_tau) {
    int32_t tau = floor_tau + ((decay * decay) >> shift);
    int32_t k = 65536 / tau;
    return k < 1 ? 1 : (k > 65535 ? 65535 : k);
}

/* Triangular dither on a right shift. State is per voice (a shared global
 * would be written by both cores). Lazily seeded: node state zeroes on apply. */
static inline int32_t ditherShift_(uint32_t* rp, int32_t hi, int shift) {
    if (hi == 0) return 0;
    uint32_t r = *rp ? *rp : 0x2545F491u;
    r ^= r << 13; r ^= r >> 17; r ^= r << 5; int32_t a = (int32_t)(r >> (32 - shift));
    r ^= r << 13; r ^= r >> 17; r ^= r << 5; int32_t b = (int32_t)(r >> (32 - shift));
    *rp = r;
    return (hi + a + b - (1 << shift)) >> shift;
}

#if LENS_PERF_PROBE
#define M0P_SYSTICK_CVR (*((volatile uint32_t *)0xE000E018))
static inline uint32_t rt_dwt_read(void) { return M0P_SYSTICK_CVR; }
#endif

#ifndef __not_in_flash_func
#define __not_in_flash_func(f) f
#endif
#ifndef __noinline
#define __noinline __attribute__((noinline))
#endif

/* ===== shared constants ===== */

#define VBITS_ 12
/* Default trigger/gate pulse width (~1.35 ms at 48 kHz): visible on an LED and long
   enough for external gear; harmless to an internal consumer (which edge-detects).
   Override per-op with :width. A deliberate trigger duration, not a control window. */
#define kTickWidth 65

/* ===== arith helpers ===== */

static inline int32_t js_round_div(int32_t a, int32_t b) {
    if (b == 0) return 0;
    int32_t n = 2 * a + b;
    int32_t d = 2 * b;
    int32_t q = n / d;
    int32_t r = n % d;
    if (r != 0 && ((n ^ d) < 0)) q--;
    return q;
}

static inline int32_t js_mod(int32_t a, int32_t b) {
    if (b == 0) return 0;
    int32_t r = a % b;
    if (r != 0 && ((r ^ b) < 0)) r += b;
    return r;
}

/* Floor division, the partner of js_mod: (a/b)*b + (a%b) == a, and floor(a/b)
   matches what indexing and bit/field math expect (C-style / truncates only
   toward zero; this floors toward negative infinity like js_mod). */
static inline int32_t js_floor_div(int32_t a, int32_t b) {
    if (b == 0) return 0;
    int32_t q = a / b;
    int32_t r = a % b;
    if (r != 0 && ((r ^ b) < 0)) q--;
    return q;
}

static inline int32_t js_spread(int32_t x, int32_t n) {
    int32_t cap = n - 1;
    int32_t num = x * n;
    int32_t bucket = num / (VMAX + 1);
    if (num < 0 && num % (VMAX + 1) != 0) bucket--;
    return bucket < cap ? bucket : cap;
}

static inline int32_t vclamp(int32_t x) {
    if (x < 0)     return 0;
    if (x > VMAX) return VMAX;
    return x;
}

/* note % 12 for note in [0, 255]; falls back to soft-modulo outside. */
static const uint8_t pc12_lut[256] = {
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3,4,5,6,7,8,9,10,11,
    0,1,2,3,4,5,6,7,8,9,10,11,0,1,2,3
};

static inline int32_t snap_to_mask(int32_t note, int32_t mask) {
    if (!mask) return note;
    int32_t pc = (note >= 0 && note < 256) ? (int32_t)pc12_lut[note]
                                            : ((note % 12) + 12) % 12;
    int32_t best = pc, bestDist = 12;
    for (int32_t d = 0; d < 12; d++) {
        if ((mask >> d) & 1) {
            int32_t dist = d - pc;
            if (dist < 0) dist = -dist;
            if (12 - dist < dist) dist = 12 - dist;
            if (dist < bestDist) { bestDist = dist; best = d; }
        }
    }
    /* Move to the nearest scale tone in the minimal SIGNED direction. A linear
     * (best - pc) shift drops a full octave when the nearest tone is across the
     * octave wrap (e.g. B -> the C above), so fold the delta into [-6, +6]. */
    int32_t delta = best - pc;
    if (delta >  6) delta -= 12;
    if (delta < -6) delta += 12;
    return note + delta;
}

/* ===== stateful helpers ===== */

/* A beat/trigger is a rising crossing of a LOW threshold. One detector serves both
   a ramp (crosses just after the wrap -> fires at the downbeat) and a pulse (crosses
   on arrival). The threshold is low because a phasor ramp spans only 0..VMID. */
#define kEdgeLevel (VMAX / 16)
#define FALLING_(x, last) ((x) <= VMID && (last) > VMID)
#define RISING_(x, last)  ((x) > kEdgeLevel && (last) <= kEdgeLevel)

/* ---- the clocked-head contract, one implementation ----
 * Edge memory: trig_fall/trig_rise compare against the last value THIS
 * consumer saw, so a deferred control-rate walk still counts every edge
 * exactly once. Position: a head sits on cell 0 at t=0 (the downbeat exists
 * before any edge; head_seed returns 1 there so a kernel can voice cell 0)
 * and the first edge plays cell 1; head_take returns the cell this edge
 * plays and advances, wrapping at len. Kernels express clocking ONLY through
 * these; the step-alignment gate rejects a raw FALLING_/RISING_ elsewhere. */
static inline int trig_fall(int32_t* last, int32_t x) {
    int fired = FALLING_(x, *last);
    *last = x;
    return fired;
}
static inline int trig_rise(int32_t* last, int32_t x) {
    int fired = RISING_(x, *last);
    *last = x;
    return fired;
}
static inline int head_seed(int32_t* inited, uint32_t* pos, uint32_t len) {
    if (*inited) return 0;
    *inited = 1;
    *pos = (len > 1u) ? 1u : 0u;
    return 1;
}
static inline uint32_t head_take(uint32_t* pos, uint32_t len) {
    uint32_t p = *pos;
    if (p >= len) p = 0u;
    *pos = (p + 1u < len) ? p + 1u : 0u;
    return p;
}

static inline uint32_t midi_clamp(int32_t n) {
    if (n < 0)   return 0;
    if (n > 127) return 127;
    return (uint32_t)n;
}

static inline int32_t phase_to_ramp(uint32_t phase32) {
    /* Full 12-bit scale: 0..VMAX over one turn (the top 12 bits of the phase). */
    return (int32_t)(phase32 >> 20);
}

/* :phase mode: turn a 12-bit exchange-domain phase (0..VMAX = one turn, e.g. a
   phasor's ramp) back into the internal 32-bit phase a shaper expects. */
static inline uint32_t ramp_to_phase(int32_t ramp12) {
    return ((uint32_t)ramp12 & 0xFFFu) << 20;
}

static inline int32_t phase_to_triangle(uint32_t phase32) {
    uint32_t frac19 = phase32 & 0x7FFFFu;
    uint32_t trunc  = phase32 >> 19;
    if (phase32 < 0x80000000u) {
        return (int32_t)(trunc + (frac19 >= 0x40000u ? 1u : 0u)) - 2048;
    } else {
        return 6144 - (int32_t)(trunc + (frac19 > 0x40000u ? 1u : 0u));
    }
}

/* Linear interpolation for the oscillators (sine/phasor depth). Reads the
 * flash-resident 1024-entry sine1024 LUT whose entries are 14-bit amplitude
 * (+-16384). Interpolating at 14 bits then rounding once to +-2048 lands on the
 * 12-bit output quantization floor, so heavy FM renders cleaner than the old
 * 12-bit 256-entry table (which sat ~1 LSB above the floor). The drum voices
 * keep sineInterp_ (truncating, 256 table) to match their Plaits heritage.
 *   num = a*1024 + (b-a)*frac is the 14-bit value scaled by 1024 (the frac
 *   denominator); >>13 with +4096 rounding folds both the *1024 and the 14->12
 *   bit drop into one shift (1024*16384/2048 = 8192 = 1<<13). 32-bit throughout. */
static inline int32_t sine_interp(uint32_t phase32) {
    uint32_t idx  = (phase32 >> 22) & 0x3FFu;       /* top 10 bits: 0..1023 */
    uint32_t frac = (phase32 >> 12) & 0x3FFu;       /* next 10 bits: 0..1023 */
    int32_t a = sine1024[idx];
    int32_t b = sine1024[(idx + 1u) & 0x3FFu];
    int32_t num = a * 1024 + (b - a) * (int32_t)frac;
    if (num >= 0) return (num + 4096) >> 13;
    return -(((-num) + 4096) >> 13);
}

static inline uint32_t rate_inc(uint32_t mode, int32_t in0) {
    switch (mode) {
      case 0:
        return pitch_table[midi_clamp(in0)];
      case 1: {
        uint32_t hz = (uint32_t)(in0 < 0 ? 0 : in0);
        return hz * 89478u;
      }
      case 2: {
        uint32_t v = (uint32_t)in0;
        return rate_table[(v >> 4) & 0xFFu];
      }
      default: {
        int32_t v = vclamp_(in0);
        return rate_table[32u + (((uint32_t)v * 94u) >> 12)];
      }
    }
}

/* :fb feedback phase-swing trim. Full :fb at LENS_FB_SHIFT=2 is a feedback sawtooth;
 * lower shifts buzz toward noise, higher shifts mellow toward a pure sine. */
#ifndef LENS_FB_SHIFT
#define LENS_FB_SHIFT 2
#endif

/* op_dx self-feedback calibration. msfa shifts the operator's averaged output by
 * (8 - feedback); FM_FB_CALIB trims that to our phase/sine fixed-point scale. Tuned by
 * A/B-ing a feedback voice against the Dexed/msfa reference (attic/dx7ref). */
#ifndef FM_FB_CALIB
#define FM_FB_CALIB 0
#endif

static inline uint32_t pm_offset(int32_t pm) {
    /* Modulator value (+-2047) -> phase swing. The <<21 shift is done in uint32
     * (modular, sign preserved by two's complement) so a full-depth modulator pushes
     * the carrier phase a full cycle (index ~2pi) monotonically, with no mid-range
     * wrap/aliasing. A signed shift would clamp at the sign bit near half a cycle. */
    return ((uint32_t)pm) << 21;
}

/* round(val * depth / VMAX): the shared 12-bit-normalised product used by vca,
 * ring, lpg and sine depth. The %/correction gives round-half-up for both signs. */
static inline int32_t scale_depth(int32_t val, int32_t depth) {
    int32_t n = val * depth;
    int32_t b = VMAX;
    int32_t n2 = n * 2 + b;
    int32_t d2 = b * 2;
    int32_t q = n2 / d2;
    int32_t r = n2 % d2;
    if (r != 0 && ((n2 ^ d2) < 0)) q--;
    return q;
}

/* ===== filters helpers ===== */

static inline uint32_t lcg(uint32_t s) {
    return (uint32_t)(1664525u * s + 1013904223u);
}

static inline int32_t round16(int32_t q16) {
    if (q16 >= 0) return (q16 + 32768) >> 16;
    return -(((-q16) + 32767) >> 16);
}

static inline int32_t onepole_step(int32_t y_q16, int32_t x, int32_t cut) {
    int32_t k = cut * 16;
    int32_t diff = (mclamp_(x) << 16) - y_q16;
    int32_t step = (diff >> 16) * k;
    /* Minimum step of one q16 unit: once the integer parts match, the retained
     * fraction would otherwise hold the output one LSB off target forever
     * (envfollow/lpg leak a faint 1 at silence). Sub-LSB per sample: inaudible. */
    if (step == 0 && diff != 0) step = diff > 0 ? 1 : -1;
    return y_q16 + step;
}

/* ===== arith kernels ===== */

void OP_FN(op_add)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 + *(int32_t*)s->in1;
}
void OP_FN(op_sub)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 - *(int32_t*)s->in1;
}
/* :sat variants saturate at the value rails [0, VMAX] instead of wrapping, like a
   CV hitting the supply rails. (Audio uses `clip`/`saturate` for the bipolar rails.) */
void OP_FN(op_add_sat)(struct Slot* s) {
    *(int32_t*)s->out = vclamp_(*(int32_t*)s->in0 + *(int32_t*)s->in1);
}
void OP_FN(op_sub_sat)(struct Slot* s) {
    *(int32_t*)s->out = vclamp_(*(int32_t*)s->in0 - *(int32_t*)s->in1);
}
void OP_FN(op_mul)(struct Slot* s) {
    int32_t a = *(int32_t*)s->in0, b = *(int32_t*)s->in1;
    *(int32_t*)s->out = a * b;   /* true multiply; gain-scaling is op_vca */
}
void OP_FN(op_div)(struct Slot* s) {
    *(int32_t*)s->out = js_floor_div(*(int32_t*)s->in0, *(int32_t*)s->in1);
}
void OP_FN(op_mod)(struct Slot* s) {
    *(int32_t*)s->out = js_mod(*(int32_t*)s->in0, *(int32_t*)s->in1);
}
void OP_FN(op_spread)(struct Slot* s) {
    *(int32_t*)s->out = js_spread(*(int32_t*)s->in0, *(int32_t*)s->in1);
}
void OP_FN(op_gt)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 > *(int32_t*)s->in1 ? VMAX : 0;
}
void OP_FN(op_gte)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 >= *(int32_t*)s->in1 ? VMAX : 0;
}
void OP_FN(op_lt)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 < *(int32_t*)s->in1 ? VMAX : 0;
}
void OP_FN(op_lte)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 <= *(int32_t*)s->in1 ? VMAX : 0;
}
void OP_FN(op_eq)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 == *(int32_t*)s->in1 ? VMAX : 0;
}
void OP_FN(op_ne)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 != *(int32_t*)s->in1 ? VMAX : 0;
}
void OP_FN(op_if)(struct Slot* s) {
    int32_t cond = *(int32_t*)s->in0;
    *(int32_t*)s->out = cond != 0 ? *(int32_t*)s->in1 : *(int32_t*)s->in2;
}
void OP_FN(op_not)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 == 0 ? VMAX : 0;
}
void OP_FN(op_max)(struct Slot* s) {
    int32_t a = *(int32_t*)s->in0, b = *(int32_t*)s->in1;
    *(int32_t*)s->out = a > b ? a : b;
}
void OP_FN(op_min)(struct Slot* s) {
    int32_t a = *(int32_t*)s->in0, b = *(int32_t*)s->in1;
    *(int32_t*)s->out = a < b ? a : b;
}
void OP_FN(op_abs)(struct Slot* s) {
    int32_t a = *(int32_t*)s->in0;
    *(int32_t*)s->out = a < 0 ? -a : a;
}
void OP_FN(op_rect)(struct Slot* s) {
    int32_t a = *(int32_t*)s->in0;
    *(int32_t*)s->out = a > 0 ? a : 0;
}
void OP_FN(op_and)(struct Slot* s) {
    int32_t a = *(int32_t*)s->in0, b = *(int32_t*)s->in1;
    *(int32_t*)s->out = (a != 0 && b != 0) ? VMAX : 0;
}
void OP_FN(op_or)(struct Slot* s) {
    int32_t a = *(int32_t*)s->in0, b = *(int32_t*)s->in1;
    *(int32_t*)s->out = (a != 0 || b != 0) ? VMAX : 0;
}
void OP_FN(op_xor)(struct Slot* s) {
    *(int32_t*)s->out = *(int32_t*)s->in0 ^ *(int32_t*)s->in1;
}

/* ===== fixed-point exp2 / log2 (shared LUT core) =====
   Pure 32-bit integer (no float/trig/64-bit); 257-entry flash LUTs + linear
   interpolation. Reusable foundation for exponential CV/VCA, pitch/ratio math.

   exp2_fixed(x): x is a signed exponent in Q16 (1.0 == 65536, so x>>16 is the
   integer power of two). Returns 2^x in Q16 (1.0 == 65536). Integer part is a
   shift; the 16-bit fraction interpolates EXP2_FRAC_LUT (2^frac in Q16 over
   [1,2)). Valid x < 15<<16 (saturates above; ~0 below -31<<16).

   log2_fixed(x): x is a value in Q16 (1.0 == 65536). Returns log2(x) in Q16.
   MSB gives the integer part; the normalised mantissa [1,2) interpolates
   LOG2_FRAC_LUT. x<=0 is clamped to 1 LSB. Inverse of exp2_fixed within tol.
   op_dxeg reconstructs its envelope gain from the same EXP2_FRAC_LUT. */
static inline int32_t exp2_fixed(int32_t x) {
    int32_t ip = x >> 16;                       /* integer part (arith shift)  */
    uint32_t fr = (uint32_t)x & 0xFFFFu;        /* 16-bit fraction             */
    int32_t a = EXP2_FRAC_LUT[fr >> 8];
    int32_t b = EXP2_FRAC_LUT[(fr >> 8) + 1];
    int32_t m = a + (((b - a) * (int32_t)(fr & 0xFF)) >> 8);  /* 2^fr Q16 [65536,131072) */
    if (ip >= 15) return 0x7FFFFFFF;            /* saturate (overflows Q16/int32) */
    if (ip >= 0)  return m << ip;
    if (ip <= -31) return 0;
    return m >> (-ip);
}

/* :cents fine detune (oscillator param0 bit 6, detune value on the last input):
   VMID is centre, full scale = +-100 cents (+-1/12 octave). Scales the phase
   increment by 2^oct via the Q16 exp2 LUT; split multiply keeps it 32-bit. */
static inline uint32_t inc_detune(uint32_t inc, int32_t v) {
    int32_t oct_q16 = ((v - VMID) * 10923) >> 12;      /* ~(v-VMID)*8/3: +-1/12 oct Q16 */
    uint32_t f = (uint32_t)exp2_fixed(oct_q16);        /* Q16, ~[61858, 69433] */
    return (inc >> 16) * f + (((inc & 0xFFFFu) * f) >> 16);
}
static inline int32_t log2_fixed(int32_t x) {
    if (x <= 0) x = 1;
    uint32_t v = (uint32_t)x;
    int n = 0;                                  /* MSB index 0..30 */
    while (v > 1u) { v >>= 1; n++; }
    uint32_t mant = (n <= 30) ? ((uint32_t)x << (30 - n))   /* align MSB to bit 30 */
                              : ((uint32_t)x >> (n - 30));
    uint32_t f = (mant >> 14) & 0xFFFFu;        /* 16 fractional bits below leading 1 */
    int32_t a = LOG2_FRAC_LUT[f >> 8];
    int32_t b = LOG2_FRAC_LUT[(f >> 8) + 1];
    int32_t frac = a + (((b - a) * (int32_t)(f & 0xFF)) >> 8);  /* log2(mant) frac Q16 */
    return ((n - 16) << 16) + frac;             /* log2(x_real) in Q16 */
}

/* exp2: exponential CV/VCA transfer over ~8 octaves. :in 0..VMAX (clamped) maps
   to out = VMAX * 2^(8*(in/VMAX - 1)); in=VMAX -> VMAX (unity), in=0 -> ~16
   (-8 oct, near silence). Use as an exponential gain/response shaper. */
void OP_FN(op_exp2)(struct Slot* s) {
    int32_t v = vclamp_(*(int32_t*)s->in0);     /* 0..VMAX */
    int32_t g = exp2_fixed((v - VMAX) * 128);  /* 2^(8*(v/VMAX-1)) in Q16, [~256,65536] */
    *(int32_t*)s->out = (int32_t)(((uint32_t)VMAX * (uint32_t)g) >> 16);
}
/* log2: inverse of exp2. :in 0..VMAX (a gain*VMAX) maps back to the linear
   control out = VMAX + log2(in/VMAX)*VMAX/8 (clamped 0..VMAX). */
void OP_FN(op_log2)(struct Slot* s) {
    int32_t y = vclamp_(*(int32_t*)s->in0);             /* 0..VMAX */
    int32_t g = (int32_t)(((uint32_t)y << 16) / (uint32_t)VMAX);  /* gain in Q16 */
    int32_t x = log2_fixed(g);                          /* ~[-8<<16, 0] */
    *(int32_t*)s->out = vclamp_(VMAX + x / 128);
}
void OP_FN(op_v_oct)(struct Slot* s) {
    /* Emit the MIDI note (0..127) itself; the cv-out jack feeds it to the card's
       CALIBRATED 1V/oct DAC path (CVOutMIDINote). A pitch out is recognised by its
       source kernel being op_v_oct (see runtime_drive_terminals), so no generic
       12-bit LUT is used (that clipped against the signed DAC range). The host
       runner has no DAC, so it simply reports the note number. */
    int32_t note = *(int32_t*)s->in0;
    if (note < 0)   note = 0;
    if (note > 127) note = 127;
    *(int32_t*)s->out = note;
}
void OP_FN(op_window)(struct Slot* s) {
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    int32_t a  = *(int32_t*)s->in0;
    int32_t lo = *(int32_t*)s->in1;
    int32_t hi = *(int32_t*)s->in2;
    st->value = (a > lo && a < hi) ? VMAX : 0;
}
void OP_FN(op_range)(struct Slot* s) {
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    int32_t x = *(int32_t*)s->in0;
    uint32_t p = s->param0;
    if (p & 0x40000000u) {              /* to-value: expand 0..V-1 -> 0..VMAX */
        int32_t V = (int32_t)(p & 0x3FFFFFFFu);
        if (V <= 1) { st->value = 0; return; }
        st->value = js_round_div(x * VMAX, V - 1);
    } else if (p) {                     /* to-index: quantise to N buckets */
        int32_t N = (int32_t)(p & 0x3FFFFFFFu);
        if (N < 1) N = 1;
        int32_t bucket = (x * N) / (VMAX + 1);
        if (bucket < 0) bucket = 0;
        if (bucket > N - 1) bucket = N - 1;
        st->value = bucket;
    } else {
        st->value = x;
    }
}
void OP_FN(op_cv)(struct Slot* s) {
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    int32_t x = *(int32_t*)s->in0;
    if (s->param0 & 1u) {
        st->value = x - VMID;
    } else {
        st->value = js_round_div(x * 2047, VMAX);
    }
}
void OP_FN(op_snap)(struct Slot* s) {
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    int32_t note = *(int32_t*)s->in0;
    int32_t mask = s->param0 ? (int32_t)s->param0 : *(int32_t*)s->in1;
    st->value = snap_to_mask(note, mask & 0xFFF);
}
void OP_FN(op_quantise)(struct Slot* s) {
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    int32_t val  = *(int32_t*)s->in0;
    int32_t midi = js_round_div(val * 127, VMAX);
    int32_t mask = s->param0 ? (int32_t)s->param0 : *(int32_t*)s->in1;
    int32_t note = snap_to_mask(midi, mask & 0xFFF);
    st->value = js_round_div(note * VMAX, 127);   /* MIDI 127 -> VMAX */
}
void OP_FN(op_transpose)(struct Slot* s) {
    ((struct NodeStateBase*)s->out)->value = vclamp(*(int32_t*)s->in0 + *(int32_t*)s->in1);
}
void OP_FN(op_invert)(struct Slot* s) {
    ((struct NodeStateBase*)s->out)->value = VMAX - vclamp(*(int32_t*)s->in0);
}
void OP_FN(op_shift)(struct Slot* s) {
    ((struct NodeStateBase*)s->out)->value = vclamp(*(int32_t*)s->in0 + mclamp_(*(int32_t*)s->in1) * 256);
}
void OP_FN(op_mask)(struct Slot* s) {
    ((struct NodeStateBase*)s->out)->value = (*(int32_t*)s->in0 & *(int32_t*)s->in1) & 0xFFF;
}
void OP_FN(op_bit)(struct Slot* s) {
    int32_t v = *(int32_t*)s->in0, n = *(int32_t*)s->in1 & 0xF;
    ((struct NodeStateBase*)s->out)->value = ((v >> n) & 1) ? VMAX : 0;
}
void OP_FN(op_len)(struct Slot* s) {
    ((struct NodeStateBase*)s->out)->value = (int32_t)s->param0;
}
void OP_FN(op_record)(struct Slot* s) {
    ((struct NodeStateBase*)s->out)->value = 0;
}
/* `connected`: VMAX if a cable is patched into the jack (param0 = jack index),
   else 0. `normal` is sugar: (if (connected jack) jack default). Keyed on real
   jack detection, so a clock pulse (0 between edges) still counts as connected. */
void OP_FN(op_connected)(struct Slot* s) {
    ((struct NodeStateBase*)s->out)->value = ((hw_connected >> s->param0) & 1u) ? VMAX : 0;
}
void OP_FN(op_morph)(struct Slot* s) {
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    int32_t n = (int32_t)s->param0;
    if (n <= 1) { st->value = 0; return; }
    int32_t nsig = n - 1;
    if (nsig > 3) nsig = 3;   /* inputs are in0..in3 only; position reads in3 at most */
    const int32_t* ins[4] = {
        (const int32_t*)s->in0, (const int32_t*)s->in1,
        (const int32_t*)s->in2, (const int32_t*)s->in3
    };
    int32_t pos = *(ins[nsig]);
    if (nsig <= 1) { st->value = *(ins[0]); return; }
    int32_t nsig1 = nsig - 1;
    int32_t raw = pos * nsig1;
    int32_t i0  = raw / VMAX;
    int32_t f16 = ((raw % VMAX) << 16) / VMAX;
    if (i0 < 0) i0 = 0;
    if (i0 >= nsig - 1) { st->value = *(ins[(nsig - 1) < 3 ? nsig - 1 : 3]); return; }
    int32_t a = mclamp_(*(ins[i0 < 4 ? i0 : 3]));
    int32_t b = mclamp_(*(ins[(i0 + 1) < 4 ? (i0 + 1) : 3]));
    int32_t v_q16 = a * 65536 + (b - a) * f16;
    int32_t out;
    if (v_q16 >= 0) out = (v_q16 + 32768) >> 16;
    else            out = -(((-v_q16) + 32767) >> 16);
    st->value = out;
}

/* ===== stateful / leaf kernels ===== */

void OP_FN(op_knob)(struct Slot* s) {
    ((struct LeafState*)s->out)->value = *(const int32_t*)s->in0;
}
void OP_FN(op_midi)(struct Slot* s) {
    int32_t v = *(const int32_t*)s->in0;
    ((struct LeafState*)s->out)->value = (v < 0) ? 0 : v;   /* CC "no message" sentinel reads 0 */
}
/* midi-cc with a default: in1 (:init) holds until the first CC lands. */
void OP_FN(op_midi_cc)(struct Slot* s) {
    int32_t v = *(const int32_t*)s->in0;
    ((struct LeafState*)s->out)->value = (v < 0) ? *(const int32_t*)s->in1 : v;
}
/* latest: follow whichever control moved last. in0=a (a knob), in1=b (a CC or
 * other stream), in2=near. b owns at rest, so b's quiet value (a midi-cc
 * :init) is the default. a grabs by straying more than near from its anchor
 * (its position when b last took over), which catches arbitrarily slow turns
 * without per-sample jitter firing; while a owns, the anchor tracks it. Any
 * change of b hands ownership back and re-parks the anchor at a. */
void OP_FN(op_latest)(struct Slot* s) {
    struct LatestState* st = (struct LatestState*)s->out;
    int32_t a    = *(const int32_t*)s->in0;
    int32_t b    = *(const int32_t*)s->in1;
    int32_t near = *(const int32_t*)s->in2;
    /* Zeroed state boots right: a b with :init differs from last_b on the
     * first sample and takes over; with no init (b resting 0), a parked knob
     * strays past near from anchor 0 and owns from the start. */
    if (b != st->last_b) { st->last_b = b; st->own = 0; st->anchor = a; }
    if (!st->own) {
        int32_t d = a - st->anchor;
        if (d < 0) d = -d;
        if (d > near) st->own = 1;
    }
    st->value = st->own ? a : b;
}

/* Chorus/flanger: an LFO-swept fractional tap over a private ring, stereo by
 * an anti-phase LFO (R sweeps opposite L, a wide static image; the constant
 * is pi, not pi/2). Adapted from Vincent Maurer's
 * Flare fork of Lens; wrap is compare-and-subtract, no modulo on the audio
 * path. base/span are in Q16 samples: chorus centres ~10 ms, flanger ~3 ms. */
static inline void mod_delay_(struct Slot* s, int32_t base_q16, int32_t span, uint32_t inc_base, uint32_t inc_scale) {
    struct ChorusState* st = (struct ChorusState*)s->out;
    int32_t inVal = *(const int32_t*)s->in0;
    int32_t rate  = vclamp_(*(const int32_t*)s->in1);
    int32_t depth = vclamp_(*(const int32_t*)s->in2);
    int32_t fb    = vclamp_(*(const int32_t*)s->in3);
    struct Buffer* buf = (struct Buffer*)s->in4;
    if (!buf || buf->length == 0) { st->value = inVal; st->value_r = inVal; return; }
    int32_t len = (int32_t)buf->length;

    st->lfo_phase += inc_base + (((uint32_t)rate * inc_scale) >> 12);
    int32_t lfoL = sine_interp(st->lfo_phase);
    int32_t lfoR = sine_interp(st->lfo_phase + 0x80000000u);
    int32_t swing = (depth * span) >> 12;

    int32_t delayL = base_q16 + lfoL * swing * 32;
    int32_t ipartL = delayL >> 16, fpartL = delayL & 0xFFFF;
    int32_t iL0 = (int32_t)st->head - ipartL;     if (iL0 < 0) iL0 += len;
    int32_t iL1 = iL0 - 1;                        if (iL1 < 0) iL1 += len;
    int32_t sL0 = pack12_read_signed(buf->bytes, (uint32_t)iL0);
    int32_t sL1 = pack12_read_signed(buf->bytes, (uint32_t)iL1);
    int32_t wetL = sL0 + (((sL1 - sL0) * fpartL) >> 16);

    int32_t delayR = base_q16 + lfoR * swing * 32;
    int32_t ipartR = delayR >> 16, fpartR = delayR & 0xFFFF;
    int32_t iR0 = (int32_t)st->head - ipartR;     if (iR0 < 0) iR0 += len;
    int32_t iR1 = iR0 - 1;                        if (iR1 < 0) iR1 += len;
    int32_t sR0 = pack12_read_signed(buf->bytes, (uint32_t)iR0);
    int32_t sR1 = pack12_read_signed(buf->bytes, (uint32_t)iR1);
    int32_t wetR = sR0 + (((sR1 - sR0) * fpartR) >> 16);

    /* :fb is bipolar around VMID (VMID = none, either side inverts or sings).
     * Truncate toward zero (/, not >>): an arithmetic shift pins small negative
     * residues in the ring and the tail never dies. */
    int32_t fb_bipolar = (fb * 2) - 4095;
    pack12_write(buf->bytes, st->head, (uint32_t)sclamp_(inVal + (wetL * fb_bipolar) / 4096));
    st->head++; if (st->head >= (uint32_t)len) st->head = 0;
    st->value   = (inVal + wetL) >> 1;
    st->value_r = (inVal + wetR) >> 1;
}
void OP_FN(op_chorus)(struct Slot* s)  { mod_delay_(s, 480 << 16, 240, 8947, 885833u); }
void OP_FN(op_flanger)(struct Slot* s) { mod_delay_(s, 144 << 16,  96, 4473, 442916u); }

/* Compressor: peak follower + downward gain above :thresh. Adapted from
 * Vincent Maurer's Flare fork of Lens. The gain divide runs only while
 * compressing; ~150 cycles measured on-card by the fork. */
void OP_FN(op_compressor)(struct Slot* s) {
    struct CompressorState* st = (struct CompressorState*)s->out;
    int32_t inVal  = *(const int32_t*)s->in0;
    int32_t thresh = vclamp_(*(const int32_t*)s->in1);
    int32_t ratio  = vclamp_(*(const int32_t*)s->in2);
    int32_t attack = vclamp_(*(const int32_t*)s->in3);
    int32_t release= vclamp_(*(const int32_t*)s->in4);
    int32_t abs_in = inVal >= 0 ? inVal : -inVal;
    int32_t coef = (abs_in > st->envelope) ? (655 + attack * 15) : (32 + release * 2);
    st->envelope += ((abs_in - st->envelope) * coef) >> 16;
    int32_t gain = 4095;
    if (st->envelope > thresh && st->envelope > 0) {
        int32_t excess = st->envelope - thresh;
        int32_t target = st->envelope - ((excess * ((ratio * 3600) >> 12)) >> 12);
        gain = (target * 4095) / st->envelope;
    }
    st->value = sclamp_((inVal * gain) >> 12);
}

/* Echo: a stereo delay unit, adapted from Vincent Maurer's Flare fork of
 * Lens. param0 bits 0..1 = mode (0 mono: whole ring, longest time; 1 stereo:
 * split ring, independent sides; 2 ping-pong: mono input into the left loop,
 * each side feeds the other). param0 bits 8..19 = right-time ratio in Q11.
 * Reads are sub-sample interpolated; feedback products truncate toward zero
 * (/, not >>) so the tail genuinely dies. */
void OP_FN(op_echo)(struct Slot* s) {
    struct EchoState* st = (struct EchoState*)s->out;
    int32_t in_l = *(const int32_t*)s->in0;
    int32_t in_r = *(const int32_t*)s->in1;
    int32_t time = vclamp_(*(const int32_t*)s->in2);
    int32_t fb   = vclamp_(*(const int32_t*)s->in3);
    struct Buffer* buf = (struct Buffer*)s->in4;
    if (!buf || buf->length < 2) { st->value = in_l; st->value_r = in_r; return; }

    uint8_t  mode  = s->param0 & 0x03u;
    int32_t  ratio = (int32_t)((s->param0 >> 8) & 0xFFFu);   /* Q11 */
    int32_t wet_L, wet_R;

    if (mode == 0) {   /* mono: the whole ring, double the time */
        int32_t len = (int32_t)buf->length;
        /* Q12 first: time*(len-1) fits int32 for any pool-sized ring, where
         * the Q16 form (*16) overflows past ~32k cells. Same math exactly. */
        int32_t d12 = time * (len - 1);
        int32_t ip = (d12 >> 12) + 1, fp = (d12 & 0xFFF) << 4;
        int32_t i0 = (int32_t)st->head - ip;  if (i0 < 0) i0 += len;
        int32_t i1 = i0 - 1;                  if (i1 < 0) i1 += len;
        int32_t w0 = pack12_read_signed(buf->bytes, (uint32_t)i0);
        int32_t w1 = pack12_read_signed(buf->bytes, (uint32_t)i1);
        wet_L = w0 + (((w1 - w0) * fp) >> 16);
        wet_R = wet_L;
        int32_t in_sum = (in_l + in_r) / 2;
        pack12_write(buf->bytes, st->head, (uint32_t)sclamp_(in_sum + (wet_L * fb) / 4096));
        st->head++; if (st->head >= (uint32_t)len) st->head = 0;
    } else {           /* stereo / ping-pong: split ring */
        int32_t half = (int32_t)(buf->length >> 1);
        int32_t dL12 = time * (half - 1);
        int32_t ipL = (dL12 >> 12) + 1, fpL = (dL12 & 0xFFF) << 4;
        int32_t l0 = (int32_t)st->head - ipL;  if (l0 < 0) l0 += half;
        int32_t l1 = l0 - 1;                   if (l1 < 0) l1 += half;
        int32_t wl0 = pack12_read_signed(buf->bytes, (uint32_t)l0);
        int32_t wl1 = pack12_read_signed(buf->bytes, (uint32_t)l1);
        wet_L = wl0 + (((wl1 - wl0) * fpL) >> 16);

        int32_t time_R = (time * ratio) >> 11;
        if (time_R > 4095) time_R = 4095;
        int32_t dR12 = time_R * (half - 1);
        int32_t ipR = (dR12 >> 12) + 1, fpR = (dR12 & 0xFFF) << 4;
        int32_t r0 = (int32_t)st->head - ipR;  if (r0 < 0) r0 += half;
        int32_t r1 = r0 - 1;                   if (r1 < 0) r1 += half;
        int32_t wr0 = pack12_read_signed(buf->bytes, (uint32_t)(r0 + half));
        int32_t wr1 = pack12_read_signed(buf->bytes, (uint32_t)(r1 + half));
        wet_R = wr0 + (((wr1 - wr0) * fpR) >> 16);

        int32_t wL, wR;
        if (mode == 1) {   /* stereo: independent sides */
            wL = sclamp_(in_l + (wet_L * fb) / 4096);
            wR = sclamp_(in_r + (wet_R * fb) / 4096);
        } else {           /* ping-pong: mono in on the left, sides swap */
            int32_t in_sum = (in_l + in_r) / 2;
            wL = sclamp_(in_sum + (wet_R * fb) / 4096);
            wR = sclamp_((wet_L * fb) / 4096);
        }
        pack12_write(buf->bytes, st->head, (uint32_t)wL);
        pack12_write(buf->bytes, (uint32_t)((int32_t)st->head + half), (uint32_t)wR);
        st->head++; if (st->head >= (uint32_t)half) st->head = 0;
    }
    st->value   = wet_L;
    st->value_r = wet_R;
}

/* Freeverb comb/allpass ticks, after Freeverb (Jezar at Dreampoint, public
 * domain), adapted from Vincent Maurer's Flare fork of Lens. The reverb's
 * ring is addressed as int16 cells: its lowered buffer asks for
 * ceil(FV_BUF_CELLS*4/3) 12-bit cells so the byte count covers FV_BUF_CELLS
 * int16s (17878*2 bytes). */
static inline int32_t comb_step16(int16_t* buf, uint32_t idx, int32_t inValShifted, int32_t cFb, int32_t* p_comb_flt) {
    int32_t out = buf[idx];
    /* Truncate toward zero (/, not >>) in every feedback product: an
     * arithmetic shift pins small negative values (-1 * 0.9 >> 12 = -1) and
     * the tail never dies. Toward-zero decay guarantees the ring empties. */
    int32_t lp = sclamp_(out / 2 + (*p_comb_flt) / 2);
    *p_comb_flt = lp;
    int32_t writ = sclamp_(inValShifted + (lp * cFb) / 4096);
    buf[idx] = (int16_t)writ;
    return out;
}

__attribute__((always_inline))
static inline int32_t ap_step16(int16_t* buf, uint32_t idx, int32_t wet) {
    int32_t buf_out = buf[idx];
    int32_t writ = sclamp_(wet + buf_out / 2);
    buf[idx] = (int16_t)writ;
    return sclamp_(buf_out - writ / 2);
}

void OP_FN(op_reverb)(struct Slot* s) {
    struct ReverbState* st = (struct ReverbState*)s->out;
    int32_t inVal = *(const int32_t*)s->in0;
    int32_t decay = vclamp_(*(const int32_t*)s->in1);
    int32_t mix   = vclamp_(*(const int32_t*)s->in2);
    struct Buffer* buf = (struct Buffer*)s->in3;

    if (!buf || buf->length < FV_BUF_CELLS) {
        st->value = inVal; st->value_r = inVal; return;
    }
    int16_t* buf16 = (int16_t*)buf->bytes;

    /* Comb feedback: decay 0->4095 maps to ~0.70..0.98 in Q12 (2867..4014). */
    int32_t cFb   = 2867 + ((decay * 1147) >> 12);  /* Q12 */

    /* ---- 6 parallel comb filters (L) ---- */
    int32_t sumL = 0;
    int32_t inValShifted = inVal >> 2;

#define COMB_STEP_L(c, base, len) do { \
    uint32_t pos  = st->comb_pos_L[c]; \
    int32_t  out  = comb_step16(buf16, base + pos, inValShifted, cFb, &st->comb_flt_L[c]); \
    st->comb_pos_L[c] = (pos + 1 >= len) ? 0 : pos + 1; \
    sumL += out; \
} while(0)

    COMB_STEP_L(0, 0, 1116);
    COMB_STEP_L(1, 1116, 1188);
    COMB_STEP_L(2, 2304, 1277);
    COMB_STEP_L(3, 3581, 1356);
    COMB_STEP_L(4, 4937, 1422);
    COMB_STEP_L(5, 6359, 1491);
#undef COMB_STEP_L

    sumL = sclamp_(sumL >> 2);
    int32_t xL = sumL;
    int32_t yL = xL - st->dc_x_L + (st->dc_y_L * 4075) / 4096;
    st->dc_x_L = xL;
    st->dc_y_L = sclamp_(yL);
    sumL = st->dc_y_L;

    /* ---- 6 parallel comb filters (R) ---- */
    int32_t sumR = 0;

#define COMB_STEP_R(c, base, len) do { \
    uint32_t pos  = st->comb_pos_R[c]; \
    int32_t  out  = comb_step16(buf16, base + pos, inValShifted, cFb, &st->comb_flt_R[c]); \
    st->comb_pos_R[c] = (pos + 1 >= len) ? 0 : pos + 1; \
    sumR += out; \
} while(0)

    COMB_STEP_R(0, FV_R_COMB_OFFSET + 0, 1139);
    COMB_STEP_R(1, FV_R_COMB_OFFSET + 1139, 1211);
    COMB_STEP_R(2, FV_R_COMB_OFFSET + 2350, 1300);
    COMB_STEP_R(3, FV_R_COMB_OFFSET + 3650, 1379);
    COMB_STEP_R(4, FV_R_COMB_OFFSET + 5029, 1445);
    COMB_STEP_R(5, FV_R_COMB_OFFSET + 6474, 1514);
#undef COMB_STEP_R

    sumR = sclamp_(sumR >> 2);
    int32_t xR = sumR;
    int32_t yR = xR - st->dc_x_R + (st->dc_y_R * 4075) / 4096;
    st->dc_x_R = xR;
    st->dc_y_R = sclamp_(yR);
    sumR = st->dc_y_R;

    /* ---- 2 series allpass diffusers (L): Schroeder allpass, g=0.5 fixed ---- */
    int32_t wetL = sumL;

#define AP_STEP_L(a, base, len) do { \
    uint32_t pos  = st->ap_pos_L[a]; \
    wetL = ap_step16(buf16, base + pos, wetL); \
    st->ap_pos_L[a] = (pos + 1 >= len) ? 0 : pos + 1; \
} while(0)

    AP_STEP_L(0, FV_L_AP_OFFSET + 0, 556);
    AP_STEP_L(1, FV_L_AP_OFFSET + 556, 441);
#undef AP_STEP_L

    /* ---- 2 series allpass diffusers (R) ---- */
    int32_t wetR = sumR;

#define AP_STEP_R(a, base, len) do { \
    uint32_t pos  = st->ap_pos_R[a]; \
    wetR = ap_step16(buf16, base + pos, wetR); \
    st->ap_pos_R[a] = (pos + 1 >= len) ? 0 : pos + 1; \
} while(0)

    AP_STEP_R(0, FV_R_AP_OFFSET + 0, 579);
    AP_STEP_R(1, FV_R_AP_OFFSET + 579, 464);
#undef AP_STEP_R

    int32_t dryAmt = 4095 - mix;
    st->value   = sclamp_(((inVal * dryAmt) >> 12) + ((wetL * mix) >> 12));
    st->value_r = sclamp_(((inVal * dryAmt) >> 12) + ((wetR * mix) >> 12));
}



static inline int32_t midi_scale7(int32_t v) {
    int32_t r = (v * 127) / 4095;
    return r < 0 ? 0 : (r > 127 ? 127 : r);
}

void OP_FN(op_midi_note_out)(struct Slot* s) {
    MidiNoteOutState* st = (MidiNoteOutState*)s->out;
    int32_t pitch    = *(const int32_t*)s->in0;
    int32_t gate_in  = *(const int32_t*)s->in1;
    int32_t vel_in   = *(const int32_t*)s->in2;
    uint8_t ch       = (uint8_t)(s->param0 & 0x0F);
    int gate_high    = gate_in > 2048;
    uint8_t msg[3];
    if (gate_high && !st->prev_gate) {
        if (st->active) {
            msg[0] = 0x80 | ch; msg[1] = st->cur_note; msg[2] = 0;
            midi_out_push(msg, 3);
        }
        int32_t note = pitch < 0 ? 0 : (pitch > 127 ? 127 : pitch);
        st->cur_note = (uint8_t)note;
        msg[0] = 0x90 | ch; msg[1] = st->cur_note; msg[2] = (uint8_t)midi_scale7(vel_in);
        midi_out_push(msg, 3);
        st->active = 1;
    } else if (!gate_high && st->prev_gate) {
        if (st->active) {
            msg[0] = 0x80 | ch; msg[1] = st->cur_note; msg[2] = 0;
            midi_out_push(msg, 3);
        }
        st->active = 0;
    }
    st->prev_gate = (uint8_t)gate_high;
}

void OP_FN(op_midi_cc_out)(struct Slot* s) {
    MidiCcOutState* st = (MidiCcOutState*)s->out;
    int32_t val = *(const int32_t*)s->in0;
    uint8_t ch    = (uint8_t)(s->param0 & 0x0F);
    uint8_t ccnum = (uint8_t)((s->param0 >> 4) & 0x7F);
    int16_t v7 = (int16_t)midi_scale7(val);
    if (v7 != st->prev7) {
        uint8_t msg[3] = { 0xB0 | ch, ccnum, (uint8_t)v7 };
        midi_out_push(msg, 3);
        st->prev7 = v7;
    }
}

void OP_FN(op_midi_clock_out)(struct Slot* s) {
    MidiClockOutState* st = (MidiClockOutState*)s->out;
    int32_t tick = *(const int32_t*)s->in0;
    uint8_t high = tick > 2048 ? 1 : 0;
    if (high && !st->prev) {
        uint8_t msg = 0xF8;
        midi_out_push(&msg, 1);
    }
    st->prev = high;
}

void OP_FN(op_cv_in)(struct Slot* s) {
    ((struct LeafState*)s->out)->value = *(const int32_t*)s->in0;
}
void OP_FN(op_audio_in)(struct Slot* s) {
    ((struct LeafState*)s->out)->value = *(const int32_t*)s->in0;
}
void OP_FN(op_pulse_in)(struct Slot* s) {
    ((struct LeafState*)s->out)->value = *(const int32_t*)s->in0;
}
void OP_FN(op_switch)(struct Slot* s) {
    ((struct LeafState*)s->out)->value = *(const int32_t*)s->in0;
}
void OP_FN(op_detent)(struct Slot* s) {
    struct LeafState* st = (struct LeafState*)s->out;
    int32_t x = *(const int32_t*)s->in0;
    const int32_t W = 96;
    const void* pts[3] = { s->in1, s->in2, s->in3 };
    uint32_t npts = s->param0;
    for (uint32_t i = 0; i < npts && i < 3; i++) {
        int32_t p = *(const int32_t*)pts[i];
        if (x >= p - W && x <= p + W) { x = p; break; }
    }
    st->value = x;
}

void OP_FN(op_phasor)(struct Slot* s) {
    struct PhasorState* st = (struct PhasorState*)s->out;
    if (s->param0 & 4u) {                        /* :sync to an external clock */
        int32_t sync = *(const int32_t*)s->in1;
        st->sync_count++;
        /* Accept an edge only if it is >= one control window since the last accepted
         * one: that debounces ringing / sub-1ms noise (sync_count keeps counting
         * through rejected edges). An accepted edge IS the downbeat: it locks the
         * rate, hard-syncs the phase, and registers the beat. The external edge is
         * the beat (not the natural wrap), so a synced clock ticks on each pulse. */
        if (trig_rise(&st->last_sync, sync) && st->sync_count >= 64u) {
            if (s->param0 & 8u)              /* lock: external clock also sets the rate */
                st->locked_inc = 0xFFFFFFFFu / st->sync_count;
            st->sync_count = 0;
            st->phase = 0;                   /* hard-sync phase to the external beat */
        }
    }
    /* :lock uses the measured rate once an edge has set it; until then (and if no
       clock is ever patched) free-run at the internal rate rather than freezing. */
    uint32_t inc = ((s->param0 & 8u) && st->locked_inc) ? st->locked_inc
                                    : rate_inc(s->param0 & 3u, *(const int32_t*)s->in0);
    st->phase += inc;
    st->value = phase_to_ramp(st->phase);
    /* The phasor outputs only its ramp. A consumer that wants the beat edge-detects
       it (rising through the low threshold), firing just after the wrap/reset. */
}
void OP_FN(op_sine)(struct Slot* s) {
    struct SineState* st = (struct SineState*)s->out;
    uint32_t phase32;
    if (s->param0 & 16u) {                        /* :phase: driven by an external phase */
        phase32 = ramp_to_phase(*(const int32_t*)s->in0);
        st->phase = phase32;
    } else {
        uint32_t inc = rate_inc(s->param0 & 3u, *(const int32_t*)s->in0);
        if (s->param0 & 64u) inc = inc_detune(inc, *(const int32_t*)s->in4);
        st->phase += inc;
        phase32 = st->phase;
    }
    if (s->param0 & 4u) phase32 += pm_offset(*(const int32_t*)s->in1);
    if (s->param0 & 32u) {                        /* :fb: self-feedback (DX7 compute_fb) */
        /* (y0+y1)>>1 is a one-zero lowpass (null at Nyquist) that damps the loop.
         * The >>LENS_FB_SHIFT tames the phase swing into the DX7 feedback range
         * (full :fb is a classic feedback sawtooth, not a self-oscillating buzz). */
        int32_t fb_avg = (st->y0 + st->y1) >> 1;
        int32_t fb_pm  = scale_depth(fb_avg, *(const int32_t*)s->in3) >> LENS_FB_SHIFT;
        phase32 += pm_offset(fb_pm);
    }
    int32_t val = sine_interp(phase32);
    if (s->param0 & 8u) val = scale_depth(val, *(const int32_t*)s->in2);
    if (s->param0 & 32u) { st->y0 = st->y1; st->y1 = val; }
    st->value = val;
}
void OP_FN(op_triangle)(struct Slot* s) {
    struct TriangleState* st = (struct TriangleState*)s->out;
    uint32_t phase32;
    if (s->param0 & 16u) {                        /* :phase: driven by an external phase */
        phase32 = ramp_to_phase(*(const int32_t*)s->in0);
    } else {
        uint32_t inc = rate_inc(s->param0 & 3u, *(const int32_t*)s->in0);
        if (s->param0 & 64u) inc = inc_detune(inc, *(const int32_t*)s->in1);
        st->phase += inc;
        phase32 = st->phase;
    }
    st->value = phase_to_triangle(phase32);
}

/* DPW band-limited saw (Valimaki 2005, after Chris Johnson's Utility-Pair card). */
static inline int32_t dpw_saw(int32_t signed_phase, int32_t* last_parab, int32_t invc) {
    int32_t r    = signed_phase >> 16;
    int32_t para = r * r;
    int32_t diff = para - *last_parab;
    *last_parab  = para;
    if (invc == 0) return 0;
    return (diff / invc) >> 4;
}

void OP_FN(op_saw)(struct Slot* s) {
    struct SawState* st = (struct SawState*)s->out;
    if (s->param0 & 16u) {                        /* :phase: naive bipolar ramp from an
        external phase. NOT band-limited (the DPW needs a continuous rate); use pitch
        mode for a clean saw. */
        st->value = ((*(const int32_t*)s->in0) & 0xFFF) * 2 - VMAX;
        return;
    }
    uint32_t inc = rate_inc(s->param0 & 3u, *(const int32_t*)s->in0);
    if (s->param0 & 64u) inc = inc_detune(inc, *(const int32_t*)s->in1);
    st->phase += inc;
    int32_t invc = (int32_t)(inc >> 15);
    int32_t v = dpw_saw((int32_t)st->phase, &st->last_parab, invc);
    /* clamp: the DPW differentiator overshoots on its first sample (the previous
       parabola is zero) and occasionally at extreme rates; keep it in range. */
    if (v > VMAX) v = VMAX; else if (v < -VMAX) v = -VMAX;
    st->value = v;
}
void OP_FN(op_square)(struct Slot* s) {
    struct SquareState* st = (struct SquareState*)s->out;
    if (s->param0 & 16u) {                        /* :phase: naive square from an external
        phase (not band-limited; use pitch mode for a clean square). */
        st->value = (((*(const int32_t*)s->in0) & 0xFFF) < VMID) ? VMAX : -VMAX;
        return;
    }
    uint32_t inc = rate_inc(s->param0 & 3u, *(const int32_t*)s->in0);
    if (s->param0 & 64u) inc = inc_detune(inc, *(const int32_t*)s->in1);
    st->phase += inc;
    int32_t invc = (int32_t)(inc >> 15);
    /* :width (param0 bit 7, in2): pulse width as phase offset of the second saw.
       VMID = 50% (the plain square). Clamped off 0/100% so the pulse never dies. */
    uint32_t woff = 0x80000000u;
    if (s->param0 & 128u) {
        int32_t w = *(const int32_t*)s->in2;
        if (w < 64) w = 64; else if (w > 4031) w = 4031;
        woff = (uint32_t)w << 20;
    }
    int32_t a = dpw_saw((int32_t)st->phase, &st->last_parab_a, invc);
    int32_t b = dpw_saw((int32_t)(st->phase + woff), &st->last_parab_b, invc);
    int32_t v = a - b;
    if (v > VMAX) v = VMAX; else if (v < -VMAX) v = -VMAX;
    st->value = v;
}

void OP_FN(op_edge)(struct Slot* s) {
    struct EdgeState* st = (struct EdgeState*)s->out;
    int32_t x = *(const int32_t*)s->in0;
    if (trig_rise(&st->last, x)) st->pulse += (s->param0 != 0) ? (int32_t)s->param0 : kTickWidth;
    if (st->pulse > 0) { st->pulse--; st->value = VMAX; }
    else st->value = 0;
}
void OP_FN(op_fall)(struct Slot* s) {
    struct FallState* st = (struct FallState*)s->out;
    int32_t x = *(const int32_t*)s->in0;
    if (trig_fall(&st->last, x)) st->pulse += (s->param0 != 0) ? (int32_t)s->param0 : kTickWidth;
    if (st->pulse > 0) { st->pulse--; st->value = VMAX; }
    else st->value = 0;
}
void OP_FN(op_diff)(struct Slot* s) {
    struct DiffState* st = (struct DiffState*)s->out;
    int32_t x = *(const int32_t*)s->in0;
    st->value = x - st->last;
    st->last = x;
}
void OP_FN(op_toggle)(struct Slot* s) {
    struct ToggleState* st = (struct ToggleState*)s->out;
    int32_t x = *(const int32_t*)s->in0;
    if (trig_rise(&st->last, x)) st->state = (st->state == 0) ? VMAX : 0;
    st->value = st->state;
}
void OP_FN(op_hold)(struct Slot* s) {
    struct HoldState* st = (struct HoldState*)s->out;
    int32_t val = *(const int32_t*)s->in0;
    int32_t on  = *(const int32_t*)s->in1;
    if (on > VMID) st->last = val;
    st->value = st->last;
}
void OP_FN(op_gate)(struct Slot* s) {
    struct GateState* st = (struct GateState*)s->out;
    int32_t x   = *(const int32_t*)s->in0;
    int32_t len = (s->param0 != 0) ? (int32_t)s->param0 : kTickWidth;
    if (trig_rise(&st->last, x)) st->hold_count = len;
    if (st->hold_count > 0) { st->hold_count--; st->value = VMAX; }
    else st->value = (x > VMID) ? VMAX : 0;
}
void OP_FN(op_schmitt)(struct Slot* s) {
    struct SchmittState* st = (struct SchmittState*)s->out;
    int32_t x  = *(const int32_t*)s->in0;
    int32_t lo = *(const int32_t*)s->in1;
    int32_t hi = *(const int32_t*)s->in2;
    if (x > hi) st->last = VMAX;
    else if (x < lo) st->last = 0;
    st->value = st->last;
}
void OP_FN(op_z1)(struct Slot* s) {
    struct Z1State* st = (struct Z1State*)s->out;
    int32_t x = *(const int32_t*)s->in0;
    st->value = st->last;
    st->last = x;
}
/* Soft takeover. Holds its own value; in0=live control (knob), in1=on (high while this
   control is selected). While on, the held value stays put until the live knob crosses
   it, then follows the knob; this stops a knob from snapping a recalled value to the
   knob's physical position when you switch which thing it edits. Idle (on low) the value
   holds, so per-voice settings persist. param0 packs near (catch threshold, bits 0..15;
   0 -> default 32) and init (starting value, bits 16..27). */
void OP_FN(op_pickup)(struct Slot* s) {
    struct PickupState* st = (struct PickupState*)s->out;
    int32_t live = *(const int32_t*)s->in0;
    int32_t on   = *(const int32_t*)s->in1;
    int32_t near = (int32_t)(s->param0 & 0xFFFFu);
    if (near <= 0) near = 32;
    if (!st->primed) { st->value = (int32_t)((s->param0 >> 16) & 0xFFFu); st->primed = 1; }
    if (on > VMID && st->last_on <= VMID) {     /* arm on the rising edge of select */
        st->armed = 1;
        st->side  = (live >= st->value) ? 1 : -1;
    }
    st->last_on = on;
    if (on > VMID) {
        if (st->armed) {
            int32_t d = live - st->value; if (d < 0) d = -d;
            int32_t cur = (live >= st->value) ? 1 : -1;
            if (cur != st->side || d <= near) st->armed = 0;  /* knob reached/crossed value */
        }
        if (!st->armed) st->value = live;         /* picked up: follow the knob */
    }
}

/* ===== wavetable oscillator ===== */
typedef struct {
    int32_t  value;
    uint32_t phase;
    int32_t  last_wave_lo;
    int32_t  last_wave_hi;
    uint32_t inited;
    int16_t  buf_lo[WT_LEN];
    int16_t  buf_hi[WT_LEN];
} WavetableState;

void OP_FN(op_wavetable)(struct Slot* s) {
    WavetableState* st = (WavetableState*)s->out;
    int32_t in0 = s->in0 ? *(const int32_t*)s->in0 : 69;
    int32_t in1 = slot_in1(s);
    int32_t in2 = slot_in2(s);
    uint32_t rate = rate_inc(s->param0 & 3u, in0);
    if (s->param0 & 64u) rate = inc_detune(rate, slot_in3(s));
    st->phase += rate;
    uint32_t phase_read = st->phase + pm_offset(in2);

    /* pos: 12-bit CV -> wave index + crossfade frac */
    uint32_t pos = (uint32_t)(in1 < 0 ? 0 : in1 > 4095 ? 4095 : in1);
    uint32_t wave_pos  = pos * 31u;
    int32_t  wave_lo   = (int32_t)(wave_pos >> 12);
    int32_t  wave_hi   = wave_lo + 1;
    if (wave_hi > 31) wave_hi = 31;
    uint32_t wave_frac = wave_pos & 0xFFFu;

    int32_t table_idx = (int32_t)(s->param0 >> 2) & 3;
    if (table_idx < 0) table_idx = 0;
    if (table_idx >= WT_NUM_TABLES) table_idx = WT_NUM_TABLES - 1;
    const int16_t *table = WT_TABLES[table_idx];

    if (!st->inited || wave_lo != st->last_wave_lo) {
        memcpy(st->buf_lo, table + (uint32_t)wave_lo * WT_LEN, WT_LEN * 2);
        st->last_wave_lo = wave_lo;
    }
    if (!st->inited || wave_hi != st->last_wave_hi) {
        memcpy(st->buf_hi, table + (uint32_t)wave_hi * WT_LEN, WT_LEN * 2);
        st->last_wave_hi = wave_hi;
    }
    st->inited = 1;

    uint32_t idx  = (phase_read >> 24) & 0xFFu;
    uint32_t frac = (phase_read >> 16) & 0xFFu;
    int32_t s0 = st->buf_lo[idx];
    int32_t s1 = st->buf_lo[(idx + 1u) & 0xFFu];
    int32_t lo_samp = s0 + ((s1 - s0) * (int32_t)frac >> 8);

    s0 = st->buf_hi[idx];
    s1 = st->buf_hi[(idx + 1u) & 0xFFu];
    int32_t hi_samp = s0 + ((s1 - s0) * (int32_t)frac >> 8);

    /* crossfade lo/hi; scale from int16 range to 12-bit audio */
    int32_t raw = lo_samp + (int32_t)(((hi_samp - lo_samp) * (int32_t)wave_frac) >> 12);
    /* scale int16 range -32767..32767 to +-2047; (x*2047+16384)>>15 is equivalent */
    st->value = (raw * 2047 + 16384) >> 15;
}
void OP_FN(op_every)(struct Slot* s) {
    struct EveryState* st = (struct EveryState*)s->out;
    int32_t N   = *(const int32_t*)s->in0;
    int32_t clk = *(const int32_t*)s->in1;
    if (N < 1) N = 1;
    if (trig_fall(&st->last_clk, clk)) {
        st->counter = (st->counter + 1) % N;
        if (st->counter == 0) st->pulse = (s->param0 != 0) ? (int32_t)s->param0 : kTickWidth;
    }
    if (st->pulse > 0) { st->pulse--; st->value = VMAX; }
    else st->value = 0;
}
void OP_FN(op_euclid)(struct Slot* s) {
    struct EuclidState* st = (struct EuclidState*)s->out;
    int32_t P   = *(const int32_t*)s->in0;
    int32_t S   = *(const int32_t*)s->in1;
    int32_t clk = *(const int32_t*)s->in2;
    if (P < 0) P = 0;
    if (S < 1) S = 1;
    if (trig_fall(&st->last_clk, clk)) {
        int32_t i = st->counter % S;
        int32_t prev = (i > 0) ? (i - 1) : (S - 1);
        int32_t cur_step  = (i    * P) / S;
        int32_t prev_step = (prev * P) / S;
        if (cur_step != prev_step) st->pulse = (s->param0 != 0) ? (int32_t)s->param0 : kTickWidth;
        st->counter = (st->counter + 1) % S;
    }
    if (st->pulse > 0) { st->pulse--; st->value = VMAX; }
    else st->value = 0;
}
void OP_FN(op_turns)(struct Slot* s) {
    struct TurnsState* st = (struct TurnsState*)s->out;
    int32_t clk = *(const int32_t*)s->in0;
    if (trig_fall(&st->last_clk, clk)) st->count = (st->count + 1) & VMAX;
    st->value = st->count;
}
void OP_FN(op_counter)(struct Slot* s) {
    /* Counts clock beats: with :bars the count wraps modulo bars (a bar
       position); bars < 1 counts unbounded (a stopwatch). A rising :reset
       (in2) zeroes the count, so elapsed-time-with-reset is one form. */
    struct CounterState* st = (struct CounterState*)s->out;
    int32_t bars  = *(const int32_t*)s->in0;
    int32_t clk   = *(const int32_t*)s->in1;
    int32_t reset = *(const int32_t*)s->in2;
    if (trig_rise(&st->last_reset, reset)) st->count = 0;
    if (trig_fall(&st->last_clk, clk)) {
        st->count++;
        if (bars >= 1) st->count %= bars;
    }
    st->value = st->count;
}

/* ===== filters / audio-shaping kernels ===== */

void OP_FN(op_vca)(struct Slot* s) {
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    st->value = scale_depth(*(const int32_t*)s->in0, *(const int32_t*)s->in1);
}
void OP_FN(op_ring)(struct Slot* s) {
    /* True ring mod: signal x signal, both bipolar, full scale x full scale =
       full scale (denominator SMAX+1, so a shift). op_vca is the gain role. */
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    int32_t p = (*(const int32_t*)s->in0) * (*(const int32_t*)s->in1);
    st->value = p >= 0 ? (p + 1024) >> 11 : -((-p + 1024) >> 11);
}
void OP_FN(op_mix2)(struct Slot* s) {
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    int32_t a = *(const int32_t*)s->in0;
    int32_t b = *(const int32_t*)s->in1;
    int32_t sum = a + b;
    if (sum >= 0) st->value = (sum + 1) >> 1;
    else          st->value = -((-sum) >> 1);
}
void OP_FN(op_mix)(struct Slot* s) {
    struct NodeStateBase* st = (struct NodeStateBase*)s->out;
    uint32_t n = s->param0 ? s->param0 : 2u;
    int32_t sum = *(const int32_t*)s->in0;
    if (n > 1) sum += *(const int32_t*)s->in1;
    if (n > 2) sum += *(const int32_t*)s->in2;
    if (n > 3) sum += *(const int32_t*)s->in3;
    switch (n) {
        case 1: st->value = sum; break;
        case 2:
            st->value = sum >= 0 ? (sum + 1) >> 1 : -((-sum) >> 1);
            break;
        case 3: {
            /* 43691/131072 ~ 1/3; exact floor(x/3) for x in [0, 12286] */
            int32_t x = sum >= 0 ? sum + 1 : -sum + 1;
            int32_t q = (int32_t)(((uint32_t)x * 43691u) >> 17);
            st->value = sum >= 0 ? q : -q;
            break;
        }
        default:
            st->value = sum >= 0 ? (sum + 2) >> 2 : -((-sum + 1) >> 2);
            break;
    }
}
void OP_FN(op_lpf)(struct Slot* s) {
    struct OnePoleState* st = (struct OnePoleState*)s->out;
    st->y_q16 = onepole_step(st->y_q16, *(const int32_t*)s->in0, *(const int32_t*)s->in1);
    st->value = round16(st->y_q16);
}
void OP_FN(op_hpf)(struct Slot* s) {
    struct OnePoleState* st = (struct OnePoleState*)s->out;
    int32_t x = *(const int32_t*)s->in0;
    st->y_q16 = onepole_step(st->y_q16, x, *(const int32_t*)s->in1);
    st->value = x - round16(st->y_q16);
}
/* average, slew: one-pole low-pass smoothers (same body, different names). */
void OP_FN(op_average)(struct Slot* s) {
    struct OnePoleState* st = (struct OnePoleState*)s->out;
    st->y_q16 = onepole_step(st->y_q16, *(const int32_t*)s->in0, *(const int32_t*)s->in1);
    st->value = round16(st->y_q16);
}
void OP_FN(op_slew)(struct Slot* s) {
    struct OnePoleState* st = (struct OnePoleState*)s->out;
    st->y_q16 = onepole_step(st->y_q16, *(const int32_t*)s->in0, *(const int32_t*)s->in1);
    st->value = round16(st->y_q16);
}
void OP_FN(op_vcf)(struct Slot* s) {
    struct VcfState* st = (struct VcfState*)s->out;
    int32_t x   = *(const int32_t*)s->in0;
    int32_t cut = *(const int32_t*)s->in1;
    int32_t res  = *(const int32_t*)s->in2;
    if (res < 0) res = 0; else if (res > VMAX) res = VMAX;
    int32_t port = (int32_t)s->param0 & 3;
    int32_t k  = cut * 16;
    int32_t qf = (VMAX - res) * 16;
    int32_t bp_int = st->bp_q16 >> 16;
    st->lp_q16 += k * bp_int;
    if (st->lp_q16 >  (int32_t)(VMAX * 65536)) st->lp_q16 =  (int32_t)(VMAX * 65536);
    if (st->lp_q16 < -(int32_t)(VMAX * 65536)) st->lp_q16 = -(int32_t)(VMAX * 65536);
    int32_t lp_int = st->lp_q16 >> 16;
    int32_t hp = x - lp_int - ((qf * bp_int) >> 16);
    st->bp_q16 += k * hp;
    if (st->bp_q16 >  (int32_t)(VMAX * 65536)) st->bp_q16 =  (int32_t)(VMAX * 65536);
    if (st->bp_q16 < -(int32_t)(VMAX * 65536)) st->bp_q16 = -(int32_t)(VMAX * 65536);
    int32_t bp_new = st->bp_q16 >> 16;
    int32_t out;
    switch (port) {
        case 1: out = hp;              break;
        case 2: out = bp_new;          break;
        case 3: out = lp_int - bp_new; break;
        default: out = lp_int;         break;
    }
    if (out >  VMAX) out =  VMAX;
    if (out < -VMAX) out = -VMAX;
    st->value = out;
}
/* Resonant 2-pole Chamberlin SVF for audio: lp += f*bp; hp = x-lp-q*bp; bp += f*hp.
 * f  = cut*16  (Q16, 0..1.0 as cut goes 0..VMAX; clamps Chamberlin's HF limit).
 * q  = (VMAX-res)*16  (damping: res=0 -> q~1.0 tame, res=VMAX -> q=0 self-oscillates).
 * State clamped to +-VMAX in Q16 so it can never blow up; output to the audio rail. */
void OP_FN(op_svf)(struct Slot* s) {
    struct SvfState* st = (struct SvfState*)s->out;
    int32_t x    = *(const int32_t*)s->in0;
    int32_t cut  = *(const int32_t*)s->in1;
    int32_t res  = *(const int32_t*)s->in2;
    if (cut < 0) cut = 0; else if (cut > VMAX) cut = VMAX;
    if (res < 0) res = 0; else if (res > VMAX) res = VMAX;
    int32_t port = (int32_t)s->param0 & 3;
    int32_t k  = cut * 16;
    int32_t qf = (VMAX - res) * 16;
    int32_t bp_int = st->bp_q16 >> 16;
    st->lp_q16 += k * bp_int;
    if (st->lp_q16 >  (int32_t)(VMAX * 65536)) st->lp_q16 =  (int32_t)(VMAX * 65536);
    if (st->lp_q16 < -(int32_t)(VMAX * 65536)) st->lp_q16 = -(int32_t)(VMAX * 65536);
    int32_t lp_int = st->lp_q16 >> 16;
    int32_t hp = x - lp_int - ((qf * bp_int) >> 16);
    st->bp_q16 += k * hp;
    if (st->bp_q16 >  (int32_t)(VMAX * 65536)) st->bp_q16 =  (int32_t)(VMAX * 65536);
    if (st->bp_q16 < -(int32_t)(VMAX * 65536)) st->bp_q16 = -(int32_t)(VMAX * 65536);
    int32_t bp_new = st->bp_q16 >> 16;
    int32_t out;
    switch (port) {
        case 1: out = hp;     break;  /* hpf2 */
        case 2: out = bp_new; break;  /* bpf2 */
        default: out = lp_int; break; /* lpf2 */
    }
    st->value = sclamp_(out);
}
void OP_FN(op_noise)(struct Slot* s) {
    struct NoiseState* st = (struct NoiseState*)s->out;
    if (st->rng == 0) st->rng = 12345u;
    st->rng = lcg(st->rng);
    st->value = (int32_t)(st->rng >> 20) - VMID;
}
void OP_FN(op_random)(struct Slot* s) {
    struct RandomState* st = (struct RandomState*)s->out;
    if (st->rng == 0) { st->rng = 99991u; st->cached_value = VMID; }
    int32_t clk = *(const int32_t*)s->in0;
    if (trig_fall(&st->last_clk, clk)) {
        st->rng = lcg(st->rng);
        st->cached_value = (int32_t)(st->rng >> 20);
    }
    st->value = st->cached_value;
}
void OP_FN(op_chance)(struct Slot* s) {
    struct ChanceState* st = (struct ChanceState*)s->out;
    if (st->rng == 0) { st->rng = 77771u; st->cached_value = 0; }
    int32_t p   = *(const int32_t*)s->in0;
    int32_t clk = *(const int32_t*)s->in1;
    if (trig_fall(&st->last_clk, clk)) {
        st->rng = lcg(st->rng);
        int32_t r = (int32_t)(st->rng >> 20);
        if (r == VMAX) r = VMAX - 1;
        st->cached_value = (r < p) ? VMAX : 0;
    }
    st->value = st->cached_value;
}
void OP_FN(op_walk)(struct Slot* s) {
    struct WalkState* st = (struct WalkState*)s->out;
    if (st->rng == 0) { st->rng = 55551u; st->cached_value = VMID; }
    int32_t clk  = *(const int32_t*)s->in0;
    int32_t step = (s->param0 != 0) ? (int32_t)s->param0 : 128;
    if (trig_fall(&st->last_clk, clk)) {
        st->rng = lcg(st->rng);
        /* high bit: an LCG's low bit alternates (period 2), which collapses the walk
         * to a 2-value toggle; the top bit is well distributed. */
        int32_t dir = (st->rng >> 31) ? 1 : -1;
        int32_t v = st->cached_value + dir * step;
        if (v < 0)     v = 0;
        if (v > VMAX) v = VMAX;
        st->cached_value = v;
    }
    st->value = st->cached_value;
}
void OP_FN(op_lpg)(struct Slot* s) {
    struct LpgState* st = (struct LpgState*)s->out;
    int32_t x    = *(const int32_t*)s->in0;
    int32_t ctrl = *(const int32_t*)s->in1;
    if (ctrl < 0)     ctrl = 0;
    if (ctrl > VMAX) ctrl = VMAX;
    st->y_q16 = onepole_step(st->y_q16, x, ctrl);
    st->value = scale_depth(round16(st->y_q16), ctrl);
}
void OP_FN(op_envfollow)(struct Slot* s) {
    struct EnvFollowState* st = (struct EnvFollowState*)s->out;
    int32_t x = *(const int32_t*)s->in0;
    if (x < 0) x = -x;
    int32_t cut = *(const int32_t*)s->in1;
    if (cut < 0)     cut = 0;
    if (cut > VMAX) cut = VMAX;
    st->y_q16 = onepole_step(st->y_q16, x, cut);
    st->value = round16(st->y_q16);
}
void OP_FN(op_wavefold)(struct Slot* s) {
    struct WavefoldState* st = (struct WavefoldState*)s->out;
    int32_t sig = *(const int32_t*)s->in0;
    int32_t drive = *(const int32_t*)s->in1;
    if (drive < 0)     drive = 0;
    if (drive > VMAX) drive = VMAX;
    int32_t x   = (sig * (256 + drive)) >> 8;
    int32_t xw  = (x + 2048) & 8191;
    int32_t val;
    if (xw < 4096) {
        val = (((xw * 2 + 1) * (xw * 2 - 8191)) >> 3);
    } else {
        val = (-((xw * 2 - 8191) * (xw * 2 - 16383))) >> 3;
    }
    int32_t dx = x - st->lastx;
    int32_t ret;
    if (dx > 1 || dx < -1) {
        ret = (val - st->lastval) / dx;
    } else {
        int32_t mid = (x + st->lastx) >> 1;
        int32_t mw  = (mid + 2048) & 8191;
        ret = (mw < 4096) ? (mw - 2048) : ((8191 - mw) - 2048);
    }
    st->lastx   = x;
    st->lastval = val;
    if (ret >  SMAX) ret =  SMAX;
    if (ret < -SMAX) ret = -SMAX;
    st->value = ret;
}
void OP_FN(op_crush)(struct Slot* s) {
    struct CrushState* st = (struct CrushState*)s->out;
    int32_t x    = *(const int32_t*)s->in0;
    int32_t rate = *(const int32_t*)s->in1;
    if (rate < 0)     rate = 0;
    if (rate > VMAX) rate = VMAX;
    int32_t diff = VMAX - rate;
    int32_t rounded = (diff * 2 + 100) / 200;
    int32_t N = rounded + 1;
    if (N < 1) N = 1;
    if (st->count <= 0) { st->held = x; st->count = N; }
    st->count--;
    st->value = st->held;
}

/* Cubic soft-clip f(v) = 1.5v - 0.5v^3, normalised so |v| >= SAT_Q maps to the
   rails. Slope 1.5 at the origin gives the curve makeup so it reaches full scale
   at v = +-SAT_Q. Integer-only: SAT_Q is a power of two so the divides are shifts
   and v*v / v*u2 stay inside int32. */
#define SAT_SHIFT 11
#define SAT_Q     (1 << SAT_SHIFT)   /* 2048 */
static inline int32_t sat_cubic(int32_t v) {
    if (v >=  SAT_Q) return  SMAX;
    if (v <= -SAT_Q) return -SMAX;
    int32_t u2 = (v * v)  >> SAT_SHIFT;   /* v^2 / Q   in [0, Q]   */
    int32_t u3 = (v * u2) >> SAT_SHIFT;   /* v^3 / Q^2 in [-Q, Q]  */
    int32_t r  = (3 * v - u3) >> 1;       /* 1.5v - 0.5 v^3/Q^2    */
    return sclamp_(r);
}
/* in0=sig in1=drive in2=bias in3=mix in4=level. drive pre-gains the input; bias
   shifts the operating point for asymmetric (even-harmonic) clipping, removed
   from the output so silence stays at zero; mix blends dry/wet (Q12); level is
   output makeup gain (Q12). */
void OP_FN(op_saturate)(struct Slot* s) {
    int32_t sig   = *(const int32_t*)s->in0;
    int32_t drive = *(const int32_t*)s->in1;
    int32_t bias  = *(const int32_t*)s->in2;
    int32_t mix   = *(const int32_t*)s->in3;
    int32_t level = *(const int32_t*)s->in4;
    if (drive < 0) drive = 0; else if (drive > VMAX) drive = VMAX;
    if (mix   < 0) mix   = 0; else if (mix   > VMAX) mix   = VMAX;
    if (level < 0) level = 0; else if (level > VMAX) level = VMAX;
    int32_t g   = 256 + drive;                       /* Q8 pre-gain, drive 0 = unity */
    int32_t xg  = (sig * g) >> 8;
    int32_t wet = sat_cubic(xg + bias) - sat_cubic(bias);
    int32_t out = sig + (((wet - sig) * mix) >> 12); /* dry/wet blend */
    out = (out * level) >> 12;                       /* makeup gain */
    *(int32_t*)s->out = sclamp_(out);
}

/* Lookup-table waveshaper, after PiPicoFX (StoneRose35, waveShaper.c): a small
 * baked transfer curve, zero multiplies on the lookup. SHAPE_LUT holds four
 * curves, 256 entries each, in flash (static const). Each entry is the shaped
 * output for a post-drive input bin in [-SMAX, SMAX]:
 *   0 soft  tanh sigmoid, rounds gently
 *   1 hard  hard clip (2x knee), abrupt limiting
 *   2 asym  biased tanh, asymmetric -> even harmonics
 *   3 over  diode/overdrive sigmoid, steeper soft knee
 * Generated by scratchpad/gen_shape_lut.js (DC removed so f(0)=0). */
#include "shape_lut.h"

/* index = (xg+2048)>>4 maps [-2047,2047] -> [0,255]; pure shift, no multiply. */
static inline int32_t shape_lookup(const int16_t* lut, int32_t xg) {
    if (xg >  SMAX) xg =  SMAX;
    if (xg < -SMAX) xg = -SMAX;
    return lut[(xg + 2048) >> 4];
}
/* in0=sig in1=drive(0..VMAX). param0: bits0..2 = curve, bit4 = 4x oversample.
   drive pre-gains the signal into the curve (drive 0 = unity); harder drive
   pushes more of the waveform into the saturating region. Oversampling linearly
   upsamples the input 4x, shapes each subsample, then box-filters down to cut
   the alias products from heavy shaping; off by default (it quadruples the
   lookups), enabled with :oversample 1. */
void OP_FN(op_shape)(struct Slot* s) {
    struct ShapeState* st = (struct ShapeState*)s->out;
    int32_t sig   = *(const int32_t*)s->in0;
    int32_t drive = *(const int32_t*)s->in1;
    if (drive < 0) drive = 0; else if (drive > VMAX) drive = VMAX;
    int32_t curve = (int32_t)s->param0 & 7; if (curve > 3) curve = 3;
    const int16_t* lut = SHAPE_LUT[curve];
    int32_t g = 256 + drive;                          /* Q8 pre-gain */
    int32_t out;
    if (s->param0 & 16) {
        int32_t x0 = st->lastx, dx = sig - x0, acc = 0;
        for (int k = 1; k <= 4; k++) {                /* k=4 lands on sig */
            int32_t sub = x0 + ((dx * k) >> 2);
            acc += shape_lookup(lut, (sub * g) >> 8);
        }
        out = acc >> 2;                               /* box-filter mean */
    } else {
        out = shape_lookup(lut, (sig * g) >> 8);
    }
    st->lastx = sig;
    st->value = sclamp_(out);
}

/* ===== drums kernels ===== */
/* Drum voices after Mutable Instruments Plaits (Émilie Gillet, MIT).
 * Reimplemented for the 12-bit Loupe runtime. Numerics per
 * attic/specs/heritage-kernels.md. */

void OP_FN(op_kick)(struct Slot* s) {
    struct KickState* st = (struct KickState*)s->out;
    int32_t trig  = slot_in0(s);
    if (trig_rise(&st->last_trig, trig)) {
        st->level = SMAX << 12;
        st->pitchEnv = 1 << 15;
        st->phase = 0;
    }
    int32_t note  = slot_in1(s);
    int32_t decay = vclamp_(slot_in2(s));
    int32_t drive = vclamp_(slot_in3(s));
    int32_t sweep = vclamp_(slot_in4(s));
    uint32_t base = pitch_table[note & 0x7F];
    if (!base) base = 2926328u;
    int32_t pe = st->pitchEnv;
    uint32_t baseEnv = umulhi32(base, (uint32_t)pe << 16);
    uint32_t inc = base + ((uint32_t)(baseEnv >> 7) * (uint32_t)sweep);
    if (inc > 1700000000u || inc < base) inc = 1700000000u;
    st->pitchEnv = (pe * 65366) >> 16;
    st->phase += inc;
    int32_t osc = sineInterp_(st->phase);
    int32_t g = 256 + ((drive * drive) >> 13);
    int32_t hot = sclamp_((osc * g) >> 8);
    int32_t shaped = osc + (((hot - osc) * drive) >> VBITS_);
    if (decay != st->cached_decay || !st->cached_k) { st->cached_k = drumDecayK_(decay, 10, 48); st->cached_decay = decay; }
    int32_t k = st->cached_k;
    { int32_t dec_ = ((st->level >> 9) * k) >> 7; st->level -= (dec_ > 0 ? dec_ : 1); }
    if (st->level < 0) st->level = 0;
    int32_t out = sclamp_(ditherShift_(&st->dither, shaped * (st->level >> 5), 18));
    st->lp += onePoleStep((out << 12) - st->lp, (uint32_t)50000 << 16);
    int32_t hplp = st->hp;
    hplp += onePoleStep(st->lp - hplp, (uint32_t)400 << 16);
    st->hp = hplp;
    st->value = sclamp_(ditherShift_(&st->dither, st->lp - hplp, 12));
}

void OP_FN(op_snare)(struct Slot* s) {
    struct SnareState* st = (struct SnareState*)s->out;
    int32_t trig = slot_in0(s);
    if (trig_rise(&st->last_trig, trig)) {
        st->level = SMAX << 12;
        st->pitchEnv = 1 << 15;
    }
    if (st->rng == 0) st->rng = rngSeed_((int)s->param0);
    int32_t note   = slot_in1(s);
    int32_t decay  = vclamp_(slot_in2(s));
    int32_t snappy = vclamp_(slot_in3(s));
    int32_t tone   = vclamp_(slot_in4(s));
    uint32_t inc1 = pitch_table[note & 0x7F];
    if (!inc1) inc1 = 11705314u;
    int32_t pe = st->pitchEnv;
    inc1 += umulhi32(inc1, (uint32_t)pe << 16) * 4;
    st->pitchEnv = (pe * 65468) >> 16;
    uint32_t inc2 = inc1 + (inc1 >> 1);
    st->phase += inc1;
    st->pos   += inc2;
    int32_t body = (sineInterp_(st->phase) + sineInterp_(st->pos)) >> 1;
    if (decay != st->cached_decay || !st->cached_k) { st->cached_k = drumDecayK_(decay, 10, 48); st->cached_decay = decay; }
    int32_t k = st->cached_k;
    { int32_t dec_ = ((st->level >> 9) * k) >> 7; st->level -= (dec_ > 0 ? dec_ : 1); } if (st->level < 0) st->level = 0;
    int32_t ampB = st->level >> 12;
    int32_t ampN = (ampB * ampB) >> 11;
    st->rng = xorshift32_(st->rng);
    int32_t noise = (int32_t)(st->rng >> (32 - VBITS_)) - VMID;
    uint32_t kc = (uint32_t)((tone < 1 ? 1 : tone) << (16 - VBITS_));
    st->lp1 += onePoleStep((noise << 12) - st->lp1, kc << 16);
    int32_t hpn = noise - (st->lp1 >> 12);
    st->lp2 += onePoleStep((hpn << 12) - st->lp2, kc << 16);
    int32_t hp = st->lp2 >> 12;
    int32_t gB = ditherShift_(&st->dither, body * (st->level >> 5), 18);
    int32_t gN = (hp * ampN) >> 11;
    st->value = sclamp_(gB + (((gN - gB) * snappy) >> VBITS_));
}

void OP_FN(op_hat)(struct Slot* s) {
    struct HatState* st = (struct HatState*)s->out;
    int32_t trig = slot_in0(s);
    if (trig_rise(&st->last_trig, trig)) st->level = SMAX << 12;
    int32_t note  = slot_in1(s);
    int32_t decay = vclamp_(slot_in2(s));
    int32_t tone  = vclamp_(slot_in3(s));
    uint32_t inc1 = pitch_table[note & 0x7F];
    if (!inc1) inc1 = 93642516u;
    uint32_t inc2 = inc1 + umulhi32(inc1, 1932735283u);
    uint32_t inc3 = inc1 + umulhi32(inc1, 2662881724u);
    st->p1 += inc1;
    st->p2 += inc2;
    st->p3 += inc3;
    uint32_t p1 = st->p1, p2 = st->p2, p3 = st->p3;
    int32_t metal = ((p1 & 0x80000000u) ? 1 : -1)
                  + ((p2 & 0x80000000u) ? 1 : -1)
                  + ((p3 & 0x80000000u) ? 1 : -1)
                  + (((p1 + p2) & 0x80000000u) ? 1 : -1)
                  + (((p2 + p3) & 0x80000000u) ? 1 : -1)
                  + (((p1 + p3) & 0x80000000u) ? 1 : -1);
    int32_t sig = metal * 340;
    uint32_t kc = (uint32_t)((tone < 1 ? 1 : tone) << (16 - VBITS_));
    st->hp += ((((sig << 12) - st->hp) >> 10) * (int32_t)kc) >> 6;
    int32_t hp = sig - (st->hp >> 12);
    if (decay != st->cached_decay || !st->cached_k) { st->cached_k = drumDecayK_(decay, 10, 24); st->cached_decay = decay; }
    int32_t k = st->cached_k;
    { int32_t dec_ = ((st->level >> 9) * k) >> 7; st->level -= (dec_ > 0 ? dec_ : 1); } if (st->level < 0) st->level = 0;
    st->value = sclamp_(ditherShift_(&st->dither, hp * (st->level >> 5), 18));
}

/* ===== voices kernels ===== */

void OP_FN(op_envelope)(struct Slot* s) {
    struct EnvelopeState* st = (struct EnvelopeState*)s->out;
    int32_t trig  = slot_in0(s);
    int32_t decay = vclamp_(slot_in1(s));
    int32_t peak = (int32_t)s->param0;
    if (peak <= 0 || peak > VMAX) peak = VMAX;
    if (trig_rise(&st->last_trig, trig)) st->level = peak << 8;
    if (decay != st->cached_decay || !st->cached_k) { st->cached_k = drumDecayK_(decay, 10, 48); st->cached_decay = decay; }
    int32_t k = st->cached_k;
    /* One-pole decay with a floor of 1 per sample: the integer step underflows
     * to 0 below ~65536/k, which would park the tail just above silence
     * forever. The minimum step walks the last inaudible stretch to true 0. */
    int32_t dec = ((st->level >> 9) * k) >> 7;
    st->level -= (dec > 0 ? dec : 1);
    if (st->level < 0) st->level = 0;
    st->value = st->level >> 8;
}

/* in0=gate, in1=attack, in2=decay, in3=sustain(level 0..VMAX), in4=release; param0=peak.
   Rates are "times": higher value = slower, via drumDecayK_ (consistent with op_envelope :decay). */
void OP_FN(op_adsr)(struct Slot* s) {
    struct AdsrState* st = (struct AdsrState*)s->out;
    int32_t gate = slot_in0(s);
    int32_t a    = vclamp_(slot_in1(s));
    int32_t d    = vclamp_(slot_in2(s));
    int32_t sus  = vclamp_(slot_in3(s));
    int32_t r    = vclamp_(slot_in4(s));
    int32_t peak = (int32_t)s->param0; if (peak <= 0 || peak > VMAX) peak = VMAX;
    /* Both edges compare against the same prev; read prev once then update. */
    { int32_t prev = st->last_gate; st->last_gate = gate;
      if (gate > kEdgeLevel && prev <= kEdgeLevel) st->phase = 1;
      if (gate <= VMID      && prev > VMID)         st->phase = 3; }
    if (a != st->ca) { st->ka = drumDecayK_(a, 10, 48); st->ca = a; }
    if (d != st->cd) { st->kd = drumDecayK_(d, 10, 48); st->cd = d; }
    if (r != st->cr) { st->kr = drumDecayK_(r, 10, 48); st->cr = r; }
    int32_t peakL = peak << 8;
    int32_t susL  = ((sus * peak) / VMAX) << 8;   /* sus*peak <= ~16.7M, fits int32 */
    int32_t target, k;
    if (st->phase == 1)      { target = peakL; k = st->ka; if (st->level >= peakL - (peakL >> 6)) st->phase = 2; }
    else if (st->phase == 2) { target = susL;  k = st->kd; }
    else                     { target = 0;     k = st->kr; }
    /* One-pole toward target, minimum step 1 so every segment actually
     * arrives (the integer step underflows to 0 within ~512 of the target,
     * which would hold release just above silence and decay off the sustain). */
    int32_t step = (((target - st->level) >> 9) * k) >> 7;
    if (step == 0 && st->level != target) step = (target > st->level) ? 1 : -1;
    st->level += step;
    if (st->level < 0) st->level = 0;
    if (st->level > peakL) st->level = peakL;
    st->value = st->level >> 8;
}

/* 2^(i/256) in Q16, for reconstructing the linear envelope gain from the
   log2-domain level (port of msfa Exp2's fractional part). The curve is the
   shared EXP2_FRAC_LUT (proven byte-identical for i<256), used directly here. */

/* Set up the next DX7 EG segment: target level, climb direction, and rate
   (port of msfa Env::advance, ticking per-sample so LG_N is dropped). */
/* msfa Env::sr_multiplier = (44100/48000)*(1<<24), truncated to int. */
#define DXEG_SR_MULT_Q24 15414067u

/* x * sr_multiplier >> 24, 32-bit (positive x; segment-change cost, not hot loop). */
static inline int32_t dxeg_sr_scale(uint32_t x) {
    uint32_t hi = umulhi32(x, DXEG_SR_MULT_Q24);
    uint32_t lo = x * DXEG_SR_MULT_Q24;
    return (int32_t)((hi << 8) | (lo >> 24));
}

/* msfa env.cc statics[]: samples at 44.1 kHz to hold a flat EG segment, indexed by
   the segment's (raw rate + rate_scaling), capped at 76; above that, 20*(99-rate). */
static const int32_t DXEG_STATICS[77] = {
    1764000, 1764000, 1411200, 1411200, 1190700, 1014300, 992250,
    882000, 705600, 705600, 584325, 507150, 502740, 441000, 418950,
    352800, 308700, 286650, 253575, 220500, 220500, 176400, 145530,
    145530, 125685, 110250, 110250, 88200, 88200, 74970, 61740,
    61740, 55125, 48510, 44100, 37485, 31311, 30870, 27562, 27562,
    22050, 18522, 17640, 15435, 14112, 13230, 11025, 9261, 9261, 7717,
    6615, 6615, 5512, 5512, 4410, 3969, 3969, 3439, 2866, 2690, 2249,
    1984, 1896, 1808, 1411, 1367, 1234, 1146, 926, 837, 837, 705,
    573, 573, 529, 441, 441
};

/* msfa ScaleRate: per-note qrate boost from keyboard position * the op's rate-
   scaling sensitivity. Speeds the EG up toward the top of the keyboard. */
static inline int32_t dx7_scale_rate(int32_t midinote, int32_t sens) {
    int32_t x = midinote / 3 - 7;
    if (x < 0) x = 0; else if (x > 31) x = 31;
    return (sens * x) >> 3;
}

static inline void dxeg_advance(struct DxEgState* st, int32_t newix,
                                const int32_t R[4], const int32_t L[4]) {
    st->ix_ = newix;
    if (newix < 4) {
        st->targetlevel_ = L[newix] << 20;     /* byte<<20 == actuallevel<<16 */
        st->rising_ = st->targetlevel_ > st->level_;
        int32_t r = R[newix];
        int32_t qrate = ((r * 41) >> 6) + st->rate_scaling_;
        if (qrate > 63) qrate = 63;
        if (qrate < 0) qrate = 0;
        /* sr_multiplier (44.1k rate tables); applied at segment change, not the hot loop. */
        st->inc_ = dxeg_sr_scale((uint32_t)((4 + (qrate & 3)) << (2 + (qrate >> 2))));

        /* Flat segment (target already reached): hold for the measured static time
           instead of advancing instantly, so the EG timing matches the real DX7. */
        if (st->targetlevel_ == st->level_) {
            int32_t sr = r + st->rate_scaling_;
            if (sr > 99) sr = 99;
            if (sr < 0)  sr = 0;
            st->staticcount_ = dxeg_sr_scale((uint32_t)(sr < 77 ? DXEG_STATICS[sr]
                                                               : 20 * (99 - sr)));
        } else {
            st->staticcount_ = 0;
        }
    }
}

/* DX7-faithful log-domain EG, one per-sample step (port of Dexed/msfa Env).
   Shared core: op_dxeg wraps it from slots; op_dx runs one per operator. Reads
   gate + raw DX7 rates R[0..3] and packed segment-target levels L[0..3] (each
   byte == clamp(actuallevel>>4,0,255)); advances the segment state in `st` and
   returns a LINEAR amplitude with unity gain (1.0) == VMAX. */
int32_t __noinline __not_in_flash_func(dxeg_step)(struct DxEgState* st, int32_t gate,
                                const int32_t R[4], const int32_t L[4], int32_t rate_scaling) {
    st->rate_scaling_ = rate_scaling;           /* read by dxeg_advance's qrate */
    if (!st->inited) {                          /* idle at the floor until first gate */
        st->inited = 1;
        st->level_ = 16 << 16;
        st->ix_ = 4;
    }
    /* Both edges compare against the same prev; read prev once then update. */
    { int32_t prev = st->last_gate; st->last_gate = gate;
      if (gate > kEdgeLevel && prev <= kEdgeLevel) { st->down_ = 1; dxeg_advance(st, 0, R, L); }
      if (gate <= VMID      && prev > VMID)         { st->down_ = 0; dxeg_advance(st, 3, R, L); } }

    if (st->staticcount_) {                     /* holding a flat segment */
        if (--st->staticcount_ <= 0) {
            st->staticcount_ = 0;
            dxeg_advance(st, st->ix_ + 1, R, L);
        }
    }

    if (st->ix_ < 3 || (st->ix_ < 4 && !st->down_)) {
        if (st->staticcount_) {
            /* still holding: level stays put */
        } else if (st->rising_) {
            const int32_t jumptarget = 1716;
            if (st->level_ < (jumptarget << 16)) st->level_ = jumptarget << 16;
            st->level_ += (((17 << 24) - st->level_) >> 24) * st->inc_;
            if (st->level_ >= st->targetlevel_) {
                st->level_ = st->targetlevel_;
                dxeg_advance(st, st->ix_ + 1, R, L);
            }
        } else {
            st->level_ -= st->inc_;
            if (st->level_ <= st->targetlevel_) {
                st->level_ = st->targetlevel_;
                dxeg_advance(st, st->ix_ + 1, R, L);
            }
        }
    }

    /* amp = VMAX * 2^((level_>>16)/256 - 14), via 2^x = 2^int * 2^frac. */
    int32_t a = st->level_ >> 16;               /* actuallevel ~16..3840 */
    if (a < 0) a = 0;
    int32_t ip = a >> 8;                         /* integer part 0..15 */
    int32_t fr = a & 0xFF;                       /* fractional 0..255 */
    int32_t amp;
    if (ip >= 16) {
        amp = 4 * VMAX;
    } else {
        amp = (int32_t)(((uint32_t)VMAX * (uint32_t)EXP2_FRAC_LUT[fr]) >> (30 - ip));
    }
    if (amp > 4 * VMAX) amp = 4 * VMAX;
    if (amp < 0) amp = 0;
    return amp;
}

/* in0=gate, in1=R1, in2=R2, in3=R3, in4=R4 (raw DX7 rates 0..99);
   param0 packs four segment-target bytes L1|L2<<8|L3<<16|L4<<24. */
void OP_FN(op_dxeg)(struct Slot* s) {
    struct DxEgState* st = (struct DxEgState*)s->out;
    int32_t gate = slot_in0(s);
    int32_t R[4] = { slot_in1(s), slot_in2(s), slot_in3(s), slot_in4(s) };
    uint32_t p = (uint32_t)s->param0;
    int32_t L[4] = { (int32_t)(p & 0xFF), (int32_t)((p >> 8) & 0xFF),
                     (int32_t)((p >> 16) & 0xFF), (int32_t)((p >> 24) & 0xFF) };
    st->value = dxeg_step(st, gate, R, L, 0);
}

/* Round-to-nearest divide for signed sums (positive and negative). */
static inline int32_t avg_round(int32_t sum, int32_t n) {
    if (sum >= 0) return (sum + n / 2) / n;
    else          return -((-sum + (n - 1) / 2) / n);
}

static inline int32_t dx7_scale_out(int32_t x);   /* defined with dx7_parse_voice */

/* Keyboard level scaling (msfa dx7note.cc ScaleLevel/ScaleCurve). An operator's
   output level is offset by keyboard position so timbre mellows toward the treble.
   Returns a signed delta in scaleoutlevel units, added to the op's output level. */
static const uint8_t DX7_EXP_SCALE[33] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 16, 19, 23, 27, 33, 39, 47, 56, 66,
    80, 94, 110, 126, 142, 158, 174, 190, 206, 222, 238, 250
};
static inline int32_t dx7_scale_curve(int32_t group, int32_t depth, int32_t curve) {
    int32_t scale;
    if (curve == 0 || curve == 3) {                 /* linear */
        scale = (group * depth * 329) >> 12;
    } else {                                        /* exponential */
        int32_t g = group < 0 ? 0 : (group > 32 ? 32 : group);
        scale = ((int32_t)DX7_EXP_SCALE[g] * depth * 329) >> 15;
    }
    return curve < 2 ? -scale : scale;
}
static inline int32_t dx7_scale_level(int32_t midinote, int32_t break_pt,
                                      int32_t left_depth, int32_t right_depth,
                                      int32_t left_curve, int32_t right_curve) {
    int32_t offset = midinote - break_pt - 17;
    if (offset >= 0) return dx7_scale_curve((offset + 1) / 3, right_depth, right_curve);
    else             return dx7_scale_curve(-(offset - 1) / 3, left_depth, left_curve);
}

/* Per-op keyboard-scaling gain multiplier (Q16). msfa folds level_scaling into the
   op's outlevel before the min(127) cap and the <<5; the net effect on every EG
   target is a constant offset, so we apply it as a gain multiply: 2^(delta_ol/8). */
static inline int32_t dx7_keyscale_mult(const uint8_t* op17, int32_t midinote) {
    int32_t ls = dx7_scale_level(midinote, op17[8], op17[9], op17[10],
                                 op17[11] & 3, (op17[11] >> 2) & 3);
    int32_t so = dx7_scale_out((int32_t)op17[14]);
    int32_t base   = so > 127 ? 127 : so;
    int32_t capped = (so + ls) > 127 ? 127 : (so + ls);
    int32_t delta_ol = (capped - base) << 5;        /* outlevel units, <<5 like msfa */
    return exp2_fixed(delta_ol << 8);               /* 2^(delta_ol/256) in Q16 */
}

/* Load the global pitch EG from a DX7 voice (bytes 102..109) into a PitchEnvState. */
static inline void pitchenv_load(struct PitchEnvState* pe, const uint8_t* v128) {
    for (int i = 0; i < 4; i++) pe->rates_[i]  = (int32_t)(v128[102 + i] & 0x7F);
    for (int i = 0; i < 4; i++) pe->levels_[i] = (int32_t)(v128[106 + i] & 0x7F);
}

/* PitchEnv::advance (pitchenv.cc): set the next segment's target + per-sample inc. */
static inline void pitchenv_advance(struct PitchEnvState* pe, int32_t newix) {
    pe->ix_ = newix;
    if (newix < 4) {
        pe->targetlevel_ = (int32_t)msfa_pitchenv_tab[pe->levels_[newix]] << 19;
        pe->rising_ = pe->targetlevel_ > pe->level_;
        pe->inc_ = (msfa_pitchenv_rate[pe->rates_[newix]] * MSFA_PITCHENV_UNIT_Q8) >> 8;
    }
}

/* PitchEnv::getsample with gate handling, one per-sample step. Returns the pitch
   offset (logfreq Q24) added to ratio operators' frequency. */
static inline int32_t pitchenv_step(struct PitchEnvState* pe, int32_t gate) {
    if (!pe->inited) {                              /* settle at L4 before the first gate */
        pe->inited = 1;
        pe->level_ = (int32_t)msfa_pitchenv_tab[pe->levels_[3]] << 19;
        pe->ix_ = 4;
        pe->down_ = 1;
    }
    /* Both edges compare against the same prev; read prev once then update. */
    { int32_t prev = pe->last_gate; pe->last_gate = gate;
      if (gate > kEdgeLevel && prev <= kEdgeLevel) { pe->down_ = 1; pitchenv_advance(pe, 0); }
      if (gate <= VMID      && prev > VMID)         { pe->down_ = 0; pitchenv_advance(pe, 3); } }

    if (pe->ix_ < 3 || (pe->ix_ < 4 && !pe->down_)) {
        if (pe->rising_) {
            pe->level_ += pe->inc_;
            if (pe->level_ >= pe->targetlevel_) { pe->level_ = pe->targetlevel_; pitchenv_advance(pe, pe->ix_ + 1); }
        } else {
            pe->level_ -= pe->inc_;
            if (pe->level_ <= pe->targetlevel_) { pe->level_ = pe->targetlevel_; pitchenv_advance(pe, pe->ix_ + 1); }
        }
    }
    return pe->level_;
}

/* msfa midinote_to_logfreq: Q24 logfreq (1.0 = one octave) of a MIDI note. */
static inline int32_t midinote_to_logfreq(int32_t n) {
    return MSFA_MIDILF_BASE + n * MSFA_MIDILF_STEP;
}

/* Note-independent logfreq base for one operator (osc_freq, dx7note.cc). Ratio ops
   return coarse+fine (the played note + pitch env + ratio detune are added later);
   fixed ops return their absolute logfreq with the fixed detune folded in. */
static inline int32_t dx7_osc_logfreq_base(int32_t mode, int32_t coarse, int32_t fine,
                                           int32_t detune) {
    if (mode) {  /* fixed */
        int32_t lf = (4458616 * ((coarse & 3) * 100 + fine)) >> 3;
        return lf + (detune > 7 ? 13457 * (detune - 7) : 0);
    }
    int32_t lf = msfa_coarsemul[coarse & 31];
    if (fine) lf += msfa_fine_lf[fine < 0 ? 0 : (fine > 99 ? 99 : fine)];
    return lf;   /* ratio detune is note-dependent, added at note-on */
}

/* Ratio-op detune (osc_freq, dx7note.cc lines 61-62): a per-op micro-offset
   (a few cents at most) applied to the played-note logfreq. It depends on the
   note, so it is recomputed at note-on, costing nothing per sample. detune is
   0..14 with 7 = none. exp(-0.396*oct) = 2^(-0.5713*oct) via exp2_fixed. */
static inline int32_t dx7_ratio_detune(int32_t note_lf, int32_t detune) {
    if (detune == 7) return 0;
    int32_t arg = -(((note_lf >> 8) * 146) >> 8);   /* -0.5713*oct, Q16 */
    int32_t e   = exp2_fixed(arg);                  /* exp(-0.396*oct), Q16 */
    int32_t dr  = (e * 196) >> 16;                  /* detuneRatio = 0.0029857*e, Q16 */
    int32_t k   = dr * (detune - 7);
    uint32_t ak = (uint32_t)(k < 0 ? -k : k);
    uint32_t p  = (umulhi32((uint32_t)note_lf, ak) << 16) | (((uint32_t)note_lf * ak) >> 16);
    return k < 0 ? -(int32_t)p : (int32_t)p;
}

/* Fused DX7 voice from a flash bank. param0=bank; in0=decay(0..4095,2048=unity),
   in1=pitch, in2=gate, in3=preset, in4=tone(0..4095,2048=unity). The parsed voice
   (56 cells: cell[0]=algo, cell[1]=fbscale, op k at 2+9k) is cached per preset. */
void OP_FN(op_dx)(struct Slot* s) {
    struct FmState* st = (struct FmState*)s->out;

    int32_t pitch  = s->in1 ? slot_in1(s) : 69;
    int32_t gate   = s->in2 ? slot_in2(s) : 0;
    int32_t preset = s->in3 ? slot_in3(s) : 0;

    /* decay: 0..4095, centre 2048=unity, scales all per-op EG rates (R*decay>>11).
       tone:  0..4095, centre 2048=unity, scales modulator (non-carrier) outs. */
    int32_t decay = s->in0 ? slot_in0(s) : 2048;
    int32_t tone  = s->in4 ? slot_in4(s) : 2048;
    const struct Dx7Bank* B = &DX7_BANKS[s->param0 % (uint32_t)NUM_DX7_BANKS];
    if (!B->data || B->nvoices == 0) { st->value = 0; return; }   /* no banks loaded: silent */
    int32_t p = preset < 0 ? 0 : preset >= (int32_t)B->nvoices ? (int32_t)B->nvoices - 1 : preset;
    if (!st->cells_valid || st->cached_bank != (int32_t)s->param0 || st->cached_preset != p) {
        const uint8_t* vdata = B->data + (uint32_t)p * 128u;
        dx7_parse_voice(vdata, st->cells);
        pitchenv_load(&st->peg, vdata);
        /* per-op frequency (osc_freq logfreq base + ratio mask). Packed order
           OP6..OP1 -> ops[0..5]=OP1..OP6 (op = 5-i). Byte 15 = (coarse<<1)|mode,
           16 = fine, 12 = (detune<<3)|RS. */
        st->op_ratio = 0;
        for (int i = 0; i < 6; i++) {
            int op = 5 - i;
            const uint8_t* o = vdata + i * 17;
            int32_t mode = o[15] & 1, coarse = (o[15] >> 1) & 31, fine = o[16];
            int32_t detune = (o[12] >> 3) & 15;
            st->logfreq[op] = dx7_osc_logfreq_base(mode, coarse, fine, detune);
            if (!mode) st->op_ratio |= 1 << op;
        }
        st->cached_bank   = (int32_t)s->param0;
        st->cached_preset = p;
        st->cells_valid   = 1;
        st->ks_pitch      = -1;        /* force keyscale recompute below */
    }
    /* Keyboard level scaling depends on the played note: recompute the per-op
       gain multipliers only when the note (or voice) changes. */
    if (st->ks_pitch != pitch) {
        const uint8_t* vdata = B->data + (uint32_t)p * 128u;
        int32_t nlf = midinote_to_logfreq(midi_clamp(pitch));
        for (int i = 0; i < 6; i++) {
            int op = 5 - i;
            const uint8_t* o = vdata + i * 17;
            st->ks_mult[op] = dx7_keyscale_mult(o, pitch);
            st->rate_scaling[op] = dx7_scale_rate(pitch, o[12] & 7);   /* ScaleRate, note-on */
            /* ratio detune is note-dependent: refold it into logfreq each note. */
            if (st->op_ratio & (1 << op)) {
                int32_t coarse = (o[15] >> 1) & 31, fine = o[16], detune = (o[12] >> 3) & 15;
                st->logfreq[op] = dx7_osc_logfreq_base(0, coarse, fine, detune)
                                + dx7_ratio_detune(nlf, detune);
            }
        }
        st->ks_pitch = pitch;
    }
    const int32_t* cells = st->cells;

    /* Global pitch envelope (msfa PitchEnv) + the played note, both in the logfreq
       domain. Added per ratio operator below; fixed ops use their logfreq alone. */
    int32_t pitchmod = pitchenv_step(&st->peg, gate);
    int32_t note_lf  = midinote_to_logfreq(midi_clamp(pitch));

    uint8_t algo   = (uint8_t)(cells[0] & 31u);
    int32_t feedback = cells[1] / 585;          /* DX7 feedback 0..7 (cells[1] = 585*fb) */
    int32_t fb_shift = feedback ? (8 - feedback) : 16;   /* msfa FEEDBACK_BITDEPTH-feedback */
    const struct FmAlgo* A = &FM_ALGOS[algo];
    int32_t out[6] = {0,0,0,0,0,0};

    for (int k = 0; k < 6; k++) {
        int op = A->order[k];
        uint32_t ob = 2u + 9u * (uint32_t)op;
        int32_t R[4] = { cells[ob+1u], cells[ob+2u], cells[ob+3u], cells[ob+4u] };
        int32_t L[4] = { cells[ob+5u], cells[ob+6u], cells[ob+7u], cells[ob+8u] };

        /* :decay scales only the CARRIER operators' decay/release rates (R2..R4), so it
           sets how long the note rings without altering the modulator-driven timbre. A
           carrier's EG is the amplitude envelope (FM has no separate amp EG); modulators
           keep their rates so the timbre evolves as authored. 2048=unity, 0=slowest
           (infinite sustain), 4095~2x speed. Attack (R1) is left crisp. */
        if (decay != 2048 && (A->carriers & (uint8_t)(1u << op))) {
            for (int r = 1; r < 4; r++) {
                int32_t rs = (R[r] * decay) >> 11;
                R[r] = rs < 0 ? 0 : rs > 99 ? 99 : rs;
            }
        }

        /* Frequency via msfa freqlut. Ratio ops add the played note + pitch envelope
           (the kick's thud); fixed ops use their absolute logfreq alone. inc is the
           phase increment in 2^24/cycle units. */
        int32_t lf = (st->op_ratio & (1 << op)) ? (note_lf + st->logfreq[op] + pitchmod)
                                                : st->logfreq[op];
        uint32_t inc = (uint32_t)msfa_freqlut_lookup(lf);

        /* Modulation input at msfa scale: SUM the modulators' 24-bit gain-scaled
           outputs (a unity-gain modulator swings the carrier phase a full cycle).
           Summing, not averaging, is the faithful DX7 behaviour, and the 24-bit
           datapath is what stops FM coarseness from aliasing. */
        int32_t input = 0;
        for (int m = 0; m < 6; m++) {
            if (A->modmask[op] & (uint8_t)(1u << m)) input += out[m];
        }

        /* Envelope -> Q24 linear gain. dxeg returns a linear amplitude with unity
           == VMAX; <<12 rescales unity to ~2^24 (msfa's Exp2 gain domain). */
        int32_t env  = dxeg_step(&st->eg[op], gate, R, L, st->rate_scaling[op]);
        int32_t gain = env << 12;
        /* keyboard level scaling: gain *= ks_mult (Q16), 32-bit (gain >= 0). */
        { uint32_t g = (uint32_t)gain, k = (uint32_t)st->ks_mult[op];
          gain = (int32_t)((umulhi32(g, k) << 16) | ((g * k) >> 16)); }

        /* Self-feedback (msfa compute_fb): averaged-and-shifted sum of the op's two
           prior gain-scaled outputs, added into the 24-bit phase. y0/y1 are fb_buf. */
        int32_t scaled_fb = 0;
        if (op == (int)A->fbop && feedback)
            scaled_fb = (st->y0[op] + st->y1[op]) >> (fb_shift + 1);

        st->phase[op] += inc;                           /* phase accumulator, 2^24/cycle */
        int32_t sphase = (int32_t)st->phase[op] + input + scaled_fb;
        int32_t y = msfa_mul24(msfa_sin_lookup(sphase), gain);
        out[op] = y;
        if (op == (int)A->fbop) { st->y0[op] = st->y1[op]; st->y1[op] = y; }

        if (tone != 2048 && !(A->carriers & (uint8_t)(1u << op))) {
            /* :tone scales modulator depth (2048=unity); >>11 first keeps it 32-bit. */
            out[op] = (out[op] >> 11) * tone;
        }
    }

    int32_t acc = 0;
    for (int c = 0; c < 6; c++) {
        if (A->carriers & (uint8_t)(1u << c)) acc += out[c];
    }
    /* average carriers (matching op_mix) to hold loudness across algorithms */
    if (A->ncarriers > 1)
        acc = avg_round(acc, (int32_t)A->ncarriers);
    /* 24-bit FM accumulator -> bipolar 12-bit audio. >>15 leaves ~12 dB of headroom
       (a unity carrier sits well inside the linear region, clean), and the cubic
       soft-clip catches hot voices' peaks at the rail instead of hard-clipping.
       Round to nearest (not truncate -- truncation adds a DC bias and asymmetric
       distortion that folds up as grit). No dither: it would hiss through decay tails. */
    st->value = sat_cubic((acc + (1 << 14)) >> 15);
}

/* DX7 SysEx level-scaling LUT: indices 0..19 (port of Dexed/msfa scaleoutlevel).
   For x >= 20 the value is 28 + x. */
static const int32_t DX7_LEVELLUT[20] = {
    0,5,9,13,17,20,23,25,27,29,31,33,35,37,39,41,42,43,45,46
};
static inline int32_t dx7_scale_out(int32_t x) {
    if (x >= 20) return 28 + x;
    if (x < 0) x = 0;
    return DX7_LEVELLUT[x];
}

/* Parse one DX7 voice (128 raw bytes, packed OP6..OP1 order) into 56 op_dx cells.
   Mirrors dx7import.voiceCells exactly. Ratio->semitone via log2_fixed (Q16).
   Attribution: ratio/level maps ported from Dexed/msfa (env.cc, fm_core.cc). */
void dx7_parse_voice(const uint8_t* v128, int32_t cells[56]) {
    int32_t algorithm = (int32_t)((v128[110] & 31u) + 1u);
    int32_t feedback  = (int32_t)(v128[111] & 7u);

    cells[0] = (algorithm - 1) & 31;
    /* 4095 = 7*585, so 4095*feedback/7 is always exact. */
    cells[1] = (int32_t)((uint32_t)4095u * (uint32_t)feedback / 7u);

    for (int i = 0; i < 6; i++) {
        /* Packed order: OP6..OP1 at i*17; store as ops[0..5] = OP1..OP6. */
        int o      = i * 17;
        int opidx  = 5 - i;

        int32_t r0 = v128[o+0], r1 = v128[o+1], r2 = v128[o+2], r3 = v128[o+3];
        int32_t l0 = v128[o+4], l1 = v128[o+5], l2 = v128[o+6], l3 = v128[o+7];
        int32_t outLevel = (int32_t)v128[o+14];
        int32_t osc_mode = (int32_t)(v128[o+15] & 1u);   /* 0 = ratio, 1 = fixed Hz */
        int32_t coarse   = (int32_t)((v128[o+15] >> 1) & 31u);
        int32_t fine     = (int32_t)v128[o+16];

        /* Frequency cell. Ratio ops store (semitone offset + 64) and track the played
           note. Fixed-frequency ops (common in drums) store an absolute MIDI note biased
           by FM_FIXED_BIAS, so op_dx ignores the played note for them. */
        int32_t freq_cell;
        if (osc_mode) {
            /* Fixed-frequency operator. msfa (dx7note.cc osc_freq, fixed branch):
               logfreq = (4458616 * e) >> 3 == 557327 * e, e = (coarse&3)*100 + fine, in
               the (1<<24)=octave domain; midinote_to_logfreq(n) = 50857777 + n*1398101.
               Invert for the equivalent MIDI note (sub-semitone detune term dropped). */
            int32_t e  = (coarse & 3) * 100 + fine;
            int32_t lf = 557327 * e - 50857777;            /* fits int32 */
            /* note in 1/12-semitone units = round(lf * 12 / 1398101). lf*12 fits int32. */
            int32_t nf = lf >= 0 ? (lf * 12 + 699050) / 1398101
                                 : -((-lf * 12 + 699050) / 1398101);
            freq_cell = (nf + FM_FIXED_BIAS) & 0xFFF;
        } else {
            /* ratio = (coarse==0 ? 0.5 : coarse) * (1 + fine/100), in Q16. */
            int32_t base_q16  = (coarse == 0) ? 32768 : coarse * 65536;
            int32_t ratio_q16 = base_q16 * (100 + fine) / 100;
            int32_t semi = (12 * log2_fixed(ratio_q16) + 32768) >> 16;
            freq_cell = (semi + 64) & 0xFFF;
        }

        /* outlevel_ = min(127, scaleOut(outLevel)) << 5 */
        int32_t sc_out   = dx7_scale_out(outLevel);
        if (sc_out > 127) sc_out = 127;
        int32_t outlevel_ = sc_out << 5;

        int32_t base_cell = 2 + 9 * opidx;
        cells[base_cell + 0] = freq_cell;
        cells[base_cell + 1] = r0 & 0xFF;
        cells[base_cell + 2] = r1 & 0xFF;
        cells[base_cell + 3] = r2 & 0xFF;
        cells[base_cell + 4] = r3 & 0xFF;

        int32_t egLs[4] = {l0, l1, l2, l3};
        for (int j = 0; j < 4; j++) {
            int32_t act = ((dx7_scale_out(egLs[j]) >> 1) << 6) + outlevel_ - 4256;
            if (act < 16) act = 16;
            int32_t lv = act >> 4;
            if (lv < 0)   lv = 0;
            if (lv > 255) lv = 255;
            cells[base_cell + 5 + j] = lv;
        }
    }
}

void OP_FN(op_follow)(struct Slot* s) {
    struct FollowState* st = (struct FollowState*)s->out;
    /* Derived clock: in0 is the base clock's 12-bit ramp (0..4095, one turn per
     * base period), the ordinary value every form exchanges, so this reads through
     * an `if`/any routing with no private phase channel. The output is a PURE
     * FUNCTION of how many base turns have elapsed (mod div) and the base's
     * within-turn phase: no integrator to drift, no alignment offset to mis-fire.
     * `counter` tracks base turns mod div; out = (counter*4096 + base)*mult/div, a
     * 12-bit ramp that wraps mult times per div base-turns. A /N first wraps on the
     * Nth base turn (the honest division); a *M wraps M times per base turn. */
    uint32_t base = (uint32_t)(s->in0 ? slot_in0(s) : 0) & 0xFFFu;
    uint32_t mult = (s->param0 & 0xFFFFu) ? (s->param0 & 0xFFFFu) : 1u;
    uint32_t div  = (s->param0 >> 16) ? (s->param0 >> 16) : 1u;
    if (base < st->last_base) {                /* base turned over */
        st->counter++;
        if (st->counter >= div) st->counter = 0;
    }
    st->last_base = base;
    /* :drift (in1, 0 when unwired) is a slow per-sample phase creep so a derived
       clock slides against its base over many seconds. Accumulate in 32 bits and
       take the top 12, so a small per-sample drift moves the ramp gently. */
    st->acc += (uint32_t)(*(const int32_t*)s->in1);
    /* counter < div <= 4095, mult <= 4095; avoid (counter*4096+base)*mult overflow.
       Decompose: (counter*mult)/div gives quotient q and remainder r; then
       result = q*4096 + (r*4096 + base*mult)/div, all fits in uint32. */
    uint32_t cm  = st->counter * mult;   /* counter < div (wrapped above), so cm < div*mult <= 4095^2 */
    /* q/r change only on a base turn (or a live :mult/:div edit); the identity
       check recomputes exactly then. Per-sample cost: one divide, not two. */
    if (st->q * div + st->r != cm || st->r >= div) {
        st->q = cm / div;
        st->r = cm - st->q * div;
    }
    uint32_t out = (st->q * 4096u + (st->r * 4096u + base * mult) / div + (st->acc >> 20)) & 0xFFFu;
    st->value = (int32_t)out;
    /* follow outputs only its ramp; a consumer that wants the divided beat
       edge-detects it (rising through the low threshold), as for any clock. */
}

/* ===== tape kernels ===== */

void OP_FN(op_step)(struct Slot* s) {
    struct StepState*  st  = (struct StepState*)s->out;
    struct Buffer*     buf = (struct Buffer*)s->in0;
    int32_t clk = *(const int32_t*)s->in1;
    if (buf->length == 0) { st->last_clk = clk; st->value = st->cached; return; }
    /* :len caps the loop length: a literal lands in param0, a stream (e.g. a knob)
       in in2. in2 defaults to g_zero (0 = no cap) so plain `step` is unchanged. */
    int32_t dynlen = *(const int32_t*)s->in2;
    uint32_t len = s->param0 ? (uint32_t)s->param0
                 : (dynlen > 0) ? (uint32_t)dynlen
                 : (uint32_t)buf->length;
    if (len > (uint32_t)buf->length) len = (uint32_t)buf->length;   /* never read past the tape */
    if (!st->inited) {   /* sit on cell 0 at t=0; the first clock advances to cell 1 */
        uint32_t pos0 = 0;
        head_seed(&st->inited, &pos0, len);
        st->cached  = pack12_read(buf->bytes, 0);
        st->counter = (int32_t)pos0;
    }
    if (trig_fall(&st->last_clk, clk)) {
        uint32_t pos = (uint32_t)st->counter;
        st->cached  = pack12_read(buf->bytes, head_take(&pos, len));
        st->counter = (int32_t)pos;
    }
    st->value = st->cached;
}
void OP_FN(op_lookup)(struct Slot* s) {
    struct LookupState* st  = (struct LookupState*)s->out;
    struct Buffer*      buf = (struct Buffer*)s->in0;
    int32_t idx = *(const int32_t*)s->in1;
    if (buf->length == 0) { st->value = 0; return; }
    /* :len caps the read window: a literal lands in param0, a stream in in2. */
    int32_t dynlen = *(const int32_t*)s->in2;
    uint32_t len = s->param0 ? (uint32_t)s->param0
                 : (dynlen > 0) ? (uint32_t)dynlen
                 : (uint32_t)buf->length;
    if (len > (uint32_t)buf->length) len = (uint32_t)buf->length;   /* never read past the tape */
    uint32_t pos = (len > 0) ? ((uint32_t)idx % len) : 0;
    st->value = pack12_read(buf->bytes, pos);
}
void OP_FN(op_wave)(struct Slot* s) {
    struct WaveState* st  = (struct WaveState*)s->out;
    struct Buffer*          buf = (struct Buffer*)s->in0;
    int32_t pos = *(const int32_t*)s->in1;
    uint32_t len = (uint32_t)buf->length;
    if (len == 0) { st->value = 0; return; }
    uint32_t len1 = len - 1u;
    int32_t P = pos * (int32_t)len1;
    /* One divide by constant 4095; frac uses shift-add (exact: 65536/4095 = 16 + 1/256). */
    int32_t idx_r = P / (int32_t)VMAX;
    int32_t P_mod = P - idx_r * (int32_t)VMAX;
    if (P_mod < 0) { P_mod += (int32_t)VMAX; idx_r--; }
    int32_t frac_q16 = (P_mod << 4) + (P_mod >> 8);
    /* clamp+wrap by compare (no power-of-two requirement, so any buffer length). */
    if (idx_r < 0) idx_r = 0;
    if ((uint32_t)idx_r >= len) idx_r = (int32_t)len - 1;
    uint32_t p0 = (uint32_t)idx_r;
    uint32_t p1 = p0 + 1u; if (p1 >= len) p1 = 0u;
    int32_t a = pack12_read_signed(buf->bytes, p0);
    int32_t b = pack12_read_signed(buf->bytes, p1);
    int32_t v_q16 = a * 65536 + (b - a) * frac_q16;
    int32_t out;
    if (v_q16 >= 0) out = (v_q16 + 32768) >> 16;
    else            out = -(((-v_q16) + 32767) >> 16);
    st->value = out;
}
void OP_FN(op_tap)(struct Slot* s) {
    struct TapState* st  = (struct TapState*)s->out;
    struct Buffer*   buf = (struct Buffer*)s->in0;
    int32_t amount  = *(const int32_t*)s->in1;
    int32_t cur_head = *(const int32_t*)s->in2;
    uint32_t len = (uint32_t)buf->length;
    if (len == 0) { st->value = 0; return; }
    /* Offset in Q12 samples. :span maps amount across the whole tape with a
       fractional part, so a swept delay glides between cells instead of
       stepping; P + P/4095 == P*4096/4095 keeps it 32-bit. A raw amount is
       whole samples (fpart 0), bit-identical to the unsmoothed read. */
    int32_t offset_q12;
    if (s->param0 & 1u) {
        int32_t P = amount * (int32_t)(len - 1u);
        offset_q12 = P + P / (int32_t)VMAX;
    } else {
        offset_q12 = amount << 12;
    }
    if (offset_q12 < 0) offset_q12 = 0;
    int32_t max_q12 = (int32_t)(len - 1u) << 12;
    if (offset_q12 > max_q12) offset_q12 = max_q12;
    int32_t ipart = offset_q12 >> 12;
    int32_t fpart = offset_q12 & 0xFFF;
    /* Wrap by compare-and-subtract: cur_head is in [0,len) and ipart in
       [0,len-1], so one add of len covers a negative result. No power-of-two
       requirement, so the ring can be the full pool length. */
    int32_t readPos0 = cur_head - ipart;
    if (readPos0 < 0) readPos0 += (int32_t)len;
    int32_t readPos1 = readPos0 - 1;
    if (readPos1 < 0) readPos1 += (int32_t)len;
    int32_t s0 = pack12_read_signed(buf->bytes, (uint32_t)readPos0);
    int32_t s1 = pack12_read_signed(buf->bytes, (uint32_t)readPos1);
    st->value = s0 + (((s1 - s0) * fpart) >> 12);
}
/* pluck: integer Karplus-Strong. in0=trig (rising edge re-excites), in1=pitch
 * (MIDI note -> loop length), in2=damp (0..VMAX, decay/brightness), in3=delay
 * buffer. One multiply per sample, 32-bit only. */
void OP_FN(op_pluck)(struct Slot* s) {
    struct PluckState* st  = (struct PluckState*)s->out;
    int32_t        trig  = *(const int32_t*)s->in0;
    int32_t        pitch = *(const int32_t*)s->in1;
    int32_t        damp  = vclamp_(*(const int32_t*)s->in2);
    struct Buffer* buf   = (struct Buffer*)s->in3;
    uint32_t cap = (uint32_t)buf->length;

    /* Rising trig: set the loop length from the pitch and fill it with a fresh
     * full-scale noise burst. N = 2^32 / phase_inc = samplerate / freq; the
     * 32-bit divide runs only on note-on, never per sample. */
    if (trig_rise(&st->last_trig, trig)) {
        uint32_t inc = pitch_table[midi_clamp(pitch)];
        uint32_t n = inc ? (0xFFFFFFFFu / inc) : cap;
        if (n < 2u)   n = 2u;
        if (n > cap)  n = cap;
        uint32_t r = st->rng ? st->rng : 0x1234567u;
        for (uint32_t i = 0; i < n; i++) {
            r = lcg(r);
            int32_t noise = (int32_t)((r >> 20) & 0xFFFu) - 2048; /* -2048..2047 */
            noise -= noise >> 4;                                  /* ~15/16: keep off the rail */
            pack12_write(buf->bytes, i, noise);
        }
        st->rng  = r;
        st->n    = n;
        st->idx  = 0;
        st->last = 0;
    }

    uint32_t n = st->n;
    if (n == 0u) { st->value = 0; return; }   /* never plucked */

    uint32_t idx = st->idx;
    int32_t cur = pack12_read_signed(buf->bytes, idx);
    /* One-zero lowpass between adjacent loop samples: the high harmonics lose
     * energy first (the string dulls as it decays). damp blends the raw sample
     * (bright, long) toward the average (dull). */
    int32_t avg  = (cur + st->last) >> 1;
    int32_t filt = cur + (((avg - cur) * damp) >> 12);
    /* Loop loss sets the decay rate; more damp = lower gain = faster decay.
     * The floor keeps even damp==0 decaying (a pluck, not a drone). */
    int32_t gain = 4068 - (damp >> 3);        /* Q12: ~0.993 .. ~0.868 */
    int32_t fb   = (filt * gain) >> 12;
    pack12_write(buf->bytes, idx, fb);
    st->last = cur;
    idx++; if (idx >= n) idx = 0;
    st->idx = idx;
    st->value = cur;
}
void OP_FN(op_recordhead_per_sample)(struct Slot* s) {
    struct RecordheadPerSampleState* st  = (struct RecordheadPerSampleState*)s->out;
    int32_t      val = *(const int32_t*)s->in0;
    struct Buffer* buf = (struct Buffer*)s->in1;
    st->value = val;
    /* param0 bit0: gated by in2 (:when). Freeze (no write/advance) while low. */
    if ((s->param0 & 1u) && *(const int32_t*)s->in2 <= VMID) return;
    uint32_t len = (uint32_t)buf->length;
    uint32_t next = st->head_pos + 1u;
    if (next >= len) next = 0u;
    st->pending_pos           = st->head_pos;
    st->pending_val           = val;
    st->pending_head_pos_next = next;
    st->pending_valid         = 1;
}
void OP_FN(op_recordhead_per_cell)(struct Slot* s) {
    struct RecordheadPerCellState* st  = (struct RecordheadPerCellState*)s->out;
    int32_t      val = *(const int32_t*)s->in0;
    struct Buffer* buf = (struct Buffer*)s->in1;
    int32_t      clk = *(const int32_t*)s->in2;
    uint32_t len = (uint32_t)buf->length;
    /* sit on cell 0 at t=0 (first clock writes cell 1); a rising :reset (in4,
       0 unwired) returns the head to that same t=0 state. */
    head_seed(&st->inited, &st->head_pos, len);
    if (trig_rise(&st->last_reset, *(const int32_t*)s->in4)) st->head_pos = (len > 1u) ? 1u : 0u;
    if (trig_fall(&st->last_clk, clk)) {
        uint32_t next = st->head_pos + 1u;
        if (next >= len) next = 0u;
        st->pending_pos           = st->head_pos;
        st->pending_val           = val;
        st->pending_head_pos_next = next;
        st->pending_valid         = 1;
    }
    st->value = val;
}
void OP_FN(op_recordhead_gated)(struct Slot* s) {
    struct RecordheadGatedState* st  = (struct RecordheadGatedState*)s->out;
    int32_t      val  = *(const int32_t*)s->in0;
    struct Buffer* buf = (struct Buffer*)s->in1;
    int32_t      gate = *(const int32_t*)s->in2;
    int32_t      clk  = *(const int32_t*)s->in3;
    uint32_t len = (uint32_t)buf->length;
    /* sit on cell 0 at t=0 (first clock writes cell 1); a rising :reset (in4,
       0 unwired) returns the head to that same t=0 state. */
    head_seed(&st->inited, &st->head_pos, len);
    if (trig_rise(&st->last_reset, *(const int32_t*)s->in4)) st->head_pos = (len > 1u) ? 1u : 0u;
    if (trig_fall(&st->last_clk, clk) && gate > VMID) {
        uint32_t next = st->head_pos + 1u;
        if (next >= len) next = 0u;
        st->pending_pos           = st->head_pos;
        st->pending_val           = val;
        st->pending_head_pos_next = next;
        st->pending_valid         = 1;
    }
    st->value = val;
}
void OP_FN(op_recordhead_len_capped)(struct Slot* s) {
    struct RecordheadLenCappedState* st  = (struct RecordheadLenCappedState*)s->out;
    int32_t      val = *(const int32_t*)s->in0;
    struct Buffer* buf = (struct Buffer*)s->in1;
    int32_t      clk;
    uint32_t cap;
    if (s->param0) {
        cap = (uint32_t)s->param0;
        clk = *(const int32_t*)s->in2;
    } else {
        int32_t len_val = *(const int32_t*)s->in2;
        cap = (len_val > 0) ? (uint32_t)len_val : (uint32_t)buf->length;
        clk = *(const int32_t*)s->in3;
    }
    if (cap > (uint32_t)buf->length) cap = (uint32_t)buf->length;
    if (cap == 0) cap = 1;
    /* A shrunk dynamic cap can leave head_pos outside the loop window; snap it back. */
    if (st->head_pos >= cap) st->head_pos = 0;
    /* sit on cell 0 at t=0 (first clock writes cell 1); a rising :reset (in4,
       0 unwired) returns the head to that same t=0 state. */
    head_seed(&st->inited, &st->head_pos, cap);
    if (trig_rise(&st->last_reset, *(const int32_t*)s->in4)) st->head_pos = (cap > 1u) ? 1u : 0u;
    if (trig_fall(&st->last_clk, clk)) {
        st->pending_pos           = st->head_pos;
        st->pending_val           = val;
        st->pending_head_pos_next = (st->head_pos + 1u) % cap;
        st->pending_valid         = 1;
    }
    st->value = val;
}
void OP_FN(op_recordhead_len_capped_gated)(struct Slot* s) {
    struct RecordheadLenCappedGatedState* st  = (struct RecordheadLenCappedGatedState*)s->out;
    int32_t      val  = *(const int32_t*)s->in0;
    struct Buffer* buf = (struct Buffer*)s->in1;
    int32_t      gate = *(const int32_t*)s->in2;
    int32_t      clk  = *(const int32_t*)s->in3;
    uint32_t cap = s->param0 ? s->param0 : (uint32_t)buf->length;
    if (cap > (uint32_t)buf->length) cap = (uint32_t)buf->length;
    if (cap == 0) cap = 1;
    if (st->head_pos >= cap) st->head_pos = 0;
    /* sit on cell 0 at t=0 (first clock writes cell 1); a rising :reset (in4,
       0 unwired) returns the head to that same t=0 state. */
    head_seed(&st->inited, &st->head_pos, cap);
    if (trig_rise(&st->last_reset, *(const int32_t*)s->in4)) st->head_pos = (cap > 1u) ? 1u : 0u;
    if (trig_fall(&st->last_clk, clk) && gate > VMID) {
        st->pending_pos           = st->head_pos;
        st->pending_val           = val;
        st->pending_head_pos_next = (st->head_pos + 1u) % cap;
        st->pending_valid         = 1;
    }
    st->value = val;
}
/* Random-access tape write: on each clock edge, write in0 into the buffer cell
   addressed by in2 (the index). The head does not advance. Out-of-range indices
   wrap, matching `thru`/`seek` read semantics. param0 bit0: gated by in4 (the
   selected-tape guard and/or :when) -- write only when the gate is high, so a
   seek-write fans across a bank with exactly the selected tape recording. */
void OP_FN(op_recordhead_seek)(struct Slot* s) {
    struct RecordheadSeekState* st  = (struct RecordheadSeekState*)s->out;
    int32_t        val = *(const int32_t*)s->in0;
    struct Buffer* buf = (struct Buffer*)s->in1;
    int32_t        idx = *(const int32_t*)s->in2;
    int32_t        clk = *(const int32_t*)s->in3;
    int32_t        len = (int32_t)buf->length;
    int32_t        gate_ok = !(s->param0 & 1u) || (*(const int32_t*)s->in4 > VMID);
    if (trig_fall(&st->last_clk, clk) && len > 0 && gate_ok) {
        idx %= len; if (idx < 0) idx += len;
        st->pending_pos           = (uint32_t)idx;
        st->pending_val           = val;
        st->pending_head_pos_next = st->head_pos;   /* a seek write does not advance the head */
        st->pending_valid         = 1;
    }
    st->value = val;
}
void OP_FN(op_seek)(struct Slot* s) {
    struct SeekState* st  = (struct SeekState*)s->out;
    struct Buffer*    buf = (struct Buffer*)s->in0;
    int32_t idx = *(const int32_t*)s->in1;
    if (buf->length == 0) { st->value = 0; return; }
    /* :len caps the window; literal only on seek (param0), not a stream. */
    uint32_t len = s->param0 ? (uint32_t)s->param0 : (uint32_t)buf->length;
    if (len > (uint32_t)buf->length) len = (uint32_t)buf->length;
    uint32_t pos = (uint32_t)idx % len;
    st->value = pack12_read(buf->bytes, pos);
}
void OP_FN(op_onsets)(struct Slot* s) {
    struct OnsetsState* st  = (struct OnsetsState*)s->out;
    struct Buffer*      buf = (struct Buffer*)s->in0;
    int32_t clk = *(const int32_t*)s->in1;
    uint32_t len = (uint32_t)buf->length;
    uint32_t pos = (uint32_t)st->counter;
    if (len > 0 && head_seed(&st->inited, &pos, len)) {
        if (pack12_read(buf->bytes, 0) != 0) st->pulseLeft = kTickWidth;
        st->counter = (int32_t)pos;
    }
    if (trig_fall(&st->last_clk, clk) && len > 0) {
        int32_t cell = pack12_read(buf->bytes, head_take(&pos, len));
        if (cell != 0) st->pulseLeft = kTickWidth;
        st->counter = (int32_t)pos;
    }
    if (st->pulseLeft > 0) { st->pulseLeft--; st->value = VMAX; }
    else st->value = 0;
}
void OP_FN(op_gates)(struct Slot* s) {
    struct GatesState* st  = (struct GatesState*)s->out;
    struct Buffer*     buf = (struct Buffer*)s->in0;
    int32_t clk = *(const int32_t*)s->in1;
    uint32_t len = (uint32_t)buf->length;
    uint32_t pos = (uint32_t)st->counter;
    if (len > 0 && head_seed(&st->inited, &pos, len)) {
        st->gate = (pack12_read(buf->bytes, 0) != 0) ? VMAX : 0;
        st->counter = (int32_t)pos;
    }
    if (trig_fall(&st->last_clk, clk) && len > 0) {
        int32_t cell = pack12_read(buf->bytes, head_take(&pos, len));
        st->gate = (cell != 0) ? VMAX : 0;
        st->counter = (int32_t)pos;
    }
    st->value = st->gate;
}
void OP_FN(op_hits)(struct Slot* s) {
    struct HitsState* st  = (struct HitsState*)s->out;
    struct Buffer*    buf = (struct Buffer*)s->in0;
    int32_t clk = *(const int32_t*)s->in1;
    if (!st->inited) {   /* sit on step 0 at t=0; first clock advances to step 1 */
        st->inited = 1;
        if ((uint32_t)buf->length > 0) {
            st->cached = pack12_read(buf->bytes, 0);
            st->step_count = 1;
        }
    }
    if (trig_fall(&st->last_clk, clk)) {
        uint32_t len = (uint32_t)buf->length;
        if (len > 0) {
            uint32_t pos = (uint32_t)st->step_count % len;
            st->cached = pack12_read(buf->bytes, pos);
            st->step_count++;
        }
    }
    st->value = st->cached;
}
void OP_FN(op_degree)(struct Slot* s) {
    struct DegreeState* st  = (struct DegreeState*)s->out;
    int32_t val = *(const int32_t*)s->in0;
    struct Buffer* buf = (struct Buffer*)s->in1;
    uint32_t len = (uint32_t)buf->length;
    if (len == 0) {
        st->value = (val * 127) / (VMAX + 1);
        return;
    }
    int32_t idx    = (val * (int32_t)len) / (VMAX + 1);
    int32_t octave = idx / (int32_t)len;
    int32_t deg    = idx % (int32_t)len;
    if (deg < 0) { deg += (int32_t)len; octave--; }
    int32_t cell = pack12_read(buf->bytes, (uint32_t)deg);
    int32_t note = 48 + octave * 12 + cell;
    if (note > 127) note = 127;
    if (note < 0)   note = 0;
    st->value = note;
}
void OP_FN(op_pitch)(struct Slot* s) {
    struct PitchState* st  = (struct PitchState*)s->out;
    int32_t val = *(const int32_t*)s->in0;
    struct Buffer* buf = (struct Buffer*)s->in1;
    uint32_t len = (uint32_t)buf->length;
    int32_t note;
    if (len == 0) {
        note = (val * 127) / (VMAX + 1);
    } else {
        int32_t idx    = (val * (int32_t)len) / (VMAX + 1);
        int32_t octave = idx / (int32_t)len;
        int32_t deg    = idx % (int32_t)len;
        if (deg < 0) { deg += (int32_t)len; octave--; }
        int32_t cell = pack12_read(buf->bytes, (uint32_t)deg);
        note = 48 + octave * 12 + cell;
        if (note > 127) note = 127;
        if (note < 0)   note = 0;
    }
    st->value = js_round_div(note * VMAX, 127);   /* MIDI 127 -> VMAX */
}
void OP_FN(op_thru)(struct Slot* s) {
    struct ThruState* st  = (struct ThruState*)s->out;
    struct Buffer*    buf = (struct Buffer*)s->in0;
    uint32_t len = (uint32_t)buf->length;
    if (len == 0) { st->value = 0; return; }
    int32_t idx = *(const int32_t*)s->in1;
    if (idx < 0) idx = 0;
    if ((uint32_t)idx >= len) idx = (int32_t)(len - 1u);
    st->value = pack12_read(buf->bytes, (uint32_t)idx);
}
void OP_FN(op_wave_drumrack)(struct Slot* s) {
    struct WaveDrumrackState* st  = (struct WaveDrumrackState*)s->out;
    struct Buffer*            buf = (struct Buffer*)s->in0;
    uint32_t len = (uint32_t)buf->length;
    if (len == 0) { st->value = 0; return; }
    st->value = pack12_read(buf->bytes, st->phase);
    st->phase++;
    if (st->phase >= len) st->phase -= len;
}

/* ===== terminal write + stub ===== */

void OP_FN(op_terminal_write)(struct Slot* s) {
    *(int32_t*)s->out = *(const int32_t*)s->in0;
}

/* ===== KFN table (kid -> RAM-resident fn pointer) ===== */
/* The table itself may live in flash: it is read only at apply time, not
 * on the hot path.  The functions it points to are in RAM. */

/* op_* forward declarations not needed; all defined above in this file. */

/* KFN uses plain symbol names; the OP_FN decorator on the definitions
 * places those symbols in RAM already. */
static void (* const KFN[KID_COUNT])(struct Slot*) = {
    /* 0 */ op_add,
    /* 1 */ op_sub,
    /* 2 */ op_mul,
    /* 3 */ op_div,
    /* 4 */ op_mod,
    /* 5 */ op_spread,
    /* 6 */ op_gt,
    /* 7 */ op_gte,
    /* 8 */ op_lt,
    /* 9 */ op_lte,
    /* 10 */ op_eq,
    /* 11 */ op_ne,
    /* 12 */ op_if,
    /* 13 */ op_not,
    /* 14 */ op_max,
    /* 15 */ op_min,
    /* 16 */ op_abs,
    /* 17 */ op_rect,
    /* 18 */ op_and,
    /* 19 */ op_or,
    /* 20 */ op_xor,
    /* 21 */ op_v_oct,
    /* 22 */ op_knob,
    /* 23 */ op_cv_in,
    /* 24 */ op_audio_in,
    /* 25 */ op_pulse_in,
    /* 26 */ op_switch,
    /* 27 */ op_detent,
    /* 28 */ op_phasor,
    /* 29 */ op_sine,
    /* 30 */ op_triangle,
    /* 31 */ op_saw,
    /* 32 */ op_square,
    /* 33 */ 0, /* reserved */
    /* 34 */ op_edge,
    /* 35 */ op_fall,
    /* 36 */ op_diff,
    /* 37 */ op_toggle,
    /* 38 */ op_hold,
    /* 39 */ op_gate,
    /* 40 */ op_schmitt,
    /* 41 */ op_z1,
    /* 42 */ op_vca,
    /* 43 */ op_ring,
    /* 44 */ op_mix2,
    /* 45 */ op_lpf,
    /* 46 */ op_hpf,
    /* 47 */ 0, /* reserved */
    /* 48 */ op_average,
    /* 49 */ op_slew,
    /* 50 */ op_vcf,
    /* 51 */ op_noise,
    /* 52 */ op_random,
    /* 53 */ op_chance,
    /* 54 */ op_walk,
    /* 55 */ op_lpg,
    /* 56 */ op_envfollow,
    /* 57 */ op_wavefold,
    /* 58 */ op_crush,
    /* 59 */ op_mix,
    /* 60 */ op_window,
    /* 61 */ op_range,
    /* 62 */ op_connected,
    /* 63 */ op_cv,
    /* 64 */ op_snap,
    /* 65 */ op_quantise,
    /* 66 */ op_every,
    /* 67 */ op_euclid,
    /* 68 */ op_envelope,
    /* 69 */ op_follow,
    /* 70 */ op_kick,
    /* 71 */ op_snare,
    /* 72 */ op_hat,
    /* 73 */ op_step,
    /* 74 */ op_lookup,
    /* 75 */ op_wave,
    /* 76 */ op_tap,
    /* 77 */ op_recordhead_per_sample,
    /* 78 */ op_recordhead_per_cell,
    /* 79 */ op_recordhead_gated,
    /* 80 */ op_recordhead_len_capped,
    /* 81 */ op_recordhead_len_capped_gated,
    /* 82 */ op_seek,
    /* 83 */ op_onsets,
    /* 84 */ op_gates,
    /* 85 */ op_hits,
    /* 86 */ op_degree,
    /* 87 */ op_pitch,
    /* 88 */ op_thru,
    /* 89 */ op_saturate,
    /* 90 */ op_transpose,
    /* 91 */ op_invert,
    /* 92 */ op_shift,
    /* 93 */ op_mask,
    /* 94 */ op_bit,
    /* 95 */ 0, /* reserved */
    /* 96 */ op_add_sat,
    /* 97 */ op_len,
    /* 98 */ op_record,
    /* 99 */ op_turns,
    /* 100 */ op_counter,
    /* 101 */ op_sub_sat,
    /* 102 */ 0, /* reserved */
    /* 103 */ op_wave_drumrack,
    /* 104 */ op_morph,
    /* 105 */ op_terminal_write,
    /* 106 */ op_recordhead_seek,
    /* 107 */ op_midi,
    /* 108 */ op_midi_note_out,
    /* 109 */ op_midi_cc_out,
    /* 110 */ op_midi_clock_out,
    /* 111 */ op_adsr,
    /* 112 */ op_dxeg,
    /* 113 */ op_pluck,
    /* 114 */ op_svf,
    /* 115 */ op_shape,
    /* 116 */ op_exp2,
    /* 117 */ op_log2,
    /* 118 */ op_dx,
    /* 119 */ op_wavetable,
    /* 120 */ op_pickup,
    /* 121 */ op_midi_cc,
    /* 122 */ op_latest,
    /* 123 */ op_chorus,
    /* 124 */ op_flanger,
    /* 125 */ op_compressor,
    /* 126 */ op_reverb,
    /* 127 */ op_echo,
};
_Static_assert(sizeof(KFN) / sizeof(KFN[0]) == KID_COUNT,
               "KFN entry count must equal KID_COUNT");

/* Called once per slot at apply time to wire the fn pointer. */
void runtime_slot_wire_fn(struct Slot* s) {
    uint8_t kid = s->kernel_id;
    s->fn = (kid < KID_COUNT) ? KFN[kid] : NULL;
}

/* Per-kernel NodeState size = sizeof the struct the kernel casts to. The C struct
 * is the single source of truth for state layout. Kernels with no struct (pure
 * value ops) default to 4 (value). */
static uint16_t KSTATE_BYTES[KID_COUNT];
static void init_kstate_bytes() {
    KSTATE_BYTES[KID_OP_AUDIO_IN] = sizeof(struct LeafState);
    KSTATE_BYTES[KID_OP_AVERAGE] = sizeof(struct OnePoleState);
    KSTATE_BYTES[KID_OP_CHANCE] = sizeof(struct ChanceState);
    KSTATE_BYTES[KID_OP_COUNTER] = sizeof(struct CounterState);
    KSTATE_BYTES[KID_OP_CRUSH] = sizeof(struct CrushState);
    KSTATE_BYTES[KID_OP_CV_IN] = sizeof(struct LeafState);
    KSTATE_BYTES[KID_OP_DEGREE] = sizeof(struct DegreeState);
    KSTATE_BYTES[KID_OP_DETENT] = sizeof(struct LeafState);
    KSTATE_BYTES[KID_OP_DIFF] = sizeof(struct DiffState);
    KSTATE_BYTES[KID_OP_EDGE] = sizeof(struct EdgeState);
    KSTATE_BYTES[KID_OP_ENVELOPE] = sizeof(struct EnvelopeState);
    KSTATE_BYTES[KID_OP_ENVFOLLOW] = sizeof(struct EnvFollowState);
    KSTATE_BYTES[KID_OP_EUCLID] = sizeof(struct EuclidState);
    KSTATE_BYTES[KID_OP_EVERY] = sizeof(struct EveryState);
    KSTATE_BYTES[KID_OP_FALL] = sizeof(struct FallState);
    KSTATE_BYTES[KID_OP_FOLLOW] = sizeof(struct FollowState);
    KSTATE_BYTES[KID_OP_GATE] = sizeof(struct GateState);
    KSTATE_BYTES[KID_OP_GATES] = sizeof(struct GatesState);
    KSTATE_BYTES[KID_OP_HAT] = sizeof(struct HatState);
    KSTATE_BYTES[KID_OP_HITS] = sizeof(struct HitsState);
    KSTATE_BYTES[KID_OP_HOLD] = sizeof(struct HoldState);
    KSTATE_BYTES[KID_OP_PICKUP] = sizeof(struct PickupState);
    KSTATE_BYTES[KID_OP_MIDI_CC] = sizeof(struct LeafState);
    KSTATE_BYTES[KID_OP_LATEST] = sizeof(struct LatestState);
    KSTATE_BYTES[KID_OP_CHORUS] = sizeof(struct ChorusState);
    KSTATE_BYTES[KID_OP_FLANGER] = sizeof(struct ChorusState);
    KSTATE_BYTES[KID_OP_COMPRESSOR] = sizeof(struct CompressorState);
    KSTATE_BYTES[KID_OP_REVERB] = sizeof(struct ReverbState);
    KSTATE_BYTES[KID_OP_ECHO] = sizeof(struct EchoState);
    KSTATE_BYTES[KID_OP_WAVETABLE] = sizeof(WavetableState);
    KSTATE_BYTES[KID_OP_KICK] = sizeof(struct KickState);
    KSTATE_BYTES[KID_OP_KNOB] = sizeof(struct LeafState);
    KSTATE_BYTES[KID_OP_MIDI] = sizeof(struct LeafState);
    KSTATE_BYTES[KID_OP_LOOKUP] = sizeof(struct LookupState);
    KSTATE_BYTES[KID_OP_LPF] = sizeof(struct OnePoleState);
    KSTATE_BYTES[KID_OP_LPG] = sizeof(struct LpgState);
    KSTATE_BYTES[KID_OP_NOISE] = sizeof(struct NoiseState);
    KSTATE_BYTES[KID_OP_ONSETS] = sizeof(struct OnsetsState);
    KSTATE_BYTES[KID_OP_PHASOR] = sizeof(struct PhasorState);
    KSTATE_BYTES[KID_OP_PITCH] = sizeof(struct PitchState);
    KSTATE_BYTES[KID_OP_PULSE_IN] = sizeof(struct LeafState);
    KSTATE_BYTES[KID_OP_RANDOM] = sizeof(struct RandomState);
    KSTATE_BYTES[KID_OP_RECORDHEAD_GATED] = sizeof(struct RecordheadGatedState);
    KSTATE_BYTES[KID_OP_RECORDHEAD_LEN_CAPPED] = sizeof(struct RecordheadLenCappedState);
    KSTATE_BYTES[KID_OP_RECORDHEAD_LEN_CAPPED_GATED] = sizeof(struct RecordheadLenCappedGatedState);
    KSTATE_BYTES[KID_OP_RECORDHEAD_SEEK] = sizeof(struct RecordheadSeekState);
    KSTATE_BYTES[KID_OP_RECORDHEAD_PER_CELL] = sizeof(struct RecordheadPerCellState);
    KSTATE_BYTES[KID_OP_RECORDHEAD_PER_SAMPLE] = sizeof(struct RecordheadPerSampleState);
    KSTATE_BYTES[KID_OP_SAW] = sizeof(struct SawState);
    KSTATE_BYTES[KID_OP_SCHMITT] = sizeof(struct SchmittState);
    KSTATE_BYTES[KID_OP_SEEK] = sizeof(struct SeekState);
    KSTATE_BYTES[KID_OP_SINE] = sizeof(struct SineState);
    KSTATE_BYTES[KID_OP_SLEW] = sizeof(struct OnePoleState);
    KSTATE_BYTES[KID_OP_SNARE] = sizeof(struct SnareState);
    KSTATE_BYTES[KID_OP_SQUARE] = sizeof(struct SquareState);
    KSTATE_BYTES[KID_OP_STEP] = sizeof(struct StepState);
    KSTATE_BYTES[KID_OP_SWITCH] = sizeof(struct LeafState);
    KSTATE_BYTES[KID_OP_TAP] = sizeof(struct TapState);
    KSTATE_BYTES[KID_OP_THRU] = sizeof(struct ThruState);
    KSTATE_BYTES[KID_OP_TOGGLE] = sizeof(struct ToggleState);
    KSTATE_BYTES[KID_OP_TRIANGLE] = sizeof(struct TriangleState);
    KSTATE_BYTES[KID_OP_TURNS] = sizeof(struct TurnsState);
    KSTATE_BYTES[KID_OP_VCF] = sizeof(struct VcfState);
    KSTATE_BYTES[KID_OP_WALK] = sizeof(struct WalkState);
    KSTATE_BYTES[KID_OP_WAVE] = sizeof(struct WaveState);
    KSTATE_BYTES[KID_OP_WAVE_DRUMRACK] = sizeof(struct WaveDrumrackState);
    KSTATE_BYTES[KID_OP_WAVEFOLD] = sizeof(struct WavefoldState);
    KSTATE_BYTES[KID_OP_Z1] = sizeof(struct Z1State);
    KSTATE_BYTES[KID_OP_MIDI_NOTE_OUT] = sizeof(MidiNoteOutState);
    KSTATE_BYTES[KID_OP_MIDI_CC_OUT] = sizeof(MidiCcOutState);
    KSTATE_BYTES[KID_OP_MIDI_CLOCK_OUT] = sizeof(MidiClockOutState);
    KSTATE_BYTES[KID_OP_ADSR] = sizeof(struct AdsrState);
    KSTATE_BYTES[KID_OP_DXEG] = sizeof(struct DxEgState);
    KSTATE_BYTES[KID_OP_PLUCK] = sizeof(struct PluckState);
    KSTATE_BYTES[KID_OP_SVF] = sizeof(struct SvfState);
    KSTATE_BYTES[KID_OP_SHAPE] = sizeof(struct ShapeState);
    KSTATE_BYTES[KID_OP_DX] = sizeof(struct FmState);
};

uint32_t runtime_kernel_state_bytes(uint8_t kid) {
    init_kstate_bytes();
    uint16_t b = (kid < KID_COUNT) ? KSTATE_BYTES[kid] : 0;
    return b ? b : 4u;   /* stateless: value only */
}

/* ===== KTABLE (name -> KID) ===== */

typedef struct { const char* name; uint8_t kid; } KEntry;

static const KEntry KTABLE[] = {
    {"op_add",    KID_OP_ADD},
    {"op_sub",    KID_OP_SUB},
    {"op_add_sat",KID_OP_ADD_SAT},
    {"op_sub_sat",KID_OP_SUB_SAT},
    {"op_mul",    KID_OP_MUL},
    {"op_div",    KID_OP_DIV},
    {"op_mod",    KID_OP_MOD},
    {"op_spread", KID_OP_SPREAD},
    {"op_gt",     KID_OP_GT},
    {"op_gte",    KID_OP_GTE},
    {"op_lt",     KID_OP_LT},
    {"op_lte",    KID_OP_LTE},
    {"op_eq",     KID_OP_EQ},
    {"op_ne",     KID_OP_NE},
    {"op_if",     KID_OP_IF},
    {"op_not",    KID_OP_NOT},
    {"op_max",    KID_OP_MAX},
    {"op_min",    KID_OP_MIN},
    {"op_abs",    KID_OP_ABS},
    {"op_rect",   KID_OP_RECT},
    {"op_exp2",   KID_OP_EXP2},
    {"op_log2",   KID_OP_LOG2},
    {"op_and",    KID_OP_AND},
    {"op_or",     KID_OP_OR},
    {"op_xor",    KID_OP_XOR},
    {"op_v_oct",  KID_OP_V_OCT},
    {"op_knob",       KID_OP_KNOB},
    {"op_midi",           KID_OP_MIDI},
    {"op_midi_note_out",  KID_OP_MIDI_NOTE_OUT},
    {"op_midi_cc_out",    KID_OP_MIDI_CC_OUT},
    {"op_midi_clock_out", KID_OP_MIDI_CLOCK_OUT},
    {"op_cv_in",      KID_OP_CV_IN},
    {"op_audio_in",   KID_OP_AUDIO_IN},
    {"op_pulse_in",   KID_OP_PULSE_IN},
    {"op_switch", KID_OP_SWITCH},
    {"op_detent",     KID_OP_DETENT},
    {"op_phasor",   KID_OP_PHASOR},
    {"op_sine",     KID_OP_SINE},
    {"op_triangle", KID_OP_TRIANGLE},
    {"op_saw",      KID_OP_SAW},
    {"op_square",   KID_OP_SQUARE},
    {"op_trig",   KID_OP_FALL},
    {"op_edge",   KID_OP_EDGE},
    {"op_fall",   KID_OP_FALL},
    {"op_diff",   KID_OP_DIFF},
    {"op_toggle", KID_OP_TOGGLE},
    {"op_hold",   KID_OP_HOLD},
    {"op_pickup", KID_OP_PICKUP},
    {"op_midi_cc", KID_OP_MIDI_CC},
    {"op_latest", KID_OP_LATEST},
    {"op_chorus", KID_OP_CHORUS},
    {"op_flanger", KID_OP_FLANGER},
    {"op_compressor", KID_OP_COMPRESSOR},
    {"op_reverb", KID_OP_REVERB},
    {"op_echo", KID_OP_ECHO},
    {"op_wavetable", KID_OP_WAVETABLE},
    {"op_gate",   KID_OP_GATE},
    {"op_schmitt",KID_OP_SCHMITT},
    {"op_z1",     KID_OP_Z1},
    {"op_vca",    KID_OP_VCA},
    {"op_ring",   KID_OP_RING},
    {"op_mix2",   KID_OP_MIX2},
    {"op_lpf",    KID_OP_LPF},
    {"op_hpf",    KID_OP_HPF},
    {"op_average",KID_OP_AVERAGE},
    {"op_slew",   KID_OP_SLEW},
    {"op_vcf",    KID_OP_VCF},
    {"op_noise",  KID_OP_NOISE},
    {"op_random", KID_OP_RANDOM},
    {"op_chance", KID_OP_CHANCE},
    {"op_walk",   KID_OP_WALK},
    {"op_lpg",    KID_OP_LPG},
    {"op_envfollow",KID_OP_ENVFOLLOW},
    {"op_wavefold",KID_OP_WAVEFOLD},
    {"op_crush",   KID_OP_CRUSH},
    {"op_saturate",KID_OP_SATURATE},
    {"op_mix",    KID_OP_MIX},
    {"op_window", KID_OP_WINDOW},
    {"op_range",  KID_OP_RANGE},
    {"op_cv",     KID_OP_CV},
    {"op_snap",   KID_OP_SNAP},
    {"op_quantise",KID_OP_QUANTISE},
    {"op_every",  KID_OP_EVERY},
    {"op_euclid", KID_OP_EUCLID},
    {"op_envelope",KID_OP_ENVELOPE},
    {"op_adsr",   KID_OP_ADSR},
    {"op_dxeg",   KID_OP_DXEG},
    {"op_pluck",  KID_OP_PLUCK},
    {"op_svf",    KID_OP_SVF},
    {"op_shape",  KID_OP_SHAPE},
    {"op_follow", KID_OP_FOLLOW},
    {"op_kick",   KID_OP_KICK},
    {"op_snare",  KID_OP_SNARE},
    {"op_hat",    KID_OP_HAT},
    {"op_step",   KID_OP_STEP},
    {"op_lookup", KID_OP_LOOKUP},
    {"op_wave",KID_OP_WAVE},
    {"op_tap",    KID_OP_TAP},
    {"op_recordhead_per_sample",        KID_OP_RECORDHEAD_PER_SAMPLE},
    {"op_recordhead_per_cell",          KID_OP_RECORDHEAD_PER_CELL},
    {"op_recordhead_gated",             KID_OP_RECORDHEAD_GATED},
    {"op_recordhead_len_capped",        KID_OP_RECORDHEAD_LEN_CAPPED},
    {"op_recordhead_len_capped_gated",  KID_OP_RECORDHEAD_LEN_CAPPED_GATED},
    {"op_recordhead_seek",              KID_OP_RECORDHEAD_SEEK},
    {"op_seek",   KID_OP_SEEK},
    {"op_onsets", KID_OP_ONSETS},
    {"op_gates",  KID_OP_GATES},
    {"op_hits",   KID_OP_HITS},
    {"op_degree", KID_OP_DEGREE},
    {"op_pitch",  KID_OP_PITCH},
    {"op_thru",   KID_OP_THRU},
    {"op_transpose", KID_OP_TRANSPOSE},
    {"op_invert",    KID_OP_INVERT},
    {"op_shift",     KID_OP_SHIFT},
    {"op_mask",      KID_OP_MASK},
    {"op_bit",       KID_OP_BIT},
    {"op_len",       KID_OP_LEN},
    {"op_record",    KID_OP_RECORD},
    {"op_turns",     KID_OP_TURNS},
    {"op_counter",   KID_OP_COUNTER},
    {"op_connected", KID_OP_CONNECTED},
    {"op_wave_drumrack",    KID_OP_WAVE_DRUMRACK},
    {"op_morph",            KID_OP_MORPH},
    {"op_add2",  KID_OP_ADD},
    {"op_mul2",  KID_OP_MUL},
    {"op_or2",   KID_OP_OR},
    {"op_and2",  KID_OP_AND},
    {"op_terminal_write_audio_out_1", KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_audio_out_2", KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_cv_out_1",    KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_cv_out_2",    KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_pulse_out_1", KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_pulse_out_2", KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_led_0",       KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_led_1",       KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_led_2",       KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_led_3",       KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_led_4",       KID_OP_TERMINAL_WRITE},
    {"op_terminal_write_led_5",       KID_OP_TERMINAL_WRITE},
    {"op_dx",    KID_OP_DX},
    {NULL, KID_UNKNOWN}
};

uint8_t runtime_find_kernel(const char* name) {
    for (int i = 0; KTABLE[i].name; i++)
        if (strcmp(KTABLE[i].name, name) == 0)
            return KTABLE[i].kid;
    return KID_UNKNOWN;
}

int runtime_is_hw_leaf(uint8_t kid) {
    return (kid == KID_OP_KNOB || kid == KID_OP_CV_IN || kid == KID_OP_AUDIO_IN ||
            kid == KID_OP_PULSE_IN || kid == KID_OP_SWITCH);
}

int runtime_is_midi_leaf(uint8_t kid) {
    return kid == KID_OP_MIDI || kid == KID_OP_MIDI_CC;
}

/* ===== recordhead helpers ===== */

static void __not_in_flash_func(recordhead_commit_pending)(struct Slot* s) {
    struct RecordheadCommon* rc = (struct RecordheadCommon*)s->out;
    if (!rc->pending_valid) return;
    struct Buffer* buf = (struct Buffer*)s->in1;
    rc->pending_valid = 0;
    if (rc->pending_pos >= (uint32_t)buf->length) return;  /* empty/sentinel buffer or stale pos: no write */
    pack12_write(buf->bytes, rc->pending_pos, rc->pending_val);
    rc->head_pos     = rc->pending_head_pos_next;
    rc->head_pos_out = (int32_t)rc->pending_head_pos_next;
}

static inline int is_recordhead_kid(uint8_t kid) {
    return (uint8_t)(kid - KID_OP_RECORDHEAD_PER_SAMPLE)
             <= (KID_OP_RECORDHEAD_LEN_CAPPED_GATED - KID_OP_RECORDHEAD_PER_SAMPLE)
         || kid == KID_OP_RECORDHEAD_SEEK;
}

void recordhead_sweep(struct LensRuntime* rt) {
    if (!rt->has_recordhead) return;  /* common case: no delay/buffer writes */
    for (uint16_t i = 0; i < rt->slot_count; i++) {
        if (is_recordhead_kid(rt->slots[i].kernel_id))
            recordhead_commit_pending(&rt->slots[i]);
    }
}

/* Per-core sweeps: each core commits only its own recordheads, so the two cores
 * never race on a tape write and Core 0 need not wait for Core 1. */
void __not_in_flash_func(recordhead_sweep_core0)(struct LensRuntime* rt) {
    if (!rt->has_recordhead) return;
    for (uint16_t i = 0; i < rt->core0_count; i++)
        if (is_recordhead_kid(rt->core0_slots[i]->kernel_id))
            recordhead_commit_pending(rt->core0_slots[i]);
}
void __not_in_flash_func(recordhead_sweep_core1)(struct LensRuntime* rt) {
    if (!rt->has_recordhead) return;
    for (uint16_t i = 0; i < rt->core1_count; i++)
        if (is_recordhead_kid(rt->core1_slots[i]->kernel_id))
            recordhead_commit_pending(rt->core1_slots[i]);
}

/* ===== hw scratch ===== */

static int32_t hw_scratch[10];

void __not_in_flash_func(runtime_update_hw_scratch)(const struct HardwareInputs* hw) {
    hw_connected = hw->connected;
    hw_scratch[0] = hw->audio_in_1;
    hw_scratch[1] = hw->audio_in_2;
    hw_scratch[2] = hw->pulse_in_1;
    hw_scratch[3] = hw->pulse_in_2;
    hw_scratch[4] = hw->cv_in_1;
    hw_scratch[5] = hw->cv_in_2;
    hw_scratch[6] = hw->knob_main;
    hw_scratch[7] = hw->knob_x;
    hw_scratch[8] = hw->knob_y;
    hw_scratch[9] = hw->switch_pos;
}

int32_t* runtime_hw_jack_ptr(uint32_t idx) {
    return &hw_scratch[idx < 10 ? idx : 9];
}

/* ===== step functions ===== */

/* Run one slot. Every slot runs every sample (the synchronous model): pure ops
 * recompute from their inputs, stateful ops self-gate on their own clock/edge, so
 * there is nothing to skip. (The old skip-on-unchanged check cost ~half the per-slot
 * floor and almost never fired on the audio path, where inputs change every sample.) */
static inline void step_slot(struct Slot* s) {
    if (s->fn) s->fn(s);
}

void runtime_step(struct LensRuntime* rt,
                  const struct HardwareInputs* hw,
                  struct HardwareOutputs* hw_out) {
    runtime_update_hw_scratch(hw);
    for (uint16_t i = 0; i < rt->slot_count; i++)
        step_slot(&rt->slots[i]);
    recordhead_sweep(rt);
    /* Single-threaded walk of any snapshot, including a dual one: cross-core
       inputs read the shadow pool (last sample's value), so publish after the
       walk to feed next sample. No-op when xcore_count is 0 (single-core). */
    runtime_publish_shadows(rt);
    runtime_drive_terminals(rt, hw_out);
    rt->sample_counter++;
}

/* Oracle: every slot once per sample in walk order, in place. The optimized
 * runtime is diffed against this. */
void runtime_step_reference(struct LensRuntime* rt,
                            const struct HardwareInputs* hw,
                            struct HardwareOutputs* hw_out) {
    runtime_update_hw_scratch(hw);
    for (uint16_t i = 0; i < rt->slot_count; i++) {
        struct Slot* s = &rt->slots[i];
        if (s->fn) s->fn(s);
    }
    recordhead_sweep(rt);
    runtime_publish_shadows(rt);
    runtime_drive_terminals(rt, hw_out);
    rt->sample_counter++;
}

void __not_in_flash_func(runtime_drive_terminals)(struct LensRuntime* rt,
                                                   struct HardwareOutputs* hw_out) {
    hw_out->cv_out_1_is_pitch = 0;
    hw_out->cv_out_2_is_pitch = 0;
    for (uint8_t i = 0; i < rt->terminal_count; i++) {
        uint16_t wi = rt->terminals[i].slot_walk_idx;
        if (wi >= rt->slot_count) continue;
        int32_t* p   = (int32_t*)rt->slots[wi].out;
        int32_t  val = *p;
        /* v/oct pitch jack (mode 1): the value is a MIDI note. Clamp to 0..127
           (saturate at the rails, like a real CV) so any out-of-range tape cell or
           transposed note is safe; main.cpp then outputs calibrated 1V/oct. */
        int      is_pitch = (rt->terminals[i].mode == 1);
        int32_t  pitch_val = is_pitch ? (int32_t)midi_clamp(val) : val;
        switch (rt->terminals[i].jack_id) {
            case LENS_JACK_AUDIO_OUT_1: hw_out->audio_out_1 = val; break;
            case LENS_JACK_AUDIO_OUT_2: hw_out->audio_out_2 = val; break;
            case LENS_JACK_CV_OUT_1:    hw_out->cv_out_1 = pitch_val; hw_out->cv_out_1_is_pitch = (uint8_t)is_pitch; break;
            case LENS_JACK_CV_OUT_2:    hw_out->cv_out_2 = pitch_val; hw_out->cv_out_2_is_pitch = (uint8_t)is_pitch; break;
            case LENS_JACK_PULSE_OUT_1: hw_out->pulse_out_1 = val; break;
            case LENS_JACK_PULSE_OUT_2: hw_out->pulse_out_2 = val; break;
            case LENS_JACK_LED_0:       hw_out->led_0       = val; break;
            case LENS_JACK_LED_1:       hw_out->led_1       = val; break;
            case LENS_JACK_LED_2:       hw_out->led_2       = val; break;
            case LENS_JACK_LED_3:       hw_out->led_3       = val; break;
            case LENS_JACK_LED_4:       hw_out->led_4       = val; break;
            case LENS_JACK_LED_5:       hw_out->led_5       = val; break;
            default: break;
        }
    }
}

void __not_in_flash_func(runtime_publish_shadows)(struct LensRuntime* rt) {
    for (uint16_t i = 0; i < rt->xcore_count; i++)
        lens_shadow_pool[i] = *rt->xcore_src[i];
}

/* Per-core publish: each core publishes only the shadows IT produces, after its own
 * walk, so the value is stable when copied (no torn cross-core read, no waiting). */
void __not_in_flash_func(runtime_publish_shadows_core0)(struct LensRuntime* rt) {
    for (uint16_t i = 0; i < rt->xcore_count; i++)
        if (rt->xcore_core[i] == 0) lens_shadow_pool[i] = *rt->xcore_src[i];
}
void __not_in_flash_func(runtime_publish_shadows_core1)(struct LensRuntime* rt) {
    for (uint16_t i = 0; i < rt->xcore_count; i++)
        if (rt->xcore_core[i] == 1) lens_shadow_pool[i] = *rt->xcore_src[i];
}

void runtime_destroy(struct LensRuntime* rt) { (void)rt; }

void __not_in_flash_func(runtime_walk_core0)(struct LensRuntime* rt, uint32_t seq) {
    (void)seq;
    struct Slot** const end = rt->core0_slots + rt->core0_count;
    for (struct Slot** p = rt->core0_slots; p < end; p++) step_slot(*p);
}

void __not_in_flash_func(runtime_walk_core1)(struct LensRuntime* rt, uint32_t seq) {
    (void)seq;
    struct Slot** const end = rt->core1_slots + rt->core1_count;
    for (struct Slot** p = rt->core1_slots; p < end; p++) step_slot(*p);
}


// ──────────────────────────────────────────────────────────────────────────────
// Source: runtime/midi.c
// ──────────────────────────────────────────────────────────────────────────────

/* midi.c -- MIDI channel-voice parser; single writer of midi_scratch[].
 * No USB/TinyUSB dependencies; call midi_feed_byte once per incoming byte.
 * int32_t writes are atomic on M0+; no latch needed (one-sample skew is fine). */

#include "midi.h"
/* stripped system include */

/* Cross-core ordering for the MIDI-out ring: Core 0 fills a slot then advances
   head; Core 1 reads head then the slot. M0+ needs a real DMB between, not just a
   compiler barrier. The host sim is single-threaded, so a compiler barrier suffices. */
#if defined(__arm__) || defined(__thumb__)
  #define MIDI_BARRIER() asm volatile("" ::: "memory");
#else
  #define MIDI_BARRIER() __asm__ volatile ("" ::: "memory")
#endif

static int32_t midi_scratch[MIDI_SCRATCH_SIZE];

/* ---- Note stack (last-note priority, per channel) ---- */
#define NOTE_STACK_DEPTH 8

typedef struct {
    uint8_t notes[NOTE_STACK_DEPTH];
    uint8_t count;
} NoteStack;

static NoteStack note_stacks[16];  /* index 0 = MIDI channel 1 */

/* Per-note hold count across all channels (drives HELD region). */
static uint8_t held_count[128];

/* ---- Parser state ---- */
static uint8_t running_status;
static uint8_t in_sysex;
static uint8_t data_buf[2];
static uint8_t data_count;
static uint8_t expected_data;

/* ---- Helpers ---- */

static int status_data_len(uint8_t status) {
    uint8_t type = status & 0xF0;
    if (type == 0x80) return 2;  /* note-off */
    if (type == 0x90) return 2;  /* note-on */
    if (type == 0xB0) return 2;  /* control change */
    if (type == 0xD0) return 1;  /* channel pressure (aftertouch) */
    if (type == 0xE0) return 2;  /* pitch bend */
    return 0;
}

/* Transport + clock state (MIDI system realtime, no channel). */
static uint8_t clock_phase;   /* 0..MIDI_CLOCK_PPQ-1, advances on 0xF8 */
static uint8_t transport_on;  /* 1 between Start/Continue and Stop */

/* Update NOTE/GATE for a channel (and the omni slot). NOTE HOLDS the last note
   (so a pitch CV stays put between notes, the way a MIDI->CV converter behaves);
   GATE tracks key-down. NOTE only changes while a key is held; on release it is
   left untouched. */
static void update_note_gate(uint8_t ch) {
    if (note_stacks[ch].count > 0)
        midi_scratch[NOTE_BASE + ch + 1] = note_stacks[ch].notes[note_stacks[ch].count - 1];
    midi_scratch[GATE_BASE + ch + 1] = (note_stacks[ch].count > 0) ? 4095 : 0;

    int any_down = 0;
    for (int c = 0; c < 16; c++) {
        if (note_stacks[c].count > 0) {
            any_down = 1;
            midi_scratch[NOTE_BASE] = note_stacks[c].notes[note_stacks[c].count - 1];
        }
    }
    midi_scratch[GATE_BASE] = any_down ? 4095 : 0;
}

static void do_note_off(uint8_t ch, uint8_t note) {
    NoteStack* st = &note_stacks[ch];
    for (int i = 0; i < (int)st->count; i++) {
        if (st->notes[i] == note) {
            for (int j = i; j < (int)st->count - 1; j++)
                st->notes[j] = st->notes[j + 1];
            st->count--;
            break;
        }
    }
    if (held_count[note] > 0) {
        held_count[note]--;
        if (held_count[note] == 0)
            midi_scratch[HELD_BASE + note] = 0;
    }
    update_note_gate(ch);
}

static void do_note_on(uint8_t ch, uint8_t note, uint8_t vel) {
    if (vel == 0) { do_note_off(ch, note); return; }

    NoteStack* st = &note_stacks[ch];

    /* Remove if already present (re-push to top for last-note priority). */
    for (int i = 0; i < (int)st->count; i++) {
        if (st->notes[i] == note) {
            for (int j = i; j < (int)st->count - 1; j++)
                st->notes[j] = st->notes[j + 1];
            st->count--;
            break;
        }
    }
    /* Drop oldest on overflow. */
    if (st->count == NOTE_STACK_DEPTH) {
        for (int i = 0; i < NOTE_STACK_DEPTH - 1; i++)
            st->notes[i] = st->notes[i + 1];
        st->count = NOTE_STACK_DEPTH - 1;
    }
    st->notes[st->count++] = note;

    if (held_count[note] < 255) held_count[note]++;
    midi_scratch[HELD_BASE + note] = 4095;

    /* Velocity latches per channel + omni (0..127 -> 0..4095). */
    int32_t vel12 = ((int32_t)vel * 4095) / 127;
    midi_scratch[VEL_BASE + ch + 1] = vel12;
    midi_scratch[VEL_BASE]          = vel12;

    update_note_gate(ch);
}

static void do_control_change(uint8_t ccnum, uint8_t val) {
    /* 0..127 -> 0..4095; 32-bit integer only. */
    midi_scratch[CC_BASE + ccnum] = ((int32_t)val * 4095) / 127;
}

static void do_pitch_bend(uint8_t ch, uint8_t lsb, uint8_t msb) {
    /* 14-bit (centre 8192) -> 12-bit (centre 2048): raw >> 2. */
    int32_t raw14 = ((int32_t)msb << 7) | (int32_t)lsb;
    int32_t bend12 = raw14 >> 2;
    midi_scratch[BEND_BASE + ch + 1] = bend12;
    midi_scratch[BEND_BASE]          = bend12;
}

static void do_channel_pressure(uint8_t ch, uint8_t val) {
    /* Aftertouch 0..127 -> 0..4095 (per channel + omni). */
    int32_t p12 = ((int32_t)val * 4095) / 127;
    midi_scratch[PRESS_BASE + ch + 1] = p12;
    midi_scratch[PRESS_BASE]          = p12;
}

/* MIDI clock: publish a 12-bit beat phasor that wraps once per quarter note.
   Falling edge (wrap) = the downbeat, so (trig (midi-clock)) ticks per beat. */
static void publish_clock(void) {
    midi_scratch[CLOCK_BASE] = ((int32_t)clock_phase * 4096) / MIDI_CLOCK_PPQ;
}
static void do_clock_tick(void) {
    if (++clock_phase >= MIDI_CLOCK_PPQ) clock_phase = 0;
    publish_clock();
}
static void do_transport(uint8_t on, uint8_t reset) {
    transport_on = on;
    if (reset) { clock_phase = 0; publish_clock(); }  /* Start = downbeat */
    midi_scratch[PLAY_BASE] = on ? 4095 : 0;
}

static void dispatch(uint8_t status, uint8_t d0, uint8_t d1) {
    uint8_t type = status & 0xF0;
    uint8_t ch   = status & 0x0F;
    if (type == 0x90) { do_note_on(ch, d0, d1);       return; }
    if (type == 0x80) { do_note_off(ch, d0);           return; }
    if (type == 0xB0) { do_control_change(d0, d1);    return; }
    if (type == 0xD0) { do_channel_pressure(ch, d0); return; }
    if (type == 0xE0) { do_pitch_bend(ch, d0, d1);    return; }
}

/* ---- Public API ---- */

void midi_reset(void) {
    memset(midi_scratch, 0, sizeof(midi_scratch));
    memset(note_stacks,  0, sizeof(note_stacks));
    memset(held_count,   0, sizeof(held_count));
    /* Pitch bend rests at centre, not zero. */
    for (int i = 0; i <= 16; i++) midi_scratch[BEND_BASE + i] = MIDI_BEND_CENTRE;
    /* CC words rest at -1 ("no message yet") so a midi-cc :init can hold
       until the first CC lands. Readers map the sentinel, never expose it. */
    for (int i = 0; i < 128; i++) midi_scratch[CC_BASE + i] = -1;
    clock_phase    = 0;
    transport_on   = 0;
    running_status = 0;
    in_sysex       = 0;
    data_count     = 0;
    expected_data  = 0;
}

void midi_feed_byte(uint8_t b) {
    /* System realtime (0xF8..0xFF): may interleave mid-message, so handle and
       return WITHOUT touching running status. Clock + transport are consumed
       here; the rest are ignored. */
    if (b >= 0xF8) {
        if      (b == 0xF8) do_clock_tick();           /* timing clock */
        else if (b == 0xFA) do_transport(1, 1);        /* start: play + downbeat */
        else if (b == 0xFB) do_transport(1, 0);        /* continue: play, keep phase */
        else if (b == 0xFC) do_transport(0, 0);        /* stop */
        return;
    }

    if (b == 0xF0) {
        in_sysex = 1; running_status = 0; data_count = 0; expected_data = 0;
        return;
    }
    if (b == 0xF7) { in_sysex = 0; return; }
    if (in_sysex)  return;

    /* Other system common (0xF1..0xF6): clear running status, ignore. */
    if (b >= 0xF0) {
        running_status = 0; data_count = 0; expected_data = 0;
        return;
    }

    /* Channel status byte. */
    if (b & 0x80) {
        running_status = b;
        data_count     = 0;
        expected_data  = (uint8_t)status_data_len(b);
        return;
    }

    /* Data byte: apply running status if no current status. */
    if (expected_data == 0) {
        if (running_status == 0) return;
        expected_data = (uint8_t)status_data_len(running_status);
        if (expected_data == 0) return;
        data_count = 0;
    }

    if (data_count < 2) data_buf[data_count++] = b;

    if (data_count == expected_data) {
        dispatch(running_status, data_buf[0], data_buf[1]);
        data_count = 0;  /* next data byte reuses running status */
    }
}

int32_t* runtime_midi_jack_ptr(uint32_t idx) {
    return &midi_scratch[idx < MIDI_SCRATCH_SIZE ? idx : MIDI_SCRATCH_SIZE - 1];
}

/* ---- MIDI output TX ring (SPSC lock-free) ---- */
/* Core 0 is sole producer; Core 1 is sole consumer.
 * Each slot holds up to 4 bytes; capacity 256 slots.
 * Overflow drops the newest message and bumps a counter. */

#define MIDI_OUT_RING_CAP 256u

typedef struct {
    uint8_t data[4];
    uint8_t len;
} MidiOutSlot;

static MidiOutSlot   midi_out_ring[MIDI_OUT_RING_CAP];
static volatile uint32_t midi_out_head = 0;  /* producer (Core 0) */
static volatile uint32_t midi_out_tail = 0;  /* consumer (Core 1) */
static uint32_t      midi_out_dropped  = 0;

void midi_out_push(const uint8_t* bytes, uint8_t len) {
    uint32_t h = midi_out_head;
    uint32_t t = midi_out_tail;
    if (h - t >= MIDI_OUT_RING_CAP) { midi_out_dropped++; return; }
    MidiOutSlot* slot = &midi_out_ring[h & (MIDI_OUT_RING_CAP - 1)];
    if (len > 4) len = 4;
    for (uint8_t i = 0; i < len; i++) slot->data[i] = bytes[i];
    slot->len = len;
    MIDI_BARRIER();   /* slot data visible before head advances */
    midi_out_head = h + 1;
}

uint8_t midi_out_pop(uint8_t* out) {
    uint32_t t = midi_out_tail;
    uint32_t h = midi_out_head;
    if (t == h) return 0;
    MidiOutSlot* slot = &midi_out_ring[t & (MIDI_OUT_RING_CAP - 1)];
    uint8_t len = slot->len;
    for (uint8_t i = 0; i < len; i++) out[i] = slot->data[i];
    MIDI_BARRIER();   /* slot read complete before tail advances */
    midi_out_tail = t + 1;
    return len;
}


// ──────────────────────────────────────────────────────────────────────────────
// Source: runtime/snapshot_apply.c
// ──────────────────────────────────────────────────────────────────────────────

#include "runtime.h"
#include "kernel_ids.h"
#include "midi.h"
#include "snapshot_format.h"
/* stripped system include */
/* stripped system include */

/* pack12: 12-bit cells packed 2-per-3-bytes. */
__attribute__((always_inline))
static inline void pack12_write_snapshot(uint8_t* buf, uint32_t idx, int32_t val) {
    uint32_t v    = (uint32_t)val & 0xFFFu;
    uint32_t pair = idx >> 1;
    uint32_t base = (pair << 1) + pair;
    if ((idx & 1u) == 0u) {
        buf[base]      = (uint8_t)(v & 0xFFu);
        buf[base + 1u] = (uint8_t)((buf[base + 1u] & 0xF0u) | (v >> 8));
    } else {
        buf[base + 1u] = (uint8_t)((buf[base + 1u] & 0x0Fu) | ((v & 0xFu) << 4));
        buf[base + 2u] = (uint8_t)(v >> 4);
    }
}

/* ---- Tape geometry preservation ---- */
/* Stores the audio buffer lengths from the last successful apply.
   If incoming patch has the same layout, the audio pool is not zeroed,
   letting dub-delay tails ring through a patch swap. */
static uint32_t g_last_audio_lens[LENS_MAX_BUFFERS];
static uint8_t  g_last_audio_count = 0;

/* Same idea for the control pool (tapes + lenses), tracked as the ordered
   (kind, length) sequence. A control buffer's pool offset is fixed by every
   control buffer before it, so bytes are preserved only up to the longest
   unchanged prefix; past the first change, buffers are zero-filled. This also
   stops equal-length tape and lens buffers swapping roles through a swap. */
static uint32_t g_last_ctrl_lens[LENS_MAX_BUFFERS];
static uint8_t  g_last_ctrl_kinds[LENS_MAX_BUFFERS];
static uint8_t  g_last_ctrl_count = 0;

/* ---- Node-state layout preservation ---- */
/* Signature of the last applied node-state layout: slot_count plus the per-slot
   kernel-id sequence. Node-state size is a pure function of (slot_count, kernel
   ids in walk order) -- each slot's state bytes come from its kernel and slots
   allocate in walk order -- so a matching signature means an identical pool
   layout, and the live op state (oscillator phases, envelopes, hold registers,
   pickup takeover values) can survive a re-send instead of being zeroed. */
static uint32_t g_last_nodestate_sig  = 0;
static uint8_t  g_have_last_nodestate = 0;

/* ---- Static pools ---- */
/* Sizes come from runtime.h. Audio pool is 128 KB: 12-bit cells pack 2 per
   3 bytes, so 128 KB / 1.5 = 87381 cells = 1.82 s @ 48 kHz. */
uint8_t  lens_audio_pool[LENS_AUDIO_BUFFER_BYTES] __attribute__((aligned(4)));
uint8_t  lens_control_pool[LENS_CONTROL_BUFFER_BYTES];
uint8_t  lens_nodestate_pool[LENS_NODESTATE_BYTES];
struct Slot   lens_slot_pool[LENS_MAX_SLOTS];
struct Buffer lens_buffer_pool[LENS_MAX_BUFFERS];
struct RuntimeTerminal lens_terminal_pool[LENS_MAX_TERMINALS];
int32_t  lens_const_pool[LENS_CONST_POOL_WORDS];
int32_t  lens_shadow_pool[LENS_MAX_SLOTS];

/* Intern a constant: reuse an existing pool word with the same value, else add one.
   So N uses of the same literal cost one word, not N. Returns the pool index, or
   -1 if the (distinct-value) pool is full. */
static int lens_intern_const(struct LensRuntime* rt, int32_t v) {
    for (uint16_t k = 0; k < rt->const_count; k++)
        if (rt->const_pool[k] == v) return (int)k;
    if (rt->const_count >= LENS_CONST_POOL_WORDS) return -1;
    rt->const_pool[rt->const_count] = v;
    return (int)rt->const_count++;
}

/* Flat per-core walk-order lists: SR then CR slots for each core. */
struct Slot* lens_core0_flat_ptrs[LENS_MAX_SLOTS];
struct Slot* lens_core1_flat_ptrs[LENS_MAX_SLOTS];

static struct LensRuntime g_runtime;

/* Shared empty-buffer sentinel. A tape op whose buffer input is driven by a
   value (e.g. a dynamically selected tape via thru/lens) is wired here instead
   of at a reinterpreted int32, so it reads length 0 and no-ops rather than
   dereferencing garbage. Stateless demux that picks nothing reads as silence. */
static struct Buffer g_empty_buffer = { (uint8_t*)0, 0 };

/* The input index a kernel reads as a Buffer*, or -1. Used to redirect a
   non-buffer ref at that position to the empty-buffer sentinel. Recordheads
   (write side) are excluded: their tape target is always a real buffer. */
static int buffer_input_index(uint8_t kid) {
    switch (kid) {
        case KID_OP_STEP:   case KID_OP_LOOKUP: case KID_OP_WAVE:
        case KID_OP_TAP:    case KID_OP_SEEK:   case KID_OP_ONSETS:
        case KID_OP_GATES:  case KID_OP_HITS:   case KID_OP_THRU:
        case KID_OP_WAVE_DRUMRACK:
            return 0;
        case KID_OP_DEGREE: case KID_OP_PITCH:
            return 1;
        default:
            return -1;
    }
}

/* ---- Bump allocators ---- */
static size_t   audio_bump;
static size_t   control_bump;
static size_t   nodestate_bump;
static uint16_t slot_bump;
static uint8_t  buffer_bump;
static uint8_t  terminal_bump;
static uint16_t const_bump;

/* Live used bytes of each pool, for the settings-save overlay. The pool layout is a
   deterministic function of the snapshot, so the same snapshot re-bumps to the same
   sizes and per-slot offsets; saving these bytes and copying them back after an apply
   restores live state without interpreting any per-kernel struct. */
size_t lens_nodestate_used(void) { return nodestate_bump; }
size_t lens_control_used(void)   { return control_bump; }

static void* alloc_audio(size_t n) {
    /* Word-align every allocation: a kernel may address its ring as int16
       (reverb) and Cortex-M0+ faults on unaligned halfword access. The JS
       encoder's pool accounting rounds identically. */
    n = (n + 3u) & ~3u;
    if (audio_bump + n > LENS_AUDIO_BUFFER_BYTES) return NULL;
    void* p = &lens_audio_pool[audio_bump];
    audio_bump += n;
    return p;
}
static void* alloc_control(size_t n) {
    if (control_bump + n > LENS_CONTROL_BUFFER_BYTES) return NULL;
    void* p = &lens_control_pool[control_bump];
    control_bump += n;
    return p;
}
static void* alloc_nodestate(size_t n) {
    if (nodestate_bump + n > LENS_NODESTATE_BYTES) return NULL;
    void* p = &lens_nodestate_pool[nodestate_bump];
    nodestate_bump += n;
    return p;
}

/* ---- CRC-32 IEEE 802.3 (polynomial 0xEDB88320) ---- */
static uint32_t crc32_compute(const uint8_t* buf, size_t len) {
    static uint32_t tbl[256];
    static int ready = 0;
    if (!ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int j = 0; j < 8; j++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            tbl[i] = c;
        }
        ready = 1;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) c = tbl[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ---- Byte reader ---- */
typedef struct { const uint8_t* p; const uint8_t* end; } Cur;
static int   ok(Cur* c, size_t n) { return (size_t)(c->end - c->p) >= n; }
static uint8_t  u8(Cur* c)  { return *c->p++; }
static uint16_t u16(Cur* c) { uint16_t v=(uint16_t)(c->p[0]|(c->p[1]<<8)); c->p+=2; return v; }
static uint32_t u32(Cur* c) { uint32_t v=c->p[0]|(c->p[1]<<8)|(c->p[2]<<16)|((uint32_t)c->p[3]<<24); c->p+=4; return v; }
static int32_t  i32(Cur* c) { return (int32_t)u32(c); }

/* ---- snapshot_apply ---- */
int snapshot_apply(struct LensRuntime** out_rt, const uint8_t* bytes, size_t len) {
    if (!bytes || len < 23) return -1;   /* 19-byte header + 4-byte CRC */

    /* CRC: last 4 bytes; covers everything before. */
    uint32_t stored = (uint32_t)(bytes[len-4]|(bytes[len-3]<<8)|(bytes[len-2]<<16)|((uint32_t)bytes[len-1]<<24));
    if (stored != crc32_compute(bytes, len - 4)) return -2;

    Cur c = { bytes, bytes + len - 4 };

    /* HEADER (19 bytes). */
    if (!ok(&c, 19)) return -1;
    if (c.p[0]!='L'||c.p[1]!='E'||c.p[2]!='N'||c.p[3]!='S'||c.p[4]!='2') return -3;
    c.p += 5;
    if (u16(&c) != LENS_VERSION) return -4;   /* version mismatch */
    u16(&c);                           /* flags */
    uint16_t sc  = u16(&c);           /* slot_count */
    uint16_t master_idx = u16(&c);    /* master clock walk index (0xFFFF = none) */
    u16(&c);                           /* reserved */
    uint8_t  bc  = u8(&c);            /* buffer_count */
    uint8_t  tc  = u8(&c);            /* terminal_count */
    uint8_t  kc  = u8(&c);            /* kernel_id_count */
    u8(&c);                            /* reserved */

    /* Pool limit checks. */
    if (sc > LENS_MAX_SLOTS)     return -6;
    if (bc > LENS_MAX_BUFFERS)   return -6;
    if (tc > LENS_MAX_TERMINALS) return -6;

    /* Reset bump pointers. */
    audio_bump = control_bump = nodestate_bump = 0;
    slot_bump = buffer_bump = terminal_bump = const_bump = 0;

    /* Clear static runtime + slot/buffer/terminal pools. The node-state pool is
       cleared in pass 1 instead, conditionally: a changed layout is zeroed (a stale
       uint32 counter from a previous patch's sine phase landing in a new patch's
       op_step.counter would send pack12_read past the end of the tape buffer, 4
       notes in and 5+ notes out), but a same-layout re-send keeps its live op state
       (see preserve_nodestate below). */
    memset(&g_runtime, 0, sizeof(g_runtime));
    memset(lens_slot_pool,     0, sc * sizeof(struct Slot));
    memset(lens_buffer_pool,   0, bc * sizeof(struct Buffer));
    memset(lens_terminal_pool, 0, tc * sizeof(struct RuntimeTerminal));

    struct LensRuntime* rt = &g_runtime;
    /* Seed core1_done to a value Core 0's spin will never mistake for "done":
       sample_counter starts at 0, so a zero here would let sample 0 skip the
       wait and read Core 1's outputs before it has walked them. */
    rt->core1_done = 0xFFFFFFFFu;
    rt->slot_count    = sc;
    rt->buffer_count  = bc;
    rt->terminal_count = tc;
    rt->master_slot_idx = (master_idx < sc) ? master_idx : 0xFFFFu;

    rt->slots     = lens_slot_pool;

    /* KERNEL REGISTRY: build id -> kernel_id array. */
    if (kc > LENS_MAX_KFNS) return -6;
    uint8_t kkids[LENS_MAX_KFNS];
    for (uint8_t i = 0; i < kc; i++) {
        if (!ok(&c, 1)) return -1;
        uint8_t nlen = u8(&c);
        if (!ok(&c, nlen)) return -1;
        char name[64] = {0};
        memcpy(name, c.p, nlen < 63 ? nlen : 63);
        c.p += nlen;
        uint8_t kid = runtime_find_kernel(name);
        if (kid == KID_UNKNOWN) fprintf(stderr, "[apply] stub: %s\n", name);
        kkids[i] = kid;
    }

    /* --- Two-pass slot table parse ---
       Pass 1: compute pool_size and const count, record per-slot offsets.
       Pass 2: wire slots. */

    /* Per-slot pool offset table (walk order -> byte offset in state_pool). */
    static uint32_t soff[LENS_MAX_SLOTS];
    memset(soff, 0, sc * sizeof(uint32_t));

    /* Helper: advance cursor past one slot record, counting pool and const bytes. */
#define PASS1_SLOT(s1_, pool_, nconst_, wi_, sig_) \
    do { \
        if (!ok((s1_), 3)) return -1; \
        uint8_t wkid_ = *(s1_)->p++;  /* kernel_id (wire index) */ \
        (s1_)->p++;                    /* core */ \
        uint8_t nc_ = *(s1_)->p++;    /* in_count */ \
        if (nc_ > 5) return -5;        /* slots have at most 5 inputs; reject malformed */ \
        uint8_t rkid_ = (wkid_ < kc) ? kkids[wkid_] : KID_UNKNOWN; \
        (sig_) = ((sig_) * 16777619u) ^ rkid_;  /* FNV-1a step: layout signature */ \
        soff[(wi_)] = (pool_); \
        (pool_) += runtime_kernel_state_bytes(rkid_); \
        for (uint8_t jj_ = 0; jj_ < nc_; jj_++) { \
            if (!ok((s1_), 1)) return -1; \
            uint8_t tag_ = u8((s1_)); \
            if (tag_ == LENS_TAG_SLOT || tag_ == LENS_TAG_BUFFER || \
                tag_ == LENS_TAG_SLOT_OUT2) { \
                if (!ok((s1_), 2)) return -1; \
                (s1_)->p += 2; \
            } else if (tag_ == LENS_TAG_CONST_U8) { \
                (nconst_)++; \
                if (!ok((s1_), 1)) return -1; \
                (s1_)->p++; \
            } else if (tag_ == LENS_TAG_CONST_I32) { \
                (nconst_)++; \
                if (!ok((s1_), 4)) return -1; \
                (s1_)->p += 4; \
            } else { return -5; } \
        } \
        if (!ok((s1_), 6)) return -1; \
        (s1_)->p += 6; /* out_offset(2) + param0(4) */ \
    } while (0)

    /* Pass 1: walk the flat slot list to size the state pool and count consts.
       The same walk accumulates the node-state layout signature. */
    int preserve_nodestate = 0;
    {
        uint32_t pool = 0;
        uint16_t nconst = 0;
        uint32_t sig = 2166136261u ^ (uint32_t)sc;   /* FNV-1a offset basis, seeded by slot_count */
        Cur s1 = c;
        for (uint16_t wi = 0; wi < sc; wi++) {
            PASS1_SLOT(&s1, pool, nconst, wi, sig);
        }
        (void)nconst;  /* a use-count upper bound; the pool dedupes, so the real
                          limit is distinct values, checked as we intern in pass 2. */
        preserve_nodestate = g_have_last_nodestate && (sig == g_last_nodestate_sig);
        g_last_nodestate_sig  = sig;
        g_have_last_nodestate = 1;
        rt->state_pool_size = pool;
        rt->state_pool = (uint8_t*)alloc_nodestate(pool ? pool : 1);
        if (!rt->state_pool) return -6;
        /* Same layout -> keep the live bytes (op state survives the swap).
           Changed layout -> zero, so a stale value cannot land in a new slot. */
        if (!preserve_nodestate) memset(rt->state_pool, 0, pool ? pool : 1);
        rt->const_pool = lens_const_pool;
        memset(lens_const_pool, 0, sizeof(lens_const_pool));
        rt->const_count = 0;
    }

    /* Pass 2: wire slots. */

    /* Fixup list for buffer in_refs (stack array; 4 per slot worst case). */
    typedef struct { uint16_t wi; uint8_t in_idx; uint16_t buf_id; } BufFix;
    /* A buffer input is always one of the first four refs (in4 only ever carries a
     * value), so four per slot bounds the fixups; sc <= LENS_MAX_SLOTS. */
    static BufFix bfixes[LENS_MAX_SLOTS * 4];
    uint16_t nbfix = 0;

    static int32_t g_zero = 0;

    /* Per-slot core byte; populated in pass 2, used to fill per-core index arrays. */
    static uint8_t slot_core[LENS_MAX_SLOTS];
    memset(slot_core, 0, sc);

    /* Helper: parse one slot record into rt->slots[wi] and wire pointers. */
#define PASS2_SLOT(wi_) \
    do { \
        if (!ok(&c, 3)) return -1; \
        uint8_t kid_ = u8(&c); \
        uint8_t core_ = u8(&c);   /* core: captured for per-core index arrays */ \
        slot_core[(wi_)] = core_; \
        uint8_t nc_ = u8(&c); \
        struct Slot* s_ = &rt->slots[(wi_)]; \
        s_->kernel_id = (kid_ < kc) ? kkids[kid_] : KID_UNKNOWN; \
        s_->out = (void*)(rt->state_pool + soff[(wi_)]); \
        s_->in0 = s_->in1 = s_->in2 = s_->in3 = s_->in4 = (void*)&g_zero; \
        void** inp_[5] = { &s_->in0, &s_->in1, &s_->in2, &s_->in3, &s_->in4 }; \
        for (uint8_t j_ = 0; j_ < nc_ && j_ < 5; j_++) { \
            if (!ok(&c, 1)) return -1; \
            uint8_t tag_ = u8(&c); \
            if (tag_ == LENS_TAG_SLOT) { \
                if (!ok(&c, 2)) return -1; \
                uint16_t ref_wi_ = u16(&c); \
                if (ref_wi_ < sc) \
                    *inp_[j_] = (void*)(rt->state_pool + soff[ref_wi_]); \
            } else if (tag_ == LENS_TAG_SLOT_OUT2) { \
                if (!ok(&c, 2)) return -1; \
                uint16_t ref_wi_ = u16(&c); \
                if (ref_wi_ < sc) \
                    *inp_[j_] = (void*)(rt->state_pool + soff[ref_wi_] + 4); \
            } else if (tag_ == LENS_TAG_BUFFER) { \
                if (!ok(&c, 2)) return -1; \
                uint16_t bid_ = u16(&c); \
                if (nbfix < LENS_MAX_SLOTS * 4) \
                    bfixes[nbfix++] = (BufFix){ (wi_), j_, bid_ }; \
            } else if (tag_ == LENS_TAG_CONST_U8) { \
                if (!ok(&c, 1)) return -1; \
                int ci_ = lens_intern_const(rt, u8(&c)); \
                *inp_[j_] = (ci_ >= 0) ? (void*)&rt->const_pool[ci_] : (void*)&g_zero; \
            } else if (tag_ == LENS_TAG_CONST_I32) { \
                if (!ok(&c, 4)) return -1; \
                int ci_ = lens_intern_const(rt, i32(&c)); \
                *inp_[j_] = (ci_ >= 0) ? (void*)&rt->const_pool[ci_] : (void*)&g_zero; \
            } else { return -5; } \
        } \
        if (!ok(&c, 6)) return -1; \
        u16(&c);                    /* out_offset: ignored */ \
        s_->param0 = u32(&c); \
        if (runtime_is_hw_leaf(s_->kernel_id)) \
            s_->in0 = (void*)runtime_hw_jack_ptr(s_->param0); \
        if (runtime_is_midi_leaf(s_->kernel_id)) \
            s_->in0 = (void*)runtime_midi_jack_ptr(s_->param0); \
        runtime_slot_wire_fn(s_); \
    } while (0)

    /* Pass 2: every slot in walk order. */
    for (uint16_t wi = 0; wi < sc; wi++) {
        PASS2_SLOT(wi);
    }

    /* --- Flat per-core walk-order lists (slots[] is already in walk order). --- */
    {
        uint16_t n0 = 0, n1 = 0;
        uint8_t hasRh = 0;
        for (uint16_t i = 0; i < sc; i++) {
            uint8_t k = rt->slots[i].kernel_id;
            if ((k >= KID_OP_RECORDHEAD_PER_SAMPLE && k <= KID_OP_RECORDHEAD_LEN_CAPPED_GATED)
                || k == KID_OP_RECORDHEAD_SEEK) hasRh = 1;
            if (slot_core[i] == 0) lens_core0_flat_ptrs[n0++] = &rt->slots[i];
            else                   lens_core1_flat_ptrs[n1++] = &rt->slots[i];
        }
        rt->core0_slots = lens_core0_flat_ptrs; rt->core0_count = n0;
        rt->core1_slots = lens_core1_flat_ptrs; rt->core1_count = n1;
        rt->has_recordhead = hasRh;
    }

    /* --- Cross-core shadow build ---
       Each core's slot list is already in producer-before-consumer order, so
       intra-core reads stay live. A consumer that reads a producer on the OTHER
       core would otherwise see a value whose freshness depends on core walk
       order (a race). Redirect such reads to a per-producer shadow that is
       republished once per sample at the boundary, yielding a deterministic
       one-sample lag.

       A consumer input inN equals state_pool + soff[P] for the value of slot P,
       or state_pool + soff[P] + 4 for a +4 second-output read (TAG_SLOT_OUT2). */
    rt->xcore_count = 0;
    {
        uint8_t* base = rt->state_pool;
        /* Per-producer shadow index, -1 = none yet (dedup across consumers). */
        static int16_t shadow_idx[LENS_MAX_SLOTS];
        for (uint16_t i = 0; i < sc; i++) shadow_idx[i] = -1;

        for (uint16_t si = 0; si < sc; si++) {
            uint8_t score = slot_core[si];
            void** inp[5] = { &rt->slots[si].in0, &rt->slots[si].in1,
                              &rt->slots[si].in2, &rt->slots[si].in3,
                              &rt->slots[si].in4 };
            for (uint8_t j = 0; j < 5; j++) {
                int32_t* in = (int32_t*)*inp[j];
                if (!in) continue;
                size_t off = (size_t)((uint8_t*)in - base);
                if ((uint8_t*)in < base || off >= rt->state_pool_size) continue;

                /* Find producer P: a value read has off == soff[P]; a +4 field read
                   (recordhead head_pos, phasor/follow tick) has off == soff[P] + 4.
                   Check exact value matches FIRST, then the +4 field, or a value
                   read of P+1 (a zero-state slot is 8 bytes) would be misread as a
                   +4 field read of P. */
                int pi = -1;
                int is_field = 0;
                for (uint16_t k = 0; k < sc; k++)
                    if (off == soff[k]) { pi = k; break; }
                if (pi < 0)
                    for (uint16_t k = 0; k < sc; k++)
                        if (off == soff[k] + 4u) { pi = k; is_field = 1; break; }
                if (pi < 0) continue;
                if (slot_core[pi] == score) continue;  /* intra-core: stays live */

                if (is_field) {
                    /* Cross-core +4 field read. Shadow the field directly so it is
                       not left racy. Overflow is a hard error: leaving the read
                       unshadowed would be an order-dependent race. */
                    if (rt->xcore_count >= LENS_MAX_SLOTS) return -7;
                    uint16_t idx = rt->xcore_count++;
                    rt->xcore_src[idx] = in;
                    rt->xcore_core[idx] = slot_core[pi];   /* producer core */
                    lens_shadow_pool[idx] = *in;
                    *inp[j] = (void*)&lens_shadow_pool[idx];
                    continue;
                }

                /* Cross-core value read: one shadow per producer, reused. */
                if (shadow_idx[pi] < 0) {
                    if (rt->xcore_count >= LENS_MAX_SLOTS) return -7;
                    uint16_t idx = rt->xcore_count++;
                    rt->xcore_src[idx] = (int32_t*)(base + soff[pi]);
                    rt->xcore_core[idx] = slot_core[pi];   /* producer core */
                    lens_shadow_pool[idx] = *rt->xcore_src[idx];
                    shadow_idx[pi] = (int16_t)idx;
                }
                *inp[j] = (void*)&lens_shadow_pool[shadow_idx[pi]];
            }
        }
    }

    /* --- BUFFER TABLE --- */

    /* Collect incoming buffer geometry to check against last apply: audio
       lengths, and the control pool's ordered (kind, length) sequence. */
    uint32_t new_audio_lens[LENS_MAX_BUFFERS];
    uint8_t  new_audio_count = 0;
    uint32_t new_ctrl_lens[LENS_MAX_BUFFERS];
    uint8_t  new_ctrl_kinds[LENS_MAX_BUFFERS];
    uint8_t  new_ctrl_count = 0;
    {
        Cur scan = c;
        for (uint8_t i = 0; i < bc; i++) {
            if (!ok(&scan, 6)) { new_audio_count = 0; new_ctrl_count = 0; break; }
            uint8_t  kind  = u8(&scan);
            uint32_t blen  = u32(&scan);
            uint8_t  flags = u8(&scan);   /* bit0 = seed present, bit1 = keep */
            if (kind == LENS_BUF_KIND_AUDIO) {
                if (new_audio_count < LENS_MAX_BUFFERS)
                    new_audio_lens[new_audio_count++] = blen;
            } else if (new_ctrl_count < LENS_MAX_BUFFERS) {
                new_ctrl_kinds[new_ctrl_count] = kind;
                new_ctrl_lens[new_ctrl_count++] = blen;
            }
            if (flags & 1u) {
                /* skip seed data: blen * 2 bytes */
                if (!ok(&scan, (size_t)blen * 2u)) { new_audio_count = 0; new_ctrl_count = 0; break; }
                scan.p += (size_t)blen * 2u;
            }
        }
    }

    /* Geometry match: same number of audio buffers with identical lengths. */
    int geom_same = (new_audio_count == g_last_audio_count) && (new_audio_count > 0);
    for (uint8_t i = 0; geom_same && i < new_audio_count; i++) {
        if (new_audio_lens[i] != g_last_audio_lens[i]) geom_same = 0;
    }

    /* Control pool: preserve up to the longest unchanged (kind, length) prefix. */
    uint8_t ctrl_prefix = new_ctrl_count < g_last_ctrl_count ? new_ctrl_count : g_last_ctrl_count;
    for (uint8_t i = 0; i < ctrl_prefix; i++) {
        if (new_ctrl_kinds[i] != g_last_ctrl_kinds[i] || new_ctrl_lens[i] != g_last_ctrl_lens[i]) {
            ctrl_prefix = i;
            break;
        }
    }

    rt->buffers = lens_buffer_pool;
    uint8_t ctrl_idx = 0;
    for (uint8_t i = 0; i < bc; i++) {
        if (!ok(&c, 6)) return -1;
        uint8_t kind  = u8(&c);
        uint32_t blen = u32(&c);
        uint8_t  flags = u8(&c);            /* bit0 = seed present, bit1 = keep */
        int has_seed = (flags & 1u) != 0;
        int keep     = (flags & 2u) != 0;
        /* Cap at the audio pool's cell capacity so one buffer can span the whole
           torus (~1.82 s); the bump allocator rejects anything that won't fit. */
        const uint32_t max_cells = (LENS_AUDIO_BUFFER_BYTES * 2u) / 3u;
        if (blen > max_cells) blen = max_cells;
        rt->buffers[i].length   = blen;
        /* Packed byte storage: (blen * 3 + 1) >> 1 bytes (round up). */
        size_t nbytes = ((size_t)blen * 3u + 1u) >> 1;
        int pool_preserved;
        uint8_t* bytes;
        if (kind == LENS_BUF_KIND_AUDIO) {
            bytes = (uint8_t*)alloc_audio(nbytes);
            /* Skip zero-fill if geometry is preserved; content rings through. */
            if (!bytes) return -6;
            if (!geom_same) memset(bytes, 0, nbytes);
            pool_preserved = geom_same;
        } else {
            bytes = (uint8_t*)alloc_control(nbytes);
            if (!bytes) return -6;
            int ctrl_same = ctrl_idx < ctrl_prefix;
            ctrl_idx++;
            if (!ctrl_same) memset(bytes, 0, nbytes);
            pool_preserved = ctrl_same;
        }
        rt->buffers[i].bytes = bytes;
        /* A :keep tape skips the seed write when its pool geometry is preserved, so
           its live-evolved content survives a same-layout swap. Default (no keep) or
           a changed layout re-applies the seed. The seed bytes are always consumed
           from the stream to keep the cursor aligned. */
        int apply_seed = has_seed && !(keep && pool_preserved);
        if (has_seed) {
            for (uint32_t j = 0; j < blen; j++) {
                if (!ok(&c, 2)) return -1;
                uint16_t v = (uint16_t)u16(&c);
                if (apply_seed) pack12_write_snapshot(bytes, j, (int32_t)v);
            }
        }
    }

    /* Update stored geometry after successful buffer allocation. */
    g_last_audio_count = new_audio_count;
    for (uint8_t i = 0; i < new_audio_count; i++)
        g_last_audio_lens[i] = new_audio_lens[i];
    g_last_ctrl_count = new_ctrl_count;
    for (uint8_t i = 0; i < new_ctrl_count; i++) {
        g_last_ctrl_kinds[i] = new_ctrl_kinds[i];
        g_last_ctrl_lens[i]  = new_ctrl_lens[i];
    }

    /* Apply buffer fixups. */
    for (uint16_t i = 0; i < nbfix; i++) {
        uint16_t wi  = bfixes[i].wi;
        uint8_t  idx = bfixes[i].in_idx;
        uint16_t bid = bfixes[i].buf_id;
        if (wi >= sc || bid >= bc) continue;
        void** inp[5] = { &rt->slots[wi].in0, &rt->slots[wi].in1,
                          &rt->slots[wi].in2, &rt->slots[wi].in3,
                          &rt->slots[wi].in4 };
        if (idx < 5) *inp[idx] = (void*)&rt->buffers[bid];
    }

    /* A tape op whose Buffer* input was not wired to a real buffer (it is fed a
       value, e.g. a runtime-selected tape) points instead at the empty-buffer
       sentinel so the op no-ops rather than reading a reinterpreted int32. */
    {
        struct Buffer* blo = &rt->buffers[0];
        struct Buffer* bhi = &rt->buffers[bc];
        for (uint16_t wi = 0; wi < sc; wi++) {
            int bi = buffer_input_index(rt->slots[wi].kernel_id);
            if (bi < 0) continue;
            void** inp[5] = { &rt->slots[wi].in0, &rt->slots[wi].in1,
                              &rt->slots[wi].in2, &rt->slots[wi].in3,
                              &rt->slots[wi].in4 };
            struct Buffer* p = (struct Buffer*)*inp[bi];
            if (p < blo || p >= bhi) *inp[bi] = (void*)&g_empty_buffer;
        }
    }

    /* --- TERMINAL TABLE --- */
    rt->terminals = lens_terminal_pool;
    for (uint8_t i = 0; i < tc; i++) {
        if (!ok(&c, 4)) return -1;
        rt->terminals[i].jack_id       = u8(&c);
        rt->terminals[i].slot_walk_idx = u16(&c);
        rt->terminals[i].mode          = u8(&c);
    }

    *out_rt = rt;
    return 0;
}


} // namespace Card_Lens

extern "C" {
    __attribute__((weak)) void tuh_midi_mount_cb(uint8_t, uint8_t, uint8_t, uint8_t, uint16_t) {}
    __attribute__((weak)) void tuh_midi_umount_cb(uint8_t, uint8_t) {}
    void set_thread_globals(CardGlobals* inst) {
        t_instance = inst;
        if (inst) {
            if (!inst->card_ptr && ComputerCard::thisptr) {
                inst->card_ptr = ComputerCard::thisptr;
            }
            ComputerCard::thisptr = inst->card_ptr;
        }
    }
    void set_core1_thread(bool is_core1) {
        is_core1_thread = is_core1;
    }
    void run_card() {
        is_core1_thread = false;
        try {
            Card_Lens::main();
        } catch (const ThreadExitException& e) {
            // Thread terminated safely
        }
    }
}
