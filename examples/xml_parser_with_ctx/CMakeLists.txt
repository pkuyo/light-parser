cmake_minimum_required(VERSION 3.27)
project(xml_parser_with_ctx)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../xml_parser_with_ctx)

add_executable(xml_parser_with_ctx main.cpp
        xml_def.h)
