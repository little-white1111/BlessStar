cmake_minimum_required(VERSION 3.20)

project(blessstar_pure_kernel_skeleton LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BLESSSTAR_ENABLE_PURITY_CHECK "Run purity gate during build" ON)
option(
  BLESSSTAR_TREAT_WARNINGS_AS_ERRORS
  "Treat compiler warnings as errors (AGENTS quality gate 4 in CI)"
  OFF
)
option(
  BLESSSTAR_SANITIZER_CI
  "CI sanitizer build: memory log bus only (no spdlog / LSan false positives)"
  OFF
)
if(BLESSSTAR_TREAT_WARNINGS_AS_ERRORS)
  if(MSVC)
    # /WX: warnings as errors. /wd4996: POSIX CRT names (strdup) and "unsafe" CRT (strncpy) — keep portable C without rewriting for MSVC-only APIs.
    add_compile_options(/W3 /WX /wd4996)
  else()
    add_compile_options(-Wall -Wextra -Werror)
    # Match MSVC /wd4996: POSIX strdup/strncpy on Linux (ubuntu CI Release -Werror).
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      add_compile_options(-Wno-deprecated-declarations)
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      add_compile_options(
        -Wno-stringop-truncation
        -Wno-stringop-overflow
        -Wno-array-bounds
        -Wno-maybe-uninitialized
      )
    endif()
  endif()
endif()

# -------------------------
# Kernel: common (error/metrics/memory/plugin)
# -------------------------
add_library(bs_kernel_common
  kernel/common/src/Metrics.cpp
  kernel/common/src/MemoryPool.cpp
  kernel/common/src/Plugin.cpp
  kernel/common/src/bs_status.c
  kernel/common/src/bs_log.c
  kernel/common/src/bs_reentrancy.c
  kernel/common/src/bs_safe_format.c
)
target_include_directories(bs_kernel_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/common/include
)
if(UNIX AND NOT APPLE)
  target_link_libraries(bs_kernel_common PUBLIC ${CMAKE_DL_LIBS})
endif()

add_library(bs_kernel_test_support
  kernel/common/test_support/src/bs_test_log_bus.c
)
target_include_directories(bs_kernel_test_support
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/common/test_support/include
)
target_link_libraries(bs_kernel_test_support
  PUBLIC
    bs_kernel_common
)

# -------------------------
# Kernel: ir (unique input)
# -------------------------
add_library(bs_kernel_ir
  kernel/ir/src/ir.cpp
  kernel/ir/src/metadata.c
  kernel/ir/src/requirements.cpp
  kernel/ir/src/resolver.cpp
  kernel/ir/src/ir_gate.cpp
  kernel/ir/src/ir_plugin.cpp
)
target_include_directories(bs_kernel_ir
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/ir/include
)

# ------------------------------
# Kernel: pipeline (single line)
# ------------------------------
add_library(bs_kernel_pipeline
  kernel/pipeline/src/pipeline.cpp
  kernel/pipeline/src/stage.c
)
target_include_directories(bs_kernel_pipeline
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/pipeline/include
)
target_link_libraries(bs_kernel_pipeline
  PUBLIC
    bs_kernel_ir
    bs_kernel_report_core
)

# ------------------------------
# Kernel: runtime (Kernel + Context + Config)
# ------------------------------
add_library(bs_kernel_runtime
  kernel/runtime/src/kernel.c
  kernel/runtime/src/context.c
  kernel/runtime/src/config.c
)
target_include_directories(bs_kernel_runtime
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/runtime/include
)
target_link_libraries(bs_kernel_runtime
  PUBLIC
    bs_kernel_pipeline
    bs_kernel_ir
    bs_kernel_report_core
    bs_kernel_common
)

# ------------------------------
# Kernel: state (config lifecycle)
# ------------------------------
add_library(bs_kernel_state
  kernel/state/src/ConfigManager.cpp
  kernel/state/src/machine/StateMachine.cpp
  kernel/state/src/bus/StateBus.cpp
  kernel/state/src/bus/ShardedStateBus.cpp
  kernel/state/src/event/ConfigEvent.cpp
  kernel/state/src/event/EventBus.cpp
  kernel/state/src/event/WatchManager.cpp
  kernel/state/src/temp/TemporaryState.cpp
)
target_include_directories(bs_kernel_state
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/state/include
)
target_link_libraries(bs_kernel_state
  PUBLIC
    bs_kernel_common
)

# ------------------------------
# Kernel: registry (PathRegistry + Hub + Facade)
# ------------------------------
add_library(bs_kernel_registry
  kernel/registry/src/path_normalize.cpp
  kernel/registry/src/path_registry.cpp
  kernel/registry/src/registry_hub.cpp
  kernel/registry/src/registry_facade.cpp
  kernel/registry/src/registry_status_table.c
)
target_include_directories(bs_kernel_registry
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/registry/include
)
target_link_libraries(bs_kernel_registry
  PUBLIC
    bs_kernel_common
)

add_library(bs_kernel_common_format
  kernel/common/src/bs_status_format.c
)
target_include_directories(bs_kernel_common_format
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/common/include
)
target_link_libraries(bs_kernel_common_format
  PUBLIC
    bs_kernel_registry
    bs_kernel_common
)

# -----------------------------------
# Kernel: report (IMPL-08-08 · core vs full; after common_format)
# -----------------------------------
add_library(bs_kernel_report_core
  kernel/report/src/report.cpp
  kernel/report/src/result_status_map.c
)
target_include_directories(bs_kernel_report_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/report/include
)
target_link_libraries(bs_kernel_report_core
  PUBLIC
    bs_kernel_common
    bs_kernel_common_format
)

add_library(bs_kernel_report INTERFACE)
target_link_libraries(bs_kernel_report
  INTERFACE
    bs_kernel_report_core
    bs_kernel_pipeline
)

# ------------------------------
# Kernel: io (IoFacade + Provider ABI)
# ------------------------------
add_library(bs_kernel_io
  kernel/io/src/io_facade.cpp
  kernel/io/src/io_status_table.c
)
target_include_directories(bs_kernel_io
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/io/include
)
target_link_libraries(bs_kernel_io
  PUBLIC
    bs_kernel_registry
    bs_kernel_common
)

# ------------------------------
# Adapter: requirement filter (tests + production)
# ------------------------------
add_library(bs_adapter_requirement
  adapter/src/requirement_filter.cpp
  adapter/src/plugin_ir_requirements.cpp
)
target_include_directories(bs_adapter_requirement
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
)
target_link_libraries(bs_adapter_requirement
  PUBLIC
    bs_kernel_ir
)

# ------------------------------
# Adapter: BlessStar Config JSON v1 parser (M2 main chain)
# ------------------------------
add_library(bs_adapter_parser
  adapter/parser/src/json/json_lexer.c
  adapter/parser/src/json/json_utf8.c
  adapter/parser/src/json/json_parser.c
  adapter/parser/src/config/config_v1_ast.c
  adapter/parser/src/config/config_v1_ir.c
  adapter/parser/src/config/config_parse.c
  adapter/parser/src/config/config_parse_status.c
)
target_include_directories(bs_adapter_parser
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/parser/include
)
target_link_libraries(bs_adapter_parser
  PUBLIC
    bs_kernel_ir
    bs_kernel_common
    bs_adapter_requirement
)

# ------------------------------
# Adapter: attach runtime + spdlog log bus
# ------------------------------
add_library(bs_adapter_attach
  adapter/src/attach_runtime.cpp
  adapter/src/attach_context.cpp
  adapter/src/attach_config.cpp
  adapter/src/attach_execute.cpp
)
target_include_directories(bs_adapter_attach
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/src
)
target_link_libraries(bs_adapter_attach
  PUBLIC
    bs_kernel_registry
    bs_kernel_io
    bs_kernel_common
    bs_kernel_state
    bs_kernel_runtime
    bs_kernel_pipeline
    bs_kernel_ir
    bs_adapter_parser
)
# Test hooks in attach_config.cpp (sync_fail_path, etc.) require BS_TESTING in this TU.
target_compile_definitions(bs_adapter_attach PRIVATE BS_TESTING)

if(NOT BLESSSTAR_SANITIZER_CI)
  include(FetchContent)
  FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
  )
  set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(spdlog)
  # Linux sanitizer CI passes -fsanitize via CMAKE_CXX_FLAGS; apply to FetchContent too.
  if(CMAKE_CXX_FLAGS MATCHES "-fsanitize=" AND TARGET spdlog)
    target_compile_options(spdlog PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(spdlog PRIVATE -fsanitize=address,undefined)
  endif()
endif()

add_library(bs_adapter_log adapter/log/memory_log_bus.cpp)
if(NOT BLESSSTAR_SANITIZER_CI)
  target_sources(bs_adapter_log PRIVATE adapter/log/spdlog_log_bus.cpp)
endif()
target_include_directories(bs_adapter_log
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
)
target_link_libraries(bs_adapter_log
  PUBLIC
    bs_kernel_common
    bs_adapter_attach
)
if(BLESSSTAR_SANITIZER_CI)
  target_compile_definitions(bs_adapter_log PRIVATE BLESSSTAR_SANITIZER_CI=1)
else()
  target_link_libraries(bs_adapter_log PUBLIC spdlog::spdlog)
endif()

# ------------------------------
# Adapter: io providers
# ------------------------------
add_library(bs_adapter_io
  adapter/io/local_file_provider.cpp
  adapter/io/db_provider_stub.cpp
  adapter/io/remote_provider_stub.cpp
  adapter/io/io_providers.cpp
)
target_include_directories(bs_adapter_io
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
)
target_link_libraries(bs_adapter_io
  PUBLIC
    bs_kernel_io
    bs_kernel_registry
)

# ------------------------------
# Adapter: attach persistence (manifest / CAS / commit)
# ------------------------------
add_library(bs_adapter_persistence
  adapter/persistence/attach_store.cpp
  adapter/persistence/attach_uri_path.c
  adapter/persistence/attach_audit.c
  adapter/persistence/attach_wal.c
  adapter/persistence/attach_watch.c
  adapter/persistence/attach_crc32.c
  adapter/persistence/attach_fsync.c
)
target_include_directories(bs_adapter_persistence
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/persistence
)
target_link_libraries(bs_adapter_persistence
  PUBLIC
    bs_kernel_common
    bs_kernel_report_core
)

# ------------------------------
# Adapter: reload orchestration (batch / gate / report)
# ------------------------------
add_library(bs_adapter_orchestration
  adapter/orchestration/reload_batch_controller.cpp
  adapter/orchestration/reload_batch_factory.cpp
  adapter/orchestration/reload_gate_default.cpp
  adapter/orchestration/reload_with_report.cpp
)
target_include_directories(bs_adapter_orchestration
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
)
target_link_libraries(bs_adapter_orchestration
  PUBLIC
    bs_kernel_io
    bs_kernel_report_core
    bs_adapter_requirement
    bs_adapter_attach
    bs_adapter_parser
    bs_adapter_persistence
)

# ------------------------------
# Adapter: P2 plugins (IMPL-08-17 · static link)
# ------------------------------
add_library(bs_adapter_plugin_log_domains
  adapter/plugins/log_domains/plugin_log_domains.cpp
)
target_include_directories(bs_adapter_plugin_log_domains
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
)
target_link_libraries(bs_adapter_plugin_log_domains
  PUBLIC
    bs_adapter_plugin_support
    bs_kernel_registry
    bs_kernel_io
    bs_kernel_common
)

add_library(bs_adapter_plugin_io
  adapter/plugins/io/plugin_io.cpp
)
target_include_directories(bs_adapter_plugin_io
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
)
target_link_libraries(bs_adapter_plugin_io
  PUBLIC
    bs_adapter_io
    bs_adapter_plugin_support
    bs_kernel_registry
)

add_library(bs_adapter_plugin_orch
  adapter/plugins/orch/plugin_orch.cpp
)
target_include_directories(bs_adapter_plugin_orch
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
)
target_link_libraries(bs_adapter_plugin_orch
  PUBLIC
    bs_adapter_orchestration
    bs_adapter_plugin_support
    bs_kernel_registry
)

add_library(bs_adapter_plugin_support
  adapter/src/plugin_manifest_paths.cpp
)
target_include_directories(bs_adapter_plugin_support
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(bs_adapter_plugin_support
  PUBLIC
    bs_adapter_requirement
)

add_library(bs_adapter_plugin_loader
  adapter/src/plugin_loader.cpp
  adapter/src/attach_manifest_yaml.cpp
)
target_include_directories(bs_adapter_plugin_loader
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
)
target_link_libraries(bs_adapter_plugin_loader
  PUBLIC
    bs_adapter_plugin_io
    bs_adapter_plugin_log_domains
    bs_adapter_plugin_orch
    bs_adapter_plugin_support
    bs_adapter_attach
    bs_kernel_registry
)

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/adapter/manifest" BS_ADAPTER_MANIFEST_DIR)
string(REPLACE "\\" "/" BS_ADAPTER_MANIFEST_DIR "${BS_ADAPTER_MANIFEST_DIR}")
configure_file(
  ${CMAKE_SOURCE_DIR}/adapter/manifest/manifest_config.h.in
  ${CMAKE_BINARY_DIR}/generated/bs/adapter/manifest/manifest_config.h
  @ONLY
)

# ------------------------------
# Adapter: registry bootstrap (attach pipeline · core + loader)
# ------------------------------
add_library(bs_adapter_registry
  adapter/src/registry_bootstrap.cpp
)
target_include_directories(bs_adapter_registry
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/include
)
target_link_libraries(bs_adapter_registry
  PUBLIC
    bs_kernel_registry
    bs_kernel_ir
    bs_adapter_plugin_loader
    bs_adapter_requirement
    bs_adapter_log
    bs_adapter_attach
)

# ------------------------------
# App: SDK (day16 · App -> Adapter boundary)
# ------------------------------
add_library(bs_app_sdk
  app/sdk/src/app_contract.cpp
  app/sdk/src/app_ir_mapper.cpp
  app/sdk/src/app_scenario_policy.cpp
  app/sdk/src/vendor_config_normalizer.cpp
  app/sdk/src/vendor_reload_facade.cpp
  app/sdk/src/app_vendor_precheck.cpp
)
target_include_directories(bs_app_sdk
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/app/sdk/include
)
target_link_libraries(bs_app_sdk
  PUBLIC
    bs_kernel_common
)

# ------------------------------
# Unit tests (sources under modules; registration here + CTest)
# ------------------------------
enable_testing()
include(${CMAKE_SOURCE_DIR}/cmake/Tests.cmake)

add_custom_target(run_all_tests
  COMMAND ${CMAKE_CTEST_COMMAND}
          --test-dir "${CMAKE_BINARY_DIR}"
          --output-on-failure
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Run all CTest-registered unit tests (multi-config: also pass -C <Config> to ctest if needed)"
  USES_TERMINAL
)
get_property(_blessstar_unit_tests GLOBAL PROPERTY BLESSSTAR_UNIT_TEST_TARGETS)
if(_blessstar_unit_tests)
  add_dependencies(run_all_tests ${_blessstar_unit_tests})
endif()

add_custom_target(run_regression_tests
  COMMAND ${CMAKE_CTEST_COMMAND}
          --test-dir "${CMAKE_BINARY_DIR}"
          --output-on-failure
          -L regression
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Run CTest targets labeled regression (multi-config: add -C Debug)"
  USES_TERMINAL
)
if(_blessstar_unit_tests)
  add_dependencies(run_regression_tests ${_blessstar_unit_tests})
endif()

add_custom_target(run_integration_tests
  COMMAND ${CMAKE_CTEST_COMMAND}
          --test-dir "${CMAKE_BINARY_DIR}"
          --output-on-failure
          -L integration
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Run CTest targets labeled integration (multi-config: add -C Debug)"
  USES_TERMINAL
)
if(_blessstar_unit_tests)
  add_dependencies(run_integration_tests ${_blessstar_unit_tests})
endif()

# ------------------------------
# Benchmarks: day1, day2, day3 (build targets only; not CTest)
# ------------------------------
add_executable(bs_benchmark_ir
  kernel/ir/test/IRBenchmark.cpp
)
target_link_libraries(bs_benchmark_ir
  PRIVATE
    bs_kernel_ir
)

add_executable(bs_benchmark_pipeline
  kernel/pipeline/test/PipelineBenchmark.cpp
)
target_link_libraries(bs_benchmark_pipeline
  PRIVATE
    bs_kernel_pipeline
)

add_executable(bs_benchmark_report
  kernel/report/test/ReportBenchmark.cpp
)
target_link_libraries(bs_benchmark_report
  PRIVATE
    bs_kernel_report
)

add_executable(bs_benchmark_state
  kernel/state/test/StateBenchmark.cpp
)
target_link_libraries(bs_benchmark_state
  PRIVATE
    bs_kernel_state
)

# ------------------------------
# Adapter: CLI (pure I/O entry)
# ------------------------------
add_executable(bs_adapter_cli
  adapter/cli/src/main.cpp
)
target_include_directories(bs_adapter_cli
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter/cli/include
)
target_link_libraries(bs_adapter_cli
  PRIVATE
    bs_kernel_ir
    bs_kernel_report
)

# --------------------
# Purity gate (minimal)
# --------------------
if(BLESSSTAR_ENABLE_PURITY_CHECK)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_custom_target(purity_check
      COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/purity/verify_manifest.py
        verify
        --kernel ${CMAKE_CURRENT_SOURCE_DIR}/factory
        --manifest ${CMAKE_CURRENT_SOURCE_DIR}/factory/manifest.sha256
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Verifying factory manifest purity gate"
    )

    add_dependencies(bs_adapter_cli purity_check)
  else()
    message(
      STATUS
      "Purity gate skipped: Python3 interpreter not found (install Python or pass -DBLESSSTAR_ENABLE_PURITY_CHECK=OFF)"
    )
  endif()
endif()
