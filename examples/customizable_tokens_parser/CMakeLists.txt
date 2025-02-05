cmake_minimum_required(VERSION 3.30)
project(customizable_tokens_parser)


add_executable(customizable_tokens_parser main.cpp
        lexer.h
        ast_node.h)
