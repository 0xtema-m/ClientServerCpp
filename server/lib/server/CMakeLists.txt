find_package(Boost REQUIRED COMPONENTS program_options system json)
include_directories(${Boost_INCLUDE_DIR})

add_library(
    server_lib
    server.h
    server.cpp
)

target_link_libraries(server_lib service_lib ${Boost_LIBRARIES})
target_include_directories(server_lib PUBLIC ${PROJECT_SOURCE_DIR})
