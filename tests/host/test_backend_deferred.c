#include "test_helpers.h"

#include "axiom_backend.h"
#include "axiom_backend_deferred.h"
#include "axiom_crc.h"
#include "axiom_event.h"

#include <string.h>

/* Downstream mock backend */
static uint8_t s_downstream_buf[1024];
static uint16_t s_downstream_len;
static int s_downstream_write_calls;
static int s_downstream_flush_calls;
static int s_downstream_panic_calls;
static int s_downstream_drop_calls;
static uint32_t s_downstream_lost;
static int s_downstream_write_result; /* 0=success, -1=fail */
static int s_downstream_ready;

static void make_valid_frame(uint8_t frame[12]) {
    memset(frame, 0, 12u);
    frame[0] = AXIOM_SYNC_BYTE;
    frame[1] = AXIOM_WIRE_VERSION;
    frame[8] = 0u;
    frame[9] = 0u;
    uint16_t crc = axiom_crc16(frame, 10u);
    frame[10] = (uint8_t)(crc & 0xFFu);
    frame[11] = (uint8_t)(crc >> 8u);
}

static int mock_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)ctx;
    s_downstream_write_calls++;
    if (s_downstream_write_result < 0) return -1;
    if (s_downstream_len + len <= sizeof(s_downstream_buf)) {
        memcpy(s_downstream_buf + s_downstream_len, buf, len);
        s_downstream_len += len;
    }
    return 0;
}

static int mock_ready(void *ctx) {
    (void)ctx;
    return s_downstream_ready;
}

static int mock_panic_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)buf; (void)len; (void)ctx;
    s_downstream_panic_calls++;
    return 0;
}

static void mock_drop(uint32_t lost, void *ctx) {
    (void)ctx;
    s_downstream_drop_calls++;
    s_downstream_lost += lost;
}

static void reset_mock(void) {
    s_downstream_len = 0;
    s_downstream_write_calls = 0;
    s_downstream_flush_calls = 0;
    s_downstream_panic_calls = 0;
    s_downstream_drop_calls = 0;
    s_downstream_lost = 0;
    s_downstream_write_result = 0;
    s_downstream_ready = 1;
    memset(s_downstream_buf, 0, sizeof(s_downstream_buf));
}

static void test_init_success(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock",
        .write = mock_write,
        .panic_write = mock_panic_write,
        .on_drop = mock_drop
    );

    int rc = axiom_backend_deferred_init(&ctx, &ring_ctx, &downstream);
    CHECK("init: success", rc == 0);

    const axiom_backend_t *be = axiom_backend_deferred_get_backend(&ctx);
    CHECK("init: backend non-null", be != NULL);
    CHECK("init: backend name", strcmp(be->name, "deferred") == 0);
    CHECK("init: pending=0", axiom_backend_deferred_pending(&ctx) == 0);
}

static void test_init_fail(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock",
        .write = mock_write
    );

    CHECK("init_fail: null ctx", axiom_backend_deferred_init(NULL, &ring_ctx, &downstream) < 0);
    CHECK("init_fail: null ring", axiom_backend_deferred_init(&ctx, NULL, &downstream) < 0);
    CHECK("init_fail: null downstream", axiom_backend_deferred_init(&ctx, &ring_ctx, NULL) < 0);

    /* downstream without write */
    static const axiom_backend_t no_write = AXIOM_BACKEND_INIT(.name = "nowrite");
    CHECK("init_fail: no write", axiom_backend_deferred_init(&ctx, &ring_ctx, &no_write) < 0);
}

static void test_write_to_ring(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock",
        .write = mock_write
    );

    axiom_backend_deferred_init(&ctx, &ring_ctx, &downstream);
    reset_mock();

    const axiom_backend_t *be = axiom_backend_deferred_get_backend(&ctx);

    /* Write a frame into deferred backend */
    uint8_t frame[16] = {0};
    int rc = be->write(frame, sizeof(frame), be->ctx);
    CHECK("write: success", rc == 0);
    CHECK("write: pending > 0", axiom_backend_deferred_pending(&ctx) > 0);
    CHECK("write: downstream not called yet", s_downstream_write_calls == 0);
}

static void test_flush_cascade(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock",
        .write = mock_write
    );

    axiom_backend_deferred_init(&ctx, &ring_ctx, &downstream);
    reset_mock();

    const axiom_backend_t *be = axiom_backend_deferred_get_backend(&ctx);

    /* Write a minimal valid frame flush can fully consume:
     * 8 hdr + 1 ts + 1 payload_len + 0 payload + 2 crc = 12 bytes */
    uint8_t frame[12];
    make_valid_frame(frame);
    be->write(frame, sizeof(frame), be->ctx);

    int flushed = be->flush(be->ctx);
    CHECK("flush: dispatched > 0", flushed > 0);
    CHECK("flush: downstream called", s_downstream_write_calls > 0);
    CHECK("flush: pending=0", axiom_backend_deferred_pending(&ctx) == 0);
}

static void test_panic_write_bypass(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock",
        .write = mock_write,
        .panic_write = mock_panic_write
    );

    axiom_backend_deferred_init(&ctx, &ring_ctx, &downstream);
    reset_mock();

    const axiom_backend_t *be = axiom_backend_deferred_get_backend(&ctx);

    uint8_t frame[8] = {0xBB};
    int rc = be->panic_write(frame, sizeof(frame), be->ctx);
    CHECK("panic: success", rc == 0);
    CHECK("panic: downstream panic called", s_downstream_panic_calls > 0);
    CHECK("panic: pending still 0", axiom_backend_deferred_pending(&ctx) == 0);
}

static void test_reset(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock",
        .write = mock_write
    );

    axiom_backend_deferred_init(&ctx, &ring_ctx, &downstream);

    const axiom_backend_t *be = axiom_backend_deferred_get_backend(&ctx);
    uint8_t frame[16] = {0xCC};
    be->write(frame, sizeof(frame), be->ctx);
    CHECK("reset: pending > 0 before", axiom_backend_deferred_pending(&ctx) > 0);

    axiom_backend_deferred_reset(&ctx);
    CHECK("reset: pending=0 after", axiom_backend_deferred_pending(&ctx) == 0);
}

static void test_downstream_busy(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock",
        .write = mock_write
    );

    axiom_backend_deferred_init(&ctx, &ring_ctx, &downstream);
    reset_mock();
    s_downstream_write_result = -1; /* make downstream fail */

    const axiom_backend_t *be = axiom_backend_deferred_get_backend(&ctx);

    uint8_t frame[12];
    make_valid_frame(frame);
    be->write(frame, sizeof(frame), be->ctx);
    CHECK("busy: pending > 0", axiom_backend_deferred_pending(&ctx) > 0);

    int flushed = be->flush(be->ctx);
    CHECK("busy: flush returns 0 (nothing dispatched)", flushed == 0);
    CHECK("busy: pending still > 0", axiom_backend_deferred_pending(&ctx) > 0);
}

static void test_downstream_not_ready_still_buffers(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock",
        .write = mock_write,
        .ready = mock_ready
    );

    axiom_backend_deferred_init(&ctx, &ring_ctx, &downstream);
    reset_mock();
    s_downstream_ready = 0;
    const axiom_backend_t *be = axiom_backend_deferred_get_backend(&ctx);
    uint8_t frame[12];
    make_valid_frame(frame);

    CHECK("not_ready: deferred itself remains ready", be->ready(be->ctx) == 1);
    CHECK("not_ready: frame accepted locally", be->write(frame, sizeof(frame), be->ctx) == 0);
    CHECK("not_ready: flush waits", be->flush(be->ctx) == 0);
    CHECK("not_ready: frame retained", axiom_backend_deferred_pending(&ctx) == sizeof(frame));
}

static void test_ready_reserves_a_maximum_frame(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock", .write = mock_write);
    axiom_backend_deferred_init(&ctx, &ring_ctx, &downstream);
    reset_mock();
    const axiom_backend_t *be = axiom_backend_deferred_get_backend(&ctx);
    uint8_t filler[AXIOM_DEFERRED_RING_SIZE - AXIOM_MAX_FRAME_LEN + 1u] = {0};

    CHECK("ready_space: initially ready", be->ready(be->ctx) == 1);
    CHECK("ready_space: filler accepted", be->write(filler, sizeof(filler), be->ctx) == 0);
    CHECK("ready_space: refuses when maximum frame no longer fits",
          be->ready(be->ctx) == 0);
}

static void test_corrupt_data_resynchronizes(void) {
    axiom_backend_deferred_ctx_t ctx;
    axiom_deferred_ring_ctx_t ring_ctx;
    static const axiom_backend_t downstream = AXIOM_BACKEND_INIT(
        .name = "mock", .write = mock_write);
    axiom_backend_deferred_init(&ctx, &ring_ctx, &downstream);
    reset_mock();
    const axiom_backend_t *be = axiom_backend_deferred_get_backend(&ctx);
    uint8_t corrupt[3] = {0x00u, 0x11u, 0x22u};
    uint8_t frame[12];
    make_valid_frame(frame);
    CHECK("resync: corrupt prefix queued", be->write(corrupt, sizeof(corrupt), be->ctx) == 0);
    CHECK("resync: valid frame queued", be->write(frame, sizeof(frame), be->ctx) == 0);
    CHECK("resync: valid frame dispatched", be->flush(be->ctx) == 1);
    CHECK("resync: queue drained", axiom_backend_deferred_pending(&ctx) == 0u);
}

int main(void) {
    test_init_success();
    test_init_fail();
    test_write_to_ring();
    test_flush_cascade();
    test_panic_write_bypass();
    test_reset();
    test_downstream_busy();
    test_downstream_not_ready_still_buffers();
    test_ready_reserves_a_maximum_frame();
    test_corrupt_data_resynchronizes();

    TEST_RESULT("test_backend_deferred", failures);
    return failures;
}
