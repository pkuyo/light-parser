cmake_minimum_required(VERSION 3.30)
project(sax_xml_parser)


add_executable(sax_xml_parser main.cpp
        sax.h)

add_subdirectory(test)
