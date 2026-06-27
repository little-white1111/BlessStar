cmake_minimum_required(VERSION 3.20)
project(bs_kernel_common VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Core common library ──────────────────────────────────────────────
add_library(bs_kernel_common STATIC
    src/MemoryPool.cpp
    src/Metrics.cpp
    src/Plugin.cpp
    src/bs_log.c
    src/bs_reentrancy.c
    src/bs_status.c
    src/bs_status_format.c
    src/bs_trace_span.c
    src/bs_wait_trace.c
    src/metrics_export.cpp
)

target_include_directories(bs_kernel_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── Format helper (separate target for test dependency granularity) ──
add_library(bs_kernel_common_format STATIC
    src/bs_safe_format.c
)
target_include_directories(bs_kernel_common_format PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bs_kernel_common PUBLIC bs_kernel_common_format)

# ── Test support utilities ───────────────────────────────────────────
add_library(bs_kernel_test_support STATIC
    test_support/src/bs_test_log_bus.c
)
target_include_directories(bs_kernel_test_support PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/test_support/include
)

# Compile C sources as CXX for MSVC CRT compatibility
set_source_files_properties(
    src/bs_log.c src/bs_reentrancy.c src/bs_status.c
    src/bs_status_format.c src/bs_trace_span.c src/bs_wait_trace.c
    PROPERTIES LANGUAGE CXX
)
set_source_files_properties(
    src/bs_safe_format.c
    PROPERTIES LANGUAGE CXX
)
set_source_files_properties(
    test_support/src/bs_test_log_bus.c
    PROPERTIES LANGUAGE CXX
)

if(MSVC)
    target_compile_definitions(bs_kernel_common PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_common PRIVATE oldnames.lib)
    target_compile_definitions(bs_kernel_common_format PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_common_format PRIVATE oldnames.lib)
    target_compile_definitions(bs_kernel_test_support PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_test_support PRIVATE oldnames.lib)
endif()
