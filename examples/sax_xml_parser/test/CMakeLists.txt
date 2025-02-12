cmake_minimum_required(VERSION 3.27)
project(sax_xml_test)

add_executable(sax_xml_test sax_xml_test.cpp)
target_link_libraries(sax_xml_test  GTest::gtest GTest::gtest_main)

add_test(NAME sax_xml_test COMMAND sax_xml_test)