include(FetchContent)

find_package(Boost REQUIRED COMPONENTS program_options json)
include_directories(${Boost_INCLUDE_DIR})

FetchContent_Declare(
  libpqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG a6b1d60e74c1427c8ac2324b85cd4a0dc2068332
)
set(PQXX_LIBRARIES pqxx_static)

FetchContent_MakeAvailable(libpqxx)

add_library(
  service_lib
  service.h
  service.cpp
)

target_link_libraries(service_lib LINK_PUBLIC ${Boost_LIBRARIES})
target_link_libraries(service_lib LINK_PUBLIC ${PQXX_LIBRARIES})
