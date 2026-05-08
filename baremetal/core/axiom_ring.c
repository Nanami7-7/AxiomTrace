#include "axiom_ring.h"
#include "axiom_port.h"
#include "axiom_crc.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>

#ifndef AXIOM_RING_BUFFER_POLICY_DROP
#define AXIOM_RING_BUFFER_POLICY_DROP 0
#endif

#ifndef AXIOM_RING_BUFFER_POLICY_OVERWRITE
#define AXIOM_RING_BUFFER_POLICY_OVERWRITE 1
#endif

#ifndef AXIOM_RING_BUFFER_POLICY
#define AXIOM_RING_BUFFER_POLICY AXIOM_RING_BUFFER_POLICY_DROP
#endif

/* Internal: retrieve the data buffer pointer stored in the reserved field.
 * Returns a writable pointer to the ring buffer's backing storage. */
static inline uint8_t *ring_buf(axiom_ring_t *ring) {
    return (uint8_t *)ring->reserved;
}

/* Helper: cast ring for const contexts (e.g. axiom_ring_peek) */
static inline const uint8_t *ring_buf_const(const axiom_ring_t *ring) {
    return (const uint8_t *)ring->reserved;
}

void axiom_ring_write_chunk(axiom_ring_t *ring, const uint8_t *data, uint16_t len, uint16_t *crc) {
    uint32_t head = ring->head;
    uint32_t mask = ring->mask;
    uint8_t *buf = ring_buf(ring);
    
    for (uint16_t i = 0; i < len; ++i) {
        uint8_t b = data[i];
        buf[head & mask] = b;
        head++;
        if (crc) {
            *crc = axiom_crc16_update(*crc, b);
        }
    }
    ring->head = head;
}

void axiom_ring_init(axiom_ring_t *ring, uint8_t *buf, uint32_t size) {
    /* 验证 size 是 2 的幂，否则 mask 计算会出错 */
    assert((size & (size - 1)) == 0 && "Ring buffer size must be power of 2");

    ring->head = 0;
    ring->tail = 0;
    ring->capacity = size;
    ring->mask = size - 1;
    ring->reserved = (uintptr_t)buf;
}

bool axiom_ring_write(axiom_ring_t *ring, const uint8_t *data, uint16_t len) {
    if (len == 0 || len > ring->capacity) {
        return false;
    }

    axiom_port_critical_enter();

    uint32_t head = ring->head;
    uint32_t tail = ring->tail;
    uint32_t cap  = ring->capacity;
    uint32_t mask = ring->mask;
    uint32_t used = head - tail;
    uint8_t *buf  = ring_buf(ring);

    if (used + len > cap) {
#if AXIOM_RING_BUFFER_POLICY == AXIOM_RING_BUFFER_POLICY_DROP
        axiom_port_critical_exit();
        return false;
#else
        /* OVERWRITE: blind advancement of tail to maintain O(1).
         * Decoder is responsible for handling partially overwritten frames.
         */
        tail = (head + len) - cap;
#endif
    }

    for (uint16_t i = 0; i < len; ++i) {
        buf[(head + i) & mask] = data[i];
    }
    ring->head = head + len;
    ring->tail = tail;

    axiom_port_critical_exit();
    return true;
}

uint16_t axiom_ring_read(axiom_ring_t *ring, uint8_t *out, uint16_t max_len) {
    if (!out || max_len == 0) return 0;

    axiom_port_critical_enter();

    uint32_t head = ring->head;
    uint32_t tail = ring->tail;
    uint32_t mask = ring->mask;
    uint32_t avail = head - tail;
    uint16_t n = (avail < max_len) ? (uint16_t)avail : max_len;
    uint8_t *buf = ring_buf(ring);

    for (uint16_t i = 0; i < n; ++i) {
        out[i] = buf[(tail + i) & mask];
    }
    ring->tail = tail + n;

    axiom_port_critical_exit();
    return n;
}

uint16_t axiom_ring_peek(const axiom_ring_t *ring, uint8_t *out, uint16_t max_len) {
    if (!out || max_len == 0) return 0;

    axiom_port_critical_enter();

    uint32_t head = ring->head;
    uint32_t tail = ring->tail;
    uint32_t mask = ring->mask;
    uint32_t avail = head - tail;
    uint16_t n = (avail < max_len) ? (uint16_t)avail : max_len;
    const uint8_t *buf = ring_buf_const(ring);

    for (uint16_t i = 0; i < n; ++i) {
        out[i] = buf[(tail + i) & mask];
    }

    axiom_port_critical_exit();
    return n;
}

void axiom_ring_consume(axiom_ring_t *ring, uint16_t n) {
    axiom_port_critical_enter();
    ring->tail += n;
    axiom_port_critical_exit();
}

void axiom_ring_reset(axiom_ring_t *ring) {
    axiom_port_critical_enter();
    ring->head = 0;
    ring->tail = 0;
    axiom_port_critical_exit();
}
