include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Unit Tests - Basement Layer (Phase 0)
# ---------------------------------------------------------------------------
add_executable(test_str unit/basement/test_str.c)
target_link_libraries(test_str PRIVATE ktc_core)
kinetic_set_warnings(test_str)
add_test(NAME unit_str COMMAND test_str)
set_tests_properties(unit_str PROPERTIES LABELS "unit")

add_executable(test_arena unit/basement/test_arena.c)
target_link_libraries(test_arena PRIVATE ktc_core)
kinetic_set_warnings(test_arena)
add_test(NAME unit_arena COMMAND test_arena)
set_tests_properties(unit_arena PROPERTIES LABELS "unit")

add_executable(test_config unit/basement/test_config.c)
target_link_libraries(test_config PRIVATE ktc_core)
kinetic_set_warnings(test_config)
add_test(NAME unit_config COMMAND test_config)
set_tests_properties(unit_config PROPERTIES 
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    LABELS "unit"
)

# ---------------------------------------------------------------------------
# Unit Tests - HTTP Messaging & Parsing (Phase 2)
# ---------------------------------------------------------------------------
add_executable(test_req_line unit/http/test_req_line.c)
target_link_libraries(test_req_line PRIVATE ktc_core)
kinetic_set_warnings(test_req_line)
add_test(NAME unit_req_line COMMAND test_req_line)
set_tests_properties(unit_req_line PROPERTIES LABELS "unit")

add_executable(test_headers unit/http/test_headers.c)
target_link_libraries(test_headers PRIVATE ktc_core)
kinetic_set_warnings(test_headers)
add_test(NAME unit_headers COMMAND test_headers)
set_tests_properties(unit_headers PROPERTIES LABELS "unit")

add_executable(test_body unit/http/test_body.c)
target_link_libraries(test_body PRIVATE ktc_core)
kinetic_set_warnings(test_body)
add_test(NAME unit_body COMMAND test_body)
set_tests_properties(unit_body PROPERTIES LABELS "unit")

# ---------------------------------------------------------------------------
# Integration Tests (Phase 1 & E2E)
# ---------------------------------------------------------------------------
add_test(
    NAME integration_connection_handling
    COMMAND python3 tests/integration/test_connection_handling.py
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
set_tests_properties(integration_connection_handling PROPERTIES LABELS "integration")
