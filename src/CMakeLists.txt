add_library(
    kinetic_core
    STATIC
    config.c
    version.c
)

target_include_directories(
    kinetic_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)

target_link_libraries(
    kinetic_core
    PUBLIC
        ${KINETIC_LIBCONFIG_TARGET}
)

kinetic_set_warnings(kinetic_core)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(kinetic main.c)

target_link_libraries(
    kinetic
    PRIVATE
        kinetic_core
        ${KINETIC_LIBUV_TARGET}
        Threads::Threads
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(kinetic PRIVATE _GNU_SOURCE)
endif()

kinetic_set_warnings(kinetic)
