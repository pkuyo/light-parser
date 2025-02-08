cmake_minimum_required(VERSION 3.30)
project(light_parser)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)


add_executable(light_parser main.cpp
        include/parser.h
        include/error_handler.h
        include/traits.h
        include/base_parser.h
        include/compiler_time_parser.h
        include/runtime_parser.h
        include/token_stream.h
)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_subdirectory(test)
add_subdirectory(external/googletest)
add_subdirectory(examples/customizable_tokens_parser)
add_subdirectory(examples/xml_parser)
add_subdirectory(examples/calc_parser)

