cmake_minimum_required(VERSION 3.20)
project(bs_adapter VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Common include paths ──────────────────────────────────────────────
set(ADAPTER_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/parser/include
)

# ── Generate manifest_config.h from template ─────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest/manifest_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/gen/bs/adapter/manifest/manifest_config.h"
    @ONLY
)
set(ADAPTER_GEN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")

# ── Single adapter library ─────────────────────────────────────────
# All adapter sources in one lib to avoid cross-module circular deps.
add_library(bs_adapter_all STATIC
    # parser
    parser/src/config/config_parse.c
    parser/src/config/config_parse_status.c
    parser/src/config/config_v1_ast.c
    parser/src/config/config_v1_ir.c
    parser/src/json/json_lexer.c
    parser/src/json/json_parser.c
    parser/src/json/json_utf8.c
    parser/src/meta_rule.cpp
    # core src
    src/attach_config.cpp
    src/attach_context.cpp
    src/attach_execute.cpp
    src/attach_ir_snapshot.cpp
    src/attach_manifest_yaml.cpp
    src/attach_notify_queue.cpp
    src/attach_recover.cpp
    src/attach_recover_sidecar.cpp
    src/attach_runtime.cpp
    src/attach_session.cpp
    src/registry_bootstrap.cpp
    src/plugin_ir_requirements.cpp
    src/plugin_manifest_paths.cpp
    src/plugin_loader.cpp
    src/requirement_filter.cpp
    # io providers
    io/io_providers.cpp
    io/local_file_provider.cpp
    io/db_provider_stub.cpp
    io/remote_provider_stub.cpp
    # log
    log/memory_log_bus.cpp
    # orchestration
    orchestration/reload_batch_controller.cpp
    orchestration/reload_batch_factory.cpp
    orchestration/reload_gate_default.cpp
    orchestration/reload_with_report.cpp
    # persistence
    persistence/attach_audit.c
    persistence/attach_crc32.c
    persistence/attach_fsync.c
    persistence/attach_runtime_sidecar.c
    persistence/attach_store.cpp
    persistence/attach_uri_path.c
    persistence/attach_wal.c
    persistence/attach_watch.c
    # plugins
    plugins/io/plugin_io.cpp
    plugins/log_domains/plugin_log_domains.cpp
    plugins/orch/plugin_orch.cpp
)

target_include_directories(bs_adapter_all PUBLIC ${ADAPTER_INCLUDE_DIRS})
target_include_directories(bs_adapter_all PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ADAPTER_GEN_INCLUDE_DIR}
)

# Link against kernel libraries (adapter uses types from all kernel modules)
target_link_libraries(bs_adapter_all PUBLIC
    bs_kernel_common
    bs_kernel_common_format
    bs_kernel_io
    bs_kernel_ir
    bs_kernel_runtime
    bs_kernel_registry
    bs_kernel_state
)

# Compile C files as CXX for MSVC CRT
set_source_files_properties(
    parser/src/config/config_parse.c parser/src/config/config_parse_status.c
    parser/src/config/config_v1_ast.c parser/src/config/config_v1_ir.c
    parser/src/json/json_utf8.c
    persistence/attach_audit.c persistence/attach_crc32.c
    persistence/attach_fsync.c persistence/attach_runtime_sidecar.c
    persistence/attach_uri_path.c persistence/attach_wal.c
    persistence/attach_watch.c
    PROPERTIES LANGUAGE CXX
)
# json_lexer.c / json_parser.c use C99 compound literals — compile as C
set_source_files_properties(
    parser/src/json/json_lexer.c parser/src/json/json_parser.c
    PROPERTIES LANGUAGE C
)

# spdlog-based implementation requires spdlog headers
find_package(spdlog QUIET)
if(spdlog_FOUND)
    target_sources(bs_adapter_all PRIVATE log/spdlog_log_bus.cpp)
    target_link_libraries(bs_adapter_all PRIVATE spdlog::spdlog)
    message(STATUS "bs_adapter_all: spdlog found")
else()
    target_sources(bs_adapter_all PRIVATE log/spdlog_stub.c)
    set_source_files_properties(log/spdlog_stub.c PROPERTIES LANGUAGE C)
    message(STATUS "bs_adapter_all: spdlog NOT found — using stub")
endif()

# Enable testing helper functions (e.g. bs_adapter_attach_config_testing_*)
target_compile_definitions(bs_adapter_all PRIVATE BS_TESTING)

# Backward-compat aliases for tests that reference individual libraries
foreach(_sub
    parser requirement persistence orchestration registry io
    attach plugin_support plugin_loader log
)
    add_library(bs_adapter_${_sub} ALIAS bs_adapter_all)
endforeach()

if(MSVC)
    target_compile_definitions(bs_adapter_all PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_adapter_all PRIVATE oldnames.lib)
    # MSVC linker skips orphan .obj in static lib; force whole archive
    target_link_options(bs_adapter_all INTERFACE /WHOLEARCHIVE:bs_adapter_all)
endif()
