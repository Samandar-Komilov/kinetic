function(kinetic_set_warnings target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
                -Wformat=2
                -Wnull-dereference
                -Wdouble-promotion
                -Wimplicit-fallthrough
        )
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    endif()
endfunction()
