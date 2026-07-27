/**
 * two_tracks_stream.c — flash-backed mono audio streaming store for Two Tracks.
 *
 * Forked from Goldfish 2.0 (releases/11_goldfish/goldfish_stream.c) by Dune
 * Desormeaux, adapted for the Two Tracks dual-read-head phase looper:
 *   - Mono (1 audio channel; both read heads read the same loop).
 *   - No CV stream (CV outputs show live head position, not recorded CV).
 *   - No DELAY mode (Two Tracks is PLAY/ARMED/RECORD only).
 *
 * See two_tracks_stream.h for the design overview. Record + flash plumbing +
 * read-back + core-1 head refill + loop-boundary previews + seeks.
 */

#include "two_tracks_stream.h"
#include "flash_size.h"
#include "adpcm.h"

#include <string.h>
#include "pico/platform.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"

#ifndef XIP_BASE
#define XIP_BASE 0x10000000u
#endif

#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE 4096u
#endif

/* ------------------------------------------------------------------ */
/* Keyframe + header layout                                           */
/* ------------------------------------------------------------------ */

typedef struct {
	int16_t predictor;
	int8_t  step_index;
	int8_t  _pad;
} tt_keyframe_t;

/* Fixed-size metadata prefix written to the header region of flash. It is
 * immediately followed in flash by num_keyframes tt_keyframe_t entries. */
typedef struct {
	uint32_t magic;
	uint32_t version;
	uint32_t sample_count;      /* recorded length, in audio samples */
	uint32_t keyframe_interval; /* samples between keyframes */
	uint32_t num_keyframes;
	uint32_t audio_off;         /* geometry echo (sanity check on load) */
} tt_stream_hdr_t;

/* ------------------------------------------------------------------ */
/* Page ring (core 0 producer -> core 1 consumer)                     */
/* ------------------------------------------------------------------ */

typedef struct {
	uint32_t flash_off;               /* absolute flash offset to program */
	uint8_t  data[TT_PAGE_SIZE];
} tt_page_t;

/* Audio page ring. Core 0 encodes ADPCM bytes DIRECTLY into the slot at s_page_w
 * (no 256-byte burst copy: a burst write here stalls ~21us whenever it lands
 * during a core 1 flash write, which the live RECORD monitor turns into a
 * click). Mono: one channel, so one slot is published per full page (w += 1). */
static tt_page_t         s_page_ring[TT_PAGE_RING_COUNT];
static volatile uint32_t s_page_w; /* producer index (core 0) */
static volatile uint32_t s_page_r; /* consumer index (core 1) */

/* ------------------------------------------------------------------ */
/* Module state                                                       */
/* ------------------------------------------------------------------ */

/* Per-channel audio state. Mono: one channel (index 0). Both read heads read
 * the same loop (channel 0), tracking independent positions. */
typedef struct {
	uint32_t        audio_off;      /* flash base of this channel's region */
	/* record encoder (core 0) */
	adpcm_state_t   enc;
	uint8_t         cur_byte;
	bool            nybble_phase;   /* false = expecting low nybble */
	uint32_t        fill;
	uint32_t        write_off;      /* next flash offset for an audio page */
	/* keyframes (core 0 writes; both cores read) */
	tt_keyframe_t   keyframes[TT_KEYFRAME_BUDGET];
	uint32_t        num_keyframes;
	/* core 1 erase-ahead + flush tracking */
	uint32_t        next_erase;
	volatile uint32_t flushed_samples;
	uint32_t        pages_written;
} tt_audio_channel_t;

static tt_audio_channel_t s_ch[TT_AUDIO_CHANNELS];

/* Geometry (computed in init) */
static uint32_t s_flash_size;
static uint32_t s_header_off;
static uint32_t s_header_size;
static uint32_t s_audio_bytes;       /* bytes in the mono audio region */
static uint32_t s_capacity_samples;
static uint32_t s_keyframe_interval;
static uint32_t s_kf_slots;          /* keyframe slots = capacity/interval */

/* Record state (core 0) */
static volatile bool s_rec_active;      /* cross-core: gates core1 erase-ahead */
static uint32_t       s_write_index;    /* audio samples written so far */
static uint32_t       s_recorded_samples; /* readable length once record stops */
static volatile uint32_t s_erase_count;

/* Header persistence flags (set by core 0, serviced by core 1) */
static volatile bool s_header_invalidate; /* record_start: erase old header      */
static volatile bool s_header_dirty;      /* record_stop:  flush new header      */

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static inline uint32_t align_down(uint32_t v, uint32_t a) { return v & ~(a - 1u); }
static inline uint32_t align_up(uint32_t v, uint32_t a)   { return (v + a - 1u) & ~(a - 1u); }

/* Modular mapping of a logical position onto the (circular) flash region. */
static inline uint32_t kf_slot(uint32_t k)          { return k % s_kf_slots; }
static inline uint32_t audio_byte_wrap(uint32_t b)  { return b % s_audio_bytes; }

static inline uint32_t next_pow2(uint32_t v)
{
	uint32_t p = 1u;
	while (p < v) p <<= 1;
	return p;
}

static inline const uint8_t *xip_ptr(uint32_t flash_off)
{
	return (const uint8_t *)(XIP_BASE + flash_off);
}

/* Program one page with interrupts masked on the calling (core 1) core.
 * Correctness relies on core 0 being fully RAM-resident so it never touches
 * XIP during these windows. */
static void flash_program_page(uint32_t off, const uint8_t *data)
{
	uint32_t ints = save_and_disable_interrupts();
	flash_range_program(off, data, TT_PAGE_SIZE);
	restore_interrupts(ints);
}

/* Account one just-programmed audio page towards the flushed (readable) limit. */
static inline void note_page_flushed(uint32_t off)
{
	if (off >= s_ch[0].audio_off && off < s_ch[0].audio_off + s_audio_bytes) {
		s_ch[0].pages_written++;
		s_ch[0].flushed_samples = s_ch[0].pages_written * (TT_PAGE_SIZE * 2u);
	}
}

/* ------------------------------------------------------------------ */
/* Low-level QSPI: erase-suspend + program-during-erase                */
/* ------------------------------------------------------------------ */
/*
 * A blocking sector erase freezes core 1 for tens of ms, during which no new
 * audio can be flushed and the playback heads cannot be refilled — the source
 * of playback underruns. To reach zero underruns we instead:
 *   1. Erase one region "ahead" of the write head (erase sector M while the
 *      producer is still filling sector M-1), so pending pages always target
 *      already-erased sectors.
 *   2. During each sector erase, repeatedly SUSPEND the erase, program any
 *      pending pages (advancing the flushed frontier) and refill the heads
 *      via XIP, then RESUME. The flushed frontier therefore keeps advancing
 *      right through the erase, so a trailing read head never starves.
 *
 * This drives the flash controller directly (bypassing hardware/flash.h) so it
 * can issue Erase-Suspend (0x75) / Erase-Resume (0x7A). It runs only on core 1
 * with interrupts masked; correctness relies on core 0 being fully RAM-resident
 * (copy_to_ram) so it never touches XIP during these windows.
 */

/* Playback heads serviced by core 1 (registered via tt_stream_set_heads). */
static tt_head_t *s_head[2];
static void head_refill(tt_head_t *h);

typedef void (*flash_rom_fn)(void);
static flash_rom_fn s_rom_connect;   /* connect_internal_flash */
static flash_rom_fn s_rom_exit_xip;  /* flash_exit_xip         */
static flash_rom_fn s_rom_flush;     /* flash_flush_cache      */
static flash_rom_fn s_rom_enter_xip; /* flash_enter_cmd_xip    */

static void qspi_rom_init(void)
{
	s_rom_connect   = (flash_rom_fn)rom_func_lookup(rom_table_code('I', 'F'));
	s_rom_exit_xip  = (flash_rom_fn)rom_func_lookup(rom_table_code('E', 'X'));
	s_rom_flush     = (flash_rom_fn)rom_func_lookup(rom_table_code('F', 'C'));
	s_rom_enter_xip = (flash_rom_fn)rom_func_lookup(rom_table_code('C', 'X'));
}

/* Drive the QSPI chip-select via the pad override (SDK does the same). */
static void __not_in_flash_func(qspi_cs)(bool high)
{
	uint32_t v = high ? IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_HIGH
	                  : IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_LOW;
	hw_write_masked(&ioqspi_hw->io[1].ctrl,
	                v << IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_LSB,
	                IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_BITS);
}

/* One command-mode transaction over the SSI in single-bit (0x03-style) mode.
 * tx may be NULL (send zeros); rx may be NULL (discard). Mirrors the inner loop
 * of the SDK's flash_do_cmd, keeping <=14 bytes in flight. */
static void __not_in_flash_func(qspi_xfer)(const uint8_t *tx, uint8_t *rx, size_t n)
{
	qspi_cs(false);
	size_t tx_rem = n, rx_rem = n;
	while (tx_rem || rx_rem) {
		uint32_t sr = ssi_hw->sr;
		if ((sr & SSI_SR_TFNF_BITS) && tx_rem && (rx_rem - tx_rem) < 14u) {
			ssi_hw->dr0 = tx ? (uint32_t)*tx++ : 0u;
			--tx_rem;
		}
		if ((sr & SSI_SR_RFNE_BITS) && rx_rem) {
			uint8_t b = (uint8_t)ssi_hw->dr0;
			if (rx) *rx++ = b;
			--rx_rem;
		}
	}
	qspi_cs(true);
}

/* Read status register 1 (WIP = bit 0). */
static uint8_t __not_in_flash_func(qspi_status)(void)
{
	uint8_t tx[2] = { 0x05u, 0x00u };
	uint8_t rx[2] = { 0u, 0u };
	qspi_xfer(tx, rx, 2);
	return rx[1];
}

static void __not_in_flash_func(qspi_write_enable)(void)
{
	uint8_t c = 0x06u;
	qspi_xfer(&c, NULL, 1);
}

/* Program one 256-byte page in command mode (XIP must already be exited).
 * Blocks on WIP so the caller may safely resume an erase afterwards. */
static void __not_in_flash_func(qspi_program_page)(uint32_t off, const uint8_t *data)
{
	qspi_write_enable();
	uint8_t hdr[4] = { 0x02u, (uint8_t)(off >> 16), (uint8_t)(off >> 8), (uint8_t)off };
	uint32_t total = 4u + TT_PAGE_SIZE;
	qspi_cs(false);
	uint32_t sent = 0u, got = 0u;
	while (sent < total || got < total) {
		uint32_t sr = ssi_hw->sr;
		if ((sr & SSI_SR_TFNF_BITS) && sent < total && (sent - got) < 14u) {
			uint8_t b = (sent < 4u) ? hdr[sent] : data[sent - 4u];
			ssi_hw->dr0 = (uint32_t)b;
			++sent;
		}
		if ((sr & SSI_SR_RFNE_BITS) && got < total) {
			(void)ssi_hw->dr0;
			++got;
		}
	}
	qspi_cs(true);
	while (qspi_status() & 0x01u) { /* WIP: page program in progress */ }
}

/* True if the sector containing flash offset `off` has already been erased by
 * the erase-ahead, i.e. it is safe to program `off` during an erase-suspend. The
 * erase frontier leads the write head, so a page is safe once the frontier is
 * at least one sector past it. At record start the frontier still sits at the
 * region base, so early pages are (correctly) reported unsafe until erase-ahead
 * has run — this stops the suspend from programming into not-yet-erased flash. */
static bool __not_in_flash_func(sector_erased)(uint32_t off)
{
	if (off >= s_ch[0].audio_off && off < s_ch[0].audio_off + s_audio_bytes) {
		uint32_t d = (s_ch[0].next_erase - off) % s_audio_bytes;
		/* d small = frontier is that far past off (erased). d >= half the
		 * region means the frontier is actually BEHIND off (wrapped): the
		 * sector is NOT erased yet. */
		return d >= FLASH_SECTOR_SIZE && d < s_audio_bytes / 2u;
	}
	return false;
}

/* Erase one 4KB sector, suspending as needed to program pending pages (keeping
 * the flushed frontier advancing) and refill the heads. Interrupts masked. */
static void __not_in_flash_func(flash_erase_sector_suspend)(uint32_t off)
{
	uint32_t ints = save_and_disable_interrupts();
	s_rom_connect();
	s_rom_exit_xip();

	qspi_write_enable();
	uint8_t er[4] = { 0x20u, (uint8_t)(off >> 16), (uint8_t)(off >> 8), (uint8_t)off };
	qspi_xfer(er, NULL, 4);

	uint32_t guard = 0u;
	uint32_t poll  = 0u;
	bool heads_active = (s_head[0] != NULL) || (s_head[1] != NULL);
	while (qspi_status() & 0x01u) {          /* WIP set: erase running */
		bool     have_page = false;
		uint32_t slot = 0u, poff = 0u;
		if (s_page_r != s_page_w) {
			slot = s_page_r & (TT_PAGE_RING_COUNT - 1u);
			poff = s_page_ring[slot].flash_off;
			/* Only program pages whose sector is already erased. A page whose
			 * erase-ahead has not run yet this pass (e.g. at record start) is
			 * left for the post-erase page loop, once the frontier is
			 * established. This prevents programming into non-erased flash. */
			have_page = sector_erased(poff);
		}

		/* Suspend to service either a ready page OR - only when heads are
		 * active (playback modes) - periodically to refill them so they don't
		 * starve during a long erase with an empty flush queue. In pure
		 * RECORD the heads are off, so no periodic suspend is needed. */
		if (have_page || (heads_active && ++poll >= 1000u)) {
			poll = 0u;
			uint8_t sus = 0x75u;
			qspi_xfer(&sus, NULL, 1);
			busy_wait_us(20);                /* tSUS: ready for next command */

			if (have_page) {
				qspi_program_page(poff, s_page_ring[slot].data);
				note_page_flushed(poff);
				__dmb();
				s_page_r++;
			}

			/* Refill the heads from flushed data (needs XIP mapped). */
			s_rom_flush();
			s_rom_enter_xip();
			head_refill(s_head[0]);
			head_refill(s_head[1]);
			s_rom_connect();
			s_rom_exit_xip();

			uint8_t res = 0x7Au;
			qspi_xfer(&res, NULL, 1);
			busy_wait_us(30);                /* tRES: let erase restart (WIP=1) */
		}
		if (++guard > 4000000u) break;       /* safety: never spin forever */
	}

	s_rom_flush();
	s_rom_enter_xip();
	restore_interrupts(ints);
	s_erase_count++;
}

/* Region-relative distance the erase frontier leads the write head, modulo the
 * region size. Kept >= TT_ERASE_LOOKAHEAD sectors so pending pages always land
 * in erased sectors. Mono: one channel. */
static void ensure_erase_ahead_audio(void)
{
	if (s_audio_bytes == 0u) return;
	tt_audio_channel_t *ch = &s_ch[0];
	uint32_t wrel = (ch->write_off - ch->audio_off) % s_audio_bytes;
	uint32_t guard = 0u;
	for (;;) {
		uint32_t erel  = (ch->next_erase - ch->audio_off) % s_audio_bytes;
		uint32_t ahead = (erel + s_audio_bytes - wrel) % s_audio_bytes;
		/* A modular "ahead" of more than half the region means the frontier
		 * actually fell BEHIND the write head (it wrapped) - force catch-up. */
		if (ahead > s_audio_bytes / 2u) ahead = 0u;
		if (ahead >= TT_ERASE_LOOKAHEAD * FLASH_SECTOR_SIZE) break;
		flash_erase_sector_suspend(ch->next_erase);
		ch->next_erase += FLASH_SECTOR_SIZE;
		if (ch->next_erase >= ch->audio_off + s_audio_bytes) ch->next_erase = ch->audio_off;
		if (++guard >= TT_ERASE_LOOKAHEAD + 2u) break;
	}
}

/* ------------------------------------------------------------------ */
/* Init / geometry                                                    */
/* ------------------------------------------------------------------ */

void tt_stream_init(void)
{
	qspi_rom_init();
	/* Skip the JEDEC probe (flash_do_cmd) — it can hang the SSI on some cards
	 * after prior flash writes, bricking the firmware before Run(). Use the
	 * SDK's compile-time PICO_FLASH_SIZE_BYTES instead. The pico board default
	 * is 2 MB; override via PICO_FLASH_SIZE_BYTES for 16 MB cards. */
	s_flash_size = PICO_FLASH_SIZE_BYTES;

	uint32_t usable = (s_flash_size > TT_FIRMWARE_RESERVE)
	                      ? (s_flash_size - TT_FIRMWARE_RESERVE)
	                      : 0u;

	/* Header holds the fixed metadata plus up to TT_KEYFRAME_BUDGET
	 * keyframe entries. */
	uint32_t header_bytes = sizeof(tt_stream_hdr_t)
	                        + TT_KEYFRAME_BUDGET * sizeof(tt_keyframe_t);
	s_header_size = align_up(header_bytes, FLASH_SECTOR_SIZE);
	s_header_off  = TT_FIRMWARE_RESERVE;

	uint32_t remaining = (usable > s_header_size) ? (usable - s_header_size) : 0u;

	/* Mono: audio gets the full remaining flash budget (no 2:2:1 split with a
	 * CV stream, since Two Tracks does not record CV). */
	s_audio_bytes = align_down(remaining, FLASH_SECTOR_SIZE);

	s_ch[0].audio_off = s_header_off + s_header_size;

	/* Capacity = audio bytes * 2 (2 ADPCM nybbles per byte). */
	s_capacity_samples = s_audio_bytes * 2u;

	/* Choose the smallest power-of-two interval that keeps the keyframe count
	 * within budget. Power-of-two keeps keyframe indexing a shift. */
	uint32_t need = (s_capacity_samples + TT_KEYFRAME_BUDGET - 1u)
	                / TT_KEYFRAME_BUDGET;
	s_keyframe_interval = next_pow2(need < 256u ? 256u : need);

	s_kf_slots = s_capacity_samples / s_keyframe_interval;
	if (s_kf_slots == 0u) s_kf_slots = 1u;
	if (s_kf_slots > TT_KEYFRAME_BUDGET) s_kf_slots = TT_KEYFRAME_BUDGET;

	s_ch[0].num_keyframes = 0u;
	s_recorded_samples = 0u;
	s_rec_active       = false;
	s_page_w = s_page_r = 0u;
	s_erase_count = 0u;
	s_header_dirty = false;
	s_header_invalidate = false;

	/* ---- Load persisted recording header from flash (if present) ----
	 * The header holds the recorded length + keyframe index. Without it the
	 * audio bytes survive reboot but can't be decoded. The header is written
	 * by core 1 after record_stop and invalidated (erased) at record_start, so
	 * a power loss during recording boots into empty PLAY, not garbage. */
	const uint8_t *hp = xip_ptr(s_header_off);
	const tt_stream_hdr_t *hdr = (const tt_stream_hdr_t *)hp;

	if (hdr->magic == TT_STREAM_MAGIC
	    && hdr->version == TT_STREAM_VERSION
	    && hdr->audio_off == s_ch[0].audio_off
	    && hdr->keyframe_interval == s_keyframe_interval
	    && hdr->sample_count > 0u
	    && hdr->sample_count <= s_capacity_samples
	    && hdr->num_keyframes > 0u
	    && hdr->num_keyframes <= s_kf_slots)
	{
		uint32_t nk = hdr->num_keyframes;
		const uint8_t *kp = xip_ptr(s_header_off + TT_PAGE_SIZE);
		memcpy(s_ch[0].keyframes, kp, nk * sizeof(tt_keyframe_t));
		s_ch[0].num_keyframes = nk;
		s_recorded_samples = hdr->sample_count;
	}
}

/* ------------------------------------------------------------------ */
/* Record path (core 0)                                               */
/* ------------------------------------------------------------------ */

void __not_in_flash_func(tt_stream_record_start)(void)
{
	s_rec_active  = true;
	s_write_index = 0u;

	tt_audio_channel_t *ch = &s_ch[0];
	ch->enc.predictor   = 0;
	ch->enc.step_index  = 0;
	ch->cur_byte        = 0u;
	ch->nybble_phase    = false;
	ch->fill            = 0u;
	ch->write_off       = ch->audio_off;
	ch->num_keyframes   = 0u;
	ch->next_erase      = ch->audio_off; /* erase-ahead starts at region base */
	ch->flushed_samples = 0u;
	ch->pages_written   = 0u;

	s_recorded_samples = 0u;
	/* Invalidate the old header so a power loss during recording doesn't leave
	 * a stale header pointing at partially-overwritten audio. Core 1 erases
	 * the header region on the next io_task call (before audio erase-ahead). */
	s_header_invalidate = true;
}

bool __not_in_flash_func(tt_stream_record_sample)(int16_t sample)
{
	if (!s_rec_active) return false;
	if (s_write_index >= s_capacity_samples) {
		return false; /* region full */
	}

	tt_audio_channel_t *ch = &s_ch[0];

	/* Keyframe boundary: capture encoder state BEFORE encoding this sample.
	 * A seeker loads the keyframe at sample k*interval and decodes k*interval
	 * .. target-1 to prime, so the keyframe must reflect decoder state at the
	 * START of the block. */
	bool     kf   = ((s_write_index & (s_keyframe_interval - 1u)) == 0u);
	uint32_t slot = kf ? kf_slot(s_write_index / s_keyframe_interval) : 0u;
	if (kf) {
		ch->keyframes[slot].predictor  = ch->enc.predictor;
		ch->keyframes[slot].step_index = ch->enc.step_index;
		ch->keyframes[slot]._pad       = 0;
		if (slot + 1u > ch->num_keyframes) ch->num_keyframes = slot + 1u;
	}

	/* Encode audio nybble, pack two per byte (low nybble first, high nybble
	 * second), write the finished byte STRAIGHT into the ring slot (spreads
	 * the ring write over 512 samples instead of a single 256-byte burst that
	 * stalls during core 1 flash writes). */
	tt_page_t *slotp = &s_page_ring[s_page_w & (TT_PAGE_RING_COUNT - 1u)];
	bool _filled = false;

	uint8_t nyb = adpcm_encode(sample, &ch->enc);
	if (!ch->nybble_phase) {
		ch->cur_byte = nyb;            /* low nybble first */
		ch->nybble_phase = true;
	} else {
		ch->cur_byte |= (uint8_t)(nyb << 4);  /* high nybble second */
		ch->nybble_phase = false;
		slotp->data[ch->fill++] = ch->cur_byte;
		if (ch->fill == TT_PAGE_SIZE) _filled = true;
	}

	/* Publish the full page: stamp its flash offset, advance the write head,
	 * bump w by 1 (mono: one slot per page). */
	if (_filled) {
		slotp->flash_off = ch->write_off;
		ch->write_off += TT_PAGE_SIZE;
		if (ch->write_off >= ch->audio_off + s_audio_bytes) ch->write_off = ch->audio_off;
		ch->fill = 0u;
		__dmb();
		s_page_w += 1u;
	}

	s_write_index++;
	return true;
}

void __not_in_flash_func(tt_stream_record_stop)(void)
{
	if (!s_rec_active) return;

	tt_audio_channel_t *ch = &s_ch[0];
	tt_page_t *slotp = &s_page_ring[s_page_w & (TT_PAGE_RING_COUNT - 1u)];

	/* Flush a dangling nybble (odd sample count) into a full byte. */
	if (ch->nybble_phase) {
		slotp->data[ch->fill++] = ch->cur_byte;
		ch->nybble_phase = false;
	}

	/* Pad + publish the partial page if there is any content. */
	if (ch->fill > 0u) {
		for (uint32_t i = ch->fill; i < TT_PAGE_SIZE; i++) slotp->data[i] = 0u;
		slotp->flash_off = ch->write_off;
		ch->write_off += TT_PAGE_SIZE;
		ch->fill = 0u;
		__dmb();
		s_page_w += 1u;
	}

	/* Readable loop length = samples written, capped at the buffer capacity. */
	s_recorded_samples = (s_write_index < s_capacity_samples)
	                   ? s_write_index : s_capacity_samples;
	s_rec_active = false;
	/* Ask core 1 to persist the header (sample_count + keyframes) to flash so
	 * the loop survives reboot. Core 1 flushes after draining the page ring. */
	s_header_dirty = true;
}

/* ------------------------------------------------------------------ */
/* Header persistence (core 1)                                         */
/* ------------------------------------------------------------------ */

/* Erase the header region so the magic word no longer validates. Called from
 * io_task at record_start, before audio erase-ahead begins. Blocks ~225ms
 * (9 sectors) but we're entering RECORD (live monitor passthrough), so no
 * playback to disrupt. Runs with interrupts masked on core 1; core 0 is fully
 * RAM-resident (copy_to_ram) so it never touches XIP during the erase. */
static void __not_in_flash_func(invalidate_header)(void)
{
	uint32_t ints = save_and_disable_interrupts();
	flash_range_erase(s_header_off, s_header_size);
	restore_interrupts(ints);
	s_header_invalidate = false;
}

/* Write the header struct + keyframe array to the header flash region so the
 * recording survives reboot. Called from io_task after the page ring is fully
 * drained (all audio is in flash). Blocks ~225ms: erase 9 sectors + program
 * ~32 KB of keyframes. During this time the playback heads can't be refilled,
 * so playback freezes briefly (hold-last) then resumes — acceptable at the
 * record->play handover. */
static void __not_in_flash_func(flush_header)(void)
{
	tt_audio_channel_t *ch = &s_ch[0];

	/* Page 0: the header struct, zero-padded to a full flash page. */
	uint8_t page0[TT_PAGE_SIZE];
	memset(page0, 0, TT_PAGE_SIZE);
	tt_stream_hdr_t hdr;
	hdr.magic            = TT_STREAM_MAGIC;
	hdr.version           = TT_STREAM_VERSION;
	hdr.sample_count      = s_recorded_samples;
	hdr.keyframe_interval = s_keyframe_interval;
	hdr.num_keyframes     = ch->num_keyframes;
	hdr.audio_off         = ch->audio_off;
	memcpy(page0, &hdr, sizeof(hdr));

	/* Keyframe block: TT_KEYFRAME_BUDGET entries, page-aligned (32 KB / 128
	 * pages). Programmed as one contiguous flash_range_program call. */
	uint32_t kf_bytes = TT_KEYFRAME_BUDGET * sizeof(tt_keyframe_t);
	uint32_t kf_padded = align_up(kf_bytes, TT_PAGE_SIZE);

	uint32_t ints = save_and_disable_interrupts();
	flash_range_erase(s_header_off, s_header_size);
	flash_range_program(s_header_off, page0, TT_PAGE_SIZE);
	flash_range_program(s_header_off + TT_PAGE_SIZE,
	                     (const uint8_t *)ch->keyframes, kf_padded);
	restore_interrupts(ints);

	s_header_dirty = false;
}

/* ------------------------------------------------------------------ */
/* Core 1 flash I/O                                                   */
/* ------------------------------------------------------------------ */

static void service_preview_request(void);

uint32_t tt_stream_io_task(void)
{
	uint32_t written = 0u;

	/* Top up the playback heads first. */
	head_refill(s_head[0]);
	head_refill(s_head[1]);

	/* Invalidate the old header at record_start (before audio erase-ahead so
	 * the header is already gone if power is lost during the recording). */
	if (s_rec_active && s_header_invalidate) {
		invalidate_header();
	}

	/* Keep the erase frontier ahead of the write head. These erases suspend to
	 * program pending pages (advancing flushed) and refill the heads, so the
	 * flushed frontier keeps advancing right through every erase.
	 * Gated on s_rec_active: the frontier is only valid once record_start has
	 * initialised it. Running before that would erase from flash offset 0
	 * (the firmware region), so this guard is essential. */
	if (s_rec_active) {
		ensure_erase_ahead_audio();
	}

	/* Program any pages not already drained during an erase-suspend above. Their
	 * sectors were pre-erased by the erase-ahead, so no inline erase is needed. */
	while (s_page_r != s_page_w) {
		uint32_t slot = s_page_r & (TT_PAGE_RING_COUNT - 1u);
		uint32_t off  = s_page_ring[slot].flash_off;

		flash_program_page(off, s_page_ring[slot].data);
		note_page_flushed(off);

		__dmb();
		s_page_r++;
		written++;
	}

	/* Persist the header after the page ring is fully drained (all audio in
	 * flash) so the written sample_count matches the readable audio. */
	if (!s_rec_active && s_header_dirty && s_page_r == s_page_w) {
		flush_header();
	}

	/* Keep the playback heads' decode windows filled. */
	head_refill(s_head[0]);
	head_refill(s_head[1]);

	/* Decode the PLAY loop-boundary crossfade previews + seeks off the audio
	 * path. */
	service_preview_request();

	return written;
}

bool tt_stream_io_idle(void)
{
	return s_page_r == s_page_w;
}

/* ------------------------------------------------------------------ */
/* Read-back (random access)                                          */
/* ------------------------------------------------------------------ */

/* Decode `count` PCM samples starting at absolute sample `start` into `out`.
 * Seeds the ADPCM decoder from the keyframe covering `start` and decodes
 * forward. Used by PLAY to pre-load the loop-start audio for the loop-boundary
 * overlap crossfade, and by seeks. Reads flash (XIP): only call when the flash
 * I/O is idle (no core-1 erase/program in flight). */
void tt_stream_decode_into(uint32_t start, uint32_t count, int16_t *out)
{
	if (out == NULL || count == 0u) return;
	if (s_recorded_samples == 0u) {
		for (uint32_t j = 0u; j < count; j++) out[j] = 0;
		return;
	}
	tt_audio_channel_t *ch = &s_ch[0];
	const uint8_t *base = xip_ptr(ch->audio_off);

	uint32_t k      = start / s_keyframe_interval;
	uint32_t kstart = k * s_keyframe_interval;
	adpcm_state_t st;
	st.predictor  = ch->keyframes[kf_slot(k)].predictor;
	st.step_index = ch->keyframes[kf_slot(k)].step_index;

	/* Prime the decoder from the keyframe up to `start`. */
	for (uint32_t i = kstart; i < start; i++) {
		uint8_t byte = base[audio_byte_wrap(i >> 1)];
		uint8_t nyb  = (i & 1u) ? (uint8_t)((byte >> 4) & 0x0Fu) : (uint8_t)(byte & 0x0Fu);
		(void)adpcm_decode(nyb, &st);
	}
	/* Emit `count` samples (clamp at the end of the recording). */
	int16_t last = 0;
	for (uint32_t j = 0u; j < count; j++) {
		uint32_t i = start + j;
		if (i >= s_recorded_samples) { out[j] = last; continue; }
		uint8_t byte = base[audio_byte_wrap(i >> 1)];
		uint8_t nyb  = (i & 1u) ? (uint8_t)((byte >> 4) & 0x0Fu) : (uint8_t)(byte & 0x0Fu);
		last = adpcm_decode(nyb, &st);
		out[j] = last;
	}
}

/* ---- Loop-boundary crossfade previews (decoded on core 1) ---------- */
/* ponytail: prev_end removed — Two Tracks has no reverse playback, so the
 * loop-end preview was decoded but never read. Add back if reverse is needed. */
static int16_t           s_prev_start[TT_PREVIEW_LEN];
static volatile uint32_t s_prev_loop_len;
static volatile uint32_t s_prev_request;   /* core 0 -> core 1: decode previews */
static volatile uint32_t s_prev_ready;     /* core 1 -> core 0: buffers valid   */

/* ---- Seek/cut preview (arbitrary target, decoded on core 1) -------- */
static int16_t           s_seek_buf[TT_PREVIEW_LEN];
static volatile uint32_t s_seek_target;
static volatile uint32_t s_seek_request;
static volatile uint32_t s_seek_ready;

void tt_stream_request_previews(uint32_t loop_len)
{
	s_prev_ready = 0u;
	__dmb();
	s_prev_loop_len = loop_len;
	s_prev_request  = 1u;
}

bool tt_stream_previews_ready(void)        { return s_prev_ready != 0u; }
const int16_t *tt_stream_preview_start(void) { return s_prev_start; }

void tt_stream_request_seek(uint32_t target)
{
	s_seek_ready = 0u;
	__dmb();
	s_seek_target = target;
	s_seek_request = 1u;
}

bool tt_stream_seek_ready(void)        { return s_seek_ready != 0u; }
const int16_t *tt_stream_seek_buf(void) { return s_seek_buf; }

/* Core 1: service a pending preview request by decoding the loop start and end
 * into the preview buffers, and any pending seek. Runs off the audio path, so
 * the decode cost is hidden by the playback heads' margin. Skipped while
 * recording (flash busy). */
static void __not_in_flash_func(service_preview_request)(void)
{
	if (s_rec_active) return;
	if (s_prev_request) {
		uint32_t L = s_prev_loop_len;
		if (L > TT_PREVIEW_LEN) {
			tt_stream_decode_into(0u, TT_PREVIEW_LEN, s_prev_start);
		}
		__dmb();
		s_prev_ready   = 1u;
		s_prev_request = 0u;
	}
	if (s_seek_request) {
		tt_stream_decode_into(s_seek_target, TT_PREVIEW_LEN, s_seek_buf);
		__dmb();
		s_seek_ready   = 1u;
		s_seek_request = 0u;
	}
}

/* ------------------------------------------------------------------ */
/* Core-1-refilled playback heads                                     */
/* ------------------------------------------------------------------ */

void tt_stream_head_init(tt_head_t *h, uint8_t channel)
{
	(void)channel; /* mono: always channel 0 */
	h->req_pos    = 0u;
	h->active     = false;
	h->lo         = 0u;
	h->hi         = 0u;
	h->last       = 0;
	h->channel    = 0;
	h->predictor  = 0;
	h->step_index = 0;
	h->fill_next  = 0u;
	h->fwd_valid  = false;
	h->need_seek  = true;
}

void tt_stream_set_heads(tt_head_t *hL, tt_head_t *hR)
{
	s_head[0] = hL;
	s_head[1] = hR;
}

int16_t __not_in_flash_func(tt_stream_head_read)(tt_head_t *h, uint32_t sample_index)
{
	if (s_recorded_samples == 0u) return 0;
	if (sample_index >= s_recorded_samples) sample_index = s_recorded_samples - 1u;

	h->req_pos = sample_index;
	h->active  = true;

	uint32_t lo = h->lo;
	uint32_t hi = h->hi;
	if (sample_index >= lo && sample_index < hi) {
		h->last = h->pcm[sample_index & TT_RING_MASK];
	}
	/* else: underrun — hold last good sample until core 1 catches up. */
	return h->last;
}

/* Core-1: keep one head's window covering its requested position, with margin
 * on BOTH sides so forward and reverse playback never outrun the decoded region.
 * Forward growth is an incremental decode; downward growth (for reverse)
 * decodes the previous keyframe block and prepends it. A full reseek only
 * happens on a large jump (e.g. loop wrap). Work per call is bounded. */
static void head_refill(tt_head_t *h)
{
	if (h == NULL || !h->active || s_recorded_samples == 0u) return;

	tt_audio_channel_t *ch = &s_ch[0];
	const uint32_t MARGIN = 1536u;   /* runway kept each side of the playhead */
	uint32_t pos  = h->req_pos;
	const uint8_t *base = xip_ptr(ch->audio_off);

	/* ponytail: forward-only — no backward refill. Goldfish supports reverse
	 * playback which needs margin below the playhead; Two Tracks doesn't.
	 * Setting want_lo = pos skips the backward branch entirely, saving up to
	 * 2048 samples of wasted decode per io_task call. Add want_lo = pos -
	 * MARGIN if reverse playback is needed. */
	uint32_t want_lo = pos;
	uint32_t want_hi = pos + MARGIN;
	if (want_hi > s_recorded_samples) want_hi = s_recorded_samples;

	/* Full reseek if the window is empty or no longer contains pos. */
	if (h->need_seek || h->hi <= h->lo || pos < h->lo || pos >= h->hi) {
		uint32_t k = want_lo / s_keyframe_interval;
		uint32_t kstart = k * s_keyframe_interval;

		h->predictor  = ch->keyframes[kf_slot(k)].predictor;
		h->step_index = ch->keyframes[kf_slot(k)].step_index;
		h->fill_next  = kstart;
		h->lo         = kstart;
		h->hi         = kstart;
		h->fwd_valid  = true;
		h->need_seek  = false;
	}

	/* Forward: extend hi up to want_hi. Re-prime the decoder first if a reverse
	 * drop invalidated it. */
	if (h->fill_next < want_hi) {
		if (!h->fwd_valid) {
			uint32_t k = h->fill_next / s_keyframe_interval;
			adpcm_state_t ps;
			ps.predictor  = ch->keyframes[kf_slot(k)].predictor;
			ps.step_index = ch->keyframes[kf_slot(k)].step_index;
			for (uint32_t i = k * s_keyframe_interval; i < h->fill_next; i++) {
				uint8_t byte = base[audio_byte_wrap(i >> 1)];
				uint8_t nyb  = (i & 1u) ? (uint8_t)((byte >> 4) & 0x0Fu)
				                        : (uint8_t)(byte & 0x0Fu);
				(void)adpcm_decode(nyb, &ps);
			}
			h->predictor  = ps.predictor;
			h->step_index = ps.step_index;
			h->fwd_valid  = true;
		}

		adpcm_state_t st;
		st.predictor  = h->predictor;
		st.step_index = h->step_index;
		uint32_t budget = 2048u;
		while (h->fill_next < want_hi && budget-- != 0u) {
			uint32_t idx = h->fill_next;
			uint8_t byte = base[audio_byte_wrap(idx >> 1)];
			uint8_t nyb  = (idx & 1u) ? (uint8_t)((byte >> 4) & 0x0Fu)
			                          : (uint8_t)(byte & 0x0Fu);
			h->pcm[idx & TT_RING_MASK] = adpcm_decode(nyb, &st);
			h->fill_next++;
			__dmb();
			h->hi = h->fill_next;
			if (h->hi - h->lo > TT_RING_SZ) h->lo = h->hi - TT_RING_SZ;
		}
		h->predictor  = st.predictor;
		h->step_index = st.step_index;
	}

	/* Backward: extend lo down to want_lo by decoding whole keyframe blocks. */
	uint32_t bbudget = 2048u;
	while (h->lo > want_lo && bbudget != 0u) {
		uint32_t span_hi = h->lo;
		uint32_t k = (span_hi - 1u) / s_keyframe_interval;
		uint32_t dstart = k * s_keyframe_interval;

		adpcm_state_t bst;
		bst.predictor  = ch->keyframes[kf_slot(k)].predictor;
		bst.step_index = ch->keyframes[kf_slot(k)].step_index;
		for (uint32_t i = dstart; i < span_hi; i++) {
			uint8_t byte = base[audio_byte_wrap(i >> 1)];
			uint8_t nyb  = (i & 1u) ? (uint8_t)((byte >> 4) & 0x0Fu)
			                        : (uint8_t)(byte & 0x0Fu);
			int16_t s = adpcm_decode(nyb, &bst);
			if (i >= want_lo) h->pcm[i & TT_RING_MASK] = s;
		}
		__dmb();
		h->lo = (dstart > want_lo) ? dstart : want_lo;
		if (h->hi - h->lo > TT_RING_SZ) {
			h->hi = h->lo + TT_RING_SZ;
			h->fill_next = h->hi;
			h->fwd_valid = false;   /* forward decoder no longer matches fill_next */
		}
		uint32_t did = span_hi - dstart;
		bbudget = (bbudget > did) ? (bbudget - did) : 0u;
	}
}

/* ------------------------------------------------------------------ */
/* Introspection                                                      */
/* ------------------------------------------------------------------ */

uint32_t tt_stream_flash_size(void)        { return s_flash_size; }
uint32_t tt_stream_keyframe_interval(void) { return s_keyframe_interval; }
uint32_t tt_stream_capacity_samples(void)  { return s_capacity_samples; }
uint32_t tt_stream_recorded_samples(void)  { return s_recorded_samples; }
uint32_t tt_stream_write_index(void)       { return s_write_index; }
uint32_t tt_stream_erase_count(void)       { return s_erase_count; }
float    tt_stream_capacity_seconds(void)  { return (float)s_capacity_samples / 48000.0f; }