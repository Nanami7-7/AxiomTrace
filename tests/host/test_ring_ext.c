/* test_ring_ext.c — extended tests for axiom_ring.c */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "axiom_ring.h"

#define RING_SIZE 16u  /* power of 2 */
static uint8_t ring_mem[RING_SIZE];

static int failures = 0;

#define CHECK(msg, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        failures++; \
    } \
} while(0)

/* ---- Test 1: axiom_ring_init ---- */
static void test_init(void) {
    axiom_ring_t ring;
    axiom_ring_init(&ring, ring_mem, RING_SIZE);
    CHECK("init: head == 0", ring.head == 0);
    CHECK("init: tail == 0", ring.tail == 0);
    CHECK("init: capacity == RING_SIZE", ring.capacity == RING_SIZE);
    CHECK("init: mask == RING_SIZE-1", ring.mask == RING_SIZE - 1);
    CHECK("init: used == 0", axiom_ring_used(&ring) == 0);
    CHECK("init: free == RING_SIZE", axiom_ring_free(&ring) == RING_SIZE);
}

/* ---- Test 2: axiom_ring_free basic ---- */
static void test_free(void) {
    axiom_ring_t ring;
    axiom_ring_init(&ring, ring_mem, RING_SIZE);
    CHECK("free: initially RING_SIZE", axiom_ring_free(&ring) == RING_SIZE);

    uint8_t data[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
    axiom_ring_write(&ring, data, 8);
    CHECK("free: after write 8", axiom_ring_free(&ring) == RING_SIZE - 8);

    uint8_t out[4];
    axiom_ring_read(&ring, out, 4);
    CHECK("free: after read 4", axiom_ring_free(&ring) == RING_SIZE - 4);
}

/* ---- Test 3: axiom_ring_reset ---- */
static void test_reset(void) {
    axiom_ring_t ring;
    axiom_ring_init(&ring, ring_mem, RING_SIZE);

    uint8_t data[8] = {1,2,3,4,5,6,7,8};
    axiom_ring_write(&ring, data, 8);
    CHECK("reset: before reset used==8", axiom_ring_used(&ring) == 8);

    axiom_ring_reset(&ring);
    CHECK("reset: after reset used==0", axiom_ring_used(&ring) == 0);
    CHECK("reset: after reset free==RING_SIZE", axiom_ring_free(&ring) == RING_SIZE);
    CHECK("reset: head==0", ring.head == 0);
    CHECK("reset: tail==0", ring.tail == 0);
}

/* ---- Test 4: axiom_ring_discard (peek + consume) ---- */
static void test_discard(void) {
    axiom_ring_t ring;
    axiom_ring_init(&ring, ring_mem, RING_SIZE);

    uint8_t data[8] = {1,2,3,4,5,6,7,8};
    axiom_ring_write(&ring, data, 8);

    /* Peek 4 bytes */
    uint8_t peeked[4];
    uint16_t n = axiom_ring_peek(&ring, peeked, 4);
    CHECK("discard: peeked 4", n == 4);
    CHECK("discard: peeked[0]==1", peeked[0] == 1);
    CHECK("discard: peeked[3]==4", peeked[3] == 4);

    /* Consume = discard 4 bytes */
    axiom_ring_consume(&ring, 4);
    CHECK("discard: after consume used==4", axiom_ring_used(&ring) == 4);
    CHECK("discard: after consume free==12", axiom_ring_free(&ring) == RING_SIZE - 4);

    /* Read the remaining 4 */
    uint8_t out[4];
    uint16_t nr = axiom_ring_read(&ring, out, 4);
    CHECK("discard: remaining read 4", nr == 4);
    CHECK("discard: remaining[0]==5", out[0] == 5);
    CHECK("discard: remaining[3]==8", out[3] == 8);
    CHECK("discard: all consumed", axiom_ring_used(&ring) == 0);
}

/* ---- Test 5: Wrap-around correctness ---- */
static void test_wraparound(void) {
    axiom_ring_t ring;
    axiom_ring_init(&ring, ring_mem, RING_SIZE);

    uint8_t data[8] = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7};
    uint8_t out[8];

    /* Write 8, read 8 → head=8, tail=8 */
    axiom_ring_write(&ring, data, 8);
    axiom_ring_read(&ring, out, 8);

    /* Write 12 bytes (wraps around the 16-byte buffer) */
    uint8_t data2[12];
    for (int i = 0; i < 12; i++) data2[i] = (uint8_t)(0xB0 + i);

    bool ok = axiom_ring_write(&ring, data2, 12);
    CHECK("wraparound: write 12 ok", ok == true);
    CHECK("wraparound: used==12", axiom_ring_used(&ring) == 12);

    /* Read all 12 and verify data integrity through the wrap */
    uint8_t out2[12];
    uint16_t nr = axiom_ring_read(&ring, out2, 12);
    CHECK("wraparound: read 12", nr == 12);
    for (int i = 0; i < 12; i++) {
        if (out2[i] != (uint8_t)(0xB0 + i)) {
            printf("  FAIL: wraparound: data[%d] expected 0x%02X got 0x%02X\n",
                   i, 0xB0 + i, out2[i]);
            failures++;
            break;
        }
    }
    CHECK("wraparound: all consumed", axiom_ring_used(&ring) == 0);
}

/* ---- Test 6: Drop / Overwrite policy (full ring) ---- */
static void test_drop_policy(void) {
    axiom_ring_t ring;
    axiom_ring_init(&ring, ring_mem, RING_SIZE);

    uint8_t data[16];
    memset(data, 0xAA, 16);
    bool ok = axiom_ring_write(&ring, data, 16);
    CHECK("drop: fill to capacity", ok == true);
    CHECK("drop: used==16", axiom_ring_used(&ring) == 16);

    /* Now try to write 1 more byte */
    uint8_t extra = 0xBB;
    ok = axiom_ring_write(&ring, &extra, 1);

#if AXIOM_RING_BUFFER_POLICY == AXIOM_RING_BUFFER_POLICY_OVERWRITE
    CHECK("overwrite: overflow returns true", ok == true);
    CHECK("overwrite: used remains at capacity", axiom_ring_used(&ring) == 16);
#else
    CHECK("drop: overflow returns false", ok == false);
    CHECK("drop: used unchanged", axiom_ring_used(&ring) == 16);
#endif
}


/* ---- Test 7: Zero-length and oversized writes ---- */
static void test_edge_cases(void) {
    axiom_ring_t ring;
    axiom_ring_init(&ring, ring_mem, RING_SIZE);

    uint8_t data[1] = {0x42};
    bool ok;

    /* Zero-length write */
    ok = axiom_ring_write(&ring, data, 0);
    CHECK("edge: zero-length write returns false", ok == false);

    /* Oversized write (> capacity) */
    ok = axiom_ring_write(&ring, data, RING_SIZE + 1);
    CHECK("edge: oversized write returns false", ok == false);

    /* Peek with zero max_len */
    uint8_t out[1];
    uint16_t n = axiom_ring_peek(&ring, out, 0);
    CHECK("edge: peek 0 returns 0", n == 0);

    /* Read with NULL */
    n = axiom_ring_read(&ring, NULL, 8);
    CHECK("edge: read NULL returns 0", n == 0);
}

/* ---- Test 8: Peek does not consume ---- */
static void test_peek_no_consume(void) {
    axiom_ring_t ring;
    axiom_ring_init(&ring, ring_mem, RING_SIZE);

    uint8_t data[4] = {1,2,3,4};
    axiom_ring_write(&ring, data, 4);

    uint8_t out[4];
    uint16_t n1 = axiom_ring_peek(&ring, out, 4);
    CHECK("peek: got 4 bytes", n1 == 4);
    CHECK("peek: still has 4 bytes used", axiom_ring_used(&ring) == 4);

    uint16_t n2 = axiom_ring_peek(&ring, out, 4);
    CHECK("peek: second peek still returns 4", n2 == 4);
}

/* ---- Test 9: Multiple write/read cycles ---- */
static void test_multi_cycle(void) {
    axiom_ring_t ring;
    axiom_ring_init(&ring, ring_mem, RING_SIZE);

    for (int cycle = 0; cycle < 10; cycle++) {
        uint8_t wdata[4];
        for (int i = 0; i < 4; i++) wdata[i] = (uint8_t)(cycle * 16 + i);

        axiom_ring_write(&ring, wdata, 4);

        uint8_t rdata[4];
        axiom_ring_read(&ring, rdata, 4);

        for (int i = 0; i < 4; i++) {
            if (rdata[i] != wdata[i]) {
                printf("  FAIL: multi_cycle[%d][%d] expected 0x%02X got 0x%02X\n",
                       cycle, i, wdata[i], rdata[i]);
                failures++;
                return;
            }
        }
    }
    CHECK("multi_cycle: all 10 cycles correct", true);
    CHECK("multi_cycle: ring empty after cycles", axiom_ring_used(&ring) == 0);
}

/* ---- Test 10: Extreme wrap-around and Overwrite logic simulation ---- */
static void test_blind_overwrite_extreme_reliability(void) {
    axiom_ring_t ring;
    uint8_t buffer[8]; /* 8 bytes small buffer */
    axiom_ring_init(&ring, buffer, 8);

    /* Test 32-bit integer overflow calculation.
     * Ensure when head wraps around, head - tail still yields correct used bytes */
    ring.head = 0xFFFFFFFCu;
    ring.tail = 0xFFFFFFFAu; /* used = 2 */
    CHECK("extreme: initial used before wrap-around is 2", axiom_ring_used(&ring) == 2);

    /* Write 4 bytes: head advances from 0xFFFFFFFCu to 0x00000000u (wraps around) */
    uint8_t wdata[4] = {1, 2, 3, 4};
    bool ok = axiom_ring_write(&ring, wdata, 4);
    CHECK("extreme: wrap-around write success", ok == true);
    CHECK("extreme: head wrapped around to 0", ring.head == 0x00000000u);
    CHECK("extreme: modular used after wrap-around is 6", axiom_ring_used(&ring) == 6);

    /* Under OVERWRITE policy config, verify blind push of tail on overflow */
#if AXIOM_RING_BUFFER_POLICY == AXIOM_RING_BUFFER_POLICY_OVERWRITE
    /* Write 4 more bytes (used is 6, space is 2, needs 4 -> overflow by 2) */
    uint8_t wdata2[4] = {5, 6, 7, 8};
    ok = axiom_ring_write(&ring, wdata2, 4);
    CHECK("extreme: overwrite write success", ok == true);
    CHECK("extreme: tail pushed forward due to overwrite", ring.tail == 0xFFFFFFFCu);
    CHECK("extreme: used remains filled at capacity", axiom_ring_used(&ring) == 8);
#endif
}

int main(void) {
    test_init();
    test_free();
    test_reset();
    test_discard();
    test_wraparound();
    test_drop_policy();
    test_edge_cases();
    test_peek_no_consume();
    test_multi_cycle();
    test_blind_overwrite_extreme_reliability();

    if (failures == 0) {
        printf("test_ring_ext: PASSED (10 test groups)\n");
    } else {
        printf("test_ring_ext: FAILED (%d failures)\n", failures);
    }
    return failures;
}
