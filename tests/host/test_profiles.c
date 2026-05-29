#include "axiomtrace.h"
#include "axiom_backend.h"
#include "axiom_event.h"

#include <stdio.h>
#include <stdint.h>

static uint16_t s_frame_count;
static uint8_t s_modules[8];
static uint16_t s_events[8];

#define CHECK(msg, cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            return 1; \
        } \
    } while (0)

static int capture_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)ctx;
    if (len < 12u || buf[0] != AXIOM_SYNC_BYTE || buf[1] != AXIOM_WIRE_VERSION) {
        return -1;
    }
    if (s_frame_count < (uint16_t)(sizeof(s_modules) / sizeof(s_modules[0]))) {
        uint16_t event_id = (uint16_t)buf[4];
        event_id = (uint16_t)(event_id | (uint16_t)((uint16_t)buf[5] << 8u));
        s_modules[s_frame_count] = buf[3];
        s_events[s_frame_count] = event_id;
    }
    s_frame_count++;
    return (int)len;
}

int main(void) {
    static const axiom_backend_t capture_backend = AXIOM_BACKEND_INIT(
        .name = "profile-capture",
        .write = capture_write
    );

    axiom_init();
    CHECK("register capture backend", axiom_backend_register(&capture_backend) == AXIOM_BACKEND_OK);

    AX_LOG("profile smoke");
    AX_LOG_INFO("profile info");
    AX_PROBE("adc", (uint8_t)7u);
    AX_EVT(INFO, 0x11u, 0x0101u, (uint8_t)1u);
    AX_KV(INFO, 0x12u, 0x0102u, "v", (uint16_t)2u);
    AX_FAULT(0x13u, 0x0103u, (uint8_t)3u);
    axiom_flush();

#if AXIOM_PROFILE == AXIOM_PROFILE_PROD
    CHECK("PROD prunes AX_PROBE but keeps structured events", s_frame_count == 3u);
    CHECK("PROD first frame is AX_EVT", s_modules[0] == 0x11u && s_events[0] == 0x0101u);
    CHECK("PROD second frame is AX_KV", s_modules[1] == 0x12u && s_events[1] == 0x0102u);
    CHECK("PROD third frame is AX_FAULT", s_modules[2] == 0x13u && s_events[2] == 0x0103u);
#else
    CHECK("DEV/FIELD preserve AX_PROBE and structured events", s_frame_count == 4u);
    CHECK("DEV/FIELD first frame is AX_PROBE system event", s_modules[0] == 0x00u && s_events[0] == 0x0000u);
    CHECK("DEV/FIELD second frame is AX_EVT", s_modules[1] == 0x11u && s_events[1] == 0x0101u);
    CHECK("DEV/FIELD third frame is AX_KV", s_modules[2] == 0x12u && s_events[2] == 0x0102u);
    CHECK("DEV/FIELD fourth frame is AX_FAULT", s_modules[3] == 0x13u && s_events[3] == 0x0103u);
#endif

    return 0;
}
