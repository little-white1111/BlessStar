cmake_minimum_required(VERSION 3.20)
project(bs_adapter_schema_watcher VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_adapter_schema_watcher STATIC
    src/schema_watcher.c
)

target_include_directories(bs_adapter_schema_watcher PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bs_adapter_schema_watcher PUBLIC
    bs_kernel_schema_loader
    bs_kernel_common
)

if(MSVC)
    target_compile_definitions(bs_adapter_schema_watcher PRIVATE
        _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_adapter_schema_watcher PRIVATE oldnames.lib)
endif()
