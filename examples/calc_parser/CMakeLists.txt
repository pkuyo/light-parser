cmake_minimum_required(VERSION 3.27)
project(calc_parser)


add_executable(calc_parser main.cpp
        calc_parser.h)

add_subdirectory(test)