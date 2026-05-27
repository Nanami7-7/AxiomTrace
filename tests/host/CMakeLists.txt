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

# Encoder-backed generator for tracked wire-format golden vectors.
add_executable(generate_golden generate_golden.c)
target_link_libraries(generate_golden PRIVATE axiomtrace)

# ---- Performance benchmark ----
add_executable(test_benchmark test_benchmark.c)
target_link_libraries(test_benchmark PRIVATE axiomtrace)
# Host regression benchmark; target-MCU timing still requires platform measurement.
add_test(NAME test_benchmark COMMAND test_benchmark)
