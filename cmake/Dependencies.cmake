include(FetchContent)

# ---------------------------------------------------------------------------
# libuv — fetched or system; linked when you add networking in src/ (notebook 2+).
# ---------------------------------------------------------------------------
set(_kinetic_libuv_target "")

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBUV QUIET libuv)
endif()

if(LIBUV_FOUND)
    message(STATUS "Using system libuv: ${LIBUV_VERSION}")
    add_library(kinetic_libuv INTERFACE)
    target_include_directories(kinetic_libuv SYSTEM INTERFACE ${LIBUV_INCLUDE_DIRS})
    target_link_libraries(kinetic_libuv INTERFACE ${LIBUV_LIBRARIES})
    set(_kinetic_libuv_target kinetic_libuv)
elseif(KINETIC_FETCH_DEPS)
    message(STATUS "libuv not found; fetching v1.49.2")
    set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://github.com/libuv/libuv.git
        GIT_TAG v1.49.2
    )
    FetchContent_MakeAvailable(libuv)
    set(_kinetic_libuv_target uv_a)
else()
    message(STATUS "libuv not found (optional until notebook 2)")
endif()

set(KINETIC_LIBUV_TARGET "${_kinetic_libuv_target}")
