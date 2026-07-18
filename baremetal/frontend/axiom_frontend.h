#ifndef AXIOM_FRONTEND_H
#define AXIOM_FRONTEND_H

#include "axiom_event.h"
#include "axiom_encode.h"
#include "axiom_port.h"
#include "axiom_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Profile control
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_PROFILE_DEV
#define AXIOM_PROFILE_DEV    0
#endif
#ifndef AXIOM_PROFILE_FIELD
#define AXIOM_PROFILE_FIELD  1
#endif
#ifndef AXIOM_PROFILE_PROD
#define AXIOM_PROFILE_PROD   2
#endif

#ifndef AXIOM_PROFILE
#define AXIOM_PROFILE AXIOM_PROFILE_DEV
#endif

/* ---------------------------------------------------------------------------
 * Variadic argument counting (GCC/Clang/MSVC compatible)
 * --------------------------------------------------------------------------- */
#define AXIOM_INTERNAL_ARG_N(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, count, ...) count
#define AXIOM_INTERNAL_NARG_EXPAND(...) AXIOM_INTERNAL_ARG_N(__VA_ARGS__)
#define AXIOM_INTERNAL_NARG(...) AXIOM_INTERNAL_NARG_EXPAND(0, ##__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define AXIOM_INTERNAL_CONCAT_RAW(a, b) a ## b
#define AXIOM_INTERNAL_CONCAT(a, b) AXIOM_INTERNAL_CONCAT_RAW(a, b)

/* ---------------------------------------------------------------------------
 * Core write declaration (defined in axiom_event.c)
 * --------------------------------------------------------------------------- */
void axiom_write(axiom_level_t level, uint8_t module_id, uint16_t event_id,
                 const uint8_t *payload, uint8_t payload_len);
void axiom_init(void);
void axiom_flush(void);
void axiom_internal_record_encode_drop(uint8_t module_id, uint16_t event_id);

/* Runtime filter control — declared in axiom_filter.h, exposed here for convenience.
 * void axiom_level_mask_set(uint32_t mask);
 * uint32_t axiom_level_mask_get(void);
 * void axiom_module_mask_set(uint32_t mask);
 * uint32_t axiom_module_mask_get(void);
 */

/* ---------------------------------------------------------------------------
 * FNV-1a 16-bit hash (runtime, for key hashing)
 * --------------------------------------------------------------------------- */
static inline uint16_t axiom_internal_fnv1a_16(const char *s) {
    uint16_t h = 0x811cu;
    while (*s) {
        h ^= (uint16_t)(unsigned char)(*s++);
        h *= 0x0103u;
    }
    return h;
}

/* ---------------------------------------------------------------------------
 * Optional source-location metadata
 * --------------------------------------------------------------------------- */
#if AXIOM_CFG_LOCATION_MODE == AXIOM_CFG_LOCATION_MODE_FILE_ID
#define AXIOM_INTERNAL_APPEND_LOCATION(builder) \
    axiom_builder_location_file_id(&(builder), (uint16_t)AXIOM_SOURCE_FILE_ID, (uint16_t)__LINE__)
#elif AXIOM_CFG_LOCATION_MODE == AXIOM_CFG_LOCATION_MODE_HASH
#if AXIOM_CFG_LOCATION_FUNCTION
#define AXIOM_INTERNAL_APPEND_LOCATION(builder) \
    axiom_builder_location_hash(&(builder), axiom_internal_fnv1a_16(__FILE__), (uint16_t)__LINE__, axiom_internal_fnv1a_16(__func__))
#else
#define AXIOM_INTERNAL_APPEND_LOCATION(builder) \
    axiom_builder_location_hash(&(builder), axiom_internal_fnv1a_16(__FILE__), (uint16_t)__LINE__, 0u)
#endif
#else
#define AXIOM_INTERNAL_APPEND_LOCATION(builder) ((void)(builder))
#endif

#define AXIOM_INTERNAL_DECLARE_BUILDER() \
    uint8_t axiom_internal_buffer[AXIOM_MAX_PAYLOAD_LEN]; \
    axiom_payload_builder_t axiom_internal_builder; \
    axiom_payload_builder_init(&axiom_internal_builder, axiom_internal_buffer)

#define AXIOM_INTERNAL_EMIT_BUILDER(level, mod, evt) \
    do { \
        AXIOM_INTERNAL_APPEND_LOCATION(axiom_internal_builder); \
        if (axiom_internal_builder.valid) { \
            axiom_write((level), (mod), (evt), axiom_internal_builder.data, \
                        (uint8_t)axiom_internal_builder.pos); \
        } else { \
            axiom_internal_record_encode_drop((uint8_t)(mod), (uint16_t)(evt)); \
        } \
    } while (0)

/* System-reserved metadata identity event used by host bundle-store selection. */
#define AXIOM_SYSTEM_MODULE_ID          0x00u
#define AXIOM_SYSTEM_EVENT_DROP_SUMMARY 0x0001u
#define AXIOM_SYSTEM_EVENT_METADATA_ID  0x0002u

static inline void axiom_emit_metadata_id(const uint8_t metadata_id[AXIOM_METADATA_ID_LEN]) {
    AXIOM_INTERNAL_DECLARE_BUILDER();
    axiom_builder_metadata_identity(&axiom_internal_builder, metadata_id);
    if (axiom_internal_builder.valid) {
        axiom_write(AXIOM_LEVEL_INFO, AXIOM_SYSTEM_MODULE_ID,
                    AXIOM_SYSTEM_EVENT_METADATA_ID, axiom_internal_builder.data,
                    (uint8_t)axiom_internal_builder.pos);
    } else {
        axiom_internal_record_encode_drop(AXIOM_SYSTEM_MODULE_ID,
                                          AXIOM_SYSTEM_EVENT_METADATA_ID);
    }
}

/* ---------------------------------------------------------------------------
 * AX_EVT — Structured Event (always compiled, all profiles)
 * --------------------------------------------------------------------------- */
#if AXIOM_CFG_LOCATION_MODE == AXIOM_CFG_LOCATION_MODE_NONE
#define AXIOM_INTERNAL_EVT_0(level, mod, evt) \
    axiom_write((level), (mod), (evt), NULL, 0)
#else
#define AXIOM_INTERNAL_EVT_0(level, mod, evt) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)
#endif

#define AXIOM_INTERNAL_EVT_1(level, mod, evt, a1) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a1); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AXIOM_INTERNAL_EVT_2(level, mod, evt, a1, a2) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a1); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a2); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AXIOM_INTERNAL_EVT_3(level, mod, evt, a1, a2, a3) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a1); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a2); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a3); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AXIOM_INTERNAL_EVT_4(level, mod, evt, a1, a2, a3, a4) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a1); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a2); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a3); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a4); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AXIOM_INTERNAL_EVT_5(level, mod, evt, a1, a2, a3, a4, a5) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a1); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a2); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a3); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a4); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a5); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AXIOM_INTERNAL_EVT_6(level, mod, evt, a1, a2, a3, a4, a5, a6) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a1); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a2); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a3); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a4); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a5); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a6); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AXIOM_INTERNAL_EVT_7(level, mod, evt, a1, a2, a3, a4, a5, a6, a7) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a1); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a2); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a3); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a4); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a5); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a6); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a7); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AXIOM_INTERNAL_EVT_8(level, mod, evt, a1, a2, a3, a4, a5, a6, a7, a8) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a1); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a2); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a3); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a4); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a5); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a6); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a7); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(axiom_internal_builder, a8); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AXIOM_INTERNAL_EVT_DISPATCH(level, mod, evt, ...) \
    AXIOM_INTERNAL_CONCAT(AXIOM_INTERNAL_EVT_, AXIOM_INTERNAL_NARG(__VA_ARGS__))(level, mod, evt, ##__VA_ARGS__)

#define AX_EVT(level, mod, evt, ...) \
    AXIOM_INTERNAL_EVT_DISPATCH(AXIOM_LEVEL_##level, mod, evt, ##__VA_ARGS__)

#define AX_FAULT(mod, evt, ...) \
    AXIOM_INTERNAL_EVT_DISPATCH(AXIOM_LEVEL_FAULT, mod, evt, ##__VA_ARGS__)

/* ---------------------------------------------------------------------------
 * AX_LOG — Development logging (PROD profile = no-op)
 * Outputs plain text via axiom_port_string_out(). Does NOT enter binary protocol.
 * --------------------------------------------------------------------------- */
#if (AXIOM_PROFILE == AXIOM_PROFILE_DEV) || (AXIOM_PROFILE == AXIOM_PROFILE_FIELD)

#define AX_LOG(msg)         do { axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)
#define AX_LOG_DEBUG(msg)   do { axiom_port_string_out("[DEBUG] "); axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)
#define AX_LOG_INFO(msg)    do { axiom_port_string_out("[INFO] ");  axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)
#define AX_LOG_WARN(msg)    do { axiom_port_string_out("[WARN] ");  axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)
#define AX_LOG_ERROR(msg)   do { axiom_port_string_out("[ERROR] "); axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)

#else /* PROD */

#define AX_LOG(msg)         ((void)0)
#define AX_LOG_DEBUG(msg)   ((void)0)
#define AX_LOG_INFO(msg)    ((void)0)
#define AX_LOG_WARN(msg)    ((void)0)
#define AX_LOG_ERROR(msg)   ((void)0)

#endif

/* ---------------------------------------------------------------------------
 * AX_PROBE — Timing probe (PROD profile = no-op)
 * Reserved system event with typed fields: its varying value is intentionally
 * decodable without an application dictionary under wire v2.
 * --------------------------------------------------------------------------- */
#if (AXIOM_PROFILE == AXIOM_PROFILE_DEV) || (AXIOM_PROFILE == AXIOM_PROFILE_FIELD)

#define AX_PROBE(tag, value) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         uint16_t axiom_internal_tag_hash = axiom_internal_fnv1a_16(tag); \
         axiom_builder_tagged_u16(&axiom_internal_builder, axiom_internal_tag_hash); \
         AXIOM_INTERNAL_BUILDER_ENCODE_TAGGED_ONE(axiom_internal_builder, value); \
         if (axiom_internal_builder.valid) { \
             axiom_write(AXIOM_LEVEL_DEBUG, 0, 0, axiom_internal_builder.data, \
                         (uint8_t)axiom_internal_builder.pos); \
         } else { axiom_internal_record_encode_drop(0u, 0u); } } while(0)

#else /* PROD */

#define AX_PROBE(tag, value) ((void)0)

#endif

/* ---------------------------------------------------------------------------
 * AX_KV — Key-Value event (always compiled)
 * Keys are hashed at compile-time to 16-bit IDs via FNV-1a.
 * --------------------------------------------------------------------------- */
#define AXIOM_INTERNAL_KV_KEYVAL(builder, key, val) \
    do { uint16_t axiom_internal_key_hash = axiom_internal_fnv1a_16(key); \
         axiom_builder_u16(&(builder), axiom_internal_key_hash); \
         AXIOM_INTERNAL_BUILDER_ENCODE_ONE(builder, val); \
    } while(0)

#define AXIOM_INTERNAL_KV_2(level, mod, evt, k1, v1) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_KV_KEYVAL(axiom_internal_builder, k1, v1); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AXIOM_INTERNAL_KV_4(level, mod, evt, k1, v1, k2, v2) \
    do { AXIOM_INTERNAL_DECLARE_BUILDER(); \
         AXIOM_INTERNAL_KV_KEYVAL(axiom_internal_builder, k1, v1); \
         AXIOM_INTERNAL_KV_KEYVAL(axiom_internal_builder, k2, v2); \
         AXIOM_INTERNAL_EMIT_BUILDER((level), (mod), (evt)); } while(0)

#define AX_KV(level, mod, evt, ...) \
    AXIOM_INTERNAL_CONCAT(AXIOM_INTERNAL_KV_, AXIOM_INTERNAL_NARG(__VA_ARGS__))(AXIOM_LEVEL_##level, mod, evt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_FRONTEND_H */
