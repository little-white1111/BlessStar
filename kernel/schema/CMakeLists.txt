cmake_minimum_required(VERSION 3.20)
project(bs_kernel_schema VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_kernel_schema STATIC
    src/schema_types.c
    src/schema_registry.c
    src/schema_validator.c
    src/custom_validator.c
    src/schema_json_converter.c
    src/schema_validator_plugin.c
)

target_include_directories(bs_kernel_schema PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compile C sources as C++ for MSVC CRT compatibility
set_source_files_properties(
    src/schema_types.c src/schema_registry.c src/schema_validator.c
    src/custom_validator.c src/schema_json_converter.c
    PROPERTIES LANGUAGE CXX
)

if(MSVC)
    target_compile_definitions(bs_kernel_schema PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_schema PRIVATE oldnames.lib)
endif()
