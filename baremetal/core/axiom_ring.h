#ifndef AXIOM_RING_H
#define AXIOM_RING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef AXIOM_RING_BUFFER_POLICY_DROP
#define AXIOM_RING_BUFFER_POLICY_DROP 0
#endif

#ifndef AXIOM_RING_BUFFER_POLICY_OVERWRITE
#define AXIOM_RING_BUFFER_POLICY_OVERWRITE 1
#endif

#ifndef AXIOM_RING_BUFFER_POLICY
#define AXIOM_RING_BUFFER_POLICY AXIOM_RING_BUFFER_POLICY_DROP
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * IRQ-safe SPSC RAM Ring Buffer with critical-section protection.
 * Single-producer (ISR or main loop), single-consumer (backend drain).
 * --------------------------------------------------------------------------- */

typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    uint32_t capacity;
    uint32_t mask;
    uintptr_t storage;
} axiom_ring_t;

/* Initialize ring buffer in the provided memory block. 
 * 'size' must be a power of two. */
void axiom_ring_init(axiom_ring_t *ring, uint8_t *buf, uint32_t size);

/* Write data to ring. Returns true on success, false if dropped. */
bool axiom_ring_write(axiom_ring_t *ring, const uint8_t *data, uint16_t len);

/* Write a chunk to the ring and optionally update CRC.
 * Internal use only, requires caller to handle critical section and capacity check. */
void axiom_ring_write_chunk(axiom_ring_t *ring, const uint8_t *data, uint16_t len, uint16_t *crc);

/* Read up to max_len bytes from ring. Returns bytes read. */
uint16_t axiom_ring_read(axiom_ring_t *ring, uint8_t *out, uint16_t max_len);

/* Peek at data without consuming. Returns bytes available. */
uint16_t axiom_ring_peek(const axiom_ring_t *ring, uint8_t *out, uint16_t max_len);

/* Advance tail by 'n' bytes without copying data.
 * Caller must have already read the data (e.g., via axiom_ring_peek).
 * Caller must ensure n <= axiom_ring_used(). */
void axiom_ring_consume(axiom_ring_t *ring, uint16_t n);

/* Bytes currently stored */
static inline uint32_t axiom_ring_used(const axiom_ring_t *ring) {
    return ring->head - ring->tail;
}

/* Free space */
static inline uint32_t axiom_ring_free(const axiom_ring_t *ring) {
    return ring->capacity - (ring->head - ring->tail);
}

/* Reset ring to empty */
void axiom_ring_reset(axiom_ring_t *ring);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_RING_H */
