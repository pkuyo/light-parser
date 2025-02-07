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
    constexpr auto number = Name(+SingleValue<char>([](char c) { return isdigit(c);})
            >>= [](const std::string & digits) {return std::atof(digits.data()); },"number");


// Defines parsers for brackets and operators.
    constexpr auto lparen = Check<char>('(');
    constexpr auto rparen = Check<char>(')');

    constexpr auto add = SingleValue<char>('+');
    constexpr auto sub = SingleValue<char>('-');
    constexpr auto mul = SingleValue<char>('*');
    constexpr auto div = SingleValue<char>('/');


// Non-terminal Symbol

/*
 * expression   = term { ( ADD | SUB ) term }
 * term         = factor { ( MUL | DIV ) factor }
 * factor       = NUMBER | LPAREN expression RPAREN
 */

// Recursive definitions of expression, term, and factor.
    struct LazyExpr;

    constexpr auto factor = Name(number | (lparen >> Lazy<char,LazyExpr>() >> rparen),"factor");

    constexpr auto term = Name( (factor >> *((mul | div) >> factor)) >>= [](std::tuple<double,std::vector<std::tuple<char,double>>> && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '*') result *= num;
            else result /= num;
        }
        return result;
    },"term");

    constexpr auto expression = Name( (term >> *((add | sub) >> term)) >>= [](std::tuple<double,std::vector<std::tuple<char,double>>> && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '+') result += num;
            else result -= num;
        }
        return result;
    },"expression");

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