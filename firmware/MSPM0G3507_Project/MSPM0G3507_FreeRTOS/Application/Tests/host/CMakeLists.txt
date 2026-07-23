cmake_minimum_required(VERSION 3.16)
project(mspm0_algorithm_regression C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(APP_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
set(ALGORITHM_DIR "${APP_DIR}/Algorithm")
set(FILTER_DIR "${APP_DIR}/Filter")

add_executable(algorithm_regression
    test_algorithms.c
    ${ALGORITHM_DIR}/app_pid.c
    ${ALGORITHM_DIR}/app_planner.c
    ${ALGORITHM_DIR}/app_position_control.c
    ${ALGORITHM_DIR}/app_model_id.c
    ${FILTER_DIR}/filter_core.c
    ${FILTER_DIR}/filter_config.c
    ${FILTER_DIR}/filter_complementary.c
    ${FILTER_DIR}/filter_lpf.c
    ${FILTER_DIR}/filter_eskf.c
    ${FILTER_DIR}/filter_lkf.c
    ${FILTER_DIR}/filter_mahony.c
    ${FILTER_DIR}/filter_madgwick.c
    ${FILTER_DIR}/filter_kf.c
)

target_include_directories(algorithm_regression PRIVATE
    support
    ${ALGORITHM_DIR}
    ${FILTER_DIR}
)

if(MSVC)
    target_compile_options(algorithm_regression PRIVATE /W4)
else()
    target_compile_options(algorithm_regression PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        -Werror=implicit-function-declaration -Werror=return-type)
    if(NOT WIN32)
        target_link_libraries(algorithm_regression PRIVATE m)
    endif()
endif()

enable_testing()
add_test(NAME algorithm_regression COMMAND algorithm_regression)
