cmake_minimum_required(VERSION 3.20)
project(bs_kernel_runtime VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_kernel_runtime STATIC
    src/config.c
    src/context.c
    src/kernel.c
    src/kernel_pool.c
)

target_include_directories(bs_kernel_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bs_kernel_runtime PUBLIC bs_kernel_common bs_kernel_pipeline bs_kernel_report)

# Compile C sources as CXX for MSVC CRT
set_source_files_properties(
    src/config.c src/context.c src/kernel.c src/kernel_pool.c
    PROPERTIES LANGUAGE CXX
)

# Enable BS_TESTING for testing helper functions (e.g. bs_kernel_pool_testing_*)
target_compile_definitions(bs_kernel_runtime PRIVATE BS_TESTING=1)

if(MSVC)
    target_compile_definitions(bs_kernel_runtime PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_runtime PRIVATE oldnames.lib)
endif()
