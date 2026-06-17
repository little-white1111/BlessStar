cmake_minimum_required(VERSION 3.20)
project(bs_kernel_gate_chain VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_kernel_gate_chain STATIC
    src/gate_chain_serialize.c
    src/gate_default_factory.c
    src/gate_policy_factory.c
    src/gate_custom_factory.c
    src/gate_matcher.c
    src/gate_evaluator.c
)

target_include_directories(bs_kernel_gate_chain PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_source_files_properties(
    src/gate_chain_serialize.c
    PROPERTIES LANGUAGE CXX
)

if(MSVC)
    target_compile_definitions(bs_kernel_gate_chain PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_gate_chain PRIVATE oldnames.lib)
endif()
