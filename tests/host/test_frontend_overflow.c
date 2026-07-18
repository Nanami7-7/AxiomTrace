#include "test_helpers.h"

#include "axiom_backend.h"
#include "axiom_diagnostics.h"
#include "axiom_frame.h"
#include "axiom_frontend.h"

static void test_frontend_overflow_is_atomic(void) {
    uint8_t capture[128] = {0};
    axiom_memory_backend_ctx_t memory_context;
    axiom_backend_t memory = axiom_backend_memory(
        "frontend-overflow", capture, sizeof(capture), &memory_context);
    axiom_init();
    CHECK("frontend overflow: memory registered",
          axiom_backend_register(&memory) == AXIOM_BACKEND_OK);

    AX_EVT(INFO, 1u, 0x0100u, (uint32_t)1u, (uint32_t)2u);
    axiom_flush();
    CHECK("frontend overflow: no partial frame dispatched", memory_context.head == 0u);

    axiom_diagnostics_t diagnostics;
    axiom_diagnostics_get(&diagnostics);
    CHECK("frontend overflow: encode diagnostic", diagnostics.encode_overflow == 1u);
    CHECK("frontend overflow: not invalid input", diagnostics.invalid_input == 0u);

    AX_EVT(INFO, 1u, 0x0101u, (uint32_t)3u);
    axiom_flush();
    uint16_t first_len = 0u;
    CHECK("frontend overflow: recovery event validates",
          axiom_frame_validate(capture, (uint16_t)memory_context.head, &first_len));
    CHECK("frontend overflow: recovery event id",
          ((uint16_t)capture[4] | ((uint16_t)capture[5] << 8u)) == 0x0101u);
    uint16_t summary_len = 0u;
    CHECK("frontend overflow: one complete summary follows",
          axiom_frame_validate(capture + first_len,
                               (uint16_t)(memory_context.head - first_len),
                               &summary_len));
    CHECK("frontend overflow: summary consumes remaining bytes",
          (uint32_t)first_len + summary_len == memory_context.head);
    CHECK("frontend overflow: drop summary event id",
          ((uint16_t)capture[first_len + 4u] |
           ((uint16_t)capture[first_len + 5u] << 8u)) ==
              AXIOM_SYSTEM_EVENT_DROP_SUMMARY);
}

int main(void) {
    test_frontend_overflow_is_atomic();
    TEST_RESULT("test_frontend_overflow", failures);
    return failures;
}
