//
// Created by pkuyo on 2025/2/2.
//

#include <vector>
#include <iostream>
#include "calc_parser.h"



int main() {


    pkuyo::parsers::string_stream input("3.14+5*(2-4)");

    auto result = num_parser::expression.Parse(input);

    if (result) std::cout << "Result: " << *result << std::endl;


    return 0;
}