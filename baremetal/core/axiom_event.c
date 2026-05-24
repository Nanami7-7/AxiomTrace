#include "axiom_event.h"
#include "axiom_encode.h"
#include "axiom_crc.h"
#include "axiom_ring.h"
#include "axiom_filter.h"
#include "axiom_timestamp.h"
#include "axiom_backend.h"
#include "axiom_frontend.h"
#include "axiom_port.h"
#include <string.h>

#ifndef AXIOM_RING_BUFFER_SIZE
#define AXIOM_RING_BUFFER_SIZE 4096u
#endif

/* Compile-time sanity checks — must come AFTER all default overrides */
_Static_assert(AXIOM_MAX_PAYLOAD_LEN <= 255,
    "AXIOM_MAX_PAYLOAD_LEN must not exceed 255 (uint8_t limit)");
_Static_assert((AXIOM_RING_BUFFER_SIZE & (AXIOM_RING_BUFFER_SIZE - 1u)) == 0,
    "AXIOM_RING_BUFFER_SIZE must be a power of two");

/* ---------------------------------------------------------------------------
 * Encode overflow flag — set by axiom_enc_xxx() when payload exceeds limit.
 * Consumed and cleared by axiom_write() after each event.
 * --------------------------------------------------------------------------- */
#if AXIOM_ENCODE_OVERFLOW_DETECTION
volatile bool axiom_encode_overflow = false;
#endif

/* ---------------------------------------------------------------------------
 * Double-buffer: pre-encode into local buffer, then single ring_write.
 * This minimizes the critical section to a single memcpy+ring_write,
 * drastically reducing interrupt latency.
 *
 * Trade-off: +AXIOM_MAX_FRAME_LEN bytes of stack/RAM per axiom_write()
 * call site. Disabled via AXIOM_SHORT_CS=0 if RAM is critical.
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_SHORT_CS
#define AXIOM_SHORT_CS 1
#endif

static uint8_t s_ring_buf[AXIOM_RING_BUFFER_SIZE];
static axiom_ring_t s_ring;
static uint16_t s_seq;
static axiom_filter_t s_filter;
static axiom_timestamp_ctx_t s_ts_ctx;

void axiom_init(void) {
    axiom_ring_init(&s_ring, s_ring_buf, AXIOM_RING_BUFFER_SIZE);
    s_seq = 0;
    axiom_filter_init(&s_filter);
    axiom_timestamp_init(&s_ts_ctx);
}

void axiom_flush(void) {
    uint8_t frame[AXIOM_MAX_FRAME_LEN];
    uint16_t n;
    while ((n = axiom_ring_peek(&s_ring, frame, sizeof(frame))) > 0) {
        /* Frame structure: Header(8B) + Timestamp(1-5B) + PayloadLen(1B) + Payload(N) + CRC(2B) */
        /* Minimum frame: header(8) + timestamp(1) + payload_len(1) + crc(2) = 12 */
        if (n < 12) break;
        /* Decode variable-length timestamp to find payload_len offset */
        uint8_t ts_len = axiom_timestamp_decode_len(frame[8]);
        /* Determine actual frame length */
        uint16_t payload_len = frame[8 + ts_len];
        uint16_t frame_len = (uint16_t)(8u + ts_len + 1u + payload_len + 2u);
        if (n < frame_len) break;
        /* Data already copied by peek — just advance the tail pointer */
        axiom_ring_consume(&s_ring, frame_len);
        axiom_backend_dispatch(frame, frame_len);
    }
}

/* ---------------------------------------------------------------------------
 * axiom_write() — core event emission path
 *
 * AXIOM_SHORT_CS (default=1): pre-encode frame into a local buffer
 * OUTSIDE the critical section, then perform a single ring_write
 * INSIDE the critical section. This minimizes interrupt latency.
 *
 * Frame layout in local_buf:
 *   [Header:8B] [Timestamp:1-5B] [PayloadLen:1B] [Payload:NB] [CRC:2B]
 *
 * CRC covers everything (header + timestamp + payload_len + payload),
 * matching the original per-phase write path's CRC scope.
 *
 * AXIOM_SHORT_CS=0: falls back to the original per-phase write pattern
 * for MCUs where stack usage is critical (saves AXIOM_MAX_FRAME_LEN bytes).
 * --------------------------------------------------------------------------- */
#if AXIOM_SHORT_CS

void axiom_write(axiom_level_t level, uint8_t module_id, uint16_t event_id,
                 const uint8_t *payload, uint8_t payload_len) {
    if (level >= AXIOM_LEVEL_MAX) return;

    if (!axiom_filter_check(&s_filter, level, module_id)) {
        /* CS protects drop_count read-modify-write from concurrent ISR races */
        axiom_port_critical_enter();
        axiom_filter_drop(&s_filter, module_id, event_id);
        axiom_port_critical_exit();
        return;
    }

    if (level == AXIOM_LEVEL_FAULT) {
        axiom_port_fault_hook(module_id, event_id, payload, payload_len);
    }

    /* Pre-encode everything into a local buffer (no lock needed yet). */
    uint8_t local_buf[AXIOM_MAX_FRAME_LEN];
    uint16_t pos = 0;

    /* Build header — level, module_id, event_id are read-only during CS.
     * s_seq is assigned inside the critical section to prevent duplicates
     * when two ISRs call axiom_write() concurrently. */
    local_buf[pos++] = AXIOM_SYNC_BYTE;
    local_buf[pos++] = AXIOM_WIRE_VERSION;
    local_buf[pos++] = (uint8_t)level;
    local_buf[pos++] = module_id;
    local_buf[pos++] = (uint8_t)(event_id & 0xFFu);
    local_buf[pos++] = (uint8_t)(event_id >> 8);
    /* seq[6..7] reserved — filled inside critical section */
    pos += 2u;

    /* Minimal critical section: s_seq + s_ts_ctx + s_ring are protected. */
    axiom_port_critical_enter();

    /* Assign sequence number — must be inside CS to avoid duplicate seq on ISR contention */
    local_buf[6] = (uint8_t)(s_seq & 0xFFu);
    local_buf[7] = (uint8_t)(s_seq >> 8);
    s_seq++;

    /* Timestamp encoding — must be inside CS to protect s_ts_ctx */
    uint8_t ts_len = axiom_timestamp_encode(&s_ts_ctx, local_buf + pos);
    pos += ts_len;

    /* Capacity check & policy enforcement */
    uint32_t head = s_ring.head;
    uint32_t tail = s_ring.tail;
    uint32_t cap  = s_ring.capacity;
    uint32_t used = head - tail;
    /* frame_len = header(8) + ts + payload_len(1) + payload + CRC(2) */
    uint16_t frame_len = (uint16_t)(pos + 1u + payload_len + 2u);

    if (used + frame_len > cap) {
#if AXIOM_RING_BUFFER_POLICY == AXIOM_RING_BUFFER_POLICY_DROP
        axiom_port_critical_exit();
        return;
#else
        s_ring.tail = (head + frame_len) - cap;
#endif
    }

    /* Payload length byte */
    local_buf[pos++] = payload_len;

    /* Append payload data to local_buf */
    if (payload_len > 0 && payload) {
        memcpy(local_buf + pos, payload, payload_len);
        pos += payload_len;
    }

    /* CRC covers the entire frame so far (header + timestamp + payload_len + payload).
     * This matches the original per-phase path where CRC covers all bytes. */
    uint16_t crc = axiom_crc16(local_buf, pos);

    /* Final CRC (little-endian) */
    local_buf[pos++] = (uint8_t)(crc & 0xFFu);
    local_buf[pos++] = (uint8_t)(crc >> 8);

    /* Single atomic write to ring — lock held for minimum time */
    axiom_ring_write_chunk(&s_ring, local_buf, pos, NULL);

    /* Snapshot drop statistics atomically before leaving CS */
    const bool has_drop = s_filter.drop_pending;
    uint32_t cached_lost = 0;
    uint8_t  cached_mod  = 0;
    uint16_t cached_evt  = 0;
    if (has_drop) {
        cached_lost = s_filter.drop_count;
        cached_mod  = s_filter.drop_module;
        cached_evt  = s_filter.drop_event;
        s_filter.drop_count   = 0;
        s_filter.drop_pending = false;
        s_filter.drop_module  = 0;
        s_filter.drop_event   = 0;
    }

    axiom_port_critical_exit();

    /* Emit DROP_SUMMARY outside critical section.
     * Recursive axiom_write() is safe: it has its own critical section.
     * Using cached_* locals avoids touching s_filter after CS exit. */
    if (has_drop) {
        uint8_t summary[10];
        uint8_t sp = 0;
        summary[sp++] = AXIOM_TYPE_U32;
        summary[sp++] = (uint8_t)(cached_lost & 0xFFu);
        summary[sp++] = (uint8_t)((cached_lost >> 8) & 0xFFu);
        summary[sp++] = (uint8_t)((cached_lost >> 16) & 0xFFu);
        summary[sp++] = (uint8_t)(cached_lost >> 24);
        summary[sp++] = AXIOM_TYPE_U8;
        summary[sp++] = cached_mod;
        summary[sp++] = AXIOM_TYPE_U16;
        summary[sp++] = (uint8_t)(cached_evt & 0xFFu);
        summary[sp++] = (uint8_t)(cached_evt >> 8);
        axiom_write(AXIOM_LEVEL_WARN, AXIOM_SYSTEM_MODULE_ID, AXIOM_SYSTEM_EVENT_DROP_SUMMARY, summary, sp);
    }
}

#else /* AXIOM_SHORT_CS == 0 — original per-phase write path */

void axiom_write(axiom_level_t level, uint8_t module_id, uint16_t event_id,
                 const uint8_t *payload, uint8_t payload_len) {
    if (level >= AXIOM_LEVEL_MAX) return;

    if (!axiom_filter_check(&s_filter, level, module_id)) {
        /* CS protects drop_count read-modify-write from concurrent ISR races */
        axiom_port_critical_enter();
        axiom_filter_drop(&s_filter, module_id, event_id);
        axiom_port_critical_exit();
        return;
    }

    if (level == AXIOM_LEVEL_FAULT) {
        axiom_port_fault_hook(module_id, event_id, payload, payload_len);
    }

    axiom_port_critical_enter();

    uint8_t ts_buf[5];
    uint8_t ts_len = axiom_timestamp_encode(&s_ts_ctx, ts_buf);
    uint16_t total_len = (uint16_t)(8u + ts_len + 1u + payload_len + 2u);

    uint32_t head = s_ring.head;
    uint32_t tail = s_ring.tail;
    uint32_t cap  = s_ring.capacity;
    uint32_t used = head - tail;

    if (used + total_len > cap) {
#if AXIOM_RING_BUFFER_POLICY == AXIOM_RING_BUFFER_POLICY_DROP
        axiom_port_critical_exit();
        return;
#else
        s_ring.tail = (head + total_len) - cap;
#endif
    }

    uint16_t crc = 0xFFFFu;
    uint8_t header[8];
    header[0] = AXIOM_SYNC_BYTE;
    header[1] = AXIOM_WIRE_VERSION;
    header[2] = (uint8_t)level;
    header[3] = module_id;
    header[4] = (uint8_t)(event_id & 0xFFu);
    header[5] = (uint8_t)(event_id >> 8);
    header[6] = (uint8_t)(s_seq & 0xFFu);
    header[7] = (uint8_t)(s_seq >> 8);
    s_seq++;

    axiom_ring_write_chunk(&s_ring, header, 8, &crc);
    axiom_ring_write_chunk(&s_ring, ts_buf, ts_len, &crc);
    axiom_ring_write_chunk(&s_ring, &payload_len, 1, &crc);
    if (payload_len > 0 && payload) {
        axiom_ring_write_chunk(&s_ring, payload, payload_len, &crc);
    }
    uint8_t crc_buf[2];
    crc_buf[0] = (uint8_t)(crc & 0xFFu);
    crc_buf[1] = (uint8_t)(crc >> 8);
    axiom_ring_write_chunk(&s_ring, crc_buf, 2, NULL);

    const bool     has_drop    = s_filter.drop_pending;
    uint32_t       cached_lost = 0;
    uint8_t        cached_mod  = 0;
    uint16_t       cached_evt  = 0;
    if (has_drop) {
        cached_lost = s_filter.drop_count;
        cached_mod  = s_filter.drop_module;
        cached_evt  = s_filter.drop_event;
        s_filter.drop_count   = 0;
        s_filter.drop_pending = false;
        s_filter.drop_module  = 0;
        s_filter.drop_event   = 0;
    }

    axiom_port_critical_exit();

    if (has_drop) {
        uint8_t summary[10];
        uint8_t sp = 0;
        summary[sp++] = AXIOM_TYPE_U32;
        summary[sp++] = (uint8_t)(cached_lost & 0xFFu);
        summary[sp++] = (uint8_t)((cached_lost >> 8) & 0xFFu);
        summary[sp++] = (uint8_t)((cached_lost >> 16) & 0xFFu);
        summary[sp++] = (uint8_t)(cached_lost >> 24);
        summary[sp++] = AXIOM_TYPE_U8;
        summary[sp++] = cached_mod;
        summary[sp++] = AXIOM_TYPE_U16;
        summary[sp++] = (uint8_t)(cached_evt & 0xFFu);
        summary[sp++] = (uint8_t)(cached_evt >> 8);
        axiom_write(AXIOM_LEVEL_WARN, AXIOM_SYSTEM_MODULE_ID, AXIOM_SYSTEM_EVENT_DROP_SUMMARY, summary, sp);
    }
}

#endif /* AXIOM_SHORT_CS */

/* ---------------------------------------------------------------------------
 * Runtime filter control API — operates on the global s_filter instance
 * --------------------------------------------------------------------------- */
void axiom_level_mask_set(uint32_t mask) {
    s_filter.level_mask = mask;
}

uint32_t axiom_level_mask_get(void) {
    return s_filter.level_mask;
}

void axiom_module_mask_set(uint32_t mask) {
    s_filter.module_mask = mask;
}

uint32_t axiom_module_mask_get(void) {
    return s_filter.module_mask;
}
