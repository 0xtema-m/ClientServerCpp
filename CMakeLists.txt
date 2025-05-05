cmake_minimum_required(VERSION 3.10)
project(ClientServerCpp)

# Set global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include subdirectories
add_subdirectory(emulator)

# Optionally include other subdirectories if they have CMake support
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/server/CMakeLists.txt")
    add_subdirectory(server)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/client/CMakeLists.txt")
    add_subdirectory(client)
endif()