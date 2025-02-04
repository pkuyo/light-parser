//
// Created by pkuyo on 2025/2/4.
//

#include <vector>
#include <string>
#include <map>
#include <variant>
#include "parser.h"
#include <iostream>

using namespace pkuyo::parsers;

int main(){

    auto pc = parser_container<char>();
    auto d = pc.SingleValue('c')>> pc.SingleValue('a') >> pc.SingleValue('b');
    std::string s("cab");
    auto re = d->Parse(s.cbegin(),s.cend());
    auto & [c,a,b] = *re;
    std::cout << b;
}