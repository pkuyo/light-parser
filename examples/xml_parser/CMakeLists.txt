cmake_minimum_required(VERSION 3.30)
project(xml_parser)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../xml_parser)

add_executable(xml_parser main.cpp
        xml_def.h)
