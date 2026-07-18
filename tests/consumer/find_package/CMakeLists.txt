cmake_minimum_required(VERSION 3.16)
project(AxiomTraceInstalledConsumer LANGUAGES C)

find_package(AxiomTrace 1.0 CONFIG REQUIRED)
add_executable(consumer ../main.c)
target_link_libraries(consumer PRIVATE AxiomTrace::axiomtrace)
