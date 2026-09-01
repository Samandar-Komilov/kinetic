cmake_minimum_required(VERSION 3.20)

project(
    kinetic
    VERSION 0.1.0
    DESCRIPTION "High-performance, cross-platform web server in C"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KINETIC_BUILD_TESTS "Build unit tests" ON)
option(KINETIC_FETCH_DEPS "Fetch missing dependencies with FetchContent" ON)
option(KINETIC_COVERAGE "Enable code coverage instrumentation" OFF)

if(KINETIC_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(Dependencies)

add_subdirectory(src)

if(KINETIC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
