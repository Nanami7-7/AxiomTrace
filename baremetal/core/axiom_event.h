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
 * Event header: exactly 8 bytes, packed, little-endian on wire
 * --------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  sync;       /* 0xA5 */
    uint8_t  version;    /* major << 4 | minor */
    uint8_t  level;      /* lower nibble: level, upper nibble: reserved (0) */
    uint8_t  module_id;
    uint16_t event_id;   /* little-endian */
    uint16_t seq;        /* little-endian */
} axiom_event_header_t;

AXIOM_CHECK_SIZE(axiom_event_header_t, 8);
AXIOM_CHECK_ALIGN(axiom_event_header_t, 1);

/* ---------------------------------------------------------------------------
 * Payload type tags (self-describing)
 * --------------------------------------------------------------------------- */
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

/* Reserved for future standard types: 0x0A - 0x7F */
/* User-defined types: 0x80 - 0xFF */

/* ---------------------------------------------------------------------------
 * Wire format version v1.0
 * --------------------------------------------------------------------------- */
#define AXIOM_WIRE_VERSION_MAJOR 1u
#define AXIOM_WIRE_VERSION_MINOR 0u
#define AXIOM_WIRE_VERSION ((uint8_t)((AXIOM_WIRE_VERSION_MAJOR << 4u) | AXIOM_WIRE_VERSION_MINOR))

/* ---------------------------------------------------------------------------
 * Frame limits (AXIOM_MAX_PAYLOAD_LEN defined in axiom_config.h)
 * --------------------------------------------------------------------------- */

/* Max timestamp length is 5 bytes (0xFE + 4-byte full delta) */
#define AXIOM_MAX_FRAME_LEN (8u + 5u + 1u + AXIOM_MAX_PAYLOAD_LEN + 2u)

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_EVENT_H */
