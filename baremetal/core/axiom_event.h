#ifndef AXIOM_EVENT_H
#define AXIOM_EVENT_H

#include <stdint.h>
#include "axiom_static_assert.h"
#include "axiom_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Log levels
 * --------------------------------------------------------------------------- */
typedef enum {
    AXIOM_LEVEL_DEBUG = 0,
    AXIOM_LEVEL_INFO  = 1,
    AXIOM_LEVEL_WARN  = 2,
    AXIOM_LEVEL_ERROR = 3,
    AXIOM_LEVEL_FAULT = 4,
    AXIOM_LEVEL_MAX   = 5
} axiom_level_t;

/* ---------------------------------------------------------------------------
 * Sync byte (frame delimiter)
 * --------------------------------------------------------------------------- */
#define AXIOM_SYNC_BYTE 0xA5u

/* ---------------------------------------------------------------------------
 * Event header: exactly 8 bytes, packed, little-endian on wire
 * --------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  sync;       /* AXIOM_SYNC_BYTE = 0xA5 */
    uint8_t  version;    /* major << 4 | minor */
    uint8_t  level;      /* lower nibble: level, upper nibble: reserved (0) */
    uint8_t  module_id;
    uint16_t event_id;   /* little-endian */
    uint16_t seq;        /* little-endian */
} axiom_event_header_t;

AXIOM_CHECK_SIZE(axiom_event_header_t, 8);
AXIOM_CHECK_ALIGN(axiom_event_header_t, 1);

/* ---------------------------------------------------------------------------
 * Wire v1 payload tags and wire v2 metadata suffix tags.
 *
 * Normal event arguments are schema-driven packed values in wire v2. The
 * numeric constants remain public for legacy v1 decoding, the reserved v2
 * AX_PROBE system payload, and metadata suffix fields that remain tagged.
 * --------------------------------------------------------------------------- */
typedef enum {
    AXIOM_TYPE_BOOL      = 0x00,
    AXIOM_TYPE_U8        = 0x01,
    AXIOM_TYPE_I8        = 0x02,
    AXIOM_TYPE_U16       = 0x03,
    AXIOM_TYPE_I16       = 0x04,
    AXIOM_TYPE_U32       = 0x05,
    AXIOM_TYPE_I32       = 0x06,
    AXIOM_TYPE_F32       = 0x07,
    AXIOM_TYPE_TIMESTAMP = 0x08,
    AXIOM_TYPE_BYTES     = 0x09,
    AXIOM_TYPE_META_LOCATION = 0x0A,
    AXIOM_TYPE_META_IDENTITY = 0x0B
    /* Reserved: 0x0C - 0x7F, User-defined: 0x80 - 0xFF */
} axiom_type_t;

/* Backward-compatible macros: allow existing code (including _Generic,
 * sizeof checks, and preprocessor usage) to keep working. */
#define AXIOM_TYPE_BOOL      0x00u
#define AXIOM_TYPE_U8        0x01u
#define AXIOM_TYPE_I8        0x02u
#define AXIOM_TYPE_U16       0x03u
#define AXIOM_TYPE_I16       0x04u
#define AXIOM_TYPE_U32       0x05u
#define AXIOM_TYPE_I32       0x06u
#define AXIOM_TYPE_F32       0x07u
#define AXIOM_TYPE_TIMESTAMP 0x08u
#define AXIOM_TYPE_BYTES     0x09u
#define AXIOM_TYPE_META_LOCATION 0x0Au
#define AXIOM_TYPE_META_IDENTITY 0x0Bu

/* ---------------------------------------------------------------------------
 * Wire format version v2.0.
 *
 * v2 changes normal argument encoding from typed fields to dictionary-driven
 * packed values. This is intentionally a major-version transition.
 * --------------------------------------------------------------------------- */
#define AXIOM_WIRE_VERSION_MAJOR 2u
#define AXIOM_WIRE_VERSION_MINOR 0u
#define AXIOM_WIRE_VERSION ((uint8_t)((AXIOM_WIRE_VERSION_MAJOR << 4u) | AXIOM_WIRE_VERSION_MINOR))

/* ---------------------------------------------------------------------------
 * Frame layout constants
 * --------------------------------------------------------------------------- */
#define AXIOM_HEADER_LEN          8u  /* 8-byte fixed header */
#define AXIOM_MAX_TIMESTAMP_LEN   5u  /* 0xFE + 4-byte full delta */
#define AXIOM_CRC_LEN             2u  /* CRC-16 trailer */

/* Total max frame: header + timestamp + payload length + payload + CRC. */
#define AXIOM_MAX_FRAME_LEN \
    (AXIOM_HEADER_LEN + AXIOM_MAX_TIMESTAMP_LEN + 1u + AXIOM_MAX_PAYLOAD_LEN + AXIOM_CRC_LEN)

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_EVENT_H */
