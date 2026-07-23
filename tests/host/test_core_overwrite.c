#include "test_helpers.h"

#include "axiom_diagnostics.h"
#include "axiom_frame.h"
#include "axiom_frontend.h"
#include "axiom_backend.h"

static void test_complete_frame_overwrite(void) {
    uint8_t capture[2048] = {0};
    axiom_memory_backend_ctx_t memory_context;
    axiom_backend_t memory = axiom_backend_memory(
        "overwrite", capture, sizeof(capture), &memory_context);
    axiom_init();
    CHECK("overwrite: memory registered",
          axiom_backend_register(&memory) == AXIOM_BACKEND_OK);

    for (uint16_t i = 0u; i < 40u; ++i) {
        axiom_write(AXIOM_LEVEL_INFO, 1u, (uint16_t)(0x400u + i), NULL, 0u);
    }
    axiom_diagnostics_t diagnostics;
    axiom_diagnostics_get(&diagnostics);
    CHECK("overwrite: complete old frames counted", diagnostics.ring_full > 0u);
    axiom_flush();

    uint32_t offset = 0u;
    uint32_t frames = 0u;
    while (offset < memory_context.head) {
        uint16_t frame_len = 0u;
        CHECK("overwrite: every retained record validates",
              axiom_frame_validate(capture + offset,
                                   (uint16_t)(memory_context.head - offset),
                                   &frame_len));
        if (frame_len == 0u) {
            break;
        }
        offset += frame_len;
        frames++;
    }
    CHECK("overwrite: capture contains frames", frames > 0u);
    CHECK("overwrite: no partial trailing bytes", offset == memory_context.head);
}

int main(void) {
    test_complete_frame_overwrite();
    TEST_RESULT("test_core_overwrite", failures);
    return failures;
}
