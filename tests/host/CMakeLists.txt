cmake_minimum_required(VERSION 3.16)
project(axiom_tests LANGUAGES C)
set(CMAKE_C_STANDARD 11)

# Simple host tests without Unity for now
add_executable(test_ring_buf test_ring.c)
target_link_libraries(test_ring_buf PRIVATE axiomtrace)
add_test(NAME test_ring_buf COMMAND test_ring_buf)

add_executable(test_crc test_crc.c)
target_link_libraries(test_crc PRIVATE axiomtrace)
add_test(NAME test_crc COMMAND test_crc)

add_executable(test_event test_event.c)
target_link_libraries(test_event PRIVATE axiomtrace)
add_test(NAME test_event COMMAND test_event)

add_executable(test_filter test_filter.c)
target_link_libraries(test_filter PRIVATE axiomtrace)
add_test(NAME test_filter COMMAND test_filter)

# test_selftest validates axiom_selftest() passes.
# NOTE: AXIOM_SELFTEST must be set at the library level (CMakeLists.txt root).
# Build with: cmake -B build_selftest -DAXIOM_SELFTEST=1 && cmake --build build_selftest
# The test is always registered; when AXIOM_SELFTEST=0 the stub always passes.
add_executable(test_selftest test_selftest.c)
target_link_libraries(test_selftest PRIVATE axiomtrace)
add_test(NAME test_selftest COMMAND test_selftest)

# ---- New extended tests ----

add_executable(test_encoder test_encoder.c)
target_link_libraries(test_encoder PRIVATE axiomtrace)
add_test(NAME test_encoder COMMAND test_encoder)

add_executable(test_ring_ext test_ring_ext.c)
target_link_libraries(test_ring_ext PRIVATE axiomtrace)
add_test(NAME test_ring_ext COMMAND test_ring_ext)

add_executable(test_event_ext test_event_ext.c)
target_link_libraries(test_event_ext PRIVATE axiomtrace)
add_test(NAME test_event_ext COMMAND test_event_ext)

add_executable(test_capsule test_capsule.c)
target_link_libraries(test_capsule PRIVATE axiomtrace)
add_test(NAME test_capsule COMMAND test_capsule)

add_executable(test_dynamic_call_chain test_dynamic_call_chain.c)
target_link_libraries(test_dynamic_call_chain PRIVATE axiomtrace)
add_test(NAME test_dynamic_call_chain COMMAND test_dynamic_call_chain)

add_executable(test_profile_dev test_profiles.c)
target_compile_definitions(test_profile_dev PRIVATE AXIOM_PROFILE=0)
target_link_libraries(test_profile_dev PRIVATE axiomtrace)
add_test(NAME test_profile_dev COMMAND test_profile_dev)

add_executable(test_profile_field test_profiles.c)
target_compile_definitions(test_profile_field PRIVATE AXIOM_PROFILE=1)
target_link_libraries(test_profile_field PRIVATE axiomtrace)
add_test(NAME test_profile_field COMMAND test_profile_field)

add_executable(test_profile_prod test_profiles.c)
target_compile_definitions(test_profile_prod PRIVATE AXIOM_PROFILE=2)
target_link_libraries(test_profile_prod PRIVATE axiomtrace)
add_test(NAME test_profile_prod COMMAND test_profile_prod)

function(add_config_preset_test name preset profile ring payload modules location time_sync short_cs degradation capsule pre post cap_ring ring_policy overflow_detection selftest debug fail_threshold recovery_us snapshot_len flash_size)
    add_executable(test_config_preset_${name} test_config_presets.c)
    target_include_directories(test_config_preset_${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/baremetal
        ${CMAKE_SOURCE_DIR}/baremetal/include
        ${CMAKE_SOURCE_DIR}/baremetal/port
        ${CMAKE_SOURCE_DIR}/baremetal/core
        ${CMAKE_SOURCE_DIR}/baremetal/frontend
        ${CMAKE_SOURCE_DIR}/baremetal/backend
    )
    target_compile_definitions(test_config_preset_${name} PRIVATE
        AXIOM_PRESET=${preset}
        EXPECT_PRESET=${preset}
        EXPECT_PROFILE=${profile}
        EXPECT_RING_BUFFER_SIZE=${ring}
        EXPECT_MAX_PAYLOAD_LEN=${payload}
        EXPECT_MODULE_MAX=${modules}
        EXPECT_LOCATION_MODE=${location}
        EXPECT_TIME_SYNC_ENABLED=${time_sync}
        EXPECT_SHORT_CS=${short_cs}
        EXPECT_BACKEND_DEGRADATION=${degradation}
        EXPECT_CAPSULE_ENABLED=${capsule}
        EXPECT_CAPSULE_PRE_EVENTS=${pre}
        EXPECT_CAPSULE_POST_EVENTS=${post}
        EXPECT_CAPSULE_RING_SIZE=${cap_ring}
        EXPECT_RING_BUFFER_POLICY=${ring_policy}
        EXPECT_ENCODE_OVERFLOW_DETECTION=${overflow_detection}
        EXPECT_SELFTEST=${selftest}
        EXPECT_DEBUG=${debug}
        EXPECT_BACKEND_FAIL_THRESHOLD=${fail_threshold}
        EXPECT_BACKEND_RECOVERY_US=${recovery_us}
        EXPECT_CAPSULE_MAX_SNAPSHOT_LEN=${snapshot_len}
        EXPECT_CAPSULE_FLASH_SIZE=${flash_size}
    )
    add_test(NAME test_config_preset_${name} COMMAND test_config_preset_${name})
endfunction()

add_config_preset_test(tiny   AXIOM_PRESET_TINY   AXIOM_PROFILE_PROD  256u  32u  16u AXIOM_CFG_LOCATION_MODE_NONE 0 0 0 0 32u 16u 4096u 0 1 0 0 5 1000000u 64u 8192u)
add_config_preset_test(prod   AXIOM_PRESET_PROD   AXIOM_PROFILE_PROD  1024u 64u  32u AXIOM_CFG_LOCATION_MODE_NONE 0 1 1 0 32u 16u 4096u 0 1 0 0 5 1000000u 64u 8192u)
add_config_preset_test(field  AXIOM_PRESET_FIELD  AXIOM_PROFILE_FIELD 2048u 96u  32u AXIOM_CFG_LOCATION_MODE_NONE 1 1 1 1 16u 8u  2048u 0 1 0 0 5 1000000u 64u 8192u)
add_config_preset_test(dev    AXIOM_PRESET_DEV    AXIOM_PROFILE_DEV   4096u 128u 32u AXIOM_CFG_LOCATION_MODE_NONE 1 1 1 1 32u 16u 4096u 0 1 0 0 5 1000000u 64u 8192u)
add_config_preset_test(custom AXIOM_PRESET_CUSTOM AXIOM_PROFILE_DEV   4096u 128u 32u AXIOM_CFG_LOCATION_MODE_NONE 1 1 1 1 32u 16u 4096u 0 1 0 0 5 1000000u 64u 8192u)


add_executable(test_location_file_id test_location.c)
target_compile_definitions(test_location_file_id PRIVATE AXIOM_CFG_LOCATION_MODE=2 AXIOM_SOURCE_FILE_ID=0x1234u)
target_link_libraries(test_location_file_id PRIVATE axiomtrace)
add_test(NAME test_location_file_id COMMAND test_location_file_id)

add_executable(test_location_hash test_location.c)
target_compile_definitions(test_location_hash PRIVATE AXIOM_CFG_LOCATION_MODE=1 AXIOM_CFG_LOCATION_FUNCTION=1)
target_link_libraries(test_location_hash PRIVATE axiomtrace)
add_test(NAME test_location_hash COMMAND test_location_hash)

add_executable(test_location_none test_location.c)
target_compile_definitions(test_location_none PRIVATE AXIOM_CFG_LOCATION_MODE=0)
target_link_libraries(test_location_none PRIVATE axiomtrace)
add_test(NAME test_location_none COMMAND test_location_none)

add_executable(test_backend_ext test_backend_ext.c)
target_link_libraries(test_backend_ext PRIVATE axiomtrace)
add_test(NAME test_backend_ext COMMAND test_backend_ext)

# ---- Wave 2: Core module dedicated tests ----

add_executable(test_timestamp test_timestamp.c)
target_link_libraries(test_timestamp PRIVATE axiomtrace)
add_test(NAME test_timestamp COMMAND test_timestamp)

add_executable(test_filter_dedicated test_filter.c)
target_link_libraries(test_filter_dedicated PRIVATE axiomtrace)
add_test(NAME test_filter_dedicated COMMAND test_filter_dedicated)

add_executable(test_backend_deferred test_backend_deferred.c
    ${CMAKE_SOURCE_DIR}/baremetal/backend/axiom_backend_deferred.c)
target_link_libraries(test_backend_deferred PRIVATE axiomtrace)
add_test(NAME test_backend_deferred COMMAND test_backend_deferred)

add_executable(test_backend_memory test_backend_memory.c
    ${CMAKE_SOURCE_DIR}/baremetal/backend/axiom_backend_memory.c)
target_link_libraries(test_backend_memory PRIVATE axiomtrace)
add_test(NAME test_backend_memory COMMAND test_backend_memory)
# Encoder-backed generator for tracked wire-format golden vectors.
add_executable(generate_golden generate_golden.c)
target_link_libraries(generate_golden PRIVATE axiomtrace)

# ---- Performance benchmark ----
add_executable(test_benchmark test_benchmark.c)
target_link_libraries(test_benchmark PRIVATE axiomtrace)
# Host regression benchmark; target-MCU timing still requires platform measurement.
add_test(NAME test_benchmark COMMAND test_benchmark)
