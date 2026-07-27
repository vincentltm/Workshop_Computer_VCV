/**
 * two_tracks_stream.h — flash-backed mono audio streaming store for Two Tracks.
 *
 * Forked from Goldfish 2.0 (releases/11_goldfish/goldfish_stream.h) by Dune
 * Desormeaux, adapted for the Two Tracks dual-read-head phase looper:
 *   - Mono (1 audio channel; both read heads read the same loop).
 *   - No CV stream (CV outputs show live head position, not recorded CV).
 *   - No DELAY mode (Two Tracks is PLAY/ARMED/RECORD only).
 *
 *   - Audio is IMA-ADPCM (4 bits/sample, ~4:1) written to a wear-levelled
 *     region of the program card's flash. Audio gets the full remaining flash
 *     budget (no 2:2:1 split with a CV stream).
 *   - Keyframes (encoder state snapshots) are captured every
 *     tt_stream_keyframe_interval() samples so any audio position can be reached
 *     by seeking to the nearest keyframe and decoding forward. The interval
 *     scales with card size so the in-RAM index stays roughly constant
 *     (~TT_KEYFRAME_BUDGET entries) regardless of 2 MB vs 16 MB flash.
 *
 * Threading model:
 *   - Core 0 (audio): tt_stream_record_sample() — encode + enqueue.
 *   - Core 1 (flash I/O): tt_stream_io_task() — drain page ring to flash with
 *     sector erase-ahead, refill playback head rings, service previews/seeks.
 * Core 0's audio path must be fully RAM-resident so it never stalls on XIP
 * while core 1 is mid-erase.
 */

#ifndef TWO_TRACKS_STREAM_H
#define TWO_TRACKS_STREAM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Compile-time configuration                                         */
/* ------------------------------------------------------------------ */

/* Flash reserved for firmware at the bottom of the chip. Generous; the audio
 * region begins after this. Must be a multiple of the 4 KB sector size. */
#ifndef TT_FIRMWARE_RESERVE
#define TT_FIRMWARE_RESERVE (384u * 1024u)
#endif

/* Target maximum number of keyframe entries kept in RAM. The keyframe interval
 * is chosen at init so the actual count stays at or below this, bounding the
 * RAM index to ~TT_KEYFRAME_BUDGET * 4 bytes (~32 KB at 8192). */
#ifndef TT_KEYFRAME_BUDGET
#define TT_KEYFRAME_BUDGET 8192u
#endif

/* Number of audio channels stored to flash. Two Tracks is mono (1 channel);
 * both read heads read the same loop. */
#define TT_AUDIO_CHANNELS 1u

/* Flash program page size (bytes) staged before hand-off to core 1. */
#define TT_PAGE_SIZE 256u

/* Number of staged pages in the core0->core1 ring. Sized to absorb worst-case
 * erase latency without the producer overrunning the consumer. */
#ifndef TT_PAGE_RING_COUNT
#define TT_PAGE_RING_COUNT 32u
#endif

/* Sectors the erase frontier leads the write head. These sectors are
 * pre-erased so pending pages always land in erased flash. */
#ifndef TT_ERASE_LOOKAHEAD
#define TT_ERASE_LOOKAHEAD 2u
#endif

#define TT_STREAM_MAGIC   0x54325453u /* 'T2TS' */
#define TT_STREAM_VERSION  1u

/* Flash layout of the header region (at TT_FIRMWARE_RESERVE):
 *   page 0:           tt_stream_hdr_t struct (zero-padded to 256 bytes)
 *   pages 1..N:       keyframe array (TT_KEYFRAME_BUDGET * sizeof(tt_keyframe_t),
 *                     page-aligned, written/read as a contiguous block).
 * The header is invalidated (erased) at record_start so a power loss during
 * recording never leaves a stale header referencing partially-overwritten audio. */

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * Detect flash size, compute the flash partition, and choose the keyframe
 * interval. Does not erase or write anything. Call once at boot (single-core).
 */
void tt_stream_init(void);

/** Begin a fresh recording (clears the current loop). Resets write cursor,
 *  encoder state and keyframe index. */
void tt_stream_record_start(void);

/**
 * Record one mono audio frame. Called from core 0 at the audio rate.
 *  sample: -32768..32767 audio sample (16-bit for ADPCM quality; pass
 *          AudioIn1() << 4 to scale the 12-bit DAC input up).
 * Returns false once the recording region is full.
 */
bool tt_stream_record_sample(int16_t sample);

/** Stop recording: flush partial encoder byte and partial pages. */
void tt_stream_record_stop(void);

/**
 * Core 1 service routine: drains staged pages to flash with erase-ahead,
 * refills playback head rings, decodes previews/seeks. Call frequently from
 * the core 1 loop. Returns the number of pages written.
 */
uint32_t tt_stream_io_task(void);

/** True once all staged pages have been written to flash by core 1. */
bool tt_stream_io_idle(void);

/* ---- Read-back (random access) ---- */

/** Decode `count` PCM samples from absolute sample `start` into `out`. Reads
 *  flash directly — only call when tt_stream_io_idle(). */
void tt_stream_decode_into(uint32_t start, uint32_t count, int16_t *out);

/* ---- Loop-boundary crossfade previews (decoded on core 1) ----
 * Length of the pre-decoded loop-start preview buffer, matched by the PLAY
 * crossfade window in main.cpp. */
#define TT_PREVIEW_LEN 388u

/** Core 0: ask core 1 to (re)decode the loop-start preview for a loop of
 *  `loop_len` samples. Clears the ready flag; poll tt_stream_previews_ready. */
void tt_stream_request_previews(uint32_t loop_len);

/** True once core 1 has filled the preview buffer for the last request. */
bool tt_stream_previews_ready(void);

/** Pointer to the pre-decoded loop-start PCM (TT_PREVIEW_LEN samples).
 *  Valid only while tt_stream_previews_ready() is true. */
const int16_t *tt_stream_preview_start(void);

/** Core 0: ask core 1 to decode a seek/cut target into the seek buffer.
 *  Clears the ready flag; poll tt_stream_seek_ready. */
void tt_stream_request_seek(uint32_t target);

/** True once core 1 has filled the seek buffer for the last seek request. */
bool tt_stream_seek_ready(void);

/** Pointer to the pre-decoded seek-target PCM (TT_PREVIEW_LEN samples). */
const int16_t *tt_stream_seek_buf(void);

/* ---- Core-1-refilled playback head ----
 *
 * A head is a decoded-PCM window kept filled by core 1 so that core 0 can read
 * any recently-visited sample in O(1) with no decode work on the audio thread.
 * Core 0 publishes the position it wants (req_pos); core 1 slides/refills the
 * window to keep it covered, seeking from a keyframe on large/backward jumps.
 * Both heads read channel 0 (the mono loop) but track independent positions. */

#define TT_RING_BITS 12u
#define TT_RING_SZ   (1u << TT_RING_BITS) /* 4096 samples, 8 KB */
#define TT_RING_MASK (TT_RING_SZ - 1u)

typedef struct {
	int16_t           pcm[TT_RING_SZ]; /* decoded window, indexed by idx&MASK */
	volatile uint32_t req_pos;   /* core 0: sample index it is reading */
	volatile bool     active;    /* core 0: head in use this block */
	volatile uint32_t lo, hi;    /* core 1: valid window [lo, hi) */
	int16_t           last;      /* core 0: last good sample (underrun hold) */
	uint8_t           channel;   /* which audio channel (always 0 for mono) */
	/* core 1 private forward-decode state */
	int16_t           predictor;
	int8_t            step_index;
	uint32_t          fill_next; /* next sample index core 1 will decode forward */
	bool              fwd_valid; /* forward decoder state matches fill_next */
	bool              need_seek; /* core 1 must re-seek before filling */
} tt_head_t;

/** Reset a playback head (core 0). Channel must be 0 (mono). */
void tt_stream_head_init(tt_head_t *h, uint8_t channel);

/** Read the decoded sample at sample_index from the head's ring (core 0). */
int16_t tt_stream_head_read(tt_head_t *h, uint32_t sample_index);

/* Register the heads that core 1 should keep refilled. Pass NULL to disable a
 * slot. Call when entering a playback mode. */
void tt_stream_set_heads(tt_head_t *hL, tt_head_t *hR);

/* ---- Introspection (geometry) ---- */

uint32_t tt_stream_flash_size(void);        /* detected total flash bytes */
uint32_t tt_stream_keyframe_interval(void); /* samples between keyframes  */
uint32_t tt_stream_capacity_samples(void);  /* max recordable audio samples */
uint32_t tt_stream_recorded_samples(void);  /* length of current recording */
uint32_t tt_stream_write_index(void);       /* monotonic samples written */
uint32_t tt_stream_erase_count(void);       /* sectors erased since boot   */
float    tt_stream_capacity_seconds(void);  /* capacity_samples / 48000    */

#ifdef __cplusplus
}
#endif

#endif /* TWO_TRACKS_STREAM_H */