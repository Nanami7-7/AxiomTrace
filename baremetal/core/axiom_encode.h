#ifndef AXIOM_ENCODE_H
#define AXIOM_ENCODE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "axiom_event.h"

/* ---------------------------------------------------------------------------
 * Overflow protection state (readable by decoder / diagnostics)
 * Set by axiom_enc_xxx() when a write would exceed AXIOM_MAX_PAYLOAD_LEN.
 * Reset to false by the caller before each axiom_write() batch.
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_ENCODE_OVERFLOW_DETECTION
#define AXIOM_ENCODE_OVERFLOW_DETECTION 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if AXIOM_ENCODE_OVERFLOW_DETECTION
extern volatile bool axiom_encode_overflow;
#endif

/* ---------------------------------------------------------------------------
 * Payload encoding constants
 * --------------------------------------------------------------------------- */
#define AXIOM_TAG_SIZE 0u  /* Wire v2 normal arguments carry no type-tag byte. */

/* ---------------------------------------------------------------------------
 * Type-safe payload encoding helpers
 * Each function writes: [value (N bytes)] in the wire v2 packed payload.
 * Overflow check: pos + value_bytes <= AXIOM_MAX_PAYLOAD_LEN
 * --------------------------------------------------------------------------- */

/**
 * @brief Encode a boolean value into the payload buffer.
 * @param buf   Payload buffer (caller owns memory)
 * @param pos   Current write position, updated on success
 * @param val   Value to encode
 *
 * Overflow protection: silently skips write if buf cannot fit
 * value(1) = 1 byte.
 * When AXIOM_ENCODE_OVERFLOW_DETECTION is enabled, axiom_encode_overflow
 * is set to true on overflow.
 */
static inline void axiom_enc_bool(uint8_t *buf, uint8_t *pos, bool val) {
    if (*pos + 1u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = val ? 1u : 0u;
}

static inline void axiom_enc_u8(uint8_t *buf, uint8_t *pos, uint8_t val) {
    /* Required: value(1) = 1 byte */
    if (*pos + 1u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = val;
}

static inline void axiom_enc_i8(uint8_t *buf, uint8_t *pos, int8_t val) {
    /* Required: value(1) = 1 byte */
    if (*pos + 1u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = (uint8_t)val;
}

static inline void axiom_enc_u16(uint8_t *buf, uint8_t *pos, uint16_t val) {
    /* Required: value(2) = 2 bytes */
    if (*pos + 2u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = (uint8_t)(val & 0xFFu);
    buf[(*pos)++] = (uint8_t)(val >> 8);
}

static inline void axiom_enc_i16(uint8_t *buf, uint8_t *pos, int16_t val) {
    /* Required: value(2) = 2 bytes */
    if (*pos + 2u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    uint16_t u = (uint16_t)val;
    buf[(*pos)++] = (uint8_t)(u & 0xFFu);
    buf[(*pos)++] = (uint8_t)(u >> 8);
}

static inline void axiom_enc_u32(uint8_t *buf, uint8_t *pos, uint32_t val) {
    /* Required: value(4) = 4 bytes */
    if (*pos + 4u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = (uint8_t)(val & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 8) & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 16) & 0xFFu);
    buf[(*pos)++] = (uint8_t)(val >> 24);
}

static inline void axiom_enc_i32(uint8_t *buf, uint8_t *pos, int32_t val) {
    /* Required: value(4) = 4 bytes */
    if (*pos + 4u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    uint32_t u = (uint32_t)val; /* cast to unsigned to avoid sign-extension */
    buf[(*pos)++] = (uint8_t)(u & 0xFFu);
    buf[(*pos)++] = (uint8_t)((u >> 8) & 0xFFu);
    buf[(*pos)++] = (uint8_t)((u >> 16) & 0xFFu);
    buf[(*pos)++] = (uint8_t)(u >> 24);
}

static inline void axiom_enc_f32(uint8_t *buf, uint8_t *pos, float val) {
    /* Required: value(4) = 4 bytes */
    if (*pos + 4u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    uint32_t u;
    memcpy(&u, &val, sizeof(u));
    buf[(*pos)++] = (uint8_t)(u & 0xFFu);
    buf[(*pos)++] = (uint8_t)((u >> 8) & 0xFFu);
    buf[(*pos)++] = (uint8_t)((u >> 16) & 0xFFu);
    buf[(*pos)++] = (uint8_t)(u >> 24);
}

static inline void axiom_enc_timestamp(uint8_t *buf, uint8_t *pos, uint32_t val) {
    /* Required: value(4) = 4 bytes */
    if (*pos + 4u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = (uint8_t)(val & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 8) & 0xFFu);
    buf[(*pos)++] = (uint8_t)((val >> 16) & 0xFFu);
    buf[(*pos)++] = (uint8_t)(val >> 24);
}

static inline void axiom_enc_bytes(uint8_t *buf, uint8_t *pos, const uint8_t *data, uint8_t len) {
    /* Required: length(1) + data(len) bytes */
    if ((uint16_t)(*pos) + (uint16_t)len + 1u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = len;
    for (uint8_t i = 0; i < len; ++i) {
        buf[(*pos)++] = data[i];
    }
}

/* Reserved system probes retain typed fields because their value type is not
 * described by an application event dictionary. */
static inline bool axiom_enc_tagged_prefix(uint8_t *buf, uint8_t *pos,
                                           uint8_t tag, uint8_t value_size) {
    if ((uint16_t)(*pos) + 1u + (uint16_t)value_size > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return false;
    }
    buf[(*pos)++] = tag;
    return true;
}

static inline void axiom_enc_tagged_bool(uint8_t *buf, uint8_t *pos, bool val) {
    if (axiom_enc_tagged_prefix(buf, pos, AXIOM_TYPE_BOOL, 1u)) {
        axiom_enc_bool(buf, pos, val);
    }
}

static inline void axiom_enc_tagged_u8(uint8_t *buf, uint8_t *pos, uint8_t val) {
    if (axiom_enc_tagged_prefix(buf, pos, AXIOM_TYPE_U8, 1u)) {
        axiom_enc_u8(buf, pos, val);
    }
}

static inline void axiom_enc_tagged_i8(uint8_t *buf, uint8_t *pos, int8_t val) {
    if (axiom_enc_tagged_prefix(buf, pos, AXIOM_TYPE_I8, 1u)) {
        axiom_enc_i8(buf, pos, val);
    }
}

static inline void axiom_enc_tagged_u16(uint8_t *buf, uint8_t *pos, uint16_t val) {
    if (axiom_enc_tagged_prefix(buf, pos, AXIOM_TYPE_U16, 2u)) {
        axiom_enc_u16(buf, pos, val);
    }
}

static inline void axiom_enc_tagged_i16(uint8_t *buf, uint8_t *pos, int16_t val) {
    if (axiom_enc_tagged_prefix(buf, pos, AXIOM_TYPE_I16, 2u)) {
        axiom_enc_i16(buf, pos, val);
    }
}

static inline void axiom_enc_tagged_u32(uint8_t *buf, uint8_t *pos, uint32_t val) {
    if (axiom_enc_tagged_prefix(buf, pos, AXIOM_TYPE_U32, 4u)) {
        axiom_enc_u32(buf, pos, val);
    }
}

static inline void axiom_enc_tagged_i32(uint8_t *buf, uint8_t *pos, int32_t val) {
    if (axiom_enc_tagged_prefix(buf, pos, AXIOM_TYPE_I32, 4u)) {
        axiom_enc_i32(buf, pos, val);
    }
}

static inline void axiom_enc_tagged_f32(uint8_t *buf, uint8_t *pos, float val) {
    if (axiom_enc_tagged_prefix(buf, pos, AXIOM_TYPE_F32, 4u)) {
        axiom_enc_f32(buf, pos, val);
    }
}

/* Metadata: [tag][mode=FILE_ID][file_id:u16][line:u16]. */
static inline void axiom_enc_meta_location_file_id(uint8_t *buf, uint8_t *pos,
                                                    uint16_t file_id, uint16_t line) {
    if (*pos + 6u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = AXIOM_TYPE_META_LOCATION;
    buf[(*pos)++] = AXIOM_CFG_LOCATION_MODE_FILE_ID;
    buf[(*pos)++] = (uint8_t)(file_id & 0xFFu);
    buf[(*pos)++] = (uint8_t)(file_id >> 8);
    buf[(*pos)++] = (uint8_t)(line & 0xFFu);
    buf[(*pos)++] = (uint8_t)(line >> 8);
}

/* Metadata: [tag][mode=HASH][file_hash:u16][line:u16][func_hash:u16]. */
static inline void axiom_enc_meta_location_hash(uint8_t *buf, uint8_t *pos,
                                                uint16_t file_hash, uint16_t line,
                                                uint16_t func_hash) {
    if (*pos + 8u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = AXIOM_TYPE_META_LOCATION;
    buf[(*pos)++] = AXIOM_CFG_LOCATION_MODE_HASH;
    buf[(*pos)++] = (uint8_t)(file_hash & 0xFFu);
    buf[(*pos)++] = (uint8_t)(file_hash >> 8);
    buf[(*pos)++] = (uint8_t)(line & 0xFFu);
    buf[(*pos)++] = (uint8_t)(line >> 8);
    buf[(*pos)++] = (uint8_t)(func_hash & 0xFFu);
    buf[(*pos)++] = (uint8_t)(func_hash >> 8);
}

#define AXIOM_METADATA_ID_LEN 8u

/* Metadata: [tag][metadata_id:8]. This selects host decode metadata, not an ELF image. */
static inline void axiom_enc_meta_identity(uint8_t *buf, uint8_t *pos,
                                           const uint8_t metadata_id[AXIOM_METADATA_ID_LEN]) {
    if (*pos + 9u > AXIOM_MAX_PAYLOAD_LEN) {
#if AXIOM_ENCODE_OVERFLOW_DETECTION
        axiom_encode_overflow = true;
#endif
        return;
    }
    buf[(*pos)++] = AXIOM_TYPE_META_IDENTITY;
    for (uint8_t i = 0; i < AXIOM_METADATA_ID_LEN; ++i) {
        buf[(*pos)++] = metadata_id[i];
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

#define _AXIOM_ENCODE_TAGGED_ONE(buf, pos, arg) \
    _Generic((arg), \
        bool:     axiom_enc_tagged_bool, \
        uint8_t:  axiom_enc_tagged_u8,  \
        int8_t:   axiom_enc_tagged_i8,  \
        uint16_t: axiom_enc_tagged_u16, \
        int16_t:  axiom_enc_tagged_i16, \
        uint32_t: axiom_enc_tagged_u32, \
        int32_t:  axiom_enc_tagged_i32, \
        float:    axiom_enc_tagged_f32  \
    )(buf, &(pos), arg)

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_ENCODE_H */
