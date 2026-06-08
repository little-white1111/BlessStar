#Centralized unit test targets + CTest registration.
#Test sources remain under each        module(kernel/*/test, adapter/test).

function(blessstar_add_unit_test name)
  set(_opts "")
  set(_one "")
  set(_multi SOURCES LIBS TEST_ARGS)
  cmake_parse_arguments(_arg "${_opts}" "${_one}" "${_multi}" ${ARGN})
  if(NOT _arg_SOURCES)
    message(FATAL_ERROR "blessstar_add_unit_test(${name}): SOURCES required")
  endif()
  add_executable(${name} ${_arg_SOURCES})
  if(MSVC)
    # Release tests must still execute assert() bodies (otherwise C4101 / false greens).
    target_compile_options(${name} PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/UNDEBUG>)
  else()
    # Release/RelWithDebInfo CI (ubuntu cmake job) must keep assert() active under -Werror.
    target_compile_options(${name} PRIVATE
      $<$<OR:$<CONFIG:Debug>,$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-UNDEBUG>)
  endif()
  if(_arg_LIBS)
    target_link_libraries(${name} PRIVATE ${_arg_LIBS})
    list(JOIN _arg_LIBS " " _bs_test_libs_joined)
    if(_bs_test_libs_joined MATCHES "bs_adapter_")
      target_sources(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/adapter/test/support/test_temp_dir.cpp
      )
      target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test)
    endif()
    if(BLESSSTAR_SANITIZER_CI)
      if(_bs_test_libs_joined MATCHES "bs_adapter_(registry|log|attach|orchestration)")
        target_sources(${name} PRIVATE
          ${CMAKE_SOURCE_DIR}/adapter/test/support/test_process_teardown.cpp
        )
        target_compile_definitions(${name} PRIVATE BLESSSTAR_SANITIZER_CI=1)
        if(NOT _bs_test_libs_joined MATCHES "bs_adapter_registry")
          target_link_libraries(${name} PRIVATE bs_adapter_registry)
        endif()
      endif()
    endif()
  endif()
  if(_arg_TEST_ARGS)
    add_test(NAME ${name} COMMAND "$<TARGET_FILE:${name}>" ${_arg_TEST_ARGS})
  else()
    add_test(NAME ${name} COMMAND "$<TARGET_FILE:${name}>")
  endif()
  set_tests_properties(${name} PROPERTIES TIMEOUT 300 LABELS "unit")
  set_property(GLOBAL APPEND PROPERTY BLESSSTAR_UNIT_TEST_TARGETS "${name}")
endfunction()

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_state_machine
  SOURCES kernel/state/test/StateMachineTest.cpp
  LIBS bs_kernel_state
)
blessstar_add_unit_test(bs_test_state_bus
  SOURCES kernel/state/test/StateBusTest.cpp
  LIBS bs_kernel_state
)
blessstar_add_unit_test(bs_test_config_manager
  SOURCES kernel/state/test/ConfigManagerTest.cpp
  LIBS bs_kernel_state
)
set_tests_properties(bs_test_config_manager
  PROPERTIES LABELS "unit;state;regression;day17")
blessstar_add_unit_test(bs_test_sharded_state_bus
  SOURCES kernel/state/test/ShardedStateBusTest.cpp
  LIBS bs_kernel_state
)
blessstar_add_unit_test(bs_test_state_all
  SOURCES kernel/state/test/AllTests.cpp
  LIBS bs_kernel_state
)

# ---------------------------------------------------------------------------
# IR + adapter requirement filter
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_ir
  SOURCES kernel/ir/test/IRTest.cpp
  LIBS bs_kernel_ir
)
blessstar_add_unit_test(bs_test_requirements
  SOURCES kernel/ir/test/RequirementsTest.cpp
  LIBS bs_kernel_ir
)
set_tests_properties(bs_test_requirements
  PROPERTIES LABELS "unit;registry;attach;regression")
blessstar_add_unit_test(bs_test_resolver
  SOURCES kernel/ir/test/ResolverTest.cpp
  LIBS bs_kernel_ir
)
blessstar_add_unit_test(bs_test_requirement_filter
  SOURCES adapter/test/RequirementFilterTest.cpp
  LIBS bs_adapter_requirement
)
set_tests_properties(bs_test_requirement_filter
  PROPERTIES LABELS "unit;registry;attach;regression")

# ---------------------------------------------------------------------------
# Parser (day 9 · M2)
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_json_lexer
  SOURCES adapter/test/JsonLexerTest.cpp
  LIBS bs_adapter_parser
)
blessstar_add_unit_test(bs_test_json_parser
  SOURCES adapter/test/JsonParserTest.cpp
  LIBS bs_adapter_parser
)
blessstar_add_unit_test(bs_test_config_parse
  SOURCES adapter/test/ConfigParseTest.cpp
  LIBS bs_adapter_parser bs_adapter_requirement
)
set_tests_properties(bs_test_json_lexer PROPERTIES LABELS "unit;parser;day9")
set_tests_properties(bs_test_json_parser PROPERTIES LABELS "unit;parser;day9")
set_tests_properties(bs_test_config_parse PROPERTIES LABELS "unit;parser;day9;regression")
blessstar_add_unit_test(bs_test_config_parse_boundary
  SOURCES adapter/test/ConfigParseBoundaryTest.cpp
  LIBS bs_adapter_parser bs_adapter_requirement
)
set_tests_properties(bs_test_config_parse_boundary
  PROPERTIES LABELS "unit;parser;day10;regression" TIMEOUT 120
)
blessstar_add_unit_test(bs_test_config_parse_security
  SOURCES adapter/test/ConfigParseSecurityTest.cpp
  LIBS bs_adapter_parser bs_adapter_requirement
)
set_tests_properties(bs_test_config_parse_security
  PROPERTIES LABELS "unit;parser;day11;regression" TIMEOUT 120
)
blessstar_add_unit_test(bs_test_config_parse_security_audit
  SOURCES adapter/test/ConfigParseSecurityAuditTest.cpp
  LIBS bs_adapter_parser bs_adapter_persistence bs_adapter_requirement
)
set_tests_properties(bs_test_config_parse_security_audit
  PROPERTIES LABELS "unit;parser;attach;day13;regression" TIMEOUT 300
)
blessstar_add_unit_test(bs_test_attach_resilience
  SOURCES adapter/test/AttachResilienceTest.cpp
  LIBS bs_adapter_orchestration bs_adapter_persistence bs_adapter_parser
       bs_adapter_requirement bs_adapter_attach bs_adapter_log
)
target_include_directories(bs_test_attach_resilience
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_attach_resilience PRIVATE BS_TESTING)
set_tests_properties(bs_test_attach_resilience
  PROPERTIES LABELS "unit;attach;day12;regression" TIMEOUT 120
)
blessstar_add_unit_test(bs_test_attach_atomicity
  SOURCES adapter/test/AttachAtomicityTest.cpp
  LIBS bs_adapter_persistence
)
target_include_directories(bs_test_attach_atomicity
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/persistence
)
set_tests_properties(bs_test_attach_atomicity
  PROPERTIES LABELS "unit;attach;day14;regression" TIMEOUT 120
)
blessstar_add_unit_test(bs_test_attach_fsync_spot
  SOURCES adapter/test/AttachFsyncSpotTest.cpp
  LIBS bs_adapter_persistence
)
target_include_directories(bs_test_attach_fsync_spot
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/persistence
)
set_tests_properties(bs_test_attach_fsync_spot
  PROPERTIES LABELS "unit;attach;day14;win_spot;regression" TIMEOUT 60
)
blessstar_add_unit_test(bs_test_attach_watch
  SOURCES adapter/test/AttachWatchTest.cpp
  LIBS bs_adapter_persistence
)
target_include_directories(bs_test_attach_watch
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/persistence
)
set_tests_properties(bs_test_attach_watch
  PROPERTIES LABELS "unit;attach;day15;regression" TIMEOUT 120
)
blessstar_add_unit_test(bs_test_app_sdk_contract
  SOURCES app/sdk/test/AppSdkContractTest.cpp
  LIBS bs_app_sdk
)
target_include_directories(bs_test_app_sdk_contract
  PRIVATE ${CMAKE_SOURCE_DIR}/app/sdk/include
)
set_tests_properties(bs_test_app_sdk_contract
  PROPERTIES LABELS "unit;app;day16;day17;regression" TIMEOUT 120
)
blessstar_add_unit_test(bs_test_vendor_config_normalizer
  SOURCES app/sdk/test/VendorConfigNormalizerTest.cpp
  LIBS bs_app_sdk bs_adapter_parser bs_kernel_ir bs_kernel_common
)
target_include_directories(bs_test_vendor_config_normalizer
  PRIVATE
    ${CMAKE_SOURCE_DIR}/app/sdk/include
    ${CMAKE_SOURCE_DIR}/adapter/test
)
set_tests_properties(bs_test_vendor_config_normalizer
  PROPERTIES
    LABELS "unit;app;day17;regression"
    TIMEOUT 120
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
blessstar_add_unit_test(bs_test_app_vendor_reload_integration
  SOURCES app/sdk/test/AppVendorReloadIntegrationTest.cpp
  LIBS
    bs_app_sdk
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_parser
    bs_kernel_io
    bs_kernel_report
    bs_kernel_common
)
target_include_directories(bs_test_app_vendor_reload_integration
  PRIVATE
    ${CMAKE_SOURCE_DIR}/app/sdk/include
    ${CMAKE_SOURCE_DIR}/adapter/test
)
set_tests_properties(bs_test_app_vendor_reload_integration
  PROPERTIES
    LABELS "unit;integration;app;day17;regression"
    TIMEOUT 180
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESOURCE_LOCK "attach_integration"
)
blessstar_add_unit_test(bs_test_reload_config_json_integration
  SOURCES adapter/test/ReloadConfigJsonIntegrationTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_kernel_io
    bs_kernel_report
    bs_kernel_common
)
target_include_directories(bs_test_reload_config_json_integration
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
set_tests_properties(bs_test_reload_config_json_integration
  PROPERTIES LABELS "unit;integration;day9;io;attach;parser;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)

# ---------------------------------------------------------------------------
# Registry (day 5)
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_path_registry
  SOURCES kernel/registry/test/PathRegistryTest.cpp
  LIBS bs_kernel_registry
)
blessstar_add_unit_test(bs_test_registry_hub
  SOURCES kernel/registry/test/RegistryHubTest.cpp
  LIBS bs_kernel_registry
)
blessstar_add_unit_test(bs_test_registry_facade
  SOURCES kernel/registry/test/RegistryFacadeTest.cpp
  LIBS bs_kernel_registry
)
blessstar_add_unit_test(bs_test_registry_guard
  SOURCES kernel/registry/test/RegistryGuardTest.cpp
  LIBS bs_kernel_registry
)
blessstar_add_unit_test(bs_test_registry_integration
  SOURCES adapter/test/RegistryIntegrationTest.cpp
  LIBS bs_adapter_registry bs_adapter_requirement bs_kernel_ir bs_kernel_common
)
set_tests_properties(bs_test_path_registry PROPERTIES LABELS "unit;registry;day8;regression")
set_tests_properties(bs_test_registry_hub PROPERTIES LABELS "unit;registry;regression")
set_tests_properties(bs_test_registry_facade PROPERTIES LABELS "unit;registry;regression")
set_tests_properties(bs_test_registry_guard PROPERTIES LABELS "unit;registry;regression")
set_tests_properties(bs_test_registry_integration
  PROPERTIES LABELS "unit;registry;attach;regression"
                   RESOURCE_LOCK "attach_integration")

# Attach-phase integration: implemented pipeline only (see test file header).
blessstar_add_unit_test(bs_test_attach_pipeline_registry
  SOURCES adapter/test/AttachPipelineRegistryTest.cpp
  LIBS bs_adapter_registry bs_kernel_common
)
set_tests_properties(bs_test_attach_pipeline_registry
  PROPERTIES LABELS "unit;registry;attach;integration;regression" TIMEOUT 120
                   RESOURCE_LOCK "attach_integration")

blessstar_add_unit_test(bs_test_registry_attach_contract
  SOURCES adapter/test/RegistryAttachContractTest.cpp
  LIBS bs_adapter_registry bs_kernel_common
)
set_tests_properties(bs_test_registry_attach_contract
  PROPERTIES LABELS "unit;registry;attach;regression" TIMEOUT 120)

# Topic regression (local/CI):
#   ctest --test-dir <build> -C <Config> -L attach --output-on-failure
#   ctest --test-dir <build> -C <Config> -L registry --output-on-failure

# ---------------------------------------------------------------------------
# IO (day 6)
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_io_facade
  SOURCES kernel/io/test/IoFacadeTest.cpp
  LIBS bs_kernel_io bs_kernel_common
)
blessstar_add_unit_test(bs_test_io_local_provider
  SOURCES adapter/test/IoLocalProviderTest.cpp
  LIBS bs_adapter_io bs_kernel_common
)
blessstar_add_unit_test(bs_test_io_registry_phase
  SOURCES adapter/test/IoRegistryPhaseTest.cpp
  LIBS bs_adapter_registry bs_adapter_io bs_kernel_common
)
blessstar_add_unit_test(bs_test_io_facade_boundary
  SOURCES kernel/io/test/IoFacadeBoundaryTest.cpp
  LIBS bs_kernel_io bs_kernel_common
)
blessstar_add_unit_test(bs_test_io_local_provider_boundary
  SOURCES adapter/test/IoLocalProviderBoundaryTest.cpp
  LIBS bs_adapter_io bs_kernel_common
)
blessstar_add_unit_test(bs_test_io_provider_stub
  SOURCES adapter/test/IoProviderStubTest.cpp
  LIBS bs_adapter_io bs_kernel_common
)
blessstar_add_unit_test(bs_test_io_reload_batch
  SOURCES adapter/test/ReloadBatchControllerTest.cpp
  LIBS bs_adapter_orchestration bs_adapter_log bs_kernel_common
)
target_include_directories(bs_test_io_reload_batch
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
blessstar_add_unit_test(bs_test_reload_per_batch_config_manager
  SOURCES adapter/test/ReloadPerBatchConfigManagerTest.cpp
  LIBS bs_adapter_orchestration bs_adapter_registry bs_adapter_attach bs_adapter_log
       bs_kernel_state bs_kernel_common
)
target_include_directories(bs_test_reload_per_batch_config_manager
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_reload_per_batch_config_manager PRIVATE BS_TESTING)
set_tests_properties(bs_test_reload_per_batch_config_manager
  PROPERTIES LABELS "unit;integration;state;day17;regression" TIMEOUT 120
                   RESOURCE_LOCK "attach_integration"
)
blessstar_add_unit_test(bs_test_reload_ir_execute_integration
  SOURCES adapter/test/ReloadIrExecuteIntegrationTest.cpp
  LIBS bs_adapter_orchestration bs_adapter_registry bs_adapter_attach bs_adapter_log
       bs_kernel_runtime bs_kernel_common
)
target_include_directories(bs_test_reload_ir_execute_integration
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
set_tests_properties(bs_test_reload_ir_execute_integration
  PROPERTIES LABELS "unit;integration;runtime;attach;day17;regression" TIMEOUT 120
                   RESOURCE_LOCK "attach_integration"
)
blessstar_add_unit_test(bs_test_reload_per_batch_parallel_exec
  SOURCES adapter/test/ReloadPerBatchParallelExecTest.cpp
  LIBS bs_adapter_orchestration bs_adapter_registry bs_adapter_attach bs_adapter_log
       bs_kernel_runtime bs_kernel_common
)
target_include_directories(bs_test_reload_per_batch_parallel_exec
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
set_tests_properties(bs_test_reload_per_batch_parallel_exec
  PROPERTIES LABELS "unit;integration;runtime;kernel_pool;regression" TIMEOUT 120
                   RESOURCE_LOCK "attach_integration"
)
blessstar_add_unit_test(bs_test_io_facade_max_read
  SOURCES kernel/io/test/IoFacadeMaxReadTest.cpp
  LIBS bs_kernel_io bs_kernel_common
)
blessstar_add_unit_test(bs_test_io_local_provider_timeout
  SOURCES adapter/test/IoLocalProviderTimeoutTest.cpp
  LIBS bs_adapter_io
)
blessstar_add_unit_test(bs_test_io_attach_pipeline
  SOURCES adapter/test/IoAttachPipelineTest.cpp
  LIBS bs_adapter_registry bs_adapter_io bs_kernel_common
)
blessstar_add_unit_test(bs_test_registry_bootstrap_io
  SOURCES adapter/test/RegistryBootstrapIoTest.cpp
  LIBS bs_adapter_registry bs_kernel_common
)
blessstar_add_unit_test(bs_test_attach_context_bootstrap
  SOURCES adapter/test/AttachContextBootstrapTest.cpp
  LIBS bs_adapter_registry bs_kernel_common
)
blessstar_add_unit_test(bs_test_attach_freeze_eventbus_integration
  SOURCES adapter/test/AttachFreezeEventBusIntegrationTest.cpp
  LIBS bs_adapter_registry bs_kernel_state bs_kernel_common
)
blessstar_add_unit_test(bs_test_attach_facade_freeze_delegate
  SOURCES adapter/test/AttachFacadeFreezeDelegateTest.cpp
  LIBS bs_adapter_registry bs_adapter_attach bs_kernel_state bs_kernel_common
)
set_tests_properties(bs_test_attach_facade_freeze_delegate
  PROPERTIES LABELS "unit;integration;attach;day17;regression" TIMEOUT 120
                   RESOURCE_LOCK "attach_integration")
blessstar_add_unit_test(bs_test_plugin_loader_attach
  SOURCES adapter/test/PluginLoaderAttachTest.cpp
  LIBS bs_adapter_registry bs_kernel_common
)
blessstar_add_unit_test(bs_test_plugin_orch_reload_registry
  SOURCES adapter/test/PluginOrchReloadRegistryTest.cpp
  LIBS bs_adapter_registry bs_adapter_orchestration bs_kernel_common
)
blessstar_add_unit_test(bs_test_plugin_ir_requirements
  SOURCES adapter/test/PluginIrRequirementsTest.cpp
  LIBS bs_adapter_plugin_support bs_adapter_requirement
)
blessstar_add_unit_test(bs_test_attach_manifest_yaml
  SOURCES adapter/test/AttachManifestYamlTest.cpp
  LIBS bs_adapter_plugin_loader
)
target_include_directories(bs_test_plugin_ir_requirements PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_include_directories(bs_test_attach_manifest_yaml PRIVATE ${CMAKE_BINARY_DIR}/generated)
blessstar_add_unit_test(bs_test_plugin_log_domains_attach_integration
  SOURCES adapter/test/PluginLogDomainsAttachIntegrationTest.cpp
  LIBS bs_adapter_registry bs_kernel_common bs_kernel_common_format
)
target_include_directories(bs_test_plugin_log_domains_attach_integration
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
blessstar_add_unit_test(bs_test_day8_attach_full_integration
  SOURCES adapter/test/Day8AttachFullIntegrationTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_kernel_io
    bs_kernel_report
    bs_kernel_common
    bs_kernel_common_format
)
target_include_directories(bs_test_day8_attach_full_integration
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
set_tests_properties(bs_test_io_facade PROPERTIES LABELS "unit;io;regression")
set_tests_properties(bs_test_io_local_provider PROPERTIES LABELS "unit;io;regression")
set_tests_properties(bs_test_io_registry_phase PROPERTIES LABELS "unit;io;registry;regression")
set_tests_properties(bs_test_io_facade_boundary PROPERTIES LABELS "unit;io;regression")
set_tests_properties(bs_test_io_local_provider_boundary PROPERTIES LABELS "unit;io;regression")
set_tests_properties(bs_test_io_provider_stub PROPERTIES LABELS "unit;io;registry;regression")
set_tests_properties(bs_test_io_reload_batch PROPERTIES LABELS "unit;io;regression")
set_tests_properties(bs_test_io_facade_max_read PROPERTIES LABELS "unit;io;regression")
set_tests_properties(bs_test_io_local_provider_timeout PROPERTIES LABELS "unit;io;regression")
set_tests_properties(bs_test_io_attach_pipeline
  PROPERTIES LABELS "unit;io;registry;attach;integration;regression" TIMEOUT 120)
set_tests_properties(bs_test_registry_bootstrap_io
  PROPERTIES LABELS "unit;io;registry;attach;regression")
set_tests_properties(bs_test_attach_context_bootstrap
  PROPERTIES LABELS "unit;registry;attach;day8;regression")
set_tests_properties(bs_test_attach_freeze_eventbus_integration
  PROPERTIES LABELS "unit;registry;attach;day8;integration;regression")
set_tests_properties(bs_test_plugin_loader_attach
  PROPERTIES LABELS "unit;registry;attach;day8;regression")
set_tests_properties(bs_test_plugin_orch_reload_registry
  PROPERTIES LABELS "unit;registry;attach;day8;regression")
set_tests_properties(bs_test_plugin_ir_requirements
  PROPERTIES LABELS "unit;registry;attach;day8;regression")
set_tests_properties(bs_test_attach_manifest_yaml
  PROPERTIES LABELS "unit;registry;attach;day8;regression")
set_tests_properties(bs_test_plugin_log_domains_attach_integration
  PROPERTIES LABELS "unit;registry;attach;day8;integration;regression" TIMEOUT 120)
set_tests_properties(bs_test_day8_attach_full_integration
  PROPERTIES LABELS "unit;registry;attach;day8;io;integration;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration")

# IO regression (local/CI):
#   ctest --test-dir <build> -C <Config> -L io --output-on-failure
#
# Regression / integration (local/CI):
#   ctest --test-dir <build> -C <Config> -L regression --output-on-failure
#   ctest --test-dir <build> -C <Config> -L integration --output-on-failure
#   ctest --test-dir <build> -C <Config> -L day7 --output-on-failure
#   ctest --test-dir <build> -C <Config> -L day8 --output-on-failure

# ---------------------------------------------------------------------------
# Pipeline / Report
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_pipeline
  SOURCES kernel/pipeline/test/PipelineTest.cpp
  LIBS bs_kernel_pipeline
)
blessstar_add_unit_test(bs_test_report
  SOURCES kernel/report/test/ReportTest.cpp
  LIBS bs_kernel_report
)

blessstar_add_unit_test(bs_test_kernel_runtime
  SOURCES kernel/runtime/test/KernelRuntimeTest.cpp
  LIBS bs_kernel_runtime bs_kernel_pipeline bs_kernel_ir bs_kernel_report_core
)
set_tests_properties(bs_test_kernel_runtime
  PROPERTIES LABELS "unit;runtime;regression;day17" TIMEOUT 120
)
blessstar_add_unit_test(bs_test_kernel_executor
  SOURCES kernel/runtime/test/KernelExecutorTest.cpp
  LIBS bs_kernel_runtime bs_kernel_pipeline bs_kernel_ir bs_kernel_report_core
)
set_tests_properties(bs_test_kernel_executor
  PROPERTIES LABELS "unit;runtime;kernel_conc;kernel_pool;regression" TIMEOUT 120
)
blessstar_add_unit_test(bs_test_kernel_pool
  SOURCES kernel/runtime/test/KernelPoolTest.cpp
  LIBS bs_kernel_runtime bs_kernel_ir bs_kernel_report_core
)
set_tests_properties(bs_test_kernel_pool
  PROPERTIES LABELS "unit;runtime;kernel_conc;kernel_pool;regression" TIMEOUT 120
)

# ---------------------------------------------------------------------------
# Boundary
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_ir_boundary
  SOURCES kernel/ir/test/IRBoundaryTest.cpp
  LIBS bs_kernel_ir
)
blessstar_add_unit_test(bs_test_pipeline_boundary
  SOURCES kernel/pipeline/test/PipelineBoundaryTest.cpp
  LIBS bs_kernel_pipeline bs_kernel_report
)
blessstar_add_unit_test(bs_test_report_boundary
  SOURCES kernel/report/test/ReportBoundaryTest.cpp
  LIBS bs_kernel_report
)
blessstar_add_unit_test(bs_test_state_boundary
  SOURCES kernel/state/test/StateBoundaryTest.cpp
  LIBS bs_kernel_state bs_kernel_common
)

# ---------------------------------------------------------------------------
# Common
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_metrics
  SOURCES kernel/common/test/MetricsTest.cpp
  LIBS bs_kernel_common
)
blessstar_add_unit_test(bs_test_memory_pool
  SOURCES kernel/common/test/MemoryPoolTest.cpp
  LIBS bs_kernel_common
)
blessstar_add_unit_test(bs_test_plugin
  SOURCES kernel/common/test/PluginTest.cpp
  LIBS bs_kernel_common
)
blessstar_add_unit_test(bs_test_status
  SOURCES kernel/common/test/BsStatusTest.cpp
  LIBS bs_kernel_common bs_kernel_registry bs_kernel_io bs_kernel_common_format
)
blessstar_add_unit_test(bs_test_log
  SOURCES kernel/common/test/BsLogTest.cpp
  LIBS bs_kernel_common bs_kernel_test_support
)
blessstar_add_unit_test(bs_test_registry_status_domain
  SOURCES kernel/registry/test/StatusDomainFreezeTest.cpp
  LIBS bs_kernel_registry
)
blessstar_add_unit_test(bs_test_reload_report
  SOURCES adapter/test/ReloadReportTest.cpp
  LIBS bs_adapter_orchestration bs_adapter_log bs_kernel_report bs_kernel_common
)
set_tests_properties(bs_test_status PROPERTIES LABELS "unit;day7;registry;day8;regression")
set_tests_properties(bs_test_log PROPERTIES LABELS "unit;day7;regression")
set_tests_properties(bs_test_registry_status_domain PROPERTIES LABELS "unit;day7;registry;regression")

# ---------------------------------------------------------------------------
# Day 7 — additional unit + integration (implemented chains only)
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_reload_attach_guard
  SOURCES adapter/test/ReloadAttachGuardTest.cpp
  LIBS bs_adapter_orchestration bs_adapter_attach bs_kernel_common
)
blessstar_add_unit_test(bs_test_reload_gate_default
  SOURCES adapter/test/ReloadGateDefaultTest.cpp
  LIBS bs_adapter_orchestration bs_adapter_log bs_kernel_common
)
target_include_directories(bs_test_reload_gate_default
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
blessstar_add_unit_test(bs_test_log_domain_registration
  SOURCES kernel/registry/test/LogDomainRegistrationTest.cpp
  LIBS bs_kernel_registry bs_kernel_io
)
blessstar_add_unit_test(bs_test_log_audit
  SOURCES kernel/common/test/BsLogAuditTest.cpp
  LIBS bs_kernel_common bs_kernel_test_support
)
blessstar_add_unit_test(bs_test_reentrancy_io
  SOURCES adapter/test/BsReentrancyIoTest.cpp
  LIBS bs_adapter_registry bs_adapter_attach bs_kernel_io bs_kernel_state
       bs_kernel_test_support bs_kernel_common
)
blessstar_add_unit_test(bs_test_result_status_map
  SOURCES kernel/report/test/ResultStatusMapTest.cpp
  LIBS bs_kernel_report bs_kernel_common
)
blessstar_add_unit_test(bs_test_io_status_table
  SOURCES kernel/io/test/IoStatusTableTest.cpp
  LIBS bs_kernel_io bs_kernel_registry bs_kernel_common bs_kernel_common_format
)
blessstar_add_unit_test(bs_test_reload_default_gate_report_eventbus_reentry_integration
  SOURCES adapter/test/ReloadDefaultGateReportEventBusReentryIntegrationTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_attach
    bs_adapter_log
    bs_kernel_io
    bs_kernel_state
    bs_kernel_report
    bs_kernel_common
)
set_tests_properties(bs_test_reload_attach_guard
  PROPERTIES LABELS "unit;day7;io;attach;regression"
                   RESOURCE_LOCK "attach_integration")
set_tests_properties(bs_test_reload_gate_default PROPERTIES LABELS "unit;day7;day9;io;parser;regression")
set_tests_properties(bs_test_log_domain_registration PROPERTIES LABELS "unit;day7;registry;regression")
set_tests_properties(bs_test_log_audit PROPERTIES LABELS "unit;day7;regression")
set_tests_properties(bs_test_reentrancy_io
  PROPERTIES LABELS "unit;day7;io;registry;attach;regression"
                   RESOURCE_LOCK "attach_integration")
set_tests_properties(bs_test_result_status_map PROPERTIES LABELS "unit;day7;regression")
set_tests_properties(bs_test_io_status_table PROPERTIES LABELS "unit;day7;io;registry;regression")
set_tests_properties(bs_test_reload_report PROPERTIES LABELS "unit;day7;io;regression")
set_tests_properties(bs_test_reload_default_gate_report_eventbus_reentry_integration
  PROPERTIES LABELS "unit;integration;day7;attach;io;registry;regression" TIMEOUT 120
                   RESOURCE_LOCK "attach_integration")

# ---------------------------------------------------------------------------
# Day19 · memory baseline + 72h-RP stress harness (T19)
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_day19_memory_baseline
  SOURCES adapter/test/Day19MemoryBaselineTest.cpp
  LIBS bs_adapter_parser bs_kernel_common
)
set_tests_properties(bs_test_day19_memory_baseline
  PROPERTIES LABELS "unit;day19;mem;regression" TIMEOUT 120
)
if(WIN32)
  target_sources(bs_test_day19_memory_baseline PRIVATE
    ${CMAKE_SOURCE_DIR}/adapter/test/support/day19_rss_sampler_win.cpp
  )
endif()

blessstar_add_unit_test(bs_test_day19_stress_reload_loop
  SOURCES adapter/test/Day19StressReloadLoopTest.cpp
  TEST_ARGS --profile=ci
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_common
)
target_include_directories(bs_test_day19_stress_reload_loop
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_day19_stress_reload_loop PRIVATE BS_TESTING)
set_tests_properties(bs_test_day19_stress_reload_loop
  PROPERTIES LABELS "unit;day19;stress;regression" TIMEOUT 600
                   RESOURCE_LOCK "attach_integration"
)
if(WIN32)
  target_sources(bs_test_day19_stress_reload_loop PRIVATE
    ${CMAKE_SOURCE_DIR}/adapter/test/support/day19_rss_sampler_win.cpp
  )
endif()

# smoke_fail_ci (~25s); 900s negative run uses day19-stress-smoke-fail workflow.
add_test(NAME bs_test_day19_stress_fail_ci
         COMMAND "$<TARGET_FILE:bs_test_day19_stress_reload_loop>" --profile=smoke_fail_ci)
set_tests_properties(bs_test_day19_stress_fail_ci
  PROPERTIES LABELS "unit;day19;stress;regression;negative" TIMEOUT 600
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_concurrency
  SOURCES adapter/test/AttachConcurrencyTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_state
    bs_kernel_common
)
target_include_directories(bs_test_attach_concurrency
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
set_tests_properties(bs_test_attach_concurrency
  PROPERTIES LABELS "unit;day20;attach;regression" TIMEOUT 180
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_p2_shortcoming_regression
  SOURCES adapter/test/AttachP2ShortcomingRegressionTest.cpp
  LIBS
    bs_adapter_attach
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_runtime
    bs_kernel_common
    bs_kernel_test_support
)
target_include_directories(bs_test_attach_p2_shortcoming_regression
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
set_tests_properties(bs_test_attach_p2_shortcoming_regression
  PROPERTIES LABELS "unit;arch_gap;day20;attach;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_day19_shortcoming_regression
  SOURCES adapter/test/AttachDay19ShortcomingRegressionTest.cpp
  LIBS
    bs_adapter_attach
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_runtime
    bs_kernel_common
    bs_kernel_test_support
)
target_include_directories(bs_test_attach_day19_shortcoming_regression
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_attach_day19_shortcoming_regression PRIVATE BS_TESTING)
set_tests_properties(bs_test_attach_day19_shortcoming_regression
  PROPERTIES LABELS "unit;arch_gap;day19;attach;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)
set_tests_properties(bs_test_attach_p2_shortcoming_regression
  PROPERTIES DEPENDS bs_test_attach_day19_shortcoming_regression
)

blessstar_add_unit_test(bs_test_attach_recover_cold
  SOURCES adapter/test/AttachRecoverColdTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_report
    bs_kernel_common
)
target_include_directories(bs_test_attach_recover_cold
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_attach_recover_cold PRIVATE BS_TESTING)
set_tests_properties(bs_test_attach_recover_cold
  PROPERTIES LABELS "unit;recover;arch_gap;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_recover_fsm
  SOURCES adapter/test/AttachRecoverFsmTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_adapter_persistence
    bs_kernel_io
    bs_kernel_common
)
target_include_directories(bs_test_attach_recover_fsm
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_attach_recover_fsm PRIVATE BS_TESTING)
set_tests_properties(bs_test_attach_recover_fsm
  PROPERTIES LABELS "unit;recover;arch_gap;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_recover_sidecar
  SOURCES adapter/test/AttachRecoverSidecarTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_report
    bs_kernel_common
)
target_include_directories(bs_test_attach_recover_sidecar
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_attach_recover_sidecar PRIVATE BS_TESTING)
set_tests_properties(bs_test_attach_recover_sidecar
  PROPERTIES LABELS "unit;recover;arch_gap;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_recover_concurrency
  SOURCES adapter/test/AttachRecoverConcurrencyTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_report
    bs_kernel_common
)
target_include_directories(bs_test_attach_recover_concurrency
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_attach_recover_concurrency PRIVATE BS_TESTING)
set_tests_properties(bs_test_attach_recover_concurrency
  PROPERTIES LABELS "unit;recover;arch_gap;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_dual_truth_sync
  SOURCES adapter/test/AttachDualTruthSyncTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_report
    bs_kernel_common
)
target_include_directories(bs_test_attach_dual_truth_sync
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_attach_dual_truth_sync PRIVATE BS_TESTING)
set_tests_properties(bs_test_attach_dual_truth_sync
  PROPERTIES LABELS "unit;arch_gap;runtime;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_wire_full_chain_integration
  SOURCES adapter/test/AttachWireFullChainIntegrationTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_report
    bs_kernel_common
)
target_include_directories(bs_test_attach_wire_full_chain_integration
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_attach_wire_full_chain_integration PRIVATE BS_TESTING)
set_tests_properties(bs_test_attach_wire_full_chain_integration
  PROPERTIES LABELS "unit;wire;integration;arch_gap;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_notify_flush
  SOURCES adapter/test/AttachNotifyFlushTest.cpp
  LIBS
    bs_adapter_registry
    bs_adapter_orchestration
    bs_adapter_log
    bs_kernel_io
    bs_kernel_report
    bs_kernel_common
    bs_kernel_state
)
target_include_directories(bs_test_attach_notify_flush
  PRIVATE ${CMAKE_SOURCE_DIR}/adapter/test
)
target_compile_definitions(bs_test_attach_notify_flush PRIVATE BS_TESTING)
set_tests_properties(bs_test_attach_notify_flush
  PROPERTIES LABELS "unit;arch_gap;day20;attach;regression" TIMEOUT 300
                   RESOURCE_LOCK "attach_integration"
)

blessstar_add_unit_test(bs_test_attach_watch_benchmark
  SOURCES adapter/test/AttachWatchBenchmark.cpp
  LIBS bs_adapter_persistence
)
set_tests_properties(bs_test_attach_watch_benchmark
  PROPERTIES LABELS "unit;day20;attach;benchmark" TIMEOUT 180
)

# ---------------------------------------------------------------------------
# Comprehensive (still unit scope; may be slower)
# ---------------------------------------------------------------------------
blessstar_add_unit_test(bs_test_memory_pool_comprehensive
  SOURCES kernel/common/test/MemoryPoolComprehensiveTest.cpp
  LIBS bs_kernel_common
)
set_tests_properties(bs_test_memory_pool_comprehensive PROPERTIES LABELS "unit;comprehensive")

blessstar_add_unit_test(bs_test_metrics_comprehensive
  SOURCES kernel/common/test/MetricsComprehensiveTest.cpp
  LIBS bs_kernel_common
)
set_tests_properties(bs_test_metrics_comprehensive PROPERTIES LABELS "unit;comprehensive")

# ---------------------------------------------------------------------------
# Day16/Day17: contract + style gates (Python; requires Interpreter at configure)
# ---------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(
    NAME bs_test_day16_contract_registry_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/scripts/gates/check_day16_contract_registry.py"
            "${CMAKE_SOURCE_DIR}/docs/DAY16_CONTRACT_REGISTRY_TEMPLATE.md"
  )
  set_tests_properties(bs_test_day16_contract_registry_check
    PROPERTIES LABELS "unit;docs;day16;regression" TIMEOUT 60
  )
  add_test(
    NAME bs_test_day16_contract_files_check
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/scripts/gates/check_day16_contract_files.py"
            "${CMAKE_SOURCE_DIR}/docs/contracts/architecture.contracts.json"
            "${CMAKE_SOURCE_DIR}/docs/contracts/integration.contracts.json"
  )
  set_tests_properties(bs_test_day16_contract_files_check
    PROPERTIES LABELS "unit;docs;day16;regression" TIMEOUT 60
  )

  set(_bs_scripts_dir "${CMAKE_SOURCE_DIR}/tools/scripts")
  set(_bs_contracts_dir "${_bs_scripts_dir}/contracts")
  set(_bs_gates_dir "${_bs_scripts_dir}/gates")
  set(_bs_format_dir "${_bs_scripts_dir}/format")

  add_test(
    NAME bs_test_day17_contract_files_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_contracts_dir}/contract_validate_instances.py"
  )
  set_tests_properties(bs_test_day17_contract_files_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 60
  )

  add_test(
    NAME bs_test_day17_public_api_prefix_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_public_api_prefix.py"
  )
  set_tests_properties(bs_test_day17_public_api_prefix_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 120
  )

  add_test(
    NAME bs_test_day17_layered_api_prefix_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_layered_api_prefix.py"
  )
  set_tests_properties(bs_test_day17_layered_api_prefix_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 120
  )

  add_test(
    NAME bs_test_day17_vendor_config_boundary_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_vendor_config_boundary.py"
  )
  set_tests_properties(bs_test_day17_vendor_config_boundary_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 60
  )

  add_test(
    NAME bs_test_attach_sync_prod_paths_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_attach_sync_prod_paths.py"
  )
  set_tests_properties(bs_test_attach_sync_prod_paths_check
    PROPERTIES LABELS "unit;docs;arch_gap;regression" TIMEOUT 60
  )

  add_test(
    NAME bs_test_day17_namespace_boundary_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_namespace_boundary.py"
  )
  set_tests_properties(bs_test_day17_namespace_boundary_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 60
  )

  add_test(
    NAME bs_test_day17_target_name_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_cmake_target_names.py"
  )
  set_tests_properties(bs_test_day17_target_name_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 60
  )

  add_test(
    NAME bs_test_day17_contract_id_prefix_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_contract_id_prefix.py"
  )
  set_tests_properties(bs_test_day17_contract_id_prefix_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 60
  )

  add_test(
    NAME bs_test_day17_ctest_labels_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_ctest_labels.py"
  )
  set_tests_properties(bs_test_day17_ctest_labels_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 60
  )

  add_test(
    NAME bs_test_day17_test_tempdir_unique_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_test_tempdir_unique.py"
  )
  set_tests_properties(bs_test_day17_test_tempdir_unique_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 120
  )

  add_test(
    NAME bs_test_day17_test_environment_tier_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_test_environment_tiers.py"
  )
  set_tests_properties(bs_test_day17_test_environment_tier_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 60
  )

  add_test(
    NAME bs_test_day17_test_l1_gate_scope_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_contract_gate_runner_l1_scope.py"
  )
  set_tests_properties(bs_test_day17_test_l1_gate_scope_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 60
  )

  add_test(
    NAME bs_test_day17_public_header_contract_block_check
    COMMAND ${Python3_EXECUTABLE} "${_bs_gates_dir}/check_public_header_contract_block.py"
  )
  set_tests_properties(bs_test_day17_public_header_contract_block_check
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 120
  )

  add_test(
    NAME bs_test_day17_clang_format_include
    COMMAND ${Python3_EXECUTABLE} "${_bs_format_dir}/run_clang_format_check.py"
            --batch include
  )
  set_tests_properties(bs_test_day17_clang_format_include
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 600
  )

  add_test(
    NAME bs_test_day17_clang_format_src
    COMMAND ${Python3_EXECUTABLE} "${_bs_format_dir}/run_clang_format_check.py"
            --batch src
  )
  set_tests_properties(bs_test_day17_clang_format_src
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 900
  )

  add_test(
    NAME bs_test_day17_contract_compile
    COMMAND ${Python3_EXECUTABLE} "${_bs_contracts_dir}/contract_compile.py"
  )
  set_tests_properties(bs_test_day17_contract_compile
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 120
  )

  add_test(
    NAME bs_test_day17_contract_gate_runner
    COMMAND ${Python3_EXECUTABLE} "${_bs_contracts_dir}/contract_gate_runner.py"
            --through-stage ci
  )
  set_tests_properties(bs_test_day17_contract_gate_runner
    PROPERTIES LABELS "unit;docs;day17;regression" TIMEOUT 1900
  )
endif()
