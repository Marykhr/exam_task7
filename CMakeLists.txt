
cmake_minimum_required(VERSION 3.20)

project(http_server
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)

enable_testing()

add_executable(server
    src/main.cpp
    src/storage/Storage.cpp
    src/http/HttpParser.cpp
    src/server/Router.cpp
)

target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(storage_tests
    tests/storage_tests.cpp
    src/storage/Storage.cpp
)

target_include_directories(storage_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(storage_tests PRIVATE
    GTest::gtest_main
)

add_executable(parser_tests
    tests/parser_tests.cpp
    src/http/HttpParser.cpp
)

target_include_directories(parser_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(parser_tests PRIVATE
    GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(storage_tests)
gtest_discover_tests(parser_tests)
