add_executable(test_version test_version.c)

target_link_libraries(
    test_version
    PRIVATE
        kinetic_core
)

kinetic_set_warnings(test_version)

add_test(NAME version COMMAND test_version)

add_test(
    NAME hello
    COMMAND kinetic "${CMAKE_CURRENT_SOURCE_DIR}/../configs/kinetic.conf.example"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

set_tests_properties(hello PROPERTIES PASS_REGULAR_EXPRESSION "Hello from kinetic")
