//
// Created by pkuyo on 2025/2/12.
//

#ifndef LIGHT_PARSER_CALC_PARSER_H
#define LIGHT_PARSER_CALC_PARSER_H
#include "pkuyo/parser.h"
using namespace pkuyo::parsers;

namespace num_parser {

    // Terminal Symbol

    // Defines number parsers.
    constexpr auto number = (+SingleValue<char>(isdigit) >> ~('.' >> +SingleValue<char>(isdigit)))
                                    .Name("number")
                                    >>= [](auto&& t) {
                auto& [dig, opt] = t;
                if (opt)
                    dig+=('.') + *opt;
                return std::stod(dig);
            };


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

    constexpr auto term =((factor >> *((mul | div) >> factor)) >>= [](auto && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '*') result *= num;
            else result /= num;
        }
        return result;
    }).Name("term");

    constexpr auto expression = ((term >> *((add | sub) >> term)) >>= [](auto && tuple) {
        auto& [result, nums] = tuple;
        for (const auto& [op, num] : nums) {
            if (op== '+') result += num;
            else result -= num;
        }
        return result;
    }).Name("expression");

    struct LazyExpr : public base_parser<char,LazyExpr> {
        std::optional<double> parse_impl(auto& stream,auto g_ctx,auto ctx) const {
            return expression.Parse(stream,g_ctx,ctx);
        }
        bool peek_impl(auto& stream) const {
            return expression.Peek(stream);
        }
    };
}

#endif //LIGHT_PARSER_CALC_PARSER_H
