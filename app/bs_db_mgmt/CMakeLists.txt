cmake_minimum_required(VERSION 3.20)
project(bs_db_mgmt VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_db_mgmt STATIC
    src/db_pool.cpp
    src/db_health_monitor.cpp
    src/db_config_loader.cpp
    src/db_watcher.cpp
    src/db_plugin_loader.cpp
    src/db_mgr.cpp
)

target_include_directories(bs_db_mgmt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Depend on bs_db_core for DbConnector/DbDriverFactory interfaces
target_link_libraries(bs_db_mgmt PUBLIC bs_db_core)
