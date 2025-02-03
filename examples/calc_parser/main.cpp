//
// Created by pkuyo on 2025/2/2.
//

#include <vector>
#include <iostream>
#include <cctype>
#include "parser.h"

using namespace pkuyo::parsers;

int main() {
    parser_container<char> container;


    // Terminal Symbol

    // Defines number parsers.
    auto digit = container.SingleValue<char>([](char c) { return std::isdigit(c) != 0; }).Name("digit");
    auto number = digit.More()
            .Map([](auto && digits) {
                return std::atoi(digits.c_str());
            })
            .Name("number");

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
    std::function<base_parser<char, int>*()> expr_factory;


    auto factor = container.Lazy([&]() -> base_parser<char, int>* {
        return (number | (lparen >> container.Lazy(expr_factory) >> rparen)).Get();
    }).Name("factor");

    auto term = (factor >> ((mul | div) >>factor).Many()).Map([](auto && pair) {
        int a = pair.first;
        for (const auto& op_num : pair.second) {
            if (op_num.first == '*') {
                a *= op_num.second;
            } else {
                a /= op_num.second;
            }
        }
        return a;
    }).Name("term");

    auto expression = (term >>((add | sub) >> term).Many()).Map([](auto && pair) {
        int a = pair.first;
        for (const auto& op_num : pair.second) {
            if (op_num.first == '+') {
                a += op_num.second;
            } else {
                a -= op_num.second;
            }
        }
        return a;
    }).Name("expression");

    expr_factory = [&]() { return expression.Get(); };

    // 测试用例
    std::string input = "3+5*(2-4)";

    auto result = expression->Parse(input.begin(), input.end());

    if (result) std::cout << "Result: " << *result << std::endl; // 输出: Result: -7


    return 0;
}