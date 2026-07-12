cmake_minimum_required(VERSION 3.10)
project(blessstar C)

set(CMAKE_C_STANDARD 11)

# Source files
set(SOURCES
    src/contract_parser.c
    src/contract_validator.c
    src/spec_renderer.c
    src/spec.c
)

# Include directories
set(INCLUDES
    include
)

add_executable(blessstar ${SOURCES})
target_include_directories(blessstar PRIVATE ${INCLUDES})
