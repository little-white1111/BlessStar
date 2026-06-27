cmake_minimum_required(VERSION 3.20)
project(bs_kernel_state VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bs_kernel_state STATIC
    src/ConfigManager.cpp
    src/bus/ShardedStateBus.cpp
    src/bus/StateBus.cpp
    src/event/ConfigEvent.cpp
    src/event/EventBus.cpp
    src/event/WatchManager.cpp
    src/machine/StateMachine.cpp
    src/temp/TemporaryState.cpp
)

target_include_directories(bs_kernel_state PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(bs_kernel_state PUBLIC bs_kernel_common)

if(MSVC)
    target_compile_definitions(bs_kernel_state PRIVATE _CRT_NONSTDC_NO_WARNINGS _CRT_DECLARE_NONSTDC_NAMES=1)
    target_link_libraries(bs_kernel_state PRIVATE oldnames.lib)
endif()
