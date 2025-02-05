//
// Created by pkuyo on 2025/2/2.
//

#include <vector>
#include <iostream>
#include "parser.h"

using namespace pkuyo::parsers;



namespace num_parser {
    parser_container<char> container;

    // Terminal Symbol

    // Defines number parsers.
    auto number = (container.Regex(R"([+-]?(?:\d+\.\d+|\d+|\.\d+))") >>= [](auto && digits) {return std::atof(digits.c_str()); }).Name("number");


// Defines parsers for brackets and operators.
    auto lparen = container.Check('(').Name("(");
    auto rparen = container.Check(')').Name(")");

    auto add = container.SingleValue('+').Name("+");
    auto sub = container.SingleValue('-').Name("-");
    auto mul = container.SingleValue('*').Name("*");
    auto div = container.SingleValue('/').Name("/");


// Non-terminal Symbol

/*
 * expression   = term { ( ADD | SUB ) term }
 * term         = factor { ( MUL | DIV ) factor }
 * factor       = NUMBER | LPAREN expression RPAREN
 */

// Recursive definitions of expression, term, and factor.
    extern base_parser<char, double>* pExpr;


    auto factor = (number | (lparen >> container.Lazy([&]() {return pExpr;}) >> rparen)).Name("factor");

    auto term = ( (factor >> *((mul | div) >> factor)) >>= [](auto && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '*') result *= num;
            else result /= num;
        }
        return result;
    }).Name("term");

    auto expression = ( (term >> *((add | sub) >> term)) >>= [](auto && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '+') result += num;
            else result -= num;
        }
        return result;
    }).Name("expression");

    base_parser<char, double>* pExpr = expression.Get();
}

int main() {



    std::string input = "3.14+5*(2-4)";

    auto result = num_parser::expression->Parse(input.begin(), input.end());

    if (result) std::cout << "Result: " << *result << std::endl;


    return 0;
}