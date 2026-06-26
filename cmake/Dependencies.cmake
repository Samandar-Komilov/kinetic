include(FetchContent)

# ---------------------------------------------------------------------------
# libuv
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
    message(FATAL_ERROR "libuv not found. Install libuv or set KINETIC_FETCH_DEPS=ON")
endif()

# ---------------------------------------------------------------------------
# libconfig
# ---------------------------------------------------------------------------
set(_kinetic_libconfig_target "")

find_package(libconfig CONFIG QUIET)
if(TARGET libconfig::libconfig)
    message(STATUS "Using CMake package libconfig::libconfig")
    set(_kinetic_libconfig_target libconfig::libconfig)
elseif(PkgConfig_FOUND)
    pkg_check_modules(LIBCONFIG QUIET libconfig)
    if(LIBCONFIG_FOUND)
        message(STATUS "Using system libconfig via pkg-config")
        add_library(kinetic_libconfig INTERFACE)
        target_include_directories(kinetic_libconfig SYSTEM INTERFACE ${LIBCONFIG_INCLUDE_DIRS})
        target_link_libraries(kinetic_libconfig INTERFACE ${LIBCONFIG_LIBRARIES})
        set(_kinetic_libconfig_target kinetic_libconfig)
    endif()
endif()

if(_kinetic_libconfig_target STREQUAL "" AND KINETIC_FETCH_DEPS)
    message(STATUS "libconfig not found; fetching v1.7.3")
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        libconfig
        GIT_REPOSITORY https://github.com/hyperrealm/libconfig.git
        GIT_TAG v1.7.3
    )
    FetchContent_MakeAvailable(libconfig)
    if(TARGET config)
        add_library(kinetic_libconfig_fetched INTERFACE)
        target_link_libraries(kinetic_libconfig_fetched INTERFACE config)
        target_include_directories(
            kinetic_libconfig_fetched
            SYSTEM
            INTERFACE
                "${libconfig_SOURCE_DIR}/lib"
        )
        set(_kinetic_libconfig_target kinetic_libconfig_fetched)
    elseif(TARGET libconfig::libconfig)
        set(_kinetic_libconfig_target libconfig::libconfig)
    else()
        message(FATAL_ERROR "Fetched libconfig but no known CMake target was exported")
    endif()
elseif(_kinetic_libconfig_target STREQUAL "")
    message(FATAL_ERROR "libconfig not found. Install libconfig or set KINETIC_FETCH_DEPS=ON")
endif()

set(KINETIC_LIBUV_TARGET "${_kinetic_libuv_target}")
set(KINETIC_LIBCONFIG_TARGET "${_kinetic_libconfig_target}")
