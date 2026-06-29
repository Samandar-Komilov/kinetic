include(FetchContent)

find_package(PkgConfig QUIET)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# libuv — fetched or system.
# ---------------------------------------------------------------------------
add_library(kinetic_libuv INTERFACE)
target_compile_definitions(kinetic_libuv INTERFACE _DEFAULT_SOURCE)
target_link_libraries(kinetic_libuv INTERFACE Threads::Threads)

if(PkgConfig_FOUND)
    pkg_check_modules(LIBUV QUIET libuv)
endif()

if(LIBUV_FOUND)
    message(STATUS "Using system libuv: ${LIBUV_VERSION}")
    target_include_directories(kinetic_libuv SYSTEM INTERFACE ${LIBUV_INCLUDE_DIRS})
    target_compile_options(kinetic_libuv INTERFACE ${LIBUV_CFLAGS_OTHER})
    target_link_libraries(kinetic_libuv INTERFACE ${LIBUV_LIBRARIES})
elseif(KINETIC_FETCH_DEPS)
    message(STATUS "libuv not found; fetching v1.49.2")
    set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://github.com/libuv/libuv.git
        GIT_TAG v1.49.2
    )
    FetchContent_MakeAvailable(libuv)
    target_link_libraries(kinetic_libuv INTERFACE uv_a)
else()
    message(FATAL_ERROR "libuv not found — install libuv-devel or set KINETIC_FETCH_DEPS=ON")
endif()

set(KINETIC_LIBUV_TARGET kinetic_libuv)

# ---------------------------------------------------------------------------
# libyaml — fetched or system; config parsing (notebook 0).
# ---------------------------------------------------------------------------
add_library(kinetic_libyaml INTERFACE)

if(PkgConfig_FOUND)
    pkg_check_modules(LIBYAML QUIET yaml-0.1)
endif()

if(LIBYAML_FOUND)
    message(STATUS "Using system libyaml: ${LIBYAML_VERSION}")
    target_include_directories(kinetic_libyaml SYSTEM INTERFACE ${LIBYAML_INCLUDE_DIRS})
    target_compile_options(kinetic_libyaml INTERFACE ${LIBYAML_CFLAGS_OTHER})
    target_link_libraries(kinetic_libyaml INTERFACE ${LIBYAML_LIBRARIES})
elseif(KINETIC_FETCH_DEPS)
    message(STATUS "libyaml not found; fetching 0.2.5")
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        libyaml
        GIT_REPOSITORY https://github.com/yaml/libyaml.git
        GIT_TAG 0.2.5
    )
    FetchContent_MakeAvailable(libyaml)
    if(TARGET yaml)
        target_compile_definitions(yaml PRIVATE _DEFAULT_SOURCE)
    endif()
    target_link_libraries(kinetic_libyaml INTERFACE yaml)
else()
    message(FATAL_ERROR "libyaml not found — install libyaml-devel or set KINETIC_FETCH_DEPS=ON")
endif()

set(KINETIC_LIBYAML_TARGET kinetic_libyaml)
