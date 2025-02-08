//
// Created by pkuyo on 2025/2/2.
//

#include <iostream>
#include "parser.h"

int main() {

    using namespace std;
    using namespace pkuyo::parsers;

    constexpr auto parser = ((SeqValue<char,std::string>("Hello") >> ",Parser!")
                                <<= [](const std::string &) { cout << "parsing" << endl; })
                                >>= [](const std::string & t){return t + ",world!";} ;

    string input("Hello,Parser!");

    string_stream stream(input);
    auto result = parser.Parse(stream);

    if(result) {
        cout << *result << endl;
    }

    return 0;
}

