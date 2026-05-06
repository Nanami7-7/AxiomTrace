#ifndef AXIOM_ENCODE_H
#define AXIOM_ENCODE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "axiom_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type-safe payload encoding helpers
 * Each function writes: [type_tag (1B)] [value (N bytes)]
 * --------------------------------------------------------------------------- */

static inline void axiom_enc_bool(uint8_t *buf, uint8_t *pos, bool val) {
    buf[(*pos)++] = AXIOM_TYPE_BOOL;
    buf[(*pos)++] = val ? 1u : 0u;
}

static inline void axiom_enc_u8(uint8_t *buf, uint8_t *pos, uint8_t val) {
    buf[(*pos)++] = AXIOM_TYPE_U8;
    buf[(*pos)++] = val;
}

static inline void axiom_enc_i8(uint8_t *buf, uint8_t *pos, int8_t val) {
    buf[(*pos)++] = AXIOM_TYPE_I8;
    buf[(*pos)++] = (uint8_t)val;
}

static inline void axiom_enc_u16(uint8_t *buf, uint8_t *pos, uint16_t val) {
    buf[(*pos)++] = AXIOM_TYPE_U16;
    buf[(*pos)++] = (uint8_t)(val & 0xFFu);
    buf[(*pos)++] = (uint8_t)(val >> 8);
}

static inline void axiom_enc_i16(uint8_t *buf, uint8_t *pos, int16_t val) {
    buf[(*pos)++] = AXIOM_TYPE_I16;
    buf[(*pos)++] = (uint8_t)(val & 0xFFu);
    buf[(*pos)++] = (uint8_t)((uint16_t)val >> 8);
}

static inline void axiom_enc_u32(uint8_t *buf, uint8_t *pos, uint32_t val) {
    buf[(*pos)++] = AXIOM_TYPE_U32;
    buf[(*pos)++] = (uint8_t)(val & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 8) & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 16) & 0xFFu);
    buf[(*pos)++] = (uint8_t)(val >> 24);
}

static inline void axiom_enc_i32(uint8_t *buf, uint8_t *pos, int32_t val) {
    uint32_t u = (uint32_t)val;
    buf[(*pos)++] = AXIOM_TYPE_I32;
    buf[(*pos)++] = (uint8_t)(u & 0xFFu);
    buf[(*pos)++] = (uint8_t)((u >> 8) & 0xFFu);
    buf[(*pos)++] = (uint8_t)((u >> 16) & 0xFFu);
    buf[(*pos)++] = (uint8_t)(u >> 24);
}

static inline void axiom_enc_f32(uint8_t *buf, uint8_t *pos, float val) {
    buf[(*pos)++] = AXIOM_TYPE_F32;
    uint32_t u;
    memcpy(&u, &val, sizeof(u));
    buf[(*pos)++] = (uint8_t)(u & 0xFFu);
    buf[(*pos)++] = (uint8_t)((u >> 8) & 0xFFu);
    buf[(*pos)++] = (uint8_t)((u >> 16) & 0xFFu);
    buf[(*pos)++] = (uint8_t)(u >> 24);
}

static inline void axiom_enc_timestamp(uint8_t *buf, uint8_t *pos, uint32_t val) {
    buf[(*pos)++] = AXIOM_TYPE_TIMESTAMP;
    /* Write raw 32-bit value (no duplicate type tag — unlike the other enc helpers) */
    buf[(*pos)++] = (uint8_t)(val & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 8) & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 16) & 0xFFu);
    buf[(*pos)++] = (uint8_t)(val >> 24);
}

static inline void axiom_enc_bytes(uint8_t *buf, uint8_t *pos, const uint8_t *data, uint8_t len) {
    buf[(*pos)++] = AXIOM_TYPE_BYTES;
    buf[(*pos)++] = len;
    for (uint8_t i = 0; i < len; ++i) {
        buf[(*pos)++] = data[i];
    }
}

/* ---------------------------------------------------------------------------
 * C11 _Generic dispatcher for a single argument
 * Note: double literals (e.g., 3.14) are NOT supported.
 * Use float literals (e.g., 3.14f) or explicit (float)3.14 cast.
 * --------------------------------------------------------------------------- */
#define _AXIOM_ENCODE_ONE(buf, pos, arg) \
    _Generic((arg), \
        bool:     axiom_enc_bool, \
        uint8_t:  axiom_enc_u8,  \
        int8_t:   axiom_enc_i8,  \
        uint16_t: axiom_enc_u16, \
        int16_t:  axiom_enc_i16, \
        uint32_t: axiom_enc_u32, \
        int32_t:  axiom_enc_i32, \
        float:    axiom_enc_f32  \
    )(buf, &(pos), arg)

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_ENCODE_H */
