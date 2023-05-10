cmake_minimum_required(VERSION 3.17)
project(udp_connections_client C)

set(CMAKE_C_STANDARD 99)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else ()
    message("no debug")
    set(isDebug FALSE)
endif ()

if (isDebug)
    message("Debug build detected")
    add_compile_definitions(CONFIGURATION_DEBUG TYRAN_CONFIGURATION_DEBUG)
    set(CONFIGURATION_DEBUG 1)
endif ()

set(DEPS ../../deps/)

file(GLOB_RECURSE deps_src FOLLOW_SYMLINKS "${DEPS}*/*/src/lib/*.c")

message("something ${example_src}")

add_executable(
        udp_connections_client_cli
        main.c ${deps_src}
)

if (WIN32)
    target_link_libraries(udp_connections_client_cli udp_connections_client)
else ()
    target_link_libraries(udp_connections_client_cli PUBLIC udp_connections_client
            m)
endif (WIN32)

target_compile_options(
        udp_connections_client_cli
        PRIVATE -Wall
        -Wextra
        -Wshadow
        -Wstrict-aliasing
        -ansi
        -pedantic
        -Wno-declaration-after-statement
        -Wno-extra-semi-stmt
        -Wno-undef
        -Wno-unused-variable
        -Wno-unused-parameter
        -Wno-padded
        -Werror=implicit-function-declaration
        -Werror=incompatible-pointer-types
        -Werror=missing-prototypes
        -Werror=int-conversion
        -Werror=return-type
        -Werror=incompatible-function-pointer-types
)
if (COMPILER_CLANG)
    target_compile_options(
            udp_connections_client_cli PRIVATE -Wmost -Weverything
            -Werror=missing-variable-declarations)
endif ()

target_include_directories(udp_connections_client_cli PUBLIC ${DEPS}piot/udp-client-c/src/include)