#ifndef AXIOM_SELFTEST_H
#define AXIOM_SELFTEST_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Runtime Self-Test — verifies core subsystems at boot time.
 *
 * Enable via AXIOM_SELFTEST=1 in axiom_config.h.
 * Returns true if all checks pass, false if any subsystem is broken.
 *
 * Estimated cost: ~200B stack, ~2KB ROM (for test vectors), ~500 cycles.
 * Should be called once in axiom_init() or main() before first axiom_write().
 * --------------------------------------------------------------------------- */

bool axiom_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_SELFTEST_H */