cmake_minimum_required(VERSION 3.20)
project(bs_kernel_report VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Report core (result types) ──────────────────────────────────────
add_library(bs_kernel_report STATIC
    src/report.cpp
)

target_include_directories(bs_kernel_report PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bs_kernel_report PUBLIC bs_kernel_common bs_kernel_common_format bs_kernel_report_core)

# ── Report core utilities (status-map, result) ──────────────────────
add_library(bs_kernel_report_core STATIC
    src/result.c
    src/result_status_map.c
)

target_include_directories(bs_kernel_report_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bs_kernel_report_core PUBLIC bs_kernel_common)

# Compile C sources as CXX for MSVC CRT
set_source_files_properties(
    src/result.c src/result_status_map.c
    PROPERTIES LANGUAGE CXX
)

if(MSVC)
    target_compile_definitions(bs_kernel_report PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_report PRIVATE oldnames.lib)
    target_compile_definitions(bs_kernel_report_core PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_report_core PRIVATE oldnames.lib)
endif()
