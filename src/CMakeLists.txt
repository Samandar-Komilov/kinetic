add_library(
    ktc_core
    STATIC
    core/str.c
    core/arena.c
    core/parseyml.c
    http/req_line.c
    external/log.c
    version.c
)

target_include_directories(
    ktc_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/ktc/external"
)

kinetic_set_warnings(ktc_core)

target_link_libraries(
    ktc_core
    PUBLIC
        ${KINETIC_LIBYAML_TARGET}
)

add_executable(kinetic main.c)

target_link_libraries(
    kinetic
    PRIVATE
        ktc_core
        ${KINETIC_LIBUV_TARGET}
)

kinetic_set_warnings(kinetic)
