cmake_minimum_required(VERSION 3.27)
project(light_parser)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/googletest
)
FetchContent_MakeAvailable(googletest)


if(WIN32)
    add_compile_definitions(IS_WINDOWS)
endif()
include_directories(include)


add_executable(light_parser main.cpp
        include/pkuyo/parser.h
        include/pkuyo/error_handler.h
        include/pkuyo/traits.h
        include/pkuyo/base_parser.h
        include/pkuyo/compile_time_parser.h
        include/pkuyo/runtime_parser.h
        include/pkuyo/token_stream.h
)

enable_testing()

add_subdirectory(test)
add_subdirectory(examples/customizable_tokens_parser)
add_subdirectory(examples/xml_parser_with_ctx)
add_subdirectory(examples/sax_xml_parser)
add_subdirectory(examples/calc_parser)


