#ifndef AXIOM_CONFIG_H
#define AXIOM_CONFIG_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Library Version (SemVer 2.0.0)
 * Bump on release. Checked at compile-time by downstream consumers.
 * --------------------------------------------------------------------------- */
#define AXIOMTRACE_VERSION_MAJOR 0u
#define AXIOMTRACE_VERSION_MINOR 1u
#define AXIOMTRACE_VERSION_PATCH 0u

/* Compile-time version check: AXIOMTRACE_VERSION_CHECK(0,1,0) */
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

/* If enabled, every AX_EVT will automatically append __LINE__ and a hash of __FILE__.
 * Adds approximately 4 bytes of payload per event. */
#ifndef AXIOM_CFG_USE_LOCATION
#define AXIOM_CFG_USE_LOCATION 0
#endif

/* If enabled, the core will periodically (or manually) emit sync events 
 * to map local counters to host-provided Unix time. */
#ifndef AXIOM_CFG_TIME_SYNC_ENABLED
#define AXIOM_CFG_TIME_SYNC_ENABLED 1
#endif

#endif /* AXIOM_CONFIG_H */
