cmake_minimum_required(VERSION 3.20)
project(bs_kernel_ir VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_kernel_ir STATIC
    src/ir.cpp
    src/ir_gate.cpp
    src/ir_plugin.cpp
    src/metadata.c
    src/requirements.cpp
    src/resolver.cpp
)

target_include_directories(bs_kernel_ir PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bs_kernel_ir PUBLIC bs_kernel_common)

# Compile C sources as CXX for MSVC CRT
set_source_files_properties(
    src/metadata.c
    PROPERTIES LANGUAGE CXX
)

if(MSVC)
    target_compile_definitions(bs_kernel_ir PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_ir PRIVATE oldnames.lib)
endif()
