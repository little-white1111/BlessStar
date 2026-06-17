cmake_minimum_required(VERSION 3.20)
project(bs_kernel_ui_map VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_kernel_ui_map STATIC
    src/ui_render_desc.c
    src/schema_to_uidl.c
    src/uidl_validator.c
)

target_include_directories(bs_kernel_ui_map PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compile C sources as C++ for MSVC CRT compatibility
set_source_files_properties(
    src/ui_render_desc.c src/schema_to_uidl.c src/uidl_validator.c
    PROPERTIES LANGUAGE CXX
)

target_link_libraries(bs_kernel_ui_map PUBLIC bs_kernel_schema)

if(MSVC)
    target_compile_definitions(bs_kernel_ui_map PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_ui_map PRIVATE oldnames.lib)
endif()
