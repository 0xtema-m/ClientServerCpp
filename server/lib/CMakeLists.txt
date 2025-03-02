include(FetchContent)

find_package(Boost REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIR})

FetchContent_Declare(
  libpqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG a6b1d60e74c1427c8ac2324b85cd4a0dc2068332
)
set(PQXX_LIBRARIES pqxx_static)

FetchContent_MakeAvailable(libpqxx)

add_subdirectory(proto)

add_library(
    server_lib
    server.h
    server.cpp
)

target_link_libraries(server_lib LINK_PUBLIC ${Boost_LIBRARIES})
target_link_libraries(server_lib LINK_PUBLIC ${PQXX_LIBRARIES})
target_link_libraries(server_lib LINK_PUBLIC proto_lib)

target_link_libraries(
    server_lib PRIVATE
    proto_lib
    ${Protobuf_LIBRARIES}
)

target_include_directories(server_lib PRIVATE
    ${CMAKE_BINARY_DIR}/lib
)
