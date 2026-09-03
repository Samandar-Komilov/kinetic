include(FetchContent)

find_package(PkgConfig QUIET)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# libuv — vcpkg, system pkg-config, or FetchContent.
# ---------------------------------------------------------------------------
add_library(kinetic_libuv INTERFACE)
target_compile_definitions(kinetic_libuv INTERFACE _DEFAULT_SOURCE)
target_link_libraries(kinetic_libuv INTERFACE Threads::Threads)

find_package(unofficial-libuv CONFIG QUIET)
find_package(libuv CONFIG QUIET)

if(TARGET unofficial-libuv::unofficial-libuv)
    message(STATUS "Using vcpkg libuv (unofficial-libuv)")
    target_link_libraries(kinetic_libuv INTERFACE unofficial-libuv::unofficial-libuv)
elseif(TARGET libuv::uv)
    message(STATUS "Using CMake target libuv::uv")
    target_link_libraries(kinetic_libuv INTERFACE libuv::uv)
elseif(TARGET libuv::uv_a)
    message(STATUS "Using CMake target libuv::uv_a")
    target_link_libraries(kinetic_libuv INTERFACE libuv::uv_a)
elseif(PkgConfig_FOUND)
    pkg_check_modules(LIBUV QUIET libuv)
    if(LIBUV_FOUND)
        message(STATUS "Using system libuv via pkg-config: ${LIBUV_VERSION}")
        target_include_directories(kinetic_libuv SYSTEM INTERFACE ${LIBUV_INCLUDE_DIRS})
        target_compile_options(kinetic_libuv INTERFACE ${LIBUV_CFLAGS_OTHER})
        target_link_libraries(kinetic_libuv INTERFACE ${LIBUV_LIBRARIES})
    endif()
endif()

if(NOT TARGET unofficial-libuv::unofficial-libuv AND NOT TARGET libuv::uv AND NOT TARGET libuv::uv_a AND NOT LIBUV_FOUND)
    if(KINETIC_FETCH_DEPS)
        message(STATUS "libuv not found; fetching v1.49.2 via FetchContent")
        set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            libuv
            GIT_REPOSITORY https://github.com/libuv/libuv.git
            GIT_TAG v1.49.2
        )
        FetchContent_MakeAvailable(libuv)
        target_link_libraries(kinetic_libuv INTERFACE uv_a)
    else()
        message(FATAL_ERROR "libuv not found — install libuv-devel, use vcpkg, or set KINETIC_FETCH_DEPS=ON")
    endif()
endif()

set(KINETIC_LIBUV_TARGET kinetic_libuv)

# ---------------------------------------------------------------------------
# libyaml — vcpkg, system pkg-config, or FetchContent.
# ---------------------------------------------------------------------------
add_library(kinetic_libyaml INTERFACE)

find_package(yaml CONFIG QUIET)
find_package(unofficial-libyaml CONFIG QUIET)

if(TARGET yaml)
    message(STATUS "Using CMake target yaml")
    target_link_libraries(kinetic_libyaml INTERFACE yaml)
elseif(TARGET unofficial-libyaml::unofficial-libyaml)
    message(STATUS "Using vcpkg libyaml (unofficial-libyaml)")
    target_link_libraries(kinetic_libyaml INTERFACE unofficial-libyaml::unofficial-libyaml)
elseif(PkgConfig_FOUND)
    pkg_check_modules(LIBYAML QUIET yaml-0.1)
    if(LIBYAML_FOUND)
        message(STATUS "Using system libyaml via pkg-config: ${LIBYAML_VERSION}")
        target_include_directories(kinetic_libyaml SYSTEM INTERFACE ${LIBYAML_INCLUDE_DIRS})
        target_compile_options(kinetic_libyaml INTERFACE ${LIBYAML_CFLAGS_OTHER})
        target_link_libraries(kinetic_libyaml INTERFACE ${LIBYAML_LIBRARIES})
    endif()
endif()

if(NOT TARGET yaml AND NOT TARGET unofficial-libyaml::unofficial-libyaml AND NOT LIBYAML_FOUND)
    if(KINETIC_FETCH_DEPS)
        message(STATUS "libyaml not found; fetching 0.2.5 via FetchContent")
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
        message(FATAL_ERROR "libyaml not found — install libyaml-devel, use vcpkg, or set KINETIC_FETCH_DEPS=ON")
    endif()
endif()

set(KINETIC_LIBYAML_TARGET kinetic_libyaml)
