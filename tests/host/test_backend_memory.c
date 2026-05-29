#include "test_helpers.h"

#include "axiom_backend.h"

#include <string.h>

/* Forward declaration for the memory backend factory */
typedef struct {
    uint8_t *buf;
    uint32_t size;
    uint32_t head;
} axiom_memory_backend_ctx_t;

extern axiom_backend_t axiom_backend_memory(const char *name, uint8_t *buf, uint32_t size,
                                             axiom_memory_backend_ctx_t *ctx);

static void test_valid_struct(void) {
    static uint8_t buf[256];
    axiom_memory_backend_ctx_t ctx;
    axiom_backend_t be = axiom_backend_memory("test-mem", buf, sizeof(buf), &ctx);

    CHECK("valid: name set", strcmp(be.name, "test-mem") == 0);
    CHECK("valid: write non-null", be.write != NULL);
    CHECK("valid: ready non-null", be.ready != NULL);
    CHECK("valid: ctx set", be.ctx == &ctx);
}

static void test_write_to_buffer(void) {
    static uint8_t buf[256];
    axiom_memory_backend_ctx_t ctx;
    axiom_backend_t be = axiom_backend_memory("test-mem", buf, sizeof(buf), &ctx);

    uint8_t data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    int rc = be.write(data, sizeof(data), be.ctx);
    CHECK("write: success", rc == 0);
    CHECK("write: data in buf", memcmp(buf, data, sizeof(data)) == 0);
}

static void test_wrap_behavior(void) {
    static uint8_t buf[32];
    axiom_memory_backend_ctx_t ctx;
    axiom_backend_t be = axiom_backend_memory("test-mem", buf, sizeof(buf), &ctx);

    uint8_t data1[24] = {0xAA};
    uint8_t data2[24] = {0xBB};

    be.write(data1, sizeof(data1), be.ctx);
    CHECK("wrap: head after first write", ctx.head == 24);

    /* Second write exceeds buffer, should wrap to 0 */
    be.write(data2, sizeof(data2), be.ctx);
    CHECK("wrap: head wrapped", ctx.head == 24);
    CHECK("wrap: data2 at start", buf[0] == 0xBB);
}

static void test_always_ready(void) {
    static uint8_t buf[64];
    axiom_memory_backend_ctx_t ctx;
    axiom_backend_t be = axiom_backend_memory("test-mem", buf, sizeof(buf), &ctx);

    CHECK("ready: always 1", be.ready(be.ctx) == 1);
}

static void test_write_fail_too_large(void) {
    static uint8_t buf[16];
    axiom_memory_backend_ctx_t ctx;
    axiom_backend_t be = axiom_backend_memory("test-mem", buf, sizeof(buf), &ctx);

    uint8_t data[32] = {0};
    int rc = be.write(data, sizeof(data), be.ctx);
    CHECK("fail: len > size returns -1", rc == -1);
}

static void test_default_name(void) {
    static uint8_t buf[64];
    axiom_memory_backend_ctx_t ctx;
    axiom_backend_t be = axiom_backend_memory(NULL, buf, sizeof(buf), &ctx);

    CHECK("default_name: is 'memory'", strcmp(be.name, "memory") == 0);
}

int main(void) {
    test_valid_struct();
    test_write_to_buffer();
    test_wrap_behavior();
    test_always_ready();
    test_write_fail_too_large();
    test_default_name();

    TEST_RESULT("test_backend_memory", failures);
    return failures;
}
