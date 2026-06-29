add_library(
    ktc_core
    STATIC
    core/str.c
    core/arena.c
    external/log/log.c
    version.c
)

target_include_directories(
    ktc_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/log"
)

kinetic_set_warnings(ktc_core)

add_executable(kinetic main.c)

target_link_libraries(
    kinetic
    PRIVATE
        ktc_core
        ${KINETIC_LIBUV_TARGET}
        ${KINETIC_LIBYAML_TARGET}
)

kinetic_set_warnings(kinetic)
