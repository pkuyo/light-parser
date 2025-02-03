//
// Created by pkuyo on 2025/2/2.
//

#include <iostream>
#include "parser.h"

int main() {

    using namespace std;
    using namespace pkuyo::parsers;

    auto container = parser_container<char>();
    auto mapper= []( const string_view && t){return string(t) + ",world!";};
    auto parser = container.SeqValue<string,string_view>("Hello") [mapper] >>= [](auto &&) { cout << "parsing!" << endl;};

    string input("Hello");

    auto result = parser->Parse(input.cbegin(),input.cend());

    if(result) {
        cout << *result << endl;
    }

    return 0;
}

