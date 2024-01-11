cmake_minimum_required(VERSION 3.16.3)
project(udp-connections-client C)

add_subdirectory(deps/piot/clog/src/lib)
add_subdirectory(deps/piot/datagram-transport-c/src/lib)
add_subdirectory(deps/piot/flood-c/src/lib)
add_subdirectory(deps/piot/secure-random-c/src/lib)
add_subdirectory(deps/piot/tiny-libc/src/lib)
add_subdirectory(deps/piot/udp-connections-serialize/src/lib)


add_subdirectory(src)
