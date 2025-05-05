cmake_minimum_required(VERSION 3.10)
project(metrics_emulator)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system json)
find_package(Threads REQUIRED)

add_executable(emulator emulator.cpp)
target_link_libraries(emulator PRIVATE
    Boost::system
    Boost::json
    Threads::Threads
)

# If you want to install the emulator
install(TARGETS emulator DESTINATION bin)
