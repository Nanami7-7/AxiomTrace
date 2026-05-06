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

void axiom_write(axiom_level_t level, uint8_t module_id, uint16_t event_id,
                 const uint8_t *payload, uint8_t payload_len) {
    if (level >= AXIOM_LEVEL_MAX) return;

    if (!axiom_filter_check(&s_filter, level, module_id)) {
        axiom_filter_drop(&s_filter, module_id, event_id);
        return;
    }

    if (level == AXIOM_LEVEL_FAULT) {
        axiom_port_fault_hook(module_id, event_id, payload, payload_len);
    }

    axiom_port_critical_enter();

    /* Encode timestamp inside critical section — s_ts_ctx.last_raw
     * must not be corrupted by a pre-empting ISR calling axiom_write() */
    uint8_t ts_buf[5];
    uint8_t ts_len = axiom_timestamp_encode(&s_ts_ctx, ts_buf);
    uint16_t total_len = (uint16_t)(8u + ts_len + 1u + payload_len + 2u);

    /* Capacity check & Policy enforcement */
    uint32_t head = s_ring.head;
    uint32_t tail = s_ring.tail;
    uint32_t cap  = s_ring.capacity;
    uint32_t used = head - tail;

    if (used + total_len > cap) {
#if AXIOM_RING_BUFFER_POLICY == AXIOM_RING_BUFFER_POLICY_DROP
        axiom_port_critical_exit();
        return;
#else
        /* OVERWRITE: blind advancement of tail */
        s_ring.tail = (head + total_len) - cap;
#endif
    }

    uint16_t crc = 0xFFFFu;
    uint8_t header[8];
    header[0] = 0xA5;
    header[1] = AXIOM_WIRE_VERSION;
    header[2] = (uint8_t)level;
    header[3] = module_id;
    header[4] = (uint8_t)(event_id & 0xFFu);
    header[5] = (uint8_t)(event_id >> 8);
    header[6] = (uint8_t)(s_seq & 0xFFu);
    header[7] = (uint8_t)(s_seq >> 8);
    s_seq++;

    /* Phase 1: Header */
    axiom_ring_write_chunk(&s_ring, header, 8, &crc);
    /* Phase 2: Timestamp */
    axiom_ring_write_chunk(&s_ring, ts_buf, ts_len, &crc);
    /* Phase 3: Payload Length */
    axiom_ring_write_chunk(&s_ring, &payload_len, 1, &crc);
    /* Phase 4: Payload */
    if (payload_len > 0 && payload) {
        axiom_ring_write_chunk(&s_ring, payload, payload_len, &crc);
    }
    /* Phase 5: Final CRC */
    uint8_t crc_buf[2];
    crc_buf[0] = (uint8_t)(crc & 0xFFu);
    crc_buf[1] = (uint8_t)(crc >> 8);
    axiom_ring_write_chunk(&s_ring, crc_buf, 2, NULL);

    /* Snapshot drop statistics atomically before leaving the critical section.
     * Prevents a pre-empting ISR from corrupting drop_count mid-read. */
    const bool     has_drop    = s_filter.drop_pending;
    uint32_t       cached_lost = 0;
    uint8_t        cached_mod  = 0;
    uint16_t       cached_evt  = 0;
    if (has_drop) {
        cached_lost = s_filter.drop_count;
        cached_mod  = s_filter.drop_module;
        cached_evt  = s_filter.drop_event;
        /* Clear atomically — next drop will start fresh */
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
        axiom_write(AXIOM_LEVEL_WARN, 0x00, 0x0001, summary, sp);
    }

    /* NOTE: axiom_filter_drop() above is called outside critical section.
     * On 32-bit MCUs, drop_count++ is typically atomic (single LDR+STR),
     * but under extreme contention a count increment may be lost.
     * This is acceptable for statistics — no data corruption occurs. */
}

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