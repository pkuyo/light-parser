cmake_minimum_required(VERSION 3.30)
project(json_parser)


add_executable(json_parser main.cpp
        lexer.h
        ast_node.h)
