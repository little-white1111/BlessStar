cmake_minimum_required(VERSION 3.20)
project(bs_kernel_schema_loader VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_kernel_schema_loader STATIC
    src/schema_loader.c
    src/schema_yaml_parser.c
)

target_include_directories(bs_kernel_schema_loader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bs_kernel_schema_loader PUBLIC
    bs_kernel_schema
    bs_kernel_common
)

if(MSVC)
    target_compile_definitions(bs_kernel_schema_loader PRIVATE
        _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_schema_loader PRIVATE oldnames.lib)
endif()

# ── SchemaLoader bundled test ────────────────────────────────────
add_executable(bs_test_schema_loader_bundled
    test/test_schema_loader_bundled.c
)
target_link_libraries(bs_test_schema_loader_bundled PRIVATE
    bs_kernel_schema_loader
    bs_kernel_schema
    bs_kernel_common
)
target_include_directories(bs_test_schema_loader_bundled PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
add_test(NAME bs_test_schema_loader_bundled COMMAND bs_test_schema_loader_bundled)
set_tests_properties(bs_test_schema_loader_bundled PROPERTIES
    LABELS "unit;schema_loader;bundled"
    TIMEOUT 30
)
