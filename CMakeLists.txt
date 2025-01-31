cmake_minimum_required(VERSION 3.30)
project(light_parser)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)


add_executable(light_parser main.cpp
        include/parser.h
        test/parser_test.cpp
)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_subdirectory(external/googletest)
add_subdirectory(test)
