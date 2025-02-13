//
// Created by pkuyo on 2025/2/2.
//

#include <iostream>
#include "pkuyo/parser.h"

int main() {

    using namespace std;
    using namespace pkuyo::parsers;

    constexpr auto parser = ((SeqValue<char,std::string>("Hello") >> ",Parser!")
                                <<= [](const auto&) { cout << "parsing" << endl; })
                                >>= [](const auto& t){return t + ",world!";} ;

    string input("Hello,Parser!");

    string_stream stream(input);
    auto result = parser.Parse(stream);

    if(result) {
        cout << *result << endl;
    }

    return 0;
}

