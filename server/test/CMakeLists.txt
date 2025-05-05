include(FetchContent)
cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options system json)
include_directories(${Boost_INCLUDE_DIR})

FetchContent_Declare(
  libpqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG a6b1d60e74c1427c8ac2324b85cd4a0dc2068332
)
set(PQXX_LIBRARIES pqxx_static)

FetchContent_MakeAvailable(libpqxx)

add_executable(service_integration_test service_integration_test.cpp)

target_link_libraries(service_integration_test
  GTest::GTest
  GTest::Main
  ${PQXX_LIBRARIES}
  service_lib
  server_lib
  ${Boost_LIBRARIES}
)

target_include_directories(service_integration_test PUBLIC ${PROJECT_SOURCE_DIR})

add_test(NAME ServiceIntegrationTest COMMAND service_integration_test)

set_tests_properties(ServiceIntegrationTest PROPERTIES ENVIRONMENT "VAR=value")
