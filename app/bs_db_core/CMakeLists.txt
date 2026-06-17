cmake_minimum_required(VERSION 3.20)
project(bs_db_core VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_db_core STATIC
    src/db_connector_null.cpp
)

target_include_directories(bs_db_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# No external dependencies — this library is self-contained.