cmake_minimum_required(VERSION 3.20)
project(bs_kernel_io VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_kernel_io STATIC
    src/io_facade.cpp
    src/io_status_table.c
)

target_include_directories(bs_kernel_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bs_kernel_io PUBLIC bs_kernel_common bs_kernel_registry)

# Compile C sources as CXX for MSVC CRT
set_source_files_properties(
    src/io_status_table.c
    PROPERTIES LANGUAGE CXX
)

if(MSVC)
    target_compile_definitions(bs_kernel_io PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_io PRIVATE oldnames.lib)
endif()
