#ifndef AXIOM_CONFIG_H
#define AXIOM_CONFIG_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Library Version (SemVer 2.0.0)
 * Bump on release. Checked at compile-time by downstream consumers.
 * --------------------------------------------------------------------------- */
#define AXIOMTRACE_VERSION_MAJOR 0u
#define AXIOMTRACE_VERSION_MINOR 7u
#define AXIOMTRACE_VERSION_PATCH 0u

/* Compile-time version check: AXIOMTRACE_VERSION_CHECK(0,7,0) */
#define AXIOMTRACE_VERSION_CHECK(ma, mi, pa) \
    ((AXIOMTRACE_VERSION_MAJOR > (ma)) ||    \
     (AXIOMTRACE_VERSION_MAJOR == (ma) && AXIOMTRACE_VERSION_MINOR > (mi)) || \
     (AXIOMTRACE_VERSION_MAJOR == (ma) && AXIOMTRACE_VERSION_MINOR == (mi) && \
      AXIOMTRACE_VERSION_PATCH >= (pa)))

/* ---------------------------------------------------------------------------
 * Compiler Helpers
 * --------------------------------------------------------------------------- */

/* Mark a symbol as deprecated with a migration hint.
 * Example: AXIOM_DEPRECATED("use axiom_new_api() instead") */
#if defined(__GNUC__) || defined(__clang__)
#define AXIOM_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#define AXIOM_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__IAR_SYSTEMS_ICC__)
#define AXIOM_DEPRECATED(msg) _Pragma("message=\"" msg "\"")
#else
#define AXIOM_DEPRECATED(msg)
#endif

/* ---------------------------------------------------------------------------
 * Debug Switch (NEW)
 * Enables runtime assertions and diagnostic hooks when set to 1.
 * Cost: ~50 bytes ROM for assert handlers. Disable in production.
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_DEBUG
#define AXIOM_DEBUG 0
#endif

/* Source-location metadata modes. Metadata is a tagged suffix in the wire v2
 * packed payload; the fixed 8-byte frame header is unchanged. */
#define AXIOM_CFG_LOCATION_MODE_NONE    0
#define AXIOM_CFG_LOCATION_MODE_HASH    1
#define AXIOM_CFG_LOCATION_MODE_FILE_ID 2

/* ---------------------------------------------------------------------------
 * Resource Presets
 * A single knob for common MCU classes. Every value remains individually
 * overridable by defining the specific macro before including axiomtrace.h or
 * by passing a compile definition from the build system.
 * --------------------------------------------------------------------------- */
#define AXIOM_PROFILE_DEV    0
#define AXIOM_PROFILE_FIELD  1
#define AXIOM_PROFILE_PROD   2

#define AXIOM_PRESET_CUSTOM 0
#define AXIOM_PRESET_TINY   1
#define AXIOM_PRESET_PROD   2
#define AXIOM_PRESET_FIELD  3
#define AXIOM_PRESET_DEV    4

#ifndef AXIOM_PRESET
#define AXIOM_PRESET AXIOM_PRESET_DEV
#endif

#if (AXIOM_PRESET < AXIOM_PRESET_CUSTOM) || (AXIOM_PRESET > AXIOM_PRESET_DEV)
#error "Invalid AXIOM_PRESET. Use CUSTOM, TINY, PROD, FIELD, or DEV."
#endif

#if AXIOM_PRESET == AXIOM_PRESET_TINY
    #ifndef AXIOM_PROFILE
    #define AXIOM_PROFILE AXIOM_PROFILE_PROD
    #endif
    #ifndef AXIOM_RING_BUFFER_SIZE
    #define AXIOM_RING_BUFFER_SIZE 256u
    #endif
    #ifndef AXIOM_MAX_PAYLOAD_LEN
    #define AXIOM_MAX_PAYLOAD_LEN 32u
    #endif
    #ifndef AXIOM_MODULE_MAX
    #define AXIOM_MODULE_MAX 16u
    #endif
    #ifndef AXIOM_CFG_LOCATION_MODE
    #define AXIOM_CFG_LOCATION_MODE AXIOM_CFG_LOCATION_MODE_NONE
    #endif
    #ifndef AXIOM_CFG_TIME_SYNC_ENABLED
    #define AXIOM_CFG_TIME_SYNC_ENABLED 0
    #endif
    #ifndef AXIOM_SHORT_CS
    #define AXIOM_SHORT_CS 0
    #endif
    #ifndef AXIOM_BACKEND_DEGRADATION
    #define AXIOM_BACKEND_DEGRADATION 0
    #endif
    #ifndef AXIOM_CAPSULE_ENABLED
    #define AXIOM_CAPSULE_ENABLED 0
    #endif
#elif AXIOM_PRESET == AXIOM_PRESET_PROD
    #ifndef AXIOM_PROFILE
    #define AXIOM_PROFILE AXIOM_PROFILE_PROD
    #endif
    #ifndef AXIOM_RING_BUFFER_SIZE
    #define AXIOM_RING_BUFFER_SIZE 1024u
    #endif
    #ifndef AXIOM_MAX_PAYLOAD_LEN
    #define AXIOM_MAX_PAYLOAD_LEN 64u
    #endif
    #ifndef AXIOM_CFG_LOCATION_MODE
    #define AXIOM_CFG_LOCATION_MODE AXIOM_CFG_LOCATION_MODE_NONE
    #endif
    #ifndef AXIOM_CFG_TIME_SYNC_ENABLED
    #define AXIOM_CFG_TIME_SYNC_ENABLED 0
    #endif
    #ifndef AXIOM_CAPSULE_ENABLED
    #define AXIOM_CAPSULE_ENABLED 0
    #endif
#elif AXIOM_PRESET == AXIOM_PRESET_FIELD
    #ifndef AXIOM_PROFILE
    #define AXIOM_PROFILE AXIOM_PROFILE_FIELD
    #endif
    #ifndef AXIOM_RING_BUFFER_SIZE
    #define AXIOM_RING_BUFFER_SIZE 2048u
    #endif
    #ifndef AXIOM_MAX_PAYLOAD_LEN
    #define AXIOM_MAX_PAYLOAD_LEN 96u
    #endif
    #ifndef AXIOM_CAPSULE_PRE_EVENTS
    #define AXIOM_CAPSULE_PRE_EVENTS 16u
    #endif
    #ifndef AXIOM_CAPSULE_POST_EVENTS
    #define AXIOM_CAPSULE_POST_EVENTS 8u
    #endif
    #ifndef AXIOM_CAPSULE_RING_SIZE
    #define AXIOM_CAPSULE_RING_SIZE 2048u
    #endif
#elif AXIOM_PRESET == AXIOM_PRESET_DEV
    #ifndef AXIOM_PROFILE
    #define AXIOM_PROFILE AXIOM_PROFILE_DEV
    #endif
#endif

/* ---------------------------------------------------------------------------
 * Core Configuration
 * --------------------------------------------------------------------------- */

/* Main ring buffer size. MUST be a power of two. */
#ifndef AXIOM_RING_BUFFER_SIZE
#define AXIOM_RING_BUFFER_SIZE 4096u
#endif

/* DROP: Discard new events when full. 
 * OVERWRITE: Overwrite oldest events when full (maintains O(1)). */
#define AXIOM_RING_BUFFER_POLICY_DROP      0
#define AXIOM_RING_BUFFER_POLICY_OVERWRITE 1

#ifndef AXIOM_RING_BUFFER_POLICY
#define AXIOM_RING_BUFFER_POLICY AXIOM_RING_BUFFER_POLICY_DROP
#endif

/* Maximum payload size per event in bytes. */
#ifndef AXIOM_MAX_PAYLOAD_LEN
#define AXIOM_MAX_PAYLOAD_LEN 128u
#endif

/* Maximum number of unique module IDs (0 .. AXIOM_MODULE_MAX-1).
 * Determines the width of the module filter bitmask. */
#ifndef AXIOM_MODULE_MAX
#define AXIOM_MODULE_MAX 32u
#endif

/* ---------------------------------------------------------------------------
 * Observability Extensions
 * --------------------------------------------------------------------------- */

/* Compatibility switch retained for existing integrations. New code should
 * select AXIOM_CFG_LOCATION_MODE explicitly. */
#ifndef AXIOM_CFG_USE_LOCATION
#define AXIOM_CFG_USE_LOCATION 0
#endif

#ifndef AXIOM_CFG_LOCATION_MODE
#if AXIOM_CFG_USE_LOCATION
#define AXIOM_CFG_LOCATION_MODE AXIOM_CFG_LOCATION_MODE_HASH
#else
#define AXIOM_CFG_LOCATION_MODE AXIOM_CFG_LOCATION_MODE_NONE
#endif
#endif

/* Include a function hash only in HASH mode. Production FILE_ID mode restores
 * function information on the host from retained build artifacts. */
#ifndef AXIOM_CFG_LOCATION_FUNCTION
#define AXIOM_CFG_LOCATION_FUNCTION 0
#endif

/* FILE_ID mode callers may override this per translation unit. */
#ifndef AXIOM_SOURCE_FILE_ID
#define AXIOM_SOURCE_FILE_ID 0u
#endif

/* If enabled, the core will periodically (or manually) emit sync events
 * to map local counters to host-provided Unix time. */
#ifndef AXIOM_CFG_TIME_SYNC_ENABLED
#define AXIOM_CFG_TIME_SYNC_ENABLED 1
#endif

/* ---------------------------------------------------------------------------
 * Encode Overflow Detection (optimization #10)
 * When enabled, axiom_enc_xxx() silently skips writes that would exceed
 * AXIOM_MAX_PAYLOAD_LEN and sets axiom_encode_overflow=true.
 * Cost: 1 byte RAM + 1 branch per enc call (mostly predicted-not-taken).
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_ENCODE_OVERFLOW_DETECTION
#define AXIOM_ENCODE_OVERFLOW_DETECTION 1
#endif

/* ---------------------------------------------------------------------------
 * Short Critical Section (optimization #1)
 * When enabled (default), axiom_write() pre-encodes the entire frame into
 * a stack buffer OUTSIDE the critical section, then performs a single
 * ring_write() INSIDE the critical section. Minimizes interrupt latency.
 * Cost: +AXIOM_MAX_FRAME_LEN bytes of stack per axiom_write() call.
 * Set to 0 on extremely RAM-constrained MCUs to save ~255B stack.
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_SHORT_CS
#define AXIOM_SHORT_CS 1
#endif

/* ---------------------------------------------------------------------------
 * Runtime Self-Test (NEW)
 * When enabled, axiom_selftest() validates CRC, ring buffer, and encoder
 * at boot time. Returns true if all checks pass.
 * Cost: ~200B stack + ~2KB ROM for test vectors. Call axiom_selftest()
 * once in main() before first axiom_write().
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_SELFTEST
#define AXIOM_SELFTEST 0
#endif

/* ---------------------------------------------------------------------------
 * Backend Degradation & Recovery (NEW)
 * When enabled, a backend that fails AXIOM_BACKEND_FAIL_THRESHOLD
 * consecutive writes is soft-disabled for AXIOM_BACKEND_RECOVERY_US
 * microseconds, then auto-recovered. Prevents a broken transport
 * (e.g., unplugged UART) from stalling all backends.
 * Panic path always bypasses degradation.
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_BACKEND_DEGRADATION
#define AXIOM_BACKEND_DEGRADATION 1
#endif

#if AXIOM_BACKEND_DEGRADATION
    #ifndef AXIOM_BACKEND_FAIL_THRESHOLD
    #define AXIOM_BACKEND_FAIL_THRESHOLD 5
    #endif
    #ifndef AXIOM_BACKEND_RECOVERY_US
    #define AXIOM_BACKEND_RECOVERY_US 1000000u  /* 1 second */
    #endif
#endif

/* ---------------------------------------------------------------------------
 * Fault Capsule
 * Captures pre/post fault Event Records into RAM and commits a CRC-protected
 * capsule image to the port flash backend only when axiom_capsule_commit()
 * is called from a non-hot path.
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_CAPSULE_ENABLED
#define AXIOM_CAPSULE_ENABLED 1
#endif

#ifndef AXIOM_CAPSULE_PRE_EVENTS
#define AXIOM_CAPSULE_PRE_EVENTS 32u
#endif

#ifndef AXIOM_CAPSULE_POST_EVENTS
#define AXIOM_CAPSULE_POST_EVENTS 16u
#endif

#ifndef AXIOM_CAPSULE_RING_SIZE
#define AXIOM_CAPSULE_RING_SIZE 4096u
#endif

#ifndef AXIOM_CAPSULE_MAX_SNAPSHOT_LEN
#define AXIOM_CAPSULE_MAX_SNAPSHOT_LEN 64u
#endif

#ifndef AXIOM_CAPSULE_FLASH_BASE
#define AXIOM_CAPSULE_FLASH_BASE 0u
#endif

#ifndef AXIOM_CAPSULE_FLASH_SIZE
#define AXIOM_CAPSULE_FLASH_SIZE 8192u
#endif

#ifndef AXIOM_CAPSULE_FIRMWARE_HASH
#define AXIOM_CAPSULE_FIRMWARE_HASH 0u
#endif

#ifndef AXIOM_CAPSULE_SNAPSHOT_ID
#define AXIOM_CAPSULE_SNAPSHOT_ID 0u
#endif

#define AXIOM_CAPSULE_MAX_IMAGE_SIZE \
    (21u + AXIOM_CAPSULE_MAX_SNAPSHOT_LEN + AXIOM_CAPSULE_RING_SIZE + 4u)

#endif /* AXIOM_CONFIG_H */
