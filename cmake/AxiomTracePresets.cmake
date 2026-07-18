# ============================================================================
# AxiomTrace Preset Configuration
# ============================================================================
#
# Centralized preset definitions. Each preset maps to a set of compile
# definitions that control the library's behavior and resource usage.
#
# Usage:
#   include(AxiomTracePresets)
#   axiom_apply_preset(axiomtrace tiny)
#   axiom_get_preset_values(tiny PROFILE RING_BUFFER_SIZE MAX_PAYLOAD_LEN ...)
#
# ============================================================================

# Preset value definitions
# Format: set(AXIOM_PRESET_<name>_<macro> <value>)

# --- CUSTOM (0) ---
# No overrides - user provides all values individually

# --- TINY (1) ---
set(AXIOM_PRESET_tiny_PRESET AXIOM_PRESET_TINY)
set(AXIOM_PRESET_tiny_PROFILE AXIOM_PROFILE_PROD)
set(AXIOM_PRESET_tiny_RING_BUFFER_SIZE 256u)
set(AXIOM_PRESET_tiny_MAX_PAYLOAD_LEN 32u)
set(AXIOM_PRESET_tiny_MODULE_MAX 16u)
set(AXIOM_PRESET_tiny_LOCATION_MODE AXIOM_CFG_LOCATION_MODE_NONE)
set(AXIOM_PRESET_tiny_TIME_SYNC_ENABLED 0)
set(AXIOM_PRESET_tiny_SHORT_CS 0)
set(AXIOM_PRESET_tiny_BACKEND_DEGRADATION 0)
set(AXIOM_PRESET_tiny_CAPSULE_ENABLED 0)
set(AXIOM_PRESET_tiny_CAPSULE_PRE_EVENTS 32u)
set(AXIOM_PRESET_tiny_CAPSULE_POST_EVENTS 16u)
set(AXIOM_PRESET_tiny_CAPSULE_RING_SIZE 4096u)
set(AXIOM_PRESET_tiny_RING_BUFFER_POLICY 0)
set(AXIOM_PRESET_tiny_ENCODE_OVERFLOW_DETECTION 1)
set(AXIOM_PRESET_tiny_SELFTEST 0)
set(AXIOM_PRESET_tiny_DEBUG 0)
set(AXIOM_PRESET_tiny_BACKEND_FAIL_THRESHOLD 5)
set(AXIOM_PRESET_tiny_BACKEND_RECOVERY_US 1000000u)
set(AXIOM_PRESET_tiny_CAPSULE_MAX_SNAPSHOT_LEN 64u)
set(AXIOM_PRESET_tiny_CAPSULE_FLASH_SIZE 8192u)

# --- PROD (2) ---
set(AXIOM_PRESET_prod_PRESET AXIOM_PRESET_PROD)
set(AXIOM_PRESET_prod_PROFILE AXIOM_PROFILE_PROD)
set(AXIOM_PRESET_prod_RING_BUFFER_SIZE 1024u)
set(AXIOM_PRESET_prod_MAX_PAYLOAD_LEN 64u)
set(AXIOM_PRESET_prod_MODULE_MAX 32u)
set(AXIOM_PRESET_prod_LOCATION_MODE AXIOM_CFG_LOCATION_MODE_NONE)
set(AXIOM_PRESET_prod_TIME_SYNC_ENABLED 0)
set(AXIOM_PRESET_prod_SHORT_CS 1)
set(AXIOM_PRESET_prod_BACKEND_DEGRADATION 1)
set(AXIOM_PRESET_prod_CAPSULE_ENABLED 0)
set(AXIOM_PRESET_prod_CAPSULE_PRE_EVENTS 32u)
set(AXIOM_PRESET_prod_CAPSULE_POST_EVENTS 16u)
set(AXIOM_PRESET_prod_CAPSULE_RING_SIZE 4096u)
set(AXIOM_PRESET_prod_RING_BUFFER_POLICY 0)
set(AXIOM_PRESET_prod_ENCODE_OVERFLOW_DETECTION 1)
set(AXIOM_PRESET_prod_SELFTEST 0)
set(AXIOM_PRESET_prod_DEBUG 0)
set(AXIOM_PRESET_prod_BACKEND_FAIL_THRESHOLD 5)
set(AXIOM_PRESET_prod_BACKEND_RECOVERY_US 1000000u)
set(AXIOM_PRESET_prod_CAPSULE_MAX_SNAPSHOT_LEN 64u)
set(AXIOM_PRESET_prod_CAPSULE_FLASH_SIZE 8192u)

# --- FIELD (3) ---
set(AXIOM_PRESET_field_PRESET AXIOM_PRESET_FIELD)
set(AXIOM_PRESET_field_PROFILE AXIOM_PROFILE_FIELD)
set(AXIOM_PRESET_field_RING_BUFFER_SIZE 2048u)
set(AXIOM_PRESET_field_MAX_PAYLOAD_LEN 96u)
set(AXIOM_PRESET_field_MODULE_MAX 32u)
set(AXIOM_PRESET_field_LOCATION_MODE AXIOM_CFG_LOCATION_MODE_NONE)
set(AXIOM_PRESET_field_TIME_SYNC_ENABLED 1)
set(AXIOM_PRESET_field_SHORT_CS 1)
set(AXIOM_PRESET_field_BACKEND_DEGRADATION 1)
set(AXIOM_PRESET_field_CAPSULE_ENABLED 1)
set(AXIOM_PRESET_field_CAPSULE_PRE_EVENTS 16u)
set(AXIOM_PRESET_field_CAPSULE_POST_EVENTS 8u)
set(AXIOM_PRESET_field_CAPSULE_RING_SIZE 2048u)
set(AXIOM_PRESET_field_RING_BUFFER_POLICY 0)
set(AXIOM_PRESET_field_ENCODE_OVERFLOW_DETECTION 1)
set(AXIOM_PRESET_field_SELFTEST 0)
set(AXIOM_PRESET_field_DEBUG 0)
set(AXIOM_PRESET_field_BACKEND_FAIL_THRESHOLD 5)
set(AXIOM_PRESET_field_BACKEND_RECOVERY_US 1000000u)
set(AXIOM_PRESET_field_CAPSULE_MAX_SNAPSHOT_LEN 64u)
set(AXIOM_PRESET_field_CAPSULE_FLASH_SIZE 8192u)

# --- DEV (4) ---
set(AXIOM_PRESET_dev_PRESET AXIOM_PRESET_DEV)
set(AXIOM_PRESET_dev_PROFILE AXIOM_PROFILE_DEV)
set(AXIOM_PRESET_dev_RING_BUFFER_SIZE 4096u)
set(AXIOM_PRESET_dev_MAX_PAYLOAD_LEN 128u)
set(AXIOM_PRESET_dev_MODULE_MAX 32u)
set(AXIOM_PRESET_dev_LOCATION_MODE AXIOM_CFG_LOCATION_MODE_NONE)
set(AXIOM_PRESET_dev_TIME_SYNC_ENABLED 1)
set(AXIOM_PRESET_dev_SHORT_CS 1)
set(AXIOM_PRESET_dev_BACKEND_DEGRADATION 1)
set(AXIOM_PRESET_dev_CAPSULE_ENABLED 1)
set(AXIOM_PRESET_dev_CAPSULE_PRE_EVENTS 32u)
set(AXIOM_PRESET_dev_CAPSULE_POST_EVENTS 16u)
set(AXIOM_PRESET_dev_CAPSULE_RING_SIZE 4096u)
set(AXIOM_PRESET_dev_RING_BUFFER_POLICY 0)
set(AXIOM_PRESET_dev_ENCODE_OVERFLOW_DETECTION 1)
set(AXIOM_PRESET_dev_SELFTEST 0)
set(AXIOM_PRESET_dev_DEBUG 0)
set(AXIOM_PRESET_dev_BACKEND_FAIL_THRESHOLD 5)
set(AXIOM_PRESET_dev_BACKEND_RECOVERY_US 1000000u)
set(AXIOM_PRESET_dev_CAPSULE_MAX_SNAPSHOT_LEN 64u)
set(AXIOM_PRESET_dev_CAPSULE_FLASH_SIZE 8192u)

# List of all presets (excluding custom which has no predefined values)
set(AXIOM_PRESETS tiny prod field dev)

# ============================================================================
# axiom_apply_preset(target preset)
#
# Apply preset compile definitions to a target.
# ============================================================================
function(axiom_apply_preset target preset)
    string(TOLOWER "${preset}" preset_lower)

    if(preset_lower STREQUAL "custom")
        return()
    endif()

    if(NOT DEFINED AXIOM_PRESET_${preset_lower}_PRESET)
        message(FATAL_ERROR "Unknown preset: '${preset}'. Valid: custom, ${AXIOM_PRESETS}")
    endif()

    # 短名称 → C 宏全名映射表
    # 修复：原实现直接传递 RING_BUFFER_SIZE 等短名称，与 C 代码中的
    # AXIOM_RING_BUFFER_SIZE 不匹配，导致预设值未生效。
    set(_macro_map
        PROFILE:AXIOM_PROFILE
        RING_BUFFER_SIZE:AXIOM_RING_BUFFER_SIZE
        MAX_PAYLOAD_LEN:AXIOM_MAX_PAYLOAD_LEN
        MODULE_MAX:AXIOM_MODULE_MAX
        LOCATION_MODE:AXIOM_CFG_LOCATION_MODE
        TIME_SYNC_ENABLED:AXIOM_CFG_TIME_SYNC_ENABLED
        SHORT_CS:AXIOM_SHORT_CS
        BACKEND_DEGRADATION:AXIOM_BACKEND_DEGRADATION
        CAPSULE_ENABLED:AXIOM_CAPSULE_ENABLED
        CAPSULE_PRE_EVENTS:AXIOM_CAPSULE_PRE_EVENTS
        CAPSULE_POST_EVENTS:AXIOM_CAPSULE_POST_EVENTS
        CAPSULE_RING_SIZE:AXIOM_CAPSULE_RING_SIZE
        RING_BUFFER_POLICY:AXIOM_RING_BUFFER_POLICY
        ENCODE_OVERFLOW_DETECTION:AXIOM_ENCODE_OVERFLOW_DETECTION
        BACKEND_FAIL_THRESHOLD:AXIOM_BACKEND_FAIL_THRESHOLD
        BACKEND_RECOVERY_US:AXIOM_BACKEND_RECOVERY_US
        CAPSULE_MAX_SNAPSHOT_LEN:AXIOM_CAPSULE_MAX_SNAPSHOT_LEN
        CAPSULE_FLASH_SIZE:AXIOM_CAPSULE_FLASH_SIZE
    )

    foreach(pair ${_macro_map})
        string(REPLACE ":" ";" _parts "${pair}")
        list(GET _parts 0 _short)
        list(GET _parts 1 _full)
        if(DEFINED AXIOM_PRESET_${preset_lower}_${_short})
            target_compile_definitions(${target} PRIVATE
                ${_full}=${AXIOM_PRESET_${preset_lower}_${_short}}
            )
        endif()
    endforeach()
endfunction()

# ============================================================================
# axiom_get_preset_values(preset OUT_VAR)
#
# Get all preset values as a list of KEY=VALUE pairs.
# Useful for generating test expected values.
# ============================================================================
function(axiom_get_preset_values preset out_var)
    string(TOLOWER "${preset}" preset_lower)

    if(preset_lower STREQUAL "custom")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    if(NOT DEFINED AXIOM_PRESET_${preset_lower}_PRESET)
        message(FATAL_ERROR "Unknown preset: '${preset}'. Valid: custom, ${AXIOM_PRESETS}")
    endif()

    set(_result)
    set(_macros
        PRESET PROFILE RING_BUFFER_SIZE MAX_PAYLOAD_LEN MODULE_MAX
        LOCATION_MODE TIME_SYNC_ENABLED SHORT_CS BACKEND_DEGRADATION
        CAPSULE_ENABLED CAPSULE_PRE_EVENTS CAPSULE_POST_EVENTS CAPSULE_RING_SIZE
        RING_BUFFER_POLICY ENCODE_OVERFLOW_DETECTION SELFTEST DEBUG
        BACKEND_FAIL_THRESHOLD BACKEND_RECOVERY_US
        CAPSULE_MAX_SNAPSHOT_LEN CAPSULE_FLASH_SIZE
    )

    foreach(macro ${_macros})
        if(DEFINED AXIOM_PRESET_${preset_lower}_${macro})
            list(APPEND _result "${macro}=${AXIOM_PRESET_${preset_lower}_${macro}}")
        endif()
    endforeach()

    set(${out_var} "${_result}" PARENT_SCOPE)
endfunction()

# ============================================================================
# axiom_print_preset(preset)
#
# Print all values for a preset (for debugging).
# ============================================================================
function(axiom_print_preset preset)
    string(TOLOWER "${preset}" preset_lower)
    message(STATUS "=== Preset: ${preset} ===")

    set(_macros
        PRESET PROFILE RING_BUFFER_SIZE MAX_PAYLOAD_LEN MODULE_MAX
        LOCATION_MODE TIME_SYNC_ENABLED SHORT_CS BACKEND_DEGRADATION
        CAPSULE_ENABLED CAPSULE_PRE_EVENTS CAPSULE_POST_EVENTS CAPSULE_RING_SIZE
        RING_BUFFER_POLICY ENCODE_OVERFLOW_DETECTION SELFTEST DEBUG
        BACKEND_FAIL_THRESHOLD BACKEND_RECOVERY_US
        CAPSULE_MAX_SNAPSHOT_LEN CAPSULE_FLASH_SIZE
    )

    foreach(macro ${_macros})
        if(DEFINED AXIOM_PRESET_${preset_lower}_${macro})
            message(STATUS "  ${macro}: ${AXIOM_PRESET_${preset_lower}_${macro}}")
        endif()
    endforeach()
endfunction()
