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
    constexpr auto number = (+SingleValue<char>([](char c) { return isdigit(c);})).Name("number")
            >>= [](const std::string & digits) {return std::atof(digits.data()); };


// Defines parsers for brackets and operators.
    constexpr auto lparen = Check<char>('(').Name("(");
    constexpr auto rparen = Check<char>(')').Name(")");

    constexpr auto add = SingleValue<char>('+').Name("+");
    constexpr auto sub = SingleValue<char>('-').Name("-");
    constexpr auto mul = SingleValue<char>('*').Name("*");
    constexpr auto div = SingleValue<char>('/').Name("/");


// Non-terminal Symbol

/*
 * expression   = term { ( ADD | SUB ) term }
 * term         = factor { ( MUL | DIV ) factor }
 * factor       = NUMBER | LPAREN expression RPAREN
 */

// Recursive definitions of expression, term, and factor.
    struct LazyExpr;

    constexpr auto factor = number | (lparen >> Lazy<char,LazyExpr>() >> rparen).Name("factor");

    constexpr auto term =((factor >> *((mul | div) >> factor)) >>= [](std::tuple<double,std::vector<std::tuple<char,double>>> && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '*') result *= num;
            else result /= num;
        }
        return result;
    }).Name("term");

    constexpr auto expression = ((term >> *((add | sub) >> term)) >>= [](std::tuple<double,std::vector<std::tuple<char,double>>> && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '+') result += num;
            else result -= num;
        }
        return result;
    }).Name("expression").OnError([](const _abstract_parser<char> &,const std::optional<char> &) {
        return;
    });

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