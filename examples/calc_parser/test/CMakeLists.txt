cmake_minimum_required(VERSION 3.30)
project(calc_test)

add_executable(calc_test calc_test.cpp)

target_link_libraries(calc_test GTest::gtest GTest::gtest_main)

add_test(NAME calc_test COMMAND calc_test)