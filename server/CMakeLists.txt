cmake_minimum_required(VERSION 3.22.1)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(server)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(lib)
add_subdirectory(bin)

enable_testing()
add_subdirectory(test)
