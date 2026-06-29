add_executable(test_version test_version.c)

target_link_libraries(
    test_version
    PRIVATE
        ktc_core
)

kinetic_set_warnings(test_version)

add_test(NAME version COMMAND test_version)

add_executable(test_str test_str.c)

target_link_libraries(
    test_str
    PRIVATE
        ktc_core
)

kinetic_set_warnings(test_str)

add_test(NAME str COMMAND test_str)

add_executable(test_arena test_arena.c)

target_link_libraries(
    test_arena
    PRIVATE
        ktc_core
)

kinetic_set_warnings(test_arena)

add_test(NAME arena COMMAND test_arena)

add_test(
    NAME hello
    COMMAND kinetic
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
