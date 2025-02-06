//
// Created by pkuyo on 2025/2/2.
//

#include <iostream>
#include "parser.h"

int main() {

    using namespace std;
    using namespace pkuyo::parsers;


    auto parser = ((Str<char>("Hello") >> ",Parser!") <<= [](const std::string_view &) { cout << "parsing" << endl; })
                    >>= [](const std::string_view & t){return string(t) + ",world!";} ;

    string input("Hello,Parser!");

    auto result = parser.Parse(input);

    if(result) {
        cout << *result << endl;
    }

    return 0;
}

