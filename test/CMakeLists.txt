cmake_minimum_required(VERSION 3.27)
project(parser_test)

add_executable(parser_test parser_test.cpp)

target_link_libraries(parser_test  GTest::gtest GTest::gtest_main)

add_test(NAME parser_test COMMAND parser_test)