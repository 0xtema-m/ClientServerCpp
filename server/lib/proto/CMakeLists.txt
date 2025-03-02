cmake_minimum_required(VERSION 3.12)
find_package(Protobuf REQUIRED)

set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTOC_OUTPUT_DIR ${PROTO_GEN_DIR})

protobuf_generate_cpp(
    PROTO_SRCS
    PROTO_HDRS
    ${PROTO_SRC_DIR}/metric.proto
)

add_library(
    proto_lib STATIC
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_include_directories(proto_lib PUBLIC
    ${PROTO_GEN_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(proto_lib PRIVATE
    ${Protobuf_LIBRARIES}
)

install(TARGETS proto_lib DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ DESTINATION include)
