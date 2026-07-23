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
    return (uint8_t *)ring->storage;
}

/* Helper: cast ring for const contexts (e.g. axiom_ring_peek) */
static inline const uint8_t *ring_buf_const(const axiom_ring_t *ring) {
    return (const uint8_t *)ring->storage;
}

void axiom_ring_write_chunk(axiom_ring_t *ring, const uint8_t *data, uint16_t len, uint16_t *crc) {
    uint32_t head = ring->head;
    uint32_t mask = ring->mask;
    uint8_t *buf = ring_buf(ring);

    if (crc) {
        /* CRC 路径：逐字节写入，同步更新 CRC */
        for (uint16_t i = 0; i < len; ++i) {
            uint8_t b = data[i];
            buf[head & mask] = b;
            head++;
            *crc = axiom_crc16_update(*crc, b);
        }
    } else {
        /* 无 CRC 路径：使用 memcpy 加速连续区域写入 */
        uint32_t idx = head & mask;
        uint32_t first = ring->capacity - idx;
        if (first >= len) {
            memcpy(buf + idx, data, len);
        } else {
            memcpy(buf + idx, data, first);
            memcpy(buf, data + first, len - first);
        }
        head += len;
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
    ring->storage = (uintptr_t)buf;
}

static bool ring_write_internal(axiom_ring_t *ring, const uint8_t *data, uint16_t len,
                                bool allow_overwrite) {
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
        if (!allow_overwrite) {
            axiom_port_critical_exit();
            return false;
        }
        tail = (head + len) - cap;
    }

    /* memcpy 优化：分段拷贝避免逐字节循环开销 */
    uint32_t idx = head & mask;
    uint32_t first = cap - idx;
    if (first >= len) {
        memcpy(buf + idx, data, len);
    } else {
        memcpy(buf + idx, data, first);
        memcpy(buf, data + first, len - first);
    }
    ring->head = head + len;
    ring->tail = tail;

    axiom_port_critical_exit();
    return true;
}

bool axiom_ring_write(axiom_ring_t *ring, const uint8_t *data, uint16_t len) {
#if AXIOM_RING_BUFFER_POLICY == AXIOM_RING_BUFFER_POLICY_OVERWRITE
    return ring_write_internal(ring, data, len, true);
#else
    return ring_write_internal(ring, data, len, false);
#endif
}

bool axiom_ring_try_write(axiom_ring_t *ring, const uint8_t *data, uint16_t len) {
    return ring_write_internal(ring, data, len, false);
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

    /* memcpy 优化：分段拷贝避免逐字节循环开销 */
    uint32_t idx = tail & mask;
    uint32_t first = ring->capacity - idx;
    if (first >= n) {
        memcpy(out, buf + idx, n);
    } else {
        memcpy(out, buf + idx, first);
        memcpy(out + first, buf, n - first);
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

    /* memcpy 优化：分段拷贝避免逐字节循环开销 */
    uint32_t idx = tail & mask;
    uint32_t first = ring->capacity - idx;
    if (first >= n) {
        memcpy(out, buf + idx, n);
    } else {
        memcpy(out, buf + idx, first);
        memcpy(out + first, buf, n - first);
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
