//
// Created by pkuyo on 2025/2/1.
//
// Parse customizable token example
//

#include "parser.h"

#include "lexer.h"
#include "ast_node.h"

/*
 *   JSON       = Value ;
 *   Value      = Object | Array | STRING | NUMBER | TRUE | FALSE | NULL ;
 *   Object     = LBRACE [ Members ] RBRACE ;
 *   Members    = Pair { COMMA Pair } ;
 *   Pair       = STRING COLON Value ;
 *   Array      = LBRACKET [ Elements ] RBRACKET ;
 *   Elements   = Value { COMMA Value } ;
 *
*/



int main() {


    auto container = pkuyo::parsers::parser_container<Token>();


    //Terminal Symbol

    auto number = container.SinglePtr<NumberNode>(TokenType::NUMBER).Name("Name");

    auto string = container.SinglePtr<StringNode>(TokenType::STRING).Name("String");

    //bool
    auto true_false = (container.SinglePtr<BoolNode>(TokenType::FALSE) | container.SinglePtr<BoolNode>(TokenType::TRUE)).Name("Bool");

    auto null = container.SinglePtr<NullNode>(TokenType::NULL_).Name("Null") >>= [](auto &&t) { return std::unique_ptr<AstNode>(std::move(t)); };

    auto comma = container.Check(TokenType::COMMA).Name(",");

    //Non-terminal Symbol

    pkuyo::parsers::parser_wrapper<Token, std::unique_ptr<AstNode>> *pValue = nullptr;

    auto lazy_value = container.Lazy([&]() { return pValue->Get(); }).Name("Value");

    auto elements = (lazy_value >> *(comma >> lazy_value)).Map([](auto &&t) {
        auto & [first,values] = t;
        values.emplace_back(std::move(first));
        return std::move(values);
    }).Name("Elements");


    auto array = (TokenType::LBRACKET >> elements.Optional() >>TokenType::RBRACKET).Map(
            [](auto &&t) {
                if (!t) return std::make_unique<ArrayNode>(std::vector<std::unique_ptr<AstNode>>());
                return std::make_unique<ArrayNode>(std::move(t.value()));
            }).Name("Array");

    auto pair = (string >> TokenType::COLON >> lazy_value).Map([](auto &&t) {
        return std::make_unique<PairNode>(std::move(std::get<0>(t)), std::move(std::get<1>(t)));
    }).Name("Pair");

    auto members = (pair >> *(comma >> pair)).Map([](auto &&t) {
        auto & [first,values] = t;
        values.emplace_back(std::move(first));
        return std::move(values);
    }).Name("Members");

    auto object = ( TokenType::LBRACE >> members.Optional() >> TokenType::RBRACE ).Map([](auto &&t) {
        if (!t) return std::make_unique<ObjectNode>(std::vector<std::unique_ptr<PairNode>>());
        return std::make_unique<ObjectNode>(std::move(t.value()));
    }).Name("Object");

    auto value = ( null | object | array | string | number | true_false).Name("Value");

    pValue = &value;

    auto & parser = value;

    std::string input =R"(
    {
        "name": "John Doe",
        "age": 30,
        "is_student": false,
        "skills": ["C++", "Python", "JavaScript"],
        "address": {
            "city": "New York",
            "zip": "10001"
        }
    }
    )";

    auto lexer = JSONLexer(input);
    auto tokens = lexer.tokenize();

    auto result = parser->Parse(tokens.cbegin(), tokens.cend());

    Visitor visitor{};

    (result.value())->visit(visitor);
    return 0;
}