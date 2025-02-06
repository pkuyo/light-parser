//
// Created by pkuyo on 2025/2/2.
//

#include <vector>
#include <iostream>
#include "parser.h"

using namespace pkuyo::parsers;



namespace num_parser {

    // Terminal Symbol

    // Defines number parsers.
    auto number = (Regex<char>(R"([+-]?(?:\d+\.\d+|\d+|\.\d+))") >>= [](std::string && digits) {return std::atof(digits.data()); }).Name("number");


// Defines parsers for brackets and operators.
    auto lparen = Check<char>('(').Name("(");
    auto rparen = Check<char>(')').Name(")");

    auto add = SingleValue<char>('+').Name("+");
    auto sub = SingleValue<char>('-').Name("-");
    auto mul = SingleValue<char>('*').Name("*");
    auto div = SingleValue<char>('/').Name("/");


// Non-terminal Symbol

/*
 * expression   = term { ( ADD | SUB ) term }
 * term         = factor { ( MUL | DIV ) factor }
 * factor       = NUMBER | LPAREN expression RPAREN
 */

// Recursive definitions of expression, term, and factor.
    struct LazyExpr;

    auto factor = (number | (lparen >> Lazy<char,LazyExpr>() >> rparen)).Name("factor");

    auto term = ( (factor >> *((mul | div) >> factor)) >>= [](std::tuple<double,std::vector<std::tuple<char,double>>> && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '*') result *= num;
            else result /= num;
        }
        return result;
    }).Name("term");

    auto expression = ( (term >> *((add | sub) >> term)) >>= [](std::tuple<double,std::vector<std::tuple<char,double>>> && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '+') result += num;
            else result -= num;
        }
        return result;
    }).Name("expression");

    struct LazyExpr : public base_parser<char,LazyExpr> {
         std::optional<double> parse_impl(auto& begin, auto end) const {
            return expression.Parse(begin,end);
        }
        bool peek_impl(auto begin, auto end) const {
            return expression.Peek(begin,end);
        }
    };
}

int main() {



    std::string input = "3.14+5*(2-4)";

    auto result = num_parser::expression.Parse(input);

    if (result) std::cout << "Result: " << *result << std::endl;


    return 0;
}