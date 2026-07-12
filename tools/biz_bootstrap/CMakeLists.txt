cmake_minimum_required(VERSION 3.20)
project(biz_bootstrap LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(biz_bootstrap
    main.cpp
    gate_rule_loader.cpp
)

target_include_directories(biz_bootstrap PRIVATE
    ${CMAKE_SOURCE_DIR}/adapter/business/include
    ${CMAKE_SOURCE_DIR}/adapter/include
    ${CMAKE_SOURCE_DIR}/app/sdk/include
    ${CMAKE_SOURCE_DIR}/kernel/gate_chain/include
    ${CMAKE_SOURCE_DIR}/kernel/common/include
    ${CMAKE_SOURCE_DIR}/kernel/ir/include
    ${CMAKE_SOURCE_DIR}/kernel/schema/include
)

target_link_libraries(biz_bootstrap PRIVATE
    bs_adapter_all
    bs_app_sdk
    bs_kernel_gate_chain
    bs_kernel_schema
    bs_kernel_common
    bs_kernel_common_format
    bs_kernel_ir
    bs_kernel_registry
    bs_kernel_runtime
    bs_kernel_state
)
