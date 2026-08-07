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

add_executable(test_config test_config.c)

target_link_libraries(
    test_config
    PRIVATE
        ktc_core
)

kinetic_set_warnings(test_config)

add_test(NAME config COMMAND test_config)
set_tests_properties(config PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_executable(test_req_line test_req_line.c)

target_link_libraries(
    test_req_line
    PRIVATE
        ktc_core
)

kinetic_set_warnings(test_req_line)

add_test(NAME req_line COMMAND test_req_line)

add_executable(test_headers test_headers.c)

target_link_libraries(
    test_headers
    PRIVATE
        ktc_core
)

kinetic_set_warnings(test_headers)

add_test(NAME headers COMMAND test_headers)

add_executable(test_body test_body.c)

target_link_libraries(
    test_body
    PRIVATE
        ktc_core
)

kinetic_set_warnings(test_body)

add_test(NAME body COMMAND test_body)

add_test(
    NAME hello
    COMMAND python3 tests/test_hello.py
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
