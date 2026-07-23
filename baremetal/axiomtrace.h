/* ============================================================================
 * AxiomTrace public modular header
 * ============================================================================
 * Include this file to get all public APIs:
 *   - Core:     axiom_init(), axiom_write()
 *   - Frontend: AX_LOG, AX_EVT, AX_PROBE, AX_FAULT, AX_KV
 *   - Backend:  axiom_backend_register()
 *   - Port:     axiom_port_* (override weak defaults)
 *
 * For the true single-file library (amalgamated), use the generated
 * axiomtrace.h under dist/.
 * ============================================================================ */

#ifndef AXIOMTRACE_H
#define AXIOMTRACE_H

#include "axiom_config.h"
#include "axiom_static_assert.h"
#include "axiom_port.h"
#include "axiom_event.h"
#include "axiom_diagnostics.h"
#include "axiom_encode.h"
#include "axiom_crc.h"
#include "axiom_ring.h"
#include "axiom_timestamp.h"
#include "axiom_filter.h"
#include "axiom_capsule.h"
#include "axiom_frontend.h"
#include "axiom_backend.h"
#include "axiom_backend_deferred.h"
#include "axiom_selftest.h"

#endif /* AXIOMTRACE_H */
