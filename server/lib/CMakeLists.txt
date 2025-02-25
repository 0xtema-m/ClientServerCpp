find_package(Boost REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIR})

add_library(
    server_lib
    server.h
    server.cpp
)

target_link_libraries(server_lib LINK_PUBLIC ${Boost_LIBRARIES})
