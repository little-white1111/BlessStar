cmake_minimum_required(VERSION 4.3)
project(BlessStar VERSION 1.0 LANGUAGES CXX)

# ── Build output convention ──────────────────────────────────────
# Default: cmake --preset default   (or)   cmake -B build
# CMakePresets.json locks binaryDir to build/
# Produces:
#   build/bin/  — executables and DLLs
#   build/lib/  — static and import libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: treat all source files as UTF-8
if(MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>")
endif()

enable_testing()

# Include directories for all layers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/common/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/io/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/ir/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/schema/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/ui_map/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/app/bs_db_core/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/app/bs_db_mgmt/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/pipeline/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/registry/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/report/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/runtime/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kernel/state/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/adapter/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/adapter/parser/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/app/sdk/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/app/sdk/src/agent_indexer/include)

option(BS_WITH_DB "Enable database integration features" ON)

# ── Imported libraries ──────────────────────────────────────────────
add_library(sqlite3_lib STATIC IMPORTED)
set_target_properties(sqlite3_lib PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.lib"
)

add_library(mysql_dll SHARED IMPORTED)
set_target_properties(mysql_dll PROPERTIES
    IMPORTED_LOCATION "C:/Program Files/MySQL/MySQL Server 9.7/lib/libmysql.dll"
    IMPORTED_IMPLIB   "C:/Program Files/MySQL/MySQL Server 9.7/lib/libmysql.lib"
)

set(SQLITE3_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3")
set(MYSQL_INCLUDE_DIR    "C:/Program Files/MySQL/MySQL Server 9.7/include")

# ── bs_app_sdk ──────────────────────────────────────────────────────
add_library(bs_app_sdk
    app/sdk/src/app_contract.cpp
    app/sdk/src/app_ir_mapper.cpp
    app/sdk/src/app_scenario_policy.cpp
    app/sdk/src/vendor_config_normalizer.cpp
    app/sdk/src/vendor_reload_facade.cpp
    app/sdk/src/app_vendor_precheck.cpp
    app/sdk/src/app_meta_rule.cpp
    app/sdk/src/config_reload_session.cpp
    app/sdk/src/config_session_reader.cpp
    app/sdk/src/app_session.cpp
    app/sdk/src/mem_audit_log.cpp
    app/sdk/src/shm/shm_manager.cpp
    app/sdk/src/shm/double_buffer_writer.cpp
    app/sdk/src/shm/eventfd_notifier.cpp
    app/sdk/src/config_consumer_manager.cpp
    app/sdk/src/config_consumer_abi.cpp
    app/sdk/src/config_consumer.cpp
    app/sdk/src/watch_callback_adapter.cpp
    app/sdk/src/db/config_raw_parse.cpp
    app/sdk/src/agent_indexer/src/agent_indexer.c
    app/sdk/src/bs_biz_introspector.c
)

# Build bs_db_core independent pure DB interfaces
add_subdirectory(app/bs_db_core)

# Build bs_db_mgmt management layer
add_subdirectory(app/bs_db_mgmt)

# Build schema library
add_subdirectory(kernel/schema)

# Build ui_map library
add_subdirectory(kernel/ui_map)

# Build gate_chain library
add_subdirectory(kernel/gate_chain)

# Link kernel libraries unconditionally
target_link_libraries(bs_app_sdk PRIVATE bs_kernel_schema bs_kernel_gate_chain)

# Build tests
add_subdirectory(app/sdk/test)

# Register module tests from Tests.cmake
include(cmake/Tests.cmake)

if(BS_WITH_DB)
    target_compile_definitions(bs_app_sdk PRIVATE BS_WITH_DB=1)

    target_include_directories(bs_app_sdk PRIVATE
        ${SQLITE3_INCLUDE_DIR}
        ${MYSQL_INCLUDE_DIR}
    )

    target_link_libraries(bs_app_sdk PRIVATE bs_db_core bs_db_mgmt sqlite3_lib mysql_dll)

    target_sources(bs_app_sdk PRIVATE
        app/sdk/src/db_drivers/sqlite_connector.cpp
        app/sdk/src/db_drivers/mysql_connector.cpp
    )

    # ── SQLite smoke test ──────────────────────────────────────────
    add_executable(sqlite_smoke_test
        app/sdk/test/db/sqlite_smoke_test.cpp
        app/sdk/src/db_drivers/sqlite_connector.cpp
    )
    target_include_directories(sqlite_smoke_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/app/bs_db_core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/app/bs_db_mgmt/include
        ${SQLITE3_INCLUDE_DIR}
    )
    target_compile_definitions(sqlite_smoke_test PRIVATE BS_WITH_DB=1)
    target_link_libraries(sqlite_smoke_test PRIVATE bs_db_core bs_db_mgmt sqlite3_lib)
    add_test(NAME sqlite_smoke_test COMMAND sqlite_smoke_test)

    # ── MySQL smoke test ───────────────────────────────────────────
    add_executable(mysql_smoke_test
        app/sdk/test/db/mysql_smoke_test.cpp
        app/sdk/src/db_drivers/mysql_connector.cpp
    )
    target_include_directories(mysql_smoke_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/app/bs_db_core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/app/bs_db_mgmt/include
        ${MYSQL_INCLUDE_DIR}
    )
    target_compile_definitions(mysql_smoke_test PRIVATE BS_WITH_DB=1)
    target_link_libraries(mysql_smoke_test PRIVATE bs_db_core bs_db_mgmt mysql_dll sqlite3_lib)
    add_test(NAME mysql_smoke_test COMMAND mysql_smoke_test)

    # Copy libmysql.dll + SSL deps to output directory for runtime loading
    add_custom_command(TARGET mysql_smoke_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/Program Files/MySQL/MySQL Server 9.7/lib/libmysql.dll"
            $<TARGET_FILE_DIR:mysql_smoke_test>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/Program Files/MySQL/MySQL Connector C++ 9.7/lib64/libssl-3-x64.dll"
            $<TARGET_FILE_DIR:mysql_smoke_test>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/Program Files/MySQL/MySQL Connector C++ 9.7/lib64/libcrypto-3-x64.dll"
            $<TARGET_FILE_DIR:mysql_smoke_test>
        COMMENT "Copying MySQL libmysql.dll + SSL deps to output directory"
    )
else()
    target_compile_definitions(bs_app_sdk PRIVATE BS_WITH_DB=0)
endif()
