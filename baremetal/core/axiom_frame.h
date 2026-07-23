#ifndef AXIOM_FRAME_H
#define AXIOM_FRAME_H

#include "axiom_crc.h"
#include "axiom_event.h"
#include "axiom_timestamp.h"

#include <stdbool.h>
#include <stdint.h>

/* Validate one complete, contiguous frame copied from a ring. */
static inline bool axiom_frame_validate(const uint8_t *frame, uint16_t available,
                                        uint16_t *frame_len) {
    if (!frame || available < 12u || frame[0] != AXIOM_SYNC_BYTE ||
        frame[1] != AXIOM_WIRE_VERSION || frame[2] >= AXIOM_LEVEL_MAX) {
        return false;
    }

    uint8_t ts_len = axiom_timestamp_decode_len(frame[AXIOM_HEADER_LEN]);
    uint16_t payload_offset = (uint16_t)(AXIOM_HEADER_LEN + ts_len);
    if (payload_offset >= available) {
        return false;
    }

    uint16_t payload_len = frame[payload_offset];
    uint16_t total = (uint16_t)(payload_offset + 1u + payload_len + AXIOM_CRC_LEN);
    if (total > AXIOM_MAX_FRAME_LEN || total > available) {
        return false;
    }

    uint16_t expected_crc = (uint16_t)frame[total - 2u] |
        (uint16_t)((uint16_t)frame[total - 1u] << 8u);
    if (axiom_crc16(frame, total - AXIOM_CRC_LEN) != expected_crc) {
        return false;
    }

    if (frame_len) {
        *frame_len = total;
    }
    return true;
}

#endif /* AXIOM_FRAME_H */
