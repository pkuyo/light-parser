//
// Created by pkuyo on 2025/2/2.
//

#include <iostream>
#include "parser.h"

int main() {

    using namespace std;
    using namespace pkuyo::parsers;

    auto container = parser_container<char>();

    auto parser = ((container.Str("Hello") >> ",Parser!") <<= [](auto &&) { cout << "parsing" << endl; }) >>= [](auto && t){return string(t) + ",world!";} ;

    string input("Hello,Parser!");

    auto result = parser->Parse(input.cbegin(),input.cend());

    if(result) {
        cout << *result << endl;
    }

    return 0;
}

